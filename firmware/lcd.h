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

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_WIDTH (128U)
#define DISPLAY_HEIGHT (64U)

// The SPI mode would be set to SPI_MODE_MEMORY_CARD or SPI_MODE_MEMORY_CARD_SLOW by the FATFS module,
// and would be set back to SPI_MODE_LCD after completion of the FATFS operation.
// The userspace code can always assume the mode is SPI_MODE_LCD.
enum spi_mode {
	SPI_MODE_LCD,
	SPI_MODE_MEMORY_CARD,
	SPI_MODE_MEMORY_CARD_SLOW, // For card initialization.
};

// Also include code to initialize SPI interface, which's shared by the external memory card.
void lcd_and_spi_init(void);
// Default is SPI_MODE_LCD. But during initialization it's briefly switch to SPI_MODE_MEMORY_CARD_SLOW
void lcd_spi_set_mode(enum spi_mode mode);

// Send out the full 128x64 buffer, column major
// The transfer is done with DMA and it isn't blocking.
void lcd_transfer_begin(void *buffer);
bool lcd_is_transfer_in_progress(void);

// Recommended to call once in a while so that any soft glitch would be fixed.
// In case an LCD transfer is in progress, it'll block until completion of the transfer.
void lcd_refresh(void);

void lcd_set_backlight(uint8_t value);
void lcd_set_contrast(uint8_t value);
