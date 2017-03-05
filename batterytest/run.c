//#define	TEST_NO_WRITE

/*
 * This module is the guts of the data gather routine.
 *
 

 *
 * There are three parts.
 *
 * There is a ring of three buffers, each 512 bytes.
 *
 * There is an interrupt routine.  It runs off the high
 * priority interrupt, divided down to 0.1 HZ
 *
 * There is a base level routine that pushes data to the
 * dataflash card.
 */

/*
 * Buffers.
 * WARNING: The first of these overlaps the main df buffer.
 *
 */

/*
 * The three channels are assumed to be 0, 1, and 2.
 */
#define	SAMPLES_PER_BUFFER	((BLOCK_SIZE - 2) / 5)	// 5 bytes/samp

#define	N_BUFFERS		3
unsigned char buffers[BLOCK_SIZE * N_BUFFERS] @ 0x100;

uns8 next_to_write;
uns8 filling_buffer;
uns16 samples_till_switch;

bit running;
bit stopping;
bit buffer_switched;

/*
 * This is the last value seen on the adc channel that meaasures
 * batter voltage.  Gets compared to an EPROM value.
 *
 * Voltage is on channel 1.
 */
#pragma rambank 0
uns16 last_voltage;
uns16 last_voltage_threshold;
uns8 last_voltage_state;
#pragma rambank -

size2 unsigned char *store_p;

/*
 * Record skipped samples.
 */
uns16 skipped;

/*
 * Buffer state is 0 if empty, 1 if full.
 */
uns8 buffer_state[N_BUFFERS];

void
run_init(void)
{
	uns8 i;

	RAW_0 = LOAD_OFF;		// turn off the load

	adc_unipolar = 1;	// the pressure sensors are unipolar.
	next_to_write = 0;
	filling_buffer = 0;
	running = 0;
	last_voltage_state = 0;
	/*
	 * The next two lines multiply the EPROM value by 16 and
	 * then left justify it so it can be compared to the samples
	 * from the ADC.
	 */
	last_voltage_threshold.high8 = Read_b_eep(EPROM_VOLTAGE_THRESH);
	last_voltage_threshold.low8 = 0;
	adc_power_down();
}

/*
 * See if we are supposed to stop recording.
 *
 * We stop when two consecutive voltage readings are below
 * the threshold or when the record/stop switch is flipped.
 *
 * Here are the meanings of last_voltage_state:
 *  0 - No measurement taken
 *  1 - Measurement taken, unknown w.r.t. threshold
 *  2 - No new measurement, not in stop condition.
 *  3 - New measurement, have not been in stop condition.
 *  8 - No new measurement, in maybe stop condition.
 *  9 - New measurement, have been in maybe stop condition.
 */
void
check_stop()
{
	if (RECORD_STOP == 0)
		stopping = 1;

	if (stopping ||
	    last_voltage_state == 0 ||
	    last_voltage_state == 2 ||
	    last_voltage_state == 8)
		return;

	if (last_voltage >= last_voltage_threshold) {
		last_voltage_state = 2;
		return;
	}

	if (last_voltage_state == 1 ||
	    last_voltage_state == 3) {
		last_voltage_state = 8;
		return;
	}

	// last voltage state must be 9
	stopping = 1;
}

/*
 * This routine runs off the interrupt.
 * It is responsible for gathering data and
 * putting it into the output buffers.
 */

void
adc_isr(void)
{
	uns16 v1, v2;

	if (!running)
		return;

	check_stop();
	/*
	 * If we are supposed to stop, proceed, so we can fill up
	 * the buffer.  If we are not supposed to stop, divide
	 * the clock down by 10K to get samples every 10 seconds.
	 */
	if (!stopping && t1divide != 0)
		return;

	/*
	 * Gather up three channels.
	 * First 2 bytes are the first sample, the opto channels,
	 * and 2 marker bits.
	 * Next 12 bits are the second sample.
	 * Low 4 bits of word 2 (byte 3) are the low bits of sample 3.
	 */
	if (stopping)
		adc_value = 0xffff;
	else {
		adc_channel = 0;
		adc_sample();
	}

	if (!OPTO_0)
		adc_value.3 = 1;
	if (!OPTO_1)
		adc_value.2 = 1;
	adc_value.1 = 1;
	v1 = adc_value;

	if (stopping)
		adc_value = 0xffff;
	else {
		adc_channel = 1;
		adc_sample();
		last_voltage = adc_value;
		last_voltage_state++;
	}
	v2 = adc_value;

	if (stopping)
		adc_value = 0xffff;
	else {
		adc_channel = 2;
		adc_sample();
	}

	v2 |= (adc_value.low8 >> 4) & 0xf;

	/*
	 * If we've just switched then
	 * write the header info.
	 */
	if (buffer_switched) {
		/*
		 * If this buffer isn't empty then discard this sample.
		 */
		if (buffer_state[filling_buffer]) {
			LED_RED = 1;
			skipped++;
			return;
		}
		*store_p++ = skipped.low8;
		*store_p++ = skipped.high8;
		skipped = 0;
		LED_RED = 0;
		buffer_switched = 0;
	}

	/*
	 * Store the sample.
	 */
	*store_p++ = v1.low8;
	*store_p++ = v1.high8;
	*store_p++ = v2.low8;
	*store_p++ = v2.high8;
	*store_p++ = adc_value.high8;

	/*
	 * Moving to next buffer?
	 */
	samples_till_switch -= 1;
	if (samples_till_switch == 0) {
		buffer_state[filling_buffer] = 1;	// mark as full
		samples_till_switch = SAMPLES_PER_BUFFER;
		filling_buffer += 1;
		if (filling_buffer == N_BUFFERS) {
			filling_buffer = 0;
			store_p = buffers;
		}
		buffer_switched = 1;
	}
}

