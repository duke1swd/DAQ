/*
 * Low level TOD driver.  CC8E version.
 *
 * Assumes the HybridSky DAQ 1.0 board.
 *
 * Stephen Daniel
 * November, 2008
 */


/*
 * Note on timing for this chip.
 *
 *  Timing varies as a function of supply voltage.  Values given here
 *  assume a 5V supply.
 *
 *  - Chip select most go high before the first clock rising by
 *    1 microsecond.
 *
 *  - Clock must stay in each phase for a minimum of 250 ns.
 *
 *  - CS must stay high for at least 70 ns after last rising clock.
 *
 *  On write:
 *
 *  - Data bit must be valid before rising clock by 50 ns.
 *
 *  - Data bit must stay valid after rising clock for another 70 ns.
 *
 *  - After the last bit of the command byte, the data byte follows on
 *    the very next bit time with no gap.
 *
 *  On read:
 *
 *  - The last bit of the command byte is sent on a rising clock.
 *    The first bit of response data is transmitted on the falling edge
 *    of that clock.
 *
 *  - Data is available within 200 ns of falling clock.
 *
 *  All low-level timing assumes that the chip is running 12MHz instruction
 *  clock
 *
 */

#include "tod.h"

/*
 * Common entry code for both the send and receive.
 * Standard TRIS bits are already set.
 */
void
tod_internal_init(void)
{
	TOD_SCLK = 0;
	TOD_CS = 1;

}

/*
 * Send 8 bits to the chip.
 * Assumes CS is high and SCLK is low.
 * Leaves SCLK high on exit.
 */
void
tod_internal_send(unsigned char val)
{
	unsigned char i;

	TOD_DATA_TRIS = 0;	// output pin

	for (i = 0; i < 8; i++) {
		TOD_SCLK = 0;

		nop2(); nop(); // delay for 250 ns.

		TOD_DATA = val.0;

		val >>= 1;

		TOD_SCLK = 1;

		nop2(); nop(); // delay for 250 ns.
	}
}

/*
 * Receive 8 bits from the chip.
 *
 * Assumes CS is high and SCLK is high.
 * Leaves SCLK low on exit.
 */
unsigned char
tod_internal_read(void)
{
	unsigned char i;
	unsigned char val;

	TOD_DATA_TRIS = 1;	// input pin

	val = 0;

	for (i = 0; i < 8; i++) {
		TOD_SCLK = 0;
		val >>= 1;
		if (TOD_DATA)
			val.7 = 1;

		nop2(); nop(); // delay for 250 ns.

		TOD_SCLK = 1;

		nop2(); nop(); // delay for 250 ns.
	}

	return val;
}

void
tod_send_byte(unsigned char addr, unsigned char val)
{
	tod_internal_init();

	tod_internal_send(addr | TOD_CMD_WRITE);
	TOD_SCLK = 0;

	tod_internal_send(val);
	TOD_SCLK = 0;

	TOD_CS = 0;
}

unsigned char
tod_receive_byte(unsigned char addr)
{
	unsigned char v;

	tod_internal_init();

	tod_internal_send(addr | TOD_CMD_READ);

	v = tod_internal_read();

	TOD_CS = 0;

	return v;
}

/*
 * Burst read.  undebugged.
 */

uns8 tod_burst_buffer[8];

void
tod_burst_read()
{
	tod_internal_init();

	tod_internal_send(TOD_BURST | TOD_CMD_READ);

	tod_burst_buffer[0] = tod_internal_read();
	tod_burst_buffer[1] = tod_internal_read();
	tod_burst_buffer[2] = tod_internal_read();
	tod_burst_buffer[3] = tod_internal_read();
	tod_burst_buffer[4] = tod_internal_read();
	tod_burst_buffer[5] = tod_internal_read();
	tod_burst_buffer[6] = tod_internal_read();
	tod_burst_buffer[7] = tod_internal_read();

	TOD_CS = 0;
}
