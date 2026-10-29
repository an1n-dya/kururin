// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// menu: the menu driver and its key handlers for items and values.
// 0800459C-08004C24, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "menu.h"

s32 Menu_Move(struct Menu *p, u32 keys);
s32 Menu_MoveWrap(struct Menu *p, u32 keys);

/**
 * Menu_Run with Menu_MoveWrap for the input: moves the current item on *keys*, calls
 * func with it (the result), then redraws every item and ends with 0x1FF.
 */
s32 Menu_RunWrap(struct Menu *p, u32 keys)
{
    s32 result;
    s32 i;

    p->unk4 = p->x;
    p->unk5 = p->y;
    Menu_MoveWrap(p, keys);
    result = p->func(p, p->current);
    for (i = 0; i <= p->itemCount; i++)
    {
        if (i == p->current)
            p->func(p, 0xFFFF);
        p->func(p, 0x100 + i);
    }
    p->func(p, 0x1FF);
    return result;
}
/**
 * Menu_RunWrap with Menu_MoveLeftRightWrap (left/right, wrapping) for the keys: moves the current item, calls the menu function
 * p->func for it (its result is returned), then for the cursor (0xFFFF, before the current
 * item) and items 0x100..0x100 + p->itemCount, then 0x1FF.
 */
s32 Menu_RunLeftRightWrap(struct Menu *p, u32 keys)
{
    s32 result;
    s32 i;

    p->unk4 = p->x;
    p->unk5 = p->y;
    Menu_MoveLeftRightWrap(p, keys);
    result = p->func(p, p->current);
    for (i = 0; i <= p->itemCount; i++)
    {
        if (i == p->current)
            p->func(p, 0xFFFF);
        p->func(p, 0x100 + i);
    }
    p->func(p, 0x1FF);
    return result;
}
/**
 * Updates *p* with *keys* (Menu_Move), then calls p->func with the current item, with
 * 0x100 + i for each item 0..p->itemCount (preceded by 0xFFFF for the current one) and finally
 * with 0x1FF. Returns the result of the first call.
 */
s32 Menu_Run(struct Menu *p, u32 keys)
{
    s32 result;
    s32 i;

    p->unk4 = p->x;
    p->unk5 = p->y;
    Menu_Move(p, keys);
    result = p->func(p, p->current);
    for (i = 0; i <= p->itemCount; i++)
    {
        if (i == p->current)
            p->func(p, 0xFFFF);
        p->func(p, 0x100 + i);
    }
    p->func(p, 0x1FF);
    return result;
}
s32 Menu_MoveLeftRight(struct Menu *p, u32 keys);
/**
 * Menu_RunWrap with Menu_MoveLeftRight (left/right) for the keys: moves the current item, calls the menu function
 * p->func for it (its result is returned), then for the cursor (0xFFFF, before the current
 * item) and items 0x100..0x100 + p->itemCount, then 0x1FF.
 */
s32 Menu_RunLeftRight(struct Menu *p, u32 keys)
{
    s32 result;
    s32 i;

    p->unk4 = p->x;
    p->unk5 = p->y;
    Menu_MoveLeftRight(p, keys);
    result = p->func(p, p->current);
    for (i = 0; i <= p->itemCount; i++)
    {
        if (i == p->current)
            p->func(p, 0xFFFF);
        p->func(p, 0x100 + i);
    }
    p->func(p, 0x1FF);
    return result;
}
s32 Menu_MoveSelect(struct Menu *p, u32 keys);
/**
 * Menu_RunWrap with Menu_MoveSelect (Select) for the keys: moves the current item, calls the menu function
 * p->func for it (its result is returned), then for the cursor (0xFFFF, before the current
 * item) and items 0x100..0x100 + p->itemCount, then 0x1FF.
 */
