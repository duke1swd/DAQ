/*
 * Quick Check
 *
 * For now just copy the record stop switch onto the red LED
 * and blink the green LED at just under 1 HZ.
 */

/*
 * Wait 1 second.
 */
void
wait1sec(void)
{
	uns8 i;

	i = lbolt + 50;
	while (lbolt != i)
		;
}

void
run(void)
{
	uns8 i;
	uns8 r;

	/*
	 * Initialize: blink both LEDs
	 * 5 HZ 5 times.
	 */
	for (i = 0; i < 5; i++) {
		while ((lbolt & 4) == 0)
			;

		LED_GREEN = 1;
		LED_RED_LAT = 1;

		while (lbolt & 4)
			;

		LED_GREEN = 0;
		LED_RED_LAT = 0;
	}
	wait1sec();

	/*
	 * Now, loop.
	 * Red follows record/stop switch.
	 * Green blinks.
	 * External LED is on when Green is on or either opto.
	 */

	for (;;) {
		r = 0;
		if (RECORD_STOP) {
			LED_RED = 1;
			r = 1;
		} else
			LED_RED = 0;

		if ((lbolt & 0x70) == 0x40) {
			LED_GREEN = 1;
#ifndef VQC
			r = 1;
#endif // VQC
		} else
			LED_GREEN = 0;

		if (!OPTO_0 || !OPTO_1)
			r = 1;

		if (r == 0)
			RAW_0 = 0;
		else
			RAW_0 = 1;
	}
}
