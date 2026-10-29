// The GBA hardware the game's C talks to (docs/port.md,
// "Hardware, BIOS and asm replaced"): the DMA unit, the interrupt dispatch (IntrMain), the
// frame boundary (Port_WaitVBlank) and the BIOS calls (CpuSet, CpuFastSet, MultiBoot).
// The I/O registers are plain memory at 0x04000000: a write only stores the value, except
// for what goes through the functions here.
#include "port.h"

#include <string.h>

// --- reading the I/O registers through the BIOS and DMA --------------------------------------
//
// The game copies the I/O registers into RAM in bulk (Main_InstallFrameCallback: CpuSet of
// 0x2B halfwords from 0x04000000; Main_InstallLinkFrameCallback: the same with DMA 3), write-
// only ones included, and its frame callback later writes those copies back: what a write-only
// register reads as reaches the screen (the mode select's affine BG2 for two frames, 30B2 in
// MOSAIC). The port's I/O is plain memory, so these reads are made here the way mGBA makes
// them (GBAIORead, GBALoadBad), the reference the port is tested against: a readable register
// reads its value as the hardware keeps it, a write-only or unused one is open bus: for the
// BIOS's CpuSet the low half of the ARM instruction two ahead in mGBA's HLE BIOS, for DMA the
// last value the DMA unit moved. Only the video block (0x000-0x05F) is modelled; the game
// reads nothing else in bulk, and the rest reads as stored.

static uint32_t sDmaBus;  // the DMA unit's last value (mGBA's dmaTransferRegister)

static bool isIo(uint32_t a) {
    return a >= IO_START && a < IO_START + IO_SIZE;
}

// The halfword at I/O offset *ofs* (even), or *openBus* when it is write-only or unused.
static uint16_t ioRead16(uint32_t ofs, uint16_t openBus) {
    uint16_t v = IO16(ofs);
    switch (ofs) {
    case 0x000: return v & 0xFFF7;  // DISPCNT
    case 0x002:                     // GREENSWP
    case 0x004:                     // DISPSTAT
    case 0x006:                     // VCOUNT
    case 0x00C:                     // BG2CNT
    case 0x00E:                     // BG3CNT
        return v;
    case 0x008:  // BG0CNT, BG1CNT
    case 0x00A:
        return v & 0xDFFF;
    case 0x048:  // WININ, WINOUT
    case 0x04A:
        return v & 0x3F3F;
    case 0x050: return v & 0x3FFF;  // BLDCNT
    case 0x052: return v & 0x1F1F;  // BLDALPHA
    default:
        return ofs < 0x060 ? openBus : v;
    }
}

// --- DMA -------------------------------------------------------------------------------------

#define DMA_ENABLE 0x80000000u
#define DMA_TIMING(c) (((c) >> 28) & 3)  // 0 now, 1 VBlank, 2 HBlank, 3 special (sound FIFO)
#define DMA_REPEAT_BIT 0x02000000u

static uint32_t step(uint32_t mode, uint32_t unit) {
    switch (mode) {
    case 0: return unit;           // increment
    case 1: return (uint32_t)-unit;  // decrement
    case 2: return 0;              // fixed
    default: return unit;          // increment (and reload, for a repeating destination)
    }
}

// A channel's internal registers: where its next unit is read and written, and how many
// units one start (or one HBlank, for a repeating channel) moves.
struct DmaChannel {
    uint32_t src, dst, count, control;
};
static struct DmaChannel sDma[4];

static void runChannel(uint32_t num) {
    struct DmaChannel *ch = &sDma[num];
    uint32_t control = ch->control;
    uint32_t unit = (control & 0x04000000) ? 4 : 2;
    uint32_t srcStep = step((control >> 23) & 3, unit), dstStep = step((control >> 21) & 3, unit);
    for (uint32_t i = 0; i < ch->count; i++, ch->src += srcStep, ch->dst += dstStep) {
        if (unit == 4) {
            if (isIo(ch->src))
                sDmaBus = ioRead16(ch->src - IO_START, (uint16_t)sDmaBus) |
                          (uint32_t)ioRead16(ch->src + 2 - IO_START, (uint16_t)sDmaBus) << 16;
            else
                sDmaBus = *(const volatile uint32_t *)(uintptr_t)ch->src;
            *(volatile uint32_t *)(uintptr_t)ch->dst = sDmaBus;
        } else {
            uint16_t v = isIo(ch->src) ? ioRead16(ch->src - IO_START, (uint16_t)sDmaBus)
                                       : *(const volatile uint16_t *)(uintptr_t)ch->src;
            sDmaBus = v | (uint32_t)v << 16;
            *(volatile uint16_t *)(uintptr_t)ch->dst = v;
        }
    }
}

