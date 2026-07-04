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
#include "ch32fun.h"
#include <stdbool.h>

struct adc_row_scan_config {
	uint8_t pin_channel;
	uint32_t adc_scan_sequence; // The value set to ADC1->RSQR3
};

// Layout of adc_dma_buffer/adc_button_state/adc_button_just_pressed/adc_button_just_released:
//      [16]
// [0]  [1]  [2]  [3]
// [4]  [5]  [6]  [7]
// [8]  [9]  [10] [11]
// [12] [13] [14] [15]
// 17: card insert
// 18 and 19: Vref internal voltage reference

// 16bit would fit but I'm using 32bit for performance
uint32_t adc_dma_buffer[20];
uint32_t adc_dma_buffer_debounce; // Record the previous data. For debouncing. Only take the input after 2 consecutive same data.
uint32_t adc_button_state; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read by adc_button_get_state()
uint32_t adc_button_just_pressed; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read and reset in adc_button_get_just_pressed()
uint32_t adc_button_just_released; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read and reset in adc_button_get_just_released()
bool adc_card_inserted; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read by adc_card_is_inserted()
bool adc_card_just_inserted; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read by adc_card_has_insert_event(), reset by adc_card_reset_insert_event()
bool adc_card_just_removed; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read and reset by adc_card_is_just_removed()
uint32_t adc_vref_reading_smoothed; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read by adc_get_supply_voltage()
bool adc_reading_ready; // CONCURRENCY_VARIABLE: Written in adc_button_scan_next_row(), read by adc_is_reading_ready()
#define ADC_BUTTON_STATE_SIZE (17)
#define ADC_BUTTON_PRESSED_THRESHOLD (1024) // Any ADC reading above this value would be considered as button pressed
#define ADC_READING_READY_THRESHOLD (10) // The ADC readings is regarded as READY after this amount of complete scans

#if ADC_BUTTON_STATE_SIZE >= 32
	#error adc_button_state, adc_button_just_pressed and adc_button_just_released are unable to accomodate the content of adc_dma_buffer
#endif

static const struct adc_row_scan_config row_scan_config[] = {
	{.pin_channel = 2, .adc_scan_sequence = (4U << (5*0)) | (7U << (5*1)) | (5U << (5*2)) | (6U << (5*3))},
	{.pin_channel = 3, .adc_scan_sequence = (3U << (5*0)) | (7U << (5*1)) | (5U << (5*2)) | (6U << (5*3))},
	{.pin_channel = 4, .adc_scan_sequence = (3U << (5*0)) | (4U << (5*1)) | (5U << (5*2)) | (6U << (5*3))},
	{.pin_channel = 5, .adc_scan_sequence = (3U << (5*0)) | (4U << (5*1)) | (7U << (5*2)) | (6U << (5*3))},
	{.pin_channel = 6, .adc_scan_sequence = (3U << (5*0)) | (4U << (5*1)) | (8U << (5*2)) | (8U << (5*3))}, // The last two channels here is set to the internal Vref
};

#define ROW_SCAN_SEQUENCE_CHANNEL_COUNT (4)
// ADC sampling time configuration. configuring 0~7 means 3.5/7.5/11.5/19.5/35.5/55.5/71.5/239.5 cycles
// At maximum value 7, ADC clk div16, it's empiracally found that a full scan would be performed at frequency of 727 Hz
#define ADC_SAMPLING_TIME (7)

