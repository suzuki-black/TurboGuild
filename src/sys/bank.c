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
