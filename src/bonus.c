// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// bonus: the bonus items of a level and their tasks.
// 08018330-08018D34, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "sprite.h"
#include "task.h"
#include "util.h"
#include "collision.h"
#include "helirin.h"
#include "bonus.h"

extern const struct LongAnimFrame gItemAnims[10][5];
extern const void *const gTypeBonusTiles[];
extern const void *const gColorBonusPalettes[];
extern const u8 gColorBonusTilesA[];
extern const u8 gColorBonusTilesB[];
extern const u8 gTypeBonusGfx[];
extern const struct CollisionShape gBonusShape[];
extern const void *const gItemPalettes[];
extern const u8 gWornItemPalettes[10];
extern const u16 gWornItemFollowAnim[];
extern const u16 gWornItemFlyAnim[];
extern const void *const gWornItemTiles[4][4];
extern const struct CollisionShape gWornItemShape[];

/**
 * The bonus task of Bonus_UpdateType with palette gColorBonusPalettes[value], tiles gColorBonusTilesA (odd
 * value) or gColorBonusTilesB and gMenuCursorTiles, swapping value with the Helirin's color
 * (Helirin_SetColor) instead of its type.
 */
void Bonus_UpdateColor(struct BonusTask *t)
{
    struct Helirin *h;
    struct Sprite *spr;
    u16 prev;

    switch (t->state)
    {
    case 0:
        Main_QueueCopy(gColorBonusPalettes[t->value], (void *)0x05000300, 0x20);
        if (t->value & 1)
            Main_QueueCopy(gColorBonusTilesA, (void *)0x06016DA0, 0x80);
        else
            Main_QueueCopy(gColorBonusTilesB, (void *)0x06016DA0, 0x80);
        Main_QueueCopy(gMenuCursorTiles, (void *)0x060169A0, 0x200);
        t->state = 1;
        // fall through
    case 1:
        if (gGameState.paused == 0)
        {
            h = gGameState.helirin;
            if (Collision_FindPointInShapes(h, t->x, t->y, gBonusShape) >= 0)
            {
                prev = t->value;
                gGameState.flags |= GAME_FLAG_BONUS_TAKEN;
                t->value = h->color;
                Helirin_SetColor(h, prev);
                t->timer = 0x78;
                m4aSongNumStart(0x47);
                t->state = 2;
            }
        }
        t->blink = (t->blink + 1) & 0xF;
        *(u16 *)0x050003DE = *gMenuCursorColors[t->blink / 4];
        if (Util_GetScreenPos(t->x, t->y, t->screen, 8) != 0)
            break;
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = 0x40000000 | (((t->screen[0] - 8) << 16) & 0x1FF0000)
                      | (u8)(t->screen[1] - 8);
            spr->attr2 = 0x876D;
        }
        spr = Sprite_Alloc(8);
        if (spr != NULL)
        {
            spr->attr01 = 0x80000000 | (((t->screen[0] - 16) << 16) & 0x1FF0000)
                      | (u8)(t->screen[1] - 16);
            spr->attr2 = 0xE74D;
        }
        break;
    case 2:
        if (gGameState.paused == 0)
        {
            h = gGameState.helirin;
            if (--t->timer == -1)
            {
                Helirin_SetColor(h, t->value);
                t->timer = 0x78;
                Task_Destroy((struct Task *)t);
            }
        }
        break;
    }
}
/**
 * The bonus task (see struct BonusTask): copies its tiles and palette, then, unless
 * gGameState.paused is set, waits for the current Helirin to touch it (Collision_FindPointInShapes with
 * gBonusShape): it then sets GAME_FLAG_BONUS_TAKEN, swaps its value with the Helirin's type
 * (Helirin_SetSize), plays song 0x47 and, 0x79 frames later, gives the Helirin its type back
 * and frees itself. Until the touch it blinks palette colour 0x050003DE and draws two sprites
 * around its screen position.
 */
