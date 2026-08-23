**(English: Scroll down for English | toki ike Inli li lon anpa pi toki pona)**

# sona pi ilo musi 8

kepeken sona ni la ilo musi 8 li ken pali. sina ken pana e sona ni tawa ilo musi 8 kepeken nasin tu:

* nasin nanpa wan: o kepeken ilo `CH32FUN_PATH=/path/to/ch32fun make`. kin la sina ken kepeken ilo `minichlink` kepeken lipu `ilomusi8.bin`
* nasin nanpa tu: o pana e `ilomusi8.bin` tawa lipu sona "TF Card". o pana e lipu sona ni tawa ilo musi 8 sina. sina pana e wawa la sona pi ilo musi 8 li kama sona `ilomusi8.bin`

# Firmware of ilo musi 8

This is the firmware of ilo musi 8. You can flash it with the following means:

* Method 1: Run `CH32FUN_PATH=/path/to/ch32fun make`. Alternatively you can use `minichlink` to load the prebuilt binary `ilomusi8.bin` to your ilo musi 8 unit
* Method 2: Put the `ilomusi8.bin` to a TF card and insert it into your ilo musi 8 unit. Power it on and the bootloader would flash it

# Technical Documentation

(ni li ike suli. mi o kepeken ala toki pona!)

## To Build

Download [ch32fun](https://github.com/cnlohr/ch32fun/tree/master) separately.
Run `CH32FUN_PATH=/path/to/ch32fun make` to build.
It'd also perform flashing if you attached your ilo nena 8 via WCH-LinkE.

## DMA Allocation

The following DMA channels are used:

* Very high priority: SPI RX, DMA channel 2. Used by the TF Card operation `rcvr_mmc_with_crc()`.
* High Priority: Buzzer Timer (TIM3), DMA channel 4. Triggering rate depends on the pitch.
* Medium Priority: Button ADC, DMA channel 1. Triggered for each row scan.
* Low Priority: SPI TX, DMA channel 3. Used by the lcd operation `lcd_transfer_next_row()`, and the TF Card operations `xmit_mmc_with_crc()` and `rcvr_mmc_with_crc()`.

## ISR Allocation

The follow ISR handlers has been configured:

* Highest priority (0x00): Button ADC (DMA1_Channel1_IRQHandler). Triggered upon completion of row scanning
* Second highest priority (0x80): SPI TX for LCD (DMA1_Channel3_IRQHandler). Triggered upon completion of transfer of an LCD data row

The MCU supports two levels of preemption and the ADC interrupt handler is configured to be able to preempt the LCD interrupt handler.

## SPI Sharing

The SPI peripheral is shared between TF card and the LCD as follows:

* SPI_SCK - shared
* SPI_MOSI - shared
* SPI_MISO - for TF card only
* CARD_CS - for TF card only
* LCD_CS - for LCD only

Upon insertion of the TF card, it uses an incompatible proprietary protocol with CARD_CS ignored. It must be switched to SPI mode as soon as possible to avoid LCD commands messing with the card.

This mechanism is handled in `file_loop()`, which gets called once in the main loop of `main()`.

## PWM Configuration

TIM1 is used for PWM with frequency of 400khz. The valid PWM value is 0~15 (0 is full off, 15 is full on, 1~14 is anything in between).

It's used for LCD backlight adjustment and the buzzer volume adjustment.

* Initialization function: See `tim1_pwm_init()`
* LCD backlight adjustment: See `lcd_set_brightness()`
* Buzzer volume adjustment: See `buzzer_reload_dma_buffer()`. The volume (0-15) gets loaded to the DMA buffer, which's played by DMA1_Channel4.
	* For example, if the waveform is `[0,1,0,0,1,0,...]` and the volume is 15, the DMA buffer would be `[0,15,0,0,15,0,...]`
	* For the same waveform, if the volume is 8, the DMA buffer would become `[0,8,0,0,8,0,...]`

## CHIP-8 Emulator

The `chip8.c` and `chip8.h` are an implementation of CHIP-8 emulator. These two files are intentionally self-contained and does not make reference to any other files in this repo.

This way one could easily reuse the emulator in any other C codebases.

This is also the reason why the draw function `DXYN` does not share the draw functions in `draw.c`
