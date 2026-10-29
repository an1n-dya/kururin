// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_SPRITE_H
#define GUARD_SPRITE_H

// src/sprite.c (08002438-08002848): the sprite lists (OAM buffer) and the affine parameter sets.
// The functions other modules call.

#include "game.h"

void Sprite_ClearLists(void);
void Sprite_BuildOam(void);
struct Sprite *Sprite_Alloc(u8 a);
void Sprite_InitRotations(void);
s32 Sprite_AllocAffine(void);
void Sprite_SetAffine(s32 n, s32 a, s32 b, s32 c, s32 d);
void Sprite_SetAffineRotation(s32 n, u8 dir);

#endif // GUARD_SPRITE_H
