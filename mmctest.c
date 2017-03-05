/*
 * First test of driving the MMC from a PIC.
 * This program writes a block to the MMC card.
 *
 * Modified to use multiblock writes.
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
							// an error status was returned on initialization
#define	ABORT_INIT_ERROR	3
#define	ABORT_READ_TO	4	// timeout on reading status  (8 byte times)
#define	ABORT_LONG_TO	5	// never came ready on a busy response wait
#define	ABORT_WR_BLK	6	// didn't get a zero back from the write block command
							// didn't get back a success token after a write block data phase.
#define	ABORT_WRITE_FAILED	7
#define	ABORT_APP_CMD	8	// non-zero status on APP_CMD send
							// didn't get back a success at the end of multiple block write
#define	ABORT_WRITE_MULTIPLE_FAILED	9
#define	ABORT_WR_ERASE	10	// failed to set the erase block count
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
	T1CON = 0b.1011.0001;	// Enable timer 1 as an instruction cycle counter, prescaled 1:8
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
	clearRAM();
}

/*
 * Main Buffer.
 * These data structures and routines
 * define the buffering of data between
 * the data sources (A/D) and the data sink (MMC)
 */

// NOTE: A record must be a power of 2 in size or things break.

#define	RECORDSIZE	8
#define	N_BUFFERS	8

uns16 r_seqn[N_BUFFERS];	// Sequence number
uns8  r_stat[N_BUFFERS];	// Status bits
uns16 r_data1[N_BUFFERS];	// Data on channel 1
uns16 r_data2[N_BUFFERS];	// Data on channel 2

unsigned int to_fill;
unsigned int to_empty;
int addr_low:16, addr_high:16;	// disk address to write to next.
unsigned int seqn:16;			// record sequence number generator

#define	BLOCKSIZE			4096	// multiple of a sector, can be tuned.
#define	SECTORSIZE			512		// constant of the universe.  Never change.
#define	SECTORS_PER_BLOCK	(BLOCKSIZE / SECTORSIZE)
#define	RECORDS_PER_SECTOR	(SECTORSIZE / RECORDSIZE)

#ifdef UNUSED
/*
 * This routine is not used because RAM is cleared
 * by the PIC init routine.
 */
void
buffer_init()
{
	to_fill = 0;
	to_empty = 0;
	seqn = 0;
}
#endif

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

#define	mmc_clock(c)	MMC_SCK = c

/*
 * Sends one bit to the MMC chip.  Assumes that the clock is high on entry.
 */
#define mmc_spi_tx_bit(b) MMC_SCK = 0; MMC_SDO = 0; if (b) MMC_SDO = 1; MMC_SCK = 1

/* ASSUMPTIONS:
		MMC_CS = 0
		MMC_SD0 = 1
		MMC_SCK = 1

   NOTE: this function has been manually inlined in the three places it is used
 */
#ifdef notdef
bit
mmc_spi_read_bit()
{
	mmc_clock(0);
	mmc_clock(1);
	return MMC_SDI;
}
#endif

/*
 * Send a byte to the MMC.
 */
void
mmc_spi_tx(unsigned char out)
{
	mmc_spi_tx_bit(out.7);
	mmc_spi_tx_bit(out.6);
	mmc_spi_tx_bit(out.5);
	mmc_spi_tx_bit(out.4);
	mmc_spi_tx_bit(out.3);
	mmc_spi_tx_bit(out.2);
	mmc_spi_tx_bit(out.1);
	mmc_spi_tx_bit(out.0);
	return;
}

void
mmc_spi_tx_16(uns16 data)
{
	mmc_spi_tx(data.low8);
	mmc_spi_tx(data.high8);
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
		MMC_SCK = 0;
		MMC_SCK = 1;
		if (MMC_SDI == 0)
			goto data_found;
	}
	abort(ABORT_READ_TO);	// no response within specified time
  data_found:
	r = 0;
	for (i = 0; i < 7; i++) {
		r = r << 1;
		MMC_SCK = 0;
		MMC_SCK = 1;
		r.0 = MMC_SDI;
	}

	/*
     * Wait for ready.
     * I believe that on a 40MHz clock this waits about half a second.
     */
	for (j = 1; j; j++) {
		MMC_SCK = 0;
		MMC_SCK = 1;
		if (MMC_SDI)
			goto ready_found;
	}
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
	mmc_spi_tx_bit(1);	// not clear these bit times are necessary
	mmc_spi_tx_bit(1);
	mmc_spi_tx(command);
	mmc_spi_tx(AdrH.high8);
	mmc_spi_tx(AdrH.low8);
	mmc_spi_tx(AdrL.high8);
	mmc_spi_tx(AdrL.low8);
	mmc_spi_tx(0x95);		// the only command that requires a CRC is CMD0, which uses 95
	r = mmc_spi_read();	// return the last received character
	return r;
}
/*
 * The Following MMC commands and tokens are used.
 */

// reset
#define	MMC_GO_IDLE_STATE				(0x40 |  0)

// MMC initialize
#define	MMC_SEND_OP_COND				(0x40 |  1)

// MMC write one block
#define	MMC_WRITE_BLOCK					(0x40 | 24)

// MMC write multiple blocks
#define	MMC_WRITE_MULTIPLE_BLOCK		(0x40 | 25)

