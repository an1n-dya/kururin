// The sound output (docs/port.md, "The sound output"): what the GBA's sound hardware does with
// what m4a writes, into SDL audio (and a WAV file with --wav).
//
// Once a frame, at the frame boundary (before the VBlank interrupt, whose m4aSoundVSync may
// restart the sound DMAs), Port_AudioFrame makes the sound of the frame that ends there,
// 280 896 cycles of it, as the hardware played it while the game did that frame's work:
// - DirectSound: timer 0 (reload from TM0CNT_L: 1 254 cycles, 13 379 Hz) pops a byte from each
//   FIFO at every overflow; a FIFO holding 16 bytes or fewer asks its DMA for 16 more
//   (port/src/hw.c reads them from the PCM buffer m4a's mixer filled). The sample is held
//   until the next overflow.
// - the GB channels (port/src/apu.c), with the registers CgbSound wrote during the frame's
//   work taking effect 40 lines into the frame (PSG_WRITE_SAMPLE): the port runs the work at
//   once, so this is where the original typically wrote them.
// - mixed as the GBA mixes them (and as mGBA, the tests' reference, computes it): SOUNDCNT_H's
//   PSG volume, NR50/NR51, DirectSound A/B volume and sides, SOUNDBIAS's clamp to 10 bits.
// The output has 896 samples a frame (4 per DirectSound sample: 53 516 Hz, the frame rate
// times 896), each the average of the GB channels over its 313.5 cycles; a high-pass at 10 Hz
// (the output capacitor) removes the GB channels' DC.
//
// The frames go into a ring buffer that SDL's audio callback empties. The frame loop is the
// clock (59.73 Hz, port/src/frame.c); Port_AudioPacing nudges it (at most 0.2%) so the ring stays
// at its target fill, so the audio device's clock and the frame timer never drift apart.
// - Start: the device is started when it is opened, and the first frame waits until it is
//   running (Port_AudioInit): PulseAudio takes a buffer's worth at once, then nothing for a
//   second or two; the game's first second of sound was dropped there before. It plays once
//   the ring holds its target.
// - Underrun (the callback finds the ring short: the game ran late, or the device takes more
//   at once than the ring holds): it holds the last sample and plays again once the ring is
//   back at its target, one gap rather than many. The target is then raised to a frame more
//   than the most the device has taken at once (up to MAX_TARGET), so a device that asks in
//   large bursts (a long latency, as WSL's PulseAudio has) stops running short.
// - Overrun (the device stalled: the ring holds more than the target, a burst and 4 frames):
//   the oldest samples are dropped down to the target, so the sound does not lag the picture.
// Underruns and overruns are counted and logged (a line a second at most).
#include "port.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CYCLES_PER_FRAME 280896u
#define SAMPLES_PER_FRAME 896u
#define OUTPUT_RATE 53516  // 896 x 59.7275 Hz
#define RING_SIZE 32768                      // stereo samples (~0.6 s; a power of two)
#define START_TARGET (int)(SAMPLES_PER_FRAME * 2)  // ~33 ms
#define MAX_TARGET (int)(SAMPLES_PER_FRAME * 16)  // ~267 ms
#define START_WAIT_MS 3000  // at most this long for the device to ask for its first samples
// Where in the frame CgbSound's register writes take effect: the original writes them when
// m4aSoundMain runs, at the end of the frame's work: 24 to 51 lines (the median, depending on
// the input) after the VBlank that started the frame (port/test/refdump --psg-writes). 40 lines.
#define PSG_WRITE_SAMPLE (40 * 1232 * SAMPLES_PER_FRAME / CYCLES_PER_FRAME)

#define IO8(ofs) (*(volatile uint8_t *)(uintptr_t)(IO_START + (ofs)))

// --- DirectSound: the FIFOs ------------------------------------------------------------------

struct Fifo {
    uint8_t data[32];
    uint32_t read, count;
    int8_t sample;  // the one being played
};
static struct Fifo sFifo[2];
static int64_t sTimerNext;  // cycles from the frame's start to timer 0's next overflow

static void fifoPop(int n) {
    struct Fifo *f = &sFifo[n];
    if (f->count) {
        f->sample = (int8_t)f->data[f->read];
        f->read = (f->read + 1) & 31;
        f->count--;
    }
    if (f->count <= 16) {
        uint8_t in[16];
        if (Port_SoundDmaRequest((uint32_t)n, in))
            for (int i = 0; i < 16; i++, f->count++)
                f->data[(f->read + f->count) & 31] = in[i];
    }
}

