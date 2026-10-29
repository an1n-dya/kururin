// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// effects: effect tasks: window and fade effects, sparks, palette cycles, banners.
// 08017270-080181FC, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "task.h"
#include "util.h"
#include "effects.h"
#include "sound.h"

// The task Helirin_UpdateCollision starts with Effect_UpdateTimePenalty (0x10 bytes) when a hit adds to the timer.
struct TimePenalty {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ u16 y;
    /*0x0C*/ s16 frames; // frames left: the task frees itself when it reaches -1
    /*0x0E*/ u16 unkE;
};
// The task Level_StartTasks starts with Effect_CyclePalette (0x14 bytes).
struct PaletteCycle {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ const u8 *colorSets; // the world's colour sets, 0x20 bytes each
    /*0x0C*/ struct Anim anim;
};
extern const u16 gPaletteCycleAnim[];
extern const u16 gSpringSparkAnim[];
extern const u16 gCrashAnim[];
extern const void *const gCrashTiles[6];
// The task Level_StartTasks starts with Effect_UpdateGoal (0x18 bytes).
struct GoalEffect {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ u8 unk8[4];
    /*0x0C*/ struct Anim anim;
    /*0x14*/ u16 frames; // frames since the Helirin's flags got bit 3
    /*0x16*/ u16 unk16;
};
extern const u16 gGoalAnim[];
extern const u16 gGoalReachedAnim[];
extern const u8 *const gGoalPalettes[8];
// The tasks Effect_Firework starts with Effect_UpdateFireworkSpark (0x18 bytes): a sprite thrown with a
// velocity, x/y in 16.16 fixed point.
struct FireworkSpark {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
    /*0x10*/ s16 vx;     // 8.8 fixed point, slowed to 0xF4/0x100 every frame
    /*0x12*/ s16 vy;     // the same, plus 4 every frame
    /*0x14*/ u16 attr2;  // the sprite's tile/palette halfword (Sprite.attr2)
    /*0x16*/ u8 frames;  // frames left
    /*0x17*/ u8 list;    // the Sprite_Alloc list it is drawn in
};
extern const u16 gZoneHitRippleAnim[];
extern const u16 gHitSparkAnim[];

/**
 * A task: state 0 calls Bg_StartWindowCircle(0, 120, 80); state 1 passes gWindowRadius + 8 to
 * Bg_DrawWindowCircle every frame and, once gWindowRadius is 0xA0 or more, clears bits 0x2100 of
 * gDispcntCopy and frees itself.
 */
void Effect_OpenWindow(struct Task *task)
{
    switch ((s16)task->state)
    {
    case 0:
        Bg_StartWindowCircle(0, 0x78, 0x50);
        task->state = 1;
        break;
    case 1:
        Bg_DrawWindowCircle(gWindowRadius + 8, 0x78, 0x50);
        if (gWindowRadius >= 0xA0)
        {
            gDispcntCopy &= ~0x2100;
            Task_Destroy(task);
        }
        break;
    }
}
/**
 * A task: starts the window effect of Bg_StartWindowCircle at size 0xA0 around (0x78, 0x50), then
 * shrinks it by 8 a frame (Bg_DrawWindowCircle); at 0 or below it clears DISPCNT bits 8 and 13 (BG0
 * and window 0) in gDispcntCopy and frees itself.
 */
void Effect_CloseWindow(struct Task *task)
{
    switch ((s16)task->state)
    {
    case 0:
        Bg_StartWindowCircle(0xA0, 0x78, 0x50);
        task->state = 1;
        break;
    case 1:
        Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
        if (gWindowRadius <= 0)
        {
            gDispcntCopy &= 0xDEFF;
            Task_Destroy(task);
        }
        break;
    }
}
/**
 * A task: from 0, adds unk8 (0x80) to unkA every frame up to 0x1000 and stores unkA >> 8 in
 * gBldyCopy (Effect_BrightnessDown counts the other way); at 0x1000 it frees itself.
 */
