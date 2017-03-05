/*
 * This module contains a rudimentary SD card driver.
 *
 * This code assumes the chip has 1 hardware SPI module and
 * that the SD card is connected to it.
 */

/*
 * These variables define the LBA we are interested in.
 */
unsigned int sd_lba:24 @ 0;

/*
 * Return status from I/O routines
 */
unsigned char sd_status;

/*
 * THE BUFFER
 */
unsigned char buffer[512] @ 0x100;

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
 * SD Commands
 */
#define	GO_IDLE_STATE		(0x40 | 0)
#define	SEND_OP_COND		(0x40 | 1)
#define	SET_BLOCKLEN		(0x40 | 16)
#define	READ_SINGLE_BLOCK	(0x40 | 17)
#define	CRC_ON_OFF		(0x40 | 59)

/*
 * SD Tokens
 */
#define	DATA_START_TOKEN	0xFE

bit sd_slow_mode;
bit sd_read_error_fatal;

void
sd_error(unsigned char code, unsigned char response)
{
	blink_error_code(0xF & code, code, response);
}

/*
 * Use the SPI hardware to send one byte to the SD card.
 * You can pick up the received byte from the SSPBUF register
 * after this routine has run.
 */
void
send_byte(unsigned char b)
{
	unsigned char j;

	j = SSPBUF;
	SSPIF = 0;	// clear the interrupt request flag.
	SSPBUF = b;
	while (!SSPIF) ; // Wait for TX complete.

	/*
	 * This loop is only required when clocking slowly.
	 * Can be dropped for full speed (FOSC/4) clocking.
	 */
	if (sd_slow_mode)
	    for (j = 240; j; j++)
		nop2();
}

/*
 * This routine sends a command to the SD card.
 * NOTE:
 *  SD_CS is assumed to be 1 on entry and is left as 0 on exit.
 */
unsigned char
sd_cmd(unsigned char cmd, uns16 a1, uns16 a2, unsigned char crc)
{
	unsigned char timeout;
	unsigned char response;

	SD_CS = 0;	// enable the card.
	send_byte(cmd);
	send_byte(a1.high8);
	send_byte(a1.low8);
	send_byte(a2.high8);
	send_byte(a2.low8);
	send_byte(crc);

	/*
	 * This routine assumes an R1 (or R1b) response.
	 */
	for (timeout = 0; timeout < 8; timeout++) {
		send_byte(0xFF);
		response = SSPBUF;
		/*
		 * High bit of response must be zero.
		 * All 8 bits set to 1 means card is not
		 * yet driving the bus.
		 */
		if (response != 0xFF)
			break;
	}
	send_byte(0xFF);	// required clocking. (??)
	return response;
}

void
sd_read(void)
{
	unsigned char i, j;
	uns16 timeout;
	uns16 n;
	unsigned char *p;
	uns16 lba_hi;
	uns16 lba_low;

	sd_status = 0;

	/*
	 * This code converts a 24 bit LBA into a 32 bit byte address.
	 */
	/*xxx*/sd_lba += 775;
	lba_hi.high8 = sd_lba.high8;
	lba_hi.low8 = sd_lba.mid8;
	lba_hi *= 2;

	lba_low.high8 = sd_lba.low8;
	lba_low.low8 = 0;
	if (lba_low.15)
		lba_hi.0 = 1;
	lba_low *= 2;
	/*xxx*/sd_lba -= 775;

	/*
	 * Tell the SD card we are reading.
	 */
	i = sd_cmd(READ_SINGLE_BLOCK, lba_hi, lba_low, 0xFF);
	if (i != 0) {
		j = SD_ERROR_READ_1;
		goto sd_read_error;
	}

	/*
	 * Sample input until we see a data token.
	 */
	for (timeout = 0; timeout < 0x2FF; timeout++) {
		/*
		 * Delay "at least 8 clock cycles".  Not sure what or why.
		 */
		for (i = 0; i < 0x40; i++)
			nop();

		/*
		 * Recieve the data token.
		 */
		send_byte(0xFF);
		i = SSPBUF;
		if (i != 0xFF)
			goto data_found;
	}
	/*
	 * Timed out waiting for data token.
	 */
	j = SD_ERROR_READ_2;
	goto sd_read_error;

    data_found:
    	/*
	 * Found something.  Was it the data token?
	 */
    	if (i != DATA_START_TOKEN) {
		j = SD_ERROR_READ_3;
		goto sd_read_error;
	}

	/*
	 * Loop receiving data.
	 */
	p = buffer;
	for (n = 0; n < 512; n++) {
		send_byte(0xFF);
		*p++ = SSPBUF;
	}
	send_byte(0xFF);	// recieve CRC high byte
	send_byte(0xFF);	// recieve CRC low byte
	send_byte(0xFF);	// extra 8 clock cycles
	return;

    sd_read_error:
    	sd_status = 1;
	if (sd_read_error_fatal)
		sd_error(j, i);
}

