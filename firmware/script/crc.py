# Copyright 2026 Wong Cho Ching <https://sadale.net>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

from util import to_c_array

def get_mask_by_bitwidth(bitwidth):
	ret = 0
	for i in range(bitwidth):
		ret |= 1<<i
	return ret

def compute_crc_single(polynomial, bitwidth, b):
	mask = get_mask_by_bitwidth(bitwidth)
	polynomial &= mask

	if bitwidth < 8:
		ret = b >> (8-bitwidth)
		for i in range(8-bitwidth):
			p = polynomial if (ret & (1<<(bitwidth-1))) else 0
			ret <<= 1
			ret |= 1 if (b & (1<<(8-bitwidth-1-i))) else 0
			ret ^= p
			ret &= mask
	else:
		ret = b

	for i in range(bitwidth):
		p = polynomial if (ret & (1<<(bitwidth-1))) else 0
		ret <<= 1
		ret ^= p
		ret &= mask
	return ret

# For verifying if the table's correct
def compute_crc_by_table(table, bitwidth, crc, payload):
	mask = get_mask_by_bitwidth(bitwidth)
	for b in payload:
		if bitwidth < 8:
			crc = table[(crc << (8-bitwidth)) ^ b]
		else:
			crc = (crc << 8) ^ table[(crc >> (bitwidth-8)) ^ b]
		crc &= mask
	return crc

def get_crc_declarations():
	ret = ""
	ret += f"extern const uint8_t CRC7_TABLE[256];\n"
	ret += f"extern const uint16_t CRC16_TABLE[256];\n"
	ret += f"extern const uint32_t CRC32_TABLE[256];\n"
	return ret

def get_crc_tables():
	CRC7_TABLE = [compute_crc_single(0x89, 7, i) for i in range(256)]
	CRC16_TABLE = [compute_crc_single(0x1021, 16, i) for i in range(256)]
	CRC32_TABLE = [compute_crc_single(0x04C11DB7, 32, i) for i in range(256)]

	ret = ""
	ret += f"const uint8_t CRC7_TABLE[] = {{ // Polynomial 0x89\n{to_c_array(CRC7_TABLE)} }};\n"
	ret += f"const uint16_t CRC16_TABLE[] = {{ // Polynomial 0x1021\n{to_c_array(CRC16_TABLE, 4)} }};\n"
	ret += f"const uint32_t CRC32_TABLE[] = {{ // Polynomial 0x04C11DB7\n{to_c_array(CRC32_TABLE, 8, 8)} }};\n"
	return ret
