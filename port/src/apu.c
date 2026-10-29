// The GB-compatible sound channels (docs/port.md, "The sound output"): the two squares (channel
// 1 with its frequency sweep), the wave channel and the noise channel, with the hardware
// envelope and length counter, as the GBA runs them, in CPU cycles (16.78 MHz, four times the
// GB's clock). Only what the game uses (the sound survey): one wave bank, NR30 bit 5 never set.
//
// The registers are plain memory (port/src/memory.c), so the game's writes are not seen as
// they happen: Port_ApuSync reads them once a frame, after SoundMain's CgbSound wrote them, and
// applies them at the start of the frame's sound. A write to NRx4 is how a channel is
// (re)started (bit 7) or gets a new frequency; NRx4's bits 3-5 do nothing on the hardware and
// CgbSound always writes them 0, so after reading NRx4 the port sets them: when they read 0
// again, the game wrote the register (the same value or not). The trigger bit is cleared
// (it reads 0 on the hardware too).
#include "port.h"

#include <stdio.h>

#define CPU_HZ 16777216u
#define SEQ_PERIOD (CPU_HZ / 512)  // the frame sequencer: length 256 Hz, sweep 128 Hz, envelope 64 Hz
#define NRX4_MARK 0x38

#define IO8(ofs) (*(volatile uint8_t *)(uintptr_t)(IO_START + (ofs)))

struct Channel {
    bool on;
    uint32_t freq;     // 11 bits (the noise channel: unused)
    uint32_t timer;    // cycles to the next step of the waveform
    uint32_t pos;      // the waveform's step: duty 0-7, wave sample 0-31
    uint32_t volume;   // the envelope's 0-15
    uint32_t envTimer; // envelope ticks left to the next step
    uint32_t length;   // length counter
    // channel 1
    uint32_t shadow;
    uint32_t sweepTimer;
    bool sweepOn;
    // channel 4
    uint32_t lfsr;
};

static struct Channel sCh[4];
// The frame sequencer's phase: it ticks every SEQ_PERIOD cycles from power-on, which comes
// ~41 000 cycles before the first VBlank (mGBA, the tests' reference, starts the cartridge at
// line 126.7); the port's sound starts a frame before that VBlank (port/src/audio.c renders
// each frame at its end). Switching the sound on (SOUNDCNT_X bit 7) starts it at step 0; the
// port sees the switch later than mGBA made it (SoundInit, at power-on), so it starts at the
// step that puts the port's steps on mGBA's (port/test/refdump --psg-trace).
#define SEQ_FIRST ((280896 - 41000) % SEQ_PERIOD)
#define SEQ_START_STEP 2
static uint32_t sSeqTimer = SEQ_FIRST;
static uint32_t sSeqStep;
static bool sSoundOn;

// The registers of channel n (0-3): NRx1 duty/length, NRx2 envelope, NRx3, NRx4
static const uint32_t kNrx1[4] = { 0x62, 0x68, 0x72, 0x78 };
static const uint32_t kNrx2[4] = { 0x63, 0x69, 0x73, 0x79 };
static const uint32_t kNrx3[4] = { 0x64, 0x6C, 0x74, 0x7C };
static const uint32_t kNrx4[4] = { 0x65, 0x6D, 0x75, 0x7D };

static const uint8_t kDuty[4][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 }, // 12.5%
    { 1, 0, 0, 0, 0, 0, 0, 1 }, // 25%
    { 1, 0, 0, 0, 0, 1, 1, 1 }, // 50%
    { 0, 1, 1, 1, 1, 1, 1, 0 }, // 75%
};

static bool dacOn(int n) {
    return n == 2 ? (IO8(0x70) & 0x80) : (IO8(kNrx2[n]) & 0xF8);
}

