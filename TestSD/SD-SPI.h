/******************************************************************************
 *
 *                Microchip Memory Disk Drive File System
 *
 ******************************************************************************
 * FileName:        SD-SPI.h
 * Dependencies:    GenericTypeDefs.h
 *					FSconfig.h
 *					FSDefs.h
 * Processor:       PIC18/PIC24/dsPIC30/dsPIC33/PIC32
 * Compiler:        C18/C30/C32
 * Company:         Microchip Technology, Inc.
 * Version:         1.1.1
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the “Company”) for its PICmicro® Microcontroller is intended and
 * supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PICmicro Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
*****************************************************************************/

// **
// ** This file modified by Stephen Daniel, 7/4/08
// ** Modified for use in Hybridsky.com data capture project.
// ** Modified to remove dependencies on the file system code.
// ** Also, stripped out all but PIC18 support for readability.
// **

#ifndef SDMMC_H
#define	SDMMC_H

#define USE_SDMMC

#define	USB_USE_MSD

#include "GenericTypeDefs.h"

#define	MEDIA_SECTOR_SIZE	512	// moved here from FSConfig.h

#ifdef SD_SPI_INTERNAL


#define   SYNC_MODE_FAST	0x00
#define   SYNC_MODE_MED		0x01
#define   SYNC_MODE_SLOW	0x02 
#define   BUS_MODE		0 
#define   SMP_PHASE		0x80


/*****************************************************************/
/*                  Strcutures and defines                       */
/*****************************************************************/


/* Command Operands */
#define BLOCKLEN_64                 0x0040
#define BLOCKLEN_128                0x0080
#define BLOCKLEN_256                0x0100
#define BLOCKLEN_512                0x0200

/* Data Token */
#define DATA_START_TOKEN            0xFE
#define DATA_MULT_WRT_START_TOK     0xFC
#define DATA_MULT_WRT_STOP_TOK      0xFD

/* Data Response */
#define DATA_ACCEPTED               0x05
#define DATA_CRC_ERR                0x0B
#define DATA_WRT_ERR                0x0D

#define MMC_Interrupt   *IntReg

#define MOREDATA    !0
#define NODATA      0


#define MMC_FLOATING_BUS    0xFF
#define MMC_BAD_RESPONSE    MMC_FLOATING_BUS
#define MMC_ILLEGAL_CMD     0x04
#define MMC_GOOD_CMD        0x00

// The SDMMC Commands
#define     cmdGO_IDLE_STATE        0
#define     cmdSEND_OP_COND         1        
#define     cmdSEND_CSD             9
#define     cmdSEND_CID             10
#define     cmdSTOP_TRANSMISSION    12
#define     cmdSEND_STATUS          13
#define     cmdSET_BLOCKLEN         16
#define     cmdREAD_SINGLE_BLOCK    17
#define     cmdREAD_MULTI_BLOCK     18
#define     cmdWRITE_SINGLE_BLOCK   24    
#define     cmdWRITE_MULTI_BLOCK    25
#define     cmdTAG_SECTOR_START     32
#define     cmdTAG_SECTOR_END       33
#define     cmdUNTAG_SECTOR         34
#define     cmdTAG_ERASE_GRP_START  35 
#define     cmdTAG_ERASE_GRP_END    36
#define     cmdUNTAG_ERASE_GRP      37
#define     cmdERASE                38
#define     cmdLOCK_UNLOCK          49
#define     cmdSD_APP_OP_COND       41
#define     cmdAPP_CMD              55
#define     cmdREAD_OCR             58
#define     cmdCRC_ON_OFF           59

// the various possible responses
typedef enum
{
    R1,
    R1b,
    R2,
    R3    // we don't use R3 since we don't care about OCR 
}RESP;

// The various command informations needed 
typedef struct
{
    BYTE      CmdCode;            // the command number
    BYTE      CRC;            // the CRC value (CRC's are not required once you turn the option off!)
    RESP    responsetype;   // the Response Type
    BYTE    moredataexpected;   // True if more data is expected
} typMMC_CMD;

