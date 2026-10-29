// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// pause: the pause menu and its items.
// 08009438-0800AA0C, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "sprite.h"
#include "text.h"
#include "util.h"
#include "menu.h"
#include "save.h"
#include "pause.h"
#include "helirin.h"
#include "sound.h"

// 0x030043F0: the menu Pause_Update runs with Menu_RunWrap (its func/unkC are set there),
// followed by a list of OBJ VRAM copies at +0x1C.
struct PauseMenu {
    /*0x00*/ struct Menu menu;
    /*0x18*/ u32 unk18;
    /*0x1C*/ const struct TileCopy *tiles; // the items' OBJ tiles: copied back when See Map / Make Up end
};
extern struct PauseMenu gPauseMenu;
extern const u8 gSeeMapArrowTiles[];
s32 Pause_MenuItem(struct Menu *p, s32 item);
s32 Pause_DemoMenuItem(struct Menu *p, s32 item);
s32 Pause_PracticeMenuItem(struct Menu *p, s32 item);
s32 Pause_LinkMenuItem(struct Menu *p, s32 item);
void Pause_SeeMap(void);
void Pause_MakeUp(void);
extern const struct TileCopy gPauseMenuTiles[];
extern const struct TileCopy gPracticePauseMenuTiles[];
extern const struct TileCopy gDemoPauseMenuTiles[];
extern const struct TileCopy gLinkPauseMenuTiles[];
extern const u8 gTextPause[];

/**
 * The pause menu, once per frame of a level (Play_RunLevel). In modes 1, 2, 3 and 5,
 * with gGameState.flags bit 7, Start (or B on the menu itself) opens and closes it
 * (gGameState.paused; pauseRequest asks for it once): opening stops the sound effects (and,
 * outside mode 5, the music: gMPlayInfo_SE2, gMPlayInfo_BGM) and sets up
 * gPauseMenu (Pause_MenuItem, or in mode 5 Pause_DemoMenuItem / Pause_PracticeMenuItem by
 * GAME_FLAG_DEMO) and its OBJ tiles, closing clears flags bit 0 and restarts gGameState.song. While open it runs the menu
 * (Menu_RunWrap), or Pause_SeeMap / Pause_MakeUp when gGameState.pauseScreen is 1 / 2, and returns
 * its result. Mode 6: Select + Start returns -1, Start pauses (a blinking text). Mode 4 (the
 * linked game): player 0 runs the menu (Pause_LinkMenuItem) and sends its result in
 * gLinkSendData[1] / [7], the others show player 0's menu (gLinkRecvData[0]); 2 and 3 from
 * player 0 return 2 and -1. Else 0.
 */
