/* psg.h — PSG の最小ドライバ（効果音 1 音）。BGM は Phase1 以降 */
#ifndef PSG_H
#define PSG_H
#include "../types.h"
void psg_init(void);
void psg_beep(u16 tone, u8 frames);
void psg_tick(void);              /* 1 フレーム 1 回 */
u8   psg_read_joy(u8 port);
#endif
