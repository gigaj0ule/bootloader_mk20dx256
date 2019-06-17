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
#include "mk20dx128.h"
#include "usb_dev.h"
#include "dfu.h"


// Internal flash-programming state machine
static uint32_t g_fl_block_base_addr = 0;
static uint16_t g_fl_block_longword_offset = 0;

static enum 
{
    flsIDLE = 0,
    flsBLOCKBEGIN,
    flsPROGRAMMING,
	flsCLEARCACHE,
	flsVERIFY,
} flash_state;

// DFU state machine
static dfu_state_t g_dfu_state = dfuIDLE;
static dfu_status_t g_dfu_status = OK;
static uint16_t g_dfu_poll_timeout = 1;

// Programming data buffer 
static uint8_t dfu_download_buffer[DFU_TRANSFER_SIZE];


static void *memcpy(void *dst, const void *src, size_t cnt) 
{
    uint8_t *dst8 = dst;
    const uint8_t *src8 = src;
    while (cnt > 0)
	{
        cnt--;
        *(dst8++) = *(src8++);
    }
    return dst;
}

static bool ftfl_busy()
{
    // Is the flash memory controller busy?
	return ((FTFL_FSTAT & FTFL_FSTAT_CCIF) != FTFL_FSTAT_CCIF);
}

static void ftfl_busy_wait()
{
    // Wait for the flash memory controller to finish any pending operation.
    while (ftfl_busy());
}

static void ftfl_launch_command()
{
    // Begin a flash memory controller command
	
	// Clear error flags
    FTFL_FSTAT = FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_RDCOLERR;
	// Launch command
    FTFL_FSTAT = FTFL_FSTAT_CCIF;
}

static void ftfl_begin_erase_sector(uint32_t sector_address)
{
	// Dont erase bootloader
	if(!(sector_address < APP_ORIGIN) || (sector_address >= (256 * 1024)))
	{				
		FTFL_FCCOB0 = FTFL_CMD_ERASE_FLASH_SECTOR;
		
		FTFL_FCCOB1 = (unsigned char)((sector_address) >> 16);
		FTFL_FCCOB2 = (unsigned char)((sector_address) >> 8);
		FTFL_FCCOB3 = (unsigned char)(sector_address);
		ftfl_launch_command();
	}
}

static bool ftfl_begin_program_long_word(uint32_t write_address, uint8_t flash_data_0, uint8_t flash_data_1, uint8_t flash_data_2, uint8_t flash_data_3)
{
	if((write_address < APP_ORIGIN) || write_address >= (256 * 1024))
	{			
		return true;
	}
	else
	{
		FTFL_FCCOB0 = FTFL_CMD_PROGRAM_LONG_WORD;
		
		FTFL_FCCOB1 = (unsigned char)((write_address) >> 16) & 0xFF;
		FTFL_FCCOB2 = (unsigned char)((write_address) >> 8) & 0xFF;
		FTFL_FCCOB3 = (unsigned char)(write_address) & 0xFF;
		
		FTFL_FCCOB4 = (unsigned char)(flash_data_0); // Byte at flashAddress		(MSB of longword)
		FTFL_FCCOB5 = (unsigned char)(flash_data_1); // Byte at flashAddress + 1
		FTFL_FCCOB6 = (unsigned char)(flash_data_2); // Byte at flashAddress + 2
		FTFL_FCCOB7 = (unsigned char)(flash_data_3); // Byte at flashAddress + 3  (LSB of longword)
		ftfl_launch_command();
		return false;
	}
}

static uint32_t flash_address_from_wBlockNum(uint16_t wBlockNum)
{
    return APP_ORIGIN + (DFU_TRANSFER_SIZE * wBlockNum);
}

void dfu_init()
{
	flash_state = flsIDLE;
}

uint8_t dfu_getstate()
{
    return g_dfu_state;
}

