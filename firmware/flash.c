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

#include "flash.h"
#include "ch32fun.h"
#include <assert.h>
#include <stdio.h> // TODO: remove

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

void flash_erase_256(uint32_t offset) {
	assert(offset%FLASH_PAGE_SIZE == 0 && offset >= FLASH_CONFIG_START && offset < FLASH_CONFIG_END);

	bool already_erased = true;
	for(uint32_t addr=offset; addr<offset+FLASH_PAGE_SIZE; addr+=sizeof(uint32_t)) {
		if(*((volatile uint32_t*)addr) != 0xFFFFFFFF) {
			already_erased = false;
			break;
		}
	}
	if(already_erased) {
		// The content was already empty. No need to erase again
		return;
	}

	// Perform fast erase
	while(FLASH->STATR & FLASH_STATR_BSY){}
	FLASH->CTLR |= FLASH_CTLR_PAGE_FTER;
	FLASH->ADDR = offset;
	FLASH->CTLR |= FLASH_CTLR_STRT;
	while(!(FLASH->STATR & FLASH_STATR_EOP)){}
	FLASH->STATR |= FLASH_STATR_EOP; // Clear EOP
	FLASH->CTLR &= ~FLASH_CTLR_PAGE_FTER;
}

void flash_write_256(uint32_t offset, const void *data) {
	assert(offset%FLASH_PAGE_SIZE == 0 && offset >= FLASH_CONFIG_START && offset < FLASH_CONFIG_END);

	// Perform fast programming
	while(FLASH->STATR & FLASH_STATR_BSY){}
	FLASH->CTLR |= FLASH_CTLR_PAGE_FTPG;
	FLASH->CTLR |= FLASH_CTLR_BUF_RST;
	while(!(FLASH->STATR & FLASH_STATR_EOP)){}
	FLASH->STATR |= FLASH_STATR_EOP; // Clear EOP
	for(uint32_t i=0; i<FLASH_PAGE_SIZE; i+=sizeof(uint32_t)) {
		*((volatile uint32_t*)(offset+i)) = *((uint32_t*)(data+i));
		FLASH->CTLR |= FLASH_CTLR_BUF_LOAD;
		while(FLASH->STATR & FLASH_STATR_BSY){}
	}
	FLASH->ADDR = offset;
	FLASH->CTLR |= FLASH_CTLR_STRT;
	while(!(FLASH->STATR & FLASH_STATR_EOP)){}
	FLASH->STATR |= FLASH_STATR_EOP; // Clear EOP
	FLASH->CTLR &= ~FLASH_CTLR_PAGE_FTPG;
}

void flash_write_4(uint32_t offset, const uint32_t data) {
	assert(offset%4 == 0 && offset >= FLASH_CONFIG_START && offset < FLASH_CONFIG_END);

	uint32_t page_offset = offset/FLASH_PAGE_SIZE*FLASH_PAGE_SIZE;
	// Perform fast programming
	while(FLASH->STATR & FLASH_STATR_BSY){}
	FLASH->CTLR |= FLASH_CTLR_PAGE_FTPG;
	FLASH->CTLR |= FLASH_CTLR_BUF_RST;
	while(!(FLASH->STATR & FLASH_STATR_EOP)){}
	FLASH->STATR |= FLASH_STATR_EOP; // Clear EOP
	for(uint32_t i=0; i<FLASH_PAGE_SIZE; i+=sizeof(uint32_t)) {
		if(i == offset%FLASH_PAGE_SIZE) {
			// Only program the specified offset
			*((volatile uint32_t*)(page_offset+i)) = data;
		} else {
			// Retain the content for the non-specified offset
			// Due to the nature of flash memory, it can only be programmed from 1->0, not 0->1
			// Therefore, by programming 0xFFFFFFFF, the content would be unchanged.
			*((volatile uint32_t*)(page_offset+i)) = 0xFFFFFFFF;
		}
		FLASH->CTLR |= FLASH_CTLR_BUF_LOAD;
		while(FLASH->STATR & FLASH_STATR_BSY){}
	}
	FLASH->ADDR = page_offset;
	FLASH->CTLR |= FLASH_CTLR_STRT;
	while(!(FLASH->STATR & FLASH_STATR_EOP)){}
	FLASH->STATR |= FLASH_STATR_EOP; // Clear OP
	FLASH->CTLR &= ~FLASH_CTLR_PAGE_FTPG;
}
