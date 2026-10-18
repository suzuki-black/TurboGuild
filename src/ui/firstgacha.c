/* firstgacha.c — 最初の 10 連（プレミアの確率, SR 以上 1 枚確定）。何度でも引き直せる（リセマラ）。
 * 決めたら所持を入れ替え、おまかせ編成をして HOME へ。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/deck.h"
#include "../core/gacha.h"
#include "../core/inventory.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"

#define PREMIUM_ID 3
#define LIST_Y     ROW(1)

static void roll(void)
{
    g_gacha.gacha = PREMIUM_ID;
    g_gacha.count = FIRST_GACHA_COUNT;
    gacha_roll(&g_gacha);
    g_save.pity_count = 0;       /* 引き直しで天井が貯まらないように */
}

static void draw_choice(void)
{
    ui_fill_rows(ROW(11), 1, COL_BLACK);
    ui_cursor(0, ROW(11), (u8)(g_cursor == 0));
    txt_id(2, ROW(11), T_FG_REROLL, g_cursor == 0 ? COL_LYELLOW : COL_WHITE, COL_BLACK);
    ui_cursor(12, ROW(11), (u8)(g_cursor == 1));
    txt_id(14, ROW(11), T_FG_DECIDE, g_cursor == 1 ? COL_LYELLOW : COL_WHITE, COL_BLACK);
}

FAR(void, firstgacha_init, (void))
{
    roll();
}

FAR(void, firstgacha_draw, (void))
{
    ui_clear();
    ui_title(T_FG_TITLE);
    ui_footer(T_FG_SUB);
    ui_reveal();
    draw_choice();
}

static void decide(void)
{
    u8 i;
    if (!ui_confirm(T_FG_CONFIRM)) { g_dirty = 1; return; }
    inv_clear();
    for (i = 0; i < FIRST_GACHA_COUNT; i++) inv_add(g_gacha.ids[i]);
    deck_auto();
    g_save.flags |= SF_STARTED;
    g_save_dirty = 1;
    screen_change(&scr_home);
}

FAR(void, firstgacha_update, (void))
{
    u8 p = input_pressed();
    if (p & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) {
        g_cursor ^= 1;
        psg_beep(0x100, 2);
        draw_choice();
    }
    if (!(p & BTN_OK)) return;
    if (g_cursor == 0) {
        roll();
        g_dirty = 1;
    } else {
        decide();
    }
}

SCREEN(scr_firstgacha, firstgacha_init, firstgacha_update, firstgacha_draw, 0)
