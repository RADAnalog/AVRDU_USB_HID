//*****************************************************************************
//	USB Protocol Functions - AVR_DU series
//
//	Minimal HID implementation with Interrupt EP1.IN and EP1.OUT
//
//	Notes:
//	The arrival of a setup packet automatically clears the TRNCOMPL flag.  Interrupt
//	transfers have to clear the flag manually.
//
//	Control packet: 21 0a 0000 0000 0000 = "SET_IDLE" is not implemented.
//
//	Pin7 - Configured LED
//	Pin6 - Suspended LED
//
//	Author: Richard
//	Date:	2026-04-13
//
//*****************************************************************************
#include "config.h"			// F_CPU
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>				// printf
#include <stdbool.h>			// true/false
#include <string.h>				// memcpy
#include <stddef.h>				// wchar_t
#include "usb.h"				// USB Common functions
#include "usb_private.h"		// USB helper private functions
#include "led.h"				// LED functions
#include "descriptor.h"			// Descriptor

//	Define Endpoint table
Usb_Endpoint_Table_t endpointTable;

//	Define Endpoint buffers
Usb_SetupPacket_t setupPacket;	// EP0.OUT 8 byte Setup packet
uint8_t controlPacket[64];		// EP0.IN/OUT Control packet
uint8_t ep1InPacket[8];			// EP1.IN packet
uint8_t ep1OutPacket[8];		// EP1.OUT packet

// USB state
volatile bool usbStateConfigured = false;


//*****************************************************************************
//	Initialize USB interface
//*****************************************************************************
void usbInit(void)
{
	// Enable USB module
	
	// Enable internal 3.3 V regulator
	SYSCFG.VUSBCTRL = SYSCFG_USBVREG_bm;
	
	// Set the maximum endpoint address used and enable the USB peripheral
	USB0.CTRLA = USB_ENABLE_bm | EP_MAX_ADDR;

	// Wait for PLL to lock
	usbWaitPllLock();		
	

	// Initialize Endpoint Table pointer
	USB0.EPPTR = (uint16_t)endpointTable.EP;
	
	
	// Initialize EP0.OUT (Setup) Host -> Device
	endpointTable.EP[EP0].OUT.STATUS	=	0;
	endpointTable.EP[EP0].OUT.CTRL		=	USB_TYPE_CONTROL_gc |	
											USB_TCDSBL_bm |					// Disable global interrupt
											USB_BUFSIZE_DEFAULT_BUF8_gc;	// Setup packet is always 8 bytes
	endpointTable.EP[EP0].OUT.DATAPTR	=	(uint16_t)&setupPacket;			// Setup packet buffer
	endpointTable.EP[EP0].OUT.CNT		=	0;
	endpointTable.EP[EP0].OUT.MCNT		=	0;
	
	// Initialize Control EP0.IN Device -> Host/Host -> Device
	endpointTable.EP[EP0].IN.STATUS		=	0;
	endpointTable.EP[EP0].IN.CTRL		=	USB_TYPE_CONTROL_gc |
											USB_TCDSBL_bm |					// Disable global interrupt
											USB_MULTIPKT_bm | USB_AZLP_bm |	// Multipacket and Automatic ZLP
											USB_BUFSIZE_DEFAULT_BUF64_gc;	// Buffer size must match DeviceDescriptor.MaxPacketSize0
	endpointTable.EP[EP0].IN.DATAPTR	=	(uint16_t)controlPacket;		// Control IN/OUT buffer (Datasheet 27.3.2.2 SETUP)
	endpointTable.EP[EP0].IN.CNT		=	0;
	endpointTable.EP[EP0].IN.MCNT		=	0;
	

	// Initialize Interrupt EP1.IN Device -> Host
	endpointTable.EP[EP1].IN.STATUS		=	0;
	endpointTable.EP[EP1].IN.CTRL		=	USB_TYPE_BULKINT_gc |
											USB_BUFSIZE_DEFAULT_BUF8_gc;
	endpointTable.EP[EP1].IN.DATAPTR	=	(uint16_t)ep1InPacket;
	endpointTable.EP[EP1].IN.CNT		=	0;
	endpointTable.EP[EP1].IN.MCNT		=	0;	
	
	// Initialize Interrupt EP1 OUT Host -> Device
	endpointTable.EP[EP1].OUT.STATUS	=	0;
	endpointTable.EP[EP1].OUT.CTRL		=	USB_TYPE_BULKINT_gc |
											USB_BUFSIZE_DEFAULT_BUF8_gc;
	endpointTable.EP[EP1].OUT.DATAPTR	=	(uint16_t)ep1OutPacket;
	endpointTable.EP[EP1].OUT.CNT		=	0;
	endpointTable.EP[EP1].OUT.MCNT		=	0;	
	
	
	// Enable USB Bus and Endpoint interrupts
	
	// Enable Bus interrupts
	USB0.INTCTRLA = USB_RESET_bm | USB_STALLED_bm;

	// Enable Endpoint interrupts
	USB0.INTCTRLB = USB_SETUP_bm | USB_TRNCOMPL_bm;
}

