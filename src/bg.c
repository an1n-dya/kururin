// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// bg: video modes, background maps and layers, the window effect, screen backgrounds.
// 08000AEC-08002438, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "decompress.h"

extern u8 gBgLayerBuffers[0x2000];
extern const u8 gUnk_080EAC64[];
extern const u8 gUnk_080F7280[];
extern const u8 gUnk_080F7380[];
extern const u8 gUnk_080F697A[];
extern const u8 gUnk_080F69DA[];
extern const u8 gUnk_080EA764[];
extern const u8 gUnk_080EA360[];
extern const u8 gUnk_080F6B7C[];
extern const u8 gUnk_080F707C[];
extern const u8 gUnk_080EEC68[];
extern const u8 gUnk_080EF168[];
extern const u8 gUnk_080F3D74[];
extern const u8 gUnk_080F4278[];
extern const u8 gUnk_080F4778[];
extern const u8 gUnk_080F6378[];
extern const u8 gUnk_0812C488[];
extern const u8 gUnk_0812CC88[];
extern const u8 gUnk_08133968[];
extern const u8 gUnk_08136050[];
extern const u8 gUnk_08133B48[];
extern const u8 gUnk_08136554[];
extern const u8 gUnk_08136A58[];
extern const u8 gUnk_08136F5C[];
extern const u8 gUnk_080F316C[];
extern const u8 gUnk_080F3670[];
extern const u8 gUnk_0818698C[];
extern const u8 gUnk_08186448[];
extern const u8 gUnk_081866A4[];
extern const u8 gUnk_08182448[];
extern const u8 gUnk_0818683C[];
extern const u8 gUnk_0812BC84[];
extern const u8 gUnk_08187B88[];
extern const u8 gUnk_08186F88[];
extern u16 gWindowLines[];

// The copies of the I/O registers at gDispcntCopy (Main_InstallFrameCallback and
// Main_InstallLinkFrameCallback copy the first 0x2B halfwords of 0x04000000 there) as
// Bg_SetVideoMode writes them: through one base, as fields. The PC port's GCC would take an
// index past gDispcntCopy as outside that object (undefined, and not aliasing the other copies):
// there the base is gDispcntCopy's address as a number (kururin.toml [symbols], the port's
// memory map).
#ifdef PLATFORM_PC
#define IO_COPY ((vu16 *)0x030008F0)
#else
#define IO_COPY ((vu16 *)&gDispcntCopy)
#endif
// Every layer of gBgLayers gets the size w x h tiles (stored from the last layer down)
#define LAYER_SIZES(w, h)                                                                  \
    {                                                                                      \
        gBgLayers[0].width = gBgLayers[1].width = gBgLayers[2].width           \
            = gBgLayers[3].width = (w);                                                \
        gBgLayers[0].height = gBgLayers[1].height = gBgLayers[2].height           \
            = gBgLayers[3].height = (h);                                                \
    }
/**
 * Sets up video mode *mode* (0-9): the copies of DISPCNT, BG0CNT-BG3CNT, MOSAIC and the
 * blending registers (gDispcntCopy, the I/O registers Main_InstallLinkFrameCallback copies) and, for each of
 * the four layers of gBgLayers, a 32x32-tile size, its map and character VRAM (mapVram,
 * mapSize, charVram), position 0 and its 0x800-byte buffer in gBgLayerBuffers. Mode 1 picks other
 * blending for world 2. Other values (and 0) set the plain mode.
 */
