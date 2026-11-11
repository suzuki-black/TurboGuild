/* SPDX-License-Identifier: MIT */
/* opsim.c — 運営モードの模型（opsim.h）。数字はすべて「史実の選択で本編の年表に近い形になる」ように
 * tests/sim_ops.c で合わせた（DECISIONS #88）。★1 日に 1000 人なめるので、32bit の掛け算・割り算を
 * 人ごとの処理に書かない（日ごとの計算でだけ使う）。 */
#include "opsim.h"
#include "tables.h"
#include "../config.h"
#include "../data/tables.h"
#include "../data/text_ids.h"
#include "../far.h"

OpsState g_ops;

#define F_CLS     0x03
#define F_RETIRED 0x04
#define F_CONV    0x08           /* 誘惑で微課金に転ぶことがある無課金 */
#define F_PAID    0x10           /* 転んだ（いまは微課金） */
#define F_JOINED  0x20           /* 入った（e[4] は入ったときの不満） */
#define CHUNK     32
#define PERM      389            /* 入る順: 添字 i の人は (i × PERM) mod 1000 番目に入る（1000 と互いに素） */

/* 会議ごとの行動。選択肢 1〜3 の順 */
#define A_NONE     0
#define A_RATEUP2  1
#define A_RATEUP3  2
#define A_INFLATE  3             /* 新しい SSR を強くする（インフレ） */
#define A_RARITY   4             /* 新しいレア度 */
#define A_GIFT300  5
#define A_GIFT500  6
#define A_GIFT1000 7
#define A_GIFT3000 8
#define A_PUSH     9             /* プラットフォームの一押し枠・相互送客 */
#define A_AD1      10            /* 広告（史実の予算と打ち切り） */
#define A_AD2      11            /* 広告（大きな予算で、CVR が低くても粘る） */

typedef struct { u16 day; u8 n; u8 act[OPS_OPTIONS]; } Meet;

/* 0: 排出率（3% / 1% / 6%）と 1: 天井（なし / あり）は最初に決めて、ずっと効く。
 * 広告は 0 日目（開始の宣伝）と 95 日目（新しいカードの宣伝）。予算と打ち切りの CVR は ads[] */
static const Meet meets[OPS_MEETINGS] = {
    {   0, 3, { A_NONE, A_NONE, A_NONE } },
    {   0, 2, { A_NONE, A_NONE, A_NONE } },
    {   0, 3, { A_AD1, A_AD2, A_NONE } },
    {  14, 3, { A_RATEUP2, A_NONE, A_PUSH } },
    {  40, 2, { A_INFLATE, A_NONE, A_NONE } },
    {  60, 3, { A_RATEUP2, A_NONE, A_PUSH } },
    {  90, 3, { A_RARITY, A_NONE, A_RATEUP3 } },
    {  95, 3, { A_AD1, A_AD2, A_NONE } },
    { 110, 3, { A_GIFT500, A_NONE, A_GIFT3000 } },
    { 140, 3, { A_RATEUP3, A_NONE, A_PUSH } },
    { 180, 3, { A_RARITY, A_NONE, A_GIFT1000 } },
    { 240, 0, { A_NONE, A_NONE, A_NONE } },
};
#define AD_LAUNCH 2              /* 開始の宣伝の会議 */

/* 広告: 予算（万円）・打ち切る CVR（0.01%）。[開始の宣伝 / 新しいカードの宣伝][史実 / 大きな予算] */
typedef struct { u16 budget; u16 cut; } Ad;
static const Ad ads[2][2] = {
    { { OPS_AD_L1_MAN, OPS_AD_CUT1 }, { OPS_AD_L2_MAN, OPS_AD_CUT2 } },
    { { OPS_AD_N1_MAN, OPS_AD_CUT1 }, { OPS_AD_N2_MAN, OPS_AD_CUT2 } },
};
#define RATE_LOW  1              /* pick[0] */
#define RATE_HIGH 2
#define PITY_ON   1              /* pick[1] */

