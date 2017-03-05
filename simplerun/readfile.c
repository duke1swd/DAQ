/*
 * This program runs on the PC and pulls data off the card.
 */

#define	NSECT	(8*2048)

#include <stdio.h>

char *myname;
char *device = "/cygdrive/e/data";
int seek = 0;

void
record(int sector, unsigned char *rp)
{
	printf("%d, ", sector);
	printf("%x, ", rp[0]);
	printf("%d, ", (rp[1] << 4) | ( (rp[2] >> 4) & 0xf));
	printf("%d, ", (rp[3] << 4) | ( (rp[4] >> 4) & 0xf));
	printf("%d, ", (rp[5] << 4) | ( (rp[6] >> 4) & 0xf));
	printf("%d\n", rp[7]);
}

int
main(int argc, char **argv)
{
	int fd;
	int sector;
	int r;
	int i;
	unsigned char buffer[512];

	myname = *argv;

	fd = open(device, 0);
	if (fd < 0) {
		fprintf(stderr, "%s: cannot open %s for reading.\n",
			myname, device);
		perror("open");
		exit(1);
	}

	for (sector = 0; sector <= NSECT; sector++) {
		r = read(fd, buffer, sizeof buffer);
		if (r != sizeof buffer) {
			fprintf(stderr, "%s: read error on %s, sector %d\n",
				myname, device, sector);
			perror("read");
			exit(1);
		}
		if (sector >= seek)
			for (i = 0; i < sizeof (buffer); i += 8)
				record(sector, buffer + i);
	}
	return 0;
}