void Bg_SetVideoMode(s32 mode)
{
    s32 i;

    switch ((s16)mode)
    {
    case 0:
    default:
        IO_COPY[0] = 0x140;
        IO_COPY[4] = 0x1C00;
        IO_COPY[5] = 0x1D01;
        IO_COPY[6] = 0x1E02;
        IO_COPY[7] = 0x1F03;
        IO_COPY[0x28] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        break;
    case 9:
        IO_COPY[0] = 0x41;
        IO_COPY[4] = 0x1C0C;
        IO_COPY[5] = 0x1D85;
        IO_COPY[6] = 0x3E82;
        IO_COPY[0x28] = 0;
        IO_COPY[0x29] = 0x808;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x0600C000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x06004000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x200;
        gBgLayers[2].charVram = (void *)0x06000000;
        gBgLayers[3].mapVram = NULL;
        gBgLayers[3].mapSize = 0;
        gBgLayers[3].charVram = NULL;
        break;
    case 1:
        IO_COPY[0] = 0x1F40;
        if (gGameState.world != 2)
        {
            IO_COPY[4] = 0x1C0C;
            IO_COPY[5] = 0x1D0D;
            IO_COPY[6] = 0x1E0A;
            IO_COPY[7] = 0x1F03;
            IO_COPY[0x28] = 0x3F00;
            IO_COPY[0x29] = 0x808;
            IO_COPY[0x2A] = 0;
        }
        else
        {
            IO_COPY[4] = 0x1C0C;
            IO_COPY[5] = 0x1D0D;
            IO_COPY[6] = 0x1E0A;
            IO_COPY[7] = 0x1F03;
            IO_COPY[0x28] = 0x3F44;
            IO_COPY[0x29] = 0xB05;
            IO_COPY[0x2A] = 0;
        }
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x0600C000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x0600C000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x800;
        gBgLayers[2].charVram = (void *)0x06008000;
        gBgLayers[3].mapVram = (void *)0x0600F800;
        gBgLayers[3].mapSize = 0x800;
        gBgLayers[3].charVram = (void *)0x06000000;
        IO_COPY[0x26] = 0;
        break;
    case 2:
        IO_COPY[0] = 0x40;
        IO_COPY[4] = 0x1C0C;
        IO_COPY[5] = 0x1D01;
        IO_COPY[6] = 0x1EC2;
        IO_COPY[0x28] = 0;
        IO_COPY[0x29] = 0;
        IO_COPY[0x2A] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x0600C000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x06000000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x800;
        gBgLayers[2].charVram = (void *)0x06000000;
        gBgLayers[3].mapVram = NULL;
        gBgLayers[3].mapSize = 0;
        gBgLayers[3].charVram = NULL;
        break;
    case 3:
        IO_COPY[0] = 0x140;
        IO_COPY[4] = 0x1C0C;
        IO_COPY[5] = 0x1D0D;
        IO_COPY[6] = 0x1E82;
        IO_COPY[0x28] = 0x3F00;
        IO_COPY[0x29] = 0x808;
        IO_COPY[0x2A] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x0600C000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x0600C000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x800;
        gBgLayers[2].charVram = (void *)0x06000000;
        gBgLayers[3].mapVram = NULL;
        gBgLayers[3].mapSize = 0;
        gBgLayers[3].charVram = NULL;
        break;
    case 4:
        IO_COPY[0] = 0x141;
        IO_COPY[4] = 0x1C0C;
        IO_COPY[5] = 0x1D05;
        IO_COPY[6] = 0x3E82;
        IO_COPY[0x28] = 0;
        IO_COPY[0x29] = 0;
        IO_COPY[0x2A] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x0600C000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x06004000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x200;
        gBgLayers[2].charVram = (void *)0x06000000;
        break;
    case 5:
        IO_COPY[0] = 0x140;
        IO_COPY[4] = 0x1C0C;
        IO_COPY[5] = 0x1D81;
        IO_COPY[6] = 0x1E0A;
        IO_COPY[7] = 0x1F0B;
        IO_COPY[0x28] = 0;
        IO_COPY[0x29] = 0;
        IO_COPY[0x2A] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x0600C000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x06000000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x800;
        gBgLayers[2].charVram = (void *)0x06008000;
        gBgLayers[3].mapVram = (void *)0x0600F800;
        gBgLayers[3].mapSize = 0x800;
        gBgLayers[3].charVram = (void *)0x06008000;
        break;
    case 6:
        IO_COPY[0] = 0x40;
        IO_COPY[4] = 0x1C00;
        IO_COPY[5] = 0x1D01;
        IO_COPY[6] = 0x1E02;
        IO_COPY[7] = 0x1F03;
        IO_COPY[0x28] = 0xFF;
        IO_COPY[0x29] = 0;
        IO_COPY[0x2A] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x06000000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x06008000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x800;
        gBgLayers[2].charVram = (void *)0x06000000;
        gBgLayers[3].mapVram = (void *)0x0600F800;
        gBgLayers[3].mapSize = 0x800;
        gBgLayers[3].charVram = (void *)0x06000000;
        break;
    case 7:
        IO_COPY[0] = 0x140;
        IO_COPY[4] = 0x1C0C;
        IO_COPY[5] = 0x1D09;
        IO_COPY[6] = 0x1E0A;
        IO_COPY[7] = 0x1F0B;
        IO_COPY[0x28] = 0;
        IO_COPY[0x29] = 0;
        IO_COPY[0x2A] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x0600C000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x06008000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x800;
        gBgLayers[2].charVram = (void *)0x06008000;
        gBgLayers[3].mapVram = (void *)0x0600F800;
        gBgLayers[3].mapSize = 0x800;
        gBgLayers[3].charVram = (void *)0x06008000;
        break;
    case 8:
        IO_COPY[0] = 0x40;
        IO_COPY[4] = 0x1C00;
        IO_COPY[5] = 0x1D01;
        IO_COPY[6] = 0x1E02;
        IO_COPY[7] = 0x1F03;
        IO_COPY[0x28] = 0xFF;
        IO_COPY[0x29] = 0;
        IO_COPY[0x2A] = 0;
        IO_COPY[0x26] = 0;
        LAYER_SIZES(0x20, 0x20);
        gBgLayers[0].mapVram = (void *)0x0600E000;
        gBgLayers[0].mapSize = 0x800;
        gBgLayers[0].charVram = (void *)0x06000000;
        gBgLayers[1].mapVram = (void *)0x0600E800;
        gBgLayers[1].mapSize = 0x800;
        gBgLayers[1].charVram = (void *)0x06000000;
        gBgLayers[2].mapVram = (void *)0x0600F000;
        gBgLayers[2].mapSize = 0x800;
        gBgLayers[2].charVram = (void *)0x06000000;
        gBgLayers[3].mapVram = (void *)0x0600F800;
        gBgLayers[3].mapSize = 0x800;
        gBgLayers[3].charVram = (void *)0x06000000;
        break;
    }

    for (i = 0; i <= 3; i++)
    {
        gBgLayers[i].vofs = 0;
        gBgLayers[i].hofs = 0;
    }
    gBgLayers[0].buffer = gBgLayerBuffers;
    gBgLayers[1].buffer = gBgLayerBuffers + 0x800;
    gBgLayers[2].buffer = gBgLayerBuffers + 0x1000;
    gBgLayers[3].buffer = gBgLayerBuffers + 0x1800;
}
/**
 * Sets *map* up to show *tiles* (*width* x *height* tiles) through *layer*, then draws it from
 * position (0, 0) (Bg_ScrollMap).
 */
