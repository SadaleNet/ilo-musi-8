// Copyright 2025 Wong Cho Ching <https://sadale.net>
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

#include <stdint.h>

#define CHIP8_PROGRAM_START_OFFSET (0x200U)
#define CHIP8_MEMORY_SIZE (4096U)
#define CHIP8_PC_STACK_SIZE (16U) // 12 for original CHIP, 16 for SuperCHIP. Let's support 16. There's no harm.

#define CHIP8_DISPLAY_WIDTH (128U)
#define CHIP8_DISPLAY_HEIGHT (64U)
#define CHIP8_AUDIO_BUFFER_SIZE (16U)

#define CHIP8_QUIRK_SHIFT (1U<<0)
#define CHIP8_QUIRK_MEMORY_LEAVE_I_UNCHANGED (1U<<1)
#define CHIP8_QUIRK_MEMORY_INCREASE_BY_X (1U<<2)
#define CHIP8_QUIRK_WRAP (1U<<3)
#define CHIP8_QUIRK_JUMP (1U<<4)
#define CHIP8_QUIRK_VBLANK (1U<<5)
#define CHIP8_QUIRK_LOGIC (1U<<6)

#define CHIP8_QUIRK_LORES_WIDE_SPRITE (1U<<7)
#define CHIP8_QUIRK_LORES_TALL_SPRITE (1U<<8) // Also draws 8x16 in hires
#define CHIP8_QUIRK_LORES_SCROLL_DIV2 (1U<<9)
#define CHIP8_QUIRK_HIRES_COLLISION (1U<<10)
#define CHIP8_QUIRK_RESIZE_CLEAR_SCREEN (1U<<11)
#define CHIP8_QUIRK_VF_ORDER (1U<<12) // No clue on what it does. Unimplemented.

struct chip8_cpu {
	uint8_t pc_index:4;
	uint8_t halt:1;
	uint16_t i;
	uint32_t quirks;
	uint8_t v[16];
	uint16_t pc[CHIP8_PC_STACK_SIZE];
};

#define CHIP8_REQUEST_WAIT_DISPLAY_REFRESH (1U << 0)
#define CHIP8_REQUEST_AUDIO_BUFFER_UPDATED (1U << 1)
#define CHIP8_REQUEST_AUDIO_PITCH_UPDATED (1U << 2)
#define CHIP8_REQUEST_HALT_EXIT_EMULATOR (1U << 24) // Received instruction to exit the emulator
#define CHIP8_REQUEST_HALT_I_ERROR (1U << 25) // I overread/overflow
#define CHIP8_REQUEST_HALT_STACK_ERROR (1U << 26) // stack overflow/underflow
#define CHIP8_REQUEST_HALT_PC_ERROR (1U << 27) // PC overflow
#define CHIP8_REQUEST_HALT_INVALID_INSTRUCTION (1U << 28) //Invalid instruction
#define CHIP8_REQUEST_HALT_MASK (0xFF000000)

enum chip8_input_layout {
	CHIP8_LAYOUT_STANDARD,
	CHIP8_LAYOUT_QWERTY,
	CHIP8_LAYOUT_COUNT,
};

struct chip8_periph {
	uint8_t delay_timer;
	uint8_t sound_timer;
	uint16_t key_held;
	uint16_t key_just_released;
	uint8_t high_res;
	uint8_t random_num;
	uint8_t audio_pitch; // sample rate: 4000*(2**((audio_pitch-64)/48)) Hz
	uint8_t speed; // Max execution speed in cycle per frame. fps is always 60Hz.
	uint32_t requests;
	uint8_t audio[CHIP8_AUDIO_BUFFER_SIZE];
	uint8_t display[CHIP8_DISPLAY_HEIGHT*CHIP8_DISPLAY_WIDTH/8]; // column-major, first column is leftmost. Each column is 64bit, the top bit is LSB.
	uint8_t storage_flags[16];
};

struct chip8_machine {
	struct chip8_cpu cpu; // contains CPU state that's read-only by the external code (not enforced!)
	struct chip8_periph periph; // contains variables that can be both read and written by external code
	uint8_t mem[CHIP8_MEMORY_SIZE]; // Upon run, external code load the program to chip8.mem[CHIP8_PROGRAM_START_OFFSET] with size of CHIP8_MEMORY_SIZE-CHIP8_PROGRAM_START_OFFSET.
};

#define CHIP8_CFG_VERSION (0) // Used only for boot ROM. For INI file, assume latest version

struct chip8_config {
	// To update the struct version, you can only append fields to the end and then increment the version
	uint32_t version;
	uint8_t font[16*5];
	uint8_t font_highres[16*10];
	uint8_t audio[CHIP8_AUDIO_BUFFER_SIZE];
	uint8_t storage_flags[16];
	uint32_t quirks;
	uint8_t speed; // Unit: cycle per frame. fps is always 60 Hz.
	uint8_t input_layout; // Datatype: enum chip8_input_layout. The keyboard layout. For showing on SCREEN_PRE_GAMEPLAY. No actual in-game function.
	uint16_t input_navigation; // Buttons for navigation. For showing on SCREEN_PRE_GAMEPLAY. No actual in-game function.
	uint16_t input_action; // Buttons for control. Ditto.
	uint16_t input_replay; // Buttons for replay. Ditto.
};

void chip8_step(struct chip8_machine *machine);
void chip8_timer_step(struct chip8_machine *machine);
void chip8_init(struct chip8_machine *machine, const struct chip8_config *config);

#define CHIP8_QUIRK_PLATFORM_VIP (CHIP8_QUIRK_VBLANK|CHIP8_QUIRK_LOGIC) // 0x00000060
#define CHIP8_QUIRK_PLATFORM_SCHIP (CHIP8_QUIRK_SHIFT|CHIP8_QUIRK_MEMORY_LEAVE_I_UNCHANGED|CHIP8_QUIRK_JUMP|CHIP8_QUIRK_HIRES_COLLISION)  // 0x00000413
#define CHIP8_QUIRK_PLATFORM_OCTO (CHIP8_QUIRK_WRAP|CHIP8_QUIRK_LORES_WIDE_SPRITE|CHIP8_QUIRK_RESIZE_CLEAR_SCREEN) // 0x00000888

// 500Hz squarewave, pulse width: 8 samples, 50% duty cycle
#define CHIP8_DEFAULT_AUDIO_SAMPLE ((uint8_t[CHIP8_AUDIO_BUFFER_SIZE]){0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0})
#define CHIP8_DEFAULT_AUDIO_PITCH (64U)

extern const struct chip8_config CHIP8_CFG_DEFAULT;
