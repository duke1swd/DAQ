/*
 * Small program to test my ability to program the instruction memory.
 */

#include "int18xxx.h"
#include "hexcodes.h"

/*
 * Define the I/O infrastructure.
 */
#pragma resetVector 0x800
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2
#define	TRISA_SET		0b.0000.0001	// bit 0 is the mode switch
#define	TRISB_SET		0b.0000.0001	// one input from SD
#define	TRISC_SET		0b.0000.0000	// all are outputs.
#define	ADCON1_SET		0b.0000.1111	/* No analog inputs */

/*
 * Globals
 */
unsigned char boot_mode;

void
waitms(void)
{
	unsigned char i, j;

	for (j = 7; j; j--)
		for (i = 1; i; i++)
			nop2();
}

#define	BLINK_NO_EPROM
#include "blink.c"

/*
 * Test by erasing 64 bytes, writing 64 bytes of boot_mode,
 * and reading them back.
 */

#define	LOAD_ADDRESS		0x1000		// location in program memory
#define	LOAD_LOW		(LOAD_ADDRESS & 0xFF)
#define	LOAD_HIGH		(LOAD_ADDRESS >> 8)

void
start_write(void)
{
	EECON2 = 0x55;
	EECON2 = 0xAA;
	WR = 1;
}

#define	REGION	8*512		// number of bytes to test.

void
doit(void)
{
	uns16 i;
	uns16 segment;
	uns16 load_address;

	blink_debug(boot_mode);

	for (segment = 0; segment < REGION; segment += 64) {

		/*
		 * Erase.
		 */
		load_address = LOAD_ADDRESS + segment;
		TBLPTRU = 0;
		TBLPTRH = load_address.high8;
		TBLPTRL = load_address.low8;
		TABLAT = boot_mode;

		EECON1 = 0x94;	// program memory, erase, write enabled.
		start_write();

		/*
		 * Write loop.
		 */

		TBLPTRU = 0;
		TBLPTRH = load_address.high8;
		TBLPTRL = load_address.low8;
		TABLAT = boot_mode;

		EECON1 = 0x84;	// program memory, write enabled.

		for (i = 0; i < 64; i++) {
			TABLAT = boot_mode;
			tableWrite();
			if ((i & 0x1f) == 0x1f)
				start_write();
			tableReadInc();
		}
	}

	/*
	 * Read back loop.
	 */

	TBLPTRU = 0;
	TBLPTRH = LOAD_HIGH;
	TBLPTRL = LOAD_LOW;

	EECON1 = 0x80;	// program memory, write disabled.

	for (i = 0; i < REGION; i++) {
		tableReadInc();
		if (TABLAT != boot_mode)
			blink_error(2);
	}
}

void
main(void)
{
	clearRAM();

	/*
	 * Turn off stuff we don't use.
	 */
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
	 * Using the on-chip A/D, sample the position of the mode switch.
	 */
	ADCON1 = 0x0E;		// RA0/AN0 is analog
	ADCON2 = 0x16;		// Left just, Aq time = 4 TAD, TAD = Fosc/64
	ADON = 1;		// Enable the A/D converter module
	GO = 1;			// Start conversion
	while (GO) ;		// Wait for conversion


	/*
	 * The boot mode is the top 3 bits of the
	 * A/D result.
	 */
	boot_mode = 0x7 & (ADRESH >> 5);

        ADCON0 = 0;          	// Turn A/D off
	ADCON1 = ADCON1_SET;

	blink_init();
	doit();
	blink_ok();
}
