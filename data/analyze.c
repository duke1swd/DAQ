/*
 * Analyze one of the test runs.
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#define	LINE_LEN	128
#define	LINES_PER_MS	5

char *myname;
char *input_file_name;
FILE *input;
int csv;
int plotfile;
int ipa_mode;
double chamber_threshold;
double chamber_hysteresis; 
int chamber_transient_time;

static void
set_defaults()
{
	csv = 0;
	plotfile = 0;
	ipa_mode = 0;
	chamber_threshold = 75;	// PSI absolute
	chamber_hysteresis = 5; // PSI
	chamber_transient_time = 3;
}

static void
usage()
{
	set_defaults();
	fprintf(stderr, "Usage: %s <options> [input file]\n",
		myname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-e (output in CSV format)\n");
	fprintf(stderr, "\t-p (generate plot file commands)\n");
	fprintf(stderr, "\t-I (analyze IPA flow performance)\n");
	exit(1);
}

static void
grok_args(int argc, char **argv)
{
	int c;
	int errors;
	int nargs;

	myname = *argv;
	errors = 0;
	set_defaults();

	while ((c = getopt(argc, argv, "Ipeh")) != EOF)
	switch(c) {
	    case 'I':
		ipa_mode++;
		break;

	    case 'p':
	    	plotfile++;
		break;

	    case 'e':
	    	csv++;
		break;

	    case 'h':
	    case '?':
	    default:
	    	usage();
	}

	if (ipa_mode && plotfile) {
		fprintf(stderr, "%s: cannot use both -I and -p\n",
				myname);
		errors++;
	}

	nargs = argc - optind;

	if (nargs == 0)  {
		input_file_name = "-";
		if (plotfile) {
			fprintf(stderr, "%s: plotfile (-p) requires named"
				"input file\n",
				myname);
			errors++;
		}
	} else
		input_file_name = argv[optind];

	if (nargs <= 1) {
		if (strcmp(input_file_name, "-") == 0)
			input = stdin;
		else
			input = fopen(input_file_name, "r");
		if (input == NULL) {
			fprintf(stderr, "%s: cannot open file %s for reading\n",
				myname,
				input_file_name);
			perror("open");
			errors++;
		}
	}

	if (nargs > 1) {
		fprintf(stderr, "%s: Can only process one input file.\n",
				myname);
		errors++;
	}

	if (errors)
		usage();
}

static char dateline[LINE_LEN];
static int line_number;
static int end_of_input;

/*
 * Returns 1 on OK, 0 on EOF, fails to return on all errors
 */
static int
my_get_line(char *lbuf)
{
	if (end_of_input)
		return 0;

	if (fgets(lbuf, LINE_LEN, input) != lbuf) {
		if (feof(input)) {
			end_of_input = 1;
			return 0;
		}
		fprintf(stderr, "%s: Cannot read a line from input file %s\n",
			myname, input_file_name);
		perror("read");
		exit(1);
	}
	if (strcmp(lbuf, "EOF\n") == 0) {
		end_of_input = 1;
		return 0;
	}
	line_number++;
	return 1;
}

static void
header()
{
	char lbuf[LINE_LEN];

	if (my_get_line(lbuf) != 1 ||
	    my_get_line(lbuf) != 1 ||
	    my_get_line(lbuf) != 1 ||
	    my_get_line(lbuf) != 1 ||
	    my_get_line(lbuf) != 1 ||
	    my_get_line(dateline) != 1 ||
	    my_get_line(lbuf) != 1 ||
	    my_get_line(lbuf) != 1) {
	    	fprintf(stderr, "%s: EOF while reading 8-line header of file %sn",
			myname,
			input_file_name);
		exit(1);
	}
	printf("Test date line: %s", dateline);
}

static double
count_to_psi(int count)
{
	double psi;

	psi = count;
	psi = psi * 5. / 4096;	 // now in volts.
	psi = psi - .5;
	psi = 500. * psi / 4;

	return psi;
}

