/* 
 * Program reads the .csv files produced by readMarch.c
 * and tells us somethings of interest.
 */

#include <stdio.h>

char *myname;

char *outfilename;
char *infilename;

FILE *out;
FILE *in;

int num_samples;
int start_sample;
int start_sample_set;
int compact_format;
int down_sample_factor;

static void
set_defaults()
{
	outfilename = (char *)0;
	infilename = (char *)0;
	start_sample_set = 0;
	num_samples = 32000;
	compact_format = 0;
	down_sample_factor = 1;
}

static void
usage()
{
	set_defaults();
	fprintf(stderr, "Usage: %s [options] [input file]\n",
		myname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-o <output file>\n"
			"\t-s <start sample (pressure rise)>\n"
			"\t-c (output in compact format, not original format)\n"
			"\t-D <down sample factor (%d)>\n"
			"\t-n <num samples (%d)>\n",
			down_sample_factor, num_samples);
	exit(1);
}

static void
grok_args(int argc, char **argv)
{
	int errors;
	int c;
	int nargs;
	extern int optind;
	extern char *optarg;

	myname = *argv;
	errors = 0;

	set_defaults();

	while ((c = getopt(argc, argv, "D:cs:n:o:h")) != EOF)
	switch (c) {
	    case 'D':
	    	down_sample_factor = atoi(optarg);
		break;
	    case 'c':
	    	compact_format++;
		break;
	    case 's':
	    	start_sample = atoi(optarg);
		start_sample_set = 1;
		break;
	    case 'n':
	    	num_samples = atoi(optarg);
		break;
	    case 'o':
	    	outfilename = optarg;
		break;
	    case 'h':
	    case '?':
	    default:
	    	usage();
	}

	nargs = argc - optind;
	if (nargs < 0 || nargs > 1)
		usage();

	if (nargs == 1)
		infilename = argv[optind];

	if (infilename && strcmp(infilename, "-") != 0) {
		in = fopen(infilename, "r");
		if (in == NULL) {
			fprintf(stderr, "%s: cannot open %s for reading\n",
				myname, infilename);
			errors++;
		}
	} else {
		infilename = "-";
		in = stdin;
	}

	out = NULL;
	if (outfilename) {
		out = fopen(outfilename, "w");
		if (out == NULL) {
			fprintf(stderr, "%s: cannot open %s for writing\n",
				myname, outfilename);
			errors++;
		}
	}

	if (start_sample_set && start_sample < 1) {
		fprintf(stderr, "%s: start sample number (%d) must be "
				"greater than zero.\n",
				myname, start_sample);
		errors++;
	}

	if (down_sample_factor < 1) {
		fprintf(stderr, "%s: down sample factor (%d) must be "
				"greater than zero.\n",
				myname, down_sample_factor);
		errors++;
	}

	if (down_sample_factor > 1 && !compact_format) {
		fprintf(stderr, "%s: down sample factor (%d) > 1 requires "
				"-c (compact format)\n",
				myname, down_sample_factor);
		errors++;
	}

	if (errors)
		exit(1);
}

/*
 * Processing happens here.
 */

#define 	NZS	48	/* number of sample to average to obtain zero */

static char lbuf[256];
int sample;
int pressure;
int digital_in;
double zero;

/*
 * Returns true if got a sample.
 * False if EOF
 */
static int
get_values()
{
	int r;
	int x, y;

	if (fgets(lbuf, sizeof lbuf, in) != lbuf)
		return 0;
	r = sscanf(lbuf, "%d,%d,%d,%d,%d", &sample, &pressure,
		&x, &y, &digital_in);
	
	if (r != 5) {
		fprintf(stderr, "%s: malformed input.\n", myname);
		exit(1);
	}
	
	return 1;
}

static double
psi(int sample)
{
	return ((double)sample - zero) * 5. * 500. / 4. / 4096.;
}

static int down_sample_counter;
static double down_sample_pressure;

static void
down_sample_init()
{
	down_sample_counter = 0;
	down_sample_pressure = 0.;
}

static void
down_sample()
{
	if (out == NULL)
		return;

	down_sample_pressure += psi(pressure);

	if (down_sample_counter++ < down_sample_factor)
		return;

	fprintf(out, "%d,%.2f\n", sample, down_sample_pressure /
					(double) down_sample_factor);
	num_samples--;

	down_sample_init();
}

static void
doit()
{
	int i;
	int max_pressure;
	int mpsample;
	int found_digital;
	int found_pressure;

	/*
	 * Header.
	 */
	fgets(lbuf, sizeof lbuf, in);
	if (out != NULL)
	    if (compact_format)
		fprintf(out, "Sample,PSI\n");
	    else
	    	fputs(lbuf, out);

	/* loop getting baseline */
	zero = 0.;
	for (i = 0; i < NZS; i++) {
		if (!get_values()) {
			fprintf(stderr, "%s: input file %s has too few samples\n",
				myname, infilename);
			exit(1);
		}
		if (i == 0)
		    printf("First sample found was %d (%.1f)\n", sample,
			(double)sample / 8000.);
		
		zero += pressure;
	}
	zero /= (double)NZS;
	printf("zero is %.1f\n", zero);

	found_digital = 0;
    	max_pressure = 0;
	found_pressure = 0;

	down_sample_init();

    	while (get_values()) {

		if (!found_pressure && pressure - zero > 3) {
			found_pressure = 1;
			printf("First pressure rise seen at sample %d (%1.f)\n",
				sample, (double)sample/8000.);
			if (!start_sample_set)
				start_sample = sample;
		}

		if (!found_digital && digital_in) {
			found_digital = 1;
			printf("Digital signal first seen at sample %d (%.1f)\n",
				sample, (double)sample/8000.);
		}

		if (out != NULL &&
		    sample >= start_sample &&
		    num_samples > 0)
		    	if (compact_format)
				down_sample();
			else {
				num_samples--;
				fputs(lbuf, out);
			}
	
		if (max_pressure < pressure) {
			max_pressure = pressure;
			mpsample = sample;
		}
	}

	printf("Max pressure was %d (%.2f)\n", max_pressure, psi(max_pressure));
	printf("Max pressure found at sample %d, (%.1f)\n",
		mpsample, (double)mpsample/8000.);
	printf("Last sample found was %d (%.1f)\n", sample,
		(double)sample / 8000.);
}


/*
 * Sequencing.
 */
int
main(int argc, char **argv)
{
	grok_args(argc, argv);
	doit();
	fclose(in);
	if (out != NULL)
		fclose(out);
	return 0;
}
