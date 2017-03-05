/********************************************************************
 FileName:     	HardwareProfile.h
 Dependencies:	See INCLUDES section
 Processor:		PIC18 or PIC24 USB Microcontrollers
 Hardware:		The code is natively intended to be used on the following
 				hardware platforms: PICDEM™ FS USB Demo Board, 
 				PIC18F87J50 FS USB Plug-In Module, or
 				Explorer 16 + PIC24 USB PIM.  The firmware may be
 				modified for use on other USB platforms by editing this
 				file (HardwareProfile.h).
 Complier:  	Microchip C18 (for PIC18) or C30 (for PIC24)
 Company:		Microchip Technology, Inc.

 Software License Agreement:

 The software supplied herewith by Microchip Technology Incorporated
 (the “Company”) for its PIC® Microcontroller is intended and
 supplied to you, the Company’s customer, for use solely and
 exclusively on Microchip PIC Microcontroller products. The
 software is owned by the Company and/or its supplier, and is
 protected under applicable copyright laws. All rights are reserved.
 Any use in violation of the foregoing restrictions may subject the
 user to criminal sanctions under applicable laws, as well as to
 civil liability for the breach of the terms and conditions of this
 license.

 THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.

********************************************************************
 File Description:

 Change History:
  Rev   Date         Description
  1.0   11/19/2004   Initial release
  2.1   02/26/2007   Updated for simplicity and to use common
                     coding style

********************************************************************/

/*
 * Modified by Stephen Daniel
 * Modified to work on the HybridSky DAQ 1.0 board.
 * 10/2008
 *
 * This version has been stripped of some non-essentials.
 */


#ifndef HARDWARE_PROFILE_H
#define HARDWARE_PROFILE_H

#define CLOCK_FREQ 48000000
#define	GetSystemClock()	CLOCK_FREQ
#define	GetInstructionClock()	GetSystemClock()/4

/** TRIS ***********************************************************/
#define INPUT_PIN           1
#define OUTPUT_PIN          0

/** USB ************************************************************/
	// DAQ 1.0 has neither of these features.
	//#define USE_SELF_POWER_SENSE_IO	
	//#define USE_USB_BUS_SENSE_IO

	#define PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER

    #define U1ADDR UADDR
    #define U1IE UIE
    #define U1IR UIR
    #define U1EIR UEIR
    #define U1EIE UEIE
    #define U1CON UCON
    #define U1EP0 UEP0
    #define U1CONbits UCONbits
    #define U1EP1 UEP1
    #define U1CNFG1 UCFG
    #define U1STAT USTAT
    #define U1EP0bits UEP0bits
    
    #define tris_usb_bus_sense  TRISAbits.TRISA1    // Input
    
    #if defined(USE_USB_BUS_SENSE_IO)
    #define USB_BUS_SENSE       PORTAbits.RA1
    #else
    #define USB_BUS_SENSE       1
    #endif
    
    #define tris_self_power     TRISAbits.TRISA2    // Input
    
    #if defined(USE_SELF_POWER_SENSE_IO)
    #define self_power          PORTAbits.RA2
    #else
    #define self_power          1
    #endif
    
//#define USE_SD_INTERFACE_WITH_SPI       // SD-SPI.c and .h
#define	USE_DATAFLASH


/*********************************************************************/
/******************* Pin and Register Definitions ********************/
/*********************************************************************/

/* LEDs on the HybridSky DAQ board */

#define	LED_GREEN	PORTAbits.RA3
#define	LED_GREEN_TRIS	TRISAbits.TRISA3

#define	LED_RED		PORTAbits.RA2
#define	LED_RED_TRIS	TRISAbits.TRISA2

/* SD Card definitions */

// Chip Select Signal
#define SD_CS               PORTCbits.RC6
#define SD_CS_TRIS          TRISCbits.TRISC6

// Card detect signal  (if none, don't define these symbols)
//#define SD_CD               PORTBbits.RB4
//#define SD_CD_TRIS          TRISBbits.TRISB4

// Write protect signal  (if none, don't define these symbols)
//#define SD_WE               PORTAbits.RA4
//#define SD_WE_TRIS          TRISAbits.TRISA4

// Registers for the SPI module
#define SPICON1             SSPCON1
#define SPISTAT             SSPSTAT
#define SPIBUF              SSPBUF
#define SPISTAT_RBF         SSPSTATbits.BF
#define SPICON1bits         SSPCON1bits
#define SPISTATbits         SSPSTATbits
#define	SPIENABLE           SSPCON1bits.SSPEN

#define SPI_INTERRUPT_FLAG  PIR1bits.SSPIF   

// Defines for 18F2455
#define SPICLOCK            TRISBbits.TRISB1
#define SPIIN               TRISBbits.TRISB0
#define SPIOUT              TRISCbits.TRISC7

// Latch pins for SCK/SDI/SDO lines
#define SPICLOCKLAT         LATBbits.LATB1
#define SPIINLAT            LATBbits.LATB0
#define SPIOUTLAT           LATCbits.LATC7

// Port pins for SCK/SDI/SDO lines
#define SPICLOCKPORT        PORTBbits.RB1
#define SPIINPORT           PORTBbits.RB0
#define SPIOUTPORT          PORTCbits.RC7


// Macros for input and output TRIS bits
#define INPUT_PIN   1
#define OUTPUT_PIN  0

#include <p18cxxx.h>

#endif  //HARDWARE_PROFILE_H
