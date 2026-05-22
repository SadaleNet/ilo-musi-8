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

#include "lcd.h"
#include "ch32fun.h"

#include <stdbool.h>
#include <stdint.h>


#define PIN_CARD_CS (0)
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
static uint8_t lcd_contrast; // Range: 0x00..0x3F

static uint8_t* lcd_dma_buffer; // CONCURRENCY_VARIABLE: Written in lcd_transfer_begin(), read/written by lcd_transfer_next_row()
static size_t lcd_dma_row_index; // CONCURRENCY_VARIABLE: ditto
static volatile bool lcd_dma_transfer_in_progress; // CONCURRENCY_VARIABLE: Written in lcd_transfer_begin() and DMA1_Channel3_IRQHandler(), read by lcd_is_transfer_in_progress()

static void lcd_use_command_mode(void) {
	// LCD CS LOW, DC LOW (select LCD, send command)
	GPIOC->BSHR = ((1<<PIN_LCD_CS)<<16) | ((1<<PIN_LCD_DC)<<16);
}

static void lcd_use_data_mode(void) {
	// LCD CS LOW, DC HIGH (select LCD, send data)
	GPIOC->BSHR = ((1<<PIN_LCD_CS)<<16) | ((1<<PIN_LCD_DC)<<0);
}

static void lcd_use_deselect_mode(void) {
	// LCD CS HIGH, CAFD CS HIGH
	GPIOC->BSHR = ((1<<PIN_LCD_CS)<<0) | ((1<<PIN_CARD_CS)<<0);
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

	// write memory barrier to make sure that the content of lcd_dma_buffer_row is correct to the DMA
	asm volatile("fence ow,ow" ::: "memory");
	DMA1_Channel3->MADDR = (uint32_t)lcd_dma_buffer_row; // memory source
	DMA1_Channel3->CNTR = DISPLAY_WIDTH; // number of items to write
}

void INTERRUPT_DECORATOR DMA1_Channel3_IRQHandler(void) {
	DMA1->INTFCR |= DMA_CTCIF3;
	// Must wait for completion of SPI transfer before changing the LCD control lines
	// DMA SPI TX transfer compelte only look for TXE flag and doesn't wait for BSY flag
	while(SPI1->STATR & SPI_STATR_BSY){}

	if(++lcd_dma_row_index < DISPLAY_HEIGHT/8) {
		lcd_transfer_next_row();
	} else {
		// Turn on display after completing the DMA transfer
		lcd_use_command_mode();
		lcd_spi_send_byte(0xAF);

		// All done. Deselecting LCD CS
		lcd_use_deselect_mode();
		lcd_dma_transfer_in_progress = false;
	}
}