typedef union
{
    struct
    {
	        BYTE field[6];
    };
    struct
    {
        BYTE crc;
        BYTE addr0;
        BYTE addr1;
        BYTE addr2;
        BYTE addr3;
        BYTE cmd;
    };
    struct
    {
        BYTE  END_BIT:1;
        BYTE  CRC7:7;
        DWORD     address;
        BYTE  CMD_INDEX:6;
        BYTE  TRANSMIT_BIT:1;
        BYTE  START_BIT:1;
    };
} CMD_PACKET;

typedef union
{
    BYTE _byte;
    struct
    {
        unsigned IN_IDLE_STATE:1;
        unsigned ERASE_RESET:1;
        unsigned ILLEGAL_CMD:1;
        unsigned CRC_ERR:1;
        unsigned ERASE_SEQ_ERR:1;
        unsigned ADDRESS_ERR:1;
        unsigned PARAM_ERR:1;
        unsigned B7:1;
    };
} RESPONSE_1;

typedef union
{
    WORD _word;
    struct
    {
        BYTE      _byte0;
        BYTE      _byte1;
    };
    struct
    {
        unsigned IN_IDLE_STATE:1;
        unsigned ERASE_RESET:1;
        unsigned ILLEGAL_CMD:1;
        unsigned CRC_ERR:1;
        unsigned ERASE_SEQ_ERR:1;
        unsigned ADDRESS_ERR:1;
        unsigned PARAM_ERR:1;
        unsigned B7:1;
        unsigned CARD_IS_LOCKED:1;
        unsigned WP_ERASE_SKIP_LK_FAIL:1;
        unsigned ERROR:1;
        unsigned CC_ERROR:1;
        unsigned CARD_ECC_FAIL:1;
        unsigned WP_VIOLATION:1;
        unsigned ERASE_PARAM:1;
        unsigned OUTRANGE_CSD_OVERWRITE:1;
    };
} RESPONSE_2;

typedef union
{
    RESPONSE_1  r1;  
    RESPONSE_2  r2;
}MMC_RESPONSE;


typedef union
{
    struct
    {
        DWORD _u320;
        DWORD _u321;
        DWORD _u322;
        DWORD _u323;
    };
    struct
    {
        BYTE _byte[16];
    };
    struct
    {
        unsigned NOT_USED           :1;
        unsigned CRC                :7; //bit 000 - 007
        
        unsigned ECC                :2;
        unsigned FILE_FORMAT        :2;
        unsigned TMP_WRITE_PROTECT  :1;
        unsigned PERM_WRITE_PROTECT :1;
        unsigned COPY               :1;
        unsigned FILE_FORMAT_GRP    :1; //bit 008 - 015
        
        unsigned RESERVED_1         :5;
        unsigned WRITE_BL_PARTIAL   :1;
        unsigned WRITE_BL_LEN_L     :2;
        
        unsigned WRITE_BL_LEN_H     :2;
        unsigned R2W_FACTOR         :3;
        unsigned DEFAULT_ECC        :2;
        unsigned WP_GRP_ENABLE      :1; //bit 016 - 031
        
        unsigned WP_GRP_SIZE        :5;
        unsigned ERASE_GRP_SIZE_L   :3;
        
        unsigned ERASE_GRP_SIZE_H   :2;
        unsigned SECTOR_SIZE        :5;
        unsigned C_SIZE_MULT_L      :1;
        
        unsigned C_SIZE_MULT_H      :2;
        unsigned VDD_W_CURR_MAX     :3;
        unsigned VDD_W_CUR_MIN      :3;
        
        unsigned VDD_R_CURR_MAX     :3;
        unsigned VDD_R_CURR_MIN     :3;
        unsigned C_SIZE_L           :2;
        
        unsigned C_SIZE_H           :8;
        
        unsigned C_SIZE_U           :2;
        unsigned RESERVED_2         :2;
        unsigned DSR_IMP            :1;
        unsigned READ_BLK_MISALIGN  :1;
        unsigned WRITE_BLK_MISALIGN :1;
        unsigned READ_BL_PARTIAL    :1;
        
        unsigned READ_BL_LEN        :4;
        unsigned CCC_L              :4;
        
        unsigned CCC_H              :8;
        
        unsigned TRAN_SPEED         :8;
        
        unsigned NSAC               :8;
        
        unsigned TAAC               :8;
        
        unsigned RESERVED_3         :2;
        unsigned SPEC_VERS          :4;
        unsigned CSD_STRUCTURE      :2;
    };
} CSD;


