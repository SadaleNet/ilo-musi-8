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

#include "bootloader.h"
#include "ch32fun.h"
#include "pff3a/pff.h"

__attribute__((noreturn)) void launch_user_code(void) {
	FLASH->BOOT_MODEKEYR = FLASH_KEY1;
	FLASH->BOOT_MODEKEYR = FLASH_KEY2;
	FLASH->STATR = 0; // Same as `FLASH->STATR &= ~FLASH_STATR_BOOT_MODE;` but it saves space
	PFIC->SCTLR = (1<<31); // Same as `PFIC->SCTLR |= (1<<SCTLR_SYSRST)` but it saves space

	__builtin_unreachable(); // Tells the compiler that the code won't run past this point
	// while(1) {} // Not including it saves 4 bytes of code space
}

static uint8_t display_buffer[128] = {0};

uint8_t LCD_INIT_SEQUENCE[] = {
	0xE2, // Software RESET
	0xAE, // Display OFF
	0x40, // Set Start Line to 0

	0xA2, // Bias Select 1/9
	0xA1, // SEG Direction reverse
	0xC0, // COM Direction normal

	0x24, // Regulation Ratio = 5 (max is 7)
	0x81, 0x20, // Set EV to 0x20 (max is 0x3F)

	0x2C, // Power Control: Booster on
	0x2E, // Power Control: Regulator on
	0x2F, // Power Control: Follower on

	0xAF, // Display ON (For simplicity, I'm gonna turn it on here before filling in the display buffer)
};

void flash_unlock(void) {
	if(FLASH->CTLR & FLASH_CTLR_LOCK) {
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}
	if(FLASH->CTLR & FLASH_CTLR_FLOCK) {
		FLASH->MODEKEYR = FLASH_KEY1;
		FLASH->MODEKEYR = FLASH_KEY2;
	}
}

void flash_lock(void) {
	FLASH->CTLR |= FLASH_CTLR_LOCK|FLASH_CTLR_FLOCK;
}

// Returns 0 if no content change
// Returns 1 if content is changed
// Returns 2 if input is invalid
uint8_t flash_write_sector(uint32_t offset, const void *data) {
	if(offset < FLASH_START_OFFSET || offset+FLASH_SECTOR_SIZE > FLASH_END_OFFSET || offset%FLASH_SECTOR_SIZE != 0) {
		return 2;
	}
	uint8_t flashing_required = 0;
	for(size_t i=0; i<FLASH_SECTOR_SIZE; i+=sizeof(uint32_t)) {
		if(*((volatile uint32_t*)(offset+i)) != *((uint32_t*)(data+i))) {
			flashing_required = 1;
			break;
		}
	}
	if(!flashing_required) {
		return 0;
	}

	// Perform standard flash erase (1024 bytes)
	while(FLASH->STATR & FLASH_STATR_BSY){} // The reference manual didn't say that it's needed but I'm putting it here anyway just to be safe
	FLASH->CTLR = FLASH_CTLR_PER; // Same as `FLASH->CTLR |= FLASH_CTLR_PER;` but saves 4 bytes of FLASH space
	FLASH->ADDR = offset;
	FLASH->CTLR = FLASH_CTLR_PER|FLASH_CTLR_STRT; // Same as `FLASH->CTLR |= FLASH_CTLR_STRT;`
	while(FLASH->STATR & FLASH_STATR_BSY){} // Checking BSY flag instead of EOP for saving flash space
	FLASH->CTLR = 0; // Same as `FLASH->CTLR &= ~FLASH_CTLR_PER;`

	// Perform flash "fast" programming (there's no standard programming option so we must use fast programming)
	while(FLASH->STATR & FLASH_STATR_BSY){}
	FLASH->CTLR = FLASH_CTLR_PAGE_FTPG; // Same as `FLASH->CTLR |= FLASH_CTLR_PAGE_FTPG;`

	for(size_t i=0; i<FLASH_SECTOR_SIZE; i+=FLASH_PAGE_SIZE) {
		FLASH->CTLR = FLASH_CTLR_PAGE_FTPG|FLASH_CTLR_BUF_RST; // Same as `FLASH->CTLR |= FLASH_CTLR_BUF_RST;`
		while(FLASH->STATR & FLASH_STATR_BSY){} // Checking BSY flag instead of EOP for saving flash space
		for(size_t j=0; j<FLASH_PAGE_SIZE; j+=sizeof(uint32_t)) {
			*((volatile uint32_t*)(offset+i+j)) = *((uint32_t*)(data+i+j));
			FLASH->CTLR = FLASH_CTLR_PAGE_FTPG|FLASH_CTLR_BUF_LOAD; // Same as `FLASH->CTLR |= FLASH_CTLR_BUF_LOAD;`
			while(FLASH->STATR & FLASH_STATR_BSY){}
		}
		FLASH->ADDR = offset+i;
		FLASH->CTLR = FLASH_CTLR_PAGE_FTPG|FLASH_CTLR_STRT; // Same as `FLASH->CTLR |= FLASH_CTLR_STRT;`
		while(FLASH->STATR & FLASH_STATR_BSY){} // Checking BSY flag instead of EOP for saving flash space
	}
	FLASH->CTLR = 0; // Same as `FLASH->CTLR &= ~FLASH_CTLR_PAGE_FTPG;`
	return 1;
}

