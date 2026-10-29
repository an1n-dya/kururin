// The port test's reference (docs/port.md, "Headless mode and the port test"): runs the
// original ROM in mGBA on a kurutest input and writes the watched RAM after every frame, in
// the format port/kururin_pc --dump writes.
//
// With --timing FILE it also writes, for every frame, whether the VBlank that ends it requests
// its interrupt (DISPSTAT bit 3 at the sample, which mGBA takes at the start of VBlank) and
// whether m4a's VSync ignores it (gSoundInfo.ident) and whether it came while the mixer ran: the
// port test needs them for the original's lag frames (VBlanks before AgbMain enables the
// interrupt are not counted by the game; VBlanks while m4a's VSync is off do not count down its
// DMA period; one during the mixing counts after the mixer chose its buffer segment).
//
// With --video FILE it also writes a hash of the screen every --fb-every frames (FNV-1a over
// each pixel's 24-bit colour, as port/kururin_pc --video does), and with --shot FRAME (and
// --shot-dir DIR) the screen after that frame as DIR/ref_baserom_frameNNNNNN.ppm and the
// video state as DIR/ref_stateNNNNNN.bin: the pixel test.
//
// With --mid-frame FILE it writes, for every frame, what the port cannot reproduce because it
// draws a frame at once from the hardware as the frame's work left it (port/src/video.c): the
// writes the original made to VRAM, palette RAM, OAM and the video registers while lines were
// drawn (VCOUNT < 160; the HBlank DMA's own writes left out), and the HBlank DMA reading data
// that had changed by the end of the frame (the circle window's table being redrawn while it
// was displayed). One line each: "<frame> <what> <first line> <last line>".
//
// It is kurutest itself (tests/gameplay/kurutest.c, included unchanged, its main renamed):
// the same core setup (HLE BIOS, blank or `save` SRAM), the same input format and the same
// frame stepping, so the port is compared with exactly what `make test` runs.
#define main kurutest_main
#include "../../tests/gameplay/kurutest.c"
#undef main

// --- --audio: mGBA's own mixed output (the stream's postAudioFrame: each sample as GBAAudioSample
// mixes it, at the PWM rate SOUNDBIAS sets, before resampling), for comparing the port's sound
// output with (the samples, 16-bit left/right, into FILE; FILE.frames: after each frame, how
// many have been written). Output only: the emulation is the same with or without it.

static FILE *sAudio;
static long sAudioSamples;

static void postAudioFrame(struct mAVStream *stream, int16_t left, int16_t right) {
    (void)stream;
    int16_t lr[2] = { left, right };
    fwrite(lr, sizeof(lr), 1, sAudio);
    sAudioSamples++;
}

// --- --psg-writes: when in the frame the game writes the GB channels' registers (0x04000060-
// 0x0400008F, wave RAM excluded): for each frame, the line of the first and of the last such
// write, or nothing. The port applies them at the frame's end (port/src/apu.c).

static FILE *sPsgWrites;
static struct GBA *sPsgGba;
static int sPsgFirst = -1, sPsgLast = -1;
static void (*sOrigStore8)(struct ARMCore *, uint32_t, int8_t, int *);
static void (*sOrigStore16)(struct ARMCore *, uint32_t, int16_t, int *);
static void (*sOrigStore32)(struct ARMCore *, uint32_t, int32_t, int *);

static void psgWrite(uint32_t address) {
    if (address >= 0x04000060u && address < 0x04000090u) {
        // lines from the frame's start: the frame ends at line 160 (VBlank), so 0 is line 161
        int line = (sPsgGba->video.vcount + 228 - 161) % 228;
        if (sPsgFirst < 0)
            sPsgFirst = line;
        sPsgLast = line;
    }
}
static void psgStore8(struct ARMCore *c, uint32_t a, int8_t v, int *cy) { psgWrite(a); sOrigStore8(c, a, v, cy); }
static void psgStore16(struct ARMCore *c, uint32_t a, int16_t v, int *cy) { psgWrite(a); sOrigStore16(c, a, v, cy); }
static void psgStore32(struct ARMCore *c, uint32_t a, int32_t v, int *cy) { psgWrite(a); sOrigStore32(c, a, v, cy); }

// --- --mid-frame: hooks on mGBA's renderer (every video write goes through them) ------------

