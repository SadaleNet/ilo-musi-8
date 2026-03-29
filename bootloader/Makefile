all : flash

TARGET:=bootloader
LINKER_SCRIPT:=./linker_script/bootloader.ld
WRITE_SECTION:=bootloader
FLASH_COMMAND:=$(CH32FUN_PATH)/minichlink/minichlink -a -U -w $(TARGET).bin $(WRITE_SECTION) -B

TARGET_MCU?=CH32V006
ADDITIONAL_C_FILES+=pff3a/diskio.c pff3a/pff.c
include $(CH32FUN_PATH)/ch32fun/ch32fun.mk

optionbytes :
	$(MINICHLINK)/minichlink -w +a55af708 option # Enable bootloader, enable RESET

flash : cv_flash
clean : cv_clean
