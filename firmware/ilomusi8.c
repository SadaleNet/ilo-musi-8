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
#include "bootrom.h"
#include "buzzer.h"
#include "config.h"
#include "draw.h"
#include "file.h"
#include "flash.h"
#include "generated.h"
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

#define FIRMWARE_VERSION "v0.1"
#define GAMEPLAY_INSTRUCTION_DURATION_MS (5000U) // Show gameplay instruction for this long
#define GAMEPLAY_EXIT_DURATION_MS (3000U) // Tell the user to hold <X> for this long to exit the game
#define GAMEPLAY_EXIT_BANNER_ROW_POS (3) // The row position of the EXIT banner for warning the user about the exit
#define GAMEPLAY_EXIT_BANNER_ROW_HEIGHT (2) // How tall the confirm game quit banner is. Each row is 8px.

enum screen_state {
	SCREEN_ERROR, // File IO Error Screen
	SCREEN_MENU,
	SCREEN_GAME_CONFIG, // game-specific configuration. Saved into an INI file
	SCREEN_GLOBAL_CONFIG, // configuration applied to the entire game console. Saved into internal flash.
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

// Shared buffer definition for saving RAM space
union {
	struct chip8_config chip8_cfg; // Used in all screens except for SCREEN_GAMEPLAY
	uint8_t game_paused_screen_buffer_backup[GAMEPLAY_EXIT_BANNER_ROW_HEIGHT][DISPLAY_WIDTH]; // Used in SCREEN_GAMEPLAY
} union_buffer;

// Shared by all screens
struct shared_buffer *bulkmem;
static struct chip8_machine chip8;
struct chip8_config *chip8_cfg;
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

// Shortcut definitions for SCREEN_MENU and SCREEN_GAME_CONFIG
#define MENU_OFFSET_ON_CURRENT_PAGE (menu_offset%MENU_PAGE_SIZE)
#define MENU_SELECTED_FILENAME (menu_file_list[MENU_OFFSET_ON_CURRENT_PAGE])

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
bool game_is_bootrom;
uint32_t game_paused_start_tick; // After pausing for long enough (i.e. holding X for long enough), the game would be quit.
uint8_t (*game_paused_screen_buffer_backup)[DISPLAY_WIDTH]; // The pause message overlays on the game's display. Need to restore upon unpause.

static void wait_button_release(void) {
	// Clean screen reasons:
	// 1. We wait until release of all buttons.
	// In case a button got held, the old screen content would get shown.
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

static void apply_volume(bool with_sound) {
	assert(global_config.volume < GLOBAL_CONFIG_MAX_VALUE);
	static const uint8_t VOLUME_MAP[GLOBAL_CONFIG_MAX_VALUE] = {0, 1, 2, 4, 6, 8, 10, 12, 14, 15};
	buzzer_set_volume(with_sound ? VOLUME_MAP[global_config.volume] : 0);
}

static void apply_volume_with_feedback_sound(void) {
	buzzer_set_pitch(CHIP8_DEFAULT_AUDIO_PITCH);
	buzzer_set_buffer(CHIP8_DEFAULT_AUDIO_SAMPLE);
	apply_volume(true);
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

static void draw_message_with_dots_suffix(enum tr_msg_id msg_id, uint8_t max_dots, uint8_t x, uint8_t y) {
	draw_translated(chip8.periph.display, global_config.language, msg_id, x, y);
	uint8_t width = draw_get_translated_width(global_config.language, msg_id);
	char str[22];
	uint8_t prefix_letters = (width+5)/6;
	memset(str, '.', max_dots-prefix_letters);
	str[max_dots-prefix_letters] = '\0';
	draw_text(chip8.periph.display, str, 12+prefix_letters*6, y+Y_ADJ);
}

static void prepare_game_launch(void) {
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
}

static void screen_error_handler(void) {
	bool device_disabled = (file_io_result == FR_LOW_BATTERY);
	if(!device_disabled) {
		uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
		if(button_press & (1<<0x10)) {
			screen_state = SCREEN_MENU;
		} else if((button_press & (1<<0xD))) { // Allows visiting global config screen with D button even with card error
			memcpy(&global_config_backup, &global_config, sizeof(global_config_backup));
			menu_display_update_required = true;
			screen_state = SCREEN_GLOBAL_CONFIG;
		}

		if(screen_state != SCREEN_ERROR) {
			// Always invalidate cache and change to root directory
			// and reset cursor before leaving the screen.
			// This way next time we land on SCREEN_MENU, via or not via SCREEN_GLOBAL_CONFIG,
			// we'd always retry loading the root directory
			menu_current_dir[0] = '\0';
			menu_offset = 0;
			menu_cache_invalidated = true;
			return;
		}
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 0);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_TITLE, 0, 10);

		char errorcode[3] = {0};
		errorcode[0] = file_io_result/10 + '0';
		errorcode[1] = file_io_result%10 + '0';
		errorcode[2] = '\0';
		draw_text(chip8.periph.display, errorcode, draw_get_translated_width(global_config.language, TR_MSG_ERR_TITLE), 10+Y_ADJ);
		switch(file_io_result) {
			case FR_NOT_READY: // No card
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_NOT_READY, 0, 20);
			break;
			case FR_NO_FILESYSTEM:
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_NO_FILESYSTEM, 0, 20);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_NO_FILESYSTEM2, 0, 30);
			break;
			case FR_INI_PARSE_ERROR:
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_INI_PARSE, 0, 20);
			break;
			case FR_VOLUME_FULL:
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_VOLUME_FULL, 0, 20);
			break;
			case FR_PATH_LENGTH_ERROR:
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_PATH_LENGTH, 0, 20);
			break;
			case FR_FIRMWARE_VERIFICATION_ERROR:
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_FIRMWARE_VERIFICATION, 0, 20);
			break;
			case FR_BOOTROM_VERIFICATION_ERROR:
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_BOOTROM_VERIFICATION, 0, 20);
			break;
			case FR_LOW_BATTERY:
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_LOW_BATTERY, 0, 20);
			break;
			default:
			break;
		}
		if(!device_disabled) {
			draw_translated(chip8.periph.display, global_config.language, TR_MSG_ERR_PROCEED, 0, 48);
		}
		draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 58);
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static bool screen_menu_selected_item_is_a_file(void) {
	return (!menu_cache_invalidated && // content of menu_file_list is valid
			menu_file_count_of_current_page > 0 && // non-empty directory
			strlen(MENU_SELECTED_FILENAME) > 0 && // Boundary check for the next condition
			MENU_SELECTED_FILENAME[strlen(MENU_SELECTED_FILENAME)-1] != '/' // Not having trailing slash
		);
}

