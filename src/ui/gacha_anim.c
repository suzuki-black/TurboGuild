/* SPDX-License-Identifier: MIT */
/* gacha_anim.c — ガチャの開封演出（種類ごとに違う）と、カードをめくって見せる部分。
 *   無料      カプセルマシン: ハンドルを回す → カプセルが転がり出る → 割れて光る
 *   ゴールド  宝箱: 揺れる → ふたが開く → 光が立ち上る
 *   プレミア  魔法陣: 陣が明滅 → 光の柱（SR は金、SSR は虹色に変わり画面が白く光る＝確定演出）
 *   チケット  巻物: 広がって光る
 *   SR 確定   黄金の扉: 開いて白い光があふれる
 * 光の色は「出た中でいちばん高いレアリティ」で変える（当時の「色で察する」演出）。 */
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/gacha.h"
#include "../core/tables.h"
#include "../data/palette.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define CX 128
#define STAGE_Y 16
#define STAGE_H 176

static const u8 glow[4] = { COL_LGRAY, COL_CYAN, COL_YELLOW, COL_WHITE };
static const u8 rainbow[7][3] = {
    { 7, 1, 1 }, { 7, 4, 0 }, { 7, 7, 2 }, { 2, 6, 1 }, { 3, 6, 7 }, { 1, 3, 7 }, { 5, 1, 6 },
};

static u8 best(void)
{
    CardRec c;
    u8 i, b = 0;
    for (i = 0; i < g_gacha.got; i++) {
        tbl_card(g_gacha.ids[i], &c);
        if (c.rarity > b) b = c.rarity;
    }
    return b > RARITY_SSR ? RARITY_SSR : b;          /* USR・XUSR も SSR と同じ確定演出 */
}

static void stage_clear(void)
{
    vdp_fill(0, STAGE_Y, SCREEN_W, STAGE_H, COL_BLACK);
}

/* 色 index を虹色に回す（frames フレーム） */
static void cycle(u8 index, u8 frames)
{
    u8 k;
    for (k = 0; k < frames; k++) {
        const u8 *c = rainbow[k % 7];
        gfx_pal(index, c[0], c[1], c[2]);
        ui_wait(2);
    }
}

static void flash_white(void)
{
    gfx_pal(COL_BLACK, 7, 7, 7);
    psg_beep(0x030, 12);
    ui_wait(4);
    gfx_pal_reset();
}

static void sparkles(u8 n)
{
    u8 k;
    u16 r = (u16)(g_gacha.ids[0] * 37 + 11);
    for (k = 0; k < n; k++) {
        r = (u16)(r * 73 + 41);
        gfx_sprite(ATL_SPARK, (u8)(24 + (r & 0x7F) + (r >> 9 & 0x3F)), (u8)(STAGE_Y + 8 + (r >> 4 & 0x7F)));
        ui_wait(2);
    }
}

/* 下から光の柱が伸びる（色は col） */
static void pillar(u8 col, u8 x0, u8 bottom)
{
    u8 h;
    for (h = 8; h < bottom - STAGE_Y; h = (u8)(h + 8)) {
        vdp_fill((u16)x0 - 8, (u16)(bottom - h), 16, h, col);
        psg_beep((u16)(0x200 - h * 2), 1);
        ui_wait(1);
    }
}

static void anim_capsule(u8 b)
{
    static const u8 caps[4][2] = { { ATL_CAPSULE0_X, ATL_CAPSULE0_Y }, { ATL_CAPSULE1_X, ATL_CAPSULE1_Y },
                                   { ATL_CAPSULE2_X, ATL_CAPSULE2_Y }, { ATL_CAPSULE3_X, ATL_CAPSULE3_Y } };
    u8 k, y;
    gfx_sprite(ATL_MACHINE, CX - 24, 40);
    for (k = 0; k < 3; k++) {                       /* ハンドルを回す（カチッ、カチッ） */
        vdp_fill(CX - 2, 80, 4, 12, COL_WHITE);
        psg_beep(0x300, 3);
        ui_wait(8);
        gfx_sprite(ATL_MACHINE, CX - 24, 40);
        ui_wait(6);
    }
    for (y = 98; y < 160; y = (u8)(y + 6)) {        /* カプセルが転がり出る */
        vdp_fill(CX + 4, 98, 24, 80, COL_BLACK);
        gfx_sprite(caps[b][0], caps[b][1], 16, 16, CX + 8, y);
        ui_wait(2);
    }
    psg_beep(0x080, 4);
    ui_wait(12);
    pillar(glow[b], CX + 16, 172);
    if (b == RARITY_SSR) cycle(glow[b], 21);
}

static void anim_chest(u8 b)
{
    u8 k;
    for (k = 0; k < 8; k++) {                       /* 揺れる */
        vdp_fill(CX - 26, 120, 52, 40, COL_BLACK);
        gfx_sprite(ATL_CHEST, (u8)(CX - 20 + ((k & 1) ? 3 : -3)), 124);
        psg_beep(0x280, 2);
        ui_wait(4);
    }
    vdp_fill(CX - 26, 120, 52, 40, COL_BLACK);
    gfx_sprite(ATL_CHEST_OPEN, CX - 20, 124);
    psg_beep(0x060, 8);
    pillar(glow[b], CX, 128);
    sparkles(6);
    if (b == RARITY_SSR) cycle(glow[b], 21);
}

