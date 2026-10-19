/* sim_economy.c — ゲームと同じ core/ を PC で動かし、サービス開始から終了（270 日）までを遊ばせる。
 * tools/sim_economy.py が何人分か走らせて表にする（Phase3 の受け入れ: 終了時に SSR 3〜8 枚）。
 *
 *   sim_economy <コース 0..4> <1 日のプレイ回数> <種>
 *   コース 0 = 課金しない（おこづかいコースを選ぶが石を買わない）、1..4 = courses.csv の id
 *
 * 1 回のプレイ: 回復した AP でクエスト（イベント中はイベントのボス）→ 無料・ゴールドガチャ →
 * 石が 10 連分あればプレミア 10 連 → N を素材に合成 → 進化 → おまかせ編成。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/deck.h"
#include "core/event.h"
#include "core/fusion.h"
#include "core/gacha.h"
#include "core/inventory.h"
#include "core/login.h"
#include "core/player.h"
#include "core/quest.h"
#include "core/save.h"
#include "core/tables.h"
#include "core/time.h"
#include "core/timeline.h"
#include "core/wallet.h"

static u8 buy;   /* 課金するか */

static u16 count_rarity(u8 r)
{
    CardRec c;
    u16 i, n = 0;
    for (i = 0; i < g_save.card_count; i++) {
        tbl_card(g_save.cards[i].id, &c);
        if (c.rarity == r) n++;
    }
    return n;
}

static u8 pull(u8 id, u8 count)
{
    GachaReq g;
    g.gacha = id;
    g.count = count;
    gacha_pull(&g);
    return g.status == G_OK;
}

static void gachas(void)
{
    GachaReq g;
    u8 prem = g_save.event_id ? 4 : 3;
    pull(1, 1);
    while (g_save.gold >= 1000 && g_save.card_count < CARD_MAX - 20 && pull(2, 1)) { }
    while (g_save.gacha_tickets && g_save.card_count < CARD_MAX - 20 && pull(5, 1)) { }
    while (g_save.sr_tickets && pull(6, 1)) { }
    if (buy) {
        while (g_save.gems < 2700) {
            if (wallet_buy_gems(2) != W_OK) break;      /* 1000 円ずつ */
        }
    }
    for (;;) {
        g.gacha = prem; g.count = 10;
        if (gacha_can_pay(&g) != G_OK || g_save.card_count > CARD_MAX - 10) break;
        gacha_pull(&g);
    }
}

/* 最大 Lv でないカードのうち攻撃の一番高いもの（デッキ優先）をベースに、N を全部食べさせる。
 * それでも溢れそうなら R も素材にする（当時の「とりあえず全部合成」） */
static void fuse(void)
{
    FuseReq f;
    CardRec c;
    u16 i, j, base = 0xFFFF;
    u32 best = 0;
    for (i = 0; i < g_save.card_count; i++) {
        u32 v = card_atk_of(i);
        tbl_card(g_save.cards[i].id, &c);
        if (g_save.cards[i].lv >= c.max_lv || c.rarity == RARITY_N) continue;
        if (inv_in_deck(i)) v *= 4;
        if (v > best) { best = v; base = i; }
    }
    if (base != 0xFFFF) {
        memset(&f, 0, sizeof f);
        f.base = base;
        fusion_select_all_n(&f);
        if (g_save.card_count > CARD_MAX - 30) {
            for (i = 0; i < g_save.card_count; i++) {
                tbl_card(g_save.cards[i].id, &c);
                if (c.rarity == RARITY_R && i != base && !inv_in_deck(i)) SEL_SET(f.sel, i);
            }
        }
        fusion_preview(&f);
        if (f.status == F_OK) fusion_apply(&f);
    }
    for (i = 0; i < g_save.card_count; i++)         /* 進化できる組があれば進化 */
        for (j = i + 1; j < g_save.card_count; j++)
            if (evolve_partner_ok(i, j) && evolve(i, j) == F_OK) return;
}

static void quests(void)
{
    QuestResult q;
    u8 lv;
    u16 fails = 0;
    for (;;) {
        player_ap_update();
        if (g_save.event_id && g_save.event_id == event_active()) {
            for (lv = EVENT_LEVELS; lv >= 1; lv--) {     /* 勝てる一番上の面 */
                quest_run(EVENT_STAGE_BASE + lv, &q);
                if (q.status == Q_NO_AP) return;
                if (q.status == Q_OK && q.b.win) break;
            }
            if (lv == 0) return;
            continue;
        }
        quest_run(quest_max_stage(), &q);
        if (q.status == Q_NO_AP) return;
        if (q.status == Q_OK && !q.b.win) {             /* 勝てなければ一つ前を周回 */
            if (++fails > 3 || quest_max_stage() < 2) {
                quest_run((u16)(quest_max_stage() - 1), &q);
                if (q.status == Q_NO_AP) return;
            }
        }
    }
}

int main(int argc, char **argv)
{
    u8 course = (u8)(argc > 1 ? atoi(argv[1]) : 0);
    u32 per_day = argc > 2 ? (u32)atoi(argv[2]) : 4;
    u32 seed = argc > 3 ? (u32)atoi(argv[3]) : 1;
    u32 gap = (u32)(86400UL * TIME_SCALE / per_day);    /* 現実の 1 日 = ゲームの 10 日 */
    LoginResult lr;
    EventResult er;
    TlResult tr;
    TlState tl;
    u32 next_report = 0;
    u8 i;
    buy = (u8)(course != 0);
    save_new(&g_save, 0);
    g_save.rng_gacha = seed * 2654435761UL | 1;
    wallet_start(course ? course : 1);
    {   /* 最初の 10 連（引き直しはしない） */
        GachaReq g; g.gacha = 3; g.count = 10;
        gacha_roll(&g);
        for (i = 0; i < 10; i++) inv_add(g.ids[i]);
        g_save.pity_count = 0;
    }
    printf("day,level,stage,ssr,sr,cards,gold,gems,funds,spent,event_pt\n");
    for (;;) {
        timeline_state(&tl);
        if (tl.ended) break;
        login_check(&lr);
        wallet_payday();
        event_update(&er);
        if (er.ended) fprintf(stderr, "event %u: pt %lu rank %u\n", er.id, (unsigned long)er.points, er.rank);
        timeline_update(&tr);
        deck_auto();
        quests();
        gachas();
        fuse();
        deck_auto();
        if (tl.day >= next_report) {
            printf("%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu\n", tl.day, g_save.level, g_save.stage_cleared,
                   count_rarity(RARITY_SSR), count_rarity(RARITY_SR), g_save.card_count,
                   (unsigned long)g_save.gold, (unsigned long)g_save.gems, (unsigned long)g_save.funds,
                   (unsigned long)g_save.spent, (unsigned long)g_save.event_pt);
            next_report = tl.day + 30;
        }
        g_save.g_time += gap;
    }
    printf("END,%u,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu\n", g_save.level, g_save.stage_cleared,
           count_rarity(RARITY_SSR), count_rarity(RARITY_SR), g_save.card_count,
           (unsigned long)g_save.gold, (unsigned long)g_save.gems, (unsigned long)g_save.funds,
           (unsigned long)g_save.spent, (unsigned long)g_save.last_rank);
    return 0;
}