static void screen_menu_handler(void) {
	int menu_offset_prev = menu_offset;
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());

	// Must not handle keypress if menu_cache_invalidated==true
	// That's because the keypress handling might access menu_file_list,
	// which's a part of bulkmem, which might have invalid content if menu_cache_invalidated==true
	if(!menu_cache_invalidated) {
		if((button_press & (1<<0xC))) { // The C button
			if(screen_menu_selected_item_is_a_file()) {
				// Load the INI file into chip8_cfg, then restore menu_current_dir's content
				directory_attach_filename(menu_current_dir, MENU_SELECTED_FILENAME);
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
			directory_attach_filename(menu_current_dir, MENU_SELECTED_FILENAME);

			if(!screen_menu_selected_item_is_a_file()) { // Not a file. That gotta be a directory.
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
						prepare_game_launch();
						if(chip8_cfg->input_navigation || chip8_cfg->input_action || chip8_cfg->input_layout) {
							// If specified in the config INI file, show input buttons and the layout
							menu_display_update_required = true;
							screen_state = SCREEN_PRE_GAMEPLAY;
						} else {
							// If the input buttons haven't been specified in the config file, just run the game!
							wait_button_release();
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
			menu_offset = 0;
			menu_cache_invalidated = true;
		} else {
			if(button_press & (1<<2)) { menu_offset--; }
			if(button_press & (1<<8)) { menu_offset++; }
			if(button_press & (1<<4)) { menu_offset -= 10; }
			if(button_press & (1<<6)) { menu_offset += 10; }
		}
		if(screen_state != SCREEN_MENU) {
			return;
		}
	}

	if(menu_offset < 0) {
		menu_offset = 0;
	}

	if(menu_offset != menu_offset_prev) {
		menu_display_update_required = true;
	}

	if(menu_cache_invalidated || menu_offset/MENU_PAGE_SIZE != menu_offset_prev/MENU_PAGE_SIZE) {
		// Clear the screen just in case loading directory took long.
		// The cleared screen is a form of visual feedback to the user
		draw_clear(chip8.periph.display);
		lcd_transfer_begin(chip8.periph.display);

		for(size_t i=0; i<2; i++) {
			menu_file_count_of_current_page = MENU_PAGE_SIZE;
			file_io_result = file_readdir(menu_current_dir, menu_cache_invalidated, menu_offset/MENU_PAGE_SIZE*MENU_PAGE_SIZE, menu_file_list, &menu_file_count_of_current_page);
			if(file_io_result == FR_OK) {
				menu_cache_invalidated = false;
				if(menu_offset > 0 && menu_file_count_of_current_page == 0) {
					// The new page's empty. It happens when we reached the end of the directory
					// Let's select the last entry of the previous page
					menu_offset = (menu_offset/MENU_PAGE_SIZE-1)*MENU_PAGE_SIZE +MENU_PAGE_SIZE-1;
					continue;
				}
				menu_display_update_required = true;
				break;
			} else {
				break; // Skip to error handling mechanism
			}
		}
		// Get rid of all button press events after potentially long operation
		adc_button_get_just_pressed();
	}

	if(file_io_result != FR_OK) {
		menu_display_update_required = true;
		screen_state = SCREEN_ERROR;
		return;
	}

	// Prevent selection of empty entries
	if(MENU_OFFSET_ON_CURRENT_PAGE > menu_file_count_of_current_page-1) {
		menu_offset = menu_offset/MENU_PAGE_SIZE*MENU_PAGE_SIZE + menu_file_count_of_current_page-1;
	}

	// Render the menu
	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		if(menu_file_count_of_current_page > 0) {
			// Draw filelist and cursor
			for(size_t i=0; i<menu_file_count_of_current_page; i++) {
				draw_text(chip8.periph.display, menu_file_list[i], 6, 6*i);
			}
			draw_text(chip8.periph.display, ">", 0, 6*MENU_OFFSET_ON_CURRENT_PAGE);
		} else {
			draw_translated(chip8.periph.display, global_config.language, TR_MSG_MENU_EMPTY, 0, 0);
		}
		// Draw legend
		draw_text(chip8.periph.display, "2468", 96, 24+1);
		draw_bitmap_h8(chip8.periph.display, ICON_NAVIGATION, ICON_NAVIGATION_LEN, 120, 24);
		if(screen_menu_selected_item_is_a_file()) {
			draw_text(chip8.periph.display, "C", 114, 32+1);
			draw_bitmap_h8(chip8.periph.display, ICON_GAMECONF, ICON_GAMECONF_LEN, 120, 32);
		}
		draw_text(chip8.periph.display, "D", 114, 40+1);
		draw_bitmap_h8(chip8.periph.display, ICON_GLOBALCONF, ICON_GLOBALCONF_LEN, 120, 40);
		draw_text(chip8.periph.display, "F", 114, 48+1);
		draw_bitmap_h8(chip8.periph.display, ICON_PLAY, ICON_PLAY_LEN, 120, 48);
		draw_text(chip8.periph.display, "X", 114, 56+1);
		draw_bitmap_h8(chip8.periph.display, ICON_UPDIR, ICON_UPDIR_LEN, 120, 56);
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_game_config_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	switch(game_config_selection) {
		case GAME_CONFIG_MAIN:
			if(button_press & (1<<0xA)) { // The A button
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_QUIRKS;
			} else if(button_press & (1<<0xB)) { // The B button
				game_config_index = 0;
				game_config_old_value = chip8_cfg->speed;
				chip8_cfg->speed = 0;
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_SPEED;
			} else if(button_press & (1<<0xC)) { // The C button
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_BOOT_ROM;
			} else if(button_press & (1<<0xF)) { // The F button
				directory_attach_filename(menu_current_dir, MENU_SELECTED_FILENAME);
				memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "INI", 3);
				file_io_result = file_save_config(menu_current_dir, chip8_cfg);
				directory_remove_filename(menu_current_dir);

				menu_cache_invalidated = true; // INI file updated. file tree of the dircectory may be changed. Need to invalidate cache
				menu_display_update_required = true;
				screen_state = (file_io_result == FR_OK) ? SCREEN_MENU : SCREEN_ERROR;
			} else if(button_press & (1<<0x10)) { // The X button
				// Discard game config by not saving it
				menu_display_update_required = true;
				screen_state = SCREEN_MENU;
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
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_MAIN;
			} else if(button_press & (1<<0x4)) {
				game_config_old_value = chip8_cfg->quirks;
				chip8_cfg->quirks = 0;
				game_config_index = 0;
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_QUIRKS_CUSTOM;
			} else if(button_press & (1<<0x10)) {
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_MAIN;
			}
		break;
		case GAME_CONFIG_QUIRKS_CUSTOM:
			if(button_press & (1<<0x10)) {
				if(game_config_index == 0) {
					chip8_cfg->quirks = game_config_old_value;
					menu_display_update_required = true;
					game_config_selection = GAME_CONFIG_MAIN;
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
					menu_display_update_required = true;
					game_config_selection = GAME_CONFIG_MAIN;
				} else if(game_config_index == 1) {
					chip8_cfg->speed = 0;
					game_config_index--;
					menu_display_update_required = true;
				} else {
					assert(false); // Should never happen!
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
						menu_display_update_required = true;
						game_config_selection = GAME_CONFIG_MAIN;
						break;
					}
				}
			}
		break;
		case GAME_CONFIG_BOOT_ROM:
			if(button_press & (1<<0xF)) {
				// Remove the cancel option from the list for visual feedback of potentially long operation
				draw_clear_row(chip8.periph.display, 7);
				lcd_transfer_begin(chip8.periph.display);

				// Flash the bootrom
				memset(chip8_cfg->storage_flags, 0, sizeof(chip8_cfg->storage_flags)); // Never save the storage flag into the bootrom
				directory_attach_filename(menu_current_dir, MENU_SELECTED_FILENAME);
				file_io_result = bootrom_program(menu_current_dir, chip8_cfg);
				directory_remove_filename(menu_current_dir);

				// Get rid of all button press events after potentially long operation
				adc_button_get_just_pressed();

				if(file_io_result == FR_OK) {
					menu_display_update_required = true;
					game_config_selection = GAME_CONFIG_MAIN;
				} else {
					menu_display_update_required = true;
					screen_state = SCREEN_ERROR;
				}
			} else if(button_press & (1<<0x10)) {
				menu_display_update_required = true;
				game_config_selection = GAME_CONFIG_MAIN;
			}
		break;
	}

	if(screen_state != SCREEN_GAME_CONFIG) {
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_GMC_CONFIG, 0, 0);
		draw_text(chip8.periph.display, MENU_SELECTED_FILENAME,
			draw_get_translated_width(global_config.language, TR_MSG_GMC_CONFIG)+6, 0+Y_ADJ
		);

		draw_message_with_dots_suffix(TR_MSG_GMC_QUIRKS, 11, 12, 14);
		draw_message_with_dots_suffix(TR_MSG_GMC_SPEED_LIMIT, 17, 12, 23);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_GMC_USE_BOOTROM, 12, 32);

		if(game_config_selection == GAME_CONFIG_QUIRKS_CUSTOM) {
			char value_str[9];
			sprintf(value_str, "%08lX", chip8_cfg->quirks);
			value_str[game_config_index] = '\0';
			draw_text(chip8.periph.display, value_str, 78, 14+Y_ADJ);
		} else {
			switch(chip8_cfg->quirks) {
				case CHIP8_QUIRK_PLATFORM_VIP:
					draw_text(chip8.periph.display, ".....VIP", 78, 14+Y_ADJ);
				break;
				case CHIP8_QUIRK_PLATFORM_SCHIP:
					draw_text(chip8.periph.display, "...SCHIP", 78, 14+Y_ADJ);
				break;
				case CHIP8_QUIRK_PLATFORM_OCTO:
					draw_text(chip8.periph.display, "....OCTO", 78, 14+Y_ADJ);
				break;
				default:
				{
					char value_str[9];
					sprintf(value_str, "%08lX", chip8_cfg->quirks);
					draw_text(chip8.periph.display, value_str, 78, 14+Y_ADJ);
				}
				break;
			}
		}
		if(game_config_selection == GAME_CONFIG_SPEED) {
			if(game_config_index == 1) {
				char value_str[2];
				value_str[0] = (chip8_cfg->speed/10) + '0';
				value_str[1] = '\0';
				draw_text(chip8.periph.display, value_str, 114, 23+Y_ADJ);
			}
		} else {
			char value_str[3];
			value_str[0] = (chip8_cfg->speed/10) + '0';
			value_str[1] = (chip8_cfg->speed%10) + '0';
			value_str[2] = '\0';
			draw_text(chip8.periph.display, value_str, 114, 23+Y_ADJ);
		}

		switch(game_config_selection) {
			case GAME_CONFIG_MAIN:
				draw_text(chip8.periph.display, "A)", 0, 14+Y_ADJ);
				draw_text(chip8.periph.display, "B)", 0, 23+Y_ADJ);
				draw_text(chip8.periph.display, "C)", 0, 32+Y_ADJ);
				draw_text(chip8.periph.display, "F)", 0, 47+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_SAVE, 12, 47);
				draw_text(chip8.periph.display, "X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 12, 56);
			break;
			case GAME_CONFIG_QUIRKS:
				draw_text(chip8.periph.display, "=>", 0, 14+Y_ADJ);
				draw_text(chip8.periph.display, "1)VIP 2)SCHIP 3)OCTO", 0, 47+Y_ADJ);
				draw_text(chip8.periph.display, "4)       X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_GMC_CUSTOM, 12, 56);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 66, 56);
			break;
			case GAME_CONFIG_QUIRKS_CUSTOM:
				draw_text(chip8.periph.display, "=>", 0, 14+Y_ADJ);
				draw_text(chip8.periph.display, "0-F)", 0, 47+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_GMC_TYPEHEX, 24, 47);
				draw_text(chip8.periph.display, "X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 12, 56);
			break;
			case GAME_CONFIG_SPEED:
				draw_text(chip8.periph.display, "=>", 0, 23+Y_ADJ);
				draw_text(chip8.periph.display, "0-9)", 0, 47+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_GMC_TYPEDIGITS, 24, 47);
				draw_text(chip8.periph.display, "X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 12, 56);
			break;
			case GAME_CONFIG_BOOT_ROM:
				draw_text(chip8.periph.display, "=>", 0, 32+Y_ADJ);
				draw_text(chip8.periph.display, "F)", 0, 47+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_GMC_OVERWRITE, 12, 47);
				draw_text(chip8.periph.display, "X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 12, 56);
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
				global_config_old_value = global_config.volume;
				menu_display_update_required = true;
				global_config_selection = GLOBAL_CONFIG_VOLUME;
			} else if(button_press & (1<<0xB)) { // The B button
				global_config_old_value = global_config.backlight;
				menu_display_update_required = true;
				global_config_selection = GLOBAL_CONFIG_BACKLIGHT;
			} else if(button_press & (1<<0xC)) { // The C button
				global_config_old_value = global_config.contrast;
				menu_display_update_required = true;
				global_config_selection = GLOBAL_CONFIG_CONTRAST;
			} else if(button_press & (1<<0xD)) { // The D button
				menu_display_update_required = true;
				global_config_selection = GLOBAL_CONFIG_LANGUAGE;
			} else if(button_press & (1<<0xE)) { // The E button
				menu_display_update_required = true;
				global_config_selection = GLOBAL_CONFIG_BOOT_ROM;
			} else if(button_press & (1<<0xF)) { // The F button
				config_save(&global_config);
				menu_display_update_required = true;
				screen_state = SCREEN_MENU;
			} else if(button_press & (1<<0x10)) { // The X button
				// Revert to original global config
				memcpy(&global_config, &global_config_backup, sizeof(global_config));
				// apply_volume(false); // No need to do that. Unlike brightness and contrast, this apply_volume() only need to run when sound is needed.
				apply_brightness();
				apply_contrast();
				menu_display_update_required = true;
				screen_state = SCREEN_MENU;
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
				CONFIG_ADJUSTED_HANDLER(); \
				menu_display_update_required = true; \
				global_config_selection = GLOBAL_CONFIG_MAIN; \
			} else if(button_press & (1<<0x10)) { \
				FIELD = global_config_old_value; \
				CONFIG_ADJUSTED_HANDLER(); \
				menu_display_update_required = true; \
				global_config_selection = GLOBAL_CONFIG_MAIN; \
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
				menu_display_update_required = true;
				global_config_selection = GAME_CONFIG_MAIN;
			} else if(button_press & (1<<0x10)) {
				menu_display_update_required = true;
				global_config_selection = GAME_CONFIG_MAIN;
			}
		break;
		case GLOBAL_CONFIG_BOOT_ROM:
			if(button_press & (1<<0xF)) {
				bootrom_erase(); // Assumed to be always successful
				menu_display_update_required = true;
				global_config_selection = GAME_CONFIG_MAIN;
			} else if(button_press & (1<<0x10)) {
				menu_display_update_required = true;
				global_config_selection = GAME_CONFIG_MAIN;
			}
		break;
	}

	if(screen_state != SCREEN_GLOBAL_CONFIG) {
		// Always set mute before leaving the scene
		// so that the buzzer won't be constantly on
		apply_volume(false);
		return;
	}

	static const uint32_t FEEDBACK_AUDIO_DURATION = FUNCONF_SYSTEM_CORE_CLOCK*20/60; // 20 frames of feedback audio
	if(systick_now-global_config_buzzer_start_tick >= FEEDBACK_AUDIO_DURATION) {
		apply_volume(false);
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);

		draw_message_with_dots_suffix(TR_MSG_GLBC_VOLUME, 18, 12, 0);
		draw_message_with_dots_suffix(TR_MSG_GLBC_BACKLIGHT, 18, 12, 9);
		draw_message_with_dots_suffix(TR_MSG_GLBC_CONTRAST, 18, 12, 18);
		draw_message_with_dots_suffix(TR_MSG_GLBC_LANG, 16, 12, 27);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_GLBC_CLR_BOOTROM, 12, 36);

		char value_str[2];
		value_str[1] = '\0';
		value_str[0] = (global_config.volume%10) + '0';
		draw_text(chip8.periph.display, value_str, 120, 0+Y_ADJ);
		value_str[0] = (global_config.backlight%10) + '0';
		draw_text(chip8.periph.display, value_str, 120, 9+Y_ADJ);
		value_str[0] = (global_config.contrast%10) + '0';
		draw_text(chip8.periph.display, value_str, 120, 18+Y_ADJ);
		switch(global_config.language) {
			case LANG_EN: draw_text(chip8.periph.display, ".EN", 108, 27+Y_ADJ); break;
			case LANG_TOK: draw_text(chip8.periph.display, "TOK", 108+1, 27+Y_ADJ); break;
			case LANG_SP: draw_bitmap_h8(chip8.periph.display, ICON_LANG_SP, ICON_LANG_SP_LEN, 108, 27+Y_ADJ_SP); break;
			case LANG_QSS: draw_bitmap_h8(chip8.periph.display, ICON_LANG_QSS, ICON_LANG_QSS_LEN, 108+1, 27); break;
			break;
		}

		switch(global_config_selection) {
			case GLOBAL_CONFIG_MAIN:
				draw_text(chip8.periph.display, "A)", 0, 0+Y_ADJ);
				draw_text(chip8.periph.display, "B)", 0, 9+Y_ADJ);
				draw_text(chip8.periph.display, "C)", 0, 18+Y_ADJ);
				draw_text(chip8.periph.display, "D)", 0, 27+Y_ADJ);
				draw_text(chip8.periph.display, "E)", 0, 36+Y_ADJ);
				draw_text(chip8.periph.display, "F)", 0, 47+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_SAVE, 12, 47);
				draw_text(chip8.periph.display, "X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 12, 56);
				draw_text(chip8.periph.display, FIRMWARE_VERSION, DISPLAY_WIDTH-strlen(FIRMWARE_VERSION)*6, 58);
			break;
			case GLOBAL_CONFIG_VOLUME:
			case GLOBAL_CONFIG_BACKLIGHT:
			case GLOBAL_CONFIG_CONTRAST:
				switch(global_config_selection) {
					case GLOBAL_CONFIG_VOLUME: draw_text(chip8.periph.display, "=>", 0, 0+Y_ADJ); break;
					case GLOBAL_CONFIG_BACKLIGHT: draw_text(chip8.periph.display, "=>", 0, 9+Y_ADJ); break;
					case GLOBAL_CONFIG_CONTRAST: draw_text(chip8.periph.display, "=>", 0, 18+Y_ADJ); break;
					default: assert(false); break; // Should never happen!
				}
				draw_text(chip8.periph.display, "4)     6)", 0, 47+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ADJ_LESS, 12, 47);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_ADJ_MORE, 54, 47);
				draw_text(chip8.periph.display, "F)     X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_SAVE, 12, 56);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 54, 56);
			break;
			case GLOBAL_CONFIG_LANGUAGE:
				draw_text(chip8.periph.display, "=>", 0, 27+Y_ADJ);
				draw_text(chip8.periph.display, "1)EN 2)TOK 3)", 0, 47+Y_ADJ);
				draw_bitmap_h8(chip8.periph.display, ICON_LANG_SP, ICON_LANG_SP_LEN, 78, 47+Y_ADJ_SP);

				draw_text(chip8.periph.display, "4)", 0, 56+Y_ADJ);
				draw_bitmap_h8(chip8.periph.display, ICON_LANG_QSS, ICON_LANG_QSS_LEN, 12, 56);
				draw_text(chip8.periph.display, "X)", 36, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 48, 56);
			break;
			case GLOBAL_CONFIG_BOOT_ROM:
				draw_text(chip8.periph.display, "=>", 0, 36+Y_ADJ);
				draw_text(chip8.periph.display, "F)", 0, 47+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_GLBC_CONFIRM_CLR_BOOTROM, 12, 47);
				draw_text(chip8.periph.display, "X)", 0, 56+Y_ADJ);
				draw_translated(chip8.periph.display, global_config.language, TR_MSG_CANCEL, 12, 56);
			break;
		}
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_pre_gameplay_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	if(button_press & (1<<0x10)) {
		menu_display_update_required = true;
		screen_state = SCREEN_MENU;
	} else if(button_press || systick_now - last_frame_processed_tick >= FUNCONF_SYSTEM_CORE_CLOCK/1000*GAMEPLAY_INSTRUCTION_DURATION_MS) {
		wait_button_release();
		screen_state = SCREEN_GAMEPLAY;
	}
	if(screen_state != SCREEN_PRE_GAMEPLAY) {
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_PGP_CONTROLS, 64-(draw_get_translated_width(global_config.language, TR_MSG_PGP_CONTROLS)/2), 0);
		char button_str[17];
		if(chip8_cfg->input_navigation) {
			util_print_button_buffer(button_str, chip8_cfg->input_navigation);
			uint8_t x = DISPLAY_WIDTH/2-(ICON_NAVIGATION_LEN+1+6*strlen(button_str))/2;
			draw_bitmap_h8(chip8.periph.display, ICON_NAVIGATION, ICON_NAVIGATION_LEN, x, 10);
			draw_text(chip8.periph.display, button_str, x+ICON_NAVIGATION_LEN+1, 11);
		}
		if(chip8_cfg->input_action) {
			util_print_button_buffer(button_str, chip8_cfg->input_action);
			uint8_t x = DISPLAY_WIDTH/2-(ICON_ACTION_LEN+1+6*strlen(button_str))/2;
			draw_bitmap_h8(chip8.periph.display, ICON_ACTION, ICON_ACTION_LEN, x, 20);
			draw_text(chip8.periph.display, button_str, x+ICON_ACTION_LEN+1, 21);
		}
		if(chip8_cfg->input_replay) {
			util_print_button_buffer(button_str, chip8_cfg->input_replay);
			uint8_t x = DISPLAY_WIDTH/2-(ICON_REPLAY_LEN+1+6*strlen(button_str))/2;
			draw_bitmap_h8(chip8.periph.display, ICON_REPLAY, ICON_REPLAY_LEN, x, 30);
			draw_text(chip8.periph.display, button_str, x+ICON_REPLAY_LEN+1, 31);
		}
		switch(chip8_cfg->input_layout) {
			case CHIP8_LAYOUT_QWERTY:
				draw_text(chip8.periph.display, "123C  1234", 34, 40);
				draw_text(chip8.periph.display, "456D  QWER", 34, 40+6);
				draw_text(chip8.periph.display, "789E  ASDF", 34, 40+12);
				draw_text(chip8.periph.display, "A0BF  ZXCV", 34, 40+18);
				draw_text(chip8.periph.display, "=", 64-3, 40+9);
			break;
			case CHIP8_LAYOUT_HP48:
				draw_text(chip8.periph.display, "123C  789/", 34, 40);
				draw_text(chip8.periph.display, "456D  456X", 34, 40+6);
				draw_text(chip8.periph.display, "789E  123-", 34, 40+12);
				draw_text(chip8.periph.display, "A0BF  0.S+", 34, 40+18);
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
				apply_volume(chip8.periph.sound_timer > 0);
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

		bool user_exit = game_paused && (game_is_bootrom || systick_now-game_paused_start_tick >= FUNCONF_SYSTEM_CORE_CLOCK/1000*GAMEPLAY_EXIT_DURATION_MS);
		if(user_exit || chip8.periph.requests & CHIP8_REQUEST_HALT_MASK) {
			apply_volume(false); // Must mute before exiting the game or the sound might never stop

			if(memcmp(last_storage_flag, chip8.periph.storage_flags, sizeof(chip8.periph.storage_flags))) {
				// Storage flag changed. Let's save it!
				file_io_result = file_save_storage_flag(chip8.periph.storage_flags, sizeof(chip8.periph.storage_flags));
				// file content changed. file tree of the dircectory may be changed. Need to invalidate cache
				// (Actually as of the time of writing the menu_cache_invalidated is already set true upon game load.
				// Still, I'd be better off setting it here again for now because the assumption above might not hold true in the future)
				menu_cache_invalidated = true;
			}

			menu_display_update_required = true;
			if(file_io_result != FR_OK) {
				screen_state = SCREEN_ERROR;
			} else if(user_exit) {
				screen_state = SCREEN_MENU;
			} else if (chip8.periph.requests & CHIP8_REQUEST_HALT_EXIT_EMULATOR) {
				if(game_is_bootrom) {
					// Clear the screen immediately
					// because it takes a visible split-second to load the menu
					draw_clear(chip8.periph.display);
					lcd_transfer_begin(chip8.periph.display);
					screen_state = SCREEN_MENU;
				} else {
					screen_state = SCREEN_GAMEOVER;
				}
			} else {
				screen_state = SCREEN_GAME_CRASHED;
			}
			game_is_bootrom = false; // We're ending the game. Whichever next ROM being loaded won't be bootrom anymore.
			return;
		}
		last_frame_processed_tick = systick_now;
	}

	if(systick_now - last_lcd_blit_tick >= FUNCONF_SYSTEM_CORE_CLOCK/60) { // 60fps
		if(!game_paused) {
			chip8_timer_step(&chip8);
			apply_volume(chip8.periph.sound_timer > 0);
		} else {
			apply_volume(false); // Do not play any sound when the game's paused
			// Display exit countdown
			draw_clear_row(chip8.periph.display, GAMEPLAY_EXIT_BANNER_ROW_POS);
			draw_clear_row(chip8.periph.display, GAMEPLAY_EXIT_BANNER_ROW_POS+1);

			uint8_t w = draw_get_translated_width(global_config.language, TR_MSG_GP_HOLDTOQUIT);
			uint8_t x = 64-(w+12)/2;
			draw_translated(chip8.periph.display, global_config.language, TR_MSG_GP_HOLDTOQUIT, x, 28);
			uint8_t digit = (GAMEPLAY_EXIT_DURATION_MS/1000 - (systick_now-game_paused_start_tick)/FUNCONF_SYSTEM_CORE_CLOCK);
			char str[2]; str[0] = '0' + digit; str[1] = '\0';
			draw_text(chip8.periph.display, str, x+w+6, 28+Y_ADJ);
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
		menu_display_update_required = true;
		screen_state = SCREEN_MENU;
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_GAMEOVER,
			64-draw_get_translated_width(global_config.language, TR_MSG_GAMEOVER)/2, 22);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_GAMEOVER_PROCEED,
			64-draw_get_translated_width(global_config.language, TR_MSG_GAMEOVER_PROCEED)/2, 32);
		lcd_transfer_begin(chip8.periph.display);
		menu_display_update_required = false;
	}
}

static void screen_game_crashed_handler(void) {
	uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
	if(button_press & (1<<0x10)) {
		menu_display_update_required = true;
		screen_state = SCREEN_MENU;
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		// Intentionally untranslated
		// There isn't any appropriate toki pona words for the errors
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
		menu_display_update_required = true;
		screen_state = SCREEN_MENU;
		return;
	}

	if(menu_display_update_required) {
		draw_clear(chip8.periph.display);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_FWU_OK,
			64-draw_get_translated_width(global_config.language, TR_MSG_FWU_OK)/2, 22);
		draw_translated(chip8.periph.display, global_config.language, TR_MSG_FWU_PROCEED,
			64-draw_get_translated_width(global_config.language, TR_MSG_FWU_PROCEED)/2, 32);
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
	apply_volume(false); // Always use mute the buzzer at the beginning
	apply_brightness();
	apply_contrast();

	watchdog_feed();

	static_assert(sizeof(struct shared_buffer) <= sizeof(chip8.mem));
	bulkmem = (struct shared_buffer*)chip8.mem;
	chip8_cfg = &union_buffer.chip8_cfg;
	game_paused_screen_buffer_backup = union_buffer.game_paused_screen_buffer_backup;

	// Shared by all screens except for SCREEN_GAMEPLAY
	menu_display_update_required = true;

	// For SCREEN_MENU
	menu_file_list = bulkmem->menu_file_list;
	menu_current_dir[0] = '\0';
	menu_offset = 0;
	menu_file_count_of_current_page = 0;
	menu_cache_invalidated = true;
	file_io_result = FR_OK;

	// For SCREEN_GAME_CONFIG
	game_config_selection = GAME_CONFIG_MAIN;
	game_config_index = 0;
	game_config_old_value = 0;

	// For SCREEN_GAMEPLAY
	game_min_cycle_interval = 0;
	last_frame_processed_tick = SysTick->CNT;
	last_lcd_blit_tick = SysTick->CNT;
	game_is_bootrom = false;

	// Verify firmware update content
	file_io_result = file_verify_firmware_update();

	switch(file_io_result) {
		case FR_OK:
			screen_state = SCREEN_FW_UPDATE_OK;
		break;
		case FR_FIRMWARE_VERIFICATION_ERROR:
			screen_state = SCREEN_ERROR;
		break;
		case FR_NO_FILE: // Firmware update file not found, which's a perfectly normal case. Gotta suppress this error
			file_io_result = FR_OK;
		default: // Fallthrough. (default is "other error occurred")
			// For the case of "other error had occurred", there's no need to jump to SCREEN_ERROR for now
			// After the bootrom exits, the error would be shown
			// There's a slight chance that the file_io_result would be overwritten but that's ok
			// because we're already having io error. Chances are that it'd just be replaced by another error
			if(bootrom_load(chip8_cfg, &chip8)) {
				prepare_game_launch();
				game_is_bootrom = true;
				screen_state = SCREEN_GAMEPLAY;
			} else {
				screen_state = SCREEN_MENU;
			}
		break;
	}

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
