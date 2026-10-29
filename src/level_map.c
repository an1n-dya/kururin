// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// level_map: reading a level map: start positions and the objects it places.
// 080057B8-08005F24, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "task.h"
#include "level_map.h"
#include "objects.h"
#include "bonus.h"

extern const u16 *gMapReadCursor;

// The tasks LevelMap_Read starts from the number at the start of the map (Bonus_UpdateItem:
// 0x20 bytes, Bonus_UpdateColor and Bonus_UpdateType: 0x18 bytes).
struct MapBonusTask {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ u16 x;
    /*0x0A*/ u16 y;
    /*0x0C*/ u8 unkC[4];
    /*0x10*/ u16 unlock; // which bit of gGameState.saveFile->unlocks (within its group)
};
s32 LevelMap_ReadNumber(void);
void LevelMap_StartMarkedObject(struct Map *map, s32 id, s32 x, s32 y);

// The x and y words of a Piston or Shooter task (16.16 fixed point, at +0x08 and +0x0C). The
// original stores them through a pointer to +0x0A: the ARM7 ignores the low bits of a
// misaligned str, so they land at +0x08 and +0x0C (the tasks are word-aligned, Heap_Alloc).
// The GBA build keeps those stores, and with them the original's code and cycles: LevelMap_Read
// runs at every level start, and a slower or faster scan moves the frame a level starts on
// (docs/audit.md, Timing). The PC port, where a misaligned store is stored as it is, writes
// the fields.
#ifdef PLATFORM_PC
#define SET_POSITION(task, px, py) \
    {                              \
        (task)->x = (px);          \
        (task)->y = (py);          \
    }
#else
#define SET_POSITION(task, px, py)                 \
    {                                              \
        *(s32 *)((u8 *)(task) + 0xA) = (px);       \
        *(s32 *)((u8 *)(task) + 0xE) = (py);       \
    }
#endif

/**
 * The number written at the start of *str* in the game's characters, whose digits are
 * 0xE0-0xE9: stops at the first other character.
 */
s32 LevelMap_ParseNumber(const u16 *str)
{
    s32 value = 0;
    u16 c;

    while ((u16)((c = *str++) - 0xE0) <= 9)
        value = value * 10 + (c - 0xE0);
    return value;
}
/**
 * LevelMap_ParseNumber on the string cursor gMapReadCursor: the number at the cursor, in the game's
 * digits 0xE0-0xE9; the cursor ends past the first other character.
 */
s32 LevelMap_ReadNumber(void)
{
    s32 value = 0;
    u16 c;

    for (;;) {
        c = *gMapReadCursor;
        gMapReadCursor++;
        if ((u16)(c - 0xE0) > 9)
            break;
        value = value * 10 + (c - 0xE0);
    }
    return value;
}
/**
 * Reads level map *map* (tiles, low 10 bits): codes 0xED-0xFF set the positions in gCamera
 * (0xFB/0xFC startX/startY[0], 0xED-0xEF entries 1-3, 0xFF goalX/goalY; bit n of ccwStarts
 * from the tile's flip bits) or start object tasks there, with parameters from the digits
 * after the code or in the rows below; 0xF0 starts the tasks of the 0xF1 marker it names
 * (LevelMap_StartMarkedObject). The camera bounds are the extent of tiles 1-0x9F.
 * In Adventure, Normal or Easy (modes 1-2), in worlds 1-10, unless bit (world * 3 + level - 3) of
 * gGameState.saveFile->bonusTaken is set, the number at the start of the map picks one bit of
 * gGameState.saveFile->unlocks (1-10: bits 0-9, 11-19: bits 11-19, 20+: bits 17+); if that
 * bit is clear, its task is started at the position that follows and the bit goes in
 * gCamera.bonusUnlock. (The original's world 0 branch, bit level - 5, is never taken.)
 */
