// m4a_1.s in C (docs/port.md, "The sound"): MusicPlayer2000's hand-written part, transcribed
// from this ROM's asm (asm/m4a_1.s, 0x0801A528-0x0801B11C), an older revision than the one
// pret's projects reconstruct: the mixer interpolates every resampled channel, has no
// VCOUNT-limited "fast" path for this game (maxLines 0), and the sequencer's commands differ in
// details (ply_rept, the LFO). Each function does what its asm does, step by step, with the
// same arithmetic (widths, signedness, the order of reads and writes), so the RAM the original
// fills (gSoundInfo, its pcmBuffer, the players and their tracks) is filled the same way:
// `make -C port test-audio` compares it frame by frame.
//
// Names: the routines the ROM's renames name keep them (kururin.toml); the others keep their
// sub_ names, which the port's tables use (port/tools/gen_port_tables.py), with pret's name of
// the routine at that place of m4a's jump table in a comment.
#include "port.h"

#include <string.h>

#include "../../include/m4a.h"

// src/m4a.c (no prototype in m4a.h)
u32 MidiKeyToFreq(struct WaveData *wav, u8 key, u8 fineAdjust);

// gClockTable (pret): the note and wait lengths of commands 0x80-0xB0 and 0xCF-0xFF
extern const u8 gUnk_0801F548[];
// The jump table's template (36 code pointers, patched to host functions at load)
extern const u32 gUnk_0801F2D4[36];

#define VCOUNT (*(volatile u8 *)(uintptr_t)(IO_START + 0x006))

// umul3232H32 (0x0801A528, ARM umull): the high word of a 32x32-bit unsigned product
u32 umul3232H32(u32 a, u32 b) {
    return (u32)(((u64)a * b) >> 32);
}

// --- the mixer ---------------------------------------------------------------------------------

// The mixer works on the buffer four samples at a time: a word of each half (right at +0, left
// at +PCM_DMA_BUF_SIZE) is loaded, each sample is added into its lowest byte and the word
// rotated right by 8 (`mul; bic #0xFF0000; add rW, r1, rW, ror #8`), and after four samples
// the word is back in place and stored. A byte's addition wraps (the carry leaves the word).
static inline u32 ror32(u32 v, u32 n) {
    n &= 31;
    return n ? (v >> n) | (v << (32 - n)) : v;
}

struct MixWords {
    u8 *out;   // the words' address in the right half (r5 without its top bits)
    u32 sub;   // samples of the word done (r5's top two bits)
    u32 right; // r6
    u32 left;  // r7
    u32 volR;  // sl: envelopeVolumeRight << 16
    u32 volL;  // fp: envelopeVolumeLeft << 16
};

static inline void mixLoad(struct MixWords *w) {
    memcpy(&w->right, w->out, 4);
    memcpy(&w->left, w->out + PCM_DMA_BUF_SIZE, 4);
}

static inline void mixSample(struct MixWords *w, s32 sample) {
    w->right = ((w->volR * (u32)sample) & ~0xFF0000u) + ror32(w->right, 8);
    w->left = ((w->volL * (u32)sample) & ~0xFF0000u) + ror32(w->left, 8);
}

static inline void mixStore(struct MixWords *w) {
    memcpy(w->out + PCM_DMA_BUF_SIZE, &w->left, 4);
    memcpy(w->out, &w->right, 4);
    w->out += 4;
}

// `adds r5, r5, #0x40000000; blo`: true while the word has samples left
static inline bool mixNextInWord(struct MixWords *w) {
    if (++w->sub < 4)
        return true;
    w->sub = 0;
    return false;
}

// A channel that ends inside a word: the word is rotated back into place (by the samples it
// still had to go) and stored.
static inline void mixStorePartial(struct MixWords *w) {
    u32 shift = (3 - w->sub) * 8;
    w->right = ror32(w->right, shift);
    w->left = ror32(w->left, shift);
    w->sub = 0;
    mixStore(w);
}

