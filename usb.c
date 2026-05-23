//*****************************************************************************
//	USB Protocol Functions - AVR_DU series
//
//	Minimal HID implementation with Interrupt EP1.IN and EP1.OUT
//
//  Multipacket mode is used for the control EP0 IN endpoint to allow descriptors
//	larger than BUFSIZE.  After a USB RESET, the host expects only 8 bytes total on
//	the first Get_Descriptor request, and ignores the remaining 10 byte of the
//	descriptor.  If EP0.IN BUFSIZE is less than the full descriptor size, the 
//	multipacket processing will not complete properly and TRNCOMPL will not assert.
//  This will cause an infinite endpoint busy_wait loop. The workaround is to use a BUFSIZE
//  larger than the Device Descriptor length (i.e., =32 or = 64).
//
//	2026-05-12 - To mitigate the above issue somewhat, exit wait loop if INTFLAGSA.RESET.
//
//	If using the Control Endpoint for data transfer (HidGet/Set Report), the
//	the EP0.IN is used for *both* IN and OUT transactions.
//
//	80 06 0600 0000 000A = "Get Device Qualifier" not implemented for single speed devices.
//	21 0a 0000 0000 0000 = "HID Set Idle" - means NAK IN unless data is ready to send to host.
//
//	Pin7 - Configured LED - On when device is configured and ready
//	Pin6 - Suspended LED - On when Host signals suspend mode
//
//	Author:		Richard
//	Date:		2026-04-13
//	Updated:	2026-05-23 - minor comments clarifications
//
//*****************************************************************************
#include "config.h"				// F_CPU
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>				// printf
#include <stdbool.h>			// true/false
#include <string.h>				// memcpy
#include <stddef.h>				// wchar_t
#include "usb.h"				// USB Common functions
#include "usb_private.h"		// USB Private functions
#include "led.h"				// LED functions
#include "descriptor.h"			// Descriptor

//	Define Endpoint table (struct is word aligned)
Usb_Endpoint_Table_t endpointTable;

//	Define Endpoint buffers
Usb_SetupPacket_t setupPacket;	// EP0.OUT 8 byte Setup packet
uint8_t controlPacket[64];		// EP0.IN and EP0.OUT Control packet
uint8_t ep1InPacket[8];			// EP1.IN packet
uint8_t ep1OutPacket[8];		// EP1.OUT packet

// USB peripheral 'Configured' status
volatile bool usbStateConfigured = false;


