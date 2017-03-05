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
	int v1, counter, f;

	char buffer[BUFSIZE];
	struct record_s {
		unsigned short w1;
	} *rp;
	

	fd = open("/dev/sdb", 0);
	if (fd < 0) {
		fprintf(stderr, "cannot open\n");
		perror("open");
		exit(1);
	}

	printf("Sample, V1, File, Skip, Digital\n");

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
			if (rp->w1 == 0)
				goto done;
			v1 = (rp->w1 >> 4) & 0xfff;
			d = (rp->w1 >> 3) & 1;
			skip = (rp->w1 >> 2) & 1;
			f = (rp->w1) & 3;
			printf("%d,%d,%d,%d,%d\n",
				counter, v1, f, skip, d);
			rp++;
			counter++;
		}
	}
    done:;
    	fprintf(stderr, "%d records found.\n", counter);
}
