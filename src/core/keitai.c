/* SPDX-License-Identifier: MIT */
/* keitai.c — ガラケーと 2011 年の空気（keitai.h, DECISIONS #95）: 伝言板の文体・冒険中の人数・足あと・メンテの計算。
 * 画面は ui/keitai_ui.c（同じバンク 99） */
#include "keitai.h"
#include "board.h"
#include "event.h"
#include "memo.h"
#include "pev.h"
#include "ranking.h"
#include "save.h"
#include "tables.h"
#include "timeline.h"
#include "../config.h"
#include "../data/tables.h"
#include "../far.h"

static u16 mix(u16 a, u16 b)
{
    u16 h = (u16)(a * 0x9E37u ^ b * 0x85EBu ^ 0x3C6Fu);
    h ^= h >> 7;
    h = (u16)(h * 0x2C1Bu);
    h ^= h >> 9;
    return h;
}

/* ---- 文体 ---- */

/* 全角カタカナ（Shift-JIS 8340h〜8396h、837Fh は欠番）の添字 → 半角ｶﾅ。下位 6 ビットが字 - A0h、上 2 ビットが濁点 1 / 半濁点 2。
 * ひらがな（829Fh〜82F1h）も同じ添字の並び。tools で作った表（unicodedata の NFKC と NFD から） */
static const u8 hankana[86] = {
    0x07,0x11,0x08,0x12,0x09,0x13,0x0A,0x14,0x0B,0x15,0x16,0x56,0x17,0x57,0x18,0x58,0x19,0x59,0x1A,0x5A,
    0x1B,0x5B,0x1C,0x5C,0x1D,0x5D,0x1E,0x5E,0x1F,0x5F,0x20,0x60,0x21,0x61,0x0F,0x22,0x62,0x23,0x63,0x24,
    0x64,0x25,0x26,0x27,0x28,0x29,0x2A,0x6A,0xAA,0x2B,0x6B,0xAB,0x2C,0x6C,0xAC,0x2D,0x6D,0xAD,0x2E,0x6E,
    0xAE,0x2F,0x30,0x31,0x32,0x33,0x0C,0x34,0x0D,0x35,0x0E,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3C,0x12,
    0x14,0x06,0x3D,0x53,0x16,0x19
};

/* Shift-JIS の k が、ひらがなかカタカナなら表の添字。違えば 0xFF */
static u8 kana_index(u16 k)
{
    if (k >= 0x829F && k <= 0x82F1) return (u8)(k - 0x829F);
    if (k >= 0x8340 && k <= 0x837E) return (u8)(k - 0x8340);
    if (k >= 0x8380 && k <= 0x8396) return (u8)(k - 0x8341);
    return 0xFF;
}

/* 2 バイトの字 hi lo を w に書く。書いたバイト数（1〜2） */
static u8 put_wide(u8 *w, u8 hi, u8 lo, u8 st)
{
    u16 k = (u16)(((u16)hi << 8) | lo);
    if (st == STYLE_KANA) {
        u8 i = kana_index(k);
        if (k == 0x815B) { w[0] = 0xB0; return 1; }           /* ー */
        if (i != 0xFF) {
            u8 t = hankana[i];
            w[0] = (u8)(0xA0 + (t & 0x3F));
            if (!(t & 0xC0)) return 1;
            w[1] = (t & 0x40) ? 0xDE : 0xDF;                  /* ﾞ ﾟ */
            return 2;
        }
    }
    if (st == STYLE_SMALL && hi == 0x82 && lo >= 0xA0 && lo <= 0xA8 && !(lo & 1)) lo--;   /* あいうえお → ぁぃぅぇぉ */
    w[0] = hi;
    w[1] = lo;
    return 2;
}

FAR(void, style_apply, (char *s, u16 style))
{
    u8 *r = (u8 *)s, *w = (u8 *)s, st = (u8)style, c;
    while ((c = *r) != 0) {
        if ((c >= 0x81 && c <= 0x9F) || c >= 0xE0) {          /* 2 バイトの字 */
            if (!r[1]) break;
            w += put_wide(w, c, r[1], st);
            r += 2;
            continue;
        }
        r++;
#ifdef LANG_EN
        if (st == STYLE_KANA && c >= 'A' && c <= 'Z') c = (u8)(c + 32);   /* 英語版だけ（日本語版の AP・Lv はそのまま） */
#endif
        if (st == STYLE_SMALL && c == 'y' && r[0] == 'o' && r[1] == 'u') { c = 'u'; r += 2; }   /* you → u */
        *w++ = c;
    }
    *w = 0;
}

/* ---- 冒険中の人数 ---- */

FAR(u16, online_now, (u16 active))
{
    static const u8 curve[24] = {      /* 時刻ごとの、残っている人のうちつないでいる割合（/1000 を 2 で割った値） */
        60, 40, 25, 15, 10, 10, 20, 45, 55, 45, 40, 45, 80, 55, 45, 45, 55, 70, 85, 100, 120, 140, 130, 95
    };
    u32 t = g_save.g_time;
    u16 d = (u16)(t / GSEC_PER_DAY), hr = (u16)((t % GSEC_PER_DAY) / 3600);
    u32 n = (u32)active * curve[hr] * 2 / 1000;
    n = n * (90 + mix((u16)(d * 24 + hr), 0x51u) % 21) / 100;  /* ±1 割の揺らぎ */
    if (!n && active) n = 1;
    return (u16)n;
}

/* ---- 足あと ---- */

