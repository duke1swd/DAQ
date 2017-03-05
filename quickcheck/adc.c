/*
 * This module contains routines to talk to the ADC chip.
 */

/*
 * October 2008, Stehen Daniel
 * Written for the Hybridsky.com DAQ 1.0 board.
 */

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

/*
 * Transmit functions
 */
#define	ADC_TX_BIT_0	\
	ADC_CLK = 1; \
	ADC_DATA_O = 0; \
	 nop2(); \
	 nop2(); \
	ADC_CLK = 0;  \
	 nop2(); \
	 nop2(); \
	 nop();

#define	ADC_TX_BIT_1	\
	ADC_CLK = 1;  \
	ADC_DATA_O = 1; \
	 nop2(); \
	 nop2(); \
	ADC_CLK = 0;  \
	 nop2(); \
	 nop2(); \
	 nop()

#define	ADC_TX_BIT(bit)	   \
	ADC_CLK = 1;  \
	ADC_DATA_O = 0;  \
	if (bit)  \
		ADC_DATA_O = 1;  \
	 nop();  \
	ADC_CLK = 0; \
	 nop2(); \
	 nop2(); \
	 nop()


/*
 * Receive a bit.  Actually, ORs the bit in, so clear the word first.
 */
#define	ADC_RX_BIT(bit) \
	ADC_CLK = 1; \
	if (ADC_DATA_I) bit = 1; \
	ADC_DATA_O = 0; \
	 nop2(); \
	ADC_CLK = 0; \
	 nop2(); \
	 nop2(); \
	 nop()

/*
 * Get 1 sample from the indicated channel
 */
uns16 adc_value;
unsigned char adc_channel;
unsigned char extra;
bit adc_unipolar;

void
adc_sample(void)
{

	ADC_CS = 0;	// select the part.

	/* 
	 * Select the channel.
	 */

	ADC_TX_BIT_1;			// start bit
	ADC_TX_BIT_1;			// single-ended

	ADC_TX_BIT(adc_channel & 1);	// 3 bit channel selector
	ADC_TX_BIT(adc_channel & 4);	
	ADC_TX_BIT(adc_channel & 2);	
	ADC_TX_BIT(adc_unipolar);

	ADC_TX_BIT_1;			// MSB first
	ADC_TX_BIT_1;			// don't power down

	adc_value = 0;

	ADC_RX_BIT(extra.0);	// discard the first bit
	ADC_RX_BIT(extra.0);	// discard the first bit
	ADC_RX_BIT(adc_value.15);
	ADC_RX_BIT(adc_value.14);
	ADC_RX_BIT(adc_value.13);
	ADC_RX_BIT(adc_value.12);
	ADC_RX_BIT(adc_value.11);
	ADC_RX_BIT(adc_value.10);
	ADC_RX_BIT(adc_value.9);
	ADC_RX_BIT(adc_value.8);
	ADC_RX_BIT(adc_value.7);
	ADC_RX_BIT(adc_value.6);
	ADC_RX_BIT(adc_value.5);
	ADC_RX_BIT(adc_value.4);

	/*
	 * Note: CS must remain high for at least 500 nanoseconds.
	 */
	ADC_CS = 1;	// deselect the part.
}

/*
 * Power down the ADC chip.
 */
void
adc_power_down()
{
	ADC_CS = 0;	// select the part.

	/*
	 * Send start bit.
	 */
	ADC_TX_BIT_1;

	/*
	 * Now send 6 dont-care bits followed by a zero (for power-down).
	 */
	for (extra = 0; extra < 7; extra++) {
		ADC_TX_BIT_0;
	}

	/*
	 * Clock out the garbage data.
	 */
	for (extra = 0; extra < 14; extra++) {
		ADC_RX_BIT(adc_value.0);
	}

	/*
	 * Note: CS must remain high for at least 500 nanoseconds.
	 */
	ADC_CS = 1;	// deselect the part.

}
