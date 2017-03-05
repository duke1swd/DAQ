/*
 * Routines to blink the DAQ V1.0 lights.
 */

#include "HardwareProfile.h"
#include "blink.h"
#include "eprommap.h"
#include <delays.h>
#include <eep.h>

#define	blink_sdelay()	Delay10KTCYx(12 * 10)	// 100 milliseconds

void
blink_ldelay(void)
{
	unsigned char j;

	for (j = 0; j < 15; j++)
		blink_sdelay();
}

/*
 * Turns on Red, off Green.
 * Then blinks Green 'code' times.
 * Turns both off
 */
void
blink_debug(unsigned char code)
{
	LED_GREEN = 0;
	LED_RED = 1;
	blink_ldelay();
	
	for (; code > 0; code--) {
		LED_GREEN = 1;
		blink_sdelay();
		LED_GREEN = 0;
		blink_sdelay();
		blink_sdelay();
	}

	LED_RED = 0;
	blink_ldelay();
}

/*
 * The code is a 6 bit error code.
 * sub_code adds 8 more bits to it.
 * detail adds another 8.
 */

void
blink_error_code(unsigned char code, unsigned char sub_code, unsigned char detail)
{
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
	code = (code << 2) | b1;

	Busy_eep();
	Write_b_eep(addr, code);
	addr++;

	Busy_eep();
	Write_b_eep(addr, sub_code);
	addr++;

	Busy_eep();
	Write_b_eep(addr, detail);

	/*
	 * Blink the primary code (low order 4 bits only).
	 */
	code = (code >> 2) & 0xf;

	for (;;)
		blink_debug(code);
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

/*
 * Initializes TRIS bits.
 * ADCON registers must be set to not interfere.
 * Blinks both LEDs once, slowly.
 */
void
blink_init(void)
{

	LED_GREEN_TRIS = 0;	// output
	LED_RED_TRIS = 0;	// output

	LED_GREEN = 1;
	blink_sdelay();
	LED_RED = 1;

	blink_ldelay();

	LED_GREEN = 0;
	LED_RED = 0;

	blink_ldelay();
}
