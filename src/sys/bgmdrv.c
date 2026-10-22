/* SPDX-License-Identifier: MIT */
/* bgmdrv.c — BGM を VBLANK 割り込み（H.TIMI）で 1/60 秒ずつ進める（DECISIONS #71）。
 * 画面の描き直しが長くてもテンポが崩れない。曲データはページ BGMDATA にあり、割り込みの中で窓 A（8000h）に
 * 載せて読み、終わったら窓をメインの状態（g_bank_dataA）に戻す。
 * ★割り込みはカーネル（4000h〜）を呼ぶので、カートリッジが 4000h に居ないとき（ディスクの読み書きで
 *   ディスク ROM が出ている間）は呼んではいけない。H.TIMI からはまず RAM の小さな入口に飛び、
 *   g_isr_safe が 0 なら何もせず元のフックへつなぐ。ディスクの前後は bgm_hold / bgm_release で包む
 *   （鳴っている音を消して止め、終わったら同じところから続ける）。
 * ★テンポは割り込みの回数でなく時計で数える（DECISIONS #74）。turboR のシステムタイマー（I/O E6h/E7h,
 *   3.911µs ごとに 1 進む 16bit）で前の割り込みからの経過を測り、その分だけ 1/60 秒ずつ進める。
 *   処理が重くて割り込みを取りこぼしても、次の割り込みで追いつく（曲のテンポは処理の重さで変わらない）。 */
#include "bgmdrv.h"
#include "../ram.h"
#include "../data/rompages.h"

__sfr __at 0x7C opll_addr;
__sfr __at 0x7D opll_data;
__sfr __at 0xE6 sys_timer_lo;
__sfr __at 0xE7 sys_timer_hi;

#define TICK_UNITS 4262u        /* 1/60 秒 = 16,667µs ÷ 3.911µs */
#define MAX_CATCHUP 15          /* 1 回の割り込みで追いつく上限（1/4 秒ぶん） */

/* システムタイマーを読む（上位を読む間に下位が繰り上がったら読み直す） */
static u16 timer_now(void)
{
    u8 h, l;
    do {
        h = sys_timer_hi;
        l = sys_timer_lo;
    } while (h != sys_timer_hi);
    return (u16)((h << 8) | l);
}

/* いまから数え直す（曲の始め・ディスクの後。止まっていた時間は追いかけない） */
static void clock_reset(void)
{
    g_bgm_clock = timer_now();
    g_bgm_acc = 0;
}

#define REG_DATAA (*(volatile u8 *)0x7000)
#define PAGE_BASE ((const u8 *)0x8010)       /* ページの中身は窓の先頭 + 10h から */
#define H_TIMI    ((volatile u8 *)0xFD9F)

/* オクターブ = OPLL のブロックとしたときの 12 音の F ナンバー（A4 = 440Hz） */
static const u16 fnum[12] = { 172, 183, 194, 205, 217, 230, 244, 258, 274, 290, 307, 326 };

/* ★OPLL はアドレスの後に約 3.4µs、データの後に約 23.5µs 待たせる。R800 は速いので回して待つ */
void opll_w(u8 reg, u8 val)
{
    volatile u8 i;
    opll_addr = reg;
    for (i = 0; i < 2; i++) { }
    opll_data = val;
    for (i = 0; i < 12; i++) { }
}

static void key_off(u8 ch)
{
    opll_w((u8)(0x20 + ch), (u8)(g_bgm_ch[ch].hi & 0x0F));
    g_bgm_ch[ch].on = 0;
}

static void note_on(u8 ch, u8 note)
{
    BgmCh *c = &g_bgm_ch[ch];
    u8 block = (u8)(note / 12);
    u16 f = fnum[note % 12];
    if (block > 7) block = 7;
    key_off(ch);
    c->lo = (u8)f;
    c->hi = (u8)((block << 1) | (f >> 8));
    opll_w((u8)(0x10 + ch), c->lo);
    opll_w((u8)(0x20 + ch), (u8)(0x10 | c->hi));
    c->on = 1;
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
        if (b == 0x81) { c->inst = (u8)((s[c->pos++] << 4) | (c->inst & 0x0F)); opll_w((u8)(0x30 + ch), c->inst); continue; }
        if (b == 0x82) { c->inst = (u8)((c->inst & 0xF0) | (s[c->pos++] & 0x0F)); opll_w((u8)(0x30 + ch), c->inst); continue; }
        if (b == 0x80) { key_off(ch); c->wait = s[c->pos++]; return; }
        note_on(ch, b);
        c->wait = s[c->pos++];
        return;
    }
}

