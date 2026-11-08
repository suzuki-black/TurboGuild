/* SPDX-License-Identifier: MIT */
/* board.c — ギルド伝言板（board.h, DECISIONS #85・#91）。1 日を 4 つ（夜中・朝・昼・夜）に分け、それぞれで高々 1 件書き込む。
 * 書き込む確率は「残っている仲間の数 / 8」なので、仲間が辞めていくほど板は静かになる。
 * 書き込みは上から順に決める:
 *   1. 引退の 3 日前からは半分が弱音。
 *   2. 自分（プレイヤー）の最近の出来事（g_save.pev）から 3 回は、仲間が反応する（出来事のあった時間帯は必ず。すぐ見られるように）。
 *   3. 4 回に 1 回は、1 つ前の時間帯の書き込みへの返事（「＞○○さん それな」。弱音には励まし、脱退には見送り）。
 *   4. 6 割がその日の状況（イベント・対抗戦・確率アップ・新しいレア度・サービス終了）。
 *   5. 残りは暮らしの話: 給料日・休日・小さな出来事・去った仲間を懐かしむ・課金コースの話・時間帯の世間話。
 * 文の # には人の名前（ref）、$ には自分の名前、% には話題（param）が入る（ui/news_ui.c）。
 * バンク 14 が埋まったので、このファイルだけバンク 98 に置き、ranking.c のハッシュと仲間の式の写しを持つ。 */
#include "board.h"
#include "event.h"
#include "pev.h"
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
#define REACT_SLOTS   3           /* 自分の出来事に反応する時間帯の数 */
#define RE_TEXTS      3           /* 反応 1 種類の文の数 */

typedef struct { u16 base; u8 count; u16 param; } Topic;

typedef struct {
    u16 m[MATES_MAX];
    u16 rd[MATES_MAX];            /* 引退の日（BOARD_NEVER = 最後まで残る） */
    u8  nm;
    u16 day;                      /* t を求めた日（0xFFFF = まだ） */
    Topic t[TOPICS_MAX];
    u8  nt;
} Work;

/* ranking.c の npc_hash の写し（同じ式。伝言板は前と同じ書き込みになる） */
static u16 hash(u16 a, u16 b)
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
static u16 retire_day(u16 i)
{
    u16 h = hash(i, SALT_MATE_R);
    if ((u8)h < 77) return BOARD_NEVER;
    return (u16)(20 + (h >> 8) % 230);
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
        if (r.kind == TL_ANNOUNCE_END) { w->nt = 0; add(w, T_BD_END0, 8, 0); return; }   /* 終わりの話でもちきり */
        if (r.kind == TL_NEW_RARITY && d < r.day + NEW_DAYS) add(w, T_BD_NEW0, 8, (u16)(T_RAR_0 + r.param));
        if (r.kind == TL_GUILDWAR && d < r.day + GVG_DAYS) add(w, T_BD_GVG0, 6, 0);
        if (r.kind == TL_GACHA_RATEUP && d < r.day + RATEUP_DAYS) add(w, T_BD_RATE0, 6, 0);
    }
    for (i = 1; i <= EVENT_COUNT; i++) {
        tbl_event(i, &e);
        if (d < e.start_day || d >= e.start_day + e.duration) continue;
        if (e.kind == EV_RAID) add(w, T_BD_RAID0, 8, e.name_text);
        else if (e.kind == EV_PVP) add(w, T_BD_PVP0, 6, e.name_text);
        else add(w, T_BD_HUNT0, 8, e.name_text);
    }
}

static void say(BoardPost *p, u16 base, u8 count, u16 x)
{
    p->text = (u16)(base + x % count);
    p->alt = (u16)(base + (x + 1) % count);          /* 続けて同じ文になったときの代わり */
}

/* 時間帯 s の、反応すべき自分の出来事（無ければ 0）。出来事のあった時間帯なら *first = 1 */
static const PlayerEv *event_at(u16 s, u8 *first)
{
    u8 j;
    for (j = 0; j < PEV_MAX; j++) {
        const PlayerEv *e = &g_save.pev[j];
        if (!e->kind || e->kind >= PEV_KINDS) continue;
        if (s >= e->slot && s < e->slot + REACT_SLOTS) { *first = (u8)(s == e->slot); return e; }
    }
    return 0;
}

