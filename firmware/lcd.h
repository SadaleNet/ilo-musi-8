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

// Perform initialization without involving SPI. Start resetting LCD for turning it off
void lcd_init_first_stage(void);
// Perform initialization with SPI sequence. Also complete resetting LCD
void lcd_init_second_stage(void);

// Send out the full 128x64 buffer, column major
// The transfer is done with DMA and it isn't blocking.
void lcd_transfer_begin(const void *buffer);
bool lcd_is_transfer_in_progress(void);

// Recommended to call once in a while so that any soft glitch would be fixed.
// Please notice that calling this function would cause the LCD to blink for like half a second.
// In case an LCD transfer is in progress, it'll block until completion of the transfer.
void lcd_refresh(void);

void lcd_set_brightness(uint8_t value); // Range: 0~15. The most effective values follows: 0, 3, 4, 5, 6, 7, 8
void lcd_set_backlight_suppression(bool value); // For suppression backlight during card reading operation to avoid overloading the power supply
void lcd_set_contrast(uint8_t value); // Range: 0~63