static void anim_circle(u8 b)
{
    gfx_sprite(ATL_CIRCLE, CX - 32, 148);
    cycle(COL_CYAN, 14);                            /* 陣が明滅する */
    gfx_pal_reset();
    pillar(b >= RARITY_SR ? COL_YELLOW : COL_BLUE, CX, 164);
    if (b == RARITY_SSR) {                          /* 確定演出: 柱が虹に変わり、画面が白く光る */
        cycle(COL_YELLOW, 28);
        flash_white();
        sparkles(10);
    }
    gfx_pal_reset();
}

static void anim_scroll(u8 b)
{
    u8 w;
    for (w = 8; w <= 120; w = (u8)(w + 8)) {        /* 巻物が左右に広がる */
        vdp_fill((u16)(CX - w / 2), 90, w, 28, COL_SKIN);
        gfx_sprite(ATL_SCROLL, (u8)(CX - w / 2 - 4), 92);
        psg_beep((u16)(0x180 - w), 1);
        ui_wait(2);
    }
    vdp_fill(CX - 50, 102, 100, 4, glow[b]);
    sparkles(6);
}

static void anim_door(u8 b)
{
    u8 k;
    gfx_sprite(ATL_DOOR, CX - 20, 80);
    for (k = 0; k < 3; k++) { cycle(COL_YELLOW, 7); }
    gfx_pal_reset();
    gfx_sprite(ATL_DOOR_OPEN, CX - 20, 80);
    psg_beep(0x040, 12);
    for (k = 8; k <= 96; k = (u8)(k + 8)) {         /* 扉の向こうから白い光 */
        vdp_fill((u16)(CX - k), 90, (u16)k * 2, 46, COL_WHITE);
        ui_wait(1);
    }
    (void)b;
    flash_white();
}

/* ---- カードをめくる ---- */

static void flip_big(u16 id)
{
    CardRec c;
    u8 w, cx;
    stage_clear();
    gfx_part(ATL_CARDBACK, CX - 28, 56);
    ui_wait(20);
    for (w = 48; w >= 8; w = (u8)(w - 16)) {        /* 裏が細くなる */
        vdp_fill(CX - 28, 56, 56, 72, COL_BLACK);
        gfx_part((u8)(ATL_CARDBACK_X + (56 - w) / 2), ATL_CARDBACK_Y, w, 72, (u8)(CX - w / 2), 56);
        ui_wait(2);
    }
    tbl_card(id, &c);
    stage_clear();
    gfx_card2x(id, CX - 56, STAGE_Y + 4);
    psg_beep(c.rarity >= RARITY_SR ? 0x040 : 0x100, 10);
    vdp_fill(0, 166, SCREEN_W, 18, rarity_color(c.rarity));
    cx = ui_rarity(4, 167, c.rarity, rarity_color(c.rarity));
    txt_id((u8)(cx + 1), 167, c.name_text, COL_BLACK, rarity_color(c.rarity));
    if (c.rarity >= RARITY_SSR) { sparkles(8); cycle(COL_PURPLE, 21); gfx_pal_reset(); }
}

static void flip_grid(void)
{
    static const u16 tone[RARITIES] = { 0x200, 0x140, 0x0C0, 0x060, 0x050, 0x040 };
    CardRec c;
    u8 i;
    stage_clear();
    for (i = 0; i < g_gacha.got; i++) {
        gfx_part(ATL_CARDBACK_S, (u8)(18 + (i % 5) * 46), (u8)(40 + (i / 5) * 64));
    }
    ui_wait(20);
    for (i = 0; i < g_gacha.got; i++) {
        u8 x = (u8)(18 + (i % 5) * 46), y = (u8)(40 + (i / 5) * 64);
        tbl_card(g_gacha.ids[i], &c);
        if (c.rarity >= RARITY_SR) {
            vdp_fill((u16)x - 3, (u16)y - 3, 34, 42, c.rarity >= RARITY_SSR ? COL_PURPLE : COL_YELLOW);
            psg_beep(0x050, 6);
            ui_wait(10);
        }
        gfx_thumb(g_gacha.ids[i], x, y);
        if (c.rarity >= RARITY_SR) gfx_sprite(ATL_SPARK, (u8)(x + 22), (u8)(y - 4));
        psg_beep(tone[c.rarity], c.rarity >= RARITY_SR ? 10 : 3);
        ui_wait(c.rarity >= RARITY_SR ? 16 : 6);
    }
}

FAR(void, gacha_show, (u8 kind))
{
    u8 b = best();
    stage_clear();
    switch (kind) {
    case 1:  anim_capsule(b); break;
    case 2:  anim_chest(b); break;
    case 5:  anim_scroll(b); break;
    case 6:  anim_door(b); break;
    default: anim_circle(b); break;
    }
    ui_wait(10);
    gfx_pal_reset();
    if (g_gacha.got == 1) flip_big(g_gacha.ids[0]);
    else flip_grid();
}