void Bg_InitMap(struct Map *map, struct BgLayer *layer, const u16 *tiles, u16 width, u16 height)
{
    map->layer = layer;
    map->tiles = tiles;
    map->width = width;
    map->height = height;
    map->pixelWidth = width * 8;
    map->pixelHeight = height * 8;
    Bg_ScrollMap(map, 0, 0);
}
/**
 * Moves *map* to (*x*, *y*), wrapped into the map (modulo pixelWidth / pixelHeight), and
 * redraws it into its layer's buffer (struct BgLayer buffer, marked dirty for the frame
 * callback): the fine offset (x & 7, y & 7) goes to the layer, then 21 rows of 32 tiles from
 * tile (x / 8, y / 8), the rows and columns wrapping around the map's edges. With the serial
 * interrupt on (REG_IE bits 6-7) it copies with the BIOS, else with DMA 3.
 */
void Bg_ScrollMap(struct Map *map, s32 x, s32 y)
{
    struct BgLayer *layer = map->layer;
    u16 *buf = layer->buffer;
    const u16 *tiles;
    const u16 *src;
    u32 tx;
    u32 ty;
    u32 width;
    u32 height;
    s32 count;
    s32 rest;
    s32 row;

    x %= map->pixelWidth;
    if (x < 0)
        x += map->pixelWidth;
    map->x = x;
    y %= map->pixelHeight;
    if (y < 0)
        y += map->pixelHeight;
    map->y = y;
    layer->hofs = x & 7;
    layer->vofs = y & 7;
    tiles = map->tiles;
    if (tiles == NULL)
        return;
    tx = (u32)x >> 3;
    ty = (u32)y >> 3;
    width = map->width;
    height = map->height;
    // the columns up to the map's right edge, then the rest from its left edge
    count = width - tx;
    if (count > 31)
    {
        if (REG_IE & 0xC0)
        {
            for (row = 20; row >= 0; row--)
            {
                if (ty >= height)
                    ty -= height;
                CpuCopy16(tiles + ty * width + tx, buf, 0x40);
                buf += 32;
                ty++;
            }
        }
        else
        {
            for (row = 20; row >= 0; row--)
            {
                if (ty >= height)
                    ty -= height;
                DmaCopy16(3, tiles + ty * width + tx, buf, 0x40);
                buf += 32;
                ty++;
            }
        }
    }
    else
    {
        rest = 32 - count;
        if (REG_IE & 0xC0)
        {
            for (row = 20; row >= 0; row--)
            {
                if (ty >= height)
                    ty -= height;
                src = tiles + ty * width + tx;
                CpuCopy16(src, buf, count * 2);
                src += count - width;
                buf += count;
                CpuCopy16(src, buf, rest * 2);
                buf += rest;
                ty++;
            }
        }
        else
        {
            for (row = 20; row >= 0; row--)
            {
                if (ty >= height)
                    ty -= height;
                src = tiles + ty * width + tx;
                DmaCopy16(3, src, buf, count * 2);
                src += count - width;
                buf += count;
                DmaCopy16(3, src, buf, rest * 2);
                buf += rest;
                ty++;
            }
        }
    }
    layer->dirty = 1;
}

