/* SPDX-License-Identifier: MIT */
/* test_game.c — 戦闘・AP・クエスト・ガチャ・ログインボーナス・デッキ（表は build/ の .bin の実物） */
#include "check.h"
#include <string.h>
#include "core/battle.h"
#include "core/deck.h"
#include "core/opsim.h"
#include "core/gacha.h"
#include "core/inventory.h"
#include "core/login.h"
#include "core/player.h"
#include "core/quest.h"
#include "core/save.h"
#include "core/tables.h"
#include "core/time.h"
#include "core/wallet.h"
#include "core/fusion.h"
#include "core/event.h"
#include "core/evfight.h"
#include "core/guildwar.h"
#include "core/ranking.h"
#include "core/timeline.h"
#include "core/board.h"
#include "core/ending.h"
#include "core/memo.h"
#include "core/steal.h"
#include "core/pev.h"
#include "data/text_ids.h"


static void fresh(void) { save_new(&g_save, 1000); }

static void test_struct_sizes(void)
{
    /* 生成ヘッダは PC でも詰めて並べる（ROM のバイナリをそのまま読むため） */
    CHECK(sizeof(CardRec) == 22);
    CHECK(sizeof(StageRec) == 15);
    CHECK(sizeof(GachaRec) == 18);
}

static void test_battle(void)
{
    BattleIn in = { 1000, 500, 300, 3990, 100, 300 };
    BattleOut o;
    battle_run(&in, &o);
    CHECK(o.dmg_per_turn == 900 && o.turns == 5 && o.taken_total == 0 && o.win);
    in.enemy_atk = 600;                       /* 毎ターン 100 × 5 = 500 >= 300 → 負け */
    battle_run(&in, &o);
    CHECK(o.taken_total == 500 && !o.win);
    in.deck_atk = 50;                         /* 防御を抜けなくても最低 1 */
    in.enemy_atk = 0;
    battle_run(&in, &o);
    CHECK(o.dmg_per_turn == 1 && o.turns == 3990 && o.win);
}

static void test_elem(void)
{
    CHECK(elem_pct(0, 2) == 120);   /* 炎 > 雷 */
    CHECK(elem_pct(2, 1) == 120);   /* 雷 > 水 */
    CHECK(elem_pct(1, 0) == 120);   /* 水 > 炎 */
    CHECK(elem_pct(2, 0) == 80);    /* 雷 < 炎 */
    CHECK(elem_pct(3, 4) == 120 && elem_pct(4, 3) == 120);
    CHECK(elem_pct(3, 3) == 100 && elem_pct(0, 3) == 100);
    CHECK(elem_pct(0, ELEM_NONE) == 100);
    CHECK(elem_weak(0) == 1 && elem_weak(1) == 2 && elem_weak(2) == 0);
    CHECK(elem_weak(3) == 4 && elem_weak(4) == 3 && elem_weak(ELEM_NONE) == ELEM_NONE);
}

/* 相性込みのおまかせ編成: 水 N（攻撃 130）と雷 N（140） */
static void test_auto_for(void)
{
    DeckStats d;
    fresh();
    inv_add(6); inv_add(77);
    deck_auto();
    CHECK(g_save.cards[g_save.deck[0]].id == 77);       /* 属性なしなら攻撃の高い雷 */
    deck_stats_init(&d, 0);
    deck_auto_for(&d);
    CHECK(g_save.cards[g_save.deck[0]].id == 6);        /* 炎の敵には水（130×1.2 > 140×0.8） */
    deck_stats_init(&d, 1);
    deck_auto_for(&d);
    CHECK(g_save.cards[g_save.deck[0]].id == 77);       /* 水の敵には雷 */
    deck_stats_init(&d, ELEM_NONE);
    d.bonus_card = 6; d.bonus_mul = 5;
    deck_auto_for(&d);
    CHECK(g_save.cards[g_save.deck[0]].id == 6);        /* 特効も込みで比べる */
    CHECK(g_save.deck[1] != DECK_EMPTY);
}

/* リーダー（1 枚目）と同じ属性の攻撃 +20%、戦い方（節約・全力）、デッキ 3 つ（DECISIONS #87） */
static void test_stance_leader_slots(void)
{
    DeckStats d;
    CardRec a, b;
    QuestResult q;
    StageRec st;
    u32 base, va, vb;
    u16 ia, ib, ic;
    u8 ap;
    fresh();
    ia = inv_add(6); ib = inv_add(77); ic = inv_add(39);
    tbl_card(6, &a); tbl_card(77, &b);
    CHECK(a.element != b.element);
    va = card_atk_of(ia); vb = card_atk_of(ib);
    g_save.deck[0] = (u8)ia; g_save.deck[1] = (u8)ib;
    deck_stats_init(&d, ELEM_NONE);
    deck_calc(&d);
    CHECK(d.lead_elem == a.element);
    CHECK(d.atk + 2 >= va * (100 + LEADER_PCT) / 100 + vb && d.atk <= va * (100 + LEADER_PCT) / 100 + vb + 2);
    g_save.deck[0] = (u8)ib; g_save.deck[1] = (u8)ia;   /* リーダーを替えると、かかるカードも替わる */
    deck_calc(&d);
    CHECK(d.lead_elem == b.element);
    CHECK(d.atk + 2 >= vb * (100 + LEADER_PCT) / 100 + va && d.atk <= vb * (100 + LEADER_PCT) / 100 + va + 2);
    g_save.deck[0] = DECK_EMPTY; g_save.deck[1] = (u8)ia;
    deck_calc(&d);
    CHECK(d.lead_elem == ELEM_NONE && d.atk == va);      /* 1 枚目が空ならリーダーはいない */
    /* 戦い方: 攻撃は 60% / 250%、総戦力（ranking）は通常のまま */
    g_save.deck[0] = (u8)ia; g_save.deck[1] = (u8)ib;
    deck_calc(&d);
    base = d.atk;
    d.stance = STANCE_SAVE; deck_calc(&d);
    CHECK(d.atk == base * STANCE_SAVE_ATK / 100);
    d.stance = STANCE_ALL; deck_calc(&d);
    CHECK(d.atk == base * STANCE_ALL_ATK / 100);
    g_save.stance = STANCE_ALL;
    CHECK(player_power() == base + d.def);
    /* AP: 節約は半分（切り上げ）、全力は 2 倍。報酬は変わらない */
    tbl_stage(1, &st);
    g_save.stance = STANCE_NORMAL; ap = quest_ap_cost(1);
    CHECK(ap == st.ap);
    g_save.stance = STANCE_SAVE;
    CHECK(quest_ap_cost(1) == (st.ap + 1) / 2);
    g_save.stance = STANCE_ALL;
    CHECK(quest_ap_cost(1) == st.ap * 2);
    g_save.ap = AP_MAX; g_save.ap_gtime = g_save.g_time;
    g_save.stance = STANCE_SAVE;
    quest_run(1, &q);
    CHECK(q.status == Q_OK && q.ap_used == (st.ap + 1) / 2 && g_save.ap == AP_MAX - q.ap_used);
    CHECK(!q.b.win || q.exp == st.exp);
    g_save.stance = STANCE_NORMAL;
    /* デッキ 3 つ: 替えると空のデッキになり、戻すと元のデッキ */
    CHECK(deck_slot_count(0) == 2 && deck_slot_count(1) == 0);
    deck_switch(1);
    CHECK(g_save.deck_sel == 1 && g_save.deck[0] == DECK_EMPTY && deck_slot_count(0) == 2);
    g_save.deck[0] = (u8)ic;
    CHECK(inv_in_deck(ia) && inv_in_deck(ib) && inv_in_deck(ic));   /* 控えのカードも素材にしない */
    inv_remove(0);                                     /* 控えのデッキの 1 枚目（6）を抜くと空き、ほかは詰める */
    CHECK(g_save.deck[0] == ic - 1);
    deck_switch(0);
    CHECK(g_save.deck_sel == 0 && g_save.deck[0] == DECK_EMPTY && g_save.deck[1] == ib - 1);
    CHECK(deck_slot_count(1) == 1);
    deck_switch(1);
    CHECK(g_save.cards[g_save.deck[0]].id == 39);
    g_save.deck_sel = 7;                               /* 壊れたセーブの値でも配列の外に書かない */
    g_save.deck_slot[2][0] = 250;                      /* 所持数を超える添字は空きにする */
    deck_switch(2);
    CHECK(g_save.deck_sel == 2 && g_save.deck[0] == DECK_EMPTY);
    deck_switch(3);
    CHECK(g_save.deck_sel == 2);
}

