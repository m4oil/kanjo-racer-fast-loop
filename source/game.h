#ifndef GAME_H
#define GAME_H
#include "gba.h"

void game_init(void);
void game_step(u16 keys);   // one 60 Hz logic tick (keys: 1 = held)
void game_draw(void);       // render into the hidden page + sprite list
void game_present(void);    // call right after vblank starts: palette, page flip, OAM copy
int game_frames_wanted(void);

#endif
