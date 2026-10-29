// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// level: a new game, the level number, loading a level and starting its tasks.
// 08004C24-08005178, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "decompress.h"
#include "task.h"
#include "util.h"
#include "level.h"
#include "save.h"
#include "level_map.h"
#include "makeup.h"
#include "effects.h"

extern u32 gWorldTileAnims;
extern const u32 gPhysicalTiles[][8];
extern const struct TileCopy gLevelSpriteTiles[];
extern u8 gSaveData[];
extern const struct GameState gInitialGameState;
void m4aSoundInit(void);
// The three Effect_AnimateCopies tasks of every level, and the one of worlds 2, 5 and 7
// (Level_StartTasks): {animation frames, the copies of each frame}.
extern const u16 gUnk_080C6088[];
extern const struct QueuedCopy gUnk_080C60AC[];
extern const u16 gUnk_080C6100[];
extern const struct QueuedCopy gUnk_080C6120[];
extern const u16 gUnk_080C6150[];
extern const struct QueuedCopy gUnk_080C6164[];
extern const u16 gUnk_080C6194[];
extern const struct QueuedCopy gUnk_080C61BC[];
extern const u16 gUnk_080C6204[];
extern const struct QueuedCopy gUnk_080C6228[];

/**
 * The game's main state 0, at power-on: empties the copy queue of gTransferCount, copies the initial
 * gGameState (0x40 bytes of gInitialGameState), points gSaveBank and gSaveFiles at the
 * EWRAM buffer gSaveData, loads the save (Save_LoadAtBoot) and goes on to main-loop state 1.
 */
void Game_Boot(void)
{
    gTransferCount = 0;
    m4aSoundInit();
    Main_ClearKeys();
    Main_InstallLinkFrameCallback();
    Main_EndFrame();
    memcpy(&gGameState, &gInitialGameState, 0x40);
    Lives_Reset();
    gSaveBank = gSaveData;
    gSaveFiles = gSaveData;
    Save_LoadAtBoot();
    Random_SetIndices(0);
    Random_Reset();
    Main_EndFrame();
    gMain.state = 1;
}
/**
 * Sets gGameState.world/level from a level index *n* (docs/save_format.md): 0-4 are world 0 level n,
 * 5-0x25 are three levels a world from world 1 (world (n - 2) / 3, level (n - 2) % 3), and
 * the rest are world 12 level n - 0x26.
 */
void Level_SetFromIndex(s32 n)
{
    if (n <= 4)
    {
        gGameState.world = 0;
        gGameState.level = n;
    }
    else if (n <= 0x25)
    {
        n -= 2;
        gGameState.world = n / 3;
        gGameState.level = n % 3;
    }
    else
    {
        gGameState.world = 12;
        gGameState.level = n - 0x26;
    }
}
/**
 * Loads the current level (gGameState.world, .level) from gWorldEntries: decompresses the
 * world's graphical and background tiles and palette (the training one in mode 5, Practice)
 * into the EWRAM buffer and copies them to VRAM, copies the physical tiles (the Challenge
 * ones in world 12) and the sprite graphics, then decompresses the level's three layers
 * into maps 1-3 of gMaps (a missing layer is cleared). Returns -1 when the level
 * has no level data, else 0.
 */
