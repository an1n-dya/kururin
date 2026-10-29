// The PC port's entry (docs/port.md): what crt0 and the cartridge did. Maps the GBA's memory at
// its own addresses, loads and patches the player's ROM, then runs the game's AgbMain, which
// never returns: the frames end in Port_WaitVBlank.
#include "port.h"

#define SDL_MAIN_HANDLED  // this main is the program's (no SDL2main)
#include <SDL.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

void Port_Die(const char *fmt, ...) {
    static char msg[16384];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    fprintf(stderr, "kururin_pc: %s\n", msg);
    // The message also goes into kururin.log next to the program (to send along with a report)
    // and, for a game started from Explorer or a desktop's file manager (no console or terminal
    // to show stderr), into a message box
    char *base = SDL_GetBasePath(), path[4096];
    snprintf(path, sizeof(path), "%skururin.log", base ? base : "");
    SDL_free(base);
    FILE *log = fopen(path, "w");
    if (log) {
        fprintf(log, "kururin_pc: %s\n", msg);
        fclose(log);
        size_t n = strlen(msg);
        snprintf(msg + n, sizeof(msg) - n, "\n\n(This message is also in %s.)", path);
    }
#ifdef _WIN32
    bool box = !gPortOptions.headless;
#else
    bool box = !gPortOptions.headless && !isatty(STDERR_FILENO);
#endif
    if (box)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Kuru Kuru Kururin", msg, NULL);
    exit(2);
}

#ifdef _WIN32
// The Windows build is a GUI program (no console window of its own). Started from a console
// (cmd, PowerShell), its messages go there; output redirected to a file or pipe stays so.
static void attachConsole(void) {
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    DWORD type = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
    if (type != FILE_TYPE_DISK && type != FILE_TYPE_PIPE)
        freopen("CONOUT$", "w", stdout);
    type = GetFileType(GetStdHandle(STD_ERROR_HANDLE));
    if (type != FILE_TYPE_DISK && type != FILE_TYPE_PIPE)
        freopen("CONOUT$", "w", stderr);
}
#endif

// The ROM when --rom is not given: baserom.gba in the current directory, else next to the
// executable (double-clicked, or started from elsewhere).
static const char *defaultRomPath(void) {
    static char path[4096];
    FILE *f = fopen("baserom.gba", "rb");
    if (f) {
        fclose(f);
        return "baserom.gba";
    }
    char *base = SDL_GetBasePath();
    snprintf(path, sizeof(path), "%sbaserom.gba", base ? base : "");
    SDL_free(base);
    return path;
}

static jmp_buf sReset;

// The game keeps everything in the GBA's memory, so leaving its call stack is safe.
_Noreturn void Port_Reset(void) {
    longjmp(sReset, 1);
}

static void usage(void) {
    fputs("usage: kururin_pc [--rom baserom.gba] [--save FILE.sav | --no-save]\n"
          "                  [--headless] [--frames N] [--inputs FILE.inp]\n"
          "                  [--dump FILE|-] [--watch tests/gameplay/watch.txt] [--lag-frames FILE]\n"
          "                  [--scale N] [--fullscreen] [--mute] [--volume N] [--wav FILE] [--config FILE]\n"
          "                  [--video FILE] [--fb-every N] [--shot FRAME]... [--shot-dir DIR] [--poke F:A:V]...\n"
          "  --rom         your own dump of Kuru Kuru Kururin (Europe), SHA1 " BASEROM_SHA1 "\n"
          "  --save        the save file (default in play: kururin.sav next to the executable): read at\n"
          "                start, written each time the game saves and at exit\n"
          "  --no-save     no save file (the game starts with a blank cartridge each time)\n"
          "  --headless    no window (needs --frames or --inputs to end)\n"
          "  --frames N    stop after N frames\n"
          "  --inputs      keys per frame from a kurutest .inp file (tests/gameplay/run.py)\n"
          "  --dump        write the watched RAM after every frame (the port test)\n"
          "  --watch       the watch list for --dump\n"
          "  --lag-frames  the original's lag frames (the port test, port/test/port_test.py)\n"
          "  --scale N     the window is 240x160 times N (default 3)\n"
          "  --fullscreen  start in fullscreen (F11 or Alt+Enter switch)\n"
          "  --mute        no sound\n"
          "  --volume N    the sound's volume, 0-100 percent (default 100)\n"
          "  --config FILE the settings (default: kururin.ini next to the executable, written with the\n"
          "                defaults if missing; not read with --headless). The options above win over it.\n"
          "  --wav FILE    also write the sound into FILE (16-bit stereo, 53516 Hz; with --headless too)\n"
          "  --sound-only psg|ds  only the GB channels, or only DirectSound (to compare with refdump --audio-only)\n"
          "  --psg-trace FILE     the GB channels' state after each frame (to compare with refdump --psg-trace)\n"
          "  --video FILE  write a hash of the screen every --fb-every frames (default 1; the pixel test)\n"
          "  --shot FRAME  write the screen after that frame as <shot-dir>/port_frameNNNNNN.ppm, and\n"
          "                the I/O, palette, VRAM and OAM it was drawn from as port_stateNNNNNN.bin\n"
          "  --shot-dir    where --shot writes (default .)\n"
          "  --poke FRAME:ADDR:VALUE  write the byte VALUE to ADDR (EWRAM or IWRAM) before frame FRAME's\n"
          "                work, e.g. --poke 700:0x03000DCA:5 opens the debug stage select (docs/unused_content.md)\n"
          "Keys (kururin.ini changes them): arrows, Z = A, X = B, A = L, S = R, Enter = Start,\n"
          "Backspace = Select; a gamepad; F11 or Alt+Enter: fullscreen\n",
          stderr);
    exit(2);
}

