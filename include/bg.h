// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_BG_H
#define GUARD_BG_H

// src/bg.c (08000AEC-08002438): video modes, background maps and layers, the window effect, screen
// backgrounds. The functions other modules call.

#include "game.h"

void Bg_SetVideoMode(s32 a);
void Bg_InitMap(struct Map *map, struct BgLayer *layer, const u16 *tiles, u16 width, u16 height);
void Bg_ScrollMap(struct Map *map, s32 x, s32 y);
void Bg_ScrollMapClamped(struct Map *map, s32 x, s32 y);
void Bg_DrawLevelMinimap(const u8 *map);
void Bg_StartWindowCircle(s32 a, s32 x, s32 y);
void Bg_DrawWindowCircle(s32 a, s32 x, s32 y);
void Bg_LoadScreen(s32 a);
void Bg_LoadMap8bpp(s32 n, const u8 *src, const u8 *gfx, const u16 *list, u32 width, s32 height);
void Bg_LoadMap(s32 n, const u8 *src, const u8 *gfx, const u16 *list, u32 width, s32 height);

#endif // GUARD_BG_H
