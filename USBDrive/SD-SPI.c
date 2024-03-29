/******************************************************************************
 *
 *               Microchip Memory Disk Drive File System
 *
 ******************************************************************************
 * FileName:        SD-SPI.c
 * Dependencies:    SD-SPI.h
 *                  string.h
 *                  FSIO.h
 *                  FSDefs.h
 * Processor:       PIC18/PIC24/dsPIC30/dsPIC33/PIC32
 * Compiler:        C18/C30/C32
 * Company:         Microchip Technology, Inc.
 * Version:         1.2.0
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the �Company�) for its PICmicro� Microcontroller is intended and
 * supplied to you, the Company�s customer, for use solely and
 * exclusively on Microchip PICmicro Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN �AS IS� CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
*****************************************************************************/

// **
// ** This file modified by Stephen Daniel, October, 2008
// ** Modified for use in Hybridsky.com data capture project.
// **

#define SD_SPI_INTERNAL	// includes all of SD-SPI.h, not just some
#include "SD-SPI.h"
#include "string.h"
#include "HardwareProfile.h"
#include "blink.h"

/******************************************************************************
 * Global Variables
 *****************************************************************************/

// Description:  Used for the mass-storage library to determine capacity
DWORD MDD_SDSPI_finalLBA;


    // Summary: Table of SD card commands and parameters
    // Description: The sdmmc_cmdtable contains an array of SD card commands, the corresponding CRC code, the
    //              response type that the card will return, and a parameter indicating whether to expect
    //              additional data from the card.
    const rom typMMC_CMD sdmmc_cmdtable[] =
{
    // cmd                      crc     response
    {cmdGO_IDLE_STATE,          0x95,   R1,     NODATA},
    {cmdSEND_OP_COND,           0xF9,   R1,     NODATA},
    {cmdSET_BLOCKLEN,           0xFF,   R1,     NODATA},
    {cmdREAD_SINGLE_BLOCK,      0xFF,   R1,     MOREDATA},
    {cmdWRITE_SINGLE_BLOCK,     0xFF,   R1,     MOREDATA},
    {cmdCRC_ON_OFF,             0x25,   R1,     NODATA}
};




/******************************************************************************
 * Prototypes
 *****************************************************************************/

extern void Delayms(BYTE milliseconds);
BYTE MDD_SDSPI_ReadMedia(void);
BYTE MDD_SDSPI_MediaInitialize(void);
MMC_RESPONSE SendMMCCmd(BYTE cmd, DWORD address);

    void OpenSPIM ( unsigned char sync_mode);
    void CloseSPIM( void );
    unsigned char WriteSPIM( unsigned char data_out );

    unsigned char WriteSPIManual(unsigned char data_out);
    BYTE ReadMediaManual (void);
    MMC_RESPONSE SendMMCCmdManual(BYTE cmd, DWORD address);


/*********************************************************
  Function:
    BYTE MDD_SDSPI_MediaDetect
  Summary:
    Determines whether an SD card is present
  Conditions:
    The MDD_MediaDetect function pointer must be configured
    to point to this function in FSconfig.h
  Input:
    None
  Return Values:
    TRUE -  Card detected
    FALSE - No card detected
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_MediaDetect function will determine if an
    SD card is connected to the microcontroller by polling
    the SD card detect pin.
  Remarks:
    None                                                  
  *********************************************************/

BYTE MDD_SDSPI_MediaDetect (void)
{
#ifdef SD_CD
    return(!SD_CD);
#else
	return 1;
#endif
}//end MediaDetect


/*********************************************************
  Function:
    WORD MDD_SDSPI_ReadSectorSize (void)
  Summary:
    Determines the current sector size on the SD card
  Conditions:
    MDD_MediaInitialize() is complete
  Input:
    None
  Return:
    The size of the sectors for the physical media
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadSectorSize function is used by the
    USB mass storage class to return the card's sector
    size to the PC on request.
  Remarks:
    None
  *********************************************************/

WORD MDD_SDSPI_ReadSectorSize(void)
{
    return MEDIA_SECTOR_SIZE;
}


/*********************************************************
  Function:
    DWORD MDD_SDSPI_ReadCapacity (void)
  Summary:
    Determines the current capacity of the SD card
  Conditions:
    MDD_MediaInitialize() is complete
  Input:
    None
  Return:
    The capacity of the device
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadCapacity function is used by the
    USB mass storage class to return the total number
    of sectors on the card.
  Remarks:
    None
  *********************************************************/