void Port_Dma(uint32_t num, const volatile void *src, volatile void *dest, uint32_t control) {
    uint32_t base = 0xB0 + 12 * num;
    IO32(base) = (uint32_t)(uintptr_t)src;
    IO32(base + 4) = (uint32_t)(uintptr_t)dest;
    IO32(base + 8) = control;
    if (!(control & DMA_ENABLE))
        return;
    // Started: the internal registers are loaded. The hardware ignores the low address bits
    // of the unit (src/dst are host addresses: the GBA's own, or the host stack for the SDK's
    // fill-from-a-local macros).
    uint32_t unit = (control & 0x04000000) ? 4 : 2;
    uint32_t count = control & (num == 3 ? 0xFFFF : 0x3FFF);
    if (count == 0)
        count = num == 3 ? 0x10000 : 0x4000;
    sDma[num] = (struct DmaChannel){ (uint32_t)(uintptr_t)src & ~(unit - 1), (uint32_t)(uintptr_t)dest & ~(unit - 1),
                                     count, control };
    if (DMA_TIMING(control) != 0)
        return;  // started by the display (HBlank: Port_RunHblankDma) or the sound FIFO (not run)
    runChannel(num);
    IO16(base + 10) &= ~(DMA_ENABLE >> 16);  // an immediate transfer ends with its enable bit clear
}

// At the HBlank of each visible line (port/src/video.c): the channels started with HBlank
// timing. A repeating one stays on (the destination reloaded if asked: mode 3); the others
// end. A channel the game stopped by writing its control register directly is not run.
void Port_RunHblankDma(void) {
    for (uint32_t num = 0; num < 4; num++) {
        uint32_t base = 0xB0 + 12 * num;
        uint16_t controlHi = IO16(base + 10);
        if (!(controlHi & 0x8000) || DMA_TIMING((uint32_t)controlHi << 16) != 2)
            continue;
        struct DmaChannel *ch = &sDma[num];
        runChannel(num);
        if (ch->control & DMA_REPEAT_BIT) {
            if (((ch->control >> 21) & 3) == 3)
                ch->dst = IO32(base + 4) & ~((ch->control & 0x04000000) ? 3u : 1u);
        } else {
            IO16(base + 10) = controlHi & ~0x8000;
        }
    }
}

// --- the sound FIFOs' DMA ---------------------------------------------------------------------
//
// DMA 1 and 2 feed the sound FIFOs (sound FIFO timing, 32-bit, repeat): each time a FIFO asks
// (port/src/audio.c), the channel whose destination is that FIFO moves 4 words to it from its
// internal source, which counts up. m4aSoundVSync stops and starts both channels every 7
// VBlanks, which reloads the source from DMAxSAD: the start of the PCM buffer's two halves.

void Port_SoundDmaRestart(void) {
    for (uint32_t num = 1; num <= 2; num++) {
        uint32_t base = 0xB0 + 12 * num;
        sDma[num] = (struct DmaChannel){ IO32(base) & ~3u, IO32(base + 4) & ~3u, 4, IO32(base + 8) };
    }
}

bool Port_SoundDmaRequest(uint32_t fifo, uint8_t out[16]) {
    for (uint32_t num = 1; num <= 2; num++) {
        uint32_t base = 0xB0 + 12 * num;
        uint32_t control = IO32(base + 8);
        if (!(control & DMA_ENABLE) || DMA_TIMING(control) != 3 || IO32(base + 4) != 0x040000A0u + 4 * fifo)
            continue;
        struct DmaChannel *ch = &sDma[num];
        if (!(ch->control & DMA_ENABLE))  // started by the game's own register write (m4aSoundVSyncOn)
            Port_SoundDmaRestart();
        for (int i = 0; i < 16; i += 4) {
            uint32_t v = *(const volatile uint32_t *)(uintptr_t)ch->src;
            memcpy(out + i, &v, 4);
            ch->src += step((control >> 23) & 3, 4);
        }
        return true;
    }
    return false;
}

