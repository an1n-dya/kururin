// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// objects: the world objects of a level (spinners, shooters, swingers, turrets).
// 08015890-08016BF4, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "sprite.h"
#include "task.h"
#include "util.h"
#include "collision.h"
#include "helirin.h"
#include "objects.h"
#include "effects.h"

extern const u16 gSpringAnim[];
extern const u16 gSpringHitAnim[];
extern const struct CollisionShape gSpringShape[];
// The task Object_Roller starts with Object_RollerProjectile (0x2C bytes); x/y are 16.16 fixed point.
struct RollerProjectile {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
    /*0x10*/ s32 vx;    // Object_RollerProjectile: gSineTable[dir] * speed, added to x every frame
    /*0x14*/ s32 vy;    // -gCosineTable[dir] * speed, added to y
    /*0x18*/ s16 screen[2]; // from Util_GetScreenPos
    /*0x1C*/ u8 affineSet;  // its sprite's affine set (0: none)
    /*0x1D*/ u8 dir;      // the Roller's dir: into gSineTable/gCosineTable
    /*0x1E*/ s16 speed;   // the Roller's speed: multiplies them
    /*0x20*/ struct Anim anim;
    /*0x28*/ s16 aheadX; // 15/256 of the x step: the map is tested that far ahead of x
    /*0x2A*/ s16 aheadY; // and of y
};
struct RollerProjectile;
void Object_RollerProjectile(struct RollerProjectile *task);
extern const struct CollisionShape gPistonShape[];
extern const struct CollisionShape gRollerProjectileShape[];
extern const u16 gRollerProjectileAnim[];

/**
 * An up spring (map tile 0xF8, KururinTAS spring.txt "UP"): Object_SpringDown for one Helirin only:
 * Helirin_Bump(h, 0, 0x100), the angle tested as is or + 0x8000 with Object_SpringRight's
 * conditions, its Effect_UpdateSpringSpark task at (x + 8, y), a plain sprite.
 */
void Object_SpringUp(struct AnimObject *t)
{
    struct Helirin *h;
    struct Task *task;
    struct Sprite *spr;
    u32 hits;
    u16 angle;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gSpringAnim;
        t->anim.value = gSpringAnim[1];
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    h = gGameState.helirin;
    if (gGameState.paused == 0)
    {
        hits = Collision_TestShapes(h, t->x, t->y, gSpringShape);
        if (hits == 0)
        {
            Anim_Step(&t->anim);
        }
        else
        {
            Anim_Start(&t->anim, gSpringHitAnim);
            Helirin_Bump(h, 0x00, 0x100);
            m4aSongNumStart(0x36);
            if (hits & 0x15554)
                angle = h->angle;
            else if (hits & 0xAAAA)
                angle = h->angle + 0x8000;
            else
                goto draw;
            if (h->angleDefaultRate > 0 ? (s16)angle >= 0 : angle > 0x8000)
            {
                h->nextAngleDefaultRate = -h->angleDefaultRate;
                task = Task_Create(Effect_UpdateSpringSpark, 0x18);
                if (task != NULL)
                {
                    task->unk8 = t->x + 8;
                    task->unkA = t->y;
                }
            }
            h->nextAngleRate = h->nextAngleDefaultRate > 0 ? 0x300 : -0x300;
        }
    }

draw:
    if (Util_GetScreenPos(t->x, t->y, t->screen, 0x10) == 0)
    {
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = ((t->screen[0] << 16) & 0x01FF0000) | 0x40000000 | (u8)t->screen[1];
            spr->attr2 = t->anim.value | 0x3400;
        }
    }
}
/**
 * A down spring (map tile 0xF8 flipped vertically, spring.txt "DOWN"): until gGameState.paused
 * is set, each frame it tests the current
 * Helirin (state 1) or the first two (state 2, world 0) with Collision_TestShapes. On a hit it
 * restarts its animation (gSpringHitAnim), calls Helirin_Bump, plays sound 0x36 and, when
 * the Helirin turns towards it (by the hit's bits, its angle and angleDefaultRate), sets
 * nextAngleDefaultRate = -angleDefaultRate and starts a Effect_UpdateSpringSpark task at (x + 8, y + 0x10); nextAngleRate
 * becomes 0x300 with nextAngleDefaultRate's sign. Without a hit the animation steps. It is drawn when on
 * screen.
 */
