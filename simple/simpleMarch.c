/* 
 * This is the version that ran during the static tests
 * of March 29, 2008.
 */
//#define ADC_TEST

/*
 * Firmware for the small A/D converter board.
 *
 * This is the not-quite-so-simple A/D converter firmware.
 *
 * Supports 2 analog and 1 digital channel.
 * Goal is 10,000 samples/second/channel (20K samples/sec total).
 *
 * Simplifications include no UI and no file format.
 */

#include "int18xxx.h"

//#define	FAIL_ON_FULL

/*

Notes:
 - This version coded for a PIC 18F1320
 - Timing assumes a 10MHz crystal running with FOSC = 40MHz.


 Threading model:
   Data gathering is driven off of timer 1.  It interrupts
   periodically.  (Rate, TBD.  Fastest possible sustainable rate.)
   Each interrupt gathers two data samples, one from each of two
   channels, and stores them in a ring buffer.

   LED display is driven of timer 2.  It interrupts at about 150 Hz.
   This is software divided by 10, to call the LED code at about 10 Hz.

   Base level code tries to keep the buffer empty by writing it to
   the flash memory.

   The ring buffer is big enough to hold some number of 4-byte samples.
   The buffer will be as big as possible to ensure best possible
   thoughput.

   Both flash and A/D converter are driven by software emulation of SPI.
   The A/D driver code attempts to generate a 1MHz SPI clock and
   to spend about 50 us gathering each sample.   The goal is to spend
   about 50% of time in the ISS.

   Baselevel code will clock the SD card as fast as it can, as the
   cards are rated to 20 MHz.

   Timer 0 is a free running 10 MHz counter.  Provides time base.

   Timer 1 generates 1,000.00 interrupts per second.  Drives the
   A/D sampling.

   Timer 2 is not used.

   Timer 3 runs free, generating 10,000,000/65536 = 152.6 interrupts/sec.
   This generates the low priority interrupt used to control the LED.

   Pins needed:
   	Each device (ADC, MMC) needs 4 pins:
		CLK: bit clock for data transfers
		DATA_I: data transfered from device to PIC
		DATA_O: data transfered from PIC to device
		SEL: chip select for the device.

   Note on pressure sensor CAT-5 wiring:
	Orange is +
	Green is -
	Blue is signal.

   Port bit assignments:

   PIC  ext  Port	  Description
   pin  pin  name
   ---  ---  ---    -----------
    1        RA0    Abort code output (for debugging)
    2   14   RA1    ADC Chip Select
    6   15   RA2    ADC Clock
    7   12   RA3    ADC Data from PIC to ADC

    3   13   RA4    ADC Data from ADC to PIC
    4        RA5    digital input line (input only)
   15        RA6    OSC2
   16        RA7    OSC1

    8        RB0    status LED out
    9        RB1    Abort Triger out
   17    2   RB2    MMC Data from PIC to MMC
   18    5   RB3    MMC Clock

   10        RB4    record switch
   11        RB5    unused
   12    7   RB6    MMC Data from MMC to PIC
   13    1   RB7    MMC Chip Select


   MMC (SD) Pin Assignments
   
   An MMC card has 7 pins.  Looking at the metal with the cut corner
   top left, the pins are 1-7, left to right.  An SD has 2 more pins,
   one further left and one further right, that we ignore.

   MMC       Signal
    1         Chip Select (inverted)
    2         Data In (to MMC)
    3         GND
    4         +3.3V
    5         Clock
    6         GND
    7         Data Out (to PIC)

   ADC       Signal
    14        Chip Select (inverted)
    13        Data Out (to PIC)
    15        Clock
    12        Data In (from PIC)


  Note: the MMC input pins need a resistor divider to prevent over-voltage.
  We use 2.2/3.9 K ohms.


  Note: The I/O ports are named here.
        If you change any of them, make sure to change the
	TRIS register settings as well.
 */


#define	TRISA_SET	0b.0011.0000	/* RA4, 5 are port A inputs */
#define	TRISB_SET	0b.0101.0000	/* RB4, 6 are port B inputs */

#define	ADCON1_SET	0b.0111.1111	/* No analog inputs */


/*
 * LTC1293 A/D converter
 */

#pragma bit ADC_CLK       @ PORTA.2	// pin  6
#pragma bit ADC_DATA_I    @ PORTA.4	// pin  3
#pragma bit ADC_DATA_O    @ PORTA.3	// pin  7
#pragma bit ADC_SEL       @ PORTA.1	// pin  2

/*
 * Flash memory.
 */
