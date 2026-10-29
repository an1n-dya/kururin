// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// main: the entry, the frame loop, interrupts, keys, the frame callbacks, the transfer queue and
// the heap.
// 080002D8-08000AEC, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "link.h"

// The entry Main_QueueCopy and Main_QueueFill fill: they read the count again for each field (the
// count is first read and checked once), as the original does; with the count CSE'd into one
// register each call is a load shorter, and a level start that queues many transfers ends its
// frame earlier than the original (save_adventure, The Last Land 3).
#define QUEUE_ENTRY gTransferQueue[*(vu16 *)&gTransferCount]
// The copies of BG0HOFS/VOFS-BG3HOFS/VOFS, from the layers' positions (the frame callbacks)
extern u32 gBgOfsCopies[4];
void CpuFastSet(const void *src, void *dest, u32 control);
void Main_RunTransfersDma(void);
void Main_RunTransfersBios(void);

void IntrMain(void);
// The interrupt handlers of gIntrTable as they start (IntrDummy and the game's own)
extern void (*const gIntrTableInit[14])(void);
// IWRAM space AgbMain copies IntrMain (ARM code, 0x400 bytes) to and runs it from
extern u32 gIntrMainBuffer[0x100];
// AgbMain's state functions, indexed by gMain.state
extern void (*const gMainStates[])(void);
// The BIOS's interrupt vector: where it jumps on an interrupt
#define INTR_VECTOR (*(void *volatile *)0x03007FFC)

/**
 * The game's entry (crt0 jumps here): sets the wait states, clears EWRAM, IWRAM up to
 * 0x03007E00, VRAM, OAM and palette RAM, copies the interrupt table (gIntrTableInit) and
 * IntrMain to IWRAM and installs it as the interrupt vector, sets BG0CNT/BG1CNT, enables
 * interrupts (IE 0x2F01: VBlank, DMA 0-3, Game Pak; DISPSTAT's VBlank IRQ) and then runs
 * gMainStates[gMain.state] forever.
 */
void AgbMain(void)
{
    REG_WAITCNT = 0x4014;
    DmaFill32(3, 0, (void *)0x02000000, 0x40000);
    DmaFill32(3, 0, (void *)0x03000000, 0x7E00);
    DmaFill32(3, 0, (void *)0x06000000, 0x18000);
    DmaFill32(3, 0, (void *)0x07000000, 0x400);
    DmaFill32(3, 0, (void *)0x05000000, 0x400);
    DmaCopy32(3, gIntrTableInit, gIntrTable, sizeof(gIntrTableInit));
    DmaCopy32(3, IntrMain, gIntrMainBuffer, sizeof(gIntrMainBuffer));
    INTR_VECTOR = gIntrMainBuffer;
    *(vu16 *)(REG_BASE + REG_OFFSET_BG0CNT) = 0x1C00;
    *(vu16 *)(REG_BASE + REG_OFFSET_BG0CNT + 2) = 0x1D01;
    gMain.frameCallback = NULL;
    REG_IE = 0x2F01;
    REG_DISPSTAT = 8;
    REG_IME = 1;
    REG_DISPCNT = 0x40;
    gMain.subState = 0;
    gMain.frameCount = 0;
    gMain.state = 0;
    for (;;)
        gMainStates[gMain.state]();
}
void m4aSoundMain(void);
u16 Main_ReadKeys(void);
#ifdef PLATFORM_PC
void Port_WaitVBlank(void);
#endif

/**
 * Ends a frame: runs m4aSoundMain (m4a), waits for the VBlank interrupt (Main_VBlankIntr sets
 * bit 0 of gMain.intrFlags), runs gMain.frameCallback if set, clears gMain.vblankCount, counts
 * gMain.frameCount up and reads the keys (Main_ReadKeys).
 */
void Main_EndFrame(void)
{
    m4aSoundMain();
    // intrFlags is set by the interrupt handler: read through a volatile gMain
    ((volatile struct Main *)&gMain)->intrFlags &= ~1;
#ifdef PLATFORM_PC
    // The PC port's frame boundary (port/src/hw.c): it raises the VBlank interrupt, which sets
    // the flag this loop waits for.
    Port_WaitVBlank();
#endif
    while (!(((volatile struct Main *)&gMain)->intrFlags & 1))
        ;
    if (gMain.frameCallback != NULL)
        gMain.frameCallback();
    gMain.vblankCount = 0;
    gMain.frameCount++;
    Main_ReadKeys();
}

