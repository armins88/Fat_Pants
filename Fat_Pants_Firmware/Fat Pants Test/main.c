/************************************************************************
	main.c

	WFF USB Generic HID Demonstration 3
    usbGenericHidCommunication reference firmware 3_0_0_0
    Copyright (C) 2011 Simon Inns

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Email: simon.inns@gmail.com

************************************************************************/

#ifndef MAIN_C
#define MAIN_C

// Global includes
// Note: string.h is required for sprintf commands for debug
#include <string.h>

// Local includes
#include "HardwareProfile.h"
#include "./debug.h"

// Microchip Application Library includes
// (expects V2.9a of the USB library from "Microchip Solutions v2011-07-14")
//
// The library location must be set in:
// Project -> Build Options Project -> Directories -> Include search path
// in order for the project to compile.
#include "./usb.h"
#include "./usb_function_hid.h"

// Ensure we have the correct target PIC device family
#if !defined(__18F4550) && !defined(__18F2450)
	#error "This firmware only supports either the PIC18F4550 or PIC18F2550 microcontrollers."
#endif

#define BLACK                          0 ///< Draw 'off' pixels
#define WHITE                          1 ///< Draw 'on' pixels
#define INVERSE                        2 ///< Invert pixels

#define SSD1306_MEMORYMODE          0x20 ///< See datasheet
#define SSD1306_COLUMNADDR          0x21 ///< See datasheet
#define SSD1306_PAGEADDR            0x22 ///< See datasheet
#define SSD1306_SETCONTRAST         0x81 ///< See datasheet
#define SSD1306_CHARGEPUMP          0x8D ///< See datasheet
#define SSD1306_SEGREMAP            0xA0 ///< See datasheet
#define SSD1306_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SSD1306_DISPLAYALLON        0xA5 ///< Not currently used
#define SSD1306_NORMALDISPLAY       0xA6 ///< See datasheet
#define SSD1306_INVERTDISPLAY       0xA7 ///< See datasheet
#define SSD1306_SETMULTIPLEX        0xA8 ///< See datasheet
#define SSD1306_DISPLAYOFF          0xAE ///< See datasheet
#define SSD1306_DISPLAYON           0xAF ///< See datasheet
#define SSD1306_COMSCANINC          0xC0 ///< Not currently used
#define SSD1306_COMSCANDEC          0xC8 ///< See datasheet
#define SSD1306_SETDISPLAYOFFSET    0xD3 ///< See datasheet
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5 ///< See datasheet
#define SSD1306_SETPRECHARGE        0xD9 ///< See datasheet
#define SSD1306_SETCOMPINS          0xDA ///< See datasheet
#define SSD1306_SETVCOMDETECT       0xDB ///< See datasheet

#define SSD1306_SETLOWCOLUMN        0x00 ///< Not currently used
#define SSD1306_SETHIGHCOLUMN       0x10 ///< Not currently used
#define SSD1306_SETSTARTLINE        0x40 ///< See datasheet

#define SSD1306_EXTERNALVCC         0x01 ///< External display voltage source
#define SSD1306_SWITCHCAPVCC        0x02 ///< Gen. display voltage from 3.3V

#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26 ///< Init rt scroll
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27 ///< Init left scroll
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A ///< Init diag scroll
#define SSD1306_DEACTIVATE_SCROLL                    0x2E ///< Stop scroll
#define SSD1306_ACTIVATE_SCROLL                      0x2F ///< Start scroll
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3 ///< Set scroll range

// Define the globals for the USB data in the USB RAM of the PIC18F*550
#pragma udata
#pragma udata USB_VARIABLES=0x480
unsigned char ReceivedDataBuffer[64];
unsigned char ToSendDataBuffer[64];
#pragma udata

USB_HANDLE USBOutHandle = 0;
USB_HANDLE USBInHandle = 0;
BOOL blinkStatusValid = FLAG_TRUE;

// PIC18F4550/PIC18F2550 configuration for the WFF Generic HID test device
#pragma config PLLDIV   = 5         // 20Mhz external oscillator
#pragma config CPUDIV   = OSC1_PLL2   
#pragma config USBDIV   = 2         // Clock source from 96MHz PLL/2
#pragma config FOSC     = HSPLL_HS
#pragma config FCMEN    = OFF
#pragma config IESO     = OFF
#pragma config PWRT     = OFF
#pragma config BOR      = ON
#pragma config BORV     = 21
#pragma config VREGEN   = ON
#pragma config WDT      = OFF
#pragma config WDTPS    = 32768
#pragma config MCLRE    = ON
#pragma config LPT1OSC  = OFF
#pragma config PBADEN   = OFF
// #pragma config CCP2MX   = ON
#pragma config STVREN   = ON
#pragma config LVP      = OFF
// #pragma config ICPRT    = OFF
#pragma config XINST    = OFF
#pragma config CP0      = OFF
#pragma config CP1      = OFF
// #pragma config CP2      = OFF
// #pragma config CP3      = OFF
#pragma config CPB      = OFF
// #pragma config CPD      = OFF
#pragma config WRT0     = OFF
#pragma config WRT1     = OFF
// #pragma config WRT2     = OFF
// #pragma config WRT3     = OFF
#pragma config WRTB     = OFF
#pragma config WRTC     = OFF
// #pragma config WRTD     = OFF
#pragma config EBTR0    = OFF
#pragma config EBTR1    = OFF
// #pragma config EBTR2    = OFF
// #pragma config EBTR3    = OFF
#pragma config EBTRB    = OFF

