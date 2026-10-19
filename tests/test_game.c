/* test_game.c — 戦闘・AP・クエスト・ガチャ・ログインボーナス・デッキ（表は build/ の .bin の実物） */
#include "check.h"
#include <string.h>
#include "core/battle.h"
#include "core/deck.h"
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
#include "core/ranking.h"

static void fresh(void) { save_new(&g_save, 1000); }

static void test_struct_sizes(void)
{
    /* 生成ヘッダは PC でも詰めて並べる（ROM のバイナリをそのまま読むため） */
    CHECK(sizeof(CardRec) == 18);
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
    CHECK(r.area_cleared && r.drop_card >= 21 && r.drop_card <= 32);
    CHECK(quest_max_stage() == 11);
}

static void test_gacha(void)
{
    GachaReq g;
    long hist[4] = {0};
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
    /* 天井: 100 連目までに必ず SSR */
    g_save.pity_count = 0;
    g_save.rng_gacha = 12345;
    for (i = 0; i < 200; i++) {
        g.gacha = 3; g.count = 1;
        gacha_roll(&g);
        CHECK(g_save.pity_count < 100);
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
    CHECK(g.status == G_OK && c.rarity >= RARITY_SR && g_save.sr_tickets == 0);
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
    event_add_points(400, &g);
    CHECK(g.n == 1 && g.item[0] == ITEM_GEMS && event_next_line() == 1500);
    memset(&g, 0, sizeof g);
    event_add_points(10, &g);
    CHECK(g.n == 0);                            /* 同じラインは 2 度出ない */
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
        event_update(&r);
        CHECK(r.ended && r.id == 1 && r.points == 540 && r.rank == ranking_final_rank(540));
        CHECK(r.gain.n == 1 && g_save.gems == gems + 100);   /* 1000 位以内: 石 100 */
        CHECK(g_save.event_id == 0 && g_save.last_event == 1 && g_save.last_rank == r.rank);
    }
    event_update(&r);
    CHECK(!r.ended && !r.started);              /* 2 度目は何も起きない */
    quest_run(EVENT_STAGE_BASE + 1, &q);
    CHECK(q.status == Q_LOCKED);                /* 終わったイベントの面には入れない */
}

static void test_ranking(void)
{
    RankList l;
    NpcRec a, b;
    u8 i;
    fresh();
    tbl_npc(0, &a); tbl_npc(999, &b);
    CHECK(a.event_pt >= b.event_pt);            /* 表は大きい順 */
    CHECK(ranking_final_rank(0xFFFFFFFFUL) == 1);
    CHECK(ranking_final_rank(0) > 800);
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
    l.first = 1;
    ranking_list(&l);
    CHECK(l.rows[0].rank == 1);
}

int main(void)
{
    test_wallet();
    test_fusion();
    test_event();
    test_ranking();
    test_struct_sizes();
    test_battle();
    test_elem();
    test_ap();
    test_inventory();
    test_quest();
    test_gacha();
    test_login();
    CHECK_DONE("game");
    return 0;
}
