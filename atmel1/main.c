/*
 * First test of talking to the Atmel Dataflash card.
 */

/*
 * The DAQ 1.0 board is a PIC 18F2550.
 * The chip is running on a 12 MHz crystal with FOSC/4 = 12MHz.
 */

#include "int18xxx.h"

/*
 * Resource usage:
 *
 * This module uses the SPI hardware to talk to the Atmel card.
 *
 * The LEDs are used to blink status and errors.
 */


/*
 * Define the I/O infrastructure.
 */
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2
#pragma	bit DATA_SCK		@ PORTB.1	// data flash clock
#pragma	bit DATA_SDI		@ PORTB.0	// data flash data to PIC
#pragma	bit DATA_SDO		@ PORTC.7	// data flash data from PIC
#pragma	bit DATA_CS		@ PORTC.6	// data flash chip select
#define	TRISA_SET		0b.0000.0000	// all are outputs.
#define	TRISB_SET		0b.0000.0001	// one input from D-F
#define	TRISC_SET		0b.0000.0000	// all are outputs.
#define	ADCON1_SET		0b.0000.1111	/* No analog inputs */

/*
 * Atmel data flash commands
 */
#define	DF_BUF1_TO_MEM	0x88
#define	DF_MEM_TO_BUF1	0x53
#define	DF_PAGE_ERASE	0x81
#define	DF_READ_BUF1	0xD4	// high speed serial read opcode
#define	DF_STATUS	0xD7
#define	DF_WRITE_BUF1	0x84

#define	BLOCK_SIZE	512

#define	BASE_LBA	((uns24) 0x1000)

#define	BLOCKS		16	// number of blocks to test

const unsigned char patterns[BLOCKS] = {0x07,
				  0xE0,
				  0x02,
				  0xC0,

				  0x81,
				  0x7E,
				  0xC3,
				  0x3C,

				  0xF0,
				  0x78,
				  0x3C,
				  0x1E,

				  0x0F,
				  0x07,
				  0x03,
				  0x01,
				};

/*
 * Proper program location for USB boot.
 */
#pragma resetVector 0x800

/*
 * Main buffer.
 */
unsigned char buffer[512] @ 0x100;

/*
 * Set this lba before calling df_read() or df_write().
 */
uns24 df_lba;

void
waitms(void)
{
	unsigned char i, j;

	for (j = 7; j; j--)
		for (i = 1; i; i++)
			nop2();
}

#define	BLINK_NO_EPROM
#define	BLINK_BYTE
#include "blink.c"

void
PIC_setup(void)
{
	clearRAM();

	/*
	 * Turn off stuff we don't use.
	 */
	ADCON0 = 0;		/* Turn off on-chip A/D converter.  */
	ADCON1 = ADCON1_SET;	/* Configure no analog ports. */
	CCP1CON = 0;		/* Make sure the ECCP is turned off */

	/*
	 * Set up the I/O ports.
	 */
	TRISA = TRISA_SET;
	TRISB = TRISB_SET;
	TRISC = TRISC_SET;

	PORTA = 0;		/* Clear the output latches */
	PORTB = 0;
	PORTC = 0;

	/*
	 * Set up the SPI hardware.
	 * The TRIS registers must already be set up.
	 */
	DATA_CS = 1;	// Deselect the dataflash chip.
	 	// b7: input data sample middle of data output time
		// b6: transmit on clk active to idle
		// b5-0: read only
	SSPSTAT = 0b.0100.0000;
		// b7: clear collision
		// b6: unused in master mode
		// b5: enable SPI
		// b4: clock idle is low
		// b3-0: master mode, fast (FOSC/4)
	SSPCON1 = 0b.0010.0000;
}

void
send_byte(unsigned char b)
{
	unsigned char clear;

	clear = SSPBUF;
	SSPIF = 0;	// clear the interrupt request flag.
	SSPBUF = b;
	while (!SSPIF) ; // Wait for TX complete.
}

unsigned char data_status;

void
get_status(void)
{
	DATA_CS = 0;	// select the card.
	send_byte(DF_STATUS);
	send_byte(0xFF);
	data_status= SSPBUF;
	DATA_CS = 1;
}

uns24 address;