void Effect_BrightnessUp(struct Task *task)
{
    u16 value;

    switch ((s16)task->state)
    {
    case 0:
        task->unk8 = 0x80;
        task->unkA = 0;
        task->state = 1;
        // fall through
    case 1:
        // value: as in Effect_BrightnessDown, agbcc then clamps the way the original does
        value = task->unkA += task->unk8;
        task->unkA = (value > 0xFFF) ? 0x1000 : task->unkA;
        gBldyCopy = task->unkA >> 8;
        if (task->unkA > 0xFFF)
            Task_Destroy(task);
        break;
    }
}
/**
 * A task: from 0x1000, takes unk8 (0x80) off unkA every frame down to 0 and stores
 * unkA >> 8 in gBldyCopy (Effect_BrightnessUp counts the other way); at 0 it frees itself.
 */
void Effect_BrightnessDown(struct Task *task)
{
    u16 value;

    switch ((s16)task->state)
    {
    case 0:
        task->unk8 = 0x80;
        task->unkA = 0x1000;
        task->state = 1;
        // fall through
    case 1:
        // value: written this way, agbcc keeps the original's clamp (a no-op for a u16)
        // instead of folding it away
        value = task->unkA -= task->unk8;
        task->unkA = (value <= 0) ? 0 : task->unkA;
        gBldyCopy = task->unkA >> 8;
        if (task->unkA == 0)
            Task_Destroy(task);
        break;
    }
}
// 0x03000910: the copies of BG2PA-BG2PD, BG2X and BG2Y (gDispcntCopy + 0x20)
struct BgAffine {
    /*0x00*/ s16 pa;
    /*0x02*/ s16 pb;
    /*0x04*/ s16 pc;
    /*0x06*/ s16 pd;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
};
extern struct BgAffine gBg2AffineCopy;

/**
 * A task (no state of its own) that turns BG2 by half the frame counter (in 256ths of a
 * turn, from the gCosineTable/gSineTable tables, also kept in layer 2's unk15[0])
 * around the screen's centre (0x78, 0x50).
 */
void Effect_RotateBg2(struct Task *task)
{
    u8 angle;
    s16 a;
    s16 b;
    s16 negB;
    s32 cx = 0x78;
    s32 cy = 0x50;

    angle = gMain.frameCount >> 1;
    gBgLayers[2].unk15[0] = angle;
    a = gCosineTable[angle];
    b = gSineTable[angle];
    negB = -b;
    gBg2AffineCopy.pa = a;
    gBg2AffineCopy.pb = b;
    gBg2AffineCopy.pc = negB;
    gBg2AffineCopy.pd = a;
    gBg2AffineCopy.x = (cx << 8) - (a * cx + b * cy);
    gBg2AffineCopy.y = (cy << 8) - (negB * cx + a * cy);
}
/**
 * The task Helirin_Update starts: it plays gCrashAnim (Anim_Step) while
 * gGameState.paused is 0, copying the 0x800 bytes of gCrashTiles[frame] to OBJ VRAM
 * 0x06015980 when the result has bit 0 set, and frees itself at the end. While on screen
 * (margin 0x40) it draws a 64x64 sprite (tile 0x2CC, palette 3) centred on (x, y).
 */
void Effect_UpdateCrash(struct Task *task)
{
    struct AnimObject *t = (struct AnimObject *)task;
    struct Sprite *spr;
    s32 result;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gCrashAnim;
        t->anim.value = gCrashAnim[1];
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.paused == 0)
        {
            result = Anim_Step(&t->anim);
            if (result < 0)
            {
                Task_Destroy((struct Task *)t);
                break;
            }
            if (result & 1)
                Main_QueueCopy(gCrashTiles[t->anim.value], (void *)0x06015980, 0x800);
        }
        if (Util_GetScreenPos(t->x, t->y, t->screen, 0x40) == 0)
        {
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = (((t->screen[0] - 0x20) << 16) & 0x1FF0000) | 0xC0000000 | (u8)(t->screen[1] - 0x20);
                spr->attr2 = 0x36CC;
            }
        }
        break;
    }
}
/**
 * A task Helirin_UpdateCollision starts at each colliding point of a Helirin (unless its flags have
 * 0x40008 set): it plays gHitSparkAnim (Anim_Step) while gGameState.paused is 0 and frees
 * itself at the end; while on screen it draws the frame as a 32x32 sprite centred on (x, y).
 */
