// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// util: the random byte table, screen positions, steps towards a value, link player bits,
// directions, animations.
// 080041F4-0800459C, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "util.h"

extern u8 gRandomTable[];
extern u8 gRandomIndex;
extern u8 gRandomIndex2;
extern const u8 gRandomTableInit[];
extern const s8 gDirectionTable[64][64];
s32 Anim_StartLong(struct LongAnim *anim, const struct LongAnimFrame *frames);

/**
 * Stores the same byte to gRandomIndex and gRandomIndex2.
 */
void Random_SetIndices(s32 value)
{
    gRandomIndex2 = gRandomIndex = value;
}

/**
 * Returns the next entry of the byte table gRandomTable (shuffled by Random_Shuffle): the
 * one at gRandomIndex, which advances by one (wrapping at 256).
 */
u8 Random_Next(void)
{
    return gRandomTable[gRandomIndex++];
}

/**
 * Returns entry *index* of the byte array gRandomTable.
 */
u8 Random_Get(s32 index)
{
    return gRandomTable[(u8)index];
}

/**
 * Returns the next entry of the byte table gRandomTable at the second index
 * gRandomIndex2, which advances by one (Random_Next with gRandomIndex's twin).
 */
u8 Random_Next2(void)
{
    return gRandomTable[gRandomIndex2++];
}
/**
 * Resets gRandomTable to its ROM copy gRandomTableInit (256 bytes, DMA 3).
 */
void Random_Reset(void)
{
    DmaCopy32(3, gRandomTableInit, gRandomTable, 0x100);
}
/**
 * Shuffles the byte table gRandomTable (read by Random_Get): swaps each of its 256
 * entries with the entry gRandomTableInit[seed] names, seed advancing by one per swap.
 */
void Random_Shuffle(s32 seed)
{
    s32 i;

    for (i = 0; i < 256; i++)
    {
        u8 *b = &gRandomTable[gRandomTableInit[(u8)seed++]];
        u8 *a = &gRandomTable[(u8)i];
        u8 tmp = *a;

        *a = *b;
        *b = tmp;
    }
}
/**
 * Stores (*x*, *y*) relative to (gCameraX, gCameraY) in screen[0..1]; returns 0
 * when that point is within *margin* pixels of the 240x160 screen (exclusive), else -1.
 */
s32 Util_GetScreenPos(s16 x, s16 y, s16 *screen, u8 margin)
{
    s32 sx = x - gCameraX;
    s32 sy;

    screen[0] = sx;
    sy = y - gCameraY;
    screen[1] = sy;
    if (sx > -margin && sx < margin + 240 && sy > -margin && sy < margin + 160)
        return 0;
    return -1;
}
/**
 * Like Util_GetScreenPos without storing the screen position: 0 when (*x*, *y*) relative to
 * (gCameraX, gCameraY) is within *margin* pixels of the 240x160 screen
 * (exclusive), else -1.
 */
s32 Util_IsOnScreen(s16 x, s16 y, u8 margin)
{
    s32 sx = x - gCameraX;
    s32 sy;

    if (sx <= -margin || sx >= margin + 240)
        return -1;
    sy = y - gCameraY;
    if (sy <= -margin || sy >= margin + 160)
        return -1;
    return 0;
}

/**
 * Moves *a* towards *b* by at most *step* (at least 1) and returns the result.
 * A leaf with branches that returns with a bare `bx lr`: it matches only without plain
 * agbcc's lr save, the evidence for the game module's -fprologue-bugfix (kururin.toml).
 */
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

/**
 * Util_StepTowards for signed bytes: moves *a* towards *b* by at most *step* (at least 1) and
 * returns the result.
 */
