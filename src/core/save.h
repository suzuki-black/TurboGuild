/* save.h — セーブデータ構造体（仕様 §5）。CRC と A/B 採用判定はハード非依存 */
#ifndef CORE_SAVE_H
#define CORE_SAVE_H
#include "../types.h"
#include "../config.h"

#define SAVE_MAGIC    0x54475356UL   /* "TGSV" */
#define SAVE_VERSION  2

#define DECK_EMPTY    0xFF
#define SF_STARTED    0x01   /* 名前と最初の 10 連が済んだ */
#define DAY_NEVER     0xFFFFFFFFUL

typedef struct { u16 id; u8 lv; u8 flags; } OwnedCard;

typedef struct {
    u32 magic;  u16 version; u16 crc;
    u32 real_time_last;
    u32 g_time;
    u32 rng_sim, rng_gacha;
    char name[12]; u8 level; u16 exp; u32 gold; u32 gems;
    u8 ap; u8 potions; u8 gacha_tickets; u8 sr_tickets;
    u32 ap_gtime;        /* AP をこの g_time まで回復済み */
    u8 flags;            /* SF_* */
    u16 stage_cleared;
    u16 pity_count;
    u8 login_day; u32 last_login_gday; u32 last_free_gacha_gday;
    u16 card_count;
    OwnedCard cards[CARD_MAX];
    u8 deck[DECK_SIZE];  /* cards[] の添字。空きは DECK_EMPTY */
    u16 event_id; u32 event_pt; u8 event_rewards_claimed;
    u16 timeline_cursor;
    u16 save_count;      /* 保存回数。動作確認と A/B 交互の目安 */
} SaveData;

u16 save_crc(const SaveData *s);
void save_seal(SaveData *s);
u8  save_valid(const SaveData *s);
void save_new(SaveData *s, u32 real_now);

extern SaveData g_save;   /* 実体は ram.c（MSX）/ テスト側 */
#endif
