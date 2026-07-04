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
// It'd be pretty awkward to make a union for chip8_machine.mem and the struct below because
// I'd like to have structure of chip8_machine dedicated for chip8-related stuff.
// Therefore, I'm creating a pointer struct shared_buffer *bulkmem, which would point
// to chip8_machine.mem. The chip8_machine.mem is 4k in size.
// Never access the variables in bulkmem while the chip8 emulator is running.
// After running the chip8 emulator, all content in bulkmem would be invalidated.

#include "chip8.h"
#include <stdint.h>
#include <stddef.h> // For size_t

#define FILE_BUFFER_SIZE (512)
#define MENU_PAGE_SIZE (10)
#define READDIR_CACHE_SIZE (200)
struct shared_buffer {
	uint8_t file_buffer[FILE_BUFFER_SIZE];
	char menu_file_list[MENU_PAGE_SIZE][14];
	size_t readdir_cache_count;
	size_t readdir_max_count; // SIZE_MAX if undetermined
	char readdir_cache[READDIR_CACHE_SIZE][14];
};

extern struct shared_buffer *bulkmem;
