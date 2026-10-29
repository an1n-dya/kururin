// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// play: main state 3: the menus-to-level loop, a level's loop and its end.
// 08008200-08009438, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "task.h"
#include "util.h"
#include "level.h"
#include "save.h"
#include "pause.h"
#include "results.h"
#include "mode_menu.h"
#include "script.h"
#include "collection.h"
#include "level_intro.h"
#include "challenge.h"
#include "vs.h"
#include "makeup.h"
#include "helirin.h"
#include "hud.h"
#include "effects.h"
#include "sound.h"
#include "practice.h"
#include "play.h"

extern s32 gLevelEndTimer;
s32 Play_CheckLevelEnd(void);
s32 Play_MoveHelirinTo(struct Helirin *h, s32 speed, s32 tx, s32 ty);

/**
 * The game's main state 3 (as long as gMain.state stays 3): a loop over sub-states in
 * gMain.subState. 0/1: the file and mode menus (ModeMenu_Run; 0 the first time), which pick
 * Adventure (1, 2: the map, MapScreen_Run), Challenge (3: Challenge_Run), 4 (Vs_RunMenus),
 * 5 (Practice_Run) or leave (gMain.state 2). Each mode then alternates between its menu and
 * the level (Play_RunLevel, with gGameState.bestTime the level's best record), the result screens
 * (Results_Run, Results_CheckRankUp) after a finished one; Adventure also shows Script_Run and
 * Collection_Run screens, and after level 2 of worlds 1-10 with all ten bonus bits ends in
 * gMain.state 4.
 */