void Object_SpringDown(struct AnimObject *t)
{
    struct Helirin *h;
    struct Task *task;
    struct Sprite *spr;
    u32 hits;
    u16 angle;
    s32 count;
    s32 i;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gSpringAnim;
        t->anim.value = gSpringAnim[1];
        if (gGameState.world == 0)
            t->state = 2;
        else
            t->state = 1;
        // the first frame always tests the current Helirin, even in state 2
        // fall through
    case 1:
        h = gGameState.helirin;
        count = 1;
        break;
    case 2:
        h = &gHelirins[0];
        count = 2;
        break;
    default:
        return;
    }

    if (gGameState.paused == 0)
    {
        for (i = 0; i < count; i++, h++)
        {
            hits = Collision_TestShapes(h, t->x, t->y, gSpringShape);
            if (hits == 0)
            {
                Anim_Step(&t->anim);
                continue;
            }
            Anim_Start(&t->anim, gSpringHitAnim);
            Helirin_Bump(h, 0x80, 0x100);
            m4aSongNumStart(0x36);
            if (hits & 0x15554)
                angle = h->angle;
            else if (hits & 0xAAAA)
                angle = h->angle + 0x8000;
            else
                continue;
            if (h->angleDefaultRate > 0 ? angle > 0x8000 : (s16)angle >= 0)
            {
                h->nextAngleDefaultRate = -h->angleDefaultRate;
                task = Task_Create(Effect_UpdateSpringSpark, 0x18);
                if (task != NULL)
                {
                    task->unk8 = t->x + 8;
                    task->unkA = t->y + 0x10;
                }
            }
            h->nextAngleRate = h->nextAngleDefaultRate > 0 ? 0x300 : -0x300;
        }
    }

    if (Util_GetScreenPos(t->x, t->y, t->screen, 0x10) == 0)
    {
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = ((t->screen[0] << 16) & 0x01FF0000) | 0x60000000 | (u8)t->screen[1];
            spr->attr2 = t->anim.value | 0x3400;
        }
    }
}
/**
 * A right spring (map tile 0xF9, spring.txt "RIGHT"), Object_SpringDown turned by a quarter: it tests only the current Helirin, turns
 * it with Helirin_Bump(h, 0x40, 0x100), compares the Helirin's angle -/+ 0x4000 by the hit's
 * bits, starts its Effect_UpdateSpringSpark task at (x + 0x10, y + 8) and is drawn as an affine sprite.
 */
void Object_SpringRight(struct AnimObject *t)
{
    struct Helirin *h;
    struct Task *task;
    struct Sprite *spr;
    u32 hits;
    u16 angle;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gSpringAnim;
        t->anim.value = gSpringAnim[1];
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    h = gGameState.helirin;
    if (gGameState.paused == 0)
    {
        hits = Collision_TestShapes(h, t->x, t->y, gSpringShape);
        if (hits == 0)
        {
            Anim_Step(&t->anim);
        }
        else
        {
            Anim_Start(&t->anim, gSpringHitAnim);
            Helirin_Bump(h, 0x40, 0x100);
            m4aSongNumStart(0x36);
            if (hits & 0x15554)
                angle = h->angle - 0x4000;
            else if (hits & 0xAAAA)
                angle = h->angle + 0x4000;
            else
                goto draw;
            if (h->angleDefaultRate > 0 ? (s16)angle >= 0 : angle > 0x8000)
            {
                h->nextAngleDefaultRate = -h->angleDefaultRate;
                task = Task_Create(Effect_UpdateSpringSpark, 0x18);
                if (task != NULL)
                {
                    task->unk8 = t->x + 0x10;
                    task->unkA = t->y + 8;
                }
            }
            h->nextAngleRate = h->nextAngleDefaultRate > 0 ? 0x300 : -0x300;
        }
    }

draw:
    if (Util_GetScreenPos(t->x, t->y, t->screen, 0x10) == 0)
    {
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = ((t->screen[0] << 16) & 0x01FF0000) | 0x44000100 | (u8)t->screen[1];
            spr->attr2 = t->anim.value | 0x3400;
        }
    }
}
/**
 * A left spring (map tile 0xF9 flipped horizontally, spring.txt "LEFT"), Object_SpringRight turned the
 * other way: Helirin_Bump(h, 0xC0, 0x100), the angle conditions of
 * Object_SpringDown, its Effect_UpdateSpringSpark task at (x, y + 8), the sprite turned by half a turn (affine set 4).
 */
