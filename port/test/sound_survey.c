// The sound survey (docs/port.md, "What the game's sound uses"): runs the original ROM in mGBA
// on kurutest inputs, the way refdump does, and samples at the end of every frame what the
// port's sound has to reproduce: the sound registers (SOUNDCNT_L/H/X, SOUNDBIAS, the sound
// DMAs, timer 0), m4a's settings in gSoundInfo (rate, channels, master volume, reverb,
// maxLines), what the DirectSound channels play (how many at once, fixed-frequency and looped
// samples, the pseudo echo), what the four GB-compatible channels play (gCgbChans: which
// channels, sweep, length, wave and noise settings), and which songs each player plays.
//
// It is kurutest (tests/gameplay/kurutest.c, included unchanged, its main renamed), like
// refdump and survey.
#define main kurutest_main
#include "../../tests/gameplay/kurutest.c"
#undef main

#include <mgba/internal/gba/io.h>

struct Counter {
    char key[120];
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
    char key[120];
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

static struct mCore *sCore;
static struct GBA *sGba;

static uint8_t r8(uint32_t a) { return sCore->rawRead8(sCore, a, -1); }
static uint16_t r16(uint32_t a) { return sCore->rawRead16(sCore, a, -1); }
static uint32_t r32(uint32_t a) { return sCore->rawRead32(sCore, a, -1); }
// An I/O register as the game last wrote it (mGBA keeps the written value; reads mask it)
static uint16_t io(uint32_t ofs) { return sGba->memory.io[ofs >> 1]; }

#define SOUND_INFO 0x03004880u  // gSoundInfo (kururin.toml [symbols])
#define CGB_CHANS 0x030058C0u   // gCgbChans
#define MPLAY_TABLE 0x08021058u // gMPlayTable: 4 x { info, tracks, numTracks, unk_A }

static void sampleFrame(void) {
    // the sound registers
    count("SOUNDCNT_H = 0x%04X", io(0x82) & 0x770F);
    count("SOUNDCNT_X = 0x%02X", io(0x84) & 0x80);
    count("SOUNDBIAS = 0x%04X", io(0x88));
    for (int d = 1; d <= 2; d++) {
        struct GBADMA *dma = &sGba->memory.dma[d];
        if (dma->reg & 0x8000)
            count("DMA%d on: control 0x%04X, source 0x%08X, dest 0x%08X", d, dma->reg, dma->source, dma->dest);
    }
    if (sGba->timers[0].flags || (io(0x102) & 0x80))
        count("timer 0: reload 0x%04X, control 0x%04X", sGba->timers[0].reload, io(0x102));
    for (int t = 1; t < 4; t++)
        if (io(0x102 + 4 * t) & 0x80)
            count("timer %d on: control 0x%04X", t, io(0x102 + 4 * t));

    // m4a's settings
    if (r32(SOUND_INFO) - 0x68736D53u > 1)
        return;  // before SoundInit
    count("gSoundInfo: freq %d, %d samples per VBlank, pcmFreq %d, divFreq %d, DMA period %d", r8(SOUND_INFO + 8),
          (int)r32(SOUND_INFO + 0x10), (int)r32(SOUND_INFO + 0x14), (int)r32(SOUND_INFO + 0x18), r8(SOUND_INFO + 0xB));
    count("gSoundInfo: master volume %d, maxLines %d", r8(SOUND_INFO + 7), r8(SOUND_INFO + 0xC));
    count("gSoundInfo: maxChans %d", r8(SOUND_INFO + 6));
    count("gSoundInfo: reverb %d", r8(SOUND_INFO + 5));

    // the DirectSound channels
    int active = 0;
    for (int c = 0; c < 12; c++) {
        uint32_t ch = SOUND_INFO + 0x50 + 0x40 * c;
        uint8_t status = r8(ch);
        if (!(status & 0xC7))
            continue;
        active++;
        uint8_t type = r8(ch + 1);
        uint32_t wav = r32(ch + 0x24);
        count("DirectSound: type 0x%02X (%s)", type, type & 8 ? "fixed frequency" : "resampled");
        if (status & 0x10)
            count("DirectSound: a looped sample");
        if (status & 0x04)
            count("DirectSound: pseudo echo (IEC)");
        if (status & 0x40)
            count("DirectSound: released (status 0x40)");
        if (wav >= 0x08000000 && wav < 0x0A000000) {
            uint32_t freq = r32(wav + 4);
            count("DirectSound: sample freq %s", freq ? "set" : "zero");
        } else {
            count("DirectSound: wave outside the ROM (0x%08X)", wav);
        }
        if (r8(ch + 0x0D) || r8(ch + 0x0C))
            count("DirectSound: pseudo echo volume/length set");
    }
    count("DirectSound channels playing at once: %d", active);

    // the GB-compatible channels (struct CgbChannel)
    for (int c = 0; c < 4; c++) {
        uint32_t ch = CGB_CHANS + 0x40 * c;
        uint8_t status = r8(ch);
        if (!status)
            continue;
        count("CGB channel %d (type %d) playing", c + 1, r8(ch + 1));
        uint8_t len = r8(ch + 0x1E), sweep = r8(ch + 0x1F);
        if (len)
            count("CGB channel %d: length %d", c + 1, len);
        if (c == 0)
            count("CGB channel 1: sweep 0x%02X", sweep);
        if (c == 2)
            count("CGB channel 3: wave 0x%08X", r32(ch + 0x24));
        if (c == 1 || c == 0)
            count("CGB channel %d: duty (wave word) 0x%08X", c + 1, r32(ch + 0x24));
        if (c == 3)
            count("CGB channel 4: noise (wave word) 0x%08X", r32(ch + 0x24));
        if (status & 0x04)
            count("CGB channel %d: pseudo echo", c + 1);
    }
    count("NR10 (channel 1 sweep) = 0x%02X", io(0x60) & 0x7F);
    count("NR11/NR21 duty: %d / %d", (io(0x62) >> 6) & 3, (io(0x68) >> 6) & 3);
    count("NR30 (channel 3 wave RAM) = 0x%02X", io(0x70) & 0xE0);
    count("NR32 (channel 3 volume) = 0x%02X", (io(0x72) >> 8) & 0xE0);
    count("NR43 (channel 4 noise) = 0x%02X", io(0x7C) & 0xFF);
    count("NR50 = 0x%02X", io(0x80) & 0x77);
    for (int b = 0; b < 8; b++)
        if (io(0x80) >> 8 & (1 << b))
            count("NR51: channel %d to the %s", (b & 3) + 1, b < 4 ? "right" : "left");

    // the songs
    for (int p = 0; p < 4; p++) {
        uint32_t info = r32(MPLAY_TABLE + 12 * p);
        uint32_t song = r32(info), status = r32(info + 4);
        if (song && (status & 0xFFFF) && !(status & 0x80000000))
            count("player %d (0x%08X, %d tracks) plays song header 0x%08X", p, info, r8(MPLAY_TABLE + 12 * p + 8), song);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fputs("usage: sound_survey WATCH ROM INPUT.inp...  (WATCH: tests/gameplay/watch.txt, for the inputs' checks)\n",
              stderr);
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
        sCore = emu.core;
        sGba = emu.core->board;
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
        printf("%8ld  %-100s first: %s frame %ld\n", sCounters[i].frames, sCounters[i].key, sCounters[i].input,
               sCounters[i].first);
    return 0;
}