// SOUNDCNT_H bits 11 and 15 empty FIFO A and B (they read 0 afterwards)
static void fifoResets(void) {
    uint16_t cnt = IO16(0x82);
    for (int n = 0; n < 2; n++)
        if (cnt & (0x0800 << (4 * n)))
            sFifo[n].count = 0, sFifo[n].read = 0;
    IO16(0x82) = cnt & ~0x8800;
}

// --- the output: ring buffer, SDL, WAV -------------------------------------------------------

static int16_t sRing[RING_SIZE][2];
static SDL_atomic_t sRingRead, sRingWrite;  // free-running sample counts
static SDL_atomic_t sUnderrun, sOverrun;    // samples short / dropped since the last log
static SDL_atomic_t sPlaying;               // the callback plays from the ring (else waits for the target)
static SDL_atomic_t sRunning;               // the device asks for samples as it plays (not just its first fill)
static SDL_atomic_t sBurst;                 // the most samples the device has asked for at once
static int16_t sLast[2];                    // the callback's last sample (held while not playing)
static int sTarget = START_TARGET;          // the fill the ring is kept at
static bool sPlayedOnce;                    // (the producer's) the callback has played from the ring
static bool sTargetRaised;                  // since the callback last ran short
static SDL_AudioDeviceID sDevice;
static int sVolume = 100;  // percent
static bool sNoPsg, sNoDs;  // --sound-only ds / psg
static FILE *sWav;
static uint32_t sWavSamples;
static double sHpPrevIn[2], sHpPrevOut[2], sHpR;
static double sFillAverage = START_TARGET;
static FILE *sPsgTrace;  // --psg-trace
static long sAudioFrames;

static void callback(void *user, Uint8 *stream, int len) {
    (void)user;
    int16_t (*out)[2] = (int16_t (*)[2])stream;
    int want = len / 4, n = 0;
    // A burst: callbacks less than 2 ms apart. The device's first one fills its buffer; it is
    // running once a callback comes later.
    static Uint32 first, last;
    static int burst;
    static bool called;
    Uint32 now = SDL_GetTicks();
    if (!called)
        first = now, called = true;
    else if (now - first >= 5)
        SDL_AtomicSet(&sRunning, 1);
    burst = now - last < 2 ? burst + want : want;
    last = now;
    if (burst > SDL_AtomicGet(&sBurst))
        SDL_AtomicSet(&sBurst, burst);
    if (SDL_AtomicGet(&sPlaying)) {
        int have = SDL_AtomicGet(&sRingWrite) - SDL_AtomicGet(&sRingRead);
        n = have < want ? have : want;
        uint32_t r = (uint32_t)SDL_AtomicGet(&sRingRead);
        for (int i = 0; i < n; i++, r++) {
            out[i][0] = sRing[r % RING_SIZE][0];
            out[i][1] = sRing[r % RING_SIZE][1];
        }
        if (n)
            memcpy(sLast, out[n - 1], sizeof(sLast));
        SDL_AtomicSet(&sRingRead, (int)r);
        if (n < want) {  // short: wait for the target again
            SDL_AtomicAdd(&sUnderrun, want - n);
            SDL_AtomicSet(&sPlaying, 0);
        }
    }
    for (int i = n; i < want; i++)
        memcpy(out[i], sLast, sizeof(sLast));
}

static void wavHeader(void) {
    uint32_t data = sWavSamples * 4;
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    uint32_t v = 36 + data;
    memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16, memcpy(h + 16, &v, 4);
    uint16_t s = 1;  // PCM
    memcpy(h + 20, &s, 2);
    s = 2, memcpy(h + 22, &s, 2);
    v = OUTPUT_RATE, memcpy(h + 24, &v, 4);
    v = OUTPUT_RATE * 4, memcpy(h + 28, &v, 4);
    s = 4, memcpy(h + 32, &s, 2);
    s = 16, memcpy(h + 34, &s, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data, 4);
    fseek(sWav, 0, SEEK_SET);
    fwrite(h, 1, sizeof(h), sWav);
    fseek(sWav, 0, SEEK_END);
}

