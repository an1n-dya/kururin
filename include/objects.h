// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_OBJECTS_H
#define GUARD_OBJECTS_H

// src/objects.c (08015890-08016BF4): the world objects of a level (spinners, shooters, swingers,
// turrets). The functions other modules call.

#include "game.h"

void Object_SpringUp(struct AnimObject *task);
void Object_SpringDown(struct AnimObject *t);
void Object_SpringRight(struct AnimObject *task);
void Object_SpringLeft(struct AnimObject *task);
void Object_Roller(struct Task *task);
void Object_RollerCatcher(struct Task *task);
void Object_Piston(struct PistonObject *task);
void Object_Shooter(struct ShooterObject *task);

#endif // GUARD_OBJECTS_H
