/* SPDX-License-Identifier: MIT */
/* ranking.c — 乱数の代わりに「NPC 番号と種から決まるハッシュ」を使うので、何度計算しても同じ。
 * ★1000 人ぶん回すので 32bit の割り算を使わない（Z80 では 1 回数千サイクル。最初は 1 回の順位計算に
 *   5 秒かかっていた）。倍率は先に「× 128 / 256 の整数」にしておき、シフトで割る。 */
#include "ranking.h"
#include "deck.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "timeline.h"
#include "../far.h"

#define DROP_WAVES 2

/* 1 回の計算の間だけ使う前提（年表から先に読んでおく） */
typedef struct {
    u16 day;
    u16 era;
    u16 era_k;          /* 総戦力の倍率: era / 10 を 128 倍した整数（× してから >> 7） */
    u16 event_k;        /* イベントポイントの倍率: 時代% × 経過割合 / 100（× してから >> 8） */
    u16 drop_day[DROP_WAVES];
    u8  drop_pct[DROP_WAVES];
    u16 event_id;
    u16 event_era;
    u16 prog;           /* イベントの経過割合 0..256 */
    u16 event_start;
} Ctx;

/* 掛け算も割り算も使わない 16bit ハッシュ（xor・シフト・足し算） */
static u16 hash(u16 a, u16 b)
{
    u16 x = (u16)(a ^ (b << 5) ^ (b >> 3) ^ 0x9E37u);
    x ^= x << 7;
    x ^= x >> 9;
    x += x << 3;            /* 足し算で線形（xor とシフトだけ）を崩す。崩さないと種を変えても判定が相関する */
    x ^= x << 8;
    x += b;
    x ^= x >> 7;
    x += x << 4;
    x ^= x >> 8;
    return x;
}

/* ★種（b）は大きく離れた値にする。1 違いだと結果が強く相関する（辞める人が偏った） */
#define SALT_RETIRE(w)  ((u16)(0x6D2Bu + (w) * 0x4F1Bu))
#define SALT_EVENT(e)   ((u16)(0x1F3Du + (e) * 0x3B9Fu))
#define SALT_FEED(k)    ((u16)(0x7A5Cu + (k) * 0x2E8Bu))

/* hash が pct% の確率で当たるか（% を使わず 16bit の範囲と比べる） */
static u8 chance(u16 h, u8 pct)
{
    return (u8)(h < (u16)(pct * 655u));
}

static void ctx_init(Ctx *c, u8 final_event)
{
    TlState tl;
    TimelineRec t;
    u8 i, w = 0;
    timeline_state(&tl);
    c->day = tl.day;
    c->era = tl.era_pct;
    c->era_k = (u16)((u32)tl.era_pct * 128 / 10);
    for (i = 0; i < DROP_WAVES; i++) { c->drop_day[i] = 0xFFFF; c->drop_pct[i] = 0; }
    for (i = 0; i < TIMELINE_COUNT && w < DROP_WAVES; i++) {
        tbl_timeline(i, &t);
        if (t.kind == TL_PLAYER_DROP) { c->drop_day[w] = t.day; c->drop_pct[w] = (u8)t.param; w++; }
    }
    c->event_id = g_save.event_id;
    c->prog = 256;
    c->event_era = 100;
    c->event_start = 0;
    if (c->event_id) {
        EventRec e;
        u32 start, len, now = g_save.g_time;
        tbl_event((u8)c->event_id, &e);
        c->event_era = e.era_pct;
        c->event_start = e.start_day;
        start = (u32)e.start_day * GSEC_PER_DAY;
        len = (u32)e.duration * GSEC_PER_DAY;
        if (!final_event) {
            if (now <= start) c->prog = 0;
            else if (now - start < len) c->prog = (u16)((now - start) / (len / 256));
        }
    }
    c->event_k = (u16)((u32)c->event_era * c->prog / 100);
}

