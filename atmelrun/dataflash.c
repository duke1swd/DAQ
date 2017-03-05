/*
 * This module contains a rudimentary dataflash card driver.
 *
 * This code assumes the chip has 1 hardware SPI module and
 * that the SD card is connected to it.
 */

uns24 df_lba;	// block to read/write

/*
 * Return status from I/O routines
 */
unsigned char sd_status;
bit df_erase_needs_init;

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
#define	DF_AUTO_REWRITE	0x58
#define	DF_BUF1_TO_MEM	0x88
#define	DF_BUF2_TO_MEM	0x89
#define	DF_COMPARE_BUF1	0x60
#define	DF_COMPARE_BUF2	0x61
#define	DF_MEM_TO_BUF1	0x53
#define	DF_PAGE_ERASE	0x81
#define	DF_READ_BUF1	0xD4	// high speed serial read opcode
#define	DF_STATUS	0xD7
#define	DF_WRITE_BUF1	0x84
#define	DF_WRITE_BUF2	0x87
#define	DF_DEEP_SLEEP	0xB9
#define	DF_WAKEUP	0xAB

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

	df_erase_needs_init = 1;
}

/*
 * Load buffer 2 with 0xff.
 * Used by the erase routine.
 */
void
df_erase_init(void)
{
	uns16 i;

	df_erase_needs_init = 0;
	DATA_CS = 0;
	send_byte(DF_WRITE_BUF2);
	send_byte(0);
	send_byte(0);
	send_byte(0);
	for (i = 0; i < 1056; i++)
		send_byte(0xFF);
	DATA_CS = 1;
}

/*
 * If the indicated page matches buffer 2, return true.
 */
uns8
block_match(void)
{
	address = df_lba << 10;
	send_cmd_with_address(DF_COMPARE_BUF2);
	wait_for_not_busy();
	if (data_status.6 == 0)
		return 1;
	return 0;
}

/*
 * Erase a page.
 *
 * Uses df_lba to specify the page.
 * WARNING, always erases 2 blocks.  The LSB of df_lba is ignored.
 */
void
df_erase(void)
{
	if (df_erase_needs_init)
		df_erase_init();

	if (block_match())
		return;

	address = df_lba << 10;
	send_cmd_with_address(DF_PAGE_ERASE);
	wait_for_not_busy();
}

/*
 * Reprogram a page.
 * Uses df_lba to specify the page.
 * The LSB of df_lba is ignored.
 */
void
df_reprogram(void)
{
	address = df_lba << 10;
	send_cmd_with_address(DF_AUTO_REWRITE);
	wait_for_not_busy();
}

/*
 * This section implements streaming writes.
 *
 * Streaming writes are high-speed, double-buffered writes
 * used by the main data capture module.  Many restrictions
 * on their use:
 *   - Between stream_begin and stream_end the only legal
 *     dataflash operation is stream_write.
 *   - Writes can only go to previously erased pages.
 *   - Writes should be sequential runs, begining on an even LBA.
 *   - All runs should be an even number of blocks in length.
 *   - If there is an odd block (half page) at the end of a run
 *     its contents are undefined except for the first 4 bytes, which
 *     will be set to zero.
 *   - Starting a sequential run on an odd lba is a fatal error.
 *
 * Note: this section of the code uses 16 bit LBAs for speed and
 *  compactness.
 */
uns16 dfs_previous_lba;
uns16 dfs_current_lba;
/*
 * If this bit is zero, then we are currently writing data to DF buffer 1.
 * If this bit is one, then we are currently writing data to DF buffer 2.
 */
bit dfs_which_buffer;

/*
 * This is true if a bufferis partially written.
 */
bit dfs_open_buffer;

/*
 * Commit current buffer to memory.
 * Switch buffers.
 */
void
dfs_commit(void)
{
	uns8 cmd;

	/*
	 * Then close this buffer transfer command
	 * and wait for the previous write to finish.
	 */
	DATA_CS = 1;
	wait_for_not_busy();

	/*
	 * Send this buffer to memory.
	 */
	address = ((uns24) dfs_previous_lba) << 10;
	cmd = DF_BUF1_TO_MEM;
	if (dfs_which_buffer)
		cmd = DF_BUF2_TO_MEM;
	DATA_CS = 0;	
	send_cmd_with_address(cmd);

	/*
	 * Flip buffers.
	 */
	dfs_which_buffer ^= 1;
	
	dfs_open_buffer = 0;
}

/*
 * External entry point for starting a stream write.
 */
void
df_stream_begin(void)
{
	dfs_which_buffer = 0;
	dfs_previous_lba = 0;
	dfs_open_buffer = 0;
	df_erase_needs_init = 1;
}


/*
 * External entry point for ending a stream write.
 */
void
df_stream_term(void)
{
	/*
	 * Are we ending with half a page uncommitted?
	 */
	if (dfs_open_buffer) {
		send_byte(0);
		send_byte(0);
		send_byte(0);
		send_byte(0);
		dfs_commit();
	}
}

/*
 * Copy 512 bytes to the buffer.
 * Assumes the dataflash command is already set up and running.
 */
size2 unsigned char *df_stream_write_data;
uns8 dfs_index;