#pragma bit MMC_CLK       @ PORTB.3	// pin 18
#pragma bit MMC_DATA_I    @ PORTB.6	// pin 12
#pragma bit MMC_DATA_O    @ PORTB.2     // pin 17
#pragma bit MMC_SEL       @ PORTB.7     // pin 13

/*
 * Other Pins
 */
#pragma bit ABORT_OUT     @ PORTA.0	// output for error codes
#pragma bit DIGITAL_INPUT @ PORTA.5	// sampled digital input
#pragma bit ABORT_TRIG    @ PORTB.1	// trigger for error codes
#pragma bit LED_OUT       @ PORTB.0	// status LED
#pragma bit RECORD_SWITCH @ PORTB.4	// input control

/*
 * Control for Timer 1.
 */
#define	T1PD		1250L		// number of FOSC/4 cycles for 8KHz
#define	T1SET_H		((-T1PD + 23L) >> 8)
#define	T1SET_L		((-T1PD + 23L) & 0xff)

/*******************
 *                 *
 * Abort routines. *
 *                 *
 *******************/

#define	ABORT_UNKNOWN_H		0x1	// unknown high-priority interrupt.
#define	ABORT_UNKNOWN_L		0x2	// unknown low-priority interrupt.
#define	ABORT_RESET		0xf	// unknown reset

void abort_h(unsigned char code);
void abort_l(unsigned char code);
void abort_b(unsigned char code);

/*********************
 *                   *
 * Error Management. *
 *                   *
 *********************/

#define	ERROR_USR_1		1	// record switch found on a power up
#define	ERROR_USR_2		2	// not used
#define	ERROR_USR_3		3	// attemting to record when done
#define	ERROR_USR_4		4	// not used
#define	ERROR_USR_5		5	// not used
#define	ERROR_READ_TO		6	// timeout on MMC data read.
#define	ERROR_LONG_TO		7	// timeout waiting for MMC ready
#define	ERROR_IDLE_TO		8	// mmc won't come idle
#define	ERROR_READY_TO		9	// mmc won't come ready
#define	ERROR_INIT_ERROR	10	// unknown mmc initialization error
#define	ERROR_APP_CMD		11	// mmc app cmd failure (??).
#define	ERROR_RING_FULL		12	// ring buffer overrun.
#define	ERROR_WR_ERASE		13	// command to mmc failed
#define	ERROR_WR_BLK		14	// command to mmc failed
#define	ERROR_WRITE_FAILED	15
#define	ERROR_WRITE_MULTIPLE_FAILED 16
unsigned char error_code;

/*********************
 *                   *
 * Global Variables. *
 *                   *
 *********************/

uns16 display_value;		// value displayed on status pin.
unsigned char file_number;
 

/***********************
 *                     *
 * Interrupt routines. *
 *                     *
 ***********************/

void highPriorityInt(void);

/*
 * The high priority interrupts drive A/D sampling
 */
#pragma origin 0x8
interrupt highPriorityIntServer(void)
{
	// W, STATUS and BSR are saved to shadow registers
	// handle the interrupt
	highPriorityInt();
	// restore W, STATUS and BSR from shadow registers
#pragma fastMode
}

/*
 * Forward calls from the ISS routines.
 */
void timer1_interrupt();
void timer3_interrupt();
void ssp1_interrupt();

/*
 * The low priority interrupts drive pseudo DMA traffic to the MMC
 */
#pragma origin 0x18
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
	else
		abort_l(ABORT_UNKNOWN_L);

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
	else
		abort_h(ABORT_UNKNOWN_H);

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

/****************
 *              *
 * A/D Control. *
 *              *
 ****************/

/*
 * Notes on LTC1193.
 * Data is transmitted on falling clock, captured on rising clock
 *      (in both directions.)
 * 
 */

#define	ADC_SAMPLE_0	0b.1100.0111;// start, channel 0, uni, msb, full power
#define	ADC_SAMPLE_1	0b.1110.0111;// start, channel 1, uni, msb, full power
#define	ADC_SAMPLE_2	0b.1100.1111;// start, channel 2, uni, msb, full power
#define	ADC_SAMPLE_3	0b.1110.1111;// start, channel 3, uni, msb, full power

/*
 * Ring buffer.
 */
void error_h(unsigned int code);
void error_check();

/*
 * The ring buffer is sized in samples.  Each sample
 * is 2 16-bit words.
 */

#define	RING_SIZE	48		// number of entries in ring buffer.
uns16 ring[RING_SIZE];			// ring buffer, not in the access bank

