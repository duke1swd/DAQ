/*
 * LTC1293 A/D converter
 */

#pragma bit ADC_CLK       @ PORTA.5
#pragma bit ADC_DATA_I    @ PORTC.1
#pragma bit ADC_DATA_O    @ PORTC.2
#pragma bit ADC_SEL       @ PORTC.0

#define	ADC_TX_BIT_0	\
	ADC_CLK = 1; \
	ADC_DATA_O = 0; \
	nop2(); \
	nop2(); \
	ADC_CLK = 0; \
	nop(); \
	nop2(); \
	nop2()

#define	ADC_TX_BIT_1	\
	ADC_CLK = 1; \
	ADC_DATA_O = 1; \
	nop2(); \
	nop2(); \
	ADC_CLK = 0; \
	nop(); \
	nop2(); \
	nop2()

#define	ADC_TX_BIT(bit)	   \
	ADC_CLK = 1;  \
	ADC_DATA_O = 0;  \
	if (bit)  \
		ADC_DATA_O = 1;  \
	nop(); \
	ADC_CLK = 0; \
	nop(); \
	nop2(); \
	nop2()

/*
 * Receive a bit.  Actually, ORs the bit in, so clear the word first.
 */

#define	ADC_RX_BIT(bit) \
	ADC_CLK = 1; \
	if (ADC_DATA_I) bit = 1; \
	ADC_DATA_O = 0; \
	nop2(); \
	ADC_CLK = 0; \
	nop(); \
	nop2(); \
	nop2()

/*
 * Get 1 sample from an analog channel and return as a 16 bit number
 *
 * This is a software implementation of SPI mode 0, 1 MHz clock.
 */

uns16 value;
uns8 adc_channel;

void
adc_sample(void)
{

	uns8 extra;

	ADC_SEL = 0;	// select the part.

	value = 0;	// this goes here for timing reasons

	/* 
	 * Select the channel.
	 */

	ADC_TX_BIT_1;			// start bit
	ADC_TX_BIT_1;			// single-ended

	ADC_TX_BIT(adc_channel & 1);	// 3 bit channel selector
	ADC_TX_BIT(adc_channel & 4);	
	ADC_TX_BIT(adc_channel & 2);	

	ADC_TX_BIT_0;			// Bi-polar
	ADC_TX_BIT_1;			// MSB first
	ADC_TX_BIT_1;			// don't power down


	ADC_RX_BIT(extra.0);	// discard the first bit
	ADC_RX_BIT(extra.0);	// discard the first bit
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
	value = 0xdead;

	ADC_SEL = 1;	// deselect the part.
}

void
main()
{
	adc_channel = 1;
	adc_sample();
}