s32 Level_Load(void)
{
    u8 world = gGameState.world;
    u8 level = gGameState.level;
    const struct WorldInfo *info;
    const u8 *data;
    const struct TileCopy *e;
    u16 *buf;
    u32 zero;
    s32 n;
    s32 i;

    gWorldInfo = info = gWorldEntries[world].info;
    gWorldData = data = gWorldEntries[world].data;
    gWorldTileAnims = gWorldEntries[world].tileAnims;
    gMain.ewramCursor = (u8 *)0x02000000;
    n = Decompress_LZ(data + info->tilesOffset, (u8 *)0x02000000, info->tilesSize);
    Main_QueueCopy(gMain.ewramCursor, (void *)0x06008000, 0x4000);
    gMain.ewramCursor += n;
    if (world == 12)
    {
        n = Decompress_LZ(gWorldData + gWorldInfo->bgTilesOffset, gMain.ewramCursor, gWorldInfo->bgTilesSize);
        Main_QueueCopy(gMain.ewramCursor, (void *)0x06002000, 0x2000);
    }
    else
    {
        n = Decompress_LZ(gWorldData + gWorldInfo->bgTilesOffset, gMain.ewramCursor, gWorldInfo->bgTilesSize);
        Main_QueueCopy(gMain.ewramCursor, (void *)0x06000000, 0x8000);
    }
    gMain.ewramCursor += n;
    if ((s8)gGameState.mode == 5)
        n = Decompress_LZ(gWorldData + gWorldInfo->trainingPaletteOffset, gMain.ewramCursor,
                         gWorldInfo->trainingPaletteSize);
    else
        n = Decompress_LZ(gWorldData + gWorldInfo->paletteOffset, gMain.ewramCursor, gWorldInfo->paletteSize);
    Main_QueueCopy(gMain.ewramCursor, (void *)0x05000000, 0x1C0);
    gMain.ewramCursor += n;
    if (world == 12)
        Main_QueueCopy(gPhysicalTilesChallenge, (void *)0x0600C000, 0x2000);
    else
        Main_QueueCopy(gPhysicalTiles, (void *)0x0600C000, 0x2000);
    Main_QueueCopy(gLevelSpritePalettes, (void *)0x05000200, 0x1C0);
    for (e = gLevelSpriteTiles; e->src != NULL; e++)
        Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
    Main_EndFrame();

    gMain.ewramCursor = (u8 *)0x02000000;
    for (i = 1; i <= 3; i++)
    {
        if (gWorldInfo->levels[level].layers[i - 1].size != 0)
        {
            Decompress_LZ(gWorldData + gWorldInfo->levels[level].layers[i - 1].offset, gMain.ewramCursor,
                         gWorldInfo->levels[level].layers[i - 1].size);
            // the layer starts with its width and height
            buf = (u16 *)gMain.ewramCursor;
            gMain.ewramCursor += gWorldInfo->levels[level].layers[i - 1].size;
            Bg_InitMap(&gMaps[i], &gBgLayers[i], buf + 2, buf[0], buf[1]);
        }
        else
        {
            if (i == 1)
                return -1;
            Bg_InitMap(&gMaps[i], &gBgLayers[i], NULL, 0, 0);
            zero = 0;
            CpuSet(&zero, gBgLayers[i].buffer, 0x05000000 | (gBgLayers[i].mapSize >> 2));
            gBgLayers[i].dirty = 1;
        }
    }
    return 0;
}
/**
 * Starts a level's tasks: clears bits 4 and 6 of gGameState.flags and sets bit 7, then a
 * Effect_OpenWindow task, a Effect_AnimateBlockCopies task per {frames, copies} pair of the world's list
 * gWorldTileAnims (NULL-terminated; it stops when a task cannot be allocated), the
 * Effect_UpdateGoal and Effect_CyclePalette tasks and three Effect_AnimateCopies tasks. Then by world it sets
 * gGameState.song (0: 13, 1-10: the world, 11: 0x16, 12: 11, 13: 12; worlds 2, 5 and 7 also
 * get a Effect_AnimateCopies task of their own) and starts the map's objects (LevelMap_Read).
 */
void Level_StartTasks(void)
{
    const u32 *list;
    struct BlockCopyAnim *anim;
    struct CopyAnim *task;

    gGameState.flags = (gGameState.flags & ~0x50) | 0x80;
    Task_Create(Effect_OpenWindow, 8);
    if (gWorldTileAnims != 0)
    {
        for (list = (const u32 *)gWorldTileAnims; *list != 0;)
        {
            anim = (struct BlockCopyAnim *)Task_Create(Effect_AnimateBlockCopies, sizeof(*anim));
            if (anim == NULL)
                break;
            anim->anim.timer = 1;
            anim->anim.next = (const u16 *)*list++;
            anim->anim.value = anim->anim.next[1];
            anim->copies = (const struct BlockCopy *)*list++;
        }
    }
    Task_Create(Effect_UpdateGoal, 0x18);
    Task_Create(Effect_CyclePalette, 0x14);
    task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
    if (task != NULL)
    {
        task->anim.timer = 1;
        task->anim.next = gUnk_080C6088;
        task->anim.value = gUnk_080C6088[1];
        task->copies = gUnk_080C60AC;
    }
    task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
    if (task != NULL)
    {
        task->anim.timer = 1;
        task->anim.next = gUnk_080C6100;
        task->anim.value = gUnk_080C6100[1];
        task->copies = gUnk_080C6120;
    }
    task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
    if (task != NULL)
    {
        task->anim.timer = 1;
        task->anim.next = gUnk_080C6270;
        task->anim.value = gUnk_080C6270[1];
        task->copies = gUnk_080C6284;
    }

    switch (gGameState.world)
    {
    case 0:
        gGameState.song = 13;
        break;
    case 1:
        gGameState.song = 1;
        break;
    case 2:
        gGameState.song = 2;
        task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
        if (task != NULL)
        {
            task->anim.timer = 1;
            task->anim.next = gUnk_080C6150;
            task->anim.value = gUnk_080C6150[1];
            task->copies = gUnk_080C6164;
        }
        break;
    case 3:
        gGameState.song = 3;
        break;
    case 4:
        gGameState.song = 4;
        break;
    case 5:
        gGameState.song = 5;
        task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
        if (task != NULL)
        {
            task->anim.timer = 1;
            task->anim.next = gUnk_080C6194;
            task->anim.value = gUnk_080C6194[1];
            task->copies = gUnk_080C61BC;
        }
        break;
    case 6:
        gGameState.song = 6;
        break;
    case 7:
        gGameState.song = 7;
        task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
        if (task != NULL)
        {
            task->anim.timer = 1;
            task->anim.next = gUnk_080C6204;
            task->anim.value = gUnk_080C6204[1];
            task->copies = gUnk_080C6228;
        }
        break;
    case 8:
        gGameState.song = 8;
        break;
    case 9:
        gGameState.song = 9;
        break;
    case 10:
        gGameState.song = 10;
        break;
    case 11:
        gGameState.song = 0x16;
        break;
    case 12:
        gGameState.song = 11;
        break;
    case 13:
        gGameState.song = 12;
        break;
    }
    LevelMap_Read(&gWallMap);
}
