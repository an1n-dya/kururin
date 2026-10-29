// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// collision: map collision and the Helirin's points against shapes.
// 08013D0C-080142DC, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "collision.h"
#include "text.h"

/**
 * Collision_GetTilePixel for tiles 1-0x9F only: any other tile has no collision (0).
 */
u32 Collision_TestWallPixel(struct Map *map, u16 x, u16 y)
{
    u16 tile = map->tiles[(u32)(y >> 3) % map->height * map->width + (u32)(x >> 3) % map->width];
    const u32 *pixels;
    u32 px = x;
    u32 py = y;

    if ((u32)((tile & 0x3FF) - 1) > 0x9E)
        return 0;
    pixels = gPhysicalTilesChallenge[tile & 0x3FF];
    if (tile & 0x400)
        px = ~px;
    if (tile & 0x800)
        py = ~py;
    return (pixels[py & 7] >> ((px & 7) * 4)) & 0xF;
}
/**
 * The collision value (0-15) of the map's pixel (*x*, *y*), wrapping around the map: the
 * pixel's 4 bits in the physical tile at that place, flipped as the map says.
 */
u32 Collision_GetTilePixel(struct Map *map, u16 x, u16 y)
{
    u16 tile = map->tiles[(u32)(y >> 3) % map->height * map->width + (u32)(x >> 3) % map->width];
    const u32 *pixels = gPhysicalTilesChallenge[tile & 0x3FF];
    u32 px = x;
    u32 py = y;

    if (tile & 0x400)
        px = ~px;
    if (tile & 0x800)
        py = ~py;
    return (pixels[py & 7] >> ((px & 7) * 4)) & 0xF;
}
/**
 * Places the Helirin's points[] along its stick: point i is pointDistances[i] pixels from its centre in
 * the direction of angle (x by gSineTable, y by -gCosineTable, 1/0x100). minX-maxY get
 * the smaller and larger x and y of the last two points.
 */
void Collision_UpdatePoints(struct Helirin *h)
{
    u32 dir = (u16)h->angle >> 8;
    s32 cos = gCosineTable[dir];
    s32 sin = gSineTable[dir];
    const s8 *dist = h->pointDistances;
    s32 i;
    s16 x1, x2, y1, y2;

    for (i = 0; i < h->numPoints; i++)
    {
        h->points[i].x = sin * dist[i] / 0x100 + POS_PIXEL(h->x);
        h->points[i].y = dist[i] * -cos / 0x100 + POS_PIXEL(h->y);
    }
    x1 = h->points[h->numPoints - 1].x;
    x2 = h->points[h->numPoints - 2].x;
    y1 = h->points[h->numPoints - 1].y;
    y2 = h->points[h->numPoints - 2].y;
    if (x1 <= x2)
    {
        h->minX = x1;
        h->maxX = x2;
    }
    else
    {
        h->minX = x2;
        h->maxX = x1;
    }
    if (y1 <= y2)
    {
        h->minY = y1;
        h->maxY = y2;
    }
    else
    {
        h->minY = y2;
        h->maxY = y1;
    }
}
/**
 * Collision_FindPointInShapes for every point and shape: returns the mask of *h*'s points (bit n for
 * points[n]) inside any of the shapes at (*x*, *y*), 0 when the Helirin ignores collisions.
 */
