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
#include "bulkmem.h"
#include "adc.h"
#include "file.h"
#include "lcd.h"
#include "spi.h"
#include "util.h"
#include "fatfs/ff.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define FLASH_FILE "ILOMUSI8.BIN"
#define FLASH_START_OFFSET (0x08000000)
#define FLASH_END_OFFSET (0x0800F800)

#define STORAGE_FLAG_FILE "FX75FX85.BIN"

static const struct chip8_config CHIP8_CFG_DEFAULT = {
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
	.audio = CHIP8_DEFAULT_AUDIO_SAMPLE,
	.storage_flags = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.quirks = CHIP8_QUIRK_PLATFORM_OCTO,
	.speed = 0,
	.input_layout = 0,
	.input_navigation = 0,
	.input_action = 0,
	.input_replay = 0,
};

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

enum ini_state {
	PARSING_KEY,
	PARSING_VALUE,
	PARSING_TRAILING,
};

enum ini_key {
	PARSING_INI_QUIRKS,
	PARSING_INI_SPEED,
	PARSING_INI_AUDIO,
	PARSING_INI_FONT,
	PARSING_INI_FONT_LARGE,
	PARSING_INI_LAYOUT,
	PARSING_INI_NAVIGATION,
	PARSING_INI_ACTION,
	PARSING_INI_REPLAY,
};

struct ini_parser {
	enum ini_state state;
	char parsing_key[16];
	size_t parse_index;
	enum ini_key parsing_key_type;
	struct chip8_config *output_cfg;
};

