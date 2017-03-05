#include "int18xxx.h"
#include "hexcodes.h"

/*
 * Define the I/O infrastructure.
 */
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2
#pragma	bit SD_SCK		@ PORTB.1	// SD clock
#pragma	bit SD_SDI		@ PORTB.0	// SD data to PIC
#pragma	bit SD_SDO		@ PORTC.7	// SD data from PIC
#pragma	bit SD_CS		@ PORTC.6	// SD chip select
#define	TRISA_SET		0b.0000.0000	// all are outputs.
#define	TRISB_SET		0b.0000.0001	// one input from SD
#define	TRISC_SET		0b.0000.0000	// all are outputs.
#define	ADCON1_SET		0b.0000.1111	/* No analog inputs */

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

unsigned char blat;

void
main(void)
{
	uns16 i;
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

	LED_GREEN = 1;
	LED_RED = 0;
	for (;;)  {
		for (i = 1; i; i++)
			nop2();

		LED_GREEN = 0;
		LED_RED = 1;
		for (i = 1; i; i++)
			nop2();
		LED_GREEN = 1;
		LED_RED = 0;
	}

	TBLPTRL = 0;
	#asm
	TBLRD*+
	MOVF TABLAT, W
	MOVWF blat
	#endasm


}
