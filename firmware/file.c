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
#include "crc.h"
#include "file.h"
#include "flash.h"
#include "generated.h"
#include "lcd.h"
#include "spi.h"
#include "util.h"
#include "fatfs/ff.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>

#define FLASH_FILE "ILOMUSI8.BIN"
#define FLASH_FILE_OLD "ILOMUSI8.BI_" // After complete flashing, change the filename to this one
#define FLASH_START_OFFSET (0x08000000)
#define FLASH_END_OFFSET (0x0800F800)

#define LOW_BATTERY_THRESHOLD (3000) // Disable device for card protection if the internal voltage's too low. Unit is mV

#define STORAGE_FLAG_FILE "FX75FX85.BIN"

static FATFS filesystem;
static FRESULT mount_result;

static FRESULT mount_filesystem(void) {
	return f_mount(&filesystem, "", 1);
}

static uint8_t file_card_mode_enter(void) {
	uint8_t ret = FR_OK;
	if(mount_result != FR_OK) {
		// If not mounted, give it a chance to mount right now!
		if(adc_card_is_inserted()) {
			spi_set_mode(SPI_MODE_MEMORY_CARD_SLOW);
			mount_result = mount_filesystem();
		}
		ret = mount_result;
	}

	// Check voltage level for disabling card write/read access.
	// This is intentionally done after mounting to make sure that
	// the card initialization mechanism has been attempted.
	// Without the initialization, the LCD SPI signals would get read by the card,
	// which the random LCD SPI data can end up bricking the card.
	// This check's performed regardless of mount_result so that when the error screen
	// get shown, the battery error would have higher priority than mount errors.
	uint32_t supply_voltage = adc_get_supply_voltage();
	static bool battery_failure_triggered = false;
	if(!battery_failure_triggered && supply_voltage < LOW_BATTERY_THRESHOLD) {
		battery_failure_triggered = true; // Once triggered, never resume
	}
	if(battery_failure_triggered) {
		ret = FR_LOW_BATTERY; // Prevents card read
	}

	// Must wait for transfer completion before setting mode, even if we're setting it to SPI_MODE_LCD
	// Otherwise it can break the on-going LCD transfer
	spi_set_mode(ret == FR_OK ? SPI_MODE_MEMORY_CARD : SPI_MODE_LCD);

	// If the return value is FR_OK, The SPI bus would be in SPI_MODE_MEMORY_CARD and the caller function must
	// call file_card_mode_exit() after it finishes working with the card to release the SPI bus for LCD
	// Otherwise the SPI bus would be in SPI_MODE_LCD and the caller function wouldn't need to release the SPI bus
	return ret;
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
	// In practice, if LCD SPI communication is on-going, it won't stop until the row's sent
	// and there's no implemented mechanism to stop the LCD SPI transfer
	// so there might be a very little bit of delay of SPI initialization for the card
	if(adc_card_is_just_removed()) {
		mount_result = FR_NOT_READY;
	}
	if(adc_card_has_insert_event()) {
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
	PARSING_INI_AUDIO_PITCH,
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
	uint32_t result = *output_value;
	if(parse_index >= 3) {
		return false;
	}
	if(c >= '0' && c <= '9') {
		result *= 10;
		result += c - '0';
	} else {
		return false;
	}
	if(result > UINT8_MAX) {
		return false;
	}
	*output_value = result;
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

static bool file_parse_ini(struct ini_parser *parser, const uint8_t *buffer, size_t length) {
	for(size_t i=0; i<length; i++) {
		uint8_t c = buffer[i];
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
						} else if(!strcmp(parser->parsing_key, "pitch") || !strcmp(parser->parsing_key, "wawa-kalama")) {
							parser->parsing_key_type = PARSING_INI_AUDIO_PITCH;
							parser->output_cfg->audio_pitch = 0;
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
							case PARSING_INI_AUDIO_PITCH:
								if(!file_parse_dec_uint8(&parser->output_cfg->audio_pitch, parser->parse_index, c)) { return false; }
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

uint8_t file_load_storage_flag_inner(uint8_t *storage_flags, size_t flag_size) {
	// Fallback value in case the flags failed to get loaded
	memset(storage_flags, 0, flag_size);

	FIL fil;
	UINT bytesread;
	uint8_t ret = f_open(&fil, STORAGE_FLAG_FILE, FA_READ);
	if(ret == FR_OK) {
		ret = f_read(&fil, storage_flags, flag_size, &bytesread);
		f_close(&fil);
	}
	return ret;
}

uint8_t file_load_config(const char *path, struct chip8_config *chip8_cfg) {
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	FIL fil;
	UINT bytesread;
	uint8_t *buffer = bulkmem->file_buffer;

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
		// Attempt to read the storage flag file, which'd fail silently upon error/file not found
		file_load_storage_flag_inner(chip8_cfg->storage_flags, sizeof(chip8_cfg->storage_flags));
	}

	file_card_mode_exit();
	return ret;
}

uint8_t file_load_storage_flag(uint8_t *storage_flags, size_t flag_size) {
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	ret = file_load_storage_flag_inner(storage_flags, flag_size);

	file_card_mode_exit();
	return ret;
}

uint8_t file_save_storage_flag(const uint8_t *storage_flags, size_t flag_size) {
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	FIL fil;
	ret = f_open(&fil, STORAGE_FLAG_FILE, FA_WRITE|FA_CREATE_ALWAYS);
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
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	FIL fil;
	ret = f_open(&fil, path, FA_WRITE|FA_CREATE_ALWAYS);
	if(ret == FR_OK) {
		do {
			// Assumption 1: sprintf() wouldn't return negative value because there shouldn't be any encoding error
			// Assumption 2: The buffer always has enough space for storing the entire string
			static_assert(sizeof(bulkmem->file_buffer) >= 512);
			size_t index = 0;
			if(chip8_cfg->quirks != CHIP8_CFG_DEFAULT.quirks) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "quirks = %08lX\n", chip8_cfg->quirks);
			}
			if(chip8_cfg->speed != CHIP8_CFG_DEFAULT.speed) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "speed = %u\n", chip8_cfg->speed);
			}
			if(memcmp(chip8_cfg->audio, CHIP8_CFG_DEFAULT.audio, sizeof(chip8_cfg->audio))) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "audio = ");
				index += file_print_hex_buffer((char*)&bulkmem->file_buffer[index], chip8_cfg->audio, sizeof(chip8_cfg->audio));
				index += sprintf((char*)&bulkmem->file_buffer[index], "\n");
			}
			if(chip8_cfg->audio_pitch != CHIP8_CFG_DEFAULT.audio_pitch) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "pitch = %u\n", chip8_cfg->audio_pitch);
			}
			if(chip8_cfg->input_layout != CHIP8_CFG_DEFAULT.input_layout) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "layout = %u\n", chip8_cfg->input_layout);
			}
			if(chip8_cfg->input_navigation != CHIP8_CFG_DEFAULT.input_navigation) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "navigation = ");
				index += util_print_button_buffer((char*)&bulkmem->file_buffer[index], chip8_cfg->input_navigation);
				index += sprintf((char*)&bulkmem->file_buffer[index], "\n");
			}
			if(chip8_cfg->input_action != CHIP8_CFG_DEFAULT.input_action) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "action = ");
				index += util_print_button_buffer((char*)&bulkmem->file_buffer[index], chip8_cfg->input_action);
				index += sprintf((char*)&bulkmem->file_buffer[index], "\n");
			}
			if(chip8_cfg->input_replay != CHIP8_CFG_DEFAULT.input_replay) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "replay = ");
				index += util_print_button_buffer((char*)&bulkmem->file_buffer[index], chip8_cfg->input_replay);
				index += sprintf((char*)&bulkmem->file_buffer[index], "\n");
			}
			if(memcmp(chip8_cfg->font, CHIP8_CFG_DEFAULT.font, sizeof(chip8_cfg->font))) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "font = ");
				index += file_print_hex_buffer((char*)&bulkmem->file_buffer[index], chip8_cfg->font, sizeof(chip8_cfg->font));
				index += sprintf((char*)&bulkmem->file_buffer[index], "\n");
			}
			UINT byteswritten;
			ret = f_write(&fil, bulkmem->file_buffer, index, &byteswritten);
			if(byteswritten != index) { ret = FR_VOLUME_FULL; }
			if(ret != FR_OK) { break; }
			f_sync(&fil);

			// Need to split the f_write() into two blocks to fit the string into the 512 bytes buffer
			index = 0;
			if(memcmp(chip8_cfg->font_highres, CHIP8_CFG_DEFAULT.font_highres, sizeof(chip8_cfg->font_highres))) {
				index += sprintf((char*)&bulkmem->file_buffer[index], "font-large = ");
				index += file_print_hex_buffer((char*)&bulkmem->file_buffer[index], chip8_cfg->font_highres, sizeof(chip8_cfg->font_highres));
				index += sprintf((char*)&bulkmem->file_buffer[index], "\n");
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
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	chip8_init(chip8_machine, chip8_cfg);

	FIL fil;
	UINT bytesread;
	ret = f_open(&fil, path, FA_READ);
	if(ret == FR_OK) {
		ret = f_read(&fil, &chip8_machine->mem[CHIP8_PROGRAM_START_OFFSET], CHIP8_MEMORY_SIZE-CHIP8_PROGRAM_START_OFFSET, &bytesread);
		f_close(&fil);
	}

	file_card_mode_exit();
	return ret;
}