void Object_SpringLeft(struct AnimObject *t)
{
    struct Helirin *h;
    struct Task *task;
    struct Sprite *spr;
    u32 hits;
    u16 angle;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gSpringAnim;
        t->anim.value = gSpringAnim[1];
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    h = gGameState.helirin;
    if (gGameState.paused == 0)
    {
        hits = Collision_TestShapes(h, t->x, t->y, gSpringShape);
        if (hits == 0)
        {
            Anim_Step(&t->anim);
        }
        else
        {
            Anim_Start(&t->anim, gSpringHitAnim);
            Helirin_Bump(h, 0xC0, 0x100);
            m4aSongNumStart(0x36);
            if (hits & 0x15554)
                angle = h->angle - 0x4000;
            else if (hits & 0xAAAA)
                angle = h->angle + 0x4000;
            else
                goto draw;
            if (h->angleDefaultRate > 0 ? angle > 0x8000 : (s16)angle >= 0)
            {
                h->nextAngleDefaultRate = -h->angleDefaultRate;
                task = Task_Create(Effect_UpdateSpringSpark, 0x18);
                if (task != NULL)
                {
                    task->unk8 = t->x;
                    task->unkA = t->y + 8;
                }
            }
            h->nextAngleRate = h->nextAngleDefaultRate > 0 ? 0x300 : -0x300;
        }
    }

draw:
    if (Util_GetScreenPos(t->x, t->y, t->screen, 0x10) == 0)
    {
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = ((t->screen[0] << 16) & 0x01FF0000) | 0x4C000100 | (u8)t->screen[1];
            spr->attr2 = t->anim.value | 0x3400;
        }
    }
}
/**
 * A Roller (map code 0xF7, KuruTools), drawn like Object_RollerCatcher: while gGameState.paused is 0
 * it counts timer down and at 0 restarts it from period and starts an Object_RollerProjectile task
 * at its position with its direction (dir, affineSet) and speed.
 */
void Object_Roller(struct Task *task)
{
    struct RollerObject *t = (struct RollerObject *)task;
    struct RollerProjectile *n;
    struct Sprite *spr;

    switch ((s16)t->state)
    {
    case 0:
        t->affineSet = ((t->dir + 0x10) >> 5) & 7;
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.paused == 0)
        {
            if (t->timer != 0)
            {
                t->timer--;
            }
            else
            {
                t->timer = t->period;
                n = (struct RollerProjectile *)Task_Create((void (*)(struct Task *))Object_RollerProjectile, sizeof(*n));
                if (n != NULL)
                {
                    n->x = (u32)t->x << 16;
                    n->y = (u32)t->y << 16;
                    n->dir = t->dir;
                    n->speed = t->speed;
                    n->affineSet = t->affineSet;
                }
            }
        }
        if (Util_GetScreenPos(t->x, t->y, t->screen, 0x20) == 0)
        {
            if (t->affineSet & 1)
            {
                spr = Sprite_Alloc(5);
                if (spr != NULL)
                {
                    spr->attr01 = (t->affineSet << 25) | (((t->screen[0] - 0x20) << 16) & 0x1FF0000) | 0x80000300
                        | (u8)(t->screen[1] - 0x20);
                    spr->attr2 = 0x563C;
                }
            }
            else
            {
                spr = Sprite_Alloc(5);
                if (spr != NULL)
                {
                    spr->attr01 = (t->affineSet << 25) | (((t->screen[0] - 0x10) << 16) & 0x1FF0000) | 0x80000100
                        | (u8)(t->screen[1] - 0x10);
                    spr->attr2 = 0x563C;
                }
            }
        }
        break;
    }
}
/**
 * A Roller Catcher (map code 0xF6, KuruTools): it rounds its angle dir to one of 8 directions
 * (affineSet) and, while on screen (margin 0x20), draws one 32x32 affine sprite (Sprite_Alloc
 * list 5, tile 0x23C, palette 5) with affine set affineSet centred on (x, y), double-size for the
 * diagonals (odd affineSet).
 */
