/* SPDX-License-Identifier: MIT */
/* board.c — ギルド伝言板（board.h, DECISIONS #85・#91）。1 日を 4 つ（夜中・朝・昼・夜）に分け、それぞれで高々 1 件書き込む。
 * 書き込む確率は「残っている仲間の数 / 8」なので、仲間が辞めていくほど板は静かになる。
 * 書き込みは上から順に決める:
 *   1. 引退の 3 日前からは半分が弱音。
 *   2. 自分（プレイヤー）の最近の出来事（g_save.pev）から 3 回は、仲間が反応する（出来事のあった時間帯は必ず。すぐ見られるように）。
 *   3. 4 回に 1 回は、1 つ前の時間帯の書き込みへの返事（「＞○○さん それな」。弱音には励まし、脱退には見送り）。
 *   4. 6 割がその日の状況（イベント・対抗戦・確率アップ・新しいレア度・サービス終了）。
 *   5. 残りは暮らしの話: 給料日・休日・小さな出来事・去った仲間を懐かしむ・課金コースの話・時間帯の世間話。
 * 文の # には人の名前（ref）、$ には自分の名前、% には話題（param）が入る（ui/news_ui.c）。
 * 仲間の去り方（DECISIONS #94, mate_fate）: 引退の理由で弱音と懐かしむ文が替わる。黙って消える人は弱音も脱退の知らせも無く、
 * 後でギルドマスターが整理する。ギルマスが去ると次の人が継ぎ（誰もいなければ自分）、1 割ほどは 1 度だけ戻ってきて 3 日いる。
 * 半分ほどは去る日の朝にカードを置いていく（受け取るのは steal.c の keep_tick）。決まった時刻の書き込みは fixed_at。
 * バンク 14 が埋まったので、このファイルだけバンク 98 に置き、ranking.c のハッシュと仲間の式の写しを持つ。 */
#include "board.h"
#include "event.h"
#include "memo.h"
#include "pev.h"
#include "save.h"
#include "tables.h"
#include "timeline.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../far.h"

#define SALT_MATE     0x5A17u
#define SALT_MATE_R   0x33C5u
#define SALT_FATE     0x2D71u
#define SALT_FATE2    0x61B3u
#define FADE_TEXTS    4           /* 去り方 1 つあたりの弱音の数（機種変の人には無い） */
#define PURGE_AFTER   14          /* 何も言わずに来なくなった人を、ギルマスが整理するまでの日数（+0〜7） */
#define NO_K          0xFF
#define SALT_POST(k)  ((u16)(0x4B1Du + (k) * 0x6C8Fu))
#define FADE_DAYS     3
#define TOPIC_PCT     60
#define NEW_DAYS      7           /* 新しいレア度の話題が続く日数 */
#define MATES_MAX     8
#define TOPICS_MAX    6
#define REACT_SLOTS   3           /* 自分の出来事に反応する時間帯の数 */
#define RE_TEXTS      3           /* 反応 1 種類の文の数 */

typedef struct { u16 base; u8 count; u16 param; } Topic;

typedef struct {
    u16 m[MATES_MAX];
    u16 rd[MATES_MAX];            /* 引退の日（BOARD_NEVER = 最後まで残る） */
    u16 f[MATES_MAX];             /* 去り方（mate_fate） */
    u16 rb[MATES_MAX];            /* 戻ってくる日（BOARD_NEVER = 戻らない） */
    u16 pg[MATES_MAX];            /* 何も言わずに来なくなった人を整理する日（それ以外は BOARD_NEVER） */
    u16 joined;                   /* 自分がギルドに入った日（0xFFFF = 分からない） */
    u8  nm;
    u16 day;                      /* t を求めた日（0xFFFF = まだ） */
    Topic t[TOPICS_MAX];
    u8  nt;
} Work;

/* ranking.c の npc_hash の写し（同じ式。伝言板は前と同じ書き込みになる） */
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

/* ranking.c の mate_retire_day の写し */
static u16 retire_day(u16 i)
{
    u16 h = hash(i, SALT_MATE_R);
    if ((u8)h < 77) return BOARD_NEVER;
    return (u16)(20 + (h >> 8) % 230);
}

