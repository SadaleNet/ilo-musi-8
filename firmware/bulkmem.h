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

// The content in this struct is meant to be used for storing temporary values
// Due to the limited amount of RAM and that the CHIP8 emulator's taking too much RAM,
// I'd like to utilize the RAM while the CHIP8 emulator is inactive.
// The purpose of the pointer bulkmem serves the same purpose as union for letting
// multiple variables sharing the same memory space.
// The reason I'm not using union is that, a part of the chip8_machine
// is not available for memory space sharing, which's chip8_machine.periph.display
// I don't wanna take the display out of the chip8_machine.
// Therefore, the pointer bulkmem would be pointing at chip8_machine.mem, which's 4K in size,
// which's available

#include "chip8.h"
#include <stdint.h>

#define FILE_BUFFER_SIZE (512)
#define MENU_PAGE_SIZE (10)
struct shared_buffer {
	struct chip8_config chip8_cfg;
	char file_buffer[FILE_BUFFER_SIZE];
	char menu_file_list[MENU_PAGE_SIZE][14];
};

extern struct shared_buffer *bulkmem;