DWORD MDD_SDSPI_ReadCapacity(void)
{
    return (MDD_SDSPI_finalLBA);
}


/*********************************************************
  Function:
    WORD MDD_SDSPI_InitIO (void)
  Summary:
    Initializes the I/O lines connected to the card
  Conditions:
    MDD_MediaInitialize() is complete.  The MDD_InitIO
    function pointer is pointing to this function.
  Input:
    None
  Return:
    None
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_InitIO function initializes the I/O
    pins connected to the SD card.
  Remarks:
    None
  *********************************************************/

void MDD_SDSPI_InitIO (void)
{
    // Turn off the card
#ifdef SD_CD
    SD_CD_TRIS = INPUT;            //Card Detect - input
#endif
    SD_CS = 1;                     //Initialize Chip Select line
    SD_CS_TRIS = OUTPUT;            //Card Select - output
#ifdef SD_WE
    SD_WE_TRIS = INPUT;            //Write Protect - input
#endif
}



/*********************************************************
  Function:
    WORD MDD_SDSPI_ShutdownMedia (void)
  Summary:
    Disables the SD card
  Conditions:
    The MDD_ShutdownMedia function pointer is pointing 
    towards this function.
  Input:
    None
  Return:
    None
  Side Effects:
    None.
  Description:
    This function will disable the SPI port and deselect
    the SD card.
  Remarks:
    None
  *********************************************************/

void MDD_SDSPI_ShutdownMedia(void)
{
    // close the spi bus
    CloseSPIM();
    
    // deselect the device
    SD_CS = 1;
}


/*****************************************************************************
  Function:
    MMC_RESPONSE SendMMCCmd (BYTE cmd, DWORD address)
  Summary:
    Sends a command packet to the SD card.
  Conditions:
    None.
  Input:
    None.
  Return Values:
    MMC_RESPONSE    - The response from the card
                    - Bit 0 - Idle state
                    - Bit 1 - Erase Reset
                    - Bit 2 - Illegal Command
                    - Bit 3 - Command CRC Error
                    - Bit 4 - Erase Sequence Error
                    - Bit 5 - Address Error
                    - Bit 6 - Parameter Error
                    - Bit 7 - Unused. Always 0.
  Side Effects:
    None.
  Description:
    SendMMCCmd prepares a command packet and sends it out over the SPI interface.
    Response data of type 'R1' (as indicated by the SD/MMC product manual is returned.
  Remarks:
    None.
  ***************************************************************************************/

MMC_RESPONSE SendMMCCmd(BYTE cmd, DWORD address)
{
    WORD timeout = 0x8;
    BYTE index;
    MMC_RESPONSE    response;
    CMD_PACKET  CmdPacket;
    
    SD_CS = 0;                           //Card Select
    
    // Copy over data
    CmdPacket.cmd        = sdmmc_cmdtable[cmd].CmdCode;
    CmdPacket.address    = address;
    CmdPacket.crc        = sdmmc_cmdtable[cmd].CRC;       // Calc CRC here
    
    CmdPacket.TRANSMIT_BIT = 1;             //Set Tranmission bit
    
    WriteSPIM(CmdPacket.cmd);                //Send Command
    WriteSPIM(CmdPacket.addr3);              //Most Significant Byte
    WriteSPIM(CmdPacket.addr2);
    WriteSPIM(CmdPacket.addr1);
    WriteSPIM(CmdPacket.addr0);              //Least Significant Byte
    WriteSPIM(CmdPacket.crc);                //Send CRC
    
    // see if we are going to get a response
    if(sdmmc_cmdtable[cmd].responsetype == R1 || sdmmc_cmdtable[cmd].responsetype == R1b)
    {
        do
        {
            response.r1._byte = MDD_SDSPI_ReadMedia();
            timeout--;
        }while(response.r1._byte == MMC_FLOATING_BUS && timeout != 0);
    }
    else if(sdmmc_cmdtable[cmd].responsetype == R2)
    {
        MDD_SDSPI_ReadMedia();
        
        response.r2._byte1 = MDD_SDSPI_ReadMedia();
        response.r2._byte0 = MDD_SDSPI_ReadMedia();
    }

    if(sdmmc_cmdtable[cmd].responsetype == R1b)
    {
        response.r1._byte = 0x00;
        
        for(index =0; index < 0xFF && response.r1._byte == 0x00; index++)
        {
            timeout = 0xFFFF;
            
            do
            {
                response.r1._byte = MDD_SDSPI_ReadMedia();
                timeout--;
            }while(response.r1._byte == 0x00 && timeout != 0);
        }
    }

    mSend8ClkCycles();                      //Required clocking (see spec)

    // see if we are expecting data or not
    if(!(sdmmc_cmdtable[cmd].moredataexpected))
        SD_CS = 1;

    return(response);
}