/* 仲間の去り方（board.h の MR_* / MF_*）。引退の日は変えないので、ランキングや結末はこれまでどおり */
static u16 fate_of(u16 i)
{
    /* 理由の重み（/16）: 受験 2・就職 3・結婚 2・親 2・飽きた 3・ほかのゲーム 2・凍結 1・機種変 1 */
    static const u8 why[16] = { MR_EXAM, MR_EXAM, MR_JOB, MR_JOB, MR_JOB, MR_MARRY, MR_MARRY, MR_PARENT,
                                MR_PARENT, MR_BORED, MR_BORED, MR_BORED, MR_OTHER, MR_OTHER, MR_BANNED, MR_PHONE };
    u16 h = hash(i, SALT_FATE), h2 = hash(i, SALT_FATE2), rd = retire_day(i), f, back = 0;
    u8 r = why[(h >> 4) & 15];
    f = (u16)(r | (((hash(i, SALT_MATE) >> 2) & 3) << 5));
    if (r == MR_PHONE || ((h >> 8) & 7) == 0) f |= MF_GHOST;   /* 機種変の人と、ほかの 1 割ほどは黙って消える */
    else if (r != MR_BANNED && (h & 0x2000)) f |= MF_GIVER;     /* 残りの半分はカードを置いていく */
    if (r != MR_BANNED && rd != BOARD_NEVER && (h2 & 0xFF) < 26) {   /* 1 割は 1 度だけ戻ってくる（凍結の人は戻れない） */
        back = (u16)(20 + ((h2 >> 8) & 31));
        if (rd + back + BOARD_BACK_DAYS > SERVICE_END_DAY) back = 0;
    }
    return (u16)(f | (back << 8));
}

FAR(u16, mate_fate, (u16 i)) { return fate_of(i); }

/* 時間帯 s に、仲間 k はもう板にいない（引退の日の昼から） */
static u8 gone_at(const Work *w, u8 k, u16 s)
{
    u16 rd = w->rd[k], d = s >> 2;
    return (u8)(rd != BOARD_NEVER && (d > rd || (d == rd && (s & 3) >= 2)));
}

/* 時間帯 s に、去った仲間 k が戻ってきている */
static u8 back_at(const Work *w, u8 k, u16 s)
{
    u16 d = s >> 2;
    return (u8)(w->rb[k] != BOARD_NEVER && d >= w->rb[k] && d < w->rb[k] + BOARD_BACK_DAYS);
}

/* 時間帯 s のギルドマスター（仲間の並びで最初の、まだ役を持っている人。いなければ NO_K = 自分）。
 * 黙って消えた人は、整理される日の昼まで役を持ったまま */
static u8 gm_of(const Work *w, u16 s)
{
    u8 k;
    for (k = 0; k < w->nm; k++) {
        u16 pg = w->pg[k], d = s >> 2;
        if (pg == BOARD_NEVER) { if (!gone_at(w, k, s)) return k; }
        else if (d < pg || (d == pg && (s & 3) < 2)) return k;
    }
    return NO_K;
}

static void add(Work *w, u16 base, u8 count, u16 param)
{
    Topic *t;
    if (w->nt >= TOPICS_MAX) return;
    t = &w->t[w->nt++];
    t->base = base; t->count = count; t->param = param;
}

/* その日の話題を年表とイベントの表から集める */
static void topics(Work *w, u16 d)
{
    TimelineRec r;
    EventRec e;
    u8 i;
    w->day = d;
    w->nt = 0;
    for (i = 0; i < TIMELINE_COUNT; i++) {
        tbl_timeline(i, &r);
        if (r.day > d) break;                            /* 日付順 */
        if (r.kind == TL_ANNOUNCE_END) { w->nt = 0; add(w, T_BD_END0, 8, 0); return; }   /* 終わりの話でもちきり */
        if (r.kind == TL_NEW_RARITY && d < r.day + NEW_DAYS) add(w, T_BD_NEW0, 8, (u16)(T_RAR_0 + r.param));
        if (r.kind == TL_GUILDWAR && d < r.day + GVG_DAYS) add(w, T_BD_GVG0, 6, 0);
        if (r.kind == TL_GACHA_RATEUP && d < r.day + RATEUP_DAYS) add(w, T_BD_RATE0, 6, 0);
    }
    for (i = 1; i <= EVENT_COUNT; i++) {
        tbl_event(i, &e);
        if (d < e.start_day || d >= e.start_day + e.duration) continue;
        if (e.kind == EV_RAID) add(w, T_BD_RAID0, 8, e.name_text);
        else if (e.kind == EV_PVP) add(w, T_BD_PVP0, 6, e.name_text);
        else add(w, T_BD_HUNT0, 8, e.name_text);
    }
}

