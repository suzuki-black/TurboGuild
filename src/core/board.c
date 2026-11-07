/* SPDX-License-Identifier: MIT */
/* board.c — ギルド伝言板（board.h）。1 日を 4 つ（夜中・朝・昼・夜）に分け、それぞれで高々 1 件書き込む。
 * 書き込む確率は「残っている仲間の数 / 8」なので、仲間が辞めていくほど板は静かになる。
 * 話題は 6 割がその日の状況（イベント・対抗戦・確率アップ・新しいレア度・サービス終了）、
 * 残りは課金コースと時間帯の世間話。引退の 3 日前からは半分が弱音になる。 */
#include "board.h"
#include "event.h"
#include "save.h"
#include "tables.h"
#include "timeline.h"
#include "../data/text_ids.h"
#include "../far.h"

#define SALT_MATE     0x5A17u
#define SALT_MATE_R   0x33C5u
#define SALT_POST(k)  ((u16)(0x4B1Du + (k) * 0x6C8Fu))
#define FADE_DAYS     3
#define TOPIC_PCT     60
#define NEW_DAYS      7           /* 新しいレア度の話題が続く日数 */
#define MATES_MAX     8
#define TOPICS_MAX    6

typedef struct { u16 base; u8 count; u16 param; } Topic;

typedef struct {
    u16 m[MATES_MAX];
    u8  nm;
    u16 day;                      /* t を求めた日（0xFFFF = まだ） */
    Topic t[TOPICS_MAX];
    u8  nt;
} Work;

u16 mate_retire_day(u16 i)
{
    u16 h = npc_hash(i, SALT_MATE_R);
    if ((u8)h < 77) return BOARD_NEVER;                  /* 3 割は最後まで残る */
    return (u16)(20 + (h >> 8) % 230);
}

u8 npc_tone(u16 i)
{
    return (u8)(npc_hash(i, SALT_MATE) & 3);
}

FAR(u16, mate_info, (u16 i))
{
    u16 rd = mate_retire_day(i);                         /* 引退の日は 20..249 なので 8 ビットに入る */
    return (u16)(((u16)npc_tone(i) << 8) | (rd == BOARD_NEVER ? MATE_STAYS : rd));
}

static void add(Work *w, u16 base, u8 count, u16 param)
{
    Topic *t;
    if (w->nt >= TOPICS_MAX) return;
    t = &w->t[w->nt++];
    t->base = base; t->count = count; t->param = param;
}

/* その日の話題を年表とイベントの表から集める */
static void topics(Work *w, u16 d)
{
    TimelineRec r;
    EventRec e;
    u8 i;
    w->day = d;
    w->nt = 0;
    for (i = 0; i < TIMELINE_COUNT; i++) {
        tbl_timeline(i, &r);
        if (r.day > d) break;                            /* 日付順 */
        if (r.kind == TL_ANNOUNCE_END) { w->nt = 0; add(w, T_BD_END0, 4, 0); return; }   /* 終わりの話でもちきり */
        if (r.kind == TL_NEW_RARITY && d < r.day + NEW_DAYS) add(w, T_BD_NEW0, 4, (u16)(T_RAR_0 + r.param));
        if (r.kind == TL_GUILDWAR && d < r.day + GVG_DAYS) add(w, T_BD_GVG0, 3, 0);
        if (r.kind == TL_GACHA_RATEUP && d < r.day + RATEUP_DAYS) add(w, T_BD_RATE0, 3, 0);
    }
    for (i = 1; i <= EVENT_COUNT; i++) {
        tbl_event(i, &e);
        if (d < e.start_day || d >= e.start_day + e.duration) continue;
        if (e.kind == EV_RAID) add(w, T_BD_RAID0, 4, e.name_text);
        else if (e.kind == EV_PVP) add(w, T_BD_PVP0, 3, e.name_text);
        else add(w, T_BD_HUNT0, 4, e.name_text);
    }
}

