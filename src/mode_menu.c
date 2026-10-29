// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// mode_menu: the mode menu (and the MultiBoot send), then the Adventure map screen it leads to,
// its level menu and map tasks.
// 0800C75C-0800E560, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "link.h"
#include "decompress.h"
#include "task.h"
#include "util.h"
#include "menu.h"
#include "level.h"
#include "intro.h"
#include "mode_menu.h"
#include "vs.h"
#include "makeup.h"
#include "sound.h"

// gWorldIconFrames: {offset into gIconTiles, ?}, two entries per world (MapScreen_UpdateWorldIcon).
struct WorldIconFrame {
    /*0x00*/ u32 offset;
    /*0x04*/ u32 unk4;
};
extern const struct WorldIconFrame gWorldIconFrames[];
extern const u8 gIconTiles[];
extern const struct WorldIconFrame gModeMenuCursorFrames[6]; // gWorldIconFrames[23]: the frames of ModeMenu_DrawCursor
extern u16 gModeMenuCursorFrame;
extern const u8 gModeMenuCursorPositions[][2];
extern s32 gModeMenuDescriptionItem;
extern const void *const gModeMenuDescriptionTiles[];
// The task MapScreen_Run starts with MapScreen_UpdateCursor (0x1C bytes).
struct MapCursor {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ s16 y;
    /*0x0C*/ s16 screen[2];
    /*0x10*/ struct LongAnim anim; // value: the tiles to copy
};
extern const struct LongAnimFrame gMapCursorAnim[];
extern const u8 gTextTop3[];
// The task MapScreen_Run starts with MapScreen_UpdateSlidingCursor (0x20 bytes): MapScreen_UpdateCursor's marker that
// slides from level number unk1C to the current one in 60 frames (unk1E counts them).
struct MapSlidingCursor {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ s16 y;
    /*0x0C*/ s16 screen[2];
    /*0x10*/ struct LongAnim anim; // value: the tiles to copy
    /*0x1C*/ s16 unk1C;
    /*0x1E*/ s16 unk1E;
};
struct MapSlidingCursor;
void MapScreen_UpdateSlidingCursor(struct MapSlidingCursor *t);
extern const u8 gMultiBootLabelTiles[];
extern const u8 *const gUnk_0801E82C[];
void ModeMenu_DrawDescription(s32 n);
void ModeMenu_DrawCursor(u16 n);
void ModeMenu_DrawMultiBootLabel(s32 palette);
void sub_0801A250(u32 a, const u8 *data);
s32 sub_0801A2E8(s32 a);
// The task MapScreen_Run starts with MapScreen_UpdatePath (0x10 bytes): x/y step along the map's
// path, screen from Util_GetScreenPos.
struct MapPath {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ s16 y;
    /*0x0C*/ s16 screen[2];
};

/**
 * The mode menu (Play_MainState), with its own frame loop over the states in *state*: *a* = 0
 * enters at the menu (state 2), else the screen first opens (0, 1). The menu (Menu_MoveLeftRightWrap,
 * items 1-5 drawn by ModeMenu_DrawCursor/ModeMenu_DrawDescription) waits for a link partner in states 3-4
 * (gLinkPlayer the player number from SIOCNT) or, in 5-6, sends the MultiBoot image
 * gMultiBootProgram (states 12-13, then the transfer of states 15-16 and the wait in 14).
 * A on item 1 picks mode 5 (only once the save file's level 4 has a mark), 2 Adventure (1,
 * or 2 for Easy: asked once in states 7-8), 3 Challenge (3), 4 the linked game (4, when a
 * partner answers), 5 the Make Up screen (MakeUp_Run). B returns -1; after a mode the screen
 * closes (10, 11) and it returns the mode.
 */
