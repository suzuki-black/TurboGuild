/* player.h — レベル・経験値・AP（仕様 §3, 企画書「スタミナ」） */
#ifndef CORE_PLAYER_H
#define CORE_PLAYER_H
#include "../types.h"
#include "../config.h"

#define PLAYER_HP(lv)       ((u16)(PLAYER_HP_BASE + PLAYER_HP_PER_LV * (u16)(lv)))
#define PLAYER_COST_MAX(lv) ((u16)(DECK_COST_BASE + DECK_COST_PER_LV * (u16)(lv)))
#define EXP_NEED(lv)        ((u16)(EXP_NEED_BASE + EXP_NEED_K * (u16)(lv) * (u16)(lv)))

void player_ap_update(void);      /* g_time までの自然回復を反映 */
u8   player_add_exp(u16 exp);     /* 上がったレベル数。上がったら AP 全回復 */
u8   player_use_potion(void);     /* 0 = 回復薬が無い */
void player_add_item(u16 item, u16 amount);  /* ITEM_*。所持数の上限は 99 */
#endif
