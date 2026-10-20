/* SPDX-License-Identifier: MIT */
/* bgm.c — FM 音源（MSX-MUSIC = YM2413/OPLL, I/O 7Ch/7Dh）の BGM ドライバ。3 チャンネル。
 * 曲は tools/mml.py が data/bgm/ の .mml から作る（bgm_data.h）。メインループが毎フレーム bgm_tick を呼ぶ。
 * 長い描画やディスクの読み書きの間は止まる（割り込みでは鳴らさない: バンクの窓を割り込みで触らないため）。 */
#include "../far.h"
#include "../ram.h"
#define BGM_DATA_HERE
#include "../data/bgm_data.h"

__sfr __at 0x7C opll_addr;
__sfr __at 0x7D opll_data;

#define CHANNELS 3

/* オクターブ = OPLL のブロックとしたときの 12 音の F ナンバー（A4 = 440Hz） */
static const u16 fnum[12] = { 172, 183, 194, 205, 217, 230, 244, 258, 274, 290, 307, 326 };

/* ★OPLL はアドレスの後に約 3.4µs、データの後に約 23.5µs 待たせる。R800 は速いので回して待つ */
static void opll(u8 reg, u8 val)
{
    volatile u8 i;
    opll_addr = reg;
    for (i = 0; i < 2; i++) { }
    opll_data = val;
    for (i = 0; i < 12; i++) { }
}

static void key_off(u8 ch)
{
    opll((u8)(0x20 + ch), (u8)(g_bgm_ch[ch].hi & 0x0F));
}

static void note_on(u8 ch, u8 note)
{
    u8 block = (u8)(note / 12);
    u16 f = fnum[note % 12];
    if (block > 7) block = 7;
    key_off(ch);
    g_bgm_ch[ch].hi = (u8)((block << 1) | (f >> 8));
    opll((u8)(0x10 + ch), (u8)f);
    opll((u8)(0x20 + ch), (u8)(0x10 | g_bgm_ch[ch].hi));
}

static void silence(void)
{
    u8 ch;
    for (ch = 0; ch < 9; ch++) opll((u8)(0x20 + ch), 0);
}

FAR(void, bgm_stop, (void))
{
    silence();
    g_bgm_song = 0;
}

FAR(void, bgm_play, (u8 song))
{
    const u8 *s;
    u8 ch;
    if (song == g_bgm_song) return;              /* 同じ曲なら続けて流す */
    silence();
    opll(0x0E, 0);                               /* リズム音源は使わない */
    g_bgm_song = song;
    if (!song || song > BGM_COUNT) return;
    s = songs[song - 1];
    g_bgm_nch = s[0] > CHANNELS ? CHANNELS : s[0];
    for (ch = 0; ch < g_bgm_nch; ch++) {
        g_bgm_ch[ch].pos = (u16)(s[1 + ch * 2] | (s[2 + ch * 2] << 8));
        g_bgm_ch[ch].loop = 0;
        g_bgm_ch[ch].wait = 1;
        g_bgm_ch[ch].inst = 0x30;
        g_bgm_ch[ch].hi = 0;
        opll((u8)(0x30 + ch), 0x30);
    }
}

/* 次の音か休みまで読む。曲の終わりはループ位置へ（無ければそのチャンネルは止まる） */
static void step(u8 ch, const u8 *s)
{
    BgmCh *c = &g_bgm_ch[ch];
    u8 guard;
    for (guard = 0; guard < 32; guard++) {
        u8 b = s[c->pos++];
        if (b == 0xFE) { c->loop = c->pos; continue; }
        if (b == 0xFF) {
            if (!c->loop) { c->wait = 0; c->pos--; key_off(ch); return; }
            c->pos = c->loop;
            continue;
        }
        if (b == 0x81) { c->inst = (u8)((s[c->pos++] << 4) | (c->inst & 0x0F)); opll((u8)(0x30 + ch), c->inst); continue; }
        if (b == 0x82) { c->inst = (u8)((c->inst & 0xF0) | (s[c->pos++] & 0x0F)); opll((u8)(0x30 + ch), c->inst); continue; }
        if (b == 0x80) { key_off(ch); c->wait = s[c->pos++]; return; }
        note_on(ch, b);
        c->wait = s[c->pos++];
        return;
    }
}

FAR(void, bgm_tick, (void))
{
    const u8 *s;
    u8 ch;
    if (!g_bgm_song) return;
    s = songs[g_bgm_song - 1];
    for (ch = 0; ch < g_bgm_nch; ch++) {
        BgmCh *c = &g_bgm_ch[ch];
        if (c->wait == 0) continue;              /* 止まったチャンネル */
        if (--c->wait == 0) step(ch, s);
    }
}
