/* gfx.h — 絵の描画。部品は VRAM ページ 1 のアトラスから VDP コマンドで、カード絵は ROM から CPU で。
 * 座標はドット。部品は ATL_<名前> をそのまま渡す（x, y, w, h の 4 つに展開される）:
 *     gfx_part(ATL_ICON_QUEST, 16, 120);     不透明（HMMM。x と幅は偶数）
 *     gfx_sprite(ATL_ENEMY0, 160, 40);       透過（LMMM, 色 0 を抜く） */
#ifndef GFX_H
#define GFX_H
#include "../types.h"
#include "../data/art.h"

void gfx_load_atlas(void);
void gfx_part(u8 sx, u8 sy, u8 w, u8 h, u8 dx, u8 dy);
void gfx_sprite(u8 sx, u8 sy, u8 w, u8 h, u8 dx, u8 dy);
void gfx_move(u8 sx, u8 sy, u8 w, u8 h, u8 dx, u8 dy);    /* 画面内（ページ 0）の矩形コピー */
/* カード絵は UI_COMMON バンク（ui/gfx_card.c）。座標を 1 つの 16bit にまとめて中継を通す */
void gfx_card_xy(u16 id, u16 xy);
void gfx_thumb_xy(u16 id, u16 xy);
void gfx_card2x_xy(u16 id, u16 xy);
u8   gfx_num_xy(u16 xy, u16 v);
#define GFX_XY(x, y)          ((u16)((u8)(x) | ((u16)(u8)(y) << 8)))
#define gfx_card(id, x, y)    gfx_card_xy((id), GFX_XY(x, y))     /* 56x72 */
#define gfx_thumb(id, x, y)   gfx_thumb_xy((id), GFX_XY(x, y))    /* 28x36 */
#define gfx_card2x(id, x, y)  gfx_card2x_xy((id), GFX_XY(x, y))   /* 112x144 */
#define gfx_num(x, y, v)      gfx_num_xy(GFX_XY(x, y), (u16)((v) > 65535UL ? 65535UL : (v)))
void gfx_gauge(u8 x, u8 y, u8 w, u16 val, u16 max, u8 col);   /* 高さ 6 の棒 */
void gfx_pal(u8 index, u8 r, u8 g, u8 b); /* パレット 1 色（演出用） */
void gfx_pal_reset(void);
#endif