void Play_MainState(void)
{
    struct LevelRecord *rec;
    s32 n;
    s32 result;
    s32 i;

    gMain.subState = 0;
    while (gMain.state == 3)
    {
        switch ((s8)gMain.subState)
        {
        case 0:
        case 1:
            switch (ModeMenu_Run((s8)gMain.subState != 0 ? 1 : 0))
            {
            case 1:
            case 2:
                if (!(gGameState.saveFile->flags & 1))
                {
                    Script_Run(2);
                    gGameState.saveFile->flags |= 1;
                }
                for (i = 0; i <= 0x24; i++)
                {
                    if (gGameState.saveFile->marks[i] == 0)
                        break;
                }
                Level_SetFromIndex(i);
                gMain.subState = 2;
                break;
            case 3:
                gMain.subState = 5;
                break;
            case 4:
                gMain.subState = 8;
                break;
            case 5:
                gMain.subState = 11;
                break;
            default:
                gMain.state = 2;
                break;
            }
            break;
        case 2:
            if (MapScreen_Run(0) != 0)
                gMain.subState = 1;
            else
                gMain.subState = 3;
            break;
        case 3:
            Script_Run(0);
            LevelIntro_Run();
            if (gGameState.saveFile->flags & 0x10)
                rec = (struct LevelRecord *)(gSaveBank + 0x1B34);
            else
                rec = (struct LevelRecord *)(gSaveBank + 0x9C4);
            if (gGameState.world == 0)
                n = gGameState.level;
            else
                n = gGameState.world * 3 + gGameState.level + 2;
            gGameState.bestTime = rec[n * 3].time;
            if (Play_RunLevel() != 0)
                gMain.subState = 4;
            else
                gMain.subState = 2;
            break;
        case 4:
            Results_Run();
            Results_CheckRankUp();
            Script_Run(1);
            Collection_Run(0);
            if (!(gGameState.saveFile->flags & 0x20))
            {
                if (!(gGameState.saveFile->flags & 0x40) && gGameState.world == 10 && gGameState.level == 2
                    && (gGameState.saveFile->unlocks & 0x3FF) != 0x3FF)
                {
                    Script_Run(4);
                    gGameState.saveFile->flags |= 0x40;
                    Save_Write();
                    Collection_Run(3);
                }
                if ((u8)(gGameState.world - 1) <= 9 && gGameState.level == 2
                    && (gGameState.saveFile->unlocks & 0x3FF) == 0x3FF)
                {
                    Script_Run(3);
                    gMain.state = 4;
                    return;
                }
            }
            if (MapScreen_Run(1) != 0)
                gMain.subState = 1;
            else
                gMain.subState = 3;
            break;
        case 5:
            if (Challenge_Run() != 0)
                gMain.subState = 1;
            else
                gMain.subState = 6;
            break;
        case 6:
            rec = (struct LevelRecord *)(gSaveBank + 0x9C4);
            if (gGameState.world == 0)
                n = gGameState.level;
            else
                n = gGameState.world * 3 + gGameState.level + 2;
            gGameState.bestTime = rec[n * 3].time;
            if (Play_RunLevel() != 0)
                gMain.subState = 7;
            else
                gMain.subState = 5;
            break;
        case 7:
            Results_Run();
            Results_CheckRankUp();
            gMain.subState = 5;
            break;
        case 8:
            if (Vs_RunMenus(0) == 0)
                gMain.subState = 9;
            else
                gMain.subState = 1;
            break;
        case 9:
            rec = (struct LevelRecord *)(gSaveBank + 0x9C4);
            if (gGameState.world == 0)
                n = gGameState.level;
            else
                n = gGameState.world * 3 + gGameState.level + 2;
            gGameState.bestTime = rec[n * 3].time;
            result = Play_RunLevel();
            if (result == 0)
                gMain.subState = 8;
            else if (result == 1)
                gMain.subState = 10;
            else
                gMain.subState = 1;
            break;
        case 10:
            result = Vs_RunResults();
            if (result == 1)
            {
                if (Vs_RunMenus(gGameState.unk2D + 1) == 0)
                    gMain.subState = 9;
                else
                    gMain.subState = result;
            }
            else if (result == 2)
            {
                gMain.subState = 8;
            }
            else
            {
                gMain.subState = 1;
            }
            break;
        case 11:
            if (Practice_Run() != 0)
                gMain.subState = 1;
            else
                gMain.subState = 12;
            break;
        case 12:
            if (gGameState.saveFile->flags & 0x10)
                rec = (struct LevelRecord *)(gSaveBank + 0x1B34);
            else
                rec = (struct LevelRecord *)(gSaveBank + 0x9C4);
            if (gGameState.world == 0)
                n = gGameState.level;
            else
                n = gGameState.world * 3 + gGameState.level + 2;
            gGameState.bestTime = rec[n * 3].time;
            if (Play_RunLevel() != 0)
                gMain.subState = 13;
            else
                gMain.subState = 11;
            break;
        case 13:
            gMain.subState = 11;
            break;
        }
        Main_EndFrame();
    }
}
/**
 * A level's loop, one state per frame until -1; returns 0, 1 (Play_CheckLevelEnd returned 1) or -1
 * (state 6 ended). State 0 loads the level (Level_Load; failure returns 0) and sets up
 * the Helirins: the four of a linked game (gGameState.mode == 4) or gHelirins[0], with a
 * second one (gHelirins[1]) in world 0. 1 starts the intro, 2 waits for the linked players,
 * 3 is a frame of play (Pause_Update, Play_CheckLevelEnd, the Helirins' moves and collisions), 4
 * restarts, 5 and 6 end a linked game after 0x78 frames.
 */