/* 型ごとの数字: 毎日の飽き、1 回の課金（百円）、ふだんの課金する気（/256） */
static const u8 bore[OPS_CLASSES]   = { OPS_BORE_FREE, OPS_BORE_LIGHT, OPS_BORE_HEAVY, OPS_BORE_WHALE };
static const u8 spend[OPS_CLASSES]  = { 0, OPS_SPEND_LIGHT, OPS_SPEND_HEAVY, OPS_SPEND_WHALE };
static const u8 desire[OPS_CLASSES] = { 0, OPS_WANT_LIGHT, OPS_WANT_HEAVY, OPS_WANT_WHALE };

static u16 rnd(void)
{
    u16 x = g_ops.rng;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    return g_ops.rng = x;
}

static void add(u8 c, s16 v)
{
    s16 s = (s16)g_ops.stress[c];
    if (v < 0 && s < -v) g_ops.stress[c] = 0;
    else g_ops.stress[c] = (u16)(s + v);
}

static void add_all(s16 free_, s16 light, s16 heavy, s16 whale)
{
    add(OPS_FREE, free_); add(OPS_LIGHT, light); add(OPS_HEAVY, heavy); add(OPS_WHALE, whale);
}

FAR(void, ops_start, (void))
{
    u8 buf[CHUNK * OPS_REC], *p = (u8 *)&g_ops;
    u16 i, k;
    NpcRec r;
    for (i = 0; i < sizeof(OpsState); i++) p[i] = 0;
    g_ops.rng = 0x2011;
    g_ops.reg = OPS_PREREG;
    for (i = 0; i < 1000; i += CHUNK) {
        for (k = 0; k < CHUNK && i + k < 1000; k++) {
            u8 *e = buf + k * OPS_REC;
            u16 h = rnd(), h2 = rnd();
            u8 a, c;
            tbl_npc(i + k, &r);
            a = r.activity;
            c = (u8)h < OPS_PCT_WHALE ? OPS_WHALE : (u8)h < OPS_PCT_HEAVY ? OPS_HEAVY :
                (u8)h < OPS_PCT_LIGHT ? OPS_LIGHT : OPS_FREE;
            if (r.flags & 2) c = OPS_WHALE;                  /* ランカーは石油王 */
            e[0] = c;
            if (c == OPS_FREE && (u8)(h >> 8) < OPS_PCT_CONV) e[0] |= F_CONV;
            e[1] = (u8)(OPS_TOL_BASE + (h2 & 127) + (a >> 1) > 255 ? 255 : OPS_TOL_BASE + (h2 & 127) + (a >> 1));
            e[2] = (u8)(OPS_CONV_BASE + (h2 >> 9));
            e[3] = a;
            e[4] = 0;
        }
        ops_mem_write((u16)(i * OPS_REC), (u8)(k * OPS_REC), buf);
    }
}

/* じっくりの日々の判断（DECISIONS #98）。会議の日とは重ねない。選択肢 1 が当時ありがちだった対応 */
#define I_MAINT  0               /* 障害でメンテ。告知の仕方と詫び石 */
#define I_CS0    1               /* CS: 石が減った */
#define I_CS1    2               /* CS: 子どもが勝手に課金した。返金してほしい */
#define I_CS2    3               /* CS: アカウントを乗っ取られた */
#define I_CS3    4               /* CS: 引退します、楽しかった */
#define I_BOSS0  5               /* 社長: 来週もう 1 本イベントをやれ */
#define I_BOSS1  6               /* 社長: 知り合いの描いたカードを入れろ */
#define I_BOSS2  7               /* 社長: 広告をもっと打て */
typedef struct { u16 day; u8 kind; } Inc;
static const Inc incs[OPS_INCS] = {
    {   8, I_CS0 }, {  20, I_MAINT }, {  27, I_CS1 }, {  45, I_BOSS0 }, {  52, I_CS2 }, {  70, I_MAINT },
    {  83, I_CS3 }, { 101, I_CS0 }, { 118, I_MAINT }, { 130, I_CS1 }, { 150, I_BOSS1 }, { 160, I_CS2 },
    { 185, I_MAINT }, { 200, I_CS1 }, { 205, I_BOSS2 }, { 225, I_CS3 },
};