static int c0, c1, c2, o0, o1;
/*
 * Parses a data line into c0, c1, c2, o0, and o1.
 * Does not return on error.
 * Otherwise returns as per my_get_line
 */
static int
process_line()
{
	int r;
	char lbuf[128];

	if (my_get_line(lbuf) == 0)
		return 0;
	r = sscanf(lbuf, "%d,%d,%d,%d,%d", &c0, &c1, &c2, &o0, &o1);

	if (r != 5) {
		fprintf(stderr, "%s: badly formed line %d in file %s\n",
			myname,
			line_number,
			input_file_name);
		fprintf(stderr, "Input line is:\n");
		fputs(lbuf, stderr);
		exit(1);
	}
	if (o0 < 0 || o0 > 1 || o1 < 0 || o1 > 1) {
		fprintf(stderr, "%s: bad opto data in line %d in file %s\n",
			myname,
			line_number,
			input_file_name);
		fprintf(stderr, "Input line is:\n");
		fputs(lbuf, stderr);
		exit(1);
	}
	return 1;
}

// These three routines do the IPA analysis
int chamber_on;			// boolean
int chamber_on_count;
int chamber_on_line;		// time, measured in lines of data
int line;			// time, now.

static void
ipa_init()
{
	chamber_on = 0;
	chamber_on_count = 0;
	chamber_on_line = -1;	
	line = 0;
}

static void
ipa_process()
{
	double c_now;

	c_now = count_to_psi(c2);
	if (chamber_on && c_now < chamber_threshold - chamber_hysteresis) {
		chamber_on = 0;
		chamber_on_line = -1;
	} else if (!chamber_on &&
			c_now > chamber_threshold + chamber_hysteresis) {
		// need good chamber pressure for several lines to turn on the chamber
		if (chamber_on_line < 0) {
			chamber_on_line = line;
		} else if (line - chamber_on_line > chamber_transient_time) {
			chamber_on = 1;
			chamber_on_count++;
		}
	}
	line++;
}

static void
ipa_end()
{
	if (chamber_on_count != 1 ||
			count_to_psi(c2) < chamber_threshold)
		printf("Bad Run\n");
	else
		printf("Good Run\n");
}

static void
process()
{
	int sub_test;
	/*
	 * Loop once per test firing, as shown on opto0
	 */
	for (sub_test = 1;; sub_test++) {
		/*
		 * Loop looking for the start of the firing
		 */
		for (;;) {
			if (process_line() == 0)
				return;
			if (o0 == 1)
				break;
		}

		if (csv) {
			printf("%d,%d,%.1f,%.1f,",
				sub_test,
				line_number/LINES_PER_MS,
				count_to_psi(c0),
				count_to_psi(c1));
		} else {
			printf("\nsub test %d at line %d\n",
				sub_test, line_number);
			printf("Init N2O: %.0f\n", count_to_psi(c0));
			printf("Init IPA: %.0f\n", count_to_psi(c1));
		}

		if (ipa_mode)
			ipa_init();

		/*
		 * Loop looking for the end of the firing
		 */
		for (;;) {
			if (process_line() == 0)
				break;
			if (o0 == 0)
				break;
			if (ipa_mode)
				ipa_process();
		}

		if (csv) {
			printf("%d,%.1f,%.1f,%.1f\n",
				line_number/LINES_PER_MS,
				count_to_psi(c0),
				count_to_psi(c1),
				count_to_psi(c2));
		} else {
			printf("Final N2O: %.0f\n", count_to_psi(c0));
			printf("Final IPA: %.0f\n", count_to_psi(c1));
			printf("Final Chamber: %.0f\n", count_to_psi(c2));
		}

		if (ipa_mode)
			ipa_end();
	}
}

static void
report_header()
{
	printf( "sub-test,start-time,initial-n2o,initial-ipa,"
		"end-time,final-n2o,final-ipa,final-chamber\n");
}

int
main(int argc, char **argv)
{
	grok_args(argc, argv);

	line_number = 0;
	end_of_input = 0;
	header();
	if (csv)
		report_header();
	process();
	return 0;
}