s32 Play_RunLevel(void)
{
    struct Helirin *h;
    struct Helirin *other;
    struct Task *task;
    s32 result;
    s32 state;
    s32 countdown; // set only for a linked game, as in the original
    u32 mask;
    u32 bit;
    u32 ready;
    s32 r;
    s32 i;

    state = 0;
    result = 0;
    do
    {
        switch (state)
        {
        case 0:
            Bg_SetVideoMode(1);
            Sprite_InitRotations();
            Task_Init();
            m4aMPlayAllStop();
            Sprite_ClearLists();
            Sprite_BuildOam();
            m4aSoundVSyncOff();
            if (Level_Load() != 0)
            {
                result = 0;
                state = -1;
                break;
            }
            Main_EndFrame();
            Level_StartTasks();
            m4aSoundVSyncOn();
            if ((s8)gGameState.mode == 4)
            {
                gGameState.helirin = &gHelirins[gLinkPlayer];
                Helirin_Init(&gHelirins[0], 0);
                Helirin_Init(&gHelirins[1], 1);
                Helirin_Init(&gHelirins[2], 2);
                Helirin_Init(&gHelirins[3], 3);
                gLinkTimer = 0;
                countdown = 5;
            }
            else
            {
                gGameState.helirin = &gHelirins[0];
                gGameState.playerCount = 1;
                Helirin_Init(&gHelirins[0], 0);
                MakeUp_StartWornItems();
                if (gGameState.world == 0 && (s8)gGameState.mode != 6)
                {
                    Helirin_Init(&gHelirins[1], 1);
                    gHelirins[1].flags |= 0x0A010000;
                    gHelirins[1].size = 0;
                    Helirin_SetSize(&gHelirins[1]);
                }
                // the size the original passes is whatever r1 held (8, from the test)
                if (gGameState.flags & GAME_FLAG_DEMO)
                    Task_Create(Effect_DrawDemoLabel, 8);
            }
            Hud_Init(gGameState.helirin);
            gCameraX = POS_PIXEL(gGameState.helirin->x) - 0x78;
            gCameraY = POS_PIXEL(gGameState.helirin->y) - 0x50;
            for (i = 1; i <= 3; i++)
                Bg_ScrollMap(&gMaps[i], gCameraX, gCameraY);
            gGameState.paused = 0;
            gGameState.pauseRequest = 0;
            gGameState.timer = 0;
            if ((s8)gGameState.mode == 4)
                state = 2;
            else
                state = 1;
            break;
        case 1:
            Sprite_ClearLists();
            Sprite_BuildOam();
            m4aSoundMode(gSongSoundModes[gGameState.song] << 8);
            task = Task_Create(Sound_UpdateDelayedSongOnly, 0xC);
            if (task != NULL)
            {
                task->unk8 = gGameState.song;
                task->unkA = 0x1E;
            }
            state = 3;
            break;
        case 2:
            Sprite_ClearLists();
            if (Link_FindCommand(1, 0x700) != 0 || Link_FindCommand(1, 0x800) != 0)
            {
                gGameState.song = 0xC;
                // the size the original passes is whatever r1 held (0xC, just stored)
                Task_Create(Effect_UpdateLinkCountdown, 0xC);
                state = 3;
            }
            else
            {
                if (gLinkPlayer == 0)
                {
                    ready = gLinkPlayers & ~gUnk_03000DB4;
                    if (Link_FindCommand(ready, 0x600) == (gLinkPlayers & ~gUnk_03000DB4))
                    {
                        if (countdown == 0)
                        {
                            gLinkSendData[0] = 0x700;
                            for (i = 1; i < 8; i++)
                                gLinkSendData[i] = 0;
                        }
                        else
                        {
                            countdown--;
                        }
                    }
                }
                else
                {
                    gLinkSendData[0] = 0x600;
                    for (i = 1; i < 8; i++)
                        gLinkSendData[i] = 0;
                }
                if (Vs_CheckLevelLink(0) < 0)
                    state = 5;
            }
            Sprite_BuildOam();
            break;
        case 3:
            Sprite_ClearLists();
            if ((s8)gGameState.mode == 4 && Vs_CheckLevelLink(0x800) < 0)
            {
                Sprite_BuildOam();
                state = 5;
                break;
            }
            r = Pause_Update();
            if (r == -1)
            {
                result = 0;
                state = -1;
            }
            else if (r == 2)
            {
                state = 0;
            }
            else if (r == 3)
            {
                state = 4;
            }
            r = Play_CheckLevelEnd();
            if (r == -1)
            {
                result = 0;
                state = -1;
            }
            else if (r == 1)
            {
                result = 1;
                state = -1;
            }
            else if (r == 2)
            {
                state = 0;
            }
            Camera_Update();
            if ((s8)gGameState.mode == 4)
            {
                h = gGameState.helirin;
                Helirin_Update(h);
                Task_RunAll();
                Helirin_UpdateCollision(h);
                mask = Link_FindCommand(gLinkPlayers, 0x800);
                for (i = 0, bit = 1; i < 4; i++, bit <<= 1)
                {
                    if (h->player == i)
                    {
                        Helirin_Draw(h);
                    }
                    else if (mask & bit)
                    {
                        // another player's Helirin, as its console sent it
                        other = &gHelirins[i];
                        POS_PIXEL(other->x) = gLinkRecvData[i].unk4;
                        POS_PIXEL(other->y) = gLinkRecvData[i].unk6;
                        other->angle = gLinkRecvData[i].unk8 & 0xFF00;
                        other->progress = gLinkRecvData[i].unk8;
                        other->flags = gLinkRecvData[i].unkA | 0x2000000;
                        Helirin_Draw(other);
                    }
                }
                gLinkSendData[0] = 0x800;
                gLinkSendData[2] = POS_PIXEL(h->x);
                gLinkSendData[3] = POS_PIXEL(h->y);
                gLinkSendData[4] = (h->angle & 0xFF00) | h->progress;
                gLinkSendData[5] = h->flags;
                Hud_DrawLinkProgress();
            }
            else if (gGameState.world == 0 && (s8)gGameState.mode != 6)
            {
                Helirin_Update(&gHelirins[1]);
                Helirin_Update(gGameState.helirin);
                Task_RunAll();
                Helirin_UpdateCollision(&gHelirins[1]);
                Helirin_UpdateCollision(gGameState.helirin);
                Helirin_Draw(&gHelirins[1]);
                if (!(gGameState.helirin->flags & 0x20))
                    Helirin_Draw(gGameState.helirin);
            }
            else
            {
                Helirin_Update(gGameState.helirin);
                Task_RunAll();
                Helirin_UpdateCollision(gGameState.helirin);
                if (!(gGameState.helirin->flags & 0x20))
                    Helirin_Draw(gGameState.helirin);
            }
            gGameState.timer = gGameState.helirin->timer;
            gGameState.bumpCount = gGameState.helirin->bumpCount;
            if (!(gGameState.flags & GAME_FLAG_DEMO))
            {
                Hud_DrawTimes();
                Hud_Draw(gGameState.helirin);
            }
            Sprite_BuildOam();
            break;
        case 4:
            Task_Init();
            Sprite_InitRotations();
            Level_StartTasks();
            gGameState.helirin = &gHelirins[0];
            gGameState.playerCount = 1;
            Helirin_Init(&gHelirins[0], 0);
            gGameState.helirin->flags |= 2;
            MakeUp_StartWornItems();
            // the size the original passes is whatever r1 held (8, from the test)
            if (gGameState.flags & GAME_FLAG_DEMO)
                Task_Create(Effect_DrawDemoLabel, 8);
            gGameState.paused = 0;
            gGameState.pauseRequest = 0;
            state = 3;
            break;
        case 5:
            gLinkTimer = 0;
            state = 6;
            // fallthrough
        case 6:
            if (gLinkTimer++ > 0x78)
            {
                gDispcntCopy |= 0x2100;
                Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
                if (gWindowRadius <= 0)
                {
                    result = -1;
                    state = -1;
                }
            }
            break;
        }
        Main_EndFrame();
    } while (state != -1);
    return result;
}
/**
 * Once per frame of play (Play_RunLevel): the end of a level for the current Helirin. flags
 * bit 6 starts it (fade the music, Bg_StartWindowCircle, a 0x3C- or 0x1E-frame wait in
 * gLevelEndTimer), bit 5 runs it (shrink gWindowRadius through Bg_DrawWindowCircle, or in a linked
 * game, mode 4, move to gCamera.startX/startY[0]), bit 2 and bit 3 handle reaching
 * the goal (move to gCamera.goalX/goalY). Returns -1 or 2 when the level is left, 1 when it is
 * finished, 0 otherwise. Bit 1 without bit 3 counts timer frames (up to 0x34BBF).
 */
