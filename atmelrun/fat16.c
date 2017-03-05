/* 
 * This code finds the space on flash where we will put the data.
 *
 * This is a very primitive, read-only, implementation of FAT16.
 * Much of this code is shared with the boot2 program.
 */

/*
 * This defines the chunk structure.
 * Each chunk is a contigous set of LBAs where
 * we can store blocks.
 */
#define	MAX_CHUNKS	4
#define	MAX_CHUNK_LEN	65535
uns24 chunk_base[MAX_CHUNKS];
uns16 chunk_size[MAX_CHUNKS];
unsigned char n_chunks;

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


#define	ERROR_PHASE1_1		3	// bad formatted
#define	ERROR_PHASE1_2		4 	// Too many chunks

#define	LOAD_ADDRESS		0x1000		// location in program memory
#define	LOAD_LOW		(LOAD_ADDRESS & 0xFF)
#define	LOAD_HIGH		(LOAD_ADDRESS >> 8)

/*
 * Some things we know about this file system.
 * These are set up by boot_1().
 */
#pragma rambank 0
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
uns24 old_lba;

#pragma rambank -

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
find_name(void)
{
	unsigned char i;
	unsigned char addr;
	unsigned char index;
	unsigned char *dir_p;

	/*
	 * First, read the MBR enough to find the root directory.
	 */
	df_lba = 0;
	df_read();

	/*
	 * Check if things are formmatted.
	 */
	if (buffer[BS_55AA] != 0x55 || buffer[BS_55AA+1] != 0xAA)
		blink_error_code(ERROR_PHASE1_1, 1, buffer[BS_55AA]);

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
		blink_error_code(ERROR_PHASE1_1, 2, sec_per_clus);
	}
	max_cluster += 2;


	/* 
	 * Loop through the directory looking for a match.
	 */
	addr = EPROM_DATA_NAME;

	df_lba = dirbase;
	for (sect = n_dirent / DIR_per_Sect; sect; sect--) {
		// skip to next sector
		df_read();
		df_lba++;
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
	blink_error_code(ERROR_PHASE1_1, 3, 0);

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
	df_lba = (uns24)cluster - 2;
	df_lba *= (uns24)sec_per_clus;
	df_lba += (uns24)database;

	sect = 0;
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
		df_lba = fatbase;
		df_lba += cluster.high8;
		if (old_lba != df_lba)
			df_read();
		old_lba = df_lba;
		cluster = get_number((uns16)(cluster.low8) * 2);
		if (cluster < 2 || cluster >= max_cluster)
			return 0;

		new_cluster();
	} else {
		df_lba++;
	}
	return 1;
}

/*
 * Map the data file by reading the metadata.
 *
 * We record the file as a series of chunks.  No more than MAX_CHUNKS.
 */
void
find_space(void)
{
	bit new_chunk;
	unsigned char i;
	unsigned char cur_chunk;
	uns16 cur_chunk_size;
	uns24 next_lba;

	/*
	 * First map the name to a cluster.
	 */
	find_name();
	old_lba = 0;

	/*
	 * Init the chunk structure.
	 */
	n_chunks = 0;
	for (i = 0; i < MAX_CHUNKS; i++)
		chunk_size[i] = 0;

	/*
	 * Walk the cluster chain.
	 */
	new_cluster();
	next_lba = df_lba + 1;
	chunk_base[0] = df_lba;
	cur_chunk_size = 1;
	while (next_sect()) {

		/*
		 * Skip to the next chunk if we are
		 * discontiguous or this chunk has
		 * hit maximum size.
		 */
		if (cur_chunk_size < MAX_CHUNK_LEN &&
		    df_lba == next_lba)
			// Update current chunk
			cur_chunk_size += 1;
		else {
			// skip to new chunk
			chunk_size[n_chunks] = cur_chunk_size;
			cur_chunk_size = 1;
			if (n_chunks == MAX_CHUNKS)
				blink_error_code(ERROR_PHASE1_2, 0, n_chunks);
			n_chunks++;
			chunk_base[n_chunks] = df_lba;
		}
		next_lba = df_lba + 1;
	}
	chunk_size[n_chunks] = cur_chunk_size;
	n_chunks++;
}

void
validate_space()
{
	uns8 i;
	uns24 cb;

	for (i = 0; i < n_chunks; i++) {
		cb = chunk_base[i];
		if (cb < 4)
			blink_error_code(ERROR_PHASE1_2, 1, i);
		if (cb & 0x1)
			blink_error_code(ERROR_PHASE1_2, 2, i);
		if (chunk_size[i] == 0)
			blink_error_code(ERROR_PHASE1_2, 3, i);
		if (chunk_size[i] & 0x1)
			blink_error_code(ERROR_PHASE1_2, 4, i);
	}
}
