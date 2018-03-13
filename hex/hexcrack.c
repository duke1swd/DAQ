/*
 * Crack a hex file.
 *
 * Format is documented here: http://en.wikipedia.org/wiki/.hex
 *
 * July 5, 2008
 *
 * Stephen Daniel
 *
 * Hybridsky.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

char *myname;
FILE *input;
FILE *output;
char *input_name;
char *output_name;
int line_count;
int low_cut_address;

int id;
int version;

static int
atoihn(char *s, int n)
{
	int result;

	result = 0;

	while (n-- > 0) {
		if (!*s)
			break;
		result <<= 4;
		if (*s >= '0' && *s <= '9')
			result |= (*s - '0');
		else if (*s >= 'A' && *s <= 'F')
			result |= (*s - 'A' + 10);
		else if (*s >= 'a' && *s <= 'f')
			result |= (*s - 'a' + 10);
		else if (line_count >= 0)
			fprintf(stderr, "%s: line %d of file %s "
					"contains an invalid hex digit\n",
					myname, line_count, input_name);
		s++;
	}

	return result;
}

int load_address;
int load_top;
int base_address;
int byte_count;
int address;

unsigned char data[65536 + 512];

static void
set_defaults()
{
	input_name = "(stdin)";
	input = stdin;

	id = 1;
	version = 1;
	low_cut_address = 0;
}

static void
usage()
{
	set_defaults();

	fprintf(stderr, "Usage: %s <options> <input file> <output file>\n",
		myname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-i <id (%d)>\n", id);
	fprintf(stderr, "\t-v <version (%d)>\n", version);
	fprintf(stderr, "\t-L <low address cutoff (0x%x\n", low_cut_address);
	exit(1);
}


static void
grok_args(int argc, char **argv)
{
	int c;
	int errors;
	int nargs;

	myname = *argv;

	set_defaults();

	errors = 0;
	while ((c = getopt(argc, argv, "i:v:L:?")) != EOF)
	switch (c) {
	    case 'i':
	    	id = atoi(optarg);
		break;
	    case 'v':
	    	version = atoi(optarg);
		break;
	    case 'L':
	    	line_count = -1;
		low_cut_address = atoihn(optarg, 4);
		break;
	    case '?':
	    case 'h':
	    default:
		usage(0);
		break;
	}

	nargs = argc - optind;
	if (errors || nargs != 2)
		usage();

	input_name = argv[optind++];
	output_name = argv[optind++];

	input = fopen(input_name, "r");
	if (input == NULL) {
		fprintf(stderr, "%s: cannot open %s for reading\n",
			myname, input_name);
		perror("open");
		errors++;
	}

	output = fopen(output_name, "w");
	if (output == NULL) {
		fprintf(stderr, "%s: cannot open %s for writing\n",
			myname, output_name);
		perror("open");
		errors++;
	}

	if (errors)
		usage();
}

static void
process(char *s)
{
	int i;
	int top;

	top = address + byte_count;

	if (load_address < 0) {
		load_address = address;
		load_top = top;
	}

	if (top > load_top)
		load_top = top;

	if (top >= sizeof data) {
		fprintf(stderr, "%s: line %d of file %s "
				"loads data above 64K\n",
				myname, line_count, input_name);
		exit(1);
	}

	for (i = 0; i < byte_count; i++)
		data[i + address] = atoihn(s + 9 + i * 2, 2);
}


void
doinput(void)
{
	int i;
	int eof;
	int record_type;
	char lbuf[128];

	line_count = 0;
	eof = 0;
	load_address = -1;
	load_top = -1;
	while (fgets(lbuf, sizeof lbuf, input) == lbuf) {
		line_count++;
		if (eof) {
			fprintf(stderr, "%s: line %d of file %s "
					"comes after EOF\n",
					myname, line_count, input_name);
			exit(1);
		}

		i = strlen(lbuf) - 1;
		if (lbuf[i] == '\n')
			i--;
		if (lbuf[i] == '\r')
			i--;
		lbuf[++i] = '\0';

		if (lbuf[0] != ':') {
			fprintf(stderr, "%s: line %d of file %s does not "
					"begin with a colon\n",
					myname, line_count, input_name);
			exit(1);
		}
		byte_count = atoihn(lbuf + 1, 2);
		if (strlen(lbuf) != byte_count * 2 + 11) {
			fprintf(stderr, "%s: line %d of file %s should be "
					"%d bytes long, is %lu\n",
					myname, line_count, input_name,
					byte_count * 2 + 11, strlen(lbuf));
			exit(1);
		}
		address = atoihn(lbuf + 3, 4);
		record_type = atoihn(lbuf + 7, 2);

		switch (record_type) {
		case 0:
			process(lbuf);
			break;
		case 1:
			eof = 1;
			break;
		case 4:
			i = atoihn(lbuf + 9, 4);
			if (i != 0) {
				fprintf(stderr, "%s: line %d of file %s "
						"invalid record type 4\n",
						myname, line_count, input_name);
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "%s: line %d of file %s "
					"invalid rcord type %d\n",
					myname, line_count, input_name,
					record_type);
			exit(1);
		}
	}

	if (!eof) {
		fprintf(stderr, "%s: line %d of file %s "
				"no EOF\n",
				myname, line_count, input_name);
		exit(1);
	}


	printf("%d lines found.\n", line_count);
	printf("data loaded from %x to %x\n", load_address, load_top);
}

void
dooutput(void)
{
	int i, j;
	unsigned char buffer[512];

	for (j = 0; j < sizeof buffer; j++)
		buffer[j] = 0;

	/*
	 * Set up the header record.
	 * (See the README for format information.)
	 */
	buffer[0] = 0x44;
	buffer[1] = 0x55;
	buffer[2] = id;
	buffer[3] = version;

	if (base_address < low_cut_address)
		base_address = low_cut_address;

	buffer[4] = (base_address >> 8) & 0xff;
	buffer[5] = base_address & 0xff;

	i = load_top - base_address;
	buffer[6] = (i+511) / 512;
	buffer[7] = (i % 512)/2;

	fwrite(buffer, sizeof buffer, 1, output);

	fwrite(data + base_address, i, 1, output);

	/*
	 * Now zero fill the end of the file.
	 */
	for (j = 0; j < sizeof buffer; j++)
		buffer[j] = 0;

	if (i % 512)
		fwrite(buffer, 512 - (i % 512), 1, output);
}

int
main(int argc, char **argv)
{
	grok_args(argc, argv);
	printf("ID =      %d\n", id);
	printf("Version = %d\n", version);
	if (low_cut_address)
		printf("Low Address Cutoff = %x\n", low_cut_address);
	doinput();
	dooutput();
	exit(0);
}
