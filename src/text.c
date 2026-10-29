// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// text: times, numbers and text drawn as sprites, text conversion.
// 08002848-08003C18, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "sprite.h"
#include "text.h"

extern const u16 gDakutenChars[];
extern const u16 gHandakutenChars[];
extern const u8 gTextNoTime[];
extern const u8 gFrameHundredths[60];
extern const u32 gPowersOf10[11];

u8 *Text_FormatTime(u8 *buf, u32 frames);
void Text_DrawDigits(u8 a, s32 x, u8 y, const u8 *str, u8 palette);
extern const u8 gSmallFontWidths[26];
extern const u16 gSmallRankMarks[][4];
extern const u8 gFontWidths[26];
extern const u16 gRankMarks[][4];

/**
 * Writes *frames* as the time string MM'SS"CC to *buf* (with a NUL) and returns *buf*:
 * minutes and seconds at 60 frames per second, CC = gFrameHundredths[frames % 60]. Each
 * tens digit counts up from '0', so minutes past 99 give characters beyond '9'.
 */
u8 *Text_FormatTime(u8 *buf, u32 frames)
{
    u8 *p = buf;
    u32 cs = gFrameHundredths[frames % 60];
    u32 sec = (frames / 60) % 60;
    u32 min = (frames / 60) / 60;
    u32 c;

    for (c = '0'; min > 9; min -= 10)
        c++;
    *p++ = c;
    *p++ = '0' + min;
    *p++ = '\'';
    for (c = '0'; sec > 9; sec -= 10)
        c++;
    *p++ = c;
    *p++ = '0' + sec;
    *p++ = '"';
    for (c = '0'; cs > 9; cs -= 10)
        c++;
    *p++ = c;
    *p = '0' + cs;
    p[1] = '\0';
    return buf;
}
/**
 * Text_Draw with 8x8 sprites: character c gets tile c - 0x20 (and palette *palette*), with
 * the same 0xDE/0xDF marks for characters 0x80-0xFF (the original tests 0xA0-0xDF and the
 * rest of 0x80-0xFF in two branches that do the same).
 */
