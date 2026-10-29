// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_VS_H
#define GUARD_VS_H

// src/vs.c (0801016C-080131F8): the linked (VS) game: waiting for the consoles, its menus and
// screens. The functions other modules call.

#include "game.h"

s32 Vs_WaitForConsoles(u32 hi);
s32 Vs_CheckLevelLink(u32 hi);
s32 Vs_RunMenus(s32 a);
s32 Vs_RunResults(void);

#endif // GUARD_VS_H
