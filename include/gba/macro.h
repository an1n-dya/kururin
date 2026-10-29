// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_GBA_MACRO_H
#define GUARD_GBA_MACRO_H

#include "gba/io_reg.h"

#ifdef PLATFORM_PC
// The PC port (docs/port.md): the DMA unit is port/src/hw.c, which writes the registers and
// does an immediate transfer at once.
void Port_Dma(u32 dmaNum, const volatile void *src, volatile void *dest, u32 control);
#define DmaSet(dmaNum, src, dest, control) Port_Dma(dmaNum, (const volatile void *)(src), (volatile void *)(dest), (u32)(control))
#else
// DMA transfers as the SDK writes them: source, destination, control, then a read-back of
// the control register (the trailing `ldr rX, [rY, #8]` in the asm).
#define DmaSet(dmaNum, src, dest, control)                  \
    {                                                       \
        vu32 *dmaRegs = (vu32 *)REG_ADDR_DMA##dmaNum;       \
        dmaRegs[0] = (vu32)(src);                           \
        dmaRegs[1] = (vu32)(dest);                          \
        dmaRegs[2] = (vu32)(control);                       \
        dmaRegs[2];                                         \
    }
#endif

#define DmaCopy16(dmaNum, src, dest, size) \
    DmaSet(dmaNum, src, dest, DMA_ENABLE | DMA_START_NOW | DMA_16BIT | DMA_SRC_INC | DMA_DEST_INC | ((size) / 2))
#define DmaCopy32(dmaNum, src, dest, size) \
    DmaSet(dmaNum, src, dest, DMA_ENABLE | DMA_START_NOW | DMA_32BIT | DMA_SRC_INC | DMA_DEST_INC | ((size) / 4))
// Fill size bytes at dest with the word value (a fixed source: a copy of value on the stack).
#define DmaFill32(dmaNum, value, dest, size)                                                  \
    {                                                                                         \
        vu32 tmp = (vu32)(value);                                                             \
        DmaSet(dmaNum, &tmp, dest,                                                            \
               DMA_ENABLE | DMA_START_NOW | DMA_32BIT | DMA_SRC_FIXED | DMA_DEST_INC | ((size) / 4)); \
    }

// Fill size bytes at dest with the halfword value (a fixed source: a copy of value on the stack).
#define DmaFill16(dmaNum, value, dest, size)                                                  \
    {                                                                                         \
        vu16 tmp = (vu16)(value);                                                             \
        DmaSet(dmaNum, &tmp, dest,                                                            \
               DMA_ENABLE | DMA_START_NOW | DMA_16BIT | DMA_SRC_FIXED | DMA_DEST_INC | ((size) / 2)); \
    }

#define CPU_SET_SRC_FIXED 0x01000000
#define CPU_SET_16BIT     0x00000000
#define CPU_SET_32BIT     0x04000000

void CpuSet(const void *src, void *dest, u32 control);

// BIOS CpuSet copies and fills of size bytes, as the SDK (and pret's gba/macro.h) write them
#define CpuFill16(value, dest, size)                                                    \
    {                                                                                   \
        vu16 tmp = (vu16)(value);                                                       \
        CpuSet((void *)&tmp, dest, CPU_SET_16BIT | CPU_SET_SRC_FIXED | ((size) / 2 & 0x1FFFFF)); \
    }
#define CpuFill32(value, dest, size)                                                    \
    {                                                                                   \
        vu32 tmp = (vu32)(value);                                                       \
        CpuSet((void *)&tmp, dest, CPU_SET_32BIT | CPU_SET_SRC_FIXED | ((size) / 4 & 0x1FFFFF)); \
    }
#define CpuCopy16(src, dest, size) CpuSet(src, dest, CPU_SET_16BIT | ((size) / 2 & 0x1FFFFF))
#define CpuCopy32(src, dest, size) CpuSet(src, dest, CPU_SET_32BIT | ((size) / 4 & 0x1FFFFF))

#endif // GUARD_GBA_MACRO_H
