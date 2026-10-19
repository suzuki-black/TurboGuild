/* ranking.c — NPC の値は大きい順に並んでいる（イベントは最終ポイント順、総戦力は添字表で並べ替え済み）。
 * 「経過割合」を全員に同じ倍率で掛けるので、並びは崩れず二分探索が使える。 */
#include "ranking.h"
#include "deck.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "../far.h"

/* イベントの経過割合（0..256）。終わったイベントなら 256 */
static u16 progress(void)
{
    EventRec e;
    u32 start, len, now = g_save.g_time;
    if (!g_save.event_id) return 256;
    tbl_event((u8)g_save.event_id, &e);
    start = (u32)e.start_day * GSEC_PER_DAY;
    len = (u32)e.duration * GSEC_PER_DAY;
    if (now <= start) return 0;
    if (now - start >= len) return 256;
    return (u16)((now - start) / (len / 256));
}

static u32 npc_value(u8 mode, u16 pos, u16 prog)
{
    NpcRec n;
    if (mode == RANK_POWER) {
        tbl_npc(tbl_npc_by_power(pos), &n);
        return (u32)n.power * 10;
    }
    tbl_npc(pos, &n);
    return n.event_pt * prog / 256;
}

/* 値が v より大きい NPC の人数（表は大きい順） */
static u16 count_above(u8 mode, u32 v, u16 prog)
{
    u16 lo = 0, hi = NPC_COUNT;
    while (lo < hi) {
        u16 mid = (u16)((lo + hi) / 2);
        if (npc_value(mode, mid, prog) > v) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

FAR(u32, player_power, (void))
{
    DeckStats d;
    deck_stats_init(&d, ELEM_NONE);
    deck_calc(&d);
    return d.atk + d.def;
}

u16 ranking_final_rank(u32 points)      /* 同じバンクの event.c だけが呼ぶので中継なし */
{
    return (u16)(count_above(RANK_EVENT, points, 256) + 1);
}

FAR(void, ranking_list, (RankList *l))
{
    u16 prog = progress();
    u16 r, first;
    l->player_value = l->mode == RANK_POWER ? player_power() : g_save.event_pt;
    l->player_rank = (u16)(count_above(l->mode, l->player_value, prog) + 1);
    l->total = NPC_COUNT + 1;
    first = l->first;
    if (first == 0) first = l->player_rank > RANK_ROWS / 2 ? (u16)(l->player_rank - RANK_ROWS / 2) : 1;
    if (first + RANK_ROWS - 1 > l->total) first = (u16)(l->total - RANK_ROWS + 1);
    l->n = 0;
    for (r = first; r < first + RANK_ROWS && r <= l->total; r++) {
        RankRow *row = &l->rows[l->n++];
        row->rank = r;
        if (r == l->player_rank) { row->npc = RANK_PLAYER; row->value = l->player_value; continue; }
        row->npc = (u16)(r < l->player_rank ? r - 1 : r - 2);
        row->value = npc_value(l->mode, row->npc, prog);
        if (l->mode == RANK_POWER) row->npc = tbl_npc_by_power(row->npc);
    }
}
