// The display survey (docs/port.md, "What the game draws with"): runs the original ROM in
// mGBA on kurutest inputs, the way refdump does, and reports which display features it uses:
// BG modes and layers, BGxCNT bits, affine BG parameters, windows, blending, mosaic, sprite
// shapes, sizes and modes, the sprite cycle budget per line, timed DMA and the display
// interrupts, and which video registers, VRAM, palette and OAM writes happen while lines are
// drawn (VCOUNT < 160) rather than in VBlank.
//
// The renderer's write hooks see every write as mGBA's memory unit makes it, with the line it
// happens on; the per-frame state is sampled at the end of each frame (VBlank).
#define main kurutest_main
#include "../../tests/gameplay/kurutest.c"
#undef main

#include <mgba/internal/gba/io.h>

struct Counter {
    char key[96];
    long frames;  // frames it was seen on
    long first;   // the first frame (of the input it was first seen in)
    char input[48];
};
#define MAX_COUNTERS 4096
static struct Counter sCounters[MAX_COUNTERS];
static int sNumCounters;
static long sFrame;
static const char *sInput;
static uint8_t sSeenThisFrame[MAX_COUNTERS];

static void count(const char *fmt, ...) {
    char key[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(key, sizeof(key), fmt, args);
    va_end(args);
    int i;
    for (i = 0; i < sNumCounters && strcmp(sCounters[i].key, key); i++)
        ;
    if (i == sNumCounters) {
        if (sNumCounters == MAX_COUNTERS)
            die("too many counters");
        snprintf(sCounters[i].key, sizeof(sCounters[i].key), "%s", key);
        sCounters[i].first = sFrame;
        snprintf(sCounters[i].input, sizeof(sCounters[i].input), "%s", sInput);
        sNumCounters++;
    }
    if (!sSeenThisFrame[i]) {
        sSeenThisFrame[i] = 1;
        sCounters[i].frames++;
    }
}

static struct GBA *sGba;
static uint16_t (*sOrigWriteReg)(struct GBAVideoRenderer *, uint32_t, uint16_t);
static void (*sOrigWriteVram)(struct GBAVideoRenderer *, uint32_t);
static void (*sOrigWritePal)(struct GBAVideoRenderer *, uint32_t, uint16_t);
static void (*sOrigWriteOam)(struct GBAVideoRenderer *, uint32_t);

static int vcount(void) {
    return sGba->video.vcount;
}

// --log: every write made while lines are drawn, with its frame and line (WIN0H, the HBlank
// DMA's, left out)
static bool sLog;
static void logWrite(const char *what, uint32_t address) {
    static long lastFrame = -1;
    static int lastLine = -1;
    static const char *lastWhat;
    if (!sLog || (sFrame == lastFrame && vcount() == lastLine && what == lastWhat))
        return;
    lastFrame = sFrame, lastLine = vcount(), lastWhat = what;
    printf("%s frame %ld line %d: %s 0x%05X\n", sInput, sFrame, vcount(), what, address);
}

static uint16_t hookReg(struct GBAVideoRenderer *r, uint32_t address, uint16_t value) {
    int v = vcount();
    if (v < 160) {
        uint16_t old = sGba->memory.io[address >> 1];
        if (old != value) {
            count("mid-frame write (value changes): video register 0x%03X", address);
            if (address != 0x40)
                logWrite("register", address);
        }
    }
    return sOrigWriteReg(r, address, value);
}

static void hookVram(struct GBAVideoRenderer *r, uint32_t address) {
    if (vcount() < 160) {
        count("mid-frame write: VRAM %s", address < 0x10000 ? "BG" : "OBJ");
        logWrite("VRAM", address);
    }
    sOrigWriteVram(r, address);
}

static void hookPal(struct GBAVideoRenderer *r, uint32_t address, uint16_t value) {
    if (vcount() < 160) {
        count("mid-frame write: palette %s", address < 0x200 ? "BG" : "OBJ");
        logWrite("palette", address);
    }
    sOrigWritePal(r, address, value);
}

static void hookOam(struct GBAVideoRenderer *r, uint32_t oam) {
    if (vcount() < 160) {
        count("mid-frame write: OAM");
        logWrite("OAM", oam);
    }
    sOrigWriteOam(r, oam);
}

static const char *const kShapes[3][4] = {
    { "8x8", "16x16", "32x32", "64x64" },
    { "16x8", "32x8", "32x16", "64x32" },
    { "8x16", "8x32", "16x32", "32x64" },
};
static const int kSizes[3][4][2] = {
    { { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
    { { 16, 8 }, { 32, 8 }, { 32, 16 }, { 64, 32 } },
    { { 8, 16 }, { 8, 32 }, { 16, 32 }, { 32, 64 } },
};

static void sampleFrame(void) {
    const uint16_t *io = sGba->memory.io;
    uint16_t dispcnt = io[0];
    if (dispcnt & 0x80) {
        count("DISPCNT forced blank");
        return;
    }
    int mode = dispcnt & 7;
    count("DISPCNT mode %d", mode);
    count("DISPCNT OBJ mapping %s", dispcnt & 0x40 ? "1D" : "2D");
    if (dispcnt & 0x10)
        count("DISPCNT frame select 1");
    if (dispcnt & 0x20)
        count("DISPCNT HBlank interval free");
    if (dispcnt & 0x2000)
        count("window 0 on");
    if (dispcnt & 0x4000)
        count("window 1 on");
    if (dispcnt & 0x8000)
        count("OBJ window on");
    for (int bg = 0; bg < 4; bg++) {
        if (!(dispcnt & (0x100 << bg)))
            continue;
        bool affine = (mode == 1 && bg == 2) || (mode == 2 && bg >= 2);
        if (mode == 1 && bg == 3)
            continue;
        uint16_t cnt = io[(0x08 >> 1) + bg];
        count("BG%d on, %s", bg, affine ? "affine" : "text");
        if (affine) {
            count("BG%d affine: size %d, %s", bg, 128 << (cnt >> 14), cnt & 0x2000 ? "wraps" : "transparent outside");
            int16_t pa = io[(0x20 >> 1) + (bg - 2) * 8], pb = io[(0x22 >> 1) + (bg - 2) * 8];
            int16_t pc = io[(0x24 >> 1) + (bg - 2) * 8], pd = io[(0x26 >> 1) + (bg - 2) * 8];
            if (pa != 0x100 || pb || pc || pd != 0x100)
                count("BG%d affine: rotated/scaled (not the identity)", bg);
        } else {
            count("BG%d text: %s, %dx%d", bg, cnt & 0x80 ? "8bpp" : "4bpp", 256 << (cnt >> 14 & 1),
                  256 << (cnt >> 15 & 1));
        }
        if (cnt & 0x40)
            count("BG%d mosaic bit (%s), MOSAIC %04X", bg, affine ? "affine" : "text", io[0x4C >> 1]);
    }
    uint16_t bldcnt = io[0x50 >> 1];
    static const char *const kEffects[] = { "none", "alpha", "brighten", "darken" };
    int effect = bldcnt >> 6 & 3;
    if (effect || (bldcnt & 0x3F3F))
        count("BLDCNT effect %s, target1 %02X, target2 %02X", kEffects[effect], bldcnt & 0x3F, bldcnt >> 8 & 0x3F);
    if (effect == 2 || effect == 3)
        count("BLDY %d (%s)", io[0x54 >> 1] & 0x1F, kEffects[effect]);
    if (effect == 1)
        count("BLDALPHA %04X", io[0x52 >> 1]);
    if (io[0x4C >> 1])
        count("MOSAIC register %04X", io[0x4C >> 1]);
    if (dispcnt & 0xE000)
        count("WININ %04X, WINOUT %04X", io[0x48 >> 1], io[0x4A >> 1]);

    uint16_t dispstat = io[0x04 >> 1], ie = io[0x200 >> 1];
    if ((dispstat & 0x10) && (ie & 2))
        count("HBlank interrupt on");
    if ((dispstat & 0x20) && (ie & 4))
        count("VCount interrupt on (line %d)", dispstat >> 8);
    for (int ch = 0; ch < 4; ch++) {
        uint16_t c = io[(0xBA >> 1) + ch * 6];
        if (c & 0x8000)
            count("DMA%d running at the sample: timing %d, dest 0x%08X", ch, c >> 12 & 3,
                  sGba->memory.dma[ch].dest);
    }

    if (!(dispcnt & 0x1000))
        return;
    int lineCycles[160] = { 0 };
    const uint16_t *oam = (const uint16_t *)sGba->video.oam.raw;
    for (int i = 0; i < 128; i++) {
        uint16_t a = oam[i * 4], b = oam[i * 4 + 1], c = oam[i * 4 + 2];
        bool affine = a & 0x100;
        if (!affine && (a & 0x200))
            continue;
        int shape = a >> 14, size = b >> 14;
        if (shape == 3) {
            count("OBJ shape 3 (prohibited)");
            continue;
        }
        int w = kSizes[shape][size][0], h = kSizes[shape][size][1];
        int cycles = w;
        if (affine) {
            if (a & 0x200)
                w *= 2, h *= 2;
            cycles = 10 + w * 2;
        }
        int y = a & 0xFF, x = b & 0x1FF;
        // as mGBA's GBAVideoRendererCleanOAM: entirely off screen
        if (y >= 160 && y + h < 228)
            continue;
        if (x >= 240 && x + w < 512)
            continue;
        count("OBJ %s%s%s", kShapes[shape][size], affine ? " affine" : "", affine && (a & 0x200) ? " double-size" : "");
        static const char *const kModes[] = { "normal", "semi-transparent", "OBJ window", "prohibited" };
        count("OBJ mode %s", kModes[a >> 10 & 3]);
        if (a & 0x1000)
            count("OBJ mosaic");
        if (a & 0x2000)
            count("OBJ 8bpp");
        if (!affine && (b & 0x1000))
            count("OBJ hflip");
        if (!affine && (b & 0x2000))
            count("OBJ vflip");
        count("OBJ priority %d", c >> 10 & 3);
        if (y + h > 256)
            count("OBJ wraps vertically (y + height > 256)");
        if (x + w > 512)
            count("OBJ wraps horizontally (x + width > 512)");
        for (int line = 0; line < 160; line++) {
            int ly = (line - y) & 0xFF;
            if (ly < h)
                lineCycles[line] += cycles;
        }
    }
    int budget = (dispcnt & 0x20) ? 954 : 1210, worst = 0;
    for (int line = 0; line < 160; line++)
        if (lineCycles[line] > worst)
            worst = lineCycles[line];
    if (worst > budget)
        count("OBJ cycle budget exceeded on a line (worst %d > %d)", worst, budget);
    count("OBJ worst line cycles: %s", worst <= 200 ? "<=200" : worst <= 600 ? "<=600" : worst <= 954 ? "<=954" : worst <= 1210 ? "<=1210" : ">1210");
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--log"))
        sLog = true, argv++, argc--;
    if (argc < 3) {
        fputs("usage: survey [--log] WATCH ROM INPUT.inp...  (WATCH: tests/gameplay/watch.txt, for the inputs' checks)\n"
              "  --log: also print every write made while lines are drawn (frame, line, what)\n", stderr);
        return 2;
    }
    mLogSetDefaultLogger(&sQuietLogger);
    loadWatch(argv[1]);
    argv++, argc--;
    long total = 0;
    for (int arg = 2; arg < argc; arg++) {
        sInput = strrchr(argv[arg], '/') ? strrchr(argv[arg], '/') + 1 : argv[arg];
        sSave = NULL;
        int numItems;
        long totalFrames;
        struct Item *items = loadInputs(argv[arg], &numItems, &totalFrames);
        struct Emu emu = { 0 };
        emuInit(&emu, "baserom", argv[1], 0x7FFFFFFF);
        sGba = emu.core->board;
        struct GBAVideoRenderer *r = sGba->video.renderer;
        sOrigWriteReg = r->writeVideoRegister, r->writeVideoRegister = hookReg;
        sOrigWriteVram = r->writeVRAM, r->writeVRAM = hookVram;
        sOrigWritePal = r->writePalette, r->writePalette = hookPal;
        sOrigWriteOam = r->writeOAM, r->writeOAM = hookOam;
        sFrame = 0;
        for (int it = 0; it < numItems; it++) {
            const struct Item *item = &items[it];
            if (item->kind == ITEM_RESET) {
                emu.reset = true;
                stepAll(&emu, 1);
                continue;
            }
            if (item->kind != ITEM_KEYS)
                continue;
            for (uint32_t k = 0; k < item->count; k++, sFrame++) {
                emu.keys = item->keys;
                memset(sSeenThisFrame, 0, sizeof(sSeenThisFrame));
                stepAll(&emu, 1);
                sampleFrame();
            }
        }
        total += sFrame;
        emu.core->deinit(emu.core);
        free(items);
    }
    printf("%ld frames\n", total);
    for (int i = 0; i < sNumCounters; i++)
        printf("%8ld  %-84s first: %s frame %ld\n", sCounters[i].frames, sCounters[i].key, sCounters[i].input,
               sCounters[i].first);
    return 0;
}