s32 Pause_Update(void)
{
    const struct TileCopy *e;
    u16 v;
    s32 result;

    switch ((s8)gGameState.mode)
    {
    case 1:
    case 2:
    case 3:
    case 5:
        if (gGameState.pauseRequest != 0)
        {
            gGameState.pauseRequest = 0;
            if (gGameState.paused == 0)
                goto toggle;
        }
        if (!(gGameState.flags & 0x80))
            goto menu;
        if (gMain.newKeys & START_BUTTON)
            goto toggle;
        if (gGameState.paused == 0)
            return 0;
        if (gGameState.pauseScreen != 0 || !(gMain.newKeys & B_BUTTON))
            goto menu;
    toggle:
        gGameState.pauseScreen = 0;
        gGameState.paused = gGameState.paused == 0;
        if (gGameState.paused != 0)
        {
            m4aMPlayStop(&gMPlayInfo_SE2);
            if ((s8)gGameState.mode != 5)
            {
                m4aMPlayStop(&gMPlayInfo_BGM);
                gPauseMenu.menu.func = Pause_MenuItem;
                gPauseMenu.menu.itemCount = 4;
                gPauseMenu.menu.current = 1;
                gPauseMenu.tiles = gPauseMenuTiles;
            }
            else if (gGameState.flags & GAME_FLAG_DEMO)
            {
                gPauseMenu.menu.func = Pause_DemoMenuItem;
                gPauseMenu.menu.itemCount = 5;
                gPauseMenu.menu.current = 1;
                gPauseMenu.tiles = gDemoPauseMenuTiles;
            }
            else
            {
                gPauseMenu.menu.func = Pause_PracticeMenuItem;
                gPauseMenu.menu.itemCount = 5;
                gPauseMenu.menu.current = 1;
                gPauseMenu.tiles = gPracticePauseMenuTiles;
            }
            for (e = gPauseMenu.tiles; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
        }
        else
        {
            gGameState.flags &= ~1;
            Sound_PlaySongOrContinue(gGameState.song);
        }
        m4aSongNumStart(0x24);
    menu:
        if (gGameState.paused == 0)
            return 0;
        switch (gGameState.pauseScreen)
        {
        case 1:
            Pause_SeeMap();
            return 0;
        case 2:
            Pause_MakeUp();
            return 0;
        }
        return Menu_RunWrap(&gPauseMenu.menu, gMain.newKeys);

    case 6:
        if ((gMain.heldKeys & (SELECT_BUTTON | START_BUTTON)) == (SELECT_BUTTON | START_BUTTON))
            return -1;
        if (gMain.newKeys & START_BUTTON)
        {
            gGameState.paused = gGameState.paused == 0;
            m4aSongNumStart(0x24);
        }
        if (gGameState.paused != 0 && (gMain.frameCount & 0x18))
            Text_DrawSmall(0x64, 0x4C, gTextPause, 0xE);
        return 0;

    case 4:
        gLinkSendData[1] = 0;
        if (Link_FindCommand(1, 0x800) != 0)
        {
            if (gLinkRecvData[0].unk2 == 2)
                return 2;
            if (gLinkRecvData[0].unk2 == 3)
                return -1;
        }
        if (gLinkPlayer == 0)
        {
            gPauseMenu.menu.unkC = gMain.newKeys;
            if (gGameState.flags & 0x80)
            {
                if (!(gMain.newKeys & START_BUTTON))
                {
                    if (gGameState.paused == 0)
                        goto closed;
                    if (gGameState.pauseScreen != 0 || !(gMain.newKeys & B_BUTTON))
                        goto run;
                }
                if (gGameState.paused == 0)
                {
                    gGameState.paused = 1;
                    gGameState.pauseScreen = 0;
                    gPauseMenu.menu.func = Pause_LinkMenuItem;
                    gPauseMenu.menu.itemCount = 2;
                    gPauseMenu.menu.current = 1;
                    gPauseMenu.tiles = gLinkPauseMenuTiles;
                    for (e = gPauseMenu.tiles; e->src != NULL; e++)
                        Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
                    m4aMPlayStop(&gMPlayInfo_BGM);
                    m4aMPlayStop(&gMPlayInfo_SE2);
                    m4aSongNumStart(0x24);
                }
                else
                {
                    gGameState.paused = 0;
                    if (gGameState.helirin->flags & 2)
                        Sound_PlaySongOrContinue(gGameState.song);
                    m4aSongNumStart(0x24);
                }
            }
        run:
            if (gGameState.paused == 0)
                goto closed;
            result = Menu_RunWrap(&gPauseMenu.menu, gPauseMenu.menu.unkC);
            switch (result)
            {
            case 0:
                gLinkSendData[1] = 1;
                gLinkSendData[7] = gPauseMenu.menu.current;
                break;
            case 2:
                gLinkSendData[1] = 2;
                gLinkSendData[7] = gPauseMenu.menu.current;
                break;
            case -1:
                gLinkSendData[1] = 3;
                gLinkSendData[7] = gPauseMenu.menu.current;
                break;
            }
            return 0;
        closed:
            gLinkSendData[1] = 0;
            gLinkSendData[7] = 0;
            return 0;
        }
        if (gLinkPlayer > 3)
            return 0;
        if (Link_FindCommand(1, 0x800) != 0)
        {
            v = gLinkRecvData[0].unk2;
            if (v != 0)
            {
                if (gGameState.paused == 0)
                {
                    gGameState.pauseScreen = 0;
                    gPauseMenu.menu.func = Pause_LinkMenuItem;
                    gPauseMenu.menu.itemCount = 2;
                    gPauseMenu.menu.current = 1;
                    gPauseMenu.tiles = gLinkPauseMenuTiles;
                    for (e = gPauseMenu.tiles; e->src != NULL; e++)
                        Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
                    m4aMPlayStop(&gMPlayInfo_BGM);
                    m4aSongNumStart(0x24);
                }
                gGameState.paused = 1;
            }
            else
            {
                if (gGameState.paused != 0)
                {
                    Sound_PlaySongOrContinue(gGameState.song);
                    m4aSongNumStart(0x24);
                }
                gGameState.paused = v;
            }
        }
        if (gGameState.paused != 0)
        {
            gPauseMenu.menu.current = gLinkRecvData[0].unkE;
            Menu_Draw(&gPauseMenu.menu);
        }
        gLinkSendData[7] = gMain.newKeys;
        return 0;
    }
    return 0;
}
/**
 * The pause menu's items (Pause_Update, through Menu_RunWrap): 0x100 places it at (0x60,
 * 0x30) in gTextCursor and draws its frame (four affine sprites, their matrix all 0),
 * 0x101-0x104 draw items 1-4 one row lower each (the current one blinking in palette 14).
 * A on item 1 or 3 sets gGameState.pauseScreen to 1 / 2; on item 2 it returns 2, on item 4 -1;
 * else 0.
 */
s32 Pause_MenuItem(struct Menu *p, s32 item)
{
    struct Sprite *spr;
    s32 idx;
    s32 x, y;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;

    switch (item)
    {
    case 0x100:
        p->x = 0x60;
        p->y = 0x30;
        gTextCursor.x = 0x60;
        gTextCursor.y = 0x30;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x10;
        gTextCursor.palette = 14;
        gTextCursor.list = 1;
        idx = Sprite_AllocAffine();
        if (idx < 0)
            break;
        x = gTextCursor.x;
        y = gTextCursor.y;
        Sprite_SetAffine(idx, 0, 0, 0, 0);
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (idx << 25) | (((x - 4) << 16) & 0x1FF0000) | 0xC0008500 | (u8)(y - 4);
            spr->attr2 = 0xE3EE;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (idx << 25) | (((x - 4) << 16) & 0x1FF0000) | 0x40004500 | (u8)(y + 0x3C);
            spr->attr2 = 0xE3FE;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (idx << 25) | ((x + 0x14) << 16) | 0xC0008500 | (u8)(y - 4);
            spr->attr2 = 0xE3EE;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (idx << 25) | ((x + 0x14) << 16) | 0x40004500 | (u8)(y + 0x3C);
            spr->attr2 = 0xE3FE;
        }
        break;
    case 0x101:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x397;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x8000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x39F;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x102:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3A1;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3A9;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x103:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x38D;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x8000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x395;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x104:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x40000000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3AD;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x10) << 16) | 0x8000 | gTextCursor.y;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3B1;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 1:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.pauseScreen = 1;
        }
        break;
    case 2:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return 2;
        }
        break;
    case 3:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.pauseScreen = 2;
        }
        break;
    case 4:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return -1;
        }
        break;
    }
    return 0;
}
/**
 * The pause menu's items in mode 5 (Pause_Update), like Pause_MenuItem: 0x100 places it at
 * (0x5C, 0x28) in gTextCursor and draws its frame (two affine sprites, their matrix all
 * 0), 0x101-0x105 draw items 1-5 one row lower each (the current one blinking in palette
 * 14). A on item 1 saves gGameState.area in savedArea, sets GAME_FLAG_DEMO and returns 2; on item
 * 2 or 4 it sets gGameState.pauseScreen to 1 / 2; on item 3 it returns 2, on item 5 -1; else 0.
 */