s32 ModeMenu_Run(s32 a)
{
    struct Menu menu;
    struct Menu easyMenu;
    s32 state;
    s32 result = 0;
    s32 cursor;
    bool32 linked;
    bool32 hasMark;
    u32 sub;
    const u8 *data;
    u8 retries;
    s32 sent;
    s32 i;

    state = 0;
    if (a == 0)
        state = 2;
    ModeMenu_DrawDescription(-1);
    while (state != -1)
    {
        switch (state)
        {
        case 0:
            Bg_SetVideoMode(4);
            Sprite_ClearLists();
            Sprite_BuildOam();
            Main_QueueCopy(gMenuObjPalettes, (void *)0x05000200, 0x1C0);
            Main_QueueCopy(gMenuObjTiles, (void *)0x06012400, 0x4680);
            Bg_LoadScreen(2);
            gDispcntCopy |= 0x1600;
            Main_EndFrame();
            Bg_StartWindowCircle(0, 0x78, 0x50);
            Task_Init();
            Intro_StartBackgroundTasks();
            Sprite_InitRotations();
            m4aSongNumStart(0x29);
            state = 1;
            break;
        case 1:
            Bg_DrawWindowCircle(gWindowRadius + 8, gWindowX, gWindowY);
            if (gWindowRadius > 0x9F)
            {
                gDispcntCopy &= 0xDEFF;
                REG_IE &= 0xF0FB;
                Sound_PlaySongOrContinue(0xE);
                state = 2;
            }
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 2:
            Sprite_ClearLists();
            Sprite_BuildOam();
            Bg_LoadScreen(7);
            menu.itemCount = 5;
            menu.current = 2;
            cursor = 2;
            hasMark = gGameState.saveFile->marks[4] != 0;
            Sound_PlaySongOrContinue(0xE);
            state = 3;
            break;

        case 3:
            Main_InstallLinkFrameCallback();
            REG_IE &= 0xFF3F;
            gLinkStatus = 0;
            Link_Init(0);
            gLinkPlayers = 0xF;
            gLinkWaitFrames = 0;
            gLinkSendData[0] = 0x100;
            for (i = 1; i < 8; i++)
                gLinkSendData[i] = 0;
            state = 4;
            // fall through
        case 4:
            Sprite_ClearLists();
            gLinkPlayer = (*(vu32 *)REG_ADDR_SIOCNT >> 4) & 3;
            gUnk_03000DB4 = 1 << gLinkPlayer;
            switch (Link_FindCommand(0xF, 0x100))
            {
            case 3:
            case 7:
            case 0xF:
                gLinkWaitFrames = 0;
                linked = TRUE;
                Main_QueueCopy(gUnk_080EA560, (void *)0x05000000, 0x20);
                break;
            default:
                if (++gLinkWaitFrames > 0x3C)
                    state = 5;
                linked = FALSE;
                Main_QueueCopy(gUnk_080F661A, (void *)0x05000002, 0x1E);
                break;
            }
            Menu_MoveLeftRightWrap(&menu, gMain.newKeys);
            cursor = menu.current;
            ModeMenu_DrawCursor(cursor);
            if (Link_FindCommandExact(1, 0x100, 2) != 0 && linked)
            {
                // the partner started the linked game
                gLinkPlayers = gLinkRecvData[0].unk2;
                gGameState.unk2C = gLinkRecvData[0].unk4;
                gLivesSettings.linkLives[gLinkPlayer] = (u8)gLinkRecvData[0].unk4 != 0 ? 3 : 1;
                gLivesSettings.linkSizes[gLinkPlayer] = 0;
                result = 4;
                state = 10;
            }
            else if (gLinkPlayer == 0 && Link_FindCommandExact(gLinkPlayers, 0x100, 1) != 0 && linked)
            {
                gLinkSendData[0] = 0x102;
                gLinkSendData[1] = Link_FindCommand(gLinkPlayers, 0x100);
                gLinkSendData[2] = Link_FindCommandExact(gLinkPlayers, 0x100, 0xFF);
                for (i = 3; i < 8; i++)
                    gLinkSendData[i] = 0;
            }
            goto keys;

        case 5:
            REG_IE &= 0xFF3F;
            Main_InstallFrameCallback();
            gMultiBootImage = gMultiBootProgram;
            gMultiBootImageSize = gUnk_080E3B60 - gMultiBootProgram;
            gMultiBootParam.masterp = gMultiBootProgram;
            gMultiBootParam.server_type = 1;
            MultiBootInit(&gMultiBootParam);
            gLinkWaitFrames = 0;
            state = 6;
            // fall through
        case 6:
            Sprite_ClearLists();
            MultiBootMain(&gMultiBootParam);
            if (gMultiBootParam.probe_count == 0 && gMultiBootParam.client_bit != 0
                && ((gMultiBootParam.client_bit & 0xF) == 2 || (gMultiBootParam.client_bit & 0xF) == 6
                    || (gMultiBootParam.client_bit & 0xF) == 0xE))
            {
                gLinkWaitFrames = 0;
                linked = TRUE;
                Main_QueueCopy(gUnk_080EA560, (void *)0x05000000, 0x20);
            }
            else
            {
                if (++gLinkWaitFrames > 0x3C)
                    state = 3;
                linked = FALSE;
                Main_QueueCopy(gUnk_080F661A, (void *)0x05000002, 0x1E);
            }
            Menu_MoveLeftRightWrap(&menu, gMain.newKeys);
            cursor = menu.current;
            ModeMenu_DrawCursor(cursor);
        keys:
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                for (i = 0; i < 8; i++)
                    gLinkSendData[i] = 0;
                result = -1;
                state = -1;
            }
            else if (gMain.newKeys & A_BUTTON)
            {
                switch (menu.current)
                {
                case 1:
                    if (!hasMark)
                    {
                        m4aSongNumStart(0x36);
                        break;
                    }
                    m4aSongNumStart(0x21);
                    for (i = 0; i < 8; i++)
                        gLinkSendData[i] = 0;
                    gLinkPlayer = 0;
                    gUnk_03000DB4 = 1;
                    result = 5;
                    break;
                case 2:
                    m4aSongNumStart(0x21);
                    for (i = 0; i < 8; i++)
                        gLinkSendData[i] = 0;
                    gLinkPlayer = 0;
                    gUnk_03000DB4 = 1;
                    state = 7;
                    result = -1;
                    break;
                case 3:
                    m4aSongNumStart(0x21);
                    for (i = 0; i < 8; i++)
                        gLinkSendData[i] = 0;
                    gLinkPlayer = 0;
                    gUnk_03000DB4 = 1;
                    result = 3;
                    break;
                case 4:
                    if (!linked)
                    {
                        m4aSongNumStart(0x36);
                        break;
                    }
                    m4aSongNumStart(0x21);
                    if (state == 4)
                    {
                        gLinkSendData[0] = 0x101;
                        for (i = 1; i < 8; i++)
                            gLinkSendData[i] = 0;
                    }
                    else if (state == 6)
                    {
                        Main_QueueCopy(gMultiBootLabelTiles, (void *)0x06017A00, 0x500);
                        ModeMenu_DrawMultiBootLabel(0xE);
                        state = 12;
                    }
                    break;
                case 5:
                    m4aSongNumStart(0x21);
                    REG_IE &= 0xFF3F;
                    for (i = 0; i < 8; i++)
                        gLinkSendData[i] = 0;
                    gLinkPlayer = 0;
                    gUnk_03000DB4 = 1;
                    MakeUp_Run();
                    Main_QueueCopy(gMenuObjPalettes, (void *)0x05000200, 0x1C0);
                    Task_Init();
                    Intro_StartBackgroundTasks();
                    state = 2;
                    break;
                }
            }
            if (result > 0)
            {
                gGameState.mode = result;
                state = 10;
            }
            ModeMenu_DrawDescription(cursor);
            Task_RunAll();
            Sprite_BuildOam();
            break;

        case 12:
            gLinkPlayer = (*(vu32 *)REG_ADDR_SIOCNT >> 4) & 3;
            gUnk_03000DB4 = 1 << gLinkPlayer;
            gLinkPlayers = (gMultiBootParam.client_bit | 1) & 0xF;
            MultiBootStartMaster(&gMultiBootParam, gMultiBootImage + 0xC0, gMultiBootImageSize - 0xC0, 6, 1);
            m4aMPlayAllStop();
            m4aSoundVSyncOff();
            REG_IE &= 0xF0FB;
            gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1100;
            state = 13;
            // fall through
        case 13:
            if (MultiBootMain(&gMultiBootParam) != 0)
            {
                m4aSoundVSyncOn();
                Sound_PlaySong(0xE);
                gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1600;
                state = 3;
            }
            else if (MultiBootCheckComplete(&gMultiBootParam) != 0)
            {
                state = 15;
            }
            break;

        case 15:
            // the transfer to the booted partners, with its own frame loop
            sub = 0;
            data = NULL;
            retries = 60;
            ((u8 *)gLinkSendData)[5] = 0;
            sent = 0;
            while (state == 15)
            {
                switch (sub)
                {
                case 0:
                case 1:
                    gIntrTable[1] = (void (*)(void))0x03003EF0;
                    Link_Init((gLinkStatus & 0xF00) >> 8);
                    sent = 0;
                    gLinkStatus = 0;
                    if (sub == 0)
                        sub = 3;
                    else
                        sub = 2;
                    break;
                case 2:
                    if (gLinkStatus & 0x10)
                    {
                        if (gLinkStatus & 0x40)
                            ((u8 *)gLinkSendData)[5]++;
                        sub = 3;
                    }
                    // fall through
                case 3:
                    if (gLinkStatus & 0x20)
                    {
                        if ((gLinkStatus & 0x80) && (gLinkRecvData[0].unk4 >> 8) > 3 && retries != 0)
                            retries--;
                        else
                            sub = 4;
                    }
                    if (gLinkStatus & 0x10)
                        sent = 1;
                    gLinkStatus = Link_Exchange(gLinkSendData, gLinkRecvData, sent);
                    data = gUnk_0801E82C[gLinkRecvData[0].unk4 >> 8];
                    break;
                case 4:
                    if ((gLinkRecvData[0].unk4 >> 8) == 4)
                    {
                        state = 16;
                    }
                    else
                    {
                        REG_IE &= 0xFF3F;
                        gIntrTable[1] = (void (*)(void))0x03003EF1;
                        sub_0801A250(gLinkStatus & 0x80, data);
                        sub = 5;
                    }
                    break;
                case 5:
                    if (sub_0801A2E8(0) != 0)
                        sub = 1;
                    break;
                }
                Sprite_ClearLists();
                if (gMain.frameCount & 8)
                    ModeMenu_DrawMultiBootLabel(0xE);
                else
                    ModeMenu_DrawMultiBootLabel(0xF);
                Sprite_BuildOam();
                Main_EndFrame();
            }
            // fall through
        case 16:
            Main_InstallLinkFrameCallback();
            REG_IE &= 0xFF3F;
            gLinkStatus = 0;
            Link_Init(0);
            gLinkTimer = 0;
            gLinkWaitFrames = 0;
            gLinkSendData[0] = 0x100;
            for (i = 1; i < 8; i++)
                gLinkSendData[i] = 0;
            m4aSoundVSyncOn();
            m4aSongNumStart(0x21);
            gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x100;
            state = 14;
            break;
        case 14:
            if (Vs_WaitForConsoles(0x100) < 0)
            {
                Sound_PlaySong(0xE);
                state = 3;
            }
            else if (Link_FindCommandExact(1, 0x100, 2) != 0)
            {
                gLinkPlayers = gLinkRecvData[0].unk2;
                gGameState.unk2C = gLinkRecvData[0].unk4;
                gLivesSettings.linkLives[gLinkPlayer] = (u8)gLinkRecvData[0].unk4 != 0 ? 3 : 1;
                gLivesSettings.linkSizes[gLinkPlayer] = 0;
                result = 4;
                m4aMPlayAllStop();
                Sprite_ClearLists();
                Sprite_BuildOam();
                state = -1;
            }
            else if (gLinkTimer == 0 && (gLinkStatus & (gLinkPlayers & 0xF)) == gLinkPlayers)
            {
                gLinkSendData[0] = 0x102;
                gLinkSendData[1] = gLinkPlayers;
                gLinkSendData[2] = gLinkPlayers & 0xFFFE;
                for (i = 3; i < 8; i++)
                    gLinkSendData[i] = 0;
            }
            break;

        case 7:
            REG_IE &= 0xFF3F;
            if (gGameState.saveFile->flags & 8)
            {
                if (gGameState.saveFile->flags & 0x10)
                {
                    result = 2;
                    gGameState.mode = 2;
                }
                else
                {
                    result = 1;
                    gGameState.mode = 1;
                }
                state = 10;
            }
            else
            {
                Bg_LoadScreen(10);
                easyMenu.itemCount = 2;
                easyMenu.current = 1;
                state = 8;
            }
            break;
        case 8:
            Sprite_ClearLists();
            Menu_MoveLeftRightWrap(&easyMenu, gMain.newKeys);
            cursor = easyMenu.current + 5;
            ModeMenu_DrawCursor(cursor);
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                result = 0;
                state = 2;
            }
            if (gMain.newKeys & A_BUTTON)
            {
                m4aSongNumStart(0x21);
                if (easyMenu.current == 1)
                {
                    gGameState.saveFile->flags |= 8;
                    result = 1;
                    gGameState.mode = 1;
                }
                else
                {
                    gGameState.saveFile->flags |= 0x18;
                    result = 2;
                    gGameState.mode = 2;
                }
                state = 10;
            }
            ModeMenu_DrawDescription(cursor);
            Task_RunAll();
            Sprite_BuildOam();
            break;

        case 10:
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            Bg_StartWindowCircle(0xA0, 0x78, 0x50);
            state = 11;
            break;
        case 11:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            Sprite_ClearLists();
            if (gWindowRadius <= 0)
            {
                m4aMPlayAllStop();
                state = -1;
            }
            else
            {
                ModeMenu_DrawCursor(cursor);
                ModeMenu_DrawDescription(cursor);
                Task_RunAll();
            }
            Sprite_BuildOam();
            break;
        }
        Main_EndFrame();
    }

    if (result != 4)
        REG_IE &= 0xFF3F;
    Main_InstallLinkFrameCallback();
    return result;
}
/**
 * The mode menu's cursor (ModeMenu_Run) at item *item* (1-7, by gModeMenuCursorPositions): a 64x64
 * sprite whose tiles go through the six frames of gModeMenuCursorFrames (gModeMenuCursorFrame advances
 * 0x40 a frame, 0x100 per frame).
 */
