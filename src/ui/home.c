/* home.c — マイページ。左にリーダーカードの絵、右に状態のゲージ、中段にバナー、
 * 下に数字キーつきのアイコンメニュー（携帯の 1〜8 キーで直接選べる）、最下段に流れるお知らせ。 */
#include "screen.h"
#include "widget.h"
#include "../far.h"
#include "../data/bgm_data.h"
void bgm_play(u8 song);
#include "../ram.h"
#include "../core/rng.h"
#include "../core/login.h"
#include "../core/player.h"
#include "../core/time.h"
#include "../core/wallet.h"
#include "../core/event.h"
#include "../core/guildwar.h"
#include "../core/tables.h"
#include "../core/timeline.h"
#include "../data/screens.h"
#include "../data/text_ids.h"
#include "../sys/gfx.h"
#include "../sys/input.h"
#include "../sys/psg.h"
#include "../sys/text.h"
#include "../sys/vdp.h"

#define ITEMS    8
#define MENU_Y   118
#define CELL_W   64
#define CELL_H   39
#define BANNER_Y 92
#define TICK_Y   ROW_LAST

static const u16 labels[ITEMS] = {
    T_M_QUEST, T_M_GACHA, T_M_CARD, T_M_DECK, T_M_SHOP, T_M_RANK, T_M_SAVE, T_M_NEWS,
};
/* アイコンはアトラスの位置（x, y）。並びは labels と同じ */
static const u8 icons[ITEMS][2] = {
    { ATL_ICON_QUEST_X, ATL_ICON_QUEST_Y }, { ATL_ICON_GACHA_X, ATL_ICON_GACHA_Y },
    { ATL_ICON_CARDS_X, ATL_ICON_CARDS_Y }, { ATL_ICON_DECK_X, ATL_ICON_DECK_Y },
    { ATL_ICON_SHOP_X, ATL_ICON_SHOP_Y },   { ATL_ICON_NEWS_X, ATL_ICON_NEWS_Y },
    { ATL_ICON_SAVE_X, ATL_ICON_SAVE_Y },   { ATL_ICON_LOGIN_X, ATL_ICON_LOGIN_Y },
};

static void cell(u8 i)
{
    u8 x = (u8)((i % 4) * CELL_W), y = (u8)(MENU_Y + (i / 4) * CELL_H);
    u8 on = (u8)(i == g_cursor);
    u8 bg = on ? COL_BLUE : COL_DBLUE;
    u8 w = txt_id_width(labels[i]);
    ui_box(x, y, CELL_W, CELL_H, bg, on ? COL_YELLOW : COL_DGRAY);
    gfx_sprite(icons[i][0], icons[i][1], 24, 24, (u8)(x + 20), (u8)(y + 2));
    ui_key((u8)(i + 1), (u8)(x + 4), (u8)(y + 4));
    txt_id((u8)((x + (CELL_W - w * 8) / 2) / 8), (u8)(y + 22), labels[i], on ? COL_YELLOW : COL_WHITE, bg);
}

static void banner(void);
void gvg_result_show(void);

/* バナーが入口になるもの: イベントかギルド対抗戦（イベントと対抗戦は日が重ならない） */
static u8 has_banner(void) { return (u8)(g_save.event_id || g_save.gvg_war); }

static void redraw_sel(u8 i)
{
    if (i == ITEMS) banner(); else cell(i);
}

static void leader(void)
{
    u8 i = g_save.deck[0];
    ui_box(0, 16, 64, 76, COL_BLACK, COL_DGRAY);
    if (i != DECK_EMPTY && i < g_save.card_count) gfx_card(g_save.cards[i].id, 4, 18);
}

