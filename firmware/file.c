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
#include "adc.h"
#include "lcd.h"
#include "spi.h"
#include "fatfs/ff.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static FATFS filesystem;
static FRESULT mount_result;

static FRESULT mount_filesystem(void) {
	return f_mount(&filesystem, "", 1);
}

static bool file_card_mode_enter(void) {
	if(mount_result != FR_OK) {
		// If not mounted, give it a chance to mount right now!
		if(adc_card_is_inserted()) {
			while(lcd_is_transfer_in_progress()){}
			spi_set_mode(SPI_MODE_MEMORY_CARD_SLOW);
			mount_result = mount_filesystem();
		}
		if(mount_result != FR_OK) {
			return false;
		}
	}

	while(lcd_is_transfer_in_progress()){}
	spi_set_mode(SPI_MODE_MEMORY_CARD);
	return true;
}

static void file_card_mode_exit(void) {
	spi_set_mode(SPI_MODE_LCD);
}

void file_first_mount(void) {
	mount_result = FR_NOT_READY;
	if(adc_card_has_insert_event()) {
		// Must be the first SPI operation to run
		// spi_set_mode(SPI_MODE_MEMORY_CARD_SLOW); // No need. That's because it's same as the initial state
		mount_result = mount_filesystem();
		adc_card_reset_insert_event();
	}
}

void file_loop(void) {
	// Handle card reinsertion. Must initialize the card before any LCD SPI communication
	// In practice, if LCD SPI communicaition is on-going, it won't stop until the row's sent
	// so there might be a bit of delay of SPI initialization for the card
	if(adc_card_is_just_removed()) {
		mount_result = FR_NOT_READY;
	}
	if(adc_card_has_insert_event()) {
		while(lcd_is_transfer_in_progress()){}
		spi_set_mode(SPI_MODE_MEMORY_CARD_SLOW);
		mount_result = mount_filesystem();
		spi_set_mode(SPI_MODE_LCD);
		adc_card_reset_insert_event();
	}
}

FRESULT file_load_rom(const char *path, struct chip8_machine *chip8_machine) {
	if(!file_card_mode_enter()) {
		return mount_result;
	}
	
	static const struct chip8_config chip8_cfg = {
		.font = {
			0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
			0x20, 0x60, 0x20, 0x20, 0x70, // 1
			0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
			0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
			0x90, 0x90, 0xF0, 0x10, 0x10, // 4
			0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
			0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
			0xF0, 0x10, 0x20, 0x40, 0x40, // 7
			0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
			0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
			0xF0, 0x90, 0xF0, 0x90, 0x90, // A
			0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
			0xF0, 0x80, 0x80, 0x80, 0xF0, // C
			0xE0, 0x90, 0x90, 0x90, 0xE0, // D
			0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
			0xF0, 0x80, 0xF0, 0x80, 0x80, // F
		},
		.font_highres = {
			0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 0
			0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
			0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // 2
			0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 3
			0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, // 4
			0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 5
			0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 6
			0xFF, 0xFF, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
			0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 8
			0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 9
			0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, // A
			0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, // B
			0x3C, 0xFF, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0xFF, 0x3C, // C
			0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC, // D
			0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // E
			0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0  // F
		},
		// 1000Hz squarewave, pulse width: 4 samples, 50% duty cycle
		.audio = {0xCCCCCCCC, 0xCCCCCCCC, 0xCCCCCCCC, 0xCCCCCCCC},
		.storage_flags = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.quirks = (CHIP8_QUIRK_WRAP|CHIP8_QUIRK_LORES_WIDE_SPRITE|CHIP8_QUIRK_RESIZE_CLEAR_SCREEN)
	};
	chip8_init(chip8_machine, &chip8_cfg);

	FIL fil;
	FRESULT ret;
	UINT bytesread;
	ret = f_open(&fil, path, FA_READ);
	if(ret == FR_OK) {
		ret = f_read(&fil, &chip8_machine->mem[CHIP8_PROGRAM_START_OFFSET], CHIP8_MEMORY_SIZE-CHIP8_PROGRAM_START_OFFSET, &bytesread);
		f_close(&fil);
	}

	file_card_mode_exit();
	return ret;
}

FRESULT file_readdir(const char *path, size_t offset, char (*filelist)[14], size_t *count) {
	if(!file_card_mode_enter()) {
		return mount_result;
	}

	size_t listed_file_count = 0;
	size_t required_count = *count;
	size_t fulfilled_count = 0;
	DIR dir;
	FRESULT ret;
    FILINFO fileinfo;
    ret = f_opendir(&dir, path);
    if(ret == FR_OK) {
		while(true) {
			ret = f_readdir(&dir, &fileinfo);
			if(ret != FR_OK) { // Error detected!
				break;
			}
			if(fileinfo.fname[0] == '\0') { // End of directory
				break;
			}
			if(listed_file_count >= offset) {
				memcpy(filelist[fulfilled_count], fileinfo.fname, sizeof(fileinfo.fname));
				// For directory, attach slash to the end of the filename
				if(fileinfo.fattrib & AM_DIR) {
					// Max length of 8.3 filename is 8+1+3 = 12
					size_t endpos = strlen(filelist[fulfilled_count]);
					filelist[fulfilled_count][endpos] = '/';
					filelist[fulfilled_count][endpos+1] = '\0';
				}
				fulfilled_count++;
			}
			listed_file_count++;
			if(fulfilled_count >= required_count) { // Enough entries got read
				break;
			}
		}
		f_closedir(&dir);
	}
	*count = fulfilled_count;

	file_card_mode_exit();
	return ret;
}