static void say(BoardPost *p, u16 base, u8 count, u16 x)
{
    p->text = (u16)(base + x % count);
    p->alt = (u16)(base + (x + 1) % count);          /* 続けて同じ文になったときの代わり */
}

/* 時間帯 s の、反応すべき自分の出来事（無ければ 0）。出来事のあった時間帯なら *first = 1 */
static const PlayerEv *event_at(u16 s, u8 *first)
{
    u8 j;
    for (j = 0; j < PEV_MAX; j++) {
        const PlayerEv *e = &g_save.pev[j];
        if (!e->kind || e->kind >= PEV_KINDS) continue;
        if (s >= e->slot && s < e->slot + REACT_SLOTS) { *first = (u8)(s == e->slot); return e; }
    }
    return 0;
}

static void put(BoardPost *p, u8 kind, u16 npc, u16 d, u16 ref)
{
    p->npc = npc; p->kind = kind; p->text = p->alt = 0; p->param = 0; p->day = d; p->ref = ref;
    p->tone = kind == BP_POST ? (u8)(hash(npc, SALT_MATE) & 3) : 0;
}

/* 去り方の決まった時刻の書き込み（形見分け・復帰・整理・ギルマスの交代）。無ければ 0 */
static u8 fixed_at(const Work *w, u16 s, BoardPost *p)
{
    u16 d = s >> 2, x = (u16)(s + d);
    u8 k, g;
    for (k = 0; k < w->nm; k++) {
        u16 rd = w->rd[k], f = w->f[k];
        if (rd == BOARD_NEVER) continue;
        if ((f & MF_GIVER) && s == rd * 4 + 1 && w->joined <= rd) {   /* 辞める日の朝: カードを置いていく */
            put(p, BP_POST, w->m[k], d, BOARD_NOREF);
            say(p, T_BD_KEEP0, 2, x);
            return 1;
        }
        if (w->rb[k] != BOARD_NEVER) {
            if (s == w->rb[k] * 4 + 1) {                 /* 戻ってきた朝 */
                put(p, BP_POST, w->m[k], d, BOARD_NOREF);
                if (MF_REASON(f) == MR_PHONE) p->text = p->alt = T_BD_RETPH;
                else say(p, T_BD_RET0, 3, x);
                return 1;
            }
            if (s == (w->rb[k] + BOARD_BACK_DAYS - 1) * 4 + 3) {   /* 最後の夜にまた去る */
                put(p, BP_POST, w->m[k], d, BOARD_NOREF);
                say(p, T_BD_REBYE0, 2, x);
                return 1;
            }
        }
        if (s == w->pg[k] * 4 + 2) {                     /* 黙って消えた人を、ギルマスが整理する */
            u8 was = gm_of(w, (u16)(s - 1));
            g = gm_of(w, s);
            if (g == NO_K) { if (was == k) { put(p, BP_SYS, 0, d, BOARD_NOREF); return 1; } continue; }
            if (gone_at(w, g, s)) continue;              /* ギルマスも来ていない */
            put(p, BP_POST, w->m[g], d, w->m[k]);
            if (was == k) p->text = p->alt = T_BD_GMGHOST;
            else say(p, T_BD_PURGE0, 3, x);
            return 1;
        }
        if (!(f & MF_GHOST) && s == rd * 4 + 3 && gm_of(w, (u16)(rd * 4 + 1)) == k) {   /* ギルマスが辞めた日の夜に、次の人が継ぐ */
            g = gm_of(w, s);
            if (g == NO_K) { put(p, BP_SYS, 0, d, BOARD_NOREF); return 1; }   /* 最後は自分に回ってくる */
            if (gone_at(w, g, s)) continue;
            put(p, BP_POST, w->m[g], d, w->m[k]);
            say(p, T_BD_GM0, 3, x);
            return 1;
        }
    }
    return 0;
}

