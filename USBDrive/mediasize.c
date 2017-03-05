/*
 * replicate the SD-SPI.c algorithm for finding the media size.
 */
#include <stdio.h>

char *dev = "/dev/sdb";

#define	N_LOC 3

int loc[N_LOC] = { 0x1C6, 0x1D6, 0x1E6 };

main()
{
	int i, t;
	int j;
	int first, num;
	int fd, ret;

	unsigned char buffer[512];

	fd = open(dev, 0);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s\n", dev);
		perror("open");
		exit(1);
	};

	ret = read(fd, buffer, sizeof buffer);
	if (ret != sizeof buffer) {
		fprintf(stderr, "read failed\n");
		perror("read");
		exit(1);
	}

	for (i = 0; i < N_LOC; i++) {
		t = loc[i];
		printf("Loc = 0x%x\n", t);

		printf("\t");
		for (j = 0; j < 8; j++)
			printf("\t%02x", buffer[t+j]);
		printf("\n");

		first = buffer[t++];
		first |= (buffer[t++] << 8);
		first |= (buffer[t++] << 16);
		first |= (buffer[t++] << 24);
		num = buffer[t++];
		num |= (buffer[t++] << 8);
		num |= (buffer[t++] << 16);
		num |= (buffer[t++] << 24);

		printf("\tFirst = %d\n", first);
		printf("\tNum = %d\n", num);
		printf("\tF+N = %d\n", first + num);
		printf("\n");
	}
	close(fd);
}