// One DirectSound channel's samples into the frame's segment (the ARM code at 0x0801A754).
// out: the segment; samples: pcmSamplesPerVBlank (r8); loopStart / loopLength ([sp+0xC],
// [sp+0x10]): the looped part, loopLength 0 for a sample that ends.
static void mixChannel(struct SoundChannel *chan, u8 *out, s32 samples, s32 divFreq, s8 *loopStart,
                       s32 loopLength) {
    struct MixWords w = { out, 0, 0, 0, (u32)chan->envelopeVolumeRight << 16, (u32)chan->envelopeVolumeLeft << 16 };
    s32 count = (s32)chan->count;  // r2
    s8 *cur = chan->currentPointer; // r3
    s32 remaining = samples;        // r8

    if (chan->type & TONEDATA_TYPE_FIX) {
        // A fixed-rate sample: one of its samples per output sample.
        s32 extra; // lr: what is left to mix after the whole words
        for (;;) {
            if (count <= 4)
                goto fixedOneByOne;
            count -= remaining;
            if (count > 0) {
                extra = 0;
            } else {
                // the sample ends within the frame: whole words up to its last four
                // samples, then one by one
                extra = remaining;
                count += remaining;
                remaining = count - 4;
                extra -= remaining;
                count &= 3;
                if (count == 0)
                    count = 4;
            }
            do {
                mixLoad(&w);
                do
                    mixSample(&w, *cur++);
                while (mixNextInWord(&w));
                mixStore(&w);
                remaining -= 4;
            } while (remaining > 0);
            remaining += extra;
            if (remaining == 0)
                goto done;
        fixedOneByOne:
            mixLoad(&w);
            for (;;) {
                mixSample(&w, *cur++);
                if (--count == 0) {
                    count = loopLength;
                    if (count == 0)
                        goto ended;
                    cur = loopStart;
                }
                if (!mixNextInWord(&w))
                    break;
            }
            mixStore(&w);
            remaining -= 4;
            if (remaining <= 0)
                goto done;
        }
    } else {
        // A resampled sample: linear interpolation between the sample at cur - 1 (s0) and the
        // one at cur, by fw, a 23-bit fraction stepped by divFreq * frequency.
        u32 fw = chan->fw;                          // lr
        u32 step = (u32)divFreq * chan->frequency;  // r4
        s32 s0 = cur[0];
        s32 d = cur[1] - s0;
        cur++;
        do {
            mixLoad(&w);
            do {
                mixSample(&w, s0 + ((s32)(fw * (u32)d) >> 23));
                fw += step;
                u32 advance = fw >> 23;  // sb
                if (advance) {
                    fw &= ~0x3F800000u;
                    count -= (s32)advance;
                    if (count <= 0) {
                        if (loopLength == 0) {
                            count = 0;
                            goto ended;
                        }
                        // past the end: back into the loop by as much as it went past
                        cur = loopStart;
                        advance = (u32)-count;
                        while ((count += loopLength) <= 0)
                            advance -= (u32)loopLength;
                        cur += (s32)advance;
                        s0 = *cur;
                    } else if (--advance == 0) {
                        s0 += d;
                    } else {
                        cur += (s32)advance;
                        s0 = *cur;
                    }
                    cur++;
                    d = *cur - s0;
                }
            } while (mixNextInWord(&w));
            mixStore(&w);
            remaining -= 4;
        } while (remaining > 0);
        cur--;
        chan->fw = fw;
    }
done:
    chan->count = (u32)count;
    chan->currentPointer = cur;
    return;
ended:
    // the sample ended: the channel stops, its last word stored where it is
    chan->statusFlags = (u8)count;
    mixStorePartial(&w);
}