// following is an SDC command
#define	MMC_APP_CMD						(0x40 | 55)

// number of sectors in multi-block write
#define	SDC_SET_WR_BLOCK_ERASE_COUNT	(0x40 | 23)

// mark the start of a WRITE_BLOCK data section
#define	MMC_WRITE_BLOCK_TOKEN			0xfe
#define	MMC_WRITE_MULTIPLE_BLOCK_TOKEN	0xfc

// mark the end of a multi-block transfer
#define	MMC_STOP_TRAN					0xfd

// OK response to a write.
#define	MMC_WRITE_SUCCESS				0b.0010.1000

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

	// reset disk address to start of the card
	addr_low = 0;
	addr_high = 0;
	
	// start MMC in native mode
	// Spec says, "set CS and DI high, then send
	// at least 74 clock pulses.
	
	MMC_CS = 1;

	// send 10*8=80 clock pulses
	for(i=0; i < 10; i++)
		mmc_spi_tx(0xFF);
	MMC_CS = 0;			// enable MMC
		
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
	MMC_CS = 1;			// disable before the next command
		
	// If the MMC is initialized, then return.  All is well.
	if (i == 0)
		return;

	// If the MMC card never comes ready, then abort.
	if (!count)
		abort(ABORT_READY_TO);

	// We received a non-zero response other than IDLE.  Implies some kind of error.
	abort(ABORT_INIT_ERROR);
}

/*
 * Fill record.
 * This routine generates test data.
 */
unsigned char
fill_record()
{
	unsigned char r;

	r = to_fill;
	r_seqn[r] = seqn;
	seqn++;
	r_stat[r] = 0;
	r_data1[r].low8 = TMR1L;
	r_data1[r].high8 = TMR1H;
	r_data2[r].low8 = to_fill;
	r_data2[r].high8 = 0;

	to_fill++;
	if (to_fill >= N_BUFFERS)
		to_fill = 0;
	return r;
}

/*
 * Write a single multi-block record.
 */
void
mmc_write_blocks()
{
	unsigned char r;
	unsigned char rec;
	unsigned char i;
	unsigned char j;
	
	MMC_CS = 0;	// Enable
	r = mmc_command(MMC_APP_CMD, 0, 0);
	if (r != 0)
		abort2(ABORT_APP_CMD, r);
	r = mmc_command(SDC_SET_WR_BLOCK_ERASE_COUNT, 0, (uns16)SECTORS_PER_BLOCK);
	if (r != 0)
		abort2(ABORT_WR_ERASE, r);
	r = mmc_command(MMC_WRITE_MULTIPLE_BLOCK, addr_high, addr_low);		
	if (r != 0)
		abort2(ABORT_WR_BLK, r);

	// Loop once per sector

	for (i = 0; i < SECTORS_PER_BLOCK; i++) {
		mmc_spi_tx(MMC_WRITE_MULTIPLE_BLOCK_TOKEN);

		// send the actual data.
		for (j = 0; j < RECORDS_PER_SECTOR; j++) {
			// generate the data
			to_fill = 0;
			rec = fill_record();

			/*
			 * The number of bytes sent in this code block must
			 * exactly match RECORDSIZE.
			 */
			mmc_spi_tx_16(r_seqn[rec]);
			mmc_spi_tx(r_stat[rec]);
			mmc_spi_tx(0);				// padding
			mmc_spi_tx_16(r_data1[rec]);
			mmc_spi_tx_16(r_data2[rec]);
		}

		r = mmc_spi_read();
		if (r != MMC_WRITE_SUCCESS)
			abort2(ABORT_WRITE_FAILED, r);

		addr_low += 512;
		if (addr_low == 0)
			addr_high++;
	}

	// stop the multi-block write
	mmc_spi_tx(MMC_STOP_TRAN);
	r = mmc_spi_read();
	if (r != 0)
		abort2(ABORT_WRITE_MULTIPLE_FAILED, r);

	MMC_CS = 1;	// disable
}

void
mmc_run()
{
	char i;

	for (i = 0; i < 32; i++)
		mmc_write_blocks();
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
	nop2();
	nop2();
	ABORT_OUT = 0;
	nop2();
}

void
abort(unsigned char code)
{
	int i;

	MMC_CS = 1;
	for (;;) {
		for (i = 0; i < 24; i++)
			abort_bit(0);
		abort_bit(1);
		abort_bit(code.3);
		abort_bit(code.2);
		abort_bit(code.1);
		abort_bit(code.0);
		abort_bit(1);
	}
}

void
abort_mark()
{
	ABORT_OUT = 1;
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	ABORT_OUT = 0;
}
		
void
abort2(unsigned char code, unsigned char data)
{
	int i;
	bit b;
	
	MMC_CS = 1;
	for (;;) {
		for (i = 0; i < 32; i++)
			abort_bit(0);
		abort_mark();
		abort_bit(code.3);
		abort_bit(code.2);
		abort_bit(code.1);
		abort_bit(code.0);
		abort_mark();
		abort_bit(data.7);
		abort_bit(data.6);
		abort_bit(data.5);
		abort_bit(data.4);
		abort_bit(data.3);
		abort_bit(data.2);
		abort_bit(data.1);
		abort_bit(data.0);
		abort_mark();
	}
}