static u8 meeting_today(void) { return (u8)(g_ops.next < OPS_MEETINGS && meets[g_ops.next].day == g_ops.day); }

FAR(u8, ops_meeting, (OpsMeeting *m))
{
    const Meet *t;
    if (meeting_today()) {
        t = &meets[g_ops.next];
        m->day = t->day;
        m->n = t->n;
        m->text = (u16)(T_OPS_M0 + g_ops.next * OPS_TEXTS);
        return 1;
    }
    if (g_ops.slow && g_ops.inc < OPS_INCS && incs[g_ops.inc].day == g_ops.day) {
        m->day = g_ops.day;
        m->n = OPS_OPTIONS;
        m->text = (u16)(T_OPS_I0 + incs[g_ops.inc].kind * OPS_TEXTS);
        return 1;
    }
    return 0;
}

static void act(u8 a)
{
    switch (a) {
    case A_RATEUP2: case A_RATEUP3:
        g_ops.boost = OPS_BOOST_DAYS;
        g_ops.boost_x = (u8)(a == A_RATEUP2 ? 2 : 3);
        add_all(-OPS_RATEUP_RELIEF, -OPS_RATEUP_RELIEF / 2, 0, 0);
        break;
    case A_INFLATE:
        add_all(OPS_INFL_FREE, OPS_INFL_LIGHT, OPS_INFL_HEAVY, 0);
        g_ops.fresh = 0;
        g_ops.spike = OPS_SPIKE_DAYS;
        break;
    case A_RARITY:
        add_all(OPS_RARE_FREE, OPS_RARE_LIGHT, OPS_RARE_HEAVY, OPS_RARE_WHALE);
        g_ops.fresh = 0;
        g_ops.spike = OPS_SPIKE_DAYS * 2;
        g_ops.tempt += OPS_RARE_TEMPT;
        break;
    case A_PUSH:
        g_ops.push = OPS_PUSH_DAYS;
        break;
    case A_AD1: case A_AD2: {
        const Ad *d = &ads[g_ops.next == AD_LAUNCH ? 0 : 1][a - A_AD1];
        g_ops.ad_left = (u32)d->budget * 10000;
        g_ops.ad_cut = d->cut;
        g_ops.ad_cvr = g_ops.next == AD_LAUNCH ? OPS_AD_CVR_LAUNCH : OPS_AD_CVR_LATER;
        g_ops.ad_on = 1;
        g_ops.ad_stop = 0;
        break;
    }
    case A_GIFT300: case A_GIFT500: case A_GIFT1000: case A_GIFT3000: {
        static const u8 tens[4] = { 3, 5, 10, 30 };   /* 百個 */
        static const u8 feel[4] = { 3, 5, 9, 16 };    /* 嬉しさは配った数ほどには増えない */
        u8 h = tens[a - A_GIFT300];
        s16 v = (s16)(feel[a - A_GIFT300] * OPS_GIFT_RELIEF);
        add_all(-v, -v, -v / 2, -v / 4);
        g_ops.glut = (u8)(h * OPS_GLUT_DAYS > 255 ? 255 : h * OPS_GLUT_DAYS);
        break;
    }
    default: break;
    }
}

