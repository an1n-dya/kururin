// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_HUD_H
#define GUARD_HUD_H

// src/hud.c (08016BF4-08017270): the lives and status sprites and the times drawn during a level.
// The functions other modules call.

#include "game.h"

void Hud_Init(struct Helirin *h);
void Hud_Draw(struct Helirin *h);
void Hud_DrawTimes(void);
void Hud_DrawLinkProgress(void);

#endif // GUARD_HUD_H
