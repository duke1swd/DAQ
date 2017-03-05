/* 
 * Read the datafile.
 */
#include <stdio.h>
#include <stdlib.h>

char *myname;

char *datafile = "/cygdrive/e/DATA";

FILE *input;

void print_tod(unsigned char *p);

void
doit(void)
{
	int i;
	unsigned char *p;
	unsigned value, t;
	unsigned char buffer[512];
	int counter;
	short svalue;
	int first_record;
	int max;

	counter = 0;
	first_record = 1;
	max = -20000;
	while (fread(buffer, sizeof buffer, 1, input)) {
		p = buffer;
		for (i = 0; i < 256; i++) {
			/* skip header (12 bytes) */
			if (first_record) {
				first_record = 0;
				p = buffer + 12;
				print_tod(buffer+4);
				i = 6;
			}
			value = *p++;
			value |= (*p++) << 8;

			if (i == 0) {
			    if (value) {
				printf("Skipped = %d\n", value);
				fprintf(stderr, "Skipped = %d\n", value);
			    }
			} else if (i == 1 && value == 0) {
				printf("EOF\n");
				goto done;
			} else {
				t = value & 3;
				if (t == 0 || t == 3) {
					printf("BAD SAMPLE: 0x%02x\n", value);
					goto done;
				} else {
				    counter++;
				    svalue = value;
				    svalue >>= 4;
				    if (svalue> max)
				    	max = svalue;
				    printf("%d,%d,%d\n",
					svalue,
					(value >> 3) & 1,
					(value >> 2) & 1);
				}
			}
		}
	}
    done:
	fprintf(stderr, "%d samples found\n", counter);
	fprintf(stderr, "Max value was %d\n", max);
}

int
main(int argc, char **argv)
{
	myname = *argv;

	input = fopen(datafile, "r");
	if (input == NULL) {
		fprintf(stderr, "%s: cannot open %s for reading.\n",
			myname,datafile);
		perror("read");
		exit(1);
	}
	doit();
	fclose(input);
	return 0;
}