void ModeMenu_DrawCursor(u16 item)
{
    struct Sprite *spr;

    gModeMenuCursorFrame += 0x40;
    if (gModeMenuCursorFrame > 0x5FF)
        gModeMenuCursorFrame %= 0x600;
    Main_QueueCopy(gIconTiles + gModeMenuCursorFrames[gModeMenuCursorFrame >> 8].offset, (void *)0x06016A80, 0x800);
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = (((gModeMenuCursorPositions[item - 1][0] - 0x20) << 16) & 0x1FF0000) | 0xC0000000
                  | (u8)(gModeMenuCursorPositions[item - 1][1] - 0x20);
        spr->attr2 = 0x754;
    }
}
/**
 * The mode menu's description of item *item* (ModeMenu_Run): copies its 0x780 bytes of OBJ
 * tiles (gModeMenuDescriptionTiles[item - 1]) to 0x06017280 when the item changed (gModeMenuDescriptionItem) and
 * draws them as 5 rows of three 32x8 sprites at (0x48, 0x70). A negative *item* only resets
 * gModeMenuDescriptionItem, so the next call copies again.
 */
void ModeMenu_DrawDescription(s32 item)
{
    struct Sprite *spr;
    s32 row, col;
    u16 tile;
    u32 x;

    if (item < 0)
    {
        gModeMenuDescriptionItem = item;
        return;
    }
    if (gModeMenuDescriptionItem != item)
    {
        gModeMenuDescriptionItem = item;
        Main_QueueCopy(gModeMenuDescriptionTiles[item - 1], (void *)0x06017280, 0x780);
    }
    tile = 0x394;
    for (row = 0; row < 5; row++)
    {
        x = 0x48 << 16;
        for (col = 0; col < 3; col++)
        {
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = (x & 0x1FF0000) | 0x40004000 | (u8)(row * 8 + 0x70);
                spr->attr2 = tile | 0xA400;
            }
            tile += 4;
            x += 0x20 << 16;
        }
    }
}
/**
 * Five 32x16 sprites in list 0 with palette *palette* (ModeMenu_Run, while it sends the
 * MultiBoot image; 14 and 15 make it blink): tiles 0x3D0, 0x3D8 and 0x3E0 at x 0x50, 0x70,
 * 0x90, y 0x38, tiles 0x3E8 and 0x3F0 at x 0x58, 0x78, y 0x50 (the 0x500 bytes of
 * gMultiBootLabelTiles copied to 0x06017A00).
 */
