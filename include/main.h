// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_MAIN_H
#define GUARD_MAIN_H

#include "gba/types.h"

// Main-loop state at 0x03000DC0 (see AgbMain, Main_EndFrame).
struct Main {
    /*0x00*/ u32 frameCount;          // KururinTAS memory watch: "Global Frame Counter"
    /*0x04*/ void (*frameCallback)(void); // called once per frame after the VBlank wait, if set
    /*0x08*/ u16 intrFlags;           // bit 0 is set by the VBlank interrupt and polled by the wait
    /*0x0A*/ u8 state;                // index into AgbMain's state-function table (gMainStates)
    /*0x0B*/ u8 subState;             // the sub-state the main states run (Play_MainState, the ending)
    /*0x0C*/ u8 vblankCount;          // counted up by Main_VBlankIntr, cleared by Main_EndFrame
    /*0x0D*/ u8 unkD[3];
    /*0x10*/ u8 *ewramCursor;         // Level_Load: the next free byte of the EWRAM buffer
                                      // (from 0x02000000) it decompresses level data into
    /*0x14*/ u8 heap[0x18];           // a struct Heap the screens allocate from (Heap_Init, Heap_Alloc)
    // Written once per frame by Main_ReadKeys from KEYINPUT (0x04000130, inverted, 10 keys):
    /*0x2C*/ u16 heldKeys;    // keys down this frame
    /*0x2E*/ u16 newKeys;     // keys down this frame and not the last
    /*0x30*/ u16 prevKeys;    // heldKeys of the last frame
    /*0x32*/ u16 repeatKeys;  // newKeys, plus heldKeys every 4 frames after 20 frames held
    /*0x34*/ u16 repeatTimer; // the countdown for repeatKeys
};

// KEYINPUT bits (GBA hardware)
#define A_BUTTON 0x0001
#define B_BUTTON 0x0002
#define SELECT_BUTTON 0x0004
#define START_BUTTON 0x0008

extern struct Main gMain;

// src/main.c (080002D8-08000AEC): the entry, the frame loop, interrupts, keys, the frame
// callbacks, the transfer queue and the heap. The functions other modules call.
void Main_EndFrame(void);
void Main_ClearKeys(void);
void Main_InstallLinkFrameCallback(void);
void Main_InstallFrameCallback(void);
s32 Main_QueueCopy(const void *src, void *dst, u16 size);
s32 Main_QueueFill(const void *src, void *dst, u16 size);
s32 Main_QueueBlockCopy(const void *src, void *dst, u8 blockTiles, u8 blocks);
s32 Heap_Init(void *heap, void *mem, u32 size);
s32 Heap_Free(void *heap, void *p);
void *Heap_Alloc(void *heap, u32 size);

#endif // GUARD_MAIN_H
