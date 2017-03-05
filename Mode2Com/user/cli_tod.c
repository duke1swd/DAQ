/*
 * This module contains the code to handle
 * the CLI TOD commands.
 */

#include <p18cxxx.h>
#include "system\typedefs.h"
#include "cli.h"
#include "tod.h"
#include <stdio.h>

static unsigned char can_modify_tod = 0;

/*
 * Called to verify permission to change the time.
 * If you don't have permission, signals and error and
 * returns true.
 */
static unsigned char
check_modify(void)
{
	if (can_modify_tod == 0) {
		cmd_state = CMD_ERR;
		printf("TOD Access\n");
		return 1;
	}
	can_modify_tod = 0;
	return 0;
}

void
cli_tod_cproc(void)
{
	if (can_modify_tod)
		can_modify_tod -= 1;
}

void
cli_tod_report(void)
{
	unsigned char val;

	cli_putc(' ');
	cli_putc('2');
	cli_putc('0');
	val = tod_receive_byte(TOD_YEAR);
	puthex(val);
	cli_putc(',');

	val = tod_receive_byte(TOD_MONTH);
	puthex(val);
	cli_putc(',');

	val = tod_receive_byte(TOD_DATE);
	puthex(val);
	cli_putc(' ');

	val = tod_receive_byte(TOD_HOUR);
	puthex(val);
	cli_putc(':');

	val = tod_receive_byte(TOD_MIN);
	puthex(val);
	cli_putc(':');

	val = tod_receive_byte(TOD_SEC);
	puthex(val);
	cli_putc('\n');
	cmd_state = CMD_OK;
	can_modify_tod = 2;
}

void
cli_tod_seconds(void)
{
	if (check_modify())
		return;
	// Turn on the super-cap trickle charger
	tod_send_byte(TOD_CHARGE, TOD_TCS | TOD_1DIODE | TOD_R2K);
	tod_send_byte(TOD_SEC, 0);
	cmd_state = CMD_OK;
}

void
cli_tod_hours(unsigned char hours, unsigned char minutes)
{
	if (check_modify())
		return;
	tod_send_byte(TOD_HOUR, hours);
	tod_send_byte(TOD_MIN, minutes);
	cmd_state = CMD_OK;
}

void
cli_tod_month(unsigned char month, unsigned char day)
{
	if (check_modify())
		return;
	tod_send_byte(TOD_MONTH, month);
	tod_send_byte(TOD_DATE, day);
	tod_send_byte(TOD_DAY, 1);
	cmd_state = CMD_OK;
}

void
cli_tod_year(unsigned char year)
{
	if (check_modify())
		return;
	tod_send_byte(TOD_YEAR, year);
	cmd_state = CMD_OK;
}
