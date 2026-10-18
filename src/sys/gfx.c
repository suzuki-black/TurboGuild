/* gfx.c — 部品のコピーは VDP コマンドに任せ、CPU は待たない（次のコマンドの前に vdp_cmd が待つ）。
 * カード絵は ROM のデータ窓 8000h から直接 VRAM へ書く（RAM に溜めない）。 */
#include "gfx.h"
#include "vdp.h"
#include "bank.h"
#include "../ram.h"
#include "../config.h"
#include "../data/rompages.h"
#include "../data/palette.h"

#define CMD_HMMM 0xD0
#define CMD_LMMM_T 0x98      /* LMMM + TIMP（色 0 は書かない） */

void gfx_load_atlas(void)
{
    VdpXfer16 x;
    u8 p;
    x.addr = 0x8000;
    for (p = 0; p < ATLAS_PAGES; p++) {
        bank_dataA((u8)(PAGE_ATLAS0 + p));
        x.src = (const u8 *)(WIN_DATAA + PAGE_HDR_SIZE);
        x.len = (p + 1 < ATLAS_PAGES) ? 8192 - PAGE_HDR_SIZE : 32768 - (8192 - PAGE_HDR_SIZE) * (ATLAS_PAGES - 1);
        vdp_load(&x);
        x.addr += x.len;
    }
}

static void copy(u8 sx, u16 sy, u8 w, u8 h, u8 dx, u8 dy, u8 cmd)
{
    VdpCmd *c = &g_vdpcmd;
    c->sx = sx; c->sy = sy;
    c->dx = dx; c->dy = dy;
    c->nx = w ? w : 256; c->ny = h;
    c->clr = 0; c->arg = 0;
    c->cmd = cmd;
    vdp_cmd(c);
}

void gfx_part(u8 sx, u8 sy, u8 w, u8 h, u8 dx, u8 dy)   { copy(sx, (u16)sy + ATLAS_VY, w, h, dx, dy, CMD_HMMM); }
void gfx_sprite(u8 sx, u8 sy, u8 w, u8 h, u8 dx, u8 dy) { copy(sx, (u16)sy + ATLAS_VY, w, h, dx, dy, CMD_LMMM_T); }
void gfx_move(u8 sx, u8 sy, u8 w, u8 h, u8 dx, u8 dy)   { copy(sx, sy, w, h, dx, dy, CMD_HMMM); }

static const u8 *card_src(u16 id)
{
    u16 i = id - 1;
    bank_dataA((u8)(PAGE_CARDART0 + i / ART_CARDS_PER_PAGE));
    return (const u8 *)(WIN_DATAA + PAGE_HDR_SIZE + (i % ART_CARDS_PER_PAGE) * ART_CARD_BYTES);
}

static void put(const u8 *src, u8 x, u8 y, u8 w, u8 h)
{
    VdpRect r;
    r.addr = VADDR(x, y);
    r.buf = src;
    r.w = (u8)(w / 2);
    r.h = h;
    vdp_put_rect(&r);
}

void gfx_card(u16 id, u8 x, u8 y)
{
    put(card_src(id), x, y, ART_CARD_W, ART_CARD_H);
}

void gfx_thumb(u16 id, u8 x, u8 y)
{
    u16 i = id - 1;
    bank_dataA((u8)(PAGE_THUMB0 + i / ART_THUMBS_PER_PAGE));
    put((const u8 *)(WIN_DATAA + PAGE_HDR_SIZE + (i % ART_THUMBS_PER_PAGE) * ART_THUMB_BYTES),
        x, y, ART_THUMB_W, ART_THUMB_H);
}

/* 1 行（28 バイト = 56 ドット）を横 2 倍に広げ、同じ行を 2 回書く */
void gfx_card2x(u16 id, u8 x, u8 y)
{
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

void gfx_gauge(u8 x, u8 y, u8 w, u16 val, u16 max, u8 col)
{
    u8 fill = max ? (u8)((u32)(w - 2) * (val > max ? max : val) / max) : 0;
    vdp_fill(x, y, w, 6, COL_BLACK);
    vdp_fill((u16)x + 1, (u16)y + 1, (u16)(w - 2), 4, COL_DGRAY);
    if (fill) vdp_fill((u16)x + 1, (u16)y + 1, fill, 4, col);
}

u8 gfx_num(u8 x, u8 y, u32 v)
{
    static const u32 pow10[8] = { 10000000UL, 1000000UL, 100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL };
    u8 i, started = 0;
    for (i = 0; i < 8; i++) {
        u8 d = 0;
        while (v >= pow10[i]) { v -= pow10[i]; d++; }
        if (d || started || i == 7) {
            gfx_sprite((u8)(ATL_DIGITS_X + d * 8), ATL_DIGITS_Y, 8, ATL_DIGITS_H, x, y);
            x = (u8)(x + 7);
            started = 1;
        }
    }
    return x;
}

void gfx_pal(u8 index, u8 r, u8 g, u8 b)
{
    u8 pal[2];
    pal[0] = (u8)((r << 4) | b);
    pal[1] = g;
    vdp_pal1(index, pal);
}

void gfx_pal_reset(void)
{
    vdp_palette(art_palette);
}