// SoundMainRAM (0x0801A5BC, run from its copy in IWRAM): mixes the frame's segment of the
// buffer. soundInfo: r0; out: the segment (r5, [sp+8]); dmaCounter: pcmDmaCounter as SoundMain
// read it (r4); lineLimit: [sp+0x14].
static void soundMainRAM(struct SoundInfo *soundInfo, s8 *out, u32 dmaCounter, u32 lineLimit) {
    s32 samples = soundInfo->pcmSamplesPerVBlank;  // r8
    u32 reverb = soundInfo->reverb;

    if (reverb) {
        // Reverb: the segment mixed with the next one (the oldest), both halves summed, at
        // reverb / 512, rounded up when negative-ish (bit 7 of the result)
        s8 *p = out;
        s8 *q = dmaCounter == 2 ? soundInfo->pcmBuffer : out + samples;
        s32 n = samples;
        do {
            s32 sum = p[PCM_DMA_BUF_SIZE] + p[0] + q[PCM_DMA_BUF_SIZE] + q[0];
            s32 r = (s32)((u32)sum * reverb) >> 9;
            if (r & 0x80)
                r++;
            p[PCM_DMA_BUF_SIZE] = (s8)r;
            *p++ = (s8)r;
            q++;
        } while (--n > 0);
    } else {
        // Silence: whole words of both halves, 4 and 8 bytes for bits 2 and 3 of the count,
        // then 16 at a time (at least once)
        u32 *a = (u32 *)out, *b = (u32 *)(out + PCM_DMA_BUF_SIZE);
        u32 n = (u32)samples;
        if (n & 4)
            *a++ = 0, *b++ = 0;
        if (n & 8) {
            *a++ = 0, *b++ = 0;
            *a++ = 0, *b++ = 0;
        }
        s32 blocks = (s32)(n >> 4);
        do {
            for (int i = 0; i < 4; i++)
                *a++ = 0, *b++ = 0;
        } while (--blocks > 0);
    }

    s32 divFreq = soundInfo->divFreq;  // r12
    struct SoundChannel *chan = soundInfo->chans;
    s32 left = soundInfo->maxChans;  // [sp+4]
    do {
        struct WaveData *wav = chan->wav;  // r3
        if (lineLimit) {
            u32 line = VCOUNT;
            if (line < 160)
                line += 228;
            if (line >= lineLimit)
                return;
        }
        u32 status = chan->statusFlags;  // r6
        u32 env;                         // r5
        if (!(status & SOUND_CHANNEL_SF_ON))
            goto nextChannel;
        if (status & SOUND_CHANNEL_SF_START) {
            if (status & SOUND_CHANNEL_SF_STOP) {
                chan->statusFlags = 0;
                goto nextChannel;
            }
            status = SOUND_CHANNEL_SF_ENV_ATTACK;
            chan->statusFlags = status;
            chan->currentPointer = wav->data;
            chan->count = wav->size;
            env = 0;
            chan->envelopeVolume = 0;
            chan->fw = 0;
            if (((const u8 *)wav)[3] & 0xC0) {  // the high byte of wav->status: looped
                status |= SOUND_CHANNEL_SF_LOOP;
                chan->statusFlags = status;
            }
            goto attack;
        }
        env = chan->envelopeVolume;
        if (status & SOUND_CHANNEL_SF_IEC) {
            // the pseudo echo: its length counts down
            if (chan->pseudoEchoLength-- > 1)
                goto envelopeDone;
            chan->statusFlags = 0;
            goto nextChannel;
        }
        if (status & SOUND_CHANNEL_SF_STOP) {
            // release
            env = (env * chan->release) >> 8;
            if (env > chan->pseudoEchoVolume)
                goto envelopeDone;
        echoStart:
            env = chan->pseudoEchoVolume;
            if (env == 0) {
                chan->statusFlags = 0;
                goto nextChannel;
            }
            status |= SOUND_CHANNEL_SF_IEC;
            chan->statusFlags = status;
            goto envelopeDone;
        }
        switch (status & SOUND_CHANNEL_SF_ENV) {
        case SOUND_CHANNEL_SF_ENV_DECAY:
            env = (env * chan->decay) >> 8;
            if (env > chan->sustain)
                break;
            env = chan->sustain;
            if (env == 0)
                goto echoStart;
            status--;
            chan->statusFlags = status;
            break;
        case SOUND_CHANNEL_SF_ENV_ATTACK:
        attack:
            env += chan->attack;
            if (env >= 0xFF) {
                env = 0xFF;
                status--;
                chan->statusFlags = status;
            }
            break;
        }
    envelopeDone:
        chan->envelopeVolume = env;
        env = ((soundInfo->masterVolume + 1) * env) >> 4;
        chan->envelopeVolumeRight = (chan->rightVolume * env) >> 8;
        chan->envelopeVolumeLeft = (chan->leftVolume * env) >> 8;

        s8 *loopStart = NULL;
        s32 loopLength = 0;
        if (status & SOUND_CHANNEL_SF_LOOP) {
            loopStart = wav->data + wav->loopStart;
            loopLength = (s32)(wav->size - wav->loopStart);
        }
        mixChannel(chan, (u8 *)out, samples, divFreq, loopStart, loopLength);
    nextChannel:
        chan++;
    } while (--left > 0);
}

