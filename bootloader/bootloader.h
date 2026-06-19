#ifndef _BOOTLOADER_H
#define _BOOTLOADER_H

#include <stdint.h>

// Port: GPIOC
#define PIN_CARD_CS (0)
#define PIN_LCD_CS (1)
#define PIN_LCD_DC (2)
#define PIN_LCD_BL (3)
#define PIN_LCD_RES (4)

uint8_t spi_send_byte(uint8_t data);


// Configuration constants
#define FLASH_START_OFFSET (0x08000000)
#define FLASH_END_OFFSET (0x0800F800)
#define FLASH_SECTOR_SIZE (1024)
#define FLASH_PAGE_SIZE (256)

// Algorithm: scan and program while scanning if file content is different from flash content
// Keep scanning until there's no difference until FLASH_MAX_RETRIES is reached
#define FLASH_MAX_RETRIES (5)
#define FLASH_FILENAME "ILOMUSI8.BIN" // Must be in uppercase. 8.3 filename

// Position to display the update status. 0 is leftmost, 127 is rightmost
#define DISPLAY_RETRIES_POS (38)
#define DISPLAY_SECTOR_POS (54)
#define DISPLAY_ERROR_POS (70)
#define DISPLAY_SMILEY_POS (86)

// Pros: much better visibility to human eyes
//       easier to take photo if focused correctly on the LCD
// Cons: takes FLASH space;
//       the contrast would cause camera unable to capture unless the LCD got focused by the camera;
//       power draw might lead to instability in case the battery's running out
//#define TURN_ON_BACKLIGHT_UPON_COMPLETION

#endif
