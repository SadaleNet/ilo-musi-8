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
#include "buzzer.h"
#include "config.h"
#include "draw.h"
#include "file.h"
#include "lcd.h"
#include "spi.h"
#include "tim1_pwm.h"
#include "watchdog.h"
#include "util.h"

#include "fatfs/ff.h"
#include "ch32fun.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define GAMEPLAY_INSTRUCTION_DURATION_MS (5000U) // Show gameplay instruction for this long
#define GAMEPLAY_EXIT_DURATION_MS (3000U) // Tell the user to hold <X> for this long to exit the game
#define GAMEPLAY_EXIT_BANNER_ROW_POS (3) // The row position of the EXIT banner for warning the user about the exit
#define GAMEPLAY_EXIT_BANNER_ROW_HEIGHT (2) // How tall the EXIT banner are. Each row is 8px.

extern const uint8_t ICON_NAVIGATION[];
extern const size_t ICON_NAVIGATION_LENGTH;
extern const uint8_t ICON_GAMECONF[];
extern const size_t ICON_GAMECONF_LENGTH;
extern const uint8_t ICON_GLOBALCONF[];
extern const size_t ICON_GLOBALCONF_LENGTH;
extern const uint8_t ICON_PLAY[];
extern const size_t ICON_PLAY_LENGTH;
extern const uint8_t ICON_UPDIR[];
extern const size_t ICON_UPDIR_LENGTH;
extern const uint8_t ICON_ACTION[];
extern const size_t ICON_ACTION_LENGTH;
extern const uint8_t ICON_REPLAY[];
extern const size_t ICON_REPLAY_LENGTH;

enum screen_state {
	SCREEN_ERROR, // File IO Error Screen
	SCREEN_MENU,
	SCREEN_GAME_CONFIG, // quirks, frame limit, flash to boot rom
	SCREEN_GLOBAL_CONFIG,
	SCREEN_PRE_GAMEPLAY, // show controls and possibly keyboard layout remap
	SCREEN_GAMEPLAY,
	SCREEN_GAMEOVER,
	SCREEN_GAME_CRASHED,
	SCREEN_FW_UPDATE_OK,
};

enum game_config_selection {
	GAME_CONFIG_MAIN,
	GAME_CONFIG_QUIRKS,
	GAME_CONFIG_QUIRKS_CUSTOM,
	GAME_CONFIG_SPEED,
	GAME_CONFIG_BOOT_ROM,
};

enum global_config_selection {
	GLOBAL_CONFIG_MAIN,
	GLOBAL_CONFIG_VOLUME,
	GLOBAL_CONFIG_BACKLIGHT,
	GLOBAL_CONFIG_CONTRAST,
	GLOBAL_CONFIG_LANGUAGE,
	GLOBAL_CONFIG_BOOT_ROM,
};

static struct chip8_machine chip8;
struct chip8_config *chip8_cfg;
struct shared_buffer *bulkmem;

static uint32_t chip8_keymap(uint32_t button_state) {
	// Converts from the left layout to the right layout
	//      [16]                      [0x10]
	// [0]  [1]  [2]  [3]         [1] [2] [3] [C]
	// [4]  [5]  [6]  [7]         [4] [5] [6] [D]
	// [8]  [9]  [10] [11]        [7] [8] [9] [E]
	// [12] [13] [14] [15]        [A] [0] [B] [F]
	uint32_t ret = 0;
	const unsigned int BUTTON_MAP[] = {13, 0, 1, 2, 4, 5, 6, 8, 9, 10,
		12, 14, 3, 7, 11, 15, 16};
	for(int i=0; i<sizeof(BUTTON_MAP)/sizeof(*BUTTON_MAP); i++) {
		if(button_state & (1<<BUTTON_MAP[i])) {
			ret |= 1<<i;
		}
	}
	return ret;
}

static void directory_attach_filename(char *directory_str, const char *filename) {
	// Warning: No boundary check in this function
	// The boundary check's handled upon entering the direcotry (strlen(menu_current_dir) >= sizeof(menu_current_dir)-13-1)
	size_t current_dir_length = strlen(directory_str);
	memcpy(&directory_str[current_dir_length], filename, strlen(filename)+1);
}

static void directory_remove_filename(char *directory_str) {
	char *result = strrchr(directory_str, '/');
	if(result != NULL) {
		result[1] = '\0'; // Remove filename. Keep trailing slash.
	} else {
		directory_str[0] = '\0';
	}
}

static void directory_up(char *directory_str) {
	if(strlen(directory_str) >= 1) {
		// Remove the trailing slash
		directory_str[strlen(directory_str)-1] = '\0';
		// Look for the next trailing slash, then make it \0 for upping a directory level
		directory_remove_filename(directory_str);
	}
}


// Shared by all screens
enum screen_state screen_state;
struct global_config global_config;
struct global_config global_config_backup;
uint32_t systick_now;

// Shared by all screens except for SCREEN_GAMEPLAY
bool menu_display_update_required;

// For SCREEN_MENU
char (*menu_file_list)[14];
char menu_current_dir[256]; // Must not use bulkmem because this path's used for loading the ROM
bool menu_cache_invalidated; // Must invalidate upon bulkmem's filled by game ROM, upon path change, or upon change of directory's content
int menu_offset;
uint8_t file_io_result;
size_t menu_file_count_of_current_page;
bool menu_dir_reload_required;

// For SCREEN_GAME_CONFIG
enum game_config_selection game_config_selection;
uint8_t game_config_index;
uint32_t game_config_old_value;

// For SCREEN_GLOBAL_CONFIG
enum global_config_selection global_config_selection;
uint32_t global_config_old_value;
uint32_t global_config_buzzer_start_tick;

// For SCREEN_GAMEPLAY
uint8_t last_storage_flag[sizeof(chip8_cfg->storage_flags)/sizeof(*chip8_cfg->storage_flags)]; // storage flag state upon game launch
uint32_t game_min_cycle_interval;
uint32_t last_frame_processed_tick;
uint32_t last_lcd_blit_tick;
uint8_t game_paused;
uint32_t game_paused_start_tick;
uint8_t game_paused_screen_buffer_backup[GAMEPLAY_EXIT_BANNER_ROW_HEIGHT][DISPLAY_WIDTH]; // for showing pause state

