#include "int18xxx.h"
#include "hexcodes.h"

/*
 * Define the I/O infrastructure.
 */
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2

#pragma	bit DATA_CS		@ PORTC.6	// dataflash select

#pragma bit TOD_CS		@ PORTB.4	// TOD select
#pragma bit TOD_DATA		@ PORTB.3	// TOD data I/O
#pragma bit TOD_DATA_TRIS	@ TRISB.3	// TOD data I/O tris
#pragma bit TOD_SCLK		@ PORTB.2	// TOD clock.

#pragma bit ADC_CLK		@ PORTA.5	// ADC clock
#pragma bit ADC_DATA_O		@ PORTC.2	// data to ADC
#pragma bit ADC_DATA_I		@ PORTC.1	// data from ADC
#pragma bit ADC_CS		@ PORTC.0	// ADC select

#pragma bit RAW0		@ PORTB.6	// opto input 1
#pragma bit RAW1		@ PORTB.7	// opto input 2

#pragma bit RECORD_STOP		@ PORTA.1

#define	TRISA_SET		0b.0000.0011	// input switches
#define	TRISB_SET		0b.1100.0001	// optos and input from DF
#define	TRISC_SET		0b.0000.0010	// one input from ADC
#define	ADCON1_SET		0b.0000.1111	// no analog input

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

void
main(void)
{
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

	for (;;)  {
		LED_GREEN = RAW0;
		LED_RED = RAW1;
	}
}
