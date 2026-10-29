/* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy
 * of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Derived from mGBA's software renderer (https://mgba.io, version 0.10.5), Copyright (c)
 * 2013-2015 Jeffrey Pfau: src/gba/renderers/video-software.c, software-mode0.c, software-obj.c,
 * software-private.h and include/mgba/core/interface.h (mColorMix5Bit). The window splitting,
 * the compositing and its pixel flags, the colour arithmetic, the text BG's partial-tile drawing
 * and the sprite drawing follow that code; rewritten for this port (only what the game uses,
 * one frame at a time from the GBA's memory). This file alone is under the MPL 2.0 (its
 * file-level copyleft: port/NOTICE.md); the rest of the port is not. */
// The display (docs/port.md, "The renderer"): a scanline renderer that draws the GBA's 240x160
// screen from VRAM, palette RAM, OAM and the video registers, as the game left them at the end
// of a frame's work, one line at a time; between two lines it runs what the hardware does at
// HBlank (the HBlank DMA that shapes window 0, the HBlank interrupt, VCount matches).
//
// It implements what the game uses (docs/port.md, "What the game draws with"): BG modes 0 and 1,
// 4bpp and 8bpp text BGs, the affine BG2, 4bpp sprites (regular, affine, double-size, flipped,
// semi-transparent) with the per-line sprite cycle budget, windows 0 and 1, alpha blending,
// brightness up and down, forced blank. Not implemented, because the game never uses them:
// bitmap modes 3-5, BG mode 2, 8bpp sprites, the OBJ window, mosaic (the game sets BG2's mosaic
// bit only with MOSAIC = 0, which changes nothing), the 2D sprite tile mapping.
//
// The reference for the pixels is mGBA's software renderer (what kurutest and the pixel test
// compare with): the colours are its 8-bit-per-channel ones (a 5-bit channel c is c << 3 | c >> 2,
// 0x00BBGGRR), blending and brightness are computed on those, and layers are composited the way
// it does, through flags kept in each pixel's top byte (which layer and priority a pixel is from,
// whether it is a first or second blend target); see mGBA's src/gba/renderers/video-software.c.
#include "port.h"

#include <stdio.h>
#include <string.h>

uint32_t gPortScreen[160 * 240];

#define VRAM8 ((const uint8_t *)(uintptr_t)VRAM_START)
#define VRAM16 ((const uint16_t *)(uintptr_t)VRAM_START)
#define PLTT16 ((const uint16_t *)(uintptr_t)PLTT_START)
#define OAM16 ((const uint16_t *)(uintptr_t)OAM_START)

// A pixel of the line buffer: the colour in the low 24 bits, then these flags. The order flags
// make a lower value a pixel in front: priority, then the BG number, then BG vs OBJ.
#define FLAG_PRIORITY 0xC0000000u
#define FLAG_INDEX 0x30000000u
#define FLAG_IS_BACKGROUND 0x08000000u
#define FLAG_UNWRITTEN 0xFC000000u  // the backdrop, and an empty sprite pixel
#define FLAG_REBLEND 0x04000000u    // a semi-transparent sprite: blended after all layers
#define FLAG_TARGET_1 0x02000000u
#define FLAG_TARGET_2 0x01000000u
#define FLAG_ORDER_MASK 0xF8000000u
#define OFFSET_PRIORITY 30
#define OFFSET_INDEX 28
#define IS_WRITABLE(p) ((p) & 0xFE000000u)
#define COLOR_MASK 0x00FFFFFFu

enum { BLEND_NONE, BLEND_ALPHA, BLEND_BRIGHTEN, BLEND_DARKEN };

// Window control bits (WININ, WINOUT)
#define WIN_BG(n) (1u << (n))
#define WIN_OBJ 0x10u
#define WIN_BLEND 0x20u

struct Background {
    int index;
    bool enabled;
    int priority;
    uint32_t charBase, screenBase;
    bool multipalette, overflow;
    int size;
    int hofs, vofs;
    int16_t dx, dmx, dy, dmy;  // BGxPA-PD
    int32_t sx, sy;            // affine: the reference point of the line being drawn
    bool target1, target2;
    uint32_t flags;  // the flags its pixels get in the current window segment
    bool variant;    // its pixels use the brightened / darkened palette
};

struct Segment {
    int endX;
    uint8_t control;
};

struct WindowN {
    int hStart, hEnd, vStart, vEnd;
    uint8_t control;
};

struct Sprite {
    int y, endY, cycles;
    uint16_t a, b, c;
};

static uint16_t sDispcnt;
static struct Background sBg[4];
static int sEffect;
static bool sTarget1Obj, sTarget1Bd, sTarget2Obj, sTarget2Bd;
static int sBlda, sBldb, sBldy;
static struct WindowN sWin[2];
static uint8_t sWinout;
static struct Segment sSegments[5];
static int sNumSegments;
static int sStart, sEnd;  // the window segment being drawn
static uint8_t sCurrentWindow;
static bool sForceTarget1;
static uint32_t sRow[240], sSpriteLayer[240];
static uint32_t sNormalPalette[512], sVariantPalette[512];
static int sVariantKey;  // (effect << 8 | BLDY) the variant palette is for; -1: none
static struct Sprite sSprites[128];
static int sNumSprites;
static int sSpriteCyclesRemaining;

