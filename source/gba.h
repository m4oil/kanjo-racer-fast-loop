// Minimal GBA hardware definitions (no external libraries needed).
// Compiling with -DHOST_TEST swaps the hardware for plain RAM so the game
// can be built and checked on a normal computer.
#ifndef GBA_H
#define GBA_H

#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#ifdef HOST_TEST
extern u8 host_vram[0x18000];
extern u16 host_pal[512];
extern u16 host_oam[512];
extern u16 host_regs[0x400];
extern u8 host_sram[0x8000];
#define VRAM_BASE ((volatile u8 *)host_vram)
#define PAL_BG ((volatile u16 *)host_pal)
#define PAL_OBJ ((volatile u16 *)(host_pal + 256))
#define OAM_RAM ((volatile u16 *)host_oam)
#define SRAM_RAM ((volatile u8 *)host_sram)
#define REG16(off) (*(volatile u16 *)&host_regs[(off) >> 1])
#define REG32(off) (*(volatile u32 *)&host_regs[(off) >> 1])
#define IWRAM_CODE
#else
#define VRAM_BASE ((volatile u8 *)0x06000000)
#define PAL_BG ((volatile u16 *)0x05000000)
#define PAL_OBJ ((volatile u16 *)0x05000200)
#define OAM_RAM ((volatile u16 *)0x07000000)
#define SRAM_RAM ((volatile u8 *)0x0E000000)
#define REG16(off) (*(volatile u16 *)(0x04000000 + (off)))
#define REG32(off) (*(volatile u32 *)(0x04000000 + (off)))
#define IWRAM_CODE __attribute__((section(".iwram"), target("arm"), long_call))
#endif

#define REG_DISPCNT REG16(0x000)
#define REG_VCOUNT REG16(0x006)
#define REG_KEYINPUT REG16(0x130)

#define KEY_A 0x0001
#define KEY_B 0x0002
#define KEY_SELECT 0x0004
#define KEY_START 0x0008
#define KEY_RIGHT 0x0010
#define KEY_LEFT 0x0020
#define KEY_UP 0x0040
#define KEY_DOWN 0x0080
#define KEY_R 0x0100
#define KEY_L 0x0200

// Mode 4 bitmap + BG2 + sprites (1D tile mapping)
#define DCNT_BASE (4 | (1 << 10) | (1 << 12) | (1 << 6))
#define PAGE1_OFFSET 0xA000
#define OBJ_VRAM_OFFSET 0x14000   /* sprite tiles live here in bitmap modes */
#define OBJ_TILE_BASE 512         /* tile numbers start at 512 in bitmap modes */

#endif