// Private function prototypes
static void initialisePic(void);
void processUsbCommands(void);
void applicationInit(void);
void USBCBSendResume(void);
void highPriorityISRCode();
void lowPriorityISRCode();

/*I2C LIBRARY*/

#define SDA_PIN PORTBbits.RB1
#define SCL_PIN PORTBbits.RB0	
#define SDA_DIR TRISBbits.RB1
#define SCL_DIR TRISBbits.RB0

unsigned char i2cReadByte(void);
void i2cWriteByte(unsigned char);
void i2cNack(void);
void i2cAck(void);
void i2cStart(void);
void i2cStop(void);
void i2cHighSda(void);
void i2cLowSda(void);
void i2cHighScl(void);
void i2cLowScl(void);

#define FCY 48000000.0
#define __delay_us(x) Delay10TCYx((x*(FCY/1000000.0))/10.0)

unsigned char i2cReadByte(void)
{
   unsigned char inByte, n;
   i2cHighSda();
   for (n=0; n<8; n++)
   {
      i2cHighScl();

      if (SDA_PIN)
         inByte = (inByte << 1) | 0x01; // msbit first
      else
         inByte = inByte << 1;
      i2cLowScl();
   }
   return(inByte);
}

void i2cWriteByte(unsigned char outByte)
{
   unsigned char n;
   for(n=0; n<8; n++)
   {
      if(outByte&0x80)
         i2cHighSda();
      else
         i2cLowSda();
      i2cHighScl();
      i2cLowScl();
      outByte = outByte << 1;
   }
   i2cHighSda();
}

void i2cWriteBytes(char* buf, int num_bytes)
{
    int i;
    for (i = 0; i < num_bytes; i++) {
        i2cWriteByte(buf[i]);
    }
}

void i2cNack(void)
{
   i2cHighScl();
   i2cLowScl();		// bring data high and clock
}

void i2cAck(void)
{
   i2cLowSda();	
   i2cHighScl();
   i2cLowScl();
   i2cHighSda();		// bring data low and clock
}


void i2c_start(void)
{
   i2cLowScl();
   i2cHighSda();
   i2cHighScl();	
   i2cLowSda();
   i2cLowScl();		// bring SDA low while SCL is high
}

void i2cStop(void)
{
   i2cLowScl();
   i2cLowSda();
   i2cHighScl();
   i2cHighSda();		// bring SDA high while SCL is high
}

void i2cHighSda(void)
{
   SDA_DIR = 1;		// bring SDA to high impedance
    __delay_us(5);
}

void i2cLowSda(void)
{
   SDA_PIN = 0;
   SDA_DIR = 0;		// output a logic zero
   __delay_us(5);
}

void i2cHighScl(void)
{
   SCL_DIR = 1;		// bring SCL to high impedance
    __delay_us(5);
}

void i2cLowScl(void)
{
   SCL_PIN = 0;		
   SCL_DIR = 0;
    __delay_us(5);
}



// Remap vectors for compatibilty with Microchip USB boot loaders
#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x1000
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x1008
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x1018
#elif defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)	
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x800
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x808
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x818
#else	
	#define REMAPPED_RESET_VECTOR_ADDRESS			0x00
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x08
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x18
#endif

#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER) || defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
	extern void _startup (void);
	#pragma code REMAPPED_RESET_VECTOR = REMAPPED_RESET_VECTOR_ADDRESS
	void _reset (void)
	{
	    _asm goto _startup _endasm
	}
#endif

#pragma code REMAPPED_HIGH_INTERRUPT_VECTOR = REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS
void Remapped_High_ISR (void)
{
     _asm goto highPriorityISRCode _endasm
}

#pragma code REMAPPED_LOW_INTERRUPT_VECTOR = REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS
void Remapped_Low_ISR (void)
{
     _asm goto lowPriorityISRCode _endasm
}