static const uint8_t kObjSizes[16][2] = {
    { 8, 8 },  { 16, 16 }, { 32, 32 }, { 64, 64 }, { 16, 8 }, { 32, 8 }, { 32, 16 }, { 64, 32 },
    { 8, 16 }, { 8, 32 },  { 16, 32 }, { 32, 64 }, { 0, 0 },  { 0, 0 },  { 0, 0 },   { 0, 0 },
};

static void unsupported(const char *what) {
    static const char *seen[16];
    for (int i = 0; i < 16; i++) {
        if (seen[i] == what)
            return;
        if (!seen[i]) {
            seen[i] = what;
            fprintf(stderr, "kururin_pc: the renderer does not draw %s (docs/port.md)\n", what);
            return;
        }
    }
}

// --- colours (mGBA's 8-bit channels) -------------------------------------------------------

static uint32_t colorFrom555(uint16_t v) {
    uint32_t c = ((v & 0x1Fu) << 3) | ((v >> 5 & 0x1Fu) << 11) | ((v >> 10 & 0x1Fu) << 19);
    return c | ((c >> 5) & 0x070707u);
}

static uint32_t brighten(uint32_t color, int y) {
    uint32_t c = 0, a;
    a = color & 0xFF;
    c |= (a + ((0xFF - a) * y) / 16) & 0xFF;
    a = color & 0xFF00;
    c |= (a + ((0xFF00 - a) * y) / 16) & 0xFF00;
    a = color & 0xFF0000;
    c |= (a + ((0xFF0000 - a) * y) / 16) & 0xFF0000;
    return c;
}

static uint32_t darken(uint32_t color, int y) {
    uint32_t c = 0, a;
    a = color & 0xFF;
    c |= (a - (a * y) / 16) & 0xFF;
    a = color & 0xFF00;
    c |= (a - (a * y) / 16) & 0xFF00;
    a = color & 0xFF0000;
    c |= (a - (a * y) / 16) & 0xFF0000;
    return c;
}

// (A * weightA + B * weightB) / 16 per channel, saturated.
static uint32_t mix(int weightA, uint32_t colorA, int weightB, uint32_t colorB) {
    uint32_t c = 0, a, b;
    a = colorA & 0xFF;
    b = colorB & 0xFF;
    c |= ((a * weightA + b * weightB) / 16) & 0x1FF;
    if (c & 0x100)
        c = 0xFF;
    a = colorA & 0xFF00;
    b = colorB & 0xFF00;
    c |= ((a * weightA + b * weightB) / 16) & 0x1FF00;
    if (c & 0x10000)
        c = (c & 0xFF) | 0xFF00;
    a = colorA & 0xFF0000;
    b = colorB & 0xFF0000;
    c |= ((a * weightA + b * weightB) / 16) & 0x1FF0000;
    if (c & 0x1000000)
        c = (c & 0xFFFF) | 0xFF0000;
    return c;
}

static void updateVariantPalette(void) {
    int key = (sEffect == BLEND_BRIGHTEN || sEffect == BLEND_DARKEN) ? (sEffect << 8 | sBldy) : -1;
    if (key == sVariantKey || key < 0)
        return;
    sVariantKey = key;
    for (int i = 0; i < 512; i++)
        sVariantPalette[i] = sEffect == BLEND_BRIGHTEN ? brighten(sNormalPalette[i], sBldy) : darken(sNormalPalette[i], sBldy);
}

// --- compositing ----------------------------------------------------------------------------

// A layer's pixel *color* (with its flags) drawn under or over *pixel*: the front one stays,
// and a first target over a second target becomes their alpha blend.
static void compositeBlend(uint32_t *pixel, uint32_t color) {
    uint32_t current = *pixel;
    if (color >= current) {
        if ((current & FLAG_TARGET_1) && (color & FLAG_TARGET_2))
            color = mix(sBlda, current, sBldb, color);
        else
            color = current & (COLOR_MASK | FLAG_REBLEND | FLAG_TARGET_2);
    } else {
        color &= ~FLAG_TARGET_2;
    }
    *pixel = color;
}

static void compositeNoBlend(uint32_t *pixel, uint32_t color) {
    uint32_t current = *pixel;
    if (color >= current)
        color = current & (COLOR_MASK | FLAG_REBLEND | FLAG_TARGET_2);
    *pixel = color;
}

static void compositeBg(const struct Background *bg, uint32_t *pixel, uint32_t color) {
    if (!IS_WRITABLE(*pixel))
        return;
    if (bg->flags & FLAG_TARGET_2)
        compositeBlend(pixel, color | bg->flags);
    else
        compositeNoBlend(pixel, color | bg->flags);
}

// --- registers ---------------------------------------------------------------------------------