/*****************************************************************************
  Function:
    MMC_RESPONSE SendMMCCmdManual (BYTE cmd, DWORD address)
  Summary:
    Sends a command packet to the SD card with bit-bang SPI.
  Conditions:
    None.
  Input:
    None.
  Return Values:
    MMC_RESPONSE    - The response from the card
                    - Bit 0 - Idle state
                    - Bit 1 - Erase Reset
                    - Bit 2 - Illegal Command
                    - Bit 3 - Command CRC Error
                    - Bit 4 - Erase Sequence Error
                    - Bit 5 - Address Error
                    - Bit 6 - Parameter Error
                    - Bit 7 - Unused. Always 0.
  Side Effects:
    None.
  Description:
    SendMMCCmd prepares a command packet and sends it out over the SPI interface.
    Response data of type 'R1' (as indicated by the SD/MMC product manual is returned.
    This function is intended to be used when the clock speed of a PIC18 device is
    so high that the maximum SPI divider can't reduce the clock below the maximum
    SD card initialization sequence speed.
  Remarks:
    None.
  ***************************************************************************************/

MMC_RESPONSE SendMMCCmdManual(BYTE cmd, DWORD address)
{
    WORD timeout = 0x8;
    BYTE index;
    MMC_RESPONSE    response;
    CMD_PACKET  CmdPacket;
    
    SD_CS = 0;                           //Card Select
    
    // Copy over data
    CmdPacket.cmd        = sdmmc_cmdtable[cmd].CmdCode;
    CmdPacket.address    = address;
    CmdPacket.crc        = sdmmc_cmdtable[cmd].CRC;       // Calc CRC here
    
    CmdPacket.TRANSMIT_BIT = 1;             //Set Tranmission bit
    
    WriteSPIManual(CmdPacket.cmd);                //Send Command
    WriteSPIManual(CmdPacket.addr3);              //Most Significant Byte
    WriteSPIManual(CmdPacket.addr2);
    WriteSPIManual(CmdPacket.addr1);
    WriteSPIManual(CmdPacket.addr0);              //Least Significant Byte
    WriteSPIManual(CmdPacket.crc);                //Send CRC
    
    // see if we are going to get a response
    if(sdmmc_cmdtable[cmd].responsetype == R1 || sdmmc_cmdtable[cmd].responsetype == R1b)
    {
        do
        {
            response.r1._byte = ReadMediaManual();
            timeout--;
        }while(response.r1._byte == MMC_FLOATING_BUS && timeout != 0);
    }
    else if(sdmmc_cmdtable[cmd].responsetype == R2)
    {
        ReadMediaManual();
        
        response.r2._byte1 = ReadMediaManual();
        response.r2._byte0 = ReadMediaManual();
    }

    if(sdmmc_cmdtable[cmd].responsetype == R1b)
    {
        response.r1._byte = 0x00;
        
        for(index =0; index < 0xFF && response.r1._byte == 0x00; index++)
        {
            timeout = 0xFFFF;
            
            do
            {
                response.r1._byte = ReadMediaManual();
                timeout--;
            }while(response.r1._byte == 0x00 && timeout != 0);
        }
    }

    WriteSPIManual(0xFF);                      //Required clocking (see spec)

    // see if we are expecting data or not
    if(!(sdmmc_cmdtable[cmd].moredataexpected))
        SD_CS = 1;

    return(response);
}


/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_SectorRead (DWORD sector_addr, BYTE * buffer)
  Summary:
    Reads a sector of data from an SD card.
  Conditions:
    The MDD_SectorRead function pointer must be pointing towards this function.
  Input:
    sector_addr - The address of the sector on the card.
    byffer -      The buffer where the retrieved data will be stored.  If
                  buffer is NULL, do not store the data anywhere.
  Return Values:
    TRUE -  The sector was read successfully
    FALSE - The sector could not be read
  Side Effects:
    None
  Description:
    The MDD_SDSPI_SectorRead function reads 512 bytes of data from the SD card
    starting at the sector address and stores them in the location pointed to
    by 'buffer.'
  Remarks:
    The card expects the address field in the command packet to be a byte address.
    The sector_addr value is converted to a byte address by shifting it left nine
    times (multiplying by 512).
  ***************************************************************************************/

