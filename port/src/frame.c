// The frame boundary's host side (docs/port.md, "Headless mode and the port test"): where the
// keys come from (an SDL window, or a kurutest .inp file), the original's lag frames for the
// port test, the dump of tests/gameplay/watch.txt's values after every frame, the screen's
// hashes and screenshots for the pixel test, and the window.
#include "port.h"

#include <SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PortOptions gPortOptions;

// --- inputs (.inp, tests/gameplay/run.py's format for kurutest) ---------------------------------

struct KeyRun {
    uint16_t keys;
    long count;  // 0: a reset (the BizHawk "Power" button) before the next frame
};
static struct KeyRun *sKeyRuns;
static int sNumKeyRuns;
static long sInputFrames;

static void loadInputs(const char *path) {
    FILE *f = fopen(path, "r");
    char line[8192];  // a `save` line holds an absolute path
    int lineNo = 0, cap = 0;
    if (!f)
        Port_Die("cannot open %s", path);
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        if (!strchr(line, '\n') && !feof(f))
            Port_Die("%s:%d: line too long", path, lineNo);
        char *p = line + strspn(line, " \t");
        if (*p == '#' || *p == '\n' || *p == '\0' || *p == '!')
            continue;  // comments, and the input's checks of the original (kurutest's business)
        if (!strncmp(p, "save ", 5)) {
            // the file is the rest of the line (a path may hold spaces), as kurutest reads it
            char *file = p + 5 + strspn(p + 5, " \t");
            size_t len = strcspn(file, "\r\n");
            while (len && (file[len - 1] == ' ' || file[len - 1] == '\t'))
                len--;
            file[len] = '\0';
            if (sNumKeyRuns || !len)
                Port_Die("%s:%d: 'save <file>' must come first", path, lineNo);
            FILE *image = fopen(file, "rb");  // Port_LoadSram takes a missing file as a new save
            if (!image)
                Port_Die("%s:%d: cannot open the save image %s", path, lineNo, file);
            fclose(image);
            if (!gPortOptions.savePath)
                Port_LoadSram(file);
            continue;
        }
        char *end = p + 5;
        struct KeyRun run = { 0, 0 };
        if (strncmp(p, "reset", 5)) {
            unsigned long keys = strtoul(p, &end, 16);
            run = (struct KeyRun){ (uint16_t)keys, 1 };
            if (*end == '*')
                run.count = strtol(end + 1, &end, 10);
            if (end == p || run.count <= 0 || keys > 0x3FF)
                Port_Die("%s:%d: bad key line", path, lineNo);
        }
        if (sNumKeyRuns == cap) {
            cap = cap ? cap * 2 : 1024;
            if (!(sKeyRuns = realloc(sKeyRuns, sizeof(*sKeyRuns) * (size_t)cap)))
                Port_Die("out of memory reading %s", path);
        }
        sKeyRuns[sNumKeyRuns++] = run;
        sInputFrames += run.count;
    }
    fclose(f);
}

// The run of emulator frame n (sNumKeyRuns after the input's end); *reset: the run is
// preceded by a reset that comes after frame n - 1.
static int inputRun(long n, bool *reset) {
    static int run;
    static long runStart;
    if (n < runStart)
        run = 0, runStart = 0;
    while (run < sNumKeyRuns && n >= runStart + sKeyRuns[run].count)
        runStart += sKeyRuns[run++].count;
    if (reset)
        *reset = n == runStart && run > 0 && sKeyRuns[run - 1].count == 0;
    return run;
}

// The keys of emulator frame n (the last ones held after the input's end).
static uint16_t inputKeys(long n) {
    int run = inputRun(n, NULL);
    if (run == sNumKeyRuns)
        return sNumKeyRuns ? sKeyRuns[sNumKeyRuns - 1].keys : 0;
    return sKeyRuns[run].keys;
}

// --- the original's lag frames (port test) ----------------------------------------------------