static void test_ap(void)
{
    fresh();
    g_save.ap = 10;
    g_save.ap_gtime = 0;
    g_save.g_time = AP_RECOVER_GSEC * 3 + 5;   /* 3 回復 + 端数 */
    player_ap_update();
    CHECK(g_save.ap == 13 && g_save.ap_gtime == AP_RECOVER_GSEC * 3);
    g_save.g_time += AP_RECOVER_GSEC * 1000;
    player_ap_update();
    CHECK(g_save.ap == AP_MAX);
    /* レベルアップで全回復 */
    g_save.ap = 0;
    CHECK(player_add_exp(EXP_NEED(1)) == 1 && g_save.level == 2 && g_save.ap == AP_MAX);
    CHECK(player_add_exp(EXP_NEED(2) + EXP_NEED(3)) == 2 && g_save.level == 4 && g_save.exp == 0);
}

static void test_inventory(void)
{
    u16 i;
    fresh();
    for (i = 0; i < CARD_MAX; i++) CHECK(inv_add(1) == i);
    CHECK(inv_add(1) == INV_FULL);
    g_save.deck[0] = 5; g_save.deck[1] = 10; g_save.deck[2] = 199;
    inv_remove(5);
    CHECK(g_save.deck[0] == DECK_EMPTY && g_save.deck[1] == 9 && g_save.deck[2] == 198);
    CHECK(g_save.card_count == CARD_MAX - 1 && inv_in_deck(9) == 2);
}

static void test_quest(void)
{
    QuestResult r;
    u16 n;
    fresh();
    inv_add(39); inv_add(40); inv_add(33);    /* SSR 2 枚 + SR */
    deck_auto();
    CHECK(g_save.deck[0] != DECK_EMPTY);
    quest_run(2, &r);
    CHECK(r.status == Q_LOCKED);              /* まだ 1 しか挑戦できない */
    quest_run(1, &r);
    CHECK(r.status == Q_OK && r.b.win && g_save.stage_cleared == 1);
    CHECK(r.ap_used == 3 && g_save.ap == AP_MAX - 3);
    CHECK(r.gold == 20 && g_save.gold == START_GOLD + 20);
    /* AP 切れ */
    g_save.ap = 2;
    quest_run(2, &r);
    CHECK(r.status == Q_NO_AP && g_save.ap == 2 && g_save.stage_cleared == 1);
    /* エリア 1 のボスまで進むとエリアクリア、ボスは R を 100% 落とす */
    g_save.ap = AP_MAX;
    for (n = 2; n <= 10; n++) { g_save.ap = AP_MAX; quest_run(n, &r); CHECK(r.b.win); }
    { CardRec c; CHECK(r.area_cleared && r.drop_card); tbl_card(r.drop_card, &c); CHECK(c.rarity == RARITY_R); }
    CHECK(quest_max_stage() == 11);
}

static void test_box(void);

static void test_gacha(void)
{
    GachaReq g;
    long hist[RARITIES] = {0};
    CardRec c;
    int i, j;
    fresh();
    g_save.gems = 1000000;
    /* プレミア 1 回 × 20000: SSR 1% / SR 5% / R 20% / N 74%（天井で SSR が少し増える） */
    for (i = 0; i < 20000; i++) {
        g.gacha = 3; g.count = 1;
        gacha_roll(&g);
        tbl_card(g.ids[0], &c);
        hist[c.rarity]++;
    }
    CHECK(hist[3] > 150 && hist[3] < 350);
    CHECK(hist[2] > 800 && hist[2] < 1200);
    CHECK(hist[0] > 14000 && hist[0] < 15600);
    /* 10 連は SR 以上が必ず 1 枚 */
    for (i = 0; i < 2000; i++) {
        int best = 0;
        g.gacha = 1; g.count = 10;
        gacha_roll(&g);
        for (j = 0; j < 10; j++) { tbl_card(g.ids[j], &c); if (c.rarity > best) best = c.rarity; }
        CHECK(best >= RARITY_SR);
    }
    /* 天井は無い（当時のソシャゲに合わせた）: 何連しても天井で確定しない */
    g_save.rng_gacha = 12345;
    for (i = 0; i < 300; i++) {
        g.gacha = 3; g.count = 1;
        gacha_roll(&g);
        CHECK(!g.pity_hit);
    }
    /* 支払いと所持 */
    fresh();
    g.gacha = 3; g.count = 10;
    gacha_pull(&g);
    CHECK(g.status == G_OK && g.got == 10 && g_save.card_count == 10 && g_save.gems == START_GEMS - 2700);
    g.gacha = 2; g.count = 10;
    gacha_pull(&g);
    CHECK(g.status == G_UNAVAILABLE);         /* ゴールドに 10 連は無い */
    g.gacha = 1; g.count = 1;
    gacha_pull(&g);
    CHECK(g.status == G_OK);
    gacha_pull(&g);
    CHECK(g.status == G_FREE_USED);           /* 同じゲーム日にもう 1 回は不可 */
    g_save.g_time += GSEC_PER_DAY;
    gacha_pull(&g);
    CHECK(g.status == G_OK);
    g.gacha = 6; g.count = 1;
    gacha_pull(&g);
    CHECK(g.status == G_NO_MONEY);            /* SR 確定チケットが無い */
    g_save.sr_tickets = 1;
    gacha_pull(&g);
    tbl_card(g.ids[0], &c);
    CHECK(g.status == G_OK && c.rarity == RARITY_SR && g_save.sr_tickets == 0);   /* SR 確定は SR だけ */
    test_box();
}