void Object_RollerCatcher(struct Task *task)
{
    struct RollerCatcherObject *t = (struct RollerCatcherObject *)task;
    struct Sprite *spr;

    switch ((s16)t->state)
    {
    case 0:
        t->affineSet = ((t->dir + 0x10) >> 5) & 7;
        t->state = 1;
        // fall through
    case 1:
        if (Util_GetScreenPos(t->x, t->y, t->screen, 0x20) == 0)
        {
            if (t->affineSet & 1)
            {
                spr = Sprite_Alloc(5);
                if (spr != NULL)
                {
                    spr->attr01 = (t->affineSet << 25) | (((t->screen[0] - 0x20) << 16) & 0x1FF0000) | 0x80000300
                        | (u8)(t->screen[1] - 0x20);
                    spr->attr2 = 0x563C;
                }
            }
            else
            {
                spr = Sprite_Alloc(5);
                if (spr != NULL)
                {
                    spr->attr01 = (t->affineSet << 25) | (((t->screen[0] - 0x10) << 16) & 0x1FF0000) | 0x80000100
                        | (u8)(t->screen[1] - 0x10);
                    spr->attr2 = 0x563C;
                }
            }
        }
        break;
    }
}
/**
 * A Roller's projectile (Object_Roller): it moves in direction dir at speed speed (song 0x49 when it
 * starts near the screen, Util_IsOnScreen), playing gRollerProjectileAnim, until the map ahead of it
 * (Collision_TestWallPixel at its position + (aheadX, aheadY)) is solid, when it frees itself. A Helirin
 * it hits is pushed as by Object_Piston. Frozen while gGameState.paused is set.
 */
