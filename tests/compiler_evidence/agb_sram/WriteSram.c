// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// The SDK's WriteSram matches only at -O: at -O2 the WAITCNT read-modify-write uses
// r0/r1 the other way round (99.67%).

#include "gba/types.h"
#include "gba/io_reg.h"

void WriteSram(const u8 *src, u8 *dest, u32 size)
{
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    while (--size != -1)
        *dest++ = *src++;
}