unsigned char ring_in;			// next locn to fill in the ring.
unsigned char ring_out;			// next locn to drain in the ring.
unsigned char ring_n;			// number of samples in ring
bit ring_filled;			// we lost some samples

void
ring_init()
{
	ring_filled = 0;
	ring_in = 0;
	ring_out = 0;
	ring_n = 0;
}

/*
 * Push data into the ring buffer.
 * Sets an error status and discards data on ring buffer full.
 * Must be called from high priority interrupt level.
 */

#ifdef FAIL_ON_FULL

#define	ring_push(data) \
	if (ring_n >= RING_SIZE) { \
		error_h(ERROR_RING_FULL); \
	} else { \
		ring[ring_in] = data; \
		ring_in++; \
		if (ring_in >= RING_SIZE) \
			ring_in = 0; \
		ring_n++;  \
		ring_filled = 0; \
	} 

#else	// FAIL_ON_FULL

#define	ring_push(data) \
	if (ring_n >= RING_SIZE) { \
		ring_filled = 1; \
	} else { \
		ring[ring_in] = data; \
		ring_in++; \
		if (ring_in >= RING_SIZE) \
			ring_in = 0; \
		ring_n++;  \
		ring_filled = 0; \
	} 

#endif	// FAIL_ON_FULL

/*
 * Pull data from the ring buffer.
 * Waits forever for more data.  Checks for error status on underrun too.
 * Must be called from base level.
 *
 * Use of GIEH=0 is problematic, but normally we expect this
 * system to run with the ring buffer empty, so the GIE glitch
 * will occur just after we've service a timer 1 interrupt. 
 * Under these conditions there will be no sample jitter.
 *
 * Under load, we'll have jitter.  To investigate: is the ring_n--
 * coding atomic wrt interrupts?  Seems very likely as it is a single
 * instruction.
 *
 * Two versions.  One pulls the first word of the sample, one the next.
 */

uns16
ring_pop()
{
	uns16 data;

	while (ring_n == 0) {
		error_check();
	}

	data = ring[ring_out];
	ring_n--;
	ring_out++;
	if (ring_out >= RING_SIZE)
		ring_out = 0;

	return data;
}

/*
 * This routine pulls a sample off the ADC chip.
 *
 * For now it is stubbed out.
 */
unsigned int adc_running:1;

#define	ADC_TX_BIT_0	\
	ADC_CLK = 1; \
	ADC_DATA_O = 0; \
	nop(); \
	nop2(); \
	ADC_CLK = 0; \
	nop2(); \
	nop2()

#define	ADC_TX_BIT_1	\
	ADC_CLK = 1; \
	ADC_DATA_O = 1; \
	nop(); \
	nop2(); \
	ADC_CLK = 0; \
	nop2(); \
	nop2()

/*
 * Receive a bit.  Actually, ORs the bit in, so clear the word first.
 */

#define	ADC_RX_BIT(bit) \
	ADC_CLK = 1; \
	if (ADC_DATA_I) bit = 1; \
	ADC_DATA_O = 0; \
	nop(); \
	ADC_CLK = 0; \
	nop2(); \
	nop2()

/*
 * Get 1 sample from each of 2 analog channels and push into the
 * ring buffer.
 *
 * This is a software implementation of SPI mode 0, 1 MHz clock.
 *
 */

void
adc_sample()
{

	uns16 value;
	int extra:1;

	if (!adc_running)
		return;

	ADC_SEL = 0;	// select the part.
	nop2();
	nop2();

	value = file_number;	// this goes here for timing reasons

	/* 
	 * ADC_SAMPLE_0 0b.1100.0111
	 */

	ADC_TX_BIT_1;
	ADC_TX_BIT_1;
	ADC_TX_BIT_0;
	ADC_TX_BIT_0;

	ADC_TX_BIT_0;
	ADC_TX_BIT_1;
	ADC_TX_BIT_1;
	ADC_TX_BIT_1;

	ADC_RX_BIT(extra);	// discard the first bit
	ADC_RX_BIT(extra);	// discard the first bit
	ADC_RX_BIT(value.15);
	ADC_RX_BIT(value.14);
	ADC_RX_BIT(value.13);
	ADC_RX_BIT(value.12);
	ADC_RX_BIT(value.11);
	ADC_RX_BIT(value.10);
	ADC_RX_BIT(value.9);
	ADC_RX_BIT(value.8);
	ADC_RX_BIT(value.7);
	ADC_RX_BIT(value.6);
	ADC_RX_BIT(value.5);
	ADC_RX_BIT(value.4);

	/*
	 * Note: CS must remain high for at least 500 nanoseconds.
	 */
	ADC_SEL = 1;	// deselect the part.
	display_value = value;

	if (DIGITAL_INPUT)
		value.3 = 1;

	if (ring_filled)
		value.2 = 1;

	ring_push(value);
}