static uint8_t file_parse_ini_hex2uint(char c) {
	if(c >= '0' && c <= '9') {
		return c - '0';
	} else if(c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if(c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	} else {
		return UINT8_MAX;
	}
}

static bool file_parse_dec_uint8(uint8_t *output_value, size_t parse_index, char c) {
	if(parse_index >= 2) {
		return false;
	}
	if(c >= '0' && c <= '9') {
		*output_value *= 10;
		*output_value += c - '0';
	} else {
		return false;
	}
	return true;
}

static bool file_parse_buffer(void *buffer, size_t length, size_t parse_index, char c) {
	if(parse_index >= length*2) {
		return false;
	}
	uint8_t value = file_parse_ini_hex2uint(c);
	if(value >= 16) {
		return false;
	}
	((uint8_t*)buffer)[parse_index/2] |= value << (parse_index%2 ? 0 : 4);
	return true;
}

static bool file_parse_input_button(uint16_t *output_value, char c) {
	uint8_t value = file_parse_ini_hex2uint(c);
	if(value >= 16) {
		return false;
	}
	*output_value |= (1 << value);
	return true;
}

static bool file_parse_ini(struct ini_parser *parser, const char *buffer, size_t length) {
	for(size_t i=0; i<length; i++) {
		char c = buffer[i];
		if(c == '\r' || c == '\n') {
			parser->state = PARSING_KEY;
			memset(parser->parsing_key, '\0', sizeof(parser->parsing_key));
			parser->parse_index = 0;
			continue;
		}
		switch(parser->state) {
			case PARSING_KEY:
				switch(c) {
					case ' ': break; // skip. do nothing
					case '=':
						parser->state = PARSING_VALUE;
						parser->parse_index = 0;
						if(!strcmp(parser->parsing_key, "quirks") || !strcmp(parser->parsing_key, "nasin")) {
							parser->parsing_key_type = PARSING_INI_QUIRKS;
							parser->output_cfg->quirks = 0;
						} else if(!strcmp(parser->parsing_key, "speed") || !strcmp(parser->parsing_key, "tenpo")) {
							parser->parsing_key_type = PARSING_INI_SPEED;
							parser->output_cfg->speed = 0;
						} else if(!strcmp(parser->parsing_key, "audio") || !strcmp(parser->parsing_key, "kalama")) {
							parser->parsing_key_type = PARSING_INI_AUDIO;
							memset(parser->output_cfg->audio, 0, sizeof(parser->output_cfg->audio));
						} else if(!strcmp(parser->parsing_key, "font") || !strcmp(parser->parsing_key, "sitelen")) {
							parser->parsing_key_type = PARSING_INI_FONT;
							memset(parser->output_cfg->font, 0, sizeof(parser->output_cfg->font));
						} else if(!strcmp(parser->parsing_key, "font-large") || !strcmp(parser->parsing_key, "sitelen-suli")) {
							parser->parsing_key_type = PARSING_INI_FONT_LARGE;
							memset(parser->output_cfg->font_highres, 0, sizeof(parser->output_cfg->font_highres));
						} else if(!strcmp(parser->parsing_key, "layout") || !strcmp(parser->parsing_key, "ma-nena")) {
							parser->parsing_key_type = PARSING_INI_LAYOUT;
							parser->output_cfg->input_layout = 0;
						} else if(!strcmp(parser->parsing_key, "navigation") || !strcmp(parser->parsing_key, "nena-tawa")) {
							parser->parsing_key_type = PARSING_INI_NAVIGATION;
							parser->output_cfg->input_navigation = 0;
						} else if(!strcmp(parser->parsing_key, "action") || !strcmp(parser->parsing_key, "nena-pali")) {
							parser->parsing_key_type = PARSING_INI_ACTION;
							parser->output_cfg->input_action = 0;
						} else if(!strcmp(parser->parsing_key, "replay") || !strcmp(parser->parsing_key, "nena-sin")) {
							parser->parsing_key_type = PARSING_INI_REPLAY;
							parser->output_cfg->input_replay = 0;
						} else {
							// Unsupported key. Not gonna process that.
							parser->state = PARSING_TRAILING;
						}
					break;
					default:
						if(parser->parse_index < sizeof(parser->parsing_key)-1) { // truncate long string
							parser->parsing_key[parser->parse_index++] = c;
						}
					break;
				}
			break;
			case PARSING_VALUE:
				switch(c) {
					case ' ':
						if(parser->parse_index > 0) {
							// Only skip if the space occures after a word/letter's detected.
							// Not entering this if-block is a string trimming operation.
							parser->state = PARSING_TRAILING;
						}
					break;
					default:
						switch(parser->parsing_key_type) {
							case PARSING_INI_QUIRKS:
								if(parser->parse_index >= 8) {
									return false;
								}
								parser->output_cfg->quirks <<= 4;
								uint8_t value = file_parse_ini_hex2uint(c);
								if(value < 16) {
									parser->output_cfg->quirks |= value;
								} else {
									return false;
								}
							break;
							case PARSING_INI_SPEED:
								if(!file_parse_dec_uint8(&parser->output_cfg->speed, parser->parse_index, c)) { return false; }
							break;
							case PARSING_INI_AUDIO:
								if(!file_parse_buffer(&parser->output_cfg->audio, sizeof(parser->output_cfg->audio), parser->parse_index, c)) { return false; }
							break;
							case PARSING_INI_FONT:
								if(!file_parse_buffer(&parser->output_cfg->font, sizeof(parser->output_cfg->font), parser->parse_index, c)) { return false; }
							break;
							case PARSING_INI_FONT_LARGE:
								if(!file_parse_buffer(&parser->output_cfg->font_highres, sizeof(parser->output_cfg->font_highres), parser->parse_index, c)) { return false; }
							break;
							case PARSING_INI_LAYOUT:
								if(!file_parse_dec_uint8(&parser->output_cfg->input_layout, parser->parse_index, c)) { return false; }
							break;
							case PARSING_INI_NAVIGATION:
								if(!file_parse_input_button(&parser->output_cfg->input_navigation, c)) { return false; }
							break;
							case PARSING_INI_ACTION:
								if(!file_parse_input_button(&parser->output_cfg->input_action, c)) { return false; }
							break;
							case PARSING_INI_REPLAY:
								if(!file_parse_input_button(&parser->output_cfg->input_replay, c)) { return false; }
							break;
						}
						parser->parse_index++;
					break;
				}
			break;
			case PARSING_TRAILING:
				// do nothing. Just wait for newline
			break;
		}
	}
	return true;
}

uint8_t file_load_config(const char *path, struct chip8_config *chip8_cfg) {
	if(!file_card_mode_enter()) {
		return mount_result;
	}

	FIL fil;
	FRESULT ret;
	UINT bytesread;
	char *buffer = (char*)bulkmem->file_buffer;

	struct ini_parser ini_parser;
	memset(&ini_parser, 0, sizeof(ini_parser));
	ini_parser.output_cfg = chip8_cfg;
	memcpy(chip8_cfg, &CHIP8_CFG_DEFAULT, sizeof(struct chip8_config));
	chip8_cfg->version = CHIP8_CFG_VERSION;

	ret = f_open(&fil, path, FA_READ);
	if(ret == FR_OK) {
		while(true) {
			ret = f_read(&fil, buffer, FILE_BUFFER_SIZE, &bytesread);
			if(ret != FR_OK) { // Error condition
				break;
			}
			if(!file_parse_ini(&ini_parser, buffer, bytesread)) {
				ret = FR_INI_PARSE_ERROR; // Borrowing the enum for INI parsing error
				break;
			}
			if(bytesread < FILE_BUFFER_SIZE) { // EOF condition
				break;
			}
		}
		f_close(&fil);
	}

	if(ret == FR_OK) {
		// Validation of parsed content
		if(chip8_cfg->speed > 99 ||
			chip8_cfg->input_layout >= CHIP8_LAYOUT_COUNT) {
			ret = FR_INI_PARSE_ERROR; // Parsing validation error!
		}
	} else if(ret == FR_NO_FILE) {
		// It's ok to have the INI file missing
		// The default config would be loaded and used
		ret = FR_OK;
	}

	// Attempt to load the storage flag
	if(ret == FR_OK) {
		// Let's set the default storage flag state to zero by default
		// Then attempt to read the storage flag file, which'd fail siltently upon failure
		memset(chip8_cfg->storage_flags, 0, sizeof(chip8_cfg->storage_flags));
		if(f_open(&fil, STORAGE_FLAG_FILE, FA_READ) == FR_OK) {
			f_read(&fil, chip8_cfg->storage_flags, sizeof(chip8_cfg->storage_flags), &bytesread);
			f_close(&fil);
		}
	}

	file_card_mode_exit();
	return ret;
}

uint8_t file_save_storage_flag(const uint8_t *storage_flags, size_t flag_size) {
	if(!file_card_mode_enter()) {
		return mount_result;
	}

	FIL fil;
	FRESULT ret = f_open(&fil, STORAGE_FLAG_FILE, FA_WRITE|FA_CREATE_ALWAYS);
	if(ret == FR_OK) {
		UINT byteswritten;
		ret = f_write(&fil, storage_flags, flag_size, &byteswritten);
		if(byteswritten != flag_size) { ret = FR_VOLUME_FULL; }
		f_close(&fil);
	}

	file_card_mode_exit();
	return ret;
}

int file_print_hex_buffer(char *dest, const uint8_t *buffer, size_t buffer_size) {
	int ret = 0;
	for(size_t i=0; i<buffer_size; i++) {
		ret += sprintf(&dest[ret], "%02X", buffer[i]);
	}
	dest[ret] = '\0';
	return ret;
}

uint8_t file_save_config(const char *path, const struct chip8_config *chip8_cfg) {
	if(!file_card_mode_enter()) {
		return mount_result;
	}

	FIL fil;
	FRESULT ret;
	ret = f_open(&fil, path, FA_WRITE|FA_CREATE_ALWAYS);
	if(ret == FR_OK) {
		do {
			// Assumption 1: sprintf() wouldn't return negative value because there shouldn't be any encoding error
			// Assumption 2: The buffer always has enough space for storing the entire string
			assert(sizeof(bulkmem->file_buffer) >= 512);
			size_t index = 0;
			if(chip8_cfg->quirks != CHIP8_CFG_DEFAULT.quirks) {
				index += sprintf(&bulkmem->file_buffer[index], "quirks = %08lX\n", chip8_cfg->quirks);
			}
			if(chip8_cfg->speed != CHIP8_CFG_DEFAULT.speed) {
				index += sprintf(&bulkmem->file_buffer[index], "speed = %u\n", chip8_cfg->speed);
			}
			if(memcmp(chip8_cfg->audio, CHIP8_CFG_DEFAULT.audio, sizeof(chip8_cfg->audio))) {
				index += sprintf(&bulkmem->file_buffer[index], "audio = ");
				index += file_print_hex_buffer(&bulkmem->file_buffer[index], chip8_cfg->audio, sizeof(chip8_cfg->audio));
				index += sprintf(&bulkmem->file_buffer[index], "\n");
			}
			if(chip8_cfg->input_layout != CHIP8_CFG_DEFAULT.input_layout) {
				index += sprintf(&bulkmem->file_buffer[index], "layout = %u\n", chip8_cfg->input_layout);
			}
			if(chip8_cfg->input_navigation != CHIP8_CFG_DEFAULT.input_navigation) {
				index += sprintf(&bulkmem->file_buffer[index], "navigation = ");
				index += util_print_button_buffer(&bulkmem->file_buffer[index], chip8_cfg->input_navigation);
				index += sprintf(&bulkmem->file_buffer[index], "\n");
			}
			if(chip8_cfg->input_action != CHIP8_CFG_DEFAULT.input_action) {
				index += sprintf(&bulkmem->file_buffer[index], "action = ");
				index += util_print_button_buffer(&bulkmem->file_buffer[index], chip8_cfg->input_action);
				index += sprintf(&bulkmem->file_buffer[index], "\n");
			}
			if(chip8_cfg->input_replay != CHIP8_CFG_DEFAULT.input_replay) {
				index += sprintf(&bulkmem->file_buffer[index], "replay = ");
				index += util_print_button_buffer(&bulkmem->file_buffer[index], chip8_cfg->input_replay);
				index += sprintf(&bulkmem->file_buffer[index], "\n");
			}
			if(memcmp(chip8_cfg->font, CHIP8_CFG_DEFAULT.font, sizeof(chip8_cfg->font))) {
				index += sprintf(&bulkmem->file_buffer[index], "font = ");
				index += file_print_hex_buffer(&bulkmem->file_buffer[index], chip8_cfg->font, sizeof(chip8_cfg->font));
				index += sprintf(&bulkmem->file_buffer[index], "\n");
			}
			UINT byteswritten;
			ret = f_write(&fil, bulkmem->file_buffer, index, &byteswritten);
			if(byteswritten != index) { ret = FR_VOLUME_FULL; }
			if(ret != FR_OK) { break; }
			f_sync(&fil);

			// Need to split the f_write() into two blocks to fit the string into the 512 bytes buffer
			index = 0;
			if(memcmp(chip8_cfg->font_highres, CHIP8_CFG_DEFAULT.font_highres, sizeof(chip8_cfg->font_highres))) {
				index += sprintf(&bulkmem->file_buffer[index], "font-large = ");
				index += file_print_hex_buffer(&bulkmem->file_buffer[index], chip8_cfg->font_highres, sizeof(chip8_cfg->font_highres));
				index += sprintf(&bulkmem->file_buffer[index], "\n");
			}
			ret = f_write(&fil, bulkmem->file_buffer, index, &byteswritten);
			if(byteswritten != index) { ret = FR_VOLUME_FULL; }
			if(ret != FR_OK) { break; }
		} while(false);
		f_close(&fil);
	}

	file_card_mode_exit();
	return ret;
}

uint8_t file_load_rom(const char *path, const struct chip8_config *chip8_cfg, struct chip8_machine *chip8_machine) {
	if(!file_card_mode_enter()) {
		return mount_result;
	}

	chip8_init(chip8_machine, chip8_cfg);

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

uint8_t file_readdir(const char *path, size_t offset, char (*filelist)[14], size_t *count) {
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
			// Filter out non-directory and non-CH8 files
			if(!(fileinfo.fattrib & AM_DIR) &&
				(strlen(fileinfo.fname) < 4 || memcmp(&fileinfo.fname[strlen(fileinfo.fname)-4], ".CH8", 4))) {
				continue;
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

uint8_t file_verify_firmware_update(void) {
	if(!file_card_mode_enter()) {
		return mount_result;
	}

	FIL fil;
	FRESULT ret;
	UINT bytesread;
	uint8_t *flash_offset = (uint8_t*)FLASH_START_OFFSET;
	ret = f_open(&fil, FLASH_FILE, FA_READ);
	if(ret == FR_OK) {
		char *buffer = (char*)bulkmem->file_buffer;
		while(flash_offset < (uint8_t*)FLASH_END_OFFSET) {
			ret = f_read(&fil, buffer, FILE_BUFFER_SIZE, &bytesread);
			if(ret != FR_OK) { // Error condition
				break;
			}
			if(memcmp(flash_offset, buffer, bytesread)) {
				ret = FR_FIRMWARE_VERIFICATION_ERROR;
				break;
			}
			flash_offset += bytesread;
			if(bytesread < FILE_BUFFER_SIZE) { // EOF condition
				break;
			}
		}
		f_close(&fil);
	}
	if(ret == FR_OK) {
		// Verification completed
		// Delete firmware file so that the bootloader won't flash it again
		ret = f_unlink(FLASH_FILE);
	}

	file_card_mode_exit();
	return ret;
}
