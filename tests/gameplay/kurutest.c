// Gameplay equivalence harness: runs two GBA ROMs (normally baserom.gba and the rebuilt
// kururin.gba) side by side in mGBA, feeding both the same input on every frame, and stops at
// the first frame where they differ.
//
// Every frame it compares each entry of the watch list (tests/gameplay/watch.txt: address,
// size, name), the save data (SRAM) and what the game wrote to the sound hardware (see "The
// sound" below); every --fb-every frames it compares a hash of the framebuffer. On a mismatch it
// prints the frame, the watch entry (address, both values), the first differing sound write or
// the first differing pixel, writes both screens as PPM into --dump-dir, and exits 1.
//
// The sound: what each ROM writes to the sound registers, 0x04000060-0x040000A7 (the GB
// channels, SOUNDCNT, SOUNDBIAS, wave RAM, FIFO A and B), with the values, in order:
//   - the CPU's writes (str, strh, strb, stm: m4a's CgbSound, SoundInit, ...): the same list
//     every frame;
//   - the sound DMAs' writes to FIFO A and B (the PCM samples): the same words in the same
//     order, compared as two streams, since which frame a transfer falls in depends on the
//     sample timer's phase. Each stream is cut at every restart of its DMA (a write that
//     enables the channel, m4aSoundVSync every few VBlanks and m4aSoundVSyncOn): the timer's
//     request and the restart race, so at a restart one ROM may have done one transfer (4
//     words) more than the other since the last one; that transfer is not compared;
// together with the PCM buffer those samples come from (watch.txt's gSoundInfo.pcmBuffer,
// marked "finished": compared only for frames whose work the original finished before the
// next VBlank, see compareFinished).
// When in the frame a write happens (its cycle, its line) is not compared: mGBA's mixed
// samples depend on it, and the game's non-matching C changes it without changing what is
// written (docs/audit.md, "Audio with mGBA's volume on"). mGBA's master volume stays 0: the
// mixed samples are not used.
//
// Input file (.inp, written by tests/gameplay/run.py), one item per line:
//   <hex keys>[*<count>]     the GBA KEYINPUT bits held for one frame (or <count> frames)
//   ! <watch name> <op> <n>  check ROM A (the original) after the frames so far: op is one of
//                            == != < <= > >=; fails the test if the input no longer does what
//                            it claims (e.g. "reaches level 1")
//   reset                    reset both consoles (the BizHawk "Power" button; SRAM is kept)
//   save <file>              before the first frame only: both consoles boot with this SRAM
//                            image (tests/gameplay/saves/*.sav, written by tools/make_save.py)
//                            instead of a blank one; each gets its own copy in memory, so the
//                            file is never written. <file> is the rest of the line (spaces too)
//   # ...                    comment
//
// --coverage FILE and --rom-loads FILE run ROM A (the original) one instruction at a time,
// with the same event order as mGBA's own frame loop, and write what it did as hex ranges
// "start end" (end exclusive), one per line, merged:
//   --coverage   every address ROM A executed: ROM ranges, and "ram" lines for code run from
//                EWRAM/IWRAM, with its bytes as they were when it ran (so scripts/coverage.py
//                can tell which ROM function was copied there; see struct Episode);
//   --rom-loads  what ROM A's own instructions (not DMA, not the HLE BIOS) loaded from ROM:
//                "w" lines for words loaded with ldr/ldm, "n" lines for bytes loaded with
//                ldrb/ldrh/ldrsb/ldrsh: the evidence scripts/find_pointers.py uses.
//
// --dump-save FILE writes ROM A's SRAM at the end of the run (to check what a save image
// became: e.g. that the game kept it instead of formatting it).
//
// --rom-map FILE (scripts/rom_map.py) says where each of ROM A's ROM addresses is in ROM B, as
// "<start> <end> <delta>" hex lines. RAM can hold ROM addresses (function and data pointers),
// which legitimately differ when code or data moved: a watched aligned word that differs is
// equal if ROM A's value is a ROM address that the map turns into ROM B's value (see sameWord).
//
// Built against libmgba (tools/mgba, built by setup.sh) with mGBA's HLE BIOS.

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/gba/core.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/video.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WATCH 256
#define AUDIO_BUFFER 4096  // samples per channel; a frame makes about 550
#define MAX_SHOTS 64
#define SOUND_IO_START 0x04000060u
#define SOUND_IO_END 0x040000A8u  // GB channels, SOUNDCNT, SOUNDBIAS, wave RAM, FIFO A and B
#define FIFO_A 0x040000A0u
#define MAX_SOUND_WRITES 4096   // CPU writes in a frame (a frame of m4a makes about 50)
#define FIFO_TRANSFER 4         // words a sound DMA request moves
#define FIFO_SLACK 128          // words one ROM's stream may be ahead (a frame moves about 56)
#define FIFO_RESTART 0xFFFFFFFFu  // not a word: a restart of the DMA feeding the FIFO

struct Watch {
    uint32_t addr;
    uint32_t size;
    char name[64];
    bool finished;  // a fourth word "finished": see compareFinished
};

struct Item {
    enum { ITEM_KEYS, ITEM_EXPECT, ITEM_RESET } kind;
    uint32_t keys;
    uint32_t count;
    int watch;      // ITEM_EXPECT
    char op[3];
    int64_t value;
    int line;
};

struct Emu {
    const char *label;
    struct mCore *core;
    color_t *video;
    unsigned width, height;
    uint32_t keys;
    bool reset;
    // the sound (see the top): the CPU's writes of the last frame, and the FIFO words not yet
    // compared with the other ROM's
    struct SoundWrite { uint32_t addr, value; uint8_t size; } soundWrites[MAX_SOUND_WRITES];
    int numSoundWrites;  // more than MAX_SOUND_WRITES: counted, not kept
    uint32_t *fifo[2];         // words, and FIFO_RESTART where the channel was restarted
    size_t fifoCount[2], fifoCap[2];
    uint64_t fifoCompared[2];  // words compared so far (for the report)
    uint16_t dmaCnt[4];        // the DMA channels' CNT_H as last written (restarts)
};