int main(int argc, char **argv) {
    struct PortOptions *o = &gPortOptions;
#ifdef _WIN32
    attachConsole();
#endif
    o->romPath = NULL;
    o->watchPath = "tests/gameplay/watch.txt";
    o->shotDir = ".";
    o->fbEvery = 1;
    o->volume = -1;
    o->fullscreen = -1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        bool v = i + 1 < argc;
        if (!strcmp(a, "--rom") && v) o->romPath = argv[++i];
        else if (!strcmp(a, "--save") && v) o->savePath = argv[++i];
        else if (!strcmp(a, "--no-save")) o->noSave = true;
        else if (!strcmp(a, "--headless")) o->headless = true;
        else if (!strcmp(a, "--frames") && v) o->frames = atol(argv[++i]);
        else if (!strcmp(a, "--inputs") && v) o->inputsPath = argv[++i];
        else if (!strcmp(a, "--dump") && v) o->dumpPath = argv[++i];
        else if (!strcmp(a, "--watch") && v) o->watchPath = argv[++i];
        else if (!strcmp(a, "--lag-frames") && v) o->lagPath = argv[++i];
        else if (!strcmp(a, "--scale") && v) o->scale = atoi(argv[++i]);
        else if (!strcmp(a, "--fullscreen")) o->fullscreen = 1;
        else if (!strcmp(a, "--config") && v) o->configPath = argv[++i];
        else if (!strcmp(a, "--mute")) o->mute = true;
        else if (!strcmp(a, "--volume") && v) o->volume = atoi(argv[++i]);
        else if (!strcmp(a, "--wav") && v) o->wavPath = argv[++i];
        else if (!strcmp(a, "--sound-only") && v) o->soundOnly = argv[++i];
        else if (!strcmp(a, "--psg-trace") && v) o->psgTracePath = argv[++i];
        else if (!strcmp(a, "--video") && v) o->videoPath = argv[++i];
        else if (!strcmp(a, "--fb-every") && v) o->fbEvery = atoi(argv[++i]);
        else if (!strcmp(a, "--shot") && v && o->numShots < 64) o->shots[o->numShots++] = atol(argv[++i]);
        else if (!strcmp(a, "--shot-dir") && v) o->shotDir = argv[++i];
        else if (!strcmp(a, "--poke") && v && o->numPokes < 64) {
            char *end;
            long frame = strtol(argv[++i], &end, 0);
            unsigned long addr = *end == ':' ? strtoul(end + 1, &end, 0) : 0;
            unsigned long value = *end == ':' ? strtoul(end + 1, &end, 0) : 0x100;
            if (*end || frame < 0 || value > 0xFF || addr < EWRAM_START || addr >= IO_START)
                usage();
            o->pokes[o->numPokes].frame = frame;
            o->pokes[o->numPokes].addr = (uint32_t)addr;
            o->pokes[o->numPokes++].value = (uint8_t)value;
        }
        else usage();
    }
    if (o->scale < 0 || o->scale > 16 || o->fbEvery < 1 || o->volume < -1 || o->volume > 100)
        usage();
    if (!o->headless)
        Port_LoadConfig(o->configPath);  // what the command line left unset
    if (!o->scale)
        o->scale = 3;
    if (o->volume < 0)
        o->volume = 100;
    if (o->fullscreen < 0)
        o->fullscreen = 0;
    if (o->headless && !o->frames && !o->inputsPath)
        Port_Die("--headless needs --frames or --inputs");

    Port_InstallDivideHandler();
    Port_MapMemory(argv);
    Port_LoadRom(o->romPath ? o->romPath : defaultRomPath());
    Port_ProtectRom();
    if (o->savePath)
        Port_SetSaveFile(o->savePath);
    else if (!o->noSave && !o->headless && !o->inputsPath)
        Port_SetDefaultSaveFile();  // play: kururin.sav next to the executable
    Port_InitFrames();
    if (setjmp(sReset)) {
        Port_ClearMemory();
        Port_AudioReset();
        Port_StartFrame();
    }
    AgbMain();
    return 0;
}