// SoundMain (0x0801A538): once a frame (m4aSoundMain). Locks gSoundInfo (ident + 1), runs the
// music players and CgbSound, then mixes the segment the sound DMA plays next frame:
// pcmSamplesPerVBlank samples at (pcmDmaPeriod - (pcmDmaCounter - 1)) segments into the
// buffer, at the start when pcmDmaCounter is 0 or 1.
void SoundMain(void) {
    // The port test: the VBlanks the original took during this frame's work, which ends here
    // (one that came during the mixing, after the segment is chosen below)
    Port_RunLagFrames(true);
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    if (soundInfo->ident != ID_NUMBER)
        return;
    soundInfo->ident++;

    u32 lineLimit = soundInfo->maxLines;  // [sp+0x14]
    if (lineLimit) {
        u32 line = VCOUNT;
        if (line < 160)
            line += 228;
        lineLimit += line;
    }
    if (soundInfo->MPlayMainHead)
        soundInfo->MPlayMainHead(soundInfo->musicPlayerHead);
    soundInfo->CgbSound();

    s32 samples = soundInfo->pcmSamplesPerVBlank;
    s8 *out = soundInfo->pcmBuffer;
    u32 dmaCounter = soundInfo->pcmDmaCounter;
    if (dmaCounter > 1)
        out += samples * (s32)(soundInfo->pcmDmaPeriod - (dmaCounter - 1));
    Port_RunLagFrames(false);
    soundMainRAM(soundInfo, out, dmaCounter, lineLimit);
    soundInfo->ident = ID_NUMBER;
}

// --- the chains of channels -------------------------------------------------------------------

// sub_0801A960 (jump table 35, m4a.c's Clear64byte): clears 64 bytes
void sub_0801A960(u32 *p) {
    for (int i = 0; i < 16; i++)
        p[i] = 0;
}

// sub_0801A978 (jump table 34, m4a.c's ClearChain; pret's RealClearChain): unlinks a channel
// from its track's chain
void sub_0801A978(struct SoundChannel *chan) {
    struct MusicPlayerTrack *track = chan->track;
    if (!track)
        return;
    struct SoundChannel *next = chan->nextChannelPointer, *prev = chan->prevChannelPointer;
    if (prev)
        prev->nextChannelPointer = next;
    else
        track->chan = next;
    if (next)
        next->prevChannelPointer = prev;
    chan->track = NULL;
}

// sub_0801A998 (jump table 0, pret's ply_fine): ends a track: each of its playing channels
// released and unlinked
void sub_0801A998(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    for (struct SoundChannel *chan = track->chan; chan; chan = chan->nextChannelPointer) {
        if (chan->statusFlags & SOUND_CHANNEL_SF_ON)
            chan->statusFlags |= SOUND_CHANNEL_SF_STOP;
        sub_0801A978(chan);
    }
    track->flags = 0;
}

// MPlayJumpTableCopy (0x0801A9C8): the template into gMPlayJumpTable, each word through the
// BIOS check below (the template, in ROM, always passes)
static inline u32 biosCheck(const void *address, u32 value);
void MPlayJumpTableCopy(MPlayFunc *dest) {
    for (int i = 0; i < 36; i++)
        ((u32 *)dest)[i] = biosCheck(&gUnk_0801F2D4[i], gUnk_0801F2D4[i]);
}

// --- reading the song ---------------------------------------------------------------------------

// sub_0801A9E2: a value read from below 0x02000000 (the BIOS) reads as 0, so that a song cannot
// read the BIOS (the compare with the template's address can never succeed there).
static inline u32 biosCheck(const void *address, u32 value) {
    uint32_t a = (uint32_t)(uintptr_t)address;
    if (a >> 25)
        return value;
    if (a >= 0x0801F2D4u && (a >> 14) == 0)
        return value;
    return 0;
}

// sub_0801A9FC / sub_0801A9FE: the track's next command byte, through the BIOS check
static inline u32 readCmdByte(struct MusicPlayerTrack *track) {
    u8 *p = track->cmdPtr;
    track->cmdPtr = p + 1;
    return biosCheck(p, *p);
}

// sub_0801AA08 (jump table 1, pret's ply_goto): the 4-byte address that follows (its first
// byte through the BIOS check)
void sub_0801AA08(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    u8 *p = track->cmdPtr;
    u32 a = ((((u32)p[3] << 8 | p[2]) << 8) | p[1]) << 8;
    track->cmdPtr = (u8 *)(uintptr_t)(a | biosCheck(p, p[0]));
}

// sub_0801AA28 (jump table 2, pret's ply_patt): a call, three levels deep; deeper ends the track
void sub_0801AA28(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    if (track->patternLevel >= 3) {
        sub_0801A998(mplayInfo, track);
        return;
    }
    track->patternStack[track->patternLevel] = track->cmdPtr + 4;
    track->patternLevel++;
    sub_0801AA08(mplayInfo, track);
}

// sub_0801AA44 (jump table 3, pret's ply_pend): the return
void sub_0801AA44(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    if (track->patternLevel) {
        track->patternLevel--;
        track->cmdPtr = track->patternStack[track->patternLevel];
    }
}

