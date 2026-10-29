// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// helirin: the Helirin: setup, its frame (physics), collision response, drawing.
// 080142DC-08015890, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "sprite.h"
#include "text.h"
#include "task.h"
#include "util.h"
#include "makeup.h"
#include "collision.h"
#include "helirin.h"
#include "effects.h"

extern const u16 gHelirinHealColorAnim[];
extern const u16 *const gHelirinColorAnims[];
extern const u16 gHelirinHurtColorAnim[];
extern const u16 gHelirinZoneColorAnim[];
struct DemoKeyOffset {
    u32 offset; // into gDemoKeys
    u32 unk4;
};
extern const struct DemoKeyOffset gDemoKeyOffsets[][4];
extern const u8 gDemoKeys[];
extern u8 gKeyRecording[];
// The tasks Helirin_Update starts with Helirin_UpdatePiece (0x24 bytes), 16 per Helirin (unless
// gGameState.mode is 4); x/y and the velocity are 16.16 fixed point.
struct HelirinPiece {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
    /*0x10*/ s32 vx;
    /*0x14*/ s32 vy;
    /*0x18*/ s16 screen[2];
    /*0x1C*/ u16 tile;     // the piece (0-15), then its sprite's tile/palette halfword
    /*0x1E*/ u8 affineSet; // dir rounded to eighths: the sprite's affine set
    /*0x1F*/ u8 dir;
    /*0x20*/ struct Helirin *helirin;
};
extern const u8 *const gHelirinPieceTiles[8];
// 0x80-byte entries by Helirin type whose first word is an offset into gHelirinGfx
// (Helirin_Draw: + 0x20 * colorValue is the Helirin's OBJ palette).
struct HelirinTypeGfx {
    /*0x00*/ u32 offset;
    /*0x04*/ u8 unk4[0x7C];
};
extern const struct HelirinTypeGfx gHelirinTypeGfx[];
extern const u8 gVsHelirinPalettes[][0x20];
void Helirin_ResetToStart(struct Helirin *h);
void Helirin_UpdatePiece(struct Task *task);
extern const s16 gKeyDirections[16];
extern const u8 gTextPlayback[];
extern const u8 gTextDataEmpty[];
extern const u8 gTextRecArea[];
extern const u8 gTextEmpty[];

/**
 * Sets the Helirin's color to *n* and points colorAnim at gHelirinColorAnims[n] (gHelirinHealColorAnim
 * while flags bit 18 is set); resets colorTimer to 1 and copies colorAnim[1] to colorValue.
 */
void Helirin_SetColor(struct Helirin *h, s32 n)
{
    h->color = n;
    if (h->flags & 0x40000)
    {
        h->colorTimer = 1;
        h->colorAnim = gHelirinHealColorAnim;
    }
    else
    {
        h->colorTimer = 1;
        h->colorAnim = gHelirinColorAnims[h->color];
    }
    h->colorValue = h->colorAnim[1];
}
// gHelirinSizes: 12-byte entries by Helirin size (0: the longest, 64 pixels, 17 points; 14: the
// shortest, 8 pixels, 3 points), copied into the Helirin by Helirin_SetSize.
struct HelirinSize {
    /*0x00*/ const s8 *pointDistances; // each of these is copied to the Helirin's field
    /*0x04*/ s8 numPoints;
    /*0x05*/ s8 length;
    /*0x06*/ s8 spriteDx;  // 0: no OBJ tiles to copy
    /*0x07*/ u8 spriteDy;
    /*0x08*/ u32 spriteAttr;
};
extern const struct HelirinSize gHelirinSizes[];

/**
 * Sets Helirin *h* up for its size (gHelirinSizes) and its OBJ tiles at
 * 0x06010000 + 32 * tile: clears 0x400 bytes, then (unless the entry's spriteDx is 0) copies
 * the tiles of gHelirinSizeGfx[type][size]: sizes 0-7 eight rows of 0x40 bytes, 8-11 four,
 * larger ones 0x80 bytes in one, rows 0x80 bytes apart from tile tile + 1 (larger: tile);
 * then its palette (Helirin_SetColor with color).
 */
