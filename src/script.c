// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// script: the screens that step through a script of pictures.
// 0800E560-0800EF44, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "decompress.h"
#include "task.h"
#include "menu.h"
#include "script.h"
#include "sound.h"

// What Script_Run passes to Script_Step (on its stack): a script of words that
// Script_Step steps through, the two blocks it decompresses them into and a palette.
struct ScriptPlayer {
    /*0x00*/ u32 unk0;
    /*0x04*/ const u32 *script;
    /*0x08*/ void *bgBlock;  // a BG palette is decompressed here
    /*0x0C*/ void *objBlock; // OBJ tiles are decompressed here
    /*0x10*/ s32 palette;    // Script_Run draws its sprites with this palette
};
s32 Script_Step(struct ScriptPlayer *p);
s32 Script_SkipTrainingMenuItem(struct Menu *p, s32 item);
extern const u32 *const gRankUpScripts[];
extern const u32 *const gLevelStartScripts[];
extern const u32 *const gLevelEndScripts[];
extern const u32 gMissingItemsScript[];
extern const u32 gAllItemsScript[];
extern const u32 gAdventureStartScript[];
extern const u32 gUnk_080C3EB8[];
extern const u32 gUnk_080C3C78[];
extern const u8 gUnk_08187C94[];
extern const u8 gUnk_0819C254[];
extern const u8 gUnk_0819C554[];
extern const u8 gUnk_0819C6CC[];

// The compressed blocks the scripts of Script_Step name, told apart by where they are:
// [gScriptBgPalettes, gScriptObjTiles) BG palettes, [gScriptObjTiles, gScriptDataEnd) OBJ tiles.
extern const u8 gScriptBgPalettes[];
extern const u8 gScriptObjTiles[];
extern const u8 gScriptDataEnd[];

/**
 * Steps *p*'s script to its next stop. Each word: 0 ends it (it stays on the 0; returns -1);
 * a value below gScriptBgPalettes is a stop (returns 0); a BG palette block (0x200 bytes, into
 * the block bgBlock from gMain.heap's heap, then BG palette RAM) is followed by the tile map
 * and the tile list of map 1 (Bg_LoadMap8bpp with gUnk_0813A3DC's 8bpp tiles, 30x20), and
 * the script goes on; an OBJ tile block (0x1000 bytes, into objBlock, then 0x06012400) is
 * followed by the sprites' palette (palette) and stops (returns 0); a word past the blocks
 * is skipped.
 */
s32 Script_Step(struct ScriptPlayer *p)
{
    const u8 *w;
    const u8 *src;
    const u16 *list;

    for (;;)
    {
        w = (const u8 *)*p->script++;
        if (w == NULL)
        {
            p->script--;
            return -1;
        }
        if (w < gScriptBgPalettes)
            break;
        if (w < gScriptObjTiles)
        {
            if (p->bgBlock != NULL)
            {
                Heap_Free(gMain.heap, p->bgBlock);
                p->bgBlock = NULL;
            }
            p->bgBlock = Heap_Alloc(gMain.heap, 0x200);
            if (p->bgBlock != NULL)
            {
                Decompress_LZ(w, p->bgBlock, 0x200);
                DmaCopy32(3, p->bgBlock, (void *)0x05000000, 0x100);
            }
            src = (const u8 *)*p->script++;
            list = (const u16 *)*p->script++;
            Bg_LoadMap8bpp(1, src, gUnk_0813A3DC, list, 0x1E, 0x14);
        }
        else if (w < gScriptDataEnd)
        {
            if (p->objBlock != NULL)
            {
                Heap_Free(gMain.heap, p->objBlock);
                p->objBlock = NULL;
            }
            p->objBlock = Heap_Alloc(gMain.heap, 0x1000);
            if (p->objBlock != NULL)
            {
                Decompress_LZ(w, p->objBlock, 0x1000);
                Main_QueueCopy(p->objBlock, (void *)0x06012400, 0x1000);
            }
            p->palette = *p->script++;
            return 0;
        }
    }
    return 0;
}
/**
 * The frame loop of a screen that shows a script (struct ScriptPlayer, stepped by
 * Script_Step on A or B) chosen by *mode* 0-5, opening and closing with Bg_DrawWindowCircle.
 * States 8-12 (from mode 2) also show a two-item menu (Menu_RunWrap, Script_SkipTrainingMenuItem); its
 * second item skips the training: marks[0..4] of gGameState.saveFile (the five training
 * levels) become 1.
 */
