/* vdp.h — SCREEN5 の初期化・矩形塗り・パレット・VRAM 転送 */
#ifndef VDP_H
#define VDP_H
#include "../types.h"

typedef struct { u16 addr; const u8 *src; u8 len; } VdpXfer;
typedef struct { u16 addr; const u8 *buf; u8 w; u8 h; } VdpRect;   /* w バイト × h 行 */
typedef struct {
    u16 sx, sy, dx, dy, nx, ny;
    u8  clr, arg, cmd;
} VdpCmd;

void vdp_wreg(u8 reg, u8 val);
void vdp_blit(const VdpXfer *x);
void vdp_cmd(const VdpCmd *c);
void vdp_wait_cmd(void);
void vdp_palette(const u8 *pal);
void vdp_pal1(u8 index, const u8 *pal);
void vdp_put_rect(const VdpRect *r);
typedef struct { u16 addr; const u8 *src; u16 len; } VdpXfer16;
void vdp_load(const VdpXfer16 *x);         /* 連続 len バイト */

void vdp_init_screen5(void);
void vdp_fill(u16 x, u16 y, u16 w, u16 h, u8 color);
void vdp_wait_vsync(void);

/* SCREEN5 の VRAM アドレス。x は 2 ドット単位で切り捨て */
#define VADDR(x, y) ((u16)((u16)(y) * 128 + ((x) >> 1)))

#define SCREEN_W 256
#define SCREEN_H 212
#endif
