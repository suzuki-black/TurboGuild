/* gacha.c — レアリティを引いてから、プール内でそのレアリティを等確率に選ぶ。
 * 10 連は SR 以上 1 枚確定（9 枚目まで出なければ 10 枚目を SR に）。
 * 天井: pity を持つガチャは pity_count が達したら SSR 確定にして数え直す。 */
#include "gacha.h"
#include "inventory.h"
#include "rng.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "../far.h"

/* プールに入るときの重み。0 = 入らない。イベントガチャはイベント限定を 2 倍（仕様 §4.3） */
static u8 weight(const CardRec *c, u8 pool)
{
    if (c->event_id == 0) return 1;
    if (pool == POOL_NORMAL || c->event_id != g_save.event_id) return 0;
    return (u8)(pool == POOL_EVENT2X ? 2 : 1);
}

static u16 pick(u8 rarity, u8 pool)
{
    CardRec c;
    u16 n = tbl_card_count(), id, hits = 0, k;
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

static u8 roll_rarity(const GachaRec *gr)
{
    u16 r = rng_range(&g_save.rng_gacha, 10000);
    if (r < gr->rate_ssr) return RARITY_SSR;
    r -= gr->rate_ssr;
    if (r < gr->rate_sr) return RARITY_SR;
    r -= gr->rate_sr;
    if (r < gr->rate_r) return RARITY_R;
    return RARITY_N;
}

static void draw_all(GachaReq *g, const GachaRec *gr)
{
    u8 i, best = 0;
    for (i = 0; i < g->count; i++) {
        u8 rar = roll_rarity(gr);
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
    if (gr->pool == POOL_EVENT2X && g_save.event_id == 0) return G_UNAVAILABLE;
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