void Effect_UpdateHitSpark(struct Task *task)
{
    struct AnimObject *t = (struct AnimObject *)task;
    struct Sprite *spr;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gHitSparkAnim;
        t->anim.value = gHitSparkAnim[1];
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.paused == 0 && Anim_Step(&t->anim) < 0)
        {
            Task_Destroy((struct Task *)t);
            break;
        }
        if (Util_GetScreenPos(t->x, t->y, t->screen, 0x20) == 0)
        {
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = (((t->screen[0] - 0x10) << 16) & 0x1FF0000) | 0x80000000 | (u8)(t->screen[1] - 0x10);
                spr->attr2 = t->anim.value | 0x3400;
            }
        }
        break;
    }
}
/**
 * Effect_UpdateHitSpark with the frames gZoneHitRippleAnim: the task Helirin_UpdateCollision starts at each colliding
 * point of a Helirin whose flags have 0x40008 set (in a zone, or past the goal: no damage). On screen
 * (kurutest --shot of the original) its frames are blue rings, where Effect_UpdateHitSpark's are a
 * yellow star.
 */
void Effect_UpdateZoneHitRipple(struct Task *task)
{
    struct AnimObject *t = (struct AnimObject *)task;
    struct Sprite *spr;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gZoneHitRippleAnim;
        t->anim.value = gZoneHitRippleAnim[1];
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.paused == 0 && Anim_Step(&t->anim) < 0)
        {
            Task_Destroy((struct Task *)t);
            break;
        }
        if (Util_GetScreenPos(t->x, t->y, t->screen, 0x20) == 0)
        {
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = (((t->screen[0] - 0x10) << 16) & 0x1FF0000) | 0x80000000 | (u8)(t->screen[1] - 0x10);
                spr->attr2 = t->anim.value | 0x3400;
            }
        }
        break;
    }
}
/**
 * The task Object_SpringDown starts when a Helirin turns towards it: it plays gSpringSparkAnim
 * (Anim_Step) while gGameState.paused is 0 and frees itself at the end; while on screen
 * it draws the frame as a 16x16 sprite centred on (x, y).
 */
void Effect_UpdateSpringSpark(struct Task *task)
{
    struct AnimObject *t = (struct AnimObject *)task;
    struct Sprite *spr;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gSpringSparkAnim;
        t->anim.value = gSpringSparkAnim[1];
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.paused == 0 && Anim_Step(&t->anim) < 0)
        {
            Task_Destroy((struct Task *)t);
            break;
        }
        if (Util_GetScreenPos(t->x, t->y, t->screen, 8) == 0)
        {
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = (((t->screen[0] - 8) << 16) & 0x1FF0000) | 0x40000000 | (u8)(t->screen[1] - 8);
                spr->attr2 = t->anim.value | 0x3400;
            }
        }
        break;
    }
}
/**
 * A task that draws one sprite (tile 0x1CC, palette 0xD) at (0x60, 0x2A) moving up a pixel
 * a frame for 0x1E frames (Sprite_Alloc list 3), then frees itself.
 */
void Effect_UpdateTimePenalty(struct Task *task)
{
    struct TimePenalty *t = (struct TimePenalty *)task;
    struct Sprite *spr;

    switch ((s16)t->state)
    {
    case 0:
        t->x = 0x60;
        t->y = 0x2A;
        t->frames = 0x1E;
        t->state = 1;
        // fall through
    case 1:
        if (--t->frames == -1)
        {
            Task_Destroy((struct Task *)t);
            break;
        }
        t->y--;
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0x80000000 | (u8)t->y;
            spr->attr2 = 0xD1CC;
        }
        break;
    }
}
void Effect_UpdateFireworkSpark(struct Task *task);

// The pixel (high halfword) of a 16.16 position, written without touching the fraction
#define POS_HIGH(pos) (*((s16 *)&(pos) + 1))

/**
 * A one-frame task that bursts 32 thrown sprites (Effect_UpdateFireworkSpark tasks) from a random point,
 * one per eighth of the gSineTable/gCosineTable directions, playing song 0x50, then
 * frees itself. Every third ring (bit 3, bit 4, neither) of directions gets its own tile,
 * lifetime and speed (0x200, 0x160, 0xC0). In main state 4 the point is x 8..39 (state set)
 * or 200..231, y 0..39, in list 15; otherwise x 0x3C..0xB3, y 0x20..0x6F, in list 3, with
 * other tiles.
 */
