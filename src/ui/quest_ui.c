/* quest_ui.c — エリア選択 → ステージ選択 → 戦闘 → 結果（仕様 §9）。
 * 戦闘の中身は core/quest.c が 1 回で決める。ここはログを最大 5 行に縮めて見せるだけ。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/player.h"
#include "../core/quest.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"

#define LIST_Y  ROW(2)
#define LIST_N  10

static u16 max_area(void)
{
    return (u16)((quest_max_stage() - 1) / STAGES_PER_AREA + 1);
}

static void list_init(u16 count, u16 cursor)
{
    g_list.count = count;
    g_list.rows = LIST_N;
    g_list.cursor = cursor;
    g_list.top = cursor >= LIST_N ? cursor - LIST_N + 1 : 0;
}

static void header(u16 title)
{
    ui_clear();
    ui_title(title);
    ui_status(ROW(1));
    ui_footer(T_HELP_SELECT);
}

/* ---- エリア選択 ---- */

static void area_row(u16 i)
{
    AreaRec a;
    u8 y = (u8)(LIST_Y + (i - g_list.top) * LINE_H);
    u8 on = (u8)(i == g_list.cursor);
    u16 area = i + 1;
    ui_fill_rows(y, 1, COL_BLACK);
    tbl_area((u8)area, &a);
    ui_cursor(0, y, on);
    txt_num_r(1, y, area, 2, COL_GRAY, COL_BLACK);
    txt_id(4, y, a.name_text, on ? COL_LYELLOW : COL_WHITE, COL_BLACK);
    txt_id(22, y, (u16)(T_ELEM_0 + a.element), elem_color(a.element), COL_BLACK);
    if (g_save.stage_cleared >= area * STAGES_PER_AREA) txt_id(26, y, T_IN_DECK, COL_LYELLOW, COL_BLACK);
}

static void area_rows(void)
{
    u16 i;
    ui_fill_rows(LIST_Y, LIST_N, COL_BLACK);
    for (i = g_list.top; i < g_list.count && i < g_list.top + LIST_N; i++) area_row(i);
}

FAR(void, area_init, (void))
{
    u16 n = max_area();
    if (g_sel_area == 0 || g_sel_area > n) g_sel_area = n;
    list_init(n, g_sel_area - 1);
}

FAR(void, area_draw, (void))
{
    header(T_AREA_TITLE);
    area_rows();
}

FAR(void, area_update, (void))
{
    u8 p = input_pressed();
    u16 old = g_list.cursor;
    u8 r = ui_list_input(&g_list, p);
    if (r == UL_PAGE) area_rows();
    if (r == UL_MOVED) { area_row(old); area_row(g_list.cursor); }
    if (p & BTN_OK) {
        g_sel_area = g_list.cursor + 1;
        g_sel_stage = 0;
        screen_change(&scr_stage);
    }
    if (p & BTN_CANCEL) screen_change(&scr_home);
}

SCREEN(scr_area, area_init, area_update, area_draw, 0)

/* ---- ステージ選択 ---- */

static u16 first_stage(void) { return (u16)((g_sel_area - 1) * STAGES_PER_AREA + 1); }

static void stage_row(u16 i)
{
    u8 y = (u8)(LIST_Y + i * LINE_H);
    u8 on = (u8)(i == g_list.cursor);
    u16 no = first_stage() + i;
    u8 cx;
    ui_fill_rows(y, 1, COL_BLACK);
    ui_cursor(0, y, on);
    cx = txt_num(2, y, g_sel_area, on ? COL_LYELLOW : COL_WHITE, COL_BLACK);
    cx = txt_draw(cx, y, "-", COL_GRAY, COL_BLACK);
    txt_num(cx, y, i + 1, on ? COL_LYELLOW : COL_WHITE, COL_BLACK);
    if (i + 1 == STAGES_PER_AREA) txt_id(9, y, T_BOSS, COL_RED, COL_BLACK);
    cx = txt_id(16, y, T_AP, COL_GRAY, COL_BLACK);
    txt_num((u8)(cx + 1), y, quest_ap_cost(no), COL_WHITE, COL_BLACK);
    if (no <= g_save.stage_cleared) txt_id(26, y, T_IN_DECK, COL_LYELLOW, COL_BLACK);
}

static void stage_rows(void)
{
    u16 i;
    ui_fill_rows(LIST_Y, LIST_N, COL_BLACK);
    for (i = 0; i < g_list.count; i++) stage_row(i);
}

FAR(void, stage_init, (void))
{
    u16 n = quest_max_stage() - first_stage() + 1;
    if (n > STAGES_PER_AREA) n = STAGES_PER_AREA;
    list_init(n, g_sel_stage ? g_sel_stage - first_stage() : n - 1);
    if (g_list.cursor >= n) g_list.cursor = n - 1;
}

FAR(void, stage_draw, (void))
{
    AreaRec a;
    tbl_area((u8)g_sel_area, &a);
    header(a.name_text);
    stage_rows();
}

