/* shop.c — [1] 石を買う（軍資金で課金）[2] 回復薬を買う（石 100）[3] 回復薬を使う。
 * 軍資金はリアルマネーの代わり（DECISIONS #33）。無料の石や回復薬を使い切ったら、ここで払うしかない。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/player.h"
#include "../core/tables.h"
#include "../core/wallet.h"
#include "../data/screens.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define ITEMS 3
#define BTN_Y 116
static const u16 items[ITEMS] = { T_SHOP_GEMS, T_SHOP_POTION, T_SHOP_USE };

static void button(u8 i)
{
    u8 on = (u8)(i == g_cursor);
    u8 y = (u8)(BTN_Y + i * 24);
    ui_box(8, y, 240, 22, on ? COL_BLUE : COL_DBLUE, on ? COL_YELLOW : COL_DGRAY);
    ui_key((u8)(i + 1), 12, (u8)(y + 5));
    txt_id(4, (u8)(y + 3), items[i], on ? COL_YELLOW : COL_WHITE, on ? COL_BLUE : COL_DBLUE);
}

static void info(void)
{
    u8 cx;
    vdp_fill(0, 96, SCREEN_W, 16, COL_BLACK);
    gfx_sprite(ATL_ITEM_POTION, 2, 97);
    cx = txt_id(2, 96, T_OWNED, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), 96, g_save.potions, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 2), 96, T_SPENT, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), 96, g_save.spent, COL_ORANGE, COL_BLACK);
    txt_id(cx, 96, T_YEN, COL_LGRAY, COL_BLACK);
}

FAR(void, shop_draw, (void))
{
    u8 i;
    ui_clear();
    ui_header(T_SHOP_TITLE);
    vdp_fill(0, 16, 64, 76, COL_DGREEN);
    gfx_sprite(ATL_ICON_SHOP, 20, 40);
    ui_status_block(20);
    info();
    for (i = 0; i < ITEMS; i++) button(i);
    ui_guide(T_GUIDE_LIST);
}

/* パック選び: 1〜3 で石の量（値段は軍資金の円） */
static u8 ask_pack(void)
{
    PackRec pk;
    u8 cur = 0, k, cx;
    ui_box(16, 60, 224, 92, COL_DBLUE, COL_CYAN);
    txt_id(3, 64, T_PACK_TITLE, COL_YELLOW, COL_DBLUE);
    for (;;) {
        u8 p, d;
        for (k = 0; k < PACK_COUNT; k++) {
            u8 y = (u8)(84 + k * 20), bg = k == cur ? COL_BLUE : COL_DBLUE;
            tbl_pack((u8)(k + 1), &pk);
            vdp_fill(24, y, 208, 18, bg);
            ui_key((u8)(k + 1), 26, (u8)(y + 3));
            gfx_sprite(ATL_ITEM_GEMS, 42, (u8)(y + 2));
            cx = txt_num(8, y, pk.gems, COL_CYAN, bg);
            cx = txt_id((u8)(cx + 2), y, T_FUNDS, COL_LGRAY, bg);
            cx = txt_num((u8)(cx + 1), y, pk.price, COL_WHITE, bg);
            txt_id(cx, y, T_YEN, COL_LGRAY, bg);
        }
        g_ui_state = UIS_WAITKEY;
        do { ui_frame(); p = input_pressed(); d = input_digit(); } while (!p && d == NO_DIGIT);
        if (d >= 1 && d <= PACK_COUNT) return d;
        if (p & BTN_UP) cur = (u8)((cur + PACK_COUNT - 1) % PACK_COUNT);
        if (p & BTN_DOWN) cur = (u8)((cur + 1) % PACK_COUNT);
        if (p & BTN_CANCEL) return 0;
        if (p & BTN_OK) return (u8)(cur + 1);
    }
}

static void act(void)
{
    if (g_cursor == 0) {
        u8 pk = ask_pack();
        g_dirty = 1;
        if (!pk) return;
        if (wallet_buy_gems(pk) != W_OK) { ui_msg(T_NO_FUNDS); return; }
        g_save_dirty = 1;
        psg_beep(0x040, 10);
        ui_msg(T_GEMS_BOUGHT);
    } else if (g_cursor == 1) {
        if (g_save.gems < POTION_PRICE_GEMS) { ui_msg(T_GACHA_NO_MONEY); return; }
        g_save.gems -= POTION_PRICE_GEMS;
        player_add_item(ITEM_POTION, 1);
        g_save_dirty = 1;
        psg_beep(0x060, 6);
        ui_status_block(20);
        info();
    } else {
        if (!player_use_potion()) { ui_msg(T_NO_POTION); return; }
        g_save_dirty = 1;
        psg_beep(0x040, 12);
        ui_msg(T_SHOP_USED);
    }
}

FAR(void, shop_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    u8 old = g_cursor;
    if (p & BTN_UP) g_cursor = (u8)((g_cursor + ITEMS - 1) % ITEMS);
    if (p & BTN_DOWN) g_cursor = (u8)((g_cursor + 1) % ITEMS);
    if (d >= 1 && d <= ITEMS) g_cursor = (u8)(d - 1);
    if (old != g_cursor) { psg_beep(0x100, 2); button(old); button(g_cursor); }
    if ((p & BTN_OK) || (d >= 1 && d <= ITEMS)) act();
    if (p & BTN_CANCEL) screen_change(&scr_home);
}

SCREEN(scr_shop, 0, shop_update, shop_draw, 0)