void Port_AudioInit(bool device, int volume, const char *wavPath, const char *only, const char *psgTrace) {
    if (psgTrace && !(sPsgTrace = fopen(psgTrace, "w")))
        Port_Die("cannot write %s", psgTrace);
    sVolume = volume;
    sNoPsg = only && !strcmp(only, "ds");
    sNoDs = only && !strcmp(only, "psg");
    sHpR = 1.0 - 2.0 * M_PI * 10.0 / OUTPUT_RATE;
    if (wavPath) {
        if (!(sWav = fopen(wavPath, "wb")))
            Port_Die("cannot write %s", wavPath);
        wavHeader();
    }
    if (!device)
        return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "kururin_pc: no sound: %s\n", SDL_GetError());
        return;
    }
    SDL_AudioSpec want = { 0 }, have;
    want.freq = OUTPUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;  // ~10 ms a callback
    want.callback = callback;
    sDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);  // SDL converts to the device's rate
    if (!sDevice) {
        fprintf(stderr, "kururin_pc: no sound: %s\n", SDL_GetError());
        return;
    }
    // Started now (silence until the ring holds its target); the game's first frame waits until
    // the device is running, so the start of its sound is not lost to the device's own start-up
    SDL_PauseAudioDevice(sDevice, 0);
    for (Uint32 t0 = SDL_GetTicks(); !SDL_AtomicGet(&sRunning) && SDL_GetTicks() - t0 < START_WAIT_MS;)
        SDL_Delay(5);
}

bool Port_AudioActive(void) {
    return sDevice || sWav || sPsgTrace;
}

static void push(const int16_t (*frame)[2], uint32_t n) {
    if (sWav) {
        fwrite(frame, 4, n, sWav);
        sWavSamples += n;
    }
    if (!sDevice)
        return;
    uint32_t w = (uint32_t)SDL_AtomicGet(&sRingWrite), r = (uint32_t)SDL_AtomicGet(&sRingRead);
    uint32_t maxLag = (uint32_t)(sTarget + SDL_AtomicGet(&sBurst)) + 4 * SAMPLES_PER_FRAME;
    if (maxLag > RING_SIZE)
        maxLag = RING_SIZE;
    if (w - r + n > maxLag) {
        // the device has stalled: the oldest samples go, down to the target (the callback,
        // which moves the read position too, is held off meanwhile)
        SDL_LockAudioDevice(sDevice);
        r = (uint32_t)SDL_AtomicGet(&sRingRead);
        uint32_t keep = (uint32_t)sTarget > n ? (uint32_t)sTarget - n : 0;
        if (w - r > keep) {
            SDL_AtomicAdd(&sOverrun, (int)(w - r - keep));
            SDL_AtomicSet(&sRingRead, (int)(w - keep));
        }
        SDL_UnlockAudioDevice(sDevice);
    }
    for (uint32_t i = 0; i < n; i++, w++)
        memcpy(sRing[w % RING_SIZE], frame[i], 4);
    SDL_AtomicSet(&sRingWrite, (int)w);
    if (!SDL_AtomicGet(&sPlaying)) {
        if (sPlayedOnce && !sTargetRaised) {  // it ran short: room for the device's bursts
            int burst = SDL_AtomicGet(&sBurst) + (int)SAMPLES_PER_FRAME;
            sTarget = sTarget + (int)SAMPLES_PER_FRAME > burst ? sTarget + (int)SAMPLES_PER_FRAME : burst;
            if (sTarget > MAX_TARGET)
                sTarget = MAX_TARGET;
            sTargetRaised = true;
        }
        if (w - (uint32_t)SDL_AtomicGet(&sRingRead) >= (uint32_t)sTarget) {
            SDL_AtomicSet(&sPlaying, 1);
            sPlayedOnce = true;
            sTargetRaised = false;
        }
    }
}

// --- a frame -----------------------------------------------------------------------------------

static int16_t clampOut(double v) {
    return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)lrint(v);
}

