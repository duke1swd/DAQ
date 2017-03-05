/*
 * This is a simple program to write data to the memory card.
 * Data begins at sector 128.
 *
 * Threading model:
 *  Base thread writes data to the SD card.
 *  Low priority interrupt thread runs off timer 1 and
 *   reads the ADC.
 *
 * Timer assignments.
 *  Timer 0: instruction clock, used to de-jitter.  No interrupts.
 *  Timer 1: sample clock, drives the sampling
 *  Timer 2: reserved for future use by the SD-SPI driver.  No interrupts.
 */

/*
 * Control for Timer 1.
 *
 * Set T1PD to be the number of instruction cycles between
 * calls to the ADC sample routine.  12,000 = 1KHz, 1,200 = 10KHz, etc.
 *
 */
//#define	T1PD		3000		// 4 KHz
//#define	T1PD		2000		// 6 KHz
#define	T1PD		1500		// 8 KHz
//#define	T1PD		1200		// 10 KHz
#define	T1SET_H		((-T1PD + 21) >> 8)
#define	T1SET_L		((-T1PD + 21) & 0xff)

#define	N_BUFFERS	3

#include <p18cxxx.h>

#include "HardwareProfile.h"
#include "SD-SPI.h"
#include "blink.h"
#include "adc.h"
#include <delays.h>

/*
 * Define digitial I/O pins.
 */
#define	RAW0_PIN	PORTBbits.RB7
#define	RAW0_PIN_TRIS	TRISBbits.TRISB7

#define	RAW1_PIN	PORTBbits.RB6
#define	RAW1_PIN_TRIS	TRISBbits.TRISB6

#define	RAW2_PIN	PORTBbits.RB5
#define	RAW2_PIN_TRIS	TRISBbits.TRISB5

#define	RAW3_PIN	PORTAbits.RA4
#define	RAW3_PIN_TRIS	TRISAbits.TRISA4


/**************************************************************
 * This is the standard code to recieve the remapped vectors. *
 **************************************************************/

/*
 * Remapped reset vector.
 */
extern void _startup (void);
#pragma code remapped_reset = 0x0800
void _reset (void)
{
	_asm goto _startup _endasm
}

/*
 * Remapped high priority interrupt vector.
 */
void timer_isr (void);
#pragma code hi_vector=0x808
void high_interrupt (void)
{
	_asm GOTO timer_isr _endasm
}

/*
 * Remapped low priority interrupt vector.
 */
void low_isr (void);
#pragma code low_vector=0x818
void low_interrupt (void)
{
	_asm GOTO low_isr _endasm
}

#pragma code


/*
 * No low priority interrupts are expected.
 */
#pragma interruptlow low_isr
void
low_isr(void)
{
	INTCONbits.GIEH = 0;	// Disable
	INTCONbits.GIEL = 0;	// Disable
	blink_error(13);
}

/*
 * The only interrupt we expect is the timer1 interrupt.
 */
#pragma interrupt timer_isr

/*
 * The only interrupt we expect is the timer1 interrupt.
 */
void
timer_isr (void)
{
	void adc_isr(void);

	/*
	 * This construct de-jitters the sample clock
	 * This is necessary to account for the fact that
	 * some instructions take 2 cycles and some take
	 * only 1, but interrupts never happen in the middle
	 * of a 2-cycle instruction.
	 */
	if ((TMR0L & 1) == 1) {
		_asm
			goto done
		    done:
		_endasm
	}
	TMR1H = T1SET_H;
	TMR1L = T1SET_L;
/*xxx*/if (!PIR1bits.TMR1IF) {INTCONbits.GIEH = 0; blink_error(11);}

	PIR1bits.TMR1IF = 0;

	/*
	 * Do the actual work of the ISR.
	 */
	adc_isr();
}

#pragma code

/*
 * The disk buffer is manually allocated in the .lkr file.
 *
 * NOTE: The fact that this buffer is 3 * 512 bytes long is
 * hard coded in many places.  Do not change N_BUFFERS.
 */
#pragma udata dataBuffer
unsigned char buffer[MEDIA_SECTOR_SIZE * N_BUFFERS];
#pragma udata

