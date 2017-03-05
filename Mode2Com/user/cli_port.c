/*
 * This module contains the code to handle
 * the CLI opto-isolator port commands.
 */

#include <p18cxxx.h>
#include "system\typedefs.h"
#include "cli.h"
#include "port.h"

void
cli_port(unsigned char channel)
{
	int value;
	unsigned char v;

	RAW0_TRIS = 1;
	RAW1_TRIS = 1;
	SSPCON1bits.SSPEN = 0;

	if (channel > 1)
		cli_putc('X');
	else {
		cli_putc(' ');

		if (channel == 0)
			value = RAW0;
		else
			value = RAW1;

		puthex(v);
	}

	cli_putc('\n');
	cmd_state = CMD_OK;
}
