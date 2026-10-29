// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_MAKEUP_H
#define GUARD_MAKEUP_H

// src/makeup.c (080131F8-08013D0C): lives by game mode and the Make Up screen. The functions other
// modules call.

#include "game.h"

void Lives_Reset(void);
void Lives_Init(struct Helirin *h);
void MakeUp_StartWornItems(void);
void MakeUp_Run(void);

#endif // GUARD_MAKEUP_H