static void latchRegisters(void) {
    sDispcnt = IO16(0x000) & 0xFFF7;
    for (int n = 0; n < 4; n++) {
        struct Background *bg = &sBg[n];
        uint16_t cnt = IO16(0x008 + 2 * n);
        if (n < 2)
            cnt &= 0xDFFF;
        bg->index = n;
        bg->priority = cnt & 3;
        bg->charBase = (uint32_t)(cnt >> 2 & 3) << 14;
        if (cnt & 0x40) {
            uint16_t mosaic = IO16(0x04C);
            if (mosaic & 0xFF)
                unsupported("BG mosaic");
        }
        bg->multipalette = cnt & 0x80;
        bg->screenBase = (uint32_t)(cnt >> 8 & 0x1F) << 11;
        bg->overflow = cnt & 0x2000;
        bg->size = cnt >> 14;
        bg->hofs = IO16(0x010 + 4 * n) & 0x1FF;
        bg->vofs = IO16(0x012 + 4 * n) & 0x1FF;
        bg->enabled = sDispcnt & (0x100 << n);
    }
    for (int n = 2; n < 4; n++) {
        uint32_t base = 0x020 + 0x10 * (uint32_t)(n - 2);
        sBg[n].dx = (int16_t)IO16(base);
        sBg[n].dmx = (int16_t)IO16(base + 2);
        sBg[n].dy = (int16_t)IO16(base + 4);
        sBg[n].dmy = (int16_t)IO16(base + 6);
    }

    for (int n = 0; n < 2; n++) {
        uint16_t h = IO16(0x040 + 2 * n), v = IO16(0x044 + 2 * n);
        struct WindowN *w = &sWin[n];
        w->hEnd = h & 0xFF;
        w->hStart = h >> 8;
        if (w->hStart > 240 && w->hStart > w->hEnd)
            w->hStart = 0;
        if (w->hEnd > 240) {
            w->hEnd = 240;
            if (w->hStart > 240)
                w->hStart = 240;
        }
        w->vEnd = v & 0xFF;
        w->vStart = v >> 8;
        if (w->vStart > 160 && w->vStart > w->vEnd)
            w->vStart = 0;
        if (w->vEnd > 160) {
            w->vEnd = 160;
            if (w->vStart > 160)
                w->vStart = 160;
        }
        w->control = (IO16(0x048) >> (8 * n)) & 0x3F;
    }
    sWinout = IO16(0x04A) & 0x3F;

    uint16_t bldcnt = IO16(0x050);
    for (int n = 0; n < 4; n++) {
        sBg[n].target1 = bldcnt & (1u << n);
        sBg[n].target2 = bldcnt & (0x100u << n);
    }
    sEffect = bldcnt >> 6 & 3;
    sTarget1Obj = bldcnt & 0x10;
    sTarget1Bd = bldcnt & 0x20;
    sTarget2Obj = bldcnt & 0x1000;
    sTarget2Bd = bldcnt & 0x2000;
    uint16_t bldalpha = IO16(0x052);
    sBlda = bldalpha & 0x1F;
    if (sBlda > 16)
        sBlda = 16;
    sBldb = bldalpha >> 8 & 0x1F;
    if (sBldb > 16)
        sBldb = 16;
    sBldy = IO16(0x054) & 0x1F;
    if (sBldy > 16)
        sBldy = 16;
}

// The reference points of the affine BGs: the registers, at the start of a frame.
static int32_t refPoint(uint32_t ofs) {
    uint32_t v = IO32(ofs);
    return (int32_t)(v << 4) >> 4;
}

// --- windows ------------------------------------------------------------------------------------

// The line split into segments of one window control each: WINOUT, then window 1's span over
// it, then window 0's (it is in front).
static void breakWindowInner(const struct WindowN *win, int hStart, int hEnd) {
    int startX = 0;
    if (hEnd <= 0)
        return;
    for (int active = 0; active < sNumSegments; ++active) {
        if (hStart < sSegments[active].endX) {
            struct Segment old = sSegments[active];
            if (hStart > startX) {
                for (int next = sNumSegments++; next > active; --next)
                    sSegments[next] = sSegments[next - 1];
                sSegments[active].endX = hStart;
                ++active;
            }
            sSegments[active].control = win->control;
            sSegments[active].endX = hEnd;
            if (hEnd >= old.endX) {
                // the segments it covers entirely
                for (++active; sNumSegments > active + 1 && hEnd >= sSegments[active].endX; ++active) {
                    sSegments[active] = sSegments[active + 1];
                    --sNumSegments;
                }
            } else {
                ++active;
                for (int next = sNumSegments++; next > active; --next)
                    sSegments[next] = sSegments[next - 1];
                sSegments[active] = old;
            }
            return;
        }
        startX = sSegments[active].endX;
    }
}

static void breakWindow(const struct WindowN *win, int y) {
    if (win->vEnd >= win->vStart) {
        if (y >= win->vEnd || y < win->vStart)
            return;
    } else if (y >= win->vEnd && y < win->vStart) {
        return;
    }
    if (win->hEnd < win->hStart) {
        breakWindowInner(win, 0, win->hEnd);
        breakWindowInner(win, win->hStart, 240);
    } else {
        breakWindowInner(win, win->hStart, win->hEnd);
    }
}