/* BOX ガチャ: 100 個入りの箱から引き切ると中身どおり（N60 R30 SR9 SSR1）。SSR はイベント限定 */
static void test_box(void)
{
    GachaReq g;
    CardRec c;
    u16 hist[4] = { 0, 0, 0, 0 }, ssr = 0;
    int i, j;
    fresh();
    g.gacha = 4; g.count = 1;
    CHECK(gacha_can_pay(&g) == G_UNAVAILABLE);   /* イベントが無いときは引けない */
    g_save.g_time = GSEC_PER_DAY * 5;
    g_save.event_id = 1;
    gacha_box_sync(4);
    CHECK(g_save.box_no == 1 && g_save.box_left[RARITY_SSR] == 1 && g_save.box_left[RARITY_N] == 60);
    for (i = 0; i < 10; i++) {
        g.gacha = 4; g.count = 10;
        gacha_roll(&g);
        for (j = 0; j < 10; j++) {
            tbl_card(g.ids[j], &c);
            hist[c.rarity]++;
            if (c.rarity == RARITY_SSR) ssr = g.ids[j];
        }
    }
    CHECK(hist[0] == 60 && hist[1] == 30 && hist[2] == 9 && hist[3] == 1);
    CHECK(ssr == 43);                           /* 第 1 回の限定 SSR「宵祭りの鬼神」 */
    CHECK(g_save.box_no == 1 && !(g_save.box_left[0] | g_save.box_left[1] | g_save.box_left[2] | g_save.box_left[3]));
    g.gacha = 4; g.count = 1;
    gacha_roll(&g);
    CHECK(g_save.box_no == 2 && g_save.box_left[RARITY_N] + g_save.box_left[RARITY_R] + g_save.box_left[RARITY_SR]
          + g_save.box_left[RARITY_SSR] == 99);  /* 空になったら次の箱 */
    g_save.event_id = 2;                        /* 別のイベントになったら 1 箱目から */
    gacha_box_sync(4);
    CHECK(g_save.box_no == 1 && g_save.box_event == 2 && g_save.box_left[RARITY_SSR] == 1);
}

static void test_login(void)
{
    LoginResult r;
    int d;
    fresh();
    login_check(&r);
    CHECK(r.given && r.day == 1 && r.item == ITEM_GOLD && g_save.gold == START_GOLD + 1000);
    login_check(&r);
    CHECK(!r.given);                           /* 同じ日は 1 回 */
    for (d = 2; d <= 8; d++) {
        g_save.g_time += GSEC_PER_DAY;
        login_check(&r);
        CHECK(r.given && r.day == (d - 1) % 7 + 1);
    }
    CHECK(g_save.sr_tickets == 1 && g_save.potions == 4 && g_save.gacha_tickets == 1);
    g_save.g_time += GSEC_PER_DAY * 30;        /* 何日空いても 1 回分 */
    login_check(&r);
    CHECK(r.given && r.day == 2);
}

static void test_wallet(void)
{
    fresh();
    wallet_start(1);                           /* おこづかい: 3000 円, 月 3000 円 */
    CHECK(g_save.funds == 3000 && !wallet_infinite());
    CHECK(wallet_buy_gems(2) == W_OK && g_save.funds == 2000 && g_save.gems == START_GEMS + 1000);
    CHECK(wallet_buy_gems(3) == W_NO_FUNDS && g_save.funds == 2000);   /* 1 万円は払えない */
    CHECK(g_save.spent == 1000);
    CHECK(wallet_payday() == 0);
    g_save.g_time += GSEC_PER_DAY * 30;        /* 1 か月後: 給料日 */
    CHECK(wallet_payday() == 1 && g_save.funds == 5000);
    CHECK(wallet_payday() == 0);
    g_save.g_time += GSEC_PER_DAY * 90;        /* 3 か月放置しても 3 回分 */
    CHECK(wallet_payday() == 3 && g_save.funds == 14000);
    wallet_start(4);                           /* 無敵モード: 減らない、累計は数える */
    CHECK(wallet_infinite());
    CHECK(wallet_buy_gems(3) == W_OK && wallet_buy_gems(3) == W_OK && g_save.funds == 0);
    CHECK(g_save.spent == 20000 && g_save.gems >= 20000);
}

static void test_fusion(void)
{
    FuseReq f;
    u16 i;
    fresh();
    for (i = 0; i < 6; i++) inv_add(2);        /* N かまど番の少年 × 6 */
    inv_add(21);                                /* R 紅蓮の槍騎士 */
    memset(&f, 0, sizeof f);
    f.base = 6;                                 /* R をベースに */
    fusion_preview(&f);
    CHECK(f.status == F_NONE);
    fusion_select_all_n(&f);
    fusion_preview(&f);
    CHECK(f.count == 6 && f.gain == 60 && f.gold == 600 && f.status == F_OK);
    /* R の Lv1→2 は 1*20*2=40、Lv2→3 は 80。60 なら Lv2 で余り 20 */
    CHECK(f.lv_after == 2);
    fusion_apply(&f);
    CHECK(g_save.card_count == 1 && f.base == 0);
    CHECK(g_save.cards[0].id == 21 && g_save.cards[0].lv == 2 && g_save.cards[0].exp == 20);
    CHECK(g_save.gold == START_GOLD - 600);
    /* 同じレアリティは +50%: R を素材に R へ = 75 */
    inv_add(22);
    memset(&f, 0, sizeof f);
    f.base = 0;
    SEL_SET(f.sel, 1);
    fusion_preview(&f);
    CHECK(f.gain == 75);
    /* デッキのカードは素材にできない */
    g_save.deck[0] = 1;
    fusion_preview(&f);
    CHECK(f.status == F_BAD);
    g_save.deck[0] = DECK_EMPTY;
    /* 進化: 同じ id の 2 枚（evo_to あり）→ 進化先 Lv1 */
    fresh();
    inv_add(1); inv_add(5); inv_add(1);
    CHECK(!evolve_partner_ok(0, 1));            /* 違うカード */
    CHECK(evolve_partner_ok(0, 2));
    CHECK(evolve(0, 2) == F_OK && g_save.cards[0].id == 44 && g_save.cards[0].lv == 1 && g_save.card_count == 2);
    CHECK(evolve(1, 0) == F_NOT_EVO);
}

static void test_event(void)
{
    EventResult r;
    RewardList g;
    QuestResult q;
    u16 rank_mid;
    fresh();
    g_save.g_time = GSEC_PER_DAY * 2;           /* 2 日目: まだ */
    event_update(&r);
    CHECK(!r.started && event_active() == 0);
    g_save.g_time = GSEC_PER_DAY * 3;           /* 3 日目: 開幕 → 自動で参加 */
    event_update(&r);
    CHECK(r.started && g_save.event_id == 1 && event_days_left() == 14);
    /* ポイントの報酬ライン（100: 回復薬 2, 500: 石 300） */
    memset(&g, 0, sizeof g);
    event_add_points(120, &g);
    CHECK(g.n == 1 && g.item[0] == ITEM_POTION && g_save.potions == 2);
    memset(&g, 0, sizeof g);
    event_add_points(400, &g);                  /* 520 点: 300 の石 300 と 450 の SR 確定チケット */
    CHECK(g.n == 2 && g.item[0] == ITEM_GEMS && g.item[1] == ITEM_SRTICKET && event_next_line() == 1500);
    memset(&g, 0, sizeof g);
    event_add_points(10, &g);
    CHECK(g.n == 0);                            /* 同じラインは 2 度出ない */
    g_save.sr_tickets = 0;
    /* イベントの面は特効が効く: 特効カード（41）だけのデッキで初級に勝てる */
    inv_add(41);
    g_save.deck[0] = (u8)(g_save.card_count - 1);
    quest_run(EVENT_STAGE_BASE + 1, &q);
    CHECK(q.status == Q_OK && q.event && q.b.win && q.points == 10 && g_save.event_pt == 540);
    /* 期間中の順位は NPC が伸びるにつれて下がる */
    g_save.g_time = GSEC_PER_DAY * 4;
    { RankList l; l.mode = RANK_EVENT; l.first = 0; ranking_list(&l); rank_mid = l.player_rank; }
    g_save.g_time = GSEC_PER_DAY * 16;
    { RankList l; l.mode = RANK_EVENT; l.first = 0; ranking_list(&l); CHECK(l.player_rank > rank_mid); }
    /* ★期間をまたいだセーブ: 終わった後の日付で読み込むと、終了処理と順位報酬が走る */
    g_save.g_time = GSEC_PER_DAY * 40;
    {
        u32 gems = g_save.gems;
        u16 expect = ranking_final_rank();      /* 終了処理の前（参加中の id で NPC の点を数える） */
        event_update(&r);
        CHECK(r.ended && r.id == 1 && r.points == 540 && r.rank == expect);
        CHECK(r.gain.n == 1 && g_save.gems == gems + 1000);  /* 1000 位以内: 石 1000 */
        CHECK(g_save.event_id == 0 && g_save.last_event == 1 && g_save.last_rank == r.rank);
    }
    event_update(&r);
    CHECK(!r.ended && !r.started);              /* 2 度目は何も起きない */
    quest_run(EVENT_STAGE_BASE + 1, &q);
    CHECK(q.status == Q_LOCKED);                /* 終わったイベントの面には入れない */
}

