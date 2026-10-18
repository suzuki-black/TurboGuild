/* save.c — CRC16-CCITT で封をし、読み戻したデータの真偽を判定する。
 * crc フィールドは 0 とみなして計算する（封をする前後で同じ値になるように）。 */
#include "save.h"

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
    u16 i;
    for (i = 0; i < sizeof(SaveData); i++) {
        u8 b = (p + i == crc_at || p + i == crc_at + 1) ? 0 : p[i];
        crc = crc_byte(crc, b);
    }
    return crc;
}

void save_seal(SaveData *s)
{
    s->magic = SAVE_MAGIC;
    s->version = SAVE_VERSION;
    s->crc = save_crc(s);
}

u8 save_valid(const SaveData *s)
{
    return (u8)(s->magic == SAVE_MAGIC && s->version == SAVE_VERSION &&
                s->crc == save_crc(s));
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