bool dfu_download(unsigned wBlockNum, unsigned wLength, unsigned packetOffset, unsigned packetLength, const uint8_t *data)
{
    if (packetOffset + packetLength > DFU_TRANSFER_SIZE || packetOffset + packetLength > wLength) 
	{
        // Overflow!
        g_dfu_state = dfuERROR;
        g_dfu_status = errADDRESS;
        return false;
    }

    // Store more data...
    memcpy(dfu_download_buffer + packetOffset, data, packetLength);

    if (packetOffset + packetLength != wLength) 
	{
        // Still waiting for more data.
        return true;
    }

    if (g_dfu_state != dfuIDLE && g_dfu_state != dfuDNLOAD_IDLE) 
	{
        // Wrong state! Oops.
        g_dfu_state = dfuERROR;
        g_dfu_status = errSTALLEDPKT;
        return false;
    }

    if (ftfl_busy() || flash_state != flsIDLE) 
	{
        // Flash controller shouldn't be busy now!
        g_dfu_state = dfuERROR;
        g_dfu_status = errUNKNOWN;
        return false;       
    }

    if (!wLength) 
	{
        // End of download
        g_dfu_state = dfuMANIFEST_SYNC;
        g_dfu_status = OK;
        return true;
    }

    // Start programming a DFU block in flash
    flash_state = flsBLOCKBEGIN;
    g_fl_block_base_addr = flash_address_from_wBlockNum(wBlockNum);
		
	// Only erase when address pointer is beginning of a sector
	if((g_fl_block_base_addr % FLASH_SECTOR_SIZE) == 0)
	{
		ftfl_begin_erase_sector(g_fl_block_base_addr);
	}
	
    g_dfu_state = dfuDNLOAD_SYNC;
    g_dfu_status = OK;
    return true;
}

bool dfu_upload(unsigned wBlockNum, uint16_t expected_wLength, const uint8_t * output_buffer, uint32_t * returned_wLength)
{
	// Get memory address
	uint32_t *flash_address = flash_address_from_wBlockNum(wBlockNum);

	// Copy data from flash to output buffer
	memcpy(output_buffer, flash_address, expected_wLength);
	
	if(flash_address > (P_FLASH_END - APP_ORIGIN))
	{
		// Yes
		*returned_wLength = 0;
		g_dfu_state = dfuIDLE;
		g_dfu_status = OK;
		return false;
	}
	else
	{
		// No
		*returned_wLength = expected_wLength;
		g_dfu_state = dfuUPLOAD_IDLE;
		g_dfu_status = OK;
		return true;
	}
}

static bool fl_handle_status(uint8_t fstat, uint16_t specific_error)
{
    /*
     * Handle common errors from an FSTAT register value.
     * The indicated "specificError" is used for reporting a command-specific
     * error from MGSTAT0.
     *
     * Returns true if handled, false if not.
     */

    if (0 == (fstat & FTFL_FSTAT_CCIF)) 
	{
        // Still working...
        return true;
    }

    if (fstat & FTFL_FSTAT_RDCOLERR) 
	{
        // Bus collision. We did something wrong internally.
        g_dfu_state = dfuERROR;
        g_dfu_status = errUNKNOWN;
        flash_state = flsIDLE;
        return true;
    }

	if (fstat & FTFL_FSTAT_FPVIOL) 
	{
		// Protection error
		g_dfu_state = dfuERROR;
		g_dfu_status = errADDRESS;
		flash_state = flsIDLE;
		return true;
	}

    if (fstat & FTFL_FSTAT_ACCERR) 
	{
        // Write error
        g_dfu_state = dfuERROR;
        g_dfu_status = errWRITE;
        flash_state = flsIDLE;
        return true;
    }

    if (fstat & FTFL_FSTAT_MGSTAT0) 
	{
        // Command-specifid error
        g_dfu_state = dfuERROR;
        g_dfu_status = specific_error;
        flash_state = flsIDLE;
        return true;
    }

    return false;
}

