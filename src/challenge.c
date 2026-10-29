// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// challenge: the Challenge course menu.
// 0800FAAC-0801016C, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "decompress.h"
#include "task.h"
#include "menu.h"
#include "challenge.h"
#include "effects.h"
#include "sound.h"

extern const struct TileCopy gChallengeSpriteTiles[];
s32 Challenge_MenuItem(struct Menu *p, s32 item);
extern const u8 gTextChallengeTop3[];

/**
 * The Challenge course menu (world 12, gWorldEntries[12]): up to 11 rows (10 without bit 2
 * of the save file's flags) of five levels, drawn by Challenge_MenuItem; shows the selected level's
 * minimap (layer 3, or its level data drawn with Bg_DrawLevelMinimap, or blank). A starts level
 * (row - 1) * 5 + column (gGameState.mode = 3) and returns 0, B returns -1, each after
 * the screen closes (gWindowRadius runs down to 0).
 */
s32 Challenge_Run(void)
{
    struct Menu menu;
    const struct TileCopy *e;
    const struct LevelEntry *lv;
    s32 state = 0;
    s32 result = 0;
    s32 shown;
    s32 n;
    bool32 draw;

    do
    {
        draw = FALSE;
        switch (state)
        {
        case 0:
            gMain.ewramCursor = (u8 *)0x02000000;
            Bg_SetVideoMode(4);
            for (e = gChallengeSpriteTiles; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
            Main_QueueCopy(gMenuObjPalettes, (void *)0x05000200, 0x1C0);
            Bg_LoadScreen(3);
            gBldcntCopy = 0;
            Task_Init();
            Task_Create(Effect_RotateBg2, 8);
            menu.func = Challenge_MenuItem;
            menu.itemCount = (gGameState.saveFile->flags & 4) ? 11 : 10;
            menu.x = 0xA8;
            menu.y = 8;
            menu.unkC = 0;
            menu.unk12 = 0;
            if ((s8)gGameState.mode == 3 && gGameState.world == 12)
            {
                menu.current = gGameState.level / 5 + 1;
                menu.unkE = (u8)(gGameState.level % 5);
                if (menu.current > 7)
                    menu.unk10 = 6;
                else
                    menu.unk10 = menu.current - 1;
            }
            else
            {
                menu.current = 1;
                menu.unkE = 0;
                menu.unk10 = 0;
            }
            shown = -1;
            Sprite_ClearLists();
            Sprite_BuildOam();
            gDispcntCopy |= 0x1700;
            state = 1;
            break;
        case 1:
            Bg_StartWindowCircle(0, 0x78, 0x50);
            m4aSongNumStart(0x29);
            state = 2;
            break;
        case 2:
            Bg_DrawWindowCircle(gWindowRadius + 8, gWindowX, gWindowY);
            if (gWindowRadius > 0x9F)
            {
                gDispcntCopy &= 0xDEFF;
                Sound_PlaySongOrContinue(0xF);
                state = 3;
            }
            draw = TRUE;
            break;
        case 3:
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                result = -1;
                state = 4;
            }
            if (gMain.newKeys & A_BUTTON)
            {
                m4aSongNumStart(0x21);
                gGameState.mode = 3;
                gGameState.world = 12;
                gGameState.level = (menu.current - 1) * 5 + (u8)menu.unkE;
                result = 0;
                state = 4;
            }
            draw = TRUE;
            break;
        case 4:
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            Bg_StartWindowCircle(0xA0, 0x78, 0x50);
            m4aSongNumStart(0x2A);
            state = 5;
            break;
        case 5:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            if (gWindowRadius <= 0)
            {
                m4aMPlayAllStop();
                state = -1;
            }
            draw = TRUE;
            break;
        }

        if (draw)
        {
            Sprite_ClearLists();
            n = (menu.current - 1) * 5 + menu.unkE;
            if (shown != n)
            {
                shown = n;
                lv = &gWorldEntries[12].info->levels[n];
                if (lv->layers[3].size != 0)
                {
                    Main_QueueCopy(gWorldEntries[12].data + lv->layers[3].offset, (void *)0x060174A0,
                                 0x800);
                }
                else if (lv->layers[0].size != 0)
                {
                    Decompress_LZ(gWorldEntries[12].data + lv->layers[0].offset, gMain.ewramCursor,
                                 lv->layers[0].size);
                    Bg_DrawLevelMinimap(gMain.ewramCursor);
                }
                else
                {
                    DmaFill32(3, 0x11111111, 0x060174A0, 0x800);
                }
            }
            if (state == 3)
            {
                menu.unkC = gMain.repeatKeys;
                Menu_Run(&menu, gMain.repeatKeys);
            }
            else
            {
                Menu_Draw(&menu);
            }
            Task_RunAll();
            Sprite_BuildOam();
        }
        Main_EndFrame();
    } while (state != -1);
    return result;
}
/**
 * The Challenge course menu's items (Challenge_Run): rows 1-11 (current) of five courses
 * (column unkE): world 12's level (row - 1) * 5 + column, level index 0x26 + that in the save
 * file (bestTimes, marks, records). Items 1-11 move the column
 * (Menu_StepValueWrap); 0x100 scrolls the four (five while scrolling up) rows shown so that the
 * current one is among them (first row unk10, unk12 the scroll offset in pixels, 4 a frame),
 * and draws the save file's name and best time of the selected course and its three
 * records (gSaveBank + 0x9C4), the file's own one blinking; 0x101-0x10B draw a shown
 * row: its number and five course sprites (the current one blinking), each with the file's
 * marks[] mark when gMain.subState is 5. Returns 0.
 */