void Bonus_UpdateType(struct BonusTask *t)
{
    struct Helirin *h;
    struct Sprite *spr;
    u16 prev;

    switch (t->state)
    {
    case 0:
        Main_QueueCopy(gTypeBonusTiles[t->value], (void *)0x06016DA0, 0x80);
        Main_QueueCopy(gTypeBonusGfx, (void *)0x05000300, 0x20);
        Main_QueueCopy(gTypeBonusGfx + 0x1A80, (void *)0x060169A0, 0x200);
        t->state = 1;
        // fall through
    case 1:
        h = gGameState.helirin;
        if (gGameState.paused == 0 && Collision_FindPointInShapes(h, t->x, t->y, gBonusShape) >= 0)
        {
            prev = t->value;
            gGameState.flags |= GAME_FLAG_BONUS_TAKEN;
            t->value = (s8)h->type;
            h->type = prev;
            Helirin_SetSize(h);
            t->timer = 0x78;
            m4aSongNumStart(0x47);
            t->state = 2;
        }
        t->blink = (t->blink + 1) & 0xF;
        *(u16 *)0x050003DE = *gMenuCursorColors[t->blink / 4];
        if (Util_GetScreenPos(t->x, t->y, t->screen, 8) != 0)
            break;
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = 0x40000000 | (((t->screen[0] - 8) << 16) & 0x1FF0000)
                      | (u8)(t->screen[1] - 8);
            spr->attr2 = 0x876D;
        }
        spr = Sprite_Alloc(8);
        if (spr != NULL)
        {
            spr->attr01 = 0x80000000 | (((t->screen[0] - 16) << 16) & 0x1FF0000)
                      | (u8)(t->screen[1] - 16);
            spr->attr2 = 0xE74D;
        }
        break;
    case 2:
        h = gGameState.helirin;
        if (gGameState.paused == 0 && --t->timer == -1)
        {
            h->type = t->value;
            Helirin_SetSize(h);
            t->timer = 0x78;
            Task_Destroy((struct Task *)t);
        }
        break;
    }
}
/**
 * The bonus of bit item (see struct ItemBonus): copies its palette (gItemPalettes[item])
 * and tiles, then plays gItemAnims[item] (0x80 bytes of tiles per frame) and blinks and
 * draws like Bonus_UpdateType until the current Helirin touches it (Collision_FindPointInShapes, unless
 * gGameState.paused is set): it then sets GAME_FLAG_BONUS_TAKEN, plays song 0x47, hands
 * item and the touching point's index to a new Bonus_UpdateWornItem task and frees itself.
 */
void Bonus_UpdateItem(struct ItemBonus *t)
{
    struct Helirin *h;
    struct WornItem *worn;
    struct Sprite *spr;
    s32 n;

    switch (t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gItemAnims[t->item];
        t->anim.value = gItemAnims[t->item][0].value;
        Main_QueueCopy(gItemPalettes[t->item], (void *)0x05000300, 0x20);
        Main_QueueCopy(gMenuCursorTiles, (void *)0x060169A0, 0x200);
        t->state = 1;
        // fall through
    case 1:
        break;
    default:
        return;
    }

    h = gGameState.helirin;
    if (gGameState.paused == 0)
    {
        n = Collision_FindPointInShapes(h, t->x, t->y, gBonusShape);
        if (n >= 0)
        {
            gGameState.flags |= GAME_FLAG_BONUS_TAKEN;
            m4aSongNumStart(0x47);
            worn = (struct WornItem *)Task_Create(Bonus_UpdateWornItem, sizeof(*worn));
            if (worn != NULL)
            {
                worn->item = t->item;
                worn->point = n;
            }
            Task_Destroy((struct Task *)t);
            return;
        }
    }
    if (Anim_StepLong(&t->anim) & 1)
        Main_QueueCopy((const void *)t->anim.value, (void *)0x06016DA0, 0x80);
    t->blink = (t->blink + 1) & 0xF;
    *(u16 *)0x050003DE = *gMenuCursorColors[t->blink / 4];
    if (Util_GetScreenPos(t->x, t->y, t->screen, 8) != 0)
        return;
    spr = Sprite_Alloc(7);
    if (spr != NULL)
    {
        spr->attr01 = 0x40000000 | (((t->screen[0] - 8) << 16) & 0x1FF0000) | (u8)(t->screen[1] - 8);
        spr->attr2 = 0x876D;
    }
    spr = Sprite_Alloc(8);
    if (spr != NULL)
    {
        spr->attr01 = 0x80000000 | (((t->screen[0] - 16) << 16) & 0x1FF0000) | (u8)(t->screen[1] - 16);
        spr->attr2 = 0xE74D;
    }
}
/**
 * The bonus of bit item once taken (Bonus_UpdateItem) or switched on (MakeUp_StartWornItems,
 * MakeUp_MenuItem): a sprite of OBJ tile 0x371 + item (animated by gWornItemTiles) that
 * follows Helirin point point at angle unk26 from the Helirin's (state 2); when the
 * Helirin's flags have any of 0x34000820 it flies off (state 4) and homes back to the
 * Helirin, which catches it again after 60 frames (Collision_FindPointInShapes). In mode 7 the task ends
 * once its bit of gGameState.saveFile->items is clear.
 */