/* 日々の判断の効き目（DAU と売上に直接） */
static void inc_act(u8 kind, u8 pick)
{
    switch (kind) {
    case I_MAINT:                                            /* どれを選んでも今日はログインが半分 */
        g_ops.outage = 1;
        if (pick == 0) { add_all(OPS_I_MAINT_EXT, OPS_I_MAINT_EXT, OPS_I_MAINT_EXT, OPS_I_MAINT_EXT); act(A_GIFT300); }   /* 短く言って延長を重ね、詫び石 */
        else if (pick == 1) add_all(OPS_I_MAINT_LONG, OPS_I_MAINT_LONG, OPS_I_MAINT_LONG, OPS_I_MAINT_LONG);   /* 長めに言って早く開ける */
        else { add_all(OPS_I_MAINT_EXT, OPS_I_MAINT_EXT, OPS_I_MAINT_EXT, OPS_I_MAINT_EXT); act(A_GIFT1000); }   /* 大盤振る舞い */
        break;
    case I_CS0:
        if (pick == 0) add_all(-OPS_I_CS, -OPS_I_CS, 0, 0);                         /* 調べて補填 */
        else if (pick == 2) add_all(OPS_I_CS * 2, OPS_I_CS * 2, OPS_I_CS, OPS_I_CS / 2);   /* 放置 */
        break;
    case I_CS1:
        if (pick == 0) {                                                            /* 返金する */
            g_ops.sales_total = g_ops.sales_total > OPS_I_REFUND ? g_ops.sales_total - OPS_I_REFUND : 0;
            add_all(-OPS_I_CS, -OPS_I_CS, 0, 0);
        } else if (pick == 1) add_all(OPS_I_CS, OPS_I_CS, OPS_I_CS, OPS_I_CS);   /* 規約どおり断る */
        else { g_ops.cap = 1; add_all(-OPS_I_CS * 2, -OPS_I_CS * 2, 0, 0); }      /* 月額の上限を入れる */
        break;
    case I_CS2:
        if (pick == 0) add_all(-OPS_I_CS, -OPS_I_CS, -OPS_I_CS, -OPS_I_CS);
        else if (pick == 1) add_all(OPS_I_CS / 2, OPS_I_CS / 2, OPS_I_CS / 2, OPS_I_CS / 2);
        else add_all(OPS_I_CS * 2, OPS_I_CS * 2, OPS_I_CS * 2, OPS_I_CS * 2);
        break;
    case I_CS3:
        if (pick == 0) add_all(-OPS_I_CS / 2, -OPS_I_CS / 2, -OPS_I_CS / 2, -OPS_I_CS / 2);   /* 丁寧に返す */
        break;
    case I_BOSS0:
        if (pick == 0) { g_ops.event = 5; add_all(-OPS_EVENT_RELIEF / 2, -OPS_EVENT_RELIEF / 2, OPS_I_RUSH, OPS_I_RUSH); }   /* 急いだぶん出来が悪い */
        else if (pick == 2) { g_ops.event = 3; add_all(-OPS_EVENT_RELIEF / 4, -OPS_EVENT_RELIEF / 4, 0, 0); }
        break;
    case I_BOSS1:
        if (pick == 0) { g_ops.spike = 5; add(OPS_HEAVY, OPS_I_RUSH * 2); add(OPS_WHALE, OPS_I_RUSH * 2); }   /* 弱くて浮いたカード */
        else if (pick == 1) g_ops.tempt = g_ops.tempt > OPS_I_RUSH ? g_ops.tempt - OPS_I_RUSH : 0;   /* 断ると新しいカードが 1 枚減る */
        else { g_ops.spike = 5; add(OPS_HEAVY, OPS_I_RUSH); add(OPS_WHALE, OPS_I_RUSH); }   /* 限定ガチャの隅で */
        break;
    case I_BOSS2:
        if (pick == 0) {                                     /* 打つ（古い広告なので CVR は低め） */
            g_ops.ad_left = (u32)OPS_I_AD_MAN * 10000;
            g_ops.ad_cut = OPS_AD_CUT2;
            g_ops.ad_cvr = OPS_AD_CVR_LATER / 2;
            g_ops.ad_on = 1;
            g_ops.ad_stop = 0;
        } else if (pick == 2) g_ops.push = OPS_PUSH_DAYS;    /* 送客を頼む */
        break;
    default: break;
    }
}