static void get_rid_of_all_button_events(void) {
	// Clean screen reasons:
	// 1. We wait until release of all buttons.
	// In case a button got held, the screen content won't get shown.
	// 2. For SCREEN_PRE_GAMEPLAY, we stole the display buffer for showing the controls
	// so we also must clean it up before running the CHIP-8 emulator
	draw_clear(chip8.periph.display);
	lcd_transfer_begin(chip8.periph.display);
	// Wait until all buttons got released
	// That's because CHIP-8 has a instruction that waits for key release
	// If the button got pressed while the game's launched,
	// the button release would be detected by the game
	while(chip8_keymap(adc_button_get_state()));
	// Get rid of all button press events
	adc_button_get_just_pressed();
	adc_button_get_just_released();
}

static void apply_volume(void) {
	assert(global_config.volume < GLOBAL_CONFIG_MAX_VALUE);
	static const uint8_t VOLUME_MAP[GLOBAL_CONFIG_MAX_VALUE] = {0, 1, 2, 4, 6, 8, 10, 12, 14, 15};
	buzzer_set_volume(VOLUME_MAP[global_config.volume]);
}

static void apply_volume_with_feedback_sound(void) {
	static const uint8_t FEEDBACK_AUDIO_SAMPLE[] = CHIP8_DEFAULT_AUDIO_SAMPLE;

	buzzer_set_pitch(CHIP8_DEFAULT_AUDIO_PITCH);
	buzzer_set_buffer(FEEDBACK_AUDIO_SAMPLE);
	apply_volume();
	global_config_buzzer_start_tick = systick_now;
}

static void apply_brightness(void) {
	assert(global_config.backlight < GLOBAL_CONFIG_MAX_VALUE);
	static const uint8_t BRIGHTNESS_MAP[GLOBAL_CONFIG_MAX_VALUE] = {0, 1, 2, 3, 5, 6, 7, 9, 11, 15};
	lcd_set_brightness(BRIGHTNESS_MAP[global_config.backlight]);
}

static void apply_contrast(void) {
	assert(global_config.contrast < GLOBAL_CONFIG_MAX_VALUE);
	static const uint8_t CONTRAST_MAP[GLOBAL_CONFIG_MAX_VALUE] = {0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A};
	lcd_set_contrast(CONTRAST_MAP[global_config.contrast]);
}