/* レイド（第 2 回, 20 日目から）: HP は倒すまで残る。倒すと Lv が上がる。救援はボス 1 体に 1 回 */
static void test_raid(void)
{
    EventResult r;
    QuestResult q;
    RescueResult rs;
    u32 hp0;
    u16 n;
    fresh();
    g_save.g_time = GSEC_PER_DAY * 21;
    event_update(&r);
    CHECK(r.started && g_save.event_id == 2 && event_kind() == EV_RAID);
    inv_add(46);                                /* 特効の「浜辺の槍使い」 */
    g_save.deck[0] = (u8)(g_save.card_count - 1);
    g_save.ap = AP_MAX;
    quest_run(EVENT_STAGE_BASE + 1, &q);
    CHECK(q.status == Q_OK && q.kind == EV_RAID && q.raid_lv == 1 && q.points >= 10);
    CHECK(q.raid_max == raid_max_hp(1) && g_save.raid_hp < q.raid_before && !q.b.win);
    CHECK(q.b.turns >= 1 && q.b.turns <= RAID_TURNS);
    hp0 = q.raid_before - g_save.raid_hp;       /* 通常攻撃で削った分 */
    quest_run(EVENT_STAGE_BASE + 2, &q);        /* 全力攻撃は通常より削る（AP 15） */
    CHECK(q.status == Q_OK && q.ap_used == 15 && q.raid_before - g_save.raid_hp > hp0);
    /* 救援: ギルドに入っていれば仲間が来る。2 度目は来ない */
    g_save.guild = 3;
    raid_rescue(&rs);
    CHECK(rs.n >= 1 && g_save.raid_rescued);
    for (n = 0; n < rs.n; n++) { NpcRec x; tbl_npc(rs.npc[n], &x); CHECK(x.guild == 2 && rs.dmg[n] > 0); }
    raid_rescue(&rs);
    CHECK(rs.n == 0);
    /* 倒すまで殴ると Lv2 のボスになり、討伐ボーナスが入る */
    for (n = 0; n < 200 && g_save.raid_lv == 1; n++) { g_save.ap = AP_MAX; quest_run(EVENT_STAGE_BASE + 2, &q); }
    CHECK(g_save.raid_lv == 2 && q.b.win && q.points == 30 + 5 && g_save.raid_hp == raid_max_hp(2));
    CHECK(raid_max_hp(2) > raid_max_hp(1) && !g_save.raid_rescued);
}

/* バトル（第 3 回, 45 日目から）: 格下にはほぼ勝ち、格上にはほぼ負ける。負けても少しポイント */
static void test_pvp(void)
{
    EventResult r;
    QuestResult q;
    PvpFoe f1, f3;
    u16 n, win1 = 0, win3 = 0;
    fresh();
    g_save.g_time = GSEC_PER_DAY * 46;
    event_update(&r);
    CHECK(r.started && g_save.event_id == 3 && event_kind() == EV_PVP);
    inv_add(33);
    g_save.deck[0] = (u8)(g_save.card_count - 1);
    pvp_foe(1, &f1);
    pvp_foe(3, &f3);
    CHECK(f1.power < f3.power && f1.npc != f3.npc);
    for (n = 0; n < 200; n++) {
        g_save.ap = AP_MAX;
        quest_run(EVENT_STAGE_BASE + 1, &q);
        CHECK(q.status == Q_OK && q.kind == EV_PVP && q.points == (q.b.win ? 30 : 6));
        win1 += q.b.win;
        g_save.ap = AP_MAX;
        quest_run(EVENT_STAGE_BASE + 3, &q);
        CHECK(q.points == (q.b.win ? 160 : 32));
        win3 += q.b.win;
    }
    CHECK(win1 > 170 && win3 > 20 && win3 < 90);
    pvp_foe(1, &f3);
    CHECK(f3.npc != f1.npc);                    /* 戦うと顔ぶれが替わる */
}

/* ギルド対抗戦（第 1 回, 140 日目から 7 日・14 戦）: 参戦 → 戦の時間に攻撃と応援 → 結果 → 順位と報酬 */
static void test_guildwar(void)
{
    GvgResult r;
    GvgNow n;
    GvgAttack a;
    u32 gems;
    fresh();
    inv_add(33);
    g_save.deck[0] = (u8)(g_save.card_count - 1);
    gvg_update(&r);
    CHECK(!r.joined);                           /* ギルドに入っていない */
    g_save.guild = 5;
    g_save.g_time = GSEC_PER_DAY * 139 + 600;
    gvg_update(&r);
    CHECK(!r.joined && !g_save.gvg_war);
    g_save.g_time = GSEC_PER_DAY * 140 + 600;
    gvg_update(&r);
    CHECK(r.joined && g_save.gvg_war == 1 && g_save.gvg_start == 140 && r.n == 0);
    gvg_now(&n);
    CHECK(n.war == 1 && !n.now && n.m == 0 && n.left_min == (43200 - 600) / 60 && n.foe != 4 && n.foe < 16);
    gvg_attack(1, &a);
    CHECK(a.status == 1);                       /* 戦の時間外 */
    g_save.g_time = GSEC_PER_DAY * 140 + 43200 + 1800;   /* 12:30 */
    gvg_now(&n);
    CHECK(n.now && n.m == 0 && n.left_min == 30 && n.us > 0 && n.them > 0 && n.combo >= 1);
    g_save.ap = AP_MAX;
    gvg_attack(1, &a);
    CHECK(a.status == 0 && a.pts > 0 && g_save.gvg_pts == a.pts && g_save.gvg_m == 0 && g_save.ap == AP_MAX - GVG_AP);
    { NpcRec x; tbl_npc(a.npc, &x); CHECK(x.guild == n.foe); }
    CHECK(gvg_target(1) != gvg_target(2) && gvg_target(2) != gvg_target(3));
    CHECK(gvg_cheer() && !gvg_cheer());         /* 応援は 1 戦に 1 回 */
    g_save.ap = 5;
    gvg_attack(2, &a);
    CHECK(a.status == 2);                       /* AP 不足 */
    g_save.g_time = GSEC_PER_DAY * 141 + 600;
    gvg_update(&r);
    CHECK(r.n == 2 && r.win <= 2 && g_save.gvg_win + g_save.gvg_lose == 2 && g_save.gvg_done == 3);
    gems = g_save.gems;
    g_save.g_time = GSEC_PER_DAY * 147 + 600;   /* 7 日目の夜の戦まで全部終わった */
    gvg_update(&r);
    CHECK(r.n == 12 && r.ended && r.rank >= 1 && r.rank <= 16 && g_save.gvg_war == 0 && g_save.gvg_last_war == 1);
    CHECK(g_save.gvg_win + g_save.gvg_lose == 14 && r.gain.n >= 1);
    CHECK(g_save.gems >= gems + (u32)r.win * GVG_WIN_GEMS);
    gvg_update(&r);
    CHECK(!r.joined && !r.n && !r.ended);       /* 同じ回には入り直さない */
}

