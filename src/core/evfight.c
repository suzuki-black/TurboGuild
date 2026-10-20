/* SPDX-License-Identifier: MIT */
/* evfight.c — レイドとバトルの 1 回の戦い（evfight.h）。結果は QuestResult に入れ、
 * 画面はボス戦の画面をそのまま使う（倒せなくても与えた分のポイントが入る）。 */
#include "evfight.h"
#include "deck.h"
#include "event.h"
#include "player.h"
#include "ranking.h"
#include "rng.h"
#include "save.h"
#include "tables.h"
#include "../far.h"

FAR(u32, raid_max_hp, (u16 lv))
{
    EventStage es;
    if (!event_stage(1, &es)) return 1;
    return es.hp * (lv + 1) / 2;                 /* Lv が 1 上がるごとに Lv1 の半分ずつ増える */
}

FAR(void, raid_sync, (void))
{
    if (g_save.raid_event == g_save.event_id && g_save.raid_lv) return;
    g_save.raid_event = g_save.event_id;
    g_save.raid_lv = 1;
    g_save.raid_hp = raid_max_hp(1);
    g_save.raid_rescued = 0;
}

static void next_boss(void)
{
    if (g_save.raid_lv < 255) g_save.raid_lv++;
    g_save.raid_hp = raid_max_hp(g_save.raid_lv);
    g_save.raid_rescued = 0;
}

static u16 kill_bonus(void) { return (u16)(g_save.raid_lv * 5); }   /* 討伐ボーナス = Lv × 5 */

static void raid(const EventStage *es, u16 level, const DeckStats *d, QuestResult *r)
{
    BattleIn in;
    u16 turns;
    u32 dealt;
    raid_sync();
    r->raid_lv = g_save.raid_lv;
    r->raid_before = g_save.raid_hp;
    r->raid_max = raid_max_hp(g_save.raid_lv);
    in.deck_atk = level == 2 ? d->atk * RAID_FULL_MUL : d->atk;
    in.deck_def = d->def;
    in.player_hp = r->player_hp = PLAYER_HP(g_save.level);
    in.enemy_hp = g_save.raid_hp;
    in.enemy_def = es->def;
    in.enemy_atk = es->atk;
    battle_run(&in, &r->b);
    turns = r->b.turns < RAID_TURNS ? r->b.turns : RAID_TURNS;
    if (r->b.taken_per_turn) {                   /* 力尽きるまでのターンしか殴れない（最低 1 回） */
        u32 alive = (r->player_hp - 1) / r->b.taken_per_turn;
        if (alive < turns) turns = (u16)(alive ? alive : 1);
    }
    dealt = r->b.dmg_per_turn * turns;
    r->b.turns = turns;
    r->b.taken_total = r->b.taken_per_turn * turns;
    r->b.win = (u8)(dealt >= g_save.raid_hp);
    r->points = es->points;
    if (r->b.win) {
        r->points += kill_bonus();
        next_boss();
    } else {
        g_save.raid_hp -= dealt;
    }
}

FAR(void, pvp_foe, (u16 tier, PvpFoe *f))
{
    EventStage es;
    DeckStats d;
    u16 k = (u16)(g_save.pvp_seed * 7u + tier * 331u);
    f->npc = (u16)(k % NPC_COUNT);
    f->power = 0;
    if (!event_stage(tier, &es)) return;
    deck_stats_init(&d, es.element);
    d.bonus_card = es.bonus_card; d.bonus_mul = es.bonus_mul;
    d.evo_card = es.evo_card; d.evo_mul = es.evo_mul;
    deck_calc(&d);
    f->power = (d.atk + d.def) / 100 * es.hp;    /* hp 欄 = 相手の強さ（自分の何 %） */
}

static void pvp(const EventStage *es, u16 level, const DeckStats *d, QuestResult *r)
{
    PvpFoe f;
    u32 mine = (d->atk + d->def) / 100 * (75 + rng_range(&g_save.rng_gacha, 61));   /* 75〜135% の揺らぎ */
    pvp_foe(level, &f);
    r->foe = f.npc;
    r->player_hp = PLAYER_HP(g_save.level);
    r->b.win = (u8)(mine > f.power);
    r->b.turns = 3;                              /* 見せ方だけ: 3 ターンで決着（負けなら半分しか削れない） */
    r->raid_max = f.power + 3;
    r->b.dmg_per_turn = r->b.win ? f.power / 3 + 1 : f.power / 6 + 1;
    r->b.taken_per_turn = r->b.win ? 0 : r->player_hp / 3 + 1;
    r->b.taken_total = r->b.taken_per_turn * 3;
    r->points = r->b.win ? es->points : (u16)(es->points / 5);
    g_save.pvp_seed++;                           /* 戦うたびに相手の顔ぶれが替わる */
}

FAR(void, event_fight, (u16 level, QuestResult *r))
{
    EventStage es;
    DeckStats d;
    if (!event_stage(level, &es)) { r->status = Q_LOCKED; return; }
    deck_stats_init(&d, es.element);
    d.bonus_card = es.bonus_card; d.bonus_mul = es.bonus_mul;
    d.evo_card = es.evo_card; d.evo_mul = es.evo_mul;
    deck_calc(&d);
    r->kind = es.kind;
    r->enemy_elem = es.element;
    if (es.kind == EV_RAID) raid(&es, level, &d, r);
    else pvp(&es, level, &d, r);
    r->exp = es.exp;
    r->gold = es.gold;
    g_save.gold += es.gold;
    r->levels = player_add_exp(es.exp);
    event_add_points(r->points, &r->gain);
}

/* 救援依頼: 同じギルドの NPC（辞めていない人）が最大 3 人、最大 HP の 8〜15% ずつ削る */
FAR(void, raid_rescue, (RescueResult *r))
{
    NpcRec n;
    NpcNow now;
    u16 i, tries;
    u8 *p = (u8 *)r;
    for (i = 0; i < sizeof(RescueResult); i++) p[i] = 0;
    if (event_kind() != EV_RAID || g_save.raid_rescued || !g_save.guild) return;
    raid_sync();
    g_save.raid_rescued = 1;
    i = (u16)((g_save.raid_lv * 97u + g_save.raid_event * 13u) % NPC_COUNT);
    for (tries = 0; tries < NPC_COUNT && r->n < RESCUE_MAX; tries++, i = (u16)((i + 1) % NPC_COUNT)) {
        u32 d;
        tbl_npc(i, &n);
        if (n.guild != g_save.guild - 1) continue;
        npc_now(i, &now);
        if (now.retired) continue;
        d = raid_max_hp(g_save.raid_lv) / 100 * (8 + (i & 7));
        if (d >= g_save.raid_hp) d = g_save.raid_hp;
        r->npc[r->n] = i;
        r->dmg[r->n] = d;
        r->n++;
        g_save.raid_hp -= d;
        if (!g_save.raid_hp) {                   /* 仲間がとどめ: 発見者として半分のボーナス */
            r->kill = 1;
            r->points = (u16)(kill_bonus() / 2);
            next_boss();
            event_add_points(r->points, &r->gain);
            break;
        }
    }
}
