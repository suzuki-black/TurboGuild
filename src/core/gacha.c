/* SPDX-License-Identifier: MIT */
/* gacha.c — レアリティを引いてから、プール内でそのレアリティを等確率に選ぶ。
 * 10 連は SR 以上 1 枚確定（9 枚目まで出なければ 10 枚目を SR に）。
 * 天井: pity を持つガチャは pity_count が達したら SSR 確定にして数え直す（今のデータでは 0 = 天井なし）。
 * BOX ガチャ: 100 個入りの箱から引く。残りの個数で抽選し、引いた分だけ減る。空になれば次の箱。
 *   箱の SSR はイベント限定の SSR（無いイベントでは通常の SSR）。確率 UP と 10 連の SR 確定は無し。 */
#include "gacha.h"
#include "inventory.h"
#include "rng.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "timeline.h"
#include "../far.h"

#define POOL_LIMITED 0xFF        /* 内部用: 開催中のイベントの限定カードだけ */

/* プールに入るときの重み。0 = 入らない。イベントガチャはイベント限定を 2 倍（仕様 §4.3） */
static u8 weight(const CardRec *c, u8 pool)
{
    if (c->release_day > time_gday(g_save.g_time)) return 0;    /* まだ実装されていないカード */
    if (pool == POOL_LIMITED) return (u8)(c->event_id && c->event_id == g_save.event_id);
    if (c->event_id == 0) return 1;
    if (pool == POOL_NORMAL || c->event_id != g_save.event_id) return 0;
    return (u8)(pool == POOL_NORMAL_EVENT ? 1 : 2);   /* イベントガチャ・BOX は限定を 2 倍 */
}

static u16 pick(u8 rarity, u8 pool)
{
    CardRec c;
    u16 n = tbl_card_count(), id, hits = 0, k;
    if (pool == POOL_BOX && rarity == RARITY_SSR) {  /* 箱の目玉は限定 SSR。無ければ通常の SSR */
        for (id = 1; id <= n; id++) {
            tbl_card(id, &c);
            if (c.rarity == RARITY_SSR) hits += weight(&c, POOL_LIMITED);
        }
        if (hits) pool = POOL_LIMITED;
        hits = 0;
    }
    for (;;) {
        for (id = 1; id <= n; id++) {
            tbl_card(id, &c);
            if (c.rarity == rarity) hits += weight(&c, pool);
        }
        if (hits || rarity == 0) break;
        rarity--;                    /* そのレアリティが空なら 1 段下げる */
    }
    if (hits == 0) return 1;
    k = rng_range(&g_save.rng_gacha, hits);
    for (id = 1; id <= n; id++) {
        u8 w;
        tbl_card(id, &c);
        if (c.rarity != rarity) continue;
        w = weight(&c, pool);
        if (k < w) return id;
        k -= w;
    }
    return 1;
}

/* 年表の確率 UP キャンペーン中は SSR を倍に（N の分から回す） */
static u8 roll_rarity(const GachaRec *gr, u16 rateup)
{
    u16 r = rng_range(&g_save.rng_gacha, 10000);
    u16 ssr = (u16)((u32)gr->rate_ssr * rateup / 100);
    if (r < ssr) return RARITY_SSR;
    r -= ssr;
    if (r < gr->rate_sr) return RARITY_SR;
    r -= gr->rate_sr;
    if (r < gr->rate_r) return RARITY_R;
    return RARITY_N;
}

static void box_fill(const GachaRec *gr)
{
    g_save.box_left[RARITY_N]   = (u8)(gr->rate_n / 100);
    g_save.box_left[RARITY_R]   = (u8)(gr->rate_r / 100);
    g_save.box_left[RARITY_SR]  = (u8)(gr->rate_sr / 100);
    g_save.box_left[RARITY_SSR] = (u8)(gr->rate_ssr / 100);
    g_save.box_no++;
}

/* 開催中のイベントの箱にそろえる（別のイベントの箱なら 1 箱目から作り直す） */
FAR(void, gacha_box_sync, (u8 gacha))
{
    GachaRec gr;
    tbl_gacha(gacha, &gr);
    if (g_save.box_event == g_save.event_id && g_save.box_no) return;
    g_save.box_event = g_save.event_id;
    g_save.box_no = 0;
    box_fill(&gr);
}

