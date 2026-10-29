// No licence is granted for this file: it derives from pret's reconstruction of Nintendo SDK
// code, which carries no licence (NOTICE.md).
// The SDK's MPlayContinue (calibration: the ident write-back decides the register).
// A leaf with a branch and a bare bx lr, so not plain agbcc.

#include "gba/types.h"
#include "m4a.h"

void MPlayContinue(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->status &= ~MUSICPLAYER_STATUS_PAUSE;
        mplayInfo->ident = ID_NUMBER;
    }
}