void Bonus_UpdateWornItem(struct Task *task)
{
    struct WornItem *t = (struct WornItem *)task;
    struct Helirin *h;
    struct Sprite *spr;
    s32 idx;
    s32 n;

    if ((s8)gGameState.mode == 7 && !((gGameState.saveFile->items >> t->item) & 1))
    {
        Task_Destroy(task);
        return;
    }

    switch (t->state)
    {
    case 0:
        t->unk26 = 0x40;
        t->tile = 0x371 + t->item;
        t->attr2 = (gWornItemPalettes[t->item] << 12) | 0x400 | (t->item + 0x371);
        // fall through
    case 1:
        t->anim.timer = 1;
        t->anim.next = gWornItemFollowAnim;
        t->anim.value = gWornItemFollowAnim[1];
        t->state = 2;
        // fall through
    case 2:
        h = gGameState.helirin;
        if (gGameState.paused != 0)
            break;
        POS_PIXEL(t->x) = h->points[t->point].x;
        POS_PIXEL(t->y) = h->points[t->point].y;
        if (Anim_Step(&t->anim) & 1)
            Main_QueueCopy(gWornItemTiles[t->item & 3][t->anim.value], (u8 *)0x06010000 + t->tile * 32, 0x20);
        if (h->flags & 0x34000820)
            t->state = 3;
        t->dir = (h->angle >> 8) + t->unk26;
        break;
    case 3:
        t->anim.timer = 1;
        t->anim.next = gWornItemFlyAnim;
        t->anim.value = gWornItemFlyAnim[1];
        t->unk28 = 0x100;
        t->vx = gSineTable[(u8)t->dir] * 0x100;
        t->vy = -gCosineTable[(u8)t->dir] * 0x100;
        t->timer = 60;
        t->state = 4;
        // fall through
    case 4:
        h = gGameState.helirin;
        if (gGameState.paused != 0)
            break;
        if (t->x > h->x - 0x40000 && t->vx > -0x10000)
            t->vx -= 0x400;
        else if (t->x + 0x40000 < h->x && t->vx <= 0xFFFF)
            t->vx += 0x400;
        if (t->y > h->y - 0x40000 && t->vy > -0x10000)
            t->vy -= 0x400;
        else if (t->y + 0x40000 < h->y && t->vy <= 0xFFFF)
            t->vy += 0x400;
        t->dir = Util_GetDirection(t->vx, t->vy);
        t->x += t->vx;
        t->y += t->vy;
        if (Anim_Step(&t->anim) & 1)
            Main_QueueCopy(gWornItemTiles[t->item & 3][t->anim.value], (u8 *)0x06010000 + t->tile * 32, 0x20);
        if (t->timer > 0)
        {
            t->timer--;
            break;
        }
        if (h->flags & 0x34000820)
            break;
        n = Collision_FindPointInShapes(h, POS_PIXEL(t->x), POS_PIXEL(t->y), gWornItemShape);
        if (n < 0)
            break;
        t->point = n;
        t->unk26 = (u8)t->dir - (h->angle >> 8);
        t->state = 1;
        break;
    default:
        return;
    }

    if (Util_GetScreenPos(POS_PIXEL(t->x), POS_PIXEL(t->y), t->screen, 8) != 0)
        return;
    idx = Sprite_AllocAffine();
    if (idx >= 0)
    {
        t->unk24 = idx;
        Sprite_SetAffineRotation(idx, t->dir);
    }
    else
    {
        t->unk24 = ((t->dir + 0x10) >> 5) & 7;
    }
    spr = Sprite_Alloc(5);
    if (spr == NULL)
        return;
    spr->attr01 = (t->unk24 << 25) | (((t->screen[0] - 4) << 16) & 0x1FF0000) | 0x100
              | (u8)(t->screen[1] - 4);
    spr->attr2 = t->attr2;
}
/**
 * A task drawing a 16x16 sprite at (unk8, unkA) with tile unkE and palette unkF: step 0
 * first copies 0x80 bytes of tiles there, gColorBonusTilesB for an odd unkC, else gColorBonusTilesA.
 */
