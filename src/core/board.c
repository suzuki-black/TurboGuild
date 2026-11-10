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

#include "mates_w.h"

/* 時間帯 s に板にいる仲間の数と、k 以外で最初にいる人（いなければ NO_K） */
static u8 active_other(const Work *w, u16 s, u8 k, u8 *n)
{
    u8 j, first = NO_K;
    *n = 0;
    for (j = 0; j < w->nm; j++) {
        u8 jj = (u8)((k + 1 + j) % w->nm);
        if (gone_at(w, jj, s)) continue;
        (*n)++;
        if (jj != k && first == NO_K) first = jj;
    }
    return first;
}

/* d 日目に体験入隊してくる人（ギルドの仲間ではない NPC。いなければ BOARD_NOREF） */
static u16 newbie_of(u16 d)
{
    u16 h = hash(d, (u16)(SALT_NEWBIE + g_save.guild * 3));
    u16 i;
    if (d < 10 || d >= 239 || h % NEWBIE_EVERY) return BOARD_NOREF;
    i = (u16)(hash(h, SALT_NEWBIE) % NPC_COUNT);
    if (BOARD_IS_MATE(i)) i = (u16)((i + 1) % NPC_COUNT);
    return i;
}

/* d 日目の夜にオフ会の誘いがあるか（4 人以上いるときだけ） */
static u8 off_day(const Work *w, u16 d)
{
    u8 n, g;
    u16 s = (u16)(d * 4 + 3);
    if (d < 20 || d >= 230 || (d + g_save.guild * 7u) % OFF_EVERY || (hash(d, (u16)(SALT_OFF + g_save.guild)) & 1)) return 0;   /* 候補はギルドごとに 45 日に 1 日。その半分 */
    active_other(w, s, 0, &n);
    g = gm_of(w, s);
    return (u8)(n >= 4 && g != NO_K && !gone_at(w, g, s));   /* 誘うのはギルマス */
}

static void add(Work *w, u16 base, u8 count, u16 param)
{
    Topic *t;
    if (w->nt >= TOPICS_MAX) return;
    t = &w->t[w->nt++];
    t->base = base; t->count = count; t->param = param;
}

/* 年中行事。ゲーム内の暦は 0 日目 = 2011 年 7 月 1 日（夏祭りのイベントが 3 日目、元日が 184 日目） */
typedef struct { u16 day; u8 days; u16 base; u8 count; } Season;
static const Season season[] = {
    {  44, 3, T_BD_OBON0, 2 },    /* お盆 8/14〜16 */
    { 122, 1, T_BD_HALW0, 2 },    /* ハロウィン 10/31 */
    { 175, 2, T_BD_XMAS0, 3 },    /* クリスマス 12/24〜25 */
    { 183, 1, T_BD_NYE0,  2 },    /* 大みそか */
    { 184, 3, T_BD_NY0,   3 },    /* 正月 1/1〜3 */
    { 228, 1, T_BD_VAL0,  2 },    /* バレンタイン 2/14 */
};
#define SEASONS (sizeof(season) / sizeof(season[0]))

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
    for (i = 0; i < SEASONS; i++) {                      /* 年中行事（2 倍の重みで） */
        if (d >= season[i].day && d < season[i].day + season[i].days) {
            add(w, season[i].base, season[i].count, 0);
            add(w, season[i].base, season[i].count, 0);
        }
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
    if ((s & 3) == 3 && off_day(w, d)) {   /* ほかの決まった書き込みより先に */
        g = gm_of(w, s);   /* オフ会の誘い */
        put(p, BP_POST, w->m[g], d, BOARD_NOREF);
        p->text = p->alt = T_BD_OFF;
        return 1;
    }
    if (d >= OFF_AFTER && off_day(w, (u16)(d - OFF_AFTER)) && ((s & 3) == 1 || (s & 3) == 3)) {   /* 翌朝「楽しかった」、夜は自分にも */
        u8 n;
        g = active_other(w, s, (u8)(d % w->nm), &n);
        if (g != NO_K) {
            put(p, BP_POST, w->m[g], d, BOARD_NOREF);
            if ((s & 3) == 1) say(p, T_BD_OFFA0, 3, x); else p->text = p->alt = T_BD_OFFYOU;
            return 1;
        }
    }
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
    for (k = 0; k < w->nm; k++) {                        /* 誕生日: 朝に誰かが祝い、夜に本人が書く */
        u8 n;
        if (w->bd[k] != d || gone_at(w, k, s)) continue;
        if ((s & 3) == 1) {
            g = active_other(w, s, k, &n);
            if (g == NO_K) continue;
            put(p, BP_POST, w->m[g], d, w->m[k]);
            say(p, T_BD_BDAY0, 2, x);
            return 1;
        }
        if ((s & 3) == 3) { put(p, BP_POST, w->m[k], d, BOARD_NOREF); say(p, T_BD_BDME0, 2, x); return 1; }
    }
    {
        u16 nb = newbie_of(d);                           /* 体験入隊: 昼にあいさつして、そのまま来なくなる */
        if (nb != BOARD_NOREF && (s & 3) == 2) { put(p, BP_POST, nb, d, BOARD_NOREF); say(p, T_BD_NEWB0, 3, x); return 1; }
        nb = d >= 4 ? newbie_of((u16)(d - 4)) : BOARD_NOREF;
        if (nb != BOARD_NOREF && (s & 3) == 1 && (g = gm_of(w, s)) != NO_K && !gone_at(w, g, s)) {
            put(p, BP_POST, w->m[g], d, nb);
            p->text = p->alt = T_BD_NEWGONE;
            return 1;
        }
    }
    return 0;
}

