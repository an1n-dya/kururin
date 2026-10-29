// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// level_intro: the screen before a level and its animation.
// 0800F624-0800FAAC, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "decompress.h"
#include "util.h"
#include "level_intro.h"

extern const u8 *const gLevelIntroWorldNameTiles[];
extern const u8 *const gLevelIntroLevelNumberTiles[];
extern const u8 *const gLevelIntroPictureTiles[];
extern const u8 *const gLevelIntroAnimTiles[];
extern const u16 gLevelIntroAnim[];
extern const u16 gLevelIntroEndAnim[];
extern const u8 gLevelIntroWorld5Picture1[];
extern const u8 gLevelIntroWorld5Picture2[];
void LevelIntro_PlayStartAnim(void);

/**
 * A screen with its own frame loop on the heap gMain.heap (the EWRAM from 0x02000000):
 * decompresses OBJ tiles by world (gLevelIntroWorldNameTiles, gLevelIntroPictureTiles) and level (gLevelIntroLevelNumberTiles)
 * to OBJ VRAM and, for the first level of worlds 1-10 only, shows three sprites and a 5x5
 * sprite grid while the gLevelIntroAnim animation runs (each frame's tiles from
 * gLevelIntroAnimTiles). A or B (once, three times in world 5) swaps the gLevelIntroPictureTiles tiles for
 * gLevelIntroWorld5Picture1 / gLevelIntroWorld5Picture2 and then starts gLevelIntroEndAnim; it ends with the animation.
 */
void LevelIntro_Run(void)
{
    u8 *blocks[4];
    struct Anim anim;
    struct Sprite *spr;
    s32 result;
    s32 presses;
    s32 tile;
    s32 i, j;

    Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
    Bg_SetVideoMode(2);
    Sprite_ClearLists();
    Sprite_BuildOam();
    gDispcntCopy |= 0x1000;
    blocks[0] = Heap_Alloc(gMain.heap, 0x400);
    if (blocks[0] != NULL)
    {
        Decompress_LZ(gLevelIntroWorldNameTiles[gGameState.world], blocks[0], 0x400);
        Main_QueueCopy(blocks[0], (void *)0x06012400, 0x400);
    }
    blocks[1] = Heap_Alloc(gMain.heap, 0x200);
    if (blocks[1] != NULL)
    {
        Decompress_LZ(gLevelIntroLevelNumberTiles[gGameState.level], blocks[1], 0x200);
        Main_QueueCopy(blocks[1], (void *)0x06012800, 0x200);
    }
    blocks[2] = Heap_Alloc(gMain.heap, 0xC80);
    if (blocks[2] != NULL)
    {
        Decompress_LZ(gLevelIntroPictureTiles[gGameState.world], blocks[2], 0xC80);
        Main_QueueCopy(blocks[2], (void *)0x06012A00, 0xC80);
    }
    blocks[3] = Heap_Alloc(gMain.heap, 0x200);
    anim.timer = 1;
    anim.next = gLevelIntroAnim;
    anim.value = gLevelIntroAnim[1];
    Main_QueueCopy(gCollectionPalettes, (void *)0x05000200, 0x200);

    if ((u8)(gGameState.world - 1) <= 9 && gGameState.level == 0)
    {
        presses = 1;
        if (gGameState.world == 5)
            presses = 3;
        do
        {
            if ((gMain.newKeys & (A_BUTTON | B_BUTTON)) && presses != 0)
            {
                presses--;
                switch (presses)
                {
                case 0:
                    anim.timer = 1;
                    anim.next = gLevelIntroEndAnim;
                    anim.value = gLevelIntroEndAnim[1];
                    break;
                case 1:
                    Decompress_LZ(gLevelIntroWorld5Picture2, blocks[2], 0xC80);
                    Main_QueueCopy(blocks[2], (void *)0x06012A00, 0xC80);
                    break;
                case 2:
                    Decompress_LZ(gLevelIntroWorld5Picture1, blocks[2], 0xC80);
                    Main_QueueCopy(blocks[2], (void *)0x06012A00, 0xC80);
                    break;
                }
            }
            Sprite_ClearLists();
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = 0xC0484000 | 0x10;
                spr->attr2 = 0xC520;
            }
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = 0x80880000 | 0x10;
                spr->attr2 = 0xC540;
            }
            spr = Sprite_Alloc(4);
            if (spr != NULL)
            {
                spr->attr01 = 0x80180000 | 0x58;
                spr->attr2 = 0xE5B4;
            }
            result = Anim_Step(&anim);
            if ((result & 1) && blocks[3] != NULL)
            {
                Decompress_LZ(gLevelIntroAnimTiles[anim.value], blocks[3], 0x200);
                Main_QueueCopy(blocks[3], (void *)0x06013680, 0x200);
            }
            if (anim.value <= 3)
            {
                tile = 0x150;
                for (i = 0; i <= 4; i++)
                {
                    for (j = 0; j <= 4; j++)
                    {
                        spr = Sprite_Alloc(4);
                        if (spr != NULL)
                        {
                            spr->attr01 = 0x40004000 | ((0x38 + j * 0x20) << 16) | (0x54 + i * 8);
                            spr->attr2 = 0xC400 | tile;
                        }
                        tile += 4;
                    }
                }
            }
            Sprite_BuildOam();
            Main_EndFrame();
        } while (result >= 0);
    }

    Main_EndFrame();
    for (i = 0; i <= 3; i++)
    {
        if (blocks[i] != NULL)
            Heap_Free(gMain.heap, blocks[i]);
    }
    LevelIntro_PlayStartAnim();
    Main_QueueCopy(gTextPalettes, (void *)0x050003C0, 0x40);
    Main_QueueCopy(gTextPalettes, (void *)0x050001C0, 0x40);
}
extern const u16 gLevelStartAnim[];
// The pictures of gLevelStartAnim's frames: a tile map and its tile list (Bg_LoadMap8bpp)
extern const struct { const u8 *map; const u16 *list; } gLevelStartPictures[];

