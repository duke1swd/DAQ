/*
 * This file contains stuff needed to talk to the opto-isolator ports
 * on the Hybdridsky.com DAQ 1.0 board.
 *
 * Stephen Daniel, November, 2008
 */

#define	RAW0_PORT	PORTB
#define	RAW0_BIT	6
#define	RAW0		PORTBbits.RB6
#define	RAW0_TRIS	TRISBbits.TRISB6

#define	RAW1_PORT	PORTB
#define	RAW1_BIT	7
#define	RAW1		PORTBbits.RB7
#define	RAW1_TRIS	TRISBbits.TRISB7
