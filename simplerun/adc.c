/*
 * This module contains routines to talk to the ADC chip.
 */

/*
 * October 2008, Stehen Daniel
 * Written for the Hybridsky.com DAQ 1.0 board.
 */

#include <p18cxxx.h>
#include <delays.h>
#include "GenericTypeDefs.h"
#include "adc.h"

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
 * Setup routine.  Call once first.
 */
void
adc_init()
{
	ADC_CLK_TRIS    = 0;	// output
	ADC_DATA_O_TRIS = 0;	// output
	ADC_DATA_I_TRIS = 1;	// input
	ADC_SEL_TRIS    = 0;	// output
}

/*
 * Transmit functions
 */
#define	ADC_TX_BIT_0	\
	ADC_CLK = 1; \
	ADC_DATA_O = 0; \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	ADC_CLK = 0;  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();

#define	ADC_TX_BIT_1	\
	ADC_CLK = 1;  \
	ADC_DATA_O = 1; \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	ADC_CLK = 0;  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY()

#define	ADC_TX_BIT(bit)	   \
	ADC_CLK = 1;  \
	ADC_DATA_O = 0;  \
	if (bit)  \
		ADC_DATA_O = 1;  \
	 Delay1TCY();  \
	ADC_CLK = 0; \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY()


/*
 * Receive a bit.  Actually, ORs the bit in, so clear the word first.
 */
#define	ADC_RX_BIT(bit) \
	ADC_CLK = 1; \
	if (ADC_DATA_I) bit = 1; \
	ADC_DATA_O = 0; \
	 Delay1TCY();  \
	 Delay1TCY();  \
	ADC_CLK = 0; \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY();  \
	 Delay1TCY()

/*
 * Get 1 sample from the indicated channel
 */
#pragma udata access adc_data
near unsigned int adc_value;
near unsigned char adc_channel;
unsigned char extra;
#pragma udata

void
adc_sample(void)
{

	ADC_SEL = 0;	// select the part.

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

	adc_value = 0;


	/*
	 * The recieve portion is coded in assembler to get the
	 * timing right.
	 * (Actually, the original was in C, compiled by bknd cc8e.
	 */
	{ _asm
			//
			//
			//	ADC_RX_BIT(extra.0);	// discard the first bit
m026:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   extra,0,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m027
m027:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m028
m028:	BRA   m029
			//	ADC_RX_BIT(extra.0);	// discard the first bit
m029:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   extra,0,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m030
m030:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m031
m031:	BRA   m032
			//	ADC_RX_BIT(adc_value.15);
m032:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,7,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m033
m033:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m034
m034:	BRA   m035
			//	ADC_RX_BIT(adc_value.14);
m035:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,6,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m036
m036:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m037
m037:	BRA   m038
			//	ADC_RX_BIT(adc_value.13);
m038:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,5,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m039
m039:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m040
m040:	BRA   m041
			//	ADC_RX_BIT(adc_value.12);
m041:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,4,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m042
m042:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m043
m043:	BRA   m044
			//	ADC_RX_BIT(adc_value.11);
m044:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,3,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m045
m045:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m046
m046:	BRA   m047
			//	ADC_RX_BIT(adc_value.10);
m047:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,2,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m048
m048:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m049
m049:	BRA   m050
			//	ADC_RX_BIT(adc_value.9);
m050:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,1,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m051
m051:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m052
m052:	BRA   m053
			//	ADC_RX_BIT(adc_value.8);
m053:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value+1,0,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m054
m054:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m055
m055:	BRA   m056
			//	ADC_RX_BIT(adc_value.7);
m056:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value,7,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m057
m057:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m058
m058:	BRA   m059
			//	ADC_RX_BIT(adc_value.6);
m059:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value,6,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m060
m060:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m061
m061:	BRA   m062
			//	ADC_RX_BIT(adc_value.5);
m062:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value,5,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m063
m063:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m064
m064:	BRA   m065
			//	ADC_RX_BIT(adc_value.4);
m065:	BSF   0xF80,ADC_CLK_BIT,0
	BTFSC 0xF82,ADC_DATA_I_BIT,0
	BSF   adc_value,4,0
	BCF   0xF82,ADC_DATA_O_BIT,0
	BRA   m066
m066:	BCF   0xF80,ADC_CLK_BIT,0
	NOP  
	BRA   m067
m067:	BRA   m068
m068:
	  _endasm }

	/*
	 * Note: CS must remain high for at least 500 nanoseconds.
	 */
	ADC_SEL = 1;	// deselect the part.
}
