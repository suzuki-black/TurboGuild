/* SPDX-License-Identifier: MIT */
/* pev.c — 自分の最近の出来事（pev.h） */
#include "pev.h"
#include "save.h"
#include "board.h"

void pev_push(u8 kind, u16 param)
{
    u16 slot = (u16)(g_save.g_time / BOARD_SLOT_SECS);
    u8 j;
    if (g_save.pev[0].kind == kind && g_save.pev[0].slot == slot) return;   /* 同じ区切りに同じ出来事は 1 つ */
    for (j = PEV_MAX - 1; j; j--) g_save.pev[j] = g_save.pev[j - 1];
    g_save.pev[0].slot = slot;
    g_save.pev[0].kind = kind;
    g_save.pev[0].param = param;
}