// sub_0801AA58 (jump table 4, pret's ply_rept): count 0 jumps for ever; else the jump is taken
// count - 1 times. The repeat counter is compared before it is stored as a byte.
void sub_0801AA58(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    u8 *p = track->cmdPtr;
    if (*p == 0) {
        track->cmdPtr = p + 1;
        sub_0801AA08(mplayInfo, track);
        return;
    }
    u32 n = track->repN + 1;
    track->repN = n;
    u32 count = readCmdByte(track);
    if (n < count) {
        sub_0801AA08(mplayInfo, track);
        return;
    }
    track->repN = 0;
    track->cmdPtr = p + 5;
}

// sub_0801AA88 (jump table 9, pret's ply_prio)
void sub_0801AA88(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->priority = readCmdByte(track);
}

// sub_0801AA94 (jump table 10, pret's ply_tempo): the tempo in BPM / 2
void sub_0801AA94(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    u32 tempo = readCmdByte(track) << 1;
    mplayInfo->tempoD = tempo;
    mplayInfo->tempoI = (tempo * mplayInfo->tempoU) >> 8;
}

// sub_0801AAA8 (jump table 11, pret's ply_keysh)
void sub_0801AAA8(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->keyShift = readCmdByte(track);
    track->flags |= MPT_FLG_PITCHG;
}

// sub_0801AABC (jump table 12, pret's ply_voice): the instrument, a copy of the song's tone
// (each word through the BIOS check, made on the tone's address)
void sub_0801AABC(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    u8 *p = track->cmdPtr;
    u32 n = *p;
    track->cmdPtr = p + 1;
    const u32 *tone = (const u32 *)(mplayInfo->tone + n);
    u32 *dest = (u32 *)&track->tone;
    for (int i = 0; i < 3; i++)
        dest[i] = biosCheck(tone, tone[i]);
}

// sub_0801AAEC (jump table 13, pret's ply_vol)
void sub_0801AAEC(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->vol = readCmdByte(track);
    track->flags |= MPT_FLG_VOLCHG;
}

// sub_0801AB00 (jump table 14, pret's ply_pan)
void sub_0801AB00(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->pan = readCmdByte(track) - C_V;
    track->flags |= MPT_FLG_VOLCHG;
}

// sub_0801AB14 (jump table 15, pret's ply_bend)
void sub_0801AB14(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->bend = readCmdByte(track) - C_V;
    track->flags |= MPT_FLG_PITCHG;
}

// sub_0801AB28 (jump table 16, pret's ply_bendr)
void sub_0801AB28(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->bendRange = readCmdByte(track);
    track->flags |= MPT_FLG_PITCHG;
}

// sub_0801AB3C (jump table 18, pret's ply_lfodl)
void sub_0801AB3C(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->lfoDelay = readCmdByte(track);
}

// sub_0801AB48 (jump table 20, pret's ply_modt): the modulation's target (0 pitch, 1 volume,
// 2 pan), applied only when it changes
void sub_0801AB48(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    u32 type = readCmdByte(track);
    if (track->modT != type) {
        track->modT = type;
        track->flags |= MPT_FLG_VOLCHG | MPT_FLG_PITCHG;
    }
}

// sub_0801AB60 (jump table 23, pret's ply_tune)
void sub_0801AB60(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->tune = readCmdByte(track) - C_V;
    track->flags |= MPT_FLG_PITCHG;
}

// sub_0801AB74 (jump table 27, pret's ply_port): writes a byte to a sound register
// (0x04000060 + the first byte; the value through the BIOS check)
void sub_0801AB74(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    u8 *p = track->cmdPtr;
    volatile u8 *reg = (volatile u8 *)(uintptr_t)(IO_START + 0x60 + *p);
    track->cmdPtr = p + 1;
    *reg = readCmdByte(track);
}

// --- the VBlank side ----------------------------------------------------------------------------

// m4aSoundVSync (0x0801AB8C, from the VBlank interrupt): counts the VBlanks of the buffer's
// period; at its end the sound DMAs are stopped and started again, so they read the buffer
// from its start. Runs while SoundMain holds the lock too (ident + 1).
void m4aSoundVSync(void) {
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    // the port test: a lag VBlank the original's VSync ignored (port/src/hw.c, lagVblank)
    if (gPortInLagVBlank && gPortLagSoundOff)
        return;
    if (soundInfo->ident - ID_NUMBER > 1)
        return;
    s32 counter = soundInfo->pcmDmaCounter - 1;
    soundInfo->pcmDmaCounter = counter;
    if (counter > 0)
        return;
    soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;
    IO16(0xC6) = 0;  // DMA1CNT_H, DMA2CNT_H
    IO16(0xD2) = 0;
    IO16(0xC6) = 0xB600;  // on, sound FIFO timing, 32-bit, repeat
    IO16(0xD2) = 0xB600;
    Port_SoundDmaRestart();
}

