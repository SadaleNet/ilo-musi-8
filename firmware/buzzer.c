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

#include "buzzer.h"

#include "ch32fun.h"
#include <stdint.h>
#include <stdlib.h>

#define BUZZER_BUFFER_LENGTH (128)

// Generated with Python: [round(48e6/(4000*2**((i-64)/48))-1) for i in range(256)]
const uint16_t BUZZER_ATRLR_MAP[] = {30237, 29804, 29376, 28955, 28540, 28131, 27727, 27330, 26938, 26552, 26171, 25796, 25426, 25062, 24702, 24348, 23999, 23655, 23316, 22981, 22652, 22327, 22007, 21692, 21381, 21074, 20772, 20474, 20181, 19891, 19606, 19325, 19048, 18775, 18506, 18240, 17979, 17721, 17467, 17216, 16970, 16726, 16486, 16250, 16017, 15787, 15561, 15338, 15118, 14901, 14688, 14477, 14269, 14065, 13863, 13664, 13469, 13275, 13085, 12897, 12713, 12530, 12351, 12174, 11999, 11827, 11657, 11490, 11325, 11163, 11003, 10845, 10690, 10537, 10385, 10237, 10090, 9945, 9802, 9662, 9523, 9387, 9252, 9120, 8989, 8860, 8733, 8608, 8484, 8363, 8243, 8125, 8008, 7893, 7780, 7668, 7559, 7450, 7343, 7238, 7134, 7032, 6931, 6832, 6734, 6637, 6542, 6448, 6356, 6265, 6175, 6086, 5999, 5913, 5828, 5745, 5662, 5581, 5501, 5422, 5344, 5268, 5192, 5118, 5044, 4972, 4901, 4830, 4761, 4693, 4626, 4559, 4494, 4429, 4366, 4303, 4242, 4181, 4121, 4062, 4004, 3946, 3890, 3834, 3779, 3725, 3671, 3619, 3567, 3515, 3465, 3415, 3366, 3318, 3271, 3224, 3177, 3132, 3087, 3043, 2999, 2956, 2914, 2872, 2831, 2790, 2750, 2711, 2672, 2633, 2596, 2558, 2522, 2486, 2450, 2415, 2380, 2346, 2312, 2279, 2246, 2214, 2182, 2151, 2120, 2090, 2060, 2030, 2001, 1973, 1944, 1916, 1889, 1862, 1835, 1809, 1783, 1757, 1732, 1707, 1683, 1659, 1635, 1611, 1588, 1565, 1543, 1521, 1499, 1477, 1456, 1435, 1415, 1395, 1375, 1355, 1335, 1316, 1297, 1279, 1260, 1242, 1224, 1207, 1190, 1172, 1156, 1139, 1123, 1107, 1091, 1075, 1060, 1044, 1029, 1015, 1000, 986, 972, 958, 944, 930, 917, 904, 891, 878, 866, 853, 841, 829, 817, 805, 794, 782, 771, 760};

static uint8_t buzzer_dma_buffer[2][BUZZER_BUFFER_LENGTH]; // double buffered. Contains values to be loaded to TIM1->CH1CVR
static uint8_t buzzer_buffer[BUZZER_BUFFER_LENGTH/8]; // Format: Same as the one supplied by buzzer_set_buffer(). Required for volume adjustment
static uint8_t buzzer_current_volume;

// Workaround of an incorrect definition in ch32fun.h
#ifdef RCC_TIM3EN
	#undef RCC_TIM3EN
	#define RCC_TIM3EN ((uint32_t)0x00000004)
#endif

static void buzzer_reload_dma_buffer(void) {
	static size_t buzzer_dma_buffer_index = 0;
	size_t new_buffer_index = !buzzer_dma_buffer_index;
	// Expands buzzer_buffer (16 bytes, MSB first for each byte) into 128bytes of audio sample
	for(size_t i=0; i<BUZZER_BUFFER_LENGTH; i++) {
		if(buzzer_buffer[i/8] & (1<<(7-(i%8)))) {
			buzzer_dma_buffer[new_buffer_index][i] = buzzer_current_volume;
		} else {
			buzzer_dma_buffer[new_buffer_index][i] = 0;
		}
	}
	// write memory barrier to flush the content of the buffer for DMA to load
	asm volatile("fence ow,ow" ::: "memory");
	DMA1_Channel4->MADDR = (uint32_t)(buzzer_dma_buffer[new_buffer_index]); // Memory address register
	buzzer_dma_buffer_index = new_buffer_index;
}

