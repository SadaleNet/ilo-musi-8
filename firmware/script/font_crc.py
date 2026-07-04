#!/usr/bin/python3

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

BITMAP = {
' ':
'''
_____
_____
_____
_____
_____
''',
'!':
'''
__X__
__X__
__X__
_____
__X__
''',
'"':
'''
_X_X_
_X_X_
_____
_____
_____
''',
'#':
'''
_X_X_
XXXXX
_X_X_
XXXXX
_X_X_
''',
'$':
'''
X___X
_X_X_
__X__
XXXXX
__X__
''',
'%':
'''
X___X
___X_
__X__
_X___
X___X
''',
'&':
'''
_XX__
X__X_
_XX__
X__X_
XXX_X
''',
'\'':
'''
X____
_X___
__X__
___X_
____X
''',
'(':
'''
__X__
_X___
_X___
_X___
__X__
''',
')':
'''
__X__
___X_
___X_
___X_
__X__
''',
'*':
'''
X_X_X
_XXX_
_XXX_
X_X_X
_____
''',
'+':
'''
_____
__X__
_XXX_
__X__
_____
''',
',':
'''
_____
_____
_XX__
__X__
_X___
''',
'-':
'''
_____
_____
_XXX_
_____
_____
''',
'.':
'''
_____
_____
_____
_XX__
_XX__
''',
'/':
'''
____X
___X_
__X__
_X___
X____
''',
'0':
'''
_XXX_
X___X
X_X_X
X___X
_XXX_
''',
'1':
'''
__X__
_XX__
X_X__
__X__
XXXXX
''',
'2':
'''
_XXX_
X___X
__XX_
_X___
XXXXX
''',
'3':
'''
_XXX_
X___X
__XX_
X___X
_XXX_
''',
'4':
'''
__XX_
_X_X_
X__X_
XXXXX
___X_
''',
'5':
'''
XXXXX
X____
XXXX_
____X
XXXX_
''',
'6':
'''
__X__
_X___
XXXX_
X___X
_XXX_
''',
'7':
'''
XXXXX
____X
___X_
__X__
_X___
''',
'8':
'''
_XXX_
X___X
_XXX_
X___X
_XXX_
''',
'9':
'''
_XXX_
X___X
_XXXX
___X_
__X__
''',
':':
'''
_XX__
_XX__
_____
_XX__
_XX__
''',
';':
'''
_____
__X__
_____
__X__
_X___
''',
'<':
'''
___X_
__X__
_X___
__X__
___X_
''',
'=':
'''
_____
_XXX_
_____
_XXX_
_____
''',
'>':
'''
_X___
__X__
___X_
__X__
_X___
''',
'?':
'''
_XXX_
X___X
__XX_
_____
__X__
''',
'@':
'''
XXXX_
____X
_XX_X
X_X_X
_XXX_
''',
'A':
'''
__X__
_X_X_
X___X
XXXXX
X___X
''',
'B':
'''
XXXX_
X___X
XXXX_
X___X
XXXX_
''',
'C':
'''
_XXX_
X___X
X____
X___X
_XXX_
''',
'D':
'''
XXXX_
X___X
X___X
X___X
XXXX_
''',
'E':
'''
XXXXX
X____
XXXX_
X____
XXXXX
''',
'F':
'''
XXXXX
X____
XXXX_
X____
X____
''',
'G':
'''
_XXX_
X____
X_XXX
X___X
_XXXX
''',
'H':
'''
X___X
X___X
XXXXX
X___X
X___X
''',
'I':
'''
_XXX_
__X__
__X__
__X__
_XXX_
''',
'J':
'''
__XXX
___X_
___X_
X__X_
_XX__
''',
'K':
'''
X__X_
X_X__
XX___
X_X__
X__X_
''',
'L':
'''
X____
X____
X____
X____
XXXXX
''',
'M':
'''
X___X
XX_XX
X_X_X
X_X_X
X___X
''',
'N':
'''
X___X
XX__X
X_X_X
X__XX
X___X
''',
'O':
'''
_XXX_
X___X
X___X
X___X
_XXX_
''',
'P':
'''
XXXX_
X___X
XXXX_
X____
X____
''',
'Q':
'''
_XXX_
X___X
X_X_X
X__X_
_XX_X
''',
'R':
'''
XXXX_
X___X
XXXX_
X__X_
X___X
''',
'S':
'''
_XXXX
X____
_XXX_
____X
XXXX_
''',
'T':
'''
XXXXX
__X__
__X__
__X__
__X__
''',
'U':
'''
X___X
X___X
X___X
X___X
_XXX_
''',
'V':
'''
X___X
X___X
X___X
_X_X_
__X__
''',
'W':
'''
X___X
X___X
X_X_X
X_X_X
_X_X_
''',
'X':
'''
X___X
_X_X_
__X__
_X_X_
X___X
''',
'Y':
'''
X___X
_X_X_
__X__
__X__
__X__
''',
'Z':
'''
XXXXX
___X_
__X__
_X___
XXXXX
''',
'[':
'''
_XXX_
_X___
_X___
_X___
_XXX_
''',
'\\':
'''
X____
_X___
__X__
___X_
____X
''',
']':
'''
_XXX_
___X_
___X_
___X_
_XXX_
''',
'^':
'''
__X__
_X_X_
_____
_____
_____
''',
'_':
'''
_____
_____
_____
_____
XXXXX
''',
'`':
'''
_X___
__X__
_____
_____
_____
''',
'a':
'''
_XXX_
____X
_XXXX
X___X
_XXXX
''',
'b':
'''
X____
X____
XXXX_
X___X
XXXX_
''',
'c':
'''
_____
_XXX_
X____
X____
_XXX_
''',
'd':
'''
____X
____X
_XXXX
X___X
_XXXX
''',
'e':
'''
_____
_XXX_
XXXXX
X____
_XXX_
''',
'f':
'''
__XX_
_X__X
XXX__
_X___
_X___
''',
'g':
'''
_____
_XXX_
XXXXX
____X
_XXX_
''',
'h':
'''
X____
X____
XXXX_
X___X
X___X
''',
'i':
'''
__X__
_____
_XX__
__X__
_XXX_
''',
'j':
'''
___X_
_____
___X_
X__X_
_XX__
''',
'k':
'''
X____
X____
X_XX_
XX___
X_XX_
''',
'l':
'''
_XX__
__X__
__X__
__X__
_XXX_
''',
'm':
'''
_____
_____
_X_X_
X_X_X
X___X
''',
'n':
'''
_____
_____
X_XX_
XX__X
X___X
''',
'o':
'''
_____
_XXX_
X___X
X___X
_XXX_
''',
'p':
'''
_____
_XXX_
X___X
XXXX_
X____
''',
'q':
'''
_____
_XXXX
X___X
_XXXX
____X
''',
'r':
'''
_____
X_XX_
XX__X
X____
X____
''',
's':
'''
_____
_XXX_
XX___
__XX_
XXX__
''',
't':
'''
_X___
XXX__
_X___
_X__X
__XX_
''',
'u':
'''
_____
_____
X___X
X___X
_XXX_
''',
'v':
'''
_____
_____
X___X
_X_X_
__X__
''',
'w':
'''
_____
_____
X___X
X_X_X
_X_X_
''',
'x':
'''
_____
_____
XX_XX
__X__
XX_XX
''',
'y':
'''
_____
X___X
_XXXX
____X
_XXX_
''',
'z':
'''
_____
XXXX_
__X__
_X___
XXXX_
''',
'{':
'''
__XX_
_X___
XX___
_X___
__XX_
''',
'|':
'''
__X__
__X__
__X__
__X__
__X__
''',
'}':
'''
_XX__
___X_
___XX
___X_
_XX__
''',
'~':
'''
_____
_X___
X_X_X
___X_
_____
''',
}


