/* SPDX-License-Identifier: MIT */
/* quest.c — 負けても AP は減る（当時風）。報酬とドロップは勝ったときだけ。
 * ドロップはエリアの属性・レアリティ（通常 N / ボス R）の恒常カードから等確率。 */
#include "quest.h"
#include "deck.h"
#include "evfight.h"
#include "inventory.h"
#include "player.h"
#include "rng.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "../far.h"

FAR(u16, quest_max_stage, (void))
{
    u16 n = g_save.stage_cleared + 1;
    return n > STAGE_COUNT ? STAGE_COUNT : n;
}

/* 1 回の戦いの条件。通常の面もイベントの面もここに揃えてから同じ流れで戦う */
typedef struct {
    StageRec st;
    u8 element;
    u8 event;
    u16 points;
    u8 kind;            /* イベントの型（EV_*） */
} Fight;

/* ★イベントかどうかの分岐はここ 1 か所（仕様 §4.4）。デッキの特効もここで決める */
static u8 setup(u16 stage, Fight *f, DeckStats *d)
{
    f->event = 0;
    f->points = 0;
    f->kind = EV_HUNT;
    if (stage >= EVENT_STAGE_BASE) {
        EventStage es;
        if (!event_stage(stage - EVENT_STAGE_BASE, &es)) return 0;
        f->event = 1;
        f->kind = es.kind;
        f->points = es.points;
        f->element = es.element;
        f->st.ap = es.ap; f->st.hp = es.hp; f->st.def = es.def; f->st.atk = es.atk;
        f->st.exp = es.exp; f->st.gold = es.gold; f->st.drop_rate = 0; f->st.drop_tbl = 0;
        deck_stats_init(d, es.element);
        d->bonus_card = es.bonus_card; d->bonus_mul = es.bonus_mul;
        d->evo_card = es.evo_card; d->evo_mul = es.evo_mul;
    } else {
        AreaRec area;
        if (stage == 0 || stage > quest_max_stage()) return 0;
        tbl_stage(stage, &f->st);
        tbl_area((u8)((stage - 1) / STAGES_PER_AREA + 1), &area);
        f->element = area.element;
        deck_stats_init(d, area.element);
    }
    deck_calc(d);
    return 1;
}

FAR(u8, quest_ap_cost, (u16 stage))
{
    Fight f;
    DeckStats d;
    if (!setup(stage, &f, &d)) return 0;
    return (u8)(d.ap_minus && f.st.ap > 1 ? f.st.ap - 1 : f.st.ap);
}

static u8 droppable(const CardRec *c, u8 rarity, u8 element)
{
    return (u8)(c->rarity == rarity && c->element == element && c->event_id == 0 &&
                c->release_day <= time_gday(g_save.g_time));
}

static u16 pick_drop(u8 rarity, u8 element)
{
    CardRec c;
    u16 n = tbl_card_count(), id, hits = 0, k;
    for (id = 1; id <= n; id++) {
        tbl_card(id, &c);
        if (droppable(&c, rarity, element)) hits++;
    }
    if (hits == 0) return 0;
    k = rng_range(&g_save.rng_gacha, hits);
    for (id = 1; id <= n; id++) {
        tbl_card(id, &c);
        if (droppable(&c, rarity, element) && k-- == 0) return id;
    }
    return 0;
}

static void reward(u16 stage, const Fight *f, u8 drop_bonus, QuestResult *r)
{
    const StageRec *st = &f->st;
    r->exp = st->exp;
    r->gold = st->gold;
    g_save.gold += st->gold;
    r->levels = player_add_exp(st->exp);
    if (f->event) {
        r->points = f->points;
        event_add_points(f->points, &r->gain);
        return;
    }
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
    Fight f;
    DeckStats d;
    BattleIn in;
    u8 *p = (u8 *)r;
    u8 i;
    for (i = 0; i < sizeof(QuestResult); i++) p[i] = 0;
    if (!setup(stage, &f, &d)) { r->status = Q_LOCKED; return; }
    player_ap_update();
    r->ap_used = (u8)(d.ap_minus && f.st.ap > 1 ? f.st.ap - 1 : f.st.ap);
    if (g_save.ap < r->ap_used) { r->status = Q_NO_AP; return; }
    if (g_save.ap >= AP_MAX) g_save.ap_gtime = g_save.g_time;   /* 満タンから減らしたらここから数える */
    g_save.ap -= r->ap_used;
    r->event = f.event;
    r->enemy_elem = f.element;
    if (f.kind != EV_HUNT) { event_fight(stage - EVENT_STAGE_BASE, r); return; }   /* レイド・バトル */
    in.deck_atk = d.atk;
    in.deck_def = d.def;
    in.player_hp = r->player_hp = PLAYER_HP(g_save.level);
    in.enemy_hp = f.st.hp;
    in.enemy_def = f.st.def;
    in.enemy_atk = f.st.atk;
    battle_run(&in, &r->b);
    if (r->b.win) reward(stage, &f, d.drop_bonus, r);
}
