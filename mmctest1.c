/*
 * First test of driving the MMC from a PIC.
 * This program writes a block to the MMC card.
 */

/*
Notes:
 This version assumes a PIC 18F1320 (or 1220).

 Port bit assignments:
	RA0: Error code output
	RA1: unused
	RA2: unused
	RA3: MMC (flash) select

	RA4: MMC (flash) data from MMC to PIC
	RA5: used for in-circuit programming
	RA6: OSC2
	RA7: OSC1

	RB0: unused
	RB1: reserved for future USART use (TX)
	RB2: MMC (flash) data from PIC to MMC
	RB3: MMC (flash) clock

	RB4: reserved for future USART use (RX)
	RB5: used for in-circuit programming
	RB6: used for in-circuit programming
	RB7: used for in-circuit programming
*/

#ifdef ORIGINAL
#pragma bit ABORT_OUT @ PORTA.0	// output for error codes
#pragma bit MMC_CS    @ PORTA.3	// output for chip select CS
#pragma bit MMC_SCK   @ PORTB.3	// clock output
#pragma bit MMC_SDO   @ PORTB.2	// data output (PIC)
#pragma bit MMC_SDI   @ PORTA.4	// data input (PIC)
#else
#pragma bit ABORT_OUT @ PORTA.0	// output for error codes
#pragma bit MMC_CS    @ PORTB.7	// output for chip select CS
#pragma bit MMC_SCK   @ PORTB.3	// clock output
#pragma bit MMC_SDO   @ PORTB.2	// data output (PIC)
#pragma bit MMC_SDI   @ PORTB.6	// data input (PIC)
#endif

/*
 * Configuration (Fuses) setup:
1H: IES0 = 0	Internal/External Switchover disabled
	FSCM = 0	Fail Safe Clock Monitor disabled
	FOSC = 6	HS-PLL clocking
1L	BOR  = 0	Brown-out enable reset disabled
	PWRTEN=1	Power On Timeout disabled.
2H	WDTPS= 0	Watch-dog timer prescaler set to 1
	WDT  = 0	Watch-dog time disabled
3H  MCLRE= 0	RA5 inut enabled, master clear disabled
 */
#pragma config[0] = 0b.0000.0110	//IESO, FSCM off; HS+PLL enabled.
/*
  Note: the I/O ports are named here, but the TRIS registers
        are set up in PIC_setup() and in the A/D routines.
        Any changes to pin assignments must be reflected there too.
 */

// ABORT CODES
#define	ABORT_IDLE_TO	1	// timeout waiting for enter idle status
#define	ABORT_READY_TO	2	// timeout on waiting for initialization complete
#define	ABORT_INIT_ERROR 3	// an error status was returned on initialization
#define	ABORT_READ_TO	4	// timeout on reading status  (8 byte times)
#define	ABORT_LONG_TO	5	// never came ready on a busy response wait
#define	ABORT_SUCCESS	0xff// SUCCESS!

#include "int18xxx.h"

/*
 * Forward references
 */
void PIC_setup();
void mmc_init();
void mmc_run();
void abort(unsigned char code);
void abort2(unsigned char code, unsigned char data);

void main()
{
	PIC_setup();
	mmc_init();
	mmc_run();
}

/*
 * This routine does basic chip setup.
 */
void
PIC_setup()
{
	ADCON0 = 0;		/* Turn off on-chip A/D converter.  */
	ADCON1 = 0x7f;	/* All inputs are digital, not analog */
#ifdef ORIGINAL
	TRISA = 0b.0001.0000;	/* RA4 is input from MMC */
	TRISB = 0b.0000.0000;	/* All outputs */
#else
	TRISA = 0b.0000.0000;	/* All outputs */
	TRISB = 0b.0100.0000;	/* RB6 is input from MMC */
#endif
}

/*
 * Main Buffer.
 * These data structures and routines
 * define the buffering of data between
 * the data sources (A/D) and the data sink (MMC)
 */
struct record_s {
	unsigned int sequence  : 16;
	unsigned int status    :  8;
	unsigned int pad       :  8;
	unsigned int data1     : 16;
	unsigned int data2     : 16;
};

// this expression should evaluate to 16
#define N_BUFFERS	(64 / sizeof (struct record_s))

struct record_s buffers[N_BUFFERS];

void
buffer_init()
{
}

/*
 * MMC card control
 * The design for this code taken from two sources.
 * First:
		http://www.captain.at/electronics/pic-mmc/
 *    which also gives credit to:
		http://home.wtal.de/Mischka/MMC/
 * Second:
		http://elm-chan.org/docs/mmc/mmc_e.html
 */

/*
 * For now we require 32 instruction times between clock phase transitions.
 * Overhead of using this routine is at least 4 instruction times.
 */
#define	MMC_CLOCK	(32-4)
#define mmc_clock(c)	while (TMR2 < MMC_CLOCK) ; TMR2 = 0; MMC_SCK = c

/*
 * Sends one bit to the MMC chip.  Assumes that the clock is high on entry.
 */
void
mmc_spi_tx_bit(bit b)
{
	mmc_clock(0);		// falling edge of clock
	nop2();
	MMC_SDO = b;		// send the bit
	mmc_clock(1);		// Rising edge of the clock, latches the sent data.
}

/* ASSUMPTIONS:
		MMC_CS = 0
		MMC_SD0 = 1
		MMC_SCK = 1
 */
bit
mmc_spi_read_bit()
{
	mmc_clock(0);
	mmc_clock(1);
	nop();
	return MMC_SDI;
}

