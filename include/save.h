// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_SAVE_H
#define GUARD_SAVE_H

// src/save.c (08005178-080057B8): the save data: boot load, save, file and record reset. The
// functions other modules call.

#include "game.h"

void Save_LoadAtBoot(void);
s32 Save_Write(void);
void Save_ResetFile(s32 n);

#endif // GUARD_SAVE_H
