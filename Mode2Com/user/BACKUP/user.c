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

char state;
#define	T_F	100
char d1, d2, d3;

void echoHandlerInit(void);
void echoHandler(void);

rom char welcome[]={"Hybrid Sky USB Debug Board Demo\r\n\r\n"};

/** P R I V A T E  P R O T O T Y P E S ***************************************/

/** D E C L A R A T I O N S **************************************************/
#pragma code
void UserInit(void)
{

	state = 0;
	d1 = 0;
	d2 = 0;
	d3 = 0;
	mInitAllLEDs();
	mLED_1_On();
	echoHandlerInit();
}//end UserInit

/*
 * This routine makes the comm port echo its input.
 * It takes input when available and puts it into a ring buffer.
 * When we can transmit, we transmit from the ring buffer.
 */
#define RING_SIZE 16
#define	IN_BUFFER_SIZE 8
char ring[RING_SIZE];
char in_buffer[IN_BUFFER_SIZE];
char ri;
char ro;
char n_in_ring;

void
echoHandlerInit()
{
	ri = 0;
	ro = 0;
	n_in_ring = 0;
}

void
echoPutc(char c)
{
	ring[ri] = c;
	ri++;
	if (ri >= RING_SIZE)
		ri = 0;
	n_in_ring++;
}

void
echoHandler()
{
	char r, i;
	byte *p;

	// do we have room to accept more input?
	if (IN_BUFFER_SIZE < RING_SIZE - n_in_ring) {
		r = getsUSBUSART(in_buffer, IN_BUFFER_SIZE);
		for (i = 0; i < r; i++)
			echoPutc(in_buffer[i]);
	}

	// do we have something to send, and is the
	// transmitter ready for us to send it?
	if (n_in_ring > 0 && mUSBUSARTIsTxTrfReady()) {
		p = (byte *)ring + ro;
		mUSBUSARTTxRam(p, 1);
		ro++;
		if (ro >= RING_SIZE)
			ro = 0;
		n_in_ring = 0;
	}
}


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
     * This pile of cumbersome code blinks the LEDs at various rates
     * so we can get an idea of how often we come here.
     */
    d1++;
    if (d1 >= T_F) {
    	d1 = 0;
	mLED_2_Toggle();
	d2++;
	if (d2 >= T_F) {
		d2 = 0;
		mLED_3_Toggle();
		d3++;
		if (d3 >= T_F) {
			d3 = 0;
			mLED_4_Toggle();
			state = 0;	// resend the message
		}
	}
    }

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

    echoHandler();

}//end ProcessIO


/** EOF user.c ***************************************************************/
