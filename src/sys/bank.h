/* bank.h — ASCII8 バンク切替。現在値は RAM に鏡写しする（仕様 §2.3） */
#ifndef BANK_H
#define BANK_H
#include "../types.h"
void bank_code(u8 n);
void bank_dataA(u8 n);
void bank_dataB(u8 n);
#endif
