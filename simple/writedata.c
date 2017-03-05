#include <stdio.h>

#define	BUFSIZE	(64*1024)

int
main(int argc, char **argv)
{
	int fd;
	int r;
	unsigned char buffer[BUFSIZE];

	fd = open("/dev/sdb", 1);
	if (fd < 0) {
		fprintf(stderr, "cannot open\n");
		perror("open");
		exit(1);
	}
	fprintf(stderr, "open for writing succeeded.\n");
	fprintf(stderr, "sleeping for 15 seconds.  THINK!\n");
	sleep(15);


	for (r = 0; r < sizeof buffer; r++)
		buffer[r] = 0x0;

	r = write(fd, buffer, sizeof buffer);
fprintf(stderr, "r=%d\n", r);

	if (r < 0) {
		fprintf(stderr, "cannot write\n");
		perror("write");
		exit(1);
	} else if (r == 0) {
		fprintf(stderr, "EOF\n");
		exit(1);
	} else if (r < sizeof buffer)
		fprintf(stderr, "short write: %d\n", r);
}
