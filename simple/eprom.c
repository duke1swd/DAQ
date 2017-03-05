/*
 * This routines read and write stuff to eprom.
 * The only use we make of this is to store the last
 * file number and retrieve it.
 *
 * For now these routines use DATA EEPROM.
 * 
 * The chip specifications are a bit unclear about whether
 * this is a good idea.  It might be better to use
 * program memory instead.
 */

#define	CHECK	0x55
#define	MIN_EPROM_ADDR	0x48
 
unsigned char eprom_addr;

/* 
 * These EPROM routines are a bit obscure.
 * To understand them read the chip spec, chapter 7.
 */
unsigned char
eprom_read_1()
{
	EEADR = eprom_addr;
	eprom_addr += 1;
	EEPGD = 0;
	EECON1.0 = 1;	// start the read cycle
	return EEDATA;
}

void
eprom_write_1(unsigned char v)
{
	EEIF = 0;		// clear the interrupt flag
	EEADR = eprom_addr;	// set the address to write
	eprom_addr += 1;
	EEPGD = 0;		// access is to data eprom
	EEDATA = v;		// here is the data
	WREN = 1;		// enable writes to eprom
	EECON2 = 0x55;		// magic sequence, step 1
	EECON2 = 0xaa;		// magic sequence, step 2
	EECON1.1 = 1;		// start the write cycle.
	while (!EEIF) ;		// wait for the write to complete
	WREN = 0;		// disable writes
	EEIF = 0;		// clear the interrupt flag
}

/*
 * Returns true if EPROM had valid data
 */
int
eprom_read()
{
	eprom_addr = MIN_EPROM_ADDR;

	if (eprom_read_1() != CHECK)
		return 0;

	file_number= eprom_read_1();
	return 1;
}

void
eprom_write()
{
	eprom_addr = MIN_EPROM_ADDR;

	eprom_write_1(CHECK);

	eprom_write_1(file_number);
}