s32 Menu_RunSelect(struct Menu *p, u32 keys)
{
    s32 result;
    s32 i;

    p->unk4 = p->x;
    p->unk5 = p->y;
    Menu_MoveSelect(p, keys);
    result = p->func(p, p->current);
    for (i = 0; i <= p->itemCount; i++)
    {
        if (i == p->current)
            p->func(p, 0xFFFF);
        p->func(p, 0x100 + i);
    }
    p->func(p, 0x1FF);
    return result;
}
/**
 * Menu_Run without the input and the result: redraws every item of *p* (0x100 + i,
 * with 0xFFFF before the current one) and ends with 0x1FF.
 */
void Menu_Draw(struct Menu *p)
{
    s32 i;

    p->unk4 = p->x;
    p->unk5 = p->y;
    for (i = 0; i <= p->itemCount; i++)
    {
        if (i == p->current)
            p->func(p, 0xFFFF);
        p->func(p, 0x100 + i);
    }
    p->func(p, 0x1FF);
}
/**
 * Menu_Move with wrap-around: moves the current item p->current within 1..p->itemCount on
 * *keys*, up (0x40) from 1 going to the last item, down (0x80) from the last to 1; plays
 * song 0x20 when it changed and returns whether it did.
 */
s32 Menu_MoveWrap(struct Menu *p, u32 keys)
{
    s32 old = p->current;

    if (keys & 0x40)
    {
        if (old > 1)
            p->current--;
        else
            p->current = p->itemCount;
    }
    if (keys & 0x80)
    {
        if (p->current < p->itemCount)
            p->current++;
        else
            p->current = 1;
    }
    if (old != p->current)
        m4aSongNumStart(0x20);
    return p->current != old;
}
/**
 * Menu_MoveWrap on left (0x20, down) and right (0x10, up): moves the current item p->current
 * within 1..p->itemCount, wrapping around; plays song 0x20 when it changed and returns whether
 * it did.
 */
s32 Menu_MoveLeftRightWrap(struct Menu *p, u32 keys)
{
    s32 old = p->current;

    if (keys & 0x20)
    {
        if (old > 1)
            p->current--;
        else
            p->current = p->itemCount;
    }
    if (keys & 0x10)
    {
        if (p->current < p->itemCount)
            p->current++;
        else
            p->current = 1;
    }
    if (old != p->current)
        m4aSongNumStart(0x20);
    return p->current != old;
}
/**
 * Moves the current item p->current within 1..p->itemCount on *keys*: up (0x40) down by one, down
 * (0x80) up by one, stopping at the ends; plays song 0x20 when it changed and returns
 * whether it did.
 */
s32 Menu_Move(struct Menu *p, u32 keys)
{
    s32 old = p->current;

    if ((keys & 0x40) && old > 1)
        p->current--;
    if ((keys & 0x80) && p->current < p->itemCount)
        p->current++;
    if (old != p->current)
        m4aSongNumStart(0x20);
    return p->current != old;
}
/**
 * Menu_Move on left (0x20, down) and right (0x10, up): moves the current item p->current
 * within 1..p->itemCount, stopping at the ends; plays song 0x20 when it changed and returns
 * whether it did.
 */
s32 Menu_MoveLeftRight(struct Menu *p, u32 keys)
{
    s32 old = p->current;

    if ((keys & 0x20) && p->current > 1)
        p->current--;
    if ((keys & 0x10) && p->current < p->itemCount)
        p->current++;
    if (old != p->current)
        m4aSongNumStart(0x20);
    return p->current != old;
}
/**
 * Moves the current item p->current one down the list on Select (0x04), back to 1 after the
 * last one (p->itemCount); plays song 0x20 when it changed and returns whether it did.
 */
s32 Menu_MoveSelect(struct Menu *p, u32 keys)
{
    s32 old = p->current;

    if (keys & 4)
    {
        if (p->current < p->itemCount)
            p->current++;
        else
            p->current = 1;
    }
    if (old != p->current)
        m4aSongNumStart(0x20);
    return p->current != old;
}
/**
 * Moves *value* within 0..*max* on *keys* like Menu_StepValue, but wraps around: left (0x20)
 * from 0 goes to max, right (0x10) from max to 0. Plays song 0x20 when it changed and
 * returns whether it did.
 */
