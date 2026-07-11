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
"TR_MSG_ERR_TITLE": ("CARD ERROR #", "pakala nanpa #",
'''
XXXXXXX__X___X_
X___X_X_XXXXXXX
X__X__X__X___X_
X_XXX_X__X___X_
X__X__X__X___X_
X_X___X_XXXXXXX
XXXXXXX__X___X_
''',
'''
XXXXX_X_X_X___XXXXX_X____
______X_X_X___X_X_X_X____
XXXXX_X_X_X_X_X_X_X_X____
X_X_X_X_X_X__XX_X_X_X____
X_X_X_X_X_X_X_X___X_X____
X_X_X_X_X_X___X___X_XXXXX
X___X__XXX____X___X_X___X
X___X___X_____X___X_XXXXX
''',
),
"TR_MSG_ERR_NOT_READY": ("NO CARD", "lipu sona li lon ala",
'''
XXXXXXX_X__X__X_X____________X_____X
X_____X__X_X_X___X____________X___X_
X_____X___________X____________X_X__
X_____X_XXXXXXX____X____X_______X___
X_____X_X_____X___X____________X_X__
X_____X_X_____X__X____________X___X_
XXXXXXX_XXXXXXX_X____XXXXXXX_X_____X
''',
'''
__X___X___XXXXX_____X__
__X___X_X__________X_X_
_XX_X_X__XXXXXX__XX___X
X_X_X_X___X_____X_X____
_XX_X_X_X_X_______X____
__XXXXX__XX______XX____
__X___X___X_____X_X____
__XXXXX__XX______XX____
''',
),
"TR_MSG_ERR_NO_FILESYSTEM": ("FILESYSTEM ERROR", "nasin sona li ike",
'''
___X____X__X__X_X___________
___X_____X_X_X___X__________
___X______________X___XXXXX_
__XXX___XXXXXXX____X_X_____X
_X_X_X__X_____X___X__X_____X
___X____X_____X__X__________
___X____XXXXXXX_X___________
''',
'''
__XXXXX___X___X__XX_____XXXXX_XXXXX_X_X_X
__X_X_X___X___X___X_____X_X_X_______X_X_X
__X_X_X___X___X_X_X_____X_X_X_XXXXX_X_X_X
_XX_X_X_XXX_X_X__XX____XX_X_X_X_X_X_X_X_X
X_X___X___X_X_X_X_X___X_X_X___X_X_X_X_X_X
__X___X___X_X_X_X_X_X___X_X___X_X_X_X_X_X
__X___X___XX_XX__XXX___XX_X__XX___X__XXX_
__X___X___X___X___X_____X_X___X___X___X__
''',
),
"TR_MSG_ERR_NO_FILESYSTEM2": ("REQUIRES FAT16/FAT32", "o nasin FAT16/FAT32",
'''
___X____XXXXXXX____X____XXXXX___X___XXXXX__X_____X______X_XXXXX___X___XXXXX__XXX___XXX_
___X____X__X__X____X____X______X_X____X___XX____X_______X_X______X_X____X___X___X_X___X
___X____XXXXXXX____X____X_____X___X___X__X_X___X_______X__X_____X___X___X_______X_____X
___________X______XXX___XXX___XXXXX___X____X___XXXX___X___XXX___X___X___X____XXX_____X_
___X_____XXXXX___X_X_X__X_____X___X___X____X___X___X__X___X_____XXXXX___X_______X___X__
__X_X__XX_____X____X____X_____X___X___X____X___X___X_X____X_____X___X___X___X___X__X___
___X____X_____X____X____X_____X___X___X__XXXXX__XXX__X____X_____X___X___X____XXX__XXXXX
''',
'''
__XXXXX__X___X___X_X___XXXXX___X___XXXXX__X_____X______X_XXXXX___X___XXXXX__XXX___XXX_
________XX___X__XX_X___X______X_X____X___XX____X_______X_X______X_X____X___X___X_X___X
__XXXXX__X___X___X_X_X_X_____X___X___X__X_X___X_______X__X_____X___X___X_______X_____X
_XX___X__X___X__XX_X_X_X_____X___X___X____X___XXXX____X__X_____X___X___X____XXX_____X_
X_X___X__X___X_X_X_X_X_XXX___XXXXX___X____X___X___X__X___XXX___XXXXX___X_______X___X__
__X___X__X___X___XXXXX_X_____X___X___X____X___X___X__X___X_____X___X___X___X___X__X___
_XX___X__X___X_________X_____X___X___X____X___X___X_X____X_____X___X___X___X___X_X____
__X___X__XXXXX___XXXXX_X_____X___X___X__XXXXX__XXX__X____X_____X___X___X____XXX__XXXXX
''',
),
"TR_MSG_ERR_INI_PARSE": ("INVALID CONFIG INI", "lipu INI li pakala",
'''
XXXXXXX__X_XXXXX_X___X_XXXXX_X___X_____XXXXXXX
X_____X_X____X___XX__X___X____X___X____X___X_X
X_____X_X____X___X_X_X___X____X____X___X__X__X
X_____X_X____X___X_X_X___X____X_____X__X_XXX_X
X_____X_X____X___X_X_X___X____X____X___X__X__X
X_____X_X____X___X__XX___X____X___X____X_X___X
XXXXXXX__X_XXXXX_X___X_XXXXX_X___X_____XXXXXXX
''',
'''
XXXXX_X____X_XXXXX___XXXXX___XXXXX__XXXXX_X_X_X
__X___XX___X___X_____________X___X________X_X_X
__X___X_X__X___X_____XXXXX___X___X__XXXXX_X_X_X
__X___X_X__X___X___XXX___X__XX___X__X_X_X_X_X_X
__X___X__X_X___X_____X___X_X_X___X__X_X_X_X_X_X
__X___X__X_X___X___XXX___X___X___X__X_X_X_X_X_X
__X___X___XX___X_____X___X___X___X_XX___X__XXX_
XXXXX_X____X_XXXXX__XX___X___X___X__X___X___X__
''',
),
"TR_MSG_ERR_VOLUME_FULL": ("VOLUME FULL", "jo ala e ma sona",
'''
XXXXXXX_X__X__X_X______XXX___X_____X__________XXX___X__X__X
X_____X__X_X_X___X____X___X___X___X__X__X____X_X_X___X_X_X_
X_____X___________X____XXX_____X_X____X__X__X__X__X________
X_____X_XXXXXXX____X__X_________X______X__X_XXXXXXX_XXXXXXX
X_____X_X_____X___X__X__XXX____X_X____X__X__X__X__X_X_____X
X_____X_X_____X__X___X_____X__X___X__X__X____X_X_X__X_____X
XXXXXXX_XXXXXXX_X_____XXXXX__X_____X__________XXX___XXXXXXX
''',
'''
XXXXX_____X_____X___X__XX__
_________X_X____X___X___X__
XXXXX__XX___X__XX_X_X_X_X__
____X_X_X_____X_X_X_X__XX__
___XX___X______XX_X_X_X_X__
__X_X__XX_______XXXXX___X__
____X_X_X_______X___X__X_X_
____X__XX_______XXXXX_X___X
''',
),
"TR_MSG_ERR_PATH_LENGTH": ("PATH TOO LONG", "nimi li suli li ike",
'''
________X____X_____X_X___________
_________X___X_____X__X__________
_XXXXX____X___X___X____X___XXXXX_
X_____X____X__X___X_____X_X_____X
_XXXXX____X____X_X_____X__X_____X
_________X_____X_X____X__________
________X_______X____X___________
''',
'''
__XXXXX___X___X___X_______XXXXX_XXXXX
__X_X_X___X___X___X_______X__________
__X_X_X___X___X___X_____X_X_____XXXXX
_XX_X_X_XXX_X_X__XX______XX_________X
X_X___X___X_X_X_X_X_____X_X________XX
__X___X___X_X_X___XXXXX___X_______X_X
__X___X___XX_XX___X___X__XX_________X
__X___X___X___X___XXXXX___X________XX
''',
),
"TR_MSG_ERR_FIRMWARE_VERIFICATION": ("FW VERIFY ERROR", "sona li kama ala sin",
'''
X__X__X_X________X___X_____X_________
_X_X_X___X______X_X___X___X_____X____
__________X_____X_X____X_X______X____
XXXXXXX____X___X___X____X_______X____
X_____X___X____X___X___X_X___________
X_____X__X___X_X___X__X___X__XXX_XXX_
XXXXXXX_X_____X__XX__X_____X_________
''',
'''
__X_____X_____X____XXXXX
_XXX___XX____XX____X___X
X_X_X___X_____X____XXXXX
__X_____X___XXX__X_X____
_XX___XXX_____X___XX____
X_X_____X___XXX__X_X____
__X_____X_____X____X____
__X___XXXXX_XXXXX__X____
''',
),
"TR_MSG_ERR_BOOTROM_VERIFICATION": ("BOOTROM CRC ERROR", "nanpa CRC li ike",
'''
_X___X___XXX__XXXX___XXX__X________X___X__X_____X__X____________
XXXXXXX_X___X_X___X_X___X_X_______X_X_X_X_X_____X___X___________
_X___X__X_____X___X_X_____X________X___X__X_____X____X____XXXXX_
_X___X__X_____XXXX__X_____X________X___X__X_____X_____X__X_____X
_X___X__X_____X_X___X_____X________X___X__XXXXXXX____X___X_____X
XXXXXXX_X___X_X__X__X___X_X________X___X__X_____X___X___________
_X___X___XXX__X___X__XXX__XXXXXXX___XXX___XXXXXXX__X____________
''',
'''
__X___X_____X____X______X___X__XXX__XXXX___XXX___XXXXX_X_X_X
_XX___X____XXX__XX______X___X_X___X_X___X_X___X________X_X_X
__X___X___X_X_X__X______X___X_X_____X___X_X______XXXXX_X_X_X
X_X_X_X_X_X_X____X____XXX___X_X_____XXXX__X______X_X_X_X_X_X
_XX_X_X__XX_X____X______X___X_X_____X_X___X______X_X_X_X_X_X
X_X_X_X_X_X_X____X_X____XXXXX_X_____X__X__X___X__X_X_X_X_X_X
___XXX____X_X____XX_X___X___X_X___X_X___X_X___X_XX___X__XXX_
____X_____X_X____X___X__XXXXX__XXX__X___X__XXX___X___X___X__
''',
),
"TR_MSG_ERR_LOW_BATTERY": ("LOW BATTERY", "jo ala e wawa",
'''
XXXXXXX____X____X______XXX___X_____X________X_______X
X__X__X____X_____X____X___X___X___X__X__X___X_______X
X__X__X____X______X____XXX_____X_X____X__X__X_______X
XXXXXXX_X__X__X____X__X_________X______X__X_X_______X
___X_____X_X_X____X__X__XXX____X_X____X__X___X__X__X_
___X______XXX____X___X_____X__X___X__X__X____X_X_X_X_
___X_______X____X_____XXXXX__X_____X_________X__X__X_
''',
'''
__X___X___XXXXX_X____
__X___X___X___X_X____
_XX___X___X___X_X____
X_X___X_XXX___X_X____
_XX___X___X___X_X____
__XXXXX___X___X_XXXXX
__X___X___X___X______
__XXXXX___X___X_XXXXX
''',
),
"TR_MSG_ERR_PROCEED": ("PRESS <X> TO PROCEED", "o pilin e nena <X>",
'''
___X____XX_XX___________XXX____X_X___X_X_
___X___X__X__X_X__X____X___X__X__X___X__X
___X___X_____X__X__X__X_____X_X___X_X___X
_______X_____X___X__X_X_____X_X____X____X
___X____X___X___X__X__X_____X_X___X_X___X
__X_X____X_X___X__X___X_____X_X__X___X__X
___X______X___________X_____X__X_X___X_X_
''',
'''
__X___X___X_XXXXX___X____XXXXX
_XXX__X___X________XXX________
X_X_X__X_X__XXXXX_X_X_X__XXXXX
__X_____X_____X_____X___XX_X_X
X_X____X_X__X_X___X_X__X_X_X_X
_XX____X_X___XX____XX____X_X_X
__X___X___X_X_X___X_X___XX_X__
__X___X___X___X_____X__X_X_X__
''',
),
"TR_MSG_MENU_EMPTY": ("[EMPTY]", "[ala li lon]",
'''
_X_X_____X_X____________X_
X___X___X___X____________X
X____X_X_____X___________X
X_____X_______X____X_____X
X____X_X_____X___________X
X___X___X___X____________X
_X_X_____X_X____XXXXXXX_X_
''',
'''
_____X_X_____
____XX_X_____
_____X_X_____
____XX_X_X___
___X_X_X_X___
X____X_X_X__X
X_____XXX___X
XXX____X__XXX
''',
),
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
"TR_MSG_GMC_TYPEHEX": ("TYPE HEX", "o pana e nanpa",
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
"TR_MSG_GMC_TYPEDIGITS": ("TYPE DIGITS", "o pana e nanpa",
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
"TR_MSG_GLBC_CONFIRM_CLR_BOOTROM": ("CONFIRM CLEAR", "o pali!",
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
),
"TR_MSG_PGP_CONTROLS": ("CONTROLS", "nena musi",
'''
__XXX____X___X_
_X___X__X_X_X_X
X_____X__X___X_
X_____X__X___X_
X_____X__X___X_
X_____X__X___X_
X_____X___XXX__
''',
'''
__X_X___XXXXX____X__
_XX_X___________X_X_
__X_X___XXXXX__X___X
_XX_X_X___X____X___X
X_X_X_X_X_X__XXX___X
__X_X_X__XX____X___X
___XXX__X_X____X___X
____X_____X____X___X
''',
),
"TR_MSG_GP_HOLDTOQUIT": ("HOLD TO QUIT...", "sina wile weka...",
'''
__X_____X___X______X___X_____X
_X_____X_____X____X_X___X___X_
X______X_____X____X_X_________
X_XXX__X_____X___X___X________
XX___X_X_____X___X___X________
X____X_X__X__X_X_X___X__X___X_
_XXXX___XX_XX___X__XX__X_____X
''',
'''
__X_____X____X_X_____X___X
_XXX___XX__X_X_X____XX___X
X_X_X___X___XX_X_____X___X
__X___XXX__X_X_X___X_X_X_X
X_X_____X____X_X____XX_X_X
_XX___XXX____XXXXX_X_X_X_X
__X_____X_____________XXX_
__X___XXXXX__XXXXX_____X__
''',
),
"TR_MSG_GAMEOVER": ("GAME OVER", "musi li pini",
'''
_X___X__X____XXXXXXX
X_X_X_X__X______X___
_X___X____X_____X___
_X___X_____X____X___
_X___X____X_____X___
_X___X___X______X___
__XXX___X____XXXXXXX
''',
'''
__X______XX_X_X____
_XX_______X_X_X____
__X_____XXX_X_X____
XXX_______X_X_X____
__X_____XXX_X_X____
__X_X___XXXXX_XXXXX
__XX_X_____________
__X___X_XXXXX_XXXXX
''',
),
"TR_MSG_GAMEOVER_PROCEED": ("PRESS <X> TO QUIT", "nena <X> la sina weka",
'''
__XXX___X___X_X_____X________X___X_____X
_X___X__X___X__X___X________X_X___X___X_
X_____X__X_X____X_X_________X_X_________
X_____X___X_____X_X_XXX____X___X________
X_____X__X_X____X_XX___X___X___X________
X_____X_X___X__X__X____X_X_X___X__X___X_
X_____X_X___X_X____XXXX___X__XX__X_____X
''',
'''
__X___X___X_XXXXX
_XXX__X___X______
X_X_X__X_X__XXXXX
__X_____X_____X__
X_X____X_X___XX__
_XX____X_X__X_X__
__X___X___X__XX__
__X___X___X___X__
''',
),
"TR_MSG_FWU_OK": ("UPDATE COMPLETED", "sona li kama sin",
'''
X__X__X_X________X___________
_X_X_X___X______X_X_____X____
__________X_____X_X_____X____
XXXXXXX____X___X___X____X____
X_____X___X____X___X_________
X_____X__X___X_X___X_XXX_XXX_
XXXXXXX_X_____X__XX__________
''',
'''
__X_____X_____X___X_____X__
_XXX___XX____XX___X____XXX_
X_X_X___X_____X___X___X_X_X
__X_____X____XX___X___X_X_X
_XX___XXX___X_X___X__XX_X_X
X_X_____X_____X___X_X_X_X__
__X_____X_____X___X___X_X__
__X___XXXXX___XXXXX___X_X__
''',
),
"TR_MSG_FWU_PROCEED": ("PRESS <X> TO PROCEED", "o pilin e nena <X>",
'''
___X____XX_XX___________XXX____X_X___X_X_
___X___X__X__X_X__X____X___X__X__X___X__X
___X___X_____X__X__X__X_____X_X___X_X___X
_______X_____X___X__X_X_____X_X____X____X
___X____X___X___X__X__X_____X_X___X_X___X
__X_X____X_X___X__X___X_____X_X__X___X__X
___X______X___________X_____X__X_X___X_X_
''',
'''
__X___X___X_XXXXX___X____XXXXX
_XXX__X___X________XXX________
X_X_X__X_X__XXXXX_X_X_X__XXXXX
__X_____X_____X_____X___XX_X_X
X_X____X_X__X_X___X_X__X_X_X_X
_XX____X_X___XX____XX____X_X_X
__X___X___X_X_X___X_X___XX_X__
__X___X___X___X_____X__X_X_X__
''',
),
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
