// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_GBA_TYPES_H
#define GUARD_GBA_TYPES_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned long long u64;

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile u64 vu64;

typedef u8 bool8;
typedef u16 bool16;
typedef u32 bool32;

#define TRUE 1
#define FALSE 0
#define NULL ((void *)0)

// SIOCNT in multi-player mode, as the Nintendo SDK's SioMultiCnt (AgbTypes.h; pret's
// gba/types.h): read as a whole 4-byte struct, ID is bits 4-5 (the original's 32-bit load
// with lsl #26, lsr #30 in ModeMenu_Run, Vs_WaitForConsoles, Vs_CheckLevelLink).
struct SioMultiCnt
{
    u16 baudRate:2;   // baud rate
    u16 si:1;         // SI terminal
    u16 sd:1;         // SD terminal
    u16 id:2;         // ID (player number)
    u16 error:1;      // error flag
    u16 enable:1;     // SIO enable
    u16 unused_11_8:4;
    u16 mode:2;       // communication mode (10: multi-player)
    u16 intrEnable:1; // IRQ enable
    u16 unused_15:1;
    u16 data;         // SIOMLT_SEND
};

#endif // GUARD_GBA_TYPES_H