/**
 * Moves *map* to (*x*, *y*) and redraws it into its layer's buffer (struct BgLayer
 * buffer, marked dirty for the frame callback): the fine offset (x & 7, y & 7) goes to the
 * layer, then the 32 x 21 tiles from tile (x / 8, y / 8), blank (0) outside the map. With
 * the serial interrupt on (REG_IE bits 6-7) it copies with the BIOS, else with DMA 3.
 */
void Bg_ScrollMapClamped(struct Map *map, s32 x, s32 y)
{
    struct BgLayer *layer = map->layer;
    u16 *buf = layer->buffer;
    const u16 *tiles;
    const u16 *src;
    s32 width, height;
    s32 skip, count, rest;
    s32 n, row;

    map->x = x;
    map->y = y;
    layer->hofs = x & 7;
    layer->vofs = y & 7;
    tiles = map->tiles;
    if (tiles == NULL)
        return;
    x >>= 3;
    y >>= 3;
    width = map->width;
    height = map->height;
    if (x <= -32 || x >= width || y <= -21 || y >= height)
    {
        if (REG_IE & 0xC0)
            CpuFill32(0, buf, 32 * 21 * 2)
        else
            DmaFill32(3, 0, buf, 32 * 21 * 2)
    }
    else
    {
        // the columns: skip blank ones left of the map, count from it, the rest blank
        skip = 0;
        if (x < 0)
            skip = -x;
        n = width - x;
        if (n > 32)
            n = 32;
        count = n - skip;
        rest = 32 - count - skip;
        src = tiles + y * width + x + skip;
        if (REG_IE & 0xC0)
        {
            for (row = 20; row >= 0; row--)
            {
                if (y < 0 || y >= height)
                {
                    CpuFill32(0, buf, 0x40)
                    buf += 32;
                }
                else
                {
                    if (skip != 0)
                    {
                        CpuFill16(0, buf, skip * 2)
                        buf += skip;
                    }
                    if (count != 0)
                    {
                        CpuCopy16(src, buf, count * 2);
                        buf += count;
                    }
                    if (rest != 0)
                    {
                        CpuFill16(0, buf, rest * 2)
                        buf += rest;
                    }
                }
                src += width;
                y++;
            }
        }
        else
        {
            for (row = 20; row >= 0; row--)
            {
                if (y < 0 || y >= height)
                {
                    DmaFill32(3, 0, buf, 0x40)
                    buf += 32;
                }
                else
                {
                    if (skip != 0)
                    {
                        DmaFill16(3, 0, buf, skip * 2)
                        buf += skip;
                    }
                    if (count != 0)
                    {
                        DmaCopy16(3, src, buf, count * 2);
                        buf += count;
                    }
                    if (rest != 0)
                    {
                        DmaFill16(3, 0, buf, rest * 2)
                        buf += rest;
                    }
                }
                src += width;
                y++;
            }
        }
    }
    layer->dirty = 1;
}
/**
 * Draws a minimap of a level from its level data (tile ids masked to 10 bits), for the menus
 * that show a level without a minimap layer (the Challenge menu, Practice, the Adventure map).
 * *map* is the decompressed level data: u16 width, u16 height, then the width * height tiles.
 * The minimap is 64x64 pixels of the level around its start (the first tile 0xFB or 0xFC):
 * from 16 tiles left of it and 32 rows above it (not less than 0), one pixel per tile, colour
 * 0xE for the tiles 1-0x9F (walls), 0 otherwise; rows past the level's height are empty. It
 * is drawn into OBJ VRAM at 0x060174A0, 8 x 8 tiles of 4 bits per pixel.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
void Bg_DrawLevelMinimap(const u8 *map)
{
    const u16 *tiles = (const u16 *)map;
    u32 width, height;
    s32 x, y;
    s32 row, i, j;
    u32 *dst;

    width = *tiles++;
    height = *tiles++;
    // Find the start
    for (y = 0; (u32)y < height; y++)
    {
        const u16 *p = tiles + y * width;

        for (x = 0; (u32)x < width; x++)
        {
            if ((u32)((*p++ & 0x3FF) - 0xFB) <= 1)
                goto found;
        }
    }
    y = 0;
    x = 0;
found:
    x -= 16;
    if (x < 0)
        x = 0;
    y -= 32;
    if (y < 0)
        y = 0;
    // Draw 64 rows of 64 tiles (8 words of 8 pixels each)
    for (row = 0; row < 64; row++, y++)
    {
        if ((u32)y >= height)
        {
            dst = (u32 *)(0x060174A0 + (row / 8) * 0x100 + (row % 8) * 4);
            for (i = 7; i >= 0; i--)
            {
                *dst = 0;
                dst += 8;
            }
        }
        else
        {
            const u16 *src = tiles + y * width + x;

            dst = (u32 *)(0x060174A0 + (row / 8) * 0x100 + (row % 8) * 4);
            for (i = 0; i < 8; i++)
            {
                u32 pixels = 0;

                for (j = 7; j >= 0; j--)
                {
                    u32 tile = *src++ & 0x3FF;

                    pixels >>= 4;
                    if (tile - 1 <= 0x9E)
                        pixels |= 0xE0000000;
                }
                *dst = pixels;
                dst += 8;
            }
        }
    }
}
/**
 * Makes map 2 a 30x20 map of the tiles 0-599 in order (rows 0x40 bytes apart), then copies
 * 0x9600 bytes of *tiles* to BG VRAM and *count* colours of *palette* to BG palette RAM:
 * a full-screen picture.
 */