FAR(void, ops_decide, (u8 pick))
{
    if (!meeting_today()) {                                  /* じっくりの日々の判断 */
        if (g_ops.slow && g_ops.inc < OPS_INCS && incs[g_ops.inc].day == g_ops.day) {
            inc_act(incs[g_ops.inc].kind, (u8)(pick < OPS_OPTIONS ? pick : 0));
            g_ops.inc++;
        }
        return;
    }
    if (g_ops.next >= OPS_MEETINGS) return;
    if (g_ops.next == OPS_VERDICT) {
        u32 pts = (g_ops.profit_judge > 0 ? (u32)g_ops.profit_judge / OPS_CONT_PROFIT : 0) + (u32)g_ops.dau_judge * 100 / OPS_CONT_DAU;
        g_ops.points = (u16)(pts > 999 ? 999 : pts);
        g_ops.verdict = (u8)(g_ops.points >= OPS_CONT_POINTS ? OPS_CONTINUE : OPS_END);
        if (g_ops.verdict == OPS_END) {
            add_all(OPS_END_SHOCK, OPS_END_SHOCK, OPS_END_SHOCK, OPS_END_SHOCK);
            g_ops.ad_on = 0;                                  /* 終わるサービスに広告は打たない */
        }
        else add_all(-OPS_CONT_RELIEF, -OPS_CONT_RELIEF, -OPS_CONT_RELIEF, -OPS_CONT_RELIEF);
    } else {
        const Meet *t = &meets[g_ops.next];
        if (pick >= t->n) pick = 0;
        g_ops.pick[g_ops.next] = pick;
        act(t->act[pick]);
    }
    g_ops.next++;
}

/* その日に始まるイベント・対抗戦（年表どおり。会議では動かせない） */
static void calendar(void)
{
    u8 i;
    TimelineRec t;
    for (i = 0; i < TIMELINE_COUNT; i++) {
        tbl_timeline(i, &t);
        if (t.day != g_ops.day) continue;
        if (t.kind == TL_EVENT) {
            EventRec e;
            tbl_event((u8)t.param, &e);
            g_ops.event = e.duration;
            add_all(-OPS_EVENT_RELIEF, -OPS_EVENT_RELIEF, -OPS_EVENT_RELIEF / 2, -OPS_EVENT_RELIEF / 2);
        } else if (t.kind == TL_GUILDWAR) {
            add_all(-OPS_EVENT_RELIEF / 2, -OPS_EVENT_RELIEF / 2, -OPS_EVENT_RELIEF / 4, -OPS_EVENT_RELIEF / 4);
        }
    }
}

/* 毎日の不満: 飽き、排出率、天井、強いカードが出ない飽き、終了の告知の後の離脱 */
static void daily(void)
{
    u8 c;
    for (c = 0; c < OPS_CLASSES; c++) add(c, bore[c]);
    if (g_ops.pick[0] == RATE_LOW) add_all(OPS_LOW_FREE, OPS_LOW_FREE / 2, 1, 0);
    if (g_ops.pick[0] == RATE_HIGH) add_all(-OPS_HIGH_FREE, -OPS_HIGH_FREE / 2, 0, 0);
    if (g_ops.pick[1] == PITY_ON) { add(OPS_HEAVY, -OPS_PITY_RELIEF); add(OPS_WHALE, -OPS_PITY_RELIEF); }
    if (g_ops.fresh > OPS_FRESH_DAYS) {
        u8 v = (u8)((g_ops.fresh - OPS_FRESH_DAYS) >> 3);
        add(OPS_HEAVY, v); add(OPS_WHALE, v);
    }
    if (g_ops.verdict == OPS_END) add_all(OPS_END_DAILY, OPS_END_DAILY, OPS_END_DAILY, OPS_END_DAILY);
    g_ops.tempt += OPS_TEMPT_DAILY;
    if (g_ops.boost) g_ops.tempt += (u16)(g_ops.boost_x * OPS_TEMPT_BOOST);
    if (g_ops.pick[1] == PITY_ON || g_ops.pick[0] == RATE_HIGH) g_ops.tempt += OPS_TEMPT_PITY;   /* 当たると思えば転ぶ */
}