static struct GBA *sGba;
static FILE *sMid;
static long sMidFrame;
enum { MID_VRAM, MID_PALETTE, MID_OAM, MID_REGISTER, MID_DMA_SOURCE, MID_KINDS };
static const char *const kMidNames[MID_KINDS] = { "vram", "palette", "oam", "register", "hblank-dma-source" };
static int sMidFirst[MID_KINDS], sMidLast[MID_KINDS];
#define MAX_DMA_READS 256
static struct { uint32_t src; uint16_t value; int line; } sDmaReads[MAX_DMA_READS];
static int sNumDmaReads;
static uint16_t (*sOrigWriteReg)(struct GBAVideoRenderer *, uint32_t, uint16_t);
static void (*sOrigWriteVram)(struct GBAVideoRenderer *, uint32_t);
static void (*sOrigWritePal)(struct GBAVideoRenderer *, uint32_t, uint16_t);
static void (*sOrigWriteOam)(struct GBAVideoRenderer *, uint32_t);

static void midWrite(int kind) {
    int line = sGba->video.vcount;
    if (line >= 160)
        return;
    if (sMidFirst[kind] < 0)
        sMidFirst[kind] = line;
    sMidLast[kind] = line;
}

static uint16_t hookReg(struct GBAVideoRenderer *r, uint32_t address, uint16_t value) {
    if (sGba->video.vcount < 160) {
        if (sGba->performingDMA) {
            // an HBlank DMA write: remember what it read, and from where
            struct GBADMA *dma = &sGba->memory.dma[sGba->performingDMA >> 1];
            if (sNumDmaReads < MAX_DMA_READS && dma->nextSource >= 0x02000000 && dma->nextSource < 0x04000000)
                sDmaReads[sNumDmaReads++] = (typeof(sDmaReads[0])){ dma->nextSource, value, sGba->video.vcount };
        } else if (sGba->memory.io[address >> 1] != value) {
            midWrite(MID_REGISTER);
        }
    }
    return sOrigWriteReg(r, address, value);
}
static void hookVram(struct GBAVideoRenderer *r, uint32_t address) {
    midWrite(MID_VRAM);
    sOrigWriteVram(r, address);
}
static void hookPal(struct GBAVideoRenderer *r, uint32_t address, uint16_t value) {
    midWrite(MID_PALETTE);
    sOrigWritePal(r, address, value);
}
static void hookOam(struct GBAVideoRenderer *r, uint32_t oam) {
    midWrite(MID_OAM);
    sOrigWriteOam(r, oam);
}

static void midStart(void) {
    for (int k = 0; k < MID_KINDS; k++)
        sMidFirst[k] = -1;
    sNumDmaReads = 0;
}

// After a frame: what happened while its lines were drawn
static void midEnd(struct Emu *emu, long frame) {
    for (int i = 0; i < sNumDmaReads; i++) {
        if (emu->core->rawRead16(emu->core, sDmaReads[i].src, -1) != sDmaReads[i].value) {
            if (sMidFirst[MID_DMA_SOURCE] < 0)
                sMidFirst[MID_DMA_SOURCE] = sDmaReads[i].line + 1;
            sMidLast[MID_DMA_SOURCE] = sDmaReads[i].line + 1;
        }
    }
    for (int k = 0; k < MID_KINDS; k++)
        if (sMidFirst[k] >= 0)
            fprintf(sMid, "%ld %s %d %d\n", frame, kMidNames[k], sMidFirst[k], sMidLast[k]);
}

static uint64_t hashScreen(const struct Emu *emu) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned i = 0; i < emu->width * emu->height; i++) {
        h ^= (uint32_t)emu->video[i] & 0xFFFFFF;
        h *= 0x100000001b3ULL;
    }
    return h;
}

// With each --shot: DIR/ref_stateNNNNNN.bin, the video state at the end of the frame: the
// I/O registers as mGBA keeps them (0x400 bytes, write-only ones included), palette RAM, VRAM,
// OAM (the same layout as port/kururin_pc's port_stateNNNNNN.bin).
static void writeState(struct Emu *emu, const char *dir, long frame) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/ref_state%06ld.bin", dir, frame);
    FILE *f = fopen(path, "wb");
    if (!f)
        die("cannot write %s", path);
    struct GBA *gba = emu->core->board;
    fwrite(gba->memory.io, 1, 0x400, f);
    fwrite(gba->video.palette, 1, 0x400, f);
    fwrite(gba->video.vram, 1, 0x18000, f);
    fwrite(gba->video.oam.raw, 1, 0x400, f);
    fclose(f);
}

// --timing's third column: 1 when m4a's VSync ignores this VBlank (m4aSoundVSync runs only while
// gSoundInfo.ident is ID_NUMBER or ID_NUMBER + 1: not before SoundInit, nor between
// m4aSoundVSyncOff and m4aSoundVSyncOn, as during a level's loading)
static int soundVSyncOff(struct Emu *emu) {
    uint32_t ident = emu->core->rawRead32(emu->core, 0x03004880, -1);  // gSoundInfo.ident
    return ident - 0x68736D53u > 1;
}

