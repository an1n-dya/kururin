// The SDK's SRAM library (src/agb_sram.c) for the PC port: the same five functions on the SRAM
// region at 0x0E000000. The original's ReadSram and VerifySram copy their _Core loop to a
// buffer on the stack and call it there (Thumb code, the address's bit 0 set): a host cannot
// run that, so here they call the loop directly. src/agb_sram.c is not part of the port.
#include "port.h"

#define REG_WAITCNT IO16(0x204)
#define WAITCNT_SRAM_MASK 3
#define WAITCNT_SRAM_8 3

void ReadSram_Core(const uint8_t *src, uint8_t *dest, uint32_t size) {
    while (--size != (uint32_t)-1)
        *dest++ = *src++;
}

void ReadSram(const uint8_t *src, uint8_t *dest, uint32_t size) {
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    ReadSram_Core(src, dest, size);
}

void WriteSram(const uint8_t *src, uint8_t *dest, uint32_t size) {
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    while (--size != (uint32_t)-1)
        *dest++ = *src++;
}

// The address of the first byte that differs (in dest), or 0.
uint32_t VerifySram_Core(const uint8_t *src, uint8_t *dest, uint32_t size) {
    while (--size != (uint32_t)-1) {
        if (*dest++ != *src++)
            return (uint32_t)(uintptr_t)(dest - 1);
    }
    return 0;
}

uint32_t VerifySram(const uint8_t *src, uint8_t *dest, uint32_t size) {
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    return VerifySram_Core(src, dest, size);
}