// --- backgrounds --------------------------------------------------------------------------------

static void updateFlags(struct Background *bg) {
    uint32_t flags = (uint32_t)bg->priority << OFFSET_PRIORITY | (uint32_t)bg->index << OFFSET_INDEX | FLAG_IS_BACKGROUND;
    if (bg->target2)
        flags |= FLAG_TARGET_2;
    if (sEffect == BLEND_ALPHA) {
        if (sBlda == 16 && sBldb == 0)
            flags &= ~FLAG_TARGET_2;
        else if (bg->target1 && (sCurrentWindow & WIN_BLEND))
            flags |= FLAG_TARGET_1;
    }
    bg->flags = flags;
    bg->variant = bg->target1 && (sCurrentWindow & WIN_BLEND) && (sEffect == BLEND_BRIGHTEN || sEffect == BLEND_DARKEN);
}

// A text BG: 32x32-tile screen blocks of 16-bit map entries (tile, H/V flip, palette).
//
// Drawn the way mGBA does it, tile by tile: a first partial tile, the whole tiles, a last
// partial tile, in each window segment. That shows in two places, both only in narrow window
// segments (the circle window's tips): mGBA reads a 4bpp partial tile's pixels even when the
// tile lies past BG VRAM (whole tiles there are transparent), and when a segment ends inside
// its first tile, that tile's pixels are taken from the wrong end if it is H-flipped (4bpp and
// 8bpp) or from the second half of the tile (8bpp, from the left half of a tile).

struct TextLine {
    const struct Background *bg;
    const uint32_t *palette;  // normal or variant
    uint32_t yBase;           // the map row (halfword index into VRAM)
    int inY;
};

static uint16_t textMap(const struct TextLine *t, uint32_t localX) {
    uint32_t xBase = localX & 0xF8;
    if (t->bg->size & 1)
        xBase += (localX & 0x100) << 5;
    return VRAM16[t->yBase + (xBase >> 3)];
}

static uint32_t tileWord(uint32_t charBase) {
    uint32_t w;
    memcpy(&w, VRAM8 + charBase, 4);
    return w;
}

static void textPixel(const struct TextLine *t, int x, uint32_t pixel, const uint32_t *palette) {
    if (pixel)
        compositeBg(t->bg, &sRow[x], palette[pixel]);
}

static uint32_t textCharBase(const struct TextLine *t, uint16_t map) {
    int row = t->inY & 7;
    if (map & 0x800)
        row = 7 - row;
    if (t->bg->multipalette)
        return t->bg->charBase + ((map & 0x3FFu) << 6) + ((uint32_t)row << 3);
    return t->bg->charBase + ((map & 0x3FFu) << 5) + ((uint32_t)row << 2);
}

// The first partial tile, from column *mod8*: pixels [outX, end)
static void textFirstTile(const struct TextLine *t, uint16_t map, int mod8, int outX, int end) {
    uint32_t charBase = textCharBase(t, map);
    bool hflip = map & 0x400;
    if (!t->bg->multipalette) {
        const uint32_t *palette = t->palette + ((map >> 12) << 4);
        uint32_t data = tileWord(charBase);
        if (!hflip) {
            data >>= 4 * mod8;
            for (; outX < end; ++outX, data >>= 4)
                textPixel(t, outX, data & 0xF, palette);
        } else {
            for (int x = end - 1; x >= sStart; --x, data >>= 4)
                textPixel(t, x, data & 0xF, palette);
        }
        return;
    }
    if (charBase >= 0x10000)
        return;
    int end2 = end - 4;
    if (!hflip) {
        int shift = mod8 & 3;
        if (end2 > outX) {
            uint32_t data = tileWord(charBase) >> (8 * shift);
            shift = 0;
            for (; outX < end2; ++outX, data >>= 8)
                textPixel(t, outX, data & 0xFF, t->palette);
        }
        uint32_t data = tileWord(charBase + 4) >> (8 * shift);
        for (; outX < end; ++outX, data >>= 8)
            textPixel(t, outX, data & 0xFF, t->palette);
    } else {
        int x = end - 1;
        if (end2 > outX) {
            uint32_t data = tileWord(charBase);
            for (; x >= end2; --x, data >>= 8)
                textPixel(t, x, data & 0xFF, t->palette);
            charBase += 4;
        }
        uint32_t data = tileWord(charBase);
        for (; x >= sStart; --x, data >>= 8)
            textPixel(t, x, data & 0xFF, t->palette);
    }
}

// A whole tile at pixels [outX, outX + 8)
static void textWholeTile(const struct TextLine *t, uint16_t map, int outX) {
    uint32_t charBase = textCharBase(t, map);
    if (charBase >= 0x10000)
        return;
    bool hflip = map & 0x400;
    for (int k = 0; k < 8; k++) {
        int col = hflip ? 7 - k : k;
        if (!t->bg->multipalette)
            textPixel(t, outX + k, (VRAM8[charBase + (col >> 1)] >> ((col & 1) * 4)) & 0xF,
                      t->palette + ((map >> 12) << 4));
        else
            textPixel(t, outX + k, VRAM8[charBase + col], t->palette);
    }
}

