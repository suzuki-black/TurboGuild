/* SPDX-License-Identifier: MIT */
/* thread.c — 攻略スレ（thread.h, DECISIONS #104） */
#include "thread.h"
#include "save.h"
#include "tables.h"
#include "../config.h"
#include "../far.h"
#include "../data/text_ids.h"

#define TH_PER_DAY  ((u16)(GSEC_PER_DAY / TH_SECS))
#define N_CHAT  10
#define N_EV     6
#define N_USR    4
#define N_XUSR   4
#define N_END    6
#define N_RE     6
#define N_NE     6
#define NE_HOURS 15               /* 通信エラーの話は、起きた時から 15 時間（翌日の昼まで） */
#define DEMA_BACK 6               /* 突っ込みは 6 レス前までのデマに */

static u16 th_hash(u16 a, u16 b)
{
    u16 x = (u16)(a * 0x9E37u ^ (u16)(b * 0x85EBu));
    x ^= x >> 7;
    x = (u16)(x * 0x2C1Bu);
    x ^= x >> 9;
    return x;
}

FAR(u8, thread_event_at, (u16 day))
{
    EventRec e;
    u8 i;
    for (i = 1; i <= EVENT_COUNT; i++) {
        tbl_event(i, &e);
        if (day >= e.start_day && day < e.start_day + e.duration) return i;
    }
    return 0;
}

FAR(u16, thread_last, (void))
{
    u32 t = g_save.g_time;
    if (t >= (u32)SERVICE_END_DAY * GSEC_PER_DAY) t = (u32)SERVICE_END_DAY * GSEC_PER_DAY - 1;   /* 終わったら書き込みも止まる */
    return (u16)(t / TH_SECS);
}

/* ボーダーのレスなら 1（tier・pct・lie を埋める） */
static u8 border_of(u16 p, ThPost *o)
{
    u16 h = th_hash(p, 0x51A7), g = th_hash(p, 0x0BD3), t;
    if (!(o->ev = thread_event_at((u16)(p / TH_PER_DAY)))) return 0;
    if ((h & 255) >= 56) return 0;               /* イベント中のレスの 2 割ほど */
    t = (u16)((h >> 8) & 3);
    o->tier = (u8)(t == 0 ? 0 : t == 3 ? 2 : 1);  /* 100 位の話がいちばん多い */
    o->lie = (u8)((g & 3) == 0);
    if (!o->lie) o->pct = (u8)(85 + (g >> 2) % 31);
    else if (g & 4) o->pct = (u8)(160 + (g >> 3) % 61);   /* 盛りすぎ */
    else o->pct = (u8)(40 + (g >> 3) % 21);               /* 甘すぎ */
    o->text = (u16)(T_TH_BD0 + o->tier * 2 + ((g >> 12) & 1));
    return 1;
}

/* 時刻 t（ゲーム内の秒）が、年表の NETERR（ガチャの通信エラー, DECISIONS #109）から NE_HOURS 時間のうちなら 1 */
static u8 neterr_near(u32 t)
{
    TimelineRec r;
    u8 i;
    u32 s;
    for (i = 0; i < TIMELINE_COUNT; i++) {
        tbl_timeline(i, &r);
        if (r.kind != TL_NETERR) continue;
        s = (u32)r.day * GSEC_PER_DAY + (u32)r.param * 3600;
        if (t >= s && t < s + (u32)NE_HOURS * 3600) return 1;
    }
    return 0;
}

/* 関係ないカード（特効のデマ）: 表のどれか、本物と同じなら隣 */
static u16 other_card(u16 h, u16 real)
{
    u16 n = tbl_card_count(), c = (u16)(1 + h % n);
    return c == real ? (u16)(c % n + 1) : c;
}

FAR(void, thread_post, (u16 p, ThPost *o))
{
    u16 day = (u16)(p / TH_PER_DAY), h = th_hash(p, 0x3C6E), g = th_hash(p, 0x7F4A), r = (u16)(h & 255), k;
    u8 ev = thread_event_at(day);
    EventRec e;
    ThPost q;
    o->no = (u16)(p % TH_PART + 1);
    o->id = th_hash(p, 0x1D2D);
    o->card = 0; o->ref = 0; o->real = 0; o->ev = 0; o->tier = 0; o->pct = 0; o->lie = 0;
    o->kind = TH_BORDER;
    if (border_of(p, o)) return;
    o->ev = 0;
    if (ev && r < 96) {                           /* 6 レス前までのデマのボーダーに突っ込む */
        for (k = 1; k <= DEMA_BACK && k < o->no; k++) {
            if (!border_of((u16)(p - k), &q) || !q.lie) continue;
            o->kind = TH_DEMA;
            o->ref = (u16)(o->no - k);
            o->text = (u16)((q.pct > 100 ? T_TH_DEMA0 : T_TH_DEMA1) + 2 * (g & 1));   /* 盛りすぎ / 甘すぎ、2 通り */
            return;
        }
    }
    if (r >= 96 && r < 132) {                     /* 特効の話（イベント中はいま、無い時期は次） */
        u8 i = ev;
        if (!i) for (i = 1; i <= EVENT_COUNT; i++) { tbl_event(i, &e); if (e.start_day > day) break; }
        if (i <= EVENT_COUNT) {
            tbl_event(i, &e);
            if (e.bonus_card) {
                o->kind = ev ? TH_BOON : TH_NEXT;
                o->lie = (u8)((g & 3) == 0);
                o->real = e.bonus_card;
                o->card = o->lie ? other_card(g >> 2, e.bonus_card) : e.bonus_card;
                o->text = (u16)((ev ? T_TH_BOON0 : T_TH_NEXT0) + ((g >> 8) & 1));
                return;
            }
        }
    }
    if (r >= 132 && r < 172 && o->no > 4) {       /* 返事 */
        o->kind = TH_REPLY;
        o->ref = (u16)(o->no - 1 - (g & 3));
        o->text = (u16)(T_TH_RE0 + (g >> 2) % N_RE);
        return;
    }
    o->kind = TH_CHAT;
    if ((g & 3) && neterr_near((u32)p * TH_SECS)) o->text = (u16)(T_TH_NE0 + (g >> 2) % N_NE);   /* 石だけ減った話でもちきり */
    else if (day >= SERVICE_END_DAY - 30 && (g & 1)) o->text = (u16)(T_TH_END0 + (g >> 1) % N_END);
    else if (day >= 180 && (g & 6) == 0) o->text = (u16)(T_TH_XUSR0 + (g >> 3) % N_XUSR);
    else if (day >= 90 && (g & 6) == 2) o->text = (u16)(T_TH_USR0 + (g >> 3) % N_USR);
    else if (ev && (g & 8)) o->text = (u16)(T_TH_EV0 + (g >> 4) % N_EV);
    else o->text = (u16)(T_TH_CHAT0 + (g >> 4) % N_CHAT);
}