void Bg_LoadPicture(const void *tiles, const void *palette, u16 count)
{
    u16 *p;
    s32 x, y, tile;

    Bg_InitMap(&gMaps[2], &gBgLayers[2], NULL, 30, 20);
    p = gBgLayers[2].buffer;
    tile = 0;
    for (y = 0; y < 20; y++)
    {
        for (x = 0; x < 30; x++)
            *p++ = tile++;
        p += 2;
    }
    gBgLayers[2].dirty = 1;
    Main_QueueCopy(tiles, (void *)0x06000000, 0x9600);
    Main_QueueCopy(palette, (void *)0x05000000, count * 2);
}
/**
 * Sets WIN0H from gWindowLines[0] and starts DMA 0 repeating at every HBlank, copying the
 * following entries to WIN0H one line at a time (a window shaped line by line).
 */
void Bg_WindowLineIntr(void)
{
    REG_WIN0H = gWindowLines[0];
    REG_DMA0CNT_H = 0;
    DmaSet(0, &gWindowLines[1], REG_ADDR_WIN0H,
           DMA_ENABLE | DMA_START_HBLANK | DMA_REPEAT | DMA_DEST_FIXED | DMA_16BIT | 1);
}
/**
 * Starts the screen's window effect at (*x*, *y*) with size *a* (gWindowRadius,
 * gWindowX, gWindowY; drawn by Bg_DrawWindowCircle): window 0 over the whole width and
 * height, the window copies (WININ 0x3E, WINOUT 1), BG0 and window 0 on in DISPCNT, and
 * Bg_WindowLineIntr as the VCount interrupt handler at line 0xE3 (gIntrTable[3], IE bit 2,
 * DISPSTAT), which starts the window's HBlank DMA for the next frame.
 */
