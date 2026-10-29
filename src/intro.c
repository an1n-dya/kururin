// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// intro: the intro (main state 1) and its tasks.
// 08005F24-080076FC, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "decompress.h"
#include "task.h"
#include "util.h"
#include "menu.h"
#include "intro.h"
#include "effects.h"
#include "sound.h"

extern const u16 gBackgroundAnim1[];
extern const u16 gBackgroundAnim2[];
extern const u16 gBackgroundAnim3[];
extern const struct QueuedCopy gBackgroundCopies[];
extern const u8 gIntroObjPalette[];
extern const u8 gIntroBgPalette[];
extern const u8 gUnk_081BA54C[];
extern const u8 gUnk_081BA64C[];
extern const u8 gUnk_081BA724[];
extern const u8 gUnk_081BA970[];
extern const u8 gUnk_081BA14C[];
extern const u8 gUnk_081BA34C[];
extern const u8 gTitleObjPalette[];
extern const u8 gUnk_081B8540[];
extern const u8 gUnk_081B8D44[];
extern const u8 gUnk_081B994C[];
extern const u8 gUnk_081BC4B8[];
extern const u8 gUnk_081BCA60[];
extern const u8 gUnk_081B6930[];
void Intro_UpdateSprite(struct Task *task);
void Intro_UpdateBgAnim(struct Task *task);
void Intro_UpdateTiltedHelirin(struct Task *task);
void Intro_UpdateMovingHelirin(struct Task *task);
void Intro_UpdateHelirin(struct Task *task);

s32 Intro_MenuItem(struct Menu *p, s32 item);
void Intro_Update(struct Task *task);

/**
 * The game's main state 1, the intro (as long as gMain.state stays 1), over sub-states in
 * gMain.subState: 0-2 stop the music, set up EWRAM from 0x02000000 as gMain.heap's heap,
 * video mode 6, the OBJ tiles gIntroObjTiles, DISPCNT 0x1C40, clear the frame counter and
 * the sprites and start the intro task Intro_Update and a one-item menu (Intro_MenuItem);
 * 3-5 run the tasks and the menu every frame; 6 goes back to 0. After each frame it seeds
 * Random_Shuffle with the frame counter. Once the state changes it runs the tasks for 30
 * more frames.
 */
void Intro_MainState(void)
{
    struct Menu menu;
    s32 i;

    gMain.subState = 2;
    while (gMain.state == 1)
    {
        switch ((s8)gMain.subState)
        {
        case 0:
            gMain.subState = 1;
            // fall through
        case 1:
            gMain.subState = 2;
            // fall through
        case 2:
            m4aSongNumStart(0);
            m4aMPlayAllStop();
            gMain.ewramCursor = (u8 *)0x02000000;
            Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
            Bg_SetVideoMode(6);
            Main_QueueCopy(gIntroObjTiles, (void *)0x06010000, 0x2400);
            gDispcntCopy = 0x1C40;
            gMain.frameCount = 0;
            Sprite_ClearLists();
            Sprite_BuildOam();
            Task_Init();
            Sprite_InitRotations();
            Task_Create(Intro_Update, 0x34);
            menu.func = Intro_MenuItem;
            menu.itemCount = 1;
            menu.current = 1;
            menu.x = 0x50;
            menu.y = 0x80;
            menu.unkE = 0;
            menu.unk10 = 0;
            Task_RunAll();
            gMain.subState = 3;
            break;
        case 3:
        case 4:
        case 5:
            Sprite_ClearLists();
            Menu_RunWrap(&menu, gMain.newKeys);
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 6:
            gMain.subState = 0;
            break;
        }
        Random_Shuffle(gMain.frameCount);
        Main_EndFrame();
    }
    for (i = 29; i >= 0; i--)
    {
        Sprite_ClearLists();
        Task_RunAll();
        Sprite_BuildOam();
        Main_EndFrame();
    }
}
extern const u8 gTextPressStart[];

/**
 * The intro's one-item menu (Intro_MainState): item 0x100 draws gTextPressStart at (0x4E, 0x80)
 * while bits 5-6 of the frame counter are not both clear (blinking); on the current item
 * (1..3), A or Start in sub-state 5 plays song 0x23 and goes on to main state 2; in any
 * other sub-state but 4 it fades the music out, plays song 0x21 and goes to sub-state 4
 * with the frame counter at 0x9BA. Returns 0.
 */
