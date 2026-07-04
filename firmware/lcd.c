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

#include "adc.h"
#include "lcd.h"

#include "ch32fun.h"
#include <stdbool.h>
#include <stdint.h>

#define PIN_LCD_CS (1)
#define PIN_LCD_DC (2)
#define PIN_LCD_BL (3)
#define PIN_LCD_RES (4)

const uint8_t LCD_REFRESH_SEQUENCE[] = {
	0xE2, // Software RESET
	0xAE, // Display OFF
	0x40, // Set Start Line to 0

	0xA2, // Bias Select 1/9
	0xA1, // SEG Direction reverse
	0xC0, // COM Direction normal

	0x24, // Regulation Ratio = 5 (max is 7)
	//0x81, 0x20, // EV is set in lcd_set_contrast()

	0x2C, // Power Control: Booster on
	0x2E, // Power Control: Regulator on
	0x2F, // Power Control: Follower on
};

#define LCD_DEFAULT_CONTRAST (0x20)
#define LCD_DEFAULT_BACKLIGHT (0)
static uint8_t lcd_contrast; // Range: 0x00..0x3F
static uint8_t lcd_backlight; // Range: 0~15
static bool lcd_backlight_suppressed;

static const uint8_t* lcd_dma_buffer; // CONCURRENCY_VARIABLE: Written in lcd_transfer_begin(), read/written by lcd_transfer_next_row()
static size_t lcd_dma_row_index; // CONCURRENCY_VARIABLE: ditto
static volatile bool lcd_dma_transfer_in_progress; // CONCURRENCY_VARIABLE: Written in lcd_transfer_begin() and DMA1_Channel3_IRQHandler(), read by lcd_is_transfer_in_progress()
static volatile bool lcd_display_config_updated; // CONCURRENCY_VARIABLE: Written in lcd_set_contrast(), lcd_refresh() and DMA1_Channel3_IRQHandler(), read by DMA1_Channel3_IRQHandler()

static void lcd_use_command_mode(void) {
	// LCD CS LOW, DC LOW (select LCD, send command)
	GPIOC->BSHR = ((1<<PIN_LCD_CS)<<16) | ((1<<PIN_LCD_DC)<<16);
}

static void lcd_use_data_mode(void) {
	// LCD CS LOW, DC HIGH (select LCD, send data)
	GPIOC->BSHR = ((1<<PIN_LCD_CS)<<16) | ((1<<PIN_LCD_DC)<<0);
}

static void lcd_use_deselect_mode(void) {
	// LCD CS HIGH
	GPIOC->BSHR = ((1<<PIN_LCD_CS)<<0);
}

static void lcd_spi_send_byte(uint8_t data) {
	while(!(SPI1->STATR & SPI_STATR_TXE)){}
	SPI1->DATAR = data;
	// Wait until completion of transfer.
	// That's because we might want to change the slave select line
	// or other control lines right after calling this function.
	while(SPI1->STATR & SPI_STATR_BSY){}
}

static void lcd_transfer_next_row(void) {
	static uint8_t lcd_dma_buffer_row[DISPLAY_WIDTH];

	// Set page
	lcd_use_command_mode();
	lcd_spi_send_byte(0xb0 | lcd_dma_row_index);
	lcd_spi_send_byte(0x10 | 0);
	lcd_spi_send_byte(0x00 | 4);

	lcd_use_data_mode();
	// Fill in the lcd_dma_buffer_row to be sent via DMA
	for (int i=0; i<DISPLAY_WIDTH; i++) {
		lcd_dma_buffer_row[i] = lcd_dma_buffer[i*DISPLAY_HEIGHT/8+lcd_dma_row_index];
	}

	// write memory barrier to make sure that the content of lcd_dma_buffer_row is updated to the DMA
	asm volatile("fence ow,ow" ::: "memory");
	DMA1_Channel3->MADDR = (uint32_t)lcd_dma_buffer_row; // memory source
	DMA1_Channel3->CNTR = DISPLAY_WIDTH; // number of items to write
	SPI1->CTLR2 |= SPI_CTLR2_TXDMAEN;
	DMA1_Channel3->CFGR |= DMA_CFGR1_EN;
}

