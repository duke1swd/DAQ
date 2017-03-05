/*
 * Sensor check program for the HybridSky.com data capture board,
 *
 * Checks 3 sensors and 2 digital inputs.
 *
 * Designed for minimum power usage.
 */

#define	N_CHANNELS	2

/*
 * The DAQ 1.0 board is a PIC 18F2450.
 * The chip is running on a 12 MHz crystal with FOSC/4 = 12MHz.
 *
 * However, once this module comes up we put the chip into
 * at low-power mode and run at FOSC = 7750 Hz.
 */

#include "int18xxx.h"

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

#pragma bit RECORD_STOP		@ PORTA.1

#define	TRISA_SET		0b.0000.0011	// input switches
#define	TRISB_SET		0b.1100.0001	// optos and input from DF
#define	TRISC_SET		0b.0000.0010	// one input from ADC
#define	ADCON1_SET		0b.0000.1111	// no analog input

/*
 * Control for Timer 1.
 *
 * Set T1PD to be the number of instruction cycles between
 * calls between updates to lbolt.  155 is about 50Hz.
 *
 */
#define	T1PD		150		// 50 Hz.

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
 * Timer 1 drives the high priority interrupt.
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

	LED_GREEN = 1;
	LED_RED_LAT = 1;
	for (;;) ;

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
	//uns16 sv_FSR0 = FSR0;
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

	//FSR0 = sv_FSR0;
	//FSR1 = sv_FSR1;
	//FSR2 = sv_FSR2;
	//PCLATH = sv_PCLATH;
	//PCLATU = sv_PCLATU;
	//PRODL = sv_PRODL;
	//PRODH = sv_PRODH;
	//TBLPTR = sv_TBLPTR;
	//TABLAT = sv_TABLAT;
}

uns8 lbolt;	// incremented at 50 Hz.

/*
 * Include various modules.
 */
#include "slowmo.c"
#include "adc.c"
#include "run.c"

/*
 * Timer 1 controls the blinking speed.
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
	lbolt += 1;
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

	/* 
	 * Set up the interrupt system.
	 */
	IPEN = 1;		// Turn on the priority interrupt system.
	GIEH = 1;		// Enable high priority interrupts.
	GIEL = 1;		// Enable low priority interrupts.

	/*
	 * Timer 0: free-running instruction counter
	 */
	TMR0IE = 0;		// Timer 0 never interrupts.
	TMR0H = 0;		// Clear the counter
	TMR0L = 0;
	T0CON = 0b.1000.1000;	// Timer 0 is on with 1x prescaler

	/*
	 * Timer 1: 50 Hz interrupt source.
	 */
	TMR1IE = 0;
	TMR1IP = 1;		// High priority interrupt.
	T1CON = 0b.1000.0001;	// enabled with a 1x prescaler.
	TMR1H = T1SET_H;
	TMR1L = T1SET_L;
	TMR1IE = 1;

	/*
	 * Turn off the flash card and slow the clock.
	 *
	 * These things reduce battery draw to a minimum.
	 */
	DATA_CS = 1;
	enter_slowmo();
	run();
}
