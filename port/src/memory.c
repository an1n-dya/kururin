// The GBA memory map at its own addresses (docs/port.md, "The GBA memory map"): the game's C
// uses them directly (absolute symbols, literal I/O and VRAM addresses, pointers in the ROM's
// data), so the port reserves host memory exactly there before anything runs. Linux: mmap with
// MAP_FIXED_NOREPLACE; Windows: VirtualAlloc (docs/port.md, "Windows").
#define _GNU_SOURCE
#include "port.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define PSAPI_VERSION 2  // GetMappedFileNameA is kernel32's K32GetMappedFileNameA
#include <psapi.h>
#else
#include <sys/mman.h>
#include <sys/personality.h>
#include <unistd.h>
#endif

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static const struct Region {
    const char *name;
    uint32_t start, size;
    uint32_t mirrorEnd;  // the GBA repeats the region up to here (0: no mirrors)
} sRegions[] = {
    // The unmapped space after the BIOS: the GBA reads open bus there (the last prefetched
    // instruction); the port reads 0. The game reads it through a table entry past the end:
    // LevelIntro_Run decompresses gLevelIntroPictureTiles[11] (0x000C0004) for The Last Land.
    // Addresses below the kernel's mmap_min_addr (usually 0x10000, which covers the BIOS)
    // cannot be mapped.
    { "open bus", 0x00010000u, 0x01FF0000u, 0 },
    { "EWRAM", EWRAM_START, EWRAM_SIZE, 0x03000000u },
    { "IWRAM", IWRAM_START, IWRAM_SIZE, 0x04000000u },
    { "I/O", IO_START, IO_SIZE, 0 },
    { "palette", PLTT_START, PLTT_SIZE, 0 },
    { "VRAM", VRAM_START, VRAM_SIZE, 0 },
    { "OAM", OAM_START, OAM_SIZE, 0 },
    { "ROM", ROM_START, ROM_RESERVE, 0 },
    { "SRAM", SRAM_START, SRAM_SIZE, 0 },
};

static void initIo(void);

#ifndef _WIN32
static uint32_t pageAlign(uint32_t size) {
    uint32_t page = (uint32_t)sysconf(_SC_PAGESIZE);
    return (size + page - 1) & ~(page - 1);
}

// What already occupies [start, start + size), from /proc/self/maps (for the error message).
static void reportOwner(uint32_t start, uint32_t size) {
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        unsigned long lo, hi;
        if (sscanf(line, "%lx-%lx", &lo, &hi) == 2 && lo < (unsigned long)start + size && hi > start)
            fprintf(stderr, "  in use: %s", line);
    }
    fclose(f);
}

static bool mapAt(uint32_t start, uint32_t size, int prot, int flags, int fd) {
    void *want = (void *)(uintptr_t)start;
    void *p = mmap(want, size, prot, flags | MAP_FIXED_NOREPLACE, fd, 0);
    if (p == want)
        return true;
    if (p != MAP_FAILED)
        munmap(p, size);  // a kernel without MAP_FIXED_NOREPLACE took it as a hint
    // The address as a plain hint: the kernel uses it when it is free.
    p = mmap(want, size, prot, flags, fd, 0);
    if (p == want)
        return true;
    if (p != MAP_FAILED)
        munmap(p, size);
    return false;
}

static bool mapRegion(const struct Region *r) {
    uint32_t size = pageAlign(r->size);
    if (r->start < IO_START && r->start >= EWRAM_START) {
        // Work RAM and its mirrors: one memory object mapped at every copy.
        int fd = memfd_create(r->name, 0);
        if (fd < 0 || ftruncate(fd, size))
            return false;
        for (uint32_t a = r->start; a < r->mirrorEnd; a += size)
            if (!mapAt(a, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd))
                return false;
        close(fd);
        return true;
    }
    int prot = r->start < EWRAM_START ? PROT_READ : PROT_READ | PROT_WRITE;
    return mapAt(r->start, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1);
}

void Port_MapMemory(char **argv) {
    for (size_t i = 0; i < sizeof(sRegions) / sizeof(sRegions[0]); i++) {
        const struct Region *r = &sRegions[i];
        if (mapRegion(r))
            continue;
        // Fallback: something was placed there at random (a library, the heap). Run again once
        // with address-space randomisation off.
        int pers = personality(0xffffffff);
        if (pers != -1 && !(pers & ADDR_NO_RANDOMIZE) && !getenv("KURURIN_PC_NORANDOM")) {
            setenv("KURURIN_PC_NORANDOM", "1", 1);
            personality(pers | ADDR_NO_RANDOMIZE);
            execv("/proc/self/exe", argv);
        }
        uint32_t end = r->mirrorEnd ? r->mirrorEnd : r->start + r->size;
        fprintf(stderr, "kururin_pc: cannot reserve the GBA's %s at 0x%08X-0x%08X (%s)\n", r->name, r->start, end,
                strerror(errno));
        reportOwner(r->start, end - r->start);
        exit(1);
    }
    memset((void *)(uintptr_t)SRAM_START, 0xFF, SRAM_SIZE);  // a blank cartridge reads 0xFF
    initIo();
}