void Effect_Firework(struct Task *task)
{
    struct FireworkSpark *t;
    s32 x;
    s32 y;
    s32 speed;
    s32 i;

    m4aSongNumStart(0x50);
    if (gMain.state == 4)
    {
        x = Random_Next() * 32 / 256;
        y = Random_Next() * 40 / 256;
        if ((s16)task->state != 0)
            x += 8;
        else
            x += 200;
        for (i = 0; i <= 0xFF; i += 8)
        {
            t = (struct FireworkSpark *)Task_Create(Effect_UpdateFireworkSpark, 0x18);
            if (t == NULL)
                continue;
            t->key = (i & 1) ? 5 : 6;
            POS_HIGH(t->x) = x;
            POS_HIGH(t->y) = y;
            if (i & 8)
            {
                t->attr2 = 0xEFFF;
                t->frames = 0x28;
                speed = 0x200;
            }
            else if (i & 0x10)
            {
                t->attr2 = 0xDFFF;
                t->frames = 0x2C;
                speed = 0x160;
            }
            else
            {
                t->attr2 = 0xCFFF;
                t->frames = 0x30;
                speed = 0xC0;
            }
            t->list = 15;
            t->vx = (gSineTable[(u8)i] * speed) >> 8;
            t->vy = (-gCosineTable[(u8)i] * speed) >> 8;
        }
    }
    else
    {
        x = Random_Next() * 120 / 256 + 0x3C;
        y = Random_Next() * 80 / 256 + 0x20;
        for (i = 0; i <= 0xFF; i += 8)
        {
            t = (struct FireworkSpark *)Task_Create(Effect_UpdateFireworkSpark, 0x18);
            if (t == NULL)
                continue;
            t->key = (i & 1) ? 5 : 6;
            POS_HIGH(t->x) = x;
            POS_HIGH(t->y) = y;
            if (i & 8)
            {
                t->attr2 = 0x33FF;
                t->frames = 0x28;
                speed = 0x200;
            }
            else if (i & 0x10)
            {
                t->attr2 = 0x63FF;
                t->frames = 0x2C;
                speed = 0x160;
            }
            else
            {
                t->attr2 = 0x23FF;
                t->frames = 0x30;
                speed = 0xC0;
            }
            t->list = 3;
            t->vx = (gSineTable[(u8)i] * speed) >> 8;
            t->vy = (-gCosineTable[(u8)i] * speed) >> 8;
        }
    }
    Task_Destroy(task);
}
/**
 * A task that moves a sprite for *frames* frames, then frees itself: each frame its
 * velocity slows to 0xF4/0x100 (vy also gains 4), it moves by the velocity and is drawn as
 * one sprite (Sprite_Alloc list *list*) at its position.
 */
void Effect_UpdateFireworkSpark(struct Task *task)
{
    struct FireworkSpark *t = (struct FireworkSpark *)task;
    struct Sprite *spr;

    switch ((s16)t->state)
    {
    case 0:
        t->state = 1;
        // fall through
    case 1:
        if (t->frames-- == 0)
        {
            Task_Destroy((struct Task *)t);
            break;
        }
        t->vx = t->vx * 0xF4 / 0x100;
        t->vy = t->vy * 0xF4 / 0x100 + 4;
        t->x += t->vx << 8;
        t->y += t->vy << 8;
        spr = Sprite_Alloc(t->list);
        if (spr != NULL)
        {
            spr->attr01 = ((POS_PIXEL(t->x) << 16) & 0x1FF0000) | (u8)POS_PIXEL(t->y);
            spr->attr2 = t->attr2;
        }
        break;
    }
}
/**
 * A task that plays gGoalAnim, then gGoalReachedAnim once the current Helirin's flags have
 * bit 3, copying gGoalPalettes[frame] (and +0x100) to BG palettes 11 and 12 (0x05000160,
 * 0x05000180) on each frame with bit 0 set (Anim_Step). While bit 3 stays set (and gGameState.mode is
 * not 5) it counts frames and starts a Effect_Firework task at frame 0x1E, and at 0x37, 0x50,
 * 0x69 and 0x82 while the Helirin's bumpCount is 0.
 */
