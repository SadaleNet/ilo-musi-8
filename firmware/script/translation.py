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
"TR_MSG_GMC_CONFIG": ("CONFIG", "ante pi",
'''
_X___X__X______
__X_X___X______
___X____X______
________X______
___X____X______
__X_X___X______
_X___X__XXXXXXX
''',
'''
__XXXXX___XXXXX
__________X___X
__XXXXX___X___X
XXX___X__XX___X
__X___X_X_X___X
XXX___X___X___X
__X___X___X___X
_XX___X___X___X
''',
),
"TR_MSG_GMC_QUIRKS": ("QUIRKS", "nasin musi",
'''
___X_____X___X_
___X____X_X_X_X
___X_____X___X_
__XXX____X___X_
_X_X_X___X___X_
___X_____X___X_
___X______XXX__
''',
'''
__X_________X__
__X________X_X_
__X_______X___X
_XX_______X___X
X_X_____XXX___X
__X___X___X___X
___X_X____X___X
____X_____X___X
''',
),
"TR_MSG_GMC_SPEED_LIMIT": ("SPEED LIMIT", "tenpo musi",
'''
__XXX____X___X_
_X___XX_X_X_X_X
X__X__X__X___X_
X__XX_X__X___X_
X_____X__X___X_
_X___X___X___X_
__XXX_____XXX__
''',
'''
__XXXXX___XXXXX
__________X_X_X
_XXXXXX___X_X_X
X_X_X_X__XX_X_X
__X_X_X_X_X___X
_XX_X_X__XX___X
X_X_X_____X___X
_XX_X_____X___X
''',
),
"TR_MSG_GMC_USE_BOOTROM": ("USE AS BOOTROM", "o kama musi open",
'''
___X_______X____X___X__X_____X
___X______X_X__X_X_X_X_X_____X
___X______X_X___X___X__X_____X
_________X___X__X___X__X_____X
___X_____X___X__X___X__XXXXXXX
__X_X__X_X___X__X___X__X_____X
___X____X__XX____XXX___XXXXXXX
''',
'''
__XXXXX___X___X___X___X_____X____X______X___X
__X___X__XX___X__XX___X____XXX__XX______X___X
__XXXXX___X___X___X___X___X_X_X__X______X___X
__X___X___X___X_X_X_X_X_X_X_X____X____XXX___X
__X___X__XX___X__XX_X_X__XX_X____X______X___X
XXX___X_X_X___X_X_X_X_X_X_X_X____X_X____XXXXX
__X___X___X___X____XXX____X_X____XX_X___X___X
__X___X___XXXXX_____X_____X_X____X___X__XXXXX
''',
),
"TR_MSG_GMC_CUSTOM": ("CUSTOM", "nanpa",
'''
_X___X____X____
XXXXXXX__X_____
_X___X___X_____
_X___X___XXXX__
_X___X___X___X_
XXXXXXX__X___X_
_X___X____XXX__
''',
'''
XXXXX___XXXXX__X___X
________X___X_XX___X
XXXXX___X___X__X___X
X___X__XX___X__X___X
X___X_X_X___X__X___X
X___X___X___X__XXXXX
X___X___X___X_______
X___X___X___X__XXXXX
''',
),
"TR_MSG_GMC_TYPEHEX": ("TYPE HEX", "o pana e nanpa!",
'''
__X___X__X__X_________X___X_
__X____X_X_X__X__X___XXXXXXX
__X____________X__X___X___X_
________XXXX____X__X__X___X_
__X____X____X__X__X___X___X_
_X_X__XX____X_X__X___XXXXXXX
__X____X____X_________X___X_
''',
'''
XXXXX___X___X_XXXXX
X______XX___X______
X_______X___X_XXXXX
X______XX___X_X___X
X_____X_X_X_X_X___X
X______XX_X_X_X___X
X_______X_X_X_X___X
X_______XXXXX_X___X
''',
),
"TR_MSG_GMC_TYPEDIGITS": ("TYPE DIGITS", "o pana e nanpa!",
'''
__X___X__X__X_________X___X_
__X____X_X_X__X__X___XXXXXXX
__X____________X__X___X___X_
________XXXX____X__X__X___X_
__X____X____X__X__X___X___X_
_X_X__XX____X_X__X___XXXXXXX
__X____X____X_________X___X_
''',
'''
XXXXX___X___X_XXXXX
X______XX___X______
X_______X___X_XXXXX
X______XX___X_X___X
X_____X_X_X_X_X___X
X______XX_X_X_X___X
X_______X_X_X_X___X
X_______XXXXX_X___X
''',
),
"TR_MSG_GMC_OVERWRITE": ("OVERWRITE BOOTROM", "o kama!",
'''
___X_______X__
___X______X_X_
___X______X_X_
_________X___X
___X_____X___X
__X_X__X_X___X
___X____X__XX_
''',
'''
XXXXX___XXXXX___X___X_____X____X______X___X
________X_X_X__XX___X____XXX__XX______X___X
XXXXX___X_X_X___X___X___X_X_X__X______X___X
__X_X_X_X_X_X_X_X_X_X_X_X_X____X____XXX___X
___XX__XX___X__XX_X_X__XX_X____X______X___X
__X_X_X_X___X_X_X_X_X_X_X_X____X_X____XXXXX
____X___X___X____XXX____X_X____XX_X___X___X
___XX___X___X_____X_____X_X____X___X__XXXXX
''',
),

