/*
 * This file contains the guts of the CLI.
 *
 * The USB system calls into inputPoll() fairly often.
 * inputPoll() drives echoing, command parseing, and for
 * most commands the execution of the command itself.
 */

#include <p18cxxx.h>
#include <stdio.h>
#include "system\typedefs.h"
#include "system\usb\usb.h"
#include "user.h"
#include "cli.h"

/*
 * DEBUGGING CONTROL
 */
#define	debug	0


#pragma udata
/*
 * Command state
 *  0: no command to execute.
 *  1: command buffer is ready to run.
 *  n: some command or command phase is ready to run.
 *
 * When the command state is > 0 the inputPoll routine waits
 *  for the output ring buffer to drain then calls the
 *  appropriate routine (see switch stmt in inputPoll).
 */
char cmd_state;
char first_time;

/*
 * Raw input buffer.
 *
 * This buffer hold raw characters from the USB input line.
 */
#define IB_LEN	64	// set to match CDC_BULK_OUT_EP_SIZE (could be smaller)
char input_buffer[IB_LEN];
unsigned char i_n;	// number of characters in the input buffer.
unsigned char i_p;	// index of the next char to process.

/*
 * Cannonical input buffer.
 * 
 * Raw characters are processed into this buffer to build
 * the command line.
 */
#define	CANNON_LEN 64
static byte cannon_buffer[CANNON_LEN];
static unsigned char n_in_cb;

/*
 * Output buffer.
 *
 * This is a ring buffer of stuff queued to send back to the host.
 */

#define	OUTPUT_RING_SIZE 128
#pragma udata outputring
static byte ring[OUTPUT_RING_SIZE];
#pragma udata
static unsigned char ri;
static unsigned char ro;
static unsigned char n_in_ring;


#pragma code

/*
 * This section of the CLI code contains the output primitives.
 *
 * NOTE: these routines may not be called from interrupt level.
 */

void
inputInit(void)
{
	ri = 0;
	ro = 0;
	n_in_ring = 0;

	i_n = 0;

	n_in_cb = 0;

	cmd_state = 0;
	stdout = _H_USER;
	first_time = 1;
}

/*
 * Send a character out.
 * Returns true if the character was queued to send, false if flow-control
 * prevented sending the character.
 */
char
cli_raw_putc(char c)
{
	if (n_in_ring >= OUTPUT_RING_SIZE)
		return 0;
	ring[ri] = c;
	ri++;
	if (ri >= OUTPUT_RING_SIZE)
		ri = 0;
	n_in_ring++;
	return 1;
}

/*
 * Cannonical output routine.
 * Expands '\n' to CRLF.
 */
char
cli_putc(char c)
{
	if (c == '\n')
		if (!cli_raw_putc('\r'))
			return 0;
	return cli_raw_putc(c);
}

int
_user_putc(char c)
{
	cli_putc(c);
	return c;
}

/*
 * This routine is passed a raw input character.
 * It echos the input character.
 * It builds the input buffer, processing character and line kill.
 * When the buffer is complete it sets the command state to 1;
 *
 * This routine should only be called in command state 0.
 */
#define	BACKSPACE	8	// control-H
#define	LINEKILL	21	// control-U
void
cli_cannon(char c)
{
	c &= 0x7f;

	switch(c) {
	case ' ':
	case '\n':
	case '\t':
		break;	// whitespace is ignored.

	case BACKSPACE:
		if (n_in_cb) {
			n_in_cb -= 1;
			(void)cli_putc(BACKSPACE);
			(void)cli_putc(' ');
			(void)cli_putc(BACKSPACE);
		}
		break;

	default:
		// the '-1' is to allow room for a terminator to be added.
		if (n_in_cb < CANNON_LEN-1) {
			(void)cli_putc(c);
			cannon_buffer[n_in_cb] = c;
			n_in_cb += 1;
			break;
		}
		// if the input buffer is full, kill the entire line.
			
	case LINEKILL:
		if (n_in_cb) {
			n_in_cb = 0;
			(void)cli_putc('@');
			(void)cli_putc('\n');
		}
		break;

	case '\r':
		(void)cli_putc('\n');
		if (n_in_cb)
			cmd_state = 1;

		break;
		
	}
}

/*
 * Syntax Error.
 * All error codes are a single letter.
 *
 * E: unknown command
 * C: command echo (debug mode)
 * 1: cannot parse first argument
 * 2: cannot parse second argument
 */
void
cmdError(char c)
{
	char i;

	/*
	 * Echo the command.
	 */
	cli_putc(c);
	cli_putc(':');
	cli_putc(' ');

	for (i = 0; i < n_in_cb; i++)
		cli_putc(cannon_buffer[i]);

	cli_putc('\n');
}

/*
 * Ascii to Binary
 */
byte
hex(byte *p)
{
	byte v;
	byte n;
	byte c;

	v = 0;
	for (;;) {
		c = *p++;
		if (c >= '0' && c <= '9')
			n = c - '0';
		else if (c >= 'a' && c <= 'f')
			n = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			n = c - 'A' + 10;
		else
			break;
		v = (v << 4) | n;
	}
	return v;
}

/*
 * Parse a signed decimal number.
 */
byte
signed_decimal(byte *p)
{
	byte v;
	byte n;
	byte c;
	byte s;

	v = 0;

	/* Pick up the sign, if any. */
	s = 1;
	c = *p;
	if (c == '-') {
		s = -1;
		p++;
	} else if (c == '+')
		p++;

	for (;;) {
		c = *p++;
		if (c >= '0' && c <= '9')
			n = c - '0';
		else
			break;
		v = v * 10 + n;
	}
	return v * s;
}