void INTERRUPT_DECORATOR DMA1_Channel3_IRQHandler(void) {
	DMA1->INTFCR |= DMA_CTCIF3;
	// Must wait for completion of SPI transfer before changing the LCD control lines
	// DMA SPI TX transfer compelte only look for TXE flag and doesn't wait for BSY flag
	while(SPI1->STATR & SPI_STATR_BSY){}
	DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
	SPI1->CTLR2 &= ~SPI_CTLR2_TXDMAEN;

	if(adc_card_has_insert_event()) {
		// Card insertion event detected
		// Aborting LCD rendering immediately!
		lcd_use_deselect_mode();
		lcd_dma_transfer_in_progress = false;
	} else {
		if(++lcd_dma_row_index < DISPLAY_HEIGHT/8) {
			lcd_transfer_next_row();
		} else {
			if(lcd_display_config_updated) {
				lcd_use_command_mode();
				// Set contrast
				lcd_spi_send_byte(0x81);
				lcd_spi_send_byte(lcd_contrast);
				// Turn on display
				lcd_spi_send_byte(0xAF);
				lcd_display_config_updated = false;
			}

			// All done. Deselecting LCD CS
			// Must deselect even if lcd_display_config_updated is false
			// becasue the DMA transfer itself would have selected the LCD CS
			lcd_use_deselect_mode();
			lcd_dma_transfer_in_progress = false;
		}
	}
}

void lcd_init_first_stage(void) {
	// This module owns the following pins: LCD_CS, LCD_DC, LCD_DC, LCD_RES
	// This module also owns the SPI's DMA because the card ain't using it.

	// Enable the GPIO
	RCC->PB2PCENR |= RCC_IOPCEN;

	// Deselect LCD CS
	lcd_use_deselect_mode();

	// GPIO PC1, PC2, PC4 is output PUSH-PULL, PC3 is output ALT PUSH-PULL
	GPIOC->CFGLR &= ~((GPIO_CFGLR_MASK << (4*1)) | (GPIO_CFGLR_MASK << (4*2)) | (GPIO_CFGLR_MASK << (4*3)) | (GPIO_CFGLR_MASK << (4*4)));
	GPIOC->CFGLR |= (GPIO_CFGLR_OUT_PP << (4*1)) | (GPIO_CFGLR_OUT_PP << (4*2)) | (GPIO_CFGLR_OUT_AF_PP << (4*3)) | (GPIO_CFGLR_OUT_PP << (4*4));

	lcd_dma_transfer_in_progress = false;
	lcd_contrast = LCD_DEFAULT_CONTRAST;
	lcd_backlight = LCD_DEFAULT_BACKLIGHT;
	lcd_backlight_suppressed = true;
	lcd_display_config_updated = true;

	Delay_Ms(20); // Wait for power to stabalize (LCD specs recommends >1ms)
	// Toggle LCD RES pin (first set it to LOW, then set it to HIGH)
	GPIOC->BSHR = ((1<<PIN_LCD_RES)<<16);
	Delay_Us(100); // LCD's requirement: >5us
}

void lcd_init_second_stage(void) {
	GPIOC->BSHR = ((1<<PIN_LCD_RES)<<0);
	Delay_Us(100); // LCD's requirement: >5us

	lcd_refresh();
}

void lcd_transfer_begin(const void *buffer) {
	// Wait until completion of the previous LCD DMA transfer
	while(lcd_is_transfer_in_progress()){}

	// Compiler-level barrier for lcd_dma_buffer and lcd_dma_row_index so that the variables wouldn't have to be defined as volatile
	asm volatile("" ::: "memory");
	lcd_dma_buffer = buffer;
	lcd_dma_row_index = 0;
	lcd_dma_transfer_in_progress = true;
	lcd_transfer_next_row();
}

bool lcd_is_transfer_in_progress(void) {
	return lcd_dma_transfer_in_progress;
}

void lcd_refresh(void) {
	// Wait until completion of LCD DMA transfer
	while(lcd_is_transfer_in_progress()){}

	lcd_use_command_mode();
	// Send out the initialization sequence
	for (size_t i=0; i<sizeof(LCD_REFRESH_SEQUENCE)/sizeof(*LCD_REFRESH_SEQUENCE); i++) {
		lcd_spi_send_byte(LCD_REFRESH_SEQUENCE[i]);
	}
	// All done. Deselect LCD CS pin. Please keep in mind that the display would be turned off until
	// the next lcd_transfer_begin() is called and the DMA it kicks off completed the transfer
	lcd_use_deselect_mode();
	lcd_display_config_updated = true;
}

static void lcd_set_brightness_inner(void) {
	TIM1->CH3CVR = lcd_backlight_suppressed ? 0 : lcd_backlight;
}

void lcd_set_brightness(uint8_t value) {
	lcd_backlight = value;
	lcd_set_brightness_inner();
}

void lcd_set_backlight_suppression(bool value) {
	lcd_backlight_suppressed = value;
	lcd_set_brightness_inner();
}

void lcd_set_contrast(uint8_t value) {
	lcd_contrast = value;
	lcd_display_config_updated = true;
}
