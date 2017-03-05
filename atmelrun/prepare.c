/*
 * This module sweeps through the atmel, page at a time.
 * It calls df_erase() or df_reprogram on each one.
 */

#define	PAGE_SIZE	2	// in sectors
#define	PAGES		8192	// how many pages are there anyway?

void
prepare_dataflash(void)
{
	uns24 i;
	unsigned char j;
	uns24 base;
	uns24 top;

	if (Read_b_eep(EPROM_ATMEL_READY))
		return;

#ifdef NOERASE
	/*
	 * If we get here then we are not supposed to proceed.
	 * We are running the data capture program but the
	 * flash has not been erased.
	 */
	LED_GREEN = 0;
	LED_RED_LAT = 1;
	EXT_RED = 1;
	for (;;) ;
#endif

	for (i = 0; i < PAGES * PAGE_SIZE; i += PAGE_SIZE) {
		/*
		 * Turning on the record/stop switch now is wrong.
		 */
		LED_RED = RECORD_STOP;

		/*
		 * Blink the green LED at about 1 HZ
		 */
		df_lba = i;
		LED_GREEN = 0;
		if (i.7)
			LED_GREEN = 1;

		for (j = 0; j < n_chunks; j++) {
			base = chunk_base[j];
			if (i < base)
				continue;
			top = base + chunk_size[j];
			if (i >= top)
				continue;
			df_erase();
			goto next_page;
		}
		df_reprogram();
	    next_page:;
	}
	LED_RED = 0;

	Write_b_eep(EPROM_ATMEL_READY, 1);
}
