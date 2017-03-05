/*
 * This module contains the code to handle
 * the CLI EPROM commands.
 */

#include <p18cxxx.h>
#include "system\typedefs.h"
#include <eep.h>
#include "cli.h"
#include "eprommap.h"

static byte last_addr;

void
cli_eprom_read(byte addr)
{
	byte v;

	Busy_eep();
	v = Read_b_eep((unsigned int) addr);
	cli_putc(' ');
	puthex(v);
	cli_putc('\n');
	last_addr = addr;
	cmd_state = CMD_OK;
}

void
cli_eprom_write(byte addr, byte value)
{
	Busy_eep();
	Write_b_eep((unsigned int) addr, value);
	cli_putc('O');
	cli_putc('K');
	cli_putc('\n');
	last_addr = 0;
	cmd_state = CMD_OK;
}

void
cli_eprom_next(void)
{
	cli_eprom_read(last_addr+1);
}

static unsigned char msg_index;

/*
 * Notes on command state processing:
 *  When this routine is called for the first time
 *  the cmd_state is 1.  In that case we set the
 *  pointer to the begining of the message.  If we
 *  are unable to fit a message character into the
 *  output buffer we set the command state to 2 and
 *  exit.  The input poll routine will call us back
 *  (with cmd_state == 2) after the output buffer has
 *  drained.  We then resume processing the message.
 *  
 *  When we are done we set the state to CMD_OK.
 *  This tells the input poll routine to look for
 *  the next input line.
 */

void
cli_eprom_msg_report(void)
{
	unsigned char v;

	Busy_eep();

	if (cmd_state == 1)
		msg_index = EPROM_MSG;

	cmd_state = CMD_OK;
	for(;;) {
		v = 0x7f & Read_b_eep(msg_index);
		if (v == 0 || v == 0x7f)
			break;

		if (!cli_putc(v)) {
			cmd_state = 2;
			break;
		}

		msg_index++;
		if (msg_index < EPROM_MSG)
			break;
	}
}

/*
 * Format and print error reports from eprom.
 */
static void
eprom_er_rpt(unsigned char addr)
{
	unsigned char b;

	Busy_eep();
	b = Read_b_eep(addr);
	addr++;
	cli_putc('S');
	cli_putc('0' + (3 & b));
	cli_putc(' ');
	puthex(b >> 2);
	cli_putc(' ');
	Busy_eep();
	b = Read_b_eep(addr);
	addr++;
	puthex(b);
	cli_putc(' ');
	Busy_eep();
	b = Read_b_eep(addr);
	puthex(b);
	cli_putc('\n');
}

void
cli_eprom_errors(byte flag)
{
	unsigned char b1, b2;

	cmd_state = CMD_OK;

	Busy_eep();
	b1 = Read_b_eep(EPROM_ERR1_CODE) & 3;
	b2 = Read_b_eep(EPROM_ERR2_CODE) & 3;
	switch ((b1 << 2) | b2) {
	    case 0x0:	// b1 == b2.
	    case 0x5:	// b1 == b2.
	    case 0xA:	// b1 == b2.
	    case 0xD:	// b1 empty, b2 = 1
	    case 0xE:	// b1 empty, b2 = 2
	    case 0x7:	// b1 = 1, b2 empty
	    case 0xB:	// b1 = 2, b2 empty
	    	// all of the above are illegal states.
	    	cli_putc('E');
		cli_putc('?');
		cli_putc('\n');
	    case 0xF:	// both empty
	    	return;
	    case 0xC:	// b1 empty, b2 = 0
	    case 0x1:	// b1 = 0, b2 = 1
	    case 0x6:	// b1 = 1, b2 = 2
	    case 0x8:	// b1 = 2, b2 = 0
	    	msg_index = EPROM_ERR2_CODE;
		break;
	    case 0x3:	// b1 = 0, b2 empty
	    case 0x2:	// b1 = 0, b2 = 2. 
	    case 0x4:	// b1 = 1, b2 = 0
	    case 0x9:	// b1 = 2, b2 = 1
		msg_index = EPROM_ERR1_CODE;
		break;
	}

	eprom_er_rpt(msg_index);
	msg_index = EPROM_ERR1_CODE + EPROM_ERR2_CODE - msg_index;
	eprom_er_rpt(msg_index);
	
	/*
	 * If the flag is true then we should clear the errors.
	 */
	if (flag) {
		cli_putc('C');
		cli_putc('\n');
		Write_b_eep(EPROM_ERR1_CODE, 0xff);
		Busy_eep();
		Write_b_eep(EPROM_ERR2_CODE, 0xff);
		Busy_eep();
	}
}

/*
 * Set and print directory names.
 */
void
cli_eprom_modes(byte mode, char *name)
{
	unsigned char i;
	char c;
	char dirname[12];

	cmd_state = CMD_OK;
	if (mode < 1 || mode > 8) {
		cli_putc('R');
		cli_putc('\n');
		return;
	}

	/*
	 * Convert mode into an eprom address.
	 */
	mode = (mode - 1) * EPROM_MODE_LEN + EPROM_MODE_1;

	/*
	 * If a name is supplied, set it.
	 */
	if (name) {
		i = make_dirfile(&name, dirname);
		if (i != 0) {
			cli_putc('N');
			if (i == '/')
				cli_putc('/');
			else {
				cli_putc(' ');
				puthex(i);
			}
			cli_putc('\n');
			for (i = 0; i < 11; i++)
				cli_putc(dirname[i]);
			cli_putc('\n');
			return;
		}
		for (i = 0; i < EPROM_MODE_LEN; i++) {
			Busy_eep();
			Write_b_eep((unsigned int)(mode + i), dirname[i]);
		}
	}

	/*
	 * Print the mode's file name.
	 */
	for (i = 0; i < EPROM_MODE_LEN; i++) {
		Busy_eep();
		c = Read_b_eep((unsigned int)(mode + i));
		if (c)
			cli_putc(c);
	}
	cli_putc('\n');
	Busy_eep();
}
