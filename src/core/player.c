/* SPDX-License-Identifier: MIT */
/* player.c — AP は ap_gtime からの経過で回復させ、端数は ap_gtime に残す（取りこぼさない）。
 * 上限に達している間は時計だけ進める（満タン放置ぶんを後で貯金しない）。 */
#include "player.h"
#include "save.h"
#include "inventory.h"
#include "../far.h"
#include "../data/tables.h"

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
    }
    g_save.exp = (u16)(e > 0xFFFF ? 0xFFFF : e);
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