u32 Collision_TestShapes(struct Helirin *h, s32 x, s32 y, const struct CollisionShape *shape)
{
    s32 left, right, top, bottom;
    s32 cx, cy, dx, dy;
    s32 px, py;
    s32 i;
    u32 hits = 0;

    if (h->disableFlags & 1)
        return 0;
    for (; shape->type != 0; shape++)
    {
        switch (shape->type)
        {
        case 1:
            left = x + shape->dx;
            if (h->maxX < left)
                break;
            right = left + shape->w;
            if (h->minX > right)
                break;
            top = y + shape->dy;
            if (h->maxY < top)
                break;
            bottom = top + shape->h;
            if (h->minY > bottom)
                break;
            for (i = 0; i < h->numPoints; i++)
            {
                px = (s16)h->points[i].x;
                py = (s16)h->points[i].y;
                if (px >= left && px <= right && py >= top && py <= bottom)
                    hits |= 1 << i;
            }
            break;
        case 2:
            cx = x + shape->dx;
            left = cx - shape->w;
            if (h->maxX < left)
                break;
            right = cx + shape->w;
            if (h->minX > right)
                break;
            cy = y + shape->dy;
            top = cy - shape->w;
            if (h->maxY < top)
                break;
            bottom = cy + shape->w;
            if (h->minY > bottom)
                break;
            for (i = 0; i < h->numPoints; i++)
            {
                px = (s16)h->points[i].x;
                py = (s16)h->points[i].y;
                if (px >= left && px <= right && py >= top && py <= bottom)
                {
                    dx = cx - px;
                    dy = cy - py;
                    if (dx * dx + dy * dy <= shape->h)
                        hits |= 1 << i;
                }
            }
            break;
        }
    }
    return hits;
}
/**
 * Tests *h*'s points[0..numPoints - 1] against the shapes at (*x*, *y*) (struct
 * CollisionShape): returns the index of the first point inside the first shape that has
 * one, or -1 when none does or the Helirin ignores collisions (disableFlags bit 0). A shape is only
 * tested when the Helirin's box (minX..maxY) meets the shape's.
 */
s32 Collision_FindPointInShapes(struct Helirin *h, s32 x, s32 y, const struct CollisionShape *shape)
{
    s32 left, right, top, bottom;
    s32 cx, cy, dx, dy;
    s32 px, py;
    s32 i;

    if (h->disableFlags & 1)
        return -1;
    for (; shape->type != 0; shape++)
    {
        switch (shape->type)
        {
        case 1:
            left = x + shape->dx;
            if (h->maxX < left)
                break;
            right = left + shape->w;
            if (h->minX > right)
                break;
            top = y + shape->dy;
            if (h->maxY < top)
                break;
            bottom = top + shape->h;
            if (h->minY > bottom)
                break;
            for (i = 0; i < h->numPoints; i++)
            {
                px = (s16)h->points[i].x;
                py = (s16)h->points[i].y;
                if (px >= left && px <= right && py >= top && py <= bottom)
                    return i;
            }
            break;
        case 2:
            cx = x + shape->dx;
            left = cx - shape->w;
            if (h->maxX < left)
                break;
            right = cx + shape->w;
            if (h->minX > right)
                break;
            cy = y + shape->dy;
            top = cy - shape->w;
            if (h->maxY < top)
                break;
            bottom = cy + shape->w;
            if (h->minY > bottom)
                break;
            for (i = 0; i < h->numPoints; i++)
            {
                px = (s16)h->points[i].x;
                py = (s16)h->points[i].y;
                if (px >= left && px <= right && py >= top && py <= bottom)
                {
                    dx = cx - px;
                    dy = cy - py;
                    if (dx * dx + dy * dy <= shape->h)
                        return i;
                }
            }
            break;
        }
    }
    return -1;
}
/**
 * Debug output nothing references: draws 8 words (from the pointer it gets) as hexadecimal
 * digits, one line of 8 each, 8 pixels apart (Text_DrawSmall), each word's lowest digit
 * first.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
void Debug_DrawHexWords(u16 x, u8 y, const u32 *values, u8 palette)
{
    u8 buf[12];
    u32 i;

    for (i = 0; i < 8; i++)
    {
        u32 value = *values++;
        u32 j;

        for (j = 0; j < 8; j++)
        {
            u32 digit = value & 0xF;

            if (digit > 9)
                digit += 'A' - 10;
            else
                digit += '0';
            buf[j] = digit;
            value >>= 4;
        }
        buf[j] = '\0';
        Text_DrawSmall(x, y, buf, palette);
        y += 8;
    }
}
