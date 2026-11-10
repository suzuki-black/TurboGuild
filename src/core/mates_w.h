/* SPDX-License-Identifier: MIT */
/* mates_w.h — ギルドの仲間の式（引退の日・去り方・ギルドマスター）。伝言板（board.c, バンク 98）と
 * メンバー一覧・メール（mates.c, バンク 100）が同じ式を使う。どちらのバンクにも写しができる（static。inline にすると SDCC がすべて展開して大きくなった）
 * （バンク 98 が埋まったので分けた, DECISIONS #96）。中から呼ぶだけで、別のバンクへは渡さない */
#ifndef CORE_MATES_W_H
#define CORE_MATES_W_H
#include "board.h"
#include "memo.h"
#include "save.h"
#include "../data/tables.h"

#ifdef __SDCC
#define MW_FN static
#else
#define MW_FN static __attribute__((unused))   /* 片方のファイルでしか使わない関数がある */
#endif

#define SALT_MATE     0x5A17u
#define SALT_MATE_R   0x33C5u
#define SALT_FATE     0x2D71u
#define SALT_FATE2    0x61B3u
#define FADE_TEXTS    4           /* 去り方 1 つあたりの弱音の数（機種変の人には無い） */
#define PURGE_AFTER   14          /* 何も言わずに来なくなった人を、ギルマスが整理するまでの日数（+0〜7） */
#define NO_K          0xFF
#define SALT_STYLE    0x4E95u
#define SALT_BDAY     0x3A6Bu
#define SALT_NEWBIE   0x7C21u
#define SALT_OFF      0x15E9u
#define NEWBIE_EVERY  30          /* 体験入隊はギルドごとにおよそ 30 日に 1 回 */
#define OFF_EVERY     45          /* オフ会の誘いはおよそ 45 日に 1 回（4 人以上いるとき） */
#define OFF_AFTER     6           /* 誘いの何日後に「楽しかった」（その前の日の午後と夜は板が静か） */
#define SALT_POST(k)  ((u16)(0x4B1Du + (k) * 0x6C8Fu))
#define FADE_DAYS     3
#define TOPIC_PCT     60
#define NEW_DAYS      7           /* 新しいレア度の話題が続く日数 */
#define MATES_MAX     8
#define TOPICS_MAX    6
#define REACT_SLOTS   3           /* 自分の出来事に反応する時間帯の数 */
#define RE_TEXTS      3           /* 反応 1 種類の文の数 */

typedef struct { u16 base; u8 count; u16 param; } Topic;

typedef struct {
    u16 m[MATES_MAX];
    u16 rd[MATES_MAX];            /* 引退の日（BOARD_NEVER = 最後まで残る） */
    u16 f[MATES_MAX];             /* 去り方（mate_fate） */
    u16 rb[MATES_MAX];            /* 戻ってくる日（BOARD_NEVER = 戻らない） */
    u16 pg[MATES_MAX];            /* 何も言わずに来なくなった人を整理する日（それ以外は BOARD_NEVER） */
    u16 joined;                   /* 自分がギルドに入った日（0xFFFF = 分からない） */
    u16 bd[MATES_MAX];            /* 誕生日（ゲーム内の日。0 日目 = 2011 年 7 月 1 日。270 以上なら期間外） */
    u8  nm;
    u16 day;                      /* t を求めた日（0xFFFF = まだ） */
    Topic t[TOPICS_MAX];
    u8  nt;
} Work;

/* ranking.c の npc_hash の写し（同じ式。伝言板は前と同じ書き込みになる） */
MW_FN u16 hash(u16 a, u16 b)
{
    u16 x = (u16)(a ^ (b << 5) ^ (b >> 3) ^ 0x9E37u);
    x ^= x << 7;
    x ^= x >> 9;
    x += x << 3;
    x ^= x << 8;
    x += b;
    x ^= x >> 7;
    x += x << 4;
    x ^= x >> 8;
    return x;
}

/* ranking.c の mate_retire_day の写し */
MW_FN u16 retire_day(u16 i)
{
    u16 h = hash(i, SALT_MATE_R);
    if ((u8)h < 77) return BOARD_NEVER;
    return (u16)(20 + (h >> 8) % 230);
}


/* 時間帯 s に、仲間 k はもう板にいない（引退の日の昼から） */
MW_FN u8 gone_at(const Work *w, u8 k, u16 s)
{
    u16 rd = w->rd[k], d = s >> 2;
    return (u8)(rd != BOARD_NEVER && (d > rd || (d == rd && (s & 3) >= 2)));
}

/* 時間帯 s に、去った仲間 k が戻ってきている */
MW_FN u8 back_at(const Work *w, u8 k, u16 s)
{
    u16 d = s >> 2;
    return (u8)(w->rb[k] != BOARD_NEVER && d >= w->rb[k] && d < w->rb[k] + BOARD_BACK_DAYS);
}

/* 時間帯 s のギルドマスター（仲間の並びで最初の、まだ役を持っている人。いなければ NO_K = 自分）。
 * 黙って消えた人は、整理される日の昼まで役を持ったまま */
MW_FN u8 gm_of(const Work *w, u16 s)
{
    u8 k;
    for (k = 0; k < w->nm; k++) {
        u16 pg = w->pg[k], d = s >> 2;
        if (pg == BOARD_NEVER) { if (!gone_at(w, k, s)) return k; }
        else if (d < pg || (d == pg && (s & 3) < 2)) return k;
    }
    return NO_K;
}

/* 自分のギルドの仲間の式を並べる（g_save.guild は 1..GUILD_COUNT であること） */
MW_FN void setup(Work *w)
{
    u16 i;
    w->nm = 0;
    w->day = 0xFFFF;
    w->joined = g_save.memo[MEMO_GUILD] ? (u16)(g_save.memo[MEMO_GUILD] - 1) : 0;
    for (i = (u16)((g_save.guild - 1) * 8 + 3); i < NPC_COUNT && w->nm < MATES_MAX; i += 128) {
        u16 rd = retire_day(i), f = mate_fate(i);   /* 去り方の式は mates.c（バンク 100）にだけ置く */
        w->rd[w->nm] = rd;
        w->f[w->nm] = f;
        w->rb[w->nm] = MF_BACK(f) ? (u16)(rd + MF_BACK(f)) : BOARD_NEVER;
        w->pg[w->nm] = (rd != BOARD_NEVER && (f & MF_GHOST)) ? (u16)(rd + PURGE_AFTER + (hash(i, SALT_FATE2) >> 13)) : BOARD_NEVER;
        w->bd[w->nm] = (u16)(hash(i, SALT_BDAY) % 365);
        w->m[w->nm++] = i;
    }
}

#endif
