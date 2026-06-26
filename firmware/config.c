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
#include <assert.h>
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
};

static bool config_validate(const struct global_config *config) {
	return (
		config->volume < GLOBAL_CONFIG_MAX_VALUE &&
		config->backlight < GLOBAL_CONFIG_MAX_VALUE &&
		config->contrast < GLOBAL_CONFIG_MAX_VALUE &&
		config->language < LANG_COUNT
	);
}

static uint32_t config_to_uint32(const void *config) {
	return *(uint32_t*)config;
}

void config_load(struct global_config *config) {
	assert(sizeof(struct global_config) == sizeof(uint32_t));

	// Look for a proper config backward from the end of the storage area to its beginning
	const size_t IDENTICAL_REQUIREMENT = 4;
	size_t identical_config_counter = 0; // Only take the config after reading the same one several times.
	memset(config, 0xFF, sizeof(struct global_config));
	for(uint32_t addr=FLASH_CONFIG_END-sizeof(uint32_t); addr>=FLASH_CONFIG_START; addr-=sizeof(uint32_t)) {
		uint32_t data = *(volatile uint32_t*)addr;
		if(data == 0xFFFFFFFF) {
			// Skip the special value 0xFFFFFFFF because it's the value we get for an erased page
			identical_config_counter = 0;
			continue;
		} else if(*(uint32_t*)config != data) {
			identical_config_counter = 1;
			memcpy(config, &data, sizeof(struct global_config));
		} else {
			if(++identical_config_counter >= IDENTICAL_REQUIREMENT && config_validate(config)) {
				// Found a proper config. We're done!
				return;
			}
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

	if(!config_validate(&new_config)) {
		// Config validation error. Report error. Not saving to flash.
		return false;
	}

	if(!memcmp(&old_config, &new_config, sizeof(old_config))) {
		// Config unchanged. Report success without saving to flash.
		return true;
	}

	// Look for the last empty page by searching backward from the end of the storage area to its beginning
	uint32_t page_addr;
	for(page_addr=FLASH_CONFIG_END-FLASH_PAGE_SIZE; page_addr>=FLASH_CONFIG_START; page_addr-=FLASH_PAGE_SIZE) {
		bool page_empty = true;
		for(uint32_t addr=page_addr; addr<page_addr+FLASH_PAGE_SIZE; addr+=sizeof(uint32_t)) {
			if(*((volatile uint32_t*)addr) != 0xFFFFFFFF) {
				page_empty = false;
				break;
			}
		}
		if(!page_empty) {
			// Obtained last non-empty page!
			break;
		}
	}
	page_addr += FLASH_PAGE_SIZE; // Add FLASH_PAGE_SIZE to obtain the last empty page address

	// Write the config to flash
	flash_unlock();
	if(page_addr >= FLASH_CONFIG_END) {
		// All pages filled
		// Erase the first page and fill in the first slot
		page_addr = FLASH_CONFIG_START;
		flash_erase_256(page_addr);
		flash_write_4x64(page_addr, config_to_uint32(&new_config));

		// Erase the remaining pages
		for(uint32_t page_addr=FLASH_CONFIG_START+FLASH_PAGE_SIZE; page_addr<FLASH_CONFIG_END; page_addr+=FLASH_PAGE_SIZE) {
			flash_erase_256(page_addr);
		}
	} else {
		flash_write_4x64(page_addr, config_to_uint32(&new_config));
	}
	flash_lock();

	return true;
}

static void config_dump(void) {
	printf("===CONFIG DUMP===\n");
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
	static struct global_config cfg = GLOBAL_CONFIG_DEFAULT;
	cfg.volume = (cfg.volume+1)%8;
	cfg.backlight = (cfg.backlight+3)%8;
	cfg.contrast = (cfg.contrast+5)%8;
	cfg.language = (cfg.language+1)%4;
	config_save(&cfg);
	config_dump();

	printf("Verification result: ");
	static struct global_config cfg2;
	config_load(&cfg2);
	if(memcmp(&cfg, &cfg2, sizeof(cfg))) {
		printf("FAIL | ");
	} else {
		printf("OK | ");
	}

	// The first kind of corruption should print "SAME". The second kind should print "DIFFERENT"
	uint32_t flash_corrupt[64] =
	//{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,};
	{0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,};
	config_load(&cfg2);

	uint32_t page_addr;
	for(page_addr=FLASH_CONFIG_END-FLASH_PAGE_SIZE; page_addr>=FLASH_CONFIG_START; page_addr-=FLASH_PAGE_SIZE) {
		bool page_empty = true;
		for(uint32_t addr=page_addr; addr<page_addr+FLASH_PAGE_SIZE; addr+=sizeof(uint32_t)) {
			if(*((volatile uint32_t*)addr) != 0xFFFFFFFF) {
				page_empty = false;
				break;
			}
		}
		if(!page_empty) {
			// Obtained last non-empty page!
			break;
		}
	}
	flash_unlock();
	flash_write_256(page_addr, flash_corrupt);
	flash_lock();
	config_load(&cfg2);
	if(memcmp(&cfg, &cfg2, sizeof(cfg))) {
		printf("DIFFERENT");
	} else {
		printf("SAME");
	}
	printf("\n");
}