s32 Menu_StepValueWrap(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x20)
    {
        if (old > 0)
            *value = old - 1;
        else
            *value = max;
    }
    else if (keys & 0x10)
    {
        if (old < max)
            *value = old + 1;
        else
            *value = 0;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
/**
 * Moves the choice *value* (0..max) up on key 0x40 and down on key 0x80 of *keys*,
 * wrapping at both ends; plays song 0x20 and returns 1 when it changed, else 0.
 */
s32 Menu_StepValueUpDownWrap(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x40)
    {
        if (old > 0)
            *value = old - 1;
        else
            *value = max;
    }
    else if (keys & 0x80)
    {
        if (old < max)
            *value = old + 1;
        else
            *value = 0;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
/**
 * Moves *value* within 0..*max* on *keys*: left (0x20) down by one, right (0x10) up by one,
 * stopping at the ends; plays song 0x20 when it changed and returns whether it did.
 */
s32 Menu_StepValue(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x20)
    {
        if (old > 0)
            *value = old - 1;
    }
    else if (keys & 0x10)
    {
        if (old < max)
            *value = old + 1;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
/**
 * Menu_StepValue with the directions swapped: right (0x10) down by one, left (0x20) up by one,
 * stopping at 0 and *max*; plays song 0x20 when it changed and returns whether it did.
 */
s32 Menu_StepValueReversed(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x10)
    {
        if (old > 0)
            *value = old - 1;
    }
    else if (keys & 0x20)
    {
        if (old < max)
            *value = old + 1;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
/**
 * Menu_StepValue on up (0x40, down) and down (0x80, up): *value* within 0..*max*, stopping
 * at the ends; plays song 0x20 when it changed and returns whether it did.
 */
s32 Menu_StepValueUpDown(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x40)
    {
        if (old > 0)
            *value = old - 1;
    }
    else if (keys & 0x80)
    {
        if (old < max)
            *value = old + 1;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
/**
 * Menu_StepValue on L (0x200, down) and R (0x100, up): *value* within 0..*max*, stopping at
 * the ends; plays song 0x20 when it changed and returns whether it did.
 */
s32 Menu_StepValueLR(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x200)
    {
        if (old > 0)
            *value = old - 1;
    }
    else if (keys & 0x100)
    {
        if (old < max)
            *value = old + 1;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
/**
 * Menu_StepValueWrap on L (0x200, down) and R (0x100, up): moves *value* within 0..*max*,
 * wrapping around; plays song 0x20 when it changed and returns whether it did.
 */
s32 Menu_StepValueLRWrap(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x200)
    {
        if (old > 0)
            *value = old - 1;
        else
            *value = max;
    }
    else if (keys & 0x100)
    {
        if (old < max)
            *value = old + 1;
        else
            *value = 0;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
/**
 * Moves *value* within 0..*max* on *keys*, wrapping around: L (0x200) down by 10 and R
 * (0x100) up by 10, else left (0x20) down by one and right (0x10) up by one. A step of 10
 * that would reach or pass an end lands on the other end instead (L from 10 or less goes to
 * *max*, R from *max* - 10 or more to 0). Plays song 0x20 when it changed and returns
 * whether it did.
 */
s32 Menu_StepValueBy10(u16 *value, s32 max, u32 keys)
{
    s32 old = *value;

    if (keys & 0x200)
    {
        if (old > 10)
            *value = old - 10;
        else
            *value = max;
    }
    else if (keys & 0x100)
    {
        if (old < max - 10)
            *value = old + 10;
        else
            *value = 0;
    }
    else if (keys & 0x20)
    {
        if (old > 0)
            *value = old - 1;
        else
            *value = max;
    }
    else if (keys & 0x10)
    {
        if (old < max)
            *value = old + 1;
        else
            *value = 0;
    }
    if (old != *value)
        m4aSongNumStart(0x20);
    return *value != old;
}