static u8 add(FootList *l, u16 npc, u16 s, u8 kind, u16 h)
{
    FootRow *r;
    if (npc >= NPC_COUNT) return 0;
    if (kind == FOOT_OTHER || kind == FOOT_RIVAL) {           /* 辞めた人は来ない */
        NpcNow n;
        npc_now(npc, &n);
        if (n.retired) return 0;
    }
    r = &l->r[l->n++];
    r->npc = npc;
    r->day = s >> 2;
    r->hour = (u8)((s & 3) * 6 + h % 6);
    r->kind = kind;
    return 1;
}

FAR(void, foot_list, (FootList *l))
{
    u16 s = (u16)(g_save.g_time / (GSEC_PER_DAY / 4)), look, first = 0, mates[8];
    u8 rd[8], nm = 0, k;
    l->n = 0;
    if (!g_save.guild || g_save.guild > GUILD_COUNT) return;   /* セーブから来た番号は範囲に限る */
    if (g_save.memo[MEMO_GUILD]) first = (u16)((g_save.memo[MEMO_GUILD] - 1) * 4);
    for (look = (u16)((g_save.guild - 1) * 8 + 3); look < NPC_COUNT && nm < 8; look += 128) {
        mates[nm] = look;
        rd[nm++] = (u8)mate_info(look);
    }
    if (!nm) return;
    for (look = 0; look < 64 && l->n < FOOT_MAX && s >= first; look++, s--) {
        u16 h = mix(s, (u16)(g_save.guild * 77u + 0x1D3u)), d = s >> 2;
        u8 j, done = 0;
        for (j = 0; j < PEV_MAX && !done; j++) {               /* 盗んだ人は、盗んだ時間帯に */
            const PlayerEv *e = &g_save.pev[j];
            if (e->kind == PEV_STOLEN && e->slot == s) done = add(l, e->param, s, FOOT_THIEF, h);
        }
        for (k = 0; k < nm && !done; k++) {                    /* 去る仲間は最後の日の朝に */
            if (rd[k] != MATE_STAYS && rd[k] * 4 + 1 == s && !(mate_fate(mates[k]) & MF_GHOST))
                done = add(l, mates[k], s, FOOT_BYE, h);
        }
        if (done || (h & 3)) { if (!s) break; continue; }      /* 1 日に 1 人ほど */
        k = (u8)((h >> 2) % 20);
        if (k < 8) {                                           /* 仲間 */
            u8 m = (u8)((h >> 7) % nm);
            if (rd[m] == MATE_STAYS || rd[m] > d) add(l, mates[m], s, FOOT_MATE, h >> 4);
        } else if (k < 11) {                                   /* ライバル */
            if (g_save.rival[RANK_POWER]) add(l, (u16)(g_save.rival[RANK_POWER] - 1), s, FOOT_RIVAL, h >> 4);
        } else {                                               /* 知らない人 */
            u16 npc = (u16)(mix(h, 0x2Bu) % NPC_COUNT);
            if (!BOARD_IS_MATE(npc) || ((npc >> 3) % GUILD_COUNT) != g_save.guild - 1) add(l, npc, s, FOOT_OTHER, h >> 4);
        }
        if (!s) break;
    }
}

/* ---- メンテ ---- */

#define MAINT_FROM   7
#define MAINT_UNTIL  240
#define MAINT_EVERY  14

/* d 日目のメンテ（無ければ kind = MAINT_NONE） */
static void maint_of(u16 d, Maint *m)
{
    EventRec e;
    TimelineRec r;
    u8 i;
    u16 h = mix(d, 0x77u);
    m->kind = MAINT_NONE;
    m->day = d;
    if (d < MAINT_FROM || d >= MAINT_UNTIL) return;
    if (h & 0x100) {                                           /* イベントの初日は、半分ほどで緊急メンテ */
        for (i = 1; i <= EVENT_COUNT; i++) {
            tbl_event(i, &e);
            if (e.start_day == d) { m->kind = MAINT_EMERG; break; }
        }
    }
    if (!m->kind && d % MAINT_EVERY == 6) m->kind = MAINT_REG;
    if (!m->kind) return;                                      /* 表を読むのはメンテのある日だけ（マイページに入るたびに呼ぶ） */
    for (i = 0; i < TIMELINE_COUNT; i++) {                     /* 対抗戦の 7 日間はしない */
        tbl_timeline(i, &r);
        if (r.kind == TL_GUILDWAR && d >= r.day && d < r.day + GVG_DAYS) { m->kind = MAINT_NONE; return; }
    }
    m->start = (u32)d * GSEC_PER_DAY + (m->kind == MAINT_EMERG ? 10UL : 13UL) * 3600;
    m->end0 = m->start + 2UL * 3600;
    m->extended = (u8)((h & 3) == 0);
    m->end = m->end0 + (m->extended ? 3600UL : 0);
}

FAR(u8, maint_now, (Maint *m))
{
    u32 t = g_save.g_time;
    maint_of((u16)(t / GSEC_PER_DAY), m);
    return (u8)(m->kind && t >= m->start && t < m->end);
}

FAR(u16, maint_comp, (void))
{
    Maint m;
    u16 d = (u16)(g_save.g_time / GSEC_PER_DAY), k;
    for (k = 0; k <= MAINT_EVERY && k <= d; k++) {
        maint_of((u16)(d - k), &m);
        if (!m.kind || g_save.g_time < m.end) continue;
        if (m.kind == MAINT_REG && !m.extended) return 0;      /* いちばん新しいメンテにお詫びは無い */
        if (g_save.maint_paid > m.day) return 0;               /* もう配った */
        if (g_save.memo[MEMO_GUILD] && m.day + 1 < g_save.memo[MEMO_GUILD]) return 0;   /* 始める前のメンテ */
        g_save.maint_paid = (u16)(m.day + 1);
        g_save.gems += MAINT_GEMS;
        return MAINT_GEMS;
    }
    return 0;
}

