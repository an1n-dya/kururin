// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_MODE_MENU_H
#define GUARD_MODE_MENU_H

// src/mode_menu.c (0800C75C-0800E560): the mode menu (and the MultiBoot send), then the Adventure
// map screen it leads to, its level menu and map tasks. The functions other modules call.

#include "game.h"

s32 ModeMenu_Run(s32 a);
u32 MapScreen_GetLevelDataSize(void);
void MapScreen_ShowMap(s32 n);
s32 MapScreen_Run(s32 a);
s32 MapScreen_LevelMenuItem(struct Menu *p, s32 item);
void MapScreen_UpdatePath(struct Task *task);
void MapScreen_UpdateCursor(struct Task *task);
void MapScreen_UpdateWorldIcon(struct Task *task);

#endif // GUARD_MODE_MENU_H
