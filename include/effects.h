// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_EFFECTS_H
#define GUARD_EFFECTS_H

// src/effects.c (08017270-080181FC): effect tasks: window and fade effects, sparks, palette
// cycles, banners. The functions other modules call.

#include "game.h"

void Effect_OpenWindow(struct Task *task);
void Effect_BrightnessUp(struct Task *task);
void Effect_BrightnessDown(struct Task *task);
void Effect_RotateBg2(struct Task *task);
void Effect_UpdateCrash(struct Task *task);
void Effect_UpdateHitSpark(struct Task *task);
void Effect_UpdateZoneHitRipple(struct Task *task);
void Effect_UpdateSpringSpark(struct Task *task);
void Effect_UpdateTimePenalty(struct Task *task);
void Effect_Firework(struct Task *task);
void Effect_UpdateGoal(struct Task *task);
void Effect_CyclePalette(struct Task *task);
void Effect_AnimateCopies(struct Task *task);
void Effect_UpdateLinkResultBanner(struct Task *task);
void Effect_AnimateBlockCopies(struct Task *task);
void Effect_UpdateGoalBanner(struct Task *task);
void Effect_UpdateLinkEndBanner(struct Task *task);
void Effect_UpdateTimeUpBanner(struct Task *task);
void Effect_DrawDemoLabel(struct Task *task);
void Effect_UpdateLinkCountdown(struct Task *task);

#endif // GUARD_EFFECTS_H