void Helirin_SetSize(struct Helirin *h)
{
    const struct HelirinSize *e = &gHelirinSizes[(s8)h->size];
    u8 *vram = (u8 *)0x06010000;
    s32 i;

    h->pointDistances = e->pointDistances;
    h->numPoints = e->numPoints;
    h->length = e->length;
    h->spriteDx = e->spriteDx;
    h->spriteDy = e->spriteDy;
    h->spriteAttr = e->spriteAttr;
    Main_QueueFill(0, vram + h->tile * 32, 0x400);
    if (e->spriteDx == 0)
        return;
    if ((s8)h->size <= 7)
    {
        for (i = 0; i < 8; i++)
            Main_QueueCopy(gHelirinGfx + gHelirinSizeGfx[(s8)h->type][(s8)h->size].offset + i * 0x40,
                         vram + (h->tile + 1 + i * 4) * 32, 0x40);
    }
    else if ((s8)h->size <= 11)
    {
        for (i = 0; i < 4; i++)
            Main_QueueCopy(gHelirinGfx + gHelirinSizeGfx[(s8)h->type][(s8)h->size].offset + i * 0x40,
                         vram + (h->tile + 1 + i * 4) * 32, 0x40);
    }
    else
    {
        Main_QueueCopy(gHelirinGfx + gHelirinSizeGfx[(s8)h->type][(s8)h->size].offset,
                     vram + h->tile * 32, 0x80);
    }
    Helirin_SetColor(h, h->color);
}
/**
 * Sets up Helirin *h* for player *n* at the start of the level, as Helirin_ResetToStart does, and
 * also: type/color from gGameState.saveFile (type/color) for this console's player, cleared for
 * the others; bumpCount, regenTimer, flags and disableFlags cleared, then flags bits 17/16 and disableFlags
 * bit 1 from gGameState.flags bits 2/3/0; per-player tile/palette; then Lives_Init and
 * Helirin_SetSize.
 */
void Helirin_Init(struct Helirin *h, s32 n)
{
    struct Camera *cam = &gCamera;
    struct SaveFile *save = gGameState.saveFile;
    u32 level;

    h->player = n;
    if (gLinkPlayer == n)
    {
        h->size = 0;
        h->type = save->type;
        h->color = save->color;
    }
    else
    {
        h->size = 0;
        h->type = 0;
        h->color = 0;
    }
    h->xb = 0;
    h->yb = 0;
    h->angle = 0;
    h->frame = 0;
    if (cam->ccwStarts & (1 << gGameState.area))
        h->angleRate = h->angleDefaultRate = -0xB6;
    else
        h->angleRate = h->angleDefaultRate = 0xB6;
    h->x = cam->startX[gGameState.area] << 16;
    h->y = cam->startY[gGameState.area] << 16;
    h->angleRateStep = 0x5B;
    h->invulnerability = 0;
    h->timer = 0;
    h->keys[0] = 0;
    h->keys[1] = 0;
    h->keys[2] = 0;
    h->keys[3] = 0;
    h->keys[4] = 0;
    h->flags = 0;
    h->disableFlags = 0;
    h->bumpCount = 0;
    h->regenTimer = 0;
    h->recordKeys = gKeyRecording;
    if (gGameState.world == 0)
        level = gGameState.level;
    else
        level = gGameState.world * 3 + gGameState.level + 2;
    h->demoKeys = gDemoKeys + gDemoKeyOffsets[level][gGameState.area].offset;
    if (gGameState.flags & 4)
        h->flags |= 0x20000;
    if (gGameState.flags & GAME_FLAG_DEMO)
        h->flags |= 0x10000;
    if (gGameState.flags & 1)
        h->disableFlags |= 2;
    Lives_Init(h);
    switch (n)
    {
    case 0:
        h->tile = 0x30D;
        h->palette = 0;
        break;
    case 1:
        h->tile = 0x32D;
        h->palette = 0x1000;
        break;
    case 2:
        h->tile = 0x34D;
        h->palette = 0x9000;
        break;
    case 3:
        h->tile = 0x36D;
        h->palette = 0xA000;
        break;
    }
    Helirin_SetSize(h);
}
/**
 * Puts Helirin *h* at the start of the level: at (gCamera.startX[n], gCamera.startY[n]) for
 * n = gGameState.area, still, spinning at 0xB6 (the other way if bit n of gCamera.ccwStarts is
 * set), and points demoKeys at the level's entry n of gDemoKeyOffsets (training levels are
 * world 0, the others follow from world * 3 + level + 2).
 */