// --timing's fourth column: 1 when the VBlank came while the mixer ran: SoundMain after it read
// pcmDmaCounter (0x0801A58A) to choose the buffer segment, or SoundMainRAM, the mixer's copy in
// IWRAM (SoundMainRAM_Buffer, 0x03000490-0x0300088F). Such a VBlank's m4aSoundVSync does not
// change the segment of the frame being mixed. (The CPU is at the instruction it executes next;
// its PC register is that address plus 4 in Thumb, plus 8 in ARM.)
static int inMixer(struct Emu *emu) {
    struct ARMCore *cpu = emu->core->cpu;
    uint32_t pc = (uint32_t)cpu->gprs[ARM_PC] - (cpu->executionMode == MODE_THUMB ? 4 : 8);
    return (pc > 0x0801A58Au && pc < 0x0801A5BCu) || (pc >= 0x03000490u && pc < 0x03000890u);
}

int main(int argc, char **argv) {
    const char *watchPath = NULL, *inputPath = NULL, *dumpPath = NULL, *timingPath = NULL, *rom = NULL;
    const char *videoPath = NULL, *shotDir = ".", *midPath = NULL, *audioPath = NULL, *audioOnly = NULL, *psgTracePath = NULL, *psgWritesPath = NULL;
    long shots[MAX_SHOTS];
    int numShots = 0, fbEvery = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--watch") && i + 1 < argc) watchPath = argv[++i];
        else if (!strcmp(argv[i], "--video") && i + 1 < argc) videoPath = argv[++i];
        else if (!strcmp(argv[i], "--fb-every") && i + 1 < argc) fbEvery = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc && numShots < MAX_SHOTS) shots[numShots++] = atol(argv[++i]);
        else if (!strcmp(argv[i], "--shot-dir") && i + 1 < argc) shotDir = argv[++i];
        else if (!strcmp(argv[i], "--mid-frame") && i + 1 < argc) midPath = argv[++i];
        else if (!strcmp(argv[i], "--audio") && i + 1 < argc) audioPath = argv[++i];
        else if (!strcmp(argv[i], "--audio-only") && i + 1 < argc) audioOnly = argv[++i];
        else if (!strcmp(argv[i], "--psg-trace") && i + 1 < argc) psgTracePath = argv[++i];
        else if (!strcmp(argv[i], "--psg-writes") && i + 1 < argc) psgWritesPath = argv[++i];
        else if (!strcmp(argv[i], "--inputs") && i + 1 < argc) inputPath = argv[++i];
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dumpPath = argv[++i];
        else if (!strcmp(argv[i], "--timing") && i + 1 < argc) timingPath = argv[++i];
        else if (argv[i][0] != '-' && !rom) rom = argv[i];
        else rom = NULL, i = argc;
    }
    if (!watchPath || !inputPath || !dumpPath || !rom || fbEvery < 1) {
        fputs("usage: refdump --watch FILE --inputs FILE.inp --dump FILE [--timing FILE]\n"
              "               [--video FILE] [--fb-every N] [--shot FRAME]... [--shot-dir DIR] [--mid-frame FILE]\n"
              "               [--audio FILE [--audio-only psg|ds]] [--psg-trace FILE] [--psg-writes FILE] ROM\n",
              stderr);
        return 2;
    }
    mLogSetDefaultLogger(&sQuietLogger);
    loadWatch(watchPath);
    int numItems;
    long totalFrames;
    struct Item *items = loadInputs(inputPath, &numItems, &totalFrames);
    struct Emu emu = { 0 };
    bool render = videoPath || numShots;
    emuInit(&emu, "baserom", rom, render ? 0 : 0x7FFFFFFF);  // every frame rendered, or none
    if (midPath) {
        if (!(sMid = fopen(midPath, "w")))
            die("cannot write %s", midPath);
        sGba = emu.core->board;
        struct GBAVideoRenderer *r = sGba->video.renderer;
        sOrigWriteReg = r->writeVideoRegister, r->writeVideoRegister = hookReg;
        sOrigWriteVram = r->writeVRAM, r->writeVRAM = hookVram;
        sOrigWritePal = r->writePalette, r->writePalette = hookPal;
        sOrigWriteOam = r->writeOAM, r->writeOAM = hookOam;
    }
    static struct mAVStream stream = { .postAudioFrame = postAudioFrame };
    FILE *audioFrames = NULL;
    if (audioPath) {
        char path[4096];
        snprintf(path, sizeof(path), "%s.frames", audioPath);
        if (!(sAudio = fopen(audioPath, "wb")) || !(audioFrames = fopen(path, "w")))
            die("cannot write %s", audioPath);
        emu.core->setAVStream(emu.core, &stream);
        // kurutest never sets mGBA's "volume" option, so its master volume is 0 and every
        // sample it mixes is 0 (make test's audio hashes compare silence); here at full volume
        struct GBAAudio *audio = &((struct GBA *)emu.core->board)->audio;
        audio->masterVolume = GBA_AUDIO_VOLUME_MAX;
        // --audio-only: the GB channels (psg) or DirectSound (ds) alone
        if (audioOnly && !strcmp(audioOnly, "psg"))
            audio->forceDisableChA = audio->forceDisableChB = true;
        if (audioOnly && !strcmp(audioOnly, "ds"))
            for (int c = 0; c < 4; c++)
                audio->psg.forceDisableCh[c] = true;
    }
    // --psg-trace: the GB channels after each frame, as port/kururin_pc --psg-trace writes them:
    // "<frame> on/volume/frequency of channels 1-4 step <the frame sequencer's last step>"
    FILE *psgTrace = psgTracePath ? fopen(psgTracePath, "w") : NULL;
    if (psgTracePath && !psgTrace)
        die("cannot write %s", psgTracePath);
    if (psgWritesPath) {
        if (!(sPsgWrites = fopen(psgWritesPath, "w")))
            die("cannot write %s", psgWritesPath);
        sPsgGba = emu.core->board;
        struct ARMCore *cpu = emu.core->cpu;
        sOrigStore8 = cpu->memory.store8, cpu->memory.store8 = psgStore8;
        sOrigStore16 = cpu->memory.store16, cpu->memory.store16 = psgStore16;
        sOrigStore32 = cpu->memory.store32, cpu->memory.store32 = psgStore32;
    }
    FILE *video = videoPath ? fopen(videoPath, "w") : NULL;
    if (videoPath && !video)
        die("cannot write %s", videoPath);
    FILE *out = fopen(dumpPath, "w");
    FILE *timing = timingPath ? fopen(timingPath, "w") : NULL;
    if (!out || (timingPath && !timing))
        die("cannot write %s", out ? timingPath : dumpPath);
    long frame = 0;
    for (int it = 0; it < numItems; it++) {
        const struct Item *item = &items[it];
        if (item->kind == ITEM_RESET) {  // as kurutest: the console restarts between two frames
            emu.reset = true;
            stepAll(&emu, 1);
            continue;
        }
        if (item->kind != ITEM_KEYS)
            continue;
        for (uint32_t k = 0; k < item->count; k++, frame++) {
            emu.keys = item->keys;
            if (sMid)
                midStart();
            stepAll(&emu, 1);
            if (sMid)
                midEnd(&emu, frame);
            fprintf(out, "%ld %03X", frame, item->keys);
            for (int w = 0; w < sNumWatch; w++) {
                fputc(' ', out);
                for (uint32_t b = 0; b < sWatch[w].size; b++)
                    fprintf(out, "%02X", emu.core->rawRead8(emu.core, sWatch[w].addr + b, -1));
            }
            fputc('\n', out);
            if (timing)
                fprintf(timing, "%ld %d %d %d\n", frame, (emu.core->rawRead16(emu.core, 0x04000004, -1) >> 3) & 1,
                        soundVSyncOff(&emu), inMixer(&emu));
            if (audioFrames)
                fprintf(audioFrames, "%ld %ld\n", frame, sAudioSamples);
            if (sPsgWrites) {
                if (sPsgFirst >= 0)
                    fprintf(sPsgWrites, "%ld %d %d\n", frame, sPsgFirst, sPsgLast);
                sPsgFirst = sPsgLast = -1;
            }
            if (psgTrace) {
                struct GBAudio *a = &((struct GBA *)emu.core->board)->audio.psg;
                fprintf(psgTrace, "%ld %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d step %d\n", frame, a->playingCh1,
                        a->ch1.envelope.currentVolume, a->ch1.control.frequency, a->playingCh2,
                        a->ch2.envelope.currentVolume, a->ch2.control.frequency, a->playingCh3, a->ch3.volume,
                        a->ch3.rate, a->playingCh4, a->ch4.envelope.currentVolume, a->ch4.ratio, a->frame);
            }
            if (video && frame % fbEvery == 0)
                fprintf(video, "%ld %016llx\n", frame, (unsigned long long)hashScreen(&emu));
            for (int s = 0; s < numShots; s++)
                if (shots[s] == frame) {
                    writePpm(&emu, shotDir, "ref", frame);
                    writeState(&emu, shotDir, frame);
                }
        }
    }
    fclose(out);
    if (timing)
        fclose(timing);
    if (video)
        fclose(video);
    if (sMid)
        fclose(sMid);
    if (sAudio)
        fclose(sAudio), fclose(audioFrames);
    if (sPsgWrites)
        fclose(sPsgWrites);
    emu.core->deinit(emu.core);
    return 0;
}