// --- the sequencer ------------------------------------------------------------------------------

// sub_0801B0CC (the asm's ClearModM): the modulation back to 0, applied at the next update
static void sub_0801B0CC(struct MusicPlayerTrack *track) {
    track->modM = 0;
    track->lfoSpeedC = 0;
    track->flags |= track->modT == 0 ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
}

// MPlayMain (0x0801ABC4, gSoundInfo.MPlayMainHead): one frame of a music player, then the
// next player of the chain (first: MPlayMainNext). Each 150 of tempo is a tick: the tracks'
// channels count their gate time, the tracks run their commands until one waits, and the LFO
// steps; after the ticks, the channels of tracks whose volume or pitch changed are updated.
void MPlayMain(struct MusicPlayerInfo *mplayInfo) {
    if (mplayInfo->ident != ID_NUMBER)
        return;
    mplayInfo->ident++;
    if (mplayInfo->MPlayMainNext)
        mplayInfo->MPlayMainNext(mplayInfo->musicPlayerNext);
    if ((s32)mplayInfo->status < 0)
        goto end;
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;  // r8
    FadeOutBody(mplayInfo);

    u32 tempo = (u32)mplayInfo->tempoC + mplayInfo->tempoI;  // compared before it is stored as a halfword
    for (;;) {
        mplayInfo->tempoC = tempo;
        if (tempo < 150)
            break;
        s32 n = mplayInfo->trackCount;  // r6
        struct MusicPlayerTrack *track = mplayInfo->tracks;
        u32 bit = 1, active = 0;
        do {
            if (!(track->flags & MPT_FLG_EXIST))
                goto nextTrack;
            active |= bit;
            for (struct SoundChannel *chan = track->chan; chan; chan = chan->nextChannelPointer) {
                if (!(chan->statusFlags & SOUND_CHANNEL_SF_ON)) {
                    ClearChain(chan);
                    continue;
                }
                if (chan->gateTime && --chan->gateTime == 0)
                    chan->statusFlags |= SOUND_CHANNEL_SF_STOP;
            }
            if (track->flags & MPT_FLG_START) {
                Clear64byte(track);
                track->flags = MPT_FLG_EXIST;
                track->bendRange = 2;
                track->volX = 64;
                track->lfoSpeed = 22;
                track->tone.type = 1;
            }
            while (track->wait == 0) {
                u8 *p = track->cmdPtr;
                u32 cmd = *p;
                if (cmd < 0x80) {
                    cmd = track->runningStatus;
                } else {
                    track->cmdPtr = p + 1;
                    if (cmd >= 0xBD)
                        track->runningStatus = cmd;
                }
                if (cmd >= 0xCF) {
                    soundInfo->plynote(cmd - 0xCF, mplayInfo, track);
                } else if (cmd > 0xB0) {
                    mplayInfo->cmd = cmd - 0xB1;
                    ((void (*)(struct MusicPlayerInfo *, struct MusicPlayerTrack *))
                         soundInfo->MPlayJumpTable[cmd - 0xB1])(mplayInfo, track);
                    if (track->flags == 0)
                        goto nextTrack;
                } else {
                    track->wait = gUnk_0801F548[cmd - 0x80];
                }
            }
            track->wait--;
            if (track->lfoSpeed && track->mod) {
                if (track->lfoDelayC) {
                    track->lfoDelayC--;
                } else {
                    // the LFO's phase: the sum is used before it is stored as a byte
                    u32 phase = (u32)track->lfoSpeedC + track->lfoSpeed;
                    track->lfoSpeedC = phase;
                    s32 wave = (s8)(phase - 64) < 0 ? (s8)phase : 128 - (s32)phase;
                    wave = (s32)(track->mod * wave) >> 6;
                    if ((u8)(track->modM ^ wave)) {
                        track->modM = wave;
                        track->flags |= track->modT == 0 ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
                    }
                }
            }
        nextTrack:
            track++;
            bit <<= 1;
        } while (--n > 0);
        mplayInfo->clock++;
        if (active == 0) {
            mplayInfo->status = MUSICPLAYER_STATUS_PAUSE;
            goto end;
        }
        mplayInfo->status = active;
        tempo = mplayInfo->tempoC - 150;
    }

    s32 n = mplayInfo->trackCount;
    struct MusicPlayerTrack *track = mplayInfo->tracks;
    do {
        if ((track->flags & MPT_FLG_EXIST) && (track->flags & (MPT_FLG_VOLCHG | MPT_FLG_PITCHG))) {
            TrkVolPitSet(mplayInfo, track);
            for (struct SoundChannel *chan = track->chan; chan; chan = chan->nextChannelPointer) {
                if (!(chan->statusFlags & SOUND_CHANNEL_SF_ON)) {
                    ClearChain(chan);
                    continue;
                }
                u32 cgb = chan->type & TONEDATA_TYPE_CGB;
                u32 flags = track->flags;
                if (flags & MPT_FLG_VOLCHG) {
                    chan->rightVolume = (track->volMR * chan->velocity) >> 7;
                    chan->leftVolume = (track->volML * chan->velocity) >> 7;
                    if (cgb)
                        ((struct CgbChannel *)chan)->modify |= CGB_CHANNEL_MO_VOL;
                }
                if (flags & MPT_FLG_PITCHG) {
                    s32 key = chan->key + (s8)track->keyM;
                    if (key < 0)
                        key = 0;
                    if (cgb) {
                        chan->frequency = soundInfo->MidiKeyToCgbFreq(cgb, key, track->pitM);
                        ((struct CgbChannel *)chan)->modify |= CGB_CHANNEL_MO_PIT;
                    } else {
                        chan->frequency = MidiKeyToFreq(chan->wav, key, track->pitM);
                    }
                }
            }
            track->flags &= 0xF0;
        }
        track++;
    } while (--n > 0);
end:
    mplayInfo->ident = ID_NUMBER;
}

