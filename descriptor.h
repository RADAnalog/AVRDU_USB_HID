//*****************************************************************************
//	USB HID Descriptor - Vendor Defined
//
//	The HidGetReport and HidSetReport use the HidReport parameters.  The number
//	of bytes to sent or received = 2
//	The IN and OUT Interrupt transactions uses the same HidReport parameters,
//	i.e., the number of bytes sent or received = 2
//
//	For proper enumeration, the EP0.IN.BUFSIZE must be greater than 18 bytes (i.e., 32
//	or 64) to accommodate the full Device Descriptor.  This is due to the way the host 
//	executes the first GET_DESCRIPTOR.  See notes.
//
//	Author: Richard
//	Date:	2026-04-07
//
//*****************************************************************************
#ifndef DESCRIPTOR_HID_VENDOR_DEFINED_H_
#define DESCRIPTOR_HID_VENDOR_DEFINED_H_

#include <stddef.h>		// wchar_t

// USB Device Identifier
#define USB_VID					0x04D8		// Microchip
#define USB_PID					0x002E		// 0x0010 - 0x002F reserved for testing/non-public

// Unicode strings, no commas permitted
#define MANUFACTURER_STR 		L"USB Project"
#define PRODUCT_STR 			L"AVR16DU14"
#define SERIAL_NUMBER_STR 		L"0001"


// Device Descriptor - 18 bytes
typedef struct {
	uint8_t     bLength;
	uint8_t     bDescriptorType;
	uint16_t    bcdUSB;
	uint8_t     bDeviceClass;
	uint8_t     bDeviceSubClass;
	uint8_t     bDeviceProtocol;
	uint8_t     bMaxPacketSize0;
	uint16_t    idVendor;
	uint16_t    idProduct;
	uint16_t    bcdDevice;
	uint8_t     iManufacturer;
	uint8_t     iProduct;
	uint8_t     iSerialNumber;
	uint8_t     bNumConfigurations;
} Usb_DeviceDescriptor_t;


// Configuration Descriptor Header - 9 bytes
typedef struct {
	uint8_t     bLength;
	uint8_t     bDescriptorType;
	uint16_t    wTotalLength;
	uint8_t     bNumInterfaces;
	uint8_t     bConfigurationValue;
	uint8_t     iConfiguration;
	uint8_t     bmAttributes;
	uint8_t     bMaxPower;
} Usb_ConfigurationDescriptorHeader_t;


// Interface Descriptor - 9 bytes
typedef struct {
	uint8_t     bLength;
	uint8_t     bDescriptorType;
	uint8_t     bInterfaceNumber;
	uint8_t     bAlternateSetting;
	uint8_t     bNumEndpoints;
	uint8_t     bInterfaceClass;
	uint8_t     bInterfaceSubClass;
	uint8_t     bInterfaceProtocol;
	uint8_t     iInterface;
} Usb_InterfaceDescriptor_t;


// HID Descriptor - 9 bytes
typedef struct {
	uint8_t     bLength;
	uint8_t     bDescriptorType;
	uint16_t    bcdHID;
	uint8_t     bCountryCode;
	uint8_t     bNumDescriptors;
	uint8_t     bReportDescriptorType;
	uint16_t    wDescriptorLength;
} Usb_HidDescriptor_t;


// Endpoint Descriptor - 7 bytes
typedef struct {
	uint8_t     bLength;
	uint8_t     bDescriptorType;
	uint8_t     bEndpointAddress;
	uint8_t     bmAttributes;
	uint16_t    wMaxPacketSize;
	uint8_t     bInterval;
} Usb_EndpointDescriptor_t;

// String Descriptor
typedef struct {
	uint8_t     bLength;
	uint8_t     bDescriptorType;
	wchar_t     bString[];
} Usb_StringDescriptor_t;

//*****************************************************************************
//	Configuration Descriptor Assembly struct
//*****************************************************************************
typedef struct {
	Usb_ConfigurationDescriptorHeader_t	Configuration;
	Usb_InterfaceDescriptor_t			Interface;
	Usb_HidDescriptor_t					Hid;
	Usb_EndpointDescriptor_t			HidReportInEndpoint;
	Usb_EndpointDescriptor_t			HidReportOutEndpoint;
} Usb_ConfigurationDescriptor_t;

//*****************************************************************************
//  Hid Report
//	The Control endpoint and IN/OUT endpoint transfers use the same Report
//*****************************************************************************
const uint8_t HidReport[] = {
	0x06, 0x00, 0xFF,        // Usage Page (Vendor Defined)
	0x09, 0x01,              // Usage (Vendor Defined)
	0xA1, 0x01,              // Collection (Application)
		// Input Report (2 bytes, no ReportID)
		0x09, 0x01,          //   Usage
		0x15, 0x00,          //   Logical Minimum (0)
		0x26, 0xFF, 0x00,    //   Logical Maximum (255)
		0x75, 8,             //   Report Size (8 bits = 1 byte per Report Count)
		0x95, 2,             //   Report Count (2 bytes) must be <= EPn.IN.BUFSIZE
		0x81, 0x02,          //   Input (Data,Var,Abs)
		// Output Report (2 bytes, no ReportID)
		0x09, 0x01,          //   Usage
		0x15, 0x00,          //   Logical Minimum (0)
		0x26, 0xFF, 0x00,    //   Logical Maximum (255)
		0x75, 8,             //   Report Size (8 bits = 1 byte per Report Count)
		0x95, 2,             //   Report Count (2 bytes) must be <= EPn.OUT.BUFSIZE
		0x91, 0x02,          //   Output (Data,Var,Abs)
	0xC0                     // End Collection
};