void Port_ProtectRom(void) {
    if (mprotect((void *)(uintptr_t)ROM_START, ROM_RESERVE, PROT_READ))
        Port_Die("cannot make the ROM read-only: %s", strerror(errno));
}
#else  // _WIN32

// Windows: the GBA's memory is part of the exe's own image (port/tools/win_image.py): sections at
// the GBA addresses, mapped by the kernel when it creates the process, before any DLL, heap or
// thread stack exists. Port_MapMemory checks that each region is there; a region that is not
// (an exe linked without those sections) is allocated with VirtualAlloc if it is free. Anything
// else is a fatal error that shows what holds the memory (diagnose).

static uint32_t pageAlign(uint32_t size) {
    return (size + 0xFFFu) & ~0xFFFu;
}

static const char *stateName(DWORD state) {
    return state == MEM_FREE ? "free" : state == MEM_RESERVE ? "reserved" : state == MEM_COMMIT ? "committed" : "?";
}

static const char *typeName(DWORD type) {
    return type == MEM_IMAGE ? "image" : type == MEM_MAPPED ? "mapped" : type == MEM_PRIVATE ? "private" : "-";
}

// The VirtualQuery map of [start, end), a line per region, appended to buf: base, size, state,
// type, protection, allocation base, and for images and mapped files the file
static void diagnose(uint32_t start, uint32_t end, char *buf, size_t size) {
    size_t n = strlen(buf);
    int lines = 0;
    for (uint32_t a = start; a < end && n + 1 < size;) {
        MEMORY_BASIC_INFORMATION m;
        if (!VirtualQuery((const void *)(uintptr_t)a, &m, sizeof(m))) {
            n += (size_t)snprintf(buf + n, size - n, "  0x%08X: VirtualQuery failed (Windows error %lu)\n", a,
                                  (unsigned long)GetLastError());
            break;
        }
        uint32_t base = (uint32_t)(uintptr_t)m.BaseAddress, rend = base + (uint32_t)m.RegionSize;
        char file[MAX_PATH] = "";
        if (m.State != MEM_FREE && (m.Type == MEM_IMAGE || m.Type == MEM_MAPPED) &&
            !GetMappedFileNameA(GetCurrentProcess(), m.BaseAddress, file, sizeof(file))) {
            HMODULE mod;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)m.BaseAddress, &mod))
                GetModuleFileNameA(mod, file, sizeof(file));
        }
        if (++lines > 48) {
            n += (size_t)snprintf(buf + n, size - n, "  ... (up to 0x%08X)\n", end);
            break;
        }
        n += (size_t)snprintf(buf + n, size - n, "  0x%08X size 0x%08X %-9s %-7s protect 0x%03lX allocation 0x%08X %s\n",
                              base, (uint32_t)m.RegionSize, stateName(m.State), typeName(m.Type),
                              (unsigned long)m.Protect, (uint32_t)(uintptr_t)m.AllocationBase, file);
        if (rend <= a)
            break;
        a = rend;
    }
}

_Noreturn static void regionFailed(const struct Region *r, uint32_t start, uint32_t end, DWORD err) {
    static char msg[8192];
    snprintf(msg, sizeof(msg),
             "cannot reserve the GBA's %s at 0x%08X-0x%08X (Windows error %lu). The program keeps the GBA's memory "
             "at its own addresses; this is what the process holds there:\n",
             r->name, start, end, (unsigned long)err);
    diagnose(start, end, msg, sizeof(msg));
    Port_Die("%s", msg);
}

// Whether [start, start + size) is committed memory of the exe's own image
static bool inImage(uint32_t start, uint32_t size) {
    HMODULE self = GetModuleHandleA(NULL);
    for (uint32_t a = start; a - start < size;) {
        MEMORY_BASIC_INFORMATION m;
        if (!VirtualQuery((const void *)(uintptr_t)a, &m, sizeof(m)) || m.State != MEM_COMMIT ||
            m.Type != MEM_IMAGE || m.AllocationBase != (void *)self)
            return false;
        uint32_t rend = (uint32_t)(uintptr_t)m.BaseAddress + (uint32_t)m.RegionSize;
        if (rend <= a)
            return false;
        a = rend;
    }
    return true;
}

static bool allocAt(uint32_t start, uint32_t size, DWORD type, DWORD prot) {
    void *want = (void *)(uintptr_t)start;
    return VirtualAlloc(want, size, type, prot) == want;
}