/*
 * Init the ADC driver and ring buffer.
 */
void
adc_init()
{
	adc_running = 0;
	ring_init();
}

/*
 * Start the ADC sampling into the ring buffer.
 */
void
adc_start()
{
#ifndef ADC_TEST
	GIEL = 0;		// turn off status display
#endif // ADC_TEST
	adc_running = 1;
}

/*
 * Stop the ADC sampling into the ring buffer.
 */
void
adc_stop()
{
	adc_running = 0;
	GIEL = 1;		// turn on status display
}

/*
 * Test the ADC driver by pulling 250 samples of data out of the ring
 * buffer.
 */
void
adc_test()
{
	uns16 data;
	unsigned int i;

	adc_start();
#ifdef ADC_TEST 
	for (;;) {
		data = ring_pop();
	}
#else
	for (i = 0; i < 250; i++) {
		data = ring_pop();
	}
#endif
	adc_stop();
}

/****************
 *              *
 * LED Control. *
 *              *
 ****************/

unsigned char led_on;		// units are 100 ms. 
unsigned char led_off;		// units are 100 ms.

unsigned char led_state;	// local
unsigned char led_divider; 	// local

/*
 * Normally, the LED blinks out a status.
 * The blink on/off times are controlled by the led_on/led_off states.
 */
void
led_control()
{
	/*
	 * Called at about 150 Hz.  Divide down to 10 Hz.
	 */
	if (led_divider < 15) {
		led_divider++;
		return;
	}
	led_divider = 0;

	if (led_state < led_on)
		LED_OUT = 1;
	else
		LED_OUT = 0;

	led_state++;
	if (led_state >= led_on + led_off)
		led_state = 0;
}

/*
 * If an error condition has been set, LED control
 * is handled here.
 *
 * According to the POC, errors are signaled as follows:
 	2 seconds off
	1 second on
	1 second off
	error code, count number of blinks, .25 on / .25 off.
 * There is no error code 0.
 */
void
error_display()
{
	unsigned char adjusted_error_code;

	/*
	 * Error state calculations are in units of 0.25 seconds.
	 * Since we are called at 152 Hz we must divide the call rate
	 * by 38.
	 */
	if (led_divider < 38) {
		led_divider++;
		return;
	}
	led_divider = 0;

	adjusted_error_code = error_code * 2;
	adjusted_error_code += 16;

	if (led_state < 8)
		LED_OUT = 0;		// 2 seconds off
	else if (led_state < 12)
		LED_OUT = 1;		// 1 second on
	else if (led_state < 16)
		LED_OUT = 0;		// 1 second off
	else if (led_state.0 == 1)
		LED_OUT = 0;		// .25 off each blink
	else if (led_state < adjusted_error_code)
		LED_OUT = 1;		// .25 on, counted.
	else {
		LED_OUT = 0;
		led_state = 0;		// repeat the cycle.
	}
	led_state++;
}

/*
 * Set an error code and don't come back.
 * Can only be called on base level.
 */
void
set_error(unsigned int code)
{
	GIEH = 0;
	GIEL = 0;
	error_code = code;
	led_divider = 0;
	led_state = 0;
	GIEH = 1;
	GIEL = 1;

	for (;;)
		nop();
}

/*
 * Set an error code.  Can be called from high priority interrupt level.
 * Returns.  Base level must at some point call error_check().
 */
void
error_h(unsigned int code)
{
	error_code = code;
	led_divider = 0;
	led_state = 0;
}

/*
 * Called from base level to see if interrupt code has set an
 * error.  Does not return if there is an error condition.
 */
void
error_check()
{
	if (error_code) {
		GIEL = 1;	// make sure the LED interrupts are running.
		for(;;)
			nop();
	}
}

/*******************
 *                 *
 * Status Display. *
 *                 *
 *******************/

uns16 local_display_value;
unsigned char display_state;

/*
 * During normal operations the 12-bit display value
 * is clocked out at 152 Hz onto pin 0.
 */
void
status_bit()
{
	display_state++;
	display_state &= 0x1f;

	if (display_state.0 == 0) {
		ABORT_OUT = 0;
		ABORT_TRIG = 0;
	} else if (display_state == 1) {
		ABORT_OUT = 1;
		ABORT_TRIG = 1;
		local_display_value = display_value;
	} else {
		ABORT_OUT = local_display_value.11;
		local_display_value <<= 1;
	}
}


