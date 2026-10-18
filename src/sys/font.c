/* font.c — 8x8 半角字形を 4bpp に展開して描く。座標は 8px グリッド（32 桁 x 26 行）。
 * 字形は本体 BIOS の CGTABL を読む。ROM に字形を持たず、半角カナもそのまま出せる。 */
#include "font.h"
#include "vdp.h"
#include "bios.h"
#include "../ram.h"

/* 2 ドット = 1 バイト。2bit の組み合わせ 4 通りを先に作って引くだけにする */
void font_putc(u8 cx, u8 cy, u8 ch, u8 fg, u8 bg)
{
    const u8 *glyph = BIOS_CGTABL + (u16)ch * 8;
    u8 *out = g_fontbuf;
    u8 lut[4];
    u8 row, i;
    lut[0] = (u8)((bg << 4) | bg);
    lut[1] = (u8)((bg << 4) | fg);
    lut[2] = (u8)((fg << 4) | bg);
    lut[3] = (u8)((fg << 4) | fg);
    for (row = 0; row < 8; row++) {
        u8 bits = glyph[row];
        for (i = 0; i < 4; i++) {
            *out++ = lut[bits >> 6];
            bits <<= 2;
        }
    }
    {
        VdpRect r;
        r.addr = (u16)(cy * 1024 + cx * 4);
        r.buf = g_fontbuf;
        r.w = 4;
        r.h = 8;
        vdp_put_rect(&r);
    }
}

void font_puts(u8 cx, u8 cy, const char *s, u8 fg, u8 bg)
{
    while (*s && cx < 32) {
        font_putc(cx++, cy, (u8)*s++, fg, bg);
    }
}

void font_puthex(u8 cx, u8 cy, u16 v, u8 digits, u8 fg, u8 bg)
{
    static const char hex[] = "0123456789ABCDEF";
    while (digits--) {
        font_putc(cx + digits, cy, (u8)hex[v & 15], fg, bg);
        v >>= 4;
    }
}

/* u32 の割り算は Z80 で 1 回数百µs かかり、表を 1 枚描くだけで数百 ms になる。
 * 10 のべき乗を引き算して桁を出す。 */
static const u32 pow10[10] = {
    1000000000UL, 100000000UL, 10000000UL, 1000000UL, 100000UL,
    10000UL, 1000UL, 100UL, 10UL, 1UL,
};

void font_putdec(u8 cx, u8 cy, u32 v, u8 fg, u8 bg)
{
    u8 i;
    u8 started = 0;
    for (i = 0; i < 10; i++) {
        u8 d = 0;
        while (v >= pow10[i]) { v -= pow10[i]; d++; }
        if (d || started || i == 9) {
            font_putc(cx++, cy, (u8)('0' + d), fg, bg);
            started = 1;
        }
    }
}