typedef union
{
    struct
    {
        DWORD _u320;
        DWORD _u321;
        DWORD _u322;
        DWORD _u323;
    };
    struct
    {
        BYTE _byte[16];
    };
    struct
    {
        unsigned 	NOT_USED           	:1;
        unsigned 	CRC                	:7;     
        unsigned 	MDT                	:8;     //Manufacturing Date Code (BCD)
        DWORD 		PSN;    					// Serial Number (PSN)
        unsigned 	PRV                	:8;     // Product Revision
		char		PNM[6];    					// Product Name
        WORD 		OID;    					// OEM/Application ID
        unsigned 	MID                	:8;     // Manufacture ID                        
    };
} CID;

#define FALSE	0
#define TRUE	!FALSE

#define INPUT	1
#define OUTPUT	0



#define CLKSPERINSTRUCTION (BYTE) 4

#define TMR1PRESCALER	(BYTE)    8
#define TMR1OVERHEAD	(BYTE)    5	
#define MILLISECDELAY   (WORD)((GetInstructionClock()/TMR1PRESCALER/(WORD)1000) - TMR1OVERHEAD)
#define B115K26MHZ      0x0C        // = 115.2K baud @26MHz

#define SD_CMD_IDLE				0
#define SD_CMD_SEND_OP_COND		1
#define SD_CMD_SET_BLOCK_LEN	16
#define SD_CMD_READ_BLOCK		17
#define SD_CMD_WRITE_BLOCK		24

#define SD_CARD_SYSTEM

typedef enum
{
	GO_IDLE_STATE,
	SEND_OP_COND,
	SET_BLOCKLEN,
	READ_SINGLE_BLOCK,
	WRITE_SINGLE_BLOCK,
	CRC_ON_OFF
}sdmmc_cmd;



/***************************************************************************/
/*                               Macros                                    */
/***************************************************************************/
#define MMC_OFF		// Power "Off" MMC Slot if available
#define MMC_ON		// Power "On" MMC Slot if available
#define GetMMC_CD()	((*MMCReg & MMC_DETECT) == MMC_DETECT ? FALSE : TRUE)


#define mSendMediaCmd_NoData()  SendMediaCmd();SD_CS=1;
#define mReadCRC()              WriteSPIM(0xFF);WriteSPIM(0xFF);
#define mSendCRC()              WriteSPIM(0xFF);WriteSPIM(0xFF);
#define mSend8ClkCycles()       WriteSPIM(0xFF);

#define low(num) (num & 0xFF)
#define high(num) ((num >> 8) & 0xFF)
#define upper(num) ((num >> 16) & 0xFF)

#endif //SD_SPI_INTERNAL



/*****************************************************************************/
/*                                 Prototypes                                */
/*****************************************************************************/

DWORD MDD_SDSPI_ReadCapacity(void);
WORD MDD_SDSPI_ReadSectorSize(void);
void MDD_SDSPI_InitIO(void);

BYTE MDD_SDSPI_MediaDetect(void);
BYTE MDD_SDSPI_MediaInitialize(void);
BYTE MDD_SDSPI_SectorRead(DWORD sector_addr, BYTE* buffer);
BYTE MDD_SDSPI_SectorWrite(DWORD sector_addr, BYTE* buffer, BYTE allowWriteToZero);

BYTE MDD_SDSPI_WriteProtectState(void);
void MDD_SDSPI_ShutdownMedia(void);

/*
 * Some people like the short names.
 */
#define	ReadCapacity		MDD_SDSPI_ReadCapacity
#define	ReadSectorSize		MDD_SDSPI_ReadSectorSize
#define	InitIO			MDD_SDSPI_InitIO
#define	MediaDetect		MDD_SDSPI_MediaDetect
#define	MediaInitialize		MDD_SDSPI_MediaInitialize
#define	SectorRead		MDD_SDSPI_SectorRead
#define	SectorWrite		MDD_SDSPI_SectorWrite
#define	WriteProtectState	MDD_SDSPI_WriteProtectState
#define	ShutdownMedia		MDD_SDSPI_ShutdownMedia

#endif