void adc_button_scan_next_row(void) {
	static size_t row_scan_index = 0;
	static size_t scan_completion_counter = 0;

	// read memory barrier to make sure that the content of adc_dma_buffer is fresh
	asm volatile("fence ir,ir" ::: "memory");
	// Compiler-level barrier for adc_... variables so that the adc_... variables wouldn't have to be defined as volatile
	asm volatile("" ::: "memory");

	// Handle button press and save the states to adc_button_state, adc_button_just_pressed and adc_button_just_released
	// Not performing debounce handling because the ADC reading's slow enough
	for(size_t i=ROW_SCAN_SEQUENCE_CHANNEL_COUNT*row_scan_index; i<ROW_SCAN_SEQUENCE_CHANNEL_COUNT*(row_scan_index+1) && i<ADC_BUTTON_STATE_SIZE; i++) {
		uint32_t button_mask = (1U<<i);
		bool button_pressed = (adc_dma_buffer[i] >= ADC_BUTTON_PRESSED_THRESHOLD);
		if(button_pressed && (adc_dma_buffer_debounce & button_mask)) {
			// Button held
			if(!(adc_button_state & button_mask)) {
				adc_button_just_pressed |= button_mask;
			}
			adc_button_state |= button_mask;
		} else if (!button_pressed && !(adc_dma_buffer_debounce & button_mask)) {
			// Button not held
			if(adc_button_state & button_mask) {
				adc_button_just_released |= button_mask;
			}
			adc_button_state &= ~button_mask;
		}
		// For debouncing. Only count the button press after detecting the same value twice
		if(button_pressed) {
			adc_dma_buffer_debounce |= button_mask;
		} else {
			adc_dma_buffer_debounce &= ~button_mask;
		}
	}

	// Derive card insertion state and vref
	if(row_scan_config[row_scan_index].pin_channel == 6) {
		bool adc_card_inserted_prev = adc_card_inserted;
		adc_card_inserted = (adc_dma_buffer[17] >= ADC_BUTTON_PRESSED_THRESHOLD);
		if(!adc_card_inserted_prev && adc_card_inserted) {
			adc_card_just_inserted = true;
		} else if(adc_card_inserted_prev && !adc_card_inserted) {
			adc_card_just_removed = true;
		}
		uint32_t adc_vref_reading = (adc_dma_buffer[18]+adc_dma_buffer[19])/2;
		adc_vref_reading_smoothed = (adc_vref_reading_smoothed*7 + adc_vref_reading*1 + 4)/8; // +4 for rounding off the /8
	}

	// Determine which row to scan next
	if(++row_scan_index >= sizeof(row_scan_config)/sizeof(*row_scan_config)) {
		row_scan_index = 0;
		// Determine if the reading is ready to be retrieved
		if(scan_completion_counter <= ADC_READING_READY_THRESHOLD) {
			if(scan_completion_counter == ADC_READING_READY_THRESHOLD) {
				// Reset just pressed and just released button events so that the button held before the readings' ready
				// would be disregarded
				adc_button_just_pressed = 0;
				adc_button_just_released = 0;
				// Make adc_is_reading_ready() return true
				adc_reading_ready = true;
			}
			scan_completion_counter++;
		}
	}

	// Configure DMA for the next read
	DMA1_Channel1->CFGR &= ~DMA_CFGR1_EN; // As per the specs, must disable DMA channel before configuring it
	DMA1_Channel1->MADDR = (uint32_t)&adc_dma_buffer[ROW_SCAN_SEQUENCE_CHANNEL_COUNT*row_scan_index]; // memory destination
	DMA1_Channel1->CNTR = ROW_SCAN_SEQUENCE_CHANNEL_COUNT; // number of items to read
	DMA1_Channel1->CFGR |= DMA_CFGR1_EN; // Re-enable the DMA channel

	// Configure GPIO for button scanning. The scanning row pin is set to output (which's set to HIGH inside adc_init()). Other pins are set to analog input.
	GPIOD->CFGLR &= ~((GPIO_CFGLR_MASK<<(4*2)) | (GPIO_CFGLR_MASK<<(4*3)) | (GPIO_CFGLR_MASK<<(4*4)) | (GPIO_CFGLR_MASK<<(4*5)) | (GPIO_CFGLR_MASK<<(4*6))); // Set PD2..6 to analog input
	GPIOD->CFGLR |= (GPIO_CNF_OUT_PP|GPIO_Speed_10MHz) << (4*row_scan_config[row_scan_index].pin_channel); // Set the scanning pin to output HIGH

	// Configure scanning channels
	ADC1->RSQR3 = row_scan_config[row_scan_index].adc_scan_sequence;

	// Kick off ADC conversion sequence
	ADC1->CTLR2 |= ADC_ADON;
}

void INTERRUPT_DECORATOR DMA1_Channel1_IRQHandler(void) {
	DMA1->INTFCR |= DMA_CTCIF1;
	adc_button_scan_next_row();
}

