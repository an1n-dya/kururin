// The SDK's two [[asm_regions]], which the port replaces with more than the weak empty stubs of
// port_tables.c (m4a_1.s is port/src/m4a_1.c).
#include "port.h"

// multiboot.c's MultiBootWaitCycles (inline asm): a delay loop, nothing to wait for here.
void MultiBootWaitCycles(uint32_t cycles) {
    (void)cycles;
}

// m4a.c's MusicPlayerJumpTableCopy (SWI 0x2A): copies the BIOS's music player jump table.
// The game never calls it (docs/coverage.md); the port has no BIOS to copy from.
void MusicPlayerJumpTableCopy(void) {
}
