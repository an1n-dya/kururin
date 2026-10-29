// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// ending: the ending (main state 4) and its tasks.
// 080076FC-08008200, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "decompress.h"
#include "task.h"
#include "util.h"
#include "save.h"
#include "collection.h"
#include "effects.h"
#include "sound.h"

void Ending_UpdateBackground(struct IntroTask *task);
struct EndingText;
void Ending_UpdateText(struct EndingText *task);
void Ending_UpdateSprite(struct Task *task);

/**
 * The ending (main state 4, as long as gMain.state stays 4), after Collection_Run(1), over
 * sub-states in gMain.subState: 0 sets up EWRAM from 0x02000000 as gMain.heap's heap, video
 * mode 8, the OBJ tiles gIntroObjTiles, DISPCNT 0x40, clears the frame counter and starts the
 * tasks Ending_UpdateBackground and Ending_UpdateText and a Sound_UpdateDelayedSong task (song 0x17 after 0x14 frames);
 * 2 runs the tasks: at frame 0x16F8 it fades the music out, at 0x189C it sets bit 5 of the
 * save file's flags and saves (Save_Write), after that A or Start starts Effect_BrightnessUp and
 * goes to 3, which waits 0x78 frames more and returns to main state 1.
 */
void Ending_MainState(void)
{
    u8 unused[0xC0] __attribute__((unused)); // the original reserves 0xC0 bytes of stack it never uses
    u32 frame;
    s32 timer;

    gMain.subState = 0;
    Collection_Run(1);
    while (gMain.state == 4)
    {
        frame = gMain.frameCount;
        switch ((s8)gMain.subState)
        {
        case 0:
        {
            struct Task *task;

            m4aSongNumStart(0);
            m4aMPlayAllStop();
            gMain.ewramCursor = (u8 *)0x02000000;
            Heap_Init(gMain.heap, (void *)0x02000000, 0x36B9C);
            Bg_SetVideoMode(8);
            Main_QueueCopy(gIntroObjTiles, (void *)0x06010000, 0x2400);
            gDispcntCopy = 0x40;
            gMain.frameCount = 0;
            Sprite_ClearLists();
            Sprite_BuildOam();
            Task_Init();
            Sprite_InitRotations();
            Task_Create((void (*)(struct Task *))Ending_UpdateBackground, 0x34);
            Task_Create((void (*)(struct Task *))Ending_UpdateText, 0x110);
            Task_RunAll();
            task = Task_Create(Sound_UpdateDelayedSong, 0xC);
            if (task != NULL)
            {
                task->unk8 = 0x17;
                task->unkA = 0x14;
            }
            gMain.subState = 2;
            break;
        }
        case 2:
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            if (frame == 0x16F8)
            {
                m4aMPlayFadeOut(&gMPlayInfo_BGM, 0x1A);
            }
            else if (frame == 0x189C)
            {
                gGameState.saveFile->flags |= 0x20;
                Save_Write();
            }
            else if (frame > 0x189C && (gMain.newKeys & (A_BUTTON | START_BUTTON)))
            {
                gBldcntCopy = 0xFF;
                Task_Create(Effect_BrightnessUp, 0xC);
                timer = 0x78;
                gMain.subState = 3;
            }
            break;
        case 3:
            Sprite_ClearLists();
            Task_RunAll();
            Sprite_BuildOam();
            if (timer == 0)
                gMain.state = 1;
            else
                timer--;
            break;
        }
        Main_EndFrame();
    }
}
extern const u8 gEndingObjPalette[];
extern const u8 gEndingBgPalette[];
extern u16 gWin1hCopy;
extern u16 gWin1vCopy;

/**
 * The ending's background task (struct IntroTask, like the intro's Intro_Update): state 0
 * loads the OBJ palette gEndingObjPalette, the intro's BG tiles (gIntroBgTiles, through a
 * 0x8000-byte buffer on gMain.heap) and layer maps 1-3, the BG palette gEndingBgPalette, puts
 * the three layers at (0, 0x98), sets up blending and the two windows, and starts twelve
 * Ending_UpdateSprite tasks (numbered 10 down to 0, then 11) and Effect_BrightnessDown. Then it frees the
 * buffer and every frame scrolls layers 1-3 left by unk1A, unk1C and unk1E (8.8 fixed point).
 */
