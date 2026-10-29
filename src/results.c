// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// results: the result screen, rank-ups, the camera origin, a level's records.
// 0800AA0C-0800B8A0, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "task.h"
#include "util.h"
#include "save.h"
#include "results.h"
#include "script.h"
#include "hud.h"
#include "effects.h"
#include "sound.h"

extern s32 gCloudDriftX;
extern s32 gCloudDriftY;
s32 Results_EnterRecord(void);
extern const struct TileCopy gResultsSpriteTiles[];
extern const u8 gResultsOtherHelirinTiles[];
extern const u8 gUnk_080C3B84[4];
extern const u16 gUnk_080C3B20[];
extern const u8 gTextBestTime[];
extern const u8 gUnk_0801E824[];

/**
 * The frame loop of a screen drawn over the level: Results_EnterRecord's result (flags) picks what
 * it shows; gGameState.timer, bestTime and bumpCount, and the level's three records of
 * *gSaveBank (struct LevelRecord), the one of flags & 3 blinking when flags & 4.
 * A or B ends it: it then runs gWindowRadius down through Bg_DrawWindowCircle and returns.
 */
void Results_Run(void)
{
    s32 state = 0;
    s32 flags;
    s32 timer;
    s32 count;
    struct Anim anim;
    const struct TileCopy *e;
    struct Sprite *spr;
    struct LevelRecord *rec;
    u8 *table;
    s32 palette;
    s32 idx;
    s32 i;
    s32 s;
    u16 v;

    do
    {
        switch (state)
        {
        case 0:
            flags = Results_EnterRecord();
            Main_EndFrame();
            gGameState.helirin->regenTimer = 0;
            gGameState.helirin->lives = gGameState.helirin->maxLives;
            if (gGameState.flags & GAME_FLAG_BONUS_TAKEN)
            {
                struct SaveFile *p = gGameState.saveFile;

                p->unlocks |= gCamera.bonusUnlock;
                if (gGameState.world == 0)
                    i = gGameState.level - 5;
                else
                    i = gGameState.world * 3 + gGameState.level - 3;
                p->bonusTaken |= 1 << i;
            }
            Main_EndFrame();
            Save_Write();
            Main_EndFrame();
            gWinoutCopy = 0x11;
            for (e = gResultsSpriteTiles; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
            if (flags & 8)
                Main_QueueCopy(gResultsOtherHelirinTiles, (void *)0x06016A80, 0x800);
            Sprite_ClearLists();
            Hud_Draw(gGameState.helirin);
            Sprite_BuildOam();
            Main_EndFrame();
            Task_Init();
            {
                struct CopyAnim *task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));

                if (task != NULL)
                {
                    task->anim.timer = 1;
                    task->anim.next = gUnk_080C6270;
                    task->anim.value = gUnk_080C6270[1];
                    task->copies = gUnk_080C6284;
                }
            }
            {
                struct SongAfterPlayer *task = (struct SongAfterPlayer *)Task_Create((void (*)(struct Task *))Sound_UpdateSongAfterPlayer, sizeof(*task));

                if (task != NULL)
                {
                    task->player = &gMPlayInfo_BGM;
                    task->song = 0x11;
                    task->delay = 0x1E;
                }
            }
            timer = 0;
            if (flags & 4)
                count = gUnk_080C3B84[flags & 3];
            else
                count = 0;
            anim.timer = 1;
            anim.next = gUnk_080C3B20;
            anim.value = gUnk_080C3B20[1];
            state = 1;
            break;
        case 1:
            Sprite_ClearLists();
            if (count != 0 && timer++ > 0x1E)
            {
                Task_Create(Effect_Firework, 8);
                count--;
                timer = 0;
            }
            spr = Sprite_Alloc(1);
            if (spr != NULL)
            {
                spr->attr01 = 0xC0384000;
                spr->attr2 = 0xD0A0;
            }
            spr = Sprite_Alloc(1);
            if (spr != NULL)
            {
                spr->attr01 = 0xC0784000;
                spr->attr2 = 0xD0C0;
            }
            if (flags & 4)
                palette = (gMain.frameCount & 8) ? 0xF : 0xE;
            else
                palette = 2;
            spr = Sprite_Alloc(1);
            if (spr != NULL)
            {
                spr->attr01 = 0x80384000 | 0x24;
                spr->attr2 = 0x6394;
            }
            Text_DrawTime(0x98, 0x24, gGameState.timer, palette);
            Text_Draw(0x38, 0x34, gTextBestTime, 6);
            Text_DrawTime(0x98, 0x34, gGameState.bestTime, 0xE);
            spr = Sprite_Alloc(1);
            if (spr != NULL)
            {
                spr->attr01 = 0x80384000 | 0x44;
                spr->attr2 = 0x639C;
            }
            Text_DrawNumberAligned(0xC8, 0x44, gGameState.bumpCount, 2, 0xE);
            spr = Sprite_Alloc(1);
            if (spr != NULL)
            {
                spr->attr01 = 0x80504000 | 0x58;
                spr->attr2 = 0x33A4;
            }
            Text_Draw(0x50, 0x58, gUnk_0801E824, 3);
            spr = Sprite_Alloc(1);
            if (spr != NULL)
            {
                spr->attr01 = 0x80804000 | 0x58;
                spr->attr2 = 0x33AC;
            }

            if ((s8)gGameState.mode == 2)
                table = gSaveBank + 0x1B34;
            else
                table = gSaveBank + 0x9C4;
            if (gGameState.world == 0)
                idx = gGameState.level;
            else
                idx = gGameState.world * 3 + gGameState.level + 2;
            rec = (struct LevelRecord *)(table + idx * 0x30);
            for (i = 0; i <= 2; i++)
            {
                u8 y;

                if ((flags & 3) == i + 1)
                {
                    if (flags & 4)
                        palette = (gMain.frameCount & 8) ? 0xF : 0xE;
                    else
                        palette = 2;
                }
                else
                    palette = 0xE;
                y = i * 16 + 0x6C;
                Text_DrawName(0x38, y, rec[i].name, rec[i].rank, palette);
                Text_DrawTime(0x98, y, rec[i].time, palette);
            }

            if (flags & 8)
            {
                if ((gMain.frameCount & 0x60) && !(gMain.heldKeys & 4))
                {
                    spr = Sprite_Alloc(1);
                    if (spr != NULL)
                    {
                        spr->attr01 = 0xC0104000 | 0x58;
                        spr->attr2 = 0xE354;
                    }
                    spr = Sprite_Alloc(1);
                    if (spr != NULL)
                    {
                        spr->attr01 = 0xC0504000 | 0x58;
                        spr->attr2 = 0xE374;
                    }
                }
            }
            else
            {
                Anim_Step(&anim);
                if (flags & 4)
                {
                    // an affine sprite whose scale follows a sine: 0x10000 / (0x100 + s / 2)
                    idx = Sprite_AllocAffine();
                    s = gSineTable[(gMain.frameCount & 0x3F) * 4];
                    if (s < 0)
                        s = 0;
                    else if (s > 0xC0)
                        s = 0x180 - s;
                    v = 0x10000 / (s / 2 + 0x100);
                    gAffineParams[idx][3] = v;
                    gAffineParams[idx][0] = v;
                    gAffineParams[idx][2] = 0;
                    gAffineParams[idx][1] = 0;
                    spr = Sprite_Alloc(1);
                    if (spr != NULL)
                    {
                        spr->attr01 = ((u32)idx << 25) | 0xC0384300 | 0x0C;
                        spr->attr2 = 0xD354;
                    }
                    if (flags & 3)
                    {
                        idx = Sprite_AllocAffine();
                        s = -gSineTable[(gMain.frameCount & 0x3F) * 4];
                        if (s < 0)
                            s = 0;
                        else if (s > 0xC0)
                            s = 0x180 - s;
                        v = 0x10000 / (s / 2 + 0x100);
                        gAffineParams[idx][3] = v;
                        gAffineParams[idx][0] = v;
                        gAffineParams[idx][2] = 0;
                        gAffineParams[idx][1] = 0;
                        spr = Sprite_Alloc(1);
                        if (spr != NULL)
                        {
                            spr->attr01 = ((u32)idx << 25) | 0xC1E84300 | 0x40;
                            spr->attr2 = 0xD374;
                        }
                    }
                }
            }
            Hud_Draw(gGameState.helirin);
            Task_RunAll();
            Sprite_BuildOam();
            if (gMain.newKeys & (A_BUTTON | B_BUTTON))
                state = 2;
            break;
        case 2:
            if (gWindowRadius-- <= 0)
            {
                gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x100;
                state = -1;
            }
            Bg_DrawWindowCircle(gWindowRadius, gWindowX, gWindowY);
            Sprite_ClearLists();
            Hud_Draw(gGameState.helirin);
            Task_RunAll();
            Sprite_BuildOam();
            break;
        }
        Main_EndFrame();
    } while (state != -1);
}
/**
 * While the save file's rank (0-2) is below 3 and its best time of every level 5-0x22 and
 * 0x26-0x57 (bestTimes) beats that level's gLevelTimes[level][rank] (levels 5-0x22 from
 * gEasyLevelTimes, the Easy Mode times, with flags bit 4): shows Script_Run(5), raises rank
 * and, when it reaches 2, enters gLevelTimes[level][2] under the name gDefaultRecordNames[2]
 * (record rank 3) in every level's records (*gSaveBank + 0x9C4, levels 0-0x22 at + 0x1B34 with
 * flags bit 4) where it is not yet, then saves (Save_Write) and waits a frame.
 */
