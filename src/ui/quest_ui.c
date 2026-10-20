/* SPDX-License-Identifier: MIT */
/* quest_ui.c — クエスト。エリア一覧 → 探索（「進む」を押すたびに 1 ステージ）→ ボスはボス戦へ。
 * 当時の携帯ソシャゲの「進むを連打して探索度ゲージを 100% にする」遊びを、
 * 仕様の 500 ステージ（1 エリア 10 ステージ）にそのまま重ねた（DECISIONS #32）。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/player.h"
#include "../core/quest.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define ROWS     4
#define ROW_H    42
#define LIST_Y   18
#define SCENE_Y  16
#define GROUND_Y 92
#define SCENE_B  112
#define WALK_X   40
#define ENEMY_X  168
#define MSG_Y    150

static u16 first_stage(u16 area) { return (u16)((area - 1) * STAGES_PER_AREA + 1); }

static u8 cleared_in(u16 area)
{
    u16 f = first_stage(area);
    if (g_save.stage_cleared < f) return 0;
    return (u8)(g_save.stage_cleared - f + 1 >= STAGES_PER_AREA ? STAGES_PER_AREA : g_save.stage_cleared - f + 1);
}

/* ================================================================ エリア一覧 */

static void area_row(u16 i)
{
    AreaRec a;
    u16 area = i + 1;
    u8 y = (u8)(LIST_Y + (i - g_list.top) * ROW_H);
    u8 on = (u8)(i == g_list.cursor);
    u8 bg = on ? COL_BLUE : COL_DBLUE;
    u8 done = cleared_in(area), cx;
    tbl_area((u8)area, &a);
    ui_box(0, y, SCREEN_W, (u8)(ROW_H - 2), bg, on ? COL_YELLOW : COL_DGRAY);
    gfx_thumb(a.boss_card, 6, (u8)(y + 2));
    cx = txt_num(5, (u8)(y + 3), area, COL_LGRAY, bg);
    txt_id((u8)(cx + 1), (u8)(y + 3), a.name_text, on ? COL_YELLOW : COL_WHITE, bg);
    txt_id(27, (u8)(y + 3), (u16)(T_ELEM_0 + a.element), elem_color(a.element), bg);
    gfx_gauge(40, (u8)(y + 26), 120, done, STAGES_PER_AREA, done >= STAGES_PER_AREA ? COL_YELLOW : COL_GREEN);
    cx = txt_num(21, (u8)(y + 21), done, COL_WHITE, bg);
    cx = txt_draw(cx, (u8)(y + 21), "/10", COL_LGRAY, bg);
    if (done >= STAGES_PER_AREA) txt_id(26, (u8)(y + 21), T_CLEAR, COL_YELLOW, bg);
    else if (done == 0) txt_id(27, (u8)(y + 21), T_NEWAREA, COL_RED, bg);
}

static void area_rows(void)
{
    u16 i;
    vdp_fill(0, LIST_Y, SCREEN_W, ROWS * ROW_H, COL_BLACK);
    for (i = g_list.top; i < g_list.count && i < g_list.top + ROWS; i++) area_row(i);
}

FAR(void, area_init, (void))
{
    u16 n = (u16)((quest_max_stage() - 1) / STAGES_PER_AREA + 1);
    bgm_play(BGM_QUEST);
    if (g_sel_area == 0 || g_sel_area > n) g_sel_area = n;
    g_list.count = n;
    g_list.rows = ROWS;
    g_list.cursor = g_sel_area - 1;
    g_list.top = g_list.cursor >= ROWS ? g_list.cursor - ROWS + 1 : 0;
}

FAR(void, area_draw, (void))
{
    ui_clear();
    ui_header(T_AREA_TITLE);
    area_rows();
    ui_guide(T_GUIDE_LIST);
}

FAR(void, area_update, (void))
{
    u8 p = input_pressed();
    u16 old = g_list.cursor;
    u8 r = ui_list_input(&g_list, p);
    if (r == UL_PAGE) area_rows();
    if (r == UL_MOVED) { area_row(old); area_row(g_list.cursor); }
    if (p & BTN_OK) {
        psg_beep(0x80, 4);
        g_sel_area = g_list.cursor + 1;
        g_replay = 0;
        screen_change(&scr_explore);
    }
    if (p & BTN_CANCEL) screen_change(&scr_home);
}

SCREEN(scr_area, area_init, area_update, area_draw, 0)

/* ================================================================ 探索 */

/* 属性ごとの空（上・中）と地面、置物 */
static const u8 sky[5][2]    = { { COL_DRED, COL_RED }, { COL_BLUE, COL_CYAN }, { COL_DBLUE, COL_PURPLE },
                                 { COL_CYAN, COL_WHITE }, { COL_BLACK, COL_DBLUE } };
static const u8 ground[5][2] = { { COL_DGRAY, COL_DRED }, { COL_GREEN, COL_DGREEN }, { COL_DGREEN, COL_GREEN },
                                 { COL_YELLOW, COL_ORANGE }, { COL_DGRAY, COL_PURPLE } };