void ModeMenu_DrawMultiBootLabel(s32 palette)
{
    struct Sprite *spr;

    // Each sprite written as the original does: the attribute word, then the tile
    // halfword through a cast (the OR's constant built in one register and copied, as the
    // original has it) and the y byte.
    spr = Sprite_Alloc(0);
    if (spr != NULL)
    {
        spr->attr01 = 0x80504000;
        ((u16 *)spr)[2] = (palette << 12) | 0x3D0;
        ((u8 *)spr)[0] = 0x38;
    }
    spr = Sprite_Alloc(0);
    if (spr != NULL)
    {
        spr->attr01 = 0x80704000;
        ((u16 *)spr)[2] = (palette << 12) | 0x3D8;
        ((u8 *)spr)[0] = 0x38;
    }
    spr = Sprite_Alloc(0);
    if (spr != NULL)
    {
        spr->attr01 = 0x80904000;
        ((u16 *)spr)[2] = (palette << 12) | 0x3E0;
        ((u8 *)spr)[0] = 0x38;
    }
    spr = Sprite_Alloc(0);
    if (spr != NULL)
    {
        spr->attr01 = 0x80584000;
        ((u16 *)spr)[2] = (palette << 12) | 0x3E8;
        ((u8 *)spr)[0] = 0x50;
    }
    spr = Sprite_Alloc(0);
    if (spr != NULL)
    {
        spr->attr01 = 0x80784000;
        ((u16 *)spr)[2] = (palette << 12) | 0x3F0;
        ((u8 *)spr)[0] = 0x50;
    }
}
/**
 * The uncompressed size of the current level's level data (KuruTools LevelEntry, layer 0).
 */
u32 MapScreen_GetLevelDataSize(void)
{
    return gWorldEntries[gGameState.world].info->levels[gGameState.level].layers[0].size;
}
/**
 * Shows the window of an 8bpp picture made of *map*'s width of 0x40-byte tiles (*src*) at
 * pixel (*x*, *y*): the tile rows from (x / 8, y / 8), 30 x 20 tiles (one more each way
 * when x or y is not a multiple of 8), go to the layer's tiles (map->layer's charVram, 31
 * tiles a row), and the layer is scrolled by x % 8 and y % 8.
 */
void MapScreen_ShowPicture(struct Map *map, const u8 *src, u16 x, u16 y)
{
    struct BgLayer *layer = map->layer;
    u16 width = map->width;
    u8 *dst = layer->charVram;
    s32 cols;
    s32 rows;

    src += (y >> 3) * width * 0x40 + (x >> 3) * 0x40;
    layer->hofs = x & 7;
    cols = (x & 7) ? 31 : 30;
    layer->vofs = y & 7;
    rows = (y & 7) ? 21 : 20;
    for (; rows > 0; rows--)
    {
        Main_QueueCopy(src, dst, cols * 0x40);
        src += width * 0x40;
        dst += 31 * 0x40;
    }
}
// 8bpp pictures for MapScreen_ShowPicture (their palettes 0x9600 and 0x2A300 / 0x33D00 bytes on)
extern const u8 gTrainingMapPicture[];
extern const u8 gWorldMapPicture[];