/**
 * Does nothing: the handler for every interrupt the game does not use. AgbMain copies
 * the interrupt table holding it to IWRAM (0x030008B0), where IntrMain dispatches from.
 */
void IntrDummy(void)
{
}

void m4aSoundVSync(void);

/**
 * The VBlank interrupt handler: stops DMA 0, runs m4a's VSync (m4aSoundVSync), sets
 * gMain.intrFlags to 1 (the wait of Main_EndFrame) and counts gMain.vblankCount up.
 */
void Main_VBlankIntr(void)
{
    REG_DMA0CNT_H = 0;
    m4aSoundVSync();
    // through a volatile gMain, as the original: agbcc then reads the halfword before the
    // store (an interrupt handler, kept to the original's cycles)
    ((volatile struct Main *)&gMain)->intrFlags = 1;
    gMain.vblankCount++;
}
/**
 * Clears the key state of gMain (Main_ReadKeys's held, new, previous and repeated keys and
 * the repeat timer).
 */
void Main_ClearKeys(void)
{
    gMain.heldKeys = 0;
    gMain.newKeys = 0;
    gMain.prevKeys = 0;
    gMain.repeatKeys = 0;
    gMain.repeatTimer = 0;
}
/**
 * Reads the keys (KEYINPUT, inverted, 10 keys) into gMain: heldKeys, newKeys (down now and
 * not the last frame), prevKeys and repeatKeys (newKeys, plus heldKeys every 4 frames once
 * the same keys are held for 20). Returns heldKeys.
 */
u16 Main_ReadKeys(void)
{
    u16 keys = ~REG_KEYINPUT & 0x3FF;
    u16 prev = gMain.heldKeys;
    u16 pressed = keys & ~prev;

    gMain.heldKeys = keys;
    gMain.prevKeys = prev;
    gMain.newKeys = pressed;
    gMain.repeatKeys = pressed;
    if (keys == 0 || keys != prev)
    {
        gMain.repeatTimer = 20;
    }
    else if (--gMain.repeatTimer == 0)
    {
        gMain.repeatKeys = keys;
        gMain.repeatTimer = 4;
    }
    return keys;
}

/**
 * Removes the per-frame callback that Main_EndFrame runs after each VBlank wait.
 */
void Main_ClearFrameCallback(void)
{
    gMain.frameCallback = NULL;
}

void Main_LinkFrameCallback(void);

/**
 * Main_InstallFrameCallback with DMA 3: copies the 0x2B halfwords of I/O registers from DISPCNT on into
 * their copies (gDispcntCopy) and installs Main_LinkFrameCallback as the frame callback.
 */
void Main_InstallLinkFrameCallback(void)
{
    DmaCopy16(3, (void *)REG_BASE, &gDispcntCopy, 0x2B * 2);
    gMain.frameCallback = Main_LinkFrameCallback;
}
// gDispcntCopy (the DISPCNT copy) and gBgCntCopies (BG0CNT on) reached from the BG
// offset copies gBgOfsCopies they precede, as the original addressed them (from one
// base): Main_LinkFrameCallback is a frame callback whose timing shows in the frame's pixels.
// The PC port's GCC would take an address before gBgOfsCopies as outside that object
// (undefined): there they are the globals themselves.
#ifdef PLATFORM_PC
#define IO_COPY_DISPCNT(ofs) gDispcntCopy
#define IO_COPY_BG0CNT(ofs) gBgCntCopies
#else
#define IO_COPY_DISPCNT(ofs) (*(u16 *)((u8 *)(ofs) - 0x10))
#define IO_COPY_BG0CNT(ofs) ((u8 *)(ofs) - 8)
#endif

/**
 * The frame callback of a game over the link cable when the serial interrupt is on
 * (REG_IE bits 6-7, timer 3 and serial): exchanges this frame's data first (Link_Exchange into
 * gLinkStatus), then does what Main_FrameCallback does. Without them it does the same with
 * DMA 3 instead of the BIOS (and runs the queue with Main_RunTransfersDma).
 */
