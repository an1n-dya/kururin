// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// hud: the lives and status sprites and the times drawn during a level.
// 08016BF4-08017270, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "sprite.h"
#include "text.h"
#include "task.h"
#include "util.h"
#include "hud.h"

extern struct Anim gHudFaceAnim;
// 0x03000018: a second animation, right before gHudFaceAnim (Hud_Draw runs gHudHurtFrames
// in it with Anim_Step while the Helirin is damaging)
extern struct Anim gHudHurtAnim;
extern const u16 gHudHurtFrames[];
extern const u16 gHudGoalFaceAnim[];
extern const void *const gHudFaceTiles[];
extern const u16 *const gHudFaceAnims[][4];
struct HudLifeIcon;
void Hud_UpdateLifeIcon(struct HudLifeIcon *task);
// The tasks Hud_Init starts with Hud_UpdateLifeIcon (0x18 bytes), numbered 1.. in n.
struct HudLifeIcon {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 n;
    /*0x0A*/ u16 unkA;
    /*0x0C*/ struct Anim anim; // value: the sprite's scale (0x100 = 1)
    /*0x14*/ s16 x;
    /*0x16*/ s16 y;
};
extern const struct { s16 x, y; } gHudLifePositions[];
extern const u16 gHudLifeScaleAnim[];
extern const u8 gHudLifeTiles[];

/**
 * Points gHudFaceAnim at gHudFaceAnims[h->maxLives][h->lives], then starts h->maxLives
 * Hud_UpdateLifeIcon tasks (struct HudLifeIcon) numbered 1..maxLives in their n.
 */
void Hud_Init(struct Helirin *h)
{
    struct HudLifeIcon *icon;
    s32 i;

    gHudHurtAnim.value = 0;
    gHudFaceAnim.timer = 1;
    gHudFaceAnim.next = gHudFaceAnims[h->maxLives][(s8)h->lives];
    gHudFaceAnim.value = gHudFaceAnims[h->maxLives][(s8)h->lives][1];
    for (i = 1; i < h->maxLives + 1; i++)
    {
        icon = (struct HudLifeIcon *)Task_Create((void (*)(struct Task *))Hud_UpdateLifeIcon, sizeof(*icon));
        if (icon != NULL)
            icon->n = i;
    }
}
/**
 * Task n of Hud_Init: a sprite at gHudLifePositions[n - 1], shown while the current
 * Helirin has at least n lives. It first plays gHudLifeScaleAnim as the scale of an affine
 * sprite (until it ends, frozen while gGameState.paused is set), then stays a plain sprite;
 * losing the life sends it back to waiting.
 */