void Port_AudioFrame(void) {
    if (!Port_AudioActive())
        return;
    static int16_t out[SAMPLES_PER_FRAME][2];
    fifoResets();

    uint16_t cntH = IO16(0x82), bias = IO16(0x88) & 0x3FE;
    uint8_t nr50 = IO8(0x80), nr51 = IO8(0x81);
    bool timerOn = IO16(0x102) & 0x80;
    uint32_t timerPeriod = 0x10000u - IO16(0x100);
    int psgShift = 4 - (cntH & 3);  // 25%, 50%, 100% (3: prohibited)
    if (psgShift < 2)
        psgShift = 2;

    for (uint32_t j = 0; j < SAMPLES_PER_FRAME; j++) {
        uint32_t t0 = (uint32_t)((uint64_t)j * CYCLES_PER_FRAME / SAMPLES_PER_FRAME);
        uint32_t t1 = (uint32_t)((uint64_t)(j + 1) * CYCLES_PER_FRAME / SAMPLES_PER_FRAME);
        if (j == PSG_WRITE_SAMPLE)
            Port_ApuSync();  // CgbSound's writes of this frame
        while (timerOn && sTimerNext <= (int64_t)t0) {
            fifoPop(0);
            fifoPop(1);
            sTimerNext += timerPeriod;
        }
        uint32_t sums[4];
        Port_ApuRun(t1 - t0, sums);
        // the GB channels, each 0-15 (averaged), to their sides (NR51), times NR50's volume + 1,
        // times 8, then the PSG volume (mGBA: << 3, >> (4 - volume))
        double side[2] = { 0, 0 };  // right, left
        for (int n = 0; n < 4 && !sNoPsg; n++) {
            double v = (double)sums[n] / (t1 - t0);
            if (nr51 & (1 << n))
                side[0] += v;
            if (nr51 & (0x10 << n))
                side[1] += v;
        }
        side[0] = side[0] * 8 * (1 + (nr50 & 7)) / (1 << psgShift);
        side[1] = side[1] * 8 * (1 + ((nr50 >> 4) & 7)) / (1 << psgShift);
        // DirectSound: x4 at 100%, x2 at 50%
        for (int n = 0; n < 2 && !sNoDs; n++) {
            int ds = sFifo[n].sample * ((cntH & (4 << n)) ? 4 : 2);
            if (cntH & (0x100 << (4 * n)))
                side[0] += ds;
            if (cntH & (0x200 << (4 * n)))
                side[1] += ds;
        }
        for (int s = 0; s < 2; s++) {
            // SOUNDBIAS: the 10-bit DAC range; then mGBA's output scale (x48)
            double v = side[s] + bias;
            v = (v < 0 ? 0 : v > 0x3FF ? 0x3FF : v) - bias;
            v *= 48;
            double y = v - sHpPrevIn[s] + sHpR * sHpPrevOut[s];
            sHpPrevIn[s] = v, sHpPrevOut[s] = y;
            out[j][1 - s] = clampOut(y * sVolume / 100);  // the output is left, right
        }
    }
    sTimerNext = timerOn ? sTimerNext - CYCLES_PER_FRAME : 0;
    if (sPsgTrace)
        Port_ApuTrace(sPsgTrace, sAudioFrames);
    sAudioFrames++;
    push((const int16_t (*)[2])out, SAMPLES_PER_FRAME);
}

// How much later (in performance counter ticks, negative: earlier) the next frame should
// come, so the ring stays at its target fill: proportional to how far it is off (averaged over
// about a second), at most 0.2% of a frame. Also logs underruns and overruns (a line a second).
int64_t Port_AudioPacing(uint64_t ticksPerFrame) {
    if (!sDevice || !sPlayedOnce)
        return 0;
    // at most a line a second
    static long frames, under, over;
    under += SDL_AtomicSet(&sUnderrun, 0);
    over += SDL_AtomicSet(&sOverrun, 0);
    if (++frames >= 60) {
        if (under)
            fprintf(stderr, "kururin_pc: sound underrun: %ld samples short in %ld frames (now buffering %d ms)\n",
                    under, frames, sTarget * 1000 / OUTPUT_RATE);
        if (over)
            fprintf(stderr, "kururin_pc: sound overrun: %ld samples dropped in %ld frames\n", over, frames);
        frames = under = over = 0;
    }
    int fill = SDL_AtomicGet(&sRingWrite) - SDL_AtomicGet(&sRingRead);
    sFillAverage += (fill - sFillAverage) / 64;
    double off = (sFillAverage - sTarget) / SAMPLES_PER_FRAME;  // in frames
    double adjust = off * 0.01;
    if (adjust > 0.002)
        adjust = 0.002;
    if (adjust < -0.002)
        adjust = -0.002;
    return (int64_t)(adjust * (double)ticksPerFrame);
}

// The window lost the focus (frame.c): the device stops and plays on from where it was
void Port_AudioPause(bool pause) {
    if (sDevice)
        SDL_PauseAudioDevice(sDevice, pause);
}

// At reset: the FIFOs empty, the GB channels off; at exit: the WAV file's header
void Port_AudioReset(void) {
    memset(sFifo, 0, sizeof(sFifo));
    sTimerNext = 0;
    Port_ApuReset();
}

void Port_AudioClose(void) {
    if (sDevice)
        SDL_CloseAudioDevice(sDevice);
    sDevice = 0;
    if (sWav) {
        wavHeader();
        fclose(sWav);
        sWav = NULL;
    }
    if (sPsgTrace)
        fclose(sPsgTrace);
    sPsgTrace = NULL;
}
