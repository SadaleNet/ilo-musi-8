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
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Extends on FRESULT of fatfs/ff.h
#define FR_INI_PARSE_ERROR (90) // The INI config file has format error
#define FR_VOLUME_FULL (91) // f_write() had indicated that the volume is full
#define FR_PATH_LENGTH_ERROR (92) // Directory recursion limit reached
#define FR_FIRMWARE_VERIFICATION_ERROR (93) // The firwmare verification failed

void file_first_mount(void); // Process card insertion/removal events. Can be called after adc_is_reading_ready()
void file_loop(void); // Process card insertion/removal events
uint8_t file_load_config(const char *path, struct chip8_config *chip8_cfg);
uint8_t file_save_config(const char *path, const struct chip8_config *chip8_cfg);
uint8_t file_load_rom(const char *path, const struct chip8_config *chip8_cfg, struct chip8_machine *chip8_machine);
uint8_t file_save_storage_flag(const uint8_t *storage_flags, size_t flag_size);
uint8_t file_readdir(const char *path, size_t offset, char (*filelist)[14], size_t *count);
uint8_t file_verify_firmware_update(void); // Verify firmware. If OK, automatically delete the firmware file
