/*
 * This module contains the code to handle
 * the CLI command to print out sensor values.
 */

#include <p18cxxx.h>
#define	USE_OR_MASKS
#include <timers.h>
#include <stdio.h>
#include "system\typedefs.h"
#include "cli.h"
#include "adc.h"

#define	OUTPUT_LINE_LENGTH	32
#define	TIMER_PERIOD		23473	/* 2 Hz */

/*
 * These routines control the timing of printing
 */

static unsigned char timer_is_ready;

/*
 * Start a new timing interval.
 */
static void
sensor_timer_reset(void)
{
	timer_is_ready = 0;
	WriteTimer0(0);
}

/*
 * Turn on timer 0, running at 12MHz / 256 = 46.875 KHz.
 */
static void
sensor_timer_on(void)
{
	OpenTimer0(
		TIMER_INT_OFF |
		T0_16BIT |
		T0_SOURCE_INT |
		T0_PS_1_256);
	sensor_timer_reset();
}

static void
sensor_timer_off(void)
{
	CloseTimer0();
}

static unsigned char
sensor_timer_ready(void)
{
	if (timer_is_ready)
		return 1;
	if (ReadTimer0() > TIMER_PERIOD) {
		timer_is_ready = 1;
		return 1;
	}
	return 0;
}

static void
print_sensor_common(float multiplier, float zero)
{
	unsigned int value;
	float fv;

	cli_putc(' ');
	cli_putc(' ');
	cli_putc(' ');

	adc_sample();
	//value = adc_value;
	//fv = (float)value / 32768.;
	//fv = multiplier * (fv - zero);

	value = 0xfff & (adc_value >> 4);
	fv = value;
	if (value < 1000)
		cli_putc(' ');
	if (value < 100)
		cli_putc(' ');
	if (value < 10)
		cli_putc(' ');
	printf("%d ", value);

	if (fv < 0.) {
		cli_putc('-');
		fv = -fv;
	} else
		cli_putc(' ');

	// convert the value to PSI (or whatever)
	fv = fv / 4096.;
	fv = fv * 5.;
	fv = fv - zero;
	fv = fv * multiplier;

	if (fv < 0) {
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
	fv -= value;

	fv *= 10.;
	value = fv;
	printf("%d", value);
}

static void
print_sensor_raw(void)
{
	unsigned int value;

	cli_putc(' ');
	cli_putc(' ');
	cli_putc(' ');

	adc_sample();
	value = adc_value;

	value = 0xfff & (adc_value >> 4);
	if (value < 1000)
		cli_putc(' ');
	if (value < 100)
		cli_putc(' ');
	if (value < 10)
		cli_putc(' ');
	printf("%d ", value);
}

/*
 * Get the value of sensor #0
 */
static void
print_sensor_0(void)
{
	adc_channel = 0;
	/*
	 * Pressure sensor.  125 PSI/volt.  0 PSI = 0.5 volts.
	 */
	print_sensor_common(125., .5);
}

/*
 * Get the value of sensor #1
 */
static void
print_sensor_1(void)
{
	adc_channel = 1;
	/*
	 * Pressure sensor.  125 PSI/volt.  0 PSI = 0.5 volts.
	 */
	print_sensor_common(125., .5);
}

/*
 * Get the value of sensor #2
 */
static void
print_sensor_2(void)
{
	/*
	 * Load cell.
	 */
	adc_channel = 2;
	/* 
	 * Load cell.  raw only.
	 */
	print_sensor_raw();
}

static void
raw_init(void)
{
	TRISBbits.TRISB6 = 1;
	TRISBbits.TRISB7 = 1;
}

static void
print_opto_common(unsigned char v)
{
	/* Note: opto isolated inputs are inverting. */
	if (v)
		cli_putc('0');
	else
		cli_putc('1');
}

#define	OPTO_0	PORTBbits.RB6
#define	OPTO_1	PORTBbits.RB7

static void
print_opto_0(void)
{
	print_opto_common(OPTO_0);
}

static void
print_opto_1(void)
{
	print_opto_common(OPTO_1);
}


/*
 * Print out one line of sensor data.
 *
 * Should be no longer than OUTPUT_LINE_LENGTH characters in length.
 */
static unsigned char line_index;
static void
sensor_print(void)
{
	if (line_index >= 4) {
		cli_putc('*');
		line_index = 0;
	} else {
		cli_putc(' ');
		line_index += 1;
	}
	cli_putc(' ');
	print_opto_0();
	print_opto_1();

	adc_init();
	raw_init();
	print_sensor_0();
	cli_putc(',');
	print_sensor_1();
	cli_putc(',');
	print_sensor_2();

	cli_putc('\n');
}


/*
 * Print out the value of the sensors 2x per second.
 */
void
cli_sensors()
{
	/*
	 * Stop printing when the user types something (anything).
	 */
	if (cli_input_pending(1)) {
		cmd_state = CMD_OK;
		sensor_timer_off();
		return;
	}

	/*
	 * If this is the first time here, turn on the timer.
	 */
	if (cmd_state == 1) {
		sensor_timer_on();
		sensor_print();

		/*
		 * Tell the input_poll routine to return here
		 * rather than waiting for a new command.
		 */
		cmd_state = 2;
		return;
	}

	/*
	 * Is it time to print yet?
	 */
	if (!sensor_timer_ready())
		return;

	sensor_timer_reset();
	sensor_print();
}