/**
 * Plays the animation gLevelStartAnim on BG map 2 (each frame a picture of gLevelStartPictures with
 * the tiles gUnk_0813A3DC, the palette after them), turning BG2 on, with two sprites on
 * top and songs 0x4A-0x4D at frames 0, 0x20, 0x5A and 0x81, until it ends or A or B is
 * pressed.
 */
void LevelIntro_PlayStartAnim(void)
{
    struct Anim anim;
    struct Sprite *spr;
    s32 frame;
    s32 result;

    anim.timer = 1;
    anim.next = gLevelStartAnim;
    anim.value = gLevelStartAnim[1];
    Anim_Step(&anim);
    Bg_LoadMap8bpp(2, gLevelStartPictures[anim.value].map, gUnk_0813A3DC, gLevelStartPictures[anim.value].list, 0x1E, 0x14);
    Main_QueueCopy(gUnk_0813A3DC + 0x3AA40, (void *)0x05000000, 0x200);
    gDispcntCopy |= 0x400;
    Main_EndFrame();
    frame = 0;
    while ((result = Anim_Step(&anim)) >= 0 && !(gMain.newKeys & (A_BUTTON | B_BUTTON)))
    {
        if (result & 1)
            Bg_LoadMap8bpp(2, gLevelStartPictures[anim.value].map, gUnk_0813A3DC, gLevelStartPictures[anim.value].list, 0x1E, 0x14);
        Sprite_ClearLists();
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0xC0484000 | 0x10;
            spr->attr2 = 0xC520;
        }
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0x80880000 | 0x10;
            spr->attr2 = 0xC540;
        }
        Sprite_BuildOam();
        switch (frame)
        {
        case 0:
            m4aSongNumStart(0x4A);
            break;
        case 0x20:
            m4aSongNumStart(0x4B);
            break;
        case 0x5A:
            m4aSongNumStart(0x4C);
            break;
        case 0x81:
            m4aSongNumStart(0x4D);
            break;
        }
        frame++;
        Main_EndFrame();
    }
}