/* オフ会の当日（誘いの OFF_AFTER - 1 日後）の午後と夜は、みんな出かけていて板が静か */
static u8 quiet_at(const Work *w, u16 s)
{
    u16 d = s >> 2;
    return (u8)((s & 3) >= 2 && d >= OFF_AFTER - 1 && off_day(w, (u16)(d - (OFF_AFTER - 1))));
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
    if (quiet_at(w, s)) return 0;
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
            else if (q.text >= T_BD_BDAY0 && q.text < T_BD_BDAY0 + 2) { if (q.ref == p->npc) say(p, T_BD_BDTY0, 2, x); else { p->ref = q.ref; say(p, T_BD_BDRE0, 2, x); } }
            else if (q.text >= T_BD_BDME0 && q.text < T_BD_BDME0 + 2) say(p, T_BD_BDRE0, 2, x);
            else if (q.text >= T_BD_NEWB0 && q.text < T_BD_NEWB0 + 3) say(p, T_BD_NEWRE0, 2, x);
            else if (q.text == T_BD_OFF) say(p, T_BD_OFFRE0, 2, x);
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
    u16 s = (u16)(g_save.g_time / BOARD_SLOT_SECS), look;
    u8 skip = l->skip, have = 0;
    l->n = 0;
    if (!g_save.guild || g_save.guild > GUILD_COUNT) return;
    setup(&w);
    /* 続けて同じ文にしない。q0 を出すときは、q1 を「その古い方（いま来た nx）と比べて直したもの」と比べる。
     * 古い方だけを見るので、新しい書き込みが来ても過去の文は変わらない */
    for (look = 0; look < BOARD_LOOKBACK && l->n < l->want && l->n < BOARD_MAX; look++, s--) {
        if (post_at(&w, s, &nx, 1)) {
            u8 g = gm_of(&w, s);
            nx.gm = (u8)(nx.kind != BP_SYS && g != NO_K && w.m[g] == nx.npc);
            nx.style = 0;
            if (nx.kind == BP_POST) {                    /* 文体: 半分ほどはふつう、2 割が半角ｶﾅ、残りが小さい「ぁぃぅ」 */
                u8 v = (u8)(hash(nx.npc, SALT_STYLE) % 20);
                nx.style = v < 11 ? 0 : v < 15 ? 1 : 2;
            }
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