/*
 * Send a byte to the MMC.
 */
void
mmc_spi_tx(unsigned char out)
{
	char i;

	for (i = 0; i < 8; i++) {
		mmc_spi_tx_bit(out.7);
		out = out<<1;     // shift output byte for next loop
	}
	return;
}

/*
 * Wait for a response from the MMC card.
 * Read it and send it back to the caller.
 * ASSUMPTIONS:
		MMC_CS = 0
		MMC_SD0 = 1
		MMC_SCK = 1
 */
#define	MAX_READ_BIT_TIMES	64
unsigned char
mmc_spi_read()
{
	bit b;
	unsigned char r;
	int i;
	int j:16;

	for (i = 0; i < MAX_READ_BIT_TIMES; i++) {
		b = mmc_spi_read_bit();
		if (b == 0)
			goto data_found;
	}
	abort(ABORT_READ_TO);	// no response within specified time
  data_found:
	r = 0;
	for (i = 0; i < 7; i++) {
		r = r << 1;
		r.0 = mmc_spi_read_bit();
	}

	/*
     * Wait for ready.
     * I believe that on a 40MHz clock this waits about half a second.
     */
	for (j = 1; j; j++)
		if (mmc_spi_read_bit() == 1)
			goto ready_found;
	abort(ABORT_LONG_TO);	// never came ready on read of response to a command
  ready_found:
	mmc_spi_tx(0xff);			// I'm told it needs 8 more clocks
	return r;
}

/*
 * Send a command block to the MMC card.
 */
unsigned char
mmc_command(unsigned char command, uns16 AdrH, uns16 AdrL)
{
	unsigned char r;
	MMC_CS=0;			// enable MMC
	mmc_spi_tx_bit(1);	// not clear these bit times are necessary
	mmc_spi_tx_bit(1);
	mmc_spi_tx(command);
	mmc_spi_tx(AdrH.high8);
	mmc_spi_tx(AdrH.low8);
	mmc_spi_tx(AdrL.high8);
	mmc_spi_tx(AdrL.low8);
	mmc_spi_tx(0x95);		// the only command that requires a CRC is CMD0, which uses 95
	r = mmc_spi_read();	// return the last received character
	MMC_CS = 0;
	return r;
}
/*
 * The Following MMC commands are used.
 */
#define	MMC_GO_IDLE_STATE	(0x40 |  0)	// reset
#define	MMC_SEND_OP_COND	(0x40 |  1)	// MMC initialize

/*
 * MMC R1 responses
 */
#define MMC_R1_IDLE		0b.0000.0001

/*
 * Init routine.
 * Loops for ever on missing card.
 * Calls abort() on errors.
 */
void
mmc_init()
{
	char i, c;
	int count : 16;

	// Start timer 2, used to control clocking of the MMC card.
	T2CON = 0b.0000.0100;	// timer on, prescale off

	// start MMC in native mode
	// Spec says, "set CS and DI high, then send
	// at least 74 clock pulses.
	
	MMC_CS = 1;

	// send 10*8=80 clock pulses
	for(i=0; i < 10; i++)
		mmc_spi_tx(0xFF);

	// Reset the MMC and force into SPI mode
	for (c = 0; c < 50; c++) {
		i = mmc_command(MMC_GO_IDLE_STATE, 0, 0);
		if (i == MMC_R1_IDLE)
			goto idling;
	}
	abort2(ABORT_IDLE_TO, i);
  idling:;

	// Initialize the MMC card.
	// Spec says, send the SEND_OP_COND command
	// over and over again until the IDLE mode bit is no longer set in the response.
	for (count = 1; count; count++) {
		i = mmc_command(MMC_SEND_OP_COND, 0, 0);
		if (i != MMC_R1_IDLE)
			break;
	}

	// If the MMC is initialized, then return.  All is well.
	if (i == 0)
		return;

	// If the MMC card never comes ready, then abort.
	if (!count)
		abort(ABORT_READY_TO);

	// We received a non-zero response other than IDLE.  Implies some kind of error.
	abort(ABORT_INIT_ERROR);
}

void
mmc_run()
{
	abort(ABORT_SUCCESS);
}	

/*
 * This routine sends an error code out on a pin where
 * we can see it with an oscilloscope.
 * The code is a 1 followed by 3 bits and then a 1 bit.
 */
void
abort_bit(bit b)
{
	ABORT_OUT = b;
	nop();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	ABORT_OUT = 0;
	nop();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
}

void
abort(unsigned char code)
{
	int i;

	for (;;) {
		for (i = 0; i < 24; i++)
			abort_bit(0);
		abort_bit(1);
		//abort_bit(code.3);
		abort_bit(code.2);
		abort_bit(code.1);
		abort_bit(code.0);
		abort_bit(1);
	}
}

void
abort2(unsigned char code, unsigned char data)
{
	int i;
	bit b;
	
	for (;;) {
		for (i = 0; i < 32; i++)
			abort_bit(0);
		abort_bit(1);
		abort_bit(code.2);
		abort_bit(code.1);
		abort_bit(code.0);
		abort_bit(1);
		abort_bit(0);
		abort_bit(data.7);
		abort_bit(data.6);
		abort_bit(data.5);
		abort_bit(data.4);
		abort_bit(data.3);
		abort_bit(data.2);
		abort_bit(data.1);
		abort_bit(data.0);
		ABORT_OUT = 1;
		nop2();
		nop2();
		nop2();
		nop2();
		nop2();
		nop2();
		ABORT_OUT = 0;
	}
}