//*****************************************************************************
//	Bus Connect and Disconnect
//*****************************************************************************
void usbConnect(void)
{
	usbAttach();
}

void usbDisconnect(void)
{
	usbDetach();
}

//*****************************************************************************
// USB Bus (Device) Interrupt Routine - INTFLAGSA
//*****************************************************************************
ISR(USB0_BUSEVENT_vect)
{
	// Reset
	if (USB0.INTFLAGSA & USB_RESET_bm)
	{
		USB0.INTFLAGSA = USB_RESET_bm;
		
		// Reset the address
		usbSetDeviceAddress(0x00);
		
		usbEnableSuspend();
		
		// Clear Status LEDs
		ledSuspended(OFF);
		ledConfigured(OFF);
		
		// Clear USB state
		usbStateConfigured = false;
	}

	// Suspended
	if (USB0.INTFLAGSA & USB_SUSPEND_bm)
	{
		USB0.INTFLAGSA = USB_SUSPEND_bm;

		usbDisableSuspend();
		usbEnableResume();

		ledSuspended(ON);
	}

	// Resume
	if (USB0.INTFLAGSA & USB_RESUME_bm)
	{
		USB0.INTFLAGSA = USB_RESUME_bm;

		usbDisableResume();
		usbEnableSuspend();

		ledSuspended(OFF);
	}

	// Stall
	if (USB0.INTFLAGSA & USB_STALLED_bm)
	{
		USB0.INTFLAGSA = USB_STALLED_bm;
		
		usbClearStallRequest();		
	}
}

//*****************************************************************************
// USB Transaction (Endpoint) Interrupt Routine - INTFLAGSB
//*****************************************************************************
ISR(USB0_TRNCOMPL_vect)
{
	// Setup
	if (USB0.INTFLAGSB & USB_SETUP_bm)
	{
		USB0.INTFLAGSB = USB_SETUP_bm;
		
		usbClearSetup();
		
		// Handle request
		usbHandleSetupRequest();
		
		return;
	}
	
	// Handle EP1 IN/OUT Transfers
	if (USB0.INTFLAGSB & USB_TRNCOMPL_bm)
	{
		USB0.INTFLAGSB = USB_TRNCOMPL_bm;
		
		// Send packet to host
		if (endpointTable.EP[EP1].IN.STATUS & USB_TRNCOMPL_bm) 
		{	
			usbClearInTransaction(EP1);
		
			ep1InPacket[0]++;
			ep1InPacket[1]++;
			
			endpointTable.EP[EP1].IN.CNT = 2;
			
			// Print what is being sent
			//printf("EP1.IN=%02x, %02x\n", ep1InPacket[0], ep1InPacket[1]);
			
			//if (ep1InPacket[0] == 255)
			//{
				//printf("EP1.IN=%i\n", ep1InPacket[0]);
			//}
			
			// Ack IN transfer
			usbAckIn(EP1);
			
			return;
		}
		
		// Receive packet from host
		if (endpointTable.EP[EP1].OUT.STATUS & USB_TRNCOMPL_bm)
		{	
			usbClearOutTransaction(EP1);
			
			printf("EP1.OUT=%02x, %02x\n", ep1OutPacket[0], ep1OutPacket[1]);		
			
			// Ack OUT transfer
			usbAckOut(EP1);
			
			return;
		}	
	}
}

//*****************************************************************************
//	Decode and process the Setup Request
//*****************************************************************************
void usbHandleSetupRequest(void)
{
	// Debug - print Setup Packet
	printf("%02x %02x %04x %04x %04x\n", setupPacket.bmRequestType, setupPacket.bRequest, setupPacket.wValue, setupPacket.wIndex, setupPacket.wLength);
	
	// Decode Setup Request Type
	switch (setupPacket.bmRequestType & BM_REQUEST_TYPE_MASK) // Mask is 01100000
	{
		case Device_Standard:
			switch (setupPacket.bRequest)
			{
				case Standard_GetDescriptor:
					usbGetDescriptor();
					break;
				case Standard_SetAddress:
					usbSetAddress();
					break;
				case Standard_SetConfiguration:
					usbSetConfiguration();
					break;
				default:
					// Unsupported Standard_Request (bRequest)
					usbEnableStallRequest();
					break;
			}
			break;
		case Device_Class:
			switch (setupPacket.bRequest)
			{
				case Hid_GetReport:
					usbHidGetReport();
					break;
				case Hid_SetReport:
					usbHidSetReport();
					break;
				default:
					// Unsupported Class_Request (bRequest)
					usbEnableStallRequest();
					break;
			}
			break;					
		default:
			// Unsupported Request_Type (bmRequestType)
			usbEnableStallRequest();
			break;
	}
}

