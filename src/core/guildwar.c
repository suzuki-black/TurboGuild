/* SPDX-License-Identifier: MIT */
/* guildwar.c — ギルド対抗戦（guildwar.h）。
 * 組み合わせ: 第 m 戦はギルド g と g xor k（k = 1..15 を戦ごとに変える）。xor なので全ギルドが 1 対 1 になる。
 * ギルドの点 = 所属 NPC（辞めていない人）の戦力の合計 / 64 × 揺らぎ 80〜120%。
 * 自分のギルドには、その戦で自分が稼いだ点と応援（+3%）が足される。 */
#include "guildwar.h"
#include "event.h"
#include "player.h"
#include "ranking.h"
#include "rng.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "timeline.h"
#include "../far.h"

static const u32 slot_sec[2] = { 43200UL, 75600UL };     /* 12:00 と 21:00 */

/* 掛け算・割り算を使わない 16bit ハッシュ（ranking.c と同じ形） */
static u16 hash(u16 a, u16 b)
{
    u16 x = (u16)(a ^ (b << 5) ^ (b >> 3) ^ 0x9E37u);
    x ^= x << 7;
    x ^= x >> 9;
    x += x << 3;
    x ^= x << 8;
    x += b;
    x ^= x >> 7;
    x += x << 4;
    x ^= x >> 8;
    return x;
}

/* ギルドごとの戦力（その日の分を覚えておく。NPC 1000 人をなめるので毎回は数えない） */
static u32 str_cache[GUILD_COUNT];
static u16 str_day = 0xFFFF;

static const u32 *strengths(void)
{
    NpcRec r;
    NpcNow n;
    u16 i, d = (u16)time_gday(g_save.g_time);
    if (str_day == d) return str_cache;
    for (i = 0; i < GUILD_COUNT; i++) str_cache[i] = 0;
    for (i = 0; i < NPC_COUNT; i++) {
        tbl_npc(i, &r);
        npc_now(i, &n);
        if (!n.retired) str_cache[r.guild] += n.power;
    }
    str_day = d;
    return str_cache;
}

static u8 foe_of(u8 g, u8 m) { return (u8)(g ^ (1 + (m * 7 + g_save.gvg_war * 3) % 15)); }

/* NPC だけで決まるギルドの点 */
static u32 base_score(u8 g, u8 m)
{
    u32 s = strengths()[g] >> 6;
    return s * (80 + hash((u16)(m * 16 + g), (u16)(0x5A3Cu + g_save.gvg_war)) % 41) / 100;
}

static u8 my_guild(void) { return (u8)(g_save.guild ? g_save.guild - 1 : 0); }

static u32 our_score(u8 m)
{
    u32 b = base_score(my_guild(), m);
    u32 s = b;
    if (g_save.gvg_m == m) {
        s += g_save.gvg_pts;
        if (g_save.gvg_cheer) s += b / 100 * 3;
    }
    return s;
}

static u32 match_start(u8 m)
{
    return ((u32)g_save.gvg_start + (m >> 1)) * GSEC_PER_DAY + slot_sec[m & 1];
}

static void give(RewardList *g, u8 item, u16 amount)
{
    player_add_item(item, amount);
    if (g->n < REWARD_MAX) { g->item[g->n] = item; g->amount[g->n] = amount; g->n++; }
}

/* 16 ギルドの勝ち数を数えて、自分のギルドの順位（勝ち数が同じなら番号の小さい方が上） */
static u8 final_rank(void)
{
    u8 wins[GUILD_COUNT];
    u8 g, m, me = my_guild(), rank = 1;
    for (g = 0; g < GUILD_COUNT; g++) wins[g] = 0;
    for (m = 0; m < GVG_MATCHES; m++)
        for (g = 0; g < GUILD_COUNT; g++) {
            u8 f = foe_of(g, m);
            if (g == me) continue;                   /* 自分のギルドは実際の結果で */
            if (f == me) { if (!(g_save.gvg_wonbits & (1u << m))) wins[g]++; continue; }
            if (base_score(g, m) > base_score(f, m)) wins[g]++;
        }
    wins[me] = g_save.gvg_win;
    for (g = 0; g < GUILD_COUNT; g++)
        if (g != me && (wins[g] > wins[me] || (wins[g] == wins[me] && g < me))) rank++;
    return rank;
}

/* 順位の報酬（第 1 回 / 第 2 回で限定カードが違う） */
static void rank_reward(u8 rank, RewardList *g)
{
    static const u16 ssr[2] = { 118, 120 }, sr[2] = { 117, 119 };
    u8 w = (u8)(g_save.gvg_war >= 2);
    if (rank == 1) give(g, ITEM_CARD, ssr[w]);
    else if (rank <= 3) give(g, ITEM_CARD, sr[w]);
    else if (rank <= 8) give(g, ITEM_GEMS, 1000);
    else give(g, ITEM_GEMS, 300);
}