void Helirin_ResetToStart(struct Helirin *h)
{
    struct Camera *cam = &gCamera;
    u32 level;

    h->xb = 0;
    h->yb = 0;
    h->angle = 0;
    h->frame = 0;
    if (cam->ccwStarts & (1 << gGameState.area))
        h->angleRate = h->angleDefaultRate = -0xB6;
    else
        h->angleRate = h->angleDefaultRate = 0xB6;
    h->x = cam->startX[gGameState.area] << 16;
    h->y = cam->startY[gGameState.area] << 16;
    h->angleRateStep = 0x5B;
    h->invulnerability = 0;
    h->timer = 0;
    h->keys[0] = 0;
    h->keys[1] = 0;
    h->keys[2] = 0;
    h->keys[3] = 0;
    h->keys[4] = 0;
    if (gGameState.world == 0)
        level = gGameState.level;
    else
        level = gGameState.world * 3 + gGameState.level + 2;
    h->demoKeys = gDemoKeys + gDemoKeyOffsets[level][gGameState.area].offset;
}
/**
 * A Helirin's frame (KururinTAS's "global description of physics", Misc/Game_analysis/
 * kkk.txt): its keys (gMain.heldKeys, or replayed from demoKeys with flags bit 16; recorded to
 * recordKeys with bit 17) into keys (held, previous, new, repeated, the repeat timer); flags
 * bit 6 (a crash) starts the Effect_UpdateCrash task and the sixteen Helirin_UpdatePiece pieces; then
 * the input speed (xs/ys, speedNorm 0x180, 0x240 with A or B, 0x300 with both, along
 * gKeyDirections's direction), the bump speeds decaying to 3/4, the angle (Util_StepTowards
 * towards angleDefaultRate) and the tile under its centre (KuruTools PhysicalMapLogic.cs):
 * the healing zones (0xEA-0xEF) and the start zones (0xF2, 0xFB-0xFD), where Select changes
 * the size in some modes and the lives are refilled once, and the goal (0xF3, 0xFE, 0xFF);
 * then the collision test (Collision_UpdatePoints).
 * While paused (gGameState.paused) it only draws the recording's two names.
 */