/**
 * Shows the picture of item *n* on BG map 2: for 0-4 the 30 x 20-tile gTrainingMapPicture at
 * (0, 0); from 5 on the 0x87-tile-wide gWorldMapPicture scrolled so that gMapLevelPositions[n].x is
 * in the middle (x clamped to 0..0x348), with the palette for main sub-state 11 or another.
 * Does nothing when the camera (gCameraX, gCameraY) is already there.
 */
void MapScreen_ShowMap(s32 n)
{
    s32 x;

    if (n <= 4)
    {
        if (gCameraX == 0 && gCameraY == 0)
            return;
        gCameraX = 0;
        gCameraY = 0;
        gMaps[2].width = 30;
        gMaps[2].height = 20;
        MapScreen_ShowPicture(&gMaps[2], gTrainingMapPicture, 0, 0);
        Main_QueueCopy(gTrainingMapPicture + 0x9600, (void *)0x05000000, 0x1C0);
    }
    else
    {
        x = gMapLevelPositions[n].x - 0x78;
        if (x < 0)
            x = 0;
        else if (x > 0x348)
            x = 0x348;
        if (gCameraX == x && gCameraY == 0)
            return;
        gCameraX = x;
        gCameraY = 0;
        gMaps[2].width = 0x87;
        gMaps[2].height = 20;
        MapScreen_ShowPicture(&gMaps[2], gWorldMapPicture, x, 0);
        if ((s8)gMain.subState == 11)
            Main_QueueCopy(gWorldMapPicture + 0x33D00, (void *)0x05000000, 0x1C0);
        else
            Main_QueueCopy(gWorldMapPicture + 0x2A300, (void *)0x05000000, 0x1C0);
    }
}
/**
 * The Adventure map screen, with its own frame loop on the heap gMain.heap: a menu
 * (MapScreen_LevelMenuItem) whose unk10 is the level number shown (the first one without a mark,
 * unkE, at most 0x22, or 0x25 with the save file's flags bit 1; the current level when it
 * is before that) and the tasks of the map: MapScreen_UpdateCursor, or for *mode* 1 while unkE is
 * new the sliding MapScreen_UpdateSlidingCursor (state 3 then moves unk10 on to the next level with level
 * data, MapScreen_GetLevelDataSize, and waits 90 frames), MapScreen_UpdateWorldIcon and MapScreen_UpdatePath. It shows each
 * level's minimap as Challenge_Run does. A starts the level (one with level data) and
 * returns 0, B returns -1, after the screen closes.
 */
s32 MapScreen_Run(s32 mode)
{
    struct SaveFile *save;
    struct Menu menu;
    const struct TileCopy *e;
    const struct WorldInfo *info;
    const u8 *data;
    const struct LevelEntry *lv;
    u16 *p;
    u8 *buf;
    s32 state = 0;
    s32 result = 0;
    s32 shown;
    s32 timer;
    s32 max;
    s32 n;
    s32 size;
    s32 i, j;
    bool32 draw;

    save = gGameState.saveFile;
    do
    {
        draw = FALSE;
        switch (state)
        {
        case 0:
            Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
            Bg_SetVideoMode(3);
            Sprite_ClearLists();
            Sprite_BuildOam();
            for (e = gMapScreenSpriteTiles; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
            Bg_LoadScreen(0xD);
            Bg_InitMap(&gMaps[2], &gBgLayers[2], NULL, 0x1F, 0x15);
            p = gBgLayers[2].buffer;
            n = 0;
            for (i = 0; i <= 0x14; i++)
            {
                for (j = 0x1E; j >= 0; j--)
                    *p++ = n++;
                p++;
            }
            gBgLayers[2].dirty = 1;
            Main_QueueCopy(gMenuObjPalettes, (void *)0x05000200, 0x1C0);
            gDispcntCopy |= 0x1700;
            gBldcntCopy = 0x3F42;
            menu.func = MapScreen_LevelMenuItem;
            menu.itemCount = 1;
            menu.current = 1;
            menu.x = 0;
            menu.y = 0;
            max = (save->flags & 2) ? 0x25 : 0x22;
            for (i = 0; i < max; i++)
            {
                if (save->marks[i] == 0)
                    break;
            }
            menu.unkE = i;
            if (gGameState.world <= 11
                && i > (gGameState.world == 0 ? gGameState.level
                                              : gGameState.world * 3 + gGameState.level + 2))
            {
                if (gGameState.world == 0)
                    menu.unk10 = gGameState.level;
                else
                    menu.unk10 = gGameState.world * 3 + 2 + gGameState.level;
            }
            else
            {
                menu.unk10 = i;
            }
            gGameState.area = 0;
            gGameState.savedArea = 0;
            gCameraY = -1;
            gCameraX = -1;
            MapScreen_ShowMap(menu.unk10);
            if (save->marks[menu.unkE] > 1 || menu.unkE - 1 > menu.unk10)
                mode = 0;
            Task_Init();
            if (mode == 0)
                Task_Create(MapScreen_UpdateCursor, 0x1C);
            else
                Task_Create((void (*)(struct Task *))MapScreen_UpdateSlidingCursor, 0x20);
            Task_Create(MapScreen_UpdateWorldIcon, 0xC);
            Task_Create(MapScreen_UpdatePath, 0x10);
            shown = -1;
            timer = 0;
            state = 1;
            break;
        case 1:
            Bg_StartWindowCircle(0, 0x78, 0x50);
            m4aSongNumStart(0x29);
            state = 2;
            break;
        case 2:
            Bg_DrawWindowCircle(gWindowRadius + 8, gWindowX, gWindowY);
            if (gWindowRadius > 0xA0)
            {
                gDispcntCopy &= 0xDEFF;
                Sound_PlaySongOrContinue(0xF);
                state = 4;
                if (mode == 1)
                    state = 3;
            }
            draw = TRUE;
            break;
        case 3:
            timer++;
            if (timer == 15)
            {
                // on to the next level number with level data
                for (;;)
                {
                    if (save->flags & 2)
                    {
                        if (menu.unk10 + 1 > 0x25)
                            break;
                    }
                    else if (menu.unk10 + 1 > 0x22)
                    {
                        break;
                    }
                    menu.unk10++;
                    if (MapScreen_GetLevelDataSize() != 0)
                        break;
                }
            }
            else if (timer > 0x5A)
            {
                state = 4;
            }
            draw = TRUE;
            break;
        case 4:
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                result = -1;
                state = 5;
            }
            if ((gMain.newKeys & A_BUTTON) && MapScreen_GetLevelDataSize() != 0)
            {
                m4aSongNumStart(0x21);
                result = 0;
                state = 5;
            }
            draw = TRUE;
            break;
        case 5:
            if (result == 0)
                Bg_LoadScreen(8);
            else
                Bg_LoadScreen(9);
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            gDispcntCopy |= 0x2100;
            state = 6;
            break;
        case 6:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            if (gWindowRadius > 0)
            {
                draw = TRUE;
                break;
            }
            m4aMPlayAllStop();
            Sprite_ClearLists();
            Sprite_BuildOam();
            state = -1;
            break;
        }

        if (draw)
        {
            Sprite_ClearLists();
            Level_SetFromIndex(menu.unk10);
            if (shown != menu.unk10)
            {
                shown = menu.unk10;
                info = gWorldEntries[gGameState.world].info;
                data = gWorldEntries[gGameState.world].data;
                lv = &info->levels[gGameState.level];
                if (lv->layers[3].size != 0)
                {
                    Main_QueueCopy(data + lv->layers[3].offset, (void *)0x060174A0, 0x800);
                }
                else if ((size = lv->layers[0].size) != 0)
                {
                    buf = Heap_Alloc(gMain.heap, size);
                    if (buf != NULL)
                    {
                        Decompress_LZ(data + lv->layers[0].offset, buf, size);
                        Bg_DrawLevelMinimap(buf);
                        Heap_Free(gMain.heap, buf);
                    }
                }
                else
                {
                    DmaFill32(3, 0x11111111, 0x060174A0, 0x800);
                }
            }
            MapScreen_ShowMap(shown);
            if (state == 4)
                Menu_RunWrap(&menu, gMain.newKeys);
            else
                Menu_Draw(&menu);
            Task_RunAll();
            Sprite_BuildOam();
        }
        Main_EndFrame();
    } while (state != -1);
    return result;
}
/**
 * The level menu's item callback (MakeUp_MenuItem's protocol), *p->unk10* being the level:
 * item 1 moves it within 0..p->unkE on gMain.repeatKeys (Menu_StepValue; in mode 5 within
 * 5..unkE); item 0x101, unless Select is held (which clears bit 9 of gDispcntCopy),
 * sets that bit and draws the file's name, its best time for the level (bestTimes), gTextTop3 and
 * the level's three records (gSaveBank + 0x9C4, or + 0x1B34 when the file's flags have bit
 * 4): a record holding the file's own time and name blinks (palette 15 while bit 3 of the
 * frame counter is clear). Returns 0.
 */
