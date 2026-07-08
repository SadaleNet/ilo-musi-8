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

def to_c_array(b, digits=2, linesplit=16):
	lines = []
	for i in range((len(b)+linesplit-1)//linesplit):
		lines.append(' '.join([f"0x{j:0{digits}X}," for j in b[i*linesplit:(i+1)*linesplit]]))
	return '\n'.join(lines)

def bitmap_to_bytes(bitmap):
	lines = [i.replace('\n', '') for i in bitmap.split('\n')[1:][:-1]] # remove the first and the final \n
	width = len(lines[0])
	ret = [0 for i in range(width)]
	assert(len(lines) <= 8) # This function doesn't support image taller than 8px
	# Column major. The top bit is LSB.
	for c in range(width):
		for r in range(len(lines)):
			if lines[r][c] == 'X':
				ret[c] |= (1 << r)
	return bytes(ret)