BYTE MDD_SDSPI_SectorRead(DWORD sector_addr, BYTE* buffer)
{
    WORD index;
    WORD delay;
    MMC_RESPONSE    response;
    BYTE data_token;
    BYTE status = TRUE;
    DWORD   new_addr;
   
#ifdef USB_USE_MSD
    DWORD firstSector;
    DWORD numSectors;
#endif

    // send the cmd
    new_addr = sector_addr << 9;
    response = SendMMCCmd(READ_SINGLE_BLOCK,new_addr);

    // Make sure the command was accepted
    if(response.r1._byte != 0x00)
    {
        response = SendMMCCmd (READ_SINGLE_BLOCK,new_addr);
        if(response.r1._byte != 0x00)
        {
            return FALSE;
        }
    }

    index = 0x2FF;
    
    // Timing delay- at least 8 clock cycles
    delay = 0x40;
    while (delay)
        delay--;
  
    //Now, must wait for the start token of data block
    do
    {
        data_token = MDD_SDSPI_ReadMedia();
        index--;   
        
        delay = 0x40;
        while (delay)
            delay--;

    }while((data_token == MMC_FLOATING_BUS) && (index != 0));

    // Hopefully that zero is the datatoken
    if((index == 0) || (data_token != DATA_START_TOKEN))
    {
        status = FALSE;
    }
    else
    {
#ifdef USB_USE_MSD
        if ((sector_addr == 0) && (buffer == NULL))
            MDD_SDSPI_finalLBA = 0x00000000;
#endif

        for(index = 0; index < MEDIA_SECTOR_SIZE; index++)      //Reads in 512-byte of data
        {
            if(buffer != NULL)
            {
                data_token = SPIBUF;
                SPI_INTERRUPT_FLAG = 0;
                SPIBUF = 0xFF;
                while(!SPI_INTERRUPT_FLAG);
                buffer[index] = SPIBUF;
            }
            else
            {
#ifdef USB_USE_MSD
                if (sector_addr == 0)
                {
                    if ((index == 0x1C6) || (index == 0x1D6) || (index == 0x1E6) || (index == 0x1F6))
                    {
                        firstSector = MDD_SDSPI_ReadMedia();
                        firstSector |= (DWORD)MDD_SDSPI_ReadMedia() << 8;
                        firstSector |= (DWORD)MDD_SDSPI_ReadMedia() << 16;
                        firstSector |= (DWORD)MDD_SDSPI_ReadMedia() << 24;
                        numSectors = MDD_SDSPI_ReadMedia();
                        numSectors |= (DWORD)MDD_SDSPI_ReadMedia() << 8;
                        numSectors |= (DWORD)MDD_SDSPI_ReadMedia() << 16;
                        numSectors |= (DWORD)MDD_SDSPI_ReadMedia() << 24;
                        index += 8;
                            if ((firstSector + numSectors) > MDD_SDSPI_finalLBA)
                        {
                            MDD_SDSPI_finalLBA = firstSector + numSectors - 1;
                        }
                    }
                    else
                    {
                        MDD_SDSPI_ReadMedia();
                    }
                }
                else
                    MDD_SDSPI_ReadMedia();
#else
                MDD_SDSPI_ReadMedia();
#endif
            }
        }
        // Now ensure CRC
        mReadCRC();               //Read 2 bytes of CRC
        //status = mmcCardCRCError;
    }

    mSend8ClkCycles();            //Required clocking (see spec)

    SD_CS = 1;

    return(status);
}//end SectorRead