s32 Pause_PracticeMenuItem(struct Menu *p, s32 item)
{
    struct Sprite *spr;
    s32 idx;
    s32 x, y;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;

    switch (item)
    {
    case 0x100:
        p->x = 0x5C;
        p->y = 0x28;
        gTextCursor.x = 0x5C;
        gTextCursor.y = 0x28;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x10;
        gTextCursor.palette = 14;
        gTextCursor.list = 1;
        idx = Sprite_AllocAffine();
        if (idx < 0)
            break;
        x = gTextCursor.x - 4;
        y = gTextCursor.y;
        Sprite_SetAffine(idx, 0, 0, 0, 0);
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (idx << 25) | (((x << 16) & 0x1FF0000) | 0xC0000500);
            spr->attr2 = 0xE3DC;
            *(u8 *)&spr->attr01 = y - 4;
        }
        spr = Sprite_Alloc(1);
        if (spr == NULL)
            break;
        spr->attr01 = (idx << 25) | (((x << 16) & 0x1FF0000) | 0xC0004500);
        spr->attr2 = 0xE3EC;
        *(u8 *)&spr->attr01 = y + 0x34;
        break;
    case 0x101:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3AD;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3B5;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x102:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x397;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x8000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x39F;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x103:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3A1;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3A9;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x104:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x38D;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x8000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x395;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x105:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3B9;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x10) << 16) | 0x8000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3BD;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 1:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.savedArea = gGameState.area;
            gGameState.flags |= GAME_FLAG_DEMO;
            return 2;
        }
        break;
    case 2:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.pauseScreen = 1;
        }
        break;
    case 3:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return 2;
        }
        break;
    case 4:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.pauseScreen = 2;
        }
        break;
    case 5:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return -1;
        }
        break;
    }
    return 0;
}