// Tracing of ROM A (--coverage, --rom-loads): one bit per halfword executed / word loaded.
#define ROM_SPAN 0x02000000u     // 0x08000000-0x09FFFFFF (the whole cartridge ROM space)
#define EWRAM_SPAN 0x40000u
#define IWRAM_SPAN 0x8000u
static uint8_t *sExecRom, *sLoadRom, *sNarrowRom;
// Code run from RAM, one episode per entry into it (from ROM, the BIOS, or outside the current
// window): the code as it was then (EP_WIN bytes around the entry) and what of it ran. RAM is
// reused (the SRAM library copies different routines to the same stack area), so an address
// alone does not say which code ran there.
#define EP_WIN 0x400u
#define MAX_EP 1024
struct Episode {
    uint32_t entry, key, base;
    uint8_t code[EP_WIN];
    uint8_t exec[EP_WIN / 16];
};
static struct Episode *sEpisodes;
static int sNumEpisodes, sLostEpisodes;
static struct Episode *sEpisode;  // the one running, NULL outside RAM
static bool sInInsn;  // inside one of ROM A's instructions (not DMA/events, not the HLE BIOS)
static uint32_t (*sOrigLoad32)(struct ARMCore *, uint32_t, int *);
static uint32_t (*sOrigLoad16)(struct ARMCore *, uint32_t, int *);
static uint32_t (*sOrigLoad8)(struct ARMCore *, uint32_t, int *);
static uint32_t (*sOrigLoadMultiple)(struct ARMCore *, uint32_t, int, enum LSMDirection, int *);
static void (*sOrigSwi16)(struct ARMCore *, int);
static void (*sOrigSwi32)(struct ARMCore *, int);

static bool sTraceA;  // run ROM A with runFrameTraced
static struct ARMCore *sTraceCpu;
static void runFrameTraced(struct Emu *emu);

static struct Watch sWatch[MAX_WATCH];
static int sNumWatch;

#define MAX_MAP 4096
static struct { uint32_t lo, hi; int32_t delta; } sMap[MAX_MAP];
static int sNumMap;

// The SRAM image both consoles boot with (an input's "save" line), or none.
static uint8_t *sSave;
static size_t sSaveSize;

static void quietLog(struct mLogger *logger, int category, enum mLogLevel level, const char *format, va_list args) {
    (void)logger; (void)category; (void)level; (void)format; (void)args;
}
static struct mLogger sQuietLogger = { .log = quietLog };

static void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fputs("kurutest: ", stderr);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(2);
}

// frameskip: render only every (frameskip + 1)th frame, counted from power-on and from each
// reset (mGBA skips the renderer, not the emulation); the screen is compared on those frames.
static void emuInit(struct Emu *emu, const char *label, const char *path, int frameskip) {
    emu->label = label;
    emu->core = GBACoreCreate();
    if (!emu->core || !emu->core->init(emu->core))
        die("cannot create the mGBA core");
    mCoreInitConfig(emu->core, NULL);
    emu->core->desiredVideoDimensions(emu->core, &emu->width, &emu->height);
    emu->video = calloc((size_t)emu->width * emu->height, sizeof(color_t));
    emu->core->setVideoBuffer(emu->core, emu->video, emu->width);
    // No BIOS file and no save file: HLE BIOS, boot straight into the ROM, blank SRAM in memory
    // (or a copy of the input's save image, in memory too).
    if (!mCoreLoadFile(emu->core, path))
        die("cannot load %s", path);
    if (sSave) {
        struct VFile *vf = VFileMemChunk(sSave, sSaveSize);
        if (!vf || !emu->core->loadSave(emu->core, vf))
            die("cannot load the save image into %s", path);
    }
    emu->core->setAudioBufferSize(emu->core, AUDIO_BUFFER);
    emu->core->opts.frameskip = frameskip;
    emu->core->reloadConfigOption(emu->core, NULL, NULL);
    emu->core->reset(emu->core);
}

// ROM B runs on its own thread, one frame per pair of barrier waits, so the two consoles
// emulate in parallel; everything else (comparing, screenshots) happens between frames.
static pthread_barrier_t sFrameStart, sFrameDone;
static volatile bool sQuit;

// Drains the samples the frame produced (both channels), which are not compared (see the top).
static void readAudio(struct Emu *emu) {
    static __thread int16_t buf[AUDIO_BUFFER];
    for (int ch = 0; ch < 2; ch++) {
        struct blip_t *blip = emu->core->getAudioChannel(emu->core, ch);
        int n = blip_samples_avail(blip);
        if (n > AUDIO_BUFFER)
            n = AUDIO_BUFFER;
        blip_read_samples(blip, buf, n, 0);
    }
}

// --- the sound: every store to the sound registers, through hooks on mGBA's memory interface
// (the CPU's stores and the DMAs' go through it; the HLE BIOS's CpuSet/CpuFastSet too)

static struct Emu *sSoundEmus[2];
static void (*sSoundOrigStore8)(struct ARMCore *, uint32_t, int8_t, int *);
static void (*sSoundOrigStore16)(struct ARMCore *, uint32_t, int16_t, int *);
static void (*sSoundOrigStore32)(struct ARMCore *, uint32_t, int32_t, int *);
static uint32_t (*sSoundOrigStoreMultiple)(struct ARMCore *, uint32_t, int, enum LSMDirection, int *);

static void fifoPush(struct Emu *emu, int f, uint32_t value) {
    if (emu->fifoCount[f] == emu->fifoCap[f]) {
        emu->fifoCap[f] = emu->fifoCap[f] ? emu->fifoCap[f] * 2 : 256;
        emu->fifo[f] = realloc(emu->fifo[f], emu->fifoCap[f] * sizeof(uint32_t));
        if (!emu->fifo[f])
            die("out of memory");
    }
    emu->fifo[f][emu->fifoCount[f]++] = value;
}

// A write to DMA channel c's CNT_H: enabling it (re)starts it; if it feeds a FIFO, the FIFO's
// stream gets a restart mark.
static void dmaControl(struct Emu *emu, struct ARMCore *cpu, int c, uint16_t value) {
    bool start = (value & 0x8000) && !(emu->dmaCnt[c] & 0x8000);
    emu->dmaCnt[c] = value;
    uint32_t dest = ((struct GBA *)cpu->master)->memory.dma[c].dest;
    if (start && (dest == FIFO_A || dest == FIFO_A + 4))
        fifoPush(emu, dest == FIFO_A + 4, FIFO_RESTART);
}

static void soundWrite(struct ARMCore *cpu, uint32_t address, uint32_t value, uint8_t size) {
    struct Emu *emu = sSoundEmus[0] && sSoundEmus[0]->core->cpu == cpu ? sSoundEmus[0] : sSoundEmus[1];
    // DMA0CNT_H .. DMA3CNT_H (0x040000BA + 12 * c), as a halfword or the top of a word
    for (int c = 0; c < 4 && address >= 0x040000B8u && address < 0x040000E0u; c++) {
        uint32_t cnt = 0x040000BAu + 12u * (uint32_t)c;
        if (size == 2 && address == cnt)
            dmaControl(emu, cpu, c, (uint16_t)value);
        else if (size == 4 && address == cnt - 2)
            dmaControl(emu, cpu, c, (uint16_t)(value >> 16));
        else if (size == 1 && address == cnt + 1)
            dmaControl(emu, cpu, c, (uint16_t)((emu->dmaCnt[c] & 0xFF) | (value << 8)));
    }
    if (address < SOUND_IO_START || address >= SOUND_IO_END)
        return;
    if (((struct GBA *)cpu->master)->performingDMA && address >= FIFO_A) {
        fifoPush(emu, address >= FIFO_A + 4, value);
        return;
    }
    if (emu->numSoundWrites < MAX_SOUND_WRITES)
        emu->soundWrites[emu->numSoundWrites] = (struct SoundWrite){ address, value, size };
    emu->numSoundWrites++;
}