//*****************************************************************************
//  Descriptor Initializers
//*****************************************************************************

// Device Descriptor
const Usb_DeviceDescriptor_t DeviceDescriptor = {
    .bLength				= sizeof(Usb_DeviceDescriptor_t),
    .bDescriptorType		= 0x01,         // Device
    .bcdUSB					= 0x0200,       // USB 2.00
    .bDeviceClass			= 0x00,         // Defined at interface
    .bDeviceSubClass		= 0x00,
    .bDeviceProtocol		= 0x00,
    .bMaxPacketSize0		= 64,			// EP0 size, must match EP0.OUT.BUFSIZE
    .idVendor				= USB_VID,
    .idProduct				= USB_PID,
    .bcdDevice				= 0x0001,       // User defined
    .iManufacturer			= 0,
    .iProduct				= 0,
    .iSerialNumber			= 0,
    .bNumConfigurations		= 1
};

// Configuration Descriptor
const Usb_ConfigurationDescriptor_t ConfigurationDescriptor = {

    .Configuration = {
        .bLength            = sizeof(Usb_ConfigurationDescriptorHeader_t),
        .bDescriptorType    = 0x02,		// Configuration
        .wTotalLength       = sizeof(Usb_ConfigurationDescriptor_t),
        .bNumInterfaces     = 1,
        .bConfigurationValue= 1,
        .iConfiguration     = 0,
        .bmAttributes       = USB_SELF_POWERED,
        .bMaxPower          = 100 / 2	// 100mA
    },

    .Interface = {
        .bLength            = sizeof(Usb_InterfaceDescriptor_t),
        .bDescriptorType    = 0x04,		// Interface
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 2,		// Number of EP descriptors
        .bInterfaceClass    = 0x03,		// HID
        .bInterfaceSubClass = 0x00,
        .bInterfaceProtocol = 0x00,
        .iInterface         = 0
    },

    .Hid = {
        .bLength            = sizeof(Usb_HidDescriptor_t),
        .bDescriptorType    = 0x21,		// HID
        .bcdHID             = 0x0110,	// HID 1.10
        .bCountryCode       = 0,
        .bNumDescriptors    = 1,
        .bReportDescriptorType = 0x22,	// HID Report
        .wDescriptorLength  = sizeof(HidReport)
    },

    .HidReportInEndpoint = {
        .bLength            = sizeof(Usb_EndpointDescriptor_t),
        .bDescriptorType    = 0x05,		// Endpoint
        .bEndpointAddress   = (USB_EP_DIR_IN | 1), // EP1.IN
        .bmAttributes       = 0x03,		// Interrupt
        .wMaxPacketSize     = 8,		// Must match EP1.IN buffer size
        .bInterval          = 10		// 10ms
    },
	
    .HidReportOutEndpoint = {
	    .bLength            = sizeof(Usb_EndpointDescriptor_t),
	    .bDescriptorType    = 0x05,     // Endpoint
	    .bEndpointAddress   = (USB_EP_DIR_OUT | 1), // EP1.OUT
	    .bmAttributes       = 0x03,     // Interrupt
	    .wMaxPacketSize     = 8,        // Must match EP1.OUT buffer size
	    .bInterval          = 10        // 10ms
    }
};

//*****************************************************************************
// String Descriptors
//*****************************************************************************
// String Index 0 - Language
const Usb_StringDescriptor_t LanguageString = {
	.bLength = 4,
	.bDescriptorType = 0x03,
	.bString = { 0x0409 }	// US English
};

// String Index 1 - Manufacturer
const Usb_StringDescriptor_t ManufacturerString = {
	.bLength = 2 + (sizeof(MANUFACTURER_STR) - 2),	// Subtract null bytes
	.bDescriptorType	= 0x03,
	.bString			= MANUFACTURER_STR
};

// String Index 2 - Product
const Usb_StringDescriptor_t ProductString = {
	.bLength = 2 + (sizeof(PRODUCT_STR) - 2),		// Subtract null bytes
	.bDescriptorType	= 0x03,
	.bString			= PRODUCT_STR
};

// String Index 3 - Serial Number
const Usb_StringDescriptor_t SerialNumberString = {
	.bLength = 2 + (sizeof(SERIAL_NUMBER_STR) - 2),	// Subtract null bytes
	.bDescriptorType	= 0x03,
	.bString			= SERIAL_NUMBER_STR
};

#endif
