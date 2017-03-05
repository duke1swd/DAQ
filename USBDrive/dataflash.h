
DWORD df_ReadCapacity(void);
WORD df_ReadSectorSize(void);
void df_InitIO(void);

BYTE df_MediaDetect(void);
BYTE df_MediaInitialize(void);
BYTE df_SectorRead(DWORD sector_addr, BYTE* buffer);
BYTE df_SectorWrite(DWORD sector_addr, BYTE* buffer, BYTE allowWriteToZero);

BYTE df_WriteProtectState(void);
void df_ShutdownMedia(void);

/*
 * Some people like the short names.
 */
#define	MDD_ReadCapacity	df_ReadCapacity
#define	MDD_ReadSectorSize	df_ReadSectorSize
#define	MDD_InitIO		df_InitIO
#define	MDD_MediaDetect		df_MediaDetect
#define	MDD_MediaInitialize	df_MediaInitialize
#define	MDD_SectorRead		df_SectorRead
#define	MDD_SectorWrite		df_SectorWrite
#define	MDD_WriteProtectState	df_WriteProtectState
#define	MDD_ShutdownMedia	df_ShutdownMedia
