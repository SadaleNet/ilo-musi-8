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

#include "ch32fun.h"

void tim1_pwm_init(void) {
	// TIM1 is shared by buzzer and LCD backlight.
	// It provides very high frequency clock for driving PWM signal
	// The PWM frequency is 3.2Mhz. The duty cycle can be set to 0~15. 0 is always LOW. 15 is always HIGH.
	// TIM1_CH1N is used for Buzzer audio playback. Use TIM1->CH1CVR to adjust duty cycle.
	// TIM1_CH3 is used for LCD backlight. Use TIM1->CH3CVR to adjust duty cycle.

	// Reset TIM1
	RCC->PB2PRSTR |= RCC_TIM1RST;
	RCC->PB2PRSTR &= ~RCC_TIM1RST;
	// Enable the TIM1 clock source
	RCC->PB2PCENR |= RCC_TIM1EN;

	TIM1->PSC = 0x0000; // Prescaler is 1
	TIM1->ATRLR = 14; // Autoreload value is 14, allowing CHxCVR value range of 0~15.
	TIM1->CCER |= TIM1_CCER_CC1NE | // Enable TIM1_CH1N output, not flipping polarity because TIM1_CCER_CC1NP is not specified
					TIM1_CCER_CC3E; // Enable TIM1_CH3 output, not flipping polarity because TIM1_CCER_CC3P is not specified
	TIM1->SWEVGR |= TIM1_SWEVGR_UG; // Update the autoreload register
	TIM1->CHCTLR1 |= TIM1_CHCTLR1_OC1PE | TIM1_CHCTLR1_OC1M_2 | TIM1_CHCTLR1_OC1M_1; // Set TIM1_CH1 to PWM mode 1
	TIM1->CHCTLR2 |= TIM1_CHCTLR2_OC3PE | TIM1_CHCTLR2_OC3M_2 | TIM1_CHCTLR2_OC3M_1; // Set TIM1_CH3 to PWM mode 1
	TIM1->BDTR |= TIM1_BDTR_MOE; // Enable TIM1's output
	TIM1->CTLR1 |= TIM1_CTLR1_ARPE | TIM1_CTLR1_CEN; // Enable the timer itself
}