s32 Challenge_MenuItem(struct Menu *p, s32 item)
{
    struct SaveFile *save = gGameState.saveFile;
    struct LevelRecord *rec;
    struct Sprite *spr;
    s32 n = (p->current - 1) * 5 + p->unkE + 0x26;
    s32 row;
    s32 last;
    s32 palette, pal;
    s32 col;
    s32 tile;
    s32 i;

    if (item == 0x100)
    {
        if (p->current - 1 < p->unk10)
        {
            p->unk10--;
            p->unk12 = -0x20;
        }
        if (p->current - 1 >= p->unk10 + 4)
        {
            p->unk10++;
            p->unk12 = 0x20;
        }
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = (p->x << 16) | 0xC0000000 | p->y;
            spr->attr2 = 0xC3A5;
        }
        gTextCursor.x = p->x;
        gTextCursor.y = p->y + 0x42;
        gTextCursor.dx = 8;
        gTextCursor.dy = 9;
        gTextCursor.list = 1;
        gTextCursor.palette = 14;
        Text_DrawSmallName(gTextCursor.x, gTextCursor.y, save->name, save->rank, 14);
        gTextCursor.y += gTextCursor.dy;
        Text_DrawSmallTime(gTextCursor.x, gTextCursor.y, save->bestTimes[n], gTextCursor.palette);
        gTextCursor.y += gTextCursor.dy;
        gTextCursor.palette = 14;
        Text_PrintSmall(&gTextCursor, gTextChallengeTop3);
        rec = (struct LevelRecord *)(gSaveBank + 0x9C4) + n * 3;
        for (i = 2; i >= 0; i--)
        {
            if (save->bestTimes[n] != 0x34BC0 && save->bestTimes[n] == rec->time
                && strcmp(rec->name, save->name) == 0 && !(gMain.frameCount & 8))
                gTextCursor.palette = 15;
            else
                gTextCursor.palette = 14;
            Text_DrawSmallName(gTextCursor.x, gTextCursor.y, rec->name, rec->rank, gTextCursor.palette);
            gTextCursor.y += gTextCursor.dy;
            Text_DrawSmallTime(gTextCursor.x, gTextCursor.y, rec->time, gTextCursor.palette);
            gTextCursor.y += gTextCursor.dy;
            rec++;
        }
        gTextCursor.x = 4;
        if ((s16)p->unk12 > 0)
        {
            p->unk12 -= 4;
            if (p->unk12 != 0)
                gTextCursor.y = p->unk12 + 8;
            else
                gTextCursor.y = 0x28;
        }
        else if ((s16)p->unk12 < 0)
        {
            p->unk12 += 4;
            gTextCursor.y = p->unk12 + 0x28;
        }
        else
        {
            gTextCursor.y = 0x28;
        }
    }
    else if (item > 0x100)
    {
        if (item > 0x10B)
            return 0;
        row = item - 0x101;
        if ((s16)p->unk12 > 0)
        {
            if (p->unk10 > item - 0x100)
                return 0;
            last = p->unk10 + 4;
        }
        else if ((s16)p->unk12 < 0)
        {
            if (p->unk10 > row)
                return 0;
            last = p->unk10 + 5;
        }
        else
        {
            if (p->unk10 > row)
                return 0;
            last = p->unk10 + 4;
        }
        if (last <= row)
            return 0;

        if ((s8)gMain.subState == 8 || (s8)gMain.subState == 10)
            palette = 4;
        else
            palette = 2;
        spr = Sprite_Alloc(9);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000 | gTextCursor.y;
            spr->attr2 = (palette << 12) | 0x800 | (row * 8 + 0x120);
        }
        n = row * 5 + 0x26;
        for (col = 0; col <= 4; col++)
        {
            if (p->unkE == col && p->current - 1 == row)
                pal = (~(gMain.frameCount >> 3) & 1) + palette;
            else
                pal = palette;
            spr = Sprite_Alloc(9);
            if (spr != NULL)
            {
                spr->attr01 = (((gTextCursor.x + 0x28 + col * 0x18 - 8) << 16) & 0x1FF0000) | 0x80000000
                          | (u8)(gTextCursor.y - 8);
                spr->attr2 = (pal << 12) | 0x800 | (col * 16 + 0x178);
            }
            if ((s8)gMain.subState == 5)
            {
                if (gGameState.saveFile->marks[n] > 1)
                {
                    tile = 0x1D8;
                    if (gGameState.saveFile->marks[n] > 3)
                        tile += 4;
                    spr = Sprite_Alloc(8);
                    if (spr != NULL)
                    {
                        spr->attr01 = (((gTextCursor.x + 0x28 + col * 0x18) << 16) & 0x1FF0000) | 0x40000000
                                  | gTextCursor.y;
                        spr->attr2 = 0x8800 | tile;
                    }
                }
                n++;
            }
        }
        gTextCursor.y += 0x20;
    }
    else if (item <= 0xB && item >= 1)
    {
        Menu_StepValueWrap(&p->unkE, 4, p->unkC);
    }
    return 0;
}
