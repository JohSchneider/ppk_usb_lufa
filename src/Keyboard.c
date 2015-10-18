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
 *  Main source file for the Keyboard demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "Keyboard.h"

/** Buffer to hold the previously generated Keyboard HID report, for comparison purposes inside the HID class driver. */
static uint8_t PrevKeyboardHIDReportBuffer[sizeof(USB_KeyboardReport_Data_t)];

/** LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Keyboard_HID_Interface =
	{
		.Config =
			{
				.InterfaceNumber              = INTERFACE_ID_Keyboard,
				.ReportINEndpoint             =
					{
						.Address              = KEYBOARD_EPADDR,
						.Size                 = KEYBOARD_EPSIZE,
						.Banks                = 1,
					},
				.PrevReportINBuffer           = PrevKeyboardHIDReportBuffer,
				.PrevReportINBufferSize       = sizeof(PrevKeyboardHIDReportBuffer),
			},
	};


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

//	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	LEDs_SetAllLEDs(0);

	GlobalInterruptEnable();

	BootKeyboard();

	for (;;)
	{
	  ProcessKeyboardSerialByte();
		HID_Device_USBTask(&Keyboard_HID_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware()
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);


	/* Hardware Initialization */

	initSoftwareUart();

	// setup timer1 to keep the keyboard awake
	TCCR1B = (1<<CS12) | (1<<CS10); // prescaler 1024
	TCCR1B |= (1 << WGM12); // CTC mode
	TIMSK1 = 1<<OCIE1A; // trigger interrupt on compare
	OCR1A = 15624; // once per second
	// we run at 16Mhz:
	// timer counts + 1 = (target time) / (timer resolution)
	//   timer resolution = 1/(16Mhz / 1024)  seconds
	// => OCR1A = 1s * 1/( 16Mhz / 1024 ) -1 = 15624

	// setup remaining pins
	//// DCD_PIN
	// a-star micro Pin 4 -> PD4
	DDRD &= ~DCD_PIN; // set input
	//// RTS_PIN
	// a-star micro Pin 2 -> PD1
	DDRD &= ~RTS_PIN; // set input
	//// PULLDOWN_PIN
	// a-star micro Pin 8 -> PB4
	DDRB |= PULLDOWN_PIN; // set output
	PORTB &= ~PULLDOWN_PIN; // set LOW
	//// VCC_PIN
	// a-star micro Pin 5 -> PC6
	DDRC |= VCC_PIN;
	PORTC &= ~VCC_PIN;
	//// GND_PIN
	// a-star micro Pin 6 -> PD7
	DDRD |= GND_PIN;
	PORTD &= ~GND_PIN;

	USB_Init();
}

void BootKeyboard(void)
{
  PORTC &= ~VCC_PIN; // power off
  _delay_ms(5);
  PORTC |= VCC_PIN; // power on
  _delay_ms(15);

  // wait until the keyboard has powerd on, it signals this by pulling DCD_PIN high..
  while(!(PIND & DCD_PIN)) {;}
  //.. and then expects the driver to pull RTS high
  if (!(PIND & RTS_PIN)) { // if we read low
    DDRD |= RTS_PIN; // set pin to output
    PORTD |= RTS_PIN; // and high
  }
  else {
    DDRD |= RTS_PIN; // set pin to output
    PORTD &= ~RTS_PIN; // low
    _delay_ms(10);
    PORTD |= RTS_PIN; // high
  }

  _delay_ms(5);

  // wait for the keyboard to send its id string (the first two bytes)
  while (SwUartRXData != 0xFA) {;}
  while (SwUartRXData != 0xFD) {;}
}


/* keyboard goes to non-responsive sleep after 10 minutes of no keypresses and RTS on high
 * so we toggle RTS every 7 minutes regardless of input just to keep it on its toes :-)
 */
ISR(TIMER1_COMPA_vect)
{
  seconds++;
  if (seconds == 60*7)
    {
      DDRD |= RTS_PIN; // set pin to output
      PORTD &= ~RTS_PIN; // low
      _delay_ms(10);
      PORTD |= RTS_PIN; // high
      seconds = 0;
    }
}