/* 辞めた日（辞めていなければ 0xFFFF） */
static u16 retire_day(u16 i, const Ctx *c)
{
    u8 w;
    for (w = 0; w < DROP_WAVES; w++) {
        if (c->drop_day[w] > c->day) break;
        if (chance(hash(i, SALT_RETIRE(w)), c->drop_pct[w])) return c->drop_day[w];
    }
    return 0xFFFF;
}

/* 日数 × 活動度で伸びる（辞めた人は辞めた日で止まる）。16bit の範囲で計算する */
static u16 now_of(u16 i, const Ctx *c, const NpcRec *r, NpcNow *n)
{
    u16 rd = retire_day(i, c);
    u16 d = rd < c->day ? rd : c->day;
    u16 grow = (u16)(d * (r->activity >> 2));          /* ≒ 日数 × 活動度 / 4（300 × 63 まで） */
    u16 lv = (u16)(r->level + (grow >> 4));
    u16 p10 = (u16)(r->power + (grow >> 1));            /* 総戦力 / 10 */
    if (p10 < r->power) p10 = 0xFFFF;
    n->retired = (u8)(rd != 0xFFFF);
    n->level = lv > 150 ? 150 : lv;
    n->power = ((u32)p10 * c->era_k) >> 7;             /* × 10 × 時代% / 100 */
    return rd;
}

static u32 event_pt_of(u16 i, const Ctx *c, const NpcRec *r, u16 rd)
{
    u16 v;
    if (!c->event_id || !r->event_pt) return 0;
    if (rd != 0xFFFF && rd <= c->event_start) return 0;
    v = (u16)((r->event_pt >> 2) > 0xFFFF ? 0xFFFF : (r->event_pt >> 2));
    v = (u16)(((u32)v * (u8)(64 + (hash(i, SALT_EVENT(c->event_id)) & 127))) >> 7);   /* この人のこのイベントの腕前 */
    return (((u32)v * c->event_k) >> 8) << 2;          /* × 時代% × 経過割合、/4 を戻す */
}

FAR(void, npc_now, (u16 index, NpcNow *n))
{
    Ctx c;
    NpcRec r;
    ctx_init(&c, 0);
    tbl_npc(index, &r);
    now_of(index, &c, &r, n);
}

FAR(u32, player_power, (void))
{
    DeckStats d;
    deck_stats_init(&d, ELEM_NONE);
    deck_calc(&d);
    return d.atk + d.def;
}

/* 並べた小さな表に入れる（desc = 大きい順） */
static void insert(RankRow *rows, u8 *n, u8 cap, u16 npc, u32 v, u8 desc)
{
    u8 k = *n;
    if (k == cap) {
        if (desc ? v <= rows[cap - 1].value : v >= rows[cap - 1].value) return;
        k--;
    } else {
        (*n)++;
    }
    while (k > 0 && (desc ? rows[k - 1].value < v : rows[k - 1].value > v)) { rows[k] = rows[k - 1]; k--; }
    rows[k].npc = npc;
    rows[k].value = v;
}

static u32 value_of(u8 mode, u16 i, const Ctx *c, NpcNow *n)
{
    NpcRec r;
    u16 rd;
    tbl_npc(i, &r);
    rd = now_of(i, c, &r, n);
    return mode == RANK_POWER ? n->power : event_pt_of(i, c, &r, rd);
}

FAR(u16, ranking_final_rank, (void))
{
    Ctx c;
    NpcNow n;
    u32 points = g_save.event_pt;
    u16 i, above = 0;
    ctx_init(&c, 1);
    for (i = 0; i < NPC_COUNT; i++) {
        u32 v = value_of(RANK_EVENT, i, &c, &n);
        if (!n.retired && v > points) above++;
    }
    return (u16)(above + 1);
}

FAR(u16, npc_active_count, (void))
{
    Ctx c;
    NpcNow n;
    NpcRec r;
    u16 i, k = 0;
    ctx_init(&c, 0);
    for (i = 0; i < NPC_COUNT; i++) {
        tbl_npc(i, &r);
        now_of(i, &c, &r, &n);
        if (!n.retired) k++;
    }
    return k;
}

