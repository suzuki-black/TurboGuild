/* SPDX-License-Identifier: MIT */
/* save.c — CRC16-CCITT で封をし、読み戻したデータの真偽を判定する。
 * crc フィールドは 0 とみなして計算する（封をする前後で同じ値になるように）。
 * 古い版のセーブも読む（上位互換, save.h の決まり）。 */
#include <stddef.h>
#include "save.h"
#include "../far.h"

/* 版ごとの大きさ（添字 = 版 - SAVE_MIN_VERSION）＝ 次の版で足した最初の欄の位置。
 * MSX（SDCC は詰めて並べる）では v5 1306・v6 1313・v7 1341・v8 1342 B（各コミットの save.h から実測）。
 * offsetof で書くので、詰め方の違う PC の単体テストでも同じ意味になる */
static const u16 save_size[] = {
    offsetof(SaveData, box_event),     /* v5: Phase2 */
    offsetof(SaveData, guild),         /* v6: BOX ガチャ */
    offsetof(SaveData, credits_seen),  /* v7: ギルド・レイド・バトル */
    offsetof(SaveData, lang),          /* v8: スタッフロール */
    sizeof(SaveData),                  /* v9: 言語と版 */
};

static u16 size_of(u16 version)
{
    if (version < SAVE_MIN_VERSION || version > SAVE_VERSION) return 0;
    return save_size[version - SAVE_MIN_VERSION];
}

static u16 crc_byte(u16 crc, u8 b)
{
    u8 i;
    crc ^= (u16)b << 8;
    for (i = 0; i < 8; i++) {
        crc = (crc & 0x8000) ? (u16)((crc << 1) ^ 0x1021) : (u16)(crc << 1);
    }
    return crc;
}

u16 save_crc(const SaveData *s)
{
    const u8 *p = (const u8 *)s;
    const u8 *crc_at = (const u8 *)&s->crc;
    u16 crc = 0xFFFF;
    u16 i, n = size_of(s->version);
    for (i = 0; i < n; i++) {
        u8 b = (p + i == crc_at || p + i == crc_at + 1) ? 0 : p[i];
        crc = crc_byte(crc, b);
    }
    return crc;
}

void save_seal(SaveData *s)
{
    s->magic = SAVE_MAGIC;
    s->version = SAVE_VERSION;
    s->lang = GAME_LANG;
    s->game_ver[0] = GAME_VER_MAJOR;
    s->game_ver[1] = GAME_VER_MINOR;
    s->game_ver[2] = GAME_VER_PATCH;
    s->crc = save_crc(s);
}

u8 save_valid(const SaveData *s)
{
    /* CRC は改ざんを防がない。ディスクから来た枚数は、配列の添字に使う前にここで止める */
    return (u8)(s->magic == SAVE_MAGIC && size_of(s->version) &&
                s->card_count <= CARD_MAX && s->crc == save_crc(s));
}

FAR(void, save_upgrade, (SaveData *s))
{
    u8 *p = (u8 *)s;
    u16 i = size_of(s->version);
    if (!i || s->version == SAVE_VERSION) return;
    for (; i < sizeof(SaveData); i++) p[i] = 0;    /* あとから足した欄は 0 から */
    s->version = SAVE_VERSION;                      /* 次に保存するときに今の版で封をする */
}

void save_new(SaveData *s, u32 real_now)
{
    u8 *p = (u8 *)s;
    u16 i;
    for (i = 0; i < sizeof(SaveData); i++) p[i] = 0;
    s->real_time_last = real_now;
    s->rng_sim = 0x2011C0DEUL;
    s->rng_gacha = real_now | 1;
    s->level = 1;
    s->ap = AP_MAX;
    s->gold = START_GOLD;
    s->gems = START_GEMS;
    s->login_day = 1;
    s->last_login_gday = DAY_NEVER;
    s->last_free_gacha_gday = DAY_NEVER;
    for (i = 0; i < DECK_SIZE; i++) s->deck[i] = DECK_EMPTY;
}