void Script_Run(s32 mode)
{
    s32 state = 0;
    u8 *block = NULL;
    struct Menu menu;
    struct ScriptPlayer text;
    struct Sprite *spr;
    s32 bgMode;
    u16 layers;
    s32 alt;
    s32 y;
    u32 tile;
    u32 x;
    s32 i;
    s32 j;

    text.bgBlock = NULL;
    text.objBlock = NULL;
    text.palette = 0xF;
    do
    {
        switch (state)
        {
        case 0:
            switch (mode)
            {
            case 0:
                if (gGameState.world != 0)
                    return;
                text.script = gLevelStartScripts[gGameState.level];
                state = 1;
                bgMode = 0xE;
                alt = 1;
                layers = 0x1700;
                break;
            case 1:
                if (gGameState.world == 0)
                {
                    if (gGameState.level <= 3)
                        return;
                    text.script = gLevelEndScripts[0];
                    state = 1;
                    bgMode = 0xE;
                    alt = 1;
                    layers = 0x1700;
                    break;
                }
                if (gGameState.world > 10)
                    return;
                if (gGameState.level <= 1)
                    return;
                if (gGameState.flags & GAME_FLAG_BONUS_TAKEN)
                    text.script = gLevelEndScripts[gGameState.world];
                else if (gCamera.bonusUnlock == 0)
                    return;
                else
                    text.script = gUnk_080C3EB8;
                state = 4;
                bgMode = 0xB;
                alt = 0;
                layers = 0x1F00;
                break;
            case 2:
                text.script = gAdventureStartScript;
                state = 7;
                bgMode = 0xB;
                alt = 0;
                layers = 0x1F00;
                break;
            case 3:
                text.script = gAllItemsScript;
                state = 1;
                bgMode = 0xB;
                alt = 0;
                layers = 0x1F00;
                break;
            case 4:
                text.script = gMissingItemsScript;
                state = 1;
                bgMode = 0xB;
                alt = 0;
                layers = 0x1F00;
                break;
            case 5:
                text.script = gRankUpScripts[gGameState.saveFile->rank];
                state = 1;
                bgMode = 0xB;
                alt = 0;
                layers = 0x1F00;
                break;
            default:
                bgMode = -1;
                break;
            }
            if (bgMode >= 0)
            {
                Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
                Bg_SetVideoMode(5);
                Main_QueueCopy(gCollectionPalettes, (void *)0x05000200, 0x200);
                Task_Init();
                Bg_LoadScreen(bgMode);
                block = Heap_Alloc(gMain.heap, 0x200);
                if (block != NULL)
                {
                    Decompress_LZ(alt ? gUnk_08187C94 : gCollectionBgPalettes, block, 0x200);
                    Main_QueueCopy(block + 0x100, (void *)0x05000100, 0x100);
                }
                Main_QueueFill(0, (void *)0x06012400, 0x1000);
                Script_Step(&text);
                gDispcntCopy |= layers;
            }
            Sound_PlaySongOrContinue(0xF);
            Bg_StartWindowCircle(0, 0x78, 0x50);
            m4aSongNumStart(0x29);
            break;
        case 12:
            text.script = gUnk_080C3C78;
            Script_Step(&text);
            state++;
            // fall through
        case 1:
        case 4:
        case 7:
        case 13:
            Bg_DrawWindowCircle(gWindowRadius + 8, gWindowX, gWindowY);
            Sprite_ClearLists();
            Task_RunAll();
            if (gWindowRadius > 0xA0)
                state++;
            Sprite_BuildOam();
            break;
        case 2:
        case 5:
        case 14:
            if (!(gMain.heldKeys & 4) && (gMain.newKeys & (A_BUTTON | B_BUTTON)))
            {
                if (Script_Step(&text) < 0)
                {
                    gWindowRadius = 0xB4;
                    state++;
                }
                m4aSongNumStart(0x21);
            }
            Sprite_ClearLists();
            Task_RunAll();
            // 8 rows of four 32x8 sprites, tiles from 0x120
            if (!(gMain.heldKeys & 4))
            {
                y = (mode <= 4 && mode >= 3 && text.palette != 0xF) ? 0x58 : 8;
                tile = 0x120;
                for (i = 0; i <= 7; i++)
                {
                    x = 0x38 << 16;
                    for (j = 3; j >= 0; j--)
                    {
                        spr = Sprite_Alloc(5);
                        if (spr != NULL)
                        {
                            spr->attr01 = (x & 0x1FF0000) | 0x40004000 | (u8)(y + i * 8);
                            spr->attr2 = (text.palette << 12) | (tile | 0x400);
                        }
                        tile += 4;
                        x += 0x20 << 16;
                    }
                }
            }
            Sprite_BuildOam();
            break;
        case 3:
        case 6:
        case 15:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            if (gWindowRadius <= 0)
                state = -1;
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 8:
            Sprite_ClearLists();
            Task_RunAll();
            if (!(gMain.heldKeys & 4))
            {
                if (gMain.newKeys & (A_BUTTON | B_BUTTON))
                {
                    if (Script_Step(&text) < 0)
                        state++;
                    m4aSongNumStart(0x21);
                }
                if (!(gMain.heldKeys & 4))
                {
                    y = text.palette == 0xF ? 8 : 0x58;
                    tile = 0x120;
                    for (i = 0; i <= 7; i++)
                    {
                        x = 0x38 << 16;
                        for (j = 3; j >= 0; j--)
                        {
                            spr = Sprite_Alloc(5);
                            if (spr != NULL)
                            {
                                spr->attr01 = (x & 0x1FF0000) | 0x40004000 | (u8)(y + i * 8);
                                spr->attr2 = (text.palette << 12) | (tile | 0x400);
                            }
                            tile += 4;
                            x += 0x20 << 16;
                        }
                    }
                }
            }
            Sprite_BuildOam();
            break;
        case 9:
            Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
            text.bgBlock = Heap_Alloc(gMain.heap, 0x1000);
            if (text.bgBlock != NULL)
            {
                Decompress_LZ(gUnk_0819C254, text.bgBlock, 0x1000);
                Main_QueueCopy(text.bgBlock, (void *)0x06012400, 0x1000);
            }
            text.objBlock = Heap_Alloc(gMain.heap, 0x1000);
            if (text.objBlock != NULL)
            {
                Decompress_LZ(gUnk_0819C554, text.objBlock, 0x1000);
                Main_QueueCopy(text.objBlock, (void *)0x06013400, 0x1000);
            }
            Main_QueueCopy(gUnk_0819C6CC, (void *)0x06014400, 0x200);
            menu.func = Script_SkipTrainingMenuItem;
            menu.itemCount = 2;
            menu.current = 1;
            state++;
            break;
        case 10:
            Sprite_ClearLists();
            if (Menu_RunWrap(&menu, gMain.newKeys))
            {
                if (menu.current != 1)
                {
                    for (i = 0; i <= 4; i++)
                        gGameState.saveFile->marks[i] = 1;
                    gWindowRadius = 0xB4;
                    state = 15;
                }
                else
                {
                    gWindowRadius = 0xB4;
                    state++;
                }
            }
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 11:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            if (gWindowRadius <= 0)
                state++;
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            break;
        }
        Main_EndFrame();
    } while (state != -1);

    if (block != NULL)
        Heap_Free(gMain.heap, block);
    Main_QueueCopy(gTextPalettes, (void *)0x050003C0, 0x40);
    Main_QueueCopy(gTextPalettes, (void *)0x050001C0, 0x40);
}
/**
 * The two-item menu of Script_Run's states 8-12: item 0x100 draws two 4 x 8 grids of
 * 32x8 sprites (tiles 0x120 and 0x1A0 on), items 0x101 and 0x102 one 32x16 sprite each, in
 * palette 13 for the current item while bit 3 of the frame counter is set (blinking), else
 * 15 (gTextCursor.palette); on the current item (1-2) A plays song 0x21 and returns 1.
 * Returns 0 otherwise.
 */