void Bonus_DrawColorIcon(struct Task *task)
{
    struct Sprite *spr;

    switch ((s16)task->state)
    {
    case 0:
        if (task->unkC & 1)
            Main_QueueCopy(gColorBonusTilesB, (u8 *)0x06010000 + task->unkE * 32, 0x80);
        else
            Main_QueueCopy(gColorBonusTilesA, (u8 *)0x06010000 + task->unkE * 32, 0x80);
        task->state = 1;
        // fall through
    case 1:
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = (((s16)task->unk8 << 16) & 0x1FF0000) | 0x40000000;
            // the cast keeps agbcc from reassociating 0x400 to the end, as the original
            spr->attr2 = (u16)((task->unkF << 12) | 0x400) | task->unkE;
            *(u8 *)&spr->attr01 = task->unkA;
        }
        break;
    }
}
/**
 * Bonus_DrawColorIcon with the tiles gTypeBonusTiles[unkC] (the bonus tiles of Bonus_UpdateType).
 */
void Bonus_DrawTypeIcon(struct Task *task)
{
    struct Sprite *spr;

    switch ((s16)task->state)
    {
    case 0:
        Main_QueueCopy(gTypeBonusTiles[task->unkC], (u8 *)0x06010000 + task->unkE * 32, 0x80);
        task->state = 1;
        // fall through
    case 1:
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = (((s16)task->unk8 << 16) & 0x1FF0000) | 0x40000000;
            // the cast keeps agbcc from reassociating 0x400 to the end, as the original
            spr->attr2 = (u16)((task->unkF << 12) | 0x400) | task->unkE;
            *(u8 *)&spr->attr01 = task->unkA;
        }
        break;
    }
}
/**
 * A task (struct MakeUpIcon) that draws item *item* (bit 0-9 of gGameState.saveFile->unlocks) as a
 * 16x16 sprite at (x, y) (list 7). While that bit of the save file's items is set (worn) it adds
 * a 32x32 sprite at (x - 8, y - 8) (list 8) and plays gItemAnims[item] (Anim_StepLong), copying
 * the frame's 0x80 bytes of tiles to the sprite's tile when the result has bit 0 set.
 */
void Bonus_UpdateMakeUpIcon(struct Task *task)
{
    struct MakeUpIcon *t = (struct MakeUpIcon *)task;
    struct SaveFile *p = gGameState.saveFile;
    struct Sprite *spr;

    switch ((s16)t->state)
    {
    case 0:
        t->anim.timer = 1;
        t->anim.next = gItemAnims[t->item];
        t->anim.value = gItemAnims[t->item][0].value;
        Anim_StepLong(&t->anim);
        Main_QueueCopy((const void *)t->anim.value, (void *)(0x06010000 + t->tile * 0x20), 0x80);
        t->state = 1;
        // fall through
    case 1:
        if ((p->items >> t->item) & 1)
        {
            spr = Sprite_Alloc(8);
            if (spr != NULL)
            {
                spr->attr01 = (((t->x - 8) << 16) & 0x1FF0000) | 0x80000000 | (u8)(t->y - 8);
                spr->attr2 = 0xE75D;
            }
            if (Anim_StepLong(&t->anim) & 1)
                Main_QueueCopy((const void *)t->anim.value, (void *)(0x06010000 + t->tile * 0x20), 0x80);
        }
        spr = Sprite_Alloc(7);
        if (spr != NULL)
        {
            spr->attr01 = ((t->x << 16) & 0x1FF0000) | 0x40000000 | (u8)t->y;
            spr->attr2 = (t->palette << 12) | 0x400 | t->tile;
        }
        break;
    }
}
