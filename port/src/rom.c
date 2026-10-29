// Loads the player's baserom.gba at 0x08000000 and patches the ROM's code pointers to host
// functions (docs/port.md, "Function pointers in ROM data: patched once at load").
#include "port.h"
#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The ROM's table of worlds (an absolute symbol, kururin.toml): 14 x {info, data, tileAnims}
extern uint32_t gWorldEntries[14 * 3];

struct HostFunc {
    uint32_t host, gba;
};
static struct HostFunc *sByHost;
static int sNumByHost;

static int cmpHost(const void *a, const void *b) {
    uint32_t x = ((const struct HostFunc *)a)->host, y = ((const struct HostFunc *)b)->host;
    return x < y ? -1 : x > y;
}

void Port_LoadRom(const char *path) {
    uint8_t *rom = (uint8_t *)(uintptr_t)ROM_START;
    FILE *f = fopen(path, "rb");
    if (!f)
        Port_Die("cannot open %s: put your own dump of Kuru Kuru Kururin (Europe) there, named baserom.gba (see README)", path);
    size_t n = fread(rom, 1, ROM_SIZE + 1, f);
    fclose(f);
    char sha1[41];
    Sha1Hex(rom, n, sha1);
    if (n != ROM_SIZE || strcmp(sha1, BASEROM_SHA1))
        Port_Die("%s is not the expected ROM (SHA1 %s, %zu bytes; want %s, %u bytes)", path, sha1, n, BASEROM_SHA1,
                 ROM_SIZE);

    for (int i = 0; i < gPortNumRomPatches; i++) {
        const struct PortRomPatch *p = &gPortRomPatches[i];
        uint32_t *word = (uint32_t *)(uintptr_t)p->location;
        if (*word != p->value)
            Port_Die("ROM word 0x%08X is 0x%08X, config/data_pointers.txt says 0x%08X", p->location, *word, p->value);
        *word = (uint32_t)(uintptr_t)p->func;
    }

    // The debug stage select's world TEST (gWorldEntries[13], docs/unused_content.md) is all
    // zero: the original takes its world info from address 0, the BIOS, whose reads give the
    // BIOS's last fetched opcode; a PC cannot map that page (mmap_min_addr) and would crash. The
    // port points the entry at an empty world instead: every offset and size 0, so nothing is
    // decompressed and every layer is cleared (docs/port.md, "Risks").
    static const uint32_t emptyWorld[(0x80 + 256 * 0x20) / 4];  // a WorldInfo with 256 levels
    uint32_t *world13 = &gWorldEntries[13 * 3];
    if (world13[0] == 0 && world13[1] == 0)  // (the ROM's SHA1 says so; checked anyway)
        world13[0] = world13[1] = (uint32_t)(uintptr_t)emptyWorld;

    if (!(sByHost = calloc((size_t)gPortNumFunctions, sizeof(*sByHost))))
        Port_Die("out of memory");
    for (int i = 0; i < gPortNumFunctions; i++)
        if (gPortFunctions[i].func)
            sByHost[sNumByHost++] = (struct HostFunc){ (uint32_t)(uintptr_t)gPortFunctions[i].func,
                                                       gPortFunctions[i].gbaAddress };
    qsort(sByHost, (size_t)sNumByHost, sizeof(*sByHost), cmpHost);
}

static const struct HostFunc *findHost(uint32_t host) {
    int lo = 0, hi = sNumByHost - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (sByHost[mid].host == host)
            return &sByHost[mid];
        if (sByHost[mid].host < host)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return NULL;
}

bool Port_HostToGba(uint32_t host, uint32_t *gba) {
    const struct HostFunc *f = findHost(host);
    if (f)
        *gba = f->gba;
    return f != NULL;
}