/* 時間帯 s の書き込み。無ければ 0。reply = 0 のときは返事にしない（1 つ前を見に行くのは 1 段だけ） */
static u8 post_at(Work *w, u16 s, BoardPost *p, u8 reply)
{
    u16 d = s >> 2, act[MATES_MAX], h, h2, rd;
    u16 x = (u16)(s + d);                            /* 文の選び方: 隣の時間帯で必ず替わり、日ごとにもずれる */
    u8 k, na = 0, trait, first = 0;
    const PlayerEv *ev;
    for (k = 0; k < w->nm; k++) {
        rd = w->rd[k];
        if (rd == d && (s & 3) == 2) {                   /* 辞める日の昼に、脱退の知らせだけが流れる */
            p->npc = w->m[k]; p->kind = BP_LEAVE; p->tone = 0; p->text = p->alt = 0; p->param = 0; p->day = d;
            p->ref = BOARD_NOREF;
            return 1;
        }
        if (rd > d || (rd == d && (s & 3) < 2)) act[na++] = w->m[k];
    }
    h = hash(s, SALT_POST(0));
    ev = event_at(s, &first);
    if (!na || ((h & 7) >= na && !first)) return 0;  /* 自分の出来事のあった時間帯は必ず誰かが書く */
    p->npc = act[(h >> 3) % na];
    p->kind = BP_POST;
    p->day = d;
    p->param = 0;
    p->ref = BOARD_NOREF;
    trait = (u8)hash(p->npc, SALT_MATE);
    p->tone = trait & 3;
    h2 = hash(s, SALT_POST(1));
    for (k = 0; k < w->nm && w->m[k] != p->npc; k++) {}
    rd = w->rd[k];
    if (rd != BOARD_NEVER && rd - d <= FADE_DAYS && (h2 & 1)) {
        say(p, T_BD_FADE0, 8, x);
        return 1;
    }
    if (ev && (first || (h2 & 0x0100))) {           /* 自分の出来事への反応 */
        say(p, (u16)(T_BD_RE0 + (ev->kind - 1) * RE_TEXTS), RE_TEXTS, x);
        if (ev->kind == PEV_SSR && ev->param >= 1 && ev->param <= tbl_card_count()) {   /* セーブから来た番号は表の範囲に限る */
            CardRec c;
            tbl_card(ev->param, &c);
            p->param = c.name_text;
        }
        if ((ev->kind == PEV_STOLEN || ev->kind == PEV_RIVAL) && ev->param < NPC_COUNT) p->ref = ev->param;
        return 1;
    }
    if (reply && s && !((h2 >> 9) & 3)) {             /* 1 つ前の時間帯への返事 */
        BoardPost q;
        if (post_at(w, (u16)(s - 1), &q, 0) && q.npc != p->npc) {
            p->ref = q.npc;
            if (q.kind == BP_LEAVE) say(p, T_BD_BYE0, 3, x);
            else if (q.text >= T_BD_FADE0 && q.text < T_BD_FADE0 + 8) say(p, T_BD_CONSOLE0, 3, x);
            else say(p, T_BD_REPLY0, 8, x);
            return 1;
        }
    }
    if (w->day != d) topics(w, d);
    if (w->nt && (h2 >> 4) % 100 < TOPIC_PCT) {
        const Topic *t = &w->t[(h2 >> 11) % w->nt];
        say(p, t->base, t->count, x);
        p->param = t->param;
        return 1;
    }
    switch ((h2 >> 2) & 7) {                         /* 暮らしの話 */
    case 0:
        if (d % 30 == 0) { say(p, T_BD_PAYDAY0, 3, x); return 1; }          /* 給料日（30 日ごと） */
        break;
    case 1:
        if (d % 7 >= 5) { say(p, T_BD_WKND0, 4, x); return 1; }             /* 休日 */
        break;
    case 2:
        say(p, T_BD_STORY0, 8, x);
        return 1;
    case 3:
        for (k = 0; k < w->nm; k++) {                /* 去った仲間を懐かしむ */
            u8 j = (u8)((k + x) % w->nm);
            if (w->rd[j] < d) { p->ref = w->m[j]; say(p, T_BD_MISS0, 3, x); return 1; }
        }
        break;
    default:
        break;
    }
    if (h2 & 0x0004) say(p, (u16)(T_BD_PAY0 + ((trait >> 2) & 3) * 2), 2, x);
    else say(p, (u16)(T_BD_HI0 + (s & 3) * 4), 4, x);
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
    for (i = (u16)((g_save.guild - 1) * 8 + 3); i < NPC_COUNT && w.nm < MATES_MAX; i += 128) {
        w.rd[w.nm] = retire_day(i);
        w.m[w.nm++] = i;
    }
    /* 続けて同じ文にしない。q0 を出すときは、q1 を「その古い方（いま来た nx）と比べて直したもの」と比べる。
     * 古い方だけを見るので、新しい書き込みが来ても過去の文は変わらない */
    for (look = 0; look < BOARD_LOOKBACK && l->n < l->want && l->n < BOARD_MAX; look++, s--) {
        if (post_at(&w, s, &nx, 1)) {
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