void Effect_UpdateGoal(struct Task *task)
{
    struct GoalEffect *t = (struct GoalEffect *)task;
    struct Helirin *h = gGameState.helirin;

    switch ((s16)t->state)
    {
    case 0:
        t->key = 2;
        t->anim.timer = 1;
        t->anim.next = gGoalAnim;
        t->anim.value = gGoalAnim[1];
        t->state = 1;
        // fall through
    case 1:
        if (!(h->flags & 8))
            break;
        t->anim.timer = 1;
        t->anim.next = gGoalReachedAnim;
        t->anim.value = gGoalReachedAnim[1];
        t->frames = 0;
        t->state = 2;
        // fall through
    case 2:
        if (!(h->flags & 8))
            break;
        if ((s8)gGameState.mode == 5)
            break;
        switch (t->frames++)
        {
        case 0x37:
        case 0x50:
        case 0x69:
        case 0x82:
            if ((s16)h->bumpCount != 0)
                break;
            // fall through
        case 0x1E:
            Task_Create(Effect_Firework, 8);
            break;
        }
        break;
    default:
        return;
    }
    if (Anim_Step(&t->anim) & 1)
    {
        Main_QueueCopy(gGoalPalettes[t->anim.value], (void *)0x05000160, 0x20);
        Main_QueueCopy(gGoalPalettes[t->anim.value] + 0x100, (void *)0x05000180, 0x20);
    }
}
/**
 * A task that cycles the first colours of the BG palette: it plays gPaletteCycleAnim
 * (Anim_Step) and on a new frame (1 or 3) copies colour set anim.value of the world
 * (KuruTools addr7) to BG palette 0 with Main_QueueCopy; at the end (-1) it frees itself.
 */
void Effect_CyclePalette(struct Task *task)
{
    struct PaletteCycle *t = (struct PaletteCycle *)task;

    switch ((s16)t->state)
    {
    case 0:
        t->key = 1;
        t->anim.timer = 1;
        t->anim.next = gPaletteCycleAnim;
        t->anim.value = gPaletteCycleAnim[1];
        t->colorSets = gWorldData + gWorldInfo->colorSetsOffset;
        t->state = 1;
        // fall through
    case 1:
        switch (Anim_Step(&t->anim))
        {
        case 0:
            break;
        case 1:
        case 3:
            Main_QueueCopy(t->colorSets + t->anim.value * 0x20, (void *)0x05000000, 0x20);
            break;
        case -1:
            Task_Destroy((struct Task *)t);
            break;
        }
        break;
    }
}
/**
 * A task that steps its animation (Anim_Step): on a new frame (1 or 3) it queues the
 * copy copies[anim.value] with Main_QueueCopy; at the end (-1) it frees itself.
 */
void Effect_AnimateCopies(struct Task *task)
{
    struct CopyAnim *t = (struct CopyAnim *)task;
    const struct QueuedCopy *copy;

    switch ((s16)t->state)
    {
    case 0:
        t->key = 1;
        t->state = 1;
        // fall through
    case 1:
        switch (Anim_Step(&t->anim))
        {
        case 0:
            break;
        case 1:
        case 3:
            copy = &t->copies[t->anim.value];
            Main_QueueCopy(copy->src, copy->dst, copy->size);
            break;
        case -1:
            Task_Destroy((struct Task *)t);
            break;
        }
        break;
    }
}
/**
 * A task that steps its animation (Anim_Step): on a new frame (1 or 3) it queues the
 * copy copies[anim.value] with Main_QueueBlockCopy; at the end (-1) it frees itself. Like
 * Effect_AnimateCopies, without setting its key.
 */
void Effect_AnimateBlockCopies(struct Task *task)
{
    struct BlockCopyAnim *t = (struct BlockCopyAnim *)task;
    const struct BlockCopy *copy;

    switch ((s16)t->state)
    {
    case 0:
        t->state = 1;
        // fall through
    case 1:
        switch (Anim_Step(&t->anim))
        {
        case 0:
            break;
        case 1:
        case 3:
            copy = &t->copies[t->anim.value];
            Main_QueueBlockCopy(copy->src, copy->dst, copy->unk8, copy->unk9);
            break;
        case -1:
            Task_Destroy((struct Task *)t);
            break;
        }
        break;
    }
}
extern const u8 gGoalBannerTiles[];