"TR_MSG_GLBC_VOLUME": ("VOLUME", "suli kalama",
'''
X_____X_X__X__X
X_____X__X_X_X_
_X___X_________
_X___X__XXXXXXX
__X_X___X_____X
__X_X____X___X_
___X______XXX__
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
),
"TR_MSG_GLBC_BACKLIGHT": ("BACKLIGHT", "suli suno",
'''
X_____X____X___
X_____X___XXX__
_X___X___X___X_
_X___X__XX___XX
__X_X____X___X_
__X_X_____XXX__
___X_______X___
''',
'''
____X___XXXXX
____X___X_X_X
___XX___X_X_X
__X_X__XX_X_X
____X_X_X___X
XXXXX__XX___X
________X___X
XXXXX___X___X
''',
),
"TR_MSG_GLBC_CONTRAST": ("CONTRAST", "wawa pimeja",
'''
X_______X____X___
X_______X___X_X__
X_______X__X___X_
X_______X__XX_XX_
_X__X__X__X__X__X
_X_X_X_X__X_X_X_X
_X__X__X__XXXXXXX
''',
'''
XXXXX___XXXXX___XXXXX
____X___X___X___X_X_X
____X___XXXXX___X_X_X
__X_X_X_X_X____XX_X_X
___XX__XX_X___X_X___X
__X_X_X_X_X____XX___X
____X___X_X_____X___X
____X___X_X_____X___X
''',
),
"TR_MSG_GLBC_LANG": ("LANGUAGE", "toki",
'''
___X___
X_____X
__XXX__
_X___X_
_X___X_
_X___X_
__XXX__
''',
'''
__X_X___X___X_
_XX_X__XX___X_
__X_X___X___X_
__X_X_XXX___X_
__X_X___X___X_
XXXXX___XXXXX_
______________
XXXXX___XXXXX_
''',
),
"TR_MSG_GLBC_CLR_BOOTROM": ("CLEAR BOOTROM", "o weka e musi open",
'''
___X___X_____X__________X___X__X_____X
___X____X___X___X__X___X_X_X_X_X_____X
___X_____________X__X___X___X__X_____X
__________________X__X__X___X__X_____X
___X_____________X__X___X___X__XXXXXXX
__X_X___X___X___X__X____X___X__X_____X
___X___X_____X___________XXX___XXXXXXX
''',
'''
__XXXXX_XXXXX___X___X_____X____X______X___X
_______________XX___X____XXX__XX______X___X
__XXXXX_XXXXX___X___X___X_X_X__X______X___X
_XX___X___X_X_X_X_X_X_X_X_X____X____XXX___X
X_X___X____XX__XX_X_X__XX_X____X______X___X
__X___X___X_X_X_X_X_X_X_X_X____X_X____XXXXX
_XX___X_____X____XXX____X_X____XX_X___X___X
__X___X____XX_____X_____X_X____X___X__XXXXX
''',
),
"TR_MSG_GLBC_CONFIRM_CLR_BOOTROM": ("CONFIRM CLEAR", "o pali!", # TODO
'''
___X_______XX__
___X______X__X_
___X_______XX__
_______________
___X______XXXX_
__X_X____X____X
___X____XX____X
''',
'''
__XXXXX_XXXXX
_____________
__XXXXX_XXXXX
_XX___X___X_X
X_X___X____XX
__X___X___X_X
_XX___X_____X
__X___X____XX
''',
),
"TR_MSG_ADJ_LESS": ("LESS", "lili",
'''
____X__________
___X_X_________
___X_X_________
__X___X________
__X___X__X___X_
X_X___X___X_X__
_X__XX_____X___
''',
'''
__X___XXXXX
_XX___X___X
__X___X___X
X_X___X___X
_XX___X___X
X_X___X___X
__X___X___X
XXXXX_X___X
''',
),
"TR_MSG_ADJ_MORE": ("MORE", "suli",
'''
____X___X_____X
___X_X__X_____X
___X_X___X___X_
__X___X__X___X_
__X___X___X_X__
X_X___X___X_X__
_X__XX_____X___
''',
'''
__XXXXX_XXXXX
__X_____X___X
__X_____X___X
X_X_____X___X
_XX_____X___X
X_X_____X___X
__X_____X___X
__X_____X___X
''',
),
"TR_MSG_SAVE": ("SAVE", "pana",
'''
X__X__X
_X_X_X_
_______
__XXXX_
_X____X
XX____X
_X____X
''',
'''
XXXXX_XXXXX
___________
XXXXX_XXXXX
__X_X___X_X
__X_X_XXX_X
__X_X___X_X
_XX_X__XX_X
__X_X___X_X
''',
),
"TR_MSG_CANCEL": ("CANCEL", "pana ala",
'''
X__X__X_X_____X
_X_X_X___X___X_
__________X_X__
__XXXX_____X___
_X____X___X_X__
XX____X__X___X_
_X____X_X_____X
''',
'''
XXXXX___X___X
_______XX___X
XXXXX___X___X
__X_X__XX___X
___XX_X_X___X
__X_X__XX___X
____X___X___X
___XX___XXXXX
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
