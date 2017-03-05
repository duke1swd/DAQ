/*
 * Routines to blink the DAQ V1.0 lights.
 */

unsigned char blink_code @ 3;
unsigned char blink_sub_code @ 4;
unsigned char blink_detail @ 5;

void
blink_sdelay(void)
{
	unsigned char i;
	
	for (i = 0; i < 100; i++)
		waitms();
}

void
blink_mdelay(void)
{
	unsigned char j;

	for (j = 0; j < 8; j++)
		blink_sdelay();
}

void
blink_ldelay(void)
{
	blink_mdelay();
	blink_mdelay();
}

/*
 * Turns on Red, off Green.
 * Then blinks Green 'code' times.
 * Turns both off
 */
void
blink_debug_x(void)
{
	unsigned char i;
	LED_GREEN = 0;
	LED_RED = 1;
	blink_ldelay();
	
	for (i = blink_code; i; i--) {
		LED_GREEN = 1;
		blink_sdelay();
		LED_GREEN = 0;
		blink_sdelay();
		blink_sdelay();
	}

	LED_RED = 0;
	blink_ldelay();
}

void
blink_debug(unsigned char code)
{
	blink_code = code;
	blink_debug_x();
}

/*
 * The code is a 6 bit error code.
 * sub_code adds 8 more bits to it.
 * detail adds another 8.
 */

void
blink_error_code_x(void)
{
#ifndef BLINK_NO_EPROM
	unsigned char t;
	unsigned char b1, b2;
	unsigned char addr;

	/*
	 * We alternate between 2 error code locations.
	 * The high order bits of each location are a 2
	 * bit sequence number.  We increment the sequence
	 * number every time, and use this to tell which one
	 * to overwrite next.  The value 0b11 is never used
	 * and denotes an empty cell.
	 */
	Busy_eep();
	b1 = Read_b_eep(EPROM_ERR1_CODE) & 0x3;
	Busy_eep();
	b2 = Read_b_eep(EPROM_ERR2_CODE) & 0x3;

	/*
	 * Figure out which cell to use.
	 */
	addr = EPROM_ERR1_CODE;
	if (b1 != 3) {		// b1 is not empty, maybe use b2.
	    if (b1 > b2)
		addr = EPROM_ERR2_CODE;	// b1 is newer, use b2
	    else if (b2 & 2)		// b2 is empty, or
		addr = EPROM_ERR2_CODE;	// b1 has wrapped but b2 hasn't
	    else
		b1 = b2;	// b1 is older
	} else
	    b1 = b2;		// b1 is empty
	
	/*
	 * Increment the sequence number, with wrap-around.
	 */
	b1++;
	if (b1 > 2)
		b1 = 0;

	/*
	 * At this point addr is the location to store the code
	 * and b1 is the sequence number.
	 *
	 * Write 3 bytes.  First has 2 bit sequence, 
	 * and 6 bit code.  Then write 8 bit sub-code and 8 bit detail.
	 */
	t = (blink_code << 2) | b1;

	Busy_eep();
	Write_b_eep(addr, t);
	addr++;

	Busy_eep();
	Write_b_eep(addr, blink_sub_code);
	addr++;

	Busy_eep();
	Write_b_eep(addr, blink_detail);
#endif // BLINK_NO_EPROM

	/*
	 * Blink the primary code (low order 4 bits only).
	 */
	blink_code &= 0xf;

	for (;;)
		blink_debug_x();
}

void
blink_error_code(unsigned char code, unsigned char sub_code, unsigned char detail)
{
	blink_code = code;
	blink_sub_code = sub_code;
	blink_detail = detail;
	blink_error_code_x();
}

/*
 * Turns on Red, off Green.
 * Then blinks Green 'code' times.
 * Turns both off
 * Repeat forever.
 */

void
blink_error(unsigned char code)
{
	blink_error_code(code, 0, 0);
}

/*
 * Success display routine.
 * Turns Green on and Red Off.  Never returns.
 */
void
blink_ok(void)
{
	LED_GREEN = 1;
	LED_RED = 0;
	for(;;)
		;
}

#ifdef BLINK_BYTE
void
blink_byte(unsigned char b)
{
	unsigned char i;


	/*
	 * Briefly flash both LEDs.
	 */
	blink_mdelay();
	LED_RED = 1;
	LED_GREEN = 1;
	blink_sdelay();
	LED_RED = 0;
	LED_GREEN = 0;
	blink_mdelay();

	/*
	 * Now blink out the byte, MSB first, Red = 1, Green = 0.
	 */
	for (i = 0; i < 8; i++) {
		if (b.7)
			LED_RED = 1;
		else
			LED_GREEN = 1;
		b <<= 1;
		blink_mdelay();
		LED_RED = 0;
		LED_GREEN = 0;
		blink_mdelay();
	}
	blink_mdelay();
}
#endif // BLINK_BYTE

/*
 * Assumes TRIS already set.
 * ADCON registers must be set to not interfere.
 * Blinks both LEDs once, slowly.
 */
void
blink_init(void)
{

	LED_GREEN = 1;
	blink_sdelay();
	LED_RED = 1;

	blink_mdelay();

	LED_GREEN = 0;
	LED_RED = 0;

	blink_mdelay();
}