// 1: a lag frame, 2: one whose VBlank raised no interrupt; | 4: m4a's VSync ignored it; | 8: it
// came while the mixer ran
static uint8_t *sLag;
static long sLagSize;

static void loadLagFrames(const char *path) {
    FILE *f = fopen(path, "r");
    char line[128];
    if (!f)
        Port_Die("cannot open %s", path);
    while (fgets(line, sizeof(line), f)) {
        char *end;
        long n = strtol(line, &end, 10);
        if (end == line || n < 0)
            continue;
        if (n >= sLagSize) {
            long size = (n + 1) * 2;
            if (!(sLag = realloc(sLag, (size_t)size)))
                Port_Die("out of memory reading %s", path);
            memset(sLag + sLagSize, 0, (size_t)(size - sLagSize));
            sLagSize = size;
        }
        sLag[n] = (strstr(end, "noirq") ? 2 : 1) | (strstr(end, "soundoff") ? 4 : 0) | (strstr(end, "mixer") ? 8 : 0);
    }
    fclose(f);
}

static int lagKind(long n) {
    return n < sLagSize ? sLag[n] : 0;
}

// --- the watch dump ---------------------------------------------------------------------------

struct Watch {
    uint32_t addr, size;
    bool raw;  // a fourth word "raw": bytes, never a function pointer (the PCM buffer)
};
static struct Watch sWatch[256];
static int sNumWatch;
static FILE *sDump;

static void loadWatch(const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    if (!f)
        Port_Die("cannot open %s", path);
    while (fgets(line, sizeof(line), f)) {
        char *p = line + strspn(line, " \t");
        char name[64], flag[16];
        struct Watch w;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        int n = sscanf(p, "%x %u %63s %15s", &w.addr, &w.size, name, flag);
        // "finished" (kurutest's: compared only after frames the original finished, byte for
        // byte) is raw here too (the PCM buffer: its words are samples, never function
        // pointers); the port test skips the original's lag frames itself
        if (n < 3 || sNumWatch == 256 || (n == 4 && strcmp(flag, "raw") && strcmp(flag, "finished") && flag[0] != '#'))
            Port_Die("%s: bad line: %s", path, line);
        if (w.size > 4096)
            Port_Die("%s: entry %s is larger than 4096 bytes", path, name);
        w.raw = n == 4 && (!strcmp(flag, "raw") || !strcmp(flag, "finished"));
        sWatch[sNumWatch++] = w;
    }
    fclose(f);
}

// One line per frame: "<frame> <keys> <entry> <entry> ...", each entry its bytes in memory
// order as hex. Aligned words that hold a host function's address are written as the
// original's GBA address of that function (docs/port.md: RAM holds host function pointers).
void Port_DumpFrame(long frame, uint16_t keys) {
    fprintf(sDump, "%ld %03X", frame, keys);
    for (int i = 0; i < sNumWatch; i++) {
        const struct Watch *w = &sWatch[i];
        uint8_t buf[4096];  // loadWatch refuses larger entries
        uint32_t size = w->size;
        memcpy(buf, (const void *)(uintptr_t)w->addr, size);
        for (uint32_t o = (4 - (w->addr & 3)) & 3; !w->raw && size >= 4 && o <= size - 4; o += 4) {
            uint32_t v, gba;
            memcpy(&v, buf + o, 4);
            if (Port_HostToGba(v, &gba))
                memcpy(buf + o, &gba, 4);
        }
        fputc(' ', sDump);
        for (uint32_t b = 0; b < size; b++)
            fprintf(sDump, "%02X", buf[b]);
    }
    fputc('\n', sDump);
}

// --- the screen for the pixel test (--video, --shot) -------------------------------------------

static FILE *sVideo;

