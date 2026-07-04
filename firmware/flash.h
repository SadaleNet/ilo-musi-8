// Copyright 2026 Wong Cho Ching <https://sadale.net>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <stdint.h>

// Flash layout:
// Usable flash area: [0x0800E000,0x0800F800)
// Global config: [0x0800E000,0x0800E800)
// Boot rom (CHIP8 game rom, game config and checksum): [0x0800E800,0x0800F800)

#define FLASH_PAGE_SIZE (256)
#define FLASH_SECTOR_SIZE (1024) // The reference manual said that it's "1K page". I invented the name "sector" for that.

#define FLASH_USER_START (0x800E000) // Inclusive
#define FLASH_USER_END (0x800F800) // Inclusive
	// Sub-items of USER area above
	#define FLASH_CONFIG_START (0x800E000) // Inclusive
	#define FLASH_CONFIG_END (0x0800E800) // Exclusive
	#define FLASH_BOOTROM_START (0x800E800) // Inclusive
	#define FLASH_BOOTROM_END (0x800F800) // Exclusive
		// Sub-items of BOOTROM above
		#define FLASH_BOOTROM_CH8_START (0x800E800) // Inclusive
		#define FLASH_BOOTROM_CH8_END (0x800F600) // Exclusive
		#define FLASH_BOOTROM_CFG_START (0x800F600) // Inclusive
		#define FLASH_BOOTROM_CFG_END (0x800F7FC) // Exclusive
		#define FLASH_BOOTROM_CRC_START (0x800F7FC) // Inclusive
		#define FLASH_BOOTROM_CRC_END (0x800F800) // Exclusive



void flash_unlock(void);
void flash_lock(void);

// According to the customer support of CH32V006, after erasing a page, you can only write the page once
void flash_erase_1024(uint32_t offset); // Erase 1024 bytes
void flash_erase_256(uint32_t offset); // Erase 256 bytes
void flash_write_256(uint32_t offset, const void *data); // Write 256 bytes. Do not call more than once after an erase.
void flash_write_4x64(uint32_t offset, const uint32_t data); // Write 4 bytes repeatedly. Do not call more than once after an erase.
