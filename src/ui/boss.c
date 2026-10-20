/* SPDX-License-Identifier: MIT */
/* boss.c — エリアボス戦。ボスのカード絵を 2 倍で大きく出し、デッキの 5 枚が順に攻撃する。
 * 勝敗は core/quest.c が式 1 本で先に決める。ここは最大 5 ターンに縮めて見せるだけ（仕様 §6）。
 * 被弾はパレットで画面を赤く光らせ、VDP の縦スクロール（R#23）で揺らす。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/event.h"
#include "../core/quest.h"
#include "../core/tables.h"
#include "../data/palette.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define BOSS_X  4
#define BOSS_Y  18
#define PANEL_X 124
#define DECK_Y  170
#define HP_Y    40

void event_gain_show(void);

static u8 is_event(void) { return (u8)(g_sel_stage >= EVENT_STAGE_BASE); }

/* バトルの相手の顔: NPC ごとに決まった R のカード */
static u16 foe_card(void) { return (u16)(21 + g_qr.foe % 12); }

static u16 boss_card(void)
{
    AreaRec a;
    if (is_event()) {
        EventStage es;
        event_stage(g_sel_stage - EVENT_STAGE_BASE, &es);
        return g_qr.kind == EV_PVP ? foe_card() : es.boss_card;
    }
    tbl_area((u8)((g_sel_stage - 1) / STAGES_PER_AREA + 1), &a);
    return a.boss_card;
}

static u32 boss_hp(void)
{
    StageRec st;
    if (g_qr.kind != EV_HUNT) return g_qr.raid_max;
    if (is_event()) {
        EventStage es;
        event_stage(g_sel_stage - EVENT_STAGE_BASE, &es);
        return es.hp;
    }
    tbl_stage(g_sel_stage, &st);
    return st.hp;
}

static void deck_row(u8 lift)
{
    u8 d;
    vdp_fill(0, DECK_Y - 8, SCREEN_W, SCREEN_H - DECK_Y + 8, COL_BLACK);
    for (d = 0; d < DECK_SIZE; d++) {
        u8 i = g_save.deck[d];
        u8 x = (u8)(8 + d * 48);
        if (i == DECK_EMPTY || i >= g_save.card_count) continue;
        gfx_thumb(g_save.cards[i].id, x, (u8)(DECK_Y - (d + 1 == lift ? 8 : 0)));
    }
}

static void hp(u32 now, u32 max)
{
    u8 cx;
    vdp_fill(PANEL_X, HP_Y, SCREEN_W - PANEL_X, 24, COL_BLACK);
    cx = txt_id(16, HP_Y, T_HP, COL_RED, COL_BLACK);
    txt_num((u8)(cx + 1), HP_Y, now, COL_WHITE, COL_BLACK);
    gfx_gauge(PANEL_X, HP_Y + 17, 124, (u16)(now >> 4), (u16)(max >> 4), COL_RED);
}

static void shake(void)
{
    u8 k;
    gfx_pal(COL_BLACK, 5, 0, 0);
    for (k = 0; k < 6; k++) {
        vdp_wreg(23, (u8)((k & 1) ? 3 : 253));
        ui_wait(2);
    }
    vdp_wreg(23, 0);
    gfx_pal_reset();
}

/* 1 ターン: d 枚目が跳ねて斬る → ダメージの数字 → HP が減る → 反撃があれば揺れる */
static void turn(u8 d, u32 *left, u32 max)
{
    deck_row((u8)(d + 1));
    psg_beep(0x0C0, 3);
    ui_wait(6);
    gfx_sprite(ATL_SLASH, (u8)(BOSS_X + 30 + d * 8), BOSS_Y + 40);
    gfx_sprite(ATL_HIT, (u8)(BOSS_X + 44 + d * 6), BOSS_Y + 56);
    psg_beep(0x090, 5);
    vdp_fill(PANEL_X, 80, SCREEN_W - PANEL_X, 14, COL_BLACK);
    gfx_num(PANEL_X + 8, 82, g_qr.b.dmg_per_turn);
    *left = *left > g_qr.b.dmg_per_turn ? *left - g_qr.b.dmg_per_turn : 0;
    hp(*left, max);
    ui_wait(12);
    gfx_card2x(boss_card(), BOSS_X, BOSS_Y);        /* 斬った跡を消す */
    if (g_qr.b.taken_per_turn) shake();
    deck_row(0);
}

