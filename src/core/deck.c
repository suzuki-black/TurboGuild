/* deck.c — 1 枚の値 = 基礎 × (100 + Lv×5)/100 × 属性% × スキル% × インフレ%（仕様 §6）。
 * インフレ乗数は Phase3 で年表から入る。いまは 100%。 */
#include "deck.h"
#include "player.h"
#include "save.h"
#include "tables.h"
#include "../far.h"

#define INFLATION_PCT 100

/* 炎(0)>雷(2)>水(1)>炎、光(3)⇔闇(4) は互いに有利 */
static const u8 beats[ELEMENTS] = { 2, 0, 1, 4, 3 };

FAR(u8, elem_pct, (u8 mine, u8 enemy))
{
    if (enemy >= ELEMENTS) return 100;
    if (beats[mine] == enemy) return ELEM_GOOD_PCT;
    if (mine < 3 && beats[enemy] == mine) return ELEM_BAD_PCT;
    return 100;
}

static u32 scale(u32 v, u8 pct)
{
    return v * pct / 100;
}

static u32 card_value(u16 index, u8 want_def, u8 enemy, CardRec *c)
{
    const OwnedCard *o = &g_save.cards[index];
    u32 v;
    tbl_card(o->id, c);
    v = want_def ? c->def : c->atk;
    v = v * (100 + (u16)o->lv * 5) / 100;
    if (c->skill == (want_def ? 2 : 1)) v = scale(v, 100 + SKILL_PCT);
    if (!want_def && enemy != ELEM_NONE) v = scale(v, elem_pct(c->element, enemy));
    return scale(v, INFLATION_PCT);
}

FAR(u32, card_atk_of, (u16 index))
{
    CardRec c;
    return card_value(index, 0, ELEM_NONE, &c);
}

FAR(u32, card_def_of, (u16 index))
{
    CardRec c;
    return card_value(index, 1, ELEM_NONE, &c);
}

FAR(void, deck_calc, (DeckStats *st))
{
    CardRec c;
    u8 d;
    st->atk = st->def = 0;
    st->cost = 0;
    st->count = st->ap_minus = st->drop_bonus = 0;
    for (d = 0; d < DECK_SIZE; d++) {
        u8 i = g_save.deck[d];
        if (i == DECK_EMPTY || i >= g_save.card_count) continue;
        st->atk += card_value(i, 0, st->enemy_elem, &c);
        st->def += card_value(i, 1, st->enemy_elem, &c);
        st->cost += c.cost;
        st->count++;
        if (c.skill == 3) st->ap_minus = 1;
        if (c.skill == 4) st->drop_bonus += SKILL_PCT;
    }
}

static u8 used(u16 i, u8 n)
{
    u8 d;
    for (d = 0; d < n; d++) if (g_save.deck[d] == i) return 1;
    return 0;
}

/* 毎回「入る中で攻撃最大」を選ぶ貪欲法。200 枚 × 5 回で十分速い */
FAR(void, deck_auto, (void))
{
    u16 cost_max = PLAYER_COST_MAX(g_save.level);
    u16 cost = 0;
    u8 d;
    CardRec c;
    for (d = 0; d < DECK_SIZE; d++) g_save.deck[d] = DECK_EMPTY;
    for (d = 0; d < DECK_SIZE; d++) {
        u32 best = 0;
        u16 pick = 0xFFFF, i;
        u8 pick_cost = 0;
        for (i = 0; i < g_save.card_count; i++) {
            u32 v;
            if (used(i, d)) continue;
            v = card_value(i, 0, ELEM_NONE, &c);
            if (cost + c.cost <= cost_max && v > best) {
                best = v;
                pick = i;
                pick_cost = c.cost;
            }
        }
        if (pick == 0xFFFF) break;
        g_save.deck[d] = (u8)pick;
        cost += pick_cost;
    }
}