void Hud_UpdateLifeIcon(struct HudLifeIcon *t)
{
    struct Helirin *h = gGameState.helirin;
    struct Sprite *spr;
    s32 idx;
    u16 scale;

    switch (t->state)
    {
    case 0:
        t->x = gHudLifePositions[t->n - 1].x;
        t->y = gHudLifePositions[t->n - 1].y;
        Main_QueueCopy(gHudLifeTiles, (void *)0x06010D40, 0x80);
        t->state = 1;
        // fall through
    case 1:
        if ((s8)h->lives < t->n)
            break;
        t->anim.timer = 1;
        t->anim.next = gHudLifeScaleAnim;
        t->anim.value = gHudLifeScaleAnim[1];
        t->state = 2;
        // fall through
    case 2:
        if ((s8)h->lives < t->n)
        {
            t->state = 1;
            break;
        }
        if (gGameState.paused == 0 && Anim_Step(&t->anim) < 0)
            t->state = 3;
        idx = Sprite_AllocAffine();
        if (idx >= 0)
        {
            scale = 0x10000 / t->anim.value;
            gAffineParams[idx][3] = scale;
            gAffineParams[idx][0] = scale;
            gAffineParams[idx][2] = 0;
            gAffineParams[idx][1] = 0;
            spr = Sprite_Alloc(3);
            if (spr != NULL)
            {
                spr->attr01 = ((u32)idx << 25) | 0x40000300 | (((t->x - 8) << 16) & 0x1FF0000)
                          | (u8)(t->y - 8);
                spr->attr2 = 0x706A;
            }
            break;
        }
        // no affine parameter set left: drawn as in state 3
        // fall through
    case 3:
        if ((s8)h->lives < t->n)
        {
            t->state = 1;
            break;
        }
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = 0x40000000 | ((t->x << 16) & 0x1FF0000) | (u8)t->y;
            spr->attr2 = 0x706A;
        }
        break;
    }
}
/**
 * Draws the HUD: the face (gHudFaceAnim's tiles at OBJ tile 0x5A, x 8, y 0x78), the Helirin's
 * status sprites: one per life (maxLives of them, at gHudLifePositions), the
 * next one and a fixed one, the four of gHudHurtAnim's animation while it runs, and in the
 * linked game (gGameState.mode == 4) a sprite and gGameState.unk30 + 1. flags bit 2 (else
 * bits 0x808 == 0x800) restarts gHudFaceAnim's (gHudHurtAnim's) animation; each frame of
 * gHudFaceAnim copies its gHudFaceTiles tiles to OBJ VRAM 0x06010B40.
 */
void Hud_Draw(struct Helirin *h)
{
    struct Sprite *spr;
    s32 i;

    if (h->flags & 4)
    {
        gHudFaceAnim.timer = 1;
        gHudFaceAnim.next = gHudGoalFaceAnim;
        gHudFaceAnim.value = gHudGoalFaceAnim[1];
        gHudHurtAnim.timer = 0;
        gHudHurtAnim.value = 0;
    }
    else if ((h->flags & 0x808) == 0x800)
    {
        gHudHurtAnim.timer = 1;
        gHudHurtAnim.next = gHudHurtFrames;
        gHudHurtAnim.value = gHudHurtFrames[1];
        Main_QueueCopy(gHudFaceTiles[6], (void *)0x06010B40, 0x200);
    }

    if (gHudHurtAnim.timer != 0)
    {
        Anim_Step(&gHudHurtAnim);
        if (gHudHurtAnim.timer == 0)
        {
            gHudFaceAnim.timer = 1;
            gHudFaceAnim.next = gHudFaceAnims[h->maxLives][(s8)h->lives];
            gHudFaceAnim.value = gHudFaceAnims[h->maxLives][(s8)h->lives][1];
        }
    }
    else
    {
        if (h->flags & 0x200)
        {
            gHudFaceAnim.timer = 1;
            gHudFaceAnim.next = gHudFaceAnims[h->maxLives][(s8)h->lives];
            gHudFaceAnim.value = gHudFaceAnims[h->maxLives][(s8)h->lives][1];
        }
        if (Anim_Step(&gHudFaceAnim) & 1)
            Main_QueueCopy(gHudFaceTiles[gHudFaceAnim.value], (void *)0x06010B40, 0x200);
    }

    for (i = 0; i < h->maxLives; i++)
    {
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0x40000000 | ((gHudLifePositions[i].x << 16) & 0x1FF0000)
                      | (u8)gHudLifePositions[i].y;
            spr->attr2 = 0x748C;
        }
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = 0x4000 | ((gHudLifePositions[i].x << 16) & 0x1FF0000)
                  | (u8)(gHudLifePositions[i].y + 8);
        spr->attr2 = 0x748A;
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = 0x80080078;
        spr->attr2 = 0x245A;
    }
    if (gHudHurtAnim.value != 0)
    {
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0x81F80068;
            spr->attr2 = 0x346E;
        }
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0x90180068;
            spr->attr2 = 0x346E;
        }
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0xA1F80088;
            spr->attr2 = 0x346E;
        }
        spr = Sprite_Alloc(4);
        if (spr != NULL)
        {
            spr->attr01 = 0xB0180088;
            spr->attr2 = 0x346E;
        }
    }
    if ((s8)gGameState.mode == 4)
    {
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = 0x40B84008;
            spr->attr2 = 0xE3FB;
        }
        Text_DrawSmallNumber(0xD8, 8, gGameState.unk30 + 1, 0xE);
    }
}
/**
 * Draws two times, each on a frame of two 32x16 sprites (Sprite_Alloc list 4, tile 0x47E,
 * at x 8 and mirrored at x 0x28): gGameState.timer at y 0x18 (Text_DrawDigitTime) and
 * gGameState.bestTime at y 8 (Text_DrawSmallTime), with a 32x8 sprite (tile 0x486) at (0x18, 9).
 */
