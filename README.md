A basic USB HID library for the Microchip AVRxxDU series of microcontrollers.  

The prototype was developed on the AVR16DU14 version, using Microchip Studio 7 (see notes below)

The purpose of this library is to facilitate the understanding of the AVRDU series USB peripheral.  To simplify things, the library uses busy-wait loops for the IN and OUT Endpoint TRNCOMPL status check.  This approach blocks the ISR briefly, but there is sufficient tolerance in the USB spec to allow the loop delay.  The result of this approach is the setup and use of the library is very simple.

In addition, to keep things as simple as possible, I chose to use the built-in HID drivers in Microsoft Windows.  By using the appropriate descriptors in the project, Windows will automaticaly load the driver.  The simple example uses the Vendor-Defined profile.  To check it out, you can use GenericHid.exe from http://janaxelson.com/.

One issue that does occur is when the Host performs the first Get_Descriptor request for the 18 byte Device_Descriptor.  The Host request is for 64 bytes but the Host only wants the first 8 bytes of the 18 byte descriptor.  Once the Host has received those first 8 bytes, the Host does not request any further packets on this request.  In multi-packet mode, with a BUFSIZE=8 (or =16), a forever busy-wait state will occur in the function usbWaitInTransactionComplete() when CNT=18, since TRNCOMPL will never assert - in the Host-driven USB protocol, the AVR will wait for the Host to request the remaining bytes before asserting.  The workaound is to make BUFSIZE=32 or =64.

Notes on Microchip Studio 7 and Atmel ICE

a) To use the newer devices with the ATMEL-ICE, the firmware needs to be updated to Version 1.2e.  Microchip Studio support ended some time ago but Studio 7 is still very useful.  However, to update the firmware I found I had to install MPLABX from Microchip and create an empty main.c to force MPLABX to update the firmware.  There might be a way to force Microchip Studio to do that by downloading the correct device packs from Microchip.com.

b) I found a very annyoing "feature" of Microchip Studio in that it continually inspects HID devices.  I couldn't find any reason for this, but if you set the descriptor string indices to anything other than 0, you will find that Studio gets into a loop looking at your device!  If you want the string descriptors, you will need to quite Microchip Studio while testing.  Erk.  Microchip (as of 2026) suggests that they are moving their existing tools to VS, so we'll see what that brings.
