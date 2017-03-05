/*
 *  The sequencer can encode messages in the data lines it sends to the DAQ.
 *  This code reads the DAQ output, finds, and decodes the messages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

char *myname;
int verbose;
int debug;
FILE *input;

/*
 * Scanning state machine stuff.
 */

#define	IDLE	0
#define	STAY	1	// stay in this state.
#define	F_ERR	2	// Framing error
#define	OK	3
#define	OUT_0	4
#define	OUT_1	5
#define	OUT_2	6
#define	OUT_3	7

#define ACTION_IDLE	{ IDLE, IDLE, IDLE, IDLE }
#define	NEXT_BIT	ACTION_IDLE, ACTION_IDLE, ACTION_IDLE, ACTION_IDLE

struct state_s {
	unsigned char act0, act1, act2, ac3;
};

// Look for valid STX character (0x02).
struct state_s search_som[] = {
	{ STAY, IDLE, IDLE, IDLE },	// search for activity
	ACTION_IDLE,			// skip a sample	
	{ F_ERR, F_ERR, F_ERR, IDLE},	// Need both lines high at this time.  If so, this is a start bit.
	ACTION_IDLE,			// skip a sample	
	NEXT_BIT,			// idle for 4 samples
	{ IDLE, F_ERR, F_ERR, F_ERR},	// Need 00
	NEXT_BIT,			// idle for 4 samples
	{ IDLE, F_ERR, F_ERR, F_ERR},	// Need 00
	NEXT_BIT,			// idle for 4 samples
	{ IDLE, F_ERR, F_ERR, F_ERR},	// Need 00
	NEXT_BIT,			// idle for 4 samples
	{ F_ERR, F_ERR, IDLE, F_ERR},	// Need 10
	NEXT_BIT,			// idle for 4 samples
	{ OK, F_ERR, F_ERR, F_ERR},	// Need 00 (stop bits).
};

// translate any valid byte
struct state_s search_som[] = {
	{ STAY, IDLE, IDLE, IDLE },	// search for activity
	ACTION_IDLE,			// skip a sample	
	{ F_ERR, F_ERR, F_ERR, IDLE},	// Need both lines high at this time.  If so, this is a start bit.
	ACTION_IDLE,			// skip a sample	
	NEXT_BIT,			// idle for 4 samples
	{ OUT_0, OUT_1, OUT_2, OUT_3},	// any 2 bits
	NEXT_BIT,			// idle for 4 samples
	{ OUT_0, OUT_1, OUT_2, OUT_3},	// any 2 bits
	NEXT_BIT,			// idle for 4 samples
	{ OUT_0, OUT_1, OUT_2, OUT_3},	// any 2 bits
	NEXT_BIT,			// idle for 4 samples
	{ OUT_0, OUT_1, OUT_2, OUT_3},	// any 2 bits
	NEXT_BIT,			// idle for 4 samples
	{ OK, F_ERR, F_ERR, F_ERR},	// Need 00 (stop bits).
};

static void
set_defaults()
{
	verbose = 0;
	debug = 0;
}

static void
usage()
{
	set_defaults();

	fprintf(stderr, "Usage: %s <options> [<input file>]\n",
		myname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-v (set verbose mode)\n");
	fprintf(stderr, "\t-d (increase debugging level)\n");
	exit(1);
}

static void
grok_args(int argc, char **argv)
{
	int c;
	int nargs;
	int errors;

	myname = *argv;
	errors = 0;
	set_defaults();

	while ((c = getopt(argc, argv, "vdh")) != EOF)
	switch(c) {
	    case 'v':
	    	verbose++;
		break;
	    case 'd':
	    	debug++;
		break;
	    case 'h':
	    case '?':
	    default:
	    	usage();
	}

	nargs = argc - optind;
	if (nargs < 0 || nargs > 1) {
		fprintf(stderr, "%s: Must supply at most one file name\n",
			myname);
		errors++;
	}

	if (nargs == 0)
		input = stdin;
	else {
		input = fopen(argv[optind], "r");
		if (input == NULL) {
			fprintf(stderr, "%s: cannot open %s for reading\n",
				myname,
				argv[optind]);
			errors++;
		}
	}

	if (errors)
		usage();
}

int
main(int argc, char **argv)
{
	grok_args(argc, argv);
	doit(input);
	fclose(input);
}
