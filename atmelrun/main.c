/*
 * Data Capture Program for the HybridSky.com data capture board,
 *
 * This version of the program captures 1, 2, or 3 channels
 * at 10KHz.
 */

#define	THREECHANNEL

/*
 * The DAQ 1.0 board is a PIC 18F2455.
 * The chip is running on a 12 MHz crystal with FOSC/4 = 12MHz.
 */

#include "int18xxx.h"
#include "hexcodes.h"
#include "eprommap.h"

/*
 * Resource usage:
 *
 * This module uses the SPI hardware to talk to the dataflash card.
 *
 * Timer 0 is used to dejitter the sampling interrupts.
 *
 * Timer 1 is used to generate the 10 KHz sampling interrupt.
 *
 * Timer 2 is reserved for use by the SPI hardware
 *
 * The LEDs are used to blink status and errors.
 *
 * The TOD chip provides time-of-day
 *
 * The ADC chip provides data.  Connection is by software SPI.
 *
 * NOTE: The opto-isolated inputs are inverted.  1 = off.
 *
 * RAW_0 drives the green stack-light LED
 * RAW_1 drives the red stack-light LED
 *
 */


/*
 * Define the I/O infrastructure.
 */
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2
#pragma bit LED_RED_LAT		@ LATA.2

#pragma	bit DATA_CS		@ PORTC.6	// dataflash select

#pragma bit TOD_CS		@ PORTB.4	// TOD select
#pragma bit TOD_DATA		@ PORTB.3	// TOD data I/O
#pragma bit TOD_DATA_TRIS	@ TRISB.3	// TOD data I/O tris
#pragma bit TOD_SCLK		@ PORTB.2	// TOD clock.

#pragma bit ADC_CLK		@ PORTA.5	// ADC clock
#pragma bit ADC_DATA_O		@ PORTC.2	// data to ADC
#pragma bit ADC_DATA_I		@ PORTC.1	// data from ADC
#pragma bit ADC_CS		@ PORTC.0	// ADC select

#pragma bit OPTO_0		@ PORTB.6	// opto input 0
#pragma bit OPTO_1		@ PORTB.7	// opto input 1
#pragma	bit RAW_0		@ PORTA.4	// raw I/O pin 0
#pragma	bit RAW_1		@ PORTB.5	// raw I/O pin 1
#define	EXT_GREEN	RAW_0
#define	EXT_RED		RAW_1

#pragma bit RECORD_STOP		@ PORTA.1

#ifdef OOPS
#define	TRISA_SET		0b.0001.0011	// input switches
#else // OOPS
#define	TRISA_SET		0b.0000.0011	// input switches
#endif // OOPS
#define	TRISB_SET		0b.1100.0001	// optos and input from DF
#define	TRISC_SET		0b.0000.0010	// one input from ADC
#define	ADCON1_SET		0b.0000.1111	// no analog input


/*
 * Control for Timer 1.
 *
 * Set T1PD to be the number of instruction cycles between
 * calls to the ADC sample routine.  12,000 = 1KHz, 1,200 = 10KHz, etc.
 *
 */
//#define	T1PD		6000		// 2 KHz
//#define	T1PD		4800		// 2.5 KHz
//#define	T1PD		4000		// 3 KHz
//#define	T1PD		3000		// 4 KHz
//#define	T1PD		2000		// 6 KHz
//#define	T1PD		1500		// 8 KHz
//#define	T1PD		1196		// 10 KHz
#define	T1PD		(1196*2)	// 5 KHz

#define	T1SET_H		((-T1PD + 21) >> 8)
#define	T1SET_L		((-T1PD + 21) & 0xff)

/*
 * Define the infrastructure of a DAQ bootable.
 */
#pragma resetVector 0x1000
#pragma unlockISR


/***********************
 *                     *
 * Interrupt routines. *
 *                     *
 ***********************/

void highPriorityInt(void);

/*
 * The high priority interrupts drive A/D sampling
 */
#pragma origin 0x1008
interrupt highPriorityIntServer(void)
{
	// W, STATUS and BSR are saved to shadow registers
	// handle the interrupt
	highPriorityInt();
	// restore W, STATUS and BSR from shadow registers
#pragma fastMode
}

void
timer1_interrupt(void);
void
timer3_interrupt(void);

/*
 * This program doesn't use the low priority interrupt vector.
 */
#pragma origin 0x1018
interrupt lowPriorityIntServer(void)
{
	// W, STATUS and BSR are saved by the next macro.
	int_save_registers

	//uns16 sv_FSR0 = FSR0;
	//uns16 sv_FSR1 = FSR1;
	//uns16 sv_FSR2 = FSR2;
	//uns8 sv_PCLATH = PCLATH;
	//uns8 sv_PCLATU = PCLATU;
	//uns8 sv_PRODL = PRODL;
	//uns8 sv_PRODH = PRODH;
	//uns24 sv_TBLPTR = TBLPTR;
	//uns8 sv_TABLAT = TABLAT;

	// handle low priority interrupt.
	if (TMR3IF)
		timer3_interrupt();
	else {
		LED_GREEN = 1;
		LED_RED_LAT = 1;
		for (;;) ;
	}

	//FSR0 = sv_FSR0;
	//FSR1 = sv_FSR1;
	//FSR2 = sv_FSR2;
	//PCLATH = sv_PCLATH;
	//PCLATU = sv_PCLATU;
	//PRODL = sv_PRODL;
	//PRODH = sv_PRODH;
	//TBLPTR = sv_TBLPTR;
	//TABLAT = sv_TABLAT;
	int_restore_registers
}

