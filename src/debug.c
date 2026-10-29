// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// debug: code no state selects: a stage select and a MultiBoot send test.
// 080194D4-08019BA0, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "sprite.h"
#include "text.h"
#include "task.h"
#include "menu.h"
#include "bg.h"
#include "decompress.h"
#include "play.h"

s32 Debug_StageSelectItem(struct Menu *p, s32 item);

/**
 * AgbMain's state function 5 (gMainStates[5]), which no code selects: a debug stage select
 * (docs/unused_content.md) with its menu Debug_StageSelectItem at (0x40, 0x50): the mode
 * (unkE), world (unk10), level (unk12) and area (unk14), from gGameState. Sub-state 0 sets up
 * the screen (video mode 3, the level sprites' palettes and gIntroObjTiles as OBJ tiles, OBJ
 * on) and the menu. Sub-state 1 runs the menu: B leaves to main state 1; A starts the level
 * in mode 6 (the save file 0, flags 0 / 1 / 4 / 8 by mode, the area only for modes 2 and 3)
 * in sub-state 2, which plays it (Play_RunLevel) and comes back. The selected level's
 * minimap is drawn when the level changes: from its level data (Bg_DrawLevelMinimap) or,
 * without level data, an empty one.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
void Debug_StageSelect(void)
{
    struct Menu menu;
    s32 shown; // the level whose minimap is drawn: unk12 in world 0, else unk10 * 3 + unk12 + 2
    s32 n;
    struct Sprite *spr;

    gMain.subState = 0;
    while (gMain.state == 5)
    {
        switch ((s8)gMain.subState)
        {
        case 0:
            gMain.ewramCursor = (u8 *)0x02000000;
            Bg_SetVideoMode(3);
            Sprite_ClearLists();
            Sprite_BuildOam();
            DmaCopy16(3, gLevelSpritePalettes, 0x05000200, 0x200);
            DmaCopy16(3, gIntroObjTiles, 0x06010000, 0x2400);
            gDispcntCopy |= 0x1100;
            menu.func = Debug_StageSelectItem;
            menu.itemCount = 3;
            menu.current = 3;
            menu.x = 0x40;
            menu.y = 0x50;
            menu.unkE = gGameState.flags & 1;
            menu.unk10 = gGameState.world;
            menu.unk12 = gGameState.level;
            menu.unk14 = gGameState.area;
            shown = -1;
            gMain.subState = 1;
            break;
        case 1:
            Sprite_ClearLists();
            Menu_RunWrap(&menu, gMain.newKeys);
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                gMain.state = 1;
            }
            if (gMain.newKeys & A_BUTTON)
            {
                m4aSongNumStart(0x21);
                gGameState.file = 0;
                gGameState.saveFile = (struct SaveFile *)gSaveFiles;
                gGameState.mode = 6;
                gGameState.flags = 0;
                gGameState.world = menu.unk10;
                gGameState.level = menu.unk12;
                switch (menu.unkE)
                {
                case 0: // GAME START
                default:
                    gGameState.area = 0;
                    break;
                case 1: // MAP TEST
                    gGameState.flags = 1;
                    gGameState.area = 0;
                    break;
                case 2: // RECORDING
                    gGameState.flags = 4;
                    gGameState.area = menu.unk14;
                    break;
                case 3: // PLAYBACK
                    gGameState.flags = GAME_FLAG_DEMO;
                    gGameState.area = menu.unk14;
                    break;
                }
                gMain.subState = 2;
            }
            if (menu.unk10 == 0)
                n = menu.unk12;
            else
                n = menu.unk10 * 3 + menu.unk12 + 2;
            if (shown != n)
            {
                const struct WorldEntry *world = &gWorldEntries[menu.unk10];
                const struct LevelEntry *lv = &world->info->levels[menu.unk12];

                if (lv->layers[0].size != 0)
                {
                    Decompress_LZ(world->data + lv->layers[0].offset, gMain.ewramCursor,
                                  lv->layers[0].size);
                    Bg_DrawLevelMinimap(gMain.ewramCursor);
                }
                else
                {
                    DmaFill32(3, 0x11111111, 0x060174A0, 0x800);
                }
                if (menu.unk10 == 0)
                    shown = menu.unk12;
                else
                    shown = menu.unk10 * 3 + menu.unk12 + 2;
            }
            spr = Sprite_Alloc(0);
            if (spr != NULL)
            {
                spr->attr01 = 0xC0080000;
                spr->attr2 = 0xE3A5;
                *(u8 *)&spr->attr01 = 8;
            }
            Sprite_BuildOam();
            break;
        case 2:
            Play_RunLevel();
            gMain.subState = 1;
            break;
        }
        Main_EndFrame();
    }
}
extern const u8 *const gDebugModeNames[];
extern const u8 *const gDebugWorldNames[];
extern const u8 gTextStage[];
extern const u8 gTextArea[];

/**
 * The menu function of Debug_StageSelect (main state 5, a debug stage select): the cursor
 * gTextCursor is colour 15 on the current item, else 14. 0x100 sets it up at the menu's
 * position; 0x101 / 0x102 draw the names gDebugModeNames[unkE] / gDebugWorldNames[unk10];
 * 0x103 gTextStage and unk12 + 1; 0x104 (for unk10 up to 11) gTextArea and unk14;
 * each one row lower. Items 1-4 change them with the repeat keys: unkE 0-3 (Menu_StepValueWrap;
 * first the menu is made 4 items long for the unkE before the step being 2-3, else 3), unk10
 * 0-13 (and unk12
 * back to 0 when it is past the new world's maximum), unk12 up to 4 (unk10 0), 2 (1-11),
 * 0x36 (12) or 0xC7 (Menu_StepValueBy10), unk14 0-3. Returns 0.
 */