s32 Play_CheckLevelEnd(void)
{
    struct Helirin *h = gGameState.helirin;
    struct Camera *cam = &gCamera;
    s32 result = 0;
    s32 n;
    s32 i;
    u32 v;
    u8 lvl;

    if (gGameState.paused != 0)
        return 0;
    if (gLevelEndTimer != 0)
        gLevelEndTimer--;
    if ((h->flags & 1) && (s8)gGameState.mode != 4)
    {
        m4aSongNumStart(0x3D);
        m4aSongNumStart(0x2F);
    }

    if (h->flags & 0x40)
    {
        h->disableFlags |= 3;
        switch ((s8)gGameState.mode)
        {
        case 1:
        case 2:
        case 3:
        case 6:
            gGameState.flags = (gGameState.flags & ~0x80) | 0x40;
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            Bg_StartWindowCircle(0xA0, 0x78, 0x50);
            gLevelEndTimer = 0x3C;
            break;
        case 5:
            gGameState.flags = (gGameState.flags & ~0x80) | 0x40;
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            Bg_StartWindowCircle(0xA0, 0x78, 0x50);
            gLevelEndTimer = 0x1E;
            break;
        case 4:
            h->flags |= 0x2000000;
            if (cam->ccwStarts & 1)
                h->angleDefaultRate = h->angleRate = -0xB6;
            else
                h->angleDefaultRate = h->angleRate = 0xB6;
            break;
        }
    }

    if (h->flags & 0x20)
    {
        switch ((s8)gGameState.mode)
        {
        case 6:
            result = -1;
            break;
        case 1:
        case 2:
        case 3:
            if (gLevelEndTimer == 0)
            {
                Bg_DrawWindowCircle(gWindowRadius - 8, gWindowX, gWindowY);
                if (gWindowRadius <= 0)
                    result = -1;
            }
            break;
        case 5:
            if (gLevelEndTimer == 0)
            {
                Bg_DrawWindowCircle(gWindowRadius - 8, gWindowX, gWindowY);
                if (gWindowRadius <= 0)
                    result = 2;
            }
            break;
        case 4:
            if ((h->flags & 0xA) == 2)
            {
                h->timer++;
                if (h->timer > 0x34BBF)
                    h->timer = 0x34BBF;
            }
            POS_PIXEL(h->x) = Util_StepTowards(POS_PIXEL(h->x), cam->startX[0], 2);
            POS_PIXEL(h->y) = Util_StepTowards(POS_PIXEL(h->y), cam->startY[0], 2);
            h->angle += h->angleRate * 8;
            if ((s8)h->progress != 0)
                h->progress--;
            if (POS_PIXEL(h->x) == cam->startX[0] && POS_PIXEL(h->y) == cam->startY[0])
            {
                Lives_Init(h);
                h->yb = 0;
                h->xb = 0;
                h->flags &= ~0x02000020;
                h->disableFlags &= ~3;
            }
            break;
        }
    }
    else if ((h->flags & 0xA) == 2)
    {
        h->timer++;
        if (h->timer > 0x34BBF)
            h->timer = 0x34BBF;
    }

    if (h->flags & 4)
    {
        switch ((s8)gGameState.mode)
        {
        case 1:
        case 2:
        case 3:
            Task_Create(Effect_UpdateGoalBanner, 0x10);
            Sound_PlaySong(0x10);
            m4aSongNumStart(0x3E);
            h->disableFlags |= 3;
            h->angleDefaultRate *= 8;
            gGameState.flags = (gGameState.flags & ~0x80) | 0x40;
            Bg_StartWindowCircle(0x110, 0x18, 0x87);
            gLevelEndTimer = 0x78;
            break;
        case 4:
            Task_Create(Effect_UpdateGoalBanner, 0x10);
            m4aSongNumStart(0x4E);
            m4aSongNumStart(0x3E);
            h->disableFlags |= 3;
            h->angleDefaultRate *= 8;
            h->flags &= ~0x400;
            break;
        case 5:
            m4aSongNumStart(0x3E);
            h->disableFlags |= 3;
            h->angleDefaultRate *= 8;
            gGameState.flags &= ~0x80;
            break;
        }
    }

    if (h->flags & 8)
    {
        switch ((s8)gGameState.mode)
        {
        case 1:
        case 2:
        case 3:
            Play_MoveHelirinTo(h, 0x300, cam->goalX, cam->goalY);
            if (gLevelEndTimer == 0)
            {
                if (gWindowRadius > 0x10)
                {
                    gWindowRadius -= 4;
                }
                else
                {
                    gWindowRadius = 0x10;
                    result = 1;
                }
                Bg_DrawWindowCircle(gWindowRadius, gWindowX, gWindowY);
            }
            break;
        case 4:
            Play_MoveHelirinTo(h, 0x300, cam->goalX, cam->goalY);
            if (!(gMain.prevKeys & 0xF0))
                h->flags |= 0x40000000;
            n = 0;
            for (i = 0; i < 4; i++)
            {
                if (gHelirins[i].flags & 0x8008)
                    n++;
            }
            if (n >= (s32)gGameState.playerCount)
            {
                if (!(gGameState.flags & 0x40))
                {
                    Bg_StartWindowCircle(0x100, 0x78, 0x50);
                    gLevelEndTimer = 0x78;
                    Sound_PlaySong(0x12);
                    gGameState.flags = (gGameState.flags & ~0x80) | 0x40;
                }
                if (gLevelEndTimer == 0)
                {
                    gWindowRadius -= 4;
                    if (gWindowRadius <= 0)
                    {
                        gWindowRadius = 0;
                        result = 1;
                    }
                    Bg_DrawWindowCircle(gWindowRadius, gWindowX, gWindowY);
                }
            }
            break;
        case 5:
            if (Play_MoveHelirinTo(h, 0x300, cam->goalX, cam->goalY) == 0)
                gGameState.pauseRequest = 1;
            gGameState.flags &= ~0x80;
            break;
        }
    }
    else if ((s8)gGameState.mode == 4)
    {
        if (h->flags & 0x8000)
        {
            if (gLevelEndTimer == 0)
            {
                gWindowRadius -= 4;
                if (gWindowRadius <= 0)
                {
                    h->timer = 0x34BC0;
                    h->disableFlags |= 3;
                    m4aMPlayStop(&gMPlayInfo_SE2);
                    gWindowRadius = 0;
                    result = 1;
                }
                Bg_DrawWindowCircle(gWindowRadius, gWindowX, gWindowY);
            }
        }
        else
        {
            n = 0;
            for (i = 0; i < 4; i++)
            {
                if (gHelirins[i].flags & 8)
                    n++;
            }
            if (n >= (s32)gGameState.playerCount - 1)
            {
                h->flags |= 0x8000;
                Task_Create(Effect_UpdateLinkEndBanner, 0x10);
            }
            else if (!(h->flags & 0x400) && h->timer > 0x2A2F)
            {
                h->flags |= 0x8400;
                Task_Create(Effect_UpdateTimeUpBanner, 0x10);
            }
            else
            {
                goto goal;
            }
            Bg_StartWindowCircle(0x100, 0x78, 0x50);
            gLevelEndTimer = 0x78;
            if (gLinkPlayer == 0 && gGameState.unk2C != 0)
                Sound_PlaySong(0x12);
            else
                Sound_PlaySong(0x13);
            gGameState.flags = (gGameState.flags & ~0x80) | 0x40;
        }
    }

goal:
    if (gGameState.flags & 4)
    {
        v = h->flags;
        lvl = gGameState.area;
        if ((v & 8) || (lvl == 0 && (v & 0x80000)) || (lvl == 1 && (v & 0x200000))
            || (lvl == 2 && (v & 0x800000)))
        {
            h->disableFlags |= 2;
            gGameState.pauseRequest = 1;
        }
    }

    if (gGameState.flags & GAME_FLAG_DEMO)
    {
        if (h->flags & 8)
            n = Play_MoveHelirinTo(h, 0x300, cam->goalX, cam->goalY);
        else if (gGameState.area == 0 && (h->flags & 0x100000))
            n = Play_MoveHelirinTo(h, h->speedNorm, cam->startX[1], cam->startY[1]);
        else if (gGameState.area <= 1 && (h->flags & 0x400000))
            n = Play_MoveHelirinTo(h, h->speedNorm, cam->startX[2], cam->startY[2]);
        else if (gGameState.area <= 2 && (h->flags & 0x1000000))
            n = Play_MoveHelirinTo(h, h->speedNorm, cam->startX[3], cam->startY[3]);
        else
            return result;
        if (n == 0)
            gGameState.pauseRequest = 1;
        gGameState.flags &= ~0x80;
        h->disableFlags |= 2;
    }
    return result;
}
/**
 * Moves the Helirin towards pixel (*tx*, *ty*) by *speed* (0: 0x180) along the direction
 * Util_GetDirection gives for the distance, through the tables gSineTable (x) and
 * gCosineTable (y, subtracted). Within reach it snaps to the target and returns 0;
 * otherwise it returns 1.
 */
s32 Play_MoveHelirinTo(struct Helirin *h, s32 speed, s32 tx, s32 ty)
{
    s32 dx = tx - (s16)(h->x >> 16);
    s32 dy = ty - (s16)(h->y >> 16);
    u8 dir;

    if (speed == 0)
        speed = 0x180;
    if (dx * dx + dy * dy > (speed * speed) >> 16)
    {
        dir = Util_GetDirection(dx, dy);
        h->x += gSineTable[dir] * speed;
        h->y -= gCosineTable[dir] * speed;
        return 1;
    }
    h->x = (u32)tx << 16;
    h->y = (u32)ty << 16;
    return 0;
}