void Text_DrawSmall(s32 x, u8 y, const u8 *str, u8 palette)
{
    u32 pos = (x << 16) & 0x1FF0000;
    u32 pal = (palette & 0xF) << 12;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;

    while ((ch = *str++) != '\0')
    {
        if (ch == ' ')
        {
            pos += 0x80000;
        }
        else
        {
            if (ch >= 0x80)
            {
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(1);
            if (e == NULL)
                return;
            e->attr01 = pos | y;
            e->attr2 = (ch - 0x20) | pal;
            pos += 0x80000;
        }
        pos &= 0x1FF0000;
    }
}
/**
 * Draws the low *digits* nibbles of *value* in hexadecimal (0-9, A-F) with Text_DrawSmall
 * (most significant first).
 */
void Text_DrawSmallHex(u16 x, u8 y, u32 value, u8 digits, u8 palette)
{
    u8 buf[12];
    u32 bits = value; // a copy, declared first: agbcc then allocates as the original does
    u8 *p = &buf[digits];
    s32 i;
    s32 d;

    *p-- = '\0';
    for (i = digits; i != 0; i--)
    {
        d = bits & 0xF;
        *p-- = (d > 9) ? d + 'A' - 10 : d + '0';
        bits >>= 4;
    }
    Text_DrawSmall(x, y, buf, palette);
}
/**
 * Draws the low *digits* bits of *value* in binary with Text_DrawSmall (most significant
 * first).
 */
void Text_DrawSmallBinary(u16 x, u8 y, u32 value, u8 digits, u8 palette)
{
    u8 buf[36];
    u8 *p = &buf[digits];
    s32 i;
    u32 bits = value; // a copy: agbcc then keeps value in r2, as the original does

    *p-- = '\0';
    for (i = digits; i != 0; i--)
    {
        *p-- = '0' + (bits & 1);
        bits >>= 1;
    }
    Text_DrawSmall(x, y, buf, palette);
}
/**
 * Draws *value* in decimal with Text_DrawSmall: a '-' first if it is negative, then its
 * digits without leading zeros.
 */
void Text_DrawSmallNumber(u16 x, u8 y, s16 value, u8 palette)
{
    u8 buf[8];
    u8 *start;
    s32 n = value;
    s32 c;
    u8 *p = buf;

    if (n < 0)
    {
        *p++ = '-';
        n = -n;
    }
    start = p;
    for (c = '0'; n > 9999; n -= 10000)
        c++;
    if (p != start || c != '0')
        *p++ = c;
    for (c = '0'; n > 999; n -= 1000)
        c++;
    if (p != start || c != '0')
        *p++ = c;
    for (c = '0'; n > 99; n -= 100)
        c++;
    if (p != start || c != '0')
        *p++ = c;
    for (c = '0'; n > 9; n -= 10)
        c++;
    if (p != start || c != '0')
        *p++ = c;
    p[0] = '0' + n;
    p[1] = '\0';
    Text_DrawSmall(x, y, buf, palette);
}
/**
 * Draws *value* with Text_DrawSmall as five right-aligned decimal digits: the leading zeros
 * become spaces (as Text_DrawSmallNumber, without the sign).
 */
void Text_DrawSmallNumber5(u16 x, u8 y, u16 value, u8 palette)
{
    u8 buf[8];
    u32 n = value;
    u8 *p = buf;
    s32 pad = ' ';
    s32 c;

    for (c = '0'; n > 9999; n -= 10000)
        c++;
    if (c != '0')
    {
        *p++ = c;
        pad = '0';
    }
    else
        *p++ = pad;
    for (c = '0'; n > 999; n -= 1000)
        c++;
    if (c != '0')
    {
        *p++ = c;
        pad = '0';
    }
    else
        *p++ = pad;
    for (c = '0'; n > 99; n -= 100)
        c++;
    if (c != '0')
    {
        *p++ = c;
        pad = '0';
    }
    else
        *p++ = pad;
    for (c = '0'; n > 9; n -= 10)
        c++;
    // `**&p` is `*p`; taking p's address stops gcse's PRE from copying p + 1 into a register
    // before the tens digit's loop, so p is incremented after the `if` as in the original.
    if (c != '0')
        *p = c;
    else
        **&p = pad;
    p++;
    p[0] = '0' + n;
    p[1] = '\0';
    Text_DrawSmall(x, y, buf, palette);
}
/**
 * Draws *value* with Text_DrawSmall as three right-aligned decimal digits: the leading zeros
 * become spaces (Text_DrawSmallNumber5 for a byte).
 */
void Text_DrawSmallNumber3(u16 x, u8 y, u8 value, u8 palette)
{
    u8 buf[4];
    u32 n = value;
    u8 *p = buf;
    s32 pad = ' ';
    s32 c;

    for (c = '0'; n > 99; n -= 100)
        c++;
    if (c != '0')
    {
        *p++ = c;
        pad = '0';
    }
    else
        *p++ = pad;
    for (c = '0'; n > 9; n -= 10)
        c++;
    // As in Text_DrawSmallNumber5: `**&p` is `*p`, and taking p's address stops gcse's PRE.
    if (c != '0')
        *p = c;
    else
        **&p = pad;
    p++;
    p[0] = '0' + n;
    p[1] = '\0';
    Text_DrawSmall(x, y, buf, palette);
}
/**
 * Text_DrawNumberAligned drawn with Text_DrawSmall: *value* in decimal, right-aligned in
 * *digits* characters, leading zeros as spaces (the last digit is always drawn).
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
void Text_DrawSmallNumberAligned(s32 x, s32 y, u32 value, s32 digits, u8 palette)
{
    u8 buf[12];
    u8 *p = buf;
    u32 pad = ' ';

    while (digits > 1)
    {
        u32 pow = gPowersOf10[digits];
        u32 c = '0';

        digits--;
        while (value >= pow)
        {
            c++;
            value -= pow;
        }
        if (c != '0')
        {
            *p++ = c;
            pad = '0';
        }
        else
            *p++ = pad;
    }
    *p = value + '0';
    p[1] = '\0';
    Text_DrawSmall((u16)x, y, buf, palette);
}
/**
 * Text_DrawTime drawn with Text_DrawSmall instead of Text_Draw: a frame count as a time
 * (Text_FormatTime), or gTextNoTime for exactly 216000 frames (60 minutes).
 */
void Text_DrawSmallTime(u16 x, u8 y, u32 frames, u8 palette)
{
    u8 buf[12];

    if (frames == 216000)
        Text_DrawSmall(x, y, gTextNoTime, palette);
    else
        Text_DrawSmall(x, y, Text_FormatTime(buf, frames), palette);
}
/**
 * Draws a frame count as a time with Text_Draw, formatted by Text_FormatTime; exactly
 * 216000 frames (60 minutes) draws the fixed string gTextNoTime instead (as Text_DrawDigitTime).
 */
void Text_DrawTime(u16 x, u8 y, u32 frames, u8 palette)
{
    u8 buf[12];

    if (frames == 216000)
        Text_Draw(x, y, gTextNoTime, palette);
    else
        Text_Draw(x, y, Text_FormatTime(buf, frames), palette);
}
/**
 * Draws *str* one 8-pixel cell per character from pixel (*x*, *y*) with Sprite_Alloc(1):
 * character c gets tile 0xE0 + 2 * c and palette *palette*, a space leaves a gap. A
 * character 0x80-0xFF followed by 0xDE or 0xDF becomes gDakutenChars[c - 0x80] or
 * gHandakutenChars[c - 0x80], and the mark is skipped, unless the table gives c itself.
 * Stops early when Sprite_Alloc runs out.
 */
void Text_Draw(s32 x, u8 y, const u8 *str, u8 palette)
{
    u32 pos = (x << 16) & 0x1FF0000;
    u32 pal = (palette & 0xF) << 12;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;

    while ((ch = *str++) != '\0')
    {
        if (ch == ' ')
        {
            pos += 0x80000;
        }
        else
        {
            if (ch >= 0x80)
            {
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(1);
            if (e == NULL)
                return;
            e->attr01 = 0x8000 | y | pos;
            e->attr2 = (ch * 2 + 0xE0) | pal;
            pos += 0x80000;
        }
        pos &= 0x1FF0000;
    }
}
/**
 * Draws *value* in decimal at (x, y) with Text_Draw: a '-' when negative, no leading
 * zeros.
 */
void Text_DrawNumber(u16 x, u8 y, s16 value, u8 palette)
{
    u8 buf[8];
    s32 n = value;
    u8 *p = buf;
    u8 *digits;
    s32 digit;

    if (n < 0)
    {
        *p++ = '-';
        n = -n;
    }
    digits = p;
    digit = '0';
    while (n > 9999)
    {
        digit++;
        n -= 10000;
    }
    if (p != digits || digit != '0')
        *p++ = digit;
    digit = '0';
    while (n > 999)
    {
        digit++;
        n -= 1000;
    }
    if (p != digits || digit != '0')
        *p++ = digit;
    digit = '0';
    while (n > 99)
    {
        digit++;
        n -= 100;
    }
    if (p != digits || digit != '0')
        *p++ = digit;
    digit = '0';
    while (n > 9)
    {
        digit++;
        n -= 10;
    }
    if (p != digits || digit != '0')
        *p++ = digit;
    *p++ = n + '0';
    *p = '\0';
    Text_Draw(x, y, buf, palette);
}
/**
 * Draws *value* in decimal with Text_Draw, right-aligned in *digits* characters: leading
 * zeros are spaces (the last digit is always drawn). gPowersOf10[n] is 10^(n-1).
 */
void Text_DrawNumberAligned(s32 x, s32 y, u32 value, s32 digits, u8 palette)
{
    u8 buf[12];
    u8 *p = buf;
    u32 pad = ' ';

    while (digits > 1)
    {
        u32 pow = gPowersOf10[digits];
        u32 c = '0';

        digits--;
        while (value >= pow)
        {
            c++;
            value -= pow;
        }
        if (c != '0')
        {
            *p++ = c;
            pad = '0';
        }
        else
            *p++ = pad;
    }
    *p = value + '0';
    p[1] = '\0';
    Text_Draw((u16)x, y, buf, palette);
}
/**
 * Draws *str* as 8x8 sprites (tile c - 0x20, as Text_DrawSmall) in list p->list & 0xFF from
 * the cursor *p*, which steps by dx per character and moves back to its start x and down by
 * dy on '\n'; the cursor is left after the text. Bits 2-3 of p->list are the sprites'
 * priority, p->palette their palette. '#' starts an escape: a decimal number, then 'h' (from
 * now on 0xA0-0xBF become 0x80-0x9F and 0xC0-0xDF become 0xE0-0xFF), 'k' (no longer), or
 * 'p' (the number is the new p->palette); any other character ends it with no effect. A
 * character 0x80-0xFF followed by 0xDE or 0xDF is combined as in Text_DrawSmall. A full sprite
 * list only skips the character.
 */
void Text_PrintSmall(struct TextCursor *p, const u8 *str)
{
    u32 pal = (p->palette & 0xF) << 12;
    u32 prio = (p->list & 0xC) << 8;
    u32 shift = FALSE;
    u32 lineStart = p->x << 16;
    u32 pos = lineStart;
    u32 y = p->y;
    u32 dx = (p->dx << 16) & 0x1FF0000;
    u32 dy = p->dy;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;
    u32 n;

    while ((ch = *str++) != '\0')
    {
        if (ch == '\n')
        {
            pos = lineStart;
            y += dy;
            continue;
        }
        if (ch == '#')
        {
            n = 0;
            while ((ch = *str++) >= '0' && ch <= '9')
                n = n * 10 + ch - '0';
            if (ch == 'h')
                shift = TRUE;
            else if (ch == 'k')
                shift = FALSE;
            else if (ch == 'p')
            {
                p->palette = n;
                pal = (n & 0xF) << 12;
            }
            continue;
        }
        if (ch != ' ')
        {
            if (ch >= 0x80)
            {
                if (ch - 0xA0 <= 0xDF - 0xA0 && shift)
                {
                    if (ch <= 0xBF)
                        ch -= 0x20;
                    else
                        ch += 0x20;
                }
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(p->list);
            if (e != NULL)
            {
                e->attr01 = pos | y;
                e->attr2 = pal | prio | (ch - 0x20);
            }
        }
        pos = (pos + dx) & 0x1FF0000;
    }
    p->x = pos >> 16;
    p->y = y;
}
/**
 * Text_PrintSmall with Text_Draw's 8x16 sprites (tile 0xE0 + 2 * c): the same cursor,
 * escapes and character combination.
 */
void Text_Print(struct TextCursor *p, const u8 *str)
{
    u32 pal = (p->palette & 0xF) << 12;
    u32 prio = (p->list & 0xC) << 8;
    u32 shift = FALSE;
    u32 lineStart = p->x << 16;
    u32 pos = lineStart;
    u32 y = p->y;
    u32 dx = (p->dx << 16) & 0x1FF0000;
    u32 dy = p->dy;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;
    u32 n;

    while ((ch = *str++) != '\0')
    {
        if (ch == '\n')
        {
            pos = lineStart;
            y += dy;
            continue;
        }
        if (ch == '#')
        {
            n = 0;
            while ((ch = *str++) >= '0' && ch <= '9')
                n = n * 10 + ch - '0';
            if (ch == 'h')
                shift = TRUE;
            else if (ch == 'k')
                shift = FALSE;
            else if (ch == 'p')
            {
                p->palette = n;
                pal = (n & 0xF) << 12;
            }
            continue;
        }
        if (ch != ' ')
        {
            if (ch >= 0x80)
            {
                if (ch - 0xA0 <= 0xDF - 0xA0 && shift)
                {
                    if (ch <= 0xBF)
                        ch -= 0x20;
                    else
                        ch += 0x20;
                }
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(p->list);
            if (e != NULL)
            {
                e->attr01 = 0x8000 | y | pos;
                e->attr2 = pal | prio | (ch * 2 + 0xE0);
            }
        }
        pos = (pos + dx) & 0x1FF0000;
    }
    p->x = pos >> 16;
    p->y = y;
}
/**
 * Draws a frame count as a time with Text_DrawDigits, formatted by Text_FormatTime; exactly
 * 216000 frames (60 minutes) draws the fixed string gTextNoTime instead.
 */
void Text_DrawDigitTime(u8 a, u16 x, u8 y, u32 frames, u8 palette)
{
    u8 buf[12];

    if (frames == 216000)
        Text_DrawDigits(a, x, y, gTextNoTime, palette);
    else
        Text_DrawDigits(a, x, y, Text_FormatTime(buf, frames), palette);
}
/**
 * Draws *str* one 8-pixel cell per character from pixel (*x*, *y*): each digit, ' " and -
 * gets an entry from Sprite_Alloc(*a*) with tile 0x40 + 2 * digit, 0x54, 0x56 or 0x58 and
 * palette *palette*; other characters leave a gap. Stops early when Sprite_Alloc runs out.
 */
void Text_DrawDigits(u8 a, s32 x, u8 y, const u8 *str, u8 palette)
{
    u32 pos = (x << 16) & 0x1FF0000;
    u32 pal = (palette & 0xF) << 12;
    u32 attr0 = 0x8000 | y;
    struct Sprite *e;
    u32 ch;
    u32 tile;

    while ((ch = *str++) != '\0')
    {
        if (ch - '0' <= 9)
            tile = 0x40 + (ch - '0') * 2;
        else if (ch == '\'')
            tile = 0x54;
        else if (ch == '"')
            tile = 0x56;
        else if (ch == '-')
            tile = 0x58;
        else
            tile = 0;
        if (tile != 0)
        {
            e = Sprite_Alloc(a);
            if (e == NULL)
                return;
            e->attr01 = pos | attr0;
            e->attr2 = tile | pal;
        }
        pos = (pos + 0x80000) & 0x1FF0000;
    }
}

/**
 * Looks up gDakutenChars for a code in 0x80-0xFF.
 */
u16 Text_GetDakutenChar(u16 code)
{
    return gDakutenChars[code - 0x80];
}

/**
 * Looks up gHandakutenChars for a code in 0x80-0xFF; the twin of Text_GetDakutenChar.
 */
u16 Text_GetHandakutenChar(u16 code)
{
    return gHandakutenChars[code - 0x80];
}

/**
 * Copies the string *str* to *dst* as halfword characters, ended by a 0, and returns how
 * many it wrote. "#h" and "#k" switch to hiragana and back (not copied): in hiragana, a
 * character 0xA0-0xBF is lowered by 0x20 and 0xC0-0xDF raised by 0x20. A character
 * 0x80-0xFF followed by 0xDE or 0xDF becomes gDakutenChars[c - 0x80] or
 * gHandakutenChars[c - 0x80], and the mark is skipped, unless the table gives c itself (the
 * original tests 0xA0-0xDF and the rest of 0x80-0xFF in two branches that do the same).
 */
s32 Text_ConvertToU16(const u8 *str, u16 *dst)
{
    s32 n = 0;
    s32 hiragana = 0;
    u32 combined;
    u32 ch;

    while ((ch = *str++) != '\0')
    {
        if (ch != '#')
        {
            if (ch - 0xA0 <= 0x3F)
            {
                if (hiragana)
                {
                    if (ch <= 0xBF)
                        ch -= 0x20;
                    else
                        ch += 0x20;
                }
                switch (*str)
                {
                case 0xDE:
                    combined = gDakutenChars[ch - 0x80];
                    goto check;
                case 0xDF:
                    combined = gHandakutenChars[ch - 0x80];
                    goto check;
                }
            }
            else if (ch - 0x80 <= 0x7F)
            {
                switch (*str)
                {
                case 0xDE:
                    combined = gDakutenChars[ch - 0x80];
                    goto check;
                case 0xDF:
                    combined = gHandakutenChars[ch - 0x80];
                check:
                    if (ch != combined)
                    {
                        ch = combined;
                        str++;
                    }
                    break;
                }
            }
            *dst++ = ch;
            n++;
        }
        else
        {
            switch (*str++)
            {
            case 'h':
                hiragana = 1;
                break;
            case 'k':
                hiragana = 0;
                break;
            }
        }
    }
    *dst = 0;
    return n;
}
/**
 * Copies the text *src* to *dst* (without its terminator) and returns the number of bytes
 * written: "#h" and "#k" are not copied, and after "#h" (until "#k") the bytes 0xA6-0xBF
 * move down and 0xC0-0xDD up by 0x20. The byte after a '#' is always skipped, even the
 * terminator.
 */
s32 Text_Convert(const u8 *src, u8 *dst)
{
    u32 c;
    s32 shift;
    s32 n = 0;

    shift = 0;
    while ((c = *src++) != 0)
    {
        if (c != '#')
        {
            if (shift && c - 0xA6 <= 0x37)
            {
                if (c <= 0xBF)
                    c -= 0x20;
                else
                    c += 0x20;
            }
            *dst++ = c;
            n++;
            continue;
        }
        c = *src++;
        switch (c)
        {
        case 'h':
            shift = 1;
            break;
        case 'k':
            shift = 0;
            break;
        }
    }
    return n;
}
/**
 * Text_DrawSmall with proportional lower case: 'a'-'z' advance by gSmallFontWidths[c - 'a']
 * pixels, every other character by 8. After the string it draws the up to three tiles of
 * gSmallRankMarks[*suffix*] (0-terminated) 8 pixels apart.
 */
void Text_DrawSmallName(s32 x, u8 y, const u8 *str, u8 suffix, u8 palette)
{
    u32 pos = (x << 16) & 0x1FF0000;
    u16 pal = (palette & 0xF) << 12;
    struct Sprite *e;
    const u16 *combined;
    const u16 *s;
    u32 ch;

    while ((ch = *str++) != '\0')
    {
        if (ch == ' ')
        {
            pos += 0x80000;
        }
        else
        {
            if (ch >= 0x80)
            {
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(1);
            if (e == NULL)
                return;
            e->attr01 = pos | y;
            e->attr2 = (ch - 0x20) | pal;
            if (ch - 'a' <= 'z' - 'a')
                pos += gSmallFontWidths[ch - 'a'] << 16;
            else
                pos += 0x80000;
        }
        pos &= 0x1FF0000;
    }
    for (s = gSmallRankMarks[suffix]; (ch = *s++) != 0;)
    {
        e = Sprite_Alloc(1);
        if (e == NULL)
            return;
        e->attr01 = pos | y;
        e->attr2 = (ch - 0x20) | pal;
        pos = (pos + 0x80000) & 0x1FF0000;
    }
}
/**
 * Text_Draw with proportional lower case: 'a'-'z' advance by gFontWidths[c - 'a']
 * pixels, every other character by 8. After the string it draws the tiles of
 * gRankMarks[*suffix*] (0-terminated, as characters) 8 pixels apart.
 */
void Text_DrawName(s32 x, u8 y, const u8 *str, u8 suffix, u8 palette)
{
    u32 pos = (x << 16) & 0x1FF0000;
    u16 pal = (palette & 0xF) << 12;
    struct Sprite *e;
    const u16 *combined;
    const u16 *s;
    u32 ch;

    while ((ch = *str++) != '\0')
    {
        if (ch == ' ')
        {
            pos += 0x80000;
        }
        else
        {
            if (ch >= 0x80)
            {
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(1);
            if (e == NULL)
                return;
            e->attr01 = 0x8000 | y | pos;
            e->attr2 = (ch * 2 + 0xE0) | pal;
            if (ch - 'a' <= 'z' - 'a')
                pos += gFontWidths[ch - 'a'] << 16;
            else
                pos += 0x80000;
        }
        pos &= 0x1FF0000;
    }
    for (s = gRankMarks[suffix]; (ch = *s++) != 0;)
    {
        e = Sprite_Alloc(1);
        if (e == NULL)
            return;
        e->attr01 = pos | (0x8000 | y);
        e->attr2 = (ch * 2 + 0xE0) | pal;
        pos = (pos + 0x80000) & 0x1FF0000;
    }
}
/**
 * Text_DrawSmall with proportional lower case: Text_DrawSmallName without the rank mark.
 * 'a'-'z' advance by gSmallFontWidths[c - 'a'] pixels, every other character by 8; a
 * character 0x80-0xFF followed by 0xDE or 0xDF is combined with that mark
 * (gDakutenChars / gHandakutenChars) when the table has another character for it. Nothing
 * calls it.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
void Text_DrawSmallProportional(s32 x, u8 y, const u8 *str, u8 palette)
{
    u32 pos = (x << 16) & 0x1FF0000;
    u16 pal = (palette & 0xF) << 12;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;

    while ((ch = *str++) != '\0')
    {
        if (ch == ' ')
        {
            pos += 0x80000;
        }
        else
        {
            if (ch >= 0x80)
            {
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(1);
            if (e == NULL)
                return;
            e->attr01 = pos | y;
            e->attr2 = (ch - 0x20) | pal;
            if (ch - 'a' <= 'z' - 'a')
                pos += gSmallFontWidths[ch - 'a'] << 16;
            else
                pos += 0x80000;
        }
        pos &= 0x1FF0000;
    }
}
/**
 * Text_Draw with proportional lower case: Text_DrawName without the rank mark (8x16 cells,
 * tiles c * 2 + 0xE0). 'a'-'z' advance by gFontWidths[c - 'a'] pixels, every other
 * character by 8; a character 0x80-0xFF followed by 0xDE or 0xDF is combined with that mark
 * (gDakutenChars / gHandakutenChars) when the table has another character for it. Nothing
 * calls it.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
void Text_DrawProportional(s32 x, u8 y, const u8 *str, u8 palette)
{
    u32 pos = (x << 16) & 0x1FF0000;
    u16 pal = (palette & 0xF) << 12;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;

    while ((ch = *str++) != '\0')
    {
        if (ch == ' ')
        {
            pos += 0x80000;
        }
        else
        {
            if (ch >= 0x80)
            {
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(1);
            if (e == NULL)
                return;
            e->attr01 = 0x8000 | y | pos;
            e->attr2 = (ch * 2 + 0xE0) | pal;
            if (ch - 'a' <= 'z' - 'a')
                pos += gFontWidths[ch - 'a'] << 16;
            else
                pos += 0x80000;
        }
        pos &= 0x1FF0000;
    }
}
/**
 * Text_PrintProportional with 8x8 characters: character c is tile c - 0x20 (as Text_DrawSmall), drawn
 * without Text_PrintProportional's 0x8000 in the attribute word; the same escapes, "\n" and
 * marks. Lower case advances by gFontWidths, the 8x16 font's widths, as in the original (not
 * gSmallFontWidths).
 */
void Text_PrintSmallProportional(struct TextCursor *p, const u8 *str)
{
    u32 pal = (p->palette & 0xF) << 12;
    u32 prio = (p->list & 0xC) << 8;
    u32 shift = FALSE;
    u32 lineStart = p->x << 16;
    u32 pos = lineStart;
    u32 y = p->y;
    u32 dx = (p->dx << 16) & 0x1FF0000;
    u32 dy = p->dy;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;
    u32 n;

    while ((ch = *str++) != '\0')
    {
        if (ch == '\n')
        {
            pos = lineStart;
            y += dy;
            continue;
        }
        if (ch == '#')
        {
            n = 0;
            while ((ch = *str++) >= '0' && ch <= '9')
                n = n * 10 + ch - '0';
            if (ch == 'h')
                shift = TRUE;
            else if (ch == 'k')
                shift = FALSE;
            else if (ch == 'p')
            {
                p->palette = n;
                pal = (n & 0xF) << 12;
            }
            continue;
        }
        if (ch != ' ')
        {
            if (ch >= 0x80)
            {
                if (ch - 0xA0 <= 0xDF - 0xA0 && shift)
                {
                    if (ch <= 0xBF)
                        ch -= 0x20;
                    else
                        ch += 0x20;
                }
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(p->list);
            if (e != NULL)
            {
                e->attr01 = pos | y;
                e->attr2 = pal | prio | (ch - 0x20);
            }
        }
        if (ch - 'a' <= 'z' - 'a')
            pos += gFontWidths[ch - 'a'] << 16;
        else
            pos += dx;
        pos &= 0x1FF0000;
    }
    p->x = pos >> 16;
    p->y = y;
}
/**
 * Text_Print with proportional lower case: 'a'-'z' advance by gFontWidths[c - 'a']
 * (as Text_DrawName), every other character by dx.
 */
void Text_PrintProportional(struct TextCursor *p, const u8 *str)
{
    u32 pal = (p->palette & 0xF) << 12;
    u32 prio = (p->list & 0xC) << 8;
    u32 shift = FALSE;
    u32 lineStart = p->x << 16;
    u32 pos = lineStart;
    u32 y = p->y;
    u32 dx = (p->dx << 16) & 0x1FF0000;
    u32 dy = p->dy;
    struct Sprite *e;
    const u16 *combined;
    u32 ch;
    u32 n;

    while ((ch = *str++) != '\0')
    {
        if (ch == '\n')
        {
            pos = lineStart;
            y += dy;
            continue;
        }
        if (ch == '#')
        {
            n = 0;
            while ((ch = *str++) >= '0' && ch <= '9')
                n = n * 10 + ch - '0';
            if (ch == 'h')
                shift = TRUE;
            else if (ch == 'k')
                shift = FALSE;
            else if (ch == 'p')
            {
                p->palette = n;
                pal = (n & 0xF) << 12;
            }
            continue;
        }
        if (ch != ' ')
        {
            if (ch >= 0x80)
            {
                if (ch - 0xA0 <= 0xDF - 0xA0 && shift)
                {
                    if (ch <= 0xBF)
                        ch -= 0x20;
                    else
                        ch += 0x20;
                }
                combined = NULL;
                if (*str == 0xDE)
                    combined = &gDakutenChars[ch - 0x80];
                else if (*str == 0xDF)
                    combined = &gHandakutenChars[ch - 0x80];
                if (combined != NULL && *combined != ch)
                {
                    ch = *combined;
                    str++;
                }
            }
            e = Sprite_Alloc(p->list);
            if (e != NULL)
            {
                e->attr01 = 0x8000 | y | pos;
                e->attr2 = pal | prio | (ch * 2 + 0xE0);
            }
        }
        if (ch - 'a' <= 'z' - 'a')
            pos += gFontWidths[ch - 'a'] << 16;
        else
            pos += dx;
        pos &= 0x1FF0000;
    }
    p->x = pos >> 16;
    p->y = y;
}