/*
 * Ring buffer code.
 *
 * Each sample is 8 bytes long.
 *  Byte 0:  High order bit is 1 for data record, 0 for control record.
 *           Low order are a sequence number in the range [1-7].  This
 *           number remains constant for the run, and is incremented
 *           between runs.
 * For data records:
 *  Byte 0:  The remaining 4 bits of byte 0 are a format number.
 *           This code implements format zero.
 *  Byte 1:  High order 8 bits of ADC channel 0.
 *  Byte 2:  The 4 MSBs are the low order bits of ADC channel 0.
 *           The 4 LSBs are the 2 opto-isolated inputs plus the
 *           two raw inputs.
 *  Byte 3:  High order 8 bits of ADC channel 1.
 *  Byte 4:  The 4 MSBs are the low order bits of ADC channel 1.
 *           The 4 LSBs are reserved, must be zero.
 *  Byte 5:  High order 8 bits of ADC channel 2.
 *  Byte 6:  The 4 MSBs are the low order bits of ADC channel 2.
 *           The 4 LSBs are reserved, must be zero.
 *  Byte 7:  Number of samples dropped since the last sample.
 *           
 *
 */

unsigned char *record_p;
#pragma udata access adc_data
near unsigned int adc_sample_skip;
near unsigned char b1flag;
near unsigned char b2flag;
near unsigned char b3flag;
near unsigned char bn;
near unsigned char adc_ring_running;
near unsigned char samples_to_fill;

near WORD_VAL value0;
near WORD_VAL value1;
near WORD_VAL value2;

near DWORD lba;

#pragma udata

/*
 * Initialize the ring buffer for the ADC code.
 */
static void
adc_ring_init(void)
{
	int i;

	adc_ring_running = 0;
	adc_sample_skip = 0;
	bn = 0;
	samples_to_fill = 100;
	b1flag = 0;
	b2flag = 0;
	b3flag = 0;
}

void
adc_isr(void)
{
	/*
	 * Don't do anything if we've been shut down.
	 */
	if (adc_ring_running == 0)
		return;

	/*
	 * Process starting a new sector.
	 */
	if (samples_to_fill == 0) {
		bn += 1;
		if (bn > 2) {
			record_p = buffer;
			bn = 0;
		}
		samples_to_fill = 100;
	}

	/*
	 * If we have just switched to a new buffer
	 * and that buffer is full, skip this sample.
	 */
	if (samples_to_fill == 100) {
		if ((bn == 0 && b1flag == 1) ||
		    (bn == 1 && b2flag == 1) ||
		    (bn == 2 && b3flag == 1)) {
		    	adc_sample_skip++;
			return;
		}
		/*
		 * At the begining of the buffer record
		 * how many samples were skipped.
		 * Reset the sample skip counter.
		 */
#ifdef notdef
		*((unsigned int *)record_p) = adc_sample_skip;
		record_p += 2;
#else
		*record_p++ = adc_sample_skip & 0xff;
		*record_p++ = (adc_sample_skip >> 8) & 0xff;
#endif
		adc_sample_skip = 0;
	}

	samples_to_fill -= 1;

	/*
	 * Get the data.
	 */
	adc_channel = 0;
	adc_sample();
	value0.Val = adc_value;

	adc_channel = 1;
	adc_sample();
	value1.Val = adc_value;

	adc_channel = 2;
	adc_sample();
	value2.Val = adc_value;

/*xxx*/value0.Val = 0x1230; value1.Val = 0x4560; value2.Val = 0x7890;

	/*
	 * Pack bits into the rcord.
	 */
	value0.bits.b3 = RAW0_PIN;
	value0.bits.b2 = RAW1_PIN;
	value0.bits.b1 = 1;
	value0.bits.b0 = 0;
	*record_p++ = value0.byte.LB;	// Byte 1
	*record_p++ = value0.byte.HB;	// Byte 2

	if (value2.bits.b7)
		value1.bits.b3 = 1;
	if (value2.bits.b6)
		value1.bits.b2 = 1;
	if (value2.bits.b5)
		value1.bits.b1 = 1;
	if (value2.bits.b4)
		value1.bits.b0 = 1;

	*record_p++ = value1.byte.HB;	// Byte 3
	*record_p++ = value1.byte.LB;	// Byte 4
	*record_p++ = value2.byte.HB;	// Byte 5

	/*
	 * When the buffer is full mark it so.
	 */
	if (samples_to_fill == 0) {
		if (bn == 0)
			b1flag = 1;
		else if (bn == 1)
			b2flag = 1;
		else
			b3flag = 1;
	}
}

