// No licence is granted for this file: it derives from pret's reconstruction of Nintendo SDK
// code, which carries no licence (NOTICE.md).
// The SDK's MultiBootCheckComplete: a leaf with a branch and a bare bx lr in the ROM,
// so not plain agbcc (see game/Util_StepTowards.c).

#include "gba/types.h"
#include "gba/multiboot.h"

int MultiBootCheckComplete(struct MultiBootParam *mp)
{
    if (mp->probe_count == 0xE9)
        return 1;
    return 0;
}