/** press a normal key (e.g. not a modifier key) */
void pressKey(uint8_t raw, uint8_t key)
{
  if (UsedKeyCodes < 6)
    {
      UsedKeyCodes++;
      pressedRawKeyCode[UsedKeyCodes] = raw;
      KeyboardReport.KeyCode[UsedKeyCodes] = key;
    }
}

  /** release a normal, non-modifier key */
void releaseKey(uint8_t raw)
{
  // check which of the pressed KeyCode has been released
  for (int i=0; i<6; i++)
    {
      if (pressedRawKeyCode[i] == raw)
	{
	  // move remaining one place up
	  for (int j=i; j<5; j++)
	    {
	      KeyboardReport.KeyCode[j] = KeyboardReport.KeyCode[j+1];
	      pressedRawKeyCode[j] = pressedRawKeyCode[j+1];
	    }
	  // and put an empty in the last place
	  KeyboardReport.KeyCode[5] = 0;
	  pressedRawKeyCode[5] = 255;
	  UsedKeyCodes--;
	}
    }
}

void ProcessKeyboardSerialByte(void)
{
  if( state == DATA_PENDING )
    {
	      unsigned char keyUpDown = SwUartRXData & 0b10000000;
	      unsigned char keyXY = SwUartRXData & 0b01111111;
	      state = IDLE;
	      seconds = 0; // reset keep awake watchdog

	      // for a table of available keycode defines see:
	      // LUFA/Drivers/USB/Class/Common/HIDClassCommon.h

	      if (keyUpDown) // true: key released
		{
		if (lastByte == SwUartRXData)
		  {
		    // releaseAll
		    // memset(&KeyboardReport, 0, sizeof(USB_KeyboardReport_Data_t));
		    KeyboardReport.KeyCode[0] = 0;
		    KeyboardReport.KeyCode[1] = 0;
		    KeyboardReport.KeyCode[2] = 0;
		    KeyboardReport.KeyCode[3] = 0;
		    KeyboardReport.KeyCode[4] = 0;
		    KeyboardReport.KeyCode[5] = 0;
		    KeyboardReport.Modifier = 0;
		    UsedKeyCodes = 0;
		  }
		else
		  {
		    switch (keyXY)
		      {
		      case 0b00100010: // FN
			FN_pressed = 0;
			break;
		      case 0b00001000: // "CMD"
			releaseKey(keyXY);
			// TODO: is it okay to set the modifier flag and press the key at the same time?
			KeyboardReport.Modifier &= ~HID_KEYBOARD_MODIFIER_LEFTGUI;
			break;
		      case 0b00011010: // CTRL (only left one exists)
			KeyboardReport.Modifier &= ~HID_KEYBOARD_MODIFIER_LEFTCTRL;
			break;
		      case 0b00100011: // ALT (only left one exists)
			KeyboardReport.Modifier &= ~HID_KEYBOARD_MODIFIER_LEFTALT;
			break;
		      case 0b01011000: // SHIFT (L)
			KeyboardReport.Modifier &= ~HID_KEYBOARD_MODIFIER_LEFTSHIFT;
			break;
		      case 0b01011001: // SHIFT (R)
			KeyboardReport.Modifier &= ~HID_KEYBOARD_MODIFIER_RIGHTSHIFT;
			break;
		      default:
			releaseKey(keyXY);
		      }
		  }
		}
	      else // key pressed
	      switch(keyXY) {
		// y0 row
	      case 0b00000000: // '1'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F1);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_1_AND_EXCLAMATION);
		break;
	      case 0b00000001: // '2'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F2);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_2_AND_AT);
		break;
	      case 0b00000010: // '3'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F3);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_3_AND_HASHMARK);
		break;
	      case 0b00000011: // 'z'
		pressKey(keyXY, HID_KEYBOARD_SC_Z);
		break;
	      case 0b00000100: // '4'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F4);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_4_AND_DOLLAR);
		break;
	      case 0b00000101: // '5'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F5);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_5_AND_PERCENTAGE);
		break;
	      case 0b00000110: // '6'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F6);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_6_AND_CARET);
		break;
	      case 0b00000111: // '7'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F7);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_7_AND_AMPERSAND);
		break;

		// y1 row
	      case 0b00001000: // "CMD"
		pressKey(keyXY, HID_KEYBOARD_SC_LEFT_GUI);
// TODO: is it okay to set the modifier flag and press the key at the same time?
		KeyboardReport.Modifier |= HID_KEYBOARD_MODIFIER_LEFTGUI;
		break;
	      case 0b00001001: // 'q'
		pressKey(keyXY, HID_KEYBOARD_SC_Q);
		break;
	      case 0b00001010: // 'w'
		pressKey(keyXY, HID_KEYBOARD_SC_W);
		break;
	      case 0b00001011: // 'e'
		pressKey(keyXY, HID_KEYBOARD_SC_E);
		break;
	      case 0b00001100: // 'r'
		pressKey(keyXY, HID_KEYBOARD_SC_R);
		break;
	      case 0b00001101: // 't'
		pressKey(keyXY, HID_KEYBOARD_SC_T);
		break;
	      case 0b00001110: // 'y'
		pressKey(keyXY, HID_KEYBOARD_SC_Y);
		break;
	      case 0b00001111: // two right of space-bar
		pressKey(keyXY, HID_KEYBOARD_SC_NON_US_BACKSLASH_AND_PIPE);  // physical row 5, 7th key
		break;

		// y2 row
	      case 0b00010000: // 'x'
		pressKey(keyXY, HID_KEYBOARD_SC_X);
		break;
	      case 0b00010001: // 'a'
		pressKey(keyXY, HID_KEYBOARD_SC_A);
		break;
	      case 0b00010010: // 's'
		pressKey(keyXY, HID_KEYBOARD_SC_S);
		break;
	      case 0b00010011: // 'd'
		pressKey(keyXY, HID_KEYBOARD_SC_D);
		break;
	      case 0b00010100: // 'f'
		pressKey(keyXY, HID_KEYBOARD_SC_F);
		break;
	      case 0b00010101: // 'g'
		pressKey(keyXY, HID_KEYBOARD_SC_G);
		break;
	      case 0b00010110: // 'h'
		pressKey(keyXY, HID_KEYBOARD_SC_H);
		break;
	      case 0b00010111: // ' ' // "Space 1"
		pressKey(keyXY, HID_KEYBOARD_SC_SPACE);
		break;

		// y3 row
	      case 0b00011000: // CAPS LK
		pressKey(keyXY, HID_KEYBOARD_SC_CAPS_LOCK);
		break;
	      case 0b00011001: // TAB
		pressKey(keyXY, HID_KEYBOARD_SC_TAB);
		break;
	      case 0b00011010: // CTRL (only left one exists)
		KeyboardReport.Modifier |= HID_KEYBOARD_MODIFIER_LEFTCTRL;
		break;

		// y4 row
	      case 0b00100010: // FN
		FN_pressed = 1;
		break;
	      case 0b00100011: // ALT (only left one exists)
		KeyboardReport.Modifier |= HID_KEYBOARD_MODIFIER_LEFTALT;
		break;

		// y5 row
	      case 0b00101100: // 'c'
		pressKey(keyXY, HID_KEYBOARD_SC_C);
		break;
	      case 0b00101101: // 'v'
		pressKey(keyXY, HID_KEYBOARD_SC_V);
		break;
	      case 0b00101110: // 'b'
		pressKey(keyXY, HID_KEYBOARD_SC_B);
		break;
	      case 0b00101111: // 'n'
		pressKey(keyXY, HID_KEYBOARD_SC_N);
		break;

		// y6 row
	      case 0b00110000: // '-'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F11);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_MINUS_AND_UNDERSCORE);
		break;
	      case 0b00110001: // '='
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F12);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_EQUAL_AND_PLUS);
		break;
	      case 0b00110010: // BACK SP
		pressKey(keyXY, HID_KEYBOARD_SC_BACKSPACE);
		break;
	      case 0b00110011: // "Special Function One"
		pressKey(keyXY, HID_KEYBOARD_SC_MEDIA_NEXT_TRACK);
		break;
	      case 0b00110100: // '8'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F8);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_8_AND_ASTERISK);
		break;
	      case 0b00110101: // '9'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F9);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_9_AND_OPENING_PARENTHESIS);
		break;
	      case 0b00110110: // '0'
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_F10);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_0_AND_CLOSING_PARENTHESIS);
		break;
	      case 0b00110111: // "Space 2" - Note: right of space-bar
		pressKey(keyXY, HID_KEYBOARD_SC_GRAVE_ACCENT_AND_TILDE); // physical row 5, 6th key
		break;

		// y7 row
	      case 0b00111000: // '['
		pressKey(keyXY, HID_KEYBOARD_SC_OPENING_BRACKET_AND_OPENING_BRACE);
		break;
	      case 0b00111001: // ']'
		pressKey(keyXY, HID_KEYBOARD_SC_CLOSING_BRACKET_AND_CLOSING_BRACE);
		break;
	      case 0b00111010: // '\'
		pressKey(keyXY, HID_KEYBOARD_SC_BACKSLASH_AND_PIPE);
		break;
	      case 0b00111011: // "Special Function Two"
		pressKey(keyXY, HID_KEYBOARD_SC_MEDIA_PREVIOUS_TRACK);
		break;
	      case 0b00111100: // 'u'
		pressKey(keyXY, HID_KEYBOARD_SC_U);
		break;
	      case 0b00111101: // 'i'
		pressKey(keyXY, HID_KEYBOARD_SC_I);
		break;
	      case 0b00111110: // 'o'
		pressKey(keyXY, HID_KEYBOARD_SC_O);
		break;
	      case 0b00111111: // 'p'
		pressKey(keyXY, HID_KEYBOARD_SC_P);
		break;

		// y8 row
	      case 0b01000000: // '''
		pressKey(keyXY, HID_KEYBOARD_SC_APOSTROPHE_AND_QUOTE);
		break;
	      case 0b01000001: // ENTER
		pressKey(keyXY, HID_KEYBOARD_SC_ENTER);
		break;
	      case 0b01000010: // "Special Function Three"
		pressKey(keyXY, HID_KEYBOARD_SC_MEDIA_VOLUME_UP);
		break;
	      case 0b01000100: // 'j'
		pressKey(keyXY, HID_KEYBOARD_SC_J);
		break;
	      case 0b01000101: // 'k'
		pressKey(keyXY, HID_KEYBOARD_SC_K);
		break;
	      case 0b01000110: // 'l'
		pressKey(keyXY, HID_KEYBOARD_SC_L);
		break;
	      case 0b01000111: // ';'
		pressKey(keyXY, HID_KEYBOARD_SC_SEMICOLON_AND_COLON);
		break;

		// y9 row
	      case 0b01001000: // '?'
		pressKey(keyXY, HID_KEYBOARD_SC_SLASH_AND_QUESTION_MARK);
		break;
	      case 0b01001001: // up arrow
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_PAGE_UP);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_UP_ARROW);
		break;
	      case 0b01001010: // "Special Function Four"
		pressKey(keyXY, HID_KEYBOARD_SC_MEDIA_VOLUME_DOWN);
		break;
	      case 0b01001100: // 'm'
		pressKey(keyXY, HID_KEYBOARD_SC_M);
		break;
	      case 0b01001101: // ','
		pressKey(keyXY, HID_KEYBOARD_SC_COMMA_AND_LESS_THAN_SIGN);
		break;
	      case 0b01001110: // '.'
		pressKey(keyXY, HID_KEYBOARD_SC_DOT_AND_GREATER_THAN_SIGN);
		break;
	      case 0b01001111: // "DONE"
		pressKey(keyXY, HID_KEYBOARD_SC_ESCAPE);
		break;

		// y10 row
	      case 0b01010000: // DEL
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_INSERT);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_DELETE);
		break;
	      case 0b01010001: // left arrow
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_HOME);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_LEFT_ARROW);
		break;
	      case 0b01010010: // down arrow
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_PAGE_DOWN);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_DOWN_ARROW);
		break;
	      case 0b01010011: // right arrow
		if (FN_pressed)
		  pressKey(keyXY, HID_KEYBOARD_SC_END);
		else
		  pressKey(keyXY, HID_KEYBOARD_SC_RIGHT_ARROW);
		break;

		// y11 row
	      case 0b01011000: // SHIFT (L)
		KeyboardReport.Modifier |= HID_KEYBOARD_MODIFIER_LEFTSHIFT;
		break;
	      case 0b01011001: // SHIFT (R)
		KeyboardReport.Modifier |= HID_KEYBOARD_MODIFIER_RIGHTSHIFT;
		break;
	      }

	      lastByte = SwUartRXData;
	  }
}


