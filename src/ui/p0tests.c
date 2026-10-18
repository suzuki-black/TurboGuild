/* p0tests.c — Phase0 の試験画面（カード表・入力/音）。コードバンク 1 に載る */
#include "screen.h"
#include "../ram.h"
#include "../config.h"
#include "../data/cards.h"
#include "../data/rompages.h"
#include "../sys/bank.h"
#include "../sys/font.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/vdp.h"


/* ---- カード表: データバンクの表をそのまま構造体で読む ---- */

static const char * const rar_names[4] = { "N", "R", "SR", "SSR" };

static void card_draw(void)
{
    const u16 *count;
    const CardRec *c;
    u16 i;
    ui_clear();
    ui_header("CARD TABLE (ROM)");
    bank_dataA(PAGE_CARDS);
    count = (const u16 *)(WIN_DATAA + PAGE_HDR_SIZE);
    c = (const CardRec *)(WIN_DATAA + PAGE_HDR_SIZE + 2);
    font_puts(1, 2, "COUNT", 7, 1);
    font_putdec(7, 2, *count, 15, 1);
    font_puts(1, 4, "ID RAR EL  ATK  DEF CO SK LV", 7, 1);
    for (i = 0; i < *count && i < 18; i++, c++) {
        u8 y = (u8)(5 + i);
        font_putdec(1, y, c->id, 15, 1);
        font_puts(4, y, c->rarity < 4 ? rar_names[c->rarity] : "?", c->rarity >= 2 ? 11 : 15, 1);
        font_putdec(8, y, c->element, 15, 1);
        font_putdec(11, y, c->atk, 15, 1);
        font_putdec(16, y, c->def, 15, 1);
        font_putdec(21, y, c->cost, 15, 1);
        font_putdec(24, y, c->skill, 15, 1);
        font_putdec(27, y, c->max_lv, 15, 1);
    }
    ui_footer("ESC:BACK");
}

const Screen scr_cardtest = { 0, back_on_cancel, card_draw, 0 };

/* ---- 入力と音: 押されているボタンを表示し、押した瞬間に音を鳴らす ---- */

static const char * const btn_names[6] = { "UP", "DOWN", "LEFT", "RIGHT", "OK", "CANCEL" };

static void input_draw_state(void)
{
    u8 h = input_held();
    u8 i;
    for (i = 0; i < 6; i++) {
        u8 on = (u8)(h & (1 << i));
        font_puts(2, (u8)(4 + i), btn_names[i], on ? 11 : 14, 1);
    }
}

static void input_update_scr(void)
{
    u8 p = input_pressed();
    if (p & ~BTN_CANCEL) psg_beep((u16)(0x80 + (p << 4)), 4);
    if (input_held() != g_ui_prev) {
        g_ui_prev = input_held();
        input_draw_state();
    }
    back_on_cancel();
}

static void input_draw(void)
{
    ui_clear();
    ui_header("INPUT / SOUND");
    input_draw_state();
    ui_footer("ESC:BACK (KEYS / JOY1)");
}

const Screen scr_inputtest = { 0, input_update_scr, input_draw, 0 };
