/* tables.h — ROM の表の読み出し口。core/ はこの関数だけを通して表を読む。
 * 実体は MSX では sys/tables.c（データ窓を切り替えて RAM へ写す）、
 * PC のテストでは tests/tables_pc.c（build/ の .bin を読む）。
 * 写して返すので、別の表を読んでも前の値は壊れない（窓を跨ぐポインタを持たないため）。 */
#ifndef CORE_TABLES_H
#define CORE_TABLES_H
#include "../types.h"
#include "../data/cards.h"
#include "../data/tables.h"

u16  tbl_card_count(void);
u8   tbl_card(u16 id, CardRec *out);          /* 0 = その id は無い */
void tbl_area(u8 area, AreaRec *out);         /* 1..AREA_COUNT */
void tbl_stage(u16 no, StageRec *out);        /* 1..STAGE_COUNT */
void tbl_gacha(u8 id, GachaRec *out);         /* 1..GACHA_COUNT */
void tbl_login(u8 day, LoginRec *out);        /* 1..7 */
#endif