void Results_CheckRankUp(void)
{
    struct SaveFile *save = gGameState.saveFile;
    struct LevelRecord *rec;
    u32 time;
    u32 d;
    u32 i, j, k;

    while (save->rank <= 2)
    {
        d = save->rank;
        if (save->flags & 0x10)
        {
            for (i = 5; i <= 0x22; i++)
            {
                if (save->bestTimes[i] >= gEasyLevelTimes[i][d])
                    return;
            }
        }
        else
        {
            for (i = 5; i <= 0x22; i++)
            {
                if (save->bestTimes[i] >= gLevelTimes[i][d])
                    return;
            }
        }
        for (i = 0x26; i <= 0x57; i++)
        {
            if (save->bestTimes[i] >= gLevelTimes[i][d])
                return;
        }

        Script_Run(5);
        save->rank++;
        if (save->rank == 2)
        {
            if (save->flags & 0x10)
            {
                for (i = 0; i <= 0x22; i++)
                {
                    rec = (struct LevelRecord *)(gSaveBank + 0x1B34) + i * 3;
                    time = gEasyLevelTimes[i][2];
                    for (j = 0; j <= 2; j++)
                    {
                        if (rec[j].time == time && strcmp(rec[j].name, gDefaultRecordNames[2]) == 0)
                            break;
                        if (rec[j].time > time)
                        {
                            for (k = 2; k > j; k--)
                            {
                                rec[k].time = rec[k - 1].time;
                                strcpy(rec[k].name, rec[k - 1].name);
                                rec[k].rank = rec[k - 1].rank;
                            }
                            rec[j].time = time;
                            strcpy(rec[j].name, gDefaultRecordNames[2]);
                            rec[j].rank = 3;
                            break;
                        }
                    }
                }
            }
            else
            {
                for (i = 0; i <= 0x22; i++)
                {
                    rec = (struct LevelRecord *)(gSaveBank + 0x9C4) + i * 3;
                    time = gLevelTimes[i][2];
                    for (j = 0; j <= 2; j++)
                    {
                        if (rec[j].time == time && strcmp(rec[j].name, gDefaultRecordNames[2]) == 0)
                            break;
                        if (rec[j].time > time)
                        {
                            for (k = 2; k > j; k--)
                            {
                                rec[k].time = rec[k - 1].time;
                                strcpy(rec[k].name, rec[k - 1].name);
                                rec[k].rank = rec[k - 1].rank;
                            }
                            rec[j].time = time;
                            strcpy(rec[j].name, gDefaultRecordNames[2]);
                            rec[j].rank = 3;
                            break;
                        }
                    }
                }
            }
            for (i = 0x26; i <= 0x57; i++)
            {
                rec = (struct LevelRecord *)(gSaveBank + 0x9C4) + i * 3;
                for (j = 0; j <= 2; j++)
                {
                    if (rec[j].time == gLevelTimes[i][2] && strcmp(rec[j].name, gDefaultRecordNames[2]) == 0)
                        break;
                    if (rec[j].time > gLevelTimes[i][2])
                    {
                        for (k = 2; k > j; k--)
                        {
                            rec[k].time = rec[k - 1].time;
                            strcpy(rec[k].name, rec[k - 1].name);
                            rec[k].rank = rec[k - 1].rank;
                        }
                        rec[j].time = gLevelTimes[i][2];
                        strcpy(rec[j].name, gDefaultRecordNames[2]);
                        rec[j].rank = 3;
                        break;
                    }
                }
            }
        }
        Save_Write();
        Main_EndFrame();
    }
}
/**
 * Moves the camera origin (gCameraX, gCameraY): normally centred on the current
 * Helirin (120, 80 pixels up-left of it); with gGameState.flags bit 0 or the Helirin's flags
 * bit 30 it is moved by the D-pad instead, 2 pixels a frame (4 with A, doubled with B),
 * unless Select was just pressed or it is a four-player game with gGameState.paused set.
 * Then clamps it to the camera bounds and scrolls maps 1-3 to it (Bg_ScrollMapClamped /
 * Bg_ScrollMap), map 3 by world: at 3/4 (plus two drifting offsets in world 6) or 1/2 speed.
 */
