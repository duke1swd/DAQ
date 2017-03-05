#include "HardwareProfile.h"
#include "GenericTypeDefs.h"
#include "dataflash.h"
#include "blink.h"
#include "delays.h"
#include <eep.h>
#include "eprommap.h"

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

/*
 * Utility routines.
 */

static DWORD df_lba;
static DWORD b_addr;

static unsigned char prepare_status;

static void
mark_df_not_prepared(void)
{
	if (prepare_status) {
		prepare_status = 0;
		Write_b_eep(EPROM_ATMEL_READY, 0);
	}
}

static void
send_byte(BYTE b)
{
	BYTE clear;

	clear = SSPBUF;		// clear the buffer full flag.
	PIR1bits.SSPIF = 0;	// clear the interrupt request flag.
	SSPBUF = b;		// start transmit
	while (!PIR1bits.SSPIF) ; // Wait for TX complete.
}

BYTE data_status;

static void
get_status(void)
{
	SD_CS = 0;	// select the card.
	send_byte(DF_STATUS);
	send_byte(0xFF);
	data_status= SSPBUF;
	SD_CS = 1;
	Delay10TCYx(1);	// make sure CS is high long enough.
}


static void
send_cmd_with_address(unsigned char cmd)
{
	SD_CS = 0;	
	send_byte(cmd);
	send_byte((b_addr >> 16) & 0xff);
	send_byte((b_addr >> 8) & 0xff);
	send_byte(b_addr & 0xff);
	SD_CS = 1;
	Delay10TCYx(1);	// make sure CS is high long enough.
}

static void
wait_for_not_busy(void)
{
	do {
		get_status();
	} while (!(data_status & 0x80));
}

static void
read_to_buf1(void)
{
	/*
	 * Discard the LSB of df_lba.
	 * Put the rest into the top 13 bits of the address register.
	 */
	b_addr = df_lba << 10;

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
static void
send_buffer_offset(void)
{
	send_byte(0);		// address byte 1
	if (df_lba & 0x01)
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
static void
df_read(BYTE *p)
{
	WORD i;

	/*
	 * Read data from memory into buffer 1.
	 */
	read_to_buf1();

	/*
	 * Now copy the data out to our buffer.
	 */
	SD_CS = 0;
	send_byte(DF_READ_BUF1);
	send_buffer_offset();
	send_byte(0);		// spec requires one more byte for clocking.

	for (i = 0; i < BLOCK_SIZE; i++) {
		send_byte(0);
		*p++ = SSPBUF;
	}
	SD_CS = 1;
	Delay10TCYx(1);	// make sure CS is high long enough.
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
static void
df_write(BYTE *p)
{
	WORD i;

	mark_df_not_prepared();

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

	/*
	 * 3: Copy our data into the buffer.
	 */
	SD_CS = 0;
	send_byte(DF_WRITE_BUF1);
	send_buffer_offset();

	for (i = 0; i < BLOCK_SIZE; i++)
		send_byte(*p++);
	SD_CS = 1;
	Delay10TCYx(1);	// make sure CS is high long enough.

	/*
	 * 4: Program the buffer into the memory array.
	 */
	send_cmd_with_address(DF_BUF1_TO_MEM);
	wait_for_not_busy();
}

/*
 * Return the card's capacity in units of 512b blocks.
 */
DWORD
df_ReadCapacity(void)
{
	DWORD v;

	v = 16384;
	
	return v;
}

WORD
df_ReadSectorSize(void)
{
	WORD v;

	v = 512;
	return v;
}

void
df_InitIO(void)
{
	/*
	 * Set the TRIS bits.
	 */
	SD_CS_TRIS = 0;
	SPICLOCK = 0;
	SPIOUT = 0;                  // define SDO1 as output (master or slave)
	SPIIN = 1;                  // define SDI1 as input (master or slave)

	/*
	 * Set up the SPI interface.
	 */
	SD_CS = 1;
	SSPSTAT = 0x40;
	SSPCON1 = 0x20;
}

/*
 * Status should be:
 *  RDY
 *  compare (don't care)
 *  0xf -- constant
 *  not protected
 *  normal page size
 *
 * As long as we can get a valid status then we are ready to go.
 */
BYTE
df_MediaInitialize(void)
{
	prepare_status = Read_b_eep(EPROM_ATMEL_READY);
	get_status();
	if ((data_status & 0xBF) == 0xBC)
		return 1;
	return 0;
}

/*
 * Assume media is present iff we can get a valid status from it.
 */
BYTE
df_MediaDetect(void)
{
	return df_MediaInitialize();
}

/*
 * Read a sector.
 */
BYTE
df_SectorRead(DWORD sector_addr, BYTE* buffer)
{
#ifdef FAKE
	WORD i;

	for (i = 0; i < BLOCK_SIZE; i++)
		buffer[i] = i & 0xff;
	get_status();
	buffer[0] = data_status;
#else // FAKE
	df_lba = sector_addr;
	df_read(buffer);
#endif // FAKE
	return 1;
}

/*
 * Write to a sector.
 * Software protection on sector zero is not implemented.
 */
BYTE
df_SectorWrite(DWORD sector_addr, BYTE* buffer, BYTE allowWriteToZero)
{
	df_lba = sector_addr;
#ifndef FAKE
	df_write(buffer);
#endif // FAKE
	return 1;
}

/*
 * We do not implement any form of write-protect.
 */
BYTE
df_WriteProtectState(void)
{
    return 0;
}

/*
 * Shutdown simply consists of waiting for any self-timed
 * operation to finish.  These include erase, program, or
 * read to internal buffer.
 */
void
df_ShutdownMedia(void)
{
	wait_for_not_busy();
}
