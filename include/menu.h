// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_MENU_H
#define GUARD_MENU_H

// src/menu.c (0800459C-08004C24): the menu driver and its key handlers for items and values. The
// functions other modules call.

#include "game.h"

s32 Menu_RunWrap(struct Menu *p, u32 keys);
s32 Menu_Run(struct Menu *p, u32 keys);
void Menu_Draw(struct Menu *p);
s32 Menu_MoveLeftRightWrap(struct Menu *p, u32 keys);
s32 Menu_StepValueWrap(u16 *value, s32 max, u32 keys);
s32 Menu_StepValueUpDownWrap(u16 *value, s32 max, u32 keys);
s32 Menu_StepValue(u16 *value, s32 max, u32 keys);
s32 Menu_StepValueReversed(u16 *value, s32 max, u32 keys);
s32 Menu_StepValueLRWrap(u16 *value, s32 max, u32 keys);
s32 Menu_StepValueBy10(u16 *value, s32 max, u32 keys);

#endif // GUARD_MENU_H
