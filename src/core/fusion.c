/* SPDX-License-Identifier: MIT */
/* fusion.c — 素材 1 枚の経験値は N 10 / R 50 / SR 300 / SSR 2000、ベースと同じレアリティなら +50%。
 * 必要経験値は lv × 20 × (rarity + 1)、最大 Lv で止める。ゴールドは素材 1 枚 100。
 * 素材を消すと後ろの添字が詰まるので、大きい添字から消し、ベースの添字もずらす。 */
#include "fusion.h"
#include "inventory.h"
#include "save.h"
#include "tables.h"
#include "../far.h"

#define GOLD_PER_MAT 100

static const u16 mat_exp[4] = { 10, 50, 300, 2000 };

static u16 need_of(u8 lv, u8 rarity)
{
    return (u16)((u16)lv * 20 * (rarity + 1));
}

FAR(u16, fusion_need, (u16 index))
{
    CardRec c;
    tbl_card(g_save.cards[index].id, &c);
    return need_of(g_save.cards[index].lv, c.rarity);
}

/* 経験値を足したあとの Lv と余り（最大 Lv で止める） */
static u8 level_after(u8 lv, u32 exp, const CardRec *c, u16 *left)
{
    while (lv < c->max_lv && exp >= need_of(lv, c->rarity)) {
        exp -= need_of(lv, c->rarity);
        lv++;
    }
    if (lv >= c->max_lv) exp = 0;
    *left = (u16)exp;
    return lv;
}

FAR(void, fusion_preview, (FuseReq *f))
{
    CardRec base, m;
    u16 i, left;
    const OwnedCard *b = &g_save.cards[f->base];
    tbl_card(b->id, &base);
    f->count = 0;
    f->gain = 0;
    f->status = F_OK;
    for (i = 0; i < g_save.card_count; i++) {
        u32 e;
        if (!SEL_GET(f->sel, i)) continue;
        if (i == f->base || inv_in_deck(i)) { f->status = F_BAD; continue; }
        tbl_card(g_save.cards[i].id, &m);
        e = mat_exp[m.rarity];
        if (m.rarity == base.rarity) e += e / 2;
        f->gain += e;
        f->count++;
    }
    f->gold = (u16)(f->count * GOLD_PER_MAT);
    f->lv_before = b->lv;
    f->lv_after = level_after(b->lv, (u32)b->exp + f->gain, &base, &left);
    if (f->status != F_OK) return;
    if (f->count == 0) f->status = F_NONE;
    else if (b->lv >= base.max_lv) f->status = F_MAX_LV;
    else if (g_save.gold < f->gold) f->status = F_NO_GOLD;
}

FAR(void, fusion_apply, (FuseReq *f))
{
    CardRec base;
    u16 i, left;
    fusion_preview(f);
    if (f->status != F_OK) return;
    g_save.gold -= f->gold;
    for (i = g_save.card_count; i-- > 0;) {
        if (!SEL_GET(f->sel, i)) continue;
        inv_remove(i);
        if (i < f->base) f->base--;
    }
    for (i = 0; i < SEL_BYTES; i++) f->sel[i] = 0;
    tbl_card(g_save.cards[f->base].id, &base);
    g_save.cards[f->base].lv = level_after(g_save.cards[f->base].lv,
                                           (u32)g_save.cards[f->base].exp + f->gain, &base, &left);
    g_save.cards[f->base].exp = left;
}

FAR(void, fusion_select_all_n, (FuseReq *f))
{
    CardRec c;
    u16 i;
    for (i = 0; i < g_save.card_count; i++) {
        if (i == f->base || inv_in_deck(i)) continue;
        tbl_card(g_save.cards[i].id, &c);
        if (c.rarity == RARITY_N) SEL_SET(f->sel, i);
    }
}

FAR(u8, evolve_partner_ok, (u16 base, u16 partner))
{
    CardRec c;
    if (base == partner || partner >= g_save.card_count || inv_in_deck(partner)) return 0;
    if (g_save.cards[base].id != g_save.cards[partner].id) return 0;
    tbl_card(g_save.cards[base].id, &c);
    return (u8)(c.evo_to != 0);
}

FAR(u8, evolve, (u16 base, u16 partner))
{
    CardRec c;
    if (!evolve_partner_ok(base, partner)) return F_NOT_EVO;
    if (g_save.gold < GOLD_PER_MAT) return F_NO_GOLD;
    tbl_card(g_save.cards[base].id, &c);
    g_save.gold -= GOLD_PER_MAT;
    g_save.cards[base].id = c.evo_to;
    g_save.cards[base].lv = 1;
    g_save.cards[base].exp = 0;
    inv_remove(partner);
    return F_OK;
}