// Try to advance our flash programming state machine.
void flash_state_machine()
{
    uint8_t fstat = FTFL_FSTAT;
	uint32_t flash_address = g_fl_block_base_addr + g_fl_block_longword_offset;
	
    switch (flash_state) 
	{
        case flsIDLE:
			// Nothing to do here
            break;

        case flsBLOCKBEGIN:
            if (!fl_handle_status(fstat, errERASE) && !ftfl_busy()) 
			{
                // Erasing done, now move on to programming the flash.
                flash_state = flsPROGRAMMING;
				g_fl_block_longword_offset = 0;
            }
            break;

        case flsPROGRAMMING:
			// Continue as long as no flash errors
			if(!fl_handle_status(fstat, errVERIFY))
			{
				if(!ftfl_busy())
				{								
					// This script may write garbage at the end of the application if the last DFU transfer is < 64 bytes...
					// Might not be an issue since that flash is never accessed anyway
					if(g_fl_block_longword_offset < DFU_TRANSFER_SIZE)
					{	
						uint8_t flash_data_0 = dfu_download_buffer[g_fl_block_longword_offset + 0x03];
						uint8_t flash_data_1 = dfu_download_buffer[g_fl_block_longword_offset + 0x02];
						uint8_t flash_data_2 = dfu_download_buffer[g_fl_block_longword_offset + 0x01];
						uint8_t flash_data_3 = dfu_download_buffer[g_fl_block_longword_offset + 0x00];
													
						if(ftfl_begin_program_long_word(flash_address, flash_data_0, flash_data_1, flash_data_2, flash_data_3))
						{
							// Exception occurred, memory address out of bounds
							g_dfu_state = dfuERROR;
							g_dfu_status = errWRITE;							
						}
						else
						{
							// Continue if no exception
							g_fl_block_longword_offset += 4;
						}
					}
					else if(g_fl_block_longword_offset >= DFU_TRANSFER_SIZE)
					{
						// Programming done, begin verify
						flash_state = flsCLEARCACHE;
						break;
					}
				}
			}
            break;
			
			case flsCLEARCACHE:
			{
				FMC_PFB0CR |= 0b00000011100000000000000000000000;
				flash_state = flsVERIFY;
			}
			break;
			
			case flsVERIFY:
			{
				if(!ftfl_busy())
				{
					// Verify the last written block and toss exception if failed
					uint8_t test_buffer[DFU_TRANSFER_SIZE];
					memcpy(&test_buffer, g_fl_block_base_addr, DFU_TRANSFER_SIZE);
					
					// I have no idea why, but the first 16 bytes take a long time to stabilize.
					// I suspect it is a bug in the kinetis FFTL controller.
					for(int i = 0; i < DFU_TRANSFER_SIZE; i++)
					{
						if(test_buffer[i] != dfu_download_buffer[i])
						{
							g_dfu_state = dfuERROR;
							g_dfu_status = errVERIFY;
							flash_state = flsIDLE;
							break;
						}
					}
					
					// If no error, set state to done
					flash_state = flsIDLE;					
					break;
				}
			}
			break;
    }
}

bool dfu_getstatus(uint8_t *status)
{
    switch (g_dfu_state) {

        case dfuDNLOAD_SYNC:
        case dfuDNBUSY:
            // Programming operation in progress. 

            if (g_dfu_state == dfuERROR) 
			{
              // An error occurred inside fl_state_poll();
            } 
			else if (flash_state == flsIDLE) 
			{
                g_dfu_state = dfuDNLOAD_IDLE;
            } 
			else 
			{
                g_dfu_state = dfuDNBUSY;
            }
            break;

        case dfuMANIFEST_SYNC:
            // Ready to reboot. The main thread will take care of this. Also let the DFU tool
            // know to leave us alone until this happens.
            g_dfu_state = dfuMANIFEST;
            g_dfu_poll_timeout = 1000;
            break;

        default:
            break;
    }

    status[0] = g_dfu_status;
    status[1] = g_dfu_poll_timeout;
    status[2] = g_dfu_poll_timeout >> 8;
    status[3] = g_dfu_poll_timeout >> 16;
    status[4] = g_dfu_state;
    status[5] = 0;  // iString

    return true;
}

bool dfu_clrstatus()
{
    switch (g_dfu_state) {

        case dfuERROR:
            // Clear an error
            g_dfu_state = dfuIDLE;
            g_dfu_status = OK;
            return true;

        default:
            // Unexpected request
            g_dfu_state = dfuERROR;
            g_dfu_status = errSTALLEDPKT;
            return false;
    }
}

bool dfu_set_idle()
{
    g_dfu_state = dfuIDLE;
    g_dfu_status = OK;
    return true;
}

