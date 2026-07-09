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

#include "lcd.h" // for DISPLAY_WIDTH and DISPLAY_HEIGHT
#include "draw.h" // for font and enum tr_msg_id
#include <assert.h>
#include <string.h>

void draw_clear(uint8_t *buffer) {
	memset(buffer, 0, DISPLAY_WIDTH*DISPLAY_HEIGHT/8);
}

void draw_transfer_row(uint8_t *buffer, uint8_t *bitmap, uint8_t row) {
	for(size_t i=0; i<DISPLAY_WIDTH; i++) {
		bitmap[i] = buffer[i*DISPLAY_HEIGHT/8+row];
	}
}

void draw_clear_row(uint8_t *buffer, uint8_t row) {
	for(size_t i=0; i<DISPLAY_WIDTH; i++) {
		buffer[i*DISPLAY_HEIGHT/8+row] = 0;
	}
}

void draw_bitmap_h8(uint8_t *buffer, const uint8_t *bitmap, uint8_t w, uint8_t x, uint8_t y) {
	for(size_t i=0; i<w && x+i<DISPLAY_WIDTH; i++) {
		if(y < DISPLAY_HEIGHT) {
			buffer[(x+i)*DISPLAY_HEIGHT/8 + y/8] |= bitmap[i] << y%8;
			if(y%8 != 0 && y < DISPLAY_HEIGHT-8) {
				buffer[(x+i)*DISPLAY_HEIGHT/8 + y/8 +1] |= bitmap[i] >> (8-y)%8;
			}
		}
	}
}

void draw_text(uint8_t *buffer, const char *text, uint8_t x, uint8_t y) {
	const char *ptr = text;
	while(*ptr && x < DISPLAY_WIDTH) {
		size_t index = *ptr - ' ';
		if(index >= sizeof(FONT_ASCII)/sizeof(*FONT_ASCII)) {
			index = 0; // Same as the first glyph. i.e. space.
		}
		draw_bitmap_h8(buffer, FONT_ASCII[index], sizeof(*FONT_ASCII), x, y);
		x += sizeof(*FONT_ASCII)+1; ptr++;
	}
}


uint8_t draw_get_translated_width(enum config_lang lang, enum tr_msg_id msg_id) {
	switch(lang) {
		case LANG_EN: return strlen(TR_MSG_EN[msg_id])*6;
		case LANG_TOK: return strlen(TR_MSG_TOK[msg_id])*6;
		case LANG_SP: return TR_MSG_SP_LEN[msg_id]+1;
		case LANG_QSS: return TR_MSG_QSS_LEN[msg_id]+1;
		default: assert(false);
	}
}

void draw_translated(uint8_t *buffer, enum config_lang lang, enum tr_msg_id msg_id, uint8_t x, uint8_t y) {
	switch(lang) {
		case LANG_EN: draw_text(buffer, TR_MSG_EN[msg_id], x, y+Y_ADJ); break;
		case LANG_TOK: draw_text(buffer, TR_MSG_TOK[msg_id], x, y+Y_ADJ); break;
		case LANG_SP: draw_bitmap_h8(buffer, TR_MSG_SP[msg_id], TR_MSG_SP_LEN[msg_id], x, y+Y_ADJ_SP); break;
		case LANG_QSS: draw_bitmap_h8(buffer, TR_MSG_QSS[msg_id], TR_MSG_QSS_LEN[msg_id], x, y); break;
		default: assert(false);
	}
}
