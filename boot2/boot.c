/* 
 * This code does the boot operation.
 *
 * This is a very primitive, read-only, implementation of FAT16.
 */

/* Offset of FAT structure members */

/*
 * BPB == Bios Parameter Block.
 * These fields are always the first sector of the volume.
 * BS == Boot Sector.  Not quite the same.  Always sector 0 (I think).
 */

/*
 * These first fields are constant, regardless of FAT12/16/32.
 */

#define BS_jmpBoot			0
#define BS_OEMName			3
#define BPB_BytsPerSec		11
#define BPB_SecPerClus		13
#define BPB_RsvdSecCnt		14
#define BPB_NumFATs			16
#define BPB_RootEntCnt		17
#define BPB_TotSec16		19
#define BPB_Media			21
#define BPB_FATSz16			22
#define BPB_SecPerTrk		24
#define BPB_NumHeads		26
#define BPB_HiddSec			28// must be zero if not partitioned.
#define BPB_TotSec32		32


#define BS_55AA				510

#define BS_DrvNum			36
#define BS_BootSig			38
#define BS_VolID			39
#define BS_VolLab			43
#define BS_FilSysType		54

#define BPB_FATSz32			36
#define BPB_ExtFlags		40
#define BPB_FSVer			42
#define BPB_RootClus		44
#define BPB_FSInfo			48
#define BPB_BkBootSec		50
#define BS_DrvNum32			64
#define BS_BootSig32		66
#define BS_VolID32			67
#define BS_VolLab32			71
#define BS_FilSysType32		82

/*
 * Directory entries
 */
#define	DIR_Name			0
#define	DIR_Attr			11
#define	DIR_NTres			12
#define	DIR_CrtTime			14
#define	DIR_CrtDate			16
#define	DIR_FstClusHI		20
#define	DIR_WrtTime			22
#define	DIR_WrtDate			24
#define	DIR_FstClusLO		26
#define	DIR_FileSize		28

#define	DIR_per_Sect		16		// directory entries per sector
#define	DIR_per_Sect_Shift	4


#define	SD_ERROR_BOOT_1		SD_ERR(3, 1)  // bad formatted
#define	SD_ERROR_BOOT_2		SD_ERR(3, 2)  // file not found
#define	SD_ERROR_BOOT_3		SD_ERR(3, 3)  // bad format (sec/cluster)
#define	SD_ERROR_BOOT_4		SD_ERR(3, 4)  // boot file missing magic number
#define	SD_ERROR_BOOT_5		SD_ERR(3, 5)  // premature EOF on boot file.
#define	SD_ERROR_BOOT_6		SD_ERR(3, 6)  // Bad start address
#define	SD_ERROR_BOOT_7		SD_ERR(4, 1)  // Memory fails to verify.

#define	LOAD_ADDRESS		0x1000		// location in program memory
#define	LOAD_LOW		(LOAD_ADDRESS & 0xFF)
#define	LOAD_HIGH		(LOAD_ADDRESS >> 8)

/*
 * The mode is an number in the range [1-7].
 */
unsigned char boot_mode;

/*
 * Some things we know about this file system.
 * These are set up by boot_1().
 */
uns16 fatbase;		// lba
uns16 fatsize;
uns16 dirbase;		// lba
uns16 database;		// lba
uns16 totalsect;	// total number of sectors in the media. (16 MB max).
uns16 n_dirent;		// count of entries in the root directory
uns16 n_dirsect;
unsigned char sec_per_clus;	// sectors in a cluster
uns16 max_cluster;	// used to determin EOF.
uns16 file_size;	// max of 64 KB.

/*
 * These variables define our state as we follow a cluster chain.
 */
uns16 cluster;		// current cluster number
unsigned char sect;	// sector within cluster.

uns16
get_number(uns16 offset)
{
	uns16 r;

	r.low8 = buffer[offset];
	r.high8 = buffer[offset+1];
	return r;
}

#define	LOAD_WORD(target, offset) target.low8 = buffer[offset]; \
		target.high8 = buffer[offset+1]