static u8 box_draw(const GachaRec *gr)
{
    u8 rar;
    u16 r;
    if (!(g_save.box_left[0] | g_save.box_left[1] | g_save.box_left[2] | g_save.box_left[3])) box_fill(gr);
    r = rng_range(&g_save.rng_gacha, (u16)g_save.box_left[0] + g_save.box_left[1] + g_save.box_left[2] + g_save.box_left[3]);
    for (rar = RARITY_SSR; ; rar--) {                /* SSR から順に。残りが 0 の段は飛ばされる */
        if (r < g_save.box_left[rar] || rar == RARITY_N) break;
        r -= g_save.box_left[rar];
    }
    g_save.box_left[rar]--;
    return rar;
}

static void draw_all(GachaReq *g, const GachaRec *gr)
{
    TlState tl;
    u8 i, best = 0;
    timeline_state(&tl);
    if (gr->pool == POOL_BOX) gacha_box_sync(g->gacha);
    for (i = 0; i < g->count; i++) {
        u8 rar;
        if (gr->pool == POOL_BOX) {
            g->ids[i] = pick(box_draw(gr), gr->pool);
            continue;
        }
        rar = roll_rarity(gr, gr->rate_ssr ? tl.rateup_pct : 100);
        if (gr->pity) {
            if (++g_save.pity_count >= gr->pity) { rar = RARITY_SSR; g->pity_hit = 1; }
            if (rar == RARITY_SSR) g_save.pity_count = 0;
        }
        if (g->count == GACHA_ONE_MAX && i == GACHA_ONE_MAX - 1 && best < RARITY_SR) rar = RARITY_SR;
        if (rar > best) best = rar;
        g->ids[i] = pick(rar, gr->pool);
    }
    g->got = g->count;
}

static u8 check_pay(GachaReq *g, const GachaRec *gr, u32 *price)
{
    *price = (g->count == GACHA_ONE_MAX) ? gr->price10 : gr->price;
    if (g->count != 1 && g->count != GACHA_ONE_MAX) return G_UNAVAILABLE;
    if (g->count == GACHA_ONE_MAX && gr->price10 == 0) return G_UNAVAILABLE;
    if ((gr->pool == POOL_EVENT2X || gr->pool == POOL_BOX) && g_save.event_id == 0) return G_UNAVAILABLE;
    switch (gr->currency) {
    case CUR_FREE:     return g_save.last_free_gacha_gday == time_gday(g_save.g_time) ? G_FREE_USED : G_OK;
    case CUR_GOLD:     return g_save.gold >= *price ? G_OK : G_NO_MONEY;
    case CUR_GEMS:     return g_save.gems >= *price ? G_OK : G_NO_MONEY;
    case CUR_TICKET:   return g_save.gacha_tickets >= *price ? G_OK : G_NO_MONEY;
    case CUR_SRTICKET: return g_save.sr_tickets >= *price ? G_OK : G_NO_MONEY;
    default:           return G_UNAVAILABLE;
    }
}

static void pay(const GachaRec *gr, u32 price)
{
    switch (gr->currency) {
    case CUR_FREE:     g_save.last_free_gacha_gday = time_gday(g_save.g_time); break;
    case CUR_GOLD:     g_save.gold -= price; break;
    case CUR_GEMS:     g_save.gems -= price; break;
    case CUR_TICKET:   g_save.gacha_tickets -= (u8)price; break;
    case CUR_SRTICKET: g_save.sr_tickets -= (u8)price; break;
    default: break;
    }
}

FAR(u8, gacha_can_pay, (GachaReq *g))
{
    GachaRec gr;
    u32 price;
    tbl_gacha(g->gacha, &gr);
    return check_pay(g, &gr, &price);
}

FAR(void, gacha_roll, (GachaReq *g))
{
    GachaRec gr;
    tbl_gacha(g->gacha, &gr);
    g->status = G_OK;
    g->pity_hit = 0;
    draw_all(g, &gr);
}

FAR(void, gacha_pull, (GachaReq *g))
{
    GachaRec gr;
    u32 price;
    u8 i;
    tbl_gacha(g->gacha, &gr);
    g->got = 0;
    g->pity_hit = 0;
    g->status = check_pay(g, &gr, &price);
    if (g->status != G_OK) return;
    if (g_save.card_count + g->count > CARD_MAX) { g->status = G_FULL; return; }
    pay(&gr, price);
    draw_all(g, &gr);
    for (i = 0; i < g->got; i++) inv_add(g->ids[i]);
}