static void test_ranking(void)
{
    RankList l;
    NpcRec a, b;
    u8 i;
    fresh();
    tbl_npc(0, &a); tbl_npc(999, &b);
    CHECK(a.event_pt >= b.event_pt);            /* 表は大きい順 */
    g_save.event_id = 1;
    g_save.event_pt = 0xFFFFFFFFUL;
    CHECK(ranking_final_rank() == 1);
    g_save.event_pt = 0;
    CHECK(ranking_final_rank() > 800);
    g_save.event_id = 0;
    /* 一覧: 自分の行が自分の順位の位置にあり、上から順位が続く */
    inv_add(39); inv_add(40);
    g_save.deck[0] = 0; g_save.deck[1] = 1;
    l.mode = RANK_POWER; l.first = 0;
    ranking_list(&l);
    CHECK(l.n == RANK_ROWS && l.player_value == player_power());
    for (i = 0; i < l.n; i++) {
        CHECK(l.rows[i].rank == l.rows[0].rank + i);
        if (l.rows[i].npc == RANK_PLAYER) CHECK(l.rows[i].rank == l.player_rank);
        if (i) CHECK(l.rows[i - 1].value >= l.rows[i].value);
    }
    l.first = RANK_VIEW_TOP;
    ranking_list(&l);
    CHECK(l.rows[0].rank == 1 && l.n == RANK_ROWS);
    for (i = 1; i < l.n; i++) CHECK(l.rows[i - 1].value >= l.rows[i].value);
}

/* ライバル: すぐ上の人を指名し、抜くまで替えない（5 人より遠く離されたら付け替える） */
static void test_rival(void)
{
    RankList l;
    RivalNow r;
    EventRec e;
    u16 i, above = RANK_PLAYER, below = RANK_PLAYER, far = RANK_PLAYER;
    fresh();
    CHECK(RIVAL_OF(RANK_POWER) == RANK_PLAYER);        /* 新しいセーブにはいない */
    inv_add(39); inv_add(40);
    g_save.deck[0] = 0; g_save.deck[1] = 1;
    g_save.g_time = GSEC_PER_DAY * 30;
    l.mode = RANK_POWER; l.first = RANK_VIEW_AROUND;
    ranking_list(&l);
    for (i = 0; i < l.n; i++) {
        if (l.rows[i].rank == l.player_rank - 1) above = l.rows[i].npc;
        if (l.rows[i].rank == l.player_rank + 1) below = l.rows[i].npc;
    }
    CHECK(above != RANK_PLAYER && below != RANK_PLAYER);
    CHECK(RIVAL_OF(RANK_POWER) == above);
    r.mode = RANK_POWER; r.npc = above;
    rival_now(&r);
    CHECK(!r.retired && r.value > r.player && r.player == player_power() && r.final == r.value && !r.pace && r.tone < 4);
    /* 抜いた相手（自分より下）は替えない。マイページが知らせてから替える */
    g_save.rival[RANK_POWER] = (u16)(below + 1);
    ranking_list(&l);
    CHECK(RIVAL_OF(RANK_POWER) == below);
    /* 遠く離れた相手は、すぐ上の人に付け替える */
    l.first = RANK_VIEW_TOP;
    ranking_list(&l);
    far = l.rows[0].npc;
    CHECK(far != RANK_PLAYER && l.player_rank > 10);
    g_save.rival[RANK_POWER] = (u16)(far + 1);
    l.first = RANK_VIEW_AROUND;
    ranking_list(&l);
    CHECK(RIVAL_OF(RANK_POWER) == above);
    /* イベント: そのイベントの間だけ有効で、抜いた元ライバル（chaser）は次のイベントで消える */
    tbl_event(1, &e);
    g_save.event_id = 1;
    g_save.event_pt = 3000;
    g_save.g_time = (u32)e.start_day * GSEC_PER_DAY + (u32)e.duration * GSEC_PER_DAY / 2;   /* 折り返し */
    g_save.chaser = 77;
    CHECK(RIVAL_OF(RANK_EVENT) == RANK_PLAYER && CHASER() == RANK_PLAYER);
    l.mode = RANK_EVENT;
    ranking_list(&l);
    CHECK(g_save.rival_event == 1 && g_save.chaser == 0 && RIVAL_OF(RANK_EVENT) != RANK_PLAYER);
    r.mode = RANK_EVENT; r.npc = RIVAL_OF(RANK_EVENT);
    rival_now(&r);
    CHECK(r.value > r.player && r.final >= r.value);
    CHECK(r.pace && r.pfinal >= 5900 && r.pfinal <= 6100);       /* 半分で 3000 点なら最終日に約 6000 点 */
    CHECK(r.final >= r.value * 2 - 4 && r.final <= r.value * 2 + 4);   /* NPC も経過に比例して伸びる */
    g_save.event_id = 2;
    CHECK(RIVAL_OF(RANK_EVENT) == RANK_PLAYER);        /* イベントが替われば、いない */
    CHECK(RIVAL_OF(RANK_POWER) == above);              /* 総戦力のライバルはそのまま */
    g_save.rival[RANK_POWER] = 0xFFFF;                 /* 壊れた・細工したセーブの番号は「いない」 */
    CHECK(RIVAL_OF(RANK_POWER) == RANK_PLAYER);
    g_save.rival[RANK_POWER] = NPC_COUNT + 1;
    CHECK(RIVAL_OF(RANK_POWER) == RANK_PLAYER);
    g_save.rival[RANK_POWER] = NPC_COUNT;
    CHECK(RIVAL_OF(RANK_POWER) == NPC_COUNT - 1);
}