/* 時間帯 s の書き込み。無ければ 0。reply = 0 のときは返事にしない（1 つ前を見に行くのは 1 段だけ） */
static u8 post_at(Work *w, u16 s, BoardPost *p, u8 reply)
{
    u16 d = s >> 2, act[MATES_MAX], h, h2, rd;
    u16 x = (u16)(s + d);                            /* 文の選び方: 隣の時間帯で必ず替わり、日ごとにもずれる */
    u8 k, na = 0, trait, first = 0;
    const PlayerEv *ev;
    for (k = 0; k < w->nm; k++) {
        rd = w->rd[k];
        if (rd == d && (s & 3) == 2 && !(w->f[k] & MF_GHOST)) {   /* 辞める日の昼に、脱退の知らせだけが流れる */
            put(p, BP_LEAVE, w->m[k], d, BOARD_NOREF);
            return 1;
        }
        if (!gone_at(w, k, s) || back_at(w, k, s)) act[na++] = w->m[k];
    }
    if (fixed_at(w, s, p)) return 1;
    h = hash(s, SALT_POST(0));
    ev = event_at(s, &first);
    if (!na || ((h & 7) >= na && !first)) return 0;  /* 自分の出来事のあった時間帯は必ず誰かが書く */
    p->npc = act[(h >> 3) % na];
    p->kind = BP_POST;
    p->day = d;
    p->param = 0;
    p->ref = BOARD_NOREF;
    trait = (u8)hash(p->npc, SALT_MATE);
    p->tone = trait & 3;
    h2 = hash(s, SALT_POST(1));
    for (k = 0; k < w->nm && w->m[k] != p->npc; k++) {}
    rd = w->rd[k];
    if (rd != BOARD_NEVER && rd >= d && rd - d <= FADE_DAYS && !(w->f[k] & MF_GHOST) && (h2 & 1)) {   /* 去り方ごとの弱音 */
        say(p, (u16)(T_BD_FADE0 + MF_REASON(w->f[k]) * FADE_TEXTS), FADE_TEXTS, x);
        return 1;
    }
    if (ev && (first || (h2 & 0x0100))) {           /* 自分の出来事への反応 */
        say(p, (u16)(T_BD_RE0 + (ev->kind - 1) * RE_TEXTS), RE_TEXTS, x);
        if (ev->kind == PEV_SSR && ev->param >= 1 && ev->param <= tbl_card_count()) {   /* セーブから来た番号は表の範囲に限る */
            CardRec c;
            tbl_card(ev->param, &c);
            p->param = c.name_text;
        }
        if ((ev->kind == PEV_STOLEN || ev->kind == PEV_RIVAL) && ev->param < NPC_COUNT) p->ref = ev->param;
        return 1;
    }
    if (reply && s && !((h2 >> 9) & 3)) {             /* 1 つ前の時間帯への返事 */
        BoardPost q;
        if (post_at(w, (u16)(s - 1), &q, 0) && q.kind != BP_SYS && q.npc != p->npc) {
            p->ref = q.npc;
            if (q.kind == BP_LEAVE) say(p, T_BD_BYE0, 3, x);
            else if (q.text >= T_BD_FADE0 && q.text < T_BD_FADE0 + (MR_COUNT - 1) * FADE_TEXTS) say(p, T_BD_CONSOLE0, 3, x);
            else if (q.text >= T_BD_RET0 && q.text <= T_BD_RETPH) say(p, T_BD_WB0, 3, x);       /* おかえり */
            else if (q.text >= T_BD_GM0 && q.text <= T_BD_GMGHOST) say(p, T_BD_GMRE0, 2, x);   /* よろしく */
            else if (q.text >= T_BD_PURGE0 && q.text < T_BD_PURGE0 + 3 && q.ref != p->npc) { p->ref = q.ref; say(p, T_BD_BYE0, 3, x); }
            else if (q.text >= T_BD_KEEP0 && q.text < T_BD_KEEP0 + 2) say(p, T_BD_CONSOLE0, 3, x);
            else say(p, T_BD_REPLY0, 8, x);
            return 1;
        }
    }
    if (w->day != d) topics(w, d);
    if (w->nt && (h2 >> 4) % 100 < TOPIC_PCT) {
        const Topic *t = &w->t[(h2 >> 11) % w->nt];
        say(p, t->base, t->count, x);
        p->param = t->param;
        return 1;
    }
    switch ((h2 >> 2) & 7) {                         /* 暮らしの話 */
    case 0:
        if (d % 30 == 0) { say(p, T_BD_PAYDAY0, 3, x); return 1; }          /* 給料日（30 日ごと） */
        break;
    case 1:
        if (d % 7 >= 5) { say(p, T_BD_WKND0, 4, x); return 1; }             /* 休日 */
        break;
    case 2:
        say(p, T_BD_STORY0, 8, x);
        return 1;
    case 3:
        for (k = 0; k < w->nm; k++) {                /* 去った仲間を懐かしむ（戻ってきている間は除く） */
            u8 j = (u8)((k + x) % w->nm);
            if (w->rd[j] >= d || back_at(w, j, s)) continue;
            p->ref = w->m[j];
            if (w->pg[j] != BOARD_NEVER && d <= w->pg[j]) say(p, T_BD_GHOST0, 3, x);   /* 整理される前は「最近見ないね」 */
            else if (x & 2) say(p, (u16)(T_BD_WHY0 + MF_REASON(w->f[j]) * 2), 2, x);  /* 去ったわけ */
            else say(p, T_BD_MISS0, 3, x);
            return 1;
        }
        break;
    default:
        break;
    }
    if (h2 & 0x0004) say(p, (u16)(T_BD_PAY0 + ((trait >> 2) & 3) * 2), 2, x);
    else say(p, (u16)(T_BD_HI0 + (s & 3) * 4), 4, x);
    return 1;
}

