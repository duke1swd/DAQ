/*
 * This version matches the data capture software running on April 13, 2008.
 */
#include <stdio.h>

#define	BUFSIZE	(16*1024)

int
main(int argc, char **argv)
{
	int fd;
	int r;
	int i;
	int d;
	int skip;
	int v1, v2, counter, f;

	char buffer[BUFSIZE];
	struct record_s {
		unsigned short w1;
		unsigned short w2;
	} *rp;
	

	fd = open("/dev/sdb", 0);
	if (fd < 0) {
		fprintf(stderr, "cannot open\n");
		perror("open");
		exit(1);
	}

	printf("Sample, V1, V2, File, Skip, Digital\n");

	counter = 0;
	for (;;) {
		r = read(fd, buffer, sizeof buffer);
		if (r < 0) {
			fprintf(stderr, "cannot read\n");
			perror("read");
			exit(1);
		} else if (r == 0) {
			fprintf(stderr, "EOF\n");
			exit(1);
		} else if (r < sizeof buffer)
			fprintf(stderr, "short read: %d\n", r);

		rp = (struct record_s *)buffer;
		for (i = 0; i < sizeof buffer / sizeof (struct record_s); i++) {

			/* All zeros == EOF */
			if (rp->w1 == 0 && rp->w2 == 0)
				goto done;

			/* Data values are in the high order 12 bits */
			v1 = (rp->w1 >> 4) & 0xfff;
			v2 = (rp->w2 >> 4) & 0xfff;

			/* Digital input is value2.3 */
			d = (rp->w2 >> 3) & 1;

			/* Skip is value1.0 */
			skip = (rp->w1) & 1;

			/* File number is value1, bits 1, 2 3 */
			f = (rp->w1 >> 1) & 3;

			printf("%d,%d,%d,%d,%d,%d\n",
				counter, v1, v2, f, skip, d);
			rp++;
			counter++;
		}
	}
    done:;
    	fprintf(stderr, "%d records found.\n", counter);
}
