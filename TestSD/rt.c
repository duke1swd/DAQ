/*
 * Read the card.
 */

#include <stdio.h>

int
main(int argc, char **argv)
{
	int i;
	int count;
	int fd;
	char buffer[512];


	fd = open("/dev/sdb", 0);
	if (fd < 0) {
		fprintf(stderr, "cannot open /dev/sdb for reading\n");
		perror("open");
		exit(1);
	}

	for (count = 0; ; count++) {
		i = read(fd, buffer, sizeof buffer);
		if (i < 0) {
			fprintf(stderr, "read error on sector %d\n", count);
			perror("read");
			exit(1);
		} else if (i == 0) {
			printf("EOF after %d sectors\n", count);
			exit(0);
		}
		if (count >= 128)
		    for(i = 0; i < sizeof buffer; i++)
			if (buffer[i] != 17) {
				printf("EOD after %d sectors\n", count-128);
				exit(0);
			}
	}
}
