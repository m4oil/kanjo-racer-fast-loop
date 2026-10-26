// Host-side harness: runs the real game code on a PC and dumps frames for inspection.
// Build:  gcc -DHOST_TEST -O1 -g -Wall -fsanitize=address,undefined -Isource source/game.c source/gfx_*.c tools/host_test.c -o /tmp/host_test
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/game.h"
#include "../source/gba.h"

u8 host_vram[0x18000];
u16 host_pal[512];
u16 host_oam[512];
u16 host_regs[0x400];
u8 host_sram[0x8000];

static void dump(const char *name)
{
    FILE *f = fopen(name, "wb");
    int page = (host_regs[0] >> 4) & 1;
    fwrite(host_vram + (page ? 0xA000 : 0), 1, 38400, f);
    fwrite(host_pal, 2, 512, f);
    fwrite(host_oam, 2, 512, f);
    fwrite(host_vram + 0x14000, 1, 16384, f);
    fclose(f);
}

int game_dbg(int what);
extern int dbg_notraffic;
int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 600;
    int every = argc > 2 ? atoi(argv[2]) : 100;
    const char *prefix = argc > 3 ? argv[3] : "/tmp/frame";
    int f;
    memset(host_sram, 0, sizeof host_sram);
    dbg_notraffic = getenv("NOTRAFFIC") != NULL;
    game_init();
    for (f = 0; f < frames; f++)
    {
        unsigned keys = 0;
        const char *mode = argc > 4 ? argv[4] : "race";
        if (!strcmp(mode, "hub")) { if (f == 5) keys = KEY_START; }
        else if (!strcmp(mode, "garage")) {
            if (f == 5) keys = KEY_START; else if (f == 12) keys = KEY_DOWN; else if (f == 16) keys = KEY_A;
            else if (f == 22 || f == 26 || f == 30) keys = KEY_RIGHT; else if (f == 36) keys = KEY_DOWN; else if (f == 40) keys = KEY_RIGHT;
        } else if (!strcmp(mode, "idle")) {
            if (f == 5) keys = KEY_START; else if (f == 12) keys = KEY_A; else if (f > 3300 && f % 40 == 0) keys = KEY_A;
        } else if (!strcmp(mode, "bot")) {
            // autopilot: start the race, hold gas, steer back to the middle, ease off in tight curves
            if (f == 5) keys = KEY_START; else if (f == 12) keys = KEY_A;
            else if (game_dbg(5) == 3) {
                int px = game_dbg(0), spd = game_dbg(1) >> 6, cv = game_dbg(3);
                keys = KEY_A;
                if (px > 9000) keys |= KEY_LEFT; else if (px < -9000) keys |= KEY_RIGHT;
                if (abs(cv) >= 56 && spd > 120) keys = (keys & ~KEY_A) | KEY_B;
                else if (abs(cv) >= 40 && spd > 150) keys &= ~KEY_A;
                if (game_dbg(4) == 2 && f % 60 == 0) keys |= KEY_A;
            }
        } else {
            if (f == 5) keys = KEY_START; else if (f == 12) keys = KEY_A;
            else if (f > 300) keys = KEY_A | ((f / 90) % 2 ? KEY_LEFT : KEY_RIGHT) * ((f % 90) < 18);
        }
        if (argc > 5 && f % 300 == 0) printf("f=%5d dist=%7d spd=%3d kmh px=%6d curve=%3d cp=%d time=%2d phase=%d segs=%d\n", f, game_dbg(2), game_dbg(1) >> 6, game_dbg(0), game_dbg(3), game_dbg(9), game_dbg(6) / 60, game_dbg(4), game_dbg(8));
        game_step((u16)keys);
        game_draw();
        game_present();
        if (f % every == 0 || f == frames - 1)
        {
            char name[256];
            snprintf(name, sizeof name, "%s_%05d.bin", prefix, f);
            dump(name);
        }
    }
    printf("ok, ran %d frames; dist=%d cp=%d phase=%d time=%d score=%d rank=%d\n", frames, game_dbg(2), game_dbg(9), game_dbg(4), game_dbg(6) / 60, game_dbg(7), game_dbg(10));
    return 0;
}
