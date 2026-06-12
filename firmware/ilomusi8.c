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
#include "draw.h"
#include "file.h"
#include "lcd.h"
#include "spi.h"
#include "tim1_pwm.h"
#include "watchdog.h"

#include "fatfs/ff.h"
#include "ch32fun.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define CHIP8_QUIRK_PLATFORM_VIP (CHIP8_QUIRK_VBLANK|CHIP8_QUIRK_LOGIC) // 0x00000060
#define CHIP8_QUIRK_PLATFORM_SCHIP (CHIP8_QUIRK_SHIFT|CHIP8_QUIRK_MEMORY_LEAVE_I_UNCHANGED|CHIP8_QUIRK_JUMP|CHIP8_QUIRK_HIRES_COLLISION)  // 0x00000413
#define CHIP8_QUIRK_PLATFORM_OCTO (CHIP8_QUIRK_WRAP|CHIP8_QUIRK_LORES_WIDE_SPRITE|CHIP8_QUIRK_RESIZE_CLEAR_SCREEN) // 0x00000888

extern const uint8_t ICON_NAVIGATION[];
extern const size_t ICON_NAVIGATION_LENGTH;
extern const uint8_t ICON_GAMECONF[];
extern const size_t ICON_GAMECONF_LENGTH;
extern const uint8_t ICON_GLOBALCONF[];
extern const size_t ICON_GLOBALCONF_LENGTH;
extern const uint8_t ICON_PLAY[];
extern const size_t ICON_PLAY_LENGTH;
extern const uint8_t ICON_ACTION[];
extern const size_t ICON_ACTION_LENGTH;
extern const uint8_t ICON_UPDIR[];
extern const size_t ICON_UPDIR_LENGTH;


enum screen_state {
	SCREEN_ERROR, // File IO Error Screen
	SCREEN_MENU,
	SCREEN_GAME_CONFIG, // quirks, frame limit, flash to boot rom
	SCREEN_GLOBAL_CONFIG,
	SCREEN_GAMEPLAY,
};

enum game_config_selection {
	GAME_CONFIG_MAIN,
	GAME_CONFIG_QUIRKS,
	GAME_CONFIG_QUIRKS_CUSTOM,
	GAME_CONFIG_SPEED,
	GAME_CONFIG_BOOT_ROM,
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
	size_t current_dir_length = strlen(directory_str);
	// TODO: this function's unsafe. Need boundry check.
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

	enum screen_state screen_state = SCREEN_MENU;

	uint8_t buzzer_volume = 15; // Just make up a value for testing
	buzzer_set_volume(buzzer_volume);

	watchdog_feed();

	assert(sizeof(struct shared_buffer) <= sizeof(chip8.mem));
	bulkmem = (struct shared_buffer*)chip8.mem;
	chip8_cfg = &bulkmem->chip8_cfg;

	// Shared by SCREEN_ERROR, SCREEN_MENU, SCREEN_GAME_CONFIG, SCREEN_GLOBAL_CONFIG
	bool menu_display_update_required = true;

	// For SCREEN_MENU
	char (*menu_file_list)[14] = bulkmem->menu_file_list;
	char *menu_current_dir = bulkmem->menu_current_dir;
	menu_current_dir[0] = '\0';
	int menu_offset = 0;
	FRESULT file_io_result = FR_OK;
	size_t menu_file_count_of_current_page = 0;
	bool menu_dir_reload_required = true;

	// For SCREEN_GAME_CONFIG
	enum game_config_selection game_config_selection = GAME_CONFIG_MAIN;
	uint8_t game_config_index = 0;
	uint32_t game_config_old_value = 0;


