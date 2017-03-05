/*
 * This defines the entry points to the low-level TOD chip driver.
 */

void
tod_send_byte(unsigned char addr, unsigned char val);

unsigned char
tod_receive_byte(unsigned char addr);

/*
 * TOD Registers
 */

#define	TOD_SEC		0x80
#define	  TOD_HALT	0x80	// Set this bit to halt the clock.

#define	TOD_MIN		0x82

#define	TOD_HOUR	0x84
#define	  TOD_12	0x80	// Set this bit to set AM/PM mode
#define	  TOD_PM	0x20	// When in AM/PM, set to indicate PM

#define	TOD_DATE	0x86

#define	TOD_MONTH	0x88

#define	TOD_DAY		0x8A

#define	TOD_YEAR	0x8C

#define	TOD_WCR		0x8E	// write control register
#define	  TOD_WP	0x80	// global write protect bit

#define	TOD_CHARGE	0x90
#define	  TOD_TCS	0xA0	// Enable trickle charge command
#define	  TOD_1DIODE	0x04	// Enable 1 diode drop
#define	  TOD_2DIODE	0x08	// Enable 2 diode drops
#define	  TOD_R2K	0x01	// Charge through a 2K resistor
#define	  TOD_R4K	0x02	// Charge through a 4K resistor
#define	  TOD_R8K	0x03	// Charge through an 8K resistor
#define	  TOD_CHARGE_OFF   0	// Turn off the charger.

/*
 * These modes are ORed into the register address to indicate the
 * direction of data flow.
 */
#define	TOD_CMD_READ	0x01
#define	TOD_CMD_WRITE	0x00
