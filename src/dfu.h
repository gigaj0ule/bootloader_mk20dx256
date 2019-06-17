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

#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    appIDLE = 0,
    appDETACH,
    dfuIDLE,
    dfuDNLOAD_SYNC,
    dfuDNBUSY,
    dfuDNLOAD_IDLE,
    dfuMANIFEST_SYNC,
    dfuMANIFEST,
    dfuMANIFEST_WAIT_RESET,
    dfuUPLOAD_IDLE,
    dfuERROR
} dfu_state_t;

typedef enum {
    OK = 0,
    errTARGET,
    errFILE,
    errWRITE,
    errERASE,
    errCHECK_ERASED,
    errPROG,
    errVERIFY,
    errADDRESS,
    errNOTDONE,
    errFIRMWARE,
    errVENDOR,
    errUSBR,
    errPOR,
    errUNKNOWN,
    errSTALLEDPKT,
} dfu_status_t;

#define DFU_INTERFACE						0
#define DFU_DETACH_TIMEOUT					10000   // 10 second timer
#define DFU_TRANSFER_SIZE					64	// Ideally multiple of 64, and no more than flash sector size.
#define FLASH_SECTOR_SIZE					0x800
#define APP_ORIGIN							0x2000
#define P_FLASH_END							0x0003FFFF

// Flash commands
#define FTFL_CMD_READ_1S_BLOCK          	0x00
#define FTFL_CMD_READ_1S_SECTION        	0x01
#define FTFL_CMD_PROGRAM_CHECK          	0x02
#define FTFL_CMD_READ_RESOURCE          	0x03
#define FTFL_CMD_PROGRAM_LONG_WORD      	0x06
#define FTFL_CMD_ERASE_FLASH_BLOCK      	0x08
#define FTFL_CMD_ERASE_FLASH_SECTOR     	0x09
#define FTFL_CMD_PROGRAM_SECTOR         	0x0B
#define FTFL_CMD_READ_1S_ALL_BLOCKS     	0x40
#define FTFL_CMD_READ_ONCE              	0x41
#define FTFL_CMD_PROGRAM_ONCE           	0x43
#define FTFL_CMD_ERASE_ALL_BLOCKS       	0x44
#define FTFL_CMD_SET_FLEXRAM				0x81

#define FTFL_SET_FLEXRAM_FUNCTION			0x81
#define FTFL_CMD_SET_FLEXRAM_EEPROM			0x00
#define FTFL_CMD_SET_FLEXRAM_RAM			0xFF

#define FTFL_STAT_MGSTAT0 					0x01	// Error detected during sequence
#define FTFL_STAT_FPVIOL  					0x10	// Flash protection violation flag
#define FTFL_STAT_ACCERR  					0x20	// Flash access error flag
#define FTFL_STAT_CCIF    					0x80	// Command complete interrupt flag

#define FLASH_ALIGN(address, align) address = ((unsigned long)address & (~(align-1)))

// Main thread
void dfu_init();

// USB entry points. Always successful.
uint8_t dfu_getstate();

// USB entry points. True on success, false for stall.
bool dfu_getstatus(uint8_t *status);
bool dfu_clrstatus();
bool dfu_set_idle();
bool dfu_download(unsigned blockNum, unsigned blockLength, unsigned packetOffset, unsigned packetLength, const uint8_t *data);
bool dfu_upload(unsigned blockNum, uint16_t wLength, const uint8_t * data, uint32_t * returnedLength);

void flash_state_machine();