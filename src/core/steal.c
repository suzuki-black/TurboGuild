/* SPDX-License-Identifier: MIT */
/* steal.c — 盗み・盗まれ（steal.h） */
#include "steal.h"
#include "board.h"
#include "inventory.h"
#include "memo.h"
#include "pev.h"
#include "player.h"
#include "ranking.h"
#include "rng.h"
#include "tables.h"
#include "../config.h"
#include "../data/cards.h"
#include "../data/tables.h"
#include "../far.h"

#define NO_NPC 0xFFFFu

static u16 mix(u16 a, u16 b)
{
    u16 h = (u16)(a * 0x9E37u ^ b * 0x85EBu ^ 0x5A5Au);
    h ^= h >> 7;
    h = (u16)(h * 0x2C1Bu);
    h ^= h >> 9;
    return h;
}

static u16 today(void) { return (u16)((u32)g_save.g_time / GSEC_PER_DAY); }

/* 総戦力の順で、自分（me）より弱い最初の順位のあたり（64 位の粗さ。NPC の今は 1 人ぶんでも重いので 4 回で止める。
 * マイページに入るたびの計算を `make check-offline` の 3 秒に収めるため） */
static u16 rank_near(u32 me)
{
    NpcNow n;
    u16 lo = 0, hi = NPC_COUNT;
    while (hi - lo > 64) {
        u16 mid = (u16)((lo + hi) / 2);
        npc_now(tbl_npc_by_power(mid), &n);
        if (n.power > me) lo = (u16)(mid + 1);
        else hi = mid;
    }
    return lo;
}

/* 順位 rank のあたりで、まだ遊んでいる人（いなければ NO_NPC）。power に総戦力 */
static u16 active_near(s16 rank, u32 *power)
{
    NpcNow n;
    u8 k;
    if (rank < 0) rank = 0;
    if (rank >= NPC_COUNT) rank = NPC_COUNT - 1;
    for (k = 0; k < 16; k++, rank++) {
        u16 i;
        if (rank >= NPC_COUNT) rank = 0;
        i = tbl_npc_by_power((u16)rank);
        npc_now(i, &n);
        if (!n.retired) { *power = n.power; return i; }
    }
    return NO_NPC;
}

/* そのレア度の、d 日目に出回っている通常のカード（無ければ 1 段下げる）を h で 1 枚 */
static u16 card_on(u8 rar, u16 h, u16 d)
{
    CardRec c;
    u16 n = tbl_card_count(), id, hits;
    for (;;) {
        hits = 0;
        for (id = 1; id <= n; id++) {
            tbl_card(id, &c);
            if (c.rarity == rar && !c.event_id && c.release_day <= d) hits++;
        }
        if (hits || !rar) break;
        rar--;
    }
    if (!hits) return 1;
    hits = (u16)(h % hits);
    for (id = 1; id <= n; id++) {
        tbl_card(id, &c);
        if (c.rarity != rar || c.event_id || c.release_day > d) continue;
        if (!hits--) return id;
    }
    return 1;
}

static u16 card_of(u8 rar, u16 h) { return card_on(rar, h, today()); }

static u8 pct_of(u32 me, u32 their, u8 back)
{
    u32 p = their ? me * STEAL_PCT_EVEN / their : STEAL_PCT_MAX;
    if (p < STEAL_PCT_MIN) p = STEAL_PCT_MIN;
    if (p > STEAL_PCT_MAX) p = STEAL_PCT_MAX;
    if (back) p += STEAL_PCT_BACK;
    return (u8)(p > 95 ? 95 : p);
}