// The last partial tile: *count* pixels from outX to the segment's end
static void textLastTile(const struct TextLine *t, uint16_t map, int count, int outX) {
    uint32_t charBase = textCharBase(t, map);
    bool hflip = map & 0x400;
    if (!t->bg->multipalette) {
        const uint32_t *palette = t->palette + ((map >> 12) << 4);
        uint32_t data = tileWord(charBase);
        if (!hflip) {
            for (; outX < sEnd; ++outX, data >>= 4)
                textPixel(t, outX, data & 0xF, palette);
        } else {
            data >>= 4 * (8 - count);
            int end = sEnd - 8 < -1 ? -1 : sEnd - 8;
            for (int x = sEnd - 1; x > end; --x, data >>= 4)
                textPixel(t, x, data & 0xF, palette);
        }
        return;
    }
    if (charBase >= 0x10000)
        return;
    int end = count - 4;
    if (!hflip) {
        if (end > 0) {
            uint32_t data = tileWord(charBase);
            for (; outX < sEnd - end; ++outX, data >>= 8)
                textPixel(t, outX, data & 0xFF, t->palette);
            charBase += 4;
        }
        uint32_t data = tileWord(charBase);
        for (; outX < sEnd; ++outX, data >>= 8)
            textPixel(t, outX, data & 0xFF, t->palette);
    } else {
        int shift = (8 - count) & 3, start = outX, x = sEnd - 1;
        if (end > 0) {
            uint32_t data = tileWord(charBase) >> (8 * shift);
            for (; x >= start + 4; --x, data >>= 8)
                textPixel(t, x, data & 0xFF, t->palette);
            shift = 0;
        }
        uint32_t data = tileWord(charBase + 4) >> (8 * shift);
        for (; x >= start; --x, data >>= 8)
            textPixel(t, x, data & 0xFF, t->palette);
    }
}

static void drawTextBg(const struct Background *bg, int y) {
    struct TextLine t = { bg, bg->variant ? sVariantPalette : sNormalPalette, 0, y + bg->vofs };
    uint32_t yBase = (uint32_t)t.inY & 0xF8;
    if (bg->size == 2)
        yBase += (uint32_t)t.inY & 0x100;
    else if (bg->size == 3)
        yBase += ((uint32_t)t.inY & 0x100) << 1;
    t.yBase = (bg->screenBase >> 1) + (yBase << 2);

    int inX = (sStart + bg->hofs) & 0x1FF;
    int length = sEnd - sStart;
    int tileEnd = ((length + inX) >> 3) - (inX >> 3);
    int tileX = 0, outX = sStart;
    if (inX & 7) {
        int mod8 = inX & 7;
        int end = outX + 8 - mod8;
        if (end > sEnd)
            end = sEnd;
        if (end == outX)
            return;
        textFirstTile(&t, textMap(&t, (uint32_t)(tileX * 8 + inX) & 0x1FF), mod8, outX, end);
        outX = end;
        if (tileX < tileEnd)
            ++tileX;
        length -= end - sStart;
    }
    for (uint32_t localX = (uint32_t)(tileX * 8 + inX); tileX < tileEnd; ++tileX, localX += 8, outX += 8)
        textWholeTile(&t, textMap(&t, localX & 0x1FF), outX);
    if (length & 7)
        textLastTile(&t, textMap(&t, (uint32_t)(tileX * 8 + inX) & 0x1FF), length & 7, outX);
}

// An affine BG: a square map of 8-bit tile numbers, 8bpp tiles, sampled at (sx, sy) +
// x * (PA, PC) in 8.8 fixed point.
static void drawAffineBg(const struct Background *bg) {
    const uint32_t *palette = bg->variant ? sVariantPalette : sNormalPalette;
    int32_t size = 0x8000 << bg->size;
    int32_t x = bg->sx + (sStart - 1) * bg->dx;
    int32_t y = bg->sy + (sStart - 1) * bg->dy;
    for (int outX = sStart; outX < sEnd; outX++) {
        x += bg->dx;
        y += bg->dy;
        int32_t localX, localY;
        if (bg->overflow) {
            localX = x & (size - 1);
            localY = y & (size - 1);
        } else {
            if ((x | y) & ~(size - 1))
                continue;
            localX = x;
            localY = y;
        }
        uint8_t map = VRAM8[bg->screenBase + (uint32_t)((localX >> 11) + (((localY >> 7) & 0x7F0) << bg->size))];
        uint8_t pixel = VRAM8[bg->charBase + ((uint32_t)map << 6) + (uint32_t)((localY & 0x700) >> 5) + (uint32_t)((localX & 0x700) >> 8)];
        if (pixel)
            compositeBg(bg, &sRow[outX], palette[pixel]);
    }
}

// --- sprites ------------------------------------------------------------------------------------

