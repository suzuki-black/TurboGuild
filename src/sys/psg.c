/* SPDX-License-Identifier: MIT */
/* psg.c — チャンネル A で短い効果音を鳴らす。割り込みから呼ばず、メインループで tick する */
#include "psg.h"
#include "../ram.h"

__sfr __at 0xA0 psg_addr;
__sfr __at 0xA1 psg_wdata;
__sfr __at 0xA2 psg_rdata;

static void psg_w(u8 reg, u8 val)
{
    __critical {
        psg_addr = reg;
        psg_wdata = val;
    }
}

void psg_init(void)
{
    psg_w(7, 0xBE);   /* A のトーンのみ有効。bit7=1(R#15 出力), bit6=0(R#14 入力) */
    psg_w(8, 0);
}

void psg_beep(u16 tone, u8 frames)
{
    psg_w(0, (u8)tone);
    psg_w(1, (u8)(tone >> 8));
    psg_w(8, 12);
    g_beep_frames = frames;
}

void psg_tick(void)
{
    if (g_beep_frames && --g_beep_frames == 0) {
        psg_w(8, 0);
    }
}

/* R#15 bit6 でポート選択し R#14 を読む。bit4,5 = トリガ A/B */
u8 psg_read_joy(u8 port)
{
    u8 v;
    __critical {
        psg_addr = 15;
        v = psg_rdata;
        psg_wdata = port ? (u8)(v | 0x40) : (u8)(v & 0xBF);
        psg_addr = 14;
        v = psg_rdata;
    }
    return v;
}