static void soundStore8(struct ARMCore *c, uint32_t a, int8_t v, int *cy) {
    soundWrite(c, a, (uint8_t)v, 1);
    sSoundOrigStore8(c, a, v, cy);
}
static void soundStore16(struct ARMCore *c, uint32_t a, int16_t v, int *cy) {
    soundWrite(c, a, (uint16_t)v, 2);
    sSoundOrigStore16(c, a, v, cy);
}
static void soundStore32(struct ARMCore *c, uint32_t a, int32_t v, int *cy) {
    soundWrite(c, a, (uint32_t)v, 4);
    sSoundOrigStore32(c, a, v, cy);
}
// stm: the registers in the order and at the addresses mGBA's GBAStoreMultiple stores them
static uint32_t soundStoreMultiple(struct ARMCore *c, uint32_t base, int mask, enum LSMDirection dir, int *cy) {
    if (base >> 24 == 4 && mask) {
        int n = 0;
        for (int i = 0; i < 16; i++)
            n += (mask >> i) & 1;
        uint32_t a = dir & LSM_D ? base - (uint32_t)(n * 4 - 4) : base;
        if (dir & LSM_B)
            a += dir & LSM_D ? -4u : 4u;
        a &= ~3u;
        for (int i = 0; i < 16; i++)
            if (mask & (1 << i)) {
                soundWrite(c, a, (uint32_t)c->gprs[i], 4);
                a += 4;
            }
    }
    return sSoundOrigStoreMultiple(c, base, mask, dir, cy);
}

static void soundInit(struct Emu *emu, int index) {
    struct ARMCore *cpu = emu->core->cpu;
    sSoundEmus[index] = emu;
    sSoundOrigStore8 = cpu->memory.store8, cpu->memory.store8 = soundStore8;
    sSoundOrigStore16 = cpu->memory.store16, cpu->memory.store16 = soundStore16;
    sSoundOrigStore32 = cpu->memory.store32, cpu->memory.store32 = soundStore32;
    sSoundOrigStoreMultiple = cpu->memory.storeMultiple, cpu->memory.storeMultiple = soundStoreMultiple;
}

static void emuStep(struct Emu *emu) {
    emu->numSoundWrites = 0;
    if (emu->reset) {
        emu->core->reset(emu->core);
        emu->reset = false;
        // the FIFO words still queued (at most FIFO_SLACK more in one ROM) are not compared
        emu->fifoCount[0] = emu->fifoCount[1] = 0;
        memset(emu->dmaCnt, 0, sizeof(emu->dmaCnt));
        return;
    }
    emu->core->setKeys(emu->core, emu->keys);
    if (sTraceA && emu->core->cpu == sTraceCpu)
        runFrameTraced(emu);
    else
        emu->core->runFrame(emu->core);
    readAudio(emu);
}

static void *emuThread(void *arg) {
    struct Emu *emu = arg;
    for (;;) {
        pthread_barrier_wait(&sFrameStart);
        if (sQuit)
            return NULL;
        emuStep(emu);
        pthread_barrier_wait(&sFrameDone);
    }
}

static inline void setBit(uint8_t *map, uint32_t i) {
    map[i >> 3] |= (uint8_t)(1u << (i & 7));
}

static void markLoad(uint32_t address) {
    if (sInInsn && address >= 0x08000000u && address - 0x08000000u < ROM_SPAN)
        setBit(sLoadRom, (address - 0x08000000u) >> 2);
}

static void markNarrow(uint32_t address, uint32_t size) {
    if (sInInsn && address >= 0x08000000u && address - 0x08000000u < ROM_SPAN)
        for (uint32_t i = 0; i < size; i++)
            setBit(sNarrowRom, ((address & ~(size - 1)) - 0x08000000u) + i);
}

static uint32_t traceLoad16(struct ARMCore *cpu, uint32_t address, int *cycleCounter) {
    markNarrow(address, 2);
    return sOrigLoad16(cpu, address, cycleCounter);
}

static uint32_t traceLoad8(struct ARMCore *cpu, uint32_t address, int *cycleCounter) {
    markNarrow(address, 1);
    return sOrigLoad8(cpu, address, cycleCounter);
}

static uint32_t traceLoad32(struct ARMCore *cpu, uint32_t address, int *cycleCounter) {
    markLoad(address);
    return sOrigLoad32(cpu, address, cycleCounter);
}

// The words GBALoadMultiple reads (mGBA's src/gba/memory.c).
static uint32_t traceLoadMultiple(struct ARMCore *cpu, uint32_t address, int mask, enum LSMDirection direction,
                                  int *cycleCounter) {
    int n = __builtin_popcount((unsigned)mask & 0xFFFF);
    uint32_t start = address;
    if (direction & LSM_D)
        start -= (uint32_t)(n << 2) - 4;
    if (direction & LSM_B)
        start += (direction & LSM_D) ? -4u : 4u;
    for (int i = 0; i < n; i++)
        markLoad((start & ~3u) + 4u * (uint32_t)i);
    return sOrigLoadMultiple(cpu, address, mask, direction, cycleCounter);
}

static void traceSwi16(struct ARMCore *cpu, int immediate) {
    bool was = sInInsn;
    sInInsn = false;
    sOrigSwi16(cpu, immediate);
    sInInsn = was;
}

static void traceSwi32(struct ARMCore *cpu, int immediate) {
    bool was = sInInsn;
    sInInsn = false;
    sOrigSwi32(cpu, immediate);
    sInInsn = was;
}

static bool isRam(uint32_t a) {
    return a - 0x02000000u < EWRAM_SPAN || a - 0x03000000u < IWRAM_SPAN;
}

static struct Episode *enterRam(struct mCore *core, uint32_t entry) {
    uint32_t key = 2166136261u;
    for (uint32_t i = 0; i < 64; i++)
        key = (key ^ (uint32_t)core->rawRead8(core, entry + i, -1)) * 16777619u;
    for (int i = 0; i < sNumEpisodes; i++)
        if (sEpisodes[i].entry == entry && sEpisodes[i].key == key)
            return &sEpisodes[i];
    if (sNumEpisodes == MAX_EP) {
        sLostEpisodes++;
        return NULL;
    }
    struct Episode *ep = &sEpisodes[sNumEpisodes++];
    ep->entry = entry;
    ep->key = key;
    ep->base = entry - EP_WIN / 2;
    for (uint32_t i = 0; i < EP_WIN; i++)
        ep->code[i] = (uint8_t)core->rawRead8(core, ep->base + i, -1);
    return ep;
}