/* 時間帯 s の書き込み。無ければ 0 */
static u8 post_at(Work *w, u16 s, BoardPost *p)
{
    u16 d = s >> 2, act[MATES_MAX], h, h2, rd;
    u16 x = (u16)(s + d);                            /* 文の選び方: 隣の時間帯で必ず替わり、日ごとにもずれる */
    u8 k, na = 0, trait;
    for (k = 0; k < w->nm; k++) {
        rd = mate_retire_day(w->m[k]);
        if (rd == d && (s & 3) == 2) {                   /* 辞める日の昼に、脱退の知らせだけが流れる */
            p->npc = w->m[k]; p->kind = BP_LEAVE; p->tone = 0; p->text = p->alt = 0; p->param = 0; p->day = d;
            return 1;
        }
        if (rd > d || (rd == d && (s & 3) < 2)) act[na++] = w->m[k];
    }
    h = npc_hash(s, SALT_POST(0));
    if (!na || (h & 7) >= na) return 0;
    p->npc = act[(h >> 3) % na];
    p->kind = BP_POST;
    p->day = d;
    p->param = 0;
    trait = (u8)npc_hash(p->npc, SALT_MATE);
    p->tone = trait & 3;
    h2 = npc_hash(s, SALT_POST(1));
    rd = mate_retire_day(p->npc);
    if (rd != BOARD_NEVER && rd - d <= FADE_DAYS && (h2 & 1)) {
        p->text = (u16)(T_BD_FADE0 + (x & 3));
        p->alt = (u16)(T_BD_FADE0 + ((x + 1) & 3));
        return 1;
    }
    if (w->day != d) topics(w, d);
    if (w->nt && (h2 >> 4) % 100 < TOPIC_PCT) {
        const Topic *t = &w->t[(h2 >> 11) % w->nt];
        p->text = (u16)(t->base + x % t->count);
        p->alt = (u16)(t->base + (x + 1) % t->count);
        p->param = t->param;
        return 1;
    }
    {
        u16 base = (h2 & 0x0004) ? (u16)(T_BD_PAY0 + ((trait >> 2) & 3) * 2) : (u16)(T_BD_HI0 + (s & 3) * 2);
        p->text = (u16)(base + (x & 1));
        p->alt = (u16)(base + ((x + 1) & 1));            /* 対になっているもう一方 */
    }
    return 1;
}

/* 古い方の older と同じ文なら代わりの文にする */
static void fix(BoardPost *cur, const BoardPost *older)
{
    if (cur->kind == BP_POST && older->kind == BP_POST && cur->text == older->text) cur->text = cur->alt;
}

static void out(BoardList *l, const BoardPost *p, u8 *skip)
{
    if (*skip) { (*skip)--; return; }
    if (l->n < l->want && l->n < BOARD_MAX) l->post[l->n++] = *p;
}

FAR(void, board_list, (BoardList *l))
{
    Work w;
    BoardPost q[2], nx;
    u16 s = (u16)(g_save.g_time / BOARD_SLOT_SECS), i, look;
    u8 skip = l->skip, have = 0;
    l->n = 0;
    if (!g_save.guild) return;
    w.nm = 0;
    w.day = 0xFFFF;
    for (i = (u16)((g_save.guild - 1) * 8 + 3); i < NPC_COUNT && w.nm < MATES_MAX; i += 128) w.m[w.nm++] = i;
    /* 続けて同じ文にしない。q0 を出すときは、q1 を「その古い方（いま来た nx）と比べて直したもの」と比べる。
     * 古い方だけを見るので、新しい書き込みが来ても過去の文は変わらない */
    for (look = 0; look < BOARD_LOOKBACK && l->n < l->want && l->n < BOARD_MAX; look++, s--) {
        if (post_at(&w, s, &nx)) {
            if (have == 2) {
                BoardPost q1f = q[1];
                fix(&q1f, &nx);
                fix(&q[0], &q1f);
                out(l, &q[0], &skip);
                q[0] = q[1];
                q[1] = nx;
            } else {
                q[have++] = nx;
            }
        }
        if (!s) break;
    }
    if (have == 2) { fix(&q[0], &q[1]); out(l, &q[0], &skip); out(l, &q[1], &skip); }
    else if (have == 1) out(l, &q[0], &skip);
}