#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER) || defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
#pragma code HIGH_INTERRUPT_VECTOR = 0x08
void High_ISR (void)
{
     _asm goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS _endasm
}

#pragma code LOW_INTERRUPT_VECTOR = 0x18
void Low_ISR (void)
{
     _asm goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS _endasm
}
#endif

#pragma code

// High-priority ISR handling function
#pragma interrupt highPriorityISRCode
void highPriorityISRCode()
{
	// Application specific high-priority ISR code goes here
	
	#if defined(USB_INTERRUPT)
		// Perform USB device tasks
		USBDeviceTasks();
	#endif

}

// Low-priority ISR handling function
#pragma interruptlow lowPriorityISRCode
void lowPriorityISRCode()
{
	// Application specific low-priority ISR code goes here
}

// String for creating debug messages
//char debugString[64];

// Main program entry point
void main(void)
{   
	// Initialise and configure the PIC ready to go
    initialisePic();

	// If we are running in interrupt mode attempt to attach the USB device
    #if defined(USB_INTERRUPT)
        USBDeviceAttach();
    #endif
	
    // Show that we are up and running
    mStatusLED0_on();
	__delay_us(10);
    
        i2c_start();
        i2cWriteByte(0x3c);
        i2cWriteBytes("\x00\xD3\x00\x00\x00\x40\x00\x8d", 8);
        i2cWriteBytes("\x00\xDB\x00\x40\x00\xA4\x00\xa6\x00\x2e\x00\xAF", 12);
        i2cStop();
    while(1);
	// Main processing loop
    while(1)
    {
        #if defined(USB_POLLING)
			// If we are in polling mode the USB device tasks must be processed here
			// (otherwise the interrupt is performing this task)
	        USBDeviceTasks();
        #endif
    	
    	// Process USB Commands
        processUsbCommands();  
        
        // Toggle the LED0
//		mStatusLED0_Toggle();
        // Note: Other application specific actions can be placed here      
    }
}

// Initialise the PIC
static void initialisePic(void)
{
    // PIC port set up --------------------------------------------------------

	// Default all pins to digital
    ADCON1 = 0xFF;

	// Configure ports as inputs (1) or outputs(0)
	TRISA = 0b00000000;
	TRISB = 0b00000000;
	TRISC = 0b00000000;
#if defined(__18F4550)
	TRISD = 0b00000000;
	TRISE = 0b00000000;
#endif

	// Clear all ports
	PORTA = 0b00000000;
	PORTB = 0b00000000;
	PORTC = 0b00000000;
#if defined(__18F4550)
	PORTD = 0b00000000;
	PORTE = 0b00000000;
#endif

	// If you have a VBUS sense pin (for self-powered devices when you
	// want to detect if the USB host is connected) you have to specify
	// your input pin in HardwareProfile.h
    #if defined(USE_USB_BUS_SENSE_IO)
    	tris_usb_bus_sense = INPUT_PIN;
    #endif
    
    // In the case of a device which can be both self-powered and bus-powered
    // the device must respond correctly to a GetStatus (device) request and
    // tell the host how it is currently powered.
    //
    // To do this you must device a pin which is high when self powered and low
    // when bus powered and define this in HardwareProfile.h
    #if defined(USE_SELF_POWER_SENSE_IO)
    	tris_self_power = INPUT_PIN;
    #endif

    // Application specific initialisation
    applicationInit();
    
    // Initialise the USB device
    USBDeviceInit();
}

// Application specific device initialisation
void applicationInit(void)
{
	// Initialise the status LEDs
	mInitStatusLeds();
	
	// Initialise the switch
	mInitAllSwitches();
	
    // Initialize the variable holding the USB handle for the last transmission
    USBOutHandle = 0;
    USBInHandle = 0;
}

