/* wallet.h — 軍資金（リアルマネーの代わりに一人ひとりが持つお財布, DECISIONS #33）。
 * 軍資金で石を買い（課金）、石でプレミアガチャや回復薬を買う。月に 1 回、コースの額が入る（給料日）。
 * 無敵モードのコースでは減らない（ソシャゲで無双した気分を味わうため）。 */
#ifndef CORE_WALLET_H
#define CORE_WALLET_H
#include "../types.h"

#define DAYS_PER_MONTH 30
#define W_OK        0
#define W_NO_FUNDS  1

void wallet_start(u8 course);        /* コースを決めて最初の軍資金を入れる */
u8   wallet_buy_gems(u8 pack);       /* 石のパックを買う（W_OK / W_NO_FUNDS） */
u8   wallet_payday(void);            /* 月が変わっていれば給料を入れる。入れた月数 */
u8   wallet_infinite(void);
#endif
