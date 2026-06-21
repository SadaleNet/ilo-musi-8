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

#include "config.h"
#include "flash.h"
#include "ch32fun.h" // TODO: remove
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

const struct global_config GLOBAL_CONFIG_DEFAULT = {
	.version = GLOBAL_CONFIG_VERSION,
	.volume = 7,
	.backlight = 7,
	.contrast = 3,
	.language = 0,
	.checksum = 0, // Does not need a valid checksum. Computed upon config save.
};


static uint8_t config_calculate_checksum(const struct global_config *config) {
	uint8_t ret = 0xAA;
	for(size_t i=0; i<sizeof(struct global_config); i++) {
		ret ^= ((const uint8_t*)config)[i];
	}
	return ret;
}

static bool config_validate(const struct global_config *config) {
	return true;
	/*return (
		config->volume <= 7 &&
		config->backlight <= 7 &&
		config->constrast <= 7 &&
		config->language <= 3
	);*/ // TODO: determine the exact range I need
}

static bool config_verify_config_slot(uint32_t start_offset, const struct global_config *config) {
	if(*((volatile uint32_t*)start_offset) != *(uint32_t*)config) {
		return false;
	}
	for(uint32_t addr=start_offset+4; start_offset<FLASH_CONFIG_END; start_offset+=sizeof(uint32_t)) {
		if(*((volatile uint32_t*)addr) != 0xFFFFFFFF) {
			return false;
		}
	}
	return true;
}

static uint32_t config_to_uint32(const void *config) {
	return *(uint32_t*)config;
}

void config_load(struct global_config *config) {
	// Look for a proper config backward from the end of the storage area to its beginning
	for(uint32_t addr=FLASH_CONFIG_END-sizeof(uint32_t); addr>=FLASH_CONFIG_START; addr-=sizeof(uint32_t)) {
		*config = *((struct global_config*)((volatile uint32_t*)addr));
		if(config_calculate_checksum(config) == config->checksum && config_validate(config)) {
			// Found a proper config. We're done!
			return;
		}
	}
	// A proper config hasn't been found in flash
	// Resorts using a default one
	*config = GLOBAL_CONFIG_DEFAULT;
}

bool config_save(const struct global_config *config) {
	struct global_config old_config, new_config;
	config_load(&old_config);
	new_config = *config;
	new_config.checksum = config_calculate_checksum(config);

	if(!config_validate(&new_config)) {
		// Config validation error. Report error. Not saving to flash.
		return false;
	}

	if(!memcmp(&old_config, &new_config, sizeof(old_config))) {
		// Config unchanged. Report success without saving to flash.
		return true;
	}

	// Look for the last empty config slot by searching backward from the end of the storage area to its beginning
	uint32_t addr;
	for(addr=FLASH_CONFIG_END-sizeof(uint32_t); addr>=FLASH_CONFIG_START; addr-=sizeof(uint32_t)) {
		if(*((volatile uint32_t*)addr) != 0xFFFFFFFF) {
			break; // Obtained last non-empty address
		}
	}
	addr += sizeof(uint32_t); // Add 4 to obtain the last empty address
	printf("%08lX %08lX %08lX %08lX\n", addr, FLASH->STATR, FLASH->CTLR, FLASH->ACTLR);

	// Write the config to flash
	flash_unlock();
	if(addr >= FLASH_CONFIG_END) {
		// All pages filled
		// Erase the first page and fill in the first slot
		addr = FLASH_CONFIG_START;
		flash_erase_256(addr);
		flash_write_4(addr, config_to_uint32(&new_config));

		// Erase the remaining pages
		for(uint32_t addr=FLASH_CONFIG_START+FLASH_PAGE_SIZE; addr<FLASH_CONFIG_END; addr+=FLASH_PAGE_SIZE) {
			flash_erase_256(addr);
		}
	} else {
		flash_write_4(addr, config_to_uint32(&new_config));
	}
	flash_lock();

	// Verify flash content
	if(!config_verify_config_slot(addr, &new_config)) {
		return false;
	}
	return true;
}

static void config_dump(void) {
	for(uint32_t addr=FLASH_CONFIG_START; addr<FLASH_CONFIG_END; addr+=sizeof(uint32_t)) {
		printf("%08lX ", *((volatile uint32_t*)addr));
		if(addr%64 == 64-sizeof(uint32_t)) {
			printf("\n");
		}
		if(addr%FLASH_PAGE_SIZE == FLASH_PAGE_SIZE-sizeof(uint32_t)) {
			printf("\n");
		}
	}
}

void config_test(void) {
	config_save(&GLOBAL_CONFIG_DEFAULT);
	config_dump();
}