// --- BIOS ------------------------------------------------------------------------------------

// SWI 0x0B: copies or fills (bit 24) count halfwords or words (bit 26), count in bits 0-20;
// the BIOS aligns both addresses to the unit. An I/O source reads as described above; the
// open-bus values are the instructions after each of mGBA's HLE BIOS loads (src/gba/hle-bios.s:
// blt 0xBAFFFFFB after the copy loops' loads, strlth 0xB0C530B2 and stmltia 0xB8A10008
// after the fills').
void CpuSet(const void *src, void *dest, uint32_t control) {
    uint32_t count = control & 0x1FFFFF, s = (uint32_t)(uintptr_t)src, d = (uint32_t)(uintptr_t)dest;
    bool fill = control & 0x01000000;
    if (control & 0x04000000) {
        s &= ~3u, d &= ~3u;
        uint16_t openBus = fill ? 0x0008 : 0xFFFB;
        for (uint32_t i = 0; i < count; i++, d += 4, s += fill ? 0 : 4)
            *(volatile uint32_t *)(uintptr_t)d = isIo(s) ? ioRead16(s - IO_START, openBus) |
                                                               (uint32_t)ioRead16(s + 2 - IO_START, openBus) << 16
                                                         : *(const volatile uint32_t *)(uintptr_t)s;
    } else {
        s &= ~1u, d &= ~1u;
        uint16_t openBus = fill ? 0x30B2 : 0xFFFB;
        for (uint32_t i = 0; i < count; i++, d += 2, s += fill ? 0 : 2)
            *(volatile uint16_t *)(uintptr_t)d = isIo(s) ? ioRead16(s - IO_START, openBus)
                                                         : *(const volatile uint16_t *)(uintptr_t)s;
    }
}

// SWI 0x0C: words only, in blocks of 8 (the count is rounded up), fill with bit 24.
void CpuFastSet(const void *src, void *dest, uint32_t control) {
    uint32_t count = ((control & 0x1FFFFF) + 7) & ~7u;
    uint32_t s = (uint32_t)(uintptr_t)src & ~3u, d = (uint32_t)(uintptr_t)dest & ~3u;
    bool fill = control & 0x01000000;
    for (uint32_t i = 0; i < count; i++, d += 4, s += fill ? 0 : 4)
        *(volatile uint32_t *)(uintptr_t)d = *(const volatile uint32_t *)(uintptr_t)s;
}

// SWI 0x25: there is never a second console: the transfer fails.
int MultiBoot(void *mp) {
    (void)mp;
    return 1;
}

// --- Interrupts ------------------------------------------------------------------------------

#define REG_OFS_DISPSTAT 0x004
#define REG_OFS_IE 0x200
#define REG_OFS_IF 0x202
#define REG_OFS_IME 0x208

// 0x030008B0: the game's interrupt handlers (gIntrTable, copied from the ROM's gIntrTableInit,
// whose code pointers the loader patched)
extern void (*gIntrTable[])(void);
extern uint8_t gMain[];  // struct Main: +8 intrFlags

// IntrMain (crt0, ARM): the handler of the first pending interrupt, in this order: VBlank,
// serial/timer 3 (one entry), HBlank, VCount, DMA 0-3, keypad. It acknowledges that interrupt
// and calls the handler with interrupts off, so they do not nest.
static void intrMain(void) {
    static const uint16_t order[] = { 0x0001, 0x00C0, 0x0002, 0x0004, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000 };
    for (;;) {
        uint16_t pending = IO16(REG_OFS_IE) & IO16(REG_OFS_IF);
        if (!(IO16(REG_OFS_IME) & 1) || !(pending & 0x3FFF))
            return;
        int i;
        for (i = 0; i < 9 && !(pending & order[i]); i++)
            ;
        if (i == 9)
            Port_Die("interrupt 0x%04X has no handler in IntrMain", pending);
        IO16(REG_OFS_IF) &= ~(pending & order[i]);
        void (*handler)(void) = gIntrTable[i];
        if ((uint32_t)(uintptr_t)handler < 0x10000000u)
            Port_Die("gIntrTable[%d] is 0x%08X, not host code (RAM code is the link cable's)", i,
                     (uint32_t)(uintptr_t)handler);
        handler();
    }
}