/**
 * A task (started by Play_CheckLevelEnd when a Helirin's flags have bit 2): it frees the task with
 * key 3 (Task_FindByKey), takes key 3 itself, copies the 0x1000 bytes of OBJ tiles
 * gGoalBannerTiles to 0x06011400 and then moves two 64x64 sprites (tiles 0xA0 and 0xE0, side by
 * side) from x 0xF0, y 0x30 left by 3 pixels a frame until x is below -0x80, where it frees
 * itself.
 */
void Effect_UpdateGoalBanner(struct Task *task)
{
    struct Task *old;
    struct Sprite *spr;
    s32 x, y;

    switch ((s16)task->state)
    {
    case 0:
        old = Task_FindByKey(3);
        if (old != NULL)
            Task_Destroy(old);
        task->key = 3;
        Main_QueueCopy(gGoalBannerTiles, (void *)0x06011400, 0x1000);
        task->unk8 = 0xF0;
        task->unkA = 0x30;
        task->state = 1;
        break;
    case 1:
        task->unk8 -= 3;
        if ((s16)task->unk8 < -0x80)
        {
            Task_Destroy(task);
            break;
        }
        x = (s16)task->unk8;
        y = (s16)task->unkA;
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = ((x << 16) & 0x1FF0000) | 0xC0000000 | (u8)y;
            spr->attr2 = 0xD0A0;
        }
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = (((x + 0x40) << 16) & 0x1FF0000) | 0xC0000000 | (u8)y;
            spr->attr2 = 0xD0E0;
        }
        break;
    }
}
extern const u8 gLinkEndBannerTiles[];

/**
 * Effect_UpdateGoalBanner with the OBJ tiles gLinkEndBannerTiles (started by Play_CheckLevelEnd in a linked game
 * when at least gGameState.playerCount - 1 Helirins have flags bit 3): two 64x64 sprites move left from x 0xF0,
 * y 0x30 by 3 pixels a frame until x is below -0x80, where the task frees itself.
 */
void Effect_UpdateLinkEndBanner(struct Task *task)
{
    struct Task *old;
    struct Sprite *spr;
    s32 x, y;

    switch ((s16)task->state)
    {
    case 0:
        old = Task_FindByKey(3);
        if (old != NULL)
            Task_Destroy(old);
        task->key = 3;
        Main_QueueCopy(gLinkEndBannerTiles, (void *)0x06011400, 0x1000);
        task->unk8 = 0xF0;
        task->unkA = 0x30;
        task->state = 1;
        break;
    case 1:
        task->unk8 -= 3;
        if ((s16)task->unk8 < -0x80)
        {
            Task_Destroy(task);
            break;
        }
        x = (s16)task->unk8;
        y = (s16)task->unkA;
        // The halfword and the y byte through casts, as in Effect_UpdateTimeUpBanner.
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = ((x << 16) & 0x1FF0000) | 0xC0000000;
            ((u16 *)spr)[2] = 0xD0A0;
            ((u8 *)spr)[0] = y;
        }
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = (((x + 0x40) << 16) & 0x1FF0000) | 0xC0000000;
            ((u16 *)spr)[2] = 0xD0E0;
            ((u8 *)spr)[0] = y;
        }
        break;
    }
}
extern const u8 gTimeUpBannerTiles[];

/**
 * A task (started by Play_CheckLevelEnd in a linked game when a Helirin's timer passes 0x2A2F): it
 * frees the task with key 3 (Task_FindByKey), takes key 3 itself, copies the 0x1000 bytes of
 * OBJ tiles gTimeUpBannerTiles to 0x06011400 and then draws two 64x64 sprites (tiles 0xA0 and 0xE0
 * at x 0x38 and 0x78, y 0x30) for 0x78 frames, after which it frees itself.
 */
