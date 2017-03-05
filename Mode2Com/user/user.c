/*********************************************************************
 *
 *                Microchip USB C18 Firmware Version 1.2
 *
 *********************************************************************
 * FileName:        user.c
 * Dependencies:    See INCLUDES section below
 * Processor:       PIC18
 * Compiler:        C18 3.11+
 * Company:         Microchip Technology, Inc.
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
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Rawin Rojvanit       11/19/04    Original.
 * Rawin Rojvanit       05/14/07    A few updates.
 ********************************************************************/

//
// This code modified by Stephen Daniel on 7/23/08.
// Modified for the Hybridsky.com debug board.
//


/** I N C L U D E S **********************************************************/
#include <p18cxxx.h>
#include "system\typedefs.h"
#include "system\usb\usb.h"
#include "io_cfg.h"             // I/O pin mapping

/** V A R I A B L E S ********************************************************/
#pragma udata

/*
 * State is initialized to zero, set to one once the banner has printed.
 */
char state;

void echoHandlerInit(void);
void echoHandler(void);

rom char welcome[]={"Hybrid Sky USB Debug Board Demo\r\n\r\n"};

/** P R I V A T E  P R O T O T Y P E S ***************************************/

/** D E C L A R A T I O N S **************************************************/
void inputInit(void);	// cli.c
void inputPoll(void);	// cli.c

#pragma code
void UserInit(void)
{

	state = 0;
	mInitAllLEDs();
	mLED_1_On();
	inputInit();
}//end UserInit


/******************************************************************************
 * Function:        void ProcessIO(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is a place holder for other user routines.
 *                  It is a mixture of both USB and non-USB tasks.
 *
 * Note:            None
 *****************************************************************************/
void ProcessIO(void)
{
    // User Application USB tasks
    if((usb_device_state < CONFIGURED_STATE)||(UCONbits.SUSPND==1)) return;

    /*
     * Display the state of readiness to transmit.
     */
    if(mUSBUSARTIsTxTrfReady()) {
    	mLED_1_On();
    } else {
    	mLED_1_Off();
    }


    // This code writes the hello string once and does nothing else.
    if(state == 0 && mUSBUSARTIsTxTrfReady()) {
	putrsUSBUSART(welcome);
	state++;
    }

    inputPoll();

}//end ProcessIO


/** EOF user.c ***************************************************************/
