/*
 * This program runs on the PC and pulls data off the card.
 * One channel per sample.
 */

#include <stdio.h>
#include <stdlib.h>

char *myname;
char *file = "/cygdrive/e/data";

static void
usage(void)
{
	fprintf(stderr, "Usage: %s\n", myname);
	fprintf(stderr, "\tReads from file %s\n", file);
	exit(1);
}

void
print_record(unsigned char record[2])
{
	unsigned short v0;
	unsigned char b1, b0;
	unsigned char c1, c0;
	int dv;
	double fv;

	v0 = 0;

	b1 = 0;
	b0 = 0;
	c1 = 0;
	c0 = 0;

	/*
	 * Decode bits out of first byte
	 */
	if (record[0] & 0x1)
		c1 = 1;
	if (record[0] & 0x2)
		c0 = 1;
	if (record[0] & 0x1)
		b1 = 1;
	if (record[0] & 0x2)
		b0 = 1;

	/*
	 * Now find the 12-bit samples.
	 */
	v0 =  0xf & (record[0] >> 4);
	v0 |= 0xff0 & (record[1] << 4);

	dv = v0;
	if (v0 & 0x800)
		dv = (signed short)(v0 | 0xf000); // sign extend
	fv = (double)dv / 2048. * 5.;

	printf("%x, %d, %.3f\n", v0, dv, fv);
}


#define	BATCH	255
#define	NSEC	1*2048

int
main(int argc, char **argv)
{
	int r, i;
	int sector;
	FILE *input;
	unsigned short sample_skip;
	unsigned char record[2];

	myname = *argv++;
	if (argc != 1)
		usage();

	input = fopen(file, "r");
	if (input == NULL) {
		fprintf(stderr, "%s: cannot open %s for reading.\n",
			myname, file);
		perror("open");
		exit(1);
	}

	for (sector = 0; sector < NSEC; sector++) {
		r = fread(&sample_skip, sizeof sample_skip, 1, input);
		if (feof(input))
			break;
		if (r != 1) {
			fprintf(stderr, "%s read error on file %s\n",
				myname, file);
			exit(1);
		}

		printf("Sample skip at sector %d: %d\n",
			sector, sample_skip);

		for (i = 0; i < BATCH; i++) {
			r = fread(record, sizeof record, 1, input);
			if (feof(input)) {
				fprintf(stderr, "%s: unexpected EOF\n",
					myname);
				exit(1);
			}
			if (r != 1) {
				fprintf(stderr, "%s read error on file %s\n",
					myname, file);
				exit(1);
			}
			print_record(record);
		}
	}
	fclose(input);
	return 0;
}