// FNV-1a over the visible colour of every pixel (24 bits, as port/test/refdump hashes mGBA's)
static uint64_t hashScreen(void) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (int i = 0; i < 240 * 160; i++) {
        h ^= gPortScreen[i] & 0xFFFFFF;
        h *= 0x100000001b3ULL;
    }
    return h;
}

static bool isShot(long frame) {
    for (int i = 0; i < gPortOptions.numShots; i++)
        if (gPortOptions.shots[i] == frame)
            return true;
    return false;
}

// <shot-dir>/port_frameNNNNNN.ppm, the colours as mGBA's 0x00BBGGRR
static void writeShot(long frame) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/port_frame%06ld.ppm", gPortOptions.shotDir, frame);
    FILE *f = fopen(path, "wb");
    if (!f)
        Port_Die("cannot write %s", path);
    fprintf(f, "P6\n240 160\n255\n");
    for (int i = 0; i < 240 * 160; i++) {
        uint32_t c = gPortScreen[i];
        uint8_t rgb[3] = { c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

// <shot-dir>/port_stateNNNNNN.bin: the video state the frame was drawn from: I/O (0x400
// bytes), palette RAM, VRAM, OAM (port/test/refdump writes the original's the same way)
static void writeState(long frame) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/port_state%06ld.bin", gPortOptions.shotDir, frame);
    FILE *f = fopen(path, "wb");
    if (!f)
        Port_Die("cannot write %s", path);
    fwrite((const void *)(uintptr_t)IO_START, 1, 0x400, f);
    fwrite((const void *)(uintptr_t)PLTT_START, 1, PLTT_SIZE, f);
    fwrite((const void *)(uintptr_t)VRAM_START, 1, VRAM_SIZE, f);
    fwrite((const void *)(uintptr_t)OAM_START, 1, OAM_SIZE, f);
    fclose(f);
}

static bool wantVideo(long frame) {
    return (sVideo && frame % gPortOptions.fbEvery == 0) || isShot(frame);
}

// --- the window --------------------------------------------------------------------------------

static SDL_Window *sWindow;
static SDL_Renderer *sRenderer;
static SDL_Texture *sTexture;
static uint16_t sWindowKeys;
static uint16_t sTapped;  // pressed during this frame (counted even if already released)
static uint64_t sNextFrameTime;
#define MAX_PADS 4
static SDL_GameController *sPads[MAX_PADS];
static bool sPaused;  // the window lost the focus (kururin.ini pause_on_focus_loss)
static volatile sig_atomic_t sQuit;

static void openWindow(void) {
    int scale = gPortOptions.scale;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER))
        Port_Die("SDL_Init: %s", SDL_GetError());
    Uint32 flags = SDL_WINDOW_RESIZABLE | (gPortOptions.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    sWindow = SDL_CreateWindow("Kuru Kuru Kururin", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 240 * scale,
                               160 * scale, flags);
    if (!sWindow)
        Port_Die("SDL_CreateWindow: %s", SDL_GetError());
    if (gPortOptions.fullscreen)
        SDL_ShowCursor(SDL_DISABLE);
    sRenderer = SDL_CreateRenderer(sWindow, -1, 0);
    if (!sRenderer)
        Port_Die("SDL_CreateRenderer: %s", SDL_GetError());
    // Whole multiples of 240x160 only (the window can be resized, or fullscreen: the picture
    // stays sharp, with black borders), nearest-neighbour scaling
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(sRenderer, 240, 160);
    SDL_RenderSetIntegerScale(sRenderer, SDL_TRUE);
    // gPortScreen's 0x00BBGGRR words are R, G, B, (flags) in memory
    sTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, 240, 160);
    if (!sTexture)
        Port_Die("SDL_CreateTexture: %s", SDL_GetError());
    sNextFrameTime = SDL_GetPerformanceCounter();
}

static void present(void) {
    SDL_UpdateTexture(sTexture, NULL, gPortScreen, 240 * sizeof(uint32_t));
    SDL_SetRenderDrawColor(sRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sRenderer);
    SDL_RenderCopy(sRenderer, sTexture, NULL, NULL);
    SDL_RenderPresent(sRenderer);
}