/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_SectorWrite (DWORD sector_addr, BYTE * buffer, BYTE allowWriteToZero)
  Summary:
    Writes a sector of data to an SD card.
  Conditions:
    The MDD_SectorWrite function pointer must be pointing to this function.
  Input:
    sector_addr -      The address of the sector on the card.
    buffer -           The buffer with the data to write.
    allowWriteToZero -
                     - TRUE -  Writes to the 0 sector (MBR) are allowed
                     - FALSE - Any write to the 0 sector will fail.
  Return Values:
    TRUE -  The sector was written successfully.
    FALSE - The sector could not be written.
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_SectorWrite function writes 512 bytes of data from the location
    pointed to by 'buffer' to the specified sector of the SD card.
  Remarks:
    The card expects the address field in the command packet to be a byte address.
    The sector_addr value is ocnverted to a byte address by shifting it left nine
    times (multiplying by 512).
  ***************************************************************************************/

BYTE MDD_SDSPI_SectorWrite(DWORD sector_addr, BYTE* buffer, BYTE allowWriteToZero)
{
    WORD            index;
    BYTE            data_response;
    BYTE            clear;
    MMC_RESPONSE    response; 
    BYTE            status = TRUE;

    if (sector_addr == 0 && allowWriteToZero == FALSE)
        status = FALSE;
    else
    {
        // send the cmd
        response = SendMMCCmd(WRITE_SINGLE_BLOCK,(sector_addr << 9));
        
        // see if it was accepted
        if(response.r1._byte != 0x00)
            status = FALSE;
        else
        {
            WriteSPIM(DATA_START_TOKEN);                 //Send data start token

            for(index = 0; index < MEDIA_SECTOR_SIZE; index++)      //Send 512 bytes of data
            {
                clear = SPIBUF;
                SPI_INTERRUPT_FLAG = 0;
                SPIBUF = buffer[index];         // write byte to SSP1BUF register
                while( !SPI_INTERRUPT_FLAG );   // wait until bus cycle complete
                data_response = SPIBUF;         // Clear the SPIBUF
            }

            // calc crc
            mSendCRC();                                 //Send 2 bytes of CRC
            
            data_response = MDD_SDSPI_ReadMedia();                //Read response
            
            if((data_response & 0x0F) != DATA_ACCEPTED)
            {
                status = FALSE;
            }
            else
            {
                index = 0;            //using i as a timeout counter
                
                do                  //Wait for write completion
                {
                    clear = SPIBUF;
                    SPI_INTERRUPT_FLAG = 0;
                    SPIBUF = 0xFF;
                    while(!SPI_INTERRUPT_FLAG);
                    data_response = SPIBUF;
                    index++;
                }while((data_response == 0x00) && (index != 0));

                if(index == 0)                                  //if timeout first
                    status = FALSE;
            }

            mSend8ClkCycles();
        }

        SD_CS = 1;

    } // Not writing to 0 sector

    return(status);
} //end SectorWrite


/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_WriteProtectState
  Summary:
    Indicates whether the card is write-protected.
  Conditions:
    The MDD_WriteProtectState function pointer must be pointing to this function.
  Input:
    None.
  Return Values:
    TRUE -  The card is write-protected
    FALSE - The card is not write-protected
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_WriteProtectState function will determine if the SD card is
    write protected by checking the electrical signal that corresponds to the
    physical write-protect switch.
  Remarks:
    None
  ***************************************************************************************/

BYTE MDD_SDSPI_WriteProtectState(void)
{
#ifdef SD_WE
    return(SD_WE);
#else
    return 0;
#endif
}


/*****************************************************************************
  Function:
    void Delayms (BYTE milliseconds)
  Summary:
    Delay.
  Conditions:
    None.
  Input:
    BYTE milliseconds - Number of ms to delay
  Return:
    None.
  Side Effects:
    None.
  Description:
    The Delayms function will delay a specified number of milliseconds.  Used for SPI
    timing.
  Remarks:
    Depending on compiler revisions, this function may delay for the exact time
    specified.  This shouldn't create a significant problem.
  ***************************************************************************************/

void Delayms(BYTE milliseconds)
{
    BYTE    ms;
    DWORD   count;
    
    ms = milliseconds;
    while (ms--)
    {
        count = MILLISECDELAY;
        while (count--);
    }
    Nop();
    return;
}


/*****************************************************************************
  Function:
    void CloseSPIM (void)
  Summary:
    Disables the SPI module.
  Conditions:
    None.
  Input:
    None.
  Return:
    None.
  Side Effects:
    None.
  Description:
    Disables the SPI module.
  Remarks:
    None.
  ***************************************************************************************/

void CloseSPIM (void)
{
    SPICON1 &= 0xDF;
}



