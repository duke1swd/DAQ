/*
 * This program uses the SD drive to write 1 sector to the SD drive.
 * Every byte in drive will be overwritten with 17, the world's only
 * truely random number.
 *
 * WARNING: This will destroy any information stored on the card.
 *
 * Written for USB boot into HybridSky DAQ board.
 *
 * 9/28/08, Stephen Daniel
 *
 * Note: works.
 * Except for the annoying fact that blink_init() doesn't turn on
 *  the green LED.  ?!?
 */


#include <p18cxxx.h>
#define USE_OR_MASKS

#include "HardwareProfile.h"
#include "SD-SPI.h"
#include "blink.h"

unsigned char xcode;

extern void _startup (void);
#pragma code remapped_reset = 0x0800
	void _reset (void)
	{
	    _asm goto _startup _endasm
	}

#pragma code

#pragma udata dataBuffer
unsigned char buffer[MEDIA_SECTOR_SIZE];
#pragma udata

void
main (void)
{
	int i;
	BYTE r;

	ADCON1 |= 0x0F;                 // Default all pins to digital
	blink_init();


	for (i = 0; i < sizeof buffer; i++)
		buffer[i] = 17;

	/*
	 * Initialize the card.
	 */
	MDD_SDSPI_InitIO();
	xcode = 2;
	r = MDD_SDSPI_MediaInitialize();
	if (!r)
		blink_error(xcode);

	/*
	 * Write the buffer to sector 0.
	 */
	r = MDD_SDSPI_SectorWrite(
		(DWORD)128,		// sector
		buffer,
		(BYTE)1			// allow overwrite of MBR
		);
	if (!r)
		blink_error(1);

	blink_ok();
}