/**********************
 *                    *
 * Timer 1 Interrupt. *
 *                    *
 **********************/
void
timer1_interrupt()
{
	if (TMR0L.0)
		nop2();
	/*
	 * We get here exactly every 10000 instructions.
	 */

	TMR1H = T1SET_H;	// set up the timer again
	TMR1L = T1SET_L;
	TMR1IF = 0;		// clear the interrupt source

	/* If there is an error don't do anything. */
	if (error_code)
		return;

	/* get the next sample */
	adc_sample();
}


/**********************
 *                    *
 * Timer 3 Interrupt. *
 *                    *
 **********************/
void
timer3_interrupt()
{
	/*
	 * Called at 10,000,000/65536 = 152.59 Hz.
	 */
	TMR3IF = 0;		// clear the interrupt source

	/* show the next status bit */
	status_bit();

	/* If there is an error, display it but take no other action. */
	if (error_code) {
		error_display();
		return;
	}

	/* Normal LED display control. */
	led_control();
}


/*********************************
 *                               *
 * Initialization and Reset Code *
 *                               *
 *********************************/

/*
 * Deal with resets
 */
void
handle_reset()
{
	unsigned char v;

	if (RCON.1 == 0) {
		RCON.1 = 1;
		RCON.0 = 1;
		return;		// normal power on reset
	}
	v = RCON;
	if (STKPTR.7)
		v.6 = 1;
	if (STKPTR.6)
		v.5 = 1;

	// if we need to debug resets, we'll have to display v here.
	abort_b(ABORT_RESET);
}

/*
 * This routine does basic chip setup.
 */
void
PIC_setup()
{
	ADCON0 = 0;		/* Turn off on-chip A/D converter.  */
	ADCON1 = ADCON1_SET;	/* Configure no analog ports. */
	CCP1CON = 0;		/* Make sure the ECCP is turned off */

	TRISA = TRISA_SET;
	TRISB = TRISB_SET;

	PORTA = 0;		/* Clear the output latches */
	PORTB = 0;

	clearRAM();

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
	 * Timer 1: 1000 Hz interrupt source.
	 */
	TMR1IE = 0;
	TMR1IP = 1;		// High priority interrupt.
	T1CON = 0b.1000.0001;	// enabled with a 1x prescaler.
	TMR1H = T1SET_H;
	TMR1L = T1SET_L;
	TMR1IE = 1;

	/*
	 * Timer 3: 152 Hz interrupt source
	 */
	TMR3IE = 0;
	TMR3IP = 0;		// Low priority interrupt.
	T3CON = 0b.1000.0001;	// enabled with a 1x prescaler.
	TMR3H = 0;
	TMR3L = 0;
	TMR3IE = 1;
}

/********************
 *                  *
 * MMC Card Driver. *
 *                  *
 ********************/

#define	BLOCKSIZE		4096	// multiple of a sector, can be tuned.
#define	SECTORSIZE		512L	// constant of the universe
#define	SECTORS_PER_BLOCK	(BLOCKSIZE / SECTORSIZE)
#define	RECORDSIZE		2L	// in bytes
#define	RECORDS_PER_SECTOR	(SECTORSIZE / RECORDSIZE)

uns16 addr_low, addr_high;	// disk address to write to next.

/*
 * MMC card control
 * The design for this code taken from two sources.
 * First:
		http://www.captain.at/electronics/pic-mmc/
 *    which also gives credit to:
		http://home.wtal.de/Mischka/MMC/
 * Second:
		http://elm-chan.org/docs/mmc/mmc_e.html
 *
 * This code does all the SPI stuff in software.
 * This code all runs at base level.
 *
 * According to the second reference (above) we use SPI Mode 0.
 * (See http://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus
 * for a definition of SPI modes.)
 *
 * SPI Mode 0 means that reads occur on clock rising edge, while
 * writes occur on clock falling edge.
 *
 * The MMC cards are supposed to support clock rates up to 25 MHz.
 * Thus if our electronics are clean, we can support switching
 * the clock on adjacent instructions.
 *
 * The MMC code here implements a half-duplex version of SPI.
 */

/*
 * Set the clock line.  Delay may be added here if necessary
 * for the circuit.  Clean circuit shouldn't need any delay.
 */
#define	mmc_clock(c)	nop2(); nop2(); MMC_CLK = c

/*
 * These macros send a bit to the MMC.
 * Assumes the the MMC is selected and clock is high.
 */