static void test_timeline(void)
{
    TlState t;
    TlResult r;
    CardRec c;
    NpcNow a, b;
    u16 id;
    fresh();
    timeline_state(&t);
    CHECK(t.day == 0 && t.rateup_pct == 100 && t.era_pct == 100 && t.drop_pct == 0 && !t.ended && t.days_to_end == 270);
    g_save.g_time = GSEC_PER_DAY * 15;           /* 14 日目から 7 日間 SSR 2 倍 */
    timeline_state(&t);
    CHECK(t.rateup_pct == 200);
    g_save.g_time = GSEC_PER_DAY * 22;
    timeline_state(&t);
    CHECK(t.rateup_pct == 100);
    g_save.g_time = GSEC_PER_DAY * 100;          /* インフレ 2 回目まで */
    timeline_state(&t);
    CHECK(t.era_pct == 160);
    /* 上のレア度は 3 か月ごと: 90 日目に USR、180 日目に XUSR（DECISIONS #79） */
    g_save.g_time = GSEC_PER_DAY * 89;
    timeline_state(&t);
    CHECK(t.top_rarity == RARITY_SSR);
    g_save.g_time = GSEC_PER_DAY * 90;
    timeline_state(&t);
    CHECK(t.top_rarity == RARITY_USR);
    g_save.g_time = GSEC_PER_DAY * 181;
    timeline_state(&t);
    CHECK(t.top_rarity == RARITY_XUSR && t.era_pct == 250);
    {   /* XUSR の時代のプレミア: XUSR 1% / USR 2% / SSR 3% */
        GachaReq g;
        long hist[RARITIES] = {0};
        int i;
        g_save.gems = 1000000;
        for (i = 0; i < 20000; i++) {
            g.gacha = 3; g.count = 1;
            gacha_roll(&g);
            tbl_card(g.ids[0], &c);
            hist[c.rarity]++;
        }
        CHECK(hist[RARITY_XUSR] > 120 && hist[RARITY_XUSR] < 300);
        CHECK(hist[RARITY_USR] > 300 && hist[RARITY_USR] < 520);
        CHECK(hist[RARITY_SSR] > 470 && hist[RARITY_SSR] < 730);
    }
    {   /* イベントの敵は、その時点の最上位のレア度を持っている前提で強い */
        EventRec e;
        tbl_event(1, &e); CHECK(e.boss_pct == 100);
        tbl_event(5, &e); CHECK(e.boss_pct == 256);    /* 時代 160% × USR 160% */
        tbl_event(7, &e); CHECK(e.boss_pct == 625);    /* 時代 250% × XUSR 250% */
        tbl_card(e.boss_card, &c); CHECK(c.rarity == RARITY_XUSR);
    }
    g_save.g_time = GSEC_PER_DAY * 200;          /* 20% と 30% が辞めた → 残り 0.8 × 0.7 = 56% */
    timeline_state(&t);
    CHECK(t.drop_pct == 44 && !t.announce);
    g_save.g_time = GSEC_PER_DAY * 250;
    timeline_state(&t);
    CHECK(t.announce && !t.ended && t.days_to_end == 20);
    g_save.g_time = GSEC_PER_DAY * 270;
    timeline_state(&t);
    CHECK(t.ended);
    /* 贈り物（110 日目の石 500）は 1 回だけ */
    fresh();
    g_save.g_time = GSEC_PER_DAY * 111;
    timeline_update(&r);
    CHECK(r.gift_gems == 500 && g_save.gems == START_GEMS + 500 && r.n_news > 5);
    timeline_update(&r);
    CHECK(r.gift_gems == 0 && r.n_news == 0);
    { u16 tx; CHECK(timeline_news(0, &tx)); }
    /* カードは出た時代の倍率を持つ（データを足すだけで新しい SSR が強くなる） */
    tbl_card(39, &c); CHECK(c.era_pct == 100 && c.release_day == 0);
    tbl_card(64, &c); CHECK(c.era_pct == 200 && c.release_day == 150);
    /* 出ていないカードはガチャに出ない */
    fresh();
    g_save.gems = 1000000;
    for (id = 0; id < 300; id++) {
        GachaReq g; g.gacha = 3; g.count = 1;
        gacha_roll(&g);
        tbl_card(g.ids[0], &c);
        CHECK(c.release_day == 0);
    }
    /* NPC は日が経つと強くなり、PLAYER_DROP の後は一部が辞める（何度計算しても同じ） */
    fresh();
    npc_now(10, &a);
    g_save.g_time = GSEC_PER_DAY * 100;
    npc_now(10, &b);
    CHECK(b.level >= a.level && b.power > a.power && !a.retired);
    g_save.g_time = GSEC_PER_DAY * 200;
    {
        u16 n = npc_active_count();
        CHECK(n > 450 && n < 650);                   /* 1000 × 0.56 前後 */
        CHECK(npc_active_count() == n);
    }
    /* 流れるお知らせは時刻で決まる */
    {
        FeedLine f1, f2;
        feed_line(0, &f1); feed_line(0, &f2);
        CHECK(f1.npc == f2.npc && f1.kind == f2.kind && f1.npc < NPC_COUNT);
    }
}

/* ギルド伝言板: 仲間はどのギルドにも 7〜8 人。辞める日に脱退の知らせが出て、その後は書き込まない */
static void test_board(void)
{
    BoardList l;
    NpcRec r;
    NpcNow n;
    u16 i, g, rd = 0, who = 0, never = 0, total = 0, posts = 0, news = 0, repeats = 0, replies = 0, miss = 0;
    u8 k, cnt[GUILD_COUNT] = {0};
    for (i = 0; i < NPC_COUNT; i++) {
        if (!BOARD_IS_MATE(i)) continue;
        tbl_npc(i, &r);
        CHECK(r.guild == (i >> 3) % GUILD_COUNT);
        cnt[r.guild]++;
        total++;
        if (mate_retire_day(i) == BOARD_NEVER) never++;
        else CHECK(mate_retire_day(i) >= 20 && mate_retire_day(i) < 250);
    }
    for (g = 0; g < GUILD_COUNT; g++) CHECK(cnt[g] == 7 || cnt[g] == 8);
    CHECK(never * 100 / total > 15 && never * 100 / total < 45);   /* 3 割前後が最後まで残る */
    fresh();
    l.skip = 0; l.want = BOARD_MAX;
    board_list(&l);
    CHECK(l.n == 0);                                   /* ギルドに入る前は何も無い */
    g_save.guild = 1;
    for (i = 3; i < NPC_COUNT; i += 128) {             /* ギルド 1 の仲間で、辞める人を 1 人 */
        u16 d = mate_retire_day(i);
        if (d != BOARD_NEVER && d > 40) { rd = d; who = i; break; }
    }
    CHECK(rd != 0);
    g_save.g_time = (u32)rd * GSEC_PER_DAY + 13UL * 3600;   /* 辞める日の昼 */
    l.skip = 0; l.want = BOARD_MAX;
    board_list(&l);
    CHECK(l.n == BOARD_MAX && l.post[0].kind == BP_LEAVE && l.post[0].npc == who && l.post[0].day == rd);
    for (k = 1; k < l.n; k++) {                        /* 新しい順で、日付は戻るだけ */
        CHECK(l.post[k].day <= l.post[k - 1].day);
    }
    npc_now(who, &n);
    CHECK(n.retired);
    g_save.g_time -= GSEC_PER_DAY;
    npc_now(who, &n);
    CHECK(!n.retired);
    /* 1 件飛ばすと 1 件ずれる */
    {
        BoardPost first = l.post[1];
        g_save.g_time = (u32)rd * GSEC_PER_DAY + 13UL * 3600;
        l.skip = 1; l.want = BOARD_MAX;
        board_list(&l);
        CHECK(l.post[0].npc == first.npc && l.post[0].text == first.text && l.post[0].day == first.day);
        l.skip = 0; l.want = 1;
        board_list(&l);
        CHECK(l.n == 1 && l.post[0].kind == BP_LEAVE && l.post[0].npc == who);   /* 流れる文は 1 件だけ求める */
    }
    /* 辞めた後は書き込まない。文の id は伝言板の範囲で、% の入る文には文言がつく */
    for (g = 1; g <= GUILD_COUNT; g++) {
        g_save.guild = (u8)g;
        for (i = 0; i < 270; i += 9) {
            g_save.g_time = (u32)i * GSEC_PER_DAY + 23UL * 3600;
            l.skip = 0; l.want = BOARD_MAX;
            board_list(&l);
            for (k = 0; k < l.n; k++) {
                const BoardPost *p = &l.post[k];
                posts++;
                tbl_npc(p->npc, &r);
                CHECK(BOARD_IS_MATE(p->npc) && r.guild == g - 1);
                if (p->kind == BP_LEAVE) { CHECK(mate_retire_day(p->npc) == p->day); continue; }
                CHECK(mate_retire_day(p->npc) >= p->day);
                CHECK(p->text >= T_BD_FADE0 && p->text <= T_BD_RE23 && p->tone < 4);
                if (p->text == T_BD_NEW0 || p->text == T_BD_NEW1 || p->text == T_BD_NEW2 || p->text == T_BD_NEW4 ||
                    p->text == T_BD_NEW5 || p->text == T_BD_NEW7) { CHECK(p->param == T_RAR_4 || p->param == T_RAR_5); news++; }
                if (p->text >= T_BD_MISS0 && p->text <= T_BD_BYE2) {   /* # の入る文: 名前はギルドの仲間 */
                    CHECK(p->ref < NPC_COUNT && BOARD_IS_MATE(p->ref) && p->ref != p->npc && (p->ref >> 3) % GUILD_COUNT == g - 1);
                    if (p->text <= T_BD_MISS2) { CHECK(mate_retire_day(p->ref) < p->day); miss++; } else replies++;
                } else CHECK(p->ref == BOARD_NOREF);
                if (k && l.post[k - 1].kind == BP_POST && l.post[k - 1].text == p->text) repeats++;
                if (p->text == T_BD_HUNT0 || p->text == T_BD_HUNT1 || p->text == T_BD_RAID3 || p->text == T_BD_PVP1) CHECK(p->param != 0);
            }
        }
    }
    CHECK(posts > 2000 && news > 0 && replies * 10 > posts && miss > 0);   /* 返事は 1 割以上 */
    CHECK(repeats * 100 < posts);                      /* 続けて同じ文はめったに出ない */
    /* 自分の出来事のあった時間帯には必ず誰かが反応し、SSR ならそのカードの名前が入る */
    {
        CardRec c;
        g_save.guild = 1;
        g_save.g_time = 30UL * GSEC_PER_DAY + 1;
        pev_push(PEV_SSR, 39);
        l.skip = 0; l.want = 1;
        board_list(&l);
        tbl_card(39, &c);
        CHECK(l.n == 1 && l.post[0].text >= T_BD_RE0 && l.post[0].text <= T_BD_RE2 && (l.post[0].text != T_BD_RE0 || l.post[0].param == c.name_text));
        g_save.g_time += BOARD_SLOT_SECS;
        pev_push(PEV_STOLEN, 5);
        board_list(&l);
        CHECK(l.n == 1 && l.post[0].text >= T_BD_RE3 && l.post[0].text <= T_BD_RE5 && (l.post[0].text == T_BD_RE5 || l.post[0].ref == 5));
        g_save.g_time += 3 * BOARD_SLOT_SECS;              /* しばらくたつと反応しない */
        board_list(&l);
        CHECK(l.n == 0 || l.post[0].text < T_BD_RE0);
        memset(g_save.pev, 0, sizeof(g_save.pev));
    }
}

