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

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

int util_print_button_buffer(char *dest, uint16_t buttons) {
	int ret = 0;
	for(size_t i=0; i<16; i++) {
		if(buttons & (1<<i)) {
			if(i < 10) {
				dest[ret++] = i + '0';
			} else {
				dest[ret++] = i - 10 + 'A';
			}
		}
	}
	dest[ret] = '\0';
	return ret;
}

void __assert_func(const char*, int, const char*, const char*) {
	while(1);
}

void qsort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*)) {
	// Implements insertion sort
	uint8_t buf[16];
	assert(size <= sizeof(buf));
	for(size_t i=1; i<count; i++) {
		memcpy(buf, &((uint8_t*)ptr)[i*size], size);
		for(size_t j=i; j>=1; j--) {
			if(comp(buf, &((uint8_t*)ptr)[(j-1)*size]) < 0) {
				memcpy(&((uint8_t*)ptr)[j*size], &((uint8_t*)ptr)[(j-1)*size], size);
				if(j == 1) {
					memcpy((uint8_t*)ptr, buf, size);
				}
			} else {
				memcpy(&((uint8_t*)ptr)[j*size], buf, size);
				break;
			}
		}
	}
}