static uint8_t file_readdir_from_card(const char *path, size_t offset, char (*filelist)[14], size_t *count) {
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	size_t listed_file_count = 0;
	size_t required_count = *count;
	size_t fulfilled_count = 0;

	DIR dir;
    FILINFO fileinfo;
    ret = f_opendir(&dir, path);
    if(ret == FR_OK) {
		while(true) {
			ret = f_readdir(&dir, &fileinfo);
			if(ret != FR_OK) { // Error detected!
				break;
			}
			if(fileinfo.fname[0] == '\0') { // End of directory
				bulkmem->readdir_max_count = listed_file_count;
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

static void file_sort_entry_insert_filetype_prefix(char dest[16], const char *src) {
	// Insert 'A' prefix for directories. 'B' prefix for files
	// This way the directories would get sorted first.
	if(strlen(src) > 0 && ((char*)src)[strlen(src)-1] == '/') {
		dest[0] = 'A';
	} else {
		dest[0] = 'B';
	}
	memcpy(&dest[1], src, sizeof(*bulkmem->readdir_cache));
}

static int file_sort_entry(const void *a, const void *b) {
	char a2[16];
	char b2[16];

	// Ensure that there's enough space for inserting the prefix letter
	static_assert(sizeof(*bulkmem->readdir_cache)+1 < sizeof(a2));
	static_assert(sizeof(*bulkmem->readdir_cache)+1 < sizeof(b2));

	// Sorting: directories come first, then it comes the file.
	// The files got sorted alphabetically
	file_sort_entry_insert_filetype_prefix(a2, a);
	file_sort_entry_insert_filetype_prefix(b2, b);
	return strcmp(a2, b2);
}

uint8_t file_readdir(const char *path, bool changed, size_t offset, char (*filelist)[14], size_t *count) {
	char (*readdir_cache)[14] = bulkmem->readdir_cache;
	uint8_t ret;
	size_t required_count = *count;
	size_t fulfilled_count = 0;

	if(changed) {
		// Cache invalidated. Need to load the first READDIR_CACHE_SIZE items into cache
		bulkmem->readdir_max_count = SIZE_MAX; // We don't know how many files is in the directory. Using SIZE_MAX until it's determined
		bulkmem->readdir_cache_count = READDIR_CACHE_SIZE;
		ret = file_readdir_from_card(path, 0, readdir_cache, &bulkmem->readdir_cache_count);
		if(ret != FR_OK) {
			return ret;
		}
		// Sort the cached directory content (Remarks: The uncached content won't be sorted)
		qsort(readdir_cache, bulkmem->readdir_cache_count, sizeof(*readdir_cache), file_sort_entry);
	}

	// First grab the cached results
	while(fulfilled_count < required_count && offset < bulkmem->readdir_cache_count) {
		memcpy(filelist[fulfilled_count++], readdir_cache[offset++], sizeof(*readdir_cache));
	}
	if(fulfilled_count >= required_count // All request fulfilled
		|| (offset >= bulkmem->readdir_max_count) // End of filelist reached
	) {
		*count = fulfilled_count;
		return FR_OK;
	}

	// If the cached result isn't enough to fulfill the request, read the remaining content from the card
	size_t uncached_count = required_count-fulfilled_count;
	ret = file_readdir_from_card(path, offset, &filelist[fulfilled_count], &uncached_count);
	fulfilled_count += uncached_count;
	*count = fulfilled_count;

	return ret;
}

uint8_t file_verify_firmware_update(void) {
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	FIL fil;
	UINT bytesread;
	uint8_t *flash_offset = (uint8_t*)FLASH_START_OFFSET;
	ret = f_open(&fil, FLASH_FILE, FA_READ);
	if(ret == FR_OK) {
		uint8_t *buffer = bulkmem->file_buffer;
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
		// Rename firmware file as backup so that the bootloader won't flash it again
		ret = f_unlink(FLASH_FILE_OLD);
		if(ret == FR_NO_FILE) {
			ret = FR_OK; // The backup file might not exist, which's ok
		}
		ret = f_rename(FLASH_FILE, FLASH_FILE_OLD);
	}

	file_card_mode_exit();
	return ret;
}

static uint8_t file_program_bootrom_inner(uint32_t flash_offset, const uint8_t *buffer, bool dry_run) {
	if(dry_run) {
		// Verify the buffer without flashing it
		if(memcmp((uint32_t*)flash_offset, buffer, FILE_BUFFER_SIZE)) {
			return FR_BOOTROM_VERIFICATION_ERROR;
		}
	} else {
		static_assert(FLASH_PAGE_SIZE == 256);
		static_assert(FILE_BUFFER_SIZE == 512);
		flash_write_256(flash_offset, buffer);
		flash_write_256(flash_offset+FLASH_PAGE_SIZE, &buffer[FLASH_PAGE_SIZE]);
	}
	return FR_OK;
}

uint8_t file_program_bootrom(const char *path, const struct chip8_config *chip8_cfg, bool dry_run) {
	uint8_t ret = file_card_mode_enter();
	if(ret != FR_OK) {
		return ret;
	}

	asm volatile("" ::: "memory"); // Prevents compiler from cutting coner and skip accessing the bootrom's FLASH content
	uint32_t crc = 0xFFFFFFFF;
	FIL fil;
	UINT bytesread;
	ret = f_open(&fil, path, FA_READ);
	if(ret == FR_OK) {
		uint8_t *buffer = (uint8_t*)bulkmem->file_buffer;
		// Flash the game ROM into the FLASH's bootrom section
		uint32_t flash_offset = FLASH_BOOTROM_CH8_START;
		bool eof = false;
		static_assert(FLASH_BOOTROM_CH8_START%FILE_BUFFER_SIZE == 0 && FLASH_BOOTROM_CH8_END%FILE_BUFFER_SIZE == 0);
		while(flash_offset < FLASH_BOOTROM_CH8_END) {
			// Always prefill ROM content with zeros just like chip8_init() inside file_load_rom()
			// It's make sure that the CHIP-8 emulator of boot ROM loaded from flash would have
			// the same behavior as the ROM loaded from the card
			memset(buffer, 0, FILE_BUFFER_SIZE);
			if(!eof) {
				ret = f_read(&fil, buffer, FILE_BUFFER_SIZE, &bytesread);
				if(ret != FR_OK) { // Error condition
					break;
				}
			}

			// Perform flashing (or verification if dry_run)
			ret = file_program_bootrom_inner(flash_offset, buffer, dry_run);
			if(ret != FR_OK) {
				// For dry_run, FR_BOOTROM_VERIFICATION_ERROR had occurred
				// We've confirmed that the bootrom content's different from
				// the one we intend to write. We can stop further verifying.
				break;
			}

			crc = crc32_compute(CRC32_TABLE, crc, buffer, FILE_BUFFER_SIZE);
			flash_offset += FILE_BUFFER_SIZE;
			if(bytesread < FILE_BUFFER_SIZE) { // EOF condition
				eof = true;
			}
		}
		f_close(&fil);

		// Flash the config into the FLASH's bootrom section
		if(ret == FR_OK) {
			// Make sure that FILE_BUFFER_SIZE is 512
			static_assert(FILE_BUFFER_SIZE == FLASH_BOOTROM_CRC_END-FLASH_BOOTROM_CFG_START);
			// Ensure that the BOOTROM layout is as expected. First comes the CFG, then comes the 4-byte CRC.
			static_assert(FLASH_BOOTROM_CFG_END == FLASH_BOOTROM_CRC_START);
			static_assert(FLASH_BOOTROM_CRC_END-FLASH_BOOTROM_CRC_START == 4);
			// Make sure that the config can fit into the BOOTROM_CFG section
			static_assert(sizeof(*chip8_cfg) < FLASH_BOOTROM_CFG_END-FLASH_BOOTROM_CFG_START);

			// Derive the 512 byte buffer that contains the game config and the CRC (the CRC is for the game rom + game config)
			memset(buffer, 0xFF, FILE_BUFFER_SIZE);
			memcpy(buffer, chip8_cfg, sizeof(*chip8_cfg));
			crc = crc32_compute(CRC32_TABLE, crc, buffer, FLASH_BOOTROM_CFG_END-FLASH_BOOTROM_CFG_START);
			memcpy(&buffer[FLASH_BOOTROM_CFG_END-FLASH_BOOTROM_CFG_START], &crc, sizeof(uint32_t));

			// Perform flashing (or verification if dry_run)
			ret = file_program_bootrom_inner(flash_offset, buffer, dry_run);
		}
	}

	file_card_mode_exit();
	return ret;
}