/* 運営モード（DECISIONS #88）: 史実の選択は終了、選び方によっては継続。同じ選択なら同じ数字 */
static u8 ops_run(const char *pick)
{
    OpsMeeting m;
    u8 met = 0;
    ops_start();
    while (g_ops.day < OPS_DAYS) {
        while (ops_meeting(&m)) {
            CHECK(m.text == T_OPS_M0 + g_ops.next * OPS_TEXTS);
            met++;
            ops_decide((u8)(pick[g_ops.next] - '0'));
        }
        ops_step();
    }
    CHECK(met == OPS_MEETINGS);
    return g_ops.verdict;
}

static void test_ops(void)
{
    u32 total;
    CHECK(ops_run("000000000000") == OPS_END);
    CHECK(g_ops.sales_total == OPS_HIST_SALES);          /* config.h の史実の売上と利益（決算の上司のひとことに使う） */
    CHECK(g_ops.sales_total - g_ops.ad_total == OPS_HIST_PROFIT);
    CHECK(g_ops.points == 187 && g_ops.points < OPS_CONT_POINTS);
    CHECK(g_ops.reg > OPS_PREREG && g_ops.reg < 1000);   /* 事前登録から流入で増えるが、1000 人は入りきらない */
    CHECK(g_ops.ad_total == (OPS_AD_L1_MAN - 60UL) * 10000 + 80UL * 10000);   /* 開始の宣伝は CVR で途中打ち切り（240 万円）、95 日目は 80 万円で打ち切り */
    total = g_ops.sales_total;
    CHECK(ops_run("000000000000") == OPS_END && g_ops.sales_total == total);   /* 何度でも同じ */
    CHECK(ops_run("101202012000") == OPS_CONTINUE);      /* 渋い排出率で稼ぎ、送客で人を呼び、USR を見送り、石 3000 個で引き留める */
    CHECK(g_ops.points >= OPS_CONT_POINTS);
    CHECK(ops_run("002000020000") == OPS_END && g_ops.ad_total == 0);   /* 広告を打たなければ広告費は 0 */
    CHECK(ops_run("999999999999") == OPS_END && g_ops.sales_total == total);   /* 範囲外の選択は史実として扱う */
}

/* 宿屋（DECISIONS #91）: 翌朝 6 時まで進んで AP が全回復し、宿代を払う。夜明け前なら同じ日の朝 */
static void test_inn(void)
{
    fresh();
    g_save.gold = INN_GOLD - 1;
    CHECK(player_sleep() == 0);
    g_save.gold = 1000;
    g_save.ap = 3;
    g_save.g_time = 5UL * GSEC_PER_DAY + 22UL * 3600;
    CHECK(player_sleep() == 8 && g_save.g_time == 6UL * GSEC_PER_DAY + INN_WAKE_SECS);
    CHECK(g_save.ap == AP_MAX && g_save.gold == 1000 - INN_GOLD && g_save.pev[0].kind == PEV_INN);
    g_save.g_time = 9UL * GSEC_PER_DAY + 2UL * 3600;
    CHECK(player_sleep() == 4 && g_save.g_time == 9UL * GSEC_PER_DAY + INN_WAKE_SECS);
}

/* 次の予定まで休む（DECISIONS #92）: 対抗戦の戦は 12:00 と 21:00、イベントは始まりの日の 0:00。AP は進んだぶんだけ */
static void test_plan(void)
{
    Plan p;
    EventRec e;
    fresh();
    g_save.g_time = 140UL * GSEC_PER_DAY + 13UL * 3600;             /* 第 1 回の対抗戦の初日、昼の戦の後 */
    plan_next(&p);
    CHECK(p.kind == PLAN_GVG && p.match == 1 && p.at == 140UL * GSEC_PER_DAY + 21UL * 3600);
    g_save.ap = 0;
    g_save.ap_gtime = g_save.g_time;
    CHECK(player_wait(&p) && g_save.g_time == 140UL * GSEC_PER_DAY + 21UL * 3600);
    CHECK(g_save.ap == 8 * 3600UL / AP_RECOVER_GSEC);                  /* 8 時間ぶんだけ回復 */
    g_save.g_time += 1800;                                            /* 夜の戦の途中 → 翌日の昼の戦 */
    plan_next(&p);
    CHECK(p.kind == PLAN_GVG && p.match == 2 && p.at == 141UL * GSEC_PER_DAY + 12UL * 3600);
    g_save.g_time = 1;                                                /* 始めたばかり: 最初のイベント */
    plan_next(&p);
    tbl_event(1, &e);
    CHECK(p.kind == PLAN_EVENT && p.at == (u32)e.start_day * GSEC_PER_DAY && p.text == e.name_text);
    g_save.g_time = 269UL * GSEC_PER_DAY;                             /* 終わりの前日: もう予定は無い */
    plan_next(&p);
    CHECK(p.kind == PLAN_NONE && !player_wait(&p) && g_save.g_time == 269UL * GSEC_PER_DAY);
}