static u8 area_elem(void)
{
    AreaRec a;
    tbl_area((u8)g_sel_area, &a);
    return a.element;
}

static void decor(u8 e, u8 x)
{
    switch (e) {
    case 0:  gfx_sprite(ATL_DECOR_FLAME, x, GROUND_Y - 12); break;
    case 1:  gfx_sprite(ATL_DECOR_TREE, x, GROUND_Y - 24); break;
    case 2:  gfx_sprite(ATL_DECOR_ROCK, x, GROUND_Y + 2); break;
    case 3:  gfx_sprite(ATL_DECOR_PILLAR, x, GROUND_Y - 24); break;
    default: gfx_sprite(ATL_DECOR_CRYSTAL, x, GROUND_Y - 16); break;
    }
}

/* 風景を描く。scroll は歩いた量（置物と雲が左へ流れる） */
static void scene(u8 scroll)
{
    u8 e = area_elem(), i;
    vdp_fill(0, SCENE_Y, SCREEN_W, 40, sky[e][0]);
    vdp_fill(0, SCENE_Y + 40, SCREEN_W, GROUND_Y - SCENE_Y - 40, sky[e][1]);
    vdp_fill(0, GROUND_Y, SCREEN_W, SCENE_B - GROUND_Y, ground[e][0]);
    vdp_fill(0, GROUND_Y + 10, SCREEN_W, 4, ground[e][1]);
    gfx_sprite(ATL_DECOR_CLOUD, (u8)(200 - scroll / 2), SCENE_Y + 6);
    gfx_sprite(ATL_DECOR_CLOUD, (u8)(60 - scroll / 2), SCENE_Y + 14);
    for (i = 0; i < 4; i++) decor(e, (u8)(i * 72 + 20 - scroll));
}

static void walker(u8 frame)
{
    if (frame) gfx_sprite(ATL_WALK1, WALK_X, SCENE_B - 32);
    else gfx_sprite(ATL_WALK0, WALK_X, SCENE_B - 32);
}

static u16 next_stage(void)
{
    u16 f = first_stage(g_sel_area);
    u8 done = cleared_in(g_sel_area);
    if (done < STAGES_PER_AREA) return f + done;
    return f + g_replay;                     /* クリア済みのエリアは 1 から周回する */
}

static u8 progress(void)
{
    return (u8)((next_stage() - first_stage(g_sel_area)) * 10);
}

static void bars(void)
{
    u8 cx;
    vdp_fill(0, 114, SCREEN_W, 34, COL_BLACK);
    txt_id(0, 114, T_EXPLORE, COL_YELLOW, COL_BLACK);
    gfx_gauge(56, 119, 144, progress(), 100, COL_YELLOW);
    cx = txt_num_r(26, 114, progress(), 3, COL_WHITE, COL_BLACK);
    txt_draw(cx, 114, "%", COL_LGRAY, COL_BLACK);
    gfx_sprite(ATL_ITEM_AP, 2, 131);
    gfx_gauge(56, 135, 144, g_save.ap, AP_MAX, COL_GREEN);
    cx = txt_num_r(26, 130, g_save.ap, 3, COL_WHITE, COL_BLACK);
    (void)cx;
}

static void msg_clear(void)
{
    ui_box(0, MSG_Y, SCREEN_W, 42, COL_DBLUE, COL_BLUE);
}

/* 「<名前>を倒した！」のような 1 行。name は文言 id、tail も文言 id */
static u8 msg_line(u8 line, u16 name, u16 tail, u8 col)
{
    u8 y = (u8)(MSG_Y + 4 + line * 16);
    u8 cx = txt_id(1, y, name, col, COL_DBLUE);
    if (tail) cx = txt_id(cx, y, tail, COL_WHITE, COL_DBLUE);
    return cx;
}

FAR(void, explore_draw, (void))
{
    AreaRec a;
    u8 cx;
    tbl_area((u8)g_sel_area, &a);
    ui_clear();
    ui_header(a.name_text);
    cx = txt_num(26, 0, (u32)next_stage() - first_stage(g_sel_area) + 1, COL_YELLOW, COL_DBLUE);
    txt_draw(cx, 0, "/10", COL_LGRAY, COL_DBLUE);
    scene(0);
    walker(0);
    bars();
    msg_clear();
    ui_guide(T_GUIDE_EXPLORE);
}

static void walk(void)
{
    u8 s;
    for (s = 1; s <= 8; s++) {
        scene((u8)(s * 6));
        walker((u8)(s & 1));
        psg_beep(0x180, 1);
        ui_wait(3);
    }
}

static void fight(void)
{
    u8 k;
    ui_wait(10);
    for (k = 0; k < 2; k++) {
        gfx_sprite(ATL_SLASH, ENEMY_X + 8, SCENE_B - 52);
        gfx_sprite(ATL_HIT, ENEMY_X + 16, SCENE_B - 40);
        psg_beep(0x0A0, 4);
        ui_wait(6);
    }
    gfx_num(ENEMY_X + 4, SCENE_Y + 4, g_qr.b.dmg_per_turn);
    ui_wait(16);
    scene(48);                  /* 倒した敵を消す */
    walker(0);
}

