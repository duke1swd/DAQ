#include <stdio.h>
#define	IDLE

#define	BUFSIZE	(16*1024)

int
main(int argc, char **argv)
{
	int fd;
	int r;
	int i;
	int d, t;
#ifdef IDLE
	int sum_idle;
	int run_len;
	int cur_run;
#endif
	char buffer[BUFSIZE];
	struct record_s {
		unsigned short w1;
		unsigned short w2;
	} *rp;
	int v1, v2, counter, f, b;

	fd = open("/dev/sdb", 0);
	if (fd < 0) {
		fprintf(stderr, "cannot open\n");
		perror("open");
		exit(1);
	}
#ifdef V1
	printf("Sample, V1, V2, File, Skip\n");
#endif
#ifdef V2
	printf("Sample, V1, V2, File, Skip, Digital, Mark\n");
#endif
#ifdef IDLE
	printf("Sample, V1, CNT, File, Skip\n");
	sum_idle = 0;
	cur_run = 0;
	run_len = 0;
#define V1
#endif

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
			if (rp->w1 == 0 && rp->w2 == 0)
				goto done;
			v1 = (rp->w1 >> 4) & 0xfff;
			f = (rp->w1 >> 1) & 7;
			b = rp->w1 & 1;
#ifdef V1
			v2 = rp->w2 & 0xfff;
			printf("%6d, %4d, %4d, %1d, %1d\n",
				counter, v1,v2, f, b);
#endif
#ifdef IDLE
			sum_idle += v2;
			if (v2 == 0)
				cur_run++;
			else {
				if (cur_run > run_len)
					run_len = cur_run;
				cur_run = 0;
			}
#endif
#ifdef V2
			v2 = (rp->w2 >> 4) & 0xfff;
			d = (rp->w2 >> 3) & 1;
			t = (rp->w2) & 1; // should always be one
			printf("%6d, %4d, %4d, %1d, %1d, %1d, %1d\n",
				counter, v1,v2, f, b, d, t);
#endif
			rp++;
			counter++;
		}
	}
    done:;
    	fprintf(stderr, "%d records found.\n", counter);
#ifdef IDLE
	fprintf(stderr, "max run of zeros was %d\n", run_len);
	fprintf(stderr, "av idle time was %d\n", sum_idle / counter);
#endif
}
