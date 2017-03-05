/******************************************************************************
 *
 *                Microchip Memory Disk Drive File System
 *
 ******************************************************************************
 * FileName:        HardwareProfile.h
 * Dependencies:    None
 * Processor:       PIC18
 * Compiler:        C18
 * Company:         Microchip Technology, Inc.
 * Version:         1.1.2
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
// ** Mostly I stripped stuff we don't need out in order
// ** to make the rest readable.
// **

#ifndef _HARDWAREPROFILE_H_
#define _HARDWAREPROFILE_H_


// Determines processor type automatically
#ifdef __18CXX
	#define USE_PIC18
#else
	#error "Use PIC18 processor"
#endif


// Clock speed definitions.
#define GetSystemClock()       48000000               // System clock (Hz)
#define GetPeripheralClock()   GetSystemClock()       // Peripheral clock freq.
#define GetInstructionClock()  (GetSystemClock() / 4) // Instruction clock freq.


#define USE_SD_INTERFACE_WITH_SPI       // SD-SPI.c and .h


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

#endif