void Hud_DrawTimes(void)
{
    struct Sprite *spr;

    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = 0x80084000 | 0x18;
        spr->attr2 = 0x747E;
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = 0x90284000 | 0x18;
        spr->attr2 = 0x747E;
    }
    Text_DrawDigitTime(4, 8, 0x17, gGameState.timer, 7);
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = 0x80084000 | 0x08;
        spr->attr2 = 0x747E;
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = 0x90284000 | 0x08;
        spr->attr2 = 0x747E;
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        spr->attr01 = 0x40184000 | 0x09;
        spr->attr2 = 0x7486;
    }
    Text_DrawSmallTime(8, 0x0F, gGameState.bestTime, 0x0E);
}
/**
 * The linked game's progress bar: draws two 32x8 sprites at (0x3C, 0x90) and (0x96, 0x90) (tiles 0x90 and 0x94, palette 4),
 * then for this console's player (layer 4) and every other player in gLinkPlayers (layer 5)
 * 16x16 sprites at x 0x50 + 2 * progress, y 0x8A + n: tile 0x9C (palette 3) when bit 8 of its
 * flags is set, then its Helirin (tile 0x98, its palette, the affine set affineSet).
 */
void Hud_DrawLinkProgress(void)
{
    struct Helirin *h;
    s32 i, bit;
    u32 layer;

    {
        struct Sprite *spr = Sprite_Alloc(5);

        if (spr != NULL)
        {
            spr->attr01 = 0x403C4000;
            ((u16 *)spr)[2] = 0x4490;
            *(u8 *)&spr->attr01 = 0x90;
        }
    }
    {
        struct Sprite *spr = Sprite_Alloc(5);

        if (spr != NULL)
        {
            spr->attr01 = 0x40964000;
            ((u16 *)spr)[2] = 0x4494;
            *(u8 *)&spr->attr01 = 0x90;
        }
    }
    for (bit = 1, i = 0; i < 4; i++, bit <<= 1)
    {
        h = &gHelirins[i];
        if (gLinkPlayer == i)
            layer = 4;
        else if (gLinkPlayers & bit)
            layer = 5;
        else
            continue;
        if (h->flags & 0x100)
        {
            struct Sprite *spr = Sprite_Alloc(layer);

            if (spr != NULL)
            {
                spr->attr01 = ((((s8)h->progress * 2 + 0x50) << 16) & 0x1FF0000) | 0x40000000;
                ((u16 *)spr)[2] = 0x349C;
                *(u8 *)&spr->attr01 = i + 0x8A;
            }
        }
        {
            struct Sprite *spr = Sprite_Alloc(layer);

            if (spr != NULL)
            {
                // the affine set and the rest in their own statements: in one expression
                // agbcc ORs the constant into the set, the original into the x part
                u32 set = h->affineSet << 25;
                u32 attr = ((((s8)h->progress * 2 + 0x50) << 16) & 0x1FF0000) | 0x40000100;

                spr->attr01 = set | attr;
                ((u16 *)spr)[2] = h->palette | 0x498;
                *(u8 *)&spr->attr01 = i + 0x8A;
            }
        }
    }
}
