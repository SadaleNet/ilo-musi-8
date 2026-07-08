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

#include "chip8.h"
#include "crc.h"
#include "file.h"
#include "flash.h"
#include "generated.h"
#include "fatfs/ff.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

bool bootrom_verify_checksum(void) {
	asm volatile("" ::: "memory");
	// Initialize the checksum with 0xFFFFFFFF because the ROM's possible to be filled with many zeros,
	// which would lead to the CRC also to be zero.
	uint32_t crc = crc32_compute(CRC32_TABLE, 0xFFFFFFFF, (uint8_t*)FLASH_BOOTROM_START, FLASH_BOOTROM_CRC_START-FLASH_BOOTROM_START);
	return crc == *(uint32_t*)(FLASH_BOOTROM_CRC_START);
}

void bootrom_erase(void) {
	bool flash_cleared = true;
	for(size_t i=FLASH_BOOTROM_START; i<FLASH_BOOTROM_END; i+=sizeof(uint32_t)) {
		if(*(volatile uint32_t*)i != 0xFFFFFFFF) {
			flash_cleared = false;
			break;
		}
	}
	if(!flash_cleared) {
		flash_unlock();
		for(size_t i=FLASH_BOOTROM_START; i<FLASH_BOOTROM_END; i+=FLASH_SECTOR_SIZE) {
			flash_erase_1024(i);
		}
		flash_lock();
	}
}

uint8_t bootrom_program(const char *path, const struct chip8_config *chip8_cfg) {
	uint8_t ret;
	ret = file_program_bootrom(path, chip8_cfg, true);
	if(ret == FR_BOOTROM_VERIFICATION_ERROR) {
		// For dry_run, FR_BOOTROM_VERIFICATION_ERROR means that the bootrom doesn't match the supplied path and config
		// It means that we need to handle the actual bootrom flashing here
		bootrom_erase();
		flash_unlock();
		ret = file_program_bootrom(path, chip8_cfg, false);
		flash_lock();
		if(ret != FR_OK) {
			return ret;
		}
		if(!bootrom_verify_checksum()) {
			return FR_BOOTROM_VERIFICATION_ERROR;
		}
	}
	return ret;
}

bool bootrom_load(struct chip8_config *chip8_cfg, struct chip8_machine *chip8_machine) {
	if(!bootrom_verify_checksum()) {
		return false;
	}
	asm volatile("" ::: "memory");
	memcpy(chip8_cfg, (uint32_t*)FLASH_BOOTROM_CFG_START, sizeof(*chip8_cfg));

	// Don't care about return value for file_load_storage_flag() because the default fallback value is acceptable
	file_load_storage_flag(chip8_cfg->storage_flags, sizeof(chip8_cfg->storage_flags));
	chip8_init(chip8_machine, chip8_cfg);
	memcpy(&chip8_machine->mem[CHIP8_PROGRAM_START_OFFSET], (uint32_t*)FLASH_BOOTROM_CH8_START, FLASH_BOOTROM_CH8_END-FLASH_BOOTROM_CH8_START);
	return true;
}