/* 型ごとの「今日課金する気」（/256）と 1 回の額（百円） */
static void want(u8 *w, u8 *amt)
{
    u8 c;
    for (c = 1; c < OPS_CLASSES; c++) {
        u16 v = desire[c], m = spend[c];
        if (g_ops.event) v = (u16)(v * 2);
        if (g_ops.boost) v = (u16)(v * g_ops.boost_x);
        if (g_ops.spike && c >= OPS_HEAVY) v = (u16)(v * 3);
        if (g_ops.fresh > OPS_FRESH_DAYS && c >= OPS_HEAVY) v = (u16)(v / 2);
        if (g_ops.glut) v = (u16)(v / 2);
        if (g_ops.verdict == OPS_END) v = (u16)(v / 2);
        if (g_ops.pick[0] == RATE_LOW) m = (u16)(m * 13 / 10);
        if (g_ops.pick[0] == RATE_HIGH) m = (u16)(m * 3 / 4);
        if (g_ops.pick[1] == PITY_ON && c >= OPS_HEAVY) m = (u16)(m * 4 / 5);
        if (g_ops.cap && c <= OPS_HEAVY) m = (u16)(m * 4 / 5);   /* 月額の上限 */
        w[c] = (u8)(v > 255 ? 255 : v);
        amt[c] = (u8)(m > 255 ? 255 : m);
    }
}

/* 今日入ってくる人（1/256 人）: 口コミ + 送客 + 広告。広告費もここで使う */
static void inflow(void)
{
    u16 pool = (u16)(1000 - g_ops.reg), n;
    u32 v = 0;
    g_ops.ad_day = 0;
    if (g_ops.verdict != OPS_END) {
        u16 wom = OPS_WOM;                                   /* 口コミ: 前の日の DAU × これ / 256 / 100 */
        if (g_ops.pick[0] == RATE_HIGH) wom += OPS_WOM_RATE;
        if (g_ops.pick[0] == RATE_LOW) wom = wom > OPS_WOM_RATE ? wom - OPS_WOM_RATE : 0;
        if (g_ops.pick[1] == PITY_ON) wom += OPS_WOM_PITY;
        v = (u32)g_ops.dau * wom * (OPS_DAYS + 30 - g_ops.day) / (OPS_DAYS + 30) / 100;   /* 古くなるほど細る */
        if (g_ops.push) v += (u32)OPS_PUSH_NEW * 256;
    }
    if (g_ops.ad_on) {
        u32 spend = g_ops.ad_left < OPS_AD_DAILY ? g_ops.ad_left : OPS_AD_DAILY;
        u16 cvr = (u16)((u32)g_ops.ad_cvr * pool / (1000 - OPS_PREREG));   /* 残っている層が薄いほど転ばない */
        if (cvr < g_ops.ad_cut) { g_ops.ad_on = 0; g_ops.ad_stop = OPS_AD_CVR; }
        else {
            v += spend / OPS_AD_CPC * cvr * 256 / 10000;     /* クリック × CVR */
            g_ops.ad_left -= spend;
            g_ops.ad_day = spend;
            g_ops.ad_cvr = (u16)((u32)g_ops.ad_cvr * OPS_AD_FATIGUE / 100);   /* 同じ広告は飽きられる */
            if (!g_ops.ad_left) { g_ops.ad_on = 0; g_ops.ad_stop = OPS_AD_SPENT; }
        }
    }
    v += g_ops.carry;
    n = (u16)(v >> 8);
    g_ops.carry = (u16)(v & 255);
    if (n > pool) n = pool;
    g_ops.fresh_users = n;
    g_ops.reg = (u16)(g_ops.reg + n);
}