uint8_t spi_send_byte(uint8_t data) {
	// Commenting out TXE handling saves 12 bytes
	// Since we wait for transfer completion after each byte, TX buffer should always be empty anyway.
	// while(!(SPI1->STATR & SPI_STATR_TXE)){}
	SPI1->DATAR = data;
	// Wait until completion of transfer.
	// That's because we might want to change the slave select line
	// or other control lines right after calling this function.
	// Also wait for availability of the RX's content
	while((SPI1->STATR & (SPI_STATR_RXNE|SPI_STATR_BSY)) != SPI_STATR_RXNE){}
	return SPI1->DATAR;
}

void lcd_transfer_row(int i, uint8_t buffer[128]) {
	// LCD CS LOW, DC low (select LCD, send command)
	GPIOC->BSHR = (((1<<PIN_LCD_CS)|(1<<PIN_LCD_DC))<<16);
	// Set page
	spi_send_byte(0xb0 | i);
	spi_send_byte(0x10 | 0);
	spi_send_byte(0x00 | 4);
	// DC high (send data)
	GPIOC->BSHR = ((1<<PIN_LCD_DC)<<0);
	// Send the row's data to LCD
	for (int j = 0; j < 128; j++) {
		// Send data for last row. Hard-code to stripe for remaining rows
		if(i == 0) {
			spi_send_byte(buffer[j]);
		} else {
			spi_send_byte(0x01);
		}
	}
	// LCD CS HIGH (deselect LCD)
	GPIOC->BSHR = ((1<<PIN_LCD_CS)<<0);
}

void lcd_init(void) {
	//Delay_Ms(10); // Wait for power to stabalize (LCD recommends >1ms); Commented out because the card initializaton takes at least 10ms, which's enough.
	// Set LCD CS DC to LOW, also toggle LCD RES pin (first set it to LOW, then set it to HIGH)
	GPIOC->BSHR = (((1<<PIN_LCD_RES)|(1<<PIN_LCD_CS)|(1<<PIN_LCD_DC))<<16);
	Delay_Us(100); // LCD's requirement: >5us
	GPIOC->BSHR = ((1<<PIN_LCD_RES)<<0);
	Delay_Us(100); // LCD's requirement: >5us

	// Send out the initialization sequence
	for (size_t i=0; i<sizeof(LCD_INIT_SEQUENCE)/sizeof(*LCD_INIT_SEQUENCE); i++) {
		spi_send_byte(LCD_INIT_SEQUENCE[i]);
	}

	// Clear display row 0. Fill row1..7 with 0xAA
	for (size_t i=0; i<8; i++) {
		lcd_transfer_row(i, display_buffer);
	}
}