s32 Intro_MenuItem(struct Menu *p, s32 item)
{
    if (item == 0x100)
    {
        if (gMain.frameCount & 0x60)
            Text_DrawSmall(0x4E, 0x80, gTextPressStart, 14);
    }
    else if (item >= 1 && item <= 3)
    {
        if (gMain.newKeys & (A_BUTTON | START_BUTTON))
        {
            if ((s8)gMain.subState == 5)
            {
                m4aSongNumStart(0x23);
                gMain.state = 2;
            }
            else if ((s8)gMain.subState != 4)
            {
                m4aMPlayFadeOut(&gMPlayInfo_BGM, 2);
                m4aSongNumStart(0x21);
                gMain.subState = 4;
                gMain.frameCount = 0x9BA;
            }
        }
    }
    return 0;
}
/**
 * Starts a Effect_RotateBg2 task and three Effect_AnimateCopies tasks, each playing its own animation
 * (gBackgroundAnim1, gBackgroundAnim2, gBackgroundAnim3) with the copies of gBackgroundCopies.
 */
void Intro_StartBackgroundTasks(void)
{
    struct CopyAnim *task;

    Task_Create(Effect_RotateBg2, 8);
    task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
    if (task != NULL)
    {
        task->anim.timer = 1;
        task->anim.next = gBackgroundAnim1;
        task->anim.value = gBackgroundAnim1[1];
        task->copies = gBackgroundCopies;
    }
    task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
    if (task != NULL)
    {
        task->anim.timer = 1;
        task->anim.next = gBackgroundAnim2;
        task->anim.value = gBackgroundAnim2[1];
        task->copies = gBackgroundCopies;
    }
    task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));
    if (task != NULL)
    {
        task->anim.timer = 1;
        task->anim.next = gBackgroundAnim3;
        task->anim.value = gBackgroundAnim3[1];
        task->copies = gBackgroundCopies;
    }
}
/**
 * The intro (a 0x34-byte task of Intro_MainState): a timeline on gMain.frameCount. State 0
 * loads its backgrounds (tiles decompressed through the heap gMain.heap, three 32x32 maps
 * DMA'd into layers 1-3), the window and blending copies and the Intro_UpdateSprite (numbered
 * 1-11) and Sound_UpdateDelayedSong tasks. Then, by frame, it fades in (BLDY from unk28), scrolls
 * layers 1-3 (8.8 fixed point positions unkA-unk16, layer 2 at speed unk1C), changes the
 * backgrounds and starts further tasks at fixed frames, fades out and back in to the title
 * (frame 0x9DC: gMain.subState 4), sets gMain.subState 5 at frame 0x9FC and 6 after frame 0xE0F.
 * Up to frame 0x9D7 it also draws four affine sprites of an all-zero matrix.
 */