/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
//	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
//	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= HID_Device_ConfigureEndpoints(&Keyboard_HID_Interface);

	USB_Device_EnableSOFEvents();

//	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
	HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);
}

/** HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either HID_REPORT_ITEM_In or HID_REPORT_ITEM_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent)
 *
 *  \return Boolean \c true to force the sending of the report, \c false to let the library determine if it needs to be sent
 */
bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{
//	USB_KeyboardReport_Data_t* KeyboardReport = (USB_KeyboardReport_Data_t*)ReportData;
//	uint8_t UsedKeyCodes = 0;
//  if (UsedKeyCodes > 0)
    {
      memcpy(ReportData, &KeyboardReport, sizeof(USB_KeyboardReport_Data_t));
      //      UsedKeyCodes = 0;
    }


	*ReportSize = sizeof(USB_KeyboardReport_Data_t);
	return false;
}

/** HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either HID_REPORT_ITEM_Out or HID_REPORT_ITEM_Feature
 *  \param[in] ReportData  Pointer to a buffer where the received report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
	uint8_t  LEDMask   = LEDS_NO_LEDS;
	uint8_t* LEDReport = (uint8_t*)ReportData;

//	if (*LEDReport & HID_KEYBOARD_LED_NUMLOCK)
//	  LEDMask |= LEDS_LED1;

	if (*LEDReport & HID_KEYBOARD_LED_CAPSLOCK)
	  LEDMask |= LEDS_LED1;

//	if (*LEDReport & HID_KEYBOARD_LED_SCROLLLOCK)
//	  LEDMask |= LEDS_LED4;

	LEDs_SetAllLEDs(LEDMask);
}


/////////////////////////////////////////////////////////////
///// Software UART
////
/// after
/// http://www.atmel.com/Images/doc0941.pdf "AVR304: Half Duplex Interrupt Driven Software UART"
/// http://www.element14.com/community/docs/DOC-56281/l/avr304-half-duplex-interrupt-driven-software-uart-on-tinyavr-and-megaavr-devices-coding

#define BAUDRATE 9600
#define RX_PIN 0               //!< Receive data pin, must be INT0
#define INVERT_LEVELS 1 // 0: use standard active low signal levels - 1: use active high

#define TRXDDR  DDRD
#define TRXPORT PORTD
#define TRXPIN  PIND

#define GET_RX_PIN( )    ( TRXPIN & ( 1 << RX_PIN ) )

// chip specific configuration
#define ENABLE_TIMER_INTERRUPT( )       ( TIMSK0 |= ( 1<< OCIE0A ) )
#define DISABLE_TIMER_INTERRUPT( )      ( TIMSK0 &= ~( 1<< OCIE0A ) )
#define CLEAR_TIMER_INTERRUPT( )        ( TIFR0 |= ((1 << OCF0A) ) )
#define ENABLE_EXTERNAL0_INTERRUPT( )   ( EIMSK |= ( 1<< INT0 ) )
#define DISABLE_EXTERNAL0_INTERRUPT( )  ( EIMSK &= ~( 1<< INT0 ) )
#define TCCR             TCCR0A             //!< Timer/Counter Control Register
#define TCCR_P           TCCR0B             //!< Timer/Counter Control (Prescaler) Register
#define OCR              OCR0A              //!< Output Compare Register
#define EXT_IFR          EIFR              //!< External Interrupt Flag Register
#define EXT_ICR          EICRA             //!< External Interrupt Control Register
#define TIMER_COMP_VECT  TIMER0_COMPA_vect  //!< Timer Compare Interrupt Vector

// some magic number?
#define INTERRUPT_EXEC_CYCL   9       //!< Cycles to execute interrupt rutine from interrupt.

//#define C 8 // used prescaling factor for timer0
//#define N   (F_CPU / BAUDRATE / C) // N = Xcal / (Baud * C)
//#define TICKS2WAITONE       N  //!< Wait one bit period.
//#define TICKS2WAITONE_HALF  (N+N/2)  //!< Wait one and a half bit period.
//#define TICKS2WAITONE 208U /// 16000000 / (9600 * 8)
//#define TICKS2WAITONE_HALF 312U //!! larger then 8bit whooops
#define TICKS2WAITONE 26 /// 16000000 / (9600 * 64)
#define TICKS2WAITONE_HALF 39

//#define STR_HELPER(x) #x
//#define STR(x) STR_HELPER(x)
//#pragma message "content of N: " STR(N)

static void initSoftwareUart( void )
{
  //PORT
  TRXPORT |= ( 1 << RX_PIN );       // RX_PIN is input, tri-stated.

  // Timer0
  DISABLE_TIMER_INTERRUPT( );
  TCCR = 0x00;    //Init.
  TCCR_P = 0x00;    //Init.
  TCCR |= (1 << WGM01);			// Timer in CTC mode.
  TCCR_P |=  ( 1 << CS01 ) | (1<<CS00);        // Divide by 64 prescaler.

  //External interrupt
  EXT_ICR = 0x00;                   // Init.

#if INVERT_LEVELS
  EXT_ICR |= ( 1 << ISC01 ) | ( 1 << ISC00 );        // Interrupt sense control: rising edge.
#else
  EXT_ICR |= ( 1 << ISC01 );        // Interrupt sense control: falling edge.
#endif
  ENABLE_EXTERNAL0_INTERRUPT( );    // Turn external interrupt on.

  //Internal State Variable
  state = IDLE;
}


/*! \brief  External interrupt service routine.
 *
 *  The falling edge in the beginning of the start
 *  bit will trig this interrupt. The state will
 *  be changed to RECEIVE, and the timer interrupt
 *  will be set to trig one and a half bit period
 *  from the falling edge. At that instant the
 *  code should sample the first data bit.
 *
 *  \note  initSoftwareUart( void ) must be called in advance.
 */