/* 開催中のイベントのバナー（[9] か上で選べる入口）。無ければサービス開始記念の告知 */
static void banner(void)
{
    u8 k, on = (u8)(g_cursor == ITEMS);
    vdp_fill(0, BANNER_Y, SCREEN_W, 26, on ? COL_YELLOW : COL_BLACK);
    for (k = 0; k < 4; k++) vdp_fill(1, (u16)(BANNER_Y + 1 + k * 6), SCREEN_W - 2, 6, (u8)(k & 1 ? COL_RED : COL_DRED));
    if (g_save.event_id) {
        EventRec e;
        u8 cx;
        tbl_event((u8)g_save.event_id, &e);
        ui_key(9, 2, BANNER_Y + 7);
        cx = txt_id(2, BANNER_Y + 4, e.name_text, COL_YELLOW, COL_DRED);
        cx = txt_id((u8)(cx + 1), BANNER_Y + 4, T_EV_LEFT, COL_WHITE, COL_DRED);
        cx = txt_num(cx, BANNER_Y + 4, event_days_left(), COL_YELLOW, COL_DRED);
        txt_id(cx, BANNER_Y + 4, T_DAYS, COL_WHITE, COL_DRED);
    } else if (g_save.gvg_war) {
        GvgNow n;
        u8 cx;
        gvg_now(&n);
        ui_key(9, 2, BANNER_Y + 7);
        cx = txt_id(2, BANNER_Y + 4, T_GVG_TITLE, COL_YELLOW, COL_DRED);
        if (n.now) {
            txt_id((u8)(cx + 1), BANNER_Y + 4, T_GVG_ONAIR, COL_WHITE, COL_DRED);
        } else {
            cx = txt_id((u8)(cx + 1), BANNER_Y + 4, T_GVG_NEXT, COL_WHITE, COL_DRED);
            cx = txt_num(cx, BANNER_Y + 4, n.left_min, COL_YELLOW, COL_DRED);
            txt_id(cx, BANNER_Y + 4, T_GVG_MIN, COL_WHITE, COL_DRED);
        }
    } else {
        TlState tl;
        u16 tx;
        u8 cx;
        timeline_state(&tl);
        gfx_sprite(ATL_ICON_GACHA, 4, BANNER_Y);
        if (tl.announce) {                         /* 終了の告知が最優先 */
            cx = txt_id(4, BANNER_Y + 4, T_END_SOON, COL_YELLOW, COL_DRED);
            cx = txt_num((u8)(cx + 1), BANNER_Y + 4, tl.days_to_end, COL_WHITE, COL_DRED);
            txt_id(cx, BANNER_Y + 4, T_DAYS, COL_YELLOW, COL_DRED);
        } else if (tl.rateup_pct > 100) {
            txt_id(4, BANNER_Y + 4, T_RATEUP_ON, COL_YELLOW, COL_DRED);
        } else if (timeline_news(0, &tx)) {
            txt_id(4, BANNER_Y + 4, tx, COL_YELLOW, COL_DRED);
        }
    }
    gfx_sprite(ATL_SPARK, 244, BANNER_Y + 2);
}

/* お知らせを右から左へ流す（携帯サイトの「マーキー」）。1 回に 1 文字ぶん送り、右端に次の字を足す。
 * 文の終わりのあとは数文字ぶん空けてから先頭に戻る */
void ticker_next(void);

static void ticker(void)
{
    const char *s = g_ticker_buf;
    char ch[3];
    u8 c, w;
    c = (u8)s[g_ticker];
    if (c == 0) {                          /* 1 本流し終えたら少し空けて次の文へ */
        if (++g_ticker_gap < 6) { c = ' '; } else { ticker_next(); g_ticker = 0; g_ticker_gap = 0; c = (u8)s[0]; }
    }
    w = (c >= 0x81 && c <= 0x9F) || c >= 0xE0 ? 2 : 1;
    gfx_move((u8)(w * 8), TICK_Y, (u8)(SCREEN_W - w * 8), 16, 0, TICK_Y);
    ch[0] = (char)c;
    ch[1] = w == 2 ? s[g_ticker + 1] : 0;
    ch[2] = 0;
    txt_draw((u8)(32 - w), TICK_Y, ch, COL_YELLOW, COL_DGRAY);
    if (c != ' ' || g_ticker_gap == 0) g_ticker = (u8)(g_ticker + w);
}

static void show_login_bonus(void)
{
    screen_change(&scr_login);
}

FAR(void, home_init, (void))
{
    g_cursor = g_menu_pos;
    bgm_play(BGM_HOME);
    player_ap_update();
    login_check(&g_login);
    g_paid = wallet_payday();
    event_update(&g_evres);
    gvg_update(&g_gvgres);
    {
        TlResult tr;
        TlState tl;
        timeline_update(&tr);
        g_gift = tr.gift_gems;
        if (tr.n_news) g_save_dirty = 1;
        timeline_state(&tl);
        if (tl.ended) { g_save_dirty = 1; screen_change(&scr_title); }   /* サービス終了 */
    }
    if (!g_save.guild) {                             /* 最初に来たとき、どこかのギルドに入る */
        g_save.guild = (u8)(1 + rng_range(&g_save.rng_sim, GUILD_COUNT));
        g_joined = 1;
    }
    if (g_login.given || g_paid || g_evres.started || g_evres.ended || g_gift || g_joined ||
        g_gvgres.joined || g_gvgres.n || g_gvgres.ended) g_save_dirty = 1;
    if (g_cursor == ITEMS && !has_banner()) g_cursor = 0;
    /* ★起動直後（タイトルから来たとき）は保存を後回しにして、すぐ遊べるようにする（Phase3 の受け入れ: 3 秒）。
     * イベントの終了・給料日・年表は日付から決まるので、保存前に電源を切っても次の起動で同じ結果になる */
    if (g_save_dirty && !g_from_title) ui_saving();
    g_from_title = 0;
}