void buzzer_init(void) {
	// Here's how this buzzer module works.
	// It uses DMA_CH4 to automatically play audio sample repeatedly.
	// The audio buffer "buzzer_dma_buffer" is double-buffered
	// TIM3_CH4 is used for pitch adjustment
	// TIM1_CH1 is used for volume adjustment and it's output'd to the pin PD0.
	// The DMA_CH4 cycle mode's used.
	// Whenever DMA_CH4 got triggered, it'd copy a byte from buzzer_dma_buffer[x] to TIM1's comparison register,
	// the comparison register's content is either VOLUME or zero. So the same audio sample would require a
	// different buzzer_dma_buffer[x] content depending on the volume.
	// The trigger source of DMA_CH4 is TIM3_CH4.
	// Therefore, the rate that TIM3 reloads would would control the rate that the buffer got copied,
	// which is the sample playback speed, which is the pitch.
	// Once the DMA buffer setup's completed, the audio sample would be played by the DMA
	// without the CPU getting involved.

	// Enable the TIM3 clock source
	RCC->PB1PRSTR |=  RCC_TIM3RST;
	RCC->PB1PRSTR &= ~(RCC_TIM3RST);
	RCC->PB1PCENR |= RCC_TIM3EN;

	// DMA got triggered when the comparison register matches this value
	// That'd be 0, the same as teh default value. No need to change that
	// Commeting out
	//TIM3->CH4CVR = 0;
	TIM3->DMAINTENR |= TIM3_DMAINTENR_CC4DE | TIM3_DMAINTENR_OC4PE;
	TIM3->CTLR1 |= TIM3_CTLR_ARPE | TIM3_CTLR_CEN;

	// Configure DMA for automated audio sample playback
	RCC->HBPCENR |= RCC_DMA1EN; // Enable DMA (other component may also enable DMA on their own. No harm to enable it multiple times.)

	DMA1_Channel4->PADDR = (uint32_t)(&TIM1->CH1CVR); // Peripheral address register
	DMA1_Channel4->CNTR = BUZZER_BUFFER_LENGTH;
	DMA1_Channel4->CFGR =
		(DMA_CFGR1_PL_0 | DMA_CFGR1_PL_1) | // Set the priority to "Very High"
		DMA_CFGR1_PSIZE_0 | // 16bit data for peripheral
		// (Not specifying DMA_CFGR1_MSIZE) 8bit data for memory
		DMA_CFGR1_MINC | // Incrememt memory address
		DMA_CFGR1_CIRC | // Enable cycle mode
		DMA_CFGR1_DIR | // Read from memory, write to peripheral
		DMA_CFGR1_EN; // Enable channel

	// Initialize DMA buffer and variables
	memset(buzzer_buffer, 0, sizeof(buzzer_buffer));
	memset(buzzer_dma_buffer, 0, sizeof(buzzer_dma_buffer));
	buzzer_current_volume = 0;
	buzzer_set_pitch(0);
	buzzer_reload_dma_buffer();

	// Configure PD0 as AF_PP after everything else's ready
	GPIOD->CFGLR &= ~(GPIO_CFGLR_MASK << (4*0));
	GPIOD->CFGLR |= (GPIO_CFGLR_OUT_AF_PP << (4*0));
}
void buzzer_set_volume(uint8_t volume) {
	if(volume != buzzer_current_volume) { // Performance optimization: Do not modify buffer if volume hasn't been changed
		buzzer_current_volume = volume;
		buzzer_reload_dma_buffer();
	}
}

void buzzer_set_buffer(const void *buffer) {
	memcpy(buzzer_buffer, buffer, sizeof(buzzer_buffer));
	buzzer_reload_dma_buffer();
}

void buzzer_set_pitch(uint8_t pitch) {
	TIM3->ATRLR = BUZZER_ATRLR_MAP[pitch];
}