__attribute__((noreturn)) int main() {
	SystemInit();

	// Reset GPIOC and SPI
	RCC->PB2PRSTR |= RCC_IOPCRST|RCC_SPI1RST;
	RCC->PB2PRSTR &= ~(RCC_IOPCRST|RCC_SPI1RST);

	// Enable the GPIO and SPI
	RCC->PB2PCENR |= RCC_IOPCEN | RCC_SPI1EN;

	// Deselect the card and the LCD by setting the pins HIGH. Also turn off the backlight by setting it LOW
	GPIOC->BSHR = ((1<<PIN_CARD_CS)<<0) | ((1<<PIN_LCD_CS)<<0) | ((1<<PIN_LCD_BL)<<16);

	// GPIO C0 to output PUSH-PULL, C5..C6 to output ALT PUSH-PULL, C7 to INPUT FLOATING (must use floating because we have external pull-up to take care of MMC requirement)
	GPIOC->CFGLR = (GPIO_CFGLR_OUT_PP << (4*0)) | (GPIO_CFGLR_OUT_PP << (4*1)) | (GPIO_CFGLR_OUT_PP << (4*2)) | (GPIO_CFGLR_OUT_PP << (4*3)) | (GPIO_CFGLR_OUT_PP << (4*4)) // GPIO
					| (GPIO_CFGLR_OUT_AF_PP << (4*5)) | (GPIO_CFGLR_OUT_AF_PP << (4*6)) | (GPIO_CFGLR_IN_FLOAT << (4*7)); // SPI

	// Configure SPI. SPI_Mode_Master and SPI_CTLR1_SPE must be set after CS pin is high
	SPI1->CTLR1 =	SPI_CTLR1_BR_2 | SPI_CTLR1_BR_1 | SPI_CTLR1_BR_0 // Required data rate for card: 100kHz - 400kHz (i.e. setting BR to 111b, which divides 48Mhz by 256 = 187kHz)
					| (SPI_CPOL_Low | SPI_CPHA_1Edge) // SPI Mode 0 (That's for the card. LCD requires SPI mode 3 instead (SPI_CPOL_High | SPI_CPHA_2Edge))
					| SPI_NSS_Soft // Software NSS mode
					| SPI_Mode_Master // Master mode
					| 0 // (lack of SPI_CTLR1_DFF) 8bit mode
					| SPI_Direction_2Lines_FullDuplex // Use both MOSI and MISO
					| SPI_CTLR1_SPE; // SPI begin!

	// Must do card initialization before doing LCD initialization to put the card into SPI mode.
	FATFS fs;
	if (pf_mount(&fs) != FR_OK){
		// No SD card. No need to perform further handling
		launch_user_code();
	}

	// Setting the divider to 16. That'd be 48Mhz/16 = 3Mhz
	// SPI1->CTLR1 &= ~SPI_CTLR1_BR;
	// SPI1->CTLR1 |= SPI_CTLR1_BR_0|SPI_CTLR1_BR_1;
	SPI1->CTLR1 &= ~(SPI_CTLR1_BR_2);

	size_t retries = 0;
	uint8_t file_content_changed = 0;
	uint8_t error_detected = 0;

	// This algorithm keeps scaning for differences between the firmware file and the flash content
	// Flashing is performed in case of differences.
	// If flashing had been performed, a full scan would be performed again
	// This process repeats until the retry limit exceeded
	// For happy path, the retry count should never exist 1 (one iteration for write, another iteration for read).
	do {
		FRESULT res;
		display_buffer[DISPLAY_RETRIES_POS+1] = retries;
		file_content_changed = 0;
		res = pf_open(FLASH_FILENAME);

		if(retries == 0) {
			if (res != FR_OK) {
				// Firmware file not found / cannot be opened. Launching user code!
				launch_user_code();
			}

			// Firmware file detected. Entering firmware update mode
			// Start by displaying firmware update screen on the LCD
			lcd_init();

			// Display smiley eyes
			display_buffer[DISPLAY_SMILEY_POS] = 0x24;

			// Display markers for indicating the status
			display_buffer[DISPLAY_RETRIES_POS] = 0x55;
			display_buffer[DISPLAY_SECTOR_POS] = 0x55;
			display_buffer[DISPLAY_ERROR_POS] = 0x55;

			lcd_transfer_row(0, display_buffer);

			flash_unlock();
		} else if (res != FR_OK) {
			display_buffer[DISPLAY_ERROR_POS+1] = res;
			error_detected = 1;
			break;
		}

		UINT bytes_read;
		size_t offset = FLASH_START_OFFSET;
		do {
			display_buffer[DISPLAY_SECTOR_POS+1] = (offset-FLASH_START_OFFSET)/FLASH_SECTOR_SIZE;
			lcd_transfer_row(0, display_buffer);

			static BYTE buf[FLASH_SECTOR_SIZE];
			memset(buf, 0xFF, FLASH_SECTOR_SIZE);
			res = pf_read(buf, FLASH_SECTOR_SIZE, &bytes_read);
			if(res != FR_OK) {
				display_buffer[DISPLAY_ERROR_POS+1] = res;
				error_detected = 1;
				break;
			}
			file_content_changed |= flash_write_sector(offset, buf);
			offset += bytes_read;
		} while (bytes_read == FLASH_SECTOR_SIZE && offset+FLASH_SECTOR_SIZE <= FLASH_END_OFFSET);
	} while (file_content_changed && ++retries<FLASH_MAX_RETRIES && !error_detected);
	flash_lock();

	// Display the smiley
	display_buffer[DISPLAY_SMILEY_POS+3] = 0x3C; // base of the mouth
	if(error_detected || file_content_changed) { // Error occurred or max retry exceeded
		display_buffer[DISPLAY_SMILEY_POS+4] = 0x42; // frown
	} else {
		display_buffer[DISPLAY_SMILEY_POS+2] = 0x42; // smile
	}
	lcd_transfer_row(0, display_buffer);

	#ifdef TURN_ON_BACKLIGHT_UPON_COMPLETION
		GPIOC->BSHR = (1<<PIN_LCD_BL)<<0;
	#endif

	// Show the result for a while, then launch user code
	Delay_Ms(10000);
	launch_user_code();
}