static void markRam(struct mCore *core, uint32_t a) {
    if (!sEpisode || a - sEpisode->base >= EP_WIN)
        sEpisode = enterRam(core, a);
    if (sEpisode)
        setBit(sEpisode->exec, (a - sEpisode->base) >> 1);
}

static void traceInit(struct Emu *emu) {
    struct ARMCore *cpu = emu->core->cpu;
    sExecRom = calloc(ROM_SPAN / 16, 1);
    sEpisodes = calloc(MAX_EP, sizeof(*sEpisodes));
    sLoadRom = calloc(ROM_SPAN / 32, 1);
    sNarrowRom = calloc(ROM_SPAN / 8, 1);
    sOrigLoad32 = cpu->memory.load32;
    sOrigLoad16 = cpu->memory.load16;
    sOrigLoad8 = cpu->memory.load8;
    sOrigLoadMultiple = cpu->memory.loadMultiple;
    sOrigSwi16 = cpu->irqh.swi16;
    sOrigSwi32 = cpu->irqh.swi32;
    cpu->memory.load32 = traceLoad32;
    cpu->memory.load16 = traceLoad16;
    cpu->memory.load8 = traceLoad8;
    cpu->memory.loadMultiple = traceLoadMultiple;
    cpu->irqh.swi16 = traceSwi16;
    cpu->irqh.swi32 = traceSwi32;
}

// mGBA's _GBACoreRunFrame + ARMRunLoop, one instruction at a time: the same instructions and
// events in the same order, so ROM A still compares equal with ROM B.
static void runFrameTraced(struct Emu *emu) {
    struct GBA *gba = emu->core->board;
    struct ARMCore *cpu = emu->core->cpu;
    uint32_t frameCounter = gba->video.frameCounter;
    uint32_t startCycle = mTimingCurrentTime(&gba->timing);
    while (gba->video.frameCounter == frameCounter &&
           mTimingCurrentTime(&gba->timing) - startCycle < VIDEO_TOTAL_LENGTH + VIDEO_HORIZONTAL_LENGTH) {
        while (cpu->cycles < cpu->nextEvent) {
            // before the step, the PC is one instruction ahead of the one about to run
            uint32_t size = cpu->executionMode == MODE_THUMB ? 2u : 4u;
            uint32_t pc = (uint32_t)cpu->gprs[ARM_PC] - size;
            for (uint32_t a = pc; a < pc + size; a += 2) {
                if (a - 0x08000000u < ROM_SPAN)
                    setBit(sExecRom, (a - 0x08000000u) >> 1);
                if (isRam(a))
                    markRam(emu->core, a);
                else
                    sEpisode = NULL;
            }
            sInInsn = true;
            ARMRun(cpu);
            sInInsn = false;
        }
        cpu->irqh.processEvents(cpu);
    }
}

// Writes the set bits of map (one bit per `unit` bytes from base) as merged "start end" ranges.
static void writeRanges(FILE *f, const char *prefix, const uint8_t *map, uint32_t bits, uint32_t base, uint32_t unit) {
    for (uint32_t i = 0; i < bits;) {
        if (!(map[i >> 3] & (1u << (i & 7)))) {
            i++;
            continue;
        }
        uint32_t j = i;
        while (j < bits && (map[j >> 3] & (1u << (j & 7))))
            j++;
        fprintf(f, "%s%08X %08X\n", prefix, base + i * unit, base + j * unit);
        i = j;
    }
}

// One line per RAM episode: "ram <start> <end> <code bytes start..end> <ran: lo-hi,...>", where
// start..end spans what ran.
static void writeEpisodes(FILE *f) {
    if (sLostEpisodes)
        fprintf(f, "# %d RAM episodes beyond %d not recorded\n", sLostEpisodes, MAX_EP);
    for (int e = 0; e < sNumEpisodes; e++) {
        const struct Episode *ep = &sEpisodes[e];
        uint32_t first = EP_WIN, last = 0;
        for (uint32_t i = 0; i < EP_WIN / 2; i++)
            if (ep->exec[i >> 3] & (1u << (i & 7))) {
                if (first == EP_WIN)
                    first = i * 2;
                last = i * 2 + 2;
            }
        if (first == EP_WIN)
            continue;
        fprintf(f, "ram %08X %08X ", ep->base + first, ep->base + last);
        for (uint32_t i = first; i < last; i++)
            fprintf(f, "%02X", ep->code[i]);
        const char *sep = " ";
        for (uint32_t i = first / 2; i < last / 2;) {
            if (!(ep->exec[i >> 3] & (1u << (i & 7)))) {
                i++;
                continue;
            }
            uint32_t j = i;
            while (j < last / 2 && (ep->exec[j >> 3] & (1u << (j & 7))))
                j++;
            fprintf(f, "%s%08X-%08X", sep, ep->base + i * 2, ep->base + j * 2);
            sep = ",";
            i = j;
        }
        fputc('\n', f);
    }
}

static void writeTrace(const char *path, bool coverage) {
    FILE *f = fopen(path, "w");
    if (!f)
        die("cannot write %s: %s", path, strerror(errno));
    if (coverage) {
        writeRanges(f, "", sExecRom, ROM_SPAN / 2, 0x08000000u, 2);
        writeEpisodes(f);
    } else {
        writeRanges(f, "w ", sLoadRom, ROM_SPAN / 4, 0x08000000u, 4);
        writeRanges(f, "n ", sNarrowRom, ROM_SPAN, 0x08000000u, 1);
    }
    fclose(f);
}


// Steps both consoles (resets them if their reset flag is set).
static void stepAll(struct Emu *emus, int numRoms) {
    if (numRoms == 1) {
        emuStep(&emus[0]);
        return;
    }
    pthread_barrier_wait(&sFrameStart);
    emuStep(&emus[0]);
    pthread_barrier_wait(&sFrameDone);
}

static uint64_t readWatch(struct Emu *emu, const struct Watch *w) {
    switch (w->size) {
    case 1: return emu->core->rawRead8(emu->core, w->addr, -1);
    case 2: return emu->core->rawRead16(emu->core, w->addr, -1);
    case 4: return emu->core->rawRead32(emu->core, w->addr, -1);
    }
    return 0;
}

