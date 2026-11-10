/* SPDX-License-Identifier: MIT */
/* mates.c — ギルドのメンバー一覧とミニメール、去り方の問い合わせ（board.h, DECISIONS #96）。バンク 100 */
#include "mates_w.h"
#include "../data/text_ids.h"
#include "../far.h"

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

FAR(void, board_roster, (Roster *r))
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

