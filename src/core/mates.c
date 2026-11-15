/* SPDX-License-Identifier: MIT */
/* mates.c — ギルドのメンバー一覧とミニメール、去り方の問い合わせ（board.h, DECISIONS #96）。バンク 100 */
#include "mates_w.h"
#include "deck.h"
#include "tables.h"
#include "../data/text_ids.h"
#include "../far.h"

u8 g_help;

#define SALT_HI    0x2B4Fu
#define SALT_HIR   0x6E1Du
#define SALT_LEAD  0x47A9u
#define HI_SECS    600UL          /* あいさつの時刻の刻み（ゲーム内 10 分） */
#define HI_PER_DAY ((u16)(GSEC_PER_DAY / HI_SECS))

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

/* 今日のあいさつの様子（HI_*）。黙って消えた人は来ないし返さない。去る日が近い人は返しにくい */
static u8 hi_of(const Work *w, u8 k, u16 s)
{
    u16 npc = w->m[k], d = s >> 2, rd = w->rd[k], h = hash(npc, (u16)(SALT_HI + d)), now = (u16)(g_save.g_time / HI_SECS), tin;
    u8 ghost = (u8)(w->pg[k] != BOARD_NEVER && gone_at(w, k, s));
    u8 fade = (u8)(rd != BOARD_NEVER && d + FADE_DAYS >= rd);
    u8 come = (u8)(!ghost && (h & 255) < (fade ? 48 : 150));         /* 向こうから来る日（ふだんは 6 割ほど） */
    tin = (u16)(d * HI_PER_DAY + 36 + (h >> 4) % 96);                 /* 6:00〜22:00 のどこか */
    if (hello_done()) {
        u16 at = (u16)(g_save.hello_at - 1), hr = hash(npc, (u16)(at ^ SALT_HIR));
        if (come && tin <= at) return HI_BACK;                         /* 来ていたのを返した */
        if (!ghost && (hr & 255) < (fade ? 80 : 216) && now >= at + 1 + (hr >> 8) % 24) return HI_BACK;   /* 10 分〜4 時間で返ってくる */
        return HI_SENT;
    }
    return (u8)(come && now >= tin ? HI_IN : HI_NONE);
}

static void roster_of(Roster *r)
{
    Work w;
    u16 s = (u16)(g_save.g_time / BOARD_SLOT_SECS), d = s >> 2;
    u8 k, g;
    r->n = 0;
    if (!g_save.guild || g_save.guild > GUILD_COUNT) return;
    setup(&w);
    g = gm_of(&w, s);
    for (k = 0; k < w.nm; k++) {
        RosterRow *o = &r->r[r->n];
        u16 t = hash(w.m[k], SALT_MATE), m;
        u8 ghost = (u8)(w.pg[k] != BOARD_NEVER && gone_at(&w, k, s));
        if (gone_at(&w, k, s) && !(ghost && (d < w.pg[k] || (d == w.pg[k] && (s & 3) < 2)))) continue;   /* 抜けた人と整理された人は載らない */
        o->npc = w.m[k];
        o->gm = (u8)(g == k);
        o->course = (u8)((t >> 2) & 3);
        o->tone = (u8)(t & 3);
        o->bd = w.bd[k];
        o->k = k;
        o->hi = hi_of(&w, k, s);
        if (ghost) {
            u32 a = (g_save.g_time - (u32)w.rd[k] * GSEC_PER_DAY) / 60;   /* 引退の日の朝から来ていない */
            o->last = a > 65535UL ? 65535u : (u16)a;
        } else {
            m = hash(s, w.m[k]);
            o->last = (u16)(m % 8 ? m % 180 : m % 600);                    /* たいていは 3 時間以内 */
        }
        r->n++;
    }
}

FAR(void, board_roster, (Roster *r)) { roster_of(r); }

FAR(u8, hello_done, (void))
{
    return (u8)(g_save.hello_at && (u16)(g_save.hello_at - 1) / HI_PER_DAY == (u16)(g_save.g_time / GSEC_PER_DAY));
}

FAR(u8, hello_send, (void))
{
    Roster r;
    if (hello_done()) return 0;
    roster_of(&r);
    if (r.n) g_save.hello_at = (u16)(g_save.g_time / HI_SECS + 1);
    return r.n;
}

/* 助っ人を借りた日の印（日 % 250 + 1） */
static u8 help_today(void) { return (u8)(g_save.g_time / GSEC_PER_DAY % 250 + 1); }

static u8 help_used(u8 k)
{
    return (u8)(g_save.help_day == help_today() && ((g_save.help_bits >> k) & 1));
}

/* 仲間のリーダーカード: 課金コースで狙うレア度を決め、その日までに出たカードから週ごとに選ぶ（無ければ 1 つ下のレア度） */
static void lead_of(u16 npc, MateLead *o)
{
    static const u8 want[8] = { RARITY_R, RARITY_SR, RARITY_SR, RARITY_SSR, RARITY_SSR, RARITY_USR, RARITY_XUSR, RARITY_XUSR };
    CardRec c;
    u16 day = (u16)(g_save.g_time / GSEC_PER_DAY), h = hash(npc, (u16)(SALT_LEAD + day / 7)), n = tbl_card_count(), id, cnt[RARITIES], pick;
    u8 course = (u8)((hash(npc, SALT_MATE) >> 2) & 3), r, i;
    for (i = 0; i < RARITIES; i++) cnt[i] = 0;
    for (id = 1; id <= n; id++) { tbl_card(id, &c); if (c.release_day <= day && c.rarity < RARITIES) cnt[c.rarity]++; }
    r = want[course * 2 + (h & 1)];
    while (r && !cnt[r]) r--;
    pick = cnt[r] ? (u16)((h >> 1) % cnt[r]) : 0;
    o->card = 0;
    for (id = 1; id <= n; id++) {
        tbl_card(id, &c);
        if (c.release_day <= day && c.rarity == r && pick-- == 0) { o->card = id; break; }
    }
    o->lv = (u8)(8 + course * 12 + (h >> 8) % 12);
    if (o->card && o->lv > c.max_lv) o->lv = c.max_lv;
}

