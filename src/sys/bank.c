/* SPDX-License-Identifier: MIT */
/* bank.c — ASCII8 のバンクレジスタ 0x6800/0x7000/0x7800 を書く。
 * 窓 0x4000 (0x6000 レジスタ) はカーネルが乗っているので決して触らない。 */
#include "bank.h"
#include "../ram.h"

#define REG_CODE  (*(volatile u8 *)0x6800)
#define REG_DATAA (*(volatile u8 *)0x7000)
#define REG_DATAB (*(volatile u8 *)0x7800)

void bank_code(u8 n)  { g_bank_code = n;  REG_CODE = n; }
void bank_dataA(u8 n) { g_bank_dataA = n; REG_DATAA = n; }
void bank_dataB(u8 n) { g_bank_dataB = n; REG_DATAB = n; }

/* ★窓 6000h も切り替えるので、カーネル上でだけ実行される（呼び出し元のバンクへは戻してから帰る） */
u8 bank_probe(u8 window, u8 page)
{
    static const u16 base[3] = { 0x6000, 0x8000, 0xA000 };
    const volatile u8 *w = (const volatile u8 *)base[window];
    u8 old = window == 0 ? g_bank_code : window == 1 ? g_bank_dataA : g_bank_dataB;
    u8 ok;
    if (window == 0) REG_CODE = page; else if (window == 1) REG_DATAA = page; else REG_DATAB = page;
    ok = (u8)(w[0] == page && w[1] == 'T' && w[2] == 'G' && w[3] == 'P');
    if (window == 0) REG_CODE = old; else if (window == 1) REG_DATAA = old; else REG_DATAB = old;
    return ok;
}