// FNV-1a over 32-bit pixels.
static uint64_t hashPixels(const color_t *p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static void writePpm(struct Emu *emu, const char *dir, const char *tag, long frame) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s_%s_frame%06ld.ppm", dir, tag, emu->label, frame);
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "kurutest: cannot write %s: %s\n", path, strerror(errno));
        return;
    }
    fprintf(f, "P6\n%u %u\n255\n", emu->width, emu->height);
    for (unsigned i = 0; i < emu->width * emu->height; i++) {
        uint32_t c = emu->video[i];  // mGBA's 32-bit color: 0x00BBGGRR
        uint8_t rgb[3] = { c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("  wrote %s\n", path);
}

static void loadMap(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f)
        die("cannot open %s", path);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned lo, hi;
        char delta[32];
        if (sscanf(line, "%x %x %31s", &lo, &hi, delta) != 3)
            continue;
        if (sNumMap == MAX_MAP)
            die("%s: too many entries", path);
        sMap[sNumMap].lo = lo;
        sMap[sNumMap].hi = hi;
        sMap[sNumMap].delta = (int32_t)strtol(delta, NULL, 16);
        sNumMap++;
    }
    fclose(f);
}

// ROM A's ROM address a as ROM B has it (bit 0, the Thumb bit, is kept); false if not mapped.
static bool mapRom(uint32_t a, uint32_t *out) {
    uint32_t key = a & ~1u;
    int lo = 0, hi = sNumMap - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (key < sMap[mid].lo)
            hi = mid - 1;
        else if (key >= sMap[mid].hi)
            lo = mid + 1;
        else {
            *out = a + (uint32_t)sMap[mid].delta;
            return true;
        }
    }
    return false;
}

// Equal, or ROM A's word is a ROM address that is ROM B's word after the move. A word at or
// above 0x08000000 that is not in the mapped ROM is ROM data's address plus an offset that
// overshoots (e.g. a base pointer plus an out-of-range table entry): equal if it differs by
// one of the map's deltas.
static bool sameWord(uint32_t va, uint32_t vb) {
    uint32_t m;
    if (va == vb)
        return true;
    if (!sNumMap)
        return false;
    if (mapRom(va, &m))
        return m == vb;
    if (va < 0x08000000u)
        return false;
    for (int i = 0; i < sNumMap; i++)
        if (sMap[i].delta && va + (uint32_t)sMap[i].delta == vb)
            return true;
    return false;
}

static int findWatch(const char *name) {
    for (int i = 0; i < sNumWatch; i++)
        if (strcmp(sWatch[i].name, name) == 0)
            return i;
    return -1;
}

static void loadWatch(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f)
        die("cannot open %s", path);
    char line[512];
    int lineNo = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        char *p = line + strspn(line, " \t");
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        if (sNumWatch == MAX_WATCH)
            die("%s: too many entries", path);
        struct Watch *w = &sWatch[sNumWatch];
        char flag[16] = "";
        int n = sscanf(p, "%" SCNx32 " %" SCNu32 " %63s %15s", &w->addr, &w->size, w->name, flag);
        if (n < 3 || w->size == 0 || (n == 4 && strcmp(flag, "finished") && flag[0] != '#'))
            die("%s:%d: expected '<hex address> <size> <name> [finished]'", path, lineNo);
        w->finished = !strcmp(flag, "finished");
        sNumWatch++;
    }
    fclose(f);
}

static void loadSaveImage(const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f)
        die("cannot open save image %s: %s", file, strerror(errno));
    sSave = malloc(0x20000 + 1);  // one byte more, to tell a 128 KiB image from a larger one
    sSaveSize = fread(sSave, 1, 0x20000 + 1, f);
    if (sSaveSize == 0 || sSaveSize > 0x20000 || ferror(f))
        die("%s: a save image must be 1 byte to 128 KiB", file);
    fclose(f);
}

static struct Item *loadInputs(const char *path, int *outCount, long *outFrames) {
    FILE *f = fopen(path, "r");
    if (!f)
        die("cannot open %s", path);
    int cap = 1024, n = 0;
    long frames = 0;
    struct Item *items = malloc(sizeof(*items) * cap);
    char line[4096];  // a save line holds an absolute path
    int lineNo = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        if (!strchr(line, '\n') && !feof(f))
            die("%s:%d: line longer than %zu characters", path, lineNo, sizeof(line) - 2);
        char *p = line + strspn(line, " \t");
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        if (n == cap)
            items = realloc(items, sizeof(*items) * (cap *= 2));
        struct Item *it = &items[n];
        memset(it, 0, sizeof(*it));
        it->line = lineNo;
        if (!strncmp(p, "save ", 5)) {
            // the file is the rest of the line (a path may hold spaces), without its end
            char *file = p + 5 + strspn(p + 5, " \t");
            size_t len = strcspn(file, "\r\n");
            while (len && (file[len - 1] == ' ' || file[len - 1] == '\t'))
                len--;
            file[len] = '\0';
            if (n != 0 || frames != 0 || sSave)
                die("%s:%d: 'save' must come before everything else", path, lineNo);
            if (!len)
                die("%s:%d: expected 'save <file>'", path, lineNo);
            loadSaveImage(file);
            continue;
        } else if (!strncmp(p, "reset", 5)) {
            it->kind = ITEM_RESET;
        } else if (*p == '!') {
            char name[64];
            long long value;
            it->kind = ITEM_EXPECT;
            if (sscanf(p + 1, "%63s %2s %lli", name, it->op, &value) != 3)
                die("%s:%d: expected '! <watch name> <op> <value>'", path, lineNo);
            it->value = value;
            if ((it->watch = findWatch(name)) < 0 || sWatch[it->watch].size > 4)
                die("%s:%d: '%s' is not a 1/2/4-byte watch entry", path, lineNo, name);
        } else {
            char *end;
            it->kind = ITEM_KEYS;
            it->keys = strtoul(p, &end, 16);
            it->count = 1;
            if (*end == '*')
                it->count = strtoul(end + 1, &end, 10);
            if (end == p || it->count == 0 || it->keys > 0x3FF)
                die("%s:%d: bad key line", path, lineNo);
            frames += it->count;
        }
        n++;
    }
    fclose(f);
    *outCount = n;
    *outFrames = frames;
    return items;
}

static bool checkOp(int64_t a, const char *op, int64_t b) {
    if (!strcmp(op, "==")) return a == b;
    if (!strcmp(op, "!=")) return a != b;
    if (!strcmp(op, "<")) return a < b;
    if (!strcmp(op, "<=")) return a <= b;
    if (!strcmp(op, ">")) return a > b;
    if (!strcmp(op, ">=")) return a >= b;
    die("unknown operator '%s'", op);
    return false;
}

