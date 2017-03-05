#include <stdio.h>
#include <ctype.h>

void
usage()
{
	fprintf(stderr, "hextodec <hex>\n");
	exit(1);
}

int
hcvt(char *in)
{
	char *p;
	int acc = 0;

	for (p = in; *p; p++)
		if (isupper(*p))
			*p = tolower(*p);

	if (in[0] ==  '0' && in[1] == 'x')
		in += 2;

	for (p = in; *p; p++) {
		acc *= 16;
		if (isdigit(*p))
			acc += *p - '0';
		if (*p >= 'a' && *p <= 'f')
			acc += *p - 'a' + 10;
	}

	return acc;
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		usage();

	printf("%d\n", hcvt(*++argv));
	return 0;
}
