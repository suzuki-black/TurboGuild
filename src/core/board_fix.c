/* SPDX-License-Identifier: MIT */
/* board_fix.c — 伝言板の決まった時刻の書き込み（形見分け・復帰・整理・ギルマスの交代・誕生日・体験入隊・オフ会）。
 * 伝言板（board.c, バンク 98）が埋まったので、時間帯ごとにここ（バンク 101）を呼ぶ（DECISIONS #97） */
#include "mates_w.h"
#include "../data/text_ids.h"
#include "../far.h"

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

FAR(u8, board_fixed, (Work *w, BoardPost *p))
{
    if (fixed_at(w, w->s, p)) return 1;
    return (u8)(quiet_at(w, w->s) ? 2 : 0);
}