FAR(void, home_draw, (void))
{
    u8 i, cx;
    ui_clear();
    ui_header(T_MYPAGE);
    cx = txt_num(24, 0, time_gday(g_save.g_time) + 1, COL_YELLOW, COL_DBLUE);
    txt_id(cx, 0, T_DAY, COL_WHITE, COL_DBLUE);
    leader();
    ui_status_block(20);
    banner();
    for (i = 0; i < ITEMS; i++) cell(i);
    vdp_fill(0, TICK_Y, SCREEN_W, 16, COL_DGRAY);
    if (!g_ticker_buf[0]) ticker_next();
    g_ticker = 0;
    g_ticker_gap = 0;
    if (g_paid) { g_paid = 0; ui_msg(T_PAYDAY); return; }
    if (g_gift) { g_gift = 0; ui_msg(T_GIFT_GOT); return; }
    if (g_joined) {
        g_joined = 0;
        ui_box(16, 80, 224, 44, COL_DBLUE, COL_YELLOW);
        cx = txt_id(3, 84, T_GUILD, COL_LGRAY, COL_DBLUE);
        txt_id((u8)(cx + 1), 84, (u16)(GUILD_TEXT + g_save.guild - 1), COL_YELLOW, COL_DBLUE);
        txt_id(3, 102, T_GUILD_JOINED, COL_WHITE, COL_DBLUE);
        psg_beep(0x060, 10);
        ui_wait_key();
        g_dirty = 1;
        return;
    }
    if (g_evres.started || g_evres.ended) { screen_change(&scr_evresult); return; }
    if (g_gvgres.joined || g_gvgres.n || g_gvgres.ended) { gvg_result_show(); return; }
    if (g_login.given) show_login_bonus();
}

static void open(u8 i)
{
    g_menu_pos = i;
    psg_beep(0x80, 4);
    if (g_save_dirty && i != 6) ui_saving();          /* 後回しにした保存はここで（6 = 保存は自分でする） */
    switch (i) {
    case 0: screen_change(&scr_area); break;
    case 1: screen_change(&scr_gacha); break;
    case 2: screen_change(&scr_cardlist); break;
    case 3: screen_change(&scr_deck); break;
    case 4: screen_change(&scr_shop); break;
    case 5: screen_change(&scr_ranking); break;
    case 6:
        g_save_dirty = 1;
        ui_saving();
        if (!g_save_dirty) ui_msg(T_SAVED);
        break;
    case ITEMS: screen_change(g_save.event_id ? &scr_event : &scr_guildwar); break;
    default: screen_change(&scr_news); break;
    }
}

FAR(void, home_update, (void))
{
    u8 p = input_pressed();
    u8 d = input_digit();
    u8 old = g_cursor;
    if (g_cursor == ITEMS) {                       /* バナーから下へ戻る */
        if (p & (BTN_DOWN | BTN_LEFT | BTN_RIGHT)) g_cursor = 0;
    } else {
        if (p & BTN_LEFT)  g_cursor = (u8)((g_cursor + ITEMS - 1) % ITEMS);
        if (p & BTN_RIGHT) g_cursor = (u8)((g_cursor + 1) % ITEMS);
        if (p & BTN_DOWN) g_cursor = (u8)((g_cursor + 4) % ITEMS);
        if (p & BTN_UP) g_cursor = (u8)(g_cursor < 4 && has_banner() ? ITEMS : (g_cursor + 4) % ITEMS);
    }
    if (old != g_cursor) { psg_beep(0x100, 2); redraw_sel(old); redraw_sel(g_cursor); }
    if (d == 9 && has_banner()) { g_cursor = ITEMS; open(ITEMS); return; }
    if (d >= 1 && d <= ITEMS) { g_cursor = (u8)(d - 1); redraw_sel(old); redraw_sel(g_cursor); open(g_cursor); return; }
    if (p & BTN_OK) { open(g_cursor); return; }
    if (p & BTN_CANCEL) {
        if (ui_confirm(T_TO_TITLE)) screen_change(&scr_title);
        return;
    }
    if ((g_frames & 15) == 0) ticker();
    if (g_frames == 0) {           /* 1 秒に 1 回: AP の回復を反映 */
        u8 ap = g_save.ap;
        player_ap_update();
        if (g_save.ap != ap) ui_status_block(20);
    }
}

SCREEN(scr_home, home_init, home_update, home_draw, 0)