//*****************************************************************************
//	Initialize USB interface
//	- Configure USB peripheral
//	- Configure Endpoints
//	- Enable Device and Endpoint interrupts
//*****************************************************************************
void usbInit(void)
{
	// Configure USB peripheral
	
	// Enable internal 3.3 V regulator
	SYSCFG.VUSBCTRL = SYSCFG_USBVREG_bm;
	
	// Set the maximum endpoint number and enable USB peripheral
	USB0.CTRLA = USB_ENABLE_bm | EP_MAX_ADDR;

	// Wait for PLL to lock
	usbWaitPllLock();		
	

	// Initialize Endpoints
	
	// Initialize Endpoint table pointer
	USB0.EPPTR = (uint16_t)endpointTable.EP;
	
	// Initialize EP0.OUT (Setup)
	endpointTable.EP[EP0].OUT.CTRL		=	USB_TYPE_CONTROL_gc |	
											USB_TCDSBL_bm |					// Disable global interrupt sync
											USB_BUFSIZE_DEFAULT_BUF8_gc;	// Setup packet is always 8 bytes
	endpointTable.EP[EP0].OUT.DATAPTR	=	(uint16_t)&setupPacket;			// Setup packet buffer
	endpointTable.EP[EP0].OUT.STATUS	=	0;
	endpointTable.EP[EP0].OUT.CNT		=	0;
	endpointTable.EP[EP0].OUT.MCNT		=	0;
	
	// Initialize EP0.IN (Control)
	endpointTable.EP[EP0].IN.CTRL		=	USB_TYPE_CONTROL_gc |
											USB_TCDSBL_bm |					// Disable global interrupt sync
											USB_MULTIPKT_bm | USB_AZLP_bm |	// Multipacket and Automatic ZLP
											USB_BUFSIZE_DEFAULT_BUF64_gc;	// Buffer size must match DeviceDescriptor.MaxPacketSize0
	endpointTable.EP[EP0].IN.DATAPTR	=	(uint16_t)controlPacket;		// Control IN and OUT buffer (Datasheet 27.3.2.2 SETUP)
	endpointTable.EP[EP0].IN.STATUS		=	0;
	endpointTable.EP[EP0].IN.CNT		=	0;
	endpointTable.EP[EP0].IN.MCNT		=	0;
	
	// Initialize Interrupt EP1.IN (Device -> Host)
	endpointTable.EP[EP1].IN.CTRL		=	USB_TYPE_BULKINT_gc |
											USB_BUFSIZE_DEFAULT_BUF8_gc;
	endpointTable.EP[EP1].IN.DATAPTR	=	(uint16_t)ep1InPacket;
	endpointTable.EP[EP1].IN.STATUS		=	0;
	endpointTable.EP[EP1].IN.CNT		=	0;
	endpointTable.EP[EP1].IN.MCNT		=	0;	
	
	// Initialize Interrupt EP1 OUT (Host -> Device)
	endpointTable.EP[EP1].OUT.CTRL		=	USB_TYPE_BULKINT_gc |
											USB_BUFSIZE_DEFAULT_BUF8_gc;
	endpointTable.EP[EP1].OUT.DATAPTR	=	(uint16_t)ep1OutPacket;
	endpointTable.EP[EP1].OUT.STATUS	=	0;
	endpointTable.EP[EP1].OUT.CNT		=	0;
	endpointTable.EP[EP1].OUT.MCNT		=	0;	
	
	
	// Enable USB Device and Endpoint interrupts
	
	// Enable Device interrupts
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
	// Setup packet received
	if (USB0.INTFLAGSB & USB_SETUP_bm)
	{
		USB0.INTFLAGSB = USB_SETUP_bm;
		
		// Clear endpoint flags
		usbClearSetup();
		
		// Handle setup request
		usbHandleSetupRequest();
		
		return;
	}
	
	// Handle EP1 IN/OUT Transfers
	if (USB0.INTFLAGSB & USB_TRNCOMPL_bm)
	{
		USB0.INTFLAGSB = USB_TRNCOMPL_bm;
		
		// Send packet to host according to HidReport.bInterval
		if (endpointTable.EP[EP1].IN.STATUS & USB_TRNCOMPL_bm) 
		{	
			usbClearInTransaction(EP1);
		
			// Load packet with data to be sent to host on next EP1.IN request
			//ep1InPacket[0] = your data here;
			//ep1InPacket[1] = your data here;
			
			// Set buffer length
			endpointTable.EP[EP1].IN.CNT = 2;
			
			// Ack IN transfer to host - only when data is available
			//usbAckIn(EP1);
			
			return;
		}
		
		// Receive packet from host on HidReport.bInterval frequency
		if (endpointTable.EP[EP1].OUT.STATUS & USB_TRNCOMPL_bm)
		{	
			usbClearOutTransaction(EP1);
			
			// Print data received from EP1.OUT transaction
			printf("EP1.OUT=%02x, %02x\n", ep1OutPacket[0], ep1OutPacket[1]);	
			
			// Test: loop back to IN from this OUT packet
			ep1InPacket[0] = ep1OutPacket[0];
			ep1InPacket[1] = ep1OutPacket[1];			
			
			// Ack OUT transfer from host
			usbAckOut(EP1);
			
			return;
		}	
	}
}

