/* SPDX-License-Identifier: MIT */
/* timeline.c — kind の処理はここの switch 1 つ（仕様 §3.1）。新しい kind を足すときだけコードを触る。
 * イベントの開催日は build_data.py が年表の EVENT 行から events の start_day に写してある。 */
#include "timeline.h"
#include "player.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "../far.h"

static u16 today(void) { return (u16)time_gday(g_save.g_time); }

FAR(void, timeline_state, (TlState *s))
{
    TimelineRec t;
    u8 i;
    u16 keep = 100;
    s->day = today();
    s->rateup_pct = 100;
    s->era_pct = 100;
    s->drop_pct = 0;
    s->announce = s->ended = 0;
    s->days_to_end = 0;
    s->gvg = 0;
    s->gvg_start = 0;
    for (i = 0; i < TIMELINE_COUNT; i++) {
        tbl_timeline(i, &t);
        if (t.kind == TL_SERVICE_END) s->days_to_end = t.day > s->day ? t.day - s->day : 0;
        if (t.day > s->day) continue;
        switch (t.kind) {
        case TL_GACHA_RATEUP: if (s->day < t.day + RATEUP_DAYS) s->rateup_pct = t.param; break;
        case TL_INFLATION:    s->era_pct = t.param; break;
        case TL_PLAYER_DROP:  keep = (u16)(keep * (100 - t.param) / 100); break;
        case TL_ANNOUNCE_END: s->announce = 1; break;
        case TL_SERVICE_END:  s->ended = 1; break;
        case TL_GUILDWAR:     if (s->day < t.day + GVG_DAYS) { s->gvg = (u8)t.param; s->gvg_start = t.day; } break;
        default: break;          /* SERVICE_START / NEWS / EVENT / GIFT は状態を持たない */
        }
    }
    s->drop_pct = (u8)(100 - keep);
}

FAR(void, timeline_update, (TlResult *r))
{
    TimelineRec t;
    u16 d = today();
    r->n_news = 0;
    r->gift_gems = 0;
    while (g_save.timeline_cursor < TIMELINE_COUNT) {
        tbl_timeline((u8)g_save.timeline_cursor, &t);
        if (t.day > d) break;
        if (t.kind == TL_GIFT) { player_add_item(ITEM_GEMS, t.param); r->gift_gems += t.param; }
        r->n_news++;
        g_save.timeline_cursor++;
    }
}

FAR(u8, timeline_news, (u16 k, u16 *text_id))
{
    TimelineRec t;
    s16 i;
    u16 d = today();
    for (i = TIMELINE_COUNT - 1; i >= 0; i--) {
        tbl_timeline((u8)i, &t);
        if (t.day > d) continue;
        if (k-- == 0) { *text_id = t.text; return 1; }
    }
    return 0;
}

FAR(u16, timeline_drop_day, (u8 nth))
{
    TimelineRec t;
    u8 i;
    for (i = 0; i < TIMELINE_COUNT; i++) {
        tbl_timeline(i, &t);
        if (t.kind == TL_PLAYER_DROP && nth-- == 0) return t.day;
    }
    return 0xFFFF;
}
