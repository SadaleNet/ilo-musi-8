all : flash

TARGET:=ilomusi8
CH32FUN:=$(CH32FUN_PATH)/ch32fun
TARGET_MCU:=CH32V006

ADDITIONAL_C_FILES+=adc.c watchdog.c

include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