static void screen_error_handler(void) {
	bool device_disabled = (file_io_result == FR_LOW_BATTERY);
	if(!device_disabled) {
		uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
		if(button_press & (1<<0x10)) {
			menu_current_dir[0] = '\0';
			menu_cache_invalidated = true;
			menu_offset = 0;
			menu_dir_reload_required = true;
			screen_state = SCREEN_MENU;
		} else if((button_press & (1<<0xD))) { // Allows visiting global config screen with D button even with card error
			memcpy(&global_config_backup, &global_config, sizeof(global_config_backup));
			screen_state = SCREEN_GLOBAL_CONFIG;
			menu_display_update_required = true;
		}

		if(screen_state != SCREEN_ERROR) {
			return;
		}
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 0);
		draw_text(chip8.periph.display, "CARD ERROR #", 0, 10);
		char errorcode[3] = {0};
		errorcode[0] = file_io_result/10 + '0';
		errorcode[1] = file_io_result%10 + '0';
		errorcode[2] = '\0';
		draw_text(chip8.periph.display, errorcode, 6*12, 10);
		switch(file_io_result) {
			case FR_NOT_READY:
				draw_text(chip8.periph.display, "NO CARD", 0, 20);
			break;
			case FR_NO_FILESYSTEM:
				draw_text(chip8.periph.display, "FILESYSTEM ERROR", 0, 20);
				draw_text(chip8.periph.display, "REQUIRES FAT16/FAT32", 0, 30);
			break;
			case FR_INI_PARSE_ERROR:
				draw_text(chip8.periph.display, "INVALID CONFIG INI", 0, 20);
			break;
			case FR_VOLUME_FULL:
				draw_text(chip8.periph.display, "VOLUME FULL", 0, 20);
			break;
			case FR_FIRMWARE_VERIFICATION_ERROR:
				draw_text(chip8.periph.display, "FW VERIFY ERROR", 0, 20);
			break;
			case FR_PATH_LENGTH_ERROR:
				draw_text(chip8.periph.display, "PATH TOO LONG", 0, 20);
			break;
			case FR_LOW_BATTERY:
				draw_text(chip8.periph.display, "LOW BATTERY", 0, 20);
				draw_text(chip8.periph.display, "PLEASE REPLACE", 0, 30);
			break;
			default:
			break;
		}
		if(!device_disabled) {
			draw_text(chip8.periph.display, "PRESS <X> TO RETRY", 0, 48);
		} else {
			draw_text(chip8.periph.display, "DEVICE DISABLED", 0, 48);
		}
		draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 58);
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_menu_handler(void) {
	int menu_offset_prev = menu_offset;
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	size_t menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
	if((button_press & (1<<0xC))) { // The C button
		if(menu_file_count_of_current_page > 0 && // Only usable inside non-empty directories
		strlen(menu_file_list[menu_offset_on_current_page]) > 0 && // Boundary check for the next condition
		menu_file_list[menu_offset_on_current_page][strlen(menu_file_list[menu_offset_on_current_page])-1] != '/'  // Only usable if the selected item isn't a directory
		) {
			// Load the INI file into chip8_cfg, then restore menu_current_dir's content
			directory_attach_filename(menu_current_dir, menu_file_list[menu_offset_on_current_page]);
			memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "INI", 3);
			file_io_result = file_load_config(menu_current_dir, chip8_cfg);
			directory_remove_filename(menu_current_dir);
			// Head to the game config screen
			screen_state = (file_io_result == FR_OK) ? SCREEN_GAME_CONFIG : SCREEN_ERROR;
			menu_display_update_required = true;
		}
	} else if((button_press & (1<<0xD))) { // The D button
		memcpy(&global_config_backup, &global_config, sizeof(global_config_backup));
		screen_state = SCREEN_GLOBAL_CONFIG;
		menu_display_update_required = true;
	} else if((button_press & (1<<0xF)) && menu_file_count_of_current_page > 0) { // The F button. Only usable for non-empty directories
		// Attach the filename to the current menu_current_dir
		directory_attach_filename(menu_current_dir, menu_file_list[menu_offset_on_current_page]);

		if(strlen(menu_file_list[menu_offset_on_current_page]) > 0 && menu_file_list[menu_offset_on_current_page][strlen(menu_file_list[menu_offset_on_current_page])-1] == '/') {
			if(strlen(menu_current_dir) >= sizeof(menu_current_dir)-13-1) { // 13 for 8.3 filename with trailing slash, 1 for null terminator
				// The path's too long
				// We must reserve enough space for storing the filename next time we select a file.
				// Since the path's too long, we need to get out of here and show error to the end-user
				directory_up(menu_current_dir);
				file_io_result = FR_PATH_LENGTH_ERROR;
				// Here, now that I just need to wait for the (file_io_result != FR_OK) handling way below
			} else {
				// After entering the directory, reset the cursor to the beginning
				menu_offset = 0;
				menu_cache_invalidated = true;
				menu_dir_reload_required = true;
			}
		} else {
			// Load INI config
			memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "INI", 3); // replace file extension to .INI
			file_io_result = file_load_config(menu_current_dir, chip8_cfg);

			if(file_io_result == FR_OK) {
				// Config file loaded successfully. Let's try loading the game!
				memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "CH8", 3); // resume file extension of .CH8
				file_io_result = file_load_rom(menu_current_dir, chip8_cfg, &chip8);
				menu_cache_invalidated = true;
				if(file_io_result == FR_OK) {
					memcpy(last_storage_flag, chip8_cfg->storage_flags, sizeof(chip8_cfg->storage_flags));
					// Initialize peripheral variables
					buzzer_set_buffer(chip8.periph.audio);
					buzzer_set_pitch(chip8.periph.audio_pitch);
					if(chip8_cfg->speed == 0) {
						game_min_cycle_interval = 0; // Unlimited framerate
					} else {
						game_min_cycle_interval = FUNCONF_SYSTEM_CORE_CLOCK/60/chip8_cfg->speed;
					}
					last_frame_processed_tick = systick_now - game_min_cycle_interval;
					last_lcd_blit_tick = systick_now - FUNCONF_SYSTEM_CORE_CLOCK/60;
					game_paused = false;
					if(chip8_cfg->input_navigation || chip8_cfg->input_action || chip8_cfg->input_layout) {
						// If specified in the config INI file, show input buttons and the layout
						menu_display_update_required = true;
						screen_state = SCREEN_PRE_GAMEPLAY;
					} else {
						// If the input button hasn't been specified in the config file, just run the game!
						get_rid_of_all_button_events();
						screen_state = SCREEN_GAMEPLAY;
					}
				} else {
					// Failed to load the game.
					// Do nothing. Just wait for error handling for file_io_result != FR_OK
				}
			}
			directory_remove_filename(menu_current_dir);
		}
	} else if(button_press & (1<<0x10)) { // The X button
		// Up a directory
		directory_up(menu_current_dir);
		// Always reload directory so that the user would have visual feedback
		menu_offset = 0;
		menu_cache_invalidated = true;
		menu_dir_reload_required = true;
	} else {
		if(button_press & (1<<2)) { menu_offset--; }
		if(button_press & (1<<8)) { menu_offset++; }
		if(button_press & (1<<4)) { menu_offset -= 10; }
		if(button_press & (1<<6)) { menu_offset += 10; }
	}
	if(screen_state != SCREEN_MENU) {
		return;
	}

	if(menu_offset < 0) {
		menu_offset = 0;
	}

	if(menu_offset != menu_offset_prev) {
		menu_display_update_required = true;
	}

	if(menu_dir_reload_required || menu_offset/MENU_PAGE_SIZE != menu_offset_prev/MENU_PAGE_SIZE) {
		draw_clear(chip8.periph.display);
		lcd_transfer_begin(chip8.periph.display);

		for(size_t i=0; i<2; i++) {
			menu_file_count_of_current_page = MENU_PAGE_SIZE;
			file_io_result = file_readdir(menu_current_dir, menu_cache_invalidated, menu_offset/MENU_PAGE_SIZE*MENU_PAGE_SIZE, menu_file_list, &menu_file_count_of_current_page);
			menu_cache_invalidated = false;
			if(file_io_result == FR_OK) {
				if(menu_offset > 0 && menu_file_count_of_current_page == 0) {
					// The new page's empty. It happens when we reached the end of the directory
					// Let's select the last entry of the previous page
					menu_offset = (menu_offset/MENU_PAGE_SIZE-1)*MENU_PAGE_SIZE +MENU_PAGE_SIZE-1;
					continue;
				}
				menu_display_update_required = true;
				menu_dir_reload_required = false;
				last_frame_processed_tick = systick_now;
				last_lcd_blit_tick = systick_now;
				break;
			} else {
				break; // Skip to error handling mechanism
			}
		}
		// Get rid of all button press events after long operation
		adc_button_get_just_pressed();
	}

	if(file_io_result != FR_OK) {
		menu_display_update_required = true;
		screen_state = SCREEN_ERROR;
		return;
	}

	menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
	// Prevent selection of empty entries
	if(menu_offset_on_current_page > menu_file_count_of_current_page-1) {
		menu_offset = menu_offset/MENU_PAGE_SIZE*MENU_PAGE_SIZE + menu_file_count_of_current_page-1;
		menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
	}

	// Render the menu
	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		if(menu_file_count_of_current_page > 0) {
			// Draw filelist and cursor
			for(size_t i=0; i<menu_file_count_of_current_page; i++) {
				draw_text(chip8.periph.display, menu_file_list[i], 6, 6*i);
			}
			draw_text(chip8.periph.display, ">", 0, 6*menu_offset_on_current_page);
		} else {
			draw_text(chip8.periph.display, "[EMPTY]", 0, 0);
		}
		// Draw legend
		draw_text(chip8.periph.display, "2468", 96, 24+1);
		draw_bitmap_h8(chip8.periph.display, ICON_NAVIGATION, ICON_NAVIGATION_LENGTH, 120, 24);
		draw_text(chip8.periph.display, "C", 114, 32+1);
		draw_bitmap_h8(chip8.periph.display, ICON_GAMECONF, ICON_GAMECONF_LENGTH, 120, 32);
		draw_text(chip8.periph.display, "D", 114, 40+1);
		draw_bitmap_h8(chip8.periph.display, ICON_GLOBALCONF, ICON_GLOBALCONF_LENGTH, 120, 40);
		draw_text(chip8.periph.display, "F", 114, 48+1);
		draw_bitmap_h8(chip8.periph.display, ICON_PLAY, ICON_PLAY_LENGTH, 120, 48);
		draw_text(chip8.periph.display, "X", 114, 56+1);
		draw_bitmap_h8(chip8.periph.display, ICON_UPDIR, ICON_UPDIR_LENGTH, 120, 56);
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_game_config_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	switch(game_config_selection) {
		case GAME_CONFIG_MAIN:
			if(button_press & (1<<0xA)) { // The A button
				game_config_selection = GAME_CONFIG_QUIRKS;
				menu_display_update_required = true;
			} else if(button_press & (1<<0xB)) { // The B button
				game_config_index = 0;
				game_config_old_value = chip8_cfg->speed;
				chip8_cfg->speed = 0;
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_SPEED;
			} else if(button_press & (1<<0xC)) { // The C button
				game_config_selection = GAME_CONFIG_BOOT_ROM;
				menu_display_update_required = true;
			} else if(button_press & (1<<0xF)) { // The F button
				size_t menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
				directory_attach_filename(menu_current_dir, menu_file_list[menu_offset_on_current_page]);
				memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "INI", 3);
				file_io_result = file_save_config(menu_current_dir, chip8_cfg);
				directory_remove_filename(menu_current_dir);

				screen_state = (file_io_result == FR_OK) ? SCREEN_MENU : SCREEN_ERROR;
				menu_cache_invalidated = true; // INI file updated. file tree of the dircectory may be changed. Need to invalidate cache
				menu_display_update_required = true;
			} else if(button_press & (1<<0x10)) { // The X button
				// Discard game config by not saving it
				screen_state = SCREEN_MENU;
				menu_display_update_required = true;
			}
		break;
		case GAME_CONFIG_QUIRKS:
			if(button_press & ((1<<0x1)|(1<<0x2)|(1<<0x3))) {
				if(button_press & (1<<0x1)) {
					chip8_cfg->quirks = CHIP8_QUIRK_PLATFORM_VIP;
				} else if(button_press & (1<<0x2)) {
					chip8_cfg->quirks = CHIP8_QUIRK_PLATFORM_SCHIP;
				} else if(button_press & (1<<0x3)) {
					chip8_cfg->quirks = CHIP8_QUIRK_PLATFORM_OCTO;
				}
				game_config_selection = GAME_CONFIG_MAIN;
				menu_display_update_required = true;
			} else if(button_press & (1<<0x4)) {
				game_config_selection = GAME_CONFIG_QUIRKS_CUSTOM;
				game_config_old_value = chip8_cfg->quirks;
				chip8_cfg->quirks = 0;
				game_config_index = 0;
				menu_display_update_required = true;
			} else if(button_press & (1<<0x10)) {
				game_config_selection = GAME_CONFIG_MAIN;
				menu_display_update_required = true;
			}
		break;
		case GAME_CONFIG_QUIRKS_CUSTOM:
			if(button_press & (1<<0x10)) {
				if(game_config_index == 0) {
					chip8_cfg->quirks = game_config_old_value;
					game_config_selection = GAME_CONFIG_MAIN;
					menu_display_update_required = true;
					break;
				} else {
					game_config_index--;
					if(game_config_index == 0) {
						// Special handling for (4*(8-game_config_index)) >= 32
						// That's because C standard said that for shift operator,
						// for 32bit datawidth, rhs value of >= 32 is undefined behavior.
						// Trust me. I just got burned by that.
						chip8_cfg->quirks = 0;
					} else {
						chip8_cfg->quirks &= 0xFFFFFFFF << (4*(8-game_config_index));
					}
					menu_display_update_required = true;
				}
			}
			for(uint32_t i=0; i<16; i++) {
				if(button_press & (1<<i)) {
					menu_display_update_required = true;
					chip8_cfg->quirks |= i << (4*(7-game_config_index));
					if(++game_config_index >= 8) {
						game_config_selection = GAME_CONFIG_MAIN;
						break;
					}
				}
			}
		break;
		case GAME_CONFIG_SPEED:
			if(button_press & (1<<0x10)) {
				if(game_config_index == 0) {
					chip8_cfg->speed = game_config_old_value;
					game_config_selection = GAME_CONFIG_MAIN;
					menu_display_update_required = true;
				} else if(game_config_index == 1) {
					chip8_cfg->speed = 0;
					game_config_index--;
					menu_display_update_required = true;
				} else {
					while(true); // Should never happen!
				}
			}
			for(uint32_t i=0; i<10; i++) {
				if(button_press & (1<<i)) {
					if(game_config_index == 0) {
						chip8_cfg->speed += i*10;
						game_config_index++;
						menu_display_update_required = true;
					} else if(game_config_index == 1) {
						chip8_cfg->speed += i;
						game_config_selection = GAME_CONFIG_MAIN;
						menu_display_update_required = true;
						break;
					}
				}
			}
		break;
		case GAME_CONFIG_BOOT_ROM:
			if(button_press & ((1<<0xF)|(1<<0x10))) {
				if(button_press & (1<<0xF)) {
					// TODO: perform self-flashing operation here!
				}
				game_config_selection = GAME_CONFIG_MAIN;
				menu_display_update_required = true;
			}
		break;
	}

	if(screen_state != SCREEN_GAME_CONFIG) {
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_text(chip8.periph.display, "CONFIG", 0, 0);
		draw_text(chip8.periph.display, menu_file_list[menu_offset%MENU_PAGE_SIZE], 42, 0);

		draw_text(chip8.periph.display, "QUIRKS.....", 12, 14);
		draw_text(chip8.periph.display, "SPEED LIMIT......", 12, 23);
		draw_text(chip8.periph.display, "USE AS BOOT ROM", 12, 32);

		if(game_config_selection == GAME_CONFIG_QUIRKS_CUSTOM) {
			char value_str[9];
			sprintf(value_str, "%08lX", chip8_cfg->quirks);
			value_str[game_config_index] = '\0';
			draw_text(chip8.periph.display, value_str, 78, 14);
		} else {
			switch(chip8_cfg->quirks) {
				case CHIP8_QUIRK_PLATFORM_VIP:
					draw_text(chip8.periph.display, ".....VIP", 78, 14);
				break;
				case CHIP8_QUIRK_PLATFORM_SCHIP:
					draw_text(chip8.periph.display, "...SCHIP", 78, 14);
				break;
				case CHIP8_QUIRK_PLATFORM_OCTO:
					draw_text(chip8.periph.display, "....OCTO", 78, 14);
				break;
				default:
				{
					char value_str[9];
					sprintf(value_str, "%08lX", chip8_cfg->quirks);
					draw_text(chip8.periph.display, value_str, 78, 14);
				}
				break;
			}
		}
		if(game_config_selection == GAME_CONFIG_SPEED) {
			if(game_config_index == 1) {
				char value_str[2];
				value_str[0] = (chip8_cfg->speed/10) + '0';
				value_str[1] = '\0';
				draw_text(chip8.periph.display, value_str, 114, 23);
			}
		} else {
			char value_str[3];
			value_str[0] = (chip8_cfg->speed/10) + '0';
			value_str[1] = (chip8_cfg->speed%10) + '0';
			value_str[2] = '\0';
			draw_text(chip8.periph.display, value_str, 114, 23);
		}

		switch(game_config_selection) {
			case GAME_CONFIG_MAIN:
				draw_text(chip8.periph.display, "A)", 0, 14);
				draw_text(chip8.periph.display, "B)", 0, 23);
				draw_text(chip8.periph.display, "C)", 0, 32);
				draw_text(chip8.periph.display, "F)SAVE", 0, 47);
				draw_text(chip8.periph.display, "X)CANCEL", 0, 56);
			break;
			case GAME_CONFIG_QUIRKS:
				draw_text(chip8.periph.display, "=>", 0, 14);
				draw_text(chip8.periph.display, "1)VIP 2)SCHIP 3)OCTO", 0, 47);
				draw_text(chip8.periph.display, "4)CUSTOM X)CANCEL", 0, 56);
			break;
			case GAME_CONFIG_QUIRKS_CUSTOM:
				draw_text(chip8.periph.display, "=>", 0, 14);
				draw_text(chip8.periph.display, "0-F)TYPE HEX", 0, 47);
				draw_text(chip8.periph.display, "X)CANCEL", 0, 56);
			break;
			case GAME_CONFIG_SPEED:
				draw_text(chip8.periph.display, "=>", 0, 23);
				draw_text(chip8.periph.display, "0-9)TYPE DIGITS", 0, 47);
				draw_text(chip8.periph.display, "X)CANCEL", 0, 56);
			break;
			case GAME_CONFIG_BOOT_ROM:
				draw_text(chip8.periph.display, "=>", 0, 32);
				draw_text(chip8.periph.display, "F)OVERWRITE BOOT ROM", 0, 47);
				draw_text(chip8.periph.display, "X)CANCEL", 0, 56);
			break;
		}
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_global_config_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	switch(global_config_selection) {
		case GLOBAL_CONFIG_MAIN:
			if(button_press & (1<<0xA)) { // The A button
				global_config_selection = GLOBAL_CONFIG_VOLUME;
				global_config_old_value = global_config.volume;
				menu_display_update_required = true;
			} else if(button_press & (1<<0xB)) { // The B button
				global_config_selection = GLOBAL_CONFIG_BACKLIGHT;
				global_config_old_value = global_config.backlight;
				menu_display_update_required = true;
			} else if(button_press & (1<<0xC)) { // The C button
				global_config_selection = GLOBAL_CONFIG_CONTRAST;
				global_config_old_value = global_config.contrast;
				menu_display_update_required = true;
			} else if(button_press & (1<<0xD)) { // The D button
				global_config_selection = GLOBAL_CONFIG_LANGUAGE;
				menu_display_update_required = true;
			} else if(button_press & (1<<0xE)) { // The E button
				global_config_selection = GLOBAL_CONFIG_BOOT_ROM;
				menu_display_update_required = true;
			} else if(button_press & (1<<0xF)) { // The F button
				config_save(&global_config);
				screen_state = (file_io_result == FR_OK) ? SCREEN_MENU : SCREEN_ERROR; // If the user came from SCREEN_ERROR, file_io_result might not be FR_OK
				menu_display_update_required = true;
			} else if(button_press & (1<<0x10)) { // The X button
				// Revert to original global config
				memcpy(&global_config, &global_config_backup, sizeof(global_config));
				apply_volume();
				apply_brightness();
				apply_contrast();
				screen_state = (file_io_result == FR_OK) ? SCREEN_MENU : SCREEN_ERROR; // If the user came from SCREEN_ERROR, file_io_result might not be FR_OK
				menu_display_update_required = true;
			}
		break;
		// Shared by GLOBAL_CONFIG_VOLUME, GLOBAL_CONFIG_BACKLIGHT, GLOBAL_CONFIG_CONTRAST
		// Cannot be defined as a function because we're operating on a struct bitfield
		#define ADJUSTMENT_HANDLER(FIELD, CONFIG_ADJUSTED_HANDLER) \
			if(button_press & (1<<0x4)) { \
				if(FIELD > 0) { \
					FIELD--; \
					CONFIG_ADJUSTED_HANDLER(); \
					menu_display_update_required = true; \
				} \
			} if(button_press & (1<<0x6)) { \
				if(FIELD < GLOBAL_CONFIG_MAX_VALUE-1) { \
					FIELD++; \
					CONFIG_ADJUSTED_HANDLER(); \
					menu_display_update_required = true; \
				} \
			} else if(button_press & (1<<0xF)) { \
				global_config_selection = GLOBAL_CONFIG_MAIN; \
				menu_display_update_required = true; \
			} else if(button_press & (1<<0x10)) { \
				global_config_selection = GLOBAL_CONFIG_MAIN; \
				FIELD = global_config_old_value; \
				CONFIG_ADJUSTED_HANDLER(); \
				menu_display_update_required = true; \
			}
		case GLOBAL_CONFIG_VOLUME:
			ADJUSTMENT_HANDLER(global_config.volume, apply_volume_with_feedback_sound);
		break;
		case GLOBAL_CONFIG_BACKLIGHT:
			ADJUSTMENT_HANDLER(global_config.backlight, apply_brightness);
		break;
		case GLOBAL_CONFIG_CONTRAST:
			ADJUSTMENT_HANDLER(global_config.contrast, apply_contrast);
		break;
		case GLOBAL_CONFIG_LANGUAGE:
			if(button_press & ((1<<0x1)|(1<<0x2)|(1<<0x3)|(1<<0x4))) {
				if(button_press & (1<<0x1)) {
					global_config.language = LANG_EN;
				} else if(button_press & (1<<0x2)) {
					global_config.language = LANG_TOK;
				} else if(button_press & (1<<0x3)) {
					global_config.language = LANG_SP;
				} else if(button_press & (1<<0x4)) {
					global_config.language = LANG_QSS;
				}
				global_config_selection = GAME_CONFIG_MAIN;
				menu_display_update_required = true;
			} else if(button_press & (1<<0x10)) {
				global_config_selection = GAME_CONFIG_MAIN;
				menu_display_update_required = true;
			}
		break;
		case GLOBAL_CONFIG_BOOT_ROM:
			if(button_press & ((1<<0xF)|(1<<0x10))) {
				if(button_press & (1<<0xF)) {
					// TODO: perform clear ROM operation here!
				}
				global_config_selection = GAME_CONFIG_MAIN;
				menu_display_update_required = true;
			}
		break;
	}

	if(screen_state != SCREEN_GLOBAL_CONFIG) {
		// Always set volume back to zero before leaving the scene
		// so that the buzzer won't be constantly on
		buzzer_set_volume(0);
		return;
	}

	static const uint32_t FEEDBACK_AUDIO_DURATION = FUNCONF_SYSTEM_CORE_CLOCK*20/60; // 20 frames of feedback audio
	if(systick_now-global_config_buzzer_start_tick >= FEEDBACK_AUDIO_DURATION) {
		buzzer_set_volume(0);
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);

		draw_text(chip8.periph.display, "VOLUME............", 12, 0);
		draw_text(chip8.periph.display, "BACKLIGHT.........", 12, 9);
		draw_text(chip8.periph.display, "CONTRAST..........", 12, 18);
		draw_text(chip8.periph.display, "LANGUAGE........", 12, 27);
		draw_text(chip8.periph.display, "CLEAR BOOT ROM", 12, 36);

		char value_str[2];
		value_str[1] = '\0';
		value_str[0] = (global_config.volume%10) + '0';
		draw_text(chip8.periph.display, value_str, 120, 0);
		value_str[0] = (global_config.backlight%10) + '0';
		draw_text(chip8.periph.display, value_str, 120, 9);
		value_str[0] = (global_config.contrast%10) + '0';
		draw_text(chip8.periph.display, value_str, 120, 18);
		switch(global_config.language) {
			case LANG_EN: draw_text(chip8.periph.display, ".EN", 110, 27); break;
			case LANG_TOK: draw_text(chip8.periph.display, "TOK", 110, 27); break;
			case LANG_SP: draw_text(chip8.periph.display, ".SP", 110, 27); break;
			case LANG_QSS: draw_text(chip8.periph.display, "QSS", 110, 27); break;
			break;
		}

		switch(global_config_selection) {
			case GLOBAL_CONFIG_MAIN:
				draw_text(chip8.periph.display, "A)", 0, 0);
				draw_text(chip8.periph.display, "B)", 0, 9);
				draw_text(chip8.periph.display, "C)", 0, 18);
				draw_text(chip8.periph.display, "D)", 0, 27);
				draw_text(chip8.periph.display, "E)", 0, 36);
				draw_text(chip8.periph.display, "F)SAVE", 0, 47);
				draw_text(chip8.periph.display, "X)CANCEL", 0, 56);
			break;
			case GLOBAL_CONFIG_VOLUME:
			case GLOBAL_CONFIG_BACKLIGHT:
			case GLOBAL_CONFIG_CONTRAST:
				switch(global_config_selection) {
					case GLOBAL_CONFIG_VOLUME: draw_text(chip8.periph.display, "=>", 0, 0); break;
					case GLOBAL_CONFIG_BACKLIGHT: draw_text(chip8.periph.display, "=>", 0, 9); break;
					case GLOBAL_CONFIG_CONTRAST: draw_text(chip8.periph.display, "=>", 0, 18); break;
					default: assert(false); break; // Should never happen!
				}
				draw_text(chip8.periph.display, "4)LESS 6)MORE", 0, 47);
				draw_text(chip8.periph.display, "F)SAVE X)CANCEL", 0, 56);
			break;
			case GLOBAL_CONFIG_LANGUAGE:
				draw_text(chip8.periph.display, "=>", 0, 27);
				draw_text(chip8.periph.display, "1)EN 2)TOK 3)SP", 0, 47);
				draw_text(chip8.periph.display, "4)QSS X)CANCEL", 0, 56);
			break;
			case GLOBAL_CONFIG_BOOT_ROM:
				draw_text(chip8.periph.display, "=>", 0, 36);
				draw_text(chip8.periph.display, "F)CONFIRM CLEAR", 0, 47);
				draw_text(chip8.periph.display, "X)CANCEL", 0, 56);
			break;
		}
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_pre_gameplay_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	if(button_press & (1<<0x10)) {
		menu_dir_reload_required = true;
		screen_state = SCREEN_MENU;
	} else if(button_press || systick_now - last_frame_processed_tick >= FUNCONF_SYSTEM_CORE_CLOCK/1000*GAMEPLAY_INSTRUCTION_DURATION_MS) {
		get_rid_of_all_button_events();
		screen_state = SCREEN_GAMEPLAY;
	}
	if(screen_state != SCREEN_PRE_GAMEPLAY) {
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_text(chip8.periph.display, "CONTROLS", 40, 0);
		char button_str[17];
		if(chip8_cfg->input_navigation) {
			util_print_button_buffer(button_str, chip8_cfg->input_navigation);
			uint8_t x = DISPLAY_WIDTH/2-(ICON_NAVIGATION_LENGTH+1+6*strlen(button_str))/2;
			draw_bitmap_h8(chip8.periph.display, ICON_NAVIGATION, ICON_NAVIGATION_LENGTH, x, 10);
			draw_text(chip8.periph.display, button_str, x+ICON_NAVIGATION_LENGTH+1, 11);
		}
		if(chip8_cfg->input_action) {
			util_print_button_buffer(button_str, chip8_cfg->input_action);
			uint8_t x = DISPLAY_WIDTH/2-(ICON_ACTION_LENGTH+1+6*strlen(button_str))/2;
			draw_bitmap_h8(chip8.periph.display, ICON_ACTION, ICON_ACTION_LENGTH, x, 20);
			draw_text(chip8.periph.display, button_str, x+ICON_ACTION_LENGTH+1, 21);
		}
		if(chip8_cfg->input_replay) {
			util_print_button_buffer(button_str, chip8_cfg->input_replay);
			uint8_t x = DISPLAY_WIDTH/2-(ICON_REPLAY_LENGTH+1+6*strlen(button_str))/2;
			draw_bitmap_h8(chip8.periph.display, ICON_REPLAY, ICON_REPLAY_LENGTH, x, 30);
			draw_text(chip8.periph.display, button_str, x+ICON_REPLAY_LENGTH+1, 31);
		}
		switch(chip8_cfg->input_layout) {
			case CHIP8_LAYOUT_QWERTY:
				draw_text(chip8.periph.display, "123C  1234", 34, 40);
				draw_text(chip8.periph.display, "456D  QWER", 34, 40+6);
				draw_text(chip8.periph.display, "789E  ASDF", 34, 40+12);
				draw_text(chip8.periph.display, "A0BF  ZXCV", 34, 40+18);
				draw_text(chip8.periph.display, "=", 64-3, 40+9);
			break;
			default:
				// do not show the layout because it's the same as the keycap label
			break;
		}
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_gameplay_handler(void) {
	if(systick_now - last_frame_processed_tick >= game_min_cycle_interval) { // (60 x CYCLES_PER_FRAME) fps
		chip8.periph.random_num = SysTick->CNT;
		uint32_t button_held = chip8_keymap(adc_button_get_state());
		uint32_t button_just_pressed = chip8_keymap(adc_button_get_just_pressed());
		uint32_t button_just_released = chip8_keymap(adc_button_get_just_released());

		// Game pause handling
		if(button_just_pressed & (1<<0x10)) {
			for(size_t i=0; i<GAMEPLAY_EXIT_BANNER_ROW_HEIGHT; i++) {
				draw_transfer_row(chip8.periph.display, game_paused_screen_buffer_backup[i], GAMEPLAY_EXIT_BANNER_ROW_POS+i);
				draw_clear_row(chip8.periph.display, GAMEPLAY_EXIT_BANNER_ROW_POS+i);
			}
			game_paused_start_tick = systick_now;
			game_paused = true;
		}
		if(button_just_released & (1<<0x10)) {
			for(size_t i=0; i<GAMEPLAY_EXIT_BANNER_ROW_HEIGHT; i++) {
				draw_clear_row(chip8.periph.display, GAMEPLAY_EXIT_BANNER_ROW_POS+i);
				draw_bitmap_h8(chip8.periph.display, game_paused_screen_buffer_backup[i], DISPLAY_WIDTH, 0, (GAMEPLAY_EXIT_BANNER_ROW_POS+i)*8);
			}
			game_paused = false;
		}

		if(!game_paused) {
			chip8.periph.key_held = (uint16_t)button_held;
			chip8.periph.key_just_released = (uint16_t)button_just_released;
			if(!(chip8.periph.requests & CHIP8_REQUEST_WAIT_DISPLAY_REFRESH)) {
				chip8_step(&chip8);
				buzzer_set_volume(chip8.periph.sound_timer > 0 ? global_config.volume : 0);
				if(chip8.periph.requests & CHIP8_REQUEST_AUDIO_BUFFER_UPDATED) {
					buzzer_set_buffer(chip8.periph.audio);
					chip8.periph.requests &= ~CHIP8_REQUEST_AUDIO_BUFFER_UPDATED;
				}
				if(chip8.periph.requests & CHIP8_REQUEST_AUDIO_PITCH_UPDATED) {
					buzzer_set_pitch(chip8.periph.audio_pitch);
					chip8.periph.requests &= ~CHIP8_REQUEST_AUDIO_PITCH_UPDATED;
				}
			}
		}

		bool user_exit = game_paused && systick_now-game_paused_start_tick >= FUNCONF_SYSTEM_CORE_CLOCK/1000*GAMEPLAY_EXIT_DURATION_MS;
		if(user_exit || chip8.periph.requests & CHIP8_REQUEST_HALT_MASK) {
			buzzer_set_volume(0);

			if(memcmp(last_storage_flag, chip8.periph.storage_flags, sizeof(chip8.periph.storage_flags))) {
				// Storage flag changed. Let's save it!
				file_io_result = file_save_storage_flag(chip8.periph.storage_flags, sizeof(chip8.periph.storage_flags));
				menu_cache_invalidated = true; // file content changed. file tree of the dircectory may be changed. Need to invalidate cache
			}

			if(file_io_result != FR_OK) {
				menu_display_update_required = true;
				screen_state = SCREEN_ERROR;
			} else if(user_exit) {
				menu_dir_reload_required = true;
				screen_state = SCREEN_MENU;
			} else if (chip8.periph.requests & CHIP8_REQUEST_HALT_EXIT_EMULATOR) {
				menu_display_update_required = true;
				screen_state = SCREEN_GAMEOVER;
			} else {
				menu_display_update_required = true;
				screen_state = SCREEN_GAME_CRASHED;
			}
			return;
		}
		last_frame_processed_tick = systick_now;
	}

	if(systick_now - last_lcd_blit_tick >= FUNCONF_SYSTEM_CORE_CLOCK/60) { // 60fps
		if(!game_paused) {
			chip8_timer_step(&chip8);
			buzzer_set_volume(chip8.periph.sound_timer > 0 ? global_config.volume : 0);
		} else {
			buzzer_set_volume(0); // Do not play any sound when the game's paused
			// Display exit countdown
			draw_clear_row(chip8.periph.display, GAMEPLAY_EXIT_BANNER_ROW_POS);
			draw_clear_row(chip8.periph.display, GAMEPLAY_EXIT_BANNER_ROW_POS+1);
			draw_text(chip8.periph.display, "HOLD TO EXIT... ", 13, 29);
			uint8_t digit = (GAMEPLAY_EXIT_DURATION_MS/1000 - (systick_now-game_paused_start_tick)/FUNCONF_SYSTEM_CORE_CLOCK);
			char str[2]; str[0] = '0' + digit; str[1] = '\0';
			draw_text(chip8.periph.display, str, 109, 29);
		}
		// There's no double-buffering for saving 1kB of RAM.
		// There still won't be tearing because the LCD's response time
		// is slow enough to have any tearing visible
		lcd_transfer_begin(chip8.periph.display);
		last_lcd_blit_tick = systick_now;
		chip8.periph.requests &= ~CHIP8_REQUEST_WAIT_DISPLAY_REFRESH;
	}
}

static void screen_gameover_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	if(button_press & (1<<0x10)) {
		menu_dir_reload_required = true;
		screen_state = SCREEN_MENU;
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_text(chip8.periph.display, "GAME OVER", 37, 24);
		draw_text(chip8.periph.display, "PRESS <X> TO EXIT", 13, 34);
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_game_crashed_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	if(button_press & (1<<0x10)) {
		menu_dir_reload_required = true;
		screen_state = SCREEN_MENU;
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		switch(chip8.periph.requests & CHIP8_REQUEST_HALT_MASK) {
			case CHIP8_REQUEST_HALT_I_ERROR:
				draw_text(chip8.periph.display, "I ERROR", 0, 0);
			break;
			case CHIP8_REQUEST_HALT_STACK_ERROR:
				draw_text(chip8.periph.display, "STACK ERROR", 0, 0);
			break;
			case CHIP8_REQUEST_HALT_PC_ERROR:
				draw_text(chip8.periph.display, "PC ERROR", 0, 0);
			break;
			case CHIP8_REQUEST_HALT_INVALID_INSTRUCTION:
				draw_text(chip8.periph.display, "INVALID INSTRUCTION", 0, 0);
			break;
			default:
				draw_text(chip8.periph.display, "UNKNOWN ERROR", 0, 0);
			break;
		}
		char strbuf[32];
		uint16_t instruction = chip8.mem[chip8.cpu.pc[chip8.cpu.pc_index]] << 8;
		instruction |= chip8.mem[chip8.cpu.pc[chip8.cpu.pc_index]+1];
		// Show I and current PC content
		sprintf(strbuf, "i:%04X pc:%04X[%04X]", chip8.cpu.i, chip8.cpu.pc[chip8.cpu.pc_index], instruction);
		draw_text(chip8.periph.display, strbuf, 0, 12);
		// Show stacktrace
		size_t pc_index = chip8.cpu.pc_index;
		size_t position_counter = 0;
		while(pc_index-- > 0) {
			sprintf(strbuf, "%03X", chip8.cpu.pc[pc_index] & 0xFFF);
			draw_text(chip8.periph.display, strbuf, 24*(position_counter%5), 18+6*(position_counter/5));
			position_counter++;
		}
		// Show v registers
		sprintf(strbuf, "v0~3: %02X %02X %02X %02X", chip8.cpu.v[0], chip8.cpu.v[1], chip8.cpu.v[2], chip8.cpu.v[3]);
		draw_text(chip8.periph.display, strbuf, 0, 36);
		sprintf(strbuf, "v4~7: %02X %02X %02X %02X", chip8.cpu.v[4], chip8.cpu.v[5], chip8.cpu.v[6], chip8.cpu.v[7]);
		draw_text(chip8.periph.display, strbuf, 0, 42);
		sprintf(strbuf, "v8~b: %02X %02X %02X %02X", chip8.cpu.v[8], chip8.cpu.v[9], chip8.cpu.v[10], chip8.cpu.v[11]);
		draw_text(chip8.periph.display, strbuf, 0, 48);
		sprintf(strbuf, "vc~f: %02X %02X %02X %02X", chip8.cpu.v[12], chip8.cpu.v[13], chip8.cpu.v[14], chip8.cpu.v[15]);
		draw_text(chip8.periph.display, strbuf, 0, 54);

		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_fw_update_ok_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	if(button_press & (1<<0x10)) {
		menu_dir_reload_required = true;
		screen_state = SCREEN_MENU;
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_text(chip8.periph.display, "UPDATE COMPLETED", 16, 24);
		draw_text(chip8.periph.display, "PRESS <X> TO PROCEED", 4, 34);
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

int main() {
	// Kickoff the watchdog as early as possible
	watchdog_init();

	SystemInit();

	// Enables interrupt preempt feature
	__set_INTSYSCR( __get_INTSYSCR() | 0x02 );

	adc_init();
	tim1_pwm_init(); // Required by LCD and buzzer
	buzzer_init();

	lcd_init_first_stage(); // Initialize IO. Turns off LCD
	spi_init(); // Depends on lcd_init_first_stage()

	while(!adc_is_reading_ready()){} // Depends on tim1_pwm_init()

	// Depends on adc_is_reading_ready()
	file_first_mount();

	spi_set_mode(SPI_MODE_LCD);
	lcd_init_second_stage(); // If card's inserted, must be done after spi_card_mount_filesystem()

	config_load(&global_config);
	apply_volume();
	apply_brightness();
	apply_contrast();

	buzzer_set_volume(0); // Always use buzzer volume of 0 at the beginning

	watchdog_feed();

	assert(sizeof(struct shared_buffer) <= sizeof(chip8.mem));
	bulkmem = (struct shared_buffer*)chip8.mem;
	chip8_cfg = &bulkmem->chip8_cfg;

	// Shared by all screens except for SCREEN_GAMEPLAY
	menu_display_update_required = true;

	// For SCREEN_MENU
	menu_file_list = bulkmem->menu_file_list;
	menu_current_dir[0] = '\0';
	menu_cache_invalidated = true;
	menu_offset = 0;
	file_io_result = FR_OK;
	menu_file_count_of_current_page = 0;
	menu_dir_reload_required = true;

	// For SCREEN_GAME_CONFIG
	game_config_selection = GAME_CONFIG_MAIN;
	game_config_index = 0;
	game_config_old_value = 0;

	// Verify firmware update content
	// It also shows low battery message in case low battery's detected (regardless if there's firmware update)
	file_io_result = file_verify_firmware_update();
	if(file_io_result == FR_NO_FILE) {
		screen_state = SCREEN_MENU;
	} else {
		screen_state = file_io_result == FR_OK ? SCREEN_FW_UPDATE_OK : SCREEN_ERROR;
	}

	// For SCREEN_GAMEPLAY
	game_min_cycle_interval = 0;
	last_frame_processed_tick = SysTick->CNT;
	last_lcd_blit_tick = SysTick->CNT;

	while(true) {
		file_loop();

		systick_now = SysTick->CNT;
		enum screen_state screen_state_prev = screen_state;
		switch(screen_state) {
			case SCREEN_ERROR: screen_error_handler(); break;
			case SCREEN_MENU: screen_menu_handler(); break;
			case SCREEN_GAME_CONFIG: screen_game_config_handler(); break;
			case SCREEN_GLOBAL_CONFIG: screen_global_config_handler(); break;
			case SCREEN_PRE_GAMEPLAY: screen_pre_gameplay_handler(); break;
			case SCREEN_GAMEPLAY: screen_gameplay_handler(); break;
			case SCREEN_GAMEOVER: screen_gameover_handler(); break;
			case SCREEN_GAME_CRASHED: screen_game_crashed_handler(); break;
			case SCREEN_FW_UPDATE_OK: screen_fw_update_ok_handler(); break;
		}

		if(screen_state != screen_state_prev) {
			// The LCD's specs recommends calling this function once a while
			// I'm gonna call it upon screen switch.
			// It also double as visual feedback to the user because
			// the screen would go blank for like 0.3s when this function's called
			lcd_refresh();
		}

		watchdog_feed();
	}
}