FAR(void, steal_list, (StealList *l))
{
    static const u8  hold[STEAL_TARGETS] = { RARITY_N, RARITY_R, RARITY_SR };   /* 格下・同格・格上が持っているカード */
    static const u16 up[STEAL_TARGETS] = { 200, 100, 10 };                     /* 1 つ上のレア度を持っている割合（/1000） */
    NpcNow n;
    StealRow *r;
    u16 base, h;
    u8 k, j;
    l->n = 0;
    l->me = player_power();
    for (k = 0; k < STOLEN_MAX; k++) {               /* 取り返し */
        const StolenRec *s = &g_save.stolen[k];
        if (!s->card || s->card > tbl_card_count() || s->npc >= NPC_COUNT) continue;   /* セーブから来た番号は表の範囲に限る */
        npc_now(s->npc, &n);
        if (n.retired) continue;
        r = &l->r[l->n++];
        r->npc = s->npc; r->card = s->card; r->lv = s->lv; r->back = (u8)(k + 1);
        r->power = n.power;
        r->pct = pct_of(l->me, n.power, 1);
    }
    base = rank_near(l->me);
    for (k = 0; k < STEAL_TARGETS; k++) {            /* 格下・同格・格上 */
        s16 rank;
        h = mix((u16)(today() * 3 + k), g_save.steal_seq);
        rank = (s16)base + (k == 0 ? 8 + (s16)(h % 25) : k == 1 ? (s16)(h % 7) - 3 : -8 - (s16)(h % 25));
        r = &l->r[l->n];
        r->npc = active_near(rank, &r->power);
        if (r->npc == NO_NPC) continue;
        for (j = 0; j < l->n && l->r[j].npc != r->npc; j++) {}
        if (j < l->n) continue;                      /* 取り返しの相手と同じ人は出さない */
        r->card = card_of((u8)(mix(h, 0x99u) % 1000 < up[k] ? hold[k] + 1 : hold[k]), mix(h, 0x77u));
        r->lv = 1;
        r->back = 0;
        r->pct = pct_of(l->me, r->power, 0);
        l->n++;
    }
}

FAR(u8, steal_try, (StealList *l, u16 k))
{
    StealRow *r = &l->r[k];
    u16 i;
    if (k >= l->n) return STEAL_FAIL;
    if (g_save.card_count >= CARD_MAX) return STEAL_FULL;
    player_ap_update();
    if (g_save.ap < STEAL_AP) return STEAL_NO_AP;
    if (g_save.ap >= AP_MAX) g_save.ap_gtime = g_save.g_time;
    g_save.ap -= STEAL_AP;
    g_save.steal_seq++;
    if (rng_range(&g_save.rng_sim, 100) >= r->pct) return STEAL_FAIL;
    i = inv_add(r->card);
    if (i == INV_FULL) return STEAL_FULL;
    g_save.cards[i].lv = r->lv;
    if (r->back) {                                   /* 取り返した: 記録から外して詰める */
        u8 j;
        for (j = (u8)(r->back - 1); j + 1 < STOLEN_MAX; j++) g_save.stolen[j] = g_save.stolen[j + 1];
        g_save.stolen[STOLEN_MAX - 1].card = 0;
    }
    return STEAL_OK;
}

/* デッキに入っていないカードを 2 枚選び、レア度の高い方の添字（無ければ INV_FULL） */
static u16 victim(u16 h)
{
    CardRec a, b;
    u16 i, n = 0, pick[2], k;
    for (i = 0; i < g_save.card_count; i++) if (!inv_in_deck(i)) n++;
    if (!n) return INV_FULL;
    for (k = 0; k < 2; k++) {
        u16 want = (u16)((k ? h >> 8 : h) % n);
        for (i = 0; i < g_save.card_count; i++) {
            if (inv_in_deck(i)) continue;
            if (!want--) break;
        }
        pick[k] = i;
    }
    tbl_card(g_save.cards[pick[0]].id, &a);
    tbl_card(g_save.cards[pick[1]].id, &b);
    return b.rarity > a.rarity ? pick[1] : pick[0];
}