void Helirin_Update(struct Helirin *h)
{
    struct Map *map;
    struct AnimObject *crash;
    struct HelirinPiece *piece;
    u16 keys;
    u16 prev;
    u32 tile;
    u32 i;

    if (gGameState.paused != 0)
    {
        if (gLinkPlayer != h->player)
            return;
        if (!(h->flags & 0x20000))
            return;
        if (h->recordKeys == NULL)
            return;
        Text_DrawSmallHex(0x34, 0x58, (u32)gKeyRecording, 8, 14);
        Text_DrawSmallHex(0x7C, 0x58, (u32)h->recordKeys, 8, 14);
        return;
    }

    h->frame++;
    if ((h->disableFlags & 2) || (h->flags & 0x40000000))
    {
        h->keys[0] = 0;
        h->keys[1] = 0;
        h->keys[2] = 0;
        h->keys[3] = 0;
    }
    else
    {
        if (h->flags & 0x10000)
        {
            if (h->demoKeys != NULL && *h->demoKeys != 0xFF)
            {
                keys = *h->demoKeys++;
            }
            else if (h->flags & 0x8000000)
            {
                Helirin_ResetToStart(h);
                keys = *h->demoKeys++;
            }
            else
            {
                keys = 0;
                Text_DrawSmall(0x58, 0x3C, gTextPlayback, 14);
                Text_DrawSmall(0x54, 0x44, gTextDataEmpty, 14);
            }
        }
        else
        {
            keys = gMain.heldKeys;
        }
        prev = h->keys[0];
        h->keys[0] = keys;
        h->keys[1] = prev;
        h->keys[2] = keys & ~prev;
        h->keys[3] = keys & ~prev;
        if (keys == 0 || keys != prev)
        {
            h->keys[4] = 20;
        }
        else if (--h->keys[4] == 0)
        {
            h->keys[3] = keys;
            h->keys[4] = 4;
        }

        if ((h->flags & 0x20000) && h->recordKeys != NULL)
        {
            if (h->recordKeys == &gKeyRecording[0x1FFF])
            {
                // the recording is full
                Text_DrawSmall(0x58, 0x3C, gTextRecArea, 14);
                Text_DrawSmall(0x64, 0x44, gTextEmpty, 14);
            }
            else
            {
                switch (keys & 3)
                {
                case 1:
                case 2:
                    *h->recordKeys++ = (keys & 0xF0) | 1;
                    break;
                case 3:
                    *h->recordKeys++ = (keys & 0xF0) | 3;
                    break;
                default:
                    *h->recordKeys++ = keys & 0xF0;
                    break;
                }
            }
            *h->recordKeys = 0xFF;
        }
    }

    if (h->flags & 0x40)
    {
        h->flags &= ~0x800;
        m4aMPlayStop(&gMPlayInfo_SE2);
        if ((s8)gGameState.mode != 4)
            m4aSongNumStart(0x38);
        else
            m4aSongNumStart(0x3A);
        crash = (struct AnimObject *)Task_Create(Effect_UpdateCrash, sizeof(*crash));
        if (crash != NULL)
        {
            crash->x = POS_PIXEL(h->x);
            crash->y = POS_PIXEL(h->y);
        }
        if ((s8)gGameState.mode != 4)
        {
            for (i = 0; i <= 15; i++)
            {
                piece = (struct HelirinPiece *)Task_Create(Helirin_UpdatePiece, 0x24);
                if (piece != NULL)
                {
                    piece->helirin = h;
                    piece->tile = i;
                }
            }
        }
    }
    h->flags &= 0xDB57FCBA;
    if (h->flags & 0x10000000)
        h->flags = (h->flags & ~0x10000000) | 0x20000000;
    if (h->flags & 0x20)
        return;
    if (h->invulnerability != 0 && --h->invulnerability != 0)
        h->flags |= 0x100;

    // the input speed
    h->inputDir = gKeyDirections[(h->keys[0] & 0xF0) >> 4];
    if (h->inputDir >= 0)
    {
        switch (h->keys[0] & 3)
        {
        case 1:
        case 2:
            m4aSongNumStartOrChange(0x30);
            h->flags |= 0x1000;
            h->speedNorm = 0x240;
            break;
        case 3:
            m4aSongNumStartOrChange(0x32);
            h->flags |= 0x2000;
            h->speedNorm = 0x300;
            break;
        default:
            if (h->flags & 0x1000)
                m4aSongNumStart(0x31);
            else if (h->flags & 0x2000)
                m4aSongNumStart(0x33);
            h->flags &= ~0x3000;
            h->speedNorm = 0x180;
            break;
        }
        h->xs = gSineTable[(u8)h->inputDir] * h->speedNorm;
        h->x += h->xs;
        h->ys = -gCosineTable[(u8)h->inputDir] * h->speedNorm;
        h->y += h->ys;
    }
    else
    {
        if (h->flags & 0x1000)
            m4aSongNumStart(0x31);
        else if (h->flags & 0x2000)
            m4aSongNumStart(0x33);
        h->flags &= ~0x3000;
        h->speedNorm = 0;
        h->xs = 0;
        h->ys = 0;
    }

    // R and L
    switch (h->keys[0] & 3)
    {
    case 1:
    case 2:
        if (h->keys[2] & 0x100)
        {
            h->flags |= 0x4000000;
            m4aSongNumStart(0x43);
        }
        else if (h->keys[2] & 0x200)
        {
            h->flags |= 0x4000000;
            m4aSongNumStart(0x46);
        }
        break;
    case 3:
        if (h->keys[2] & 0x100)
        {
            h->flags |= 0x4000000;
            m4aSongNumStart(0x45);
        }
        else if (h->keys[2] & 0x200)
        {
            h->flags |= 0x4000000;
            m4aSongNumStart(0x44);
        }
        break;
    default:
        if (h->keys[2] & 0x100)
        {
            h->flags |= 0x4000000;
            m4aSongNumStart(0x41);
        }
        else if (h->keys[2] & 0x200)
        {
            h->flags |= 0x4000000;
            m4aSongNumStart(0x42);
        }
        break;
    }

    // the bump speeds decay to 3/4 (rounded towards 0)
    h->xb = h->xb * 192 / 256;
    h->yb = h->yb * 192 / 256;
    h->x += h->xb;
    h->y += h->yb;
    h->angleRate = Util_StepTowards(h->angleRate, h->angleDefaultRate, h->angleRateStep);
    h->angle += h->angleRate;

    map = &gWallMap;
    tile = map->tiles[(POS_PIXEL(h->y) / 8 % map->height) * map->width + POS_PIXEL(h->x) / 8 % map->width]
         & 0x3FF;
    switch (tile)
    {
    case 0xEA:
    case 0xED:
        if (!(h->flags & 0x100000))
            h->flags |= 0x180000;
        goto healingZone;
    case 0xEB:
    case 0xEE:
        if (!(h->flags & 0x400000))
            h->flags |= 0x600000;
        goto healingZone;
    case 0xEC:
    case 0xEF:
        if (!(h->flags & 0x1000000))
            h->flags |= 0x1800000;
    healingZone:
        if (h->keys[3] & SELECT_BUTTON)
        {
            switch ((s8)gGameState.mode)
            {
            case 5:
                if (gGameState.saveFile->flags & 0x10)
                    h->size = (s8)h->size > 10 ? 5 : h->size + 3;
                else
                    h->size = (s8)h->size > 3 ? 0 : h->size + 2;
                m4aSongNumStart(0x21);
                h->flags |= 0x4000000;
                Helirin_SetSize(h);
                break;
            case 6:
                m4aSongNumStart(0x21);
                h->flags |= 0x4000000;
                h->size = (s8)h->size > 13 ? 0 : h->size + 1;
                Helirin_SetSize(h);
                break;
            }
        }
        goto zone;
    case 0xF2:
        if (Collision_GetTilePixel(map, POS_PIXEL(h->x), POS_PIXEL(h->y)) == 0)
            goto other;
        // fall through
    case 0xFB:
    case 0xFC:
    case 0xFD:
        h->progress = 0;
        if (h->keys[3] & SELECT_BUTTON)
        {
            switch ((s8)gGameState.mode)
            {
            case 5:
                if (gGameState.saveFile->flags & 0x10)
                    goto plus3;
                // fall through
            case 1:
                m4aSongNumStart(0x21);
                h->flags |= 0x4000000;
                h->size = (s8)h->size > 3 ? 0 : h->size + 2;
                Helirin_SetSize(h);
                break;
            case 2:
            plus3:
                m4aSongNumStart(0x21);
                h->flags |= 0x4000000;
                h->size = (s8)h->size > 10 ? 5 : h->size + 3;
                Helirin_SetSize(h);
                break;
            case 3:
            case 6:
                m4aSongNumStart(0x21);
                h->flags |= 0x4000000;
                h->size = (s8)h->size > 13 ? 0 : h->size + 1;
                Helirin_SetSize(h);
                break;
            }
        }
    zone:
        if (!(h->flags & 0x40000))
        {
            m4aSongNumStart(0x37);
            h->lives = h->maxLives;
            h->flags |= 0x200;
        }
        h->flags |= 0x40000;
        break;
    case 0xF3:
        if (Collision_GetTilePixel(map, POS_PIXEL(h->x), POS_PIXEL(h->y)) == 0)
            goto other;
        // fall through
    case 0xFE:
    case 0xFF:
        h->flags &= ~0x40000;
        if (!(h->flags & 8))
            h->flags |= 0xC;
        h->progress = 0x21;
        break;
    default:
    other:
        h->flags &= ~0x40000;
        if (tile - 0xA0 <= 0x1F)
            h->progress = tile + 0x61;
        if (!(h->flags & 2))
            h->flags |= 3;
        if ((s8)gGameState.mode == 5 && (s8)h->lives < h->maxLives)
        {
            h->regenTimer += 0x44;
            if ((s16)h->regenTimer > 0xFFF)
            {
                m4aSongNumStart(0x37);
                h->lives++;
                h->regenTimer = 0;
                h->flags |= 0x200;
            }
        }
        break;
    }

    Collision_UpdatePoints(h);
    h->collisionMask = 0;
    h->objectCollisionMask = 0;
    h->nextAngleRate = h->angleRate;
    h->nextAngleDefaultRate = h->angleDefaultRate;
}
/**
 * The Helirin's collision response, once per frame. Unless gGameState.paused is set or
 * flags bit 5 holds it, it tests points[] against the map into collisionMask. A hit (with
 * objectCollisionMask) plays a sound, starts up to two hit-effect tasks at the hit points, undoes the
 * last move when moving and, when not invulnerable (and flags & 0x40008 clear), costs a
 * life, 20 frames of invulnerability, a bump and (unless gGameState.mode is 4) 0xB4 frames
 * of timer. collisionMask then pushes it away (Helirin_Bump) and reverses its spin
 * (nextAngleRate, nextAngleDefaultRate become angleRate, angleDefaultRate). Finally the colour animation steps.
 */