	uint32_t game_min_cycle_interval = 0;
	uint32_t last_frame_processed_tick = SysTick->CNT;
	uint32_t last_lcd_blit_tick = SysTick->CNT;
	while(1) {
		file_loop();

		uint32_t systick_now = SysTick->CNT;
		switch(screen_state) {
			case SCREEN_ERROR:
			{
				if(chip8_keymap(adc_button_get_just_pressed())) {
					menu_current_dir[0] = '\0';
					menu_offset = 0;
					menu_dir_reload_required = true;
					screen_state = SCREEN_MENU;
					break;
				}
				if(menu_display_update_required) {
					while(lcd_is_transfer_in_progress()){}
					draw_clear(chip8.periph.display);
					draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 0);
					draw_text(chip8.periph.display, "CARD ERROR #", 0, 20);
					char errorcode[3] = {0};
					errorcode[0] = file_io_result/10 + '0';
					errorcode[1] = file_io_result%10 + '0';
					errorcode[2] = '\0';
					draw_text(chip8.periph.display, errorcode, 6*12, 20);
					switch(file_io_result) {
						case FR_NOT_READY:
							draw_text(chip8.periph.display, "NO CARD", 0, 30);
						break;
						case FR_NO_FILESYSTEM:
							draw_text(chip8.periph.display, "FILESYSTEM ERROR", 0, 30);
							draw_text(chip8.periph.display, "REQUIRES FAT16/FAT32", 0, 40);
						break;
						case FR_INI_PARSE_ERROR:
							draw_text(chip8.periph.display, "INVALID CONFIG INI", 0, 30);
						break;
						case FR_VOLUME_FULL:
							draw_text(chip8.periph.display, "VOLUME FULL", 0, 30);
						break;
						default:
						break;
					}
					draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 58);
					lcd_transfer_begin(chip8.periph.display);
					menu_display_update_required = false;
				}
			}
			break;
			case SCREEN_MENU:
			{
				int menu_offset_prev = menu_offset;
				uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
				size_t menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
				if((button_press & (1<<0xC)) && menu_file_count_of_current_page > 0) { // The C button. Only usable for non-empty directories
					if(strlen(menu_file_list[menu_offset_on_current_page]) > 0 && menu_file_list[menu_offset_on_current_page][strlen(menu_file_list[menu_offset_on_current_page])-1] != '/') {
						// Load the INI file into chip8_cfg, then restore menu_current_dir's content
						directory_attach_filename(menu_current_dir, menu_file_list[menu_offset_on_current_page]);
						memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "INI", 3);
						file_io_result = file_load_config(menu_current_dir, chip8_cfg);
						directory_remove_filename(menu_current_dir);

						if(file_io_result == FR_NO_FILE) {
							// It's ok to have the INI file missing
							// The default config would be loaded and
							// we'll create the config upon it's saved
							file_io_result = FR_OK;
						}

						screen_state = (file_io_result == FR_OK) ? SCREEN_GAME_CONFIG : SCREEN_ERROR;
						menu_display_update_required = true;
						break;
					}
				} else if((button_press & (1<<0xF)) && menu_file_count_of_current_page > 0) { // The F button. Only usable for non-empty directories
					// Attach the filename to the current menu_current_dir
					directory_attach_filename(menu_current_dir, menu_file_list[menu_offset_on_current_page]);

					if(strlen(menu_file_list[menu_offset_on_current_page]) > 0 && menu_file_list[menu_offset_on_current_page][strlen(menu_file_list[menu_offset_on_current_page])-1] == '/') {
						// Enter the directory
						menu_offset = 0;
						menu_dir_reload_required = true;
					} else {
						// Load INI config
						memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "INI", 3); // replace file extension to .INI
						file_io_result = file_load_config(menu_current_dir, chip8_cfg);
						if (file_io_result == FR_NO_FILE) {
							// Ignore INI file missing error.
							// The default config would be loaded in this case
							file_io_result = FR_OK;
						}

						if(file_io_result == FR_OK) {
							// Config file loaded successfully. Let's try loading the game!
							memcpy(&menu_current_dir[strlen(menu_current_dir)-3], "CH8", 3); // resume file extension of .CH8
							file_io_result = file_load_rom(menu_current_dir, chip8_cfg, &chip8);
							if(file_io_result == FR_OK) {
								// Get rid of all button press events
								adc_button_get_just_pressed();
								adc_button_get_just_released();
								// Initialize peripheral variables
								buzzer_set_buffer(chip8.periph.audio);
								buzzer_set_pitch(chip8.periph.audio_pitch);
								if(chip8_cfg->speed == 0) {
									game_min_cycle_interval = 0; // Unlimited framerate
								} else {
									game_min_cycle_interval = FUNCONF_SYSTEM_CORE_CLOCK/60/chip8_cfg->speed;
								}
								// TODO: show control and layout information before launching the game
								// Start the game!
								screen_state = SCREEN_GAMEPLAY;
							} else {
								// Failed to load the game.
								// Do nothing. Just wait for error handling for file_io_result != FR_OK
							}
						}
						directory_remove_filename(menu_current_dir);
						if(screen_state != SCREEN_MENU) {
							break;
						}
					}
				} else if(button_press & (1<<0x10)) { // The X button
					// Up a directory
					if(strlen(menu_current_dir) >= 1) {
						// Remove the trailing slash
						menu_current_dir[strlen(menu_current_dir)-1] = '\0';
						// Look for the next trailing slash, then make it \0 for upping a directory level
						directory_remove_filename(menu_current_dir);
					}
					// Always reload directory so that the user would have visual feedback
					menu_offset = 0;
					menu_dir_reload_required = true;
				} else {
					if(button_press & (1<<2)) { menu_offset--; }
					if(button_press & (1<<8)) { menu_offset++; }
					if(button_press & (1<<4)) { menu_offset -= 10; }
					if(button_press & (1<<6)) { menu_offset += 10; }
				}

				if(menu_offset < 0) {
					menu_offset = 0;
				}

				if(menu_offset != menu_offset_prev) {
					menu_display_update_required = true;
				}

				if(menu_dir_reload_required || menu_offset/MENU_PAGE_SIZE != menu_offset_prev/MENU_PAGE_SIZE) {
					while(lcd_is_transfer_in_progress()){}
					draw_clear(chip8.periph.display);
					lcd_transfer_begin(chip8.periph.display);

					for(size_t i=0; i<2; i++) {
						menu_file_count_of_current_page = MENU_PAGE_SIZE;
						file_io_result = file_readdir(menu_current_dir, menu_offset/MENU_PAGE_SIZE*MENU_PAGE_SIZE, menu_file_list, &menu_file_count_of_current_page);
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
					break;
				}

				menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
				// Prevent selection of empty entries
				if(menu_offset_on_current_page > menu_file_count_of_current_page-1) {
					menu_offset = menu_offset/MENU_PAGE_SIZE*MENU_PAGE_SIZE + menu_file_count_of_current_page-1;
					menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
				}

				// Render the menu
				if(menu_display_update_required) {
					while(lcd_is_transfer_in_progress()){}
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
			break;
			case SCREEN_GAME_CONFIG:
			{
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

							screen_state = SCREEN_MENU;
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
					break;
				}

				if(menu_display_update_required) {
					while(lcd_is_transfer_in_progress()){}
					draw_clear(chip8.periph.display);
					draw_text(chip8.periph.display, "CONFIG INI", 0, 0);
					draw_text(chip8.periph.display, menu_file_list[menu_offset%MENU_PAGE_SIZE], 66, 0);

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
							draw_text(chip8.periph.display, "F)SAVE", 0, 48);
							draw_text(chip8.periph.display, "X)CANCEL", 0, 57);
						break;
						case GAME_CONFIG_QUIRKS:
							draw_text(chip8.periph.display, "=>", 0, 14);
							draw_text(chip8.periph.display, "1)VIP 2)SCHIP 3)OCTO", 0, 48);
							draw_text(chip8.periph.display, "4)CUSTOM X)CANCEL", 0, 57);
						break;
						case GAME_CONFIG_QUIRKS_CUSTOM:
							draw_text(chip8.periph.display, "=>", 0, 14);
							draw_text(chip8.periph.display, "0-F)TYPE HEX", 0, 48);
							draw_text(chip8.periph.display, "X)CANCEL", 0, 57);
						break;
						case GAME_CONFIG_SPEED:
							draw_text(chip8.periph.display, "=>", 0, 23);
							draw_text(chip8.periph.display, "0-9)TYPE DIGITS", 0, 48);
							draw_text(chip8.periph.display, "X)CANCEL", 0, 57);
						break;
						case GAME_CONFIG_BOOT_ROM:
							draw_text(chip8.periph.display, "=>", 0, 32);
							draw_text(chip8.periph.display, "F)OVERWRITE BOOT ROM", 0, 48);
							draw_text(chip8.periph.display, "X)CANCEL", 0, 57);
						break;
					}
					lcd_transfer_begin(chip8.periph.display);
					menu_display_update_required = false;
				}
			}
			break;
			case SCREEN_GLOBAL_CONFIG:
			{
					// TODO: Unimplemented
			}
			break;
			case SCREEN_GAMEPLAY:
			{
				if(systick_now - last_frame_processed_tick >= game_min_cycle_interval) { // (60 x CYCLES_PER_FRAME) fps
					chip8.periph.random_num = SysTick->CNT;
					chip8.periph.key_held = (uint16_t)chip8_keymap(adc_button_get_state());
					chip8.periph.key_just_released = (uint16_t)chip8_keymap(adc_button_get_just_released());
					if(!(chip8.periph.requests & CHIP8_REQUEST_WAIT_DISPLAY_REFRESH)) {
						chip8_step(&chip8);
						buzzer_set_volume(chip8.periph.sound_timer > 0 ? buzzer_volume : 0);
						if(chip8.periph.requests & CHIP8_REQUEST_AUDIO_BUFFER_UPDATED) {
							buzzer_set_buffer(chip8.periph.audio);
							chip8.periph.requests &= ~CHIP8_REQUEST_AUDIO_BUFFER_UPDATED;
						}
						if(chip8.periph.requests & CHIP8_REQUEST_AUDIO_PITCH_UPDATED) {
							buzzer_set_pitch(chip8.periph.audio_pitch);
							chip8.periph.requests &= ~CHIP8_REQUEST_AUDIO_PITCH_UPDATED;
						}
					}

					uint32_t button_just_pressed = chip8_keymap(adc_button_get_just_pressed());
					if((button_just_pressed & (1<<0x10)) || chip8.periph.requests & CHIP8_REQUEST_HALT_MASK) {
						menu_dir_reload_required = true;
						buzzer_set_volume(0);
						screen_state = SCREEN_MENU;
						break;
					}
					last_frame_processed_tick = systick_now;
				}

				if(systick_now - last_lcd_blit_tick >= FUNCONF_SYSTEM_CORE_CLOCK/60) { // 60fps
					chip8_timer_step(&chip8);
					buzzer_set_volume(chip8.periph.sound_timer > 0 ? buzzer_volume : 0);
					// There's no double-buffering for saving 1kB of RAM.
					// There still won't be tearing because the LCD's response time
					// is slow enough to have any tearing visible
					lcd_transfer_begin(chip8.periph.display);
					last_lcd_blit_tick = systick_now;
					chip8.periph.requests &= ~CHIP8_REQUEST_WAIT_DISPLAY_REFRESH;
				}
			}
			break;
		}


		watchdog_feed();
	}
}
