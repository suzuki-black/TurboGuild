/* SPDX-License-Identifier: MIT */
/* vdp.c — SCREEN5 の初期化と矩形塗り。I/O は vdp_asm.s だけが触る */
#include "vdp.h"
#include "bios.h"
#include "../ram.h"
#include "../data/palette.h"

void vdp_init_screen5(void)
{
    bios_chgmod(5);
    vdp_wreg(7, 0);                   /* 周辺色 = 0 */
    vdp_wreg(8, SYS_RG8SAV | 0x22);   /* TP: 色 0 を不透明に / SPD: スプライト非表示 */
    vdp_wreg(9, SYS_RG9SAV | 0x80);   /* 212 ライン */
    vdp_palette(art_palette);
    vdp_fill(0, 0, SCREEN_W, SCREEN_H, 0);
}

void vdp_fill(u16 x, u16 y, u16 w, u16 h, u8 color)
{
    VdpCmd *c = &g_vdpcmd;
    c->sx = 0; c->sy = 0;
    c->dx = x; c->dy = y;
    c->nx = w; c->ny = h;
    c->clr = (u8)((color << 4) | color);
    c->arg = 0;
    c->cmd = 0xC0;                     /* HMMV: x は偶数で使う */
    vdp_cmd(c);
}

void vdp_wait_vsync(void)
{
    u16 j = SYS_JIFFY;
    while (SYS_JIFFY == j) { }
}
