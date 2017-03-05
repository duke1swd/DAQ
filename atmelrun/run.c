//#define	TEST_NO_WRITE
//#define	DEBUG_3

/*
 * This module is the guts of the data gather routine.
 *
 

 *
 * There are three parts.
 *
 * There is a ring of three buffers, each 512 bytes.
 *
 * There is an interrupt routine.  It runs off the high
 * priority interrupt, 10,000 times per second.
 *
 * There is a base level routine that pushes data to the
 * dataflash card.
 */

/*
 * Buffers.
 * WARNING: The first of these overlaps the main df buffer.
 *
 */
#ifdef THREECHANNEL

/*
 * The three channels are assumed to be 0, 1, and 2.
 */
#define	SAMPLES_PER_BUFFER	((BLOCK_SIZE - 2) / 5)	// 5 bytes/samp
#define	ADC_TRIGGER_CHANNEL	0	// chamber
#define	ADC_PRESSURE_CHANNEL	2	// N2O

#else // THREECHANNEL

/*
 * For now we are using channel 5.
 */
#define	SAMPLES_PER_BUFFER	(BLOCK_SIZE/2 - 1)	// 2 bytes/samp
#define	ADC_TRIGGER_CHANNEL	5

#endif // THREECHANNEL

#define	N_BUFFERS		3
unsigned char buffers[BLOCK_SIZE * N_BUFFERS] @ 0x100;

#define	TRIGGER_LEN	10

#define	PRESSURE_SENSE	(500*16)	// about 12 PSI gauge

uns8 next_to_write;
uns8 filling_buffer;
uns16 samples_till_switch;
bit triggered;
bit pressured;	// some pressure sensed 

/*
 * We are not triggerable until one buffer is completely full
 * and we've switched to the next.
 */
bit triggerable;
bit running;
bit buffer_switched;

size2 unsigned char *store_p;

/*
 * Record skipped samples.
 */
uns16 skipped;

/*
 * Buffer state is 0 if empty, 1 if full.
 */
uns8 buffer_state[N_BUFFERS];

#pragma rambank 0

uns16 trigger_threshold;
uns16 trigger_value;
uns8 trigger_count;
uns8 trigger_ptr;
uns8 trigger_bits[TRIGGER_LEN];
bit trigger_threshold_set;
uns24 trigger_ave;

#pragma rambank -

/*
 * This routine returns true if we've matched the trigger
 * condition.
 */
uns8
trigger_cond(void)
{
	bit b;

	/*
	 * Average the first 256 samples and use that
	 * to establish the threshold.
	 */
	if (!trigger_threshold_set) {
		trigger_ave += trigger_value;
		trigger_count++;
		if (trigger_count == 0) {
			trigger_threshold = trigger_ave >> 8;
			trigger_threshold += 8;
			trigger_threshold_set = 1;
		}
		return 0;	// not yet triggered, still averaging.
	}

	b = 0;
	if (trigger_value >= trigger_threshold) {
		trigger_count++;
		b = 1;
	}
	//LED_RED = b;

	if (trigger_bits[trigger_ptr])
		trigger_count--;

	trigger_bits[trigger_ptr] = b;

	trigger_ptr++;
	if (trigger_ptr >= TRIGGER_LEN)
		trigger_ptr = 0;

	if (triggerable && trigger_count >= TRIGGER_LEN - 1)
		return 1;
	return 0;
}

void
run_init(void)
{
	uns8 i;

	adc_unipolar = 1;	// the pressure sensors are unipolar.
	next_to_write = 0;
	filling_buffer = 0;
	triggered = 0;
	pressured = 0;
	triggerable = 0;
	trigger_threshold_set = 0;
	trigger_ave = 0;
	running = 0;
	adc_power_down();
	store_p = buffers;
	skipped = 0;
	samples_till_switch = SAMPLES_PER_BUFFER;

	buffer_state[0] = 0;
	buffer_state[1] = 0;
	buffer_state[2] = 0;
	buffer_switched = 1;

	/* Set up the trigger variables. */
	for (i = 0; i < TRIGGER_LEN; i++)
		trigger_bits[i] = 0;
	trigger_count = 0;
	trigger_ptr = 0;
	//trigger_threshold.high8 = Read_b_eep(EPROM_TRIGGER_THRESH);
	//trigger_threshold.low8 = 0;
}