/*
 * Map the name to a cluster chain.
 */
void
boot_1(void)
{
	unsigned char i;
	unsigned char addr;
	unsigned char index;
	unsigned char *dir_p;

	/*
	 * First, read the MBR enough to find the root directory.
	 */
	sd_lba = 0;
	sd_read();

	/*
	 * Check if things are formmatted.
	 */
	if (buffer[BS_55AA] != 0x55 || buffer[BS_55AA+1] != 0xAA)
		sd_error(SD_ERROR_BOOT_1, buffer[BS_55AA]);

	/*
	 * Load up some key attributes of the file system.
	 */
	//fatbase = get_number(BPB_RsvdSecCnt);
	LOAD_WORD(fatbase, BPB_RsvdSecCnt);

	//fatsize = get_number(BPB_FATSz16);
	LOAD_WORD(fatsize, BPB_FATSz16);
	fatsize *= buffer[BPB_NumFATs];

	dirbase = fatbase + fatsize;
	//n_dirent = get_number(BPB_RootEntCnt);
	LOAD_WORD(n_dirent, BPB_RootEntCnt);
	n_dirsect = n_dirent >> DIR_per_Sect_Shift;

	sec_per_clus = buffer[BPB_SecPerClus];
	database = dirbase + n_dirsect;

	//totalsect = get_number(BPB_TotSec16);
	LOAD_WORD(totalsect, BPB_TotSec16);
	if (!totalsect) {
		//totalsect = get_number(BPB_TotSec32+2);
		LOAD_WORD(totalsect, BPB_TotSec32+2);
	}

	/*
	 * max_cluster = (totalsect - get_number(BPB_RsvdSecCnt)
	 *		- fatsize - n_dirsect) / sec_per_clus + 2;
	 */

	max_cluster = totalsect;
	max_cluster -= get_number(BPB_RsvdSecCnt);
	max_cluster -= fatsize;
	max_cluster -= n_dirsect;;

	switch(sec_per_clus) {
	    case 4:
		max_cluster >>= 1;
	    case 2:
		max_cluster >>= 1;
	    case 1:
		break;
	    default:
		sd_error(SD_ERROR_BOOT_3, sec_per_clus);
	}
	max_cluster += 2;


	/* 
	 * Loop through the directory looking for a match.
	 */
	addr = boot_mode * EPROM_MODE_LEN +
			EPROM_MODE_1 - EPROM_MODE_LEN;

	sd_lba = dirbase;
	for (sect = n_dirent / DIR_per_Sect; sect; sect--) {
		// skip to next sector
		sd_read();
		sd_lba++;
		// loop over all entries in this sector
		dir_p = buffer;
		for (index = 0; index < DIR_per_Sect; index++) {
			// loop over all bytes in this entry
			for (i = 0; i < 11; i++)
				if (Read_b_eep(addr + i) != dir_p[i])
				    	goto next_e;
			// matched!
			goto found;
		    next_e:
		    	dir_p += 32;
		}
	}
	sd_error(SD_ERROR_BOOT_2, 0);

    found:
    	dir_p += DIR_FstClusLO;
	cluster.low8 = *dir_p++;
	cluster.high8 = *dir_p++;
	dir_p += DIR_FileSize - (DIR_FstClusLO + 2);
	// NOTE ASSUMPTION of 16 bit file size.
	file_size.low8 = *dir_p++;
	file_size.high8 = *dir_p;
}

/*
 * Code to follow a cluster chain.
 */

/*
 * This routine maps a cluster number to an LBA.
 * On exit lba is correctly set and the block has been read.
 */
void
new_cluster(void)
{
	sd_lba = (uns24)cluster - 2;
	sd_lba *= (uns24)sec_per_clus;
	sd_lba += (uns24)database;

	sect = 0;
	sd_read();
}

/*
 * This routine advances to the next sector (or cluster).
 * Returns true if there is another cluster.  In this
 * case the lba is set and the block has been read.
 * Returns false if there is not another cluster (EOF).
 */