s32 Script_SkipTrainingMenuItem(struct Menu *p, s32 item)
{
    struct Sprite *spr;
    s32 tile;
    s32 i;
    s32 j;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 13;
    else
        gTextCursor.palette = 15;

    if (item == 0x100)
    {
        tile = 0x120;
        for (i = 0; i <= 7; i++)
        {
            for (j = 0; j <= 3; j++)
            {
                spr = Sprite_Alloc(5);
                if (spr != NULL)
                {
                    spr->attr01 = 0x40004000 | (((0x38 + j * 0x20) << 16) & 0x1FF0000) | (8 + i * 8);
                    spr->attr2 = 0xD400 | tile;
                }
                tile += 4;
            }
        }
        tile = 0x1A0;
        for (i = 0; i <= 7; i++)
        {
            for (j = 0; j <= 3; j++)
            {
                spr = Sprite_Alloc(5);
                if (spr != NULL)
                {
                    spr->attr01 = 0x40004000 | (((0x38 + j * 0x20) << 16) & 0x1FF0000) | (0x58 + i * 8);
                    spr->attr2 = 0xF400 | tile;
                }
                tile += 4;
            }
        }
    }
    else if (item == 0x101)
    {
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0x80664000 | 0x66;
            spr->attr2 = (gTextCursor.palette << 12) | 0x620;
        }
    }
    else if (item == 0x102)
    {
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0x80664000 | 0x7A;
            spr->attr2 = (gTextCursor.palette << 12) | 0x628;
        }
    }
    else if (item >= 1 && item <= 2)
    {
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return 1;
        }
    }
    return 0;
}
