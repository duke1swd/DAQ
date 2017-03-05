/*
 * This module contains the code to handle
 * the CLI clibrate commands.
 */

#include <p18cxxx.h>
#include "system\typedefs.h"
#include "cli.h"
#include "tod.h"
#include <eep.h>
#include "eprommap.h"

static void
cli_nyi(void)
{
	cli_putc('C');
	cli_putc(':');
	cli_putc(' ');
	cli_putc('N');
	cli_putc('Y');
	cli_putc('I');
}

static void
cli_finish(void)
{
	cli_putc('\n');
	cmd_state = CMD_OK;
}

/*
 * Set the calibration bias.
 */
void
cli_calibrate_set(byte bias)
{
	cli_nyi();
	cli_finish();
}

/*
 * Print out the calibration bias.
 */
void
cli_calibrate_echo()
{
	cli_nyi();
	cli_finish();
}

/*
 * Calculate the correct calibration bias using the TOD crystal
 * as the time base.
 */
void
cli_calibrate_tod()
{
	cli_nyi();
	cli_finish();
}

/*
 * Calculate the correct calibration bias assuming a precision
 * 1 Hz signal on RAW0 (as from a GPS).
 */
void
cli_calibrate_GPS()
{
	cli_nyi();
	cli_finish();
}
