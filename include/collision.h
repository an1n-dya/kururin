// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_COLLISION_H
#define GUARD_COLLISION_H

// src/collision.c (08013D0C-080142DC): map collision and the Helirin's points against shapes. The
// functions other modules call.

#include "game.h"

u32 Collision_TestWallPixel(struct Map *map, u16 x, u16 y);
u32 Collision_GetTilePixel(struct Map *map, u16 x, u16 y);
void Collision_UpdatePoints(struct Helirin *h);
u32 Collision_TestShapes(struct Helirin *h, s32 x, s32 y, const struct CollisionShape *shape);
s32 Collision_FindPointInShapes(struct Helirin *h, s32 x, s32 y, const struct CollisionShape *shape);

#endif // GUARD_COLLISION_H