static void setFullscreen(bool on) {
    SDL_SetWindowFullscreen(sWindow, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
}

// The window's events: close, F11 / Alt+Enter (fullscreen), Escape (leaves fullscreen), gamepads
// plugged in and out, the focus (pausing). The GBA's keys are what the keyboard and the gamepads
// hold at the frame's end, and what was pressed during the frame (a tap shorter than a frame).
static void windowEvent(const SDL_Event *e) {
    switch (e->type) {
    case SDL_QUIT:
        Port_Exit(0);
    case SDL_KEYDOWN: {
        SDL_Keycode k = e->key.keysym.sym;
        bool full = SDL_GetWindowFlags(sWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (!e->key.repeat && (k == SDLK_F11 || (k == SDLK_RETURN && (e->key.keysym.mod & KMOD_ALT))))
            setFullscreen(!full);
        else if (!e->key.repeat && k == SDLK_ESCAPE && full)
            setFullscreen(false);
        else if (!(k == SDLK_RETURN && (e->key.keysym.mod & KMOD_ALT)))
            sTapped |= Port_KeyBits(k);
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN:
        sTapped |= Port_GamepadButtonBits(e->cbutton.button);
        break;
    case SDL_CONTROLLERDEVICEADDED:
        for (int i = 0; i < MAX_PADS; i++)
            if (!sPads[i]) {
                sPads[i] = SDL_GameControllerOpen(e->cdevice.which);
                break;
            }
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        for (int i = 0; i < MAX_PADS; i++)
            if (sPads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sPads[i])) == e->cdevice.which) {
                SDL_GameControllerClose(sPads[i]);
                sPads[i] = NULL;
            }
        break;
    case SDL_WINDOWEVENT:
        if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST && gPortOptions.pauseOnFocusLoss)
            sPaused = true;
        else if (e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            sPaused = false;
        else if (e->window.event == SDL_WINDOWEVENT_EXPOSED)
            present();
        break;
    }
}

// Without the focus: no frames (the game, the sound and the clock stop) until it comes back
static void pauseUntilFocus(void) {
    Port_AudioPause(true);
    SDL_SetWindowTitle(sWindow, "Kuru Kuru Kururin (paused)");
    while (sPaused && !sQuit) {
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, 100))
            windowEvent(&e);
    }
    SDL_SetWindowTitle(sWindow, "Kuru Kuru Kururin");
    Port_AudioPause(false);
    sNextFrameTime = SDL_GetPerformanceCounter();
}

static void pumpWindow(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e))
        windowEvent(&e);
    if (sPaused)
        pauseUntilFocus();
    uint16_t keys = Port_KeyboardBits() | sTapped;
    sTapped = 0;
    for (int i = 0; i < MAX_PADS; i++)
        if (sPads[i])
            keys |= Port_GamepadBits(sPads[i]);
    sWindowKeys = keys;
    present();
    // The GBA's frame rate, 59.73 Hz (280896 cycles of 16.78 MHz).
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t ticks = freq * 280896 / 16777216;
    sNextFrameTime += ticks + Port_AudioPacing(ticks);  // kept in step with the sound device
    uint64_t now = SDL_GetPerformanceCounter();
    if (now < sNextFrameTime)
        SDL_Delay((uint32_t)((sNextFrameTime - now) * 1000 / freq));
    else
        sNextFrameTime = now;
}

// --- the frame boundary -----------------------------------------------------------------------

static long sFrame;  // the emulator frame whose end comes next (the original's frame numbering)
static uint16_t keysOf(long n);

// Ctrl+C or a terminate request: the game stops at the end of the frame, saving first
static void onSignal(int sig) {
    (void)sig;
    sQuit = 1;
}

