// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// decompress: the run-length and LZ decompressors.
// 08004030-080040E0, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "decompress.h"

/**
 * Decompresses *size* bytes from *src* to *dst*, run-length coded: a byte 0-0x7F is
 * followed by that many + 1 literal bytes; a byte 0x80 + n by one byte to repeat n + 3
 * times. Returns *size* if exactly that many came out, else 0 (Decompress_LZ's format with
 * runs instead of back references).
 */
s32 Decompress_RLE(const u8 *src, u8 *dst, s32 size)
{
    s32 total = size;
    s32 n;

    while (size > 0)
    {
        n = (s8)*src++;
        if (n >= 0)
        {
            n++;
            size -= n;
            do
                *dst++ = *src++;
            while (--n != 0);
        }
        else
        {
            n &= 0x7F;
            n += 3;
            size -= n;
            do
                *dst++ = *src;
            while (--n != 0);
            src++;
        }
    }
    if (size != 0)
        return 0;
    return total;
}
/**
 * Decompresses *size* bytes from *src* to *dst*: the LZ format of KuruTools
 * (KuruRomExtractor/LzCompression.cs). A byte 0-0x7F is followed by that many + 1 literal
 * bytes; a byte 0x80 + n copies n + 3 bytes from the next byte's distance back in *dst*.
 * Returns *size* when the data ends exactly there, else 0.
 */
s32 Decompress_LZ(const u8 *src, u8 *dst, s32 size)
{
    const u8 *from;
    s32 total = size;
    s32 mask = 0x7F; // a variable: agbcc then keeps 0x7F in a register for the whole loop
    s32 n;

    while (size > 0)
    {
        n = (s8)*src++;
        if (n >= 0)
        {
            n++;
            size -= n;
            do
                *dst++ = *src++;
            while (--n != 0);
        }
        else
        {
            n &= mask;
            n += 3;
            size -= n;
            from = dst - *src++;
            do
                *dst++ = *from++;
            while (--n != 0);
        }
    }
    if (size != 0)
        return 0;
    return total;
}