void Intro_Update(struct Task *task)
{
    struct IntroTask *t = (struct IntroTask *)task;
    u32 frame = gMain.frameCount;
    struct Task *child;
    struct Sprite *spr;
    s32 idx;
    s32 i;

    switch (t->state)
    {
    case 0:
        Main_QueueCopy(gIntroObjPalette, (void *)0x05000200, 0x200);
        t->block = Heap_Alloc(gMain.heap, 0x8000);
        if (t->block != NULL && Decompress_LZ(gIntroBgTiles, t->block, 0x8000) != 0)
            Main_QueueCopy(t->block, (void *)0x06000000, 0x8000);
        Bg_InitMap(&gMaps[3], &gBgLayers[3], NULL, 0x20, 0x20);
        DmaCopy32(3, gIntroLayer3Map, gBgLayers[3].buffer, 0x800);
        gBgLayers[3].dirty = 1;
        Bg_InitMap(&gMaps[2], &gBgLayers[2], NULL, 0x20, 0x20);
        DmaCopy32(3, gIntroLayer2Map, gBgLayers[2].buffer, 0x800);
        gBgLayers[2].dirty = 1;
        Bg_InitMap(&gMaps[1], &gBgLayers[1], NULL, 0x20, 0x20);
        DmaCopy32(3, gIntroLayer1Map, gBgLayers[1].buffer, 0x800);
        gBgLayers[1].dirty = 1;
        Main_QueueCopy(gIntroBgPalette, (void *)0x05000000, 0x200);
        gBgLayers[3].hofs = 0;
        gBgLayers[2].hofs = 0;
        gBgLayers[1].hofs = 0;
        gBgLayers[3].vofs = 0x88;
        gBgLayers[2].vofs = 0x88;
        gBgLayers[1].vofs = 0x88;
        t->unkE = 0;
        t->unkC = 0;
        t->unkA = 0;
        t->unk16 = 0x8800;
        t->unk14 = 0x8800;
        t->unk12 = 0x8800;
        t->unk1C = 0x20;
        t->unk24 = 0;
        t->unk28 = 0x1000;
        gBldcntCopy = 0xFF;
        gBldyCopy = 0x10;
        gWin0hCopy = 0xF0;
        gWin0vCopy = 0x1878;
        gWininCopy = 0x3F;
        gWinoutCopy = 0x10;
        gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x3E00;
        // the original stores the numbers without testing the tasks for NULL
        for (i = 10; i > 0; i--)
        {
            child = Task_Create(Intro_UpdateSprite, 0x20);
            child->unk8 = i;
        }
        child = Task_Create(Intro_UpdateSprite, 0x20);
        child->unk8 = 11;
        gGameState.paused = 0;
        child = Task_Create(Sound_UpdateDelayedSong, 0xC);
        if (child != NULL)
        {
            child->unk8 = 0x15;
            child->unkA = 0x14;
        }
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    if (t->block != NULL)
        t->block = (void *)Heap_Free(gMain.heap, t->block);

    if (frame <= 0x3B)
    {
        t->unk28 -= 0x80;
        if (t->unk28 <= 0)
            t->unk28 = 0;
        gBldyCopy = t->unk28 >> 8;
        goto scroll;
    }
    if (frame <= 0x1A3)
    {
    scroll:
        t->unkA -= 0x14;
        gBgLayers[1].hofs = t->unkA >> 8;
        t->unkC -= 0x14;
        t->unkE -= 0x80;
        goto common;
    }
    if (frame <= 0x257)
        goto common;
    if (frame == 0x294)
    {
        t->unkC += 0x8000;
        t->unkE += 0x8000;
        child = Task_Create(Intro_UpdateSprite, 0x20);
        child->unk8 = 0;
        child = Task_Create(Intro_UpdateSprite, 0x20);
        child->unk8 = 12;
        goto common;
    }
    if (frame <= 0x385)
        goto common;
    if (frame == 0x386)
    {
        gBgCntCopies[1] = 0x1D09;
        Bg_LoadMap(0, gUnk_081BA54C, gUnk_0813A3DC, (const u16 *)gUnk_081BA64C, 0x1E, 0xC);
        Bg_LoadMap(1, gUnk_081BA724, gUnk_0813A3DC, (const u16 *)gUnk_081BA970, 0x1E, 0xC);
        Main_QueueCopy(gUnk_081BA14C, (void *)0x05000000, 0x200);
        gBgLayers[1].hofs = 0;
        gBgLayers[0].hofs = 0;
        gBgLayers[1].vofs = -0x18;
        gBgLayers[0].vofs = -0x18;
        gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1300;
        goto common;
    }
    if (frame <= 0x3FB)
    {
        if (frame & 0xF)
            Main_QueueCopy(gUnk_081BA34C + ((frame >> 4) % 7) * 0x20, (void *)0x05000080, 0x20);
        goto common;
    }
    if (frame == 0x3FC)
    {
        Task_Create(Intro_UpdateBgAnim, 0x10);
        child = Task_Create(Intro_UpdateSprite, 0x20);
        child->unk8 = 14;
        child = Task_Create(Intro_UpdateSprite, 0x20);
        child->unk8 = 13;
        Main_QueueCopy(gIntroBgPalette, (void *)0x05000000, 0x200);
        gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1E00;
        Main_QueueCopy(gTitleObjPalette, (void *)0x05000200, 0x200);
        t->unk1C = 0x40;
        goto common;
    }
    if (frame <= 0x527)
        goto common;
    if (frame <= 0x563)
    {
        t->unk14 -= 0x180;
        t->unk16 -= 0x180;
        goto common;
    }
    if (frame <= 0x59F)
        goto common;
    if (frame <= 0x7E9)
    {
        t->unkC += 0x100;
        t->unkE += 0x100;
        if (frame <= 0x65B)
            goto common;
        if (frame == 0x67A)
        {
            Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_081B8540, 0x40, 0x10);
            t->unk8 = -0xF0;
            Bg_ScrollMapClamped(&gMaps[0], t->unk8, 0);
            gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1F00;
            goto common;
        }
        if (frame <= 0x6BD)
            goto scrollMap0;
        if (frame == 0x6BE)
        {
            t->unk8 += 4;
            Bg_ScrollMapClamped(&gMaps[0], t->unk8, 0);
            DmaCopy32(3, gUnk_081B8D44, gBgLayers[3].buffer, 0x400);
            t->unk16 = 0;
            gBgLayers[3].dirty = 1;
            gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1B00;
            goto common;
        }
        if (frame <= 0x7A5)
            goto scrollMap0;
        if (frame == 0x7A6)
        {
            Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_081B994C, 0x40, 0x10);
            t->unk8 = -0xF0;
            Bg_ScrollMapClamped(&gMaps[0], t->unk8, 0);
            gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1B00;
            goto common;
        }
        goto scrollMap0;
    }
    if (frame == 0x7EA)
    {
        t->unk8 += 4;
        Bg_ScrollMapClamped(&gMaps[0], t->unk8, 0);
        Bg_LoadMap(1, gUnk_081BC4B8, gUnk_0813A3DC, (const u16 *)gUnk_081BCA60, 0x1E, 0x18);
        gBgLayers[1].hofs = 0;
        gBgLayers[1].vofs = 0x48;
        Main_QueueCopy(gUnk_081B6930, (void *)0x05000000, 0xA0);
        Task_Create(Intro_UpdateTiltedHelirin, 0x20);
        gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1300;
        goto common;
    }
    if (frame <= 0x8E7)
    {
    scrollMap0:
        t->unk8 += 4;
        Bg_ScrollMapClamped(&gMaps[0], t->unk8, 0);
        goto common;
    }
    if (frame == 0x8E8)
    {
        gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x1200;
        gBgLayers[1].vofs = -0x18;
        goto common;
    }
    if (frame <= 0x8F4)
        goto common;
    if (frame == 0x8F5)
    {
        Task_Create(Intro_UpdateMovingHelirin, 0x44);
        goto common;
    }
    if (frame <= 0x9B9)
        goto common;
    if (frame == 0x9BA)
    {
        if ((s8)gMain.subState == 4)
            gBldcntCopy = 0xFF;
        else
            gBldcntCopy = 0xEF;
        goto common;
    }
    if (frame <= 0x9DB)
    {
        t->unk28 += 0x80;
        if (t->unk28 > 0xFFF)
            t->unk28 = 0x1000;
        gBldyCopy = t->unk28 >> 8;
        goto common;
    }
    if (frame == 0x9DC)
    {
        Bg_SetVideoMode(9);
        Bg_LoadScreen(0);
        Intro_StartBackgroundTasks();
        if ((s8)gMain.subState == 4)
        {
            Task_DestroyByKey(4);
            Task_Create(Intro_UpdateHelirin, 0x44);
            Main_QueueCopy(gTitleObjPalette, (void *)0x05000200, 0x200);
            gBldcntCopy = 0xFF;
        }
        else
        {
            gBldcntCopy = 0xEF;
        }
        t->unk28 = 0x1000;
        gBldyCopy = 0x10;
        gDispcntCopy = (gDispcntCopy & 0xC0FF) | 0x1600;
        gMain.subState = 4;
        return;
    }
    if (frame <= 0x9EB)
    {
        t->unk28 -= 0x100;
        gBldyCopy = t->unk28 >> 8;
        goto common;
    }
    if (frame == 0x9EC)
    {
        gBldcntCopy = 0x8B;
        goto common;
    }
    if (frame <= 0x9FB)
    {
        t->unk28 += 0x100;
        gBldyCopy = t->unk28 >> 8;
        goto common;
    }
    if (frame <= 0xA8B)
    {
        if (frame == 0x9FC)
            gMain.subState = 5;
        if (frame == 0xA1E)
            m4aSongNumStart(0x3C);
        t->unk28 -= 0x40;
        if (t->unk28 <= 0)
            t->unk28 = 0;
        gBldyCopy = t->unk28 >> 8;
        goto common;
    }
    if (frame > 0xE0F)
    {
        gMain.subState = 6;
        return;
    }