// The sprites that can be on screen, in OAM order (hidden ones and ones entirely off the
// screen left out): the order the hardware fetches them in.
static void loadSprites(void) {
    sNumSprites = 0;
    for (int i = 0; i < 128; i++) {
        uint16_t a = OAM16[i * 4], b = OAM16[i * 4 + 1], c = OAM16[i * 4 + 2];
        bool affine = a & 0x100;
        if (!affine && (a & 0x200))
            continue;
        int shape = a >> 14, size = b >> 14;
        int width = kObjSizes[shape * 4 + size][0], height = kObjSizes[shape * 4 + size][1];
        int cycles = width;
        if (affine) {
            height <<= (a >> 9) & 1;
            width <<= (a >> 9) & 1;
            cycles = 10 + width * 2;
        }
        int y = a & 0xFF, x = b & 0x1FF;
        if (y >= 160 && y + height < 228)
            continue;
        if (x >= 240 && x + width < 512)
            continue;
        sSprites[sNumSprites++] = (struct Sprite){ y, y + height, cycles, a, b, c };
    }
}

// One sprite pixel into the sprite line: an opaque pixel of a sprite in front of what is
// there replaces it; a transparent one still gives the pixel its priority (the hardware's
// OBJ-vs-OBJ order: by OAM number, then the priority of the last sprite that covers it).
static void spritePixel(int outX, uint32_t pixel, const uint32_t *palette, uint32_t flags) {
    uint32_t current = sSpriteLayer[outX];
    if ((current & FLAG_ORDER_MASK) > flags) {
        if (pixel)
            sSpriteLayer[outX] = palette[pixel] | flags;
        else if (current != FLAG_UNWRITTEN)
            sSpriteLayer[outX] = (current & ~(FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)) |
                                 (flags & (FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1));
    }
}

// A 4bpp pixel of a sprite in 1D mapping: tiles in order, width / 8 per tile row.
static uint32_t spriteTexel(uint32_t charBase, uint32_t stride, int localX, int localY) {
    uint32_t xBase = (uint32_t)(localX & ~7) * 4 + (uint32_t)((localX >> 1) & 2);
    uint32_t yBase = (uint32_t)(localY & ~7) * stride + (uint32_t)(localY & 7) * 4;
    uint32_t ofs = (yBase + ((xBase + charBase) & 0x7FFE)) & 0x7FFE;
    uint16_t data = VRAM16[(0x10000 + ofs) >> 1];
    return (data >> ((localX & 3) << 2)) & 0xF;
}

// Draws one sprite's part of line *y* into the sprite line, within the current window
// segment; returns whether it drew (its priority's layer then needs compositing).
static int drawSprite(const struct Sprite *s, int y) {
    uint16_t a = s->a, b = s->b, c = s->c;
    int width = kObjSizes[(a >> 14) * 4 + (b >> 14)][0], height = kObjSizes[(a >> 14) * 4 + (b >> 14)][1];
    int mode = a >> 10 & 3;
    uint32_t flags = (uint32_t)(c >> 10 & 3) << OFFSET_PRIORITY;
    bool winBlend = sCurrentWindow & WIN_BLEND;
    if ((winBlend && sTarget1Obj && sEffect == BLEND_ALPHA) || mode == 1)
        flags |= FLAG_TARGET_1;
    if (mode == 2) {
        unsupported("OBJ window sprites");
        return 0;
    }
    if (a & 0x2000) {
        unsupported("8bpp sprites");
        return 0;
    }
    if (a & 0x1000)
        unsupported("sprite mosaic");
    if (!(sDispcnt & 0x40))
        unsupported("2D sprite tile mapping");
    int32_t x = (int32_t)((uint32_t)b << 23) >> 23;
    uint32_t charBase = (c & 0x3FFu) * 0x20;
    bool variant = sTarget1Obj && winBlend && (sEffect == BLEND_BRIGHTEN || sEffect == BLEND_DARKEN);
    if (mode == 1 || (sTarget1Obj && sEffect == BLEND_ALPHA)) {
        bool target2 = sTarget2Bd;
        for (int n = 0; n < 4; n++)
            target2 |= sBg[n].target2 && sBg[n].enabled;
        if (target2) {
            sForceTarget1 = true;
            flags |= FLAG_REBLEND;
            variant = false;
        } else {
            flags &= ~FLAG_TARGET_1;
        }
    }
    const uint32_t *palette = (variant ? sVariantPalette : sNormalPalette) + 0x100 + ((c >> 12) << 4);
    int inY = y - (a & 0xFF);
    uint32_t stride = (uint32_t)width >> 1;

    if (a & 0x100) {
        int totalWidth = width << (a >> 9 & 1), totalHeight = height << (a >> 9 & 1);
        const uint16_t *m = &OAM16[((b >> 9) & 0x1F) * 16];
        int16_t pa = (int16_t)m[3], pb = (int16_t)m[7], pc = (int16_t)m[11], pd = (int16_t)m[15];
        if (inY < 0)
            inY += 256;
        int outX = x >= sStart ? x : sStart;
        int condition = x + totalWidth;
        int inX = outX - x;
        if (sEnd < condition)
            condition = sEnd;
        int xAccum = pa * (inX - 1 - (totalWidth >> 1)) + pb * (inY - (totalHeight >> 1)) + (width << 7);
        int yAccum = pc * (inX - 1 - (totalWidth >> 1)) + pd * (inY - (totalHeight >> 1)) + (height << 7);
        // Skip the pixels before the sprite's texture starts (as mGBA does)
        if (pa) {
            if ((xAccum >> 8) < 0) {
                int32_t skip = (-xAccum - 1) / pa;
                xAccum += pa * skip, yAccum += pc * skip, outX += skip, inX += skip;
            } else if ((xAccum >> 8) >= width) {
                int32_t skip = ((width << 8) - xAccum) / pa;
                xAccum += pa * skip, yAccum += pc * skip, outX += skip, inX += skip;
            }
        }
        if (pc) {
            if ((yAccum >> 8) < 0) {
                int32_t skip = (-yAccum - 1) / pc;
                xAccum += pa * skip, yAccum += pc * skip, outX += skip, inX += skip;
            } else if ((yAccum >> 8) >= height) {
                int32_t skip = ((height << 8) - yAccum) / pc;
                xAccum += pa * skip, yAccum += pc * skip, outX += skip, inX += skip;
            }
        }
        if (outX < sStart || outX >= condition)
            return 0;
        for (; outX < condition; ++outX, ++inX) {
            xAccum += pa;
            yAccum += pc;
            int localX = xAccum >> 8, localY = yAccum >> 8;
            if ((localX & ~(width - 1)) || (localY & ~(height - 1)))
                break;
            spritePixel(outX, spriteTexel(charBase, stride, localX, localY), palette, flags);
        }
    } else {
        int outX = x >= sStart ? x : sStart;
        int condition = x + width;
        if ((a & 0xFF) + height - 256 >= 0)
            inY += 256;
        if (b & 0x2000)
            inY = height - inY - 1;
        if (sEnd < condition)
            condition = sEnd;
        int inX = outX - x, step = 1;
        if (b & 0x1000) {
            inX = width - inX - 1;
            step = -1;
        }
        for (; outX < condition; ++outX, inX += step)
            spritePixel(outX, spriteTexel(charBase, stride, inX, inY), palette, flags);
    }
    return 1;
}