void
send_cmd_with_address(unsigned char cmd)
{
	DATA_CS = 0;	
	send_byte(cmd);
	send_byte(address.high8);
	send_byte(address.mid8);
	send_byte(address.low8);
	DATA_CS = 1;
}

void
wait_for_not_busy(void)
{
/*xxx*/unsigned char b; b = 0;

	do {
/*xxx*/if (b != 0xff) b++;
		nop2();
		get_status();
	} while (!data_status.7);
/*xxx*/ //blink_byte(b);
}

void
read_to_buf1(void)
{
	/*
	 * Discard the LSB of df_lba.
	 * Put the rest into the top 13 bits of the address register.
	 */
	address = df_lba << 10;

	/*
	 * Start the read.
	 */
	send_cmd_with_address(DF_MEM_TO_BUF1);

	/*
	 * Loop until data is in the buffer.
	 */
	wait_for_not_busy();
}

/*
 * Send a three byte address to the data flash.
 * The address we send is either 00 00 00 for even LBAs
 * or 00 02 00 for odd LBAs.
 */
void
send_buffer_offset(void)
{
	send_byte(0);		// address byte 1
	if (df_lba.0)
		send_byte(2);	// address byte 2
	else
		send_byte(0);	// address byte 2
	send_byte(0);		// address byte 3
}

/*
 * Very simple read routine.
 * Reads 512 bytes at the indicated LBA.
 *
 * This routine pulls the entire page into buffer 1, then
 * lifts the selected bytes out of buffer 1.
 */
void
df_read(void)
{
	uns16 i;

	/*
	 * Read data from memory into buffer 1.
	 */
#ifndef BUF_ONLY
	read_to_buf1();
#endif

	/*
	 * Now copy the data out to our buffer.
	 */
	DATA_CS = 0;
	send_byte(DF_READ_BUF1);
	send_buffer_offset();
	send_byte(0);		// spec requires one more byte for clocking.

	for (i = 0; i < BLOCK_SIZE; i++) {
		send_byte(0);
		buffer[i] = SSPBUF;
	}
	DATA_CS = 1;
}

/*
 * Very, very simple write routine.
 *
 * Flow:
 *  1) load the page into a buffer.
 *  2) erase the page
 *  3) write the data into the right portion of the buffer
 *  4) program the page with data from the buffer.
 */
void
df_write(void)
{
	uns16 i;

#ifndef BUF_ONLY
	/*
	 * 1: Read into buffer 1.
	 */
	read_to_buf1();

	/*
	 * 2: Erase the page.
	 * The address variable was properly set by the read_to_buf fn.
	 */
	send_cmd_with_address(DF_PAGE_ERASE);
	wait_for_not_busy();
#endif

	/*
	 * 3: Copy our data into the buffer.
	 */
	nop2();
	DATA_CS = 0;
	send_byte(DF_WRITE_BUF1);
	send_buffer_offset();

	for (i = 0; i < BLOCK_SIZE; i++)
		send_byte(buffer[i]);
	DATA_CS = 1;

#ifndef BUF_ONLY
	/*
	 * 4: Program the buffer into the memory array.
	 */
	nop2();
	send_cmd_with_address(DF_BUF1_TO_MEM);
	wait_for_not_busy();
#endif
}

void
doit(void)
{
	unsigned char b;
	uns16 i;
	unsigned char v;

	//get_status();
	//blink_byte(data_status);

	/*
	 * Test:
	 *  Write some blocks with different patterns at the base LBA.
	 *  Then read them back, going backwards.
	 */

	for (b = 0; b < BLOCKS; b++) {
		df_lba = BASE_LBA + (uns24)b;
		v = patterns[b];

		for (i = 0; i < BLOCK_SIZE; i++)
			buffer[i] = v ^ i.low8;

		df_write();
	}

	for (b = BLOCKS-1; b; b--) {
		df_lba = BASE_LBA + (uns24)b;
		df_read();

		v = patterns[b];

		for (i = 0; i < BLOCK_SIZE; i++)
			if (buffer[i] != (v ^ i.low8))
				blink_error(1);
	}
}

void
main(void)
{
	PIC_setup();
	blink_init();
	doit();
	blink_ok();
}
