/* vdp.c — SCREEN5 の初期化と矩形塗り。I/O は vdp_asm.s だけが触る */
#include "vdp.h"
#include "bios.h"
#include "../ram.h"

/* 0RRR0BBB, 00000GGG。ソシャゲ風に明るめの 16 色 */
static const u8 palette[32] = {
    0x00,0x00, 0x00,0x00, 0x11,0x06, 0x33,0x07,
    0x17,0x01, 0x27,0x03, 0x51,0x01, 0x27,0x06,
    0x71,0x01, 0x73,0x03, 0x61,0x06, 0x64,0x06,
    0x11,0x04, 0x65,0x02, 0x55,0x05, 0x77,0x07,
};

void vdp_init_screen5(void)
{
    bios_chgmod(5);
    vdp_wreg(7, 0);                   /* 周辺色 = 0 */
    vdp_wreg(8, SYS_RG8SAV | 0x22);   /* TP: 色 0 を不透明に / SPD: スプライト非表示 */
    vdp_wreg(9, SYS_RG9SAV | 0x80);   /* 212 ライン */
    vdp_palette(palette);
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