/* 古い方の older と同じ文なら代わりの文にする */
static void fix(BoardPost *cur, const BoardPost *older)
{
    if (cur->kind == BP_POST && older->kind == BP_POST && cur->text == older->text) cur->text = cur->alt;
}

static void out(BoardList *l, const BoardPost *p, u8 *skip)
{
    if (*skip) { (*skip)--; return; }
    if (l->n < l->want && l->n < BOARD_MAX) l->post[l->n++] = *p;
}

FAR(void, board_list, (BoardList *l))
{
    Work w;
    BoardPost q[2], nx;
    u16 s = (u16)(g_save.g_time / BOARD_SLOT_SECS), i, look;
    u8 skip = l->skip, have = 0;
    l->n = 0;
    if (!g_save.guild) return;
    w.nm = 0;
    w.day = 0xFFFF;
    w.joined = g_save.memo[MEMO_GUILD] ? (u16)(g_save.memo[MEMO_GUILD] - 1) : 0;
    for (i = (u16)((g_save.guild - 1) * 8 + 3); i < NPC_COUNT && w.nm < MATES_MAX; i += 128) {
        u16 rd = retire_day(i), f = fate_of(i);
        w.rd[w.nm] = rd;
        w.f[w.nm] = f;
        w.rb[w.nm] = MF_BACK(f) ? (u16)(rd + MF_BACK(f)) : BOARD_NEVER;
        w.pg[w.nm] = (rd != BOARD_NEVER && (f & MF_GHOST)) ? (u16)(rd + PURGE_AFTER + (hash(i, SALT_FATE2) >> 13)) : BOARD_NEVER;
        w.m[w.nm++] = i;
    }
    /* 続けて同じ文にしない。q0 を出すときは、q1 を「その古い方（いま来た nx）と比べて直したもの」と比べる。
     * 古い方だけを見るので、新しい書き込みが来ても過去の文は変わらない */
    for (look = 0; look < BOARD_LOOKBACK && l->n < l->want && l->n < BOARD_MAX; look++, s--) {
        if (post_at(&w, s, &nx, 1)) {
            u8 g = gm_of(&w, s);
            nx.gm = (u8)(nx.kind != BP_SYS && g != NO_K && w.m[g] == nx.npc);
            if (have == 2) {
                BoardPost q1f = q[1];
                fix(&q1f, &nx);
                fix(&q[0], &q1f);
                out(l, &q[0], &skip);
                q[0] = q[1];
                q[1] = nx;
            } else {
                q[have++] = nx;
            }
        }
        if (!s) break;
    }
    if (have == 2) { fix(&q[0], &q[1]); out(l, &q[0], &skip); out(l, &q[1], &skip); }
    else if (have == 1) out(l, &q[0], &skip);
}
