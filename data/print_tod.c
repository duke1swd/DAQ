/*
 * This routine formats and prints the time stamp taken
 * from the TOD chip in the DAQ 1.0 system.
 */

#include <stdio.h>

char *monthname[] = {
	"Jan",
	"Feb",
	"Mar",
	"April",
	"May",
	"June",
	"July",
	"Aug",
	"Sept",
	"Oct",
	"Nov",
	"Dec",
};

void
print_tod(unsigned char *p)
{
	int v;
	int sec, min, hour, date, month, year;

	sec = (*p & 0xf);
	sec += (((*p) >> 4) & 0x7) * 10;
	p++;

	min = (*p & 0xf);
	min += (((*p) >> 4) & 0x7) * 10;
	p++;

	hour = (*p & 0xf);
	v = ((*p) >> 4) & 0xf;
	if (v & 1)
		hour += 10;
	if (v & 8) {
		// twelve hour mode
		if (v & 2)
			hour += 12;
	} else {
		// 24 hour mode
		if (v & 2)
			hour += 20;
	}

	p++;

	date = (*p & 0xf);
	date += (((*p) >> 4) & 0x3) * 10;
	p++;

	month = (*p & 0xf);
	month += (((*p) >> 4) & 0x1) * 10;
	p++;
	
	p++;

	year = (*p & 0xf);
	year += (((*p) >> 4) & 0xf) * 10;

	printf("%02d:%02d:%02d %s %d, 20%02d\n",
		hour, min, sec,
		monthname[month-1], date, year);
}