FAR(void, ops_step, (void))
{
    u8 buf[CHUNK * OPS_REC], w[OPS_CLASSES], amt[OPS_CLASSES], lim[OPS_CLASSES];
    u16 i, dau = 0, payers = 0, retired = 0, pos = 0;
    u32 sales = 0;                                          /* 百円 */
    u8 k, c, n, bonus, conv;
    if (g_ops.day >= OPS_DAYS) return;
    calendar();
    daily();
    inflow();
    want(w, amt);
    for (c = 0; c < OPS_CLASSES; c++) {
        u16 s = g_ops.stress[c] >> 4;
        lim[c] = (u8)(s > 255 ? 255 : s);
    }
    conv = (u8)((g_ops.tempt >> 4) > 255 ? 255 : (g_ops.tempt >> 4));
    bonus = (u8)(g_ops.event ? OPS_ACT_EVENT : 0);
    for (i = 0; i < 1000; i += CHUNK) {
        n = (u8)(1000 - i < CHUNK ? 1000 - i : CHUNK);
        ops_mem_read((u16)(i * OPS_REC), (u8)(n * OPS_REC), buf);
        for (k = 0; k < n; k++) {
            u8 *e = buf + k * OPS_REC;
            u8 f = e[0], p, joined = (u8)(pos < g_ops.reg);
            pos += PERM;
            if (pos >= 1000) pos -= 1000;
            if (f & F_RETIRED) { retired++; continue; }
            if (!joined) continue;                                   /* まだ入っていない */
            c = (u8)(f & F_CLS);
            if (!(f & F_JOINED)) { f |= F_JOINED; e[0] = f; e[4] = lim[c]; }   /* 不満は入ったときから数える */
            if ((u8)(lim[c] - e[4]) >= e[1] && lim[c] > e[4]) {    /* 我慢の限界を超えた */
                e[0] = (u8)(f | F_RETIRED);
                retired++;
                continue;
            }
            if ((f & F_CONV) && !(f & F_PAID) && conv >= e[2]) { e[0] = (u8)((f & ~F_CLS) | F_PAID | OPS_LIGHT); c = OPS_LIGHT; }
            p = (u8)(OPS_ACT_BASE + (e[3] > 255 - OPS_ACT_BASE - OPS_ACT_EVENT ? 255 - OPS_ACT_BASE - OPS_ACT_EVENT : e[3]) + bonus);
            if (g_ops.outage) p >>= 1;                               /* 障害の日 */
            if ((u8)rnd() >= p) continue;
            dau++;
            if (c && (u8)(rnd() >> 8) < w[c]) { payers++; sales += amt[c]; }
        }
        ops_mem_write((u16)(i * OPS_REC), (u8)(n * OPS_REC), buf);
    }
    g_ops.dau = dau;
    g_ops.payers = payers;
    g_ops.retired = retired;
    g_ops.sales_day = sales * 100;
    g_ops.sales_total += g_ops.sales_day;
    g_ops.ad_total += g_ops.ad_day;
    if (g_ops.day + 1 == OPS_JUDGE_DAY) {
        g_ops.dau_judge = dau;
        g_ops.profit_judge = (s32)(g_ops.sales_total - g_ops.ad_total);
    }
    if (g_ops.fresh < 255) g_ops.fresh++;
    if (g_ops.boost) g_ops.boost--;
    if (g_ops.spike) g_ops.spike--;
    if (g_ops.glut) g_ops.glut--;
    if (g_ops.event) g_ops.event--;
    if (g_ops.push) g_ops.push--;
    g_ops.outage = 0;
    g_ops.day++;
}

/* 社内チャット（DECISIONS #98）: 15 日ごとの 7 日目に、模型の様子で 1 本。文は「名前：ひとこと」 */
FAR(u16, ops_chat, (void))
{
    u8 cat;
    if (g_ops.day % 15 != 7) return 0;
    if (g_ops.verdict == OPS_END) cat = 5;
    else if (g_ops.verdict == OPS_CONTINUE) cat = 4;
    else if (g_ops.day >= OPS_JUDGE_DAY - 15) cat = 3;      /* 判定が近い */
    else if (g_ops.dau < OPS_CHAT_LOW_DAU) cat = 2;
    else if (g_ops.sales_day >= OPS_CHAT_GOOD_SALES) cat = 1;
    else cat = 0;
    return (u16)(T_OPS_CH0 + cat * 3 + (g_ops.day / 15) % 3);
}
