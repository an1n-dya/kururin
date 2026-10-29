// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// sio: provisional translation unit, 0801A250-: a block transfer over the serial port in
// 32-bit normal mode, with its serial interrupt handler. No public SDK source is known for it
// (it is not multiboot.c), so it is reconstructed from the code; library code: every
// function byte-matches the original (CLAUDE.md).

#include "global.h"

void CpuSet(const void *src, void *dest, u32 control);

#define SIO32_WORDS 0x1C80 // words of the block (0x7200 bytes)

// The transfer state; game.c reads unk3 as gUnk_03004860[3].
struct Sio32Transfer
{
    /*0x00*/ u8 unk0;      // 1: this side drives the clock and sends (sub_0801A250's first argument)
    /*0x01*/ u8 unk1;      // step of sub_0801A2E8
    /*0x02*/ u8 unk2;      // frames in the current step
    /*0x03*/ u8 unk3;      // 1: the received block's sum matched
    /*0x04*/ u32 *unk4;    // the block
    /*0x08*/ s32 unk8;     // index of the word being transferred (-1: not started)
    /*0x0C*/ u32 unkC;     // the sender's ~sum of the block, sent after it
    /*0x10*/ u32 unk10;    // the sum of the words checked so far
    /*0x14*/ s32 unk14;    // words checked so far
};

extern struct Sio32Transfer gUnk_03004860;
extern u32 gUnk_03003EF0[0x40];

void sub_0801A478(void);

void sub_0801A250(u32 a, u32 *data)
{
    u32 sum = 0;
    u32 zero;
    u32 *p;
    s32 i;

    REG_IE &= ~(INTR_FLAG_TIMER3 | INTR_FLAG_SERIAL);
    zero = 0;
    CpuSet(&zero, &gUnk_03004860, 0x05000000 | sizeof(gUnk_03004860) / 4);
    CpuSet(sub_0801A478, gUnk_03003EF0, 0x04000000 | sizeof(gUnk_03003EF0) / 4);
    *(vu32 *)REG_ADDR_SIOCNT = SIO_MULTI_MODE;
    gUnk_03004860.unk4 = data;
    gUnk_03004860.unk8 = -1;
    if (a != 0)
    {
        REG_TM3CNT = 0;
        gUnk_03004860.unk0 = 1;
        p = data;
        for (i = SIO32_WORDS; i != 0; i--)
            sum += *p++;
        gUnk_03004860.unkC = ~sum;
        REG_SIOCNT = SIO_32BIT_MODE;
        REG_SIOCNT = SIO_32BIT_MODE | 1;
    }
}

// One step of the transfer per frame. progress (may be NULL) gets the words transferred so
// far, 0..SIO32_WORDS; returns 1 once the transfer has ended (all words and the sum done, or
// 0x79 frames without it), else 0.
s32 sub_0801A2E8(s32 *progress)
{
    s32 index, n;

    switch (gUnk_03004860.unk1)
    {
    case 0:
        // unk0 or unk2 (one word load of the struct's first four bytes)
        if (*(u32 *)&gUnk_03004860 & 0x00FF00FF)
            gUnk_03004860.unk1 = 1;
        break;
    case 1:
        if (gUnk_03004860.unk0 == 1)
        {
            if (gUnk_03004860.unk2 <= 5)
                break;
        }
        else
        {
            REG_SIOCNT = SIO_32BIT_MODE;
        }
        REG_SIODATA32 = 0;
        REG_IF = INTR_FLAG_TIMER3 | INTR_FLAG_SERIAL;
        if (gUnk_03004860.unk0 == 1)
        {
            REG_SIOCNT |= SIO_START;
            REG_TM3CNT = 0x00C0EF00;
            REG_IE |= INTR_FLAG_TIMER3;
        }
        else
        {
            REG_SIOCNT |= SIO_INTR_ENABLE | SIO_START;
            REG_IE |= INTR_FLAG_SERIAL;
        }
        gUnk_03004860.unk2 = 0;
        gUnk_03004860.unk1 = 2;
        break;
    case 2:
        index = gUnk_03004860.unk8;
        n = index;
        if (n > SIO32_WORDS)
            n = SIO32_WORDS;
        else if (n < 0)
            n = 0;
        if (progress != NULL)
            *progress = n;
        if (gUnk_03004860.unk0 != 1)
        {
            for (; gUnk_03004860.unk14 < n; gUnk_03004860.unk14++)
                gUnk_03004860.unk10 += gUnk_03004860.unk4[gUnk_03004860.unk14];
            if (index > SIO32_WORDS)
            {
                gUnk_03004860.unkC += gUnk_03004860.unk10;
                if (gUnk_03004860.unkC == -1)
                    gUnk_03004860.unk3 = 1;
            }
        }
        if (index > SIO32_WORDS || gUnk_03004860.unk2 == 0x79)
            gUnk_03004860.unk1 = 3;
        break;
    case 3:
        REG_IE &= ~(INTR_FLAG_TIMER3 | INTR_FLAG_SERIAL);
        REG_SIOCNT = SIO_32BIT_MODE;
        *(vu32 *)REG_ADDR_SIOCNT = SIO_MULTI_MODE;
        REG_SIOMLT_RECV = 0;
        if (gUnk_03004860.unk0 != 0)
            REG_TM3CNT = 0;
        REG_IF = INTR_FLAG_TIMER3 | INTR_FLAG_SERIAL;
        return 1;
    }
    gUnk_03004860.unk2++;
    return 0;
}

// The serial (receiver) or timer 3 (sender) interrupt handler: sub_0801A250 copies it to
// gUnk_03003EF0 and it runs from there. Each interrupt moves one word: 0xFEFEFEFE until the
// transfer starts (index -1), then the block, then the sum.
void sub_0801A478(void)
{
    vu32 *sioData = &REG_SIODATA32;
    u32 data = *sioData;

    if (gUnk_03004860.unk0 != 1)
        REG_SIOCNT |= SIO_START;

    if (gUnk_03004860.unk0 == 1)
    {
        if (gUnk_03004860.unk8 < 0)
            *sioData = 0xFEFEFEFE;
        else if (gUnk_03004860.unk8 < SIO32_WORDS)
            *sioData = gUnk_03004860.unk4[gUnk_03004860.unk8];
        else
            *sioData = gUnk_03004860.unkC;
    }
    else if (gUnk_03004860.unk8 < 0)
    {
        if (data != 0xFEFEFEFE)
            gUnk_03004860.unk8--; // stays at -1 until the start word
    }
    else if (gUnk_03004860.unk8 < SIO32_WORDS)
    {
        gUnk_03004860.unk4[gUnk_03004860.unk8] = data;
    }
    else
    {
        gUnk_03004860.unkC = data;
    }

    if (gUnk_03004860.unk8 < SIO32_WORDS + 3)
    {
        gUnk_03004860.unk8++;
        if (gUnk_03004860.unk0 == 1)
            REG_SIOCNT |= SIO_START;
    }
}