// Process USB commands
void processUsbCommands(void)
{   
    // Check if we are in the configured state; otherwise just return
    if((USBDeviceState < CONFIGURED_STATE) || (USBSuspendControl == 1))
    {
	    // We are not configured
	    return;
	}

	// Check if data was received from the host.
    if(!HIDRxHandleBusy(USBOutHandle))
    {   
		// Command mode    
        switch(ReceivedDataBuffer[0])
		{
			case 0x10:	// Debug information request from host
				// Copy any waiting debug text to the send data buffer
				copyDebugToSendBuffer((BYTE*)&ToSendDataBuffer[0]);
			        
		        // Transmit the response to the host
                if(!HIDTxHandleBusy(USBInHandle))
				{
					USBInHandle = HIDTxPacket(HID_EP,(BYTE*)&ToSendDataBuffer[0],64);
				}
				break;
				
			// Place application specific commands here:
			
            case 0x80:  // Toggle the LED
//            	sprintf(debugString, "Received command 0x80 from host - Toggle LED");
//				debugOut(debugString);

				// Toggle the LED0
				mStatusLED0_Toggle();
            	break;
            	
            case 0x81:  // Read the push switch status
				ToSendDataBuffer[0] = sw0;

				// Transmit the response to the host
                if(!HIDTxHandleBusy(USBInHandle))
				{
					USBInHandle = HIDTxPacket(HID_EP,(BYTE*)&ToSendDataBuffer[0],64);
				}
            	break;
            	
            case 0x82:  // Read the LED status
				// Get the LED state and put it in the send buffer
				ToSendDataBuffer[0] = mStatusLED0_Get();
				
				// Transmit the response to the host
                if(!HIDTxHandleBusy(USBInHandle))
				{
					USBInHandle = HIDTxPacket(HID_EP,(BYTE*)&ToSendDataBuffer[0],64);
				}
            	break;

            default:	// Unknown command received
           		break;
		}
		 
        // Re-arm the OUT endpoint for the next packet
        USBOutHandle = HIDRxPacket(HID_EP,(BYTE*)&ReceivedDataBuffer,64);
  	}
}

// USB Callback handling routines -----------------------------------------------------------

// Call back that is invoked when a USB suspend is detected
void USBCBSuspend(void)
{
}

// This call back is invoked when a wakeup from USB suspend is detected.
void USBCBWakeFromSuspend(void)
{
}

// The USB host sends out a SOF packet to full-speed devices every 1 ms.
void USBCB_SOF_Handler(void)
{
    // No need to clear UIRbits.SOFIF to 0 here. Callback caller is already doing that.
}

// The purpose of this callback is mainly for debugging during development.
// Check UEIR to see which error causes the interrupt.
void USBCBErrorHandler(void)
{
    // No need to clear UEIR to 0 here.
    // Callback caller is already doing that.
}

// Check other requests callback
void USBCBCheckOtherReq(void)
{
    USBCheckHIDRequest();
}

// Callback function is called when a SETUP, bRequest: SET_DESCRIPTOR request arrives.
void USBCBStdSetDscHandler(void)
{
    // You must claim session ownership if supporting this request
}

//This function is called when the device becomes initialized
void USBCBInitEP(void)
{
    // Enable the HID endpoint
    USBEnableEndpoint(HID_EP,USB_IN_ENABLED|USB_OUT_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
    
    // Re-arm the OUT endpoint for the next packet
    USBOutHandle = HIDRxPacket(HID_EP,(BYTE*)&ReceivedDataBuffer,64);
}

// Send resume call-back
void USBCBSendResume(void)
{
    static WORD delay_count;
    
    // Verify that the host has armed us to perform remote wakeup.
    if(USBGetRemoteWakeupStatus() == FLAG_TRUE) 
    {
        // Verify that the USB bus is suspended (before we send remote wakeup signalling).
        if(USBIsBusSuspended() == FLAG_TRUE)
        {
            USBMaskInterrupts();
            
            // Bring the clock speed up to normal running state
            USBCBWakeFromSuspend();
            USBSuspendControl = 0; 
            USBBusIsSuspended = FLAG_FALSE;

            // Section 7.1.7.7 of the USB 2.0 specifications indicates a USB
            // device must continuously see 5ms+ of idle on the bus, before it sends
            // remote wakeup signalling.  One way to be certain that this parameter
            // gets met, is to add a 2ms+ blocking delay here (2ms plus at 
            // least 3ms from bus idle to USBIsBusSuspended() == FLAG_TRUE, yeilds
            // 5ms+ total delay since start of idle).
            delay_count = 3600U;        
            do
            {
                delay_count--;
            } while(delay_count);
            
            // Start RESUME signaling for 1-13 ms
            USBResumeControl = 1;
            delay_count = 1800U;
            do
            {
                delay_count--;
            } while(delay_count);
            USBResumeControl = 0;

            USBUnmaskInterrupts();
        }
    }
}

// USB callback function handler
BOOL USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, WORD size)
{
    switch(event)
    {
        case EVENT_TRANSFER:
            // Application callback tasks and functions go here
            break;
        case EVENT_SOF:
            USBCB_SOF_Handler();
            break;
        case EVENT_SUSPEND:
            USBCBSuspend();
            break;
        case EVENT_RESUME:
            USBCBWakeFromSuspend();
            break;
        case EVENT_CONFIGURED: 
            USBCBInitEP();
            break;
        case EVENT_SET_DESCRIPTOR:
            USBCBStdSetDscHandler();
            break;
        case EVENT_EP0_REQUEST:
            USBCBCheckOtherReq();
            break;
        case EVENT_BUS_ERROR:
            USBCBErrorHandler();
            break;
        default:
            break;
    }      
    return FLAG_TRUE; 
}

#endif