/*
 * This routine runs off the interrupt.
 * It is responsible for gathering data and
 * putting it into the output buffers.
 */

void
adc_isr(void)
{
#ifdef THREECHANNEL
	uns16 v1, v2;
#endif // THREECHANNEL

	if (!running)
		return;

	pressured = 0;

#ifdef THREECHANNEL
	/*
	 * Gather up three channels.
	 * First 2 bytes are the first sample, the opto channels,
	 * and 2 marker bits.
	 * Next 12 bits are the second sample.
	 * Low 4 bits of word 2 (byte 3) are the low bits of sample 3.
	 */
	adc_channel = 0;
	adc_sample();
#if ADC_TRIGGER_CHANNEL == 0
	trigger_value = adc_value;
#endif
#if ADC_PRESSURE_CHANNEL == 0
	if (adc_value > PRESSURE_SENSE)
		pressured = 1;
#endif
	if (!OPTO_0)
		adc_value.3 = 1;
	if (!OPTO_1)
		adc_value.2 = 1;
	adc_value.1 = 1;
	v1 = adc_value;

#ifdef DEBUG_3
	adc_channel = 0;
#else
	adc_channel = 1;
#endif
	adc_sample();
#if ADC_TRIGGER_CHANNEL == 1
	trigger_value = adc_value;
#endif
#if ADC_PRESSURE_CHANNEL == 1
	if (adc_value > PRESSURE_SENSE)
		pressured = 1;
#endif
	v2 = adc_value;

#ifdef DEBUG_3
	adc_channel = 0;
#else
	adc_channel = 2;
#endif
	adc_sample();
#if ADC_TRIGGER_CHANNEL == 2
	trigger_value = adc_value;
#endif
#if ADC_PRESSURE_CHANNEL == 2
	if (adc_value > PRESSURE_SENSE)
		pressured = 1;
#endif

	v2 |= (adc_value.low8 >> 4) & 0xf;

#else // THREECHANNEL

	/*
	 * Get the 16-bit record.
	 */
	adc_channel = ADC_TRIGGER_CHANNEL;
	adc_sample();
	trigger_value = adc_value;
	if (!OPTO_0)
		adc_value.3 = 1;
	if (!OPTO_1)
		adc_value.2 = 1;
	adc_value.1 = 1;
#endif // THREECHANNEL

	/*
	 * If we've just switched then
	 * write the header info.
	 */
	if (buffer_switched) {
		/*
		 * If this buffer isn't empty then discard this sample.
		 */
		if (buffer_state[filling_buffer]) {
			if (triggered) {
				LED_RED = 1;
				EXT_RED = 1;
				skipped++;
				return;
			} else {
				/*
				 * If we are overwriting a buffer
				 * pre trigger, mark it empty.
				 */
				buffer_state[filling_buffer] = 0;
			}
		}
		*store_p++ = skipped.low8;
		*store_p++ = skipped.high8;
		skipped = 0;
		LED_RED = 0;
		EXT_RED = 0;
		buffer_switched = 0;
	}

	/*
	 * Store the sample.
	 */
#ifdef THREECHANNEL
	*store_p++ = v1.low8;
	*store_p++ = v1.high8;
	*store_p++ = v2.low8;
	*store_p++ = v2.high8;
	*store_p++ = adc_value.high8;
#else // THREECHANNEL
	*store_p++ = adc_value.low8;
	*store_p++ = adc_value.high8;
#endif // THREECHANNEL

	/*
	 * Moving to next buffer?
	 */
	samples_till_switch -= 1;
	if (samples_till_switch == 0) {
		buffer_state[filling_buffer] = 1;	// mark as full
		triggerable = 1;
		samples_till_switch = SAMPLES_PER_BUFFER;
		filling_buffer += 1;
		if (filling_buffer == N_BUFFERS) {
			filling_buffer = 0;
			store_p = buffers;
		}
		buffer_switched = 1;
	}


#ifdef TRIGGER_IMMEDIATELY
	/* This is useful when running the DAQ in the lab. */
	if (!triggered && triggerable) {
		if (samples_till_switch == 16)
			triggered = 1;
	}
#endif

	/*
	 * If data capture is running, we are done.
	 */
	if (triggered)
		return;
	
	/*
	 * Trigger on any input on the opto-isolated inputs.
	 */
	if ((!OPTO_0 || !OPTO_1) && triggerable)
		triggered = 1;

#ifdef NOTWORKING
	if (trigger_cond())
		triggered = 1;
#endif

	if (!triggered)
		return;

	/*
	 * Set up the output writer.
	 * First, mark the next buffer we will get to as free.
	 * Then, set the one after that as the next to write.
	 *
	 * WARNING: this code ASSUMES there are 3 buffers.
	 */
	next_to_write = filling_buffer + 1;
	if (next_to_write == N_BUFFERS)
		next_to_write = 0;
	buffer_state[next_to_write] = 0;

	next_to_write++;
	if (next_to_write == N_BUFFERS)
		next_to_write = 0;
}