common:
    if (frame > 0x9D7)
        return;
    t->unkC += t->unk1C;
    gBgLayers[2].hofs = t->unkC >> 8;
    gBgLayers[3].hofs = t->unkE >> 8;
    gBgLayers[2].vofs = t->unk14 >> 8;
    gBgLayers[3].vofs = t->unk16 >> 8;
    idx = Sprite_AllocAffine();
    if (idx < 0)
        return;
    Sprite_SetAffine(idx, 0, 0, 0, 0);
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = (idx << 25) | 0xC0004300 | 0xD8;
        spr->attr2 = 0xE3EC;
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = (idx << 25) | 0xC0804300 | 0xD8;
        spr->attr2 = 0xE3EC;
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = (idx << 25) | 0xC0004300 | 0x78;
        spr->attr2 = 0xE3EC;
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = (idx << 25) | 0xC0804300 | 0x78;
        spr->attr2 = 0xE3EC;
    }
}
// The task Intro_UpdateBgAnim (0x10 bytes): an animation of BG map 1 pictures
struct IntroBgAnim {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ struct Anim anim;
};
extern const u16 gIntroBgAnim[];
// The pictures of gIntroBgAnim's frames: a tile map and its tile list (Bg_LoadMap)
extern const struct { const u8 *map; const u16 *list; } gIntroBgAnimPictures[];