static void outcome(void)
{
    CardRec c;
    u8 y = 104, cx;
    ui_box(PANEL_X - 4, y, SCREEN_W - PANEL_X + 4, 62, g_qr.b.win ? COL_DRED : COL_DBLUE, COL_YELLOW);
    if (g_qr.kind == EV_RAID) {                    /* レイドは倒せなくても削った分が残る */
        cx = txt_id(16, (u8)(y + 4), g_qr.b.win ? T_RAID_KILL : T_RAID_DMG, COL_YELLOW, g_qr.b.win ? COL_DRED : COL_DBLUE);
        if (!g_qr.b.win) txt_num((u8)(cx + 1), (u8)(y + 4), g_qr.b.dmg_per_turn * g_qr.b.turns, COL_WHITE, COL_DBLUE);
        psg_beep(g_qr.b.win ? 0x050 : 0x0A0, 20);
        return;
    }
    txt_id(16, (u8)(y + 4), g_qr.b.win ? T_WIN : T_LOSE, COL_YELLOW, g_qr.b.win ? COL_DRED : COL_DBLUE);
    psg_beep(g_qr.b.win ? 0x050 : 0x300, 20);
    if (!g_qr.b.win) return;
    cx = txt_id(16, (u8)(y + 22), T_EXP, COL_CYAN, COL_DRED);
    txt_num((u8)(cx + 1), (u8)(y + 22), g_qr.exp, COL_WHITE, COL_DRED);
    cx = txt_id(16, (u8)(y + 40), T_GOLD, COL_YELLOW, COL_DRED);
    txt_num((u8)(cx + 1), (u8)(y + 40), g_qr.gold, COL_WHITE, COL_DRED);
    if (g_qr.drop_card) {
        tbl_card(g_qr.drop_card, &c);
        gfx_thumb(g_qr.drop_card, 224, (u8)(y + 22));
        gfx_sprite(ATL_SPARK, 246, (u8)(y + 18));
    }
}

FAR(void, boss_init, (void))
{
    bgm_play(BGM_BOSS);
    quest_run(g_sel_stage, &g_qr);
    if (g_qr.status == Q_OK) g_save_dirty = 1;
}

FAR(void, boss_draw, (void))
{
    CardRec c;
    u32 left, max;
    u16 t, shown = 0;
    tbl_card(boss_card(), &c);
    max = boss_hp();
    ui_clear();
    ui_header(g_qr.kind == EV_RAID ? T_RAID_TITLE : g_qr.kind == EV_PVP ? T_PVP_TITLE : T_BOSS_BATTLE);
    gfx_card2x(boss_card(), BOSS_X, BOSS_Y);
    if (g_qr.kind == EV_PVP) {                     /* 相手は NPC の冒険者 */
        NpcRec n;
        tbl_npc(g_qr.foe, &n);
        t = txt_id(16, 20, (u16)(NPC_PRE_TEXT + n.name_id / NPC_SUF_COUNT), COL_WHITE, COL_BLACK);
        txt_id((u8)t, 20, (u16)(NPC_SUF_TEXT + n.name_id % NPC_SUF_COUNT), COL_WHITE, COL_BLACK);
    } else if (g_qr.kind == EV_RAID) {
        t = txt_id(16, 20, T_RAID_LV, COL_YELLOW, COL_BLACK);
        t = txt_num((u8)t, 20, g_qr.raid_lv, COL_YELLOW, COL_BLACK);
        txt_id((u8)(t + 1), 20, c.name_text, rarity_color(c.rarity), COL_BLACK);
    } else {
        txt_id(16, 20, c.name_text, rarity_color(c.rarity), COL_BLACK);
    }
    left = g_qr.kind == EV_RAID ? g_qr.raid_before : max;
    hp(left, max);
    deck_row(0);
    if (g_qr.status != Q_OK) return;
    ui_wait(30);
    for (t = 0; t < g_qr.b.turns && shown < BATTLE_LOG_MAX; t++, shown++) {
        if (shown == BATTLE_LOG_MAX - 1 && g_qr.kind == EV_HUNT) {   /* 長い戦いは最後の一撃まで飛ばす */
            left = g_qr.b.dmg_per_turn < left && g_qr.b.win ? g_qr.b.dmg_per_turn : left;
            if (!g_qr.b.win) left = max - (u32)(max / 2);
        }
        turn((u8)(t % DECK_SIZE), &left, max);
    }
    if (g_qr.b.win) hp(0, max);
    outcome();
    if (g_qr.event && g_qr.b.win) event_gain_show();
    if (g_qr.levels) ui_msg(T_LEVELUP);
    if (g_qr.area_cleared) ui_msg(T_AREA_CLEAR);
    g_dirty = 0;
}

FAR(void, boss_update, (void))
{
    if (!(input_pressed() & (BTN_OK | BTN_CANCEL))) return;
    if (is_event() || g_qr.event) {
        screen_change(&scr_event);
    } else if (g_qr.area_cleared) {
        g_sel_area++;
        screen_change(&scr_area);
    } else {
        screen_change(&scr_explore);
    }
}

SCREEN(scr_boss, boss_init, boss_update, boss_draw, 0)
