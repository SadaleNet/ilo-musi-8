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

#include <stdbool.h>
#include <stdint.h>

// This module handles everything related to ADC, including
// button state detection, card insertion detection and measurement of supply voltage

void adc_init(void);

bool adc_is_reading_ready(void); // Readings of all functions below are invalid until this function returns true

// The returned value of the adc_button_* functions is a uint32_t as bitflags. 1 is held, 0 is released.
// Here's the layout:
//      [16]
// [0]  [1]  [2]  [3]
// [4]  [5]  [6]  [7]
// [8]  [9]  [10] [11]
// [12] [13] [14] [15]
uint32_t adc_button_get_state(void); // 1 is held. 0 is released.
uint32_t adc_button_get_just_pressed(void); // 1 is event triggered. 0 is idle. Calling this function clears the events. Cannot detect button held before device boot
uint32_t adc_button_get_just_released(void); // ditto

bool adc_card_is_inserted(void); // external memory card state. true if inserted. false else.
bool adc_card_has_insert_event(void);
void adc_card_reset_insert_event(void);
bool adc_card_is_just_removed(void);

uint32_t adc_get_supply_voltage(void); // Unit: mV. This function involves software division and might be slow. Don't call too often.
