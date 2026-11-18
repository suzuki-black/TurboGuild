/* SPDX-License-Identifier: MIT */
/* trade.c — トレード掲示板（DECISIONS #115）。出品も買い取り希望も、6 時間の区切りと売り買いの回数から毎回決める（保存しない）。
 *   相場: レア度ごとの値（SSR はプレミアガチャで引く期待値より少し安い 2 万 4 千個）を、新しいレア度が出るたびに半分に、
 *         終了の告知の後はさらに 4 分の 1 に（投げ売り）。
 *   出品: 相場の 85〜124%。8 本に 1 本は相場の 3 割の「格安」で、その多くは払っても届かない詐欺。
 *         TRADE_AUDIT_DAY から運営が出品を審査し、格安の詐欺は 4 本に 1 本に減る。
 *   買い取り希望: 相場の 8〜14%（安く買いたたかれる）。 */
#include "trade.h"
#include "save.h"
#include "tables.h"
#include "time.h"
#include "timeline.h"
#include "../data/cards.h"
#include "../data/tables.h"
#include "../far.h"

static const u32 base[RARITIES] = { 20, 100, 1000, 24000, 60000, 120000 };
static const u8 weight[RARITIES] = { 25, 35, 25, 10, 4, 1 };   /* 出てくるレア度の重み（%） */

static u32 mix(u32 h)
{
    h ^= h >> 15; h *= 0x2C1B3C6DUL;
    h ^= h >> 12; h *= 0x297A2D39UL;
    return h ^ (h >> 15);
}

static u32 market_at(u8 r, const TlState *tl)
{
    u32 v = base[r];
    if (r < tl->top_rarity) v >>= tl->top_rarity - r;
    if (tl->announce) v >>= 2;
    return v ? v : 1;
}

FAR(u32, trade_market, (u8 rarity))
{
    TlState tl;
    timeline_state(&tl);
    return market_at(rarity < RARITIES ? rarity : 0, &tl);
}

/* その日までに出たレア度 r のカードの n 番目（数えるだけなら n = 0xFF で数を返す） */
static u16 card_of(u8 r, u16 day, u16 n, u16 *count)
{
    CardRec c;
    u16 id, k = 0;
    for (id = 1; id <= CARD_COUNT_BUILT; id++) {
        if (!tbl_card(id, &c) || c.rarity != r || c.release_day > day) continue;
        if (k == n) return id;
        k++;
    }
    *count = k;
    return 0;
}

FAR(u8, trade_row, (u16 tk, TradeRow *o))
{
    u8 tab = (u8)(tk >> 8), k = (u8)tk;
    TlState tl;
    u32 h, m;
    u16 n, pick;
    u8 r, w;
    timeline_state(&tl);
    if (tl.ended) return 0;
    h = mix((g_save.g_time / TRADE_SLOT) * 64 + TRADE_SEQ() * 16 + tab * 8 + k + 0x7A3DUL);
    w = (u8)(h % 100);
    for (r = 0; r < RARITIES - 1 && w >= weight[r]; r++) w = (u8)(w - weight[r]);
    if (r > tl.top_rarity) r = tl.top_rarity;            /* まだ出ていないレア度は、いまの最上位に */
    for (;;) {                                         /* そのレア度のカードがまだ無ければ 1 つ下へ */
        card_of(r, tl.day, 0xFFFF, &n);
        if (n || !r) break;
        r--;
    }
    if (!n) return 0;
    pick = (u16)((h >> 8) % n);
    o->card = card_of(r, tl.day, pick, &n);
    o->rarity = r;
    o->npc = (u16)((h >> 16) % NPC_COUNT);
    m = market_at(r, &tl);
    h = mix(h);
    o->cheap = o->scam = 0;
    if (tab == TRADE_SELL) {
        o->price = m * (85 + h % 40) / 100;
        if (((h >> 8) & 7) == 0) {
            o->cheap = 1;
            o->price = m * 30 / 100;
            o->scam = (u8)(tl.day < TRADE_AUDIT_DAY ? (h >> 12) % 3 != 0 : ((h >> 12) & 3) == 0);
        }
    } else o->price = m * (8 + h % 7) / 100;
    if (o->price >= 100) o->price = o->price / 10 * 10;   /* 掲示板の値付けらしく端数を落とす */
    if (!o->price) o->price = 1;
    return 1;
}

FAR(void, trade_next, (void))
{
    g_save.flags = (u8)((g_save.flags & 0xF3) | (((TRADE_SEQ() + 1) & 3) << 2));
}
