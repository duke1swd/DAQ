/*
 * Routines to blink the DAQ V1.0 lights.
 */

#include "HardwareProfile.h"
#include "blink.h"
#include <delays.h>

#define	blink_sdelay()	Delay10KTCYx(12 * 7)	// 70 milliseconds

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
 * Turns on Red, off Green.
 * Then blinks Green 'code' times.
 * Turns both off
 * Repeat forever.
 */
void
blink_error(unsigned char code)
{
	for (;;)
		blink_debug(code);
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