/*
 * High priority interrupt.
 *
 * Manages timer 0 and toggles the output pin
 * that controls the stepper.
 */

void
highPriorityInt(void)
{
	uns16 sv_FSR0 = FSR0;
	//uns16 sv_FSR1 = FSR1;
	//uns16 sv_FSR2 = FSR2;
	//uns8 sv_PCLATH = PCLATH;
	//uns8 sv_PCLATU = PCLATU;
	//uns8 sv_PRODL = PRODL;
	//uns8 sv_PRODH = PRODH;
	//uns24 sv_TBLPTR = TBLPTR;
	//uns8 sv_TABLAT = TABLAT;

	// handle the high priority interrupt
	if (TMR1IF)
		timer1_interrupt();
	else {
		LED_GREEN = 1;
		LED_RED_LAT = 1;
		for (;;) ;
	}

	FSR0 = sv_FSR0;
	//FSR1 = sv_FSR1;
	//FSR2 = sv_FSR2;
	//PCLATH = sv_PCLATH;
	//PCLATU = sv_PCLATU;
	//PRODL = sv_PRODL;
	//PRODH = sv_PRODH;
	//TBLPTR = sv_TBLPTR;
	//TABLAT = sv_TABLAT;
}

uns8 lbolt;	// incremented at 22.9 HZ.

void
waitms(void)
{
	unsigned char i, j;

	for (j = 7; j; j--)
		for (i = 1; i; i++)
			nop2();
}

/*
 * THE BUFFER
 */
#define	BLOCK_SIZE	512
unsigned char buffer[BLOCK_SIZE] @ 0x100;
bit found_eof;

/*
 * Include various modules.
 */
#include "slowmo.c"
#include "eprom.c"
#include "blink.c"
#include "dataflash.c"
#include "fat16.c"
#include "prepare.c"
#include "adc.c"
#include "tod.c"
#ifndef ERASE
#include "run.c"
#endif

/*
 * Timer 1 controls the ADC sampling.
 */
void
timer1_interrupt(void)
{
	/*
	 * This construct de-jitters the sample clock
	 * This is necessary to account for the fact that
	 * some instructions take 2 cycles and some take
	 * only 1, but interrupts never happen in the middle
	 * of a 2-cycle instruction.
	 */
	if (TMR0L.0)
		nop2();

	TMR1H = T1SET_H;
	TMR1L = T1SET_L;

	TMR1IF = 0;

	/*
	 * Do the actual work of the ISR.
	 */
#ifndef ERASE
	adc_isr();
#endif
}

/*
 * Timer 3 provides low priority time base for blinking LEDs.
 * This routine called with frequency FOSC/4 / 8 / 2**16 == 28.88 Hz
 */
void
timer3_interrupt()
{
	TMR3IF = 0;
	lbolt++;
}

void
main(void)
{
	unsigned char i;
	uns16 j;

	clearRAM();

	/*
	 * Turn off stuff we don't use.
	 */
	CCP1CON = 0;		/* Make sure the ECCP is turned off */
        ADCON0 = 0;          	// Turn A/D off
	ADCON1 = ADCON1_SET;

	/*
	 * Set up the I/O ports.
	 */
	TRISA = TRISA_SET;
	TRISB = TRISB_SET;
	TRISC = TRISC_SET;

	PORTA = 0;		/* Clear the output latches */
	PORTB = 0;
	PORTC = 0;

#ifndef ERASE
	run_init();		// call this before interrupts start.
#endif

	/* 
	 * Set up the interrupt system.
	 */
	IPEN = 1;		// Turn on the priority interrupt system.
	GIEH = 1;		// Enable high priority interrupts.
	GIEL = 1;		// Enable low priority interrupts.

	/*
	 * Timer 0: free-running 12 MHz counter
	 */
	TMR0IE = 0;		// Timer 0 never interrupts.
	TMR0H = 0;		// Clear the counter
	TMR0L = 0;
	T0CON = 0b.1000.1000;	// Timer 0 is on with 1x prescaler

	/*
	 * Timer 1: 10,000 Hz interrupt source.
	 */
	TMR1IE = 0;
	TMR1IP = 1;		// High priority interrupt.
	T1CON = 0b.1000.0001;	// enabled with a 1x prescaler.
	TMR1H = T1SET_H;
	TMR1L = T1SET_L;
	TMR1IE = 1;

	/*
	 * Timer 3: 22.9 HZ interrupt source
	 */
	TMR3IE = 0;
	TMR3IP = 0;		// low priority interrupt
	T3CON = 0b.1011.0001;	// Running at FOSC/4 / 8
	TMR3IE = 1;

/*xxx*/blink_init(); blink_sdelay();
	df_init();

	LED_GREEN = 1;
	find_space();
	LED_GREEN = 0;
	validate_space();

	prepare_dataflash();

/*
 * If we are the erase program then we are done.
 * If we are not the erase program, capture the data
 * and then we are done.
 */
#ifdef ERASE
	found_eof = 0;
#else // ERASE
	run_main();
#endif //ERASE

	/*
	 * When done, blink the green LED asymetrically.
	 * Red LED blinks lightly if we got here because of EOF.
	 */
	for (;;) {
		if (found_eof && (lbolt & 0x3F) == 2) {
			LED_RED = 1;
		} else {
			LED_RED = 0;
		}

		if (lbolt & 0x1E) {
			LED_GREEN = 1;
		} else {
			LED_GREEN = 0;
		}
	}
}
