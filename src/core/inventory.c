/* inventory.c — 所持カードは cards[0..card_count-1] に詰めて持つ。
 * 抜いたら後ろを詰め、デッキが指す添字もずらす。 */
#include "inventory.h"
#include "save.h"
#include "../far.h"

FAR(u16, inv_add, (u16 card_id))
{
    u16 i = g_save.card_count;
    if (i >= CARD_MAX) return INV_FULL;
    g_save.cards[i].id = card_id;
    g_save.cards[i].lv = 1;
    g_save.cards[i].flags = 0;
    g_save.card_count++;
    return i;
}

FAR(void, inv_remove, (u16 index))
{
    u16 i;
    u8 d;
    if (index >= g_save.card_count) return;
    for (i = index; i + 1 < g_save.card_count; i++) g_save.cards[i] = g_save.cards[i + 1];
    g_save.card_count--;
    for (d = 0; d < DECK_SIZE; d++) {
        if (g_save.deck[d] == DECK_EMPTY) continue;
        if (g_save.deck[d] == index) g_save.deck[d] = DECK_EMPTY;
        else if (g_save.deck[d] > index) g_save.deck[d]--;
    }
}

FAR(void, inv_clear, (void))
{
    u8 d;
    g_save.card_count = 0;
    for (d = 0; d < DECK_SIZE; d++) g_save.deck[d] = DECK_EMPTY;
}

FAR(u8, inv_in_deck, (u16 index))
{
    u8 d;
    for (d = 0; d < DECK_SIZE; d++) {
        if (g_save.deck[d] == index) return (u8)(d + 1);
    }
    return 0;
}