void Object_RollerProjectile(struct RollerProjectile *t)
{
    struct Helirin *h;
    struct Sprite *spr;
    u32 hits;
    s32 angle;
    s8 dir;

    switch (t->state)
    {
    case 0:
        t->vx = gSineTable[t->dir] * t->speed;
        t->vy = -gCosineTable[t->dir] * t->speed;
        t->aheadX = gSineTable[t->dir] * 15 / 256;
        t->aheadY = -gCosineTable[t->dir] * 15 / 256;
        t->anim.timer = 1;
        t->anim.next = gRollerProjectileAnim;
        t->anim.value = gRollerProjectileAnim[1];
        if (Util_IsOnScreen(POS_PIXEL(t->x), POS_PIXEL(t->y), 0x40) == 0)
            m4aSongNumStart(0x49);
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    if (gGameState.paused == 0)
    {
        t->x += t->vx;
        t->y += t->vy;
        if (Collision_TestWallPixel(&gWallMap, t->aheadX + POS_PIXEL(t->x), t->aheadY + POS_PIXEL(t->y)) != 0)
        {
            Task_Destroy((struct Task *)t);
            return;
        }
        Anim_Step(&t->anim);
        h = gGameState.helirin;
        hits = Collision_TestShapes(h, POS_PIXEL(t->x), POS_PIXEL(t->y), gRollerProjectileShape);
        if (hits != 0)
        {
            angle = h->angle / 256;
            dir = Util_GetDirection(POS_PIXEL(h->x) - POS_PIXEL(t->x), POS_PIXEL(h->y) - POS_PIXEL(t->y));
            h->objectCollisionMask |= hits;
            if (hits & 7)
                Helirin_Bump(h, dir, 0x300);
            if (hits & 0x15554)
                h->nextAngleRate = (s8)(dir - angle) >= 0 ? 0x400 : -0x400;
            else if (hits & 0xAAAA)
                h->nextAngleRate = (s8)(dir - angle) < 0 ? 0x400 : -0x400;
        }
    }

    if (Util_GetScreenPos(POS_PIXEL(t->x), POS_PIXEL(t->y), t->screen, 0x20) == 0)
    {
        if (t->affineSet == 0)
        {
            spr = Sprite_Alloc(7);
            if (spr != NULL)
            {
                spr->attr01 = 0x80000000 | (((t->screen[0] - 16) << 16) & 0x1FF0000)
                          | (u8)(t->screen[1] - 16);
                spr->attr2 = t->anim.value | 0x4400;
            }
        }
        else
        {
            spr = Sprite_Alloc(7);
            if (spr != NULL)
            {
                spr->attr01 = ((u32)t->affineSet << 25) | (((t->screen[0] - 16) << 16) & 0x1FF0000)
                          | 0x80000100 | (u8)(t->screen[1] - 16);
                spr->attr2 = t->anim.value | 0x4400;
            }
        }
    }
}
/**
 * A Piston (map code 0xF5, KuruTools) swinging along one of four directions (see struct PistonObject) that pushes
 * the current Helirin away (Helirin_Bump with speed 0x300 when one of its first three points
 * hits) and sets the Helirin's nextAngleRate to +/-0x400 by the side it was hit on and the direction
 * towards it; the hit points are or'd into its objectCollisionMask. It is frozen while gGameState.paused is
 * set, and drawn when on screen.
 */
void Object_Piston(struct PistonObject *t)
{
    struct Helirin *h;
    struct Sprite *spr;
    u32 hits;
    s32 angle;
    s8 dir;

    switch (t->state)
    {
    case 0:
        t->x0 = t->x;
        t->y0 = t->y;
        t->dirEighths = ((t->dir + 0x10) >> 5) & 7;
        t->attr2 = (t->dirEighths & 2) ? 0x45EC : 0x45DC;
        t->phase = 0;
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    if (gGameState.paused == 0)
    {
        switch (t->dirEighths)
        {
        case 0:
            t->y = t->y0 + (gCosineTable[(u8)(t->phase / 256)] << 13);
            break;
        case 2:
            t->x = t->x0 - (gCosineTable[(u8)(t->phase / 256)] << 13);
            break;
        case 4:
            t->y = t->y0 - (gCosineTable[(u8)(t->phase / 256)] << 13);
            break;
        case 6:
            t->x = t->x0 + (gCosineTable[(u8)(t->phase / 256)] << 13);
            break;
        }
        if (t->waitTimer > 0)
            t->waitTimer--;
        else
            t->phase += t->phaseStep;
        if (t->phase > 0xFFFF)
        {
            t->phase = 0;
            t->waitTimer = t->waitPeriod;
        }
        h = gGameState.helirin;
        hits = Collision_TestShapes(h, POS_PIXEL(t->x), POS_PIXEL(t->y), gPistonShape);
        if (hits != 0)
        {
            angle = h->angle / 256;
            dir = Util_GetDirection(POS_PIXEL(h->x) - POS_PIXEL(t->x), POS_PIXEL(h->y) - POS_PIXEL(t->y));
            h->objectCollisionMask |= hits;
            if (hits & 7)
                Helirin_Bump(h, dir, 0x300);
            if (hits & 0x15554)
                h->nextAngleRate = (s8)(dir - angle) >= 0 ? 0x400 : -0x400;
            else if (hits & 0xAAAA)
                h->nextAngleRate = (s8)(dir - angle) < 0 ? 0x400 : -0x400;
        }
    }

    if (Util_GetScreenPos(POS_PIXEL(t->x), POS_PIXEL(t->y), t->screen, 0x20) == 0)
    {
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = 0x80000000 | (((t->screen[0] - 16) << 16) & 0x1FF0000)
                      | (u8)(t->screen[1] - 16);
            spr->attr2 = t->attr2;
        }
    }
}

/**
 * Moves the byte angle *a* towards *b* by at most *step* (Util_StepTowardsS8), kept inside the arc
 * from *lo* to *hi*: relative to the arc's middle, a *b* outside it is replaced by the nearer
 * end (Util_StepTowards).
 */
s8 Object_TurnInArc(u8 a, u8 b, u8 step, u8 lo, u8 hi)
{
    s32 range = hi - lo;
    u8 mid;
    s8 rb, rlo, rhi;

    if (range < 0)
        range = -range;
    mid = lo + range / 2;
    rb = b - mid;
    rlo = lo - mid;
    rhi = hi - mid;
    if (rlo > rb)
        a = mid + Util_StepTowards((s8)(a - mid), rlo, step);
    else if (rhi < rb)
        a = mid + Util_StepTowards((s8)(a - mid), rhi, step);
    else
        a = Util_StepTowardsS8(a, b, step);
    return a;
}
// The shot of Object_Shooter (0x30 bytes); x/y and the velocity are 16.16 fixed point.
struct ShooterShot {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
    /*0x10*/ s32 vx;    // gSineTable[dir] * speed, added to x every frame
    /*0x14*/ s32 vy;    // -gCosineTable[dir] * speed, added to y
    /*0x18*/ s16 screen[2]; // from Util_GetScreenPos
    /*0x1C*/ u8 unk1C;
    /*0x1D*/ u8 dir;    // a direction into gSineTable/gCosineTable (Object_Shooter's dir)
    /*0x1E*/ s16 speed; // 0x140
    /*0x20*/ struct Anim anim; // gShooterShotAnim
    /*0x28*/ s16 timer; // frames (from 0x1E) before hitting the map leaves a Effect_UpdateHitSpark task
    /*0x2A*/ s16 aheadX; // 1/32 of the x step: the map is tested that far ahead of x
    /*0x2C*/ s16 aheadY; // and of y
    /*0x2E*/ u8 inMap;  // bit 0: in the map at the last test
    /*0x2F*/ u8 unk2F;
};
void Object_ShooterShot(struct ShooterShot *task);
extern const u16 gShooterAnim[];
extern const u16 gShooterFireAnim[];

/**
 * A Shooter (map code 0xF4, KuruTools): turns its direction towards the current Helirin by 1 a frame (Util_StepTowardsS8, or
 * within the arc arcMin..arcMax, Object_TurnInArc) while gGameState.paused is 0. Every period frames
 * it plays gShooterFireAnim, and on its frame 0x29C (song 0x48 near the screen) it fires a
 * Object_ShooterShot shot from 14 pixels along its direction, when within 0x20 pixels of the
 * screen. On screen it draws the frame rotated (Sprite_SetAffineRotation), 64x64 or 32x32 by the
 * affine set's bit 0.
 */
void Object_Shooter(struct ShooterObject *t)
{
    struct Helirin *h;
    struct Sprite *spr;
    struct ShooterShot *shot;
    s32 aff;
    u8 dir;

    switch (t->state)
    {
    case 0:
        if ((s8)t->arcMin <= (s8)t->arcMax)
            t->dir = ((s8)t->arcMin + (s8)t->arcMax) / 2;
        else
            t->dir = ((s8)t->arcMin + (s8)t->arcMax) / 2 - 0x80;
        t->state = 1;
        t->anim.timer = 1;
        t->anim.next = gShooterAnim;
        t->anim.value = gShooterAnim[1];
        // fall through
    case 1:
        break;
    default:
        return;
    }

    if (gGameState.paused == 0)
    {
        h = gGameState.helirin;
        dir = Util_GetDirection(POS_PIXEL(h->x) - POS_PIXEL(t->x), POS_PIXEL(h->y) - POS_PIXEL(t->y));
        if (*(u16 *)&t->arcMin == 0) // arcMin and arcMax both 0: no arc
            t->dir = Util_StepTowardsS8(t->dir, dir, 1);
        else
            t->dir = Object_TurnInArc(t->dir, dir, 1, t->arcMin, t->arcMax);
        if ((s16)t->timer == 0)
        {
            if ((s16)t->anim.timer == 0)
            {
                t->anim.timer = 1;
                t->anim.next = gShooterFireAnim;
                t->anim.value = gShooterFireAnim[1];
            }
        }
        else
        {
            t->timer--;
        }
        if ((Anim_Step(&t->anim) & 1) && t->anim.value == 0x29C)
        {
            if (Util_IsOnScreen(POS_PIXEL(t->x), POS_PIXEL(t->y), 0x40) == 0)
                m4aSongNumStart(0x48);
            if (Util_IsOnScreen(POS_PIXEL(t->x), POS_PIXEL(t->y), 0x20) == 0)
            {
                shot = (struct ShooterShot *)Task_Create((void (*)(struct Task *))Object_ShooterShot, 0x30);
                if (shot != NULL)
                {
                    shot->dir = t->dir;
                    shot->x = t->x + gSineTable[(u8)t->dir] * 0xE00;
                    shot->y = t->y - gCosineTable[(u8)t->dir] * 0xE00;
                }
            }
            t->timer = t->period;
        }
    }

    if (Util_GetScreenPos(POS_PIXEL(t->x), POS_PIXEL(t->y), t->screen, 0x20) != 0)
        return;
    aff = Sprite_AllocAffine();
    if (aff >= 0)
    {
        t->affineSet = aff;
        Sprite_SetAffineRotation(aff, t->dir);
    }
    else
    {
        t->affineSet = ((t->dir + 0x10) >> 5) & 7;
    }
    if (t->affineSet & 1)
    {
        spr = Sprite_Alloc(5);
        if (spr == NULL)
            return;
        spr->attr01 = (t->affineSet << 25) | ((((t->screen[0] - 32) << 16) & 0x1FF0000) | 0x80000300);
        spr->attr2 = t->anim.value | 0x5400;
        *(u8 *)&spr->attr01 = t->screen[1] - 32;
    }
    else
    {
        spr = Sprite_Alloc(5);
        if (spr == NULL)
            return;
        spr->attr01 = (t->affineSet << 25) | ((((t->screen[0] - 16) << 16) & 0x1FF0000) | 0x80000100);
        spr->attr2 = t->anim.value | 0x5400;
        *(u8 *)&spr->attr01 = t->screen[1] - 16;
    }
}
extern const u16 gShooterShotAnim[];
extern const struct CollisionShape gShooterShotShape[];

/**
 * The shot of Object_Shooter: flies along dir at 1.25 pixels a frame (gSineTable * 0x140) playing
 * gShooterShotAnim while gGameState.paused is 0. It passes through the map while it started in
 * it (bit 0 of inMap, cleared once outside) or for its first 0x1E frames; after that,
 * hitting the map (tested 1/32 of a step ahead) leaves a Effect_UpdateHitSpark task there and frees
 * it. Hitting the current Helirin (gShooterShotShape) pushes it like Object_RollerProjectile's shot and
 * frees it. While on screen it draws the frame as a 16x16 sprite.
 */
void Object_ShooterShot(struct ShooterShot *t)
{
    struct Helirin *h;
    struct Sprite *spr;
    struct AnimObject *mark;
    u32 hits;
    s32 angle;
    s8 dir;
    s32 speed;

    switch (t->state)
    {
    case 0:
        t->speed = speed = 0x140;
        t->vx = gSineTable[t->dir] * speed;
        t->vy = -gCosineTable[t->dir] * speed;
        t->aheadX = gSineTable[t->dir] / 32;
        t->aheadY = -gCosineTable[t->dir] / 32;
        t->inMap |= 1;
        t->timer = 0x1E;
        t->anim.timer = 1;
        t->anim.next = gShooterShotAnim;
        t->anim.value = gShooterShotAnim[1];
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    if (gGameState.paused == 0)
    {
        t->x += t->vx;
        t->y += t->vy;
        if (Collision_TestWallPixel(&gWallMap, POS_PIXEL(t->x) + t->aheadX, POS_PIXEL(t->y) + t->aheadY) != 0)
        {
            if (!(t->inMap & 1) && t->timer == 0)
            {
                mark = (struct AnimObject *)Task_Create(Effect_UpdateHitSpark, 0x18);
                if (mark != NULL)
                {
                    mark->x = POS_PIXEL(t->x) + t->aheadX;
                    mark->y = POS_PIXEL(t->y) + t->aheadY;
                }
                goto free;
            }
        }
        else
        {
            t->inMap &= ~1;
        }
        if (t->timer > 0)
            t->timer--;
        Anim_Step(&t->anim);
        h = gGameState.helirin;
        hits = Collision_TestShapes(h, POS_PIXEL(t->x), POS_PIXEL(t->y), gShooterShotShape);
        if (hits != 0)
        {
            angle = h->angle / 256;
            dir = Util_GetDirection(POS_PIXEL(h->x) - POS_PIXEL(t->x), POS_PIXEL(h->y) - POS_PIXEL(t->y));
            h->objectCollisionMask |= hits;
            if (hits & 7)
                Helirin_Bump(h, dir, 0x300);
            if (hits & 0x15554)
                h->nextAngleRate = (s8)(dir - angle) >= 0 ? 0x400 : -0x400;
            else if (hits & 0xAAAA)
                h->nextAngleRate = (s8)(dir - angle) < 0 ? 0x400 : -0x400;
            goto free;
        }
    }

    if (Util_GetScreenPos(POS_PIXEL(t->x), POS_PIXEL(t->y), t->screen, 0x10) == 0)
    {
        spr = Sprite_Alloc(5);
        if (spr != NULL)
        {
            spr->attr01 = (((t->screen[0] - 8) << 16) & 0x1FF0000) | 0x40000000;
            spr->attr2 = t->anim.value | 0x4400;
            *(u8 *)&spr->attr01 = t->screen[1] - 8;
        }
    }
    return;

free:
    Task_Destroy((struct Task *)t);
}
