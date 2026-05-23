<img width="647" height="863" alt="image" src="https://github.com/user-attachments/assets/df815d88-afc7-4808-b8c4-9355c1ac3ea3" />

A basic USB HID library for the Microchip AVRxxDU series of microcontrollers
- including Control Endpoint functions for: GET/SET HID Reports and IN/OUT Interrupt Endpoints.
- IN/OUT buffer is currently 2 bytes, but 8 bytes are possible with the Descriptor provided.

The prototype was developed on the AVR16DU14 version, using Microchip Studio 7 (see notes below) and the Cynthion USB packet analyzer (from Great Scott Gadgets).

The purpose of developing this library was for me to learn how to use the AVRDU series USB peripheral.  To simplify the code since Enumeration only occurs once after RESET and is very fast, enumeration logic occurs within the SETUP ISR routine.  Busy-wait loops are used for the IN and OUT Endpoint TRNCOMPL status checks. The USB enumeration state transitions are then easy to follow. 

To keep the project simple, I avoided developing my own device driver and chose to use the built-in HID drivers in Microsoft Windows.  By using the appropriate descriptor, Windows will automaticaly load the (HID) driver.  To communicate with the AVR once it is up-and-running, I used GenericHid.exe from http://janaxelson.com/ to execute Interrupt and Control Transfers. 

NOTE: It is possible to use other descriptors for various device types such as keyboard, mouse or CDC (e.g., serial port).

NOTE: When the Host performs the first Get_Descriptor request for the 18 byte Device_Descriptor, the Host is expecting just the first 8 bytes of the Device_Descriptor (on a Windows machine).  Once the Host has received the first 8 bytes, the Host does not request further packets and the device should consider this transaction complete.  There is no issue if the endpoint buffer size is greater than the descriptor length.  In multi-packet mode, with a BUFSIZE=8 or =16, an infinite busy-wait condition will occur in the function usbWaitInTransactionComplete() when the device descriptor is requested since the AVRDU device has no way of knowing that the host is not planning on requesting the remaining descriptor bytes.  The simple solution for me was to use a BUFSIZE > Device_Descriptor size (e.g., =32 or =64).

I also test the INTFLAGSA.RESET flag and if set, exit the wait loop.  This works to avoid complete lockup because the host will timeout if the device does not ACK the status stage within the time prescribed in the USB protocol.  

Notes on Microchip Studio 7 and Atmel ICE

a) To use the newer devices with the ATMEL-ICE, the ICE firmware needs to be updated to Version 1.2e.  Microchip Studio 7 support ended some time (years) ago but I prefer using it over MPLABX.  However, Studio no longer keeps the firmware packs up to date through the Device Pack Manager.  I found I had to install MPLABX from Microchip, create an empty main.c and load the program to force MPLABX to update the firmware (there might be a way to manually load the firmware pack directly into Studio 7).

b) I found a very annyoing feature of Microchip Studio is that it continually inspects HID devices.  I couldn't find any reason for this, but if you set the descriptor string indices to anything other than 0, you will find that Studio gets into a loop looking at your device (the setup packet printf shows a continuous roll of string requests). If you want string descriptors, you will need to quit Microchip Studio while testing.  Erk.  Microchip website suggests that they are moving to VS Code, so that may be the way to go eventually.
