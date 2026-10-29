// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// practice: Practice (mode 5) and its area menu.
// 08018D34-080194D4, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "decompress.h"
#include "task.h"
#include "menu.h"
#include "level.h"
#include "mode_menu.h"
#include "sound.h"
#include "practice.h"

s32 Practice_AreaMenuItem(struct Menu *p, s32 item);

/**
 * Practice (mode 5), with its own frame loop on the heap gMain.heap: the map screen of
 * MapScreen_Run with its tasks MapScreen_UpdateCursor, MapScreen_UpdateWorldIcon and MapScreen_UpdatePath and the menu
 * MapScreen_LevelMenuItem, whose unk10 (the level number shown) starts at the current level, or 5 in
 * world 0, and goes up to unkE, the first level from 5 on (at most 0x22) without a mark.
 * A on a level with level data opens a second menu (Practice_AreaMenuItem) of its areas
 * (gLevelAreaCounts) next to it on the map; A there picks the area (gGameState.area) and
 * returns 0, B goes back. B on the map returns -1, after the screen closes.
 */
s32 Practice_Run(void)
{
    struct Menu menu;
    const struct TileCopy *e;
    struct Menu areas;
    const struct WorldInfo *info;
    const u8 *data;
    const struct LevelEntry *lv;
    u16 *p;
    u8 *buf;
    s32 state = 0;
    s32 result = 0;
    struct SaveFile *save = gGameState.saveFile;
    s32 shown;
    s32 n;
    s32 size;
    s32 i, j;

    do
    {
        switch (state)
        {
        case 0:
            Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
            Bg_SetVideoMode(3);
            Sprite_ClearLists();
            Sprite_BuildOam();
            for (e = gMapScreenSpriteTiles; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
            Main_QueueCopy(gIntroObjTiles, (void *)0x06010000, 0x2400);
            Main_QueueCopy(gMenuObjTiles, (void *)0x06012400, 0x4680);
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
            Task_Init();
            Task_Create(MapScreen_UpdateCursor, 0x1C);
            Task_Create(MapScreen_UpdateWorldIcon, 0xC);
            Task_Create(MapScreen_UpdatePath, 0x10);
            shown = -1;
            menu.func = MapScreen_LevelMenuItem;
            menu.itemCount = 1;
            menu.current = 1;
            menu.x = 0;
            menu.y = 0;
            for (i = 5; i < 0x22; i++)
            {
                if (save->marks[i] == 0)
                    break;
            }
            menu.unkE = i;
            if (gGameState.world == 0)
                menu.unk10 = gGameState.level;
            else
                menu.unk10 = gGameState.world * 3 + 2 + gGameState.level;
            if (gGameState.world == 0)
                menu.unk10 = 5;
            else if (menu.unk10 > menu.unkE)
                menu.unk10 = menu.unkE;
            gGameState.area = 0;
            gGameState.savedArea = 0;
            gCameraY = -1;
            gCameraX = -1;
            MapScreen_ShowMap(menu.unk10);
            state = 1;
            goto next;
        case 1:
            Bg_StartWindowCircle(0, 0x78, 0x50);
            m4aSongNumStart(0x29);
            state = 2;
            goto next;
        case 2:
            Bg_DrawWindowCircle(gWindowRadius + 8, gWindowX, gWindowY);
            if (gWindowRadius > 0xA0)
            {
                gDispcntCopy &= 0xDEFF;
                Sound_PlaySongOrContinue(0xF);
                state = 3;
            }
            break;
        case 3:
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                result = -1;
                state = 5;
            }
            if ((gMain.newKeys & A_BUTTON) && MapScreen_GetLevelDataSize() != 0)
            {
                m4aSongNumStart(0x21);
                areas.func = Practice_AreaMenuItem;
                areas.itemCount = gLevelAreaCounts[menu.unk10];
                areas.current = 1;
                areas.x = gMapLevelPositions[menu.unk10].x - (gCameraX + 0x10);
                areas.y = gMapLevelPositions[menu.unk10].y - (gCameraY - 0x10);
                state = 4;
            }
            break;
        case 4:
            Sprite_ClearLists();
            Menu_RunWrap(&areas, gMain.newKeys);
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                state = 3;
            }
            if (gMain.newKeys & A_BUTTON)
            {
                m4aSongNumStart(0x21);
                gGameState.area = areas.current - 1;
                result = 0;
                state = 5;
            }
            goto draw;
        case 5:
            if (result == 0)
                Bg_LoadScreen(8);
            else
                Bg_LoadScreen(9);
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            gDispcntCopy |= 0x2100;
            state = 6;
            goto next;
        case 6:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            if (gWindowRadius <= 0)
            {
                m4aMPlayAllStop();
                state = -1;
            }
            break;
        default:
            goto next;
        }

        Sprite_ClearLists();
    draw:
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
        if (state == 3)
            Menu_RunWrap(&menu, gMain.newKeys);
        else
            Menu_Draw(&menu);
        Task_RunAll();
        Sprite_BuildOam();
    next:
        Main_EndFrame();
    } while (state != -1);
    return result;
}
extern const u8 gTextArea1[];
extern const u8 gTextArea2[];
extern const u8 gTextArea3[];
extern const u8 gTextArea4[];