void Bg_StartWindowCircle(s32 a, s32 x, s32 y)
{
    gWindowRadius = a;
    gWindowX = x;
    gWindowY = y;
    Bg_DrawWindowCircle(a, x, y);
    gWin0hCopy = 0xFF;
    gWin0vCopy = 0xA0;
    gWininCopy = 0x3E;
    gWinoutCopy = 1;
    gDispcntCopy |= 0x2100;
    gIntrTable[3] = Bg_WindowLineIntr;
    REG_IE |= 4;
    REG_DISPSTAT |= 0xE320;
}
// One line of the window gWindowLines (WIN0H for screen line *row*): the left edge in the
// high byte, the right edge in the low byte, each written only when above 0 and capped at
// 240, the line only when on screen (0..159).
#define WINDOW_LINE(row, left, right)                                              \
    {                                                                              \
        if ((u32)(row) <= 159)                                                     \
        {                                                                          \
            if ((left) > 0)                                                        \
                ((u8 *)&gWindowLines[row])[1] = (left) > 240 ? 240 : (left);      \
            if ((right) > 0)                                                       \
                ((u8 *)&gWindowLines[row])[0] = (right) > 240 ? 240 : (right);    \
        }                                                                          \
    }

/**
 * Draws a circle of radius *r* around (*x*, *y*) into the window lines gWindowLines (kept in
 * gWindowRadius, gWindowX, gWindowY): clears the 0xE4 lines (with the BIOS when
 * the serial interrupts are on, else DMA 3), then fills in each line's span with the
 * midpoint circle algorithm, one octant pair per step and the other pair once per change
 * of the first coordinate. Nothing is drawn for r <= 0.
 */
void Bg_DrawWindowCircle(s32 r, s32 x, s32 y)
{
    s32 a = r;  // the coordinate along the axis, stepping down
    s32 b = 0;  // the other, stepping up
    s32 d = 3 - 2 * r;
    s32 prevB = r - 1;
    s32 lastA = r;
    s32 left;
    s32 right;
    s32 top;
    s32 bottom;

    gWindowRadius = r;
    gWindowX = x;
    gWindowY = y;
    if (REG_IE & 0xC0)
        CpuFill16(0, gWindowLines, 0xE4 * 2)
    else
        DmaFill16(3, 0, gWindowLines, 0xE4 * 2)

    if (r > 0)
    {
        while (a >= b)
        {
            left = x - a;
            right = x + a;
            top = y - b;
            bottom = y + b;
            WINDOW_LINE(top, left, right);
            WINDOW_LINE(bottom, left, right);
            if (a != lastA)
            {
                left = x + 1 - b;
                right = x - 1 + b;
                top = y - prevB;
                bottom = prevB + y;
                WINDOW_LINE(top, left, right);
                WINDOW_LINE(bottom, left, right);
                prevB--;
                lastA = a;
            }
            if (d >= 0)
            {
                a--;
                d -= a * 4;
            }
            b++;
            d += 2 + b * 4;
        }
    }
}
/**
 * Sets up the background maps of screen *a* (0-18) with Bg_InitMap and copies their
 * tiles and palettes: 0-2 and 3 / 17 first make map 2 a 16x16 map of 8-bit entries (0-2:
 * entries 0-255 and the tiles of gUnk_080EAC64; 3 / 17: gUnk_080F7280 and its palette);
 * then map 1 (and map 0, gUnk_080F6B7C, for 0, 1, 9, 11, 13, 14 and 18) by screen. 11 and 18
 * decompress maps 2 and 3, 14 map 2, into a buffer on the stack, which the maps keep pointing
 * to after the return, as in the original. 2 and 7 copy a second palette while the save
 * file's level 4 has no mark.
 */