void Port_RaiseIrq(uint16_t bit) {
    IO16(REG_OFS_IF) |= bit;
    intrMain();
}

// The start of VBlank (line 160): its interrupt if DISPSTAT asks for it, and a VCount match
// at line 160. The later VCount matches (the game's is at line 227) come before the next
// frame's lines (Port_DrawFrame).
static void vblank(bool irq) {
    uint16_t dispstat = IO16(REG_OFS_DISPSTAT);
    IO16(0x006) = 160;  // VCOUNT
    if (irq && (dispstat & 0x0008))
        Port_RaiseIrq(0x0001);
    dispstat = IO16(REG_OFS_DISPSTAT);
    if (irq && (dispstat & 0x0020) && (dispstat >> 8) == 160)
        Port_RaiseIrq(0x0004);
}

// A VBlank of the original's that came in the middle of a frame's work (a lag frame). Where
// in the work it came, the port cannot know; m4a's VSync is the one thing that depends on it
// (m4aSoundVSyncOff and On during a level's loading): the port test says whether the
// original's VSync ignored this VBlank (gPortLagSoundOff), and while it is raised the port's
// ignores it too (gPortInLagVBlank, m4a_1.c).
bool gPortInLagVBlank;

static void lagVblank(bool irq) {
    Port_AudioFrame();  // the sound goes on through a lag frame (with the --wav of a test run)
    gPortInLagVBlank = true;
    vblank(irq);
    gPortInLagVBlank = false;
}

// The original's lag VBlanks of this frame of work, from the port's SoundMain (m4a_1.c): they
// came during the work, which m4aSoundMain ends (Main_EndFrame), so m4aSoundVSync had counted
// them when the original's SoundMain chose its buffer segment, except one that came while the
// mixer ran (beforeMixer: SoundMain's start, the others; then, after it chose, that one).
void Port_RunLagFrames(bool beforeMixer) {
    bool irq;
    while (!(beforeMixer && Port_NextLagInMixer()) && Port_LagFrame(&irq))
        lagVblank(irq);
}

// SampleFreqSet's wait for line 159 (src/m4a.c, #ifdef PLATFORM_PC), which lines the sound DMA
// up with the frame. The port's VCOUNT does not count, so there is nothing to wait for; but in
// the original the next VBlank comes a line after the wait: in the port test (the original's
// lag frames) it is raised here, after the VBlanks before AgbMain enabled the interrupt, so
// m4aSoundVSync sees m4a's lock as the original's did (at power-on, m4aSoundInit's two waits
// are each followed by a VBlank of the boot's long frame of work).
void Port_WaitForLine159(void) {
    bool irq;
    while (Port_NextLagKind() == 2 && Port_LagFrame(&irq))
        lagVblank(irq);
    if (Port_NextLagKind() == 1 && Port_LagFrame(&irq))
        lagVblank(irq);
}

// Main_EndFrame's wait for the VBlank interrupt (src/main.c, #ifdef PLATFORM_PC): the frame
// boundary. The game has done its frame's work and cleared gMain.intrFlags bit 0.
void Port_WaitVBlank(void) {
    // The original's lag frames: VBlanks that came while the game was still working. Their
    // interrupt ran in the middle of the frame's code, before Main_EndFrame cleared the flag.
    // (The VBlanks of the first frames after power-on raise none: AgbMain has not enabled
    // the interrupt yet.)
    // (SoundMain, at the end of the work, has normally taken them already: Port_RunLagFrames.)
    bool irq;
    while (Port_LagFrame(&irq)) {
        lagVblank(irq);
        *(volatile uint16_t *)(gMain + 8) &= ~1;
    }
    // The frame's lines, from the hardware as the frame's work left it (port/src/video.c):
    // drawn when someone looks (the window, the pixel test); the HBlank DMA and the display
    // interrupts run either way.
    Port_DrawFrame(Port_WantPixels());
    // The frame's sound, as the hardware played it while the game worked (port/src/audio.c):
    // the DMAs as the last VBlank left them, the GB channels as CgbSound set them
    Port_AudioFrame();
    uint16_t keys = Port_EndFrame();
    IO16(0x130) = ~keys & 0x3FF;  // KEYINPUT: a pressed key reads 0
    vblank(true);
}