/* 戦闘ログ。ターンが多いときは最初の 4 回と最後の 1 回だけ見せる */
static u8 battle_log(void)
{
    u16 t;
    u8 y = ROW(4), shown = 0;
    for (t = 1; t <= g_qr.b.turns && shown < BATTLE_LOG_MAX; t++) {
        u8 cx;
        if (shown == BATTLE_LOG_MAX - 1 && t < g_qr.b.turns) {
            txt_id(2, y, T_SKIP, COL_GRAY, COL_BLACK);
            y = (u8)(y + LINE_H);
            t = g_qr.b.turns;
        }
        cx = txt_id(2, y, T_ATTACK, COL_WHITE, COL_BLACK);
        cx = txt_num((u8)(cx + 1), y, g_qr.b.dmg_per_turn, COL_LYELLOW, COL_BLACK);
        cx = txt_id(cx, y, T_DAMAGE, COL_WHITE, COL_BLACK);
        if (g_qr.b.taken_per_turn) {
            cx = txt_num((u8)(cx + 1), y, g_qr.b.taken_per_turn, COL_RED, COL_BLACK);
            txt_id(cx, y, T_TAKEN, COL_RED, COL_BLACK);
        }
        psg_beep(0x0A0, 3);
        ui_wait(10);
        y = (u8)(y + LINE_H);
        shown++;
    }
    return y;
}

static void show_enemy(void)
{
    AreaRec a;
    CardRec c;
    u16 id;
    u8 cx;
    tbl_area((u8)g_sel_area, &a);
    if (g_sel_stage % STAGES_PER_AREA == 0) {
        tbl_card(a.boss_card, &c);
        id = c.name_text;
    } else {
        id = a.enemy_text;
    }
    cx = txt_id(1, ROW(3), id, elem_color(a.element), COL_BLACK);
    txt_id(cx, ROW(3), T_APPEAR, COL_WHITE, COL_BLACK);
    ui_wait(20);
}

static u8 show_reward(u8 y)
{
    u8 cx = txt_id(2, y, T_EXP, COL_GRAY, COL_BLACK);
    cx = txt_draw((u8)(cx + 1), y, "+", COL_GRAY, COL_BLACK);
    cx = txt_num(cx, y, g_qr.exp, COL_WHITE, COL_BLACK);
    cx = txt_id((u8)(cx + 2), y, T_GOLD, COL_LYELLOW, COL_BLACK);
    cx = txt_draw((u8)(cx + 1), y, "+", COL_GRAY, COL_BLACK);
    txt_num(cx, y, g_qr.gold, COL_WHITE, COL_BLACK);
    y = (u8)(y + LINE_H);
    if (g_qr.drop_card) {
        txt_id(2, y, g_qr.drop_lost ? T_DROP_LOST : T_DROP, COL_LYELLOW, COL_BLACK);
        ui_card_row((u8)(y + LINE_H), g_qr.drop_card, 1, 0, COL_DRED);
        psg_beep(0x070, 10);
        y = (u8)(y + 2 * LINE_H);
    }
    if (g_qr.levels) { txt_id(2, y, T_LEVELUP, COL_GREEN, COL_BLACK); y = (u8)(y + LINE_H); }
    if (g_qr.area_cleared) { txt_id(2, y, T_AREA_CLEAR, COL_PINK, COL_BLACK); y = (u8)(y + LINE_H); }
    return y;
}

static void battle(void)
{
    u8 y;
    ui_fill_rows(ROW(2), 10, COL_BLACK);
    ui_status(ROW(1));
    show_enemy();
    y = battle_log();
    if (y > ROW(9)) { ui_fill_rows(ROW(4), 6, COL_BLACK); y = ROW(4); }
    ui_fill_rows(y, 1, g_qr.b.win ? COL_DRED : COL_BLUE);
    txt_id(2, y, g_qr.b.win ? T_WIN : T_LOSE, COL_LYELLOW, g_qr.b.win ? COL_DRED : COL_BLUE);
    psg_beep(g_qr.b.win ? 0x050 : 0x300, 15);
    if (g_qr.b.win) show_reward((u8)(y + LINE_H));
    ui_status(ROW(1));
    ui_wait_key();
}

static void run(void)
{
    u16 no = first_stage() + g_list.cursor;
    g_sel_stage = no;
    quest_run(no, &g_qr);
    if (g_qr.status == Q_NO_AP) {
        if (!g_save.potions) { ui_msg(T_AP_SHORT); return; }
        if (!ui_confirm(T_USE_POTION) || !player_use_potion()) return;
        quest_run(no, &g_qr);
    }
    if (g_qr.status != Q_OK) return;
    g_save_dirty = 1;
    battle();
    if (g_qr.b.win && no == g_save.stage_cleared && no < STAGE_COUNT) g_sel_stage = no + 1;
    if (g_qr.area_cleared) { g_sel_area++; g_sel_stage = 0; screen_change(&scr_area); return; }
    screen_change(&scr_stage);         /* 進行が変わったので一覧を作り直す */
}

FAR(void, stage_update, (void))
{
    u8 p = input_pressed();
    u16 old = g_list.cursor;
    u8 r = ui_list_input(&g_list, p);
    if (r) { stage_row(old); stage_row(g_list.cursor); }
    if (p & BTN_OK) { run(); g_dirty = 1; }
    if (p & BTN_CANCEL) screen_change(&scr_area);
}

SCREEN(scr_stage, stage_init, stage_update, stage_draw, 0)
