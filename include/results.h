// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_RESULTS_H
#define GUARD_RESULTS_H

// src/results.c (0800AA0C-0800B8A0): the result screen, rank-ups, the camera origin, a level's
// records. The functions other modules call.

#include "game.h"

void Results_Run(void);
void Results_CheckRankUp(void);
void Camera_Update(void);

#endif // GUARD_RESULTS_H