void Ending_UpdateBackground(struct IntroTask *t)
{
    struct Task *child;
    s32 i;

    switch (t->state)
    {
    case 0:
        Main_QueueCopy(gEndingObjPalette, (void *)0x05000200, 0x200);
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
        Main_QueueCopy(gEndingBgPalette, (void *)0x05000000, 0x200);
        gBgLayers[3].hofs = 0;
        gBgLayers[2].hofs = 0;
        gBgLayers[1].hofs = 0;
        gBgLayers[3].vofs = 0x98;
        gBgLayers[2].vofs = 0x98;
        gBgLayers[1].vofs = 0x98;
        t->unkE = 0;
        t->unkC = 0;
        t->unkA = 0;
        t->unk16 = 0x9800;
        t->unk14 = 0x9800;
        t->unk12 = 0x9800;
        t->unk1A = -0x10;
        t->unk22 = 0;
        t->unk1C = -0x20;
        t->unk24 = 0;
        t->unk1E = -0x60;
        t->unk26 = 0;
        t->unk28 = 0xFFF;
        gBldcntCopy = 0xFF;
        gBldyCopy = 0xF;
        gWin0hCopy = 0xF0;
        gWin1hCopy = 0xF0;
        gWin0vCopy = 0x868;
        gWin1vCopy = 0x69A0;
        gWininCopy = 0x303F;
        gWinoutCopy = 0;
        gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x7E00;
        // the original stores the numbers without testing the tasks for NULL
        for (i = 10; i >= 0; i--)
        {
            child = Task_Create(Ending_UpdateSprite, 0x20);
            child->unk8 = i;
        }
        child = Task_Create(Ending_UpdateSprite, 0x20);
        child->unk8 = 11;
        Task_Create(Effect_BrightnessDown, 0xC);
        t->state = 1;
        // fall through
    case 1:
        if (t->block != NULL)
            t->block = (void *)Heap_Free(gMain.heap, t->block);
        t->unkA += t->unk1A;
        gBgLayers[1].hofs = t->unkA >> 8;
        gBgLayers[1].vofs = t->unk12 >> 8;
        t->unkC += t->unk1C;
        gBgLayers[2].hofs = t->unkC >> 8;
        gBgLayers[2].vofs = t->unk14 >> 8;
        t->unkE += t->unk1E;
        gBgLayers[3].hofs = t->unkE >> 8;
        gBgLayers[3].vofs = t->unk16 >> 8;
        break;
    }
}
extern const u16 gEndingSprite10Anim[];

/**
 * One sprite of the ending (struct IntroSprite, number n 0-11, started by Ending_UpdateBackground),
 * the intro's Intro_UpdateSprite going the other way: from x 0xE6-0x1CC it walks left 0x1400 a
 * frame and frees itself 16 pixels past the left edge; 6 also bobs (gSineTable of the
 * frame counter) and rises after frame 0x1067; 10 stops at frame 0x1680 to play
 * gEndingSprite10Anim, moving right except on its animation frame 2. Each new animation frame
 * copies its tiles; the sprite is drawn with the frame's attributes and offset.
 */
