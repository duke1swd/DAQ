/*
 * This lays out how eprom is used.
 */

#define	EPROM_CURRRENT_MODE	0	// currently loaded module 

#define	EPROM_ERR1_CODE		1	// error code
#define	EPROM_ERR1_SUBCODE	2	// error code
#define	EPROM_ERR1_DETAIL	3	// error detail status byte

#define	EPROM_ERR2_CODE		4	// error code
#define	EPROM_ERR2_SUBCODE	5	// error code
#define	EPROM_ERR2_DETAIL	6	// error detail status byte

/*
 * Bytes 7 thorugh 15 are currently unused.
 */

/*
 * The next 66 bytes hold the file names of the bootable modules.
 */
#define	EPROM_MODE_LEN		11	// length of a file name.
#define	EPROM_MODE_2		16
#define	EPROM_MODE_3		(EPROM_MODE_2 + EPROM_MODE_LEN * 1)
#define	EPROM_MODE_4		(EPROM_MODE_2 + EPROM_MODE_LEN * 2)
#define	EPROM_MODE_5		(EPROM_MODE_2 + EPROM_MODE_LEN * 3)
#define	EPROM_MODE_6		(EPROM_MODE_2 + EPROM_MODE_LEN * 4)
#define	EPROM_MODE_7		(EPROM_MODE_2 + EPROM_MODE_LEN * 5)

/*
 * Programs can put a message into eprom.
 * When the mode2com module is loaded it can print it.
 *
 * The message starts at byte 82 and ends on or before byte 255.
 * Terminated by end of eprom or by '\0' or 0xff.
 */

#define	EPROM_MSG		(EPROM_MODE_2 + EPROM_MODE_LEN * 6)
#define	EPROM_MSG_BYTES		(255-MSG)