s32 Debug_StageSelectItem(struct Menu *p, s32 item)
{
    if (item == p->current + 0x100)
        gTextCursor.palette = 15;
    else
        gTextCursor.palette = 14;

    switch (item)
    {
    case 0x100:
        gTextCursor.x = p->x;
        gTextCursor.y = p->y;
        gTextCursor.dx = 8;
        gTextCursor.dy = 8;
        gTextCursor.palette = 14;
        gTextCursor.list = 0;
        break;
    case 0x101:
        Text_PrintSmall(&gTextCursor, gDebugModeNames[p->unkE]);
        gTextCursor.x = p->x;
        gTextCursor.y += 8;
        break;
    case 1:
        switch (p->unkE)
        {
        case 0:
        case 1:
        default:
            p->itemCount = 3;
            break;
        case 2:
        case 3:
            p->itemCount = 4;
            break;
        }
        Menu_StepValueWrap(&p->unkE, 3, gMain.repeatKeys);
        break;
    case 0x102:
        Text_PrintSmall(&gTextCursor, gDebugWorldNames[p->unk10]);
        gTextCursor.x = p->x;
        gTextCursor.y += 8;
        break;
    case 2:
        Menu_StepValueWrap(&p->unk10, 13, gMain.repeatKeys);
        if (p->unk10 == 0 && p->unk12 > 4)
            p->unk12 = 0;
        else if (p->unk10 <= 11 && p->unk12 > 2)
            p->unk12 = 0;
        else if (p->unk10 == 12 && p->unk12 > 0x36)
            p->unk12 = 0;
        break;
    case 0x103:
        Text_PrintSmall(&gTextCursor, gTextStage);
        Text_DrawSmallNumber(gTextCursor.x + 6, gTextCursor.y, p->unk12 + 1, gTextCursor.palette);
        gTextCursor.x = p->x;
        gTextCursor.y += 8;
        break;
    case 3:
        if (p->unk10 == 0)
            Menu_StepValueBy10(&p->unk12, 4, gMain.repeatKeys);
        else if (p->unk10 <= 11)
            Menu_StepValueBy10(&p->unk12, 2, gMain.repeatKeys);
        else if (p->unk10 == 12)
            Menu_StepValueBy10(&p->unk12, 0x36, gMain.repeatKeys);
        else
            Menu_StepValueBy10(&p->unk12, 0xC7, gMain.repeatKeys);
        break;
    case 0x104:
        if (p->unk10 <= 11)
        {
            Text_PrintSmall(&gTextCursor, gTextArea);
            Text_DrawSmallNumber(gTextCursor.x + 5, gTextCursor.y, p->unk14, gTextCursor.palette);
            gTextCursor.x = p->x;
            gTextCursor.y += 8;
        }
        break;
    case 4:
        if (p->unk10 <= 11)
            Menu_StepValueWrap(&p->unk14, 3, gMain.repeatKeys);
        break;
    }
    return 0;
}
extern s32 gDebugMultiBootResult;
extern u8 gDebugProbeCounts[8];
extern const u8 gDebugMultiBootImage[];
s32 Debug_MultiBootTestItem(struct Menu *p, s32 item);

/**
 * AgbMain's state function 6 (gMainStates[6]), which no code selects: a MultiBoot send
 * test. Sub-state 0 sets up its menu (Debug_MultiBootTestItem at (0x20, 0x68)) and MultiBootInit for
 * the image gDebugMultiBootImage (up to gMultiBootProgram); in sub-state 1 B leaves to main state 1,
 * A or Start, once a client answered and no probe runs, starts the transfer (MultiBoot
 * mode 4) with the sound stopped; every frame runs MultiBootMain, back to state 1 when the
 * transfer completes, else keeps the last 8 probe_count values in gDebugProbeCounts and the
 * last 4 MultiBootMain results in menu.unkC..unk12.
 */