void adc_init(void) {
	// This module implements charlieplex'd button handling with NKRO support
	// It uses five pins (PD2..6) to take 18 inputs
	// In addition, it also handles reading of internal voltage reference for deriving the supply voltage

	// Reset ADC
	RCC->PB2PRSTR |= RCC_ADC1RST;
	RCC->PB2PRSTR &= ~RCC_ADC1RST;

	// ADC clock is HB clock divided by 16. i.e. 48Mhz / 16 = 3Mhz
	// I intentionally picked a slow rate.
	// That's because a fast rate would cause the card detection P-MOSFET to interfere with
	// the reading of other BTN1 pin readings.
	// With this slow rate interference still exist but it's small enough not to cause problem
	RCC->CFGR0 = (RCC->CFGR0 & ~(RCC_ADCPRE | RCC_CFGR0_ADC_CLK_MODE | RCC_CFGR0_ADC_CLK_ADJ)) | RCC_ADCPRE_DIV16;

	// Enable GPIOD and ADC
	RCC->PB2PCENR |= RCC_ADCEN | RCC_IOPDEN;

	// Set PD2..6 to analog input
	GPIOD->CFGLR &= ~((GPIO_CFGLR_MASK<<(4*2)) | (GPIO_CFGLR_MASK<<(4*3)) | (GPIO_CFGLR_MASK<<(4*4)) | (GPIO_CFGLR_MASK<<(4*5)) | (GPIO_CFGLR_MASK<<(4*6)));
	// Set the output of PD2..6 to HIGH so that when the pin is set as output mode, it'd be outputting HIGH.
	GPIOD->BSHR = (1<<2) | (1<<3) | (1<<4) | (1<<5) | (1<<6);

	// Initialize all ADC registers with default value
	RCC->PB2PRSTR |= RCC_ADC1RST;
	RCC->PB2PRSTR &= ~RCC_ADC1RST;

	// Enable DMA (other component may also enable DMA on their own. No harm to enable it multiple times.)
	RCC->HBPCENR |= RCC_DMA1EN;

	// Configure DMA for ADC
	DMA1_Channel1->PADDR = (uint32_t)(&ADC1->RDATAR); // Peripheral address register

	DMA1_Channel1->CFGR =
		DMA_CFGR1_PL_0 | // Set the priority to "Medium"
		DMA_CFGR1_PSIZE_0 | // 16bit data for peripheral
		DMA_CFGR1_MSIZE_1 | // 32bit data for memory
		DMA_CFGR1_MINC | // Incrememt memory address
		// (Not specifying DMA_CFGR1_DIR) Read from peripheral, write to memory
		DMA_CFGR1_TCIE; // Enable transfer-complete interrupt

	// Clear the interrupt flag, just in case.
	DMA1->INTFCR |= DMA_CTCIF1;

	// Configure ADC
	ADC1->CTLR2 = ADC_ADON; // Enable ADC
	Delay_Us(1); // Wait for the ADC to stabalize (tSTAB)

	ADC1->CTLR1 = ADC_SCAN; // Enable scan mode (scan thru all channels in the sequence)
	ADC1->CTLR2 |= ADC_DMA; // Enable DMA mode
	ADC1->RSQR1 = (ROW_SCAN_SEQUENCE_CHANNEL_COUNT-1) << 20; // 4 channels required
	ADC1->RSQR2 = 0;

	// Set sampling time of each ADC channel
	ADC1->SAMPTR2 = (ADC_SAMPLING_TIME<<(3*0)) | // Channel 0
					(ADC_SAMPLING_TIME<<(3*1)) | // Channel 1
					(ADC_SAMPLING_TIME<<(3*2)) | // Channel 2
					(ADC_SAMPLING_TIME<<(3*3)) | // Channel 3
					(ADC_SAMPLING_TIME<<(3*4)) | // Channel 4
					(ADC_SAMPLING_TIME<<(3*5)) | // Channel 5
					(ADC_SAMPLING_TIME<<(3*6)) | // Channel 6
					(ADC_SAMPLING_TIME<<(3*7)) | // Channel 7
					(ADC_SAMPLING_TIME<<(3*8)) | // Channel 8
					(ADC_SAMPLING_TIME<<(3*9)); // Channel 9

	// Configure interrupt controller
	// PFIC: Enable interrupt for DMA1_Channel1_IRQn
	PFIC->IPRIOR[DMA1_Channel1_IRQn] = 0x00; // The priority is 0 (the highest, and it cannot be preempted)
	PFIC->IENR[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));

	DMA1_Channel1->CFGR |= DMA_CFGR1_EN; // Enable DMA channel

	// Initialize variables
	asm volatile("" ::: "memory");
	memset(adc_dma_buffer, 0, sizeof(adc_dma_buffer));
	adc_dma_buffer_debounce = 0;
	adc_button_state = 0;
	adc_button_just_pressed = 0;
	adc_button_just_released = 0;
	adc_card_inserted = false;
	adc_card_just_inserted = false;
	adc_card_just_removed = false;
	adc_vref_reading_smoothed = 0;
	adc_reading_ready = false;

	// Kick off the first scan!
	adc_button_scan_next_row();
}

bool adc_is_reading_ready(void) {
	asm volatile("" ::: "memory");
	return adc_reading_ready;
}

uint32_t adc_button_get_state(void) {
	asm volatile("" ::: "memory");
	return adc_button_state;
}

uint32_t adc_button_get_just_pressed(void) {
	PFIC->IRER[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
	asm volatile("" ::: "memory");
	uint32_t ret = adc_button_just_pressed;
	adc_button_just_pressed = 0;
	PFIC->IENR[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
	return ret;
}

uint32_t adc_button_get_just_released(void) {
	PFIC->IRER[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
	asm volatile("" ::: "memory");
	uint32_t ret = adc_button_just_released;
	adc_button_just_released = 0;
	PFIC->IENR[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
	return ret;
}

bool adc_card_is_inserted(void) {
	asm volatile("" ::: "memory");
	return adc_card_inserted;
}

bool adc_card_has_insert_event(void) {
	asm volatile("" ::: "memory");
	return adc_card_just_inserted;
}

void adc_card_reset_insert_event(void) {
	PFIC->IRER[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
	asm volatile("" ::: "memory");
	adc_card_just_inserted = false;
	PFIC->IENR[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
}

bool adc_card_is_just_removed(void) {
	PFIC->IRER[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
	asm volatile("" ::: "memory");
	uint32_t ret = adc_card_just_removed;
	adc_card_just_removed = false;
	PFIC->IENR[DMA1_Channel1_IRQn/32] |= (1<<(DMA1_Channel1_IRQn%32));
	return ret;
}


uint32_t adc_get_supply_voltage(void) {
	asm volatile("" ::: "memory");
	return 1200*4095/adc_vref_reading_smoothed;
}
