/*
 * This module contains the code to handle
 * the CLI ADC commands.
 */

#include <p18cxxx.h>
#include "system\typedefs.h"
#include "cli.h"
#include "adc.h"
#include <stdio.h>

void
cli_adc(unsigned char channel)
{
	int value;
	unsigned char v;

	adc_init();
	if (channel > 5)
		cli_putc('X');
	else {
		cli_putc(' ');

		adc_channel = channel;
		adc_sample();
		value = adc_value;

		v = value >> 8;
		puthex(v);
		v = value & 0xff;
		puthex(v);
	}

	cli_putc('\n');
	cmd_state = CMD_OK;
}

static void
pbat(unsigned char chan)
{
	int value;
	float fv;

	adc_channel = chan;
	adc_sample();
	value = adc_value;
	fv = (float)value / 32768.;
	fv *= 4.01 * 5;

	if (chan ==3)
		fv += 0.30;	// diode drop
	else if (chan == 4)
		fv -= 0.23;	// diode drop

	if (fv < 0.) {
		cli_putc('-');
		fv = -fv;
	}

	value = fv;
	printf("%d.", value);
	fv -= value;

	fv *= 10.;
	value = fv;
	printf("%d", value);
	fv -= value;

	fv *= 10.;
	value = fv;
	printf("%d", value);
}

void
cli_adc_battery(void)
{
	adc_init();
	pbat(3);
	cli_putc(' ');
	pbat(4);
	cli_putc('\n');
	cmd_state = CMD_OK;
}
