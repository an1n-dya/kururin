// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_LEVEL_H
#define GUARD_LEVEL_H

// src/level.c (08004C24-08005178): a new game, the level number, loading a level and starting its
// tasks. The functions other modules call.

#include "game.h"

void Level_SetFromIndex(s32 n);
s32 Level_Load(void);
void Level_StartTasks(void);

#endif // GUARD_LEVEL_H