//#define mmc_spi_tx_bit(b) MMC_CLK = 0; MMC_DATA_O = 0; if (b) MMC_DATA_O = 1; MMC_CLK = 1
//#define mmc_spi_tx_bit_1 MMC_CLK = 0; MMC_DATA_O = 0; MMC_DATA_O = 1; MMC_CLK = 1

#define mmc_spi_tx_bit(b)	mmc_clock(0); \
				MMC_DATA_O = 0; \
				if (b) MMC_DATA_O = 1; \
				mmc_clock(1)


#define mmc_spi_tx_bit_1	mmc_clock(0); \
				MMC_DATA_O = 1; \
				mmc_clock(1)

			  

/*
 * This routine reads a bit from the MMC.
 * ASSUMPTIONS:
		MMC_SEL = 0
		MMC_DATA_O = 1 (not sure why this is required / assumed)
		MMC_CLK = 1

   NOTE: this function has been manually inlined in the three places it is used
 */

#define	mmc_read_bit(b) \
	nop2();nop2(); \
	mmc_clock(0); \
	nop2();nop2(); \
	mmc_clock(1); \
	if (MMC_DATA_I) b = 1

/*
 * Send a byte to the MMC.
 */
void
mmc_spi_tx(unsigned char out)
{
	mmc_spi_tx_bit(out.7);
	mmc_spi_tx_bit(out.6);
	mmc_spi_tx_bit(out.5);
	mmc_spi_tx_bit(out.4);
	mmc_spi_tx_bit(out.3);
	mmc_spi_tx_bit(out.2);
	mmc_spi_tx_bit(out.1);
	mmc_spi_tx_bit(out.0);
	return;
}

void
mmc_spi_tx_16(uns16 data)
{
	mmc_spi_tx_bit(data.7);
	mmc_spi_tx_bit(data.6);
	mmc_spi_tx_bit(data.5);
	mmc_spi_tx_bit(data.4);
	mmc_spi_tx_bit(data.3);
	mmc_spi_tx_bit(data.2);
	mmc_spi_tx_bit(data.1);
	mmc_spi_tx_bit(data.0);
	mmc_spi_tx_bit(data.15);
	mmc_spi_tx_bit(data.14);
	mmc_spi_tx_bit(data.13);
	mmc_spi_tx_bit(data.12);
	mmc_spi_tx_bit(data.11);
	mmc_spi_tx_bit(data.10);
	mmc_spi_tx_bit(data.9);
	mmc_spi_tx_bit(data.8);
}

/*
 * Wait for a response from the MMC card.
 * Read it and send it back to the caller.
 * ASSUMPTIONS:
		MMC_SEL = 0
		MMC_SD0 = 1
		MMC_CLK = 1
 */
#define	MAX_READ_BIT_TIMES	64
unsigned char
mmc_spi_read()
{
	bit b;
	unsigned char r;
	int i;
	int j:16;

	for (i = 0; i < MAX_READ_BIT_TIMES; i++) {
		/* Read a bit from the MMC card */
		mmc_clock(0);
		mmc_clock(1);
		/*xxx*/mmc_clock(1);	// delay needed?
		if (MMC_DATA_I == 0)
			goto data_found;
	}
	set_error(ERROR_READ_TO);	// no response within specified time

	/*
	 * Note: read only 7 bits here.
	 * I think this is because the first bit was read by the above.
	 */
  data_found:
	r = 0;
	mmc_read_bit(r.6);
	mmc_read_bit(r.5);
	mmc_read_bit(r.4);
	mmc_read_bit(r.3);
	mmc_read_bit(r.2);
	mmc_read_bit(r.1);
	mmc_read_bit(r.0);

	/*
	 * Wait for ready.
	 * I believe that on a 40MHz clock this waits about half a second.
	 */
	for (j = 1; j; j++) {
		mmc_clock(0);
		mmc_clock(1);
		/*xxx*/mmc_clock(1);	// delay needed?
		if (MMC_DATA_I)
			goto ready_found;
	}

	// never came ready on read of response to a command
	set_error(ERROR_LONG_TO);

  ready_found:
	mmc_spi_tx(0xff);	// I'm told it needs 8 more clocks
	return r;
}

/*
 * Send a command block to the MMC card.
 */