void
putnib(byte n)
{
	n &= 0xf;
	if (n < 10)
		cli_putc('0' + n);
	else
		cli_putc('a' + n - 10);
}

void
puthex(byte b)
{
	putnib(b >> 4);
	putnib(b);
}

/*
 * This routine parses the cannonicalized input line.
 */
void
cmdParse(void)
{
	byte *p;
	byte *arg2_ptr;
	byte cmd;
	byte arg1;
	byte arg2;
	byte arg2_set;

	if (debug)
		cmdError('C');

	// add a terminator.  helps parsing.
	cannon_buffer[n_in_cb] = ',';

	p = cannon_buffer;
	cmd = *p++;
	arg1 = 0;
	arg2 = 0;
	arg2_set = 0;
	if (n_in_cb > 2) {
		if (*p++ == ',')
			arg1 = hex(p);
		while (*p++ != ',') ;
		if (p - cannon_buffer < n_in_cb) {
			arg2_set = 1;
			arg2 = hex(p);
			arg2_ptr = p;
		}
	}
	
	/*
	 * Tell the TOD code that another command has been processed.
	 * Used to revoke modify permissions.
	 */
	cli_tod_cproc();

	switch(cmd) {
	case 'w':
		cli_eprom_write(arg1, arg2);
		break;

	case 'R':
		cli_eprom_msg_report();
		break;

	case 'E':
		cli_eprom_errors(arg1);
		break;

	case 'M':
		if (arg2_set == 0)
			p = (byte *)0;
		else
			cannon_buffer[n_in_cb] = 0;
		cli_eprom_modes(arg1, (char *)p);
		break;

	case 'r':
		cli_eprom_read(arg1);
		break;

	case '+':
		cli_eprom_next();
		break;

	case '*':
		cli_tod_report();
		break;

	case 's':
		cli_tod_seconds();
		break;

	case 'h':
		cli_tod_hours(arg1, arg2);
		break;

	case 'm':
		cli_tod_month(arg1, arg2);
		break;

	case 'y':
		cli_tod_year(arg1);
		break;

	case 'a':
		cli_adc(arg1);
		break;

	case 'b':
		cli_adc_battery();
		break;

	case 'p':
		cli_port(arg1);
		break;

	case 'e':
		cli_putc(' ');
		puthex(arg1);
		cli_putc(' ');
		puthex(arg2);
		cli_putc('\n');
		cmd_state = CMD_OK;
		break;

	case 'c':
		cli_sensors();
		break;

	case 'C':
		switch (arg1) {
		case 0:
			if (arg2_set)
				cli_calibrate_set(signed_decimal(arg2_ptr));
			else
				cli_calibrate_echo();
			break;
		case 1:
			cli_calibrate_tod();
			break;
		case 2:
			cli_calibrate_GPS();
			break;
		default:
			cmd_state = CMD_ERR;
			break;
		}
		break;

	case 'H':
		printf("Hello World\n");
		cmd_state = CMD_OK;
		break;
			

	default:
		cmdError('E');
		cmd_state = 0;
		n_in_cb = 0;
		break;
	}
}

/*
 * This routine is called frequently from the USB framework.
 * It drives the entire CLI.
 * This routine must return pretty quickly without blocking.
 */
void
inputPoll(void)
{
	byte *p;

	/*
	 * do we have something to send, and is the
	 * transmitter ready for us to send it?
	 * (QQQ NEED TO OPTIMIZE FOR BULK OUTPUT QQQ)
	 */
	while (n_in_ring > 0 && mUSBUSARTIsTxTrfReady()) {
		p = (byte *)ring + ro;
		mUSBUSARTTxRam(p, 1);
		ro++;
		if (ro >= OUTPUT_RING_SIZE)
			ro = 0;
		n_in_ring -= 1;
	}

	/*
	 * Are we ready for more input && is there more to be had?
	 */
	if (i_n == 0 && getsUSBUSART(input_buffer, IB_LEN)) {
		i_n = mCDCGetRxLength();
		i_p = 0;
	}

	/*
	 * Say Hello!
	 */
	if (first_time && i_n > 0) {
		first_time = 0;
		printf("HybridSky.com DAQ 1.0\n> ");
		i_n = 0;  // discard the first line of input.
	}


	/*
	 * Cannonicalize until either:
	 *  We need to run a command
	 *  There is no room in the output buffer.
	 *  There are no raw characters left.
	 */
	while (cmd_state == 0 && n_in_ring + 5 < OUTPUT_RING_SIZE && i_n > 0) {
		cli_cannon(input_buffer[i_p]);
		i_p++;
		i_n--;
	}

	/*
	 * If there is queued output or no command to run, we are done.
	 *
	 * This logic ensures that we do not enter the command routine
	 * without a completely empty output ring.
	 */
	if (cmd_state == 0 || n_in_ring > 0)
		return;
	
	/*
	 * Run a command.
	 */
	switch(cmd_state) {
		case CMD_ERR:
			cli_putc('E');
			cli_putc('R');
			cli_putc('R');
			cli_putc(' ');
		case CMD_OK:
			cli_putc('>');
			cli_putc(' ');
			cmd_state = 0;
			n_in_cb = 0;
			break;
		default:
			cmdParse();
			break;
	}
}

/*
 * returns true if there is input pending.
 */
unsigned char
cli_input_pending(unsigned char flag)
{
	if (i_n > 0) {
		if (flag)
			i_n = 0;
		return 1;
	}
	return 0;
}