/**
 * An intro task (Intro_Update starts it at frame 0x3FC): plays the animation gIntroBgAnim
 * on BG map 1 (30 x 12 tiles of gUnk_0813A3DC per frame, gIntroBgAnimPictures), with the layer at
 * (0, -0x18), until the frame counter passes 0x7E9, then frees itself.
 */
void Intro_UpdateBgAnim(struct Task *task)
{
    struct IntroBgAnim *t = (struct IntroBgAnim *)task;
    u32 frame = gMain.frameCount;

    switch (t->state)
    {
    case 0:
        t->key = 4;
        t->anim.timer = 1;
        t->anim.next = gIntroBgAnim;
        t->anim.value = gIntroBgAnim[1];
        t->state = 1;
        // fall through
    case 1:
        if (frame > 0x7E9)
        {
            Task_Destroy(task);
            break;
        }
        if (Anim_Step(&t->anim) & 1)
            Bg_LoadMap(1, gIntroBgAnimPictures[t->anim.value].map, gUnk_0813A3DC, gIntroBgAnimPictures[t->anim.value].list, 0x1E, 0xC);
        gBgLayers[1].hofs = 0;
        gBgLayers[1].vofs = -0x18;
        break;
    }
}
// The task Intro_UpdateTiltedHelirin (0x20 bytes, started by the intro task Intro_Update at frame 0x7EA): a
// 64x32 sprite moved by its animation (anim.value: a direction, 0xFF: none) and cycling
// through 18 frames of tiles as phase advances by rate. x and y are 16.16 fixed point.
struct IntroTiltedHelirin {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s32 phase;
    /*0x0C*/ s16 rate;
    /*0x0E*/ u16 unkE;
    /*0x10*/ struct Anim anim; // gIntroTiltedHelirinPath
    /*0x18*/ s32 x;
    /*0x1C*/ s32 y;
};
extern const u8 gIntroTiltedHelirinTiles[];
extern const u16 gIntroTiltedHelirinPath[];

/**
 * The task of struct IntroTiltedHelirin: copies its 0x4800 bytes of OBJ tiles (gIntroTiltedHelirinTiles) to
 * 0x06012400 and starts at (0xA0, 0x98); every frame it steps its animation (freeing itself
 * at the end), moves 0x180 times the step of gSineTable/gCosineTable in the frame's
 * direction and draws a 64x32 sprite centred on its position (tiles 0x120 + 32 * frame). On
 * screen (kurutest --shot of the original) the 18 frames are a Helirin drawn flattened, spinning
 * as it flies along the canyon of the title's map.
 */
void Intro_UpdateTiltedHelirin(struct Task *task)
{
    struct IntroTiltedHelirin *t = (struct IntroTiltedHelirin *)task;
    struct Sprite *spr;
    u8 dir;
    u16 tile;

    switch (t->state)
    {
    case 0:
        t->key = 4;
        Main_QueueCopy(gIntroTiltedHelirinTiles, (void *)0x06012400, 0x4800);
        t->phase = 0x5554;
        t->rate = -0xB6;
        t->anim.timer = 1;
        t->anim.next = gIntroTiltedHelirinPath;
        t->anim.value = gIntroTiltedHelirinPath[1];
        POS_PIXEL(t->x) = 0xA0;
        POS_PIXEL(t->y) = 0x98;
        t->state = 1;
        // fall through
    case 1:
        if (Anim_Step(&t->anim) < 0)
        {
            Task_Destroy(task);
            break;
        }
        if (t->anim.value != 0xFF)
        {
            dir = t->anim.value;
            t->x += gSineTable[dir] * 0x180;
            t->y += -gCosineTable[dir] * 0x180;
        }
        t->phase += t->rate;
        tile = ((((t->phase * 2) & 0xFFFF) * 18) >> 16) * 32 + 0x120;
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = (((POS_PIXEL(t->x) - 0x20) << 16) & 0x1FF0000) | 0xC0004000
                      | (u8)(((u8 *)&t->y)[2] - 0x10);
            spr->attr2 = tile | 0x4400;
        }
        break;
    }
}
// The tasks Intro_UpdateHelirin (started by the intro task Intro_Update at frame 0x9DC when
// gMain.subState is 4) and Intro_UpdateMovingHelirin (0x44 bytes): a rotating 64x64 sprite. x and y are 16.16
// fixed point.
struct IntroHelirin {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s32 angle; // 8.8: the rotation (Sprite_SetAffineRotation of angle >> 8)
    /*0x0C*/ s16 rate;  // added to angle every frame
    /*0x0E*/ u8 unkE[0xE];
    /*0x1C*/ struct Anim anim; // Intro_UpdateMovingHelirin: value a direction to move in (0xFF: none)
    /*0x24*/ s32 x;
    /*0x28*/ s32 y;
    /*0x2C*/ u8 unk2C[0x18];
};
extern const u8 gIntroHelirinCenterTile[];