/**
 * The pause menu's items in mode 5 during a demo (Pause_Update), like Pause_PracticeMenuItem: 0x100
 * draws the frame at (0x5C, 0x28), 0x101-0x105 items 1-5. Item 1 (the next area) is grey
 * (palette 4) unless this is a demo, bit 1 of the Helirin's disableFlags is set and the level
 * (gLevelAreaCounts) has an area after gGameState.area: then A moves area on and returns
 * 3; else the cursor skips it (to the last item on Up, else to item 2). A on item 2 restores
 * area from savedArea and returns 2; on 3 or 4 it sets gGameState.pauseScreen to 1 / 2; on 5 it
 * restores area, clears GAME_FLAG_DEMO and returns 2; else 0.
 */
s32 Pause_DemoMenuItem(struct Menu *p, s32 item)
{
    struct Sprite *spr;
    s32 idx;
    s32 x, y;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;

    switch (item)
    {
    case 0x100:
        p->x = 0x5C;
        p->y = 0x28;
        gTextCursor.x = 0x5C;
        gTextCursor.y = 0x28;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x10;
        gTextCursor.palette = 14;
        gTextCursor.list = 1;
        idx = Sprite_AllocAffine();
        if (idx < 0)
            break;
        x = gTextCursor.x - 4;
        y = gTextCursor.y;
        Sprite_SetAffine(idx, 0, 0, 0, 0);
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (idx << 25) | (((x << 16) & 0x1FF0000) | 0xC0000500);
            spr->attr2 = 0xE3DC;
            *(u8 *)&spr->attr01 = y - 4;
        }
        spr = Sprite_Alloc(1);
        if (spr == NULL)
            break;
        spr->attr01 = (idx << 25) | (((x << 16) & 0x1FF0000) | 0xC0004500);
        spr->attr2 = 0xE3EC;
        *(u8 *)&spr->attr01 = y + 0x34;
        break;
    case 0x101:
        if (!((gGameState.flags & GAME_FLAG_DEMO) && (gGameState.helirin->disableFlags & 2)
            && gLevelAreaCounts[gGameState.world == 0
                                    ? gGameState.level
                                    : gGameState.world * 3 + gGameState.level + 2]
                   > gGameState.area + 1))
            gTextCursor.palette = 4;
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3B9;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3C1;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x102:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3A1;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3A9;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x103:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x397;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x8000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x39F;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x104:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x38D;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x8000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x395;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x105:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3AD;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x3B5;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 1:
        if ((gGameState.flags & GAME_FLAG_DEMO) && (gGameState.helirin->disableFlags & 2)
            && gLevelAreaCounts[gGameState.world == 0
                                    ? gGameState.level
                                    : gGameState.world * 3 + gGameState.level + 2]
                   > gGameState.area + 1)
        {
            if (gMain.newKeys & A_BUTTON)
            {
                m4aSongNumStart(0x21);
                gGameState.area++;
                return 3;
            }
        }
        else if (gMain.newKeys & 0x40)
        {
            p->current = p->itemCount;
        }
        else
        {
            p->current++;
        }
        break;
    case 2:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.area = gGameState.savedArea;
            return 2;
        }
        break;
    case 3:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.pauseScreen = 1;
        }
        break;
    case 4:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.pauseScreen = 2;
        }
        break;
    case 5:
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            gGameState.area = gGameState.savedArea;
            gGameState.flags &= ~GAME_FLAG_DEMO;
            return 2;
        }
        break;
    }
    return 0;
}
/**
 * The linked game's pause menu items (as Pause_MenuItem, with the keys of p->unkC): 0x100
 * places it at (0x60, 0x40) in gTextCursor and draws its frame (six affine sprites, their
 * matrix all 0), 0x101-0x102 draw items 1-2 one row lower each (the current one blinking in
 * palette 14). A on item 1 returns 2, on item 2 -1; else 0.
 */
