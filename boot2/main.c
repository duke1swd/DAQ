/*
 * Second stage bootstrap for the HybridSky DAQ 1.0 board.
 * Boots the appropriate module from the SD card.
 */

/*
 * The DAQ 1.0 board is a PIC 18F2550.
 * The chip is running on a 12 MHz crystal with FOSC/4 = 12MHz.
 */

#define	DATAFLASH	// use the Atmel Dataflash driver, not SD driver.

#include "int18xxx.h"
#include "hexcodes.h"
#include "eprommap.h"

/*
 * Resource usage:
 *
 * This module uses the SPI hardware to talk to the SD card.
 *
 * Timer 2 is used to clock the SPI hardware when the SD card
 * is in low speed mode.
 *
 * The LEDs are used to blink status and errors.
 *
 * The mode selector switch is sampled to decide which
 * mode we should run in.  It is hooked to A.0.
 *
 * The file names are stored in data EEPROM.
 */


/*
 * Define the I/O infrastructure.
 */
#pragma	bit LED_GREEN		@ PORTA.3
#pragma	bit LED_RED		@ PORTA.2
#pragma	bit SD_SCK		@ PORTB.1	// SD clock
#pragma	bit SD_SDI		@ PORTB.0	// SD data to PIC
#pragma	bit SD_SDO		@ PORTC.7	// SD data from PIC
#pragma	bit SD_CS		@ PORTC.6	// SD chip select
#pragma bit TOD_CS		@ PORTB.4	// TOD chip select
#pragma bit TOD_DATA		@ PORTB.3	// Data pin (bi-directional)
#pragma bit TOD_DATA_TRIS	@ TRISB.3
#pragma bit TOD_SCLK		@ PORTB.2
#define	TRISA_SET		0b.0000.0001	// bit 0 is the mode switch
#define	TRISB_SET		0b.0000.0001	// one input from SD
#define	TRISC_SET		0b.0000.0000	// all are outputs.
#define	ADCON1_SET		0b.0000.1111	/* No analog inputs */

#ifdef DATAFLASH
#define	sd_init		df_init
#define	sd_read		df_read
#define	sd_lba		df_lba
#define	DATA_CS		SD_CS
#endif // DATAFLASH

/*
 * This stuff defines the linkages.
 */

/*
 * Entry points.
 */
void main(void);
void sd_init(void);
void sd_read(void);
void waitms(void);
void blink_debug_x(void);
void blink_error_code_x(void);

#pragma resetVector 0x800

#pragma origin = 0x808
void
int_redirect() {
#asm
	DW __GOTO(0x1008)
#endasm
}

#pragma origin = 0x818
void
low_int_redirect() {
#asm
	DW __GOTO(0x1018)
#endasm
}

void
waitms(void)
{
	unsigned char i, j;

	for (j = 7; j; j--)
		for (i = 1; i; i++)
			nop2();
}

#include "eprom.c"
#include "blink.c"
#ifdef DATAFLASH
#include "dataflash.c"
#else // DATAFLASH
#include "sd.c"
#endif // DATAFLASH
#include "boot.c"

void
doit(void)
{
	boot_1();
	boot_2();
}

void
main(void)
{
	unsigned char i;

	clearRAM();


	/*
	 * Turn off stuff we don't use.
	 */
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
	 * Using the on-chip A/D, sample the position of the mode switch.
	 */
	ADCON1 = 0x0E;		// RA0/AN0 is analog
	ADCON2 = 0x16;		// Left just, Aq time = 4 TAD, TAD = Fosc/64
	ADON = 1;		// Enable the A/D converter module
	GO = 1;			// Start conversion
	while (GO) ;		// Wait for conversion


	/*
	 * The boot mode is the top 3 bits of the
	 * A/D result.
	 */
	boot_mode = 0x7 & (ADRESH >> 5);

        ADCON0 = 0;          	// Turn A/D off
	ADCON1 = ADCON1_SET;

	for (i = boot_mode; i; i--) {
		LED_GREEN = 1;
		blink_sdelay();
		LED_GREEN = 0;
		blink_sdelay();
		blink_sdelay();
	}

	sd_init();

	doit();

#asm
	DW __GOTO(0x1000)
#endasm
}
