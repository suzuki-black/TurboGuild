/* SPDX-License-Identifier: MIT */
/* cardgrid.h — カードの絵を 6 列 × 3 段に並べる一覧（UI_CARD バンクの中だけで使う） */
#ifndef CARDGRID_H
#define CARDGRID_H
#include "../types.h"
#define GRID_COLS 6
#define GRID_ROWS 3
#define GRID_PAGE (GRID_COLS * GRID_ROWS)
void grid_setup(u8 with_remove);     /* with_remove: 先頭に「はずす」を置く（デッキの入れ替え用） */
void grid_draw(void);
u8   grid_input(u8 pressed);         /* 動いたら 1 */
s16  grid_card(void);                /* カーソルの所持カードの添字。「はずす」なら -1 */
#endif
