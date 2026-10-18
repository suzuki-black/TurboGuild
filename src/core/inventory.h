/* inventory.h — 所持カード（g_save.cards）の出し入れ。デッキの添字も合わせて直す */
#ifndef CORE_INVENTORY_H
#define CORE_INVENTORY_H
#include "../types.h"
#define INV_FULL 0xFFFF
u16  inv_add(u16 card_id);        /* 追加した添字。満杯なら INV_FULL */
void inv_remove(u16 index);
void inv_clear(void);
u8   inv_in_deck(u16 index);      /* デッキの何枠目か (1..5)。入っていなければ 0 */
#endif