#ifdef THREECHANNEL

/*
 * This routine creates a file header for the three-sample format.
 * The header must be <= 512 bytes long,
 * and it should be loaded at *df_stream_write_data.
 *
 * Current header is 16 bytes long.
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

	p = df_stream_write_data;

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
}
#else // THREECHANNEL

/*
 * This routine creates a file header for the single-sample format.
 * The header must be <= 512 bytes long,
 * and it should be loaded at *df_stream_write_data.
 *
 * Current header is 12 bytes long.
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

	p = df_stream_write_data;

	/*
	 * Format, 2-byte field.  This is format #2.
	 */
	*p++ = 2;
	*p++ = 0;

	/*
	 * Channel is 5.
	 */
	*p++ = 5;

	/*
	 * Bipolar or Unipolar values?
	 * 1 == bipolar.
	 */
	v = 1;
	if (adc_unipolar)
		v = 0;
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
}

#endif // THREECHANNEL

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
	bit first_buffer;
	bit rs_state;

	found_eof = 0;

	/*
	 * Wait for Record/Stop switch.
	 * To debounce we want to see it stay high for 1000 samples.
	 */
    wait_for_record_stop:
    	EXT_GREEN = 0;		// step 3a, external LED *OFF*.
//#define OLD
#ifdef OLD
	/*
	 * This version of the loop waits for the run/stop switch
	 * while at full speed.
	 */
    record_stop:
	if (lbolt & 0x1F)
		LED_GREEN = 0;
	else
		LED_GREEN = 1;

	if (RECORD_STOP == 0) {
		ctr = 0;
		goto record_stop;
	}
	ctr++;

	if (ctr < 1000)
		goto record_stop;
#else // OLD
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
	exit_slowmo();
#endif // OLD

	/*
	 * Start sampling data, wait for trigger.
	 */
	running = 1;
	df_stream_begin();

	LED_GREEN = 1;
	while (!triggered) {
		if (RECORD_STOP == 0) {
			/*
			 * False alarm.  He's turned of the run/stop switch.
			 * Reset everything.
			 */
			run_init();
			goto wait_for_record_stop;
		}
		/*
		 * Step 3b, external LED blinks slowly
		 * If no pressure, flashes every 1.5 seconds.
		 * If pressure, blinks off every 1.5 seconds.
		 */
		if (pressured) {
		    if (lbolt & 0x3E)
			EXT_GREEN = 1;
		    else
			EXT_GREEN = 0;
		} else {
		    if (lbolt & 0x3E)
			EXT_GREEN = 0;
		    else
			EXT_GREEN = 1;
		}
	}

	/*
	 * Loop forever writing data.
	 * Periodically check the record-stop switch.
	 * Look for EOF conditions.
	 * If either opto is on, then external is on.  State 4b.
	 */
	first_buffer = 1;
	for (chunk = 0; chunk < n_chunks; chunk++) {
		df_lba = chunk_base[chunk];
		for (ctr = chunk_size[chunk]; ctr; ctr--) {
			if (df_lba & 0x4) 
				LED_GREEN = 1;
			else
				LED_GREEN = 0;

			if (!OPTO_0 || !OPTO_1)
				EXT_GREEN = 1;
			else if (df_lba & 0x1E)
				EXT_GREEN = 0;
			else
				EXT_GREEN = 1;

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

			if (first_buffer) {
				create_file_header();
				first_buffer = 0;
			}


			/*
			 * Check for manually requested stop.
			 */
			if (RECORD_STOP == 0) {
				running = 0;
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
	LED_GREEN = 0;
	nop2();
	EXT_GREEN = 0;
	df_stream_term();

	adc_power_down();
}