void Effect_UpdateTimeUpBanner(struct Task *task)
{
    struct Task *old;
    struct Sprite *spr;

    switch ((s16)task->state)
    {
    case 0:
        old = Task_FindByKey(3);
        if (old != NULL)
            Task_Destroy(old);
        task->key = 3;
        Main_QueueCopy(gTimeUpBannerTiles, (void *)0x06011400, 0x1000);
        task->unkC = 0x78;
        task->state = 1;
        break;
    case 1:
        if (--task->unkC == 0xFFFF)
        {
            Task_Destroy(task);
            break;
        }
        // Word, halfword and byte stores as the original makes them: the halfword through a
        // cast (a u16 field store would load the tile straight into r0), then y.
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = 0xC0380000;
            ((u16 *)spr)[2] = 0xD0A0;
            ((u8 *)spr)[0] = 0x30;
        }
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = 0xC0780000;
            ((u16 *)spr)[2] = 0xD0E0;
            ((u8 *)spr)[0] = 0x30;
        }
        break;
    }
}
/**
 * A task of the linked game: it frees the task with key 3 (Task_FindByKey), takes key 3, and
 * moves a banner (two 64x64 sprites, tiles 0x22A and 0x26A) in from x -0x80 to 0x38 at 8
 * pixels a frame, holds it for 0x79 frames and moves it out until x passes 0xEF, where it
 * frees itself. Under the banner, for each player in gLinkPlayers whose gGameState.unk3C
 * entry is 0, two 16x16 sprites (tile 0x1E4 and the player's number, 0x1E8 + 4 * n),
 * centred, with tile 0x2AA between players; palette 5 while bit 3 of the frame counter is
 * set, else player 0's Helirin palette (gHelirins[0].palette).
 */
void Effect_UpdateLinkResultBanner(struct Task *task)
{
    struct Task *old;
    s32 x, y, i, count;
    u32 palette;

    switch ((s16)task->state)
    {
    case 0:
        old = Task_FindByKey(3);
        if (old != NULL)
            Task_Destroy(old);
        task->key = 3;
        task->unk8 = -0x80;
        task->unkA = 0x20;
        task->unkC = 0;
        task->state = 1;
        break;
    case 1:
        task->unk8 += 8;
        if ((s16)task->unk8 > 0x37)
        {
        next:
            task->state++;
        }
    draw:
        x = (s16)task->unk8;
        y = (s16)task->unkA;
        {
            struct Sprite *spr = Sprite_Alloc(2);

            if (spr != NULL)
            {
                spr->attr01 = ((x << 16) & 0x1FF0000) | 0xC0000000;
                ((u16 *)spr)[2] = 0xD22A;
                *(u8 *)&spr->attr01 = y + 0x18;
            }
        }
        {
            struct Sprite *spr = Sprite_Alloc(2);

            x += 0x40;
            if (spr != NULL)
            {
                spr->attr01 = ((x << 16) & 0x1FF0000) | 0xC0000000;
                ((u16 *)spr)[2] = 0xD26A;
                *(u8 *)&spr->attr01 = y + 0x18;
            }
        }
        if (gMain.frameCount & 8)
            palette = 0x5000;
        else
            palette = gHelirins[0].palette;

        count = 0;
        for (i = 0; i < 4; i++)
        {
            if (((gLinkPlayers >> i) & 1) && gGameState.unk3C[i] == 0)
                count++;
        }
        x -= (count * 32 + (count - 1) * 16) >> 1;
        for (i = 0; i < 4; i++)
        {
            if (((gLinkPlayers >> i) & 1) && gGameState.unk3C[i] == 0)
            {
                {
                    struct Sprite *spr = Sprite_Alloc(2);

                    if (spr != NULL)
                    {
                        spr->attr01 = ((x << 16) & 0x1FF0000) | 0x40000000;
                        ((u16 *)spr)[2] = palette | 0x1E4;
                        *(u8 *)&spr->attr01 = y;
                    }
                }
                {
                    struct Sprite *spr = Sprite_Alloc(2);

                    if (spr != NULL)
                    {
                        spr->attr01 = (((x + 0x10) << 16) & 0x1FF0000) | 0x40000000;
                        ((u16 *)spr)[2] = ((i + 1) * 4 + 0x1E4) | palette;
                        *(u8 *)&spr->attr01 = y;
                    }
                }
                x += 0x20;
                if (--count == 0)
                    break;
                {
                    struct Sprite *spr = Sprite_Alloc(2);

                    if (spr != NULL)
                    {
                        spr->attr01 = ((x << 16) & 0x1FF0000) | 0x40000000;
                        ((u16 *)spr)[2] = 0xD2AA;
                        *(u8 *)&spr->attr01 = y;
                    }
                }
                x += 0x10;
            }
        }
        break;
    case 2:
        if (task->unkC++ <= 0x78)
            goto draw;
        goto next;
    case 3:
        task->unk8 += 8;
        if ((s16)task->unk8 <= 0xEF)
            goto draw;
        Task_Destroy(task);
        break;
    }
}
extern const u8 gDemoLabelTiles[];