s32 MapScreen_LevelMenuItem(struct Menu *p, s32 item)
{
    struct SaveFile *file = gGameState.saveFile;
    struct Sprite *spr;
    struct LevelRecord *rec;
    u32 time;
    s32 i;

    if (item == 1)
    {
        if ((s8)gGameState.mode == 5)
        {
            p->unkE -= 5;
            p->unk10 -= 5;
            Menu_StepValue(&p->unk10, p->unkE, gMain.repeatKeys);
            p->unkE += 5;
            p->unk10 += 5;
        }
        else
        {
            Menu_StepValue(&p->unk10, p->unkE, gMain.repeatKeys);
        }
    }
    else if (item == 0x101)
    {
        if (gMain.heldKeys & SELECT_BUTTON)
        {
            gDispcntCopy &= ~0x200;
            return 0;
        }
        gDispcntCopy |= 0x200;
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = 0xC0080000 | 8;
            spr->attr2 = 0xC3A5;
        }
        gTextCursor.x = 8;
        gTextCursor.y = 0x4A;
        gTextCursor.dx = 8;
        gTextCursor.dy = 9;
        gTextCursor.list = 1;
        gTextCursor.palette = 14;
        Text_DrawSmallName(8, 0x4A, file->name, file->rank, 14);
        gTextCursor.y += gTextCursor.dy;
        Text_DrawSmallTime(gTextCursor.x, gTextCursor.y, file->bestTimes[p->unk10], gTextCursor.palette);
        gTextCursor.y += gTextCursor.dy;
        gTextCursor.palette = 14;
        Text_PrintSmall(&gTextCursor, gTextTop3);
        if (file->flags & 0x10)
            rec = (struct LevelRecord *)(gSaveBank + p->unk10 * 0x30 + 0x1B34);
        else
            rec = (struct LevelRecord *)(gSaveBank + p->unk10 * 0x30 + 0x9C4);
        for (i = 0; i < 3; i++, rec++)
        {
            time = file->bestTimes[p->unk10];
            if (time != 216000 && time == rec->time && strcmp(rec->name, file->name) == 0
             && !(gMain.frameCount & 8))
                gTextCursor.palette = 15;
            else
                gTextCursor.palette = 14;
            Text_DrawSmallName(gTextCursor.x, gTextCursor.y, rec->name, rec->rank, gTextCursor.palette);
            gTextCursor.y += gTextCursor.dy;
            Text_DrawSmallTime(gTextCursor.x, gTextCursor.y, rec->time, gTextCursor.palette);
            gTextCursor.y += gTextCursor.dy;
        }
    }
    return 0;
}
/**
 * The map screen's path (a 0x10-byte task of MapScreen_Run): a marker per level along
 * gTrainingMapPositions (world 0: levels 0-5) or gMapLevelPositions[4..] (the start, then level numbers
 * 5-0x22, or up to 0x25 with the save file's flags bit 1 unless gMain.subState is 0xB), each
 * marker's tile and palette by its marks[] mark (0-1, 2-3 blinking, 4: tile 0x3A1), and three
 * dots between two levels, semi-transparent while the earlier one has no mark.
 */
