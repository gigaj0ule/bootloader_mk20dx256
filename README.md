Bootloader for Kinetis MK20DX256
====================

This is a simple open source bootloader for the Freescale Kinetis MK20DX256 microcontroller. It uses the USB Device Firmware Upgrade (DFU) standard.


Installing
----------

On a Teensy 3.2, you can use the Teensy Loader application to install `bootloader.hex`. Afterwards, your Teensy should appear to the system as a "Bootloader" USB device capable of loading an application firmware image.

If you want to upload the bootloader directly, this can be done with serial-wire-debug or JTAG, and openOCD.

Application Interface
---------------------

This section describes the programming interface that exists between the bootloader and the application firmware.

When entering the application firmware, the system clocks will already be configured, and the watchdog timer is already enabled with a 10ms timeout. The application may disable the watchdog timer if desired.

The bootloader normally transfers control to the application early in boot, before setting up the USB controller. It will skip this step and run the DFU implementation if any of the following conditions are true:

* Pin "BOOT_PIN" is in a LOW state
* A 32-bit entry token (0xDEADBEEF) is found at 0x2000_1FFC. (Programmatic entry)
* The application ResetVector does not reside within application flash. (No application is installed)

Memory address range       | Description
-------------------------- | ----------------------------
0x0000_0000 - 0x0000_1FFF  | Bootloader protected flash
0x0000_2000 - 0x0000_20F7  | Application IVT in flash
0x0000_2000 - 0x0002_FFFF  | Remainder of application flash
0x1FFF_E000 - 0x2000_1FFC  | Application SRAM
0x2000_1FFC - 0x2000_1FFF  | Entry token in SRAM


External Hardware
-----------------

No external hardware is required for the bootloader to operate. The following pins are used by the bootloader for optional features:


File Format
-----------

The DFU file consists of raw 64 byte blocks to be programmed into flash starting at address 0x0000_2000. The file may contain up to 248kB of data. No additional headers or checksums are included. On disk, the standard DFU suffix and CRC are used. During transit, the standard USB CRC is used.