// The sprite line of line *y*: every sprite on it, in OAM order, until the line's sprite
// cycles run out (1210, or 954 with DISPCNT's "HBlank interval free"); returns the priorities
// drawn, one bit each.
static int drawSprites(int y) {
    int layers = 0;
    if (!(sDispcnt & 0x1000))
        return 0;
    sSpriteCyclesRemaining = (sDispcnt & 0x20) ? 954 : 1210;
    for (int i = 0; i < sNumSprites; i++) {
        const struct Sprite *s = &sSprites[i];
        if ((y < s->y && (s->endY - 256 < 0 || y >= s->endY - 256)) || y >= s->endY)
            continue;
        sEnd = 0;
        for (int w = 0; w < sNumSegments; w++) {
            sCurrentWindow = sSegments[w].control;
            sStart = sEnd;
            sEnd = sSegments[w].endX;
            if (!(sCurrentWindow & WIN_OBJ))
                continue;
            layers |= drawSprite(s, y) << (s->c >> 10 & 3);
        }
        sSpriteCyclesRemaining -= s->cycles;
        if (sSpriteCyclesRemaining <= 0)
            break;
    }
    return layers;
}

// The sprites of one priority into the line, within the current segment.
static void compositeSprites(int priority) {
    if (!(sCurrentWindow & WIN_OBJ))
        return;
    uint32_t flags = sTarget2Obj ? FLAG_TARGET_2 : 0;
    for (int x = sStart; x < sEnd; x++) {
        uint32_t color = sSpriteLayer[x] & ~FLAG_TARGET_2;
        if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (int)((color & FLAG_PRIORITY) >> OFFSET_PRIORITY) == priority)
            compositeBlend(&sRow[x], color | flags);
    }
}

// --- a line ----------------------------------------------------------------------------------

static uint32_t backdrop(uint8_t control) {
    if (!sTarget1Bd || sEffect == BLEND_NONE || sEffect == BLEND_ALPHA || !(control & WIN_BLEND))
        return sNormalPalette[0];
    return sVariantPalette[0];
}

