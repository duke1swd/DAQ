/*
 * This program tests the 10KHz sample interrupt rate
 * against the TOD crystal.
 *
 * This program is written to assume loaded by boot2.
 */

#include "int18xxx.h"
#include "hexcodes.h"

/*
 * Define the I/O infrastructure.
 */
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2

#pragma	bit SD_CS		@ PORTC.6	// dataflash select

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
#define	T1PD		1196		// 10 KHz

#define	T1SET_H		((-T1PD + 21) >> 8)
#define	T1SET_L		((-T1PD + 21) & 0xff)

#pragma resetVector 0x1000	// do the reset vector manually

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
		LED_RED = 1;
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
		LED_RED = 1;
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

void
waitms(void)
{
	unsigned char i, j;

	for (j = 7; j; j--)
		for (i = 1; i; i++)
			nop2();
}

uns8 lbolt;	// incremented at 22.9 HZ.


#define	DATAFLASH

#ifdef DATAFLASH
#define	sd_init		df_init
#define	sd_read		df_read
#define	sd_lba		df_lba
#define	DATA_CS		SD_CS
#endif // DATAFLASH

#define BLINK_NO_EPROM
#include "blink.c"
#include "tod.c"

void adc_isr(void);

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
	adc_isr();
}

/*
 * Timer 3 provides low priority time base for blinking LEDs.
 */
void
timer3_interrupt()
{
	TMR3IF = 0;
	lbolt++;
}

bit running;
uns24 counter;

void
adc_isr(void)
{
	if (running)
		counter++;
}

void
wait_till_tick()
{
	uns8 v1, v2;

	v1 = tod_receive_byte(TOD_SEC);
	do {
		v2 = tod_receive_byte(TOD_SEC);
	} while (v1 == v2);
}

void
Write_b_eep(unsigned char badd, unsigned char bdata)
{
	EEADR = badd;
  	EEDATA = bdata;
  	EEPGD = 0;
	CFGS = 0;
	WREN = 1;

	EECON2 = 0x55;
	EECON2 = 0xAA;
	WR = 1;

	WREN = 0;
	
	while (WR)
		;
}

void
display(void)
{
	GIEH = 0;
	GIEL = 0;
	LED_GREEN = 0;
	LED_RED = 0;
	Write_b_eep(1, counter.high8);
	Write_b_eep(2, counter.mid8);
	Write_b_eep(3, counter.low8);
	LED_GREEN = 1;
}

void
doit(void)
{
	uns8 seconds;

	wait_till_tick();
	running = 1;
	for (seconds = 0; seconds < 100; seconds++) {
		LED_GREEN = seconds.0;
		wait_till_tick();
	}
	running = 0;

	display();

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

	/* 
	 * Set up the interrupt system.
	 */
	IPEN = 1;		// Turn on the priority interrupt system.
	GIEH = 1;		// Enable high priority interrupts.
	GIEL = 1;		// Enable low priority interrupts.

	/*
	 * Timer 0: free-running 10 MHz counter
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

	doit();
	while (1); 
}
