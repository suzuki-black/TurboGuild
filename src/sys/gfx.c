/* SPDX-License-Identifier: MIT */
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

void gfx_gauge(u8 x, u8 y, u8 w, u16 val, u16 max, u8 col)
{
    u8 fill = max ? (u8)((u32)(w - 2) * (val > max ? max : val) / max) : 0;
    vdp_fill(x, y, w, 6, COL_BLACK);
    vdp_fill((u16)x + 1, (u16)y + 1, (u16)(w - 2), 4, COL_DGRAY);
    if (fill) vdp_fill((u16)x + 1, (u16)y + 1, fill, 4, col);
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
