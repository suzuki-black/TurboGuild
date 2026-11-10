/* SPDX-License-Identifier: MIT */
/* save.h — セーブデータ構造体（仕様 §5）。CRC と A/B 採用判定はハード非依存 */
#ifndef CORE_SAVE_H
#define CORE_SAVE_H
#include "../types.h"
#include "../config.h"
#include "memo.h"

#define SAVE_MAGIC    0x54475356UL   /* "TGSV" */
#define SAVE_VERSION  17
#define SAVE_MIN_VERSION 5           /* 読めるいちばん古い版（それより前はカードの形が違う） */
/* ★上位互換の決まり（DECISIONS #78）: 欄は必ず末尾に足し、SAVE_VERSION を 1 上げ、
 *   save.c の save_size[] にその版の大きさを足す。古い版は「その版の大きさ」で検査して読み、
 *   あとから足した欄は 0 で始まる（0 で困る欄は save_upgrade で既定値を入れる）。 */

#define DECK_EMPTY    0xFF
#define SF_STARTED    0x01   /* 名前と最初の 10 連が済んだ */
#define DAY_NEVER     0xFFFFFFFFUL

typedef struct { u16 id; u8 lv; u8 flags; u16 exp; } OwnedCard;
#define STOLEN_MAX    3
typedef struct { u16 npc; u16 card; u8 lv; } StolenRec;
#define PEV_MAX       4
typedef struct { u16 slot; u8 kind; u16 param; } PlayerEv;   /* 6 時間の区切りの番号・PEV_*・中身（0 = 空き） */   /* 盗まれた 1 件。card = 0 は空き（0 で始まるので版上げで困らない） */   /* exp: 次の Lv までに貯めた合成経験値 */

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
    /* 軍資金（リアルマネーの代わりにプレイヤーが持つお財布, DECISIONS #33） */
    u8  course;          /* courses.csv の id。0 = まだ選んでいない */
    u32 funds;           /* 軍資金（円） */
    u32 spent;           /* 累計課金額（円） */
    u16 payday_month;    /* 最後に給料が入った月（ゲーム内 30 日 = 1 か月） */
    /* 終わったイベントの結果（同じイベントに入り直さない・お知らせに出す） */
    u16 last_event;
    u16 last_rank;
    u32 last_event_pt;
    /* イベントの BOX ガチャ（DECISIONS #60）。箱の残りをレアリティ別に持つ */
    u16 box_event;       /* この箱を作ったイベント。開催中のイベントと違えば箱を作り直す */
    u8  box_left[4];     /* N, R, SR, SSR の残り */
    u8  box_no;          /* 何箱目か（1 から） */
    /* ギルドとイベントの型（DECISIONS #63〜） */
    u8  guild;           /* 所属ギルド 1..16（guilds.csv の id）。0 = まだ入っていない */
    u16 raid_event;      /* レイドボスを出したイベント。違えば Lv1 から */
    u8  raid_lv;         /* いまのレイドボスの Lv（倒すたびに上がる） */
    u32 raid_hp;         /* いまのレイドボスの残り HP */
    u8  raid_rescued;    /* このボスで救援依頼を出した */
    u16 pvp_seed;        /* バトルの対戦相手の顔ぶれ（戦うたびに替わる） */
    /* ギルド対抗戦（core/guildwar.c） */
    u8  gvg_war;         /* 参加中の回。0 = なし */
    u16 gvg_start;       /* その初日 */
    u16 gvg_done;        /* 結果を出した戦（ビット 0..13） */
    u16 gvg_wonbits;     /* そのうち勝った戦 */
    u8  gvg_win, gvg_lose;
    u8  gvg_m;           /* gvg_pts を稼いだ戦の番号（0xFF = なし） */
    u32 gvg_pts;         /* その戦で自分が稼いだ点 */
    u8  gvg_cheer;       /* その戦で応援した */
    u8  gvg_last_war;    /* 最後に終わった回（同じ回に入り直さない） */
    u8  gvg_last_rank;
    u8  credits_seen;    /* サービス終了のスタッフロールを流した */
    /* v9: 最後に保存した ROM の言語と版 */
    u8  lang;            /* LANG_JA_CODE / LANG_EN_CODE */
    u8  game_ver[3];     /* GAME_VER_MAJOR, MINOR, PATCH */
    /* v10: ライバル（ranking.h）。NPC 番号 + 1（0 = いない）。[RANK_EVENT] と chaser は rival_event のイベントの間だけ有効 */
    u16 rival[2];        /* [RANK_EVENT] イベントポイント / [RANK_POWER] 総戦力 */
    u8  rival_event;
    u16 chaser;          /* イベントで抜いた元ライバル（抜き返されるか見張る） */
    /* v11: 戦い方とデッキ 3 つ（DECISIONS #87） */
    u8  stance;          /* STANCE_*。0 = 通常 */
    u8  deck_sel;        /* いま使っているデッキ 0..DECK_SLOTS-1（中身は deck[]） */
    u8  deck_slot[DECK_SLOTS][DECK_SIZE];   /* 控えのデッキ。cards[] の添字 + 1（0 = 空き）。[deck_sel] は使わない */
    /* v12: 運営モード（DECISIONS #88） */
    u32 ops_best;        /* いちばん多かった利益（売上 - 広告費, 円） */
    u8  ops_flags;       /* OPS_PLAYED / OPS_SAVED（サービスを続けられたことがある） */
    /* v13: 思い出アルバム（core/memo.h, DECISIONS #89）。出来事ごとに初めての日 + 1（0 = まだ） */
    u16 memo[MEMO_SLOTS];
    /* v14: 盗み・盗まれ（core/steal.h, DECISIONS #90） */
    u32 steal_check;     /* 盗まれたかを最後に数えた g_time（0 = まだ。次に数えるときはここから） */
    u8  steal_seq;       /* 盗みに行った回数（狙える相手の顔ぶれが替わる） */
    u8  stolen_new;      /* まだ知らせていない盗まれた枚数 */
    StolenRec stolen[STOLEN_MAX];   /* 盗まれたカードと盗んだ人（新しい順） */
    /* v15: 自分の最近の出来事（core/pev.h, DECISIONS #91）。伝言板の仲間が反応する */
    PlayerEv pev[PEV_MAX];
    /* v16: 形見分け（DECISIONS #94）。去る仲間からカードを受け取った（ギルドの仲間の並び順のビット） */
    u8  keep_got;
    /* v17: ガラケーの空気（core/keitai.h, DECISIONS #95） */
    u32 bill_base;       /* 前の給料日までの課金の累計（請求の明細は spent - bill_base） */
    u16 maint_paid;      /* お詫びの石を配ったメンテの日 + 1（0 = まだ） */
} SaveData;

u16 save_crc(const SaveData *s);           /* その版の大きさで計算する */
void save_seal(SaveData *s);
u8  save_valid(const SaveData *s);          /* SAVE_MIN_VERSION〜SAVE_VERSION の版なら検査して 1 */
void save_upgrade(SaveData *s);             /* 古い版を今の版へ（足した欄を 0 に） */
void save_new(SaveData *s, u32 real_now);

extern SaveData g_save;   /* 実体は ram.c（MSX）/ テスト側 */
#endif
