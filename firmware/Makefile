all : flash_target

TARGET:=ilomusi8
CH32FUN:=$(CH32FUN_PATH)/ch32fun
TARGET_MCU:=CH32V006

ADDITIONAL_C_FILES+=fatfs/*.c chip8.c generated.c adc.c buzzer.c config.c draw.c file.c flash.c lcd.c spi.c tim1_pwm.c watchdog.c util.c

include $(CH32FUN)/ch32fun.mk

# Ideally I should be using the linker script for restricting the FLASH usage
# However, the linker script is on the upstream ch32fun repo.
# Since the linker script got update from time to time, I'd rather not to modify a copy of that.
# Instead, I'm gonna add an extra target `check_image_size` for checking the image size here.
# With usage limit of 56kB, That'd allow 62kB - 56kB = 6kB for user storage.
MAX_FLASH_USAGE:=57344
check_image_size : $(TARGET).elf
	$(eval IMAGE_SIZE=$(shell $(PREFIX)-size -B $(TARGET).elf | tail -n 1 | awk '{print $$1 + $$2}'))
	@if test $(IMAGE_SIZE) -ge $(MAX_FLASH_USAGE); then \
		echo "*** ERROR: Flash usage $(IMAGE_SIZE) B exceeded $(MAX_FLASH_USAGE) B, leaving no space for firwmare internal storage (Boot ROM and Global Config)"; \
		exit 1; \
	fi

flash_target : check_image_size cv_flash
clean : cv_clean
