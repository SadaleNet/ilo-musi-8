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

#define CYCLE_PER_FRAME (200) // Benchmark without buzzer: 8100fps max, which's 135 cycles per frame

#define CHIP8_QUIRK_PLATFORM_VIP (CHIP8_QUIRK_VBLANK|CHIP8_QUIRK_LOGIC)
#define CHIP8_QUIRK_PLATFORM_SCHIP (CHIP8_QUIRK_SHIFT|CHIP8_QUIRK_MEMORY_LEAVE_I_UNCHANGED|CHIP8_QUIRK_JUMP|CHIP8_QUIRK_HIRES_COLLISION)
#define CHIP8_QUIRK_PLATFORM_XOCHIP (CHIP8_QUIRK_WRAP|CHIP8_QUIRK_LORES_WIDE_SPRITE|CHIP8_QUIRK_RESIZE_CLEAR_SCREEN)
static struct chip8_machine chip8;

static uint16_t chip8_keymap(uint16_t button_state) {
	// Converts from the left layout to the right layout
	// [0]  [1]  [2]  [3]         [1] [2] [3] [C]
	// [4]  [5]  [6]  [7]         [4] [5] [6] [D]
	// [8]  [9]  [10] [11]        [7] [8] [9] [E]
	// [12] [13] [14] [15]        [A] [0] [B] [F]
	uint16_t ret = 0;
	const unsigned int BUTTON_MAP[] = {13, 0, 1, 2, 4, 5, 6, 8, 9, 10,
		12, 14, 3, 7, 11, 15};
	for(int i=0; i<sizeof(BUTTON_MAP)/sizeof(*BUTTON_MAP); i++) {
		if(button_state & (1<<BUTTON_MAP[i])) {
			ret |= 1<<i;
		}
	}
	return ret;
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

	uint8_t buzzer_volume = 3; // Just make up a value for testing
	buzzer_set_volume(0);
	buzzer_set_buffer(chip8.periph.audio);
	buzzer_set_pitch(chip8.periph.audio_pitch);

	watchdog_feed();

	printf("%u\n", file_load_rom("GAME.CH8", &chip8));

	uint32_t last_frame_processed_tick = SysTick->CNT;
	uint32_t last_lcd_blit_tick = SysTick->CNT;
	while(1) {
		file_loop();

		uint32_t systick_now = SysTick->CNT;
		if(systick_now - last_frame_processed_tick >= FUNCONF_SYSTEM_CORE_CLOCK/60/CYCLE_PER_FRAME) { // (60 x CYCLE_PER_FRAME) fps
			chip8.periph.random_num = SysTick->CNT;
			chip8.periph.key_held = chip8_keymap(adc_button_get_state());
			chip8.periph.key_just_released = chip8_keymap(adc_button_get_just_released());
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

			if(chip8.periph.requests & CHIP8_REQUEST_HALT_MASK) {
				while(true);
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

		watchdog_feed();
	}
}