s8 Util_StepTowardsS8(s8 a, s8 b, u8 step)
{
    s8 diff = b - a;

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
/**
 * Returns the bits of *mask* (bit n: entry n of gLinkRecvData) whose entry is enabled in
 * gLinkStatus and has the high byte of *hi* in its command; Link_FindCommandExact compares both bytes.
 */
u32 Link_FindCommand(u32 mask, u32 hi)
{
    u32 enabled = gLinkStatus;
    u32 found = 0;
    u32 bit = 1;
    s32 i;

    hi &= 0xFF00;
    for (i = 0; i < 4; i++, bit <<= 1)
    {
        if ((bit & 0xF & enabled) && (gLinkRecvData[i].command & 0xFF00) == hi)
            found |= bit;
    }
    return found & mask;
}
/**
 * Returns the bits of *mask* (bit n: entry n of gLinkRecvData) whose entry is enabled in
 * gLinkStatus and has command == (hi & 0xFF00) | (lo & 0xFF).
 */
u32 Link_FindCommandExact(u32 mask, u32 hi, u32 lo)
{
    u32 key = (hi & 0xFF00) | (lo & 0xFF);
    u32 enabled = gLinkStatus;
    u32 found = 0;
    u32 bit = 1;
    s32 i;

    for (i = 0; i < 4; i++, bit <<= 1)
    {
        if ((bit & 0xF & enabled) && gLinkRecvData[i].command == key)
            found |= bit;
    }
    return found & mask;
}
/**
 * The direction (a byte angle, the index of gSineTable/gCosineTable) of the vector
 * (*dx*, *dy*): both halved until they fit in 0-63, looked up in gDirectionTable[|dy|][|dx|],
 * then mirrored by their signs.
 */
s32 Util_GetDirection(s32 dx, s32 dy)
{
    s32 ax = dx;
    s32 ay = dy;
    s32 dir;

    if (dx < 0)
        ax = -dx;
    if (dy < 0)
        ay = -dy;
    while (ax > 63)
    {
        ax >>= 1;
        ay >>= 1;
    }
    while (ay > 63)
    {
        ax >>= 1;
        ay >>= 1;
    }
    dir = gDirectionTable[ay][ax];
    if (dx < 0)
        dir = -dir;
    if (dy >= 0)
        dir = -(dir + 0x80);
    return (s8)dir;
}
/**
 * Starts the animation entry *frames* (two halfwords: a frame count and a value) on *anim*:
 * 1 when its count is positive, -1 for a 0 count (the end). A negative count jumps by that
 * many entries from anim->next and starts the entry there; then the result is 3.
 */
s32 Anim_Start(struct Anim *anim, const u16 *frames)
{
    s16 count = frames[0];

    if (count >= 0)
    {
        anim->timer = count;
        anim->value = frames[1];
        anim->next = frames + 2;
        if (count != 0)
            return 1;
        return -1;
    }
    Anim_Start(anim, anim->next + count * 2);
    return 3;
}
/**
 * Advances the animation *anim* by one frame: -1 when it has ended (counter <= 0), 0 while
 * the current frame lasts, else what Anim_Start returns for the next entry.
 */
s32 Anim_Step(struct Anim *anim)
{
    if ((s16)anim->timer <= 0)
        return -1;
    if ((s16)--anim->timer > 0)
        return 0;
    return Anim_Start(anim, anim->next);
}
/**
 * Anim_Start for a LongAnim animation: starts the entry *frames*, 1 when its count is
 * positive, -1 for a 0 count (the end); a negative count jumps by that many entries from
 * anim->next and starts the entry there, then the result is 3.
 */
s32 Anim_StartLong(struct LongAnim *anim, const struct LongAnimFrame *frames)
{
    s16 count = frames->frames;

    if (count >= 0)
    {
        anim->timer = count;
        anim->value = frames->value;
        anim->next = frames + 1;
        if (count != 0)
            return 1;
        return -1;
    }
    Anim_StartLong(anim, anim->next + count);
    return 3;
}
/**
 * Anim_Step for a LongAnim animation: -1 when it has ended, 0 while the current
 * frame lasts, else what Anim_StartLong returns for the next entry.
 */
s32 Anim_StepLong(struct LongAnim *anim)
{
    if ((s16)anim->timer <= 0)
        return -1;
    if ((s16)--anim->timer > 0)
        return 0;
    return Anim_StartLong(anim, anim->next);
}