void Main_LinkFrameCallback(void)
{
    struct BgLayer *layer;
    s32 i;

    if (REG_IE & 0xC0)
    {
        gLinkStatus = Link_Exchange(gLinkSendData, gLinkRecvData, 0);
        CpuFastSet(gOamBuffer, (void *)0x07000000, 0x100);
        // A block of its own for ofs: it gives agbcc the original's registers.
        {
            u32 *ofs = gBgOfsCopies;

            // hofs and vofs together, as BGxHOFS and BGxVOFS
            ofs[0] = *(u32 *)&gBgLayers[0].hofs;
            ofs[1] = *(u32 *)&gBgLayers[1].hofs;
            ofs[2] = *(u32 *)&gBgLayers[2].hofs;
            ofs[3] = *(u32 *)&gBgLayers[3].hofs;
            REG_DISPCNT = IO_COPY_DISPCNT(ofs);
            CpuSet(IO_COPY_BG0CNT(ofs), (void *)(REG_BASE + REG_OFFSET_BG0CNT), 0x27);
        }
        Main_RunTransfersBios();
        layer = gBgLayers;
        for (i = 3; i >= 0; i--)
        {
            if ((s8)layer->dirty != 0)
            {
                CpuFastSet(layer->buffer, layer->mapVram, layer->mapSize >> 2);
                layer->dirty = 0;
            }
            layer++;
        }
    }
    else
    {
        DmaCopy32(3, gOamBuffer, (void *)0x07000000, 0x400);
        {
            u32 *ofs = gBgOfsCopies;

            ofs[0] = *(u32 *)&gBgLayers[0].hofs;
            ofs[1] = *(u32 *)&gBgLayers[1].hofs;
            ofs[2] = *(u32 *)&gBgLayers[2].hofs;
            ofs[3] = *(u32 *)&gBgLayers[3].hofs;
            REG_DISPCNT = IO_COPY_DISPCNT(ofs);
            DmaCopy16(3, IO_COPY_BG0CNT(ofs), (void *)(REG_BASE + REG_OFFSET_BG0CNT), 0x27 * 2);
        }
        Main_RunTransfersDma();
        layer = gBgLayers;
        for (i = 3; i >= 0; i--)
        {
            if ((s8)layer->dirty != 0)
            {
                DmaCopy32(3, layer->buffer, layer->mapVram, layer->mapSize);
                layer->dirty = 0;
            }
            layer++;
        }
    }
}
void Main_FrameCallback(void);

/**
 * Copies the 0x2B halfwords of I/O registers from DISPCNT on into their copies
 * (gDispcntCopy) with the BIOS (CpuSet) and installs Main_FrameCallback as the frame callback.
 */
void Main_InstallFrameCallback(void)
{
    CpuSet((void *)REG_BASE, &gDispcntCopy, 0x2B);
    gMain.frameCallback = Main_FrameCallback;
}
/**
 * The frame callback after the VBlank wait (Main_InstallFrameCallback installs it): copies gOamBuffer to
 * OAM, the layers' positions (gBgLayers hofs/vofs) into the BG0HOFS-BG3VOFS copies,
 * the I/O register copies (gDispcntCopy: DISPCNT, then 0x27 halfwords from BG0CNT) to the
 * hardware, runs Main_RunTransfersBios, and copies each layer's buffer to its VRAM map when it
 * is marked dirty.
 */
void Main_FrameCallback(void)
{
    struct BgLayer *layer;
    s32 i;

    CpuFastSet(gOamBuffer, (void *)0x07000000, 0x100);
    // hofs and vofs together, as BGxHOFS and BGxVOFS
    gBgOfsCopies[0] = *(u32 *)&gBgLayers[0].hofs;
    gBgOfsCopies[1] = *(u32 *)&gBgLayers[1].hofs;
    gBgOfsCopies[2] = *(u32 *)&gBgLayers[2].hofs;
    gBgOfsCopies[3] = *(u32 *)&gBgLayers[3].hofs;
    REG_DISPCNT = gDispcntCopy;
    CpuSet(gBgCntCopies, (void *)(REG_BASE + REG_OFFSET_BG0CNT), 0x27);
    Main_RunTransfersBios();
    layer = gBgLayers;
    for (i = 3; i >= 0; i--)
    {
        if ((s8)layer->dirty != 0)
        {
            CpuFastSet(layer->buffer, layer->mapVram, layer->mapSize >> 2);
            layer->dirty = 0;
        }
        layer++;
    }
}