// One row of Practice_AreaMenuItem's frame: its left and right edge pieces, *dy* below the cursor.
#define AREA_FRAME_ROW(dy)                                                                 \
    {                                                                                      \
        spr = Sprite_Alloc(gTextCursor.list);                                           \
        if (spr != NULL)                                                                   \
        {                                                                                  \
            spr->attr01 = (idx << 25) | ((((gTextCursor.x - 4) << 16) & 0x1FF0000) | 0x40004500); \
            spr->attr2 = ((gTextCursor.list >> 2) << 10) | 0xE3FE;                          \
            *(u8 *)&spr->attr01 = gTextCursor.y + (dy);                                    \
        }                                                                                  \
        spr = Sprite_Alloc(gTextCursor.list);                                           \
        if (spr != NULL)                                                                   \
        {                                                                                  \
            spr->attr01 = (idx << 25) | (((gTextCursor.x + 0x1C) << 16) | 0x4500);          \
            spr->attr2 = ((gTextCursor.list >> 2) << 10) | 0xE3FF;                          \
            *(u8 *)&spr->attr01 = gTextCursor.y + (dy);                                    \
        }                                                                                  \
    }

/**
 * The items of Practice's area menu (Practice_Run): 0x100 sets the cursor gTextCursor up
 * at (p->x, p->y), 8 x 9 per character and row, and draws a frame of affine sprites
 * (their matrix all 0) around p->itemCount rows (1-3; more draw like 4), then moves the cursor
 * up by half the rows; 0x101-0x104 draw the strings gTextArea1..gTextArea4
 * (Text_PrintSmallProportional), one per area. Returns 0.
 */
s32 Practice_AreaMenuItem(struct Menu *p, s32 item)
{
    struct Sprite *spr;
    s32 idx;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;

    switch (item)
    {
    case 0x100:
        gTextCursor.x = p->x;
        gTextCursor.y = p->y;
        gTextCursor.dx = 8;
        gTextCursor.dy = 9;
        gTextCursor.list = 4;
        idx = Sprite_AllocAffine();
        if (idx >= 0)
        {
            Sprite_SetAffine(idx, 0, 0, 0, 0);
            switch (p->itemCount)
            {
            default:
                AREA_FRAME_ROW(0x1C);
                // fall through
            case 3:
                AREA_FRAME_ROW(0x14);
                // fall through
            case 2:
                AREA_FRAME_ROW(0x0C);
                // fall through
            case 1:
                spr = Sprite_Alloc(gTextCursor.list);
                if (spr != NULL)
                {
                    spr->attr01 = (idx << 25) | ((((gTextCursor.x - 4) << 16) & 0x1FF0000) | 0x80004500);
                    spr->attr2 = ((gTextCursor.list >> 2) << 10) | 0xE3FA;
                    *(u8 *)&spr->attr01 = gTextCursor.y - 4;
                }
                spr = Sprite_Alloc(gTextCursor.list);
                if (spr != NULL)
                {
                    spr->attr01 = (idx << 25) | (((gTextCursor.x + 0x1C) << 16) | 0x40000500);
                    spr->attr2 = ((gTextCursor.list >> 2) << 10) | 0xE3FD;
                    *(u8 *)&spr->attr01 = gTextCursor.y - 4;
                }
                break;
            }
        }
        gTextCursor.y -= p->itemCount / 2;
        break;
    case 0x101:
        Text_PrintSmallProportional(&gTextCursor, gTextArea1);
        break;
    case 0x102:
        Text_PrintSmallProportional(&gTextCursor, gTextArea2);
        break;
    case 0x103:
        Text_PrintSmallProportional(&gTextCursor, gTextArea3);
        break;
    case 0x104:
        Text_PrintSmallProportional(&gTextCursor, gTextArea4);
        break;
    }
    return 0;
}
#undef AREA_FRAME_ROW