// Cycles of one step of channel n's waveform
static uint32_t period(int n) {
    struct Channel *c = &sCh[n];
    if (n < 2)
        return (2048 - c->freq) * 16;  // 8 steps: 131072 / (2048 - f) Hz
    if (n == 2)
        return (2048 - c->freq) * 8;   // 32 samples: 65536 / (2048 - f) Hz
    uint32_t nr43 = IO8(0x7C);
    uint32_t r = nr43 & 7, s = nr43 >> 4;
    return (r ? r * 64 : 32) << s;     // 524288 / r / 2^(s+1) Hz, r = 0 as 0.5
}

// The sweep's next frequency; above 2047 the channel stops
static uint32_t sweepNext(void) {
    struct Channel *c = &sCh[0];
    uint32_t nr10 = IO8(0x60), delta = c->shadow >> (nr10 & 7);
    uint32_t f = (nr10 & 8) ? c->shadow - delta : c->shadow + delta;
    if (f > 2047)
        c->on = false;
    return f;
}

static void trigger(int n) {
    struct Channel *c = &sCh[n];
    c->on = dacOn(n);
    if (n == 2)
        c->length = 256 - IO8(0x72);
    else
        c->length = 64 - (IO8(kNrx1[n]) & 63);
    c->timer = period(n);
    if (n == 2)
        c->pos = 0;
    if (n != 2) {
        uint32_t nrx2 = IO8(kNrx2[n]);
        c->volume = nrx2 >> 4;
        c->envTimer = nrx2 & 7;
    }
    if (n == 3)
        c->lfsr = 0x7FFF;
    if (n == 0) {
        uint32_t nr10 = IO8(0x60);
        c->shadow = c->freq;
        c->sweepTimer = (nr10 >> 4) & 7 ? (nr10 >> 4) & 7 : 8;
        c->sweepOn = (nr10 & 0x70) || (nr10 & 7);
        if (nr10 & 7)
            sweepNext();
    }
}

void Port_ApuSync(void) {
    for (int n = 0; n < 4; n++) {
        uint8_t nrx4 = IO8(kNrx4[n]);
        if ((nrx4 & NRX4_MARK) != NRX4_MARK) {  // written since the last sync
            if (n < 3)
                sCh[n].freq = IO8(kNrx3[n]) | (nrx4 & 7) << 8;
            if (nrx4 & 0x80)
                trigger(n);
            IO8(kNrx4[n]) = (nrx4 & 0x47) | NRX4_MARK;
        }
        if (!dacOn(n))
            sCh[n].on = false;
    }
    bool on = IO8(0x84) & 0x80;  // SOUNDCNT_X: the sound on
    if (on && !sSoundOn)
        sSeqStep = SEQ_START_STEP;
    sSoundOn = on;
    if (!on)
        for (int n = 0; n < 4; n++)
            sCh[n].on = false;
}

// The frame sequencer's step (512 Hz)
static void sequencer(void) {
    if (!sSoundOn)
        return;
    uint32_t step = sSeqStep++ & 7;
    if (!(step & 1)) {  // length, 256 Hz
        for (int n = 0; n < 4; n++)
            if ((IO8(kNrx4[n]) & 0x40) && sCh[n].length && --sCh[n].length == 0)
                sCh[n].on = false;
    }
    if (step == 2 || step == 6) {  // sweep, 128 Hz
        struct Channel *c = &sCh[0];
        if (c->sweepTimer && --c->sweepTimer == 0) {
            uint32_t nr10 = IO8(0x60), time = (nr10 >> 4) & 7;
            c->sweepTimer = time ? time : 8;
            if (c->sweepOn && time) {
                uint32_t f = sweepNext();
                if (f <= 2047 && (nr10 & 7)) {
                    // the sweep sets the frequency register: a later NR14 write alone (CgbOscOff)
                    // keeps this low byte
                    c->freq = c->shadow = f;
                    IO8(0x64) = (uint8_t)f;
                    sweepNext();
                }
            }
        }
    }
    if (step == 7) {  // envelope, 64 Hz
        for (int n = 0; n < 4; n++) {
            if (n == 2)
                continue;
            struct Channel *c = &sCh[n];
            uint32_t nrx2 = IO8(kNrx2[n]), p = nrx2 & 7;
            if (!p || !c->envTimer || --c->envTimer)
                continue;
            c->envTimer = p;
            if ((nrx2 & 8) && c->volume < 15)
                c->volume++;
            else if (!(nrx2 & 8) && c->volume > 0)
                c->volume--;
        }
    }
}

