#include <stdio.h>

int
main(int argc, char **argv)
{
	int i;
	int fd;
	char namebuf[128];

	for (i = 0; i < 10; i++) {
		sprintf(namebuf, "/dev/sd%c", 'a' + i);
		fd = open(namebuf, 0);
		if (fd >= 0) {
			printf("open of %s succeeded\n", namebuf);
			close(fd);
		}
	}
}