/*****************************************************************************
  Function:
    unsigned char WriteSPIM (unsigned char data_out)
  Summary:
    Writes data to the SD card.
  Conditions:
    None.
  Input:
    data_out - The data to write.
  Return:
    0.
  Side Effects:
    None.
  Description:
    The WriteSPIM function will write a byte of data from the microcontroller to the
    SD card.
  Remarks:
    None.
  ***************************************************************************************/

unsigned char WriteSPIM( unsigned char data_out )
{
    BYTE clear;
    clear = SPIBUF;
    SPI_INTERRUPT_FLAG = 0;
    SPIBUF = data_out;
    if (SPICON1 & 0x80)
        return -1;
    else
        while (!SPI_INTERRUPT_FLAG);
    return 0;
}



/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_ReadMedia (void)
  Summary:
    Reads a byte of data from the SD card.
  Conditions:
    None.
  Input:
    None.
  Return:
    The byte read.
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadMedia function will read one byte from the SPI port.
  Remarks:
    This function replaces ReadSPI, since some implementations of that function
    will initialize SSPBUF/SPIBUF to 0x00 when reading.  The card expects 0xFF.
  ***************************************************************************************/
BYTE MDD_SDSPI_ReadMedia(void)
{

    BYTE clear;
    clear = SPIBUF;
    SPI_INTERRUPT_FLAG = 0;
    SPIBUF = 0xFF;
    while (!SPI_INTERRUPT_FLAG);
    return SPIBUF;
}

/*****************************************************************************
  Function:
    void OpenSPIM (unsigned int sync_mode)
  Summary:
    Initializes the SPI module
  Conditions:
    None.
  Input:
    sync_mode - Sets synchronization
  Return:
    None.
  Side Effects:
    None.
  Description:
    The OpenSPIM function will enable and configure the SPI module.
  Remarks:
    None.
  ***************************************************************************************/

void OpenSPIM (unsigned char sync_mode)
{
    SPISTAT = 0x0000;               // power on state 

    SPICON1 = 0x0000;                // power on state
    SPICON1 |= sync_mode;          // select serial mode 

    SPICON1 |= 0x80;
    SPISTATbits.CKE = 1;

    SPICLOCK = 0;
    SPIOUT = 0;                  // define SDO1 as output (master or slave)
    SPIIN = 1;                  // define SDI1 as input (master or slave)
    SPIENABLE = 1;             // enable synchronous serial port
}


// Description: Delay value for the manual SPI clock
#define MANUAL_SPI_CLOCK_VALUE             1

/*****************************************************************************
  Function:
    unsigned char WriteSPIManual (unsigned char data_out)
  Summary:
    Write a character to the SD card with bit-bang SPI.
  Conditions:
    None.
  Input:
    data_out - Data to send.
  Return:
    0.
  Side Effects:
    None.
  Description:
    Writes a character to the SD card.
  Remarks:
    The WriteSPIManual function is for use on a PIC18 when the clock speed is so
    high that the maximum SPI clock divider cannot reduce the SPI clock speed below
    the maximum SD card initialization speed.
  ***************************************************************************************/
unsigned char WriteSPIManual(unsigned char data_out)
{
    char i = data_out;
    unsigned char clock;

    ADCON1 = 0xFF;
    SPICLOCKLAT = 0;
    SPIOUTLAT = 1;
    SPICLOCK = OUTPUT;
    SPIOUT = OUTPUT;
    
    if ((SPIOUTPORT != SPIOUTLAT) || (SPICLOCKPORT != SPICLOCKLAT))
        return (-1);

    // Perform loop operation iteratively to reduce discrepancy
    // Bit 7
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x80)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    // Bit 6
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x40)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;

    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    // Bit 5
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x20)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    // Bit 4
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x10)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    // Bit 3
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x08)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    // Bit 2
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x04)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    // Bit 1
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x02)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    // Bit 0
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    if (i & 0x01)
        SPIOUTLAT = 1;
    else
        SPIOUTLAT = 0;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);

    SPICLOCKLAT = 0;

    return 0; 
}


/*****************************************************************************
  Function:
    BYTE ReadMediaManual (void)
  Summary:
    Reads a byte of data from the SD card.
  Conditions:
    None.
  Input:
    None.
  Return:
    The byte read.
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadMedia function will read one byte from the SPI port.
  Remarks:
    This function replaces ReadSPI, since some implementations of that function
    will initialize SSPBUF/SPIBUF to 0x00 when reading.  The card expects 0xFF.
    This function is for use on a PIC18 when the clock speed is so high that the
    maximum SPI clock prescaler cannot reduce the SPI clock below the maximum SD card
    initialization speed.
  ***************************************************************************************/