// Channel n's output (0-15) now
static uint32_t level(int n) {
    struct Channel *c = &sCh[n];
    if (!c->on)
        return 0;
    switch (n) {
    case 0:
    case 1:
        return kDuty[IO8(kNrx1[n]) >> 6][c->pos] ? c->volume : 0;
    case 2: {
        uint32_t byte = IO8(0x90 + (c->pos >> 1));
        uint32_t s = (c->pos & 1) ? byte & 15 : byte >> 4;
        uint32_t nr32 = IO8(0x73);
        if (nr32 & 0x80)
            return (s * 3) >> 2;  // 75% (the GBA's NR32 bit 7)
        switch ((nr32 >> 5) & 3) {
        case 0: return 0;
        case 1: return s;
        case 2: return s >> 1;
        default: return s >> 2;
        }
    }
    default:
        return (~c->lfsr & 1) ? c->volume : 0;
    }
}

static void step(int n) {
    struct Channel *c = &sCh[n];
    if (n < 2) {
        c->pos = (c->pos + 1) & 7;
    } else if (n == 2) {
        c->pos = (c->pos + 1) & 31;
    } else {
        uint32_t x = (c->lfsr ^ (c->lfsr >> 1)) & 1;
        c->lfsr = (c->lfsr >> 1) | x << 14;
        if (IO8(0x7C) & 8)
            c->lfsr = (c->lfsr & ~0x40u) | x << 6;
    }
}

// Runs the channels for *cycles* cycles; sums[n]: channel n's output summed over each cycle
// (a box filter: the caller divides by the cycles)
void Port_ApuRun(uint32_t cycles, uint32_t sums[4]) {
    for (int n = 0; n < 4; n++)
        sums[n] = 0;
    while (cycles) {
        uint32_t chunk = cycles < sSeqTimer ? cycles : sSeqTimer;
        for (int n = 0; n < 4; n++) {
            struct Channel *c = &sCh[n];
            if (!c->on)
                continue;
            uint32_t left = chunk;
            while (left) {
                uint32_t dt = left < c->timer ? left : c->timer;
                sums[n] += level(n) * dt;
                left -= dt;
                c->timer -= dt;
                if (!c->timer) {
                    step(n);
                    c->timer = period(n);
                }
            }
        }
        cycles -= chunk;
        sSeqTimer -= chunk;
        if (!sSeqTimer) {
            sequencer();
            sSeqTimer = SEQ_PERIOD;
        }
    }
}

// --psg-trace (a debugging aid): the channels at the frame boundary, as port/test/refdump
// --psg-trace writes mGBA's: "<frame> on/volume/frequency" of channels 1-4 (channel 3: its
// NR32 volume code, channel 4: NR43's ratio), "step" and the frame sequencer's last step
void Port_ApuTrace(FILE *f, long frame) {
    fprintf(f, "%ld", frame);
    for (int n = 0; n < 4; n++) {
        uint32_t vol = n == 2 ? (IO8(0x73) >> 5) & 7 : sCh[n].volume;
        uint32_t fr = n == 3 ? IO8(0x7C) & 7 : sCh[n].freq;
        fprintf(f, " %d/%u/%u", sCh[n].on, vol, fr);
    }
    fprintf(f, " step %u\n", (sSeqStep - 1) & 7);
}

// The console's reset (port/src/main.c): every channel off
void Port_ApuReset(void) {
    for (int n = 0; n < 4; n++)
        sCh[n] = (struct Channel){ 0 };
    sSeqTimer = SEQ_FIRST;
    sSeqStep = 0;
    sSoundOn = false;
}