// TrackStop (0x0801AE30, jump table 31): stops a track's channels (CgbOscOff for a CGB one)
void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    if (!(track->flags & MPT_FLG_EXIST))
        return;
    struct SoundChannel *chan;
    for (chan = track->chan; chan; chan = chan->nextChannelPointer) {
        if (chan->statusFlags) {
            u32 cgb = chan->type & TONEDATA_TYPE_CGB;
            if (cgb)
                SOUND_INFO_PTR->CgbOscOff(cgb);
            chan->statusFlags = 0;
        }
        chan->track = NULL;
    }
    track->chan = NULL;
}

// ply_note (0x0801AE74, gSoundInfo.plynote): a note, commands 0xCF-0xFF (its length:
// gClockTable[note]), then up to three bytes below 0x80: key, velocity, extra length. The
// instrument picks a channel: a CGB tone its own channel, if it is free, releasing or of a
// lower priority; a DirectSound one a free channel, else the releasing one, else the one of
// the lowest priority (the highest track address among equals, the last one among those).
void ply_note(u32 note, struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    track->gateTime = gUnk_0801F548[note];
    u8 *p = track->cmdPtr;
    if (*p < 0x80) {
        track->key = *p++;
        if (*p < 0x80) {
            track->velocity = *p++;
            if (*p < 0x80)
                track->gateTime += *p++;
        }
        track->cmdPtr = p;
    }

    u32 rhythmPan = 0;   // [sp+0x14]
    struct ToneData *tone; // r9
    u32 key = track->key;  // r3, [sp+8]
    u32 type = track->tone.type;
    if (type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL)) {
        u32 n = key;
        if (type & TONEDATA_TYPE_SPL) {
            const u8 *keySplitTable;
            memcpy(&keySplitTable, &track->tone.attack, 4);
            n = keySplitTable[key];
        }
        tone = (struct ToneData *)track->tone.wav + n;
        if (tone->type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL))
            return;
        if (type & TONEDATA_TYPE_RHY) {
            if (tone->pan_sweep & 0x80)
                rhythmPan = tone->pan_sweep;
            key = tone->key;
        }
    } else {
        tone = &track->tone;
    }

    u32 priority = mplayInfo->priority + track->priority;  // [sp+0x10]
    if (priority > 0xFF)
        priority = 0xFF;
    u32 cgb = tone->type & TONEDATA_TYPE_CGB;  // [sp+0xC]
    struct SoundChannel *chan;                 // r4
    if (cgb) {
        if (!soundInfo->cgbChans)
            return;
        chan = (struct SoundChannel *)&soundInfo->cgbChans[cgb - 1];
        u32 status = chan->statusFlags;
        if ((status & SOUND_CHANNEL_SF_ON) && !(status & SOUND_CHANNEL_SF_STOP)) {
            if (chan->priority > priority)
                return;
            if (chan->priority == priority && (uintptr_t)chan->track < (uintptr_t)track)
                return;
        }
    } else {
        u32 bestPriority = priority;                   // r6
        struct MusicPlayerTrack *bestTrack = track;    // r7
        bool releasing = false;                        // r2
        struct SoundChannel *c = soundInfo->chans;
        s32 n = soundInfo->maxChans;
        chan = NULL;                                   // r8
        do {
            u32 status = c->statusFlags;
            if (!(status & SOUND_CHANNEL_SF_ON)) {
                chan = c;
                goto found;
            }
            if (status & SOUND_CHANNEL_SF_STOP) {
                if (!releasing) {
                    releasing = true;
                    bestPriority = c->priority;
                    bestTrack = c->track;
                    chan = c;
                    goto next;
                }
            } else if (releasing) {
                goto next;
            }
            if (c->priority < bestPriority) {
                bestPriority = c->priority;
                bestTrack = c->track;
                chan = c;
            } else if (c->priority == bestPriority) {
                if ((uintptr_t)c->track > (uintptr_t)bestTrack) {
                    bestTrack = c->track;
                    chan = c;
                } else if (c->track == bestTrack) {
                    chan = c;
                }
            }
        next:
            c++;
        } while (--n > 0);
        if (!chan)
            return;
    }