/*
 * Init raw input pins.
 */
static void
raw_init(void)
{
	RAW0_PIN_TRIS = 1;	// input.
	RAW1_PIN_TRIS = 1;	// input.
	RAW2_PIN_TRIS = 1;	// input.
	RAW3_PIN_TRIS = 1;	// input.
}

/*
 * Set up timers.
 * Be sure to call adc_ring_init() first.
 */
void
timer_setup(void)
{
	/* 
	 * Set up the interrupt system.
	 */
	RCONbits.IPEN = 1;	// Turn on the priority interrupt system.
	INTCONbits.GIEH = 1;	// Enable high priority interrupts.
	INTCONbits.GIEL = 0;	// Disable low priority interrupts.

	/*
	 * Timer 0: free-running 12 MHz counter
	 */
	INTCONbits.TMR0IE = 0;	// Timer 0 never interrupts.
	TMR0H = 0;		// Clear the counter
	TMR0L = 0;
	T0CON = 0x88;		// Timer 0 is on with 1x prescaler

	/*
	 * Timer 1: 1000 Hz interrupt source.
	 */
	PIE1bits.TMR1IE = 0;
	IPR1bits.TMR1IP = 1;	// High priority interrupt.
	T1CON = 0x81;		// enabled with a 1x prescaler.
	TMR1H = T1SET_H;
	TMR1L = T1SET_L;
	PIE1bits.TMR1IE = 1;
}

/*
 * This is the major data path.
 * For now, this does NSECT sectors and stops.
 */
#define	NSECT	(1*2048)

void
data_run(void)
{
	int i;
	BYTE r;
	unsigned char j;
	BYTE *bp;

	lba = 768;	// set the start address

	/*
	 * Erase the SD card.
	 */
	r = MDD_SDSPI_Erase(lba + 0, lba + NSECT-1);
	if (!r)
		blink_error(7);
/*xxx*/if(0)blink_ok();
/*QQQ*/blink_debug(1);

	/*
	 * Start the ADC running.
	 */
	adc_ring_running = 1;

	/*
	 * Write all but one of the sectors, starting at LBA.
	 */
	for (i = 0; i < NSECT; i += N_BUFFERS) {
/*xxx*/LED_GREEN = (i & 1);
	    bp = buffer;

	    /*
	     * Unrolled loop, once per sector in buffer.
	     */
	    while (b1flag == 0);
	    r = MDD_SDSPI_SectorWriteStart(lba, bp);
	    if (!r)
		blink_error(2);
	    b1flag = 0;			// mark as empty
	    r = MDD_SDSPI_SectorWriteFinish();
	    if (!r)
		blink_error(3);
	    lba += 1;
	    bp += MEDIA_SECTOR_SIZE;


	    while (b2flag == 0);
	    r = MDD_SDSPI_SectorWriteStart(lba, bp);
	    if (!r)
		blink_error(2);
	    b2flag = 0;			// mark as empty
	    r = MDD_SDSPI_SectorWriteFinish();
	    if (!r)
		blink_error(3);
	    lba += 1;
	    bp += MEDIA_SECTOR_SIZE;


	    while (b3flag == 0);
	    r = MDD_SDSPI_SectorWriteStart(lba, bp);
	    if (!r)
		blink_error(2);
	    b3flag = 0;			// mark as empty
	    r = MDD_SDSPI_SectorWriteFinish();
	    if (!r)
		blink_error(3);
	    lba += 1;
	}

	/*
	 * Shut down the ADC sampling
	 */
	adc_ring_running = 0;		// turn off the adc sampling.

	/*
	 * Write EOF.
	 */
	for (i = 0; i < sizeof buffer; i++)
		buffer[i] = 0x00;	// eventually we need a better EOF

	r = MDD_SDSPI_SectorWrite(
		lba,			// sector
		buffer,
		(BYTE)0			// do not allow overwrite of MBR
		);
	if (!r)
		blink_error(4);

}

void
main (void)
{
	int i;
	BYTE r;

	ADCON1 |= 0x0F;                 // Default all pins to digital
	blink_init();
	adc_init();
	adc_ring_init();
	timer_setup();
	raw_init();

	/*
	 * Initialize the card.
	 */
	MDD_SDSPI_InitIO();
	r = MDD_SDSPI_MediaInitialize();
	if (!r)
		blink_error(1);

	data_run();

	blink_ok();
}