void
dfs_block(void)
{
#define DFS_BLOCK_IN_C

#ifdef DFS_BLOCK_IN_C
	uns8 clear;

	for (dfs_index = 128; dfs_index; dfs_index--) {
		clear = SSPBUF;
		SSPIF = 0;	// clear the interrupt request flag.
		SSPBUF = *df_stream_write_data++;
		while (!SSPIF) ; // Wait for TX complete.

		clear = SSPBUF;
		SSPIF = 0;	// clear the interrupt request flag.
		SSPBUF = *df_stream_write_data++;
		while (!SSPIF) ; // Wait for TX complete.

		clear = SSPBUF;
		SSPIF = 0;	// clear the interrupt request flag.
		SSPBUF = *df_stream_write_data++;
		while (!SSPIF) ; // Wait for TX complete.

		clear = SSPBUF;
		SSPIF = 0;	// clear the interrupt request flag.
		SSPBUF = *df_stream_write_data++;
		while (!SSPIF) ; // Wait for TX complete.
	}
#else // DFS_BLOCK_IN_C
#asm
	// put the buffer pointer into the FSR0 register.
	MOVFF	df_stream_write_data,FSR0
	MOVFF	df_stream_write_data+1,FSR0+1

	MOVLW	128		// dfs_index = 128;
	MOVWF	dfs_index,0

    for_loop:

	MOVF	SSPBUF,0,0	// clear = SSBUF;
	BCF	PIR1,SSPIF,0	// SSPIF = 0;
	MOVFF	POSTINC0,SSPBUF	// SSPBUF = *df_stream_write_data++;
    t1:	BTFSS	PIR1,SSPIF,0	// while (!SSPIF) ;
	BRA	t1
	NOP
	NOP

	MOVF	SSPBUF,0,0	// clear = SSBUF;
	BCF	PIR1,SSPIF,0	// SSPIF = 0;
	MOVFF	POSTINC0,SSPBUF	// SSPBUF = *df_stream_write_data++;
    t2:	BTFSS	PIR1,SSPIF,0	// while (!SSPIF) ;
	BRA	t2
	NOP
	NOP

	MOVF	SSPBUF,0,0	// clear = SSBUF;
	BCF	PIR1,SSPIF,0	// SSPIF = 0;
	MOVFF	POSTINC0,SSPBUF	// SSPBUF = *df_stream_write_data++;
    t3:	BTFSS	PIR1,SSPIF,0	// while (!SSPIF) ;
	BRA	t3
	NOP
	NOP

	MOVF	SSPBUF,0,0	// clear = SSBUF;
	BCF	PIR1,SSPIF,0	// SSPIF = 0;
	MOVFF	POSTINC0,SSPBUF	// SSPBUF = *df_stream_write_data++;
    t4:	BTFSS	PIR1,SSPIF,0	// while (!SSPIF) ;
	BRA	t4

    	DECFSZ	dfs_index,0	// decrement index, complete the loop
	BRA	for_loop
#endasm
#endif // DFS_BLOCK_IN_C
}

/*
 * Begin a command to write data to the buffer.
 * Chooses the proper buffer and location in the buffer.
 */
void
dfs_open_buffer_write(uns8 buf_offset)
{
	uns8 cmd;

	cmd = DF_WRITE_BUF1;
	if (dfs_which_buffer)
		cmd = DF_WRITE_BUF2;

	DATA_CS = 0;
	send_byte(cmd);
	send_byte(0);		// address byte 1
	if (buf_offset)
		send_byte(2);	// address byte 2
	else
		send_byte(0);	// address byte 2
	send_byte(0);		// address byte 3
}

/*
 * This routine writes the next block in a streaming write.
 * The location of the data to write is set in the global
 * variable df_stream_write_data;
 */
void
df_stream_write()
{
	uns16 t16;
	dfs_current_lba = (df_lba & 0xffff);

	/*
	 * Is there a half-written buffer?
	 */
	if (dfs_open_buffer) {
		t16 = dfs_previous_lba + 1;
		if (dfs_current_lba == t16) {
			/*
			 * Case 1: continuing a write, second half of a buffer.
			 *  Send the data to the buffer and commit it.
			 */
			dfs_block();
			dfs_commit();
			return;
		}

		/*
		 * Case 2: chain ends after half a page.
		 *  Need to clean up, then start anew.
		 */
		send_byte(0);
		send_byte(0);
		send_byte(0);
		send_byte(0);
		dfs_commit();
	}

	/*
	 * Beging transfering data to the new buffer.
	 */
	dfs_open_buffer_write(0);
	dfs_open_buffer = 1;

	/*
	 * Is the new location the odd half of a buffer?
	 */
	if (dfs_current_lba & 0x1) {
		/*
		 * Fake the first half of the buffer
		 */
		send_byte(0);
		send_byte(0);
		send_byte(0);
		send_byte(0);
		DATA_CS = 1;
		/*
		 * Fill the second half of the buffer
		 * and commit it.
		 */
		dfs_open_buffer_write(1);
		dfs_block();
		dfs_commit();
		return;
	}

	/*
	 * Normal first-half of the buffer comes here.
	 * Fill it and leave it open.
	 */
	dfs_block();
	dfs_previous_lba = dfs_current_lba;
}