static void tick(const u8 *s)
{
    u8 ch;
    g_bgm_ticks++;
    for (ch = 0; ch < g_bgm_nch; ch++) {
        BgmCh *c = &g_bgm_ch[ch];
        if (c->wait == 0) continue;              /* 止まったチャンネル */
        if (--c->wait == 0) step(ch, s);
    }
}

/* VBLANK 割り込みから呼ばれる。前の呼び出しからの実時間ぶん進める */
void bgm_isr(void)
{
    u16 now;
    u8 n = 0;
    if (!g_bgm_song) return;
    now = timer_now();
    if (now == g_bgm_clock) {                    /* タイマーが動かない機械: 1 回 = 1/60 秒とみなす */
        g_bgm_acc = TICK_UNITS;
    } else {
        g_bgm_acc += (u16)(now - g_bgm_clock);
        g_bgm_clock = now;
    }
    REG_DATAA = PAGE_BGMDATA;
    while (g_bgm_acc >= TICK_UNITS && n < MAX_CATCHUP) {
        g_bgm_acc -= TICK_UNITS;
        tick(PAGE_BASE + g_bgm_base);
        n++;
    }
    if (n == MAX_CATCHUP) g_bgm_acc = 0;         /* 長く止まっていたら追いかけきらない */
    REG_DATAA = g_bank_dataA;                    /* メインが載せていたページへ戻す */
}

void bgm_clock_reset(void) { clock_reset(); }

void bgm_silence(void)
{
    u8 ch;
    for (ch = 0; ch < 9; ch++) opll_w((u8)(0x20 + ch), 0);
    for (ch = 0; ch < 3; ch++) g_bgm_ch[ch].on = 0;
}

/* H.TIMI に RAM の入口をつなぐ。入口: push af / g_isr_safe が 0 なら飛ばす / call bgm_isr / pop af / 元のフック */
void bgm_install(void)
{
    u8 *p = g_timi_stub;
    u16 safe = (u16)&g_isr_safe, isr = (u16)bgm_isr, stub = (u16)g_timi_stub;
    u8 i;
    __asm__("di");
    *p++ = 0xF5;                                  /* push af */
    *p++ = 0x3A; *p++ = (u8)safe; *p++ = (u8)(safe >> 8);   /* ld a,(g_isr_safe) */
    *p++ = 0xB7;                                  /* or a */
    *p++ = 0x28; *p++ = 3;                        /* jr z,+3 */
    *p++ = 0xCD; *p++ = (u8)isr; *p++ = (u8)(isr >> 8);     /* call bgm_isr */
    *p++ = 0xF1;                                  /* pop af */
    for (i = 0; i < 5; i++) *p++ = H_TIMI[i];     /* 元のフック（ディスクのモーター停止などが使う） */
    *p = 0xC9;                                    /* ret */
    H_TIMI[0] = 0xC3; H_TIMI[1] = (u8)stub; H_TIMI[2] = (u8)(stub >> 8);   /* jp 入口 */
    g_isr_safe = 1;
    __asm__("ei");
}

/* ディスクの読み書きの前: 割り込みを止め、鳴っている音を消す（どこまで進んだかは残す） */
void bgm_hold(void)
{
    u8 ch;
    g_isr_safe = 0;
    for (ch = 0; ch < g_bgm_nch; ch++) opll_w((u8)(0x20 + ch), (u8)(g_bgm_ch[ch].hi & 0x0F));
}

/* ディスクの読み書きの後: 鳴っていた音を同じ高さで鳴らし直し、続きから進める */
void bgm_release(void)
{
    u8 ch;
    for (ch = 0; ch < g_bgm_nch; ch++) {
        BgmCh *c = &g_bgm_ch[ch];
        if (!c->on) continue;
        opll_w((u8)(0x10 + ch), c->lo);
        opll_w((u8)(0x20 + ch), (u8)(0x10 | c->hi));
    }
    clock_reset();                               /* 止まっていた間は追いかけない */
    g_isr_safe = 1;
}
