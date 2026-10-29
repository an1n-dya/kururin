// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// link: the link cable: multi-player setup and the per-frame exchange.
// 08003C18-08004030, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "link.h"

// 0x03003FF0: the link cable state of the serial interrupt routines sub_0800019C and
// sub_080001DC (crt0, ARM), which Link_Init copies to IWRAM and sets up.
struct LinkState {
    /*0x000*/ u8 unk0;         // 8: Link_Exchange started the transfers (the parent)
    /*0x001*/ u8 state;        // Link_Exchange: 0, then 1
    /*0x002*/ u8 unk2;         // Link_Init's argument; Link_Exchange returns it in bits 8-15
    /*0x003*/ u8 unk3;         // Link_Exchange returns it in its low byte
    /*0x004*/ u8 unk4[5];
    /*0x009*/ u8 unk9_0:4;
    /*0x009*/ u8 unk9_4:1;     // Link_Exchange returns bits 4-6
    /*0x009*/ u8 unk9_5:1;
    /*0x009*/ u8 unk9_6:1;     // also in every packet sent (Link_Send)
    /*0x009*/ u8 unk9_7:1;
    /*0x00A*/ u8 unkA;         // 0xFF from Link_Init
    /*0x00B*/ u8 unkB;         // Link_Exchange counts it up every call
    /*0x00C*/ u8 unkC[8];
    /*0x014*/ s32 unk14;       // -1 from Link_Init; sub_080001DC counts it up to 0xF
    /*0x018*/ u8 unk18[0x10];
    /*0x028*/ u16 *unk28;      // sendBuf[0] and [1] (sub_080001DC swaps them)
    /*0x02C*/ u16 *unk2C;
    /*0x030*/ u16 *unk30[4];   // per player: recvBuf[n][0], [1] and [2]
    /*0x040*/ u16 *unk40[4];
    /*0x050*/ u16 *unk50[4];
    /*0x060*/ u16 sendBuf[2][0x10];
    /*0x0A0*/ u16 recvBuf[4][3][0x10];
};
extern struct LinkState gLinkState;
void sub_0800019C(void);
void sub_080001DC(void);
// IWRAM space Link_Init copies sub_0800019C (0x40 bytes) and sub_080001DC (0x100) to
extern u32 gUnk_03004210[0x10];
extern u32 gUnk_03003EF0[0x40];

/**
 * Sets up the link cable (multi-player mode) with *a* in gLinkState.unk2: with the
 * timer 3 and serial interrupts off, RCNT 0 and SIOCNT 0x2000 then | 0x4003 (multi-player,
 * 115200 bps, interrupt on), clears gLinkState, copies the ARM routines sub_0800019C
 * and sub_080001DC to IWRAM, points the state at its buffers and enables the serial
 * interrupt again.
 */
void Link_Init(s32 a)
{
    u32 zero;
    s32 i;

    REG_IE &= ~0xC0;
    REG_RCNT = 0;
    *(vu32 *)REG_ADDR_SIOCNT = 0x2000;
    REG_SIOCNT |= 0x4003;
    zero = 0;
    CpuSet(&zero, &gLinkState, 0x05000000 | sizeof(gLinkState) / 4);
    CpuSet(sub_0800019C, gUnk_03004210, 0x04000000 | sizeof(gUnk_03004210) / 4);
    CpuSet(sub_080001DC, gUnk_03003EF0, 0x04000000 | sizeof(gUnk_03003EF0) / 4);
    gLinkState.unk2 = a;
    gLinkState.unkA = 0xFF;
    gLinkState.unk14 = -1;
    gLinkState.unk28 = gLinkState.sendBuf[0];
    gLinkState.unk2C = gLinkState.sendBuf[1];
    for (i = 0; i < 4; i++)
    {
        gLinkState.unk30[i] = gLinkState.recvBuf[i][0];
        gLinkState.unk40[i] = gLinkState.recvBuf[i][1];
        gLinkState.unk50[i] = gLinkState.recvBuf[i][2];
    }
    REG_IE |= 0x80;
}
u32 Link_Receive(struct LinkData *recv);
void Link_Send(u16 *send, s32 a);

/**
 * A frame of the link cable exchange. The first call starts it: the parent (SIOCNT
 * multi-player ID 0) that finds the cable ready (RCNT: SD set, SI clear, SC set) and
 * gLinkState.unk14 still -1 moves the transfer interrupt from serial to timer 3 (its
 * reload 0xC352), starts the transfer and sets unk0 to 8; without SD it does nothing yet.
 * Then (and on every later call) it takes what was received (Link_Receive into *recv*) and
 * hands over *send* (Link_Send with *a*). Counts unkB up and returns unk3, bits 4-6 of
 * unk9 (unk9_4-6), 0x80 for the parent and unk2 << 8.
 */
