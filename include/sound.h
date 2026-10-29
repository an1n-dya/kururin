// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_SOUND_H
#define GUARD_SOUND_H

// src/sound.c (080181FC-08018330): song starts with the sound mode and song tasks. The functions
// other modules call.

#include "game.h"

void Sound_PlaySong(s32 a);
void Sound_PlaySongOrContinue(s32 a);
void Sound_UpdateDelayedSong(struct Task *task);
void Sound_UpdateDelayedSongOnly(struct Task *task);
void Sound_UpdateSongAfterPlayer(struct SongAfterPlayer *task);

#endif // GUARD_SOUND_H
