// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// sprite: the sprite lists (OAM buffer) and the affine parameter sets.
// 08002438-08002848, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "sprite.h"

extern u8 gAffineCount;
extern s8 gSpriteLists[16];
extern u8 gSpriteCount;
extern struct Sprite gSprites[128];
// The low bits of an OAM entry's attribute 0 (GBA hardware): the y position and the affine
// mode (2: the sprite is not drawn)
struct OamAttr0 {
    u32 y:8;
    u32 affineMode:2;
};

/**
 * Empties the 16 lists of gSpriteLists and frees every entry they draw from
 * (gSpriteCount); gAffineParams hands out entries from 8 on again.
 */
void Sprite_ClearLists(void)
{
    u32 i;

    for (i = 0; i < 16; i++)
        gSpriteLists[i] = -1;
    gSpriteCount = 0;
    gAffineCount = 8;
}
/**
 * Builds gOamBuffer from the sprites of Sprite_Alloc: lists 0 to 15 of gSpriteLists in
 * order, each from its head, then hides the entries left (attribute 0 bits 8-9 = 2: OBJ
 * disable) and stores the 32 affine parameter sets of gAffineParams.
 */
void Sprite_BuildOam(void)
{
    struct OamEntry *oam;
    const u16 *affine;
    u32 n = 0;
    u32 i;
    s32 e;

    // (a sprite's first 8 bytes are its OAM entry: copied as one struct, as the original
    // does with ldm/stm; the per-field copy is slower, and so is a level whose sprites fill
    // a frame: save_adventure, The Last Land 3)
    for (i = 0; i < 16; i++)
    {
        for (e = gSpriteLists[i]; e >= 0; e = gSprites[e].next)
        {
            gOamBuffer[n] = *(struct OamEntry *)&gSprites[e];
            n++;
        }
    }
    for (i = n; i < 128; i++)
        ((struct OamAttr0 *)&gOamBuffer[i])->affineMode = 2;
    oam = gOamBuffer;
    for (i = 0, affine = gAffineParams[0]; i < 32; i++)
    {
        oam->affineParam = affine[0];
        oam++;
        oam->affineParam = affine[1];
        oam++;
        oam->affineParam = affine[2];
        oam++;
        oam->affineParam = affine[3];
        oam++;
        affine += 4;
    }
}
/**
 * Hands out the next free entry of gSprites and pushes it onto list *list* of
 * gSpriteLists; NULL once all 128 are taken (gSpriteCount read signed).
 */
struct Sprite *Sprite_Alloc(u8 list)
{
    if ((s8)gSpriteCount < 0)
        return NULL;
    gSprites[gSpriteCount].next = gSpriteLists[list];
    gSpriteLists[list] = gSpriteCount;
    gSpriteCount++;
    return &gSprites[gSpriteCount - 1];
}
/**
 * Sprite_Alloc for *count* entries in a row: pushes the next *count* free entries of gSprites
 * onto list *list* of gSpriteLists (each onto the previous one, so the list draws them last
 * to first) and returns the first; NULL when they would not all fit in the 128 entries
 * (also for a *count* of 0 while no entry is taken: the check is unsigned).
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
struct Sprite *Sprite_AllocMany(u8 list, u8 count)
{
    u8 first = gSpriteCount;
    u32 i;

    if ((u32)(first + count - 1) > 127)
        return NULL;
    for (i = 0; i < count; i++)
    {
        gSprites[gSpriteCount].next = gSpriteLists[list];
        gSpriteLists[list] = gSpriteCount;
        gSpriteCount++;
    }
    return &gSprites[first];
}
/**
 * Fills entries 0-7 of gAffineParams (the ones Sprite_ClearLists keeps from Sprite_AllocAffine) with
 * the rotations by directions 0, 0x20, ..., 0xE0, as Sprite_SetAffineRotation does.
 */
void Sprite_InitRotations(void)
{
    u16 *e = gAffineParams[0];
    s32 dir;

    for (dir = 0; dir < 256; dir += 32)
    {
        s16 y = gCosineTable[(u8)dir];
        s16 x = gSineTable[(u8)dir];

        e[0] = y;
        e[1] = x;
        e[2] = -x;
        e[3] = y;
        e += 4;
    }
}
/**
 * Hands out the next of the 32 entries of gAffineParams (its index), or -1 when all are
 * taken.
 */
s32 Sprite_AllocAffine(void)
{
    u8 n = gAffineCount;
    s32 result;

    if (n < 32)
    {
        gAffineCount = n + 1;
        result = n;
    }
    else
        result = -1;
    return result;
}
/**
 * Sets entry *n* of gAffineParams (see Sprite_AllocAffine) to {a, b, c, d}.
 */
