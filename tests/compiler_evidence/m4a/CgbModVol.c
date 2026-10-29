// No licence is granted for this file: it derives from pret's reconstruction of Nintendo SDK
// code, which carries no licence (NOTICE.md).
// The SDK's CgbModVol (pret's m4a.c, with its inline CgbPan). The ROM loads
// rightVolume/leftVolume next to their add (ldrb r2,[r1,#3]; ldrb r3,[r1,#2]; adds),
// which is old_agbcc's fold of single-use byte loads into the operation; agbcc, with or
// without -fprologue-bugfix, keeps them apart (88%).

#include "gba/types.h"
#include "m4a.h"

static inline int CgbPan(struct CgbChannel *chan)
{
    u32 rightVolume = chan->rightVolume;
    u32 leftVolume = chan->leftVolume;

    if ((rightVolume = (u8)rightVolume) >= (leftVolume = (u8)leftVolume))
    {
        if (rightVolume / 2 >= leftVolume)
        {
            chan->pan = 0x0F;
            return 1;
        }
    }
    else
    {
        if (leftVolume / 2 >= rightVolume)
        {
            chan->pan = 0xF0;
            return 1;
        }
    }

    return 0;
}

void CgbModVol(struct CgbChannel *chan)
{
    if (!CgbPan(chan))
    {
        chan->pan = 0xFF;
        chan->envelopeGoal = (u32)(chan->rightVolume + chan->leftVolume) >> 4;
    }
    else
    {
        chan->envelopeGoal = (u32)(chan->rightVolume + chan->leftVolume) >> 4;
        if (chan->envelopeGoal > 15)
            chan->envelopeGoal = 15;
    }

    chan->sustainGoal = (chan->envelopeGoal * chan->sustain + 15) >> 4;
    chan->pan &= chan->panMask;
}
