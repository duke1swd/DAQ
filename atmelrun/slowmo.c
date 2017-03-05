/*
 * This module contains the code to put the PIC 18F2455
 * into slow (31KHz) mode and to take it out of that mode.
 */

void
enter_slowmo(void)
{
	/*
	 * Bits are as follows:
	 *  0: Sleep enabled
	 *  000:  internal clock is 31 KHz INTRC clock
	 *  R/O status bit
	 *  R/O status bit
	 *  10: use internal clock.
	 */
	OSCCON = 0b.0000.0010;
}

void
exit_slowmo(void)
{
	/*
	 * Bits are as follows:
	 *  0: Sleep enabled
	 *  000:  internal clock is 31 KHz INTRC clock
	 *  R/O status bit
	 *  R/O status bit
	 *  00: use primary clock.
	 */
	OSCCON = 0b.0000.0000;

	/*
	 * Loop until the primary clock comes back.
	 */
	while (!OSTS)
		;
}
