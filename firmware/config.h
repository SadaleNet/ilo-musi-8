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

#include <stdint.h>

#define GLOBAL_CONFIG_VERSION (0)

// This struct must be exactly 32bit
struct __attribute__((packed)) global_config {
	uint8_t version:4; // To update the struct, you can only append content to the end and then increment the version
	uint8_t volume:4;
	uint8_t backlight:4;
	uint8_t contrast:4;
	uint8_t language:2;
	uint8_t reserved:6;
	uint8_t checksum:8; // Initialize with 0xAA. XOR byte-by-byte
};

// Storage mechanism:
// 1kB of space between FLASH_CONFIG_START and FLASH_CONFIG_END is allocated for global config storage with wear-leveling implemented.
// The 1kB flash space is divided into 256 slots, each slot is 32bit wide. Each flash page has 256 bytes (i.e. 64 slots)
// At first, all pages are empty (filled with 0xFF). Upon first save, the config would be save into the first slot.
// The second save would be stored into the second slot and so on.
// When the first page is filled, the beginning of the second page would be written.
// When the final page is filled, the first page would be erased, and first slot got written. then all pages except for the first one would be erased

void config_load(struct global_config *config);
bool config_save(const struct global_config *config); // returns true on success, false on error
void config_test(void);
