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

#include "chip8.h"
#include <string.h>
#include <assert.h>

const struct chip8_config CHIP8_CFG_DEFAULT = {
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
	.audio_pitch = CHIP8_DEFAULT_AUDIO_PITCH,
	.audio = CHIP8_DEFAULT_AUDIO_SAMPLE,
	.storage_flags = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.quirks = CHIP8_QUIRK_PLATFORM_OCTO,
	.speed = 0,
	.input_layout = 0,
	.input_navigation = 0,
	.input_action = 0,
	.input_replay = 0,
};

static uint8_t chip8_check_collision(uint8_t old, uint8_t new) {
	// Truth table:
	// old	new	out
	// 0	0	0
	// 0	1	0
	// 1	1	0
	// 1	0	1
	return (old ^ new) & old;
}

static void chip8_halt_cpu(struct chip8_machine *machine, uint32_t reasion_flag) {
	assert(reasion_flag == (reasion_flag & CHIP8_REQUEST_HALT_MASK));
	machine->cpu.halt = 1;
	machine->periph.requests |= reasion_flag;
}

void chip8_step(struct chip8_machine *machine) {
	#define CHIP8_HALT(condition, flag) \
		if(condition) { \
			chip8_halt_cpu(machine, flag); \
			return; \
		}

	struct chip8_cpu *cpu = &machine->cpu;
	struct chip8_periph *periph = &machine->periph;
	uint8_t *mem = machine->mem;
	uint8_t prevents_stepping = 0;
	if(cpu->halt) {
		// The CPU has stopped. Do not allow it to execute further instructions
		return;
	}

	// The +1 here is required because 2 bytes got read for each instruction.
	// For example, if CHIP8_MEMORY_SIZE is 0x1000, PC is 0xFFF, the machine would read 0xFFF and 0x1000, which means that overflow would occur.
	// PC can only point to 0xFFE at largest, which the machine would read 0xFFE and 0xFFF, which's within CHIP8_MEMORY_SIZE
	CHIP8_HALT(cpu->pc[cpu->pc_index]+1 >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_PC_ERROR);

	uint16_t instruction = mem[cpu->pc[cpu->pc_index]] << 8;
	instruction |= mem[cpu->pc[cpu->pc_index]+1];
	uint8_t *vx = &cpu->v[(instruction & 0x0F00)>>8];
	uint8_t *vy = &cpu->v[(instruction & 0x00F0)>>4];
	uint8_t *vf = &cpu->v[15];
	uint16_t *i = &cpu->i;

	switch(instruction & 0xF000) {
		case 0x0000:
			#define NEED_DOUBLE_SCROLL() (!periph->high_res && !(cpu->quirks & CHIP8_QUIRK_LORES_SCROLL_DIV2))
			switch(instruction & 0x00F0) {
				case 0x00C0: // 00CN Superchip
				{
					uint8_t shift = (instruction & 0x000F);
					if(NEED_DOUBLE_SCROLL()) {
						shift *= 2;
					}
					static_assert(CHIP8_DISPLAY_HEIGHT == 64);
					for(size_t x=0; x<CHIP8_DISPLAY_WIDTH; x++) {
						*((uint64_t*)&periph->display[x*CHIP8_DISPLAY_HEIGHT/8]) = *((uint64_t*)&periph->display[x*CHIP8_DISPLAY_HEIGHT/8]) << shift;
					}
				}
				break;
				case 0x00D0: // 00DN XO-Chip
				{
					uint8_t shift = (instruction & 0x000F);
					if(NEED_DOUBLE_SCROLL()) {
						shift *= 2;
					}
					static_assert(CHIP8_DISPLAY_HEIGHT == 64);
					for(size_t x=0; x<CHIP8_DISPLAY_WIDTH; x++) {
						*((uint64_t*)&periph->display[x*CHIP8_DISPLAY_HEIGHT/8]) = *((uint64_t*)&periph->display[x*CHIP8_DISPLAY_HEIGHT/8]) >> shift;
					}
				}
				break;
				default:
					switch(instruction & 0x00FF) {
						case 0x00E0: // 00E0
							memset(periph->display, 0, sizeof(periph->display));
						break;
						case 0x00EE: // 00EE
							CHIP8_HALT(cpu->pc_index <= 0, CHIP8_REQUEST_HALT_STACK_ERROR);
							cpu->pc_index--;
						break;
						case 0x00FB: // 00FB Superchip
						{
							uint8_t shift = NEED_DOUBLE_SCROLL() ? 8 : 4;
							for(size_t x=CHIP8_DISPLAY_WIDTH-1; x>=shift; x--) {
								for(size_t y=0; y<CHIP8_DISPLAY_HEIGHT/8; y++) {
									periph->display[x*CHIP8_DISPLAY_HEIGHT/8+y] = periph->display[(x-shift)*CHIP8_DISPLAY_HEIGHT/8+y];
								}
							}
							memset(periph->display, 0, shift*CHIP8_DISPLAY_HEIGHT/8);
						}
						break;
						case 0x00FC: // 00FC Superchip
						{
							uint8_t shift = NEED_DOUBLE_SCROLL() ? 8 : 4;
							for(size_t x=0; x<CHIP8_DISPLAY_WIDTH-shift; x++) {
								for(size_t y=0; y<CHIP8_DISPLAY_HEIGHT/8; y++) {
									periph->display[x*CHIP8_DISPLAY_HEIGHT/8+y] = periph->display[(x+shift)*CHIP8_DISPLAY_HEIGHT/8+y];
								}
							}
							memset(&periph->display[(CHIP8_DISPLAY_WIDTH-shift)*CHIP8_DISPLAY_HEIGHT/8], 0, shift*CHIP8_DISPLAY_HEIGHT/8);
						}
						break;
						case 0x00FD: // 00FD Superchip
							CHIP8_HALT(1, CHIP8_REQUEST_HALT_EXIT_EMULATOR);
						break;
						case 0x00FE: // 00FE Superchip
							periph->high_res = 0;
							if(cpu->quirks & CHIP8_QUIRK_RESIZE_CLEAR_SCREEN) {
								memset(periph->display, 0, sizeof(periph->display));
							}
						break;
						case 0x00FF: // 00FF Superchip
							periph->high_res = 1;
							if(cpu->quirks & CHIP8_QUIRK_RESIZE_CLEAR_SCREEN) {
								memset(periph->display, 0, sizeof(periph->display));
							}
						break;
						default:
							CHIP8_HALT(1, CHIP8_REQUEST_HALT_INVALID_INSTRUCTION);
						break;
					}
				break;
			}
		break;
		case 0x2000: // 2NNN
			CHIP8_HALT(cpu->pc_index+1 >= CHIP8_PC_STACK_SIZE, CHIP8_REQUEST_HALT_STACK_ERROR);
			cpu->pc_index++;
		// Fallthrough
		case 0x1000: // 1NNN
			cpu->pc[cpu->pc_index] = instruction&0x0FFF;
			prevents_stepping = 1;
		break;
		case 0x3000: // 3XNN
			if(*vx == (instruction & 0x00FF)) {
				cpu->pc[cpu->pc_index] += 2;
			}
		break;
		case 0x4000: // 4XNN
			if(*vx != (instruction & 0x00FF)) {
				cpu->pc[cpu->pc_index] += 2;
			}
		break;
		case 0x5000:
		{
			size_t x = (instruction & 0x0F00)>>8;
			size_t y = (instruction & 0x00F0)>>4;
			switch(instruction & 0x000F) {
				case 0x0000: // 5XY0
					if(*vx == *vy) {
						cpu->pc[cpu->pc_index] += 2;
					}
				break;
				case 0x0002: // 5XY2 XO-Chip
				{
					if(x < y) {
						CHIP8_HALT(*i+(y-x) >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
						for(size_t n=0; n<=y-x; n++) {
							mem[*i+n] = cpu->v[x+n];
						}
					} else {
						CHIP8_HALT(*i+(x-y) >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
						for(size_t n=0; n<=x-y; n++) {
							mem[*i+n] = cpu->v[y-n];
						}
					}
				}
				break;
				case 0x0003: // 5XY3 XO-Chip
				{
					if(x < y) {
						CHIP8_HALT(*i+(y-x) >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
						for(size_t n=0; n<=y-x; n++) {
							cpu->v[x+n] = mem[*i+n];
						}
					} else {
						CHIP8_HALT(*i+(x-y) >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
						for(size_t n=0; n<=x-y; n++) {
							cpu->v[y-n] = mem[*i+n];
						}
					}
				}
				break;
				default:
					CHIP8_HALT(1, CHIP8_REQUEST_HALT_INVALID_INSTRUCTION);
				break;
			}
		}
		break;
		case 0x6000: // 6XNN
			*vx = instruction&0x00FF;
		break;
		case 0x7000: // 7XNN
			*vx += instruction&0x00FF;
		break;
		case 0x8000:
			switch(instruction&0x000F) {
				case 0x0000: // 8XY0
					*vx = *vy;
				break;
				case 0x0001: // 8XY1
					*vx |= *vy;
					if(cpu->quirks & CHIP8_QUIRK_LOGIC) {
						*vf = 0;
					}
				break;
				case 0x0002: // 8XY2
					*vx &= *vy;
					if(cpu->quirks & CHIP8_QUIRK_LOGIC) {
						*vf = 0;
					}
				break;
				case 0x0003: // 8XY3
					*vx ^= *vy;
					if(cpu->quirks & CHIP8_QUIRK_LOGIC) {
						*vf = 0;
					}
				break;
				case 0x0004: // 8XY4
				{
					uint16_t result = *vx + *vy;
					*vx = result;
					*vf = (result > 0xFF);
				}
				break;
				case 0x0005: // 8XY5
				{
					uint16_t result = *vx - *vy;
					*vx = result;
					*vf = (result <= 0xFF);
				}
				break;
				case 0x0006: // 8XY6
				{
					uint8_t *source = (cpu->quirks & CHIP8_QUIRK_SHIFT) ? vx : vy;
					uint8_t shifted_out = (*source & 0x01);
					*vx = *source >> 1;
					*vf = shifted_out;
				}
				break;
				case 0x0007: // 8XY7
				{
					uint16_t result = *vy - *vx;
					*vx = result;
					*vf = (result <= 0xFF);
				}
				break;
				case 0x000E: // 8XYE
				{
					uint8_t *source = (cpu->quirks & CHIP8_QUIRK_SHIFT) ? vx : vy;
					uint8_t shifted_out = !!(*source & 0x80);
					*vx = *source << 1;
					*vf = shifted_out;
				}
				break;
				default:
					CHIP8_HALT(1, CHIP8_REQUEST_HALT_INVALID_INSTRUCTION);
				break;
			}
		break;
		case 0x9000:
			switch(instruction & 0x000F) {
				case 0x0000: // 9XY0
					if(*vx != *vy) {
						cpu->pc[cpu->pc_index] += 2;
					}
				break;
				default:
					CHIP8_HALT(1, CHIP8_REQUEST_HALT_INVALID_INSTRUCTION);
				break;
			}
		break;
		case 0xA000: // ANNN
			*i = instruction & 0x0FFF;
		break;
		case 0xB000: // BXNN / BNNN
			if(cpu->quirks & CHIP8_QUIRK_JUMP) {
				cpu->pc[cpu->pc_index] = (instruction & 0x0FFF) + *vx;
			} else {
				cpu->pc[cpu->pc_index] = (instruction & 0x0FFF) + cpu->v[0];
			}
			prevents_stepping = 1;
		break;
		case 0xC000: // CXNN
			*vx = periph->random_num & (instruction & 0x00FF);
		break;
		case 0xD000: // DXYN
		{
			// Pass 1: Determine x, y, w, h position of the drawing operation
			uint16_t x = (periph->high_res ? (*vx) : (*vx*2)) % CHIP8_DISPLAY_WIDTH;
			uint16_t y = (periph->high_res ? (*vy) : (*vy*2)) % CHIP8_DISPLAY_HEIGHT;
			*vf = 0x00;

			uint8_t sprite_width = 8;
			uint8_t sprite_height = (instruction&0x000F);
			uint8_t draw_hires = periph->high_res;
			if(!sprite_height) {
				// DXY0 draws 8x16 or 16x16 sprite. The latter one is far more common
				if(cpu->quirks & CHIP8_QUIRK_LORES_TALL_SPRITE) {
					sprite_width = 8;
					sprite_height = 16;
				} else if(periph->high_res) {
					sprite_width = 16;
					sprite_height = 16;
				} else if(cpu->quirks & CHIP8_QUIRK_LORES_WIDE_SPRITE) {
					sprite_width = 16;
					sprite_height = 16;
				}
			}

			// Pass 2: Prepare sprite content in column-major format, leftmost is first column. For each column, topmost is LSB, bottommost is MSB
			static uint32_t sprite_content[32]; // static for conserving stack storage space
			memset(sprite_content, 0, sizeof(sprite_content));
			switch(sprite_width) {
				case 8: CHIP8_HALT(*i+(sprite_height-1) >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR); break;
				case 16: CHIP8_HALT(*i+(sprite_height-1)*2+1 >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR); break;
				default: assert(0); break; // Should never reach here
			}
			if(draw_hires) {
				switch(sprite_width) {
					case 8:
						for(size_t sx=0; sx<sprite_width; sx++) {
							for(size_t sy=0; sy<sprite_height; sy++) {
								if(mem[*i+(sprite_height-sy-1)] & (1U<<(sprite_width-sx-1))) {
									sprite_content[sx] |= 1U << (sprite_height-sy-1);
								}
							}
						}
					break;
					case 16:
						for(size_t sx=0; sx<sprite_width; sx++) {
							for(size_t sy=0; sy<sprite_height; sy++) {
								if(sx < 8) {
									if(mem[*i+(sprite_height-sy-1)*2] & (1U<<(8-sx-1))) {
										sprite_content[sx] |= 1U << (sprite_height-sy-1);
									}
								} else {
									if(mem[*i+(sprite_height-sy-1)*2+1] & (1U<<(8-(sx-8)-1))) {
										sprite_content[sx] |= 1U << (sprite_height-sy-1);
									}
								}
							}
						}
					break;
					default:
						assert(0); // Unimplemented. Should never reach here.
					break;
				}
			} else {
				switch(sprite_width) {
					case 8:
						for(size_t sx=0; sx<sprite_width; sx++) {
							for(size_t sy=0; sy<sprite_height; sy++) {
								if(mem[*i+(sprite_height-sy-1)] & (1U<<(sprite_width-sx-1))) {
									// Draw two pixels veritcally
									sprite_content[sx*2] |= 3U << ((sprite_height-sy-1)*2);
								}
							}
							// Draw the second pixel horizontally
							sprite_content[sx*2+1] = sprite_content[sx*2];
						}
					break;
					case 16:
						for(size_t sx=0; sx<sprite_width; sx++) {
							for(size_t sy=0; sy<sprite_height; sy++) {
								if(sx < 8) {
									if(mem[*i+(sprite_height-sy-1)*2] & (1U<<(8-sx-1))) {
										// Draw two pixels veritcally
										sprite_content[sx*2] |= 3U << ((sprite_height-sy-1)*2);
									}
								} else {
									if(mem[*i+(sprite_height-sy-1)*2+1] & (1U<<(8-(sx-8)-1))) {
										// Draw two pixels veritcally
										sprite_content[sx*2] |= 3U << ((sprite_height-sy-1)*2);
									}
								}
							}
							// Draw the second pixel horizontally
							sprite_content[sx*2+1] = sprite_content[sx*2];
						}
					break;
					default:
						assert(0); // Unimplemented. Should never reach here.
					break;
				}
				sprite_width *= 2;
				sprite_height *= 2;
			}

			// Pass 3: Blit the sprite onto the display
			uint16_t collision = 0; // The bit is set to 1 if that column has collision, 0 else.
			for(size_t sx=0; sx<sprite_width; sx++) {
				uint16_t col = (x+sx)*CHIP8_DISPLAY_HEIGHT/8;

				if(cpu->quirks & CHIP8_QUIRK_WRAP) {
					col %= CHIP8_DISPLAY_HEIGHT*CHIP8_DISPLAY_WIDTH/8;
				} else if(x+sx >= CHIP8_DISPLAY_WIDTH) {
					// No need to draw further. Everything's gonna be clipped.
					break;
				}

				for(size_t sy=0; sy<sprite_height+(y%8); sy+=8) {
					uint16_t row = (y+sy)/8;
					if(cpu->quirks & CHIP8_QUIRK_WRAP) {
						row %= CHIP8_DISPLAY_HEIGHT/8;
					} else if(row >= CHIP8_DISPLAY_HEIGHT/8) {
						// No need to draw further. Everything's gonna be clipped.
						break;
					}
					uint8_t display_old = periph->display[col + row];
					if(sy >= y%8) {
						periph->display[col + row] ^= sprite_content[sx] >> (sy-(y%8));
					} else {
						periph->display[col + row] ^= sprite_content[sx] << ((y%8)-sy);
					}
					collision |= (uint32_t)chip8_check_collision(display_old, periph->display[col + row]) << sy >> (y%8);
				}
			}

			// Pass 4: saves collision info vf and requset wait for VBLANK if needed
			if(periph->high_res && (cpu->quirks & CHIP8_QUIRK_HIRES_COLLISION)) {
				for(size_t i=0; i<sizeof(collision)*8; i++) {
					if(collision & (1U << i)) {
						(*vf)++;
					}
				}
				// Also need to add the count of the clipped content
				// This is hires-mode specific so don't add it to the clipping mechanism.
				if(y+sprite_height >= CHIP8_DISPLAY_HEIGHT) {
					*vf += (y+sprite_height-CHIP8_DISPLAY_HEIGHT);
				}
			} else {
				*vf = !!collision;
			}
			if(cpu->quirks & CHIP8_QUIRK_VBLANK) {
				periph->requests |= CHIP8_REQUEST_WAIT_DISPLAY_REFRESH;
			}
		}
		break;
		case 0xE000:
			switch(instruction & 0x00FF) {
				case 0x009E: // EX9E
					if(periph->key_held & (1U << *vx)) {
						cpu->pc[cpu->pc_index] += 2;
					}
				break;
				case 0x00A1: // EXA1
					if(!(periph->key_held & (1U << *vx))) {
						cpu->pc[cpu->pc_index] += 2;
					}
				break;
				default:
					CHIP8_HALT(1, CHIP8_REQUEST_HALT_INVALID_INSTRUCTION);
				break;
			}
		break;
		case 0xF000:
			switch(instruction & 0x00FF) {
				case 0x0002: // F002 XO-Chip
					CHIP8_HALT(*i+CHIP8_AUDIO_BUFFER_SIZE-1 >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
					memcpy(periph->audio, &mem[(*i)], CHIP8_AUDIO_BUFFER_SIZE);
					periph->requests |= CHIP8_REQUEST_AUDIO_BUFFER_UPDATED;
				break;
				case 0x0007: // FX07
					*vx = periph->delay_timer;
				break;
				case 0x000A: // FX0A
					if(!periph->key_just_released) {
						prevents_stepping = 1;
					} else {
						for(size_t k=0; k<16; k++) {
							if(periph->key_just_released & (1<<k)) {
								*vx = k;
								break;
							}
						}
					}
				break;
				case 0x0015: // FX15
					periph->delay_timer = *vx;
				break;
				case 0x0018: // FX18
					periph->sound_timer = *vx;
				break;
				case 0x001E: // FX1E
					*i += *vx;
					if(cpu->quirks & CHIP8_QUIRK_FX1E_REPORT_OVERFLOW) {
						*vf = (*i >= CHIP8_MEMORY_SIZE);
					}
				break;
				case 0x0029: // FX29
					*i = (*vx & 0x0F) * 5;
				break;
				case 0x0030: // FX30 Superchip
					*i = (16 * 5) + ((*vx & 0x0F) * 10);
				break;
				case 0x0033: // FX33
					CHIP8_HALT(*i+2 >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
					mem[*i] = *vx / 100;
					mem[*i+1] = (*vx - mem[*i] * 100) / 10;
					mem[*i+2] = *vx - mem[*i]*100 - mem[*i+1]*10;
				break;
				case 0x003A: // FX3A XO-Chip
					periph->audio_pitch = *vx;
					periph->requests |= CHIP8_REQUEST_AUDIO_PITCH_UPDATED;
				break;
				case 0x0055: // FX55
				{
					uint8_t n = (instruction & 0x0F00)>>8;
					CHIP8_HALT(*i+n >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
					for(size_t x=0; x<=n; x++) {
						mem[*i+x] = cpu->v[x];
					}
					if(cpu->quirks & CHIP8_QUIRK_MEMORY_LEAVE_I_UNCHANGED) {
						// Do not increase I here: a.k.a. do nothing!
					} else if (cpu->quirks & CHIP8_QUIRK_MEMORY_INCREASE_BY_X) {
						*i += n;
					} else {
						// With this instruction, it's possible to for i to reach 0x1000 without halting the machine
						// However, as soon as anything got accessed via the i, the machine would halt.
						*i += n+1;
					}
				}
				break;
				case 0x0065: // FX65
				{
					uint8_t n = (instruction & 0x0F00)>>8;
					CHIP8_HALT(*i+n >= CHIP8_MEMORY_SIZE, CHIP8_REQUEST_HALT_I_ERROR);
					for(size_t x=0; x<=n; x++) {
						cpu->v[x] = mem[(*i)+x];
					}
					if(cpu->quirks & CHIP8_QUIRK_MEMORY_LEAVE_I_UNCHANGED) {
						// Do not increase I here: a.k.a. do nothing!
					} else if (cpu->quirks & CHIP8_QUIRK_MEMORY_INCREASE_BY_X) {
						*i += n;
					} else {
						// With this instruction, it's possible to for i to reach 0x1000 without halting the machine
						// However, as soon as anything got accessed via the i, the machine would halt.
						*i += n+1;
					}
				}
				break;
				case 0x0075: // FX75 Superchip
					memcpy(periph->storage_flags, cpu->v, ((instruction & 0x0F00)>>8)+1);
				break;
				case 0x0085: // FX85 Superchip
					memcpy(cpu->v, periph->storage_flags, ((instruction & 0x0F00)>>8)+1);
				break;
				default:
					CHIP8_HALT(1, CHIP8_REQUEST_HALT_INVALID_INSTRUCTION);
				break;
			}
		break;
		default:
			CHIP8_HALT(1, CHIP8_REQUEST_HALT_INVALID_INSTRUCTION);
		break;
	}
	if(!prevents_stepping) {
		cpu->pc[cpu->pc_index] += 2;
	}
}

void chip8_timer_step(struct chip8_machine *machine) {
	if(machine->periph.delay_timer > 0) {
		machine->periph.delay_timer--;
	}
	if(machine->periph.sound_timer > 0) {
		machine->periph.sound_timer--;
	}
}

void chip8_init(struct chip8_machine *machine, const struct chip8_config *config) {
	// The chip8_config got loaded from INI file always bears the latest version number
	// Only the chip8_config got loaded from the BOOTROM might have older version
	// In this case, chip8_init() should load the default value for the newly defined fields.
	// Remap of config value might also be needed

	// Example handling in case CHIP8_CFG_VERSION got bumped to 1. Currently it's at 0 so no such handling is needed
	//if(config->version < 1) {
	//	config->xxx = CHIP8_CFG_DEFAULT.xxx; // Load default value for newly defined field(s)
	//	config->speed = CHIP8_CFG_DEFAULT.speed/2; // Config remap
	//}

	memset(machine->mem, 0, sizeof(machine->mem));
	memcpy(machine->mem, config->font, sizeof(config->font));
	memcpy(&machine->mem[sizeof(config->font)], config->font_highres, sizeof(config->font_highres));

	memset(&machine->cpu, 0, sizeof(machine->cpu));
	machine->cpu.pc[0] = CHIP8_PROGRAM_START_OFFSET;
	machine->cpu.quirks = config->quirks;

	memset(&machine->periph, 0, sizeof(machine->periph));
	machine->periph.audio_pitch = config->audio_pitch;
	memcpy(machine->periph.audio, config->audio, sizeof(config->audio));
	memcpy(machine->periph.storage_flags, config->storage_flags, sizeof(config->storage_flags));
}