void MapScreen_UpdatePath(struct Task *task)
{
    struct MapPath *t = (struct MapPath *)task;
    struct SaveFile *save = gGameState.saveFile;
    const struct { u16 x, y; } *pos;
    struct Sprite *spr;
    // Not set for a mark above 4: the original then draws with the values of the marker or
    // dot before (none for the first marker, as here).
    u16 tile;
    u16 palette;
    u16 blink;
    s32 n;
    s32 count;
    s32 dx, dy;
    s32 i;

    switch (t->state)
    {
    case 0:
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    blink = 0x9000;
    if (gMain.frameCount & 8)
        blink = 0x8000;

    if (gGameState.world == 0)
    {
        pos = (const void *)gTrainingMapPositions;
        t->x = pos->x;
        t->y = pos->y;
        if (Util_GetScreenPos(t->x, t->y, t->screen, 8) == 0)
        {
            switch (save->marks[0])
            {
            case 0:
            case 1:
                tile = 0x395;
                palette = 0x4000;
                break;
            case 2:
            case 3:
                tile = 0x395;
                palette = blink;
                break;
            case 4:
                tile = 0x3A1;
                palette = blink;
                break;
            }
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = 0x40000000 | (((t->screen[0] - 8) << 16) & 0x1FF0000) | (u8)(t->screen[1] - 8);
                spr->attr2 = tile | 0x800 | palette;
            }
        }
        pos++;
        n = 1;
        count = 5;
    }
    else
    {
        pos = (const void *)&gMapLevelPositions[4];
        t->x = pos->x;
        t->y = pos->y;
        if (Util_GetScreenPos(t->x, t->y, t->screen, 8) == 0)
        {
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = 0x40000000 | (((t->screen[0] - 8) << 16) & 0x1FF0000) | (u8)(t->screen[1] - 8);
                spr->attr2 = 0x2B99;
            }
        }
        pos++;
        n = 5;
        count = 0x1E;
        if ((s8)gMain.subState != 0xB && (save->flags & 2))
            count = 0x21;
    }

    while (count > 0)
    {
        dx = (pos[0].x - pos[-1].x) / 4;
        dy = (pos[0].y - pos[-1].y) / 4;
        if (save->marks[n - 1] != 0)
        {
            for (i = 2; i >= 0; i--)
            {
                t->x += dx;
                t->y += dy;
                if (Util_GetScreenPos(t->x, t->y, t->screen, 4) == 0)
                {
                    palette = 0x2000;
                    spr = Sprite_Alloc(9);
                    if (spr != NULL)
                    {
                        spr->attr01 = (((t->screen[0] - 4) << 16) & 0x1FF0000) | (u8)(t->screen[1] - 4);
                        spr->attr2 = 0x394 | 0x800 | palette;
                    }
                }
            }
        }
        else
        {
            for (i = 2; i >= 0; i--)
            {
                t->x += dx;
                t->y += dy;
                if (Util_GetScreenPos(t->x, t->y, t->screen, 4) == 0)
                {
                    palette = 0x2000;
                    spr = Sprite_Alloc(9);
                    if (spr != NULL)
                    {
                        spr->attr01 = (((t->screen[0] - 4) << 16) & 0x1FF0000) | 0x400 | (u8)(t->screen[1] - 4);
                        spr->attr2 = 0x394 | 0x800 | palette;
                    }
                }
            }
        }

        t->x = pos->x;
        t->y = pos->y;
        if (Util_GetScreenPos(t->x, t->y, t->screen, 8) == 0)
        {
            switch (save->marks[n])
            {
            case 0:
            case 1:
                tile = 0x395;
                palette = 0x4000;
                break;
            case 2:
            case 3:
                tile = 0x395;
                palette = blink;
                break;
            case 4:
                tile = 0x3A1;
                palette = blink;
                break;
            }
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = 0x40000000 | (((t->screen[0] - 8) << 16) & 0x1FF0000) | (u8)(t->screen[1] - 8);
                spr->attr2 = tile | 0x800 | palette;
            }
        }
        pos++;
        n++;
        count--;
    }
}
/**
 * A task that follows the current level on a map: it takes its position from gTrainingMapPositions
 * (world 0, by level) or gMapLevelPositions (by level number world * 3 + level + 2), plays
 * gMapCursorAnim (Anim_StepLong), copying each frame's 0x400 bytes of tiles to OBJ VRAM
 * 0x06016E80, and draws a 32x64 sprite (tile 0x374, palette 1) above its position when on
 * screen. For worlds 1+ it first draws a 64x32 mark (tile 0x120, palette 0xE) at last
 * frame's screen position, unless Select is held, gGameState.mode is 5, the level number is
 * above 0x22, its bit (number - 5) of gGameState.saveFile->bonusTaken is set or its mark is 0 or 1.
 */