/**
 * The task of struct IntroHelirin: clears 0x400 bytes of OBJ tiles at 0x060161A0, copies
 * eight rows of 0x40 bytes from gHelirinGfx + gHelirinSizeGfx[0][0] to 0x060161C0, 0x80 bytes
 * apart, and the 0x20 bytes gIntroHelirinCenterTile to 0x06016180, then every frame turns its
 * affine set (Sprite_AllocAffine) by rate and draws a double-size affine 64x64 sprite (tile
 * 0x30D) at (x - 0x20, y - 0x40) and an 8x8 one (tile 0x30C) at (x - 4, y - 4).
 */
void Intro_UpdateHelirin(struct Task *task)
{
    struct IntroHelirin *t = (struct IntroHelirin *)task;
    struct Sprite *spr;
    s32 n;
    s32 i;

    switch (t->state)
    {
    case 0:
        t->key = 4;
        Main_QueueFill(0, (void *)0x060161A0, 0x400);
        for (i = 0; i < 8; i++)
            Main_QueueCopy(gHelirinGfx + gHelirinSizeGfx[0][0].offset + i * 0x40, (u8 *)0x060161C0 + i * 0x80, 0x40);
        Main_QueueCopy(gIntroHelirinCenterTile, (void *)0x06016180, 0x20);
        t->angle = 0x3000;
        t->rate = -0xB6;
        POS_PIXEL(t->x) = 0xBC;
        POS_PIXEL(t->y) = 0x43;
        t->state = 1;
        // fall through
    case 1:
        t->angle += t->rate;
        n = Sprite_AllocAffine();
        Sprite_SetAffineRotation(n, t->angle >> 8);
        spr = Sprite_Alloc(6);
        if (spr != NULL)
        {
            spr->attr01 = (n << 25) | (((POS_PIXEL(t->x) - 0x20) << 16) & 0x1FF0000) | 0xC0008300
                      | (u8)(POS_PIXEL(t->y) - 0x40);
            spr->attr2 = 0x70D;
        }
        spr = Sprite_Alloc(6);
        if (spr != NULL)
        {
            spr->attr01 = (((POS_PIXEL(t->x) - 4) << 16) & 0x1FF0000) | (u8)(POS_PIXEL(t->y) - 4);
            spr->attr2 = 0x70C;
        }
        break;
    }
}
extern const u8 gIntroHelirinTiles[];
extern const u16 gIntroHelirinPath[];

/**
 * Intro_UpdateHelirin with its own tiles (eight rows of 0x40 bytes from gIntroHelirinTiles, then 0x20
 * bytes from gIntroHelirinTiles + 0x200), starting at (0x38, 0x98) and moved by its animation
 * gIntroHelirinPath as Intro_UpdateTiltedHelirin is. At frame 0x9DC of the intro (state 1) it takes
 * Intro_UpdateHelirin's tiles instead.
 */