FAR(void, mate_lead, (const RosterRow *r, MateLead *o))
{
    lead_of(r->npc, o);
    o->used = help_used(r->k);
}

/* いま一覧にいる、並び順 k の仲間（いなければ 0） */
static const RosterRow *row_of(const Roster *r, u8 k)
{
    u8 i;
    for (i = 0; i < r->n; i++) if (r->r[i].k == k) return &r->r[i];
    return 0;
}

FAR(u16, help_npc, (void))
{
    Roster r;
    const RosterRow *o;
    if (!g_help) return BOARD_NOREF;
    roster_of(&r);
    o = row_of(&r, (u8)(g_help & HELP_JOINED ? g_help & 7 : g_help - 1));
    return o ? o->npc : BOARD_NOREF;
}

/* 1 枚の攻撃 = 基礎 × (100 + Lv×5)/100 × スキル × 属性 × 時代（deck.c と同じ式。リーダースキルは載らない） */
static u32 atk_of(const MateLead *m, u8 enemy)
{
    CardRec c;
    u32 v;
    if (!m->card) return 0;
    tbl_card(m->card, &c);
    v = (u32)c.atk * (100 + (u16)m->lv * 5) / 100;
    if (c.skill == 1) v = v * (100 + SKILL_PCT) / 100;
    v = v * elem_pct(c.element, enemy) / 100;
    return v * c.era_pct / 100;
}

FAR(u32, help_atk, (u16 npc, u16 enemy))
{
    MateLead m;
    lead_of(npc, &m);
    return atk_of(&m, (u8)enemy);
}

/* 借りられたら g_help を HELP_JOINED | 並び順 にして、画面が「助けに来た」を出す */
FAR(u32, help_take, (u16 enemy))
{
    Roster r;
    const RosterRow *o;
    MateLead m;
    u8 k = (u8)(g_help - 1);
    if (!g_help || g_help > MATES_MAX) { g_help = 0; return 0; }
    g_help = 0;
    roster_of(&r);
    o = row_of(&r, k);
    if (!o || help_used(k)) return 0;
    lead_of(o->npc, &m);
    if (!m.card) return 0;
    if (g_save.help_day != help_today()) { g_save.help_day = help_today(); g_save.help_bits = 0; }
    g_save.help_bits |= (u8)(1 << k);
    g_help = (u8)(HELP_JOINED | k);
    return atk_of(&m, (u8)enemy);
}

static void mail_add(MailList *l, u16 npc, u16 day, u16 text)
{
    u8 j = l->n;
    if (j == MAIL_MAX) { if (day <= l->r[MAIL_MAX - 1].day) return; j--; } else l->n++;
    while (j && l->r[j - 1].day < day) { l->r[j] = l->r[j - 1]; j--; }   /* 新しい順 */
    l->r[j].npc = npc; l->r[j].day = day; l->r[j].text = text;
}

FAR(void, board_mail, (MailList *l))
{
    Work w;
    u16 s = (u16)(g_save.g_time / BOARD_SLOT_SECS), d = s >> 2;
    u8 k, g;
    l->n = 0;
    if (g_save.ask_day && d + 1 >= g_save.ask_day)       /* お問い合わせの翌日に、運営の定型の返事（ui/help_ui.c） */
        mail_add(l, MAIL_OPS, (u16)(g_save.ask_day - 1), (u16)(T_ML_ASK0 + (g_save.ask_kind & 3)));
    if (!g_save.guild || g_save.guild > GUILD_COUNT) return;
    setup(&w);
    if (g_save.memo[MEMO_GUILD]) {                       /* 入った日に、ギルマスからあいさつ */
        g = gm_of(&w, (u16)(w.joined * 4 + 1));
        if (g != NO_K) mail_add(l, w.m[g], w.joined, T_ML_HELLO);
    }
    for (k = 0; k < w.nm; k++) {
        u16 rd = w.rd[k], f = w.f[k], h = hash(w.m[k], SALT_FATE2);
        u8 r = MF_REASON(f);
        if (rd == BOARD_NEVER) {
            if (d >= SERVICE_END_DAY) mail_add(l, w.m[k], SERVICE_END_DAY, (u16)(T_ML_LAST0 + (h & 1)));   /* 最後まで残った人から */
            continue;
        }
        if (rd < w.joined) continue;                     /* 自分が入る前に去った人 */
        if (!(f & MF_GHOST) && r != MR_BANNED && (h & 0x10) && rd >= 1 && s >= (rd - 1) * 4 + 3)
            mail_add(l, w.m[k], (u16)(rd - 1), (u16)(T_ML_BYE0 + r));   /* 去る前の晩に、板には書かなかったわけ */
        if (r == MR_PHONE && d >= rd + 3) mail_add(l, w.m[k], (u16)(rd + 3), T_ML_PHONE);   /* 新しいアドレスから */
        if (d >= SERVICE_END_DAY && !(f & MF_GHOST) && r != MR_BANNED)
            mail_add(l, w.m[k], SERVICE_END_DAY, (u16)(T_ML_LASTG0 + (h & 1)));   /* もう遊んでいないけれど、終わると聞いて */
    }
}

