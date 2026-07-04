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

uint8_t crc7_compute(const uint8_t table[256], uint8_t crc, const uint8_t *payload, size_t length) {
	for(size_t i=0; i<length; i++) {
		crc = table[(crc<<1) ^ payload[i]];
	}
	return crc & 0x7F;
}

uint16_t crc16_compute(const uint16_t table[256], uint16_t crc, const uint8_t *payload, size_t length) {
	for(size_t i=0; i<length; i++) {
		crc = (crc<<8) ^ table[(crc>>8) ^ payload[i]];
	}
	return crc;
}

uint32_t crc32_compute(const uint32_t table[256], uint32_t crc, const uint8_t *payload, size_t length) {
	for(size_t i=0; i<length; i++) {
		crc = (crc<<8) ^ table[(crc>>24) ^ payload[i]];
	}
	return crc;
}
