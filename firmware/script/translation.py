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

from util import to_c_array, bitmap_to_bytes

TRANSLATION_TABLE = {
"TR_MSG_GC_VOLUME": ("VOLUME", "suli kalama",
'''
___X___
X__X__X
_X___X_
_______
XXXXXXX
X_____X
_XXXXX_
''',
'''
__X___X_____X
_XX___X_____X
__X___X____XX
_XX___X___X_X
X_X___X_____X
__X___X_XXXXX
__X___X______
__XXXXX_XXXXX
''',
)
}


def get_translation_declarations():
	ret = ""
	ret += "enum tr_msg_id {\n"
	for i in TRANSLATION_TABLE:
		ret += f"\t{i},\n"
	ret += "};\n"

	ret += "extern const char *TR_MSG_EN[];\n"
	ret += "extern const char *TR_MSG_TOK[];\n"
	ret += "extern const uint8_t *TR_MSG_SP[];\n"
	ret += "extern const uint8_t TR_MSG_SP_LEN[];\n"
	ret += "extern const uint8_t *TR_MSG_QSS[];\n"
	ret += "extern const uint8_t TR_MSG_QSS_LEN[];\n"
	return ret;

def get_translation_tables():
	ret = ""
	ret += "const char *TR_MSG_EN[] = {\n"
	for k, v in TRANSLATION_TABLE.items():
		ret += f'\t"{v[0]}", // {k}\n'
	ret += "};\n"
	ret += "const char *TR_MSG_TOK[] = {\n"
	for k, v in TRANSLATION_TABLE.items():
		ret += f'\t"{v[1]}", // {k}\n'
	ret += "};\n"
	ret += "const uint8_t *TR_MSG_SP[] = {\n"
	for k, v in TRANSLATION_TABLE.items():
		ret += f'\t(const uint8_t[]){{{to_c_array(bitmap_to_bytes(v[2]))}}}, // {k}\n'
	ret += "};\n"
	ret += "const uint8_t TR_MSG_SP_LEN[] = {\n"
	for k, v in TRANSLATION_TABLE.items():
		ret += f'\t{len(bitmap_to_bytes(v[2]))}, // {k}\n'
	ret += "};\n"
	ret += "const uint8_t *TR_MSG_QSS[] = {\n"
	for k, v in TRANSLATION_TABLE.items():
		ret += f'\t(const uint8_t[]){{{to_c_array(bitmap_to_bytes(v[3]))}}}, // {k}\n'
	ret += "};\n"
	ret += "const uint8_t TR_MSG_QSS_LEN[] = {\n"
	for k, v in TRANSLATION_TABLE.items():
		ret += f'\t{len(bitmap_to_bytes(v[3]))}, // {k}\n'
	ret += "};\n"
	return ret