void
sd_init(void)
{
	unsigned char i;
	uns16 timeout;

	/*
	 * Note: the media powers up in open drain mode.
	 *  do not run faster than 400 KHz while in that mode.
	 */

	/*
	 * First, wait a millisecond for things to stabilize.
	 */
	SD_CS = 1;
	waitms();

	/*
	 * Set up timer 2 to provide slow clock to SPI.
	 * Goal is 100KHz which is (FOSC/4) / 120
	 * So set prescaler to 4, period to 30, and postscaler to 1.
	 * (Note: postscaler ignored by SSP module.)
	 */
	TMR2IE = 0;		// no interrupts from timer 2.
	PR2 = 30;		// period is 30.
		// b7: unused, MBZ
		// b6-3: postscale
		// b2: enable
		// b1-0: prescale setting
	T2CON = 0b.0000.0101;	// prostscale = 1, timer = on, prescale = 4.

	/*
	 * Set up the SPI hardware.
	 * The TRIS registers must already be set up.
	 */
	 	// b7: input data sample middle of data output time
		// b6: transmit on clk active to idle
		// b5-0: read only
	SSPSTAT = 0b.0100.0000;

		// b7: clear collision
		// b6: unused in master mode
		// b5: enable SPI
		// b4: clock idle is low
		// b3-0: master mode, TMR2/2
	SSPCON1 = 0b.0010.0011;
	sd_slow_mode = 1;


	/*
	 * Send 80 clocks of nothing to let the card intialize.
	 */
	for (i = 0; i < 10; i++)
		send_byte(0xff);


	SD_CS = 0;

	waitms();

	/*
	 * Reset the card by sending CMD0
	 */
	i = sd_cmd(GO_IDLE_STATE, 0, 0, 0x95);
	SD_CS = 1;
	if (i == 0xFF || (i & 0xF7) != 1)
		sd_error(SD_ERROR_NO_INIT_1, i);


	for (timeout = 0; timeout < 0xFFF; timeout++) {
		i = sd_cmd(SEND_OP_COND, 0, 0, 0xF9);
		SD_CS = 1;
		if (i == 0)
			goto oc1;
	}
	sd_error(SD_ERROR_NO_INIT_2, i);

    oc1:

    	waitms();  waitms();

	/*
	 * Now shift the SPI hardware to fast (FOSC/4) mode.
	 */
	T2CON = 0;	// disable timer 2 to save power.
	SSPCON1 = 0b.0010.0000;
	sd_slow_mode = 0;

	/*
	 * Disable CRC checking.
	 * Some cards may complain about this command.
	 */
	i = sd_cmd(CRC_ON_OFF, 0, 0, 0x25);
	SD_CS = 1;
	if (i)
		sd_error(SD_ERROR_NO_INIT_3, i);


	/*
	 * Set the sector size to 512 bytes.
	 * Not sure why we do this.  I cannot imagine anybody doesn't already
	 * have this set correctly.
	 */
	i = sd_cmd(SET_BLOCKLEN, 0, 512, 0xFF);
	SD_CS = 1;
	if (i)
		sd_error(SD_ERROR_NO_INIT_4, i);

	/*
	 * Try a bunch of times to read sector 0.
	 * If we get it, then life is good.
	 */
	sd_lba = 0;
	sd_read_error_fatal = 0;
	for (timeout = 0xFF; timeout; timeout--) {
		sd_read();
		if (sd_status == 0)
			break;
	}
	sd_read_error_fatal = 1;
	sd_read();
    	return;		// done.  Success!
}

/*
 * TEST CODE
 */
#ifdef notdef
void
spi_test(void)
{
	unsigned char i, j;
	unsigned char clear;

	/*
	 * Set up timer 2 to provide slow clock to SPI.
	 * Goal is 100KHz which is (FOSC/4) / 120
	 * So set prescaler to 4, period to 30, and postscaler to 1.
	 * (Note: postscaler ignored by SSP module.)
	 */
	TMR2IE = 0;		// no interrupts from timer 2.
	PR2 = 30;		// period is 30.
		// b7: unused, MBZ
		// b6-3: postscale
		// b2: enable
		// b1-0: prescale setting
	T2CON = 0b.0000.0101;	// prostscale = 1, timer = on, prescale = 4.

	/*
	 * Set up the SPI hardware.
	 * The TRIS registers must already be set up.
	 */
	 	// b7: input data sample middle of data output time
		// b6: transmit on clk active to idle
		// b5-0: read only
	SSPSTAT = 0b.0100.0000;
		// b7: clear collision
		// b6: unused in master mode
		// b5: enable SPI
		// b4: clock idle is low
		// b3-0: master mode, TMR2/2
	SSPCON1 = 0b.0010.0011;

	clear = SSPBUF;
	SSPIF = 0;	// clear the interrupt request flag.
	SSPBUF = 0xff;
	while (!SSPIF) ; // Wait for TX complete.
	blink_debug(2);

	for (i = 0; i < 10; i++) {
		/*
		 * Wait a while.  Not sure why this is necessary,
		 * but without it we get WCOL errors.
		 * (240 works, 248 does not.)
		 */
		for (j = 240; j; j++)
			nop2();
		clear = SSPBUF;	// clear the BF flag.
		SSPIF = 0;	// clear the interrupt request flag.
		WCOL = 0;
		SSPBUF = 0xff;
		if (WCOL)
			blink_error(4);
		while(!SSPIF) ; // Wait for TX complete.
	}
	blink_ok();
}
#endif //notdef