void lcd_and_spi_init(void) {
	// Enable the GPIO
	RCC->PB2PCENR |= RCC_IOPCEN;

	// Deselect LCD CS and card CS
	lcd_use_deselect_mode();

	// GPIO C0, C1, C2, C4 to output PUSH-PULL
	// C3, C5, C6 to output ALT PUSH-PULL,
	// C7 to INPUT FLOATING (must use floating because we have external pull-up to take care of memory card's requirement)
	GPIOC->CFGLR = (GPIO_CFGLR_OUT_PP << (4*0)) | (GPIO_CFGLR_OUT_PP << (4*1)) | (GPIO_CFGLR_OUT_PP << (4*2)) | (GPIO_CFGLR_OUT_AF_PP << (4*3)) | (GPIO_CFGLR_OUT_PP << (4*4)) // GPIO
					| (GPIO_CFGLR_OUT_AF_PP << (4*5)) | (GPIO_CFGLR_OUT_AF_PP << (4*6)) | (GPIO_CFGLR_IN_FLOAT << (4*7)); // SPI

	// Reset SPI and TIM1
	RCC->PB2PRSTR |= RCC_SPI1RST | RCC_TIM1RST;
	RCC->PB2PRSTR &= ~(RCC_SPI1RST | RCC_TIM1RST);
	// Enable the SPI and TIM1 clock source
	RCC->PB2PCENR |= RCC_SPI1EN | RCC_TIM1EN;

	TIM1->PSC = 0x0000; // prescaler: 1
	TIM1->ATRLR = 255; // Autoreload value
	TIM1->CCER |= TIM1_CCER_CC3E; // Enable TIM1_CH3 output, negative polarity because TIM1_CCER_CC3P not specified
	TIM1->CH3CVR = 0;
	TIM1->SWEVGR |= TIM1_SWEVGR_UG; // Update the autoreload register AND the CH3CVR register
	TIM1->CHCTLR2 |= TIM1_CHCTLR2_OC3PE | TIM1_CHCTLR2_OC3M_2 | TIM1_CHCTLR2_OC3M_1; // Set TIM1_CH3 to PWM mode 1
	TIM1->BDTR |= TIM1_BDTR_MOE; // Enable TIM1's output
	TIM1->CTLR1 |= TIM1_CTLR1_ARPE | TIM1_CTLR1_CEN; // Enable the timer itself

	// Configure SPI. SPI_Mode_Master and SPI_CTLR1_SPE must be set after CS pin is high
	SPI1->CTLR1 =	SPI_CTLR1_BR_2 | SPI_CTLR1_BR_1 | SPI_CTLR1_BR_0 // Same as lcd_spi_set_mode(SPI_MODE_MEMORY_CARD_SLOW)
					| (SPI_CPOL_Low | SPI_CPHA_1Edge) // SPI Mode 0 (That's for the card. LCD requires SPI mode 3 instead (SPI_CPOL_High | SPI_CPHA_2Edge))
					| SPI_NSS_Soft // Software NSS mode
					| SPI_Mode_Master // Master mode
					| 0 // (lack of SPI_CTLR1_DFF) 8bit mode
					| SPI_Direction_2Lines_FullDuplex // Use both MOSI and MISO
					| SPI_CTLR1_SPE; // SPI begin!

	SPI1->CTLR2 |= SPI_CTLR2_TXDMAEN;

	// Enable DMA (other component may also enable DMA on their own. No harm to enable it multiple times.)
	RCC->HBPCENR |= RCC_DMA1EN;

	// Configure DMA for ADC
	DMA1_Channel3->PADDR = (uint32_t)(&SPI1->DATAR); // Peripheral address register

	DMA1_Channel3->CFGR =
		// (Not specifying DMA_CFGR1_PL) Set the priority to "Low"
		// (Not specifying DMA_CFGR1_PSIZE) 8bit data for peripheral
		// (Not specifying DMA_CFGR1_MSIZE) 8bit data for memory
		DMA_CFGR1_MINC | // Incrememt memory address
		DMA_CFGR1_DIR | // Read from memory, write to peripheral
		DMA_CFGR1_TCIE | // Enable transfer-complete interrupt
		DMA_CFGR1_EN; // Enable channel

	// Clear the interrupt flag, just in case.
	DMA1->INTFCR |= DMA_CTCIF3;

	SPI1->CTLR1 |= SPI_CTLR1_SPE;

	// Configure interrupt controller
	// PFIC: Enable interrupt for DMA1_Channel3_IRQn
	PFIC->IPRIOR[DMA1_Channel3_IRQn] = 0x00; // The priority is 0 (the highest, and it cannot be preempted)
	PFIC->IENR[DMA1_Channel3_IRQn/32] |= (1<<(DMA1_Channel3_IRQn%32));

	lcd_dma_transfer_in_progress = false;
	lcd_contrast = LCD_DEFAULT_CONTRAST;
	// Must do card initialization before doing LCD initialization to put the card into SPI mode
	// TODO: uncomment this piece of code after memory card driver's implemented
	// maybe also check if the card has been inserted first
	// maybe use callback function so that lcd.c won't have to hold the FATFS handle
	//FATFS fs;
	//if (pf_mount(&fs) != FR_OK){
		// card mount error handling goes here
	//}

	lcd_spi_set_mode(SPI_MODE_LCD);

	Delay_Ms(20); // Wait for power to stabalize (LCD specs recommends >1ms)
	// Toggle LCD RES pin (first set it to LOW, then set it to HIGH)
	GPIOC->BSHR = ((1<<PIN_LCD_RES)<<16);
	Delay_Us(100); // LCD's requirement: >5us
	GPIOC->BSHR = ((1<<PIN_LCD_RES)<<0);
	Delay_Us(100); // LCD's requirement: >5us

	lcd_refresh();
}

void lcd_transfer_begin(void *buffer) {
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

void lcd_spi_set_mode(enum spi_mode mode) {
	switch(mode) {
		case SPI_MODE_LCD:
			// Deselect card CS pin
			lcd_use_deselect_mode();
			// Send out a byte to complete the deselection process
			lcd_spi_send_byte(0xFF);

			// Go back to LCD mode (mode 3)
			//SPI1->CTLR1 &= ~(SPI_CPOL_High | SPI_CPHA_2Edge); // redundant because we're gonna use |=
			SPI1->CTLR1 |= (SPI_CPOL_High | SPI_CPHA_2Edge);

			// LCD max 20Mhz
			// Setting the divider to 4. That'd be 48Mhz/4 = 12Mhz
			SPI1->CTLR1 &= ~SPI_CTLR1_BR;
			SPI1->CTLR1 |= SPI_CTLR1_BR_1;
		break;
		case SPI_MODE_MEMORY_CARD:
		case SPI_MODE_MEMORY_CARD_SLOW:
			// Wait until completion of LCD DMA transfer
			while(lcd_is_transfer_in_progress()){}

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

void lcd_refresh(void) {
	// Wait until completion of LCD DMA transfer
	while(lcd_is_transfer_in_progress()){}

	lcd_use_command_mode();
	// Send out the initialization sequence
	for (size_t i=0; i<sizeof(LCD_REFRESH_SEQUENCE)/sizeof(*LCD_REFRESH_SEQUENCE); i++) {
		lcd_spi_send_byte(LCD_REFRESH_SEQUENCE[i]);
	}
	// All done. Deselect LCD CS pin
	lcd_use_deselect_mode();

	// Also set contrast because the contrast setting would be erased after refresh
	lcd_set_contrast(lcd_contrast);
}

void lcd_set_brightness(uint8_t value) {
	TIM1->CH3CVR = value;
	// Update the CH3CVR register. It works without it but the specs said that it's required
	TIM1->SWEVGR |= TIM1_SWEVGR_UG;
}

void lcd_set_contrast(uint8_t value) {
	// Wait until completion of LCD DMA transfer
	while(lcd_is_transfer_in_progress()){}

	lcd_use_command_mode();
	lcd_spi_send_byte(0x81);
	lcd_spi_send_byte(value);
	lcd_use_deselect_mode();
}
