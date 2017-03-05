/* 
 * Read the datafile.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int bipolar[3];
int channel[3];

char *myname;

char *datafile = "/cygdrive/g/DATA";

FILE *input;

void print_tod(unsigned char *p);

/*
 * Process the header.
 */
void
process_header(unsigned char *p)
{
	int i;

	if (p[0] != 1 && p[1] != 0) {
		fprintf(stderr, "WARNING: cannot grok file header.\n");
		printf("WARNING: cannot grok file header.\n");
		return;
	}
	printf("Three channel data, format #1\n");
	p += 2;
	
	for (i = 0; i < 3; i++) {
		channel[i] = p[0];
		bipolar[i] = p[3];
		printf("Channel %d, %s\n", channel[i],
			(bipolar[i]? "bipolar": "unipolar"));
		p += 1;
	}
	p += 3;

	printf("\n");
	print_tod(p);
	printf("\n");

	for (i = 0; i < 3; i++)
		printf("Channel %d, ", channel[i]);
	printf("Opto 0, Opto 1\n");
}

void
doit(void)
{
	int i;
	int low_bits;
	int opt0, opt1;
	unsigned char *p;
	unsigned value, t;
	unsigned char buffer[512];
	int counter;
	short svalue;
	int first_record;

	counter = 0;
	first_record = 1;
	while (fread(buffer, sizeof buffer, 1, input)) {
		p = buffer;
		for (i = 0; i < 102; i++) {
			/* skip header */
			if (first_record) {
				process_header(p);
				first_record = 0;
				/*
				 * set p to the first record boundary.
				 * records are 5 bytes long, and begin
				 * on an index that is 2 mod 5.
				 */
				p = buffer + 17;
				i = (17-2)/5;
			}

			// Byte 0
			value = *p++;

			// Byte 1
			value |= (*p++) << 8;

			if (i == 0) {
			    if (value) {
				printf("Skipped = %d\n", value);
				fprintf(stderr, "Skipped = %d\n", value);
			    }
			    continue;
			} else if (i == 1 && (value == 0 || value == 0xffff)) {
				printf("EOF\n");
				goto done;
			} else {
				t = value & 3;
				if (t == 0 || t == 3) {
					printf("BAD SAMPLE: 0x%02x\n", value);
				} else {
				    counter++;
				    svalue = value;
				    svalue >>= 4;
				    if (!bipolar[0])
					svalue &= 0xfff;
				    printf("%d,", svalue);
				}
				opt0 = (value >> 3) & 1;
				opt1 = (value >> 2) & 1;

				// Byte 2
				low_bits = *p;
				value = *p++;

				// Byte 3
				value |= *p << 8;
				svalue = value;
				svalue >>= 4;
				if (!bipolar[1])
					svalue &= 0xfff;
				printf("%d,", svalue);

				// Byte 3
				value = ((*p++) & 0xf) << 4;
/*xxx*/value=0;

				// Byte 4
				value |= (*p++) << 8;
				svalue = value;
				svalue >>= 4;
				svalue |= (low_bits & 0xf);
				if (!bipolar[2])
					svalue &= 0xfff;
				printf("%d,", svalue);
				printf("%d,%d\n", opt0, opt1);
			}
		}
	}
    done:
	fprintf(stderr, "%d samples found\n", counter);
}

static void
usage()
{
	fprintf(stderr, "Usage: %s [input file]\n", myname);
	exit(1);
}

int
main(int argc, char **argv)
{
	myname = *argv;

	if (argc > 2)
		usage();

	if (argc == 2 && strcmp(argv[1], "-h") == 0)
		usage();

	if (argc == 2)
		datafile = argv[1];

	input = fopen(datafile, "r");
	if (input == NULL) {
		fprintf(stderr, "%s: cannot open %s for reading.\n",
			myname,datafile);
		perror("read");
		usage();
	}
	doit();
	fclose(input);
	return 0;
}
