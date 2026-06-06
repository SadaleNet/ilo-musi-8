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

#define CYCLE_PER_FRAME (99) // Benchmark: 5500fps max, which's 90 cycles per frame

#define CHIP8_QUIRK_PLATFORM_VIP (CHIP8_QUIRK_VBLANK|CHIP8_QUIRK_LOGIC)
#define CHIP8_QUIRK_PLATFORM_SCHIP (CHIP8_QUIRK_SHIFT|CHIP8_QUIRK_MEMORY_LEAVE_I_UNCHANGED|CHIP8_QUIRK_JUMP|CHIP8_QUIRK_HIRES_COLLISION)
#define CHIP8_QUIRK_PLATFORM_XOCHIP (CHIP8_QUIRK_WRAP|CHIP8_QUIRK_LORES_WIDE_SPRITE|CHIP8_QUIRK_RESIZE_CLEAR_SCREEN)

enum {
	SCREEN_ERROR,
	SCREEN_MENU,
	SCREEN_GAMEPLAY,
} screen_state;

static struct chip8_machine chip8;

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

	screen_state = SCREEN_MENU;

	uint8_t buzzer_volume = 3; // Just make up a value for testing
	buzzer_set_volume(buzzer_volume);
	buzzer_set_buffer(chip8.periph.audio);
	buzzer_set_pitch(chip8.periph.audio_pitch);

	watchdog_feed();

	#define MENU_PAGE_SIZE (10)
	static char menu_file_list[MENU_PAGE_SIZE][14];
	char menu_current_dir[256] = {0};
	int menu_offset = 0;
	FRESULT file_io_result = FR_OK;
	size_t menu_file_count_of_current_page = 0;
	bool menu_dir_reload_required = true;
	bool menu_display_update_required = true;
	bool error_screen_rendered = false;

	uint32_t last_frame_processed_tick = SysTick->CNT;
	uint32_t last_lcd_blit_tick = SysTick->CNT;
	while(1) {
		file_loop();

		switch(screen_state) {
			case SCREEN_ERROR:
			{
				if(!error_screen_rendered) {
					while(lcd_is_transfer_in_progress()){}
					draw_clear(chip8.periph.display);
					draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 0);
					draw_text(chip8.periph.display, "XXXXXXXXXXXXXXXXXXXXX", 0, 58);
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
						default:
						break;
					}
					lcd_transfer_begin(chip8.periph.display);
				}
				if(chip8_keymap(adc_button_get_just_pressed())) {
					menu_current_dir[0] = '\0';
					menu_offset = 0;
					menu_dir_reload_required = true;
					screen_state = SCREEN_MENU;
				}
			}
			break;
			case SCREEN_MENU:
			{
				int menu_offset_prev = menu_offset;
				uint32_t button_press = chip8_keymap(adc_button_get_just_pressed());
				if(button_press & (1<<0xF)) { // The F button
					size_t menu_offset_on_current_page = menu_offset%MENU_PAGE_SIZE;
					// Attach the filename to the current menu_current_dir
					directory_attach_filename(menu_current_dir, menu_file_list[menu_offset_on_current_page]);

					if(strlen(menu_file_list[menu_offset_on_current_page]) > 0 && menu_file_list[menu_offset_on_current_page][strlen(menu_file_list[menu_offset_on_current_page])-1] == '/') {
						// Enter the directory
						menu_offset = 0;
						menu_dir_reload_required = true;
					} else {
						// Load game
						file_io_result = file_load_rom(menu_current_dir, &chip8);
						directory_remove_filename(menu_current_dir);
						if(file_io_result == FR_OK) {
							// Get rid of all button press events
							adc_button_get_just_pressed();
							adc_button_get_just_released();
							// Start the game!
							screen_state = SCREEN_GAMEPLAY;
						} else {
							 // Do nothing. Just wait for error handling for file_io_result != FR_OK
						}
					}
				} else if(button_press & (1<<0x10)) { // The X button
					// Up a directory
					if(strlen(menu_current_dir) >= 2) {
						// Remove the trailing slash
						menu_current_dir[strlen(menu_current_dir)-1] = '\0';
						// Look for the next trailing slash, then make it \0 for upping a directory level
						directory_remove_filename(menu_current_dir);
						menu_offset = 0;
						menu_dir_reload_required = true;
					}
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
							if(menu_file_count_of_current_page == 0) {
								// The new page's empty. It happens when we reached the end of the directory
								// Let's select the last entry of the previous page
								menu_offset = (menu_offset/MENU_PAGE_SIZE-1)*MENU_PAGE_SIZE +MENU_PAGE_SIZE-1;
								continue;
							}
							menu_display_update_required = true;
							menu_dir_reload_required = false;
							break;
						} else {
							break; // Skip to error handling mechanism
						}
					}
					// Get rid of all button press events after long operation
					adc_button_get_just_pressed();
				}

				if(file_io_result != FR_OK) {
					error_screen_rendered = false;
					screen_state = SCREEN_ERROR;
					break;
				}

				// Prevent selection of empty entries
				if(menu_offset%MENU_PAGE_SIZE > menu_file_count_of_current_page-1) {
					menu_offset = menu_offset/MENU_PAGE_SIZE*MENU_PAGE_SIZE + menu_file_count_of_current_page-1;
				}

				// Render the menu
				if(menu_display_update_required) {
					while(lcd_is_transfer_in_progress()){}
					draw_clear(chip8.periph.display);
					for(size_t i=0; i<menu_file_count_of_current_page; i++) {
						draw_text(chip8.periph.display, menu_file_list[i], 6, 6*i);
					}
					draw_text(chip8.periph.display, ">", 0, 6*(menu_offset%MENU_PAGE_SIZE));
					lcd_transfer_begin(chip8.periph.display);
					menu_display_update_required = false;
				}
			}

			break;
			case SCREEN_GAMEPLAY:
			{
				uint32_t systick_now = SysTick->CNT;
				if(systick_now - last_frame_processed_tick >= FUNCONF_SYSTEM_CORE_CLOCK/60/CYCLE_PER_FRAME) { // (60 x CYCLE_PER_FRAME) fps
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
						screen_state = SCREEN_MENU;
					}
					last_frame_processed_tick = systick_now;
				}

				if(systick_now - last_lcd_blit_tick >= FUNCONF_SYSTEM_CORE_CLOCK/60) { // 60fps
					chip8_timer_step(&chip8);
					buzzer_set_volume(chip8.periph.sound_timer > 0 ? buzzer_volume : 0);
					// There's no double-buffering to save 1kB of RAM.
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