void Intro_UpdateMovingHelirin(struct Task *task)
{
    struct IntroHelirin *t = (struct IntroHelirin *)task;
    struct Sprite *spr;
    u32 frame = gMain.frameCount;
    u8 dir;
    s32 n;
    s32 i;

    switch (t->state)
    {
    case 0:
        t->key = 4;
        Main_QueueFill(0, (void *)0x060161A0, 0x400);
        for (i = 0; i < 8; i++)
            Main_QueueCopy(gIntroHelirinTiles + i * 0x40, (u8 *)0x060161C0 + i * 0x80, 0x40);
        Main_QueueCopy(gIntroHelirinTiles + 0x200, (void *)0x06016180, 0x20);
        t->angle = 0x3000;
        t->rate = -0xB6;
        t->anim.timer = 1;
        t->anim.next = gIntroHelirinPath;
        t->anim.value = gIntroHelirinPath[1];
        POS_PIXEL(t->x) = 0x38;
        POS_PIXEL(t->y) = 0x98;
        t->state = 1;
        // fall through
    case 1:
        if (frame == 0x9DC)
        {
            for (i = 0; i < 8; i++)
                Main_QueueCopy(gHelirinGfx + gHelirinSizeGfx[0][0].offset + i * 0x40, (u8 *)0x060161C0 + i * 0x80, 0x40);
            Main_QueueCopy(gIntroHelirinCenterTile, (void *)0x06016180, 0x20);
        }
        // fall through
    case 2:
        Anim_Step(&t->anim);
        if (t->anim.value != 0xFF)
        {
            dir = t->anim.value;
            t->x += gSineTable[dir] * 0x180;
            t->y += -gCosineTable[dir] * 0x180;
        }
        t->angle += t->rate;
        n = Sprite_AllocAffine();
        Sprite_SetAffineRotation(n, t->angle >> 8);
        spr = Sprite_Alloc(6);
        if (spr != NULL)
        {
            spr->attr01 = (n << 25) | (((POS_PIXEL(t->x) - 0x20) << 16) & 0x1FF0000) | 0xC0008300
                      | (u8)(POS_PIXEL(t->y) - 0x40);
            spr->attr2 = 0x70D;
        }
        spr = Sprite_Alloc(6);
        if (spr != NULL)
        {
            spr->attr01 = (((POS_PIXEL(t->x) - 4) << 16) & 0x1FF0000) | (u8)(POS_PIXEL(t->y) - 4);
            spr->attr2 = 0x70C;
        }
        break;
    }
}
extern const struct IntroSpriteFrame gUnk_080C3574[];
extern const struct IntroSpriteFrame gUnk_080C3704[];
extern const u16 gUnk_080C2F2C[];
extern const u16 gUnk_080C33B0[];
extern const u16 gUnk_080C3404[];
extern const u16 gUnk_080C3418[];
extern const u16 gUnk_080C3504[];
extern const u16 gUnk_080C3550[];

/**
 * One sprite of the intro (struct IntroSprite, number n 0-14): from its start position it
 * moves on its own timeline of the intro's frame counter (most run right from x 0x100 or
 * flip and speed up after their frame; 11-13 change their animation and leave the screen,
 * 13 jumps along gCosineTable), and frees itself off screen, at a set frame or at the end
 * of its frames. Each new animation frame copies its tiles; the sprite is drawn with the
 * frame's attributes and offset.
 */