void Ending_UpdateSprite(struct Task *task)
{
    struct IntroSprite *t = (struct IntroSprite *)task;
    u32 frame = gMain.frameCount;
    const struct IntroSpriteFrame *e;
    const struct IntroSpriteFrame *frames;
    const u16 *anim;
    struct Sprite *spr;
    u16 tile;
    s16 x, y;

    switch (t->state)
    {
    case 0:
        y = 0x5C;
        switch (t->n)
        {
        case 0:
            tile = 0x120;
            frames = gUnk_080C2F38;
            anim = gUnk_080C2F10;
            x = 0x100;
            break;
        case 1:
            tile = 0x140;
            frames = gUnk_080C2FCC;
            anim = gUnk_080C2FB8;
            x = 0x118;
            break;
        case 2:
            tile = 0x150;
            frames = gUnk_080C3020;
            anim = gUnk_080C300C;
            x = 0x12C;
            break;
        case 3:
            tile = 0x160;
            frames = gUnk_080C306C;
            anim = gUnk_080C3050;
            x = 0x140;
            break;
        case 4:
            tile = 0x170;
            frames = gUnk_080C30D0;
            anim = gUnk_080C30BC;
            x = 0x154;
            break;
        case 5:
            tile = 0x180;
            frames = gUnk_080C314C;
            anim = gUnk_080C3120;
            x = 0x168;
            break;
        case 6:
            tile = 0x1A0;
            frames = gUnk_080C31D0;
            anim = gUnk_080C31AC;
            x = 0x17F;
            y = 0x54;
            break;
        case 7:
            tile = 0x1C0;
            frames = gUnk_080C323C;
            anim = gUnk_080C3220;
            x = 0x190;
            break;
        case 8:
            tile = 0x1E0;
            frames = gUnk_080C32B0;
            anim = gUnk_080C329C;
            x = 0x1A4;
            break;
        case 9:
            tile = 0x1E8;
            frames = gUnk_080C32FC;
            anim = gUnk_080C32E0;
            x = 0x1B8;
            break;
        case 10:
            tile = 0x208;
            frames = gUnk_080C335C;
            anim = gUnk_080C333C;
            x = 0x1CC;
            break;
        case 11:
            tile = 0x210;
            frames = gUnk_080C3424;
            anim = gUnk_080C338C;
            x = 0xE6;
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

    switch (t->n)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
    case 11:
        if (POS_PIXEL(t->x) < -16)
            goto free;
        t->x += -0x1400;
        break;
    case 6:
        if (POS_PIXEL(t->x) < -16)
            goto free;
        if (frame <= 0x1067)
        {
            t->x += -0x1400;
            t->y += gSineTable[(u8)frame] << 4;
        }
        else
        {
            t->x += -0x1400;
            t->y += -0x1000 + (gSineTable[(u8)frame] << 4);
        }
        break;
    case 10:
        if (POS_PIXEL(t->x) < -16)
            goto free;
        if (frame <= 0x167F)
        {
            t->x += -0x1400;
        }
        else if (frame == 0x1680)
        {
            t->anim.timer = 1;
            t->anim.next = gEndingSprite10Anim;
            t->anim.value = gEndingSprite10Anim[1];
            t->x += 0x6000;
        }
        else if (frame <= 0x16D9)
        {
            t->x += 0x6000;
        }
        else if (t->anim.value == 2)
        {
            t->x += -0x8000;
        }
        else
        {
            t->x += 0x6000;
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
        spr->attr01 = e->attr | (((POS_PIXEL(t->x) + e->dx) << 16) & 0x1FF0000);
        spr->attr2 = e->attr2 | t->tile;
        *(u8 *)&spr->attr01 = e->dy + POS_PIXEL(t->y);
    }
    return;

free:
    Task_Destroy(task);
}
// The letters of the ending's text (Ending_UpdateText starts one per character, 0x18 bytes).
struct EndingLetter {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 x;       // starts 0x100 left of targetX
    /*0x0A*/ u16 y;
    /*0x0C*/ u16 targetX;
    /*0x0E*/ u16 timer;   // frames it stays at targetX
    /*0x10*/ u16 tile;    // the character - 0x20
    /*0x12*/ u16 phase;   // a byte angle of its bounce (gSineTable)
    /*0x14*/ u16 palette;
    /*0x16*/ u16 unk16;
};
void Ending_UpdateLetter(struct EndingLetter *t);
// gEndingText: the ending's text, a page at a time: an entry whose x is the number of
// lines, then that many lines (Ending_UpdateText); a page of 0 lines ends it.
struct EndingTextLine {
    /*0x00*/ u8 x;      // the first letter's x; the line count in a page's first entry
    /*0x01*/ u8 y;
    /*0x02*/ u16 timer; // how long its letters stand (EndingLetter.timer)
    /*0x04*/ const u8 *text; // Text_ConvertToU16
};
extern const struct EndingTextLine gEndingText[];
// The ending's text task (0x110 bytes, started by Ending_MainState).
struct EndingText {
    /*0x000*/ void (*func)(struct Task *);
    /*0x004*/ u16 key;
    /*0x006*/ s16 state;
    /*0x008*/ u16 lines[4][0x20]; // the page's lines as halfword characters (Text_ConvertToU16)
    /*0x108*/ u16 timer;   // frames before the next page
    /*0x10A*/ u16 count;   // lines of the page
    /*0x10C*/ u16 column;  // the next letter to start
    /*0x10E*/ u16 index;   // the page's first line in gEndingText
};

/**
 * The ending's text (struct EndingText): after 0x3C frames it reads a page of
 * gEndingText (Text_ConvertToU16 into lines[]) and every 6 frames starts the letters of the
 * next column (0-0x17) of every line as Ending_UpdateLetter tasks, at the line's x + 8 per column
 * and its y; 0xB4 frames after the last column the next page follows. A page of 0 lines
 * frees it.
 */
void Ending_UpdateText(struct EndingText *t)
{
    u32 frame = gMain.frameCount;
    const struct EndingTextLine *e;
    struct EndingLetter *letter;
    u32 ch;
    s32 i, j;
    u32 n;

    switch (t->state)
    {
    case 0:
        t->state++;
        t->index = 0;
        t->timer = 0x3C;
        break;
    case 1:
        if (--t->timer != 0)
            break;
        for (i = 0; i < 4; i++)
        {
            for (j = 0x1F; j >= 0; j--)
                t->lines[i][j] = 0;
        }
        t->count = gEndingText[t->index++].x;
        if (t->count == 0)
        {
            Task_Destroy((struct Task *)t);
            break;
        }
        e = &gEndingText[t->index];
        for (i = 0; i < t->count; i++)
            Text_ConvertToU16(e[i].text, t->lines[i]);
        t->column = 0;
        t->state++;
        break;
    case 2:
        if (frame % 6 != 0)
            break;
        n = t->index;
        for (i = 0; i < t->count; i++, n++)
        {
            ch = t->lines[i][t->column];
            if (ch != 0 && ch != ' ')
            {
                // the original does not test the task for NULL
                letter = (struct EndingLetter *)Task_Create((void (*)(struct Task *))Ending_UpdateLetter, 0x18);
                e = &gEndingText[n];
                letter->targetX = t->column * 8 + e->x;
                letter->x = letter->targetX - 0x100;
                letter->y = e->y;
                letter->timer = e->timer;
                letter->tile = ch - 0x20;
            }
        }
        if (++t->column > 0x17)
        {
            t->index += t->count;
            t->timer = 0xB4;
            t->state = 1;
        }
        break;
    }
}

/**
 * A letter of the ending's text (struct EndingLetter): it runs in from the left 2 pixels a
 * frame, bouncing (|gSineTable[phase]| / 32 pixels up, phase + 6 a frame), stands still
 * at targetX for its timer, then runs on bouncing and frees itself past x 0xF8. 'i' and
 * 'l' (tiles 0x49, 0x4C) start 2 pixels further right.
 */
void Ending_UpdateLetter(struct EndingLetter *t)
{
    u32 frame = gMain.frameCount;
    struct Sprite *spr;

    switch (t->state)
    {
    case 0:
        t->phase = frame & 0xFF;
        t->palette = 14;
        if (t->tile == 0x49 || t->tile == 0x4C)
            t->x += 2;
        t->state++;
        break;
    case 1:
        if (t->x != t->targetX)
            t->x += 2;
        t->phase += 6;
        if (t->x < t->targetX)
            goto bounce;
        t->state++;
        goto still;
    case 2:
        if (t->timer != 0 && --t->timer == 0)
        {
            t->phase = 0;
            t->state++;
        }
    still:
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = (t->x << 16) & 0x1FF0000;
            spr->attr2 = (t->palette << 12) | t->tile;
            *(u8 *)&spr->attr01 = t->y;
        }
        break;
    case 3:
        if (t->x > 0xF8)
        {
            Task_Destroy((struct Task *)t);
            break;
        }
        t->x += 2;
        t->phase += 6;
    bounce:
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            s32 d = gSineTable[(u8)t->phase];

            if (d < 0)
                d = -d;
            spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0x10000000;
            spr->attr2 = (t->palette << 12) | t->tile;
            *(u8 *)&spr->attr01 = (u8)t->y - d / 32;
        }
        break;
    }
}
