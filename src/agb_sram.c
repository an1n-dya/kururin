// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// agb_sram: the Nintendo SDK SRAM library (AGB_SRAM_V111), as it is in other SDK games.
// Library code: every function byte-matches the original (CLAUDE.md).
//
// ReadSram and VerifySram copy their _Core routine into a buffer on the stack (IWRAM) and
// call it there; WriteSram runs from ROM.

#include "global.h"

// The library's version string: the first of its .rodata, which the linker places at
// 0x080C2798 ([[rodata]] in kururin.toml); nothing reads it.
static const char sSramLibVersion[] = "SRAM_V111";

#define READ_SRAM_CORE_BUFFER_SIZE 0x40 // halfwords
#define VERIFY_SRAM_CORE_BUFFER_SIZE 0x60 // halfwords

void ReadSram_Core(const u8 *src, u8 *dest, u32 size)
{
    while (--size != -1)
        *dest++ = *src++;
}

void ReadSram(const u8 *src, u8 *dest, u32 size)
{
    const u16 *s;
    u16 *d;
    u16 readSramCore_Work[READ_SRAM_CORE_BUFFER_SIZE];
    u16 size_;
    void (*readSramFast_Work)(const u8 *src, u8 *dest, u32 size);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    s = (u16 *)ReadSram_Core;
    s = (u16 *)((u32)s ^ 1);
    d = readSramCore_Work;
    size_ = ((u32)ReadSram - (u32)ReadSram_Core) >> 1;
    while (size_ != 0)
    {
        *d++ = *s++;
        size_--;
    }

    readSramFast_Work = (void *)((u32)readSramCore_Work + 1);
    readSramFast_Work(src, dest, size);
}

void WriteSram(const u8 *src, u8 *dest, u32 size)
{
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    while (--size != -1)
        *dest++ = *src++;
}

u32 VerifySram_Core(const u8 *src, u8 *dest, u32 size)
{
    while (--size != -1)
    {
        if (*dest++ != *src++)
            return (u32)(dest - 1);
    }
    return 0;
}

u32 VerifySram(const u8 *src, u8 *dest, u32 size)
{
    const u16 *s;
    u16 *d;
    u16 verifySramCore_Work[VERIFY_SRAM_CORE_BUFFER_SIZE];
    u16 size_;
    u32 (*verifySramFast_Work)(const u8 *src, u8 *dest, u32 size);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    s = (u16 *)VerifySram_Core;
    s = (u16 *)((u32)s ^ 1);
    d = verifySramCore_Work;
    size_ = ((u32)VerifySram - (u32)VerifySram_Core) >> 1;
    while (size_ != 0)
    {
        *d++ = *s++;
        size_--;
    }

    verifySramFast_Work = (void *)((u32)verifySramCore_Work + 1);
    return verifySramFast_Work(src, dest, size);
}