/**
 * A task (8 bytes): in state 0 it copies gDemoLabelTiles (0x180 bytes) to OBJ VRAM 0x06011400;
 * then every frame it draws two sprites from it at (0xB8, 8) and (0xD8, 8), palette 15, or
 * 14 while bit 4 or 5 of the frame counter is set.
 */
void Effect_DrawDemoLabel(struct Task *task)
{
    struct Sprite *spr;
    u32 palette;

    switch ((s16)task->state)
    {
    case 0:
        Main_QueueCopy(gDemoLabelTiles, (void *)0x06011400, 0x180);
        task->state = 1;
        break;
    case 1:
        palette = (gMain.frameCount & 0x30) ? 0xE000 : 0xF000;
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = 0x80B84000;
            spr->attr2 = palette | 0xA0;
            *(u8 *)&spr->attr01 = 8;
        }
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = 0x40D80000;
            spr->attr2 = palette | 0xA8;
            *(u8 *)&spr->attr01 = 8;
        }
        break;
    }
}
extern const u8 gCountdownTiles[];
extern const u8 gCountdownGoTiles[];

/**
 * A task started in level state 2 when the linked players are ready (Play_RunLevel): a
 * countdown of two sprites. Step 0 waits 60 frames; step 1 (while gGameState.paused is 0)
 * counts them down, then loads the tiles gCountdownTiles, plays song 0x3F and waits 120
 * frames in step 2, which at 30 left loads gCountdownGoTiles, starts Sound_PlaySong(gGameState.song),
 * plays songs 0x40 and 0x2F and sets the player's Helirin going (flags bits 0-1 set,
 * disableFlags bit 1 cleared), and at 0 ends the task. Step 2 draws both sprites each frame
 * (steps 0-1 draw nothing, as in the original).
 */
void Effect_UpdateLinkCountdown(struct Task *task)
{
    struct Helirin *h = gGameState.helirin;
    s16 timer;
    u32 x, y;

    switch ((s16)task->state)
    {
    case 0:
        task->unk8 = 60;
        task->state = 1;
        break;
    case 1:
        if (gGameState.paused == 0 && --task->unk8 == 0)
        {
            Main_QueueCopy(gCountdownTiles, (void *)0x06011400, 0x1000);
            m4aSongNumStart(0x3F);
            task->unk8 = 120;
            task->state = 2;
        }
        break;
    case 2:
        if (gGameState.paused == 0)
        {
            timer = --task->unk8;
            if (timer == 0)
            {
                Task_Destroy(task);
                break;
            }
            if (timer == 30)
            {
                Main_QueueCopy(gCountdownGoTiles, (void *)0x06011400, 0x1000);
                Sound_PlaySong(gGameState.song);
                m4aSongNumStart(0x40);
                m4aSongNumStart(0x2F);
                h->flags |= 3;
                h->disableFlags &= ~2;
            }
        }
        // Word, halfword and byte stores as the original makes them (as Effect_UpdateTimeUpBanner's),
        // one sprite variable per block (each gets its own register, as in the original);
        // x is a variable, so the first word is built with an OR, as the original does.
        x = 0x38;
        y = 0x30;
        {
            struct Sprite *spr = Sprite_Alloc(2);

            if (spr != NULL)
            {
                spr->attr01 = (x << 16) | 0xC0000000;
                ((u16 *)spr)[2] = 0xD0A0;
                ((u8 *)spr)[0] = y;
            }
        }
        {
            struct Sprite *spr = Sprite_Alloc(2);

            if (spr != NULL)
            {
                spr->attr01 = 0xC0780000;
                ((u16 *)spr)[2] = 0xD0E0;
                ((u8 *)spr)[0] = y;
            }
        }
        break;
    }
}