void Helirin_UpdateCollision(struct Helirin *h)
{
    // colorAnim/colorTimer/colorValue have the layout of a struct Anim
    struct Anim *anim = (struct Anim *)&h->colorAnim;
    struct AnimObject *spark;
    u32 mask;
    u32 bit;
    u32 a;
    u32 b;
    s32 dir;
    s32 n;
    s32 i;

    if (gGameState.paused == 0)
    {
        if (h->flags & 0x20)
        {
            if (Anim_Step(anim) & 1)
                h->flags |= 0x80;
            else
                h->flags &= ~0x80;
            return;
        }

        for (i = 0, bit = 1; i < h->numPoints; i++, bit <<= 1)
        {
            if (Collision_TestWallPixel(&gWallMap, h->points[i].x, h->points[i].y))
                h->collisionMask |= bit;
        }

        if (!(h->disableFlags & 1) && (mask = h->collisionMask | h->objectCollisionMask) != 0)
        {
            if (h->flags & 0x40008)
                m4aSongNumStart(0x35);
            else
                m4aSongNumStart(0x34);
            n = 2;
            for (i = 0; i < h->numPoints; i++)
            {
                if (mask & 1)
                {
                    if (h->flags & 0x40008)
                        spark = (struct AnimObject *)Task_Create(Effect_UpdateZoneHitRipple, sizeof(*spark));
                    else
                        spark = (struct AnimObject *)Task_Create(Effect_UpdateHitSpark, sizeof(*spark));
                    if (spark == NULL)
                        break;
                    spark->x = h->points[i].x;
                    spark->y = h->points[i].y;
                    if (--n == 0)
                        break;
                }
                mask >>= 1;
                if (mask == 0)
                    break;
            }
            if (h->speedNorm != 0)
            {
                h->x -= h->xs;
                h->y -= h->ys;
            }
            if ((s8)h->invulnerability == 0 && !(h->flags & 0x40008))
            {
                h->flags |= 0x900;
                h->invulnerability = 20;
                if ((s16)h->bumpCount < 99)
                    h->bumpCount++;
                if (gGameState.mode != 4)
                {
                    h->timer += 0xB4;
                    if (h->timer > 0x34BBF)
                        h->timer = 0x34BBF;
                    Task_Create(Effect_UpdateTimePenalty, 0x10);
                }
                h->lives--;
                if ((s8)h->lives > 0)
                {
                    anim->timer = 1;
                    anim->next = gHelirinHurtColorAnim;
                    anim->value = gHelirinHurtColorAnim[1];
                }
                else
                {
                    h->lives = 0;
                    if (!(h->flags & 0x18))
                        h->flags |= 0x60;
                }
                h->regenTimer = 0;
            }
        }
        else
        {
            h->flags &= ~0x800;
        }

        if (!(h->disableFlags & 1) && h->collisionMask != 0)
        {
            dir = h->angle >> 8;
            a = h->collisionMask & 0x15554;
            b = h->collisionMask & 0xAAAA;
            if (a != 0)
            {
                if (b == 0)
                    Helirin_Bump(h, dir - 0x80, 0x200);
            }
            else if (b != 0)
            {
                Helirin_Bump(h, dir, 0x200);
            }
            // a & b is always 0 (the masks share no bit), as in the original: only a hit at
            // points 0-2 (the centre) bumps it against the input direction
            if ((a & b) || (h->collisionMask & 7))
            {
                if (h->inputDir >= 0)
                    Helirin_Bump(h, h->inputDir - 0x80, 0x400);
            }
            h->nextAngleRate = h->angleRate > 0 ? -0x400 : 0x400;
        }
        h->angleRate = h->nextAngleRate;
        h->angleDefaultRate = h->nextAngleDefaultRate;
        if (h->flags & 0x200)
        {
            anim->timer = 1;
            anim->next = gHelirinHealColorAnim;
            anim->value = gHelirinHealColorAnim[1];
        }
    }

    if (Anim_Step(anim) & 1)
        h->flags |= 0x80;
    else
        h->flags &= ~0x80;
    if ((s16)h->colorTimer == 0)
    {
        if (h->flags & 0x40000)
        {
            anim->timer = 1;
            anim->next = gHelirinZoneColorAnim;
            anim->value = gHelirinZoneColorAnim[1];
        }
        else
        {
            Helirin_SetColor(h, h->color);
        }
    }
}
/**
 * Draws Helirin *h*: when its flags have bit 7, first copies its OBJ palette (0x20 bytes to
 * palette palette >> 12): gHelirinGfx + gHelirinTypeGfx[type].offset + 0x20 * colorValue, or in VS
 * mode (gGameState.mode == 4) gVsHelirinPalettes[player]. It takes an affine set rotated by its
 * angle (Sprite_AllocAffine, Sprite_SetAffineRotation), or when none is left the angle in eighths, and, while
 * on screen (within length of the edges), draws its sprite (when length > 8) and its centre, in
 * lists 7 and semi-transparent when its flags have bit 25, else lists 6.
 */
