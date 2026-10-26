# KANJO RACER: 高速ループ

*for Collin*

An OutRun-style pseudo-3D arcade racer for the Game Boy Advance, set on a neon-lit
Osaka-style expressway loop. Drive a white late-80s hatchback, race three kanjo rivals,
dodge traffic, and beat the clock - every checkpoint adds time. Day turns to night to dawn
as the laps go by.

## Get the ROM (no installs needed)
1. Upload everything in this folder (including the hidden `.github` folder) to a new GitHub repository.
2. Open the **Actions** tab and wait for "Build GBA ROM" to turn green (a couple of minutes).
3. Download `kanjo_racer.gba` from **Releases** on the repo's main page.
4. Play it in an emulator (mGBA, Delta on iPhone, My Boy! on Android) or on a flash cart.

## Controls
| Button | Action |
|---|---|
| A / Up | Gas |
| B / Down | Brake |
| Left / Right | Steer |
| Start / A | Confirm in menus |

Menu: **RACE**, **GARAGE** (tune engine / grip / gears / weight, saved to the cartridge),
and **GAS: HOLD / NORMAL**. In HOLD mode (the default) the car keeps its speed when you let go
of the gas; NORMAL lets it coast down.

## How it works
- `source/game.c` - everything: track, road renderer, physics, traffic, AI rivals, HUD, menus, saving.
- `source/gfx_*.c`, `source/gfx.h` - generated art data (committed, so the cloud build needs no Python).
- `tools/make_assets.py` - draws all the art from code; run it to regenerate / restyle.
- `tools/host_test.c`, `tools/compose.py` - run the real game on a PC and save frames as images.
- `Makefile` - plain devkitARM build. `.github/workflows/build.yml` - the cloud build.