unsigned char
mmc_command(unsigned char command, uns16 AdrH, uns16 AdrL)
{
	unsigned char r;
	mmc_spi_tx_bit_1;	// not clear these bit times are necessary
	mmc_spi_tx_bit_1;
	mmc_spi_tx(command);
	mmc_spi_tx(AdrH.high8);
	mmc_spi_tx(AdrH.low8);
	mmc_spi_tx(AdrL.high8);
	mmc_spi_tx(AdrL.low8);

	// the only command that requires a CRC is CMD0, which uses 95
	mmc_spi_tx(0x95);

	r = mmc_spi_read();	// return the last received character
	return r;
}

/*
 * The Following MMC commands and tokens are used.
 */

// reset
#define	MMC_GO_IDLE_STATE				(0x40 |  0)

// MMC initialize
#define	MMC_SEND_OP_COND				(0x40 |  1)

// MMC write one block
#define	MMC_WRITE_BLOCK					(0x40 | 24)

// MMC write multiple blocks
#define	MMC_WRITE_MULTIPLE_BLOCK			(0x40 | 25)

// following is an SDC command
#define	MMC_APP_CMD					(0x40 | 55)

// number of sectors in multi-block write
#define	SDC_SET_WR_BLOCK_ERASE_COUNT			(0x40 | 23)

// mark the start of a WRITE_BLOCK data section
#define	MMC_WRITE_BLOCK_TOKEN				0xfe
#define	MMC_WRITE_MULTIPLE_BLOCK_TOKEN			0xfc

// mark the end of a multi-block transfer
#define	MMC_STOP_TRAN					0xfd

// OK response to a write.
#define	MMC_WRITE_SUCCESS				0b.0010.1000

/*
 * MMC R1 responses
 */
#define MMC_R1_IDLE					0b.0000.0001

/*
 * Init routine.
 * Loops for ever on missing card.
 * Calls abort() on errors.
 */

void
mmc_init()
{
	char i, c;
	int count : 16;

	// reset disk address to start of the card
	addr_low = 0;
	addr_high = 0;
	
	// start MMC in native mode
	// Spec says, "set CS and DI high, then send
	// at least 74 clock pulses.
	
	mmc_clock(0);	// need stable clock before anything else.
	MMC_SEL = 1;

	// send 10*8=80 clock pulses
	for(i=0; i < 10; i++)
		mmc_spi_tx(0xFF);
	MMC_SEL = 0;			// enable MMC
		
	// Reset the MMC and force into SPI mode
	for (c = 0; c < 50; c++) {
		i = mmc_command(MMC_GO_IDLE_STATE, 0, 0);
		if (i == MMC_R1_IDLE)
			goto idling;
	}
	//abort2(ABORT_IDLE_TO, i);
	set_error(ERROR_IDLE_TO);

  idling:;

	/*
	 * Initialize the MMC card.
	 * Spec says, send the SEND_OP_COND command
	 * over and over again until the IDLE mode bit is no longer
	 * set in the response.
	 */
	for (count = 1; count; count++) {
		i = mmc_command(MMC_SEND_OP_COND, 0, 0);
		if (i != MMC_R1_IDLE)
			break;
	}
	MMC_SEL = 1;			// disable before the next command
		
	// If the MMC is initialized, then return.  All is well.
	if (i == 0)
		return;

	// If the MMC card never comes ready, then abort.
	if (!count)
		set_error(ERROR_READY_TO);

	/*
	 * We received a non-zero response other than IDLE.
	 * Implies some kind of error.
	 */
	set_error(ERROR_INIT_ERROR);
}

/*
 * Write a single multi-block record.
 */
void
mmc_write_blocks()
{
	unsigned char r;
	unsigned char rec;
	unsigned char i;
	uns16 j;
	uns16 data;
	
	MMC_SEL = 0;	// Enable
	r = mmc_command(MMC_APP_CMD, 0, 0);
	if (r != 0)
		set_error(ERROR_APP_CMD);
	r = mmc_command(SDC_SET_WR_BLOCK_ERASE_COUNT, 0,
			(uns16)SECTORS_PER_BLOCK);
	if (r != 0)
		set_error(ERROR_WR_ERASE);

	r = mmc_command(MMC_WRITE_MULTIPLE_BLOCK, addr_high, addr_low);		
	if (r != 0)
		set_error(ERROR_WR_BLK);

	// Loop once per sector

	for (i = 0; i < SECTORS_PER_BLOCK; i++) {
		mmc_spi_tx(MMC_WRITE_MULTIPLE_BLOCK_TOKEN);

		if (adc_running) {
			// send the actual data.
			for (j = 0; j < RECORDS_PER_SECTOR; j++) {
				// generate the data

				/*
				 * The number of bytes sent in this code block
				 * must exactly match RECORDSIZE.
				 */
				data = ring_pop();
				mmc_spi_tx_16(data);
			}
		} else {
			// send zeros as EOF
			for (j = 0; j < RECORDS_PER_SECTOR; j++) {
				mmc_spi_tx_16(0L);
			}
		}

		r = mmc_spi_read();
		if (r != MMC_WRITE_SUCCESS)
			set_error(ERROR_WRITE_FAILED);

		addr_low += 512;
		if (addr_low == 0)
			addr_high++;
	}

	// stop the multi-block write
	mmc_spi_tx(MMC_STOP_TRAN);
	r = mmc_spi_read();
	if (r != 0)
		set_error(ERROR_WRITE_MULTIPLE_FAILED);

	MMC_SEL = 1;	// disable
}