void Helirin_Draw(struct Helirin *h)
{
    struct Sprite *spr;
    s32 idx;
    s32 sx, sy;

    if (h->flags & 0x80)
    {
        if ((s8)gGameState.mode != 4)
            Main_QueueCopy(gHelirinGfx + h->colorValue * 32 + gHelirinTypeGfx[(s8)h->type].offset,
                         (u8 *)0x05000200 + (h->palette >> 12) * 32, 0x20);
        else
            Main_QueueCopy(gVsHelirinPalettes[h->player], (u8 *)0x05000200 + (h->palette >> 12) * 32, 0x20);
    }
    idx = Sprite_AllocAffine();
    if (idx >= 0)
    {
        h->affineSet = idx;
        Sprite_SetAffineRotation(idx, h->angle >> 8);
    }
    else
    {
        h->affineSet = (((h->angle >> 8) + 0x10) >> 5) & 7;
    }
    sx = POS_PIXEL(h->x) - gCameraX;
    h->screen[0] = sx;
    sy = POS_PIXEL(h->y) - gCameraY;
    h->screen[1] = sy;
    if (sx <= -h->length || sx >= h->length + 240 || sy <= -h->length || sy >= h->length + 160)
        return;

    // the low byte of each attribute word (the y) replaces spriteAttr's
    if (h->flags & 0x2000000)
    {
        if (h->length > 8)
        {
            spr = Sprite_Alloc(7);
            if (spr != NULL)
            {
                spr->attr01 = (h->spriteAttr & ~0xFF) | ((u32)h->affineSet << 25)
                          | (((sx + h->spriteDx) << 16) & 0x1FF0000) | 0x400 | (u8)(h->spriteDy + sy);
                spr->attr2 = 0x400 | h->tile | h->palette;
            }
        }
        spr = Sprite_Alloc(7);
        if (spr == NULL)
            return;
        spr->attr01 = (((sx - 4) << 16) & 0x1FF0000) | 0x400 | (u8)(sy - 4);
    }
    else
    {
        if (h->length > 8)
        {
            spr = Sprite_Alloc(6);
            if (spr != NULL)
            {
                spr->attr01 = (h->spriteAttr & ~0xFF) | ((u32)h->affineSet << 25)
                          | (((sx + h->spriteDx) << 16) & 0x1FF0000) | (u8)(h->spriteDy + sy);
                spr->attr2 = h->tile | 0x400 | h->palette;
            }
        }
        spr = Sprite_Alloc(6);
        if (spr == NULL)
            return;
        spr->attr01 = (((sx - 4) << 16) & 0x1FF0000) | (u8)(sy - 4);
    }
    spr->attr2 = h->palette | 0x70C;
}
/**
 * Pushes *h* in direction *dir* at *speed*: xb/yb become the step from the tables
 * gSineTable (x) and gCosineTable (y, subtracted) times speed and are added to x/y;
 * sets bit 28 of flags.
 */