void MapScreen_UpdateCursor(struct Task *task)
{
    struct MapCursor *t = (struct MapCursor *)task;
    struct SaveFile *p;
    struct Sprite *spr;
    s32 n;

    switch ((s16)t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gMapCursorAnim;
        t->anim.value = gMapCursorAnim[0].value;
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.world == 0)
            n = gGameState.level;
        else
            n = gGameState.world * 3 + gGameState.level + 2;
        if (gGameState.world == 0)
        {
            t->x = gTrainingMapPositions[n].x;
            t->y = gTrainingMapPositions[n].y;
        }
        else
        {
            t->x = gMapLevelPositions[n].x;
            t->y = gMapLevelPositions[n].y;
            if (!(gMain.heldKeys & SELECT_BUTTON) && (s8)gGameState.mode != 5 && n <= 0x22)
            {
                p = gGameState.saveFile;
                if (!(p->bonusTaken & (1 << (n - 5))) && p->marks[n] > 1)
                {
                    spr = Sprite_Alloc(6);
                    if (spr != NULL)
                    {
                        spr->attr01 = (((t->screen[0] - 0x20) << 16) & 0x1FF0000) | 0xC0004000 | (u8)t->screen[1];
                        spr->attr2 = 0xE520;
                    }
                }
            }
        }
        if (Anim_StepLong(&t->anim) != 0)
            Main_QueueCopy((const void *)t->anim.value, (void *)0x06016E80, 0x400);
        if (Util_GetScreenPos(t->x, t->y, t->screen, 0x20) == 0)
        {
            spr = Sprite_Alloc(6);
            if (spr != NULL)
            {
                spr->attr01 = (((t->screen[0] - 0x10) << 16) & 0x1FF0000) | 0xC0008000 | (u8)(t->screen[1] - 0x40);
                spr->attr2 = 0x1774;
            }
        }
        break;
    }
}
/**
 * MapScreen_UpdateCursor's level marker on the map screen, moving: it starts at the current level
 * number (world 0: the level; else world * 3 + level + 2) and, once that changes, slides
 * from the old level's position (gTrainingMapPositions / gMapLevelPositions) to the new one in 60 frames,
 * then stays there (state 3, which also draws MapScreen_UpdateCursor's mark).
 */
void MapScreen_UpdateSlidingCursor(struct MapSlidingCursor *t)
{
    const struct { u16 x, y; } *pos;
    struct SaveFile *p;
    struct Sprite *spr;
    s32 from;
    s32 n;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gMapCursorAnim;
        t->anim.value = gMapCursorAnim[0].value;
        if (gGameState.world == 0)
            n = gGameState.level;
        else
            n = gGameState.world * 3 + gGameState.level + 2;
        t->unk1C = n;
        if (gGameState.world == 0)
            pos = (const void *)gTrainingMapPositions;
        else
            pos = (const void *)gMapLevelPositions;
        t->x = pos[n].x;
        t->y = pos[n].y;
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.world == 0)
            n = gGameState.level;
        else
            n = gGameState.world * 3 + gGameState.level + 2;
        if (t->unk1C != n)
        {
            t->unk1E = 0;
            t->state = 2;
        }
        break;
    case 2:
        from = t->unk1C;
        if (gGameState.world == 0)
            n = gGameState.level;
        else
            n = gGameState.world * 3 + gGameState.level + 2;
        if (gGameState.world == 0)
            pos = (const void *)gTrainingMapPositions;
        else
            pos = (const void *)gMapLevelPositions;
        t->x = pos[from].x + t->unk1E * (pos[n].x - pos[from].x) / 60;
        t->y = pos[from].y + t->unk1E * (pos[n].y - pos[from].y) / 60;
        if (++t->unk1E > 59)
            t->state = 3;
        break;
    case 3:
        if (gGameState.world == 0)
            n = gGameState.level;
        else
            n = gGameState.world * 3 + gGameState.level + 2;
        if (gGameState.world == 0)
        {
            t->x = gTrainingMapPositions[n].x;
            t->y = gTrainingMapPositions[n].y;
        }
        else
        {
            t->x = gMapLevelPositions[n].x;
            t->y = gMapLevelPositions[n].y;
            if (!(gMain.heldKeys & SELECT_BUTTON) && (s8)gGameState.mode != 5 && n <= 0x22)
            {
                p = gGameState.saveFile;
                if (!(p->bonusTaken & (1 << (n - 5))) && p->marks[n] > 1)
                {
                    spr = Sprite_Alloc(6);
                    if (spr != NULL)
                    {
                        spr->attr01 = (((t->screen[0] - 0x20) << 16) & 0x1FF0000) | 0xC0004000 | (u8)t->screen[1];
                        spr->attr2 = 0xE520;
                    }
                }
            }
        }
        break;
    default:
        return;
    }

    if (Anim_StepLong(&t->anim) != 0)
        Main_QueueCopy((const void *)t->anim.value, (void *)0x06016E80, 0x400);
    if (Util_GetScreenPos(t->x, t->y, t->screen, 0x20) == 0)
    {
        spr = Sprite_Alloc(6);
        if (spr != NULL)
        {
            spr->attr01 = (((t->screen[0] - 0x10) << 16) & 0x1FF0000) | 0xC0008000 | (u8)(t->screen[1] - 0x40);
            spr->attr2 = 0x1774;
        }
    }
}
/**
 * A task (0x0C bytes) that shows the current world's tiles: unk8 (8.8 fixed point) moves
 * towards gGameState.world * 2 by at most 0x40 a frame (Util_StepTowards), and each time its
 * whole part changes, the 0x400 bytes of gWorldIconFrames[whole part] go to OBJ VRAM
 * 0x06016A80. Every frame it draws one sprite (Sprite_Alloc list 5) at (0xA8, 0x80).
 */
void MapScreen_UpdateWorldIcon(struct Task *task)
{
    u32 old;
    struct Sprite *spr;

    switch ((s16)task->state)
    {
    case 0:
        task->unk8 = gGameState.world << 9;
        Main_QueueCopy(gIconTiles + gWorldIconFrames[gGameState.world * 2].offset, (void *)0x06016A80, 0x400);
        task->state = 1;
        // fall through
    case 1:
        if (task->unk8 != gGameState.world << 9)
        {
            old = task->unk8 >> 8;
            task->unk8 = Util_StepTowards(task->unk8, gGameState.world << 9, 0x40);
            if (task->unk8 >> 8 != old)
                Main_QueueCopy(gIconTiles + gWorldIconFrames[task->unk8 >> 8].offset, (void *)0x06016A80, 0x400);
        }
        spr = Sprite_Alloc(5);
        if (spr != NULL)
        {
            spr->attr01 = 0xC0A84000 | 0x80;
            spr->attr2 = 0xD754;
        }
        break;
    }
}