void Camera_Update(void)
{
    struct Camera *cam;

    if ((gGameState.flags & 1 || gGameState.helirin->flags & 0x40000000)
        && !(gMain.newKeys & 4)
        && !((s8)gGameState.mode == 4 && gGameState.paused != 0))
    {
        u32 held = gMain.heldKeys;
        s32 step = 2;

        if (held & A_BUTTON)
            step = 4;
        if (held & B_BUTTON)
            step <<= 1;
        if (held & 0x10)
            gCameraX += step;
        if (held & 0x20)
            gCameraX -= step;
        if (held & 0x80)
            gCameraY += step;
        if (held & 0x40)
            gCameraY -= step;
    }
    else
    {
        gCameraX = (gGameState.helirin->x >> 16) - 120;
        gCameraY = (gGameState.helirin->y >> 16) - 80;
    }

    cam = &gCamera;
    if (gCameraX < cam->boundLeft)
        gCameraX = cam->boundLeft;
    else if (gCameraX > cam->boundRight)
        gCameraX = cam->boundRight;
    if (gCameraY < cam->boundTop)
        gCameraY = cam->boundTop;
    else if (gCameraY > cam->boundBottom)
        gCameraY = cam->boundBottom;

    switch (gGameState.world)
    {
    case 2:
        Bg_ScrollMapClamped(&gMaps[1], gCameraX, gCameraY);
        Bg_ScrollMap(&gMaps[2], gCameraX, gCameraY);
        Bg_ScrollMap(&gMaps[3], gCameraX, gCameraY);
        break;
    case 6:
        Bg_ScrollMapClamped(&gMaps[1], gCameraX, gCameraY);
        Bg_ScrollMapClamped(&gMaps[2], gCameraX, gCameraY);
        Bg_ScrollMap(&gMaps[3], gCameraX * 6 / 8 + gCloudDriftX / 0x10000,
                     gCameraY * 6 / 8 + gCloudDriftY / 0x10000);
        gCloudDriftX += 0x4000;
        gCloudDriftY += 0x2AAA;
        break;
    case 7:
    case 9:
        Bg_ScrollMapClamped(&gMaps[1], gCameraX, gCameraY);
        Bg_ScrollMapClamped(&gMaps[2], gCameraX, gCameraY);
        Bg_ScrollMap(&gMaps[3], gCameraX * 6 / 8, gCameraY * 6 / 8);
        break;
    case 11:
        Bg_ScrollMapClamped(&gMaps[1], gCameraX, gCameraY);
        Bg_ScrollMapClamped(&gMaps[2], gCameraX, gCameraY);
        Bg_ScrollMap(&gMaps[3], gCameraX / 2, gCameraY / 2);
        break;
    default:
        Bg_ScrollMapClamped(&gMaps[1], gCameraX, gCameraY);
        Bg_ScrollMapClamped(&gMaps[2], gCameraX, gCameraY);
        Bg_ScrollMap(&gMaps[3], gCameraX, gCameraY);
        break;
    }
}
/**
 * Enters the level's result in the current save file and its three records (Results_Run):
 * raises the level's mark (marks[]) to 3 (4 without bumps; 2, and nothing else, when the
 * Helirin's size is not 0, or 5 when gGameState.mode is 2), sets the file's flags bit 1
 * (bit 2) once every level 5-0x22 (0x26-0x57) has a mark above 3, then puts gGameState.timer
 * into the records sorted by time and keeps the file's best time of the level. Returns 8
 * for mark 2, else the place in the records (1-3, 0: none) of the file's time, or 4 | the
 * place when it is a new one.
 */