void Helirin_Bump(struct Helirin *h, u8 dir, u16 speed)
{
    h->xb = gSineTable[dir] * speed;
    h->x += h->xb;
    h->yb = -gCosineTable[dir] * speed;
    h->y += h->yb;
    h->flags |= 0x10000000;
}
/**
 * One of the 16 pieces Helirin_Update throws out of a Helirin: piece n starts at the
 * Helirin's position and flies off in direction n * 0x10 at 3 pixels a frame (gSineTable,
 * -gCosineTable), with tile gHelirinPieceTiles[n & 7] copied to OBJ tile tile + n. While
 * gGameState.paused is 0 it moves and turns by 0x18 a frame, and frees itself once off screen
 * (margin 8); it is drawn as an 8x8 affine double-size sprite (Sprite_Alloc list 6).
 */
void Helirin_UpdatePiece(struct Task *task)
{
    struct HelirinPiece *t = (struct HelirinPiece *)task;
    struct Helirin *h;
    struct Sprite *spr;

    switch ((s16)t->state)
    {
    case 0:
        h = t->helirin;
        t->x = h->x;
        t->y = h->y;
        t->dir = t->tile << 4;
        t->vx = gSineTable[t->dir] * 0x300;
        t->vy = -gCosineTable[t->dir] * 0x300;
        Main_QueueCopy(gHelirinPieceTiles[t->tile & 7], (void *)(0x06010000 + (t->tile + h->tile) * 0x20), 0x20);
        t->tile = h->palette | 0x400 | (h->tile + t->tile);
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.paused == 0)
        {
            t->x += t->vx;
            t->y += t->vy;
            t->dir += 0x18;
            t->affineSet = ((t->dir + 0x10) >> 5) & 7;
            if (Util_GetScreenPos(POS_PIXEL(t->x), POS_PIXEL(t->y), t->screen, 8) != 0)
            {
                Task_Destroy((struct Task *)t);
                break;
            }
        }
        else if (Util_GetScreenPos(POS_PIXEL(t->x), POS_PIXEL(t->y), t->screen, 8) != 0)
        {
            break;
        }
        spr = Sprite_Alloc(6);
        if (spr != NULL)
        {
            spr->attr01 = (t->affineSet << 25) | (((t->screen[0] - 8) << 16) & 0x1FF0000) | 0x300 | (u8)(t->screen[1] - 8);
            spr->attr2 = t->tile;
        }
        break;
    }
}
/**
 * A frame of a Helirin that only spins in place: takes gMain.heldKeys as its keys (keys,
 * with Helirin_Update's repeat), clears flags bits 0, 2, 6, 8-9, 19, 21, 23, 26 and 29 (mask
 * 0xDB57FCBA) and sets bit 26 again with song 0x41 on a new R (0x100) or else 0x42 on a new
 * L (0x200), turns angle by angleRate, updates its points (Collision_UpdatePoints), runs its colour
 * animation (colorAnim, bit 7 of flags from its value) and draws it (Helirin_Draw).
 */
void Helirin_UpdatePreview(struct Helirin *h)
{
    u16 keys = gMain.heldKeys;
    u16 prev = h->keys[0];
    u16 pressed = keys & ~prev;

    h->keys[0] = keys;
    h->keys[1] = prev;
    h->keys[2] = pressed;
    h->keys[3] = pressed;
    if (keys == 0 || keys != prev)
    {
        h->keys[4] = 20;
    }
    else if (--h->keys[4] == 0)
    {
        h->keys[3] = keys;
        h->keys[4] = 4;
    }

    h->flags &= 0xDB57FCBA;
    if (h->keys[2] & 0x100)
    {
        h->flags |= 0x4000000;
        m4aSongNumStart(0x41);
    }
    else if (h->keys[2] & 0x200)
    {
        h->flags |= 0x4000000;
        m4aSongNumStart(0x42);
    }
    h->angle += h->angleRate;
    Collision_UpdatePoints(h);
    if (Anim_Step((struct Anim *)&h->colorAnim) & 1)
        h->flags |= 0x80;
    else
        h->flags &= ~0x80;
    Helirin_Draw(h);
}
