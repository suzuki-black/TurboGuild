/* SPDX-License-Identifier: MIT */
/* ending.c — サービス終了の結末（ending.h） */
#include "ending.h"
#include "board.h"
#include "save.h"
#include "../data/cards.h"
#include "../data/tables.h"
#include "../far.h"

u8 ending_kind(const EndingStats *e)
{
    if (e->spent >= END_WHALE_YEN) return END_WHALE;
    if ((u16)e->kinds * 100 >= (u16)e->kinds_all * END_COLLECT_PCT) return END_COLLECT;
    if (e->mates && !e->stayed) return END_ALONE;
    if (e->gvg_won) return END_GUILD;
    if (!e->spent) return END_FREE;
    return END_NORMAL;
}

FAR(void, ending_stats, (EndingStats *e))
{
    u8 seen[16];                 /* カード id 0..127 を持っているか */
    u16 i, rd;
    u8 k;
    for (k = 0; k < sizeof(seen); k++) seen[k] = 0;
    e->kinds = 0;
    for (i = 0; i < g_save.card_count && i < CARD_MAX; i++) {
        u16 id = g_save.cards[i].id;
        if (id < 128 && !(seen[id >> 3] & (1 << (id & 7)))) { seen[id >> 3] |= (u8)(1 << (id & 7)); e->kinds++; }
    }
    e->kinds_all = CARD_COUNT_BUILT;
    e->spent = g_save.spent;
    e->gvg_won = (u8)(g_save.memo[MEMO_GVGWIN] != 0);
    e->mates = e->stayed = 0;
    e->first_leave = END_NEVER;
    if (g_save.guild) {
        for (i = (u16)((g_save.guild - 1) * 8 + 3); i < NPC_COUNT && e->mates < END_MATES_MAX; i += 128) {
            e->mates++;
            rd = mate_info(i);
            if ((u8)rd == MATE_STAYS) { e->tone[e->stayed] = (u8)(rd >> 8); e->stay[e->stayed++] = i; }
            else if ((u8)rd < e->first_leave) e->first_leave = (u8)rd;
        }
    }
    e->kind = ending_kind(e);
}