void Intro_UpdateSprite(struct Task *task)
{
    struct IntroSprite *t = (struct IntroSprite *)task;
    u32 frame = gMain.frameCount;
    const struct IntroSpriteFrame *e;
    const struct IntroSpriteFrame *frames;
    const u16 *anim;
    struct Sprite *spr;
    u32 flip;
    u16 tile;
    s16 x, y;
    s32 d;

    switch (t->state)
    {
    case 0:
        t->key = 4;
        y = 0x6C;
        switch (t->n)
        {
    case 0:
        tile = 0x120;
        frames = gUnk_080C2F38;
        anim = gUnk_080C2F10;
        x = 0x40;
        break;
    case 1:
        tile = 0x140;
        frames = gUnk_080C2FCC;
        anim = gUnk_080C2FB8;
        x = 0x18;
        break;
    case 2:
        tile = 0x150;
        frames = gUnk_080C3020;
        anim = gUnk_080C300C;
        x = 0x2C;
        break;
    case 3:
        tile = 0x160;
        frames = gUnk_080C306C;
        anim = gUnk_080C3050;
        x = 0x40;
        break;
    case 4:
        tile = 0x170;
        frames = gUnk_080C30D0;
        anim = gUnk_080C30BC;
        x = 0x54;
        break;
    case 5:
        tile = 0x180;
        frames = gUnk_080C314C;
        anim = gUnk_080C3120;
        x = 0x68;
        break;
    case 6:
        tile = 0x1A0;
        frames = gUnk_080C31D0;
        anim = gUnk_080C31AC;
        x = 0x7F;
        break;
    case 7:
        tile = 0x1C0;
        frames = gUnk_080C323C;
        anim = gUnk_080C3220;
        x = 0x90;
        break;
    case 8:
        tile = 0x1E0;
        frames = gUnk_080C32B0;
        anim = gUnk_080C329C;
        x = 0xA4;
        break;
    case 9:
        tile = 0x1E8;
        frames = gUnk_080C32FC;
        anim = gUnk_080C32E0;
        x = 0xB8;
        break;
    case 10:
        tile = 0x208;
        frames = gUnk_080C335C;
        anim = gUnk_080C333C;
        x = 0xC8;
        break;
    case 11:
        tile = 0x210;
        frames = gUnk_080C3424;
        anim = gUnk_080C338C;
        x = 0x0;
        break;
    case 12:
        tile = 0x210;
        frames = gUnk_080C3424;
        anim = gUnk_080C3404;
        x = 0x110;
        break;
    case 13:
        tile = 0x120;
        frames = gUnk_080C3574;
        anim = gUnk_080C3504;
        x = 0x14;
        break;
    case 14:
        tile = 0x140;
        frames = gUnk_080C3704;
        anim = gUnk_080C3550;
        x = 0x78;
        y = 0x50;
        break;
        default:
            goto started;
        }
        t->tile = tile;
        t->frames = frames;
        t->anim.timer = 1;
        t->anim.next = anim;
        t->anim.value = anim[1];
        t->x = x << 16;
        t->y = y << 16;
    started:
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    flip = 0;
    switch (t->n)
    {
    case 0:
        if (frame <= 0x30B)
        {
            t->x += 0x4000;
            flip = 0x10000000;
        }
        else if (frame == 0x30C)
        {
            t->anim.timer = 1;
            t->anim.next = gUnk_080C2F2C;
            t->anim.value = gUnk_080C2F2C[1];
        }
        else if (frame == 0x384)
        {
            goto free;
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
    case 10:
        if (POS_PIXEL(t->x) > 0x100)
            goto free;
        // after its frame it turns round and speeds up
        switch (t->n)
        {
        case 1: d = 0x12C; break;
        case 2: d = 0x118; break;
        case 3: d = 0x104; break;
        case 4: d = 0xF0; break;
        case 5: d = 0xDC; break;
        case 7: d = 0xB4; break;
        case 8: d = 0xA0; break;
        case 9: d = 0x8C; break;
        default: d = 0x78; break;
        }
        if (frame > d)
        {
            flip = 0x10000000;
            t->x += 0x10000;
        }
        else
        {
            t->x += 0x5000;
        }
        break;
    case 6:
        if (POS_PIXEL(t->x) > 0x100)
            goto free;
        if (frame > 0xC8)
        {
            flip = 0x10000000;
            t->x += 0x10000;
            t->y += -0x4000;
        }
        else
        {
            t->x += 0x5000;
            t->y += -0x1000;
        }
        break;
    case 11:
        if (POS_PIXEL(t->x) < 0)
            goto free;
        if (frame <= 0x1A3)
        {
            t->x += 0x4000;
        }
        else if (frame == 0x1A4)
        {
            t->anim.timer = 1;
            t->anim.next = gUnk_080C33B0;
            t->anim.value = gUnk_080C33B0[1];
        }
        else if (frame > 0x257)
        {
            t->x += -0x30000;
        }
        break;
    case 12:
        if (frame <= 0x2DE)
            break;
        if (frame <= 0x30B)
        {
            t->x += -0x30000;
        }
        else if (frame == 0x30C)
        {
            t->anim.timer = 1;
            t->anim.next = gUnk_080C3418;
            t->anim.value = gUnk_080C3418[1];
        }
        else if (frame == 0x384)
        {
            goto free;
        }
        break;
    case 13:
        if (frame <= 0x437)
        {
            t->x += 0x10000;
        }
        else if (frame <= 0x43B)
        {
            t->x += 0x10000;
            t->y += -0x40000;
        }
        else if (frame <= 0x45F)
        {
            d = gCosineTable[(u8)((s32)((frame - 0x43C) << 7) / 36)];
            t->x += 0x10000;
            t->y -= d * 0x300;
        }
        break;
    }

    if (Anim_Step(&t->anim) & 1)
    {
        e = &t->frames[t->anim.value];
        if (e->src == NULL)
            goto free;
        Main_QueueCopy(e->src, (u8 *)0x06010000 + t->tile * 32, e->tiles * 32);
    }
    else
    {
        e = &t->frames[t->anim.value];
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = (e->attr | flip) | (((POS_PIXEL(t->x) + e->dx) << 16) & 0x1FF0000)
                  | (u8)(e->dy + POS_PIXEL(t->y));
        spr->attr2 = e->attr2 | t->tile;
    }
    return;

free:
    Task_Destroy(task);
}