void Bg_LoadScreen(s32 a)
{
    u16 buf[0x400];
    u8 *p;
    s32 i;

    switch (a)
    {
    case 0:
    case 1:
    case 2:
        Bg_InitMap(&gMaps[2], &gBgLayers[2], NULL, 16, 16);
        p = gBgLayers[2].buffer;
        for (i = 0; i <= 0xFF; i++)
            *p++ = i;
        gBgLayers[2].dirty = 1;
        Main_QueueCopy(gUnk_080EAC64, (void *)0x06000000, 0x4000);
        break;
    case 3:
        Bg_InitMap(&gMaps[2], &gBgLayers[2], (const u16 *)gUnk_080F7280, 16, 16);
        Main_QueueCopy(gUnk_080F7380, (void *)0x06000000, 0x400);
        Main_QueueCopy(gUnk_080F697A, (void *)0x05000002, 0x12);
        break;
    case 17:
        Bg_InitMap(&gMaps[2], &gBgLayers[2], (const u16 *)gUnk_080F7280, 16, 16);
        Main_QueueCopy(gUnk_080F7380, (void *)0x06000000, 0x400);
        Main_QueueCopy(gUnk_080F69DA, (void *)0x05000002, 0x12);
        break;
    }

    switch (a)
    {
    case 0:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_080EA764, 0x20, 0x14);
        Main_QueueCopy(gUnk_080E3B60, gBgLayers[1].charVram, 0x8000);
        Main_QueueCopy(gUnk_080EA360, (void *)0x05000000, 0x200);
        Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_080F6B7C, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F707C, (u8 *)gBgLayers[0].buffer + 0x600, 0x200);
        break;
    case 1:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_080EEC68, 0x20, 0x14);
        Main_QueueCopy(gUnk_080EA560, (void *)0x05000000, 0x1C0);
        Main_QueueCopy(gUnk_080EF168, gBgLayers[1].charVram, 0x8000);
        Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_080F6B7C, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F707C, (u8 *)gBgLayers[0].buffer + 0x600, 0x200);
        break;
    case 2:
    case 7:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_080F3D74, 0x20, 0x14);
        Main_QueueCopy(gUnk_080EF168, gBgLayers[1].charVram, 0x8000);
        Main_QueueCopy(gUnk_080EA560, (void *)0x05000000, 0x1C0);
        Main_QueueCopy(gUnk_080F661A, (void *)0x05000002, 0x1E);
        if (gGameState.saveFile->marks[4] == 0)
            Main_QueueCopy(gUnk_080F661A, (void *)0x05000062, 0x1E);
        break;
    case 10:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_080F4278, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F4778, gBgLayers[1].charVram, 0x8000);
        Main_QueueCopy(gUnk_080F6378, (void *)0x05000000, 0x1C0);
        break;
    case 3:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_0812C488, 0x20, 0x20);
        Main_QueueCopy(gUnk_0812CC88, gBgLayers[1].charVram, 0x800);
        Main_QueueCopy(gUnk_08133968, (void *)0x05000020, 0x1A0);
        break;
    case 4:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_08136050, 0x20, 0x14);
        Main_QueueCopy(gUnk_08133B48, gBgLayers[1].charVram, 0x2000);
        Main_QueueCopy(gUnk_08133948, (void *)0x05000000, 0x1C0);
        break;
    case 15:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_08136554, 0x20, 0x14);
        Main_QueueCopy(gUnk_08133B48, gBgLayers[1].charVram, 0x2000);
        Main_QueueCopy(gUnk_08133948, (void *)0x05000000, 0x1C0);
        break;
    case 5:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_08136A58, 0x20, 0x14);
        Main_QueueCopy(gUnk_08133B48, gBgLayers[1].charVram, 0x2000);
        Main_QueueCopy(gUnk_08133948, (void *)0x05000000, 0x1C0);
        break;
    case 16:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_08136F5C, 0x20, 0x14);
        Main_QueueCopy(gUnk_08133B48, gBgLayers[1].charVram, 0x2000);
        Main_QueueCopy(gUnk_08133948, (void *)0x05000000, 0x1C0);
        break;
    case 6:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_080F316C, 0x20, 0x14);
        break;
    case 12:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_080F3670, 0x20, 0x14);
        break;
    case 9:
        Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_080F6B7C, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F707C, (u8 *)gBgLayers[0].buffer + 0x600, 0x200);
        break;
    case 18:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], (const u16 *)gUnk_0818698C, 0x20, 0x14);
        Decompress_LZ(gUnk_08186448, (u8 *)buf, 0x504);
        Bg_InitMap(&gMaps[2], &gBgLayers[2], &buf[2], 0x20, 0x14);
        Decompress_LZ(gUnk_081866A4, (u8 *)buf, 0x504);
        Bg_InitMap(&gMaps[3], &gBgLayers[3], &buf[2], 0x20, 0x14);
        Main_QueueCopy(gUnk_08182448, gBgLayers[3].charVram, 0x4000);
        Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_080F6B7C, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F707C, (u8 *)gBgLayers[0].buffer + 0x600, 0x200);
        break;
    case 11:
        Decompress_LZ(gUnk_0818683C, (u8 *)buf, 0x504);
        Bg_InitMap(&gMaps[2], &gBgLayers[2], &buf[2], 0x20, 0x14);
        Decompress_LZ(gUnk_081866A4, (u8 *)buf, 0x504);
        Bg_InitMap(&gMaps[3], &gBgLayers[3], &buf[2], 0x20, 0x14);
        Main_QueueCopy(gUnk_08182448, gBgLayers[3].charVram, 0x4000);
        Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_080F6B7C, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F707C, (u8 *)gBgLayers[0].buffer + 0x600, 0x200);
        break;
    case 13:
        Bg_InitMap(&gMaps[1], &gBgLayers[1], NULL, 0x20, 0x20);
        DmaCopy32(3, gUnk_0812BC84, gBgLayers[1].buffer, 0x800);
        gBgLayers[1].dirty = 1;
        Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_080F6B7C, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F707C, (u8 *)gBgLayers[0].buffer + 0x600, 0x200);
        break;
    case 14:
        Decompress_LZ(gUnk_08187B88, (u8 *)buf, 0x504);
        Bg_InitMap(&gMaps[2], &gBgLayers[2], &buf[2], 0x20, 0x14);
        Main_QueueCopy(gUnk_08186F88, gBgLayers[2].charVram, 0x1C00);
        Bg_InitMap(&gMaps[0], &gBgLayers[0], (const u16 *)gUnk_080F6B7C, 0x20, 0x14);
        Main_QueueCopy(gUnk_080F707C, (u8 *)gBgLayers[0].buffer + 0x600, 0x200);
        break;
    }
}
/**
 * Bg_LoadMap for 8bpp tiles: sets map *n* up with *width* x *height* tiles, copies its
 * decompressed tile map row by row, 0x40 bytes apart, to gBgLayers[n].mapVram, then the
 * 0x40 bytes at *gfx* + *t* * 0x20 to gBgLayers[n].charVram and on for each entry *t* of
 * *list* up to 0xFFFF.
 */