/**
 * Runs the transfers queued in gTransferQueue (gTransferCount of them) through DMA 3, then
 * empties the queue: mode 0 copies size bytes of halfwords, mode 1 fills them with the low
 * halfword of src itself, mode 2 copies size >> 8 blocks of (size & 0xFF) * 32 bytes of
 * words, each to the next 0x400 bytes of dst. Byte-identical to the original: a slower
 * version changed the frame's pixels (the transfers run in VBlank).
 */
void Main_RunTransfersDma(void)
{
    struct Transfer *e = gTransferQueue;
    s32 i;
    vu16 fill; // the DMA reads it: volatile, so the store comes first
    const u8 *src;
    u8 *dst;
    u32 blocks, blockSize;

    for (i = 0; i < gTransferCount; e++, i++)
    {
        switch (e->mode)
        {
        case 0:
            DMA3_SET(e->src, e->dst, DMA_ENABLE | DMA_16BIT | (e->size >> 1));
            break;
        case 1:
            fill = (u32)e->src;
            DMA3_SET(&fill, e->dst, DMA_ENABLE | DMA_SRC_FIXED | DMA_16BIT | (e->size >> 1));
            break;
        case 2:
            src = e->src;
            dst = e->dst;
            blocks = e->size >> 8;
            blockSize = (e->size & 0xFF) * 32;
            while (blocks != 0)
            {
                DMA3_SET(src, dst, DMA_ENABLE | DMA_32BIT | (blockSize >> 2));
                src += blockSize;
                dst += 0x400;
                blocks--;
            }
            break;
        }
    }
    gTransferCount = 0;
}
/**
 * Runs the transfers queued in gTransferQueue (gTransferCount of them) with the BIOS instead
 * of DMA, then empties the queue: mode 0 copies size bytes of halfwords (CpuSet), mode 1
 * fills them with the low halfword of src itself (CpuSet's fill), mode 2 copies size >> 8
 * blocks of (size & 0xFF) * 32 bytes (CpuFastSet), each to the next 0x400 bytes of dst.
 */
void Main_RunTransfersBios(void)
{
    s32 i;
    struct Transfer *e = gTransferQueue;
    vu16 fill; // volatile like the SDK's CpuFill16 temporary
    const u8 *src;
    u8 *dst;
    u32 blocks, blockSize;

    for (i = 0; i < gTransferCount; e++, i++)
    {
        switch (e->mode)
        {
        case 0:
            CpuSet(e->src, e->dst, e->size >> 1);
            break;
        case 1:
            fill = (u32)e->src;
            CpuSet((void *)&fill, e->dst, (e->size >> 1) | 0x01000000);
            break;
        case 2:
            src = e->src;
            dst = e->dst;
            blocks = e->size >> 8;
            blockSize = (e->size & 0xFF) * 32;
            while (blocks != 0)
            {
                CpuFastSet(src, dst, (blockSize >> 2) & 0x1FFFFF);
                src += blockSize;
                dst += 0x400;
                blocks--;
            }
            break;
        }
    }
    gTransferCount = 0;
}
/**
 * Queues a copy of *size* bytes from *src* to *dst* (mode 0) for Main_RunTransfersDma; -1 when the
 * 0x40 entries of gTransferQueue are taken, else 0.
 */
s32 Main_QueueCopy(const void *src, void *dst, u16 size)
{
    u16 n = gTransferCount;

    if (n >= 0x40)
        return -1;
    QUEUE_ENTRY.src = src;
    QUEUE_ENTRY.dst = dst;
    QUEUE_ENTRY.size = size;
    QUEUE_ENTRY.mode = 0;
    gTransferCount = n + 1;
    return 0;
}
/**
 * Queues a fill of *size* bytes at *dst* with the low halfword of *src* itself (mode 1) for
 * Main_RunTransfersDma; -1 when the 0x40 entries of gTransferQueue are taken, else 0.
 */
s32 Main_QueueFill(const void *src, void *dst, u16 size)
{
    u16 n = gTransferCount;

    if (n >= 0x40)
        return -1;
    QUEUE_ENTRY.src = src;
    QUEUE_ENTRY.dst = dst;
    QUEUE_ENTRY.size = size;
    QUEUE_ENTRY.mode = 1;
    gTransferCount = n + 1;
    return 0;
}
/**
 * Queues *blocks* blocks of *blockTiles* * 32 bytes from *src*, each to the next 0x400
 * bytes of *dst* (mode 2, size blocks << 8 | blockTiles) for Main_RunTransfersDma; -1 when the 0x40
 * entries of gTransferQueue are taken, else 0.
 */
