// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// collection: the bonus collection screen and its picture tasks.
// 0800EF44-0800F624, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "decompress.h"
#include "task.h"
#include "util.h"
#include "collection.h"
#include "effects.h"

extern const u16 gCollectionEndingPalettes[];
extern const u8 gUnk_081EF088[];

// The task Collection_Run starts with Collection_UpdatePicture (0x24 bytes): one picture of the bonus /
// collection screen, its OBJ tiles animated by anim (Anim_StepLong: the value is the source).
struct CollectionPicture {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ u16 y;
    /*0x0C*/ struct LongAnim anim;
    /*0x18*/ u16 tile;  // its first OBJ tile (gCollectionTiles)
    /*0x1A*/ u16 tiles; // how many tiles each animation frame copies
    /*0x1C*/ void *block; // 0x800 bytes of gMain.heap's heap
    /*0x20*/ u16 picture; // 0-9 an item, 0x10-0x1B the ending's, set by Collection_Run
    /*0x22*/ u8 unk22[2];
};
void Collection_UpdatePicture(struct Task *task);

/**
 * A screen with its own frame loop, in states 0-5 until state -1. State 0 makes the EWRAM
 * buffer from 0x02000000 a heap (gMain.heap, Heap_Init), loads palettes by *mode*,
 * decompresses gCollectionBgPalettes into a 0x200-byte block of it and starts the Collection_UpdatePicture
 * tasks: mode 0 one per set bit 0-9 of gGameState.saveFile->unlocks (it runs only in worlds 1-10,
 * level 2 (a world's third), with gCamera.bonusUnlock set and the bonus taken; mode 3 is mode 0 without
 * those checks, mode 2 mode 0 without its palette), mode 1 the ones numbered 0x10-0x1B.
 * State 1 grows gWindowRadius by 8 a frame (Bg_DrawWindowCircle) up to 0xA0; state 2 waits for A
 * or B (mode 0) or plays a timed sequence of 300 frames (mode 1); state 3 shrinks it again
 * to 0; mode 1 ends through states 4-5 (Effect_BrightnessUp, 32 frames). Then frees the block and
 * copies gTextPalettes to two palettes.
 */
void Collection_Run(s32 mode)
{
    s32 state = 0;
    u8 *block = NULL;
    s32 timer;
    s32 frame;
    s32 i;

    do
    {
        frame = gMain.frameCount;
        switch (state)
        {
        case 0:
            Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
            switch (mode)
            {
            case 0:
                if (gGameState.world > 10 || gGameState.world < 1)
                    return;
                if (gGameState.level <= 1)
                    return;
                if (gCamera.bonusUnlock == 0)
                    return;
                if (!(gGameState.flags & GAME_FLAG_BONUS_TAKEN))
                    return;
                // fall through
            case 3:
                mode = 0;
                Main_QueueCopy(gCollectionPalettes, (void *)0x05000200, 0x200);
                break;
            case 1:
                Main_QueueCopy(gCollectionEndingPalettes, (void *)0x05000200, 0x200);
                break;
            case 2:
                mode = 0;
                break;
            }
            Bg_SetVideoMode(7);
            Bg_LoadScreen(0x12);
            block = Heap_Alloc(gMain.heap, 0x200);
            if (block != NULL)
            {
                Decompress_LZ(gCollectionBgPalettes, block, 0x200);
                Main_QueueCopy(block + 0x100, (void *)0x05000100, 0x100);
            }
            Task_Init();
            gDispcntCopy |= 0x1D00;
            if (mode == 0)
            {
                for (i = 0; i <= 9; i++)
                {
                    if (gGameState.saveFile->unlocks & (1 << i))
                    {
                        struct CollectionPicture *task = (struct CollectionPicture *)Task_Create(Collection_UpdatePicture, sizeof(*task));

                        if (task != NULL)
                            task->picture = i;
                    }
                }
            }
            else if (mode == 1)
            {
                struct CollectionPicture *task;

                for (i = 0x10; i <= 0x19; i++)
                {
                    task = (struct CollectionPicture *)Task_Create(Collection_UpdatePicture, sizeof(*task));
                    if (task != NULL)
                        task->picture = i;
                }
                task = (struct CollectionPicture *)Task_Create(Collection_UpdatePicture, sizeof(*task));
                if (task != NULL)
                    task->picture = 0x1A;
                task = (struct CollectionPicture *)Task_Create(Collection_UpdatePicture, sizeof(*task));
                if (task != NULL)
                    task->picture = 0x1B;
                Main_QueueCopy(gUnk_081EF088, (void *)0x06017FE0, 0x20);
            }
            gMain.frameCount = 0;
            Bg_StartWindowCircle(0, 0x78, 0x50);
            m4aSongNumStart(0x29);
            state = 1;
            break;
        case 1:
            Bg_DrawWindowCircle(gWindowRadius + 8, gWindowX, gWindowY);
            Sprite_ClearLists();
            Task_RunAll();
            if (gWindowRadius > 0xA0)
                state++;
            Sprite_BuildOam();
            break;
        case 2:
            if (mode == 0)
            {
                if (gMain.newKeys & (A_BUTTON | B_BUTTON))
                {
                    m4aSongNumStart(0x21);
                    state++;
                }
            }
            else if (mode == 1)
            {
                if (frame > 0x2C)
                {
                    if (frame <= 0x3C)
                    {
                        gDispcntCopy |= 0x200;
                        Bg_ScrollMapClamped(&gMaps[1], 0, -((0x3C - frame) * 8));
                    }
                    else if (frame == 0xB4)
                        gDispcntCopy &= ~0x200;
                    else if (frame > 300)
                        state = 4;
                }
                if ((u32)(frame - 0x2D) <= 0xC2 && frame % 24 == 0)
                {
                    struct Task *task = Task_Create(Effect_Firework, 8);

                    if (task != NULL)
                        task->state = (frame / 24) & 1;
                }
            }
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 3:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            if (gWindowRadius <= 0)
                state = -1;
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 4:
            gBldcntCopy = 0xFF;
            Task_Create(Effect_BrightnessUp, 0xC);
            timer = 0x20;
            state = 5;
            // fall through
        case 5:
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            if (--timer == 0)
                state = -1;
            break;
        }
        Main_EndFrame();
    } while (state != -1);

    if (block != NULL)
        Heap_Free(gMain.heap, block);
    Main_QueueCopy(gTextPalettes, (void *)0x050003C0, 0x40);
    Main_QueueCopy(gTextPalettes, (void *)0x050001C0, 0x40);
}
// By picture (picture & 0xF): {first OBJ tile, tiles a frame}, {x, y}, and the animation of
// each picture (Collection_UpdatePicture)
extern const u16 gCollectionTiles[][2];
extern const u8 gCollectionPositions[][2];
extern const struct LongAnimFrame *const gCollectionAnims[];

