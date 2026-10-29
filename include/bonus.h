// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_BONUS_H
#define GUARD_BONUS_H

// src/bonus.c (08018330-08018D34): the bonus items of a level and their tasks. The functions other
// modules call.

#include "game.h"

void Bonus_UpdateColor(struct BonusTask *task);
void Bonus_UpdateType(struct BonusTask *task);
void Bonus_UpdateItem(struct ItemBonus *task);
void Bonus_UpdateWornItem(struct Task *task);
void Bonus_UpdateMakeUpIcon(struct Task *task);

#endif // GUARD_BONUS_H
