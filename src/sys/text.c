/* text.c — 文字列描画。text.dat の目次（TEXT0 ページ先頭）から文字列のページと位置を引く。
 * 漢字ROM の番地: 非漢字 (区+20h) は (hi-20h)*96+(lo-20h)、漢字は 1024+(hi-30h)*96+(lo-20h)。 */
#include "text.h"
#include "vdp.h"
#include "bios.h"
#include "bank.h"
#include "../ram.h"
#include "../config.h"
#include "../data/rompages.h"

void kanji_read(u16 index, u8 *buf);

static u16 sjis_index(u8 s1, u8 s2)
{
    u8 hi, lo;
    if (s1 >= 0xE0) s1 -= 0x40;
    hi = (u8)((s1 - 0x81) * 2 + 0x21);
    if (s2 >= 0x9F) {
        hi++;
        lo = (u8)(s2 - 0x7E);
    } else {
        lo = (u8)(s2 - 0x1F);
        if (s2 >= 0x80) lo--;
    }
    if (hi < 0x30) return (u16)((hi - 0x20) * 96 + (lo - 0x20));
    return (u16)(1024 + (hi - 0x30) * 96 + (lo - 0x20));
}

static void make_lut(u8 fg, u8 bg, u8 *lut)
{
    lut[0] = (u8)((bg << 4) | bg);
    lut[1] = (u8)((bg << 4) | fg);
    lut[2] = (u8)((fg << 4) | bg);
    lut[3] = (u8)((fg << 4) | fg);
}

/* 1bpp の 1 バイト（8 ドット）を 4bpp の 4 バイトにする */
static u8 *expand8(u8 bits, const u8 *lut, u8 *out)
{
    out[0] = lut[bits >> 6];
    out[1] = lut[(bits >> 4) & 3];
    out[2] = lut[(bits >> 2) & 3];
    out[3] = lut[bits & 3];
    return out + 4;
}

static void put_glyph(u8 cx, u8 y, u8 w)
{
    VdpRect r;
    r.addr = VADDR(cx * 8, y);
    r.buf = g_glyph;
    r.w = w;
    r.h = 16;
    vdp_put_rect(&r);
}

static void draw_wide(u8 cx, u8 y, u16 index, const u8 *lut)
{
    u8 raw[32];
    u8 *out = g_glyph;
    u8 row;
    kanji_read(index, raw);
    for (row = 0; row < 16; row++) {
        u8 q = (row < 8) ? 0 : 16;
        out = expand8(raw[q + (row & 7)], lut, out);
        out = expand8(raw[q + 8 + (row & 7)], lut, out);
    }
    put_glyph(cx, y, 8);
}

static void draw_half(u8 cx, u8 y, u8 ch, const u8 *lut)
{
    const u8 *glyph = BIOS_CGTABL + (u16)ch * 8;
    u8 *out = g_glyph;
    u8 row;
    for (row = 0; row < 8; row++) {
        u8 *line = out;
        out = expand8(glyph[row], lut, out);
        out[0] = line[0]; out[1] = line[1]; out[2] = line[2]; out[3] = line[3];
        out += 4;
    }
    put_glyph(cx, y, 4);
}

static u8 is_lead(u8 c)
{
    return (u8)((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC));
}

u8 txt_draw(u8 cx, u8 y, const char *s, u8 fg, u8 bg)
{
    u8 lut[4];
    make_lut(fg, bg, lut);
    while (*s && cx < TEXT_COLS) {
        u8 c = (u8)*s++;
        if (is_lead(c) && *s) {
            if (cx + 1 >= TEXT_COLS) break;
            draw_wide(cx, y, sjis_index(c, (u8)*s++), lut);
            cx += 2;
        } else {
            draw_half(cx, y, c, lut);
            cx++;
        }
    }
    return cx;
}

u8 txt_width(const char *s)
{
    u8 w = 0;
    while (*s) {
        if (is_lead((u8)*s) && s[1]) s++;
        else w--;
        w += 2;
        s++;
    }
    return w;
}

/* 目次: u16 件数, {u8 ページ, u16 位置} × 件数（TEXT0 ページの中身の先頭） */
const char *txt_ptr(u16 id)
{
    const u8 *dir;
    u8 page;
    u16 off;
    bank_dataB(PAGE_TEXT0);
    dir = (const u8 *)(WIN_DATAB + PAGE_HDR_SIZE + 2 + id * 3);
    page = dir[0];
    off = (u16)(dir[1] | (dir[2] << 8));
    bank_dataB((u8)(PAGE_TEXT0 + page));
    return (const char *)(WIN_DATAB + PAGE_HDR_SIZE + off);
}

u8 txt_id(u8 cx, u8 y, u16 id, u8 fg, u8 bg)
{
    return txt_draw(cx, y, txt_ptr(id), fg, bg);
}

u8 txt_id_width(u16 id)
{
    return txt_width(txt_ptr(id));
}

/* u32 の割り算は遅いので 10 のべき乗を引いて桁を出す */
static const u32 pow10[10] = {
    1000000000UL, 100000000UL, 10000000UL, 1000000UL, 100000UL,
    10000UL, 1000UL, 100UL, 10UL, 1UL,
};

static void format_num(u32 v, char *buf)
{
    u8 i, n = 0;
    for (i = 0; i < 10; i++) {
        char d = '0';
        while (v >= pow10[i]) { v -= pow10[i]; d++; }
        if (d != '0' || n || i == 9) buf[n++] = d;
    }
    buf[n] = 0;
}

u8 txt_num(u8 cx, u8 y, u32 v, u8 fg, u8 bg)
{
    char buf[11];
    format_num(v, buf);
    return txt_draw(cx, y, buf, fg, bg);
}

u8 txt_num_r(u8 cx, u8 y, u32 v, u8 width, u8 fg, u8 bg)
{
    char buf[11];
    u8 lut[4];
    u8 w;
    format_num(v, buf);
    w = txt_width(buf);
    make_lut(fg, bg, lut);
    while (w < width) {
        draw_half(cx++, y, ' ', lut);
        w++;
    }
    return txt_draw(cx, y, buf, fg, bg);
}
