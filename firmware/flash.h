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

// Flash layout:
// Usable flash area: [0x0800E000,0x0800F800)
// Reserved: [0x0800E000,0x0800E400)
// Global config: [0x0800E400,0x0800E800)
// Boot rom: [0x0800E800,0x0800F800)

#define FLASH_PAGE_SIZE (256)

#define FLASH_CONFIG_START (0x800E400) // Inclusive
#define FLASH_CONFIG_END (0x0800E800) // Exclusive

// Information required:
// Metadata (checksum)
// All config fields
// ROM content
#define FLASH_BOOT_ROM_INI_START (0x0800E800) // Inclusive
#define FLASH_BOOT_ROM_INI_END (0x800EA00) // Exclusive
#define FLASH_BOOT_ROM_CH8_START (0x800EA00) // Inclusive
#define FLASH_BOOT_ROM_CH8_END (0x0800F800) // Exclusive

void flash_unlock(void);
void flash_lock(void);
void flash_erase_256(uint32_t offset); // Erase 256 bytes
void flash_write_256(uint32_t offset, const void *data); // Write 256 bytes
void flash_write_4(uint32_t offset, const uint32_t data); // Write 4 bytes