// Compares every watch entry; prints the first maxReport differences. Returns their number.
static int compareWatches(struct Emu *a, struct Emu *b, long frame, int maxReport) {
    int diffs = 0;
    for (int i = 0; i < sNumWatch; i++) {
        const struct Watch *w = &sWatch[i];
        if (w->finished)
            continue;  // compareFinished
        if (w->size <= 4) {
            uint64_t va = readWatch(a, w), vb = readWatch(b, w);
            if (w->size == 4 && sameWord((uint32_t)va, (uint32_t)vb))
                continue;
            if (va != vb && diffs++ < maxReport)
                printf("  frame %ld: %s (0x%08X, %u bytes): %s=0x%0*llX %s=0x%0*llX\n", frame, w->name,
                       w->addr, w->size, a->label, (int)w->size * 2, (unsigned long long)va, b->label,
                       (int)w->size * 2, (unsigned long long)vb);
            continue;
        }
        for (uint32_t off = 0; off < w->size; off++) {
            uint32_t addr = w->addr + off;
            if (!(addr & 3) && off + 4 <= w->size &&
                sameWord(a->core->rawRead32(a->core, addr, -1), b->core->rawRead32(b->core, addr, -1))) {
                off += 3;
                continue;
            }
            uint32_t va = a->core->rawRead8(a->core, addr, -1);
            uint32_t vb = b->core->rawRead8(b->core, addr, -1);
            if (va != vb) {
                if (diffs++ < maxReport)
                    printf("  frame %ld: %s+0x%X (0x%08X, first differing byte of %u): %s=0x%02X %s=0x%02X\n",
                           frame, w->name, off, w->addr + off, w->size, a->label, va, b->label, vb);
                break;
            }
        }
    }
    return diffs;
}

// The watch entries marked "finished" (m4a's PCM buffer): compared for the frames whose work the
// original finished before the next VBlank. In a frame whose work runs past it (a level or a
// screen loading), the sample at that VBlank is half-way through the work: the mixer may be
// filling the buffer, a few bytes further on in one ROM (docs/audit.md). Such a frame is one
// after which the original's gMain.frameCounter has not changed by the next frame's sample
// (port/test/port_test.py's lag frames), so each frame's values are kept and compared one
// frame later, if the counter moved.
static uint8_t *sFinishedSnap[2];
static size_t sFinishedSize;
static bool sFinishedPending;
static uint32_t sFinishedCounter;
static long sFinishedFrame;
static char sFinishedReport[256];
static long sFinishedStats[2];  // frames compared, frames left out (the original's work ran past VBlank)

static const uint8_t *ramPointer(struct Emu *e, uint32_t addr, uint32_t size) {
    size_t blockSize = 0;
    int region = (int)(addr >> 24);
    const uint8_t *block = region == 2 || region == 3 ? e->core->getMemoryBlock(e->core, (size_t)region, &blockSize) : NULL;
    uint32_t off = addr & 0xFFFFFFu;
    if (!block || off + size > blockSize)
        die("watch entry at 0x%08X: a 'finished' entry must be in EWRAM or IWRAM", addr);
    return block + off;
}

static int compareFinished(struct Emu *a, struct Emu *b, long frame, bool report) {
    if (report) {
        if (sFinishedReport[0])
            printf("  frame %ld: %s\n", sFinishedFrame, sFinishedReport);
        return sFinishedReport[0] != 0;
    }
    static int counter = -2;
    if (counter == -2) {
        counter = findWatch("gMain.frameCounter");
        sFinishedSize = 0;
        for (int i = 0; i < sNumWatch; i++)
            if (sWatch[i].finished)
                sFinishedSize += sWatch[i].size;
        if (sFinishedSize && counter < 0)
            die("'finished' watch entries need the entry gMain.frameCounter");
        for (int r = 0; r < 2 && sFinishedSize; r++)
            sFinishedSnap[r] = malloc(sFinishedSize);
    }
    if (!sFinishedSize)
        return 0;
    uint32_t now = (uint32_t)readWatch(a, &sWatch[counter]);
    if (sFinishedPending)
        sFinishedStats[now != sFinishedCounter ? 0 : 1]++;
    if (sFinishedPending && now != sFinishedCounter) {
        size_t off = 0;
        for (int i = 0; i < sNumWatch; i++) {
            const struct Watch *w = &sWatch[i];
            if (!w->finished)
                continue;
            for (uint32_t k = 0; k < w->size; k++)
                if (sFinishedSnap[0][off + k] != sFinishedSnap[1][off + k]) {
                    snprintf(sFinishedReport, sizeof(sFinishedReport),
                             "%s+0x%X (0x%08X, first differing byte of %u): %s=0x%02X %s=0x%02X", w->name, k, w->addr + k,
                             w->size, a->label, sFinishedSnap[0][off + k], b->label, sFinishedSnap[1][off + k]);
                    return 1;
                }
            off += w->size;
        }
    }
    size_t off = 0;
    for (int i = 0; i < sNumWatch; i++) {
        const struct Watch *w = &sWatch[i];
        if (!w->finished)
            continue;
        memcpy(sFinishedSnap[0] + off, ramPointer(a, w->addr, w->size), w->size);
        memcpy(sFinishedSnap[1] + off, ramPointer(b, w->addr, w->size), w->size);
        off += w->size;
    }
    sFinishedPending = true;
    sFinishedCounter = now;
    sFinishedFrame = frame;
    return 0;
}

// The cartridge save memory (SRAM), as the game has written it so far.
static int compareSaves(struct Emu *a, struct Emu *b, long frame, bool report) {
    size_t sizeA = 0, sizeB = 0;
    const uint8_t *ra = a->core->getMemoryBlock(a->core, 0xE, &sizeA);  // REGION_CART_SRAM
    const uint8_t *rb = b->core->getMemoryBlock(b->core, 0xE, &sizeB);
    if (!ra) sizeA = 0;
    if (!rb) sizeB = 0;
    if (sizeA == sizeB && (sizeA == 0 || memcmp(ra, rb, sizeA) == 0))
        return 0;
    if (report) {
        if (sizeA != sizeB) {
            printf("  frame %ld: save data size: %s=%zu %s=%zu\n", frame, a->label, sizeA, b->label, sizeB);
        } else {
            size_t i = 0;
            while (ra[i] == rb[i])
                i++;
            printf("  frame %ld: save data (SRAM 0x%08zX, first differing byte): %s=0x%02X %s=0x%02X\n", frame,
                   0x0E000000 + i, a->label, ra[i], b->label, rb[i]);
        }
    }
    return 1;
}

static char sSoundReport[512];
static long sSoundStats[3];  // for the report: CPU sound writes compared, FIFO transfers left out at restarts, restarts