/**
 * The task of struct CollectionPicture: takes a 0x800-byte block (or frees itself without one),
 * sets its tiles, position and animation up by picture picture & 0xF, and each frame (the
 * pictures 0x10 and up restart their own animation, gCollectionAnims[picture], at frame 0x3C)
 * copies the animation's new frames to its tiles and draws its sprites: by picture a 32x64,
 * 64x64 or (9) 32x32 one (palette (picture & 0xF) + 1, priority 2 or 3), picture 5 bobbing by
 * gSineTable of the frame counter, 0x1A and 0x1B two (64x64 and 64x32 below it).
 */
void Collection_UpdatePicture(struct Task *task)
{
    struct CollectionPicture *t = (struct CollectionPicture *)task;
    struct Sprite *spr;
    u32 frame = gMain.frameCount;
    s32 n;

    switch (t->state)
    {
    case 0:
        t->block = Heap_Alloc(gMain.heap, 0x800);
        if (t->block == NULL)
        {
            Task_Destroy(task);
            return;
        }
        n = t->picture & 0xF;
        t->tile = gCollectionTiles[n][0];
        t->tiles = gCollectionTiles[n][1];
        t->x = gCollectionPositions[n][0];
        t->y = gCollectionPositions[n][1];
        t->anim.timer = 1;
        t->anim.next = gCollectionAnims[n];
        t->anim.value = gCollectionAnims[n]->value;
        t->state = 1;
        // fall through
    case 1:
        if (t->picture > 0xF && frame == 0x3C)
        {
            t->anim.timer = 1;
            t->anim.next = gCollectionAnims[t->picture];
            t->anim.value = gCollectionAnims[t->picture]->value;
        }
        if (Anim_StepLong(&t->anim) > 0)
            Main_QueueCopy((const void *)t->anim.value, (u8 *)0x06010000 + t->tile * 32, t->tiles * 32);
        switch (t->picture)
        {
        case 0x1A:
        case 0x1B:
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0xC0000000 | (u8)t->y;
                spr->attr2 = t->tile | (t->picture == 0x1A ? 0x800 : 0xB800);
            }
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0xC0004000 | (u8)(t->y + 0x40);
                spr->attr2 = (t->tile + 0x40) | (t->picture == 0x1A ? 0x800 : 0xB800);
            }
            break;
        case 5:
        case 0x15:
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0xC0008000
                          | (u8)(t->y + (gSineTable[(u8)gMain.frameCount] + 0x20) / 64);
                spr->attr2 = (((t->picture & 0xF) + 1) << 12) | 0x800 | t->tile;
            }
            break;
        case 0:
        case 2:
        case 3:
        case 7:
        case 0x10:
        case 0x12:
        case 0x13:
        case 0x17:
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0xC0008000 | (u8)t->y;
                spr->attr2 = (((t->picture & 0xF) + 1) << 12) | 0x800 | t->tile;
            }
            break;
        case 8:
        case 0x18:
            spr = Sprite_Alloc(12);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0xC0008000 | (u8)t->y;
                spr->attr2 = (((t->picture & 0xF) + 1) << 12) | 0xC00 | t->tile;
            }
            break;
        case 1:
        case 6:
        case 0x11:
        case 0x16:
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0xC0000000 | (u8)t->y;
                spr->attr2 = (((t->picture & 0xF) + 1) << 12) | 0x800 | t->tile;
            }
            break;
        case 4:
        case 0x14:
            spr = Sprite_Alloc(12);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0xC0000000 | (u8)t->y;
                spr->attr2 = (((t->picture & 0xF) + 1) << 12) | 0xC00 | t->tile;
            }
            break;
        case 9:
        case 0x19:
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0x80000000 | (u8)t->y;
                spr->attr2 = (((t->picture & 0xF) + 1) << 12) | 0x800 | t->tile;
            }
            break;
        }
        break;
    }
}
