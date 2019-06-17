/*
 * MK20DX256 DFU Bootloader
 * 
 * Copyright (c) 2013 Micah Elizabeth Scott
 * Copyright (c) 2018 Adam Munich
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdbool.h>
#include "kinetis.h"
#include "dfu.h"
#include "usb_dev.h"
//#include "serial.h"
#include "core_pins.h"
#include "led_functions.h"

//#define BOOT_PIN 3
#define BOOT_PIN 32

extern uint32_t boot_token;
static __attribute__ ((section(".applicationInterruptVectors"))) uint32_t applicationInterruptVectors[NVIC_NUM_INTERRUPTS+16];

static bool test_boot_token()
{
    /*
     * If we find a valid boot token in RAM, the application is asking us explicitly
     * to enter DFU mode. This is used to implement the DFU_DETACH command when the app
     * is running.
     */

    return boot_token == 0xDEADBEEF;
}

static bool test_app_missing()
{
    /*
     * If there doesn't seem to be a valid application installed, we always go to
     * bootloader mode.
     */

    uint32_t ivt_entry_point = applicationInterruptVectors[1];
	
	// Returns true if the contents of 1st address of the vector table (which is the arm program counter)
	// point to an address in the bootloader ( < APP_ORIGIN ) or return true if the contents point to an 
	// address outside the application flash area (eg, 0xFFFFFFFF).
	
	// Granted this relies in the linker actually respecting our request to place applicationInterruptVectors[0]
	// at APP_ORIGIN so maybe it's risky...
	
    return (ivt_entry_point < APP_ORIGIN) || (ivt_entry_point >= (256 * 1024));
}

static bool test_boot_pin_low()
{
	pinMode(BOOT_PIN, INPUT_PULLUP);
	
    for (int j = 1000; j; --j)
    {
		__asm__ volatile ("nop");;
    }
		
	if(digitalRead(BOOT_PIN) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void app_launch()
{
    // Relocate IVT to application flash
    __disable_irq();
    SCB_VTOR = (uint32_t) &applicationInterruptVectors[0];

    // Clear the boot token, so we don't repeatedly enter DFU mode.
    boot_token = 0;

    asm volatile (
	
		// Fill the return address with 0xFFFFFFFF (we should never return to bootloader)
        "mov lr, %0 \n\t"
		
		// Set the stack pointer to Application IVT entry 0
        "mov sp, %1 \n\t"
		
		// Branch to program counter in Application IVT entry 1
        "bx %2 \n\t"
		
		// Corresponding data
        : : "r" (0xFFFFFFFF),
            "r" (applicationInterruptVectors[0]),
            "r" (applicationInterruptVectors[1]) );
}


int main()
{	
    uint32_t i = 0;
    led_init();

    // LED helps us see what's happening, stays on during download, blinks otherwise
    while(1) {
    if ((i % 100000) == 0)
    {
        led_clear();
    }
    if ((i % 1000000) == 0)
    {
        led_set();
    }
    i++;
    }
			

    if (test_app_missing() || test_boot_token() || test_boot_pin_low()) {

        // Oh boy we're doing DFU mode!
        uint32_t i = 0;
		uint32_t j = 0;
		uint8_t dfu_returned_state = 1;

        led_init();
        dfu_init();
        usb_init();

        // Now we're ready for DFU download
        while (dfu_returned_state != dfuMANIFEST)
		{
			dfu_returned_state = dfu_getstate();
			
			// LED helps us see what's happening, stays on during download, blinks otherwise
            if ((i % 10000) == 0)
			{
				if(!(dfu_returned_state == dfuDNBUSY || dfu_returned_state == dfuDNLOAD_SYNC || dfu_returned_state == dfuDNLOAD_IDLE))
				{
					led_clear();
				}
            }
            if ((i % 1000000) == 0)
			{
				led_set();
			}
			
			// Poll the state machine hella fast or otherwise download will lock up
			// I think it needs to run faster than the USB S.O.F. by some margin
            for (j = 10000; j; --j) 
			{
				i++;
				flash_state_machine();
            }
        }
		
		// Ack DFU download (ideally we should test to see if valid IVF is in memory and fail if not)
		dfu_set_idle();
		
        // Clear boot token, to enter the new application
        boot_token = 0;

        // USB disconnect and reboot
        __disable_irq();
        USB0_CONTROL = 0;

		// Wait a few seconds because the flash won't stabilize otherwise.
		// /despite/ the CCIF register saying it's all dandy. WHAT THE ACTUAL FUCK.
        // Flash the LED super quickly to let user know DFU download is complete.
        for (i = 100; i; --i)
		{
            led_toggle();
            for (j = 1000000; j; --j)
			{
				__asm__ volatile ("nop");;
            }
        }
		
		// Force reboot by invalid write to WDOG_REFRESH
		// Any invalid write to the WDOG registers will trigger an immediate reboot
		WDOG_REFRESH = 0;
		
		// We should never get here but if we do, hang.
        while (1);
   }

    app_launch();
	while (1);
}