void Port_InitFrames(void) {
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    if (gPortOptions.inputsPath)
        loadInputs(gPortOptions.inputsPath);
    if (gPortOptions.lagPath)
        loadLagFrames(gPortOptions.lagPath);
    if (gPortOptions.dumpPath) {
        loadWatch(gPortOptions.watchPath);
        sDump = strcmp(gPortOptions.dumpPath, "-") ? fopen(gPortOptions.dumpPath, "w") : stdout;
        if (!sDump)
            Port_Die("cannot write %s", gPortOptions.dumpPath);
    }
    if (gPortOptions.videoPath) {
        sVideo = fopen(gPortOptions.videoPath, "w");
        if (!sVideo)
            Port_Die("cannot write %s", gPortOptions.videoPath);
    }
    if (gPortOptions.inputsPath && !gPortOptions.frames)
        gPortOptions.frames = sInputFrames;
    if (!gPortOptions.headless)
        openWindow();
    Port_AudioInit(!gPortOptions.headless && !gPortOptions.mute, gPortOptions.volume, gPortOptions.wavPath,
                   gPortOptions.soundOnly, gPortOptions.psgTracePath);
    Port_StartFrame();
}

// --poke: the bytes written before frame n's work (as a debugger would, between two frames)
static void poke(long n) {
    for (int i = 0; i < gPortOptions.numPokes; i++)
        if (gPortOptions.pokes[i].frame == n)
            *(volatile uint8_t *)(uintptr_t)gPortOptions.pokes[i].addr = gPortOptions.pokes[i].value;
}

// KEYINPUT during the frame that starts (power-on, reset)
void Port_StartFrame(void) {
    IO16(0x130) = ~keysOf(sFrame) & 0x3FF;
    poke(sFrame);
}

static uint16_t keysOf(long n) {
    return gPortOptions.inputsPath ? inputKeys(n) : sWindowKeys;
}

// After a frame: the end of the run, or a reset in the input before the next frame (the game
// starts again from AgbMain: port/src/main.c).
static void endOfRun(void) {
    bool reset;
    if (gPortOptions.frames && sFrame >= gPortOptions.frames)
        Port_Exit(0);
    if (gPortOptions.inputsPath && (inputRun(sFrame, &reset), reset))
        Port_Reset();
}

int Port_NextLagKind(void) {
    return lagKind(sFrame) & 3;
}

bool Port_NextLagInMixer(void) {
    return lagKind(sFrame) & 8;
}

bool gPortLagSoundOff;

bool Port_LagFrame(bool *irq) {
    int kind = lagKind(sFrame);
    if (!kind)
        return false;
    if (sDump)
        fprintf(sDump, "%ld %03X lag\n", sFrame, keysOf(sFrame));
    *irq = (kind & 3) == 1;
    gPortLagSoundOff = kind & 4;
    sFrame++;
    endOfRun();
    return true;
}

bool Port_WantPixels(void) {
    return !gPortOptions.headless || wantVideo(sFrame);
}

uint16_t Port_EndFrame(void) {
    Port_SramFrame();
    if (sQuit)
        Port_Exit(0);
    if (sDump)
        Port_DumpFrame(sFrame, keysOf(sFrame));
    if (sVideo && sFrame % gPortOptions.fbEvery == 0)
        fprintf(sVideo, "%ld %016llx\n", sFrame, (unsigned long long)hashScreen());
    if (isShot(sFrame)) {
        writeShot(sFrame);
        writeState(sFrame);
    }
    sFrame++;
    endOfRun();
    if (!gPortOptions.headless)
        pumpWindow();
    poke(sFrame);
    return keysOf(sFrame);
}

_Noreturn void Port_Exit(int status) {
    if (sDump)
        fflush(sDump);
    if (sVideo)
        fflush(sVideo);
    Port_SaveSram();
    Port_AudioClose();
    if (sWindow)
        SDL_Quit();
    exit(status);
}