found:
    ClearChain(chan);
    chan->prevChannelPointer = NULL;
    chan->nextChannelPointer = track->chan;
    if (track->chan)
        track->chan->prevChannelPointer = chan;
    track->chan = chan;
    chan->track = track;
    track->lfoDelayC = track->lfoDelay;
    if (track->lfoDelay)
        sub_0801B0CC(track);
    if (rhythmPan) {
        track->panX = (rhythmPan - 0xC0) << 1;
        track->flags |= MPT_FLG_VOLCHG;
    }
    TrkVolPitSet(mplayInfo, track);
    memcpy(&chan->gateTime, &track->gateTime, 4);  // gateTime, midiKey, velocity (and priority:)
    chan->priority = priority;
    chan->key = key;
    chan->type = tone->type;
    chan->wav = tone->wav;
    memcpy(&chan->attack, &tone->attack, 4);       // attack, decay, sustain, release
    memcpy(&chan->pseudoEchoVolume, &track->pseudoEchoVolume, 2);
    chan->rightVolume = (track->volMR * chan->velocity) >> 7;
    chan->leftVolume = (track->volML * chan->velocity) >> 7;
    s32 k = chan->key + (s8)track->keyM;
    if (k < 0)
        k = 0;
    if (cgb) {
        struct CgbChannel *cgbChan = (struct CgbChannel *)chan;
        cgbChan->length = tone->length;
        u32 sweep = tone->pan_sweep;
        if ((sweep & 0x80) || !(sweep & 0x70))
            sweep = 8;
        cgbChan->sweep = sweep;
        chan->frequency = soundInfo->MidiKeyToCgbFreq(cgb, k, track->pitM);
    } else {
        chan->frequency = MidiKeyToFreq(chan->wav, k, track->pitM);
    }
    chan->statusFlags = SOUND_CHANNEL_SF_START;
    track->flags &= 0xF0;
}

// ply_endtie (0x0801B08C, jump table 29): releases the track's first channel playing the key
// (the byte that follows, if below 0x80, else the track's last key)
void ply_endtie(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    u8 *p = track->cmdPtr;
    u32 key = *p;
    if (key < 0x80) {
        track->key = key;
        track->cmdPtr = p + 1;
    } else {
        key = track->key;
    }
    for (struct SoundChannel *chan = track->chan; chan; chan = chan->nextChannelPointer) {
        u32 status = chan->statusFlags;
        if ((status & (SOUND_CHANNEL_SF_START | SOUND_CHANNEL_SF_ENV)) && !(status & SOUND_CHANNEL_SF_STOP) &&
            chan->midiKey == key) {
            chan->statusFlags = status | SOUND_CHANNEL_SF_STOP;
            return;
        }
    }
}

// sub_0801B0E8: the next command byte, without the BIOS check
static inline u32 readCmdByteUnchecked(struct MusicPlayerTrack *track) {
    return *track->cmdPtr++;
}

// ply_lfos (0x0801B0F4, jump table 17): the LFO's speed; 0 stops the modulation
void ply_lfos(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->lfoSpeed = readCmdByteUnchecked(track);
    if (track->lfoSpeed == 0)
        sub_0801B0CC(track);
}

// ply_mod (0x0801B108, jump table 19): the modulation's depth; 0 stops it
void ply_mod(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
    (void)mplayInfo;
    track->mod = readCmdByteUnchecked(track);
    if (track->mod == 0)
        sub_0801B0CC(track);
}