s32 Main_QueueBlockCopy(const void *src, void *dst, u8 blockTiles, u8 blocks)
{
    u16 n = gTransferCount;

    if (n >= 0x40)
        return -1;
    gTransferQueue[gTransferCount].src = src;
    gTransferQueue[gTransferCount].dst = dst;
    gTransferQueue[gTransferCount].size = (blocks << 8) | blockTiles;
    gTransferQueue[gTransferCount].mode = 2;
    gTransferCount = n + 1;
    return 0;
}
/**
 * Copies *blocks* blocks of *blockTiles* * 32 bytes from *src* at once, each to the next
 * 0x400 bytes of *dst* (16-bit DMA 3): Main_QueueBlockCopy's queue entry done right away.
 */
void Main_BlockCopy(const u8 *src, u8 *dst, u8 blockTiles, u8 blocks)
{
    u32 blockSize = blockTiles * 32;

    while (blocks != 0)
    {
        DMA3_SET(src, dst, DMA_ENABLE | DMA_16BIT | (blockSize >> 1));
        src += blockSize;
        dst += 0x400;
        blocks--;
    }
}
/**
 * Sets up *heap* on the *size* bytes at *mem*: one free block that fills them, in a ring
 * with the heap's own head block.
 *
 * Declared to return a value it never sets (no caller uses it), as the original evidently was
 * (an implicit int): agbcc then keeps r0 live, copies the heap to r3 and saves r4. As a void
 * function it is a push, a pop and a move shorter, and a level start that sets up its heap
 * ends its frame earlier than the original (save_adventure, The Last Land 3).
 */
s32 Heap_Init(void *heap, void *mem, u32 size)
{
    struct Heap *h = heap;
    struct HeapBlock *b = mem;

    h->size = size;
    h->rover = b;
    h->head.used = 1;
    h->head.prev = b;
    h->head.next = b;
    h->head.size = 0;
    b->used = 0;
    b->prev = &h->head;
    b->next = &h->head;
    b->size = size;
}
/**
 * Gives the memory *p* (from Heap_Alloc) back to *heap*: its block is no longer in use
 * and merges with a free previous and a free next block; the heap's rover moves to the
 * result. Returns 0.
 */
s32 Heap_Free(void *heap, void *p)
{
    struct Heap *h = heap;
    struct HeapBlock *b = (struct HeapBlock *)p - 1;
    struct HeapBlock *prev = b->prev;
    struct HeapBlock *next = b->next;

    b->used = 0;
    if (!prev->used)
    {
        prev->next = next;
        prev->size += b->size;
        next->prev = prev;
        b = prev;
    }
    if (!next->used)
    {
        struct HeapBlock *after = next->next;

        b->next = after;
        b->size += next->size;
        after->prev = b;
    }
    h->rover = b;
    return 0;
}
/**
 * Hands out *size* bytes (rounded up to a word) of *heap*: from the rover on, the first
 * free block big enough. A block with more than 0x13 bytes to spare is split, the rest
 * staying free after it. The block is marked in use, the rover moves past it and its
 * memory is cleared (CpuSet, 32-bit fill). Returns the memory after the block's header, or
 * NULL when no block is big enough.
 */
void *Heap_Alloc(void *heap, u32 size)
{
    struct Heap *h = heap;
    struct HeapBlock *b;
    struct HeapBlock *start;
    struct HeapBlock *rest;
    struct HeapBlock *next;
    void *result = NULL;
    u32 total;
    u32 left;
    u32 zero;

    size = (size + 3) & ~3;
    total = size + sizeof(struct HeapBlock);
    b = start = h->rover;
    do
    {
        if (!b->used)
        {
            left = b->size - sizeof(struct HeapBlock);
            if (left >= size)
            {
                left -= size;
                if (left > 0x13)
                {
                    b->size = total;
                    rest = (struct HeapBlock *)((u8 *)b + total);
                    rest->used = 0;
                    rest->prev = b;
                    rest->next = b->next;
                    rest->size = left;
                    next = b->next;
                    b->next = rest;
                    next->prev = rest;
                }
                b->used = 1;
                h->rover = b->next;
                zero = 0;
                CpuSet(&zero, b + 1, 0x05000000 | (((b->size - sizeof(struct HeapBlock)) / 4) & 0x1FFFFF));
                result = b + 1;
                break;
            }
        }
        b = b->next;
    } while (b != start);
    return result;
}