void Sprite_SetAffine(s32 n, s32 a, s32 b, s32 c, s32 d)
{
    u16 *e = gAffineParams[n];

    e[0] = a;
    e[1] = b;
    e[2] = c;
    e[3] = d;
}
/**
 * Sets entry *n* of gAffineParams to the rotation by direction *dir*: {y, x, -x, y} from the
 * step tables gCosineTable (y) and gSineTable (x).
 */
void Sprite_SetAffineRotation(s32 n, u8 dir)
{
    u16 *e = gAffineParams[n];
    s16 y = gCosineTable[dir];
    s16 x = gSineTable[dir];

    e[0] = y;
    e[1] = x;
    e[2] = -x;
    e[3] = y;
}
/**
 * Sprite_SetAffineRotation with a scale: affine entry *n* of gAffineParams rotated by direction
 * *dir* ({y, x, -x, y} from gCosineTable (y) and gSineTable (x)), each divided by *scale* / 256.
 */
void Sprite_SetAffineRotScale(s32 n, s32 dir, s32 scale)
{
    u16 *e = gAffineParams[n];
    s32 y = gCosineTable[(u8)dir];
    s32 x = gSineTable[(u8)dir];
    s32 a = (y << 8) / scale;

    e[0] = a;
    e[1] = (x << 8) / scale;
    e[2] = (-x << 8) / scale;
    e[3] = a;
}
// An entry of Sprite_QueueTileList's list: *rows* rows of *width* OBJ tiles from *src*
struct TileList {
    /*0x00*/ const u8 *src;
    /*0x04*/ u16 tile; // where in OBJ VRAM (0x06010000), in 64-byte units
    /*0x06*/ u8 width; // tiles per row
    /*0x07*/ u8 rows;
};
/**
 * Queues the OBJ tile copies of a list of entries (struct TileList, ended by a NULL *src*)
 * in gTransferQueue: per entry one transfer per row of *width* * 32 bytes from *src* to OBJ
 * VRAM at *tile* * 64 (rows 0x800 bytes apart in VRAM, *width* * 64 bytes apart in *src*). It
 * does not set the entries' mode: each keeps the mode last queued in its slot, as in the
 * original (never executed). Returns -1, with the entries before it queued, when an entry's
 * rows would not fit in the queue's 0x40 entries, else 0.
 */
s32 Sprite_QueueTileList(const struct TileList *e)
{
    const u8 *src;

    while ((src = e->src) != NULL)
    {
        u8 rows = e->rows;
        u8 *dst;
        u16 size;
        s32 last = rows - 1;

        if (last + gTransferCount > 0x3F)
            return -1;
        dst = (u8 *)0x06010000 + e->tile * 64;
        size = e->width * 32;
        for (; rows != 0; rows--)
        {
            gTransferQueue[gTransferCount].src = src;
            gTransferQueue[gTransferCount].dst = dst;
            gTransferQueue[gTransferCount].size = size;
            gTransferCount++;
            src += size * 2;
            dst += 0x800;
        }
        e++;
    }
    return 0;
}
// One 0xC-byte entry of the block Sprite_AllocMany hands out (as Sprite_Alloc's, struct
// Sprite), with the OAM attributes as fields.
struct SpriteEntry {
    /*0x00*/ u8 y;
    /*0x01*/ u8 attr0hi;
    /*0x02*/ u16 x:9;
    u16 attr1hi:7;
    /*0x04*/ u16 attr2;
    /*0x06*/ u16 unk6;
    /*0x08*/ u32 unk8;
};

/**
 * Draws a 128x128 grid of OBJ tiles: 16 rows of four 32x8 sprites (layer 0), row i at y
 * 8 * i with tiles 32 * i on; holding Right (0x10) shifts the tiles by 0x10, Down
 * (0x80) by 0x200. Stops when Sprite_AllocMany has no room left.
 */
void Sprite_DebugTileGrid(void)
{
    s32 a = (gMain.heldKeys & 0x10) ? 0x10 : 0;
    s32 b = (gMain.heldKeys & 0x80) ? 0x200 : 0;
    s32 i;

    for (i = 0; i < 16; i++)
    {
        struct SpriteEntry *s = (struct SpriteEntry *)Sprite_AllocMany(0, 4);
        s32 tile;

        if (s == NULL)
            return;
        *(u32 *)&s[0] = *(u32 *)&s[1] = *(u32 *)&s[2] = *(u32 *)&s[3] = 0x40004000;
        tile = b + a + i * 32;
        s[0].attr2 = tile;
        s[1].attr2 = tile + 4;
        s[2].attr2 = tile + 8;
        s[3].attr2 = tile + 12;
        s[0].x = 0;
        s[1].x = 0x20;
        s[2].x = 0x40;
        s[3].x = 0x60;
        s[0].y = s[1].y = s[2].y = s[3].y = i * 8;
    }
}