FAR(void, gvg_update, (GvgResult *r))
{
    TlState tl;
    u8 m, *p = (u8 *)r;
    u16 i;
    for (i = 0; i < sizeof(GvgResult); i++) p[i] = 0;
    if (!g_save.guild) return;
    timeline_state(&tl);
    if (tl.gvg && g_save.gvg_war != tl.gvg && g_save.gvg_last_war != tl.gvg) {
        g_save.gvg_war = tl.gvg;
        g_save.gvg_start = tl.gvg_start;
        g_save.gvg_done = 0;
        g_save.gvg_wonbits = 0;
        g_save.gvg_win = g_save.gvg_lose = 0;
        g_save.gvg_m = 0xFF;
        g_save.gvg_pts = 0;
        g_save.gvg_cheer = 0;
        r->joined = 1;
    }
    if (!g_save.gvg_war) return;
    for (m = 0; m < GVG_MATCHES; m++) {              /* 終わった戦の結果を順に出す */
        u16 bit = (u16)(1u << m);
        if (g_save.gvg_done & bit) continue;
        if (g_save.g_time < match_start(m) + GVG_SLOT_SECS) break;
        g_save.gvg_done |= bit;
        r->us = our_score(m);
        r->them = base_score(foe_of(my_guild(), m), m);
        r->n++;
        if (r->us > r->them) {
            r->win++;
            g_save.gvg_win++;
            g_save.gvg_wonbits |= bit;
        } else {
            g_save.gvg_lose++;
        }
    }
    if (r->win) give(&r->gain, ITEM_GEMS, (u16)(r->win * GVG_WIN_GEMS));   /* 勝った数だけ石 */
    if (g_save.gvg_done == (1u << GVG_MATCHES) - 1) { /* 全部終わった: 順位と報酬 */
        r->ended = 1;
        r->rank = final_rank();
        rank_reward(r->rank, &r->gain);
        g_save.gvg_last_war = g_save.gvg_war;
        g_save.gvg_last_rank = r->rank;
        g_save.gvg_war = 0;
    }
}

FAR(void, gvg_now, (GvgNow *n))
{
    u8 m;
    u32 t = g_save.g_time;
    n->war = g_save.gvg_war;
    n->now = 0;
    n->m = 0;
    n->us = n->them = 0;
    n->combo = 0;
    n->cheered = 0;
    n->left_min = 0;
    if (!n->war) return;
    for (m = 0; m < GVG_MATCHES; m++) {
        u32 s = match_start(m);
        if (t < s) { n->m = m; n->left_min = (u16)((s - t) / 60); break; }
        if (t < s + GVG_SLOT_SECS) {
            u32 el = (t - s) / 60;                   /* 経過（分） */
            n->now = 1;
            n->m = m;
            n->left_min = (u16)(60 - el);
            n->us = base_score(my_guild(), m) / 60 * el;
            n->them = base_score(foe_of(my_guild(), m), m) / 60 * el;
            if (g_save.gvg_m == m) {
                n->us += g_save.gvg_pts;
                n->cheered = g_save.gvg_cheer;
                if (g_save.gvg_cheer) n->us += base_score(my_guild(), m) / 100 * 3;
            }
            n->combo = (u8)(1 + hash((u16)(t / 60), 0x3C5Au) % 12);
            break;
        }
        n->m = (u8)(m + 1);
    }
    n->foe = foe_of(my_guild(), n->m < GVG_MATCHES ? n->m : GVG_MATCHES - 1);
}

FAR(u16, gvg_target, (u16 k))
{
    NpcRec r;
    NpcNow nn;
    GvgNow n;
    u16 i, tries, seen = 0;
    gvg_now(&n);
    i = hash((u16)(n.m * 3 + k), (u16)(g_save.g_time / 600)) % NPC_COUNT;   /* 10 分ごとに顔ぶれが替わる */
    for (tries = 0; tries < NPC_COUNT; tries++, i = (u16)((i + 1) % NPC_COUNT)) {
        tbl_npc(i, &r);
        if (r.guild != n.foe) continue;
        npc_now(i, &nn);
        if (nn.retired) continue;
        if (seen++ == (k - 1) * 2) return i;         /* 1 人おきに拾う */
    }
    return 0;
}

static void start_match(u8 m)
{
    if (g_save.gvg_m == m) return;
    g_save.gvg_m = m;
    g_save.gvg_pts = 0;
    g_save.gvg_cheer = 0;
}

FAR(void, gvg_attack, (u16 k, GvgAttack *a))
{
    GvgNow n;
    NpcNow foe;
    u32 mine, pp;
    gvg_now(&n);
    a->status = 0; a->win = 0; a->pts = 0; a->npc = 0; a->combo = n.combo;
    if (!n.now) { a->status = 1; return; }
    player_ap_update();
    if (g_save.ap < GVG_AP) { a->status = 2; return; }
    if (g_save.ap >= AP_MAX) g_save.ap_gtime = g_save.g_time;
    g_save.ap -= GVG_AP;
    start_match(n.m);
    a->npc = gvg_target(k);
    npc_now(a->npc, &foe);
    pp = player_power();
    mine = pp / 100 * (75 + rng_range(&g_save.rng_gacha, 61));
    a->win = (u8)(mine > foe.power);
    a->pts = (pp >> 6) * (100 + 5 * n.combo) / 100;  /* 1 回の攻撃 ≒ 仲間 1 人ぶん。コンボで +5% ずつ */
    if (!a->pts) a->pts = 1;
    if (!a->win) a->pts >>= 2;
    g_save.gvg_pts += a->pts;
}

FAR(u8, gvg_cheer, (void))
{
    GvgNow n;
    gvg_now(&n);
    if (!n.now) return 0;
    start_match(n.m);
    if (g_save.gvg_cheer) return 0;
    g_save.gvg_cheer = 1;
    return 1;
}