s32 Pause_LinkMenuItem(struct Menu *p, s32 item)
{
    struct Sprite *spr;
    s32 idx;
    s32 x, y, left, top;
    u32 set, attr;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;

    switch (item)
    {
    case 0x100:
        p->x = 0x60;
        p->y = 0x40;
        gTextCursor.x = 0x60;
        gTextCursor.y = 0x40;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x10;
        gTextCursor.palette = 14;
        gTextCursor.list = 1;
        idx = Sprite_AllocAffine();
        if (idx < 0)
            break;
        // y goes through attr (set again before each sprite reads it): sharing that variable
        // changes agbcc's register priorities so idx gets r5 and the cursor r4, as in the
        // original; a temporary of its own does not.
        attr = gTextCursor.y;
        x = gTextCursor.x;
        left = x - 4;
        y = attr;
        top = y - 4;
        Sprite_SetAffine(idx, 0, 0, 0, 0);
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            set = idx << 25;
            attr = ((left << 16) & 0x1FF0000) | 0x80000500;
            spr->attr01 = set | attr;
            ((u16 *)spr)[2] = 0xE3F6;
            *(u8 *)&spr->attr01 = top;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            set = idx << 25;
            attr = ((left << 16) & 0x1FF0000) | 0x40004500;
            spr->attr01 = set | attr;
            ((u16 *)spr)[2] = 0xE3FE;
            *(u8 *)&spr->attr01 = y + 0x1C;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            set = idx << 25;
            attr = ((x + 0x1C) << 16) | 0x80008500;
            spr->attr01 = set | attr;
            ((u16 *)spr)[2] = 0xE3FB;
            *(u8 *)&spr->attr01 = top;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            set = idx << 25;
            attr = ((x + 0x1C) << 16) | 0x4500;
            spr->attr01 = set | attr;
            ((u16 *)spr)[2] = 0xE3FF;
            *(u8 *)&spr->attr01 = y + 0x1C;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            set = idx << 25;
            attr = ((x + 0x2C) << 16) | 0x40008500;
            spr->attr01 = set | attr;
            ((u16 *)spr)[2] = 0xE3FE;
            *(u8 *)&spr->attr01 = top;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            set = idx << 25;
            attr = ((x + 0x2C) << 16) | 0x500;
            spr->attr01 = set | attr;
            ((u16 *)spr)[2] = 0xE000;
            *(u8 *)&spr->attr01 = y + 0x1C;
        }
        break;
    case 0x101:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x38D;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x395;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x102:
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x399;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(1);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x10) << 16) | 0x8000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x39D;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 1:
        if (p->unkC & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return 2;
        }
        break;
    case 2:
        if (p->unkC & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return -1;
        }
        break;
    }
    return 0;
}
/**
 * The pause menu's See Map (gGameState.pauseScreen == 1, item 1 of Pause_MenuItem): sets
 * gGameState.flags bit 0 (the free camera, see_map input) and draws four blinking arrows at
 * the screen's edges (gSeeMapArrowTiles's tiles at OBJ VRAM 0x060171A0); B (song 0x22) clears the
 * bit, copies the menu's tiles (gPauseMenu.tiles) back and returns to the menu.
 */
