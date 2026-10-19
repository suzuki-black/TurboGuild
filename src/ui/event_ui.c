/* event_ui.c — イベントのトップ（あらすじ・残り日数・ポイント・順位・次の報酬ライン・特効カード）と、
 * 開幕／終了のお知らせ（最終順位と報酬）。面は 1〜3 キーで選び、戦いはボス戦の画面を使う。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../ram.h"
#include "../core/event.h"
#include "../core/quest.h"
#include "../core/ranking.h"
#include "../core/tables.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define BTN_Y 150

static const u8 item_icon[6][2] = {
    { ATL_ITEM_GOLD_X, ATL_ITEM_GOLD_Y }, { ATL_ITEM_POTION_X, ATL_ITEM_POTION_Y },
    { ATL_ITEM_TICKET_X, ATL_ITEM_TICKET_Y }, { ATL_ITEM_GEMS_X, ATL_ITEM_GEMS_Y },
    { ATL_ITEM_SRTICKET_X, ATL_ITEM_SRTICKET_Y }, { ATL_ICON_CARDS_X, ATL_ICON_CARDS_Y },
};

/* 長い文を cols 桁で折り返して lines 行まで描く */
static void wrap(u8 cx, u8 y, u16 id, u8 cols, u8 lines, u8 col)
{
    const char *s = txt_ptr(id);
    char buf[40];
    u8 n, w;
    while (*s && lines--) {
        n = 0; w = 0;
        while (s[n] && w < cols) {
            u8 c = (u8)s[n];
            u8 wide = (u8)((c >= 0x81 && c <= 0x9F) || c >= 0xE0);
            if (w + (wide ? 2 : 1) > cols) break;
            buf[n] = s[n];
            if (wide) { buf[n + 1] = s[n + 1]; n++; }
            n++;
            w = (u8)(w + (wide ? 2 : 1));
        }
        buf[n] = 0;
        txt_draw(cx, y, buf, col, COL_BLACK);
        s += n;
        y = (u8)(y + 16);
        s = txt_ptr(id) + (s - txt_ptr(id));     /* txt_draw で窓が替わっても読み直せるように */
    }
}

static void reward_line(u8 x, u8 y, u8 item, u16 amount)
{
    u8 cx;
    gfx_sprite(item_icon[item][0], item_icon[item][1], 14, 14, x, (u8)(y + 1));
    if (item == ITEM_CARD) {
        CardRec c;
        tbl_card(amount, &c);
        cx = txt_id((u8)(x / 8 + 2), y, c.name_text, rarity_color(c.rarity), COL_BLACK);
    } else {
        cx = txt_id((u8)(x / 8 + 2), y, (u16)(T_ITEM_0 + item), COL_WHITE, COL_BLACK);
        cx = txt_draw(cx, y, "x", COL_LGRAY, COL_BLACK);
        txt_num(cx, y, amount, COL_YELLOW, COL_BLACK);
    }
}

static void stage_btn(u8 level)
{
    EventStage es;
    u8 x = (u8)(4 + (level - 1) * 64), cx;
    event_stage(level, &es);
    ui_box(x, BTN_Y, 62, 40, COL_DRED, COL_ORANGE);
    ui_key(level, (u8)(x + 3), BTN_Y + 4);
    txt_id((u8)(x / 8 + 2), BTN_Y + 2, (u16)(T_EV_LV1 + level - 1), COL_YELLOW, COL_DRED);
    cx = txt_id((u8)(x / 8 + 1), BTN_Y + 20, T_AP, COL_LGRAY, COL_DRED);
    txt_num(cx, BTN_Y + 20, es.ap, COL_WHITE, COL_DRED);
}

