// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_UTIL_H
#define GUARD_UTIL_H

// src/util.c (080041F4-0800459C): the random byte table, screen positions, steps towards a value,
// link player bits, directions, animations. The functions other modules call.

#include "game.h"

void Random_SetIndices(s32 value);
u8 Random_Next(void);
void Random_Reset(void);
void Random_Shuffle(s32 seed);
s32 Util_GetScreenPos(s16 x, s16 y, s16 *screen, u8 margin);
s32 Util_IsOnScreen(s16 x, s16 y, u8 margin);
s32 Util_StepTowards(s32 a, s32 b, u32 step);
s8 Util_StepTowardsS8(s8 a, s8 b, u8 step);
u32 Link_FindCommand(u32 mask, u32 hi);
u32 Link_FindCommandExact(u32 mask, u32 hi, u32 lo);
s32 Util_GetDirection(s32 dx, s32 dy);
s32 Anim_Start(struct Anim *anim, const u16 *frames);
s32 Anim_Step(struct Anim *anim);
s32 Anim_StepLong(struct LongAnim *anim);

#endif // GUARD_UTIL_H
