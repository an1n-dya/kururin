// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// A leaf with branches. The ROM returns with a bare bx lr; plain agbcc saves lr
// (push {lr} ... pop {r1}; bx r1) because its far-jump test is sticky (thumb.c,
// far_jump_used_p) unless -fprologue-bugfix is given. Also in src/game.c.

#include "gba/types.h"

s32 Util_StepTowards(s32 a, s32 b, u32 step)
{
    s32 diff = b - a;

    if (diff != 0)
    {
        if (step == 0)
            step = 1;
        if (diff > 0)
        {
            if (diff > step)
                return a + step;
        }
        else if (-diff > step)
        {
            return a - step;
        }
        return b;
    }
    return a;
}
