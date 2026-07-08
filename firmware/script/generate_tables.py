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

from crc import get_crc_declarations, get_crc_tables
from font import get_font_and_icon_declarations, get_font_and_icon_tables
from translation import get_translation_declarations, get_translation_tables
import os
import sys

def get_header():
	ret = ""
	ret += "// This file was generated with generate_tables.py. Do not manually modify or it'll be overwritten.\n"
	ret += "#include <stdint.h>\n"
	ret += "#include <stddef.h>\n"
	return ret

if __name__ == "__main__":
	if len(sys.argv) < 2:
		print(f"Usage: {sys.argv[0]} <output-dir> #generates generated.h and generated.c")
		exit(1)

	base_dir = sys.argv[1]

	with open(os.path.join(base_dir, "generated.h"), "w") as f:
		f.write('\n'.join([get_header(), get_crc_declarations(), get_font_and_icon_declarations(), get_translation_declarations()]))

	with open(os.path.join(base_dir, "generated.c"), "w") as f:
		f.write('\n'.join([get_header(), get_crc_tables(), get_font_and_icon_tables(), get_translation_tables()]))
