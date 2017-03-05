/*
 * This module contains a rudimentary dataflash card driver.
 *
 * This code assumes the chip has 1 hardware SPI module and
 * that the SD card is connected to it.
 */

/*
 * These variables define the LBA we are interested in.
 */
unsigned int df_lba:24 @ 0;

/*
 * Return status from I/O routines
 */

unsigned char data_status;

/*
 * THE BUFFER
 */
#define	BLOCK_SIZE	512
unsigned char buffer[BLOCK_SIZE] @ 0x100;

/*
 * Error codes
 */
#define	SD_ERR(x, y)	((y<<4) | x)

#define	SD_ERROR_NO_INIT_1	SD_ERR(1, 1)
#define	SD_ERROR_NO_INIT_2	SD_ERR(1, 2)
#define	SD_ERROR_NO_INIT_3	SD_ERR(1, 3)
#define	SD_ERROR_NO_INIT_4	SD_ERR(1, 4)

#define	SD_ERROR_READ_1		SD_ERR(2, 1)
#define	SD_ERROR_READ_2		SD_ERR(2, 2)
#define	SD_ERROR_READ_3		SD_ERR(2, 3)

/*
 * Atmel data flash commands
 */
#define	DF_BUF1_TO_MEM	0x88
#define	DF_MEM_TO_BUF1	0x53
#define	DF_PAGE_ERASE	0x81
#define	DF_READ_BUF1	0xD4	// high speed serial read opcode
#define	DF_STATUS	0xD7
#define	DF_WRITE_BUF1	0x84

void
sd_error(unsigned char code, unsigned char response)
{
	blink_error_code(0xF & code, code, response);
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
	do {
		nop2();
		get_status();
	} while (!data_status.7);
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
	read_to_buf1();

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

void
df_init(void)
{
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

	/*
	 * Verify that we can talk to the card.
	 */
	get_status();
	if ((data_status & 0xBF) != 0xBC)
		sd_error(SD_ERROR_NO_INIT_1, data_status);
}
