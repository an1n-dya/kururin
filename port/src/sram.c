// The cartridge's SRAM (0x0E000000, 32 KiB used) backed by a file (docs/port.md, "Saving").
//
// In play the save file is kururin.sav next to the executable (or --save FILE): read at
// start-up, and written whenever the game has written SRAM: Port_SramFrame compares SRAM with
// what the file holds after every frame (the game writes it both through the SDK's WriteSram and
// directly, src/save.c), and once it has changed and then stayed the same for SAVE_DELAY frames
// the file is written (a save is written over several frames, so it is written once, whole).
// At exit (the window closed, Ctrl+C) a change not written yet is written. The file is written
// to a temporary name and renamed over the old one, so a crash cannot leave half a save.
//
// A test run (--inputs) or a --headless one has no save file unless --save is given (an
// input's `save` image is read and never written, as in kurutest); --no-save has none either.
#include "port.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_BYTES 0x8000  // what the game uses of the 64 KiB
#define SAVE_DELAY 30      // frames (0.5 s) without a change before the file is written

static char *sSavePath;
static uint8_t sSaved[SAVE_BYTES];  // what the file holds
static long sQuietFrames = -1;      // frames since the last change; -1: nothing to write

static const uint8_t *sram(void) {
    return (const uint8_t *)(uintptr_t)SRAM_START;
}

void Port_LoadSram(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return;  // a new save: the SRAM stays blank (0xFF)
    size_t n = fread((void *)(uintptr_t)SRAM_START, 1, SRAM_SIZE, f);
    fclose(f);
    if (n == 0)
        Port_Die("%s: empty save image", path);
}

static void writeSave(void) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s.tmp", sSavePath);
    FILE *f = fopen(tmp, "wb");
    bool ok = f && fwrite(sram(), 1, SAVE_BYTES, f) == SAVE_BYTES;
    if (f && fclose(f))
        ok = false;
#ifdef _WIN32
    if (ok)
        remove(sSavePath);  // rename does not replace a file on Windows
#endif
    if (!ok || rename(tmp, sSavePath)) {
        fprintf(stderr, "kururin_pc: cannot write %s\n", sSavePath);
        remove(tmp);
        return;
    }
    memcpy(sSaved, sram(), SAVE_BYTES);
    sQuietFrames = -1;
}

// After every frame: a change of SRAM starts the count; SAVE_DELAY quiet frames later the file
// is written.
void Port_SramFrame(void) {
    if (!sSavePath)
        return;
    if (memcmp(sSaved, sram(), SAVE_BYTES)) {
        static uint8_t last[SAVE_BYTES];
        if (sQuietFrames < 0 || memcmp(last, sram(), SAVE_BYTES)) {
            memcpy(last, sram(), SAVE_BYTES);
            sQuietFrames = 0;  // changed this frame
        } else if (++sQuietFrames >= SAVE_DELAY) {
            writeSave();
        }
    } else {
        sQuietFrames = -1;  // back to what the file holds
    }
}

// At exit: what the game wrote and the file does not hold yet
void Port_SaveSram(void) {
    if (sSavePath && memcmp(sSaved, sram(), SAVE_BYTES))
        writeSave();
}

void Port_SetSaveFile(const char *path) {
    sSavePath = strdup(path);
    Port_LoadSram(path);
    memcpy(sSaved, sram(), SAVE_BYTES);
}

// The default save file: kururin.sav in the executable's directory
void Port_SetDefaultSaveFile(void) {
    char *base = SDL_GetBasePath();
    char path[4096];
    snprintf(path, sizeof(path), "%skururin.sav", base ? base : "");
    SDL_free(base);
    Port_SetSaveFile(path);
}
