/*
             LUFA Library
     Copyright (C) Dean Camera, 2015.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2015  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for Keyboard.c.
 */

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

	/* Includes: */
		#include <avr/io.h>
		#include <avr/wdt.h>
		#include <avr/power.h>
		#include <avr/interrupt.h>
		#include <stdbool.h>
		#include <string.h>

#include <util/delay.h>

		#include "Descriptors.h"

		#include <LUFA/Drivers/Board/LEDs.h>
		#include <LUFA/Drivers/USB/USB.h>
		#include <LUFA/Platform/Platform.h>

	/* Function Prototypes: */
		void SetupHardware(void);

		void EVENT_USB_Device_Connect(void);
		void EVENT_USB_Device_Disconnect(void);
		void EVENT_USB_Device_ConfigurationChanged(void);
		void EVENT_USB_Device_ControlRequest(void);
		void EVENT_USB_Device_StartOfFrame(void);

		bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
		                                         uint8_t* const ReportID,
		                                         const uint8_t ReportType,
		                                         void* ReportData,
		                                         uint16_t* const ReportSize);
		void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
		                                          const uint8_t ReportID,
		                                          const uint8_t ReportType,
		                                          const void* ReportData,
		                                          const uint16_t ReportSize);

/** palm portable keyboard interfacing*/
// pin mapping to keyboard   // a-star micro pins
#define DCD_PIN (_BV(PIND4)) // 4
#define RTS_PIN (_BV(PIND1)) // 2
#define PULLDOWN_PIN (_BV(PINB4)) // 8
#define VCC_PIN (_BV(PINC6)) // 5
#define GND_PIN (_BV(PIND7)) // 6

void BootKeyboard(void);
static volatile unsigned long seconds;
void ProcessKeyboardSerialByte(void);
static volatile unsigned char lastByte;
static volatile unsigned char UsedKeyCodes;
static volatile USB_KeyboardReport_Data_t KeyboardReport = {0};

static volatile uint8_t pressedRawKeyCode[6] = {255};
void pressKey(uint8_t raw, uint8_t key);
void releaseKey(uint8_t raw);
static volatile uint8_t FN_pressed = 0;


// software uart
typedef enum
{
    IDLE,                                       //!< Idle state, both transmit and receive possible.
    RECEIVE,                                    //!< Receiving byte.
    DATA_PENDING                                //!< Byte received and ready to read.

}AsynchronousStates_t;

static volatile AsynchronousStates_t state;     //!< Holds the state of the UART.
static volatile unsigned char SwUartRXData;     //!< Storage for received bits.
static volatile unsigned char SwUartRXBitCount; //!< RX bit counter.

static void initSoftwareUart( void );

#endif

