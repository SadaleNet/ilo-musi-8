// This file was generated with generate_tables.py. Do not manually modify or it'll be overwritten.
#include <stdint.h>
#include <stddef.h>

extern const uint8_t CRC7_TABLE[256];
extern const uint16_t CRC16_TABLE[256];
extern const uint32_t CRC32_TABLE[256];

extern const uint8_t FONT_ASCII[0x5F][5]; // Font with definition between 0x20 (space) and 0x7E (tilde)
extern const uint8_t ICON_NAVIGATION[];
extern const size_t ICON_NAVIGATION_LENGTH;
extern const uint8_t ICON_GAMECONF[];
extern const size_t ICON_GAMECONF_LENGTH;
extern const uint8_t ICON_GLOBALCONF[];
extern const size_t ICON_GLOBALCONF_LENGTH;
extern const uint8_t ICON_PLAY[];
extern const size_t ICON_PLAY_LENGTH;
extern const uint8_t ICON_UPDIR[];
extern const size_t ICON_UPDIR_LENGTH;
extern const uint8_t ICON_ACTION[];
extern const size_t ICON_ACTION_LENGTH;
extern const uint8_t ICON_REPLAY[];
extern const size_t ICON_REPLAY_LENGTH;

enum tr_msg_id {
	TR_MSG_GC_VOLUME,
};
extern const char *TR_MSG_EN[];
extern const char *TR_MSG_TOK[];
extern const uint8_t *TR_MSG_SP[];
extern const uint8_t TR_MSG_SP_LEN[];
extern const uint8_t *TR_MSG_QSS[];
extern const uint8_t TR_MSG_QSS_LEN[];