void LevelMap_Read(struct Map *map)
{
    const u16 *tiles = map->tiles;
    struct Camera *cam = &gCamera;
    u16 width = map->width;
    u32 height = map->height;
    u32 minX, minY, maxX, maxY;
    u32 col, row;
    const u16 *p;
    // the bonus's unlock bit, then each tile: one variable for both, as in the original (its
    // register allocation, and so the scan's cycles, depend on it: see SET_POSITION)
    u32 tile;
    s32 i;

// the tile is flipped one way only (one of bits 10 and 11)
#define ONE_FLIP(tile) (((tile) & 0xC00) == 0x800 || ((tile) & 0xC00) == 0x400)

    cam->bonusUnlock = 0;
    if ((u8)(gGameState.mode - 1) <= 1 && (u8)(gGameState.world - 1) <= 9)
    {
        u32 taken = gGameState.saveFile->bonusTaken;

        if (gGameState.world == 0)
        {
            if (taken & (1 << (gGameState.level - 5)))
                goto scan;
        }
        else if (taken & (1 << (gGameState.world * 3 + gGameState.level - 3)))
        {
            goto scan;
        }
        {
            void (*func)(struct Task *);
            u32 size;
            s32 n;

            gMapReadCursor = tiles;
            n = LevelMap_ReadNumber();
            if (n == 0)
                goto scan;
            if (n <= 10)
            {
                n -= 1;
                tile = 1 << n;
                if (gGameState.saveFile->unlocks & tile)
                    goto scan;
                func = (void (*)(struct Task *))Bonus_UpdateItem;
                size = 0x20;
            }
            else if (n <= 0x13)
            {
                n -= 0xA;
                tile = 0x400 << n;
                if (gGameState.saveFile->unlocks & tile)
                    goto scan;
                func = (void (*)(struct Task *))Bonus_UpdateColor;
                size = 0x18;
            }
            else
            {
                n -= 0x14;
                tile = 0x20000 << n;
                if (gGameState.saveFile->unlocks & tile)
                    goto scan;
                func = (void (*)(struct Task *))Bonus_UpdateType;
                size = 0x18;
            }
            {
                struct MapBonusTask *task = (struct MapBonusTask *)Task_Create(func, size);

                if (task != NULL)
                {
                    task->unlock = n;
                    task->x = LevelMap_ReadNumber() * 8 + 4;
                    task->y = LevelMap_ReadNumber() * 8 + 4;
                    cam->bonusUnlock = tile;
                }
            }
        }
    }
scan:
    for (i = 0; i < 4; i++)
        cam->startX[i] = cam->startY[i] = 0;
    minX = width;
    maxX = 0;
    minY = height;
    maxY = 0;
    for (row = 0; row < height; row++)
    {
        p = tiles + row * width;
        for (col = 0; col < width; col++)
        {
            tile = *p++;
            switch (tile & 0x3FF)
            {
            case 0xF0:
                LevelMap_StartMarkedObject(map, LevelMap_ParseNumber(p), col, row);
                break;
            case 0xF1:
                // skip the marker's digits, the character that ends them and the next one
                do
                {
                    tile = *p++;
                    col++;
                } while (tile - 0xE0 <= 9);
                p++;
                col++;
                break;
            case 0xFB:
                if (ONE_FLIP(tile))
                    cam->ccwStarts |= 1;
                else
                    cam->ccwStarts &= ~1;
                cam->startX[0] = col * 8 + 8;
                cam->startY[0] = row * 8 + 8;
                break;
            case 0xFC:
                if (ONE_FLIP(tile))
                    cam->ccwStarts &= ~1;
                else
                    cam->ccwStarts |= 1;
                cam->startX[0] = col * 8 + 8;
                cam->startY[0] = row * 8 + 8;
                break;
            case 0xED:
                if (ONE_FLIP(tile))
                    cam->ccwStarts |= 2;
                else
                    cam->ccwStarts &= ~2;
                cam->startX[1] = col * 8 + 8;
                cam->startY[1] = row * 8 + 8;
                break;
            case 0xEE:
                if (ONE_FLIP(tile))
                    cam->ccwStarts |= 4;
                else
                    cam->ccwStarts &= ~4;
                cam->startX[2] = col * 8 + 8;
                cam->startY[2] = row * 8 + 8;
                break;
            case 0xEF:
                if (ONE_FLIP(tile))
                    cam->ccwStarts |= 8;
                else
                    cam->ccwStarts &= ~8;
                cam->startX[3] = col * 8 + 8;
                cam->startY[3] = row * 8 + 8;
                break;
            case 0xFF:
                cam->goalX = col * 8 + 8;
                cam->goalY = row * 8 + 8;
                break;
            case 0xF8:
            {
                struct Task *task;

                if (tile & 0x800)
                    task = Task_Create((void (*)(struct Task *))Object_SpringDown, 0x18);
                else
                    task = Task_Create((void (*)(struct Task *))Object_SpringUp, 0x18);
                if (task != NULL)
                {
                    task->unk8 = col * 8;
                    task->unkA = row * 8;
                }
                break;
            }
            case 0xF9:
            {
                struct Task *task;

                if (tile & 0x400)
                    task = Task_Create((void (*)(struct Task *))Object_SpringLeft, 0x18);
                else
                    task = Task_Create((void (*)(struct Task *))Object_SpringRight, 0x18);
                if (task != NULL)
                {
                    task->unk8 = col * 8;
                    task->unkA = row * 8;
                }
                break;
            }
            case 0xF7:
            {
                struct RollerObject *task = (struct RollerObject *)Task_Create(Object_Roller, sizeof(*task));
                u32 flags;

                if (task == NULL)
                    break;
                flags = LevelMap_ParseNumber(p) << 5;
                task->dir = flags;
                task->speed = 0xC0;
                if (flags & 0x20)
                {
                    task->x = col * 8 + 8;
                    task->y = row * 8 + 16;
                }
                else
                {
                    task->x = col * 8 + 16;
                    task->y = row * 8 + 16;
                }
                task->timer = LevelMap_ParseNumber(tiles + (row + 1) * width + col);
                task->period = LevelMap_ParseNumber(tiles + (row + 2) * width + col);
                break;
            }
            case 0xF6:
            {
                struct RollerCatcherObject *task = (struct RollerCatcherObject *)Task_Create(Object_RollerCatcher, sizeof(*task));
                u32 flags;

                if (task == NULL)
                    break;
                flags = LevelMap_ParseNumber(p) << 5;
                task->dir = flags;
                if (flags & 0x20)
                {
                    task->x = col * 8 + 8;
                    task->y = row * 8 + 16;
                }
                else
                {
                    task->x = col * 8 + 16;
                    task->y = row * 8 + 16;
                }
                break;
            }
            case 0xF5:
            {
                struct PistonObject *task = (struct PistonObject *)Task_Create((void (*)(struct Task *))Object_Piston, sizeof(*task));

                if (task == NULL)
                    break;
                SET_POSITION(task, (col << 19) + 0x100000, (row << 19) + 0x100000);
                task->dir = LevelMap_ParseNumber(p) << 6;
                task->waitTimer = LevelMap_ParseNumber(tiles + (row + 1) * width + col);
                task->waitPeriod = 0x78;
                task->phaseStep = 0x111;
                break;
            }
            case 0xF4:
            {
                struct ShooterObject *task = (struct ShooterObject *)Task_Create((void (*)(struct Task *))Object_Shooter, sizeof(*task));
                u8 v;

                if (task == NULL)
                    break;
                SET_POSITION(task, col << 19, row << 19);
                v = LevelMap_ParseNumber(p) << 5;
                task->arcMin = (v / 10) << 5;
                task->arcMax = (v % 10) << 5;
                task->timer = LevelMap_ParseNumber(tiles + (row + 1) * width + col);
                task->period = 0x90;
                break;
            }
            default:
                tile &= 0x3FF;
                if (tile - 1 <= 0x9E)
                {
                    if (col < minX)
                        minX = col;
                    else if (col >= maxX)
                        maxX = col;
                    if (row < minY)
                        minY = row;
                    else if (row >= maxY)
                        maxY = row;
                }
                break;
            }
        }
    }
    cam->boundLeft = minX * 8 - 120;
    cam->boundRight = maxX * 8 - 120;
    cam->boundTop = minY * 8 - 80;
    cam->boundBottom = maxY * 8 - 80;
#undef ONE_FLIP
}
/**
 * Looks through *map*'s tiles (low 10 bits) for marker 0xF1 followed by the number *id*
 * (LevelMap_ReadNumber), then reads on to the first of 0xF4-0xF7 and starts that one's task at tile
 * (*x*, *y*), with the numbers that follow as its parameters. The scan's count goes up once
 * per tile tested for 0xF1, not for the digits LevelMap_ReadNumber reads after one, as in the
 * original. The 0xF4/0xF5 tasks' positions go through SET_POSITION, as in LevelMap_Read.
 *
 * Its code takes the original's cycles (LevelMap_Read calls it for each 0xF0 code during the
 * scan of a level start, docs/audit.md, Timing): the rollers set x and y in each branch, as
 * the original's register order shows.
 */
