/*
 * This module can only be loaded and run by boot2.
 *
 * It makes use of a couple of boot2's functions to prove that works.
 */
#include "int18xxx.h"
#include "hexcodes.h"

/*
 * Define the I/O infrastructure.
 */
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2

#pragma resetVector -	// do the reset vector manually

#pragma origin = 0x800
void
redirect_1() {
#asm
	DW __GOTO(0x1000)
#endasm
}

extern void main(void);

#pragma origin = 0x1000
void
redirect_2() {
#asm
	call main
#endasm
}

#include "downcall.c"

void
main(void)
{
	unsigned char i, j;

	/*
	 * Toggle the lights back and forth 6 times.
	 */
	for (i = 0; i < 6; i++) {
		LED_GREEN = 0;
		LED_RED = 1;
		for(j = 1; j; j++)
			waitms();
		LED_GREEN = 1;
		LED_RED = 0;
		for(j = 1; j; j++)
			waitms();
	}

	blink_code = 2;
	blink_debug_x();

	sd_init();

	sd_lba = 0;
	sd_read();

	blink_code = 3;
	blink_debug_x();

	LED_GREEN = 1;
	LED_RED = 0;
	for (;;) ;
}
