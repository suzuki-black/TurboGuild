/* SPDX-License-Identifier: MIT */
/* gfx_card.c — カード絵（等倍・縮小・2 倍）と大きい数字。カーネル 8KB を空けるため UI_COMMON バンクに置く。
 * 呼ぶ側は sys/gfx.h のマクロ gfx_card(id, x, y) などで書ける（座標を 16bit 1 つにまとめて中継を通す）。 */
#include "../sys/gfx.h"
#include "../sys/vdp.h"
#include "../sys/bank.h"
#include "../far.h"
#include "../ram.h"
#include "../config.h"
#include "../data/rompages.h"

static void put(const u8 *src, u8 x, u8 y, u8 w, u8 h)
{
    VdpRect r;
    r.addr = VADDR(x, y);
    r.buf = src;
    r.w = (u8)(w / 2);
    r.h = h;
    vdp_put_rect(&r);
}

static const u8 *card_src(u16 id)
{
    u16 i = id - 1;
    bank_dataA((u8)(PAGE_CARDART0 + i / ART_CARDS_PER_PAGE));
    return (const u8 *)(WIN_DATAA + PAGE_HDR_SIZE + (i % ART_CARDS_PER_PAGE) * ART_CARD_BYTES);
}


FAR(void, gfx_card_xy, (u16 id, u16 xy))
{
    u8 x = (u8)xy, y = (u8)(xy >> 8);
    put(card_src(id), x, y, ART_CARD_W, ART_CARD_H);
}

FAR(void, gfx_thumb_xy, (u16 id, u16 xy))
{
    u8 x = (u8)xy, y = (u8)(xy >> 8);
    u16 i = id - 1;
    bank_dataA((u8)(PAGE_THUMB0 + i / ART_THUMBS_PER_PAGE));
    put((const u8 *)(WIN_DATAA + PAGE_HDR_SIZE + (i % ART_THUMBS_PER_PAGE) * ART_THUMB_BYTES),
        x, y, ART_THUMB_W, ART_THUMB_H);
}

/* 1 行（28 バイト = 56 ドット）を横 2 倍に広げ、同じ行を 2 回書く */
FAR(void, gfx_card2x_xy, (u16 id, u16 xy))
{
    u8 x = (u8)xy, y = (u8)(xy >> 8);
    const u8 *src = card_src(id);
    u8 row, i;
    for (row = 0; row < ART_CARD_H; row++) {
        u8 *o = g_glyph;
        for (i = 0; i < ART_CARD_W / 2; i++) {
            u8 b = *src++;
            u8 hi = b & 0xF0, lo = b & 0x0F;
            *o++ = (u8)(hi | (hi >> 4));
            *o++ = (u8)((lo << 4) | lo);
        }
        for (i = 0; i < ART_CARD_W; i++) g_glyph[ART_CARD_W + i] = g_glyph[i];
        put(g_glyph, x, (u8)(y + row * 2), ART_CARD_W * 2, 2);
    }
}

/* 大きい数字。v は 0..65535 まで（中継のレジスタ渡しの都合で 16bit） */
FAR(u8, gfx_num_xy, (u16 xy, u16 v))
{
    u8 x = (u8)xy, y = (u8)(xy >> 8);
    static const u16 pow10[5] = { 10000u, 1000u, 100u, 10u, 1u };
    u8 i, started = 0;
    for (i = 0; i < 5; i++) {
        u8 d = 0;
        while (v >= pow10[i]) { v -= pow10[i]; d++; }
        if (d || started || i == 4) {
            gfx_sprite((u8)(ATL_DIGITS_X + d * 8), ATL_DIGITS_Y, 8, ATL_DIGITS_H, x, y);
            x = (u8)(x + 7);
            started = 1;
        }
    }
    return x;
}