void Debug_MultiBootTest(void)
{
    // The menu, whose unkC onwards keep the last MultiBootMain results: the original's
    // frame is 0x2C bytes, room for 16 of them after the menu's first 0xC bytes.
    union {
        struct Menu menu;
        struct {
            u8 header[0xC];
            u16 results[16];
        } log;
    } m;
    int i;

    Main_InstallFrameCallback();
    gMain.subState = 0;
    while (gMain.state == 6)
    {
        switch ((s8)gMain.subState)
        {
        case 0:
            gMain.ewramCursor = (u8 *)0x02000000;
            Sprite_ClearLists();
            Sprite_BuildOam();
            m.menu.func = Debug_MultiBootTestItem;
            m.menu.itemCount = 1;
            m.menu.current = 1;
            m.menu.x = 0x20;
            m.menu.y = 0x68;
            m.menu.unkC = 0;
            gMultiBootImage = gDebugMultiBootImage;
            gMultiBootImageSize = gMultiBootProgram - gDebugMultiBootImage;
            gMultiBootParam.masterp = gDebugMultiBootImage;
            gMultiBootParam.server_type = 1;
            MultiBootInit(&gMultiBootParam);
            gMain.subState = 1;
            break;
        case 1:
            Sprite_ClearLists();
            Menu_Draw(&m.menu);
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                gMain.state = 1;
            }
            if ((gMain.newKeys & (A_BUTTON | START_BUTTON)) && gMultiBootParam.probe_count == 0
                && gMultiBootParam.client_bit != 0)
            {
                MultiBootStartMaster(&gMultiBootParam, gMultiBootImage + 0xC0, gMultiBootImageSize - 0xC0, 4, 1);
                m4aSongNumStart(0x21);
                m4aMPlayAllStop();
                m4aSoundVSyncOff();
                REG_IE &= 0xF0FB;
            }
            gDebugMultiBootResult = MultiBootMain(&gMultiBootParam);
            if (MultiBootCheckComplete(&gMultiBootParam))
            {
                m4aSoundVSyncOn();
                gMain.state = 1;
                break;
            }
            if (gDebugProbeCounts[0] != gMultiBootParam.probe_count)
            {
                for (i = 7; i > 0; i--)
                    gDebugProbeCounts[i] = gDebugProbeCounts[i - 1];
                gDebugProbeCounts[0] = gMultiBootParam.probe_count;
            }
            if (m.log.results[0] != gDebugMultiBootResult)
            {
                for (i = 3; i > 0; i--)
                    m.log.results[i] = m.log.results[i - 1];
                m.log.results[0] = gDebugMultiBootResult;
            }
            Task_RunAll();
            Sprite_BuildOam();
            break;
        }
        Main_EndFrame();
    }
    Main_InstallLinkFrameCallback();
}
extern const u8 gTextNotConnected[];
extern const u8 gTextChecking[];
extern const u8 gTextReady[];
extern const u8 gTextPushStart[];
extern const u8 gTextNowSending[];
extern const u8 gTextBootChecking[];

/**
 * The menu function of Debug_MultiBootTest (main state 6, the MultiBoot send test): 0x100 sets the
 * cursor gTextCursor up at the menu's position; 0x101 draws a row for each client 1-3 by
 * the MultiBoot parameters gMultiBootParam (gTextNotConnected if its response_bit is clear, else
 * gTextChecking if its client_bit is clear, else gTextReady), then, if any client bit is
 * set, a row by probe_count (0: gTextPushStart, 0xD1: gTextNowSending, above 0xDF:
 * gTextBootChecking). Always returns 0.
 */
s32 Debug_MultiBootTestItem(struct Menu *p, s32 item)
{
    int i;

    switch (item)
    {
    case 0x100:
        gTextCursor.x = p->x;
        gTextCursor.y = p->y;
        gTextCursor.dx = 8;
        gTextCursor.dy = 8;
        gTextCursor.palette = 14;
        gTextCursor.list = 0;
        break;
    case 0x101:
        for (i = 1; i <= 3; i++)
        {
            if (!((gMultiBootParam.response_bit >> i) & 1))
                Text_PrintSmall(&gTextCursor, gTextNotConnected);
            else if (!((gMultiBootParam.client_bit >> i) & 1))
                Text_PrintSmall(&gTextCursor, gTextChecking);
            else
                Text_PrintSmall(&gTextCursor, gTextReady);
            gTextCursor.y += gTextCursor.dy;
            gTextCursor.x = p->x;
        }
        if (gMultiBootParam.client_bit & 0xE)
        {
            switch (gMultiBootParam.probe_count)
            {
            case 0:
                Text_PrintSmall(&gTextCursor, gTextPushStart);
                break;
            case 0xD1:
                Text_PrintSmall(&gTextCursor, gTextNowSending);
                break;
            default:
                if (gMultiBootParam.probe_count > 0xDF)
                    Text_PrintSmall(&gTextCursor, gTextBootChecking);
                break;
            }
        }
        gTextCursor.y += gTextCursor.dy;
        gTextCursor.x = p->x;
        break;
    }
    return 0;
}