FAR(u8, steal_tick, (void))
{
    u32 now = g_save.g_time / STEAL_SLOT_SECS, s;
    u16 mates = 0, gone = 0, i, d = today(), base = 0;   /* base: 自分の順位。デッキは盗まれず総戦力は変わらないので 1 回だけ求める */
    u8 p, got = 0;
    if (!g_save.steal_check || !(g_save.flags & SF_STARTED)) { g_save.steal_check = g_save.g_time; return 0; }
    s = g_save.steal_check / STEAL_SLOT_SECS;
    g_save.steal_check = g_save.g_time;
    if (now - s > STEAL_SLOTS_MAX) s = now - STEAL_SLOTS_MAX;
    if (g_save.guild) {                              /* 残っている仲間に守ってもらう */
        for (i = (u16)((g_save.guild - 1) * 8 + 3); i < NPC_COUNT && mates < 8; i += 128) {
            u8 rd = (u8)mate_info(i);
            mates++;
            if (rd != MATE_STAYS && rd <= d) gone++;
        }
        p = (u8)(STEAL_P_BASE + STEAL_P_PER_GONE * (gone + 8 - mates));
    } else {
        p = STEAL_P_NOGUILD;
    }
    while (++s <= now && got < STEAL_PER_CHECK) {
        u16 day = (u16)(s / 4), h, v;
        u32 pw;
        StolenRec rec;
        u8 j;
        if (day < STEAL_START_DAY) continue;
        if (day >= STEAL_END_DAY) break;
        h = mix((u16)s, (u16)(g_save.guild * 31 + g_save.rng_gacha));
        if ((u8)h >= p) continue;
        v = victim(mix(h, 0x1234u));
        if (v == INV_FULL) break;
        if (!got) base = rank_near(player_power());
        rec.npc = active_near((s16)(base + (s16)((h >> 8) % 41) - 20), &pw);
        if (rec.npc == NO_NPC) break;
        rec.card = g_save.cards[v].id;
        rec.lv = g_save.cards[v].lv;
        inv_remove(v);
        for (j = STOLEN_MAX - 1; j; j--) g_save.stolen[j] = g_save.stolen[j - 1];
        g_save.stolen[0] = rec;
        if (!g_save.memo[MEMO_STOLEN]) g_save.memo[MEMO_STOLEN] = (u16)(day + 1);
        if (g_save.stolen_new < 255) g_save.stolen_new++;
        pev_push(PEV_STOLEN, rec.npc);
        got++;
    }
    return got;
}

/* 形見分け: 去った日の昼（脱退の知らせが流れる時刻）を過ぎた仲間から、課金コースに合ったレア度のカードを 1 枚 */
FAR(u16, keep_tick, (void))
{
    static const u8 rar[4] = { RARITY_N, RARITY_R, RARITY_R, RARITY_SR };
    u16 i, d = today(), joined, c, f;
    u8 k = 0;
    if (!g_save.guild) return 0;
    joined = g_save.memo[MEMO_GUILD] ? (u16)(g_save.memo[MEMO_GUILD] - 1) : 0;
    for (i = (u16)((g_save.guild - 1) * 8 + 3); i < NPC_COUNT && k < 8; i += 128, k++) {
        u8 rd = (u8)mate_info(i), bit = (u8)(1 << k);
        if (rd == MATE_STAYS || (g_save.keep_got & bit) || rd < joined) continue;
        if (g_save.g_time < (u32)rd * GSEC_PER_DAY + 2 * STEAL_SLOT_SECS) continue;   /* 去る日の昼から */
        f = mate_fate(i);
        if (!(f & MF_GIVER)) continue;
        if (d > rd + KEEP_DAYS) { g_save.keep_got |= bit; continue; }                 /* 古すぎる（前の版のセーブ） */
        c = card_on(rar[MF_COURSE(f)], mix(i, 0x4B1Du), rd);
        if (inv_add(c) == INV_FULL) return 0;                                          /* いっぱいなら次に来たとき */
        g_save.keep_got |= bit;
        return (u16)(((u16)k << 12) | c);
    }
    return 0;
}