s32 Results_EnterRecord(void)
{
    struct SaveFile *save;
    struct LevelRecord *rec;
    s32 n;
    s32 mark;
    s32 i, j;

    if (gGameState.world == 0)
        n = gGameState.level;
    else
        n = gGameState.world * 3 + gGameState.level + 2;
    save = gGameState.saveFile;
    if ((s8)gGameState.mode == 2)
    {
        rec = (struct LevelRecord *)(gSaveBank + 0x1B34) + n * 3;
        if ((s8)gGameState.helirin->size == 5)
        {
            mark = 3;
            if (gGameState.bumpCount == 0)
                mark = 4;
        }
        else
            mark = 2;
    }
    else
    {
        rec = (struct LevelRecord *)(gSaveBank + 0x9C4) + n * 3;
        if ((s8)gGameState.helirin->size == 0)
        {
            mark = 3;
            if (gGameState.bumpCount == 0)
                mark = 4;
        }
        else
            mark = 2;
    }

    if (save->marks[n] < mark)
        save->marks[n] = mark;
    gGameState.bestTime = save->bestTimes[n];
    if (mark <= 2)
        return 8;

    switch ((s8)gGameState.mode)
    {
    case 1:
    case 2:
        for (i = 5; i <= 0x22; i++)
        {
            if (save->marks[i] <= 3)
                break;
        }
        if (i > 0x22)
            save->flags |= 2;
        break;
    case 3:
        for (i = 0x26; i <= 0x57; i++)
        {
            if (save->marks[i] <= 3)
                break;
        }
        if (i > 0x57)
            save->flags |= 4;
        break;
    }

    // the file's best time is already a record: move it up to its new place
    for (i = 0; i <= 2; i++)
    {
        if (rec[i].time == save->bestTimes[n] && strcmp(rec[i].name, save->name) == 0)
        {
            if (rec[i].time > gGameState.timer)
            {
                for (j = i; j > 0; j--)
                {
                    if (rec[j - 1].time <= gGameState.timer)
                        break;
                    rec[j].time = rec[j - 1].time;
                    strcpy(rec[j].name, rec[j - 1].name);
                    rec[j].rank = rec[j - 1].rank;
                }
                save->bestTimes[n] = rec[j].time = gGameState.timer;
                strcpy(rec[j].name, save->name);
                rec[j].rank = save->rank;
                return ((j + 1) & 3) | 4;
            }
            return (i + 1) & 3;
        }
    }

    for (i = 0; i <= 2; i++)
    {
        if (rec[i].time > gGameState.timer)
        {
            for (j = 2; j > i; j--)
            {
                rec[j].time = rec[j - 1].time;
                strcpy(rec[j].name, rec[j - 1].name);
                rec[j].rank = rec[j - 1].rank;
            }
            rec[i].time = gGameState.timer;
            strcpy(rec[i].name, save->name);
            rec[i].rank = save->rank;
            break;
        }
    }
    if (save->bestTimes[n] > gGameState.timer)
    {
        save->bestTimes[n] = gGameState.timer;
        return ((i + 1) & 3) | 4;
    }
    return (i + 1) & 3;
}
