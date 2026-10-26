// Hardware entry point (the host test harness has its own main).
#include "game.h"
#include "gba.h"

#ifndef HOST_TEST
int main(void)
{
    u16 last, now;
    int steps = 1, i;
    game_init();
    REG16(0x10A) = 0x0083;          // timer 2: free running, 1024 prescale (~16384 Hz)
    last = REG16(0x108);
    for (;;)
    {
        u16 keys = (u16)((~REG_KEYINPUT) & 0x3FF);
        for (i = 0; i < steps; i++) game_step(keys);
        game_draw();
        while (REG_VCOUNT < 160) { }    // wait for vblank
        game_present();
        now = REG16(0x108);
        steps = (int)((u16)(now - last) + 137) / 274;   // keep game speed right even if a frame is slow
        last = now;
        if (steps < 1) steps = 1;
        if (steps > 3) steps = 3;
    }
    return 0;
}
#endif