unsigned char
next_sect(void)
{
	sect++;
	if (sect == sec_per_clus) {
		/*
		 * Find the next cluster in the chain.
		 */
		sd_lba = fatbase;
		sd_lba += cluster.high8;
		sd_read();
		cluster = get_number((uns16)(cluster.low8) * 2);
		if (cluster < 2 || cluster >= max_cluster)
			return 0;

		new_cluster();
	} else {
		sd_lba++;
		sd_read();
	}
	return 1;
}

#ifdef PROGRAM_VERIFY

uns16 save_cluster;

void
seek_save(void)
{
	save_cluster = cluster;
}

void
seek_reset(void)
{
	cluster = save_cluster;
	new_cluster();
}

#endif // PROGRAM_VERIFY

void
start_write(void)
{
	EECON2 = 0x55;
	EECON2 = 0xAA;
	WR = 1;
}

/*
 * Process the .daq file.  See ~/hex/README for details on the file format.
 *
 * For now we ignore the version information.
 * We also ignore the fact that the last sector is short.  We program
 * the entire 512 bytes.
 */
void
boot_2(void)
{
	unsigned char i, lim;
	uns16 j;
	unsigned char sectors;
	uns24 save_lba;
	uns16 tblp;

	/*
	 * Load the first block of the file.
	 */
	new_cluster();
#ifdef PROGRAM_VERIFY
	seek_save();
#endif // PROGRAM_VERIFY
	if (buffer[0] != 0x44)
		sd_error(SD_ERROR_BOOT_4, buffer[0]);
	if (buffer[1] != 0x55)
		sd_error(SD_ERROR_BOOT_4, buffer[1]);

	/*
	 * Require a specific start address
	 */
	if (buffer[4] != LOAD_HIGH ||
	    buffer[5] != LOAD_LOW)
		sd_error(SD_ERROR_BOOT_6, buffer[4]);

	/*
	 * Now program the memory.
	 */
	tblp = LOAD_ADDRESS;
	for (sectors = buffer[6]; sectors > 0; sectors--) {

		/*
		 * Read in the next sector of the file.
		 */
		if (!next_sect())
			sd_error(SD_ERROR_BOOT_5, 0);

		/*
		 * Copy the bytes out.
		 * 32 bytes are programmed into flash at a time.
		 */
		for (j = 0; j < 512; j++) {
			LED_GREEN = 0;
#ifdef notdef
			if (j & 0x20)
				LED_GREEN = 1;
#endif // notdef

			TBLPTRU = 0;
			TBLPTRH = tblp.high8;
			TBLPTRL = tblp.low8;

			/*
			 * Each block of 64 bytes must first be erased.
			 */
			if ((j & 0x3f) == 0) {
				// program memory, erase, write enabled.
				EECON1 = 0x94;
				start_write();
			}

			TABLAT = buffer[j];
			tableWrite();

			/*
			 * Every block of 32 bytes must be written
			 * to flash memory.
			 */
			if ((j & 0x1f) == 0x1f) {
				// program memory, write enabled.
				EECON1 = 0x84;
				start_write();
			}

			tblp++;
		}
	}
	LED_GREEN = 0;

#ifdef PROGRAM_VERIFY
	/*
	 * Verify
	 */
	seek_reset();

	EECON1 = 0x80;	// program memory, write disabled.
	tblp.high8 = LOAD_HIGH;
	tblp.low8 = LOAD_LOW;

	for (sectors = buffer[6]; sectors > 0; sectors--) {

		/*
		 * Read in the next sector of the file.
		 */
		if (!next_sect())
			sd_error(SD_ERROR_BOOT_5, 1);

		/*
		 * Verify
		 */
		for (j = 0; j < 512; j++) {
			TBLPTRU = 0;
			TBLPTRH = tblp.high8;
			TBLPTRL = tblp.low8;
			tableReadInc();
			if (buffer[j] != TABLAT)
				sd_error(SD_ERROR_BOOT_7, TABLAT);
			tblp++;
		}
	}
#endif // PROGRAM_VERIFY
}