// A region: in the image (zero-filled, as mmap's), else allocated; its mirrors read-only zeros in
// the image, else reserved without access (no code reaches them, docs/port.md, "Windows")
static void mapRegion(const struct Region *r) {
    if (r->start < EWRAM_START)
        return;  // the open bus: mapOpenBus
    uint32_t size = pageAlign(r->size);
    if (!inImage(r->start, size) && !allocAt(r->start, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
        regionFailed(r, r->start, r->start + size, GetLastError());
    if (r->mirrorEnd) {
        uint32_t from = (r->start + r->size + 0xFFFFu) & ~0xFFFFu;
        if (!inImage(from, r->mirrorEnd - from) && !allocAt(from, r->mirrorEnd - from, MEM_RESERVE, PAGE_NOACCESS))
            regionFailed(r, from, r->mirrorEnd, GetLastError());
    }
}

// The unmapped space below EWRAM (the GBA's open bus), read-only zeros as on Linux. The image
// covers it from 0x00401000 (after its headers); below, every free block is committed at
// start-up, and what the process already holds there (the main thread's stack, the process
// heap) is mapOnRead's.
static void mapOpenBus(void) {
    for (uint32_t a = 0x10000u; a < EWRAM_START;) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((const void *)(uintptr_t)a, &mbi, sizeof(mbi)))
            break;
        uint32_t end = (uint32_t)(uintptr_t)mbi.BaseAddress + (uint32_t)mbi.RegionSize;
        if (end > EWRAM_START || end <= a)
            end = EWRAM_START;
        uint32_t lo = (a + 0xFFFFu) & ~0xFFFFu;
        if (mbi.State == MEM_FREE && lo < end)
            allocAt(lo, end - lo, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);  // best effort
        a = end;
    }
}

// A read of the open bus that faults (memory reserved by something else there, or a free block
// too small to reserve): commit the page, zero-filled, and read it again. Only reads below
// EWRAM: every other fault is a crash as before.
static LONG CALLBACK mapOnRead(EXCEPTION_POINTERS *e) {
    const EXCEPTION_RECORD *r = e->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2 || r->ExceptionInformation[0] != 0)
        return EXCEPTION_CONTINUE_SEARCH;
    uintptr_t a = r->ExceptionInformation[1];
    MEMORY_BASIC_INFORMATION mbi;
    if (a < 0x10000u || a >= EWRAM_START || !VirtualQuery((const void *)a, &mbi, sizeof(mbi)))
        return EXCEPTION_CONTINUE_SEARCH;
    if (mbi.State == MEM_RESERVE) {
        // someone's reservation (a thread's stack below what it uses): readable and writable,
        // so its owner can still use it
        if (!VirtualAlloc((void *)(a & ~0xFFFu), 0x1000, MEM_COMMIT, PAGE_READWRITE))
            return EXCEPTION_CONTINUE_SEARCH;
    } else if (mbi.State == MEM_FREE) {
        if (!allocAt(a & ~0xFFFFu, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY))
            return EXCEPTION_CONTINUE_SEARCH;
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    static bool logged;
    if (!logged) {
        logged = true;
        fprintf(stderr, "kururin_pc: a read of the GBA's open bus at 0x%08X: mapped, reads 0\n", (unsigned)a);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

void Port_MapMemory(char **argv) {
    (void)argv;
    for (size_t i = 0; i < sizeof(sRegions) / sizeof(sRegions[0]); i++)
        mapRegion(&sRegions[i]);
    mapOpenBus();
    AddVectoredExceptionHandler(1, mapOnRead);
    memset((void *)(uintptr_t)SRAM_START, 0xFF, SRAM_SIZE);  // a blank cartridge reads 0xFF
    initIo();
}

void Port_ProtectRom(void) {
    DWORD old;
    if (!VirtualProtect((void *)(uintptr_t)ROM_START, ROM_RESERVE, PAGE_READONLY, &old))
        Port_Die("cannot make the ROM read-only (Windows error %lu)", (unsigned long)GetLastError());
}
#endif

// The I/O registers as the BIOS leaves them when it starts the cartridge (mGBA's GBAIOInit, the
// emulator the port is tested against): forced blank, the affine BGs' identity matrices, RCNT,
// SOUNDBIAS, all keys up.
static void initIo(void) {
    IO16(0x000) = 0x0080;  // DISPCNT
    IO16(0x020) = IO16(0x026) = 0x100;  // BG2PA, BG2PD
    IO16(0x030) = IO16(0x036) = 0x100;  // BG3PA, BG3PD
    IO16(0x088) = 0x200;   // SOUNDBIAS
    IO16(0x130) = 0x3FF;   // KEYINPUT
    IO16(0x134) = 0x8000;  // RCNT
}

// A reset: work RAM, I/O, palette, VRAM and OAM cleared (the SRAM and the ROM stay).
void Port_ClearMemory(void) {
    memset((void *)(uintptr_t)EWRAM_START, 0, EWRAM_SIZE);
    memset((void *)(uintptr_t)IWRAM_START, 0, IWRAM_SIZE);
    memset((void *)(uintptr_t)IO_START, 0, IO_SIZE);
    memset((void *)(uintptr_t)PLTT_START, 0, PLTT_SIZE);
    memset((void *)(uintptr_t)VRAM_START, 0, VRAM_SIZE);
    memset((void *)(uintptr_t)OAM_START, 0, OAM_SIZE);
    initIo();
}

