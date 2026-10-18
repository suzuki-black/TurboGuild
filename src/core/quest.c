/* quest.c — 負けても AP は減る（当時風）。報酬とドロップは勝ったときだけ。
 * ドロップはエリアの属性・レアリティ（通常 N / ボス R）の恒常カードから等確率。 */
#include "quest.h"
#include "deck.h"
#include "inventory.h"
#include "player.h"
#include "rng.h"
#include "save.h"
#include "tables.h"
#include "../far.h"

FAR(u16, quest_max_stage, (void))
{
    u16 n = g_save.stage_cleared + 1;
    return n > STAGE_COUNT ? STAGE_COUNT : n;
}

FAR(u8, quest_ap_cost, (u16 stage))
{
    StageRec st;
    DeckStats d;
    tbl_stage(stage, &st);
    d.enemy_elem = ELEM_NONE;
    deck_calc(&d);
    return (u8)(d.ap_minus && st.ap > 1 ? st.ap - 1 : st.ap);
}

static u16 pick_drop(u8 rarity, u8 element)
{
    CardRec c;
    u16 n = tbl_card_count(), id, hits = 0, k;
    for (id = 1; id <= n; id++) {
        tbl_card(id, &c);
        if (c.rarity == rarity && c.element == element && c.event_id == 0) hits++;
    }
    if (hits == 0) return 0;
    k = rng_range(&g_save.rng_gacha, hits);
    for (id = 1; id <= n; id++) {
        tbl_card(id, &c);
        if (c.rarity == rarity && c.element == element && c.event_id == 0 && k-- == 0) return id;
    }
    return 0;
}

static void reward(u16 stage, const StageRec *st, u8 drop_bonus, QuestResult *r)
{
    r->exp = st->exp;
    r->gold = st->gold;
    g_save.gold += st->gold;
    r->levels = player_add_exp(st->exp);
    if (rng_range(&g_save.rng_gacha, 100) < (u16)st->drop_rate + drop_bonus) {
        r->drop_card = pick_drop(DROP_RARITY(st->drop_tbl), DROP_ELEMENT(st->drop_tbl));
        if (r->drop_card && inv_add(r->drop_card) == INV_FULL) r->drop_lost = 1;
    }
    if (stage > g_save.stage_cleared) {
        g_save.stage_cleared = stage;
        r->area_cleared = (u8)(stage % STAGES_PER_AREA == 0);
    }
}

FAR(void, quest_run, (u16 stage, QuestResult *r))
{
    StageRec st;
    AreaRec area;
    DeckStats d;
    BattleIn in;
    u8 *p = (u8 *)r;
    u8 i;
    for (i = 0; i < sizeof(QuestResult); i++) p[i] = 0;
    if (stage == 0 || stage > quest_max_stage()) { r->status = Q_LOCKED; return; }
    player_ap_update();
    r->ap_used = quest_ap_cost(stage);
    if (g_save.ap < r->ap_used) { r->status = Q_NO_AP; return; }
    if (g_save.ap >= AP_MAX) g_save.ap_gtime = g_save.g_time;   /* 満タンから減らしたらここから数える */
    g_save.ap -= r->ap_used;

    tbl_stage(stage, &st);
    tbl_area((u8)((stage - 1) / STAGES_PER_AREA + 1), &area);
    r->enemy_elem = area.element;
    d.enemy_elem = area.element;
    deck_calc(&d);
    in.deck_atk = d.atk;
    in.deck_def = d.def;
    in.player_hp = r->player_hp = PLAYER_HP(g_save.level);
    in.enemy_hp = st.hp;
    in.enemy_def = st.def;
    in.enemy_atk = st.atk;
    battle_run(&in, &r->b);
    if (r->b.win) reward(stage, &st, d.drop_bonus, r);
}