//*****************************************************************************
//	Get Descriptor (bRequest = 0x06)
//*****************************************************************************
void usbGetDescriptor(void)
{
	// Decode Descriptor Type
	switch (setupPacket.wValueH)
	{
		case Descriptor_Device:
			usbSendDescriptor(&DeviceDescriptor, DeviceDescriptor.bLength);
			break;
		case Descriptor_Configuration:
			usbSendDescriptor(&ConfigurationDescriptor, ConfigurationDescriptor.Configuration.wTotalLength);
			break;
		case Descriptor_Hid:
			usbSendDescriptor(&ConfigurationDescriptor.Hid, ConfigurationDescriptor.Hid.bLength);
			break;
		case Descriptor_HidReport:
			usbSendDescriptor(&HidReport, ConfigurationDescriptor.Hid.wDescriptorLength);
			break;
		case Descriptor_String:
			usbGetStringDescriptor();
			break;			
		default:
			// Unsupported Descriptor_Type (wValueH)
			usbEnableStallRequest();
			break;
	}
}

//*****************************************************************************
//	Get String Descriptor (bRequest = 0x06, wValueH = 0x03)
//*****************************************************************************
void usbGetStringDescriptor(void)
{
	// Decode String Index
	switch (setupPacket.wValueL) // String index
	{
		case String_Language:
			usbSendDescriptor(&LanguageString, LanguageString.bLength);
			break;
		case String_Manufacturer:
			usbSendDescriptor(&ManufacturerString, ManufacturerString.bLength);
			break;
		case String_Product:
			usbSendDescriptor(&ProductString, ProductString.bLength);
			break;
		case String_SerialNumber:
			usbSendDescriptor(&SerialNumberString, SerialNumberString.bLength);
			break;
		default:
			// Unsupported String Index (wValueL)
			usbEnableStallRequest();
		break;
	}
}

//*****************************************************************************
//	Send Descriptor to Host (Device -> Host)
//*****************************************************************************
void usbSendDescriptor(const void *descriptor, uint16_t descriptorLength)
{
	// Data Stage
	
	// Use the minimum of Host requested length and descriptor length
	uint16_t length = (setupPacket.wLength < descriptorLength) ? setupPacket.wLength : descriptorLength;
	
	// Copy descriptor to the EP0 buffer
	memcpy(controlPacket, descriptor, length);
	
	// Set packet length
	endpointTable.EP[EP0].IN.CNT = length;
	endpointTable.EP[EP0].IN.MCNT = 0;
	
	// Ack Data Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);
	
	// Ack Status Stage
	usbAckOut(EP0);
	usbWaitOutTransactionComplete(EP0);
	usbClearOutTransaction(EP0);
}

//*****************************************************************************
//	Set Address (bRequest = 0x05) Host -> Device
//*****************************************************************************
void usbSetAddress(void)
{
	// No Data Stage

	// Status Stage: reply with a ZLP
	endpointTable.EP[EP0].IN.CNT = 0;
	endpointTable.EP[EP0].IN.MCNT = 0;

	// Ack Status Stage	
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);

	// Set the address
	usbSetDeviceAddress(setupPacket.wValueL);
}

//*****************************************************************************
//	Set Configuration (bRequest = 0x09) Host -> Device
//*****************************************************************************
void usbSetConfiguration(void)
{
	// No Data Stage

	// Status Stage: reply with a ZLP
	endpointTable.EP[EP0].IN.CNT = 0;
	endpointTable.EP[EP0].IN.MCNT = 0;
	
	// Ack Status Stage	
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);

	// Enumeration is complete
	usbStateConfigured = true;
	
	ledConfigured(ON);
}

//*****************************************************************************
//	Hid Get Report (bRequest = 0x01) Device -> Host
//*****************************************************************************
void usbHidGetReport(void)
{
	// Data Stage 
	
	controlPacket[0] = 0x01;
	controlPacket[1] = 0x02;
	
	printf("GetReport Tx=%02x %02x\n", controlPacket[0], controlPacket[1]);

	endpointTable.EP[EP0].IN.CNT = 2;
	endpointTable.EP[EP0].IN.MCNT = 0;

	// Ack Data Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);

	// Ack Status Stage
	usbAckOut(EP0);
	usbWaitOutTransactionComplete(EP0);
	usbClearOutTransaction(EP0);
}

//*****************************************************************************
//	Hid Set Report (bRequest = 0x09) Host -> Device
//*****************************************************************************
void usbHidSetReport(void)
{
	// Ack Data Stage
	usbAckOut(EP0);
	usbWaitOutTransactionComplete(EP0);
	usbClearOutTransaction(EP0);
		
	// For EP0 Reports, the IN endpoint is used for both IN and OUT transactions.
	printf("SetReport Rx=%02x %02x\n", controlPacket[0], controlPacket[1]);

	// Status Stage: reply with a ZLP
	endpointTable.EP[EP0].IN.CNT = 0;
	endpointTable.EP[EP0].IN.MCNT = 0;

	// Ack Status IN Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);
}
