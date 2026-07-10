// This file was generated with generate_tables.py. Do not manually modify or it'll be overwritten.
#include <stdint.h>
#include <stddef.h>

extern const uint8_t CRC7_TABLE[256];
extern const uint16_t CRC16_TABLE[256];
extern const uint32_t CRC32_TABLE[256];

extern const uint8_t FONT_ASCII[0x5F][5]; // Font with definition between 0x20 (space) and 0x7E (tilde)
extern const uint8_t ICON_NAVIGATION[];
extern const size_t ICON_NAVIGATION_LEN;
extern const uint8_t ICON_GAMECONF[];
extern const size_t ICON_GAMECONF_LEN;
extern const uint8_t ICON_GLOBALCONF[];
extern const size_t ICON_GLOBALCONF_LEN;
extern const uint8_t ICON_PLAY[];
extern const size_t ICON_PLAY_LEN;
extern const uint8_t ICON_UPDIR[];
extern const size_t ICON_UPDIR_LEN;
extern const uint8_t ICON_ACTION[];
extern const size_t ICON_ACTION_LEN;
extern const uint8_t ICON_REPLAY[];
extern const size_t ICON_REPLAY_LEN;
extern const uint8_t ICON_LANG_SP[];
extern const size_t ICON_LANG_SP_LEN;
extern const uint8_t ICON_LANG_QSS[];
extern const size_t ICON_LANG_QSS_LEN;

enum tr_msg_id {
	TR_MSG_GMC_CONFIG,
	TR_MSG_GMC_QUIRKS,
	TR_MSG_GMC_SPEED_LIMIT,
	TR_MSG_GMC_USE_BOOTROM,
	TR_MSG_GMC_CUSTOM,
	TR_MSG_GMC_TYPEHEX,
	TR_MSG_GMC_TYPEDIGITS,
	TR_MSG_GMC_OVERWRITE,
	TR_MSG_GLBC_VOLUME,
	TR_MSG_GLBC_BACKLIGHT,
	TR_MSG_GLBC_CONTRAST,
	TR_MSG_GLBC_LANG,
	TR_MSG_GLBC_CLR_BOOTROM,
	TR_MSG_GLBC_CONFIRM_CLR_BOOTROM,
	TR_MSG_ADJ_LESS,
	TR_MSG_ADJ_MORE,
	TR_MSG_SAVE,
	TR_MSG_CANCEL,
};
extern const char *TR_MSG_EN[];
extern const char *TR_MSG_TOK[];
extern const uint8_t *TR_MSG_SP[];
extern const uint8_t TR_MSG_SP_LEN[];
extern const uint8_t *TR_MSG_QSS[];
extern const uint8_t TR_MSG_QSS_LEN[];