/*
 * This routine creates a file header for the three-sample format.
 * The header must be <= 512 bytes long,
 * and it should be loaded at *df_stream_write_data.
 *
 * Current header is 16 bytes long.
 * We round up to 17 bytes to make it a multiple of a sample + 2.
 */
void
create_file_header(void)
{
	uns8 i, v;
	size2 uns8 *p;

#ifndef TEST_NO_WRITE
	/*
	 * Record in EEPROM that the ATMEL must be erased / reprogrammed
	 */
	Write_b_eep(EPROM_ATMEL_READY, 0);
#endif // TEST_NO_WRITE

	tod_burst_read();

	p = buffers;

	/*
	 * Format, 2-byte field.  This is format #1.
	 */
	*p++ = 1;
	*p++ = 0;

	/*
	 * Channels are 0, 1, and 2.
	 */
	*p++ = 0;
	*p++ = 1;
	*p++ = 2;

	/*
	 * Bipolar or Unipolar values?
	 * 1 == bipolar.
	 */
	v = 1;
	if (adc_unipolar)
		v = 0;
	// for now all three are identical.
	*p++ = v;
	*p++ = v;
	*p++ = v;
	
	/*
	 * Next, tod, 8 bytes.
	 * Just copies out the TOD burst read buffer.
	 * Interpreting that is an excersize for the student.
	 */
	for (i = 0; i < 8; i++) {
		v = tod_burst_buffer[i];
		*p++ = v;
	}

	/* padding */
	*p++ = 0;
}

/*
 * This is the base-level code.
 * It waits for trigger,
 * writes the header,
 * then drives data to atmel.
 */
void
run_main(void)
{
	uns16 ctr;
	uns8 chunk;
	bit rs_state;

	found_eof = 0;

	/*
	 * Wait for Record/Stop switch.
	 * To debounce we want to see it stay high for 1000 samples.
	 */
    wait_for_record_stop:
	/*
	 * This version of the loop waits for the run/stop switch
	 * while running at 31 KHz. (About 8K instructions/second.)
	 * My guess is this loop runs about 400 times per second.
	 *
	 * Running in slow mode should signficantly reduce power
	 * consumption and increase battery life.
	 *
	 * Debouncing is by waiting for 2 consecutive 
	 * samples when the record/stop switch is set.
	 */
    	enter_slowmo();
    	rs_state = 0;
    	for (ctr = 0; ; ctr++) {
		if (ctr & 0x01F8) 
			LED_GREEN = 0;
		else
			LED_GREEN = 1;
		if (RECORD_STOP) {
			if (rs_state)
				break;
			rs_state = 1;
		} else
			rs_state = 0;
	}
	RAW_0 = LOAD_ON;
	LED_GREEN = 1;
	exit_slowmo();

	/*
	 * Fill the first buffer with the header info.
	 * Must do this before running = 1;
	 */
	create_file_header();

	/*
	 * Set up the buffer machinery.
	 */
	store_p = buffers + 17;	// skip over header
	skipped = 0;
	samples_till_switch = SAMPLES_PER_BUFFER - 3; // header consumed 3 samples

	buffer_state[0] = 0;
	buffer_state[1] = 0;
	buffer_state[2] = 0;
	buffer_switched = 0;

	/*
	 * Start sampling data, wait for trigger.
	 */
	running = 1;
	df_stream_begin();

	/*
	 * Loop forever writing data.
	 * Periodically check the record-stop switch.
	 * Look for EOF conditions.
	 */
	for (chunk = 0; chunk < n_chunks; chunk++) {
		df_lba = chunk_base[chunk];
		for (ctr = chunk_size[chunk]; ctr; ctr--) {
			if (df_lba & 0x4) 
				LED_GREEN = 1;
			else
				LED_GREEN = 0;

			/*
			 * Set up the write pointer.
			 */
			switch(next_to_write) {
			    case 0:
				df_stream_write_data = buffers;
				break;
			    case 1:
				df_stream_write_data = buffers + BLOCK_SIZE;
				break;
			    case 2:
				df_stream_write_data = buffers + 2*BLOCK_SIZE;
				break;
			}

			/*
			 * Check for manually requested stop.
			 */
			if (stopping || RECORD_STOP == 0) {
				running = 0;
				RAW_0 = LOAD_OFF;	// turn off load
				df_stream_write_data[0] = 0;
				df_stream_write_data[1] = 0;
				df_stream_write_data[2] = 0;
				df_stream_write_data[3] = 0;
#ifndef TEST_NO_WRITE
				df_stream_write();
#endif  // TEST_NO_WRITE
				goto done;
			}

			/*
			 * Wait for the next buffer to be full of data.
			 */
			while (buffer_state[next_to_write] == 0)
				;

#ifndef TEST_NO_WRITE
			df_stream_write();
#endif  // TEST_NO_WRITE

			/*
			 * Mark the buffer as empty, then move on.
			 */
			buffer_state[next_to_write] = 0;
			next_to_write++;
			if (next_to_write >= N_BUFFERS)
				next_to_write = 0;

			df_lba++;
		}
	}
	found_eof = 1;

	/*
	 * Finish up
	 */
   done:
	running = 0;
	RAW_0 = LOAD_OFF;	// turn off load
	LED_GREEN = 0;
	nop2();
	df_stream_term();

	adc_power_down();
}
