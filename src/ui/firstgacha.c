/* firstgacha.c — 最初の 10 連（プレミアの確率, SR 以上 1 枚確定）。プレミアと同じ魔法陣の演出で開く。
 * [1] 引き直す（リセマラ, 何度でも）[2] このカードで始める。決めたら所持を入れ替え、おまかせ編成して HOME へ。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/deck.h"
#include "../core/gacha.h"
#include "../core/inventory.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"

void gacha_show(u8 kind);

#define PREMIUM_ID 3
#define CHOICE_Y   172

static void roll(void)
{
    g_gacha.gacha = PREMIUM_ID;
    g_gacha.count = FIRST_GACHA_COUNT;
    gacha_roll(&g_gacha);
    g_save.pity_count = 0;       /* 引き直しで天井が貯まらないように */
}

static void draw_choice(void)
{
    static const u16 labels[2] = { T_FG_REROLL, T_FG_DECIDE };
    u8 i;
    for (i = 0; i < 2; i++) {
        u8 x = (u8)(i ? 104 : 4), w = (u8)(i ? 148 : 96), on = (u8)(i == g_cursor);
        ui_box(x, CHOICE_Y, w, 20, on ? COL_BLUE : COL_DBLUE, on ? COL_YELLOW : COL_DGRAY);
        ui_key((u8)(i + 1), (u8)(x + 4), CHOICE_Y + 4);
        txt_id((u8)(x / 8 + 2), CHOICE_Y + 2, labels[i], on ? COL_YELLOW : COL_WHITE, on ? COL_BLUE : COL_DBLUE);
    }
}

FAR(void, firstgacha_init, (void))
{
    bgm_play(BGM_GACHA);
    roll();
    g_cursor = 1;
}

FAR(void, firstgacha_draw, (void))
{
    ui_clear();
    ui_header(T_FG_TITLE);
    ui_guide(T_FG_SUB);
    gacha_show(PREMIUM_ID);
    draw_choice();
}

static void decide(void)
{
    u8 i;
    if (!ui_confirm(T_FG_CONFIRM)) return;
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
    u8 d = input_digit();
    if (p & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) { g_cursor ^= 1; psg_beep(0x100, 2); draw_choice(); }
    if (d == 1 || d == 2) { g_cursor = (u8)(d - 1); p = BTN_OK; }
    if (!(p & BTN_OK)) return;
    if (g_cursor == 0) { roll(); g_dirty = 1; }
    else decide();
}

SCREEN(scr_firstgacha, firstgacha_init, firstgacha_update, firstgacha_draw, 0)