FAR(void, ranking_list, (RankList *l))
{
    Ctx c;
    NpcNow n;
    RankRow above[RANK_ROWS / 2], below[RANK_ROWS / 2];
    u8 na = 0, nb = 0, k;
    u16 i, count_above = 0, active = 0;
    ctx_init(&c, 0);
    l->player_value = l->mode == RANK_POWER ? player_power() : g_save.event_pt;
    l->n = 0;
    for (i = 0; i < NPC_COUNT; i++) {
        u32 v = value_of(l->mode, i, &c, &n);
        if (n.retired) continue;                     /* 辞めた人はランキングから消える */
        active++;
        if (v > l->player_value) {
            count_above++;
            insert(above, &na, RANK_ROWS / 2, i, v, 0);   /* 自分のすぐ上: 小さい順に 5 人 */
        } else {
            insert(below, &nb, RANK_ROWS / 2, i, v, 1);   /* 自分のすぐ下: 大きい順に 5 人 */
        }
        if (l->first == RANK_VIEW_TOP) insert(l->rows, &l->n, RANK_ROWS, i, v, 1);
    }
    l->player_rank = (u16)(count_above + 1);
    l->total = (u16)(active + 1);
    if (l->first == RANK_VIEW_TOP) {
        /* 自分が上位 10 に入るなら差し込む */
        if (l->player_rank <= RANK_ROWS) insert(l->rows, &l->n, RANK_ROWS, RANK_PLAYER, l->player_value, 1);
        for (k = 0; k < l->n; k++) l->rows[k].rank = (u16)(k + 1);
        for (k = 0; k < l->n; k++) if (l->rows[k].npc == RANK_PLAYER) l->rows[k].rank = l->player_rank;
        return;
    }
    for (k = na; k-- > 0;) {                          /* 上の人（遠い順に並べ直す） */
        RankRow *row = &l->rows[l->n++];
        *row = above[k];
        row->rank = (u16)(l->player_rank - 1 - k);
    }
    l->rows[l->n].npc = RANK_PLAYER;
    l->rows[l->n].value = l->player_value;
    l->rows[l->n].rank = l->player_rank;
    l->n++;
    for (k = 0; k < nb && l->n < RANK_ROWS; k++) {
        RankRow *row = &l->rows[l->n++];
        *row = below[k];
        row->rank = (u16)(l->player_rank + 1 + k);
    }
}

FAR(void, feed_line, (u16 k, FeedLine *f))
{
    Ctx c;
    NpcRec r;
    NpcNow n;
    u16 slot = (u16)(g_save.g_time / FEED_SLOT_SECS) - k;
    u16 i = hash(slot, SALT_FEED(1)) % NPC_COUNT, tries;
    ctx_init(&c, 0);
    for (tries = 0; tries < 16; tries++, i = (u16)((i + 37) % NPC_COUNT)) {   /* 辞めた人は出さない */
        tbl_npc(i, &r);
        now_of(i, &c, &r, &n);
        if (!n.retired) break;
    }
    f->npc = i;
    f->kind = (u8)(hash(slot, SALT_FEED(2)) % FEED_KINDS);
    if (c.day + 1 >= SERVICE_END_DAY) f->kind = FEED_BYE;  /* 最終日は全員が別れの挨拶 */
    f->param = (u16)(hash(slot, SALT_FEED(5)) % 4);          /* 挨拶の言い回し */
    switch (f->kind) {
    case FEED_SSR: {
        CardRec cr;
        u16 id, pick = hash(slot, SALT_FEED(3)) & 7, seen = 0;
        f->param = 39;
        for (id = 1; id <= tbl_card_count(); id++) {
            tbl_card(id, &cr);
            if (cr.rarity == RARITY_SSR && cr.event_id == 0 && cr.release_day <= c.day) {
                f->param = id;
                if (seen++ == pick) break;
            }
        }
        break;
    }
    case FEED_LEVEL:  f->param = n.level; break;
    case FEED_RANKUP: f->param = (u16)(1 + hash(slot, SALT_FEED(4)) % 100); break;
    case FEED_BYE:    break;
    default:          f->param = 0; break;
    }
}
