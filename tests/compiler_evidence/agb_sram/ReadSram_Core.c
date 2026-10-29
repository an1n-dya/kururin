// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// The SDK's ReadSram_Core matches only at -O (pret builds agb_flash the same way); at
// -O2 the loop is rotated differently. Both compilers give the same code at -O, so the
// module's cc is not decided by this library.

#include "gba/types.h"

void ReadSram_Core(const u8 *src, u8 *dest, u32 size)
{
    while (--size != -1)
        *dest++ = *src++;
}
