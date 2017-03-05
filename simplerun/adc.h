/*
 * This file contains stuff needed to talk to the LTC 1293 ADC chip
 * on the Hybdridsky.com DAQ 1.0 board.
 *
 * Stephen Daniel, October, 2008
 */

#define	ADC_CLK_PORT	PORTA
#define	ADC_CLK_BIT	5
#define	ADC_CLK		PORTAbits.RA5
#define	ADC_CLK_TRIS	TRISAbits.TRISA5

#define	ADC_DATA_O_PORT	PORTC
#define	ADC_DATA_O_BIT	2
#define	ADC_DATA_O	PORTCbits.RC2
#define	ADC_DATA_O_TRIS	TRISCbits.TRISC2

#define	ADC_DATA_I_PORT	PORTC
#define	ADC_DATA_I_BIT	1
#define	ADC_DATA_I	PORTCbits.RC1
#define	ADC_DATA_I_TRIS	TRISCbits.TRISC1

#define	ADC_SEL_PORT	PORTC
#define	ADC_SEL_BIT	0
#define	ADC_SEL		PORTCbits.RC0
#define	ADC_SEL_TRIS	TRISCbits.TRISC0

/*
 * Prototypes
 */

void
adc_init(void);

void
adc_sample(void);

/*
 * Shared globals
 */
extern near unsigned int adc_value;
extern near unsigned char adc_channel;
