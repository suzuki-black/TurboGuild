/* SPDX-License-Identifier: MIT */
/* bgm.c — FM 音源（MSX-MUSIC = YM2413/OPLL, I/O 7Ch/7Dh）の BGM の開始と停止。3 チャンネル。
 * 曲は tools/mml.py が data/bgm/ の .mml から作る（ページ BGMDATA）。進めるのは割り込み（sys/bgmdrv.c）。
 * 割り込みと同じ状態を書き換えるので、ここでは割り込みを止めてから触る。 */
#include "../far.h"
#include "../ram.h"
#include "../data/bgm_data.h"
#include "../data/rompages.h"
#include "../sys/bank.h"
#include "../sys/bgmdrv.h"

#define CHANNELS  3
#define PAGE_BASE ((const u8 *)0x8010)

FAR(void, bgm_stop, (void))
{
    __asm__("di");
    bgm_silence();
    g_bgm_song = 0;
    __asm__("ei");
}

FAR(void, bgm_play, (u8 song))
{
    const u8 *s;
    u8 ch;
    if (song == g_bgm_song) return;              /* 同じ曲なら続けて流す */
    __asm__("di");
    bgm_silence();
    opll_w(0x0E, 0);                             /* リズム音源は使わない */
    g_bgm_song = 0;
    if (song && song <= BGM_COUNT) {
        bank_dataA(PAGE_BGMDATA);
        g_bgm_base = (u16)(PAGE_BASE[1 + (song - 1) * 2] | (PAGE_BASE[2 + (song - 1) * 2] << 8));
        s = PAGE_BASE + g_bgm_base;
        g_bgm_nch = s[0] > CHANNELS ? CHANNELS : s[0];
        for (ch = 0; ch < g_bgm_nch; ch++) {
            BgmCh *c = &g_bgm_ch[ch];
            c->pos = (u16)(s[1 + ch * 2] | (s[2 + ch * 2] << 8));
            c->loop = 0;
            c->wait = 1;
            c->inst = 0x30;
            c->hi = 0;
            c->on = 0;
            opll_w((u8)(0x30 + ch), 0x30);
        }
        g_bgm_song = song;
    }
    __asm__("ei");
}
