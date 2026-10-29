// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_HELIRIN_H
#define GUARD_HELIRIN_H

// src/helirin.c (080142DC-08015890): the Helirin: setup, its frame (physics), collision response,
// drawing. The functions other modules call.

#include "game.h"

void Helirin_SetColor(struct Helirin *h, s32 n);
void Helirin_SetSize(struct Helirin *h);
void Helirin_Init(struct Helirin *h, s32 a);
void Helirin_Update(struct Helirin *h);
void Helirin_UpdateCollision(struct Helirin *h);
void Helirin_Draw(struct Helirin *h);
void Helirin_Bump(struct Helirin *h, u8 dir, u16 speed);
void Helirin_UpdatePreview(struct Helirin *h);

#endif // GUARD_HELIRIN_H