/* ギルドの戦力をまとめて数えても、1 人ずつ npc_now で数えたのと同じ（DECISIONS #93） */
static void test_guild_strengths(void)
{
    u32 s[GUILD_COUNT], t[GUILD_COUNT];
    NpcRec r;
    NpcNow n;
    u16 i, d;
    for (d = 0; d < 270; d += 67) {
        fresh();
        g_save.g_time = (u32)d * GSEC_PER_DAY + 3600;
        g_save.event_id = (u16)(d / 67 % 3);
        guild_strengths(s);
        for (i = 0; i < GUILD_COUNT; i++) t[i] = 0;
        for (i = 0; i < NPC_COUNT; i++) {
            tbl_npc(i, &r);
            npc_now(i, &n);
            if (!n.retired) t[r.guild] += n.power;
        }
        for (i = 0; i < GUILD_COUNT; i++) CHECK(s[i] == t[i] && s[i] > 0);
    }
}

/* 盗み・盗まれ（steal.h）: デッキのカードは盗まれず、盗まれたカードは盗んだ人から取り返せる */
static void test_steal(void)
{
    StealList l, l2;
    u16 i, before, total = 0;
    u8 got, k, back;
    fresh();
    g_save.flags |= SF_STARTED;
    g_save.guild = 1;
    for (i = 0; i < 20; i++) inv_add((u16)(1 + i % 40));
    for (i = 0; i < DECK_SIZE; i++) g_save.deck[i] = (u8)i;          /* 0..4 はデッキ */
    g_save.g_time = 10UL * GSEC_PER_DAY;
    CHECK(steal_tick() == 0 && g_save.steal_check == g_save.g_time);  /* 初めては数え始めるだけ */
    for (k = 0; k < 40 && !total; k++) {             /* 放置すると盗まれる（1 回に 3 枚まで） */
        before = g_save.card_count;
        g_save.g_time += 10UL * GSEC_PER_DAY;
        got = steal_tick();
        CHECK(got <= STEAL_PER_CHECK && g_save.card_count == before - got);
        total += got;
    }
    CHECK(total > 0 && g_save.stolen_new == total && g_save.stolen[0].card && g_save.memo[MEMO_STOLEN]);
    for (i = 0; i < DECK_SIZE; i++) CHECK(g_save.deck[i] == i);    /* デッキは崩れない */
    CHECK(steal_tick() == 0);                         /* 同じ時刻にもう一度数えても増えない */

    /* 一覧は入り直しても同じ。取り返しが先に並ぶ */
    steal_list(&l);
    steal_list(&l2);
    CHECK(l.n == l2.n && l.n >= 1 && l.r[0].npc == l2.r[0].npc && l.r[l.n - 1].card == l2.r[l.n - 1].card);
    for (k = 0, back = 0; k < l.n; k++) {
        CHECK(l.r[k].pct >= STEAL_PCT_MIN && l.r[k].pct <= 95 && l.r[k].card >= 1 && l.r[k].card <= CARD_COUNT_BUILT);
        if (l.r[k].back) back++;
        else CHECK(l.r[k].pct <= STEAL_PCT_MAX);
    }
    CHECK(back == 0 || l.r[0].back == 1);

    /* AP が足りなければ行けない。行けば AP を払い、取り返せばカードが元の Lv で戻って記録が消える */
    g_save.ap = STEAL_AP - 1;
    g_save.ap_gtime = g_save.g_time;
    CHECK(steal_try(&l, 0) == STEAL_NO_AP && g_save.ap == STEAL_AP - 1);
    for (k = 0; k < 50; k++) {
        u8 st, bk;
        u16 card, n0;
        steal_list(&l);
        bk = l.r[0].back;
        card = l.r[0].card;
        n0 = g_save.card_count;
        g_save.ap = AP_MAX;
        st = steal_try(&l, 0);
        CHECK(g_save.ap == AP_MAX - STEAL_AP && (st == STEAL_OK || st == STEAL_FAIL));
        if (st == STEAL_OK) {
            CHECK(g_save.card_count == n0 + 1 && g_save.cards[n0].id == card);
            if (bk) { CHECK(g_save.cards[n0].lv == l.r[0].lv); break; }
        }
    }
    CHECK(k < 50);                                    /* いつかは成功する */
}

/* 思い出（memo.h）は初めての日だけ残り、結末（ending.h）は上から順に当てはまったもの */
static void test_memo_ending(void)
{
    CardRec c;
    EndingStats e;
    u16 id, ssr = 0, n = 0, i, rd, first = END_NEVER;
    u8 stay = 0, mates = 0;
    for (id = 1; id <= CARD_COUNT_BUILT && !(ssr && n); id++) {
        tbl_card(id, &c);
        if (c.rarity == RARITY_SSR && !ssr) ssr = id;
        if (c.rarity == RARITY_N && !n) n = id;
    }
    fresh();
    g_save.g_time = 5 * GSEC_PER_DAY + 100;
    inv_add(n);
    CHECK(g_save.memo[MEMO_SSR] == 0);                  /* N では残らない */
    inv_add(ssr);
    CHECK(g_save.memo[MEMO_SSR] == 6);                  /* 5 日目（0 始まり）+ 1 */
    g_save.g_time = 9 * GSEC_PER_DAY;
    inv_add(ssr);
    CHECK(g_save.memo[MEMO_SSR] == 6);                  /* 2 回目では変わらない */
    player_add_exp(60000);
    CHECK(g_save.memo[MEMO_LV10] == 10 && g_save.memo[MEMO_LV50] == (g_save.level >= 50 ? 10 : 0));
    g_save.course = 4;                                  /* 無尽蔵 */
    CHECK(wallet_buy_gems(1) == W_OK && g_save.memo[MEMO_PAY] == 10);

    /* 結末の順番 */
    memset(&e, 0, sizeof(e));
    e.kinds_all = 121; e.mates = 8; e.stayed = 2;
    CHECK(ending_kind(&e) == END_FREE);
    e.spent = 3000;
    CHECK(ending_kind(&e) == END_NORMAL);
    e.gvg_won = 1;
    CHECK(ending_kind(&e) == END_GUILD);
    e.stayed = 0;
    CHECK(ending_kind(&e) == END_ALONE);
    e.kinds = 61;
    CHECK(ending_kind(&e) == END_COLLECT);
    e.kinds = 60;
    CHECK(ending_kind(&e) == END_ALONE);
    e.spent = END_WHALE_YEN;
    CHECK(ending_kind(&e) == END_WHALE);

    /* ギルドの仲間は伝言板と同じ顔ぶれ・同じ引退の日 */
    fresh();
    g_save.guild = 1;
    inv_add(ssr); inv_add(ssr); inv_add(n);
    ending_stats(&e);
    for (i = 3; i < NPC_COUNT && mates < END_MATES_MAX; i += 128) {
        mates++;
        rd = mate_retire_day(i);
        if (rd == BOARD_NEVER) { CHECK(e.stay[stay] == i && e.tone[stay] == npc_tone(i)); stay++; }
        else if (rd < first) first = rd;
    }
    CHECK(e.mates == mates && e.stayed == stay && e.first_leave == first);
    CHECK(e.kinds == 2 && e.kinds_all == CARD_COUNT_BUILT && e.spent == 0 && e.kind == (stay ? END_FREE : END_ALONE));
}

int main(void)
{
    test_timeline();
    test_wallet();
    test_fusion();
    test_event();
    test_raid();
    test_pvp();
    test_guildwar();
    test_ranking();
    test_board();
    test_rival();
    test_stance_leader_slots();
    test_ops();
    test_memo_ending();
    test_steal();
    test_inn();
    test_plan();
    test_guild_strengths();
    test_struct_sizes();
    test_battle();
    test_elem();
    test_auto_for();
    test_ap();
    test_inventory();
    test_quest();
    test_gacha();
    test_login();
    CHECK_DONE("game");
    return 0;
}