BITMAP_ICONS = {
'NAVIGATION':
'''
__XXX__
__X_X__
XXXXXXX
X_XXX_X
XXXXXXX
__X_X__
__XXX__
''',
'GAMECONF':
'''
____XX_
XXXXXXX
____XX_
_______
_XX____
XXXXXXX
_XX____
''',
'GLOBALCONF':
'''
___X__X
__XX_X_
XXXX___
XXXX_XX
XXXX___
__XX_X_
___X__X
''',
'PLAY':
'''
__X____
__XX___
__XXX__
__XXXX_
__XXX__
__XX___
__X____
''',
'UPDIR':
'''
__X____
_XXX___
X_X_X__
__X____
__X____
__X____
__XXXXX
''',
'ACTION':
'''
__XXX__
_XXXXX_
XXXX_XX
XXXXX_X
XXXXXXX
_XXXXX_
__XXX__
''',
'REPLAY':
'''
__XXXX_
______X
__X___X
_X___X_
XXXXX__
_X_____
__X____
''',
}

def bitmap_to_bytes(bitmap):
	lines = [i.replace('\n', '') for i in bitmap.split('\n')[1:][:-1]] # remove the first and the final \n
	width = len(lines[0])
	ret = [0 for i in range(width)]
	assert(len(lines) < 8) # This function doesn't support image taller than 8px
	# Column major. The top bit is LSB.
	for c in range(width):
		for r in range(len(lines)):
			if lines[r][c] == 'X':
				ret[c] |= (1 << r)
	return bytes(ret)


def to_c_array(b, digits=2, linesplit=16):
	lines = []
	for i in range((len(b)+linesplit-1)//linesplit):
		lines.append(' '.join([f"0x{j:0{digits}X}," for j in b[i*linesplit:(i+1)*linesplit]]))
	return '\n'.join(lines)

print("// This file was generated with font_crc.py. Do not manually modify or it'll be overwritten.")
print("#include <stdint.h>")
print("#include <stddef.h>")
print("const uint8_t FONT_ASCII[0x5F][5] = {")
for i in range(0x20, 0x7F):
	c = chr(i)
	print(f"\t{{{to_c_array(bitmap_to_bytes(BITMAP[c]))} }}, // {c.replace('\\', '(backslash)')}")

print("};")
print("")

for k, v in BITMAP_ICONS.items():
	bitmap = bitmap_to_bytes(v)
	print(f"const uint8_t ICON_{k}[] = {{{to_c_array(bitmap)} }};")
	print(f"const size_t ICON_{k}_LENGTH = {len(bitmap)};")

print("")
print("")

# Below starts the CRC table generation

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

CRC7_TABLE = [compute_crc_single(0x89, 7, i) for i in range(256)]
CRC16_TABLE = [compute_crc_single(0x1021, 16, i) for i in range(256)]
CRC32_TABLE = [compute_crc_single(0x04C11DB7, 32, i) for i in range(256)]

print(f"const uint8_t CRC7_TABLE[] = {{ // Polynomial 0x89\n{to_c_array(CRC7_TABLE)} }};")
print(f"const uint16_t CRC16_TABLE[] = {{ // Polynomial 0x1021\n{to_c_array(CRC16_TABLE, 4)} }};")
print(f"const uint32_t CRC32_TABLE[] = {{ // Polynomial 0x04C11DB7\n{to_c_array(CRC32_TABLE, 8, 8)} }};")