void LevelMap_StartMarkedObject(struct Map *map, s32 id, s32 x, s32 y)
{
    const u16 *tiles = map->tiles;
    u16 width = map->width;
    u16 height = map->height;
    u32 i;
    u32 code;

    gMapReadCursor = tiles;
    for (i = 0; i < width * height; i++)
    {
        if ((*gMapReadCursor++ & 0x3FF) != 0xF1 || id != LevelMap_ReadNumber())
            continue;
        for (;;)
        {
            code = *gMapReadCursor++;
            switch (code & 0x3FF)
            {
            case 0xF7:
            {
                struct RollerObject *task = (struct RollerObject *)Task_Create(Object_Roller, sizeof(*task));
                u32 flags;

                if (task == NULL)
                    return;
                flags = LevelMap_ReadNumber() << 5;
                task->dir = flags;
                if (flags & 0x20)
                {
                    task->x = x * 8 + 8;
                    task->y = y * 8 + 16;
                }
                else
                {
                    task->x = x * 8 + 16;
                    task->y = y * 8 + 16;
                }
                task->timer = LevelMap_ReadNumber();
                task->period = LevelMap_ReadNumber();
                task->speed = LevelMap_ReadNumber();
                return;
            }
            case 0xF6:
            {
                struct RollerCatcherObject *task = (struct RollerCatcherObject *)Task_Create(Object_RollerCatcher, sizeof(*task));
                u32 flags;

                if (task == NULL)
                    return;
                flags = LevelMap_ReadNumber() << 5;
                task->dir = flags;
                if (flags & 0x20)
                {
                    task->x = x * 8 + 8;
                    task->y = y * 8 + 16;
                }
                else
                {
                    task->x = x * 8 + 16;
                    task->y = y * 8 + 16;
                }
                return;
            }
            case 0xF5:
            {
                struct PistonObject *task = (struct PistonObject *)Task_Create((void (*)(struct Task *))Object_Piston, sizeof(*task));

                if (task == NULL)
                    return;
                SET_POSITION(task, (x << 19) + 0x100000, (y << 19) + 0x100000);
                task->dir = LevelMap_ReadNumber() << 6;
                task->waitTimer = LevelMap_ReadNumber();
                task->waitPeriod = LevelMap_ReadNumber();
                task->phaseStep = 0x10000 / LevelMap_ReadNumber();
                return;
            }
            case 0xF4:
            {
                struct ShooterObject *task = (struct ShooterObject *)Task_Create((void (*)(struct Task *))Object_Shooter, sizeof(*task));

                if (task == NULL)
                    return;
                SET_POSITION(task, x << 19, y << 19);
                task->arcMin = LevelMap_ReadNumber() << 5;
                task->arcMax = LevelMap_ReadNumber() << 5;
                task->timer = LevelMap_ReadNumber();
                task->period = LevelMap_ReadNumber();
                return;
            }
            }
        }
    }
}
