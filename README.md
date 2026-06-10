# MMBasic Port for CardputerADV

Micromite BASIC is the best BASIC interpreter I have ever used on a development board.

I had a lot of fun playing with it on the Raspberry Pi Pico, so I spent some time porting MMBasic to the CardputerADV — now you can enjoy BASIC directly on your Cardputer!

## About MMBasic
MMBasic was created and refined over many years by **Geoff Graham**. It is a full-featured, powerful BASIC interpreter used across a wide range of platforms, including the Maximite and Micromite families.  
Official website and original documentation: [http://mmbasic.com](http://mmbasic.com)

## About This Port
This project is a port of MMBasic to the CardputerADV platform. Since it is a port, I have not changed the name of the project — it is called **MMBasic port for CardputerADV**.

## Copyright Notice
This ported firmware retains the original copyright notice of MMBasic. All copyrights for MMBasic belong to the original author, **Geoff Graham**.  
This project provides only the compiled `.bin` firmware; no source code is included.

## What's Working So Far
- Keyboard input
- Screen display
- SD card read/write
- The most important part: the core interpreter

## Tested
- Common BASIC functions
- File operations

## Not Tested
- GPIO: Neither the code nor the hardware has been tested, so the GPIO code is provided with no guarantees at this time.

## Planned Updates
- Bluetooth keyboard support may be added in a future version? However, considering the Cardputer's small built-in screen, I'm still not sure if an external Bluetooth keyboard is really necessary...
- GPIO: I need to learn more about this area, so it will take quite a while.

## Acknowledgements
A huge thank you to **Geoff Graham** for creating MMBasic and allowing me to port it to this platform.

---

# License for the Firmware Binary

This firmware is a port of MMBasic for CardputerADV.  
MMBasic is copyright (c) Geoff Graham. All rights reserved.

- This binary is provided free of charge for personal, non-commercial use.
- You may not reverse engineer, decompile, or disassemble the firmware.
- You may not redistribute modified versions of this binary.
- The original copyright notice must be retained and not altered.

For any other use, please contact Geoff Graham.
