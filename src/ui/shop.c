/* shop.c — 回復薬を石で買う／使う（課金は無い。石はログインボーナスとゲーム内で手に入る） */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/player.h"
#include "../data/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"

#define ITEMS 2
static const u16 items[ITEMS] = { T_SHOP_BUY, T_SHOP_USE };

FAR(void, shop_draw, (void))
{
    ui_clear();
    ui_title(T_SHOP_TITLE);
    ui_status(ROW(1));
    ui_money(ROW(2));
    ui_menu_draw(4, ROW(4), items, ITEMS, g_cursor);
    ui_footer(T_HELP_SELECT);
}

static void act(void)
{
    if (g_cursor == 0) {
        if (g_save.gems < POTION_PRICE_GEMS) { ui_msg(T_GACHA_NO_MONEY); return; }
        g_save.gems -= POTION_PRICE_GEMS;
        player_add_item(ITEM_POTION, 1);
        g_save_dirty = 1;
        ui_money(ROW(2));
        ui_msg(T_SHOP_BOUGHT);
    } else {
        if (!player_use_potion()) { ui_msg(T_NO_POTION); return; }
        g_save_dirty = 1;
        ui_status(ROW(1));
        ui_money(ROW(2));
        ui_msg(T_SHOP_USED);
    }
}

FAR(void, shop_update, (void))
{
    u8 p = input_pressed();
    if (ui_menu_input(&g_cursor, ITEMS, p)) ui_menu_draw(4, ROW(4), items, ITEMS, g_cursor);
    if (p & BTN_OK) act();
    if (p & BTN_CANCEL) screen_change(&scr_home);
}

SCREEN(scr_shop, 0, shop_update, shop_draw, 0)
