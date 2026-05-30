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
#include "spi.h"

#include "ch32fun.h"

static FATFS filesystem;
static bool filesystem_mounted;

void spi_init(void) {
	// This module owns the following pins: CARD_CS, SCK, MOSI, MISO
	// Keep in mind that, in particular, LCD_CS is owned by the lcd module, and that
	// the SPI DMA isn't being owned by this module because only LCD uses it.

	// Enable the GPIO
	RCC->PB2PCENR |= RCC_IOPCEN;

	// Reset SPI
	RCC->PB2PRSTR |= RCC_SPI1RST;
	RCC->PB2PRSTR &= ~RCC_SPI1RST;
	// Enable the SPI clock source
	RCC->PB2PCENR |= RCC_SPI1EN;

	// Reset SPI
	RCC->PB2PRSTR |= RCC_SPI1RST;
	RCC->PB2PRSTR &= ~RCC_SPI1RST;
	// Enable the SPI clock source
	RCC->PB2PCENR |= RCC_SPI1EN;

	// Configure SPI. SPI_Mode_Master and SPI_CTLR1_SPE must be set after CS pin is high
	SPI1->CTLR1 =	SPI_CTLR1_BR_2 | SPI_CTLR1_BR_1 | SPI_CTLR1_BR_0 // Same as lcd_spi_set_mode(SPI_MODE_MEMORY_CARD_SLOW)
					| (SPI_CPOL_Low | SPI_CPHA_1Edge) // SPI Mode 0 (That's for the card. LCD requires SPI mode 3 instead (SPI_CPOL_High | SPI_CPHA_2Edge))
					| SPI_NSS_Soft // Software NSS mode
					| SPI_Mode_Master // Master mode
					| 0 // (lack of SPI_CTLR1_DFF) 8bit mode
					| SPI_Direction_2Lines_FullDuplex // Use both MOSI and MISO
					| SPI_CTLR1_SPE; // SPI begin!

	SPI1->CTLR2 |= SPI_CTLR2_TXDMAEN;

	// Card CS pin is P0
	// SPI pins are PC5 SCK, PC6 MOSI, and PC7 MISO
	GPIOC->CFGLR &= ~((GPIO_CFGLR_MASK << (4*0)) | (GPIO_CFGLR_MASK << (4*5)) | (GPIO_CFGLR_MASK << (4*6)) | (GPIO_CFGLR_MASK << (4*7)));
	GPIOC->CFGLR |= (GPIO_CFGLR_OUT_PP << (4*0)) | (GPIO_CFGLR_OUT_AF_PP << (4*5)) | (GPIO_CFGLR_OUT_AF_PP << (4*6)) | (GPIO_CFGLR_IN_FLOAT << (4*7));

	spi_set_mode(SPI_MODE_MEMORY_CARD_SLOW);
}

void spi_send_byte(uint8_t data) {
	while(!(SPI1->STATR & SPI_STATR_TXE)){}
	SPI1->DATAR = data;
	// Wait until completion of transfer.
	// That's because we might want to change the slave select line
	// or other control lines right after calling this function.
	while(SPI1->STATR & SPI_STATR_BSY){}
}

void spi_set_mode(enum spi_mode mode) {
	switch(mode) {
		case SPI_MODE_LCD:
			lcd_set_backlight_suppression(false);
			// Go back to LCD mode (mode 3)
			//SPI1->CTLR1 &= ~(SPI_CPOL_High | SPI_CPHA_2Edge); // redundant because we're gonna use |=
			SPI1->CTLR1 |= (SPI_CPOL_High | SPI_CPHA_2Edge);

			// LCD max 20Mhz
			// Setting the divider to 4. That'd be 48Mhz/4 = 12Mhz.
			SPI1->CTLR1 &= ~SPI_CTLR1_BR;
			SPI1->CTLR1 |= SPI_CTLR1_BR_0;
		break;
		case SPI_MODE_MEMORY_CARD:
		case SPI_MODE_MEMORY_CARD_SLOW:
			lcd_set_backlight_suppression(true);
			SPI1->DATAR; // Clear the RX byte

			// Switch to SPI mode 0 for memory card
			SPI1->CTLR1 &= ~(SPI_CPOL_High | SPI_CPHA_2Edge);

			if(mode == SPI_MODE_MEMORY_CARD) {
				// Card max 25Mhz
				// Setting the divider to 2. That'd be 48Mhz/2 = 24Mhz
				SPI1->CTLR1 &= ~SPI_CTLR1_BR;
			} else { // mode == SPI_MODE_MEMORY_CARD_SLOW
				// Required data rate for card: 100kHz ~ 400kHz
				// Setting the divider to 256. That'd be 48Mhz/256 = 187kHz
				// SPI1->CTLR1 &= ~SPI_CTLR1_BR; // Commenting out. Redundant.
				SPI1->CTLR1 |= SPI_CTLR1_BR_2 | SPI_CTLR1_BR_1 | SPI_CTLR1_BR_0;
			}
		break;
	}
}

FRESULT spi_card_mount_filesystem(void) {
	FRESULT ret;
	ret = f_mount(&filesystem, "", 1);
	filesystem_mounted = (ret == FR_OK);
	return ret;
}

bool spi_card_is_filesystem_mounted(void) {
	return filesystem_mounted;
}

void spi_card_reset_mounted_state(void) {
	filesystem_mounted = false;
}