// FIFO f's two streams: equal words pair up and are dropped; at a restart mark in one stream,
// the other may hold at most one transfer more before its own mark (dropped, not compared);
// what is left waits for the next frame.
static void fifoDrop(struct Emu *e, int f, size_t n) {
    e->fifoCount[f] -= n;
    memmove(e->fifo[f], e->fifo[f] + n, e->fifoCount[f] * sizeof(uint32_t));
}
static int compareFifo(struct Emu *a, struct Emu *b, int f) {
    size_t i = 0, j = 0;  // a's and b's next token
    int result = 0;
    while (i < a->fifoCount[f] && j < b->fifoCount[f]) {
        uint32_t ta = a->fifo[f][i], tb = b->fifo[f][j];
        if (ta != FIFO_RESTART && tb != FIFO_RESTART) {
            if (ta != tb) {
                snprintf(sSoundReport, sizeof(sSoundReport),
                         "FIFO %c word %llu (counted from power-on or reset) differs: %s 0x%08X, %s 0x%08X", 'A' + f,
                         (unsigned long long)a->fifoCompared[f], a->label, ta, b->label, tb);
                result = 1;
                break;
            }
            i++, j++, a->fifoCompared[f]++, b->fifoCompared[f]++;
            continue;
        }
        if (ta == FIFO_RESTART && tb == FIFO_RESTART) {
            sSoundStats[2]++;
            i++, j++;
            continue;
        }
        // one is at its restart: the other's words up to its own restart, at most one transfer
        struct Emu *late = ta == FIFO_RESTART ? b : a;
        size_t k = late == a ? i : j, n = 0;
        while (k + n < late->fifoCount[f] && late->fifo[f][k + n] != FIFO_RESTART && n <= FIFO_TRANSFER)
            n++;
        if (n > FIFO_TRANSFER) {
            snprintf(sSoundReport, sizeof(sSoundReport),
                     "FIFO %c: at a restart of its DMA, %s has transferred %zu words more than %s (at most %d may differ)",
                     'A' + f, late->label, n, late == a ? b->label : a->label, FIFO_TRANSFER);
            result = 1;
            break;
        }
        if (k + n == late->fifoCount[f])
            break;  // its restart has not come yet
        if (n)
            sSoundStats[1]++;
        if (late == a)
            i += n, a->fifoCompared[f] += n;
        else
            j += n, b->fifoCompared[f] += n;
    }
    fifoDrop(a, f, i);
    fifoDrop(b, f, j);
    if (!result && (a->fifoCount[f] > FIFO_SLACK || b->fifoCount[f] > FIFO_SLACK)) {
        snprintf(sSoundReport, sizeof(sSoundReport), "FIFO %c: %s has written %zu words more than %s", 'A' + f,
                 a->fifoCount[f] > FIFO_SLACK ? a->label : b->label,
                 a->fifoCount[f] > FIFO_SLACK ? a->fifoCount[f] : b->fifoCount[f],
                 a->fifoCount[f] > FIFO_SLACK ? b->label : a->label);
        result = 1;
    }
    return result;
}

// The sound (see the top). Compares the frame's CPU writes, then the FIFO words both ROMs have
// written and drops them (so it runs once a frame; the report comes from what it found).
static int compareSound(struct Emu *a, struct Emu *b, long frame, bool report) {
    if (report) {
        if (sSoundReport[0])
            printf("  frame %ld: %s\n", frame, sSoundReport);
        return sSoundReport[0] != 0;
    }
    sSoundReport[0] = 0;
    int n = a->numSoundWrites, i = 0;
    if (n > MAX_SOUND_WRITES || b->numSoundWrites > MAX_SOUND_WRITES) {
        snprintf(sSoundReport, sizeof(sSoundReport), "more than %d sound register writes in the frame (%s %d, %s %d)",
                 MAX_SOUND_WRITES, a->label, a->numSoundWrites, b->label, b->numSoundWrites);
        return 1;
    }
    while (i < n && i < b->numSoundWrites && !memcmp(&a->soundWrites[i], &b->soundWrites[i], sizeof(struct SoundWrite)))
        i++;
    if (i < n || i < b->numSoundWrites) {
        const struct SoundWrite *wa = i < n ? &a->soundWrites[i] : NULL, *wb = i < b->numSoundWrites ? &b->soundWrites[i] : NULL;
        char ta[64] = "none", tb[64] = "none";
        if (wa)
            snprintf(ta, sizeof(ta), "0x%08X = 0x%0*X", wa->addr, wa->size * 2, wa->value);
        if (wb)
            snprintf(tb, sizeof(tb), "0x%08X = 0x%0*X", wb->addr, wb->size * 2, wb->value);
        snprintf(sSoundReport, sizeof(sSoundReport),
                 "sound register write %d of the frame differs (%s wrote %d, %s %d): %s %s, %s %s", i, a->label, n,
                 b->label, b->numSoundWrites, a->label, ta, b->label, tb);
        return 1;
    }
    sSoundStats[0] += n;
    for (int f = 0; f < 2; f++)
        if (compareFifo(a, b, f))
            return 1;
    return 0;
}

static int compareScreens(struct Emu *a, struct Emu *b, long frame, bool report) {
    size_t n = (size_t)a->width * a->height;
    if (hashPixels(a->video, n) == hashPixels(b->video, n))
        return 0;
    if (!report)
        return 1;
    size_t i = 0;
    while (i < n && a->video[i] == b->video[i])
        i++;
    printf("  frame %ld: framebuffer differs; first pixel (%zu,%zu): %s=0x%06X %s=0x%06X\n", frame,
           i % a->width, i / a->width, a->label, (unsigned)a->video[i] & 0xFFFFFF, b->label,
           (unsigned)b->video[i] & 0xFFFFFF);
    return 1;
}

static void usage(void) {
    fputs("usage: kurutest --watch FILE --inputs FILE [--name NAME] [--fb-every N] [--dump-dir DIR]\n"
          "                [--shot FRAME]... [--trace NAME[,NAME...]] [--coverage FILE] [--rom-loads FILE]\n"
          "                [--rom-map FILE] [--dump-save FILE]\n"
          "                ROM_A [ROM_B]\n"
          "  With one ROM it only runs it (to write inputs: --shot, --trace, '!' checks).\n",
          stderr);
    exit(2);
}

