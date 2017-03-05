/* 
 * This code was taken from the Microchip C compiler's EEP library
 * routines.  It was fixed for 256 byte EEP and converted to BKND CC8-e
 */

void
Busy_eep(void)
{
	while(WR)
		;
}

unsigned char
Read_b_eep(unsigned char badd)
{
	EEADR = badd;
	CFGS = 0;
	EEPGD = 0;
	RD = 1;
	return EEDATA;
}

/*
 * The original Microchip code cleared and then
 * set GIE in this routine (bad).
 * This version clears GIE and then restores it to the pervious
 * value.
 */
void
Write_b_eep(unsigned char badd, unsigned char bdata)
{
	bit b;

	EEADR = badd;
  	EEDATA = bdata;
  	EEPGD = 0;
	CFGS = 0;
	WREN = 1;

	b = GIE;
	GIE = 0;

	EECON2 = 0x55;
	EECON2 = 0xAA;
	WR = 1;

	if (b)
		GIE = 1;

	WREN = 0;
}