static void drawLine(int y, uint32_t *out) {
    latchRegisters();
    if (sDispcnt & 0x80) {  // forced blank: white
        for (int x = 0; x < 240; x++)
            out[x] = 0x00FFFFFF;
        return;
    }
    int mode = sDispcnt & 7;
    if (mode > 1)
        unsupported("BG modes 2-5");

    // The window segments, the backdrop
    for (int x = 0; x < 240; x++)
        sSpriteLayer[x] = FLAG_UNWRITTEN;
    sSegments[0].endX = 240;
    sNumSegments = 1;
    if (sDispcnt & 0xE000) {
        if (sDispcnt & 0x8000)
            unsupported("the OBJ window");
        sSegments[0].control = sWinout;
        if (sDispcnt & 0x4000)
            breakWindow(&sWin[1], y);
        if (sDispcnt & 0x2000)
            breakWindow(&sWin[0], y);
    } else {
        sSegments[0].control = 0xFF;
    }
    updateVariantPalette();
    sForceTarget1 = false;
    for (int w = 0, x = 0; w < sNumSegments; w++) {
        uint32_t color = FLAG_UNWRITTEN | FLAG_PRIORITY | FLAG_IS_BACKGROUND | backdrop(sSegments[w].control);
        for (; x < sSegments[w].endX; x++)
            sRow[x] = color;
    }

    int spriteLayers = drawSprites(y);

    // The layers, front to back, segment by segment
    sEnd = 0;
    for (int w = 0; w < sNumSegments; w++) {
        sStart = sEnd;
        sEnd = sSegments[w].endX;
        sCurrentWindow = sSegments[w].control;
        for (int n = 0; n < 4; n++)
            if (sBg[n].enabled)
                updateFlags(&sBg[n]);
        for (int priority = 0; priority < 4; priority++) {
            if (spriteLayers & (1 << priority))
                compositeSprites(priority);
            for (int n = 0; n < 4; n++) {
                struct Background *bg = &sBg[n];
                if (!bg->enabled || !(sCurrentWindow & WIN_BG(n)) || bg->priority != priority)
                    continue;
                if (n < 2 || (mode == 0 && n < 4))
                    drawTextBg(bg, y);
                else if (mode == 1 && n == 2)
                    drawAffineBg(bg);
            }
        }
    }

    // A first target with nothing drawn under it blends with the backdrop; then the
    // semi-transparent sprites with no second target under them get the brightness effect
    if ((sForceTarget1 || sBg[0].target1 || sBg[1].target1 || sBg[2].target1 || sBg[3].target1) && sTarget2Bd) {
        for (int w = 0, x = 0; w < sNumSegments; w++) {
            uint32_t color = backdrop(sSegments[w].control);
            for (; x < sSegments[w].endX; x++)
                if (sRow[x] & FLAG_TARGET_1)
                    sRow[x] = mix(sBldb, color, sBlda, sRow[x]);
        }
    }
    if (sForceTarget1 && (sEffect == BLEND_DARKEN || sEffect == BLEND_BRIGHTEN)) {
        for (int w = 0, x = 0; w < sNumSegments; w++) {
            int end = sSegments[w].endX;
            if (!(sSegments[w].control & WIN_BLEND)) {
                x = end;
                continue;
            }
            for (; x < end; x++)
                if ((sRow[x] & (FLAG_REBLEND | FLAG_IS_BACKGROUND)) == FLAG_REBLEND)
                    sRow[x] = sEffect == BLEND_DARKEN ? darken(sRow[x], sBldy) : brighten(sRow[x], sBldy);
        }
    }

    // The affine BGs' reference point moves by (PB, PD) every line
    if (mode != 0) {
        for (int n = 2; n < 4; n++) {
            if (sBg[n].enabled) {
                sBg[n].sx += sBg[n].dmx;
                sBg[n].sy += sBg[n].dmy;
            }
        }
    }
    memcpy(out, sRow, sizeof(sRow));
}

// --- the frame -------------------------------------------------------------------------------

#define REG_OFS_DISPSTAT 0x004
#define REG_OFS_VCOUNT 0x006

// A VCount match at *line*, if DISPSTAT asks for its interrupt.
static void vcountMatch(int line) {
    uint16_t dispstat = IO16(REG_OFS_DISPSTAT);
    if ((dispstat >> 8) == line && (dispstat & 0x20)) {
        IO16(REG_OFS_VCOUNT) = (uint16_t)line;
        Port_RaiseIrq(0x0004);
    }
}

void Port_DrawFrame(bool draw) {
    // The end of the last VBlank (lines 161-227): the VCount interrupt the game uses (line
    // 227, Bg_WindowLineIntr) restarts the window's HBlank DMA for this frame.
    for (int line = 161; line < 228; line++)
        vcountMatch(line);
    if (draw) {
        for (int i = 0; i < 512; i++)
            sNormalPalette[i] = colorFrom555(PLTT16[i]);
        sVariantKey = -1;
        loadSprites();
    }
    sBg[2].sx = refPoint(0x028), sBg[2].sy = refPoint(0x02C);
    sBg[3].sx = refPoint(0x038), sBg[3].sy = refPoint(0x03C);
    for (int y = 0; y < 160; y++) {
        IO16(REG_OFS_VCOUNT) = (uint16_t)y;
        vcountMatch(y);
        if (draw)
            drawLine(y, &gPortScreen[y * 240]);
        // HBlank: the HBlank DMA, the HBlank interrupt (never enabled by the game)
        Port_RunHblankDma();
        if (IO16(REG_OFS_DISPSTAT) & 0x10)
            Port_RaiseIrq(0x0002);
    }
    IO16(REG_OFS_VCOUNT) = 160;
}
