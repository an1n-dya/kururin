// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_LINK_H
#define GUARD_LINK_H

// src/link.c (08003C18-08004030): the link cable: multi-player setup and the per-frame exchange.
// The functions other modules call.

#include "game.h"

void Link_Init(s32 a);
u32 Link_Exchange(u16 *send, struct LinkData *recv, s32 a);

#endif // GUARD_LINK_H