BYTE ReadMediaManual (void)
{
    char i, result = 0x00;
    unsigned char clock;

    SPICLOCKLAT = 0;
    SPIOUTLAT = 1;
    SPICLOCK = OUTPUT;
    SPIOUT = OUTPUT;
    SPIIN = INPUT;
    
    if ((SPIOUTPORT != SPIOUTLAT) || (SPICLOCKPORT != SPICLOCKLAT))
        return (-1);

    // Perform loop operation iteratively to reduce discrepancy
    // Bit 7
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    if (SPIINPORT)
        result |= 0x80;

    // Bit 6
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    if (SPIINPORT)
        result |= 0x40;

    // Bit 5
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    if (SPIINPORT)
        result |= 0x20;

    // Bit 4
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);    
    if (SPIINPORT)
        result |= 0x10;

    // Bit 3
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    if (SPIINPORT)
        result |= 0x08;

    // Bit 2
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    if (SPIINPORT)
        result |= 0x04;

    // Bit 1
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    if (SPIINPORT)
        result |= 0x02;

    // Bit 0
    SPICLOCKLAT = 0;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    SPICLOCKLAT = 1;
    clock = MANUAL_SPI_CLOCK_VALUE;
    while (clock--);
    if (SPIINPORT)
        result |= 0x01;

    SPICLOCKLAT = 0;

    return result;
}//end ReadMedia


/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_MediaInitialize (void)
  Summary:
    Initializes the SD card.
  Conditions:
    The MDD_MediaInitialize function pointer must be pointing to this function.
  Input:
    None.
  Return Values:
    TRUE -  The card was successfully initialized
    FALSE - Communication could not be established.    
  Side Effects:
    None.
  Description:
    This function will send initialization commands to and SD card.
  Remarks:
    None.
  ***************************************************************************************/

BYTE MDD_SDSPI_MediaInitialize(void)
{
    WORD timeout;
    BYTE       status = TRUE;
    MMC_RESPONSE    response; 

    SD_CS = 1;               //Initialize Chip Select line
    
    //Media powers up in the open-drain mode and cannot handle a clock faster
    //than 400kHz. Initialize SPI port to slower than 400kHz
    
    // let the card power on and initialize
    Delayms(1);
    
        // Make sure the SPI module doesn't control the bus
        SPICON1 = 0x00;

        //Media requires 80 clock cycles to startup [8 clocks/BYTE * 10 us]
        for(timeout=0; timeout<10; timeout++)
            WriteSPIManual(0xFF);
    
        SD_CS = 0;
        
        Delayms(1);
    
        // Send CMD0 to reset the media
        response = SendMMCCmdManual (GO_IDLE_STATE, 0x0);

        if ((response.r1._byte == MMC_BAD_RESPONSE) || ((response.r1._byte & 0xF7) != 0x01))
        {
            status = FALSE;     // we have not got anything back from the card
            SD_CS = 1;                              // deselect the devices

            return status;
        }

        // According to the spec cmd1 must be repeated until the card is fully initialized
        timeout = 0xFFF;
    
        do
        {
            response = SendMMCCmdManual (SEND_OP_COND, 0x0);
            timeout--;
        }while(response.r1._byte != 0x00 && timeout != 0);

    // see if it failed
    if (timeout == 0)
    {
        status = FALSE;     // we have not got anything back from the card

        SD_CS = 1;                              // deselect the devices

    }
    else
    {

        Delayms (2);

            OpenSPIM(SYNC_MODE_FAST);

        // Turn off CRC7 if we can, might be an invalid cmd on some cards (CMD59)
        response = SendMMCCmd(CRC_ON_OFF,0x0);

        // Now set the block length to media sector size. It should be already
        response = SendMMCCmd(SET_BLOCKLEN,MEDIA_SECTOR_SIZE);
        
        for(timeout = 0xFF; timeout > 0 && MDD_SDSPI_SectorRead(0x0,NULL) != TRUE; timeout--)
        {;}

        // see if we had an issue
        if(timeout == 0)
        {
            status = FALSE;
            SD_CS = 1;                               // deselect the devices

        }
    }
    return(status);
}//end MediaInitialize