//*****************************************************************************
//	Process the Setup Request
//*****************************************************************************
void usbHandleSetupRequest(void)
{
	// Debug - print Setup Packet
	printf("%02x %02x %04x %04x %04x\n", setupPacket.bmRequestType, setupPacket.bRequest, setupPacket.wValue, setupPacket.wIndex, setupPacket.wLength);
	
	// Decode Setup Request Type
	switch (setupPacket.bmRequestType & BM_REQUEST_TYPE_MASK)	// 01100000b
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
					// Stall unsupported Standard_Request (bRequest)
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
				case Hid_SetIdle:
					usbHidSetIdle();
					break;					
				default:
					// Stall unsupported Class_Request (bRequest)
					usbEnableStallRequest();
					break;
			}
			break;					
		default:
			// Stall unsupported Request_Type (bmRequestType)
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
			// Stall unsupported Descriptor_Type (wValueH)
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
			// Stall unsupported String Index (wValueL)
			usbEnableStallRequest();
			break;
	}
}

//*****************************************************************************
//	Send Descriptor to Host (Device -> Host)
//*****************************************************************************
void usbSendDescriptor(const void *descriptor, uint16_t descriptorLength)
{
	// -- Data Stage --
	
	// Use the minimum of requested length and descriptor length
	uint16_t length = (setupPacket.wLength < descriptorLength) ? setupPacket.wLength : descriptorLength;
	
	// Copy descriptor to the EP0 buffer
	memcpy(controlPacket, descriptor, length);
	
	// Set data packet length
	endpointTable.EP[EP0].IN.CNT = length;
	endpointTable.EP[EP0].IN.MCNT = 0;
	
	// Ack Data Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);
	
	// -- Status Stage --
	
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
	
	// -- Status Stage --

	// Respond with ZLP
	endpointTable.EP[EP0].IN.CNT = 0;
	endpointTable.EP[EP0].IN.MCNT = 0;

	// Ack Status Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);

	// Set this device address
	usbSetDeviceAddress(setupPacket.wValueL);
}

//*****************************************************************************
//	Set Configuration (bRequest = 0x09) Host -> Device
//*****************************************************************************
void usbSetConfiguration(void)
{
	// No Data Stage
	
	// -- Status Stage -- 

	// Respond with ZLP
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
//	Set Idle (bRequest = 0x0A) Host -> Device
//*****************************************************************************
void usbHidSetIdle(void)
{
	// No Data Stage
	
	// -- Status Stage --

	// Respond with ZLP
	endpointTable.EP[EP0].IN.CNT = 0;
	endpointTable.EP[EP0].IN.MCNT = 0;
		
	// Ack Status Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);
}

//*****************************************************************************
//	Hid Get Report (bRequest = 0x01) Device -> Host
//*****************************************************************************
void usbHidGetReport(void)
{
	// -- Data Stage  --
	
	// Load data to be sent to the host on this request, e.g.:
	//controlPacket[0] = 0x01;
	//controlPacket[1] = 0x02;
	
	// For EP0 Reports the IN endpoint is used for *both* IN and OUT transactions.
	// Debug - print data to send to the Host over EP0.
	printf("GetReport Tx=%02x %02x\n", controlPacket[0], controlPacket[1]);
	
	// Set number of bytes to be sent to the host
	endpointTable.EP[EP0].IN.CNT = 2;
	endpointTable.EP[EP0].IN.MCNT = 0;

	// Ack Data Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);
	
	// -- Status Stage --

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
	// -- Data Stage --
	
	// Ack Data Stage
	usbAckOut(EP0);
	usbWaitOutTransactionComplete(EP0);
	usbClearOutTransaction(EP0);
		
	// For EP0 Reports, the IN endpoint is used for *both* IN and OUT transactions.
	// Print data received form the Host over the Control Endpoint.
	printf("SetReport Rx=%02x %02x\n", controlPacket[0], controlPacket[1]);
	
	// -- Status Stage --

	// Respond with ZLP
	endpointTable.EP[EP0].IN.CNT = 0;
	endpointTable.EP[EP0].IN.MCNT = 0;

	// Ack Status Stage
	usbAckIn(EP0);
	usbWaitInTransactionComplete(EP0);
	usbClearInTransaction(EP0);
}