static void enemy_sprite(u8 e)
{
    static const u8 pos[5][2] = {
        { ATL_ENEMY0_X, ATL_ENEMY0_Y }, { ATL_ENEMY1_X, ATL_ENEMY1_Y }, { ATL_ENEMY2_X, ATL_ENEMY2_Y },
        { ATL_ENEMY3_X, ATL_ENEMY3_Y }, { ATL_ENEMY4_X, ATL_ENEMY4_Y },
    };
    gfx_sprite(pos[e][0], pos[e][1], 48, 64, ENEMY_X, SCENE_B - 64);
}

static void treasure(void)
{
    CardRec c;
    gfx_sprite(ATL_CHEST, 120, SCENE_B - 36);
    psg_beep(0x070, 6);
    ui_wait(20);
    gfx_sprite(ATL_CHEST_OPEN, 120, SCENE_B - 36);
    gfx_sprite(ATL_SPARK, 124, SCENE_B - 44);
    gfx_sprite(ATL_SPARK, 150, SCENE_B - 40);
    ui_wait(8);
    gfx_thumb(g_qr.drop_card, 126, SCENE_Y + 4);
    psg_beep(0x050, 10);
    tbl_card(g_qr.drop_card, &c);
    vdp_fill(4, MSG_Y + 20, SCREEN_W - 8, 16, COL_DBLUE);     /* 2 行目を「手に入れた」に差し替える */
    msg_line(1, c.name_text, g_qr.drop_lost ? T_DROP_LOST : T_GOT, rarity_color(c.rarity));
    ui_wait(20);
}

static void result(void)
{
    AreaRec a;
    u8 cx;
    tbl_area((u8)g_sel_area, &a);
    if (!g_qr.b.win) {
        msg_line(0, a.enemy_text, T_APPEAR, COL_RED);
        msg_line(1, T_TOO_STRONG, 0, COL_WHITE);
        return;
    }
    msg_line(0, a.enemy_text, T_DEFEAT, COL_YELLOW);
    cx = txt_id(1, MSG_Y + 20, T_EXP, COL_CYAN, COL_DBLUE);
    cx = txt_draw(cx, MSG_Y + 20, "+", COL_LGRAY, COL_DBLUE);
    cx = txt_num(cx, MSG_Y + 20, g_qr.exp, COL_WHITE, COL_DBLUE);
    cx = txt_id((u8)(cx + 1), MSG_Y + 20, T_GOLD, COL_YELLOW, COL_DBLUE);
    cx = txt_draw(cx, MSG_Y + 20, "+", COL_LGRAY, COL_DBLUE);
    txt_num(cx, MSG_Y + 20, g_qr.gold, COL_WHITE, COL_DBLUE);
    if (g_qr.drop_card) { ui_wait(30); treasure(); }
    if (g_qr.levels) ui_msg(T_LEVELUP);
}

static u8 ensure_ap(u16 stage)
{
    if (g_save.ap >= quest_ap_cost(stage)) return 1;
    if (!g_save.potions) { ui_msg(T_AP_SHORT); return 0; }
    return (u8)(ui_confirm(T_USE_POTION) && player_use_potion());
}

static void go(void)
{
    u16 stage = next_stage();
    player_ap_update();
    if (!ensure_ap(stage)) return;
    if (stage % STAGES_PER_AREA == 0) {          /* 最後はボス */
        g_sel_stage = stage;
        ui_msg(T_BOSS_APPEAR);
        screen_change(&scr_boss);
        return;
    }
    quest_run(stage, &g_qr);
    if (g_qr.status != Q_OK) return;
    g_save_dirty = 1;
    msg_clear();
    walk();
    enemy_sprite(area_elem());
    if (g_qr.b.win) fight();
    else ui_wait(20);
    result();
    if (g_qr.b.win && cleared_in(g_sel_area) >= STAGES_PER_AREA) g_replay = (u8)((g_replay + 1) % (STAGES_PER_AREA - 1));
    bars();
}

FAR(void, explore_update, (void))
{
    u8 p = input_pressed();
    if (p & BTN_OK) {
        if (g_dirty) return;
        scene(0);
        walker(0);
        go();
        if (!g_dirty) {
            u8 cx = txt_num(26, 0, (u32)next_stage() - first_stage(g_sel_area) + 1, COL_YELLOW, COL_DBLUE);
            txt_draw(cx, 0, "/10", COL_LGRAY, COL_DBLUE);
        }
    }
    if (p & BTN_CANCEL) screen_change(&scr_area);
}

FAR(void, explore_init, (void)) { bgm_play(BGM_QUEST); }

SCREEN(scr_explore, explore_init, explore_update, explore_draw, 0)
