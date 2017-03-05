/*
 * Downsample the output of the read3data program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

char lbuf[128];
int ratio;
char *myname;

static void
input()
{
	fgets(lbuf, sizeof lbuf, stdin);
	if (feof(stdin))
		exit(0);
}

static void
usage()
{
	fprintf(stderr, "Usage: %s [<downsample ratio (%d)>]\n",
		myname, ratio);
	exit(1);
}

static void
copy(int n)
{
	int i;

	for (i = 0; i < n; i ++) {
		input();
		fputs(lbuf, stdout);
	}
}

static void
downsample(int sample)
{
	int i;
	int v0, v1, v2, v3, v4;
	int t0, t1, t2, t3, t4;
	int r;

	v0 = 0;
	v1 = 0;
	v2 = 0; 
	v3 = 0;
	v4 = 0;
	for (i = 0; i < ratio; i++) {
		input();
		r = sscanf(lbuf, "%d,%d,%d,%d,%d", &t0, &t1, &t2, &t3, &t4);
		if (r != 5) {
			fprintf(stderr, "%s: badly formed line in sample %d\n",
				myname, sample);
			exit(1);
		}
		v0 += t0;
		v1 += t1;
		v2 += t2;
		v3 += t3;
		v4 += t4;
	}
	printf("%d,%d,%d,%d,%d,%d\n",
		sample * ratio,
		v0, v1, v2, v3, v4);
}

int
main(int argc, char **argv)
{
	int sample;
	int header_count;
	int state;

	myname = *argv;
	ratio = 2;
	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0)
			usage();
		ratio = atoi(argv[1]);
	} else if (argc > 2)
		usage();

	if (ratio <= 0)
		usage();

	state = 0;
	for (header_count = 0; header_count < 10; header_count++) {
		copy(1);
		if (strlen(lbuf) == 1 || strlen(lbuf) == 2)
			state = 1;
		if (state && strncmp(lbuf, "Channel", strlen("Channel")) == 0)
			goto down_sample;
	}
	fprintf(stderr, "%s: cannot find end of header\n", myname);
	exit(1);

    down_sample:

	for (sample = 0; ; sample++)
		downsample(sample);
}
