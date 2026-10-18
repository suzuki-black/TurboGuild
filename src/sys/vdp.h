/* vdp.h — SCREEN5 の初期化・矩形塗り・パレット・VRAM 転送 */
#ifndef VDP_H
#define VDP_H
#include "../types.h"

typedef struct { u16 addr; const u8 *src; u8 len; } VdpXfer;
typedef struct {
    u16 sx, sy, dx, dy, nx, ny;
    u8  clr, arg, cmd;
} VdpCmd;

void vdp_wreg(u8 reg, u8 val);
void vdp_blit(const VdpXfer *x);
void vdp_cmd(const VdpCmd *c);
void vdp_wait_cmd(void);
void vdp_palette(const u8 *pal);
void vdp_put8x8(u16 addr, const u8 *buf);   /* 4 バイト × 8 行 */

void vdp_init_screen5(void);
void vdp_fill(u16 x, u16 y, u16 w, u16 h, u8 color);
void vdp_wait_vsync(void);

#define SCREEN_W 256
#define SCREEN_H 212
#endif