void Pause_SeeMap(void)
{
    const struct TileCopy *e;
    struct Sprite *spr;
    u32 palette;

    if (gMain.newKeys & B_BUTTON)
    {
        m4aSongNumStart(0x22);
        gGameState.flags &= ~1;
        for (e = gPauseMenu.tiles; e->src != NULL; e++)
            Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
        gGameState.pauseScreen = 0;
        return;
    }
    gGameState.flags |= 1;
    Main_QueueCopy(gSeeMapArrowTiles, (void *)0x060171A0, 0x200);
    palette = (gMain.frameCount & 0x10) ? 14 : 15;
    spr = Sprite_Alloc(2);
    if (spr != NULL)
    {
        spr->attr01 = 0x40000000 | 0x48;
        spr->attr2 = (palette << 12) | 0x38D;
    }
    spr = Sprite_Alloc(2);
    if (spr != NULL)
    {
        spr->attr01 = 0x40E00000 | 0x48;
        spr->attr2 = (palette << 12) | 0x391;
    }
    spr = Sprite_Alloc(2);
    if (spr != NULL)
    {
        spr->attr01 = 0x40700000 | 0x00;
        spr->attr2 = (palette << 12) | 0x395;
    }
    spr = Sprite_Alloc(2);
    if (spr != NULL)
    {
        spr->attr01 = 0x40700000 | 0x90;
        spr->attr2 = (palette << 12) | 0x399;
    }
}
extern const u8 gMakeUpLRTiles[];
extern const u8 gMakeUpLRTilesBlink[];

/**
 * The other screen of Pause_Update (gGameState.pauseScreen == 2): B leaves it like Pause_SeeMap
 * (song 0x22, the tiles of gPauseMenu.tiles back to OBJ VRAM, pauseScreen cleared); otherwise it
 * copies gMakeUpLRTiles or gMakeUpLRTilesBlink (by bits 4-5 of the frame counter) to OBJ VRAM
 * 0x060171A0 and draws two sprites from it. L steps the current Helirin's type (0-14) and R
 * its color (0-6) to the next value whose bit (17 + n, 10 + n) is set in the save file's
 * unlocks; a change is stored in the save file (type, color), applied (Helirin_SetSize,
 * Helirin_SetColor), plays song 0x21 and saves (Save_Write).
 */
void Pause_MakeUp(void)
{
    const struct TileCopy *e;
    struct Sprite *spr;
    struct Helirin *h;
    s32 n;

    if (gMain.newKeys & B_BUTTON)
    {
        m4aSongNumStart(0x22);
        for (e = gPauseMenu.tiles; e->src != NULL; e++)
            Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
        gGameState.pauseScreen = 0;
        return;
    }
    if (gMain.frameCount & 0x30)
        Main_QueueCopy(gMakeUpLRTiles, (void *)0x060171A0, 0x100);
    else
        Main_QueueCopy(gMakeUpLRTilesBlink, (void *)0x060171A0, 0x100);
    spr = Sprite_Alloc(2);
    if (spr != NULL)
    {
        spr->attr01 = 0x40380000;
        // spr->unk4 as a halfword store of its own: agbcc then copies the constant from
        // another register, as the original does
        ((u16 *)spr)[2] = 0xE38D;
        *(u8 *)&spr->attr01 = 0x48;
    }
    spr = Sprite_Alloc(2);
    if (spr != NULL)
    {
        spr->attr01 = 0x40A80000;
        ((u16 *)spr)[2] = 0xE391;
        *(u8 *)&spr->attr01 = 0x48;
    }
    if (gMain.newKeys & 0x200)
    {
        h = gGameState.helirin;
        n = (s8)h->type;
        do
            n = (n > 13) ? 0 : n + 1;
        while (!(gGameState.saveFile->unlocks & (1 << (n + 17))));
        if ((s8)h->type != n)
        {
            gGameState.saveFile->type = h->type = n;
            Helirin_SetSize(h);
            m4aSongNumStart(0x21);
            Save_Write();
        }
    }
    if (gMain.newKeys & 0x100)
    {
        h = gGameState.helirin;
        n = h->color;
        do
            n = (n > 5) ? 0 : n + 1;
        while (!(gGameState.saveFile->unlocks & (1 << (n + 10))));
        if (h->color != n)
        {
            gGameState.saveFile->color = n;
            Helirin_SetColor(h, n);
            m4aSongNumStart(0x21);
            Save_Write();
        }
    }
}
