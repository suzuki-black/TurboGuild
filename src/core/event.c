/* SPDX-License-Identifier: MIT */
/* event.c — 参加中の id が開催中の id と違ったら「終わった」と判断して最終順位と順位報酬を配る。
 * 何日空けて起動しても同じ（期間をまたいだセーブを読んでも終了処理が走る, 仕様 Phase2 の受け入れ）。
 * ポイント報酬は届いた順に 1 回だけ（受け取り済みは event_rewards_claimed のビット）。 */
#include "event.h"
#include "player.h"
#include "ranking.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "../far.h"

static u8 open_now(const EventRec *e, u32 gday)
{
    return (u8)(gday >= e->start_day && gday < (u32)e->start_day + e->duration);
}

FAR(u16, event_active, (void))
{
    EventRec e;
    u32 gday = time_gday(g_save.g_time);
    u8 i;
    for (i = 1; i <= EVENT_COUNT; i++) {
        tbl_event(i, &e);
        if (open_now(&e, gday)) return i;
    }
    return 0;
}

FAR(u16, event_days_left, (void))
{
    EventRec e;
    u16 id = event_active();
    if (!id) return 0;
    tbl_event((u8)id, &e);
    return (u16)(e.start_day + e.duration - time_gday(g_save.g_time));
}

static void give(RewardList *g, u8 item, u16 amount)
{
    player_add_item(item, amount);
    if (g->n < REWARD_MAX) { g->item[g->n] = item; g->amount[g->n] = amount; g->n++; }
}

static void rank_reward(u8 tbl, u16 rank, RewardList *g)
{
    EvRewardRec r;
    u8 i;
    for (i = 0; i < EVREWARD_COUNT; i++) {          /* 閾値の小さい順。最初に入った枠の報酬 */
        tbl_evreward(i, &r);
        if (r.tbl == tbl && r.kind == REWARD_RANK && rank <= r.threshold) { give(g, r.item, r.amount); return; }
    }
}

static void finish(EventResult *r)
{
    EventRec e;
    tbl_event((u8)g_save.event_id, &e);
    r->ended = 1;
    r->id = g_save.event_id;
    r->points = g_save.event_pt;
    r->rank = ranking_final_rank();
    if (g_save.event_pt) rank_reward(e.reward_tbl, r->rank, &r->gain);
    g_save.last_event = g_save.event_id;
    g_save.last_rank = r->rank;
    g_save.last_event_pt = g_save.event_pt;
    g_save.event_id = 0;
    g_save.event_pt = 0;
    g_save.event_rewards_claimed = 0;
}

FAR(void, event_update, (EventResult *r))
{
    u16 cur = event_active();
    u8 *p = (u8 *)r;
    u8 i;
    for (i = 0; i < sizeof(EventResult); i++) p[i] = 0;
    if (g_save.event_id && g_save.event_id != cur) finish(r);
    if (cur && !g_save.event_id && g_save.last_event != cur) {
        g_save.event_id = cur;
        g_save.event_pt = 0;
        g_save.event_rewards_claimed = 0;
        r->started = 1;
        r->id = cur;
    }
}

FAR(u8, event_stage, (u16 level, EventStage *s))
{
    EventRec e;
    EvStageRec st;
    CardRec boss, bonus;
    u8 i;
    if (!g_save.event_id || g_save.event_id != event_active() || level < 1 || level > EVENT_LEVELS) return 0;
    tbl_event((u8)g_save.event_id, &e);
    for (i = 0; i < EVSTAGE_COUNT; i++) {
        tbl_evstage(i, &st);
        if (st.kind == e.kind && st.level == level) break;
    }
    if (i == EVSTAGE_COUNT) return 0;
    tbl_card(e.boss_card, &boss);
    tbl_card(e.bonus_card, &bonus);
    s->ap = st.ap;
    /* 後のイベントほど敵も強い。boss_pct = 時代% × その時点の最上位レア度の倍率（バトルは自分比なので掛けない） */
    s->hp = e.kind == EV_PVP ? st.hp : st.hp * e.boss_pct / 100;
    s->def = (u16)((u32)st.def * e.boss_pct / 100);
    s->atk = (u16)((u32)st.atk * e.boss_pct / 100);
    s->exp = st.exp; s->gold = st.gold; s->points = st.points;
    s->element = boss.element;
    s->boss_card = e.boss_card;
    s->bonus_card = e.bonus_card;
    s->evo_card = bonus.evo_to;
    s->bonus_mul = e.bonus_mul;
    s->evo_mul = e.evo_bonus_mul;
    s->kind = e.kind;
    return 1;
}

FAR(u8, event_kind, (void))
{
    EventRec e;
    if (!g_save.event_id) return EV_HUNT;
    tbl_event((u8)g_save.event_id, &e);
    return e.kind;
}

FAR(void, event_add_points, (u16 pts, RewardList *g))
{
    EventRec e;
    EvRewardRec r;
    u8 i, bit = 0;
    if (!g_save.event_id) return;
    tbl_event((u8)g_save.event_id, &e);
    g_save.event_pt += pts;
    for (i = 0; i < EVREWARD_COUNT; i++) {
        tbl_evreward(i, &r);
        if (r.tbl != e.reward_tbl || r.kind != REWARD_POINT) continue;
        if (g_save.event_pt >= r.threshold && !(g_save.event_rewards_claimed & (1 << bit))) {
            g_save.event_rewards_claimed |= (u8)(1 << bit);
            give(g, r.item, r.amount);
        }
        bit++;
    }
}

FAR(u32, event_next_line, (void))
{
    EventRec e;
    EvRewardRec r;
    u8 i;
    if (!g_save.event_id) return 0;
    tbl_event((u8)g_save.event_id, &e);
    for (i = 0; i < EVREWARD_COUNT; i++) {
        tbl_evreward(i, &r);
        if (r.tbl == e.reward_tbl && r.kind == REWARD_POINT && g_save.event_pt < r.threshold) return r.threshold;
    }
    return 0;
}
