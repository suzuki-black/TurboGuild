/* SPDX-License-Identifier: MIT */
/* player.c — AP は ap_gtime からの経過で回復させ、端数は ap_gtime に残す（取りこぼさない）。
 * 上限に達している間は時計だけ進める（満タン放置ぶんを後で貯金しない）。 */
#include "player.h"
#include "pev.h"
#include "save.h"
#include "inventory.h"
#include "../far.h"
#include "../data/tables.h"
#include "event.h"
#include "tables.h"
#include "timeline.h"

FAR(void, player_ap_update, (void))
{
    u32 now = g_save.g_time;
    u32 n;
    if (g_save.ap >= AP_MAX || now < g_save.ap_gtime) {
        g_save.ap_gtime = now;
        return;
    }
    n = (now - g_save.ap_gtime) / AP_RECOVER_GSEC;
    if (n >= (u32)(AP_MAX - g_save.ap)) {
        g_save.ap = AP_MAX;
        g_save.ap_gtime = now;
    } else {
        g_save.ap += (u8)n;
        g_save.ap_gtime += n * AP_RECOVER_GSEC;
    }
}

FAR(u8, player_add_exp, (u16 exp))
{
    u8 up = 0;
    u32 e = (u32)g_save.exp + exp;
    while (g_save.level < LEVEL_MAX && e >= EXP_NEED(g_save.level)) {
        e -= EXP_NEED(g_save.level);
        g_save.level++;
        up++;
        if (g_save.level % 10 == 0) pev_push(PEV_LEVEL, g_save.level);
    }
    g_save.exp = (u16)(e > 0xFFFF ? 0xFFFF : e);
    if (g_save.level >= 10) MEMO_MARK(MEMO_LV10);
    if (g_save.level >= 50) MEMO_MARK(MEMO_LV50);
    if (up && g_save.ap < AP_MAX) {
        g_save.ap = AP_MAX;
        g_save.ap_gtime = g_save.g_time;
    }
    return up;
}

FAR(u8, player_use_potion, (void))
{
    if (g_save.potions == 0) return 0;
    g_save.potions--;
    g_save.ap = AP_MAX;
    g_save.ap_gtime = g_save.g_time;
    return 1;
}

static u8 add_u8(u8 v, u16 a)
{
    u16 r = (u16)(v + a);
    return (u8)(r > 99 ? 99 : r);
}

FAR(void, player_add_item, (u16 item, u16 amount))
{
    switch (item) {
    case ITEM_GOLD:     g_save.gold += amount; break;
    case ITEM_POTION:   g_save.potions = add_u8(g_save.potions, amount); break;
    case ITEM_TICKET:   g_save.gacha_tickets = add_u8(g_save.gacha_tickets, amount); break;
    case ITEM_GEMS:     g_save.gems += amount; break;
    case ITEM_SRTICKET: g_save.sr_tickets = add_u8(g_save.sr_tickets, amount); break;
    case ITEM_CARD:     inv_add(amount); break;      /* amount はカード id */
    default: break;
    }
}

/* 宿屋（DECISIONS #91）。留守と同じで、寝ている間にイベントが進み、盗まれることもある（マイページに入ったときに数える） */
FAR(u8, player_sleep, (void))
{
    u32 day = g_save.g_time / GSEC_PER_DAY, wake = day * GSEC_PER_DAY + INN_WAKE_SECS, h;
    if (g_save.gold < INN_GOLD) return 0;
    if (g_save.g_time >= wake) wake += GSEC_PER_DAY;
    h = (wake - g_save.g_time + 3599) / 3600;
    g_save.gold -= INN_GOLD;
    g_save.g_time = wake;
    g_save.ap = AP_MAX;
    g_save.ap_gtime = wake;
    pev_push(PEV_INN, 0);
    return (u8)(h ? h : 1);
}

/* 次の予定（DECISIONS #92）。対抗戦の戦は年表の GUILDWAR の日から 7 日間、毎日 12:00 と 21:00。イベントは始まりの日の 0:00 */
FAR(void, plan_next, (Plan *p))
{
    static const u32 slot[2] = { 43200UL, 75600UL };
    TimelineRec r;
    EventRec e;
    u32 now = g_save.g_time, t;
    u8 i, m;
    p->kind = PLAN_NONE;
    p->at = 0xFFFFFFFFUL;
    p->text = 0;
    p->match = 0;
    for (i = 0; i < TIMELINE_COUNT; i++) {
        tbl_timeline(i, &r);
        if (r.kind != TL_GUILDWAR) continue;
        for (m = 0; m < GVG_DAYS * 2; m++) {
            t = ((u32)r.day + (m >> 1)) * GSEC_PER_DAY + slot[m & 1];
            if (t > now && t < p->at) { p->at = t; p->kind = PLAN_GVG; p->match = m; }
        }
    }
    for (i = 1; i <= EVENT_COUNT; i++) {
        tbl_event(i, &e);
        t = (u32)e.start_day * GSEC_PER_DAY;
        if (t > now && t < p->at) { p->at = t; p->kind = PLAN_EVENT; p->text = e.name_text; }
    }
}

FAR(u8, player_wait, (Plan *p))
{
    plan_next(p);
    if (p->kind == PLAN_NONE) return 0;
    player_ap_update();                              /* ここまでの回復を確かめてから、進んだぶんを自然に回復させる */
    g_save.g_time = p->at;
    player_ap_update();
    return 1;
}