int main(int argc, char **argv) {
    const char *watchPath = NULL, *inputPath = NULL, *dumpDir = ".", *name = "test", *trace = NULL;
    const char *coveragePath = NULL, *loadsPath = NULL, *dumpSavePath = NULL;
    const char *roms[2] = { NULL, NULL };
    int numRoms = 0, fbEvery = 1, numShots = 0;
    long shots[MAX_SHOTS];
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        bool hasValue = i + 1 < argc;
        if (!strcmp(arg, "--watch") && hasValue) watchPath = argv[++i];
        else if (!strcmp(arg, "--inputs") && hasValue) inputPath = argv[++i];
        else if (!strcmp(arg, "--dump-dir") && hasValue) dumpDir = argv[++i];
        else if (!strcmp(arg, "--name") && hasValue) name = argv[++i];
        else if (!strcmp(arg, "--fb-every") && hasValue) fbEvery = atoi(argv[++i]);
        else if (!strcmp(arg, "--trace") && hasValue) trace = argv[++i];
        else if (!strcmp(arg, "--coverage") && hasValue) coveragePath = argv[++i];
        else if (!strcmp(arg, "--rom-loads") && hasValue) loadsPath = argv[++i];
        else if (!strcmp(arg, "--rom-map") && hasValue) loadMap(argv[++i]);
        else if (!strcmp(arg, "--dump-save") && hasValue) dumpSavePath = argv[++i];
        else if (!strcmp(arg, "--shot") && hasValue && numShots < MAX_SHOTS) shots[numShots++] = atol(argv[++i]);
        else if (arg[0] == '-') usage();
        else if (numRoms < 2) roms[numRoms++] = arg;
        else usage();
    }
    if (!watchPath || !inputPath || numRoms == 0 || fbEvery < 0)
        usage();

    mLogSetDefaultLogger(&sQuietLogger);
    loadWatch(watchPath);
    int numItems;
    long totalFrames;
    struct Item *items = loadInputs(inputPath, &numItems, &totalFrames);

    int traceIdx[16], numTrace = 0;
    if (trace) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s", trace);
        for (char *tok = strtok(buf, ","); tok && numTrace < 16; tok = strtok(NULL, ",")) {
            if ((traceIdx[numTrace] = findWatch(tok)) < 0 || sWatch[traceIdx[numTrace]].size > 4)
                die("--trace: '%s' is not a 1/2/4-byte watch entry", tok);
            numTrace++;
        }
    }

    struct Emu emus[2] = { 0 };
    pthread_t thread;
    // Screenshots need every frame rendered.
    int frameskip = numShots || fbEvery == 0 ? 0 : fbEvery - 1;
    emuInit(&emus[0], "baserom", roms[0], frameskip);
    soundInit(&emus[0], 0);
    if (coveragePath || loadsPath) {
        sTraceA = true;
        sTraceCpu = emus[0].core->cpu;
        traceInit(&emus[0]);
    }
    if (numRoms == 2) {
        emuInit(&emus[1], "rebuilt", roms[1], frameskip);
        soundInit(&emus[1], 1);
        pthread_barrier_init(&sFrameStart, NULL, 2);
        pthread_barrier_init(&sFrameDone, NULL, 2);
        pthread_create(&thread, NULL, emuThread, &emus[1]);
    }

    long frame = 0, sinceReset = 0;
    int failures = 0;
    for (int it = 0; it < numItems && !failures; it++) {
        const struct Item *item = &items[it];
        if (item->kind == ITEM_RESET) {
            emus[0].reset = emus[1].reset = true;
            stepAll(emus, numRoms);
            sFinishedPending = false;  // the frame before the reset is not compared
            sinceReset = 0;
            continue;
        }
        if (item->kind == ITEM_EXPECT) {
            int64_t v = (int64_t)readWatch(&emus[0], &sWatch[item->watch]);
            if (!checkOp(v, item->op, item->value)) {
                printf("FAIL %s: %s line %d: after frame %ld, %s is %lld, expected %s %lld (on %s: the input no "
                       "longer does what it says)\n",
                       name, inputPath, item->line, frame - 1, sWatch[item->watch].name, (long long)v, item->op,
                       (long long)item->value, emus[0].label);
                failures++;
            }
            continue;
        }
        for (uint32_t k = 0; k < item->count && !failures; k++, frame++, sinceReset++) {
            emus[0].keys = emus[1].keys = item->keys;
            stepAll(emus, numRoms);
            for (int s = 0; s < numShots; s++)
                if (shots[s] == frame)
                    writePpm(&emus[0], dumpDir, name, frame);
            if (numTrace) {
                printf("%6ld keys=%03X", frame, item->keys);
                for (int t = 0; t < numTrace; t++)
                    printf(" %s=%lld", sWatch[traceIdx[t]].name, (long long)readWatch(&emus[0], &sWatch[traceIdx[t]]));
                putchar('\n');
            }
            if (numRoms < 2)
                continue;
            bool checkScreen = fbEvery && sinceReset % fbEvery == 0;
            if (compareWatches(&emus[0], &emus[1], frame, 0) || compareSaves(&emus[0], &emus[1], frame, false) ||
                compareSound(&emus[0], &emus[1], frame, false) || compareFinished(&emus[0], &emus[1], frame, false) || (checkScreen && compareScreens(&emus[0], &emus[1], frame, false))) {
                printf("FAIL %s: first mismatch at frame %ld of %ld (%s line %d, keys 0x%03X):\n", name, frame,
                       totalFrames, inputPath, item->line, item->keys);
                compareWatches(&emus[0], &emus[1], frame, 8);
                compareSaves(&emus[0], &emus[1], frame, true);
                compareSound(&emus[0], &emus[1], frame, true);
                compareFinished(&emus[0], &emus[1], frame, true);
                if (checkScreen)
                    compareScreens(&emus[0], &emus[1], frame, true);
                writePpm(&emus[0], dumpDir, name, frame);
                writePpm(&emus[1], dumpDir, name, frame);
                failures++;
            }
        }
    }
    if (coveragePath)
        writeTrace(coveragePath, true);
    if (loadsPath)
        writeTrace(loadsPath, false);
    if (dumpSavePath) {
        size_t size = 0;
        const uint8_t *sram = emus[0].core->getMemoryBlock(emus[0].core, 0xE, &size);  // REGION_CART_SRAM
        FILE *f = fopen(dumpSavePath, "wb");
        if (!f || (sram && size && fwrite(sram, 1, size, f) != size) || fclose(f))
            die("cannot write %s", dumpSavePath);
    }
    if (!failures)
        printf("ok   %s: %ld frames%s\n", name, frame, numRoms == 2 ? " identical" : " run");
    if (!failures && numRoms == 2)
        printf("     sound: %ld register writes, %ld FIFO restarts (%ld transfers left out), PCM buffer on %ld frames "
               "(%ld left out)\n", sSoundStats[0], sSoundStats[2], sSoundStats[1], sFinishedStats[0], sFinishedStats[1]);
    if (numRoms == 2) {
        sQuit = true;
        pthread_barrier_wait(&sFrameStart);
        pthread_join(thread, NULL);
    }
    for (int e = 0; e < numRoms; e++)
        emus[e].core->deinit(emus[e].core);
    return failures ? 1 : 0;
}