ISR( INT0_vect)
{

  state = RECEIVE;                  // Change state
  DISABLE_EXTERNAL0_INTERRUPT( );   // Disable interrupt during the data bits.

  DISABLE_TIMER_INTERRUPT( );       // Disable timer to change its registers.
  TCCR_P &= ~( 1 << CS01 | 1 << CS00 );// Reset prescaler counter.

  TCNT0 = INTERRUPT_EXEC_CYCL;      // Clear counter register. Include time to run interrupt rutine.

  TCCR_P |=  ( 1 << CS01 ) | (1<<CS00);// Start prescaler clock.

  OCR = TICKS2WAITONE_HALF;         // Count one and a half period into the future.

  SwUartRXBitCount = 0;             // Clear received bit counter.
  CLEAR_TIMER_INTERRUPT( );         // Clear interrupt bits
  ENABLE_TIMER_INTERRUPT( );        // Enable timer0 interrupt on again

}


/*! \brief  Timer0 interrupt service routine.
 *
 *  Timer0 will ensure that bits are written and
 *  read at the correct instants in time.
 *  The state variable will ensure context
 *  switching between transmit and recieve.
 *  If state should be something else, the
 *  variable is set to IDLE. IDLE is regarded
 *  as a safe state/mode.
 *
 *  \note  initSoftwareUart( void ) must be called in advance.
 */
ISR( TIMER_COMP_VECT )
{

  switch (state) {

  //Receive Byte.
  case RECEIVE:
    OCR = TICKS2WAITONE;                  // Count one period after the falling edge is trigged.
    //Receiving, LSB first.
    if( SwUartRXBitCount < 8 ) {
        SwUartRXBitCount++;
        SwUartRXData = (SwUartRXData>>1);   // Shift due to receiving LSB first.
#if INVERT_LEVELS
        if( GET_RX_PIN( ) != 1 ) {
#else
        if( GET_RX_PIN( ) != 0 ) {
#endif
            SwUartRXData |= 0x80;           // If a logical 1 is read, let the data mirror this.
        }
    }

    //Done receiving
    else {
        state = DATA_PENDING;               // Enter DATA_PENDING when one byte is received.
        DISABLE_TIMER_INTERRUPT( );         // Disable this interrupt.
        EXT_IFR |= (1 << INTF0 );           // Reset flag not to enter the ISR one extra time.
        ENABLE_EXTERNAL0_INTERRUPT( );      // Enable interrupt to receive more bytes.
    }
  break;

  // Unknown state.
  default:
    state = IDLE;                           // Error, should not occur. Going to a safe state.
  }
}