/*
 * Shut down the card.  Nothing really to do here.
 */
void
mmc_term()
{
}

/************************
 *                      *
 * Main Sequencing Code *
 *                      *
 ************************/

/*
 * Wait for specified number of milliseconds
 * This code assumes a 40MHz clock.
 */
void
wait(unsigned int n)
{
	int i, j;

	for ( ; n; n--)
	    for (j = 5; j > 0; j--)
		for (i = 56; i; i++) {
			nop2();
			nop2();
			nop();
		}
}

#include "eprom.c"

/*
 * Execution starts here.
 */
void
main()
{

	unsigned char i;
	bit local_led_state;

	handle_reset();

	PIC_setup();

	/*
	 * First, wait .25 second.
	 */
	led_on = 1;
	led_off = 0;
	wait(250);

	if (!RECORD_SWITCH)
		set_error(ERROR_USR_1);

	/*
	 * Next, test the ADC for .25 second.
	 */
	adc_init();
	adc_test();

	/*
	 * Now, set up the file number.
	 */
	i = eprom_read();
	if (i == 0)
		file_number = 0;
	file_number++;
	
	if (file_number > 3)
		file_number = 1;

	eprom_write();

	/*
	 * Adjust the file number so that it can be simply
	 * or'd into the data sample.
	 */
	file_number &= 0x3;

	/*
	 * Waiting to record.
	 */
	led_on = 1;
	led_off = 9;
	while (!RECORD_SWITCH == 0)
		;
	wait(100);	// in case the switch bounces.

	/*
	 * Recording.
	 */
	mmc_init();
	adc_start();
	led_on = 10;	// these won't be used until we stop recording.
	led_off = 10;
	while (!RECORD_SWITCH) {
		if (local_led_state) {
			LED_OUT = 0;
			local_led_state = 0;
		} else {
			LED_OUT = 1;
			local_led_state = 1;
		}
		mmc_write_blocks();
		error_check();
	}
	adc_stop();
	error_check();
	mmc_write_blocks();	// send a bunch of zeros as an EOF marker.
	mmc_term();
	error_check();	// last point where errors from interrupt are possible

	/*
	 * Terminal idle state
	 */
	for (;;)
		if (!RECORD_SWITCH)
			set_error(ERROR_USR_3);
}


/********************
 *                  *
 * Pin Display Code *
 *                  *
 ********************/

/*
 * Note, making this a macro allows the abort routine
 * to be called without error from all three threads.
 *
 * There are still some race conditions around setting
 * the display message, but so be it.
 */
#define	abort_bit(b)	if (b) ABORT_OUT = 1; abort_x()

unsigned char abort_code;

void
await()
{
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
	nop2();
}

void
abort_x()
{
	await();
	ABORT_OUT = 0;
	await();
}

void
abort_mark()
{
	ABORT_OUT = 1;
	await();
	ABORT_OUT = 0;
	await();
}

/*
 * Displays the code and data on the abort pin.
 * This routine does the display exactly once.
 */
void
pin_display()
{
	ABORT_TRIG = 1;
	abort_mark();
	ABORT_TRIG = 0;
	abort_mark();
	await();
	abort_bit(abort_code.3);
	abort_bit(abort_code.2);
	abort_bit(abort_code.1);
	abort_bit(abort_code.0);
}

/*
 * Shuts down the system and loops displaying the most recent message.
 */
unsigned char a_t, a_u;

void
abort()
{
	GIEH = 0;
	GIEL = 0;

	for (;; a_u++) {
		pin_display();
		for (a_t = 1; a_t; a_t++)
			nop2();
	}
}

void
abort_h(unsigned char code)
{
	abort_code = code;
	abort();
}


void
abort_l(unsigned char code)
{
	abort_code = code;
	abort();
}

void
abort_b(unsigned char code)
{
	abort_code = code;
	abort();
}