u32 Link_Exchange(u16 *send, struct LinkData *recv, s32 a)
{
    vu8 *sio;
    u8 siocnt;
    u8 rcnt;
    u32 flags;
    u32 players;

    switch (gLinkState.state)
    {
    case 0:
        // SIOCNT's low and high byte
        sio = (vu8 *)REG_ADDR_SIOCNT;
        siocnt = sio[0];
        if ((siocnt & 0x30) == 0)
        {
            rcnt = *(vu8 *)REG_ADDR_RCNT;
            if (!(rcnt & 2))
                break;
            if ((rcnt & 5) == 1 && gLinkState.unk14 == -1)
            {
                REG_IE &= ~0x80;
                REG_IE |= 0x40;
                sio[1] &= ~0x40;
                REG_IF = 0xC0;
                REG_TM3CNT = 0xC352;
                gLinkState.unk0 = 8;
                sio[0] = siocnt | 0x80;
            }
        }
        gLinkState.state = 1;
        // fall through
    case 1:
        Link_Receive(recv);
        Link_Send(send, a);
        break;
    }
    gLinkState.unkB++;
    flags = gLinkState.unk3;
    flags |= gLinkState.unk9_4 << 4;
    flags |= gLinkState.unk9_5 << 5;
    flags |= gLinkState.unk9_6 << 6;
    players = gLinkState.unk2 << 8;
    if (gLinkState.unk0 == 8)
        return 0x80 | players | flags;
    return flags | players;
}
// A packet of the link exchange, in gLinkState's sendBuf / recvBuf (Link_Send fills
// the one gLinkState.unk28 points to): 14 halfwords.
struct LinkPacket {
    /*0x00*/ u8 frame;    // gLinkState.unkB
    /*0x01*/ u8 unk1_0:4; // (unk2 ^ unk3) & 0xF
    /*0x01*/ u8 unk1_4:1; // Link_Send's argument
    /*0x01*/ u8 unk1_5:1; // gUnk_03004860[3] bit 0
    /*0x01*/ u8 unk1_6:1; // gLinkState.unk9_6
    /*0x01*/ u8 unk1_7:1;
    /*0x02*/ u16 checksum; // ~(sum of the 14 halfwords, this one 0)
    /*0x04*/ u32 data[6];
};
extern u8 gUnk_03004860[4];

/**
 * Hands *send* (6 words) to the link exchange: fills the packet gLinkState.unk28 points
 * to (struct LinkPacket) with the frame counter, the player bits, *a* and its checksum,
 * then copies unkA (sign-extended) to unk14; the parent (unk0 set) stops timer 3 around
 * that copy (TM3CNT_H 0, then 0xC0: running, with its interrupt).
 */
void Link_Send(u16 *send, s32 a)
{
    u32 sum = 0;
    u32 i;

    ((struct LinkPacket *)gLinkState.unk28)->unk1_4 = a;
    ((struct LinkPacket *)gLinkState.unk28)->unk1_5 = gUnk_03004860[3];
    ((struct LinkPacket *)gLinkState.unk28)->unk1_6 = gLinkState.unk9_6;
    ((struct LinkPacket *)gLinkState.unk28)->frame = gLinkState.unkB;
    ((struct LinkPacket *)gLinkState.unk28)->unk1_0 = gLinkState.unk2 ^ gLinkState.unk3;
    ((struct LinkPacket *)gLinkState.unk28)->checksum = 0;
    CpuSet(send, ((struct LinkPacket *)gLinkState.unk28)->data, 0x04000006);
    for (i = 0; i < 14; i++)
        sum += gLinkState.unk28[i];
    ((struct LinkPacket *)gLinkState.unk28)->checksum = ~sum;
    if (gLinkState.unk0 != 0)
        REG_TM3CNT_H = 0;
    gLinkState.unk14 = (s8)gLinkState.unkA;
    if (gLinkState.unk0 != 0)
        REG_TM3CNT_H = 0xC0;
}
/**
 * Takes what each player sent from gLinkState's receive buffers (Link_Exchange). The IWRAM
 * copy of sub_0800019C returns one byte per player, non-zero when its packet arrived; for
 * each player whose packet arrived with a valid checksum (its 14 halfwords sum to 0xFFFF in
 * the low 16 bits) it sets bit n of unk3, ORs the packet's unk1_5 into bit n of unk9_0 and
 * copies the packet's 6 data words to recv[n]. Every player's data words are then cleared.
 * unk2 collects unk3's bits. When player 0's packet arrived: the parent (unk0 8) sets unk9_4
 * when unk3 has player 0 or 1 and is all of unk2, and sets unk9_6 when unk9_0 and unk2 agree
 * in bits 1-3; any other console ORs player 0's unk1_6 into unk9_6; both OR player 0's
 * unk1_4 into unk9_5. Returns unk3.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
u32 Link_Receive(struct LinkData *recv)
{
    union {
        u32 word;
        u8 player[4];
    } arrived;
    u32 (*poll)(void) = (u32 (*)(void))gUnk_03004210;
    u32 zero;
    s32 i;

    arrived.word = poll();
    gLinkState.unk3 = 0;
    for (i = 0; i < 4; i++)
    {
        struct LinkPacket *packet = (struct LinkPacket *)gLinkState.unk50[i];
        u32 sum = 0;
        u32 j;

        for (j = 0; j < 14; j++)
            sum += gLinkState.unk50[i][j];
        if (arrived.player[i] != 0 && (s16)sum == -1)
        {
            gLinkState.unk3 |= 1 << i;
            gLinkState.unk9_0 |= packet->unk1_5 << i;
            CpuSet(packet->data, &recv[i], 0x04000006);
        }
        zero = 0;
        CpuSet(&zero, ((struct LinkPacket *)gLinkState.unk50[i])->data, 0x05000006);
    }
    gLinkState.unk2 |= gLinkState.unk3;
    if (gLinkState.unk3 & 1)
    {
        struct LinkPacket *packet = (struct LinkPacket *)gLinkState.unk50[0];

        if (gLinkState.unk0 == 8)
        {
            if ((gLinkState.unk3 & 3) && gLinkState.unk3 == gLinkState.unk2)
                gLinkState.unk9_4 = 1;
            if ((gLinkState.unk9_0 & 0xE) == (gLinkState.unk2 & 0xE))
                gLinkState.unk9_6 = 1;
        }
        else
            gLinkState.unk9_6 |= packet->unk1_6;
        gLinkState.unk9_5 |= packet->unk1_4;
    }
    return gLinkState.unk3;
}
