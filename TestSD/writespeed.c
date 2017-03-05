/*
 * This program writes 16MB to the SD card.
 *
 * The goal is to understand how fast we can do this.
 *
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
	unsigned int i;
	BYTE r;
	DWORD sector;

	blink_init();


	for (i = 0; i < sizeof buffer; i++)
		buffer[i] = 17;

	/*
	 * Initialize the card.
	 */
	MDD_SDSPI_InitIO();
	r = MDD_SDSPI_MediaInitialize();
	if (!r)
		blink_error(1);

	/*
	 * Write 16 MB.
	 */
	sector = 128;
	for (i = 0; i < 0x8000; i++) {
		r = MDD_SDSPI_SectorWrite(
			sector,
			buffer,
			(BYTE)1			// allow overwrite of MBR
			);
		if (!r)
			blink_error(2);
		sector += 1;
	}

	blink_ok();
}