FAR(void, evtop_draw, (void))
{
    EventRec e;
    CardRec bonus;
    u8 cx, lv;
    u32 next;
    ui_clear();
    if (!g_save.event_id || g_save.event_id != event_active()) {     /* 終わった（マイページに戻ると終了処理） */
        ui_header(T_EVENT);
        txt_id(2, 80, T_EV_CLOSED, COL_LGRAY, COL_BLACK);
        ui_guide(T_HELP_BACK);
        return;
    }
    tbl_event((u8)g_save.event_id, &e);
    ui_header(e.name_text);
    gfx_card(e.boss_card, 4, 18);
    wrap(8, 18, e.story_text, 24, 2, COL_WHITE);
    cx = txt_id(8, 52, T_EV_LEFT, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), 52, event_days_left(), COL_YELLOW, COL_BLACK);
    txt_id(cx, 52, T_DAYS, COL_LGRAY, COL_BLACK);
    g_rank.mode = RANK_EVENT;
    g_rank.first = 0;
    ranking_list(&g_rank);
    cx = txt_id(8, 70, T_EV_PT, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), 70, g_save.event_pt, COL_YELLOW, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 70, T_EV_RANK, COL_LGRAY, COL_BLACK);
    cx = txt_num((u8)(cx + 1), 70, g_rank.player_rank, COL_WHITE, COL_BLACK);
    txt_id(cx, 70, T_I, COL_LGRAY, COL_BLACK);
    next = event_next_line();
    if (next) {
        cx = txt_id(8, 88, T_EV_NEXT, COL_LGRAY, COL_BLACK);
        txt_num((u8)(cx + 1), 88, next - g_save.event_pt, COL_WHITE, COL_BLACK);
        gfx_gauge(64, 106, 184, (u16)(g_save.event_pt >> 2), (u16)(next >> 2), COL_YELLOW);
    }
    tbl_card(e.bonus_card, &bonus);
    gfx_thumb(e.bonus_card, 4, 112);
    cx = txt_id(5, 118, T_EV_BONUS, COL_RED, COL_BLACK);
    cx = txt_id((u8)(cx + 1), 118, bonus.name_text, rarity_color(bonus.rarity), COL_BLACK);
    cx = txt_draw(cx, 118, "x", COL_LGRAY, COL_BLACK);
    txt_num(cx, 118, e.bonus_mul, COL_YELLOW, COL_BLACK);
    for (lv = 1; lv <= EVENT_LEVELS; lv++) stage_btn(lv);
    ui_box(196, BTN_Y, 58, 40, COL_DBLUE, COL_CYAN);
    ui_key(4, 199, BTN_Y + 4);
    gfx_sprite(ATL_ICON_NEWS, 216, BTN_Y + 12);
    ui_guide(T_EV_GUIDE);
}

FAR(void, evtop_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    if (p & BTN_CANCEL) { screen_change(&scr_home); return; }
    if (!g_save.event_id || g_save.event_id != event_active()) return;
    if (d >= 1 && d <= EVENT_LEVELS) {
        u16 stage = (u16)(EVENT_STAGE_BASE + d);
        if (g_save.ap < quest_ap_cost(stage)) { ui_msg(T_AP_SHORT); return; }
        psg_beep(0x080, 4);
        g_sel_stage = stage;
        screen_change(&scr_boss);
    }
    if (d == 4) screen_change(&scr_ranking);
}

SCREEN(scr_event, 0, evtop_update, evtop_draw, 0)

/* ---- 開幕・終了のお知らせ（マイページに入ったときに出る） ---- */

FAR(void, evres_draw, (void))
{
    EventRec e;
    u8 i, cx;
    ui_clear();
    tbl_event((u8)g_evres.id, &e);
    ui_header(g_evres.ended ? T_EV_END : T_EV_START);
    gfx_card(e.boss_card, 4, 20);
    txt_id(8, 22, e.name_text, COL_YELLOW, COL_BLACK);
    if (g_evres.started) {
        wrap(8, 44, e.story_text, 24, 3, COL_WHITE);
        psg_beep(0x050, 15);
    } else {
        cx = txt_id(8, 44, T_EV_FINAL, COL_LGRAY, COL_BLACK);
        cx = txt_num((u8)(cx + 1), 44, g_evres.rank, COL_YELLOW, COL_BLACK);
        txt_id(cx, 44, T_I, COL_LGRAY, COL_BLACK);
        cx = txt_id(8, 62, T_EV_PT, COL_LGRAY, COL_BLACK);
        txt_num((u8)(cx + 1), 62, g_evres.points, COL_WHITE, COL_BLACK);
        for (i = 0; i < g_evres.gain.n; i++) {
            txt_id(8, 84, T_EV_REWARD, COL_ORANGE, COL_BLACK);
            reward_line(64, (u8)(100 + i * 18), g_evres.gain.item[i], g_evres.gain.amount[i]);
        }
        psg_beep(0x040, 20);
    }
    ui_guide(T_GUIDE_ANY);
    g_evres.started = 0;
    g_evres.ended = 0;
}

FAR(void, evres_update, (void))
{
    if (input_pressed() & (BTN_OK | BTN_CANCEL)) screen_change(&scr_home);
}

SCREEN(scr_evresult, 0, evres_update, evres_draw, 0)

/* ---- イベントのボス戦の後: 稼いだポイントと届いた報酬ライン（ボス戦の画面から呼ぶ） ---- */

FAR(void, event_gain_show, (void))
{
    u8 i, cx;
    ui_box(8, 60, 240, (u8)(40 + g_qr.gain.n * 18), COL_DBLUE, COL_YELLOW);
    cx = txt_id(2, 64, T_EV_PT, COL_LGRAY, COL_DBLUE);
    cx = txt_draw((u8)(cx + 1), 64, "+", COL_LGRAY, COL_DBLUE);
    txt_num(cx, 64, g_qr.points, COL_YELLOW, COL_DBLUE);
    if (g_qr.gain.n) txt_id(2, 82, T_EV_GAINED, COL_ORANGE, COL_DBLUE);
    for (i = 0; i < g_qr.gain.n; i++) {
        vdp_fill(16, (u16)(100 + i * 18), 224, 16, COL_BLACK);
        reward_line(16, (u8)(100 + i * 18), g_qr.gain.item[i], g_qr.gain.amount[i]);
    }
    ui_wait_key();
}