void Bg_LoadMap8bpp(s32 n, const u8 *src, const u8 *gfx, const u16 *list, u32 width, s32 height)
{
    u16 buf[0x400];
    const u16 *row;
    u8 *dst;
    u8 *tileDst;
    u32 t;

    Bg_InitMap(&gMaps[n], &gBgLayers[n], NULL, width, height);
    Decompress_LZ(src, (u8 *)buf, width * height * 2);
    tileDst = gBgLayers[n].charVram;
    dst = gBgLayers[n].mapVram;
    row = buf;
    for (; height > 0; height--)
    {
        DMA3_SET(row, dst, DMA_ENABLE | width);
        row += width;
        dst += 0x40;
    }
    while ((t = *list++) != 0xFFFF)
    {
        DMA3_SET(gfx + t * 0x20, tileDst, DMA_ENABLE | DMA_32BIT | 0x10);
        tileDst += 0x40;
    }
}
/**
 * Sets map *n* up (Bg_InitMap, no tiles) with *width* x *height* tiles and draws it:
 * decompresses its tile map from *src* (Decompress_LZ) and copies it row by row, 0x40 bytes
 * apart, to gBgLayers[n].mapVram (DMA 3), then copies tile *t* of *gfx* (0x20 bytes) to
 * gBgLayers[n].charVram and on for each entry of *list* up to 0xFFFF.
 */
void Bg_LoadMap(s32 n, const u8 *src, const u8 *gfx, const u16 *list, u32 width, s32 height)
{
    u16 buf[0x400];
#ifndef PLATFORM_PC
    vu32 *dma = (vu32 *)REG_ADDR_DMA3;
#endif
    const u16 *row;
    u8 *dst;
    u8 *tileDst;
    u32 t;

    Bg_InitMap(&gMaps[n], &gBgLayers[n], NULL, width, height);
    Decompress_LZ(src, (u8 *)buf, width * height * 2);
    tileDst = gBgLayers[n].charVram;
    dst = gBgLayers[n].mapVram;
    row = buf;
    for (; height > 0; height--)
    {
#ifdef PLATFORM_PC
        // The PC port's DMA unit (docs/port.md) runs a transfer when called, not when the
        // control register is written: the same writes, through Port_Dma.
        Port_Dma(3, row, dst, DMA_ENABLE | width);
#else
        dma[0] = (u32)row;
        dma[1] = (u32)dst;
        dma[2] = DMA_ENABLE | width;
#endif
        row += width;
        dst += 0x40;
    }
    while ((t = *list++) != 0xFFFF)
    {
#ifdef PLATFORM_PC
        Port_Dma(3, gfx + t * 0x20, tileDst, DMA_ENABLE | DMA_32BIT | 8);
#else
        dma[0] = (u32)(gfx + t * 0x20);
        dma[1] = (u32)tileDst;
        dma[2] = DMA_ENABLE | DMA_32BIT | 8;
#endif
        tileDst += 0x20;
    }
}
