// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// sound: song starts with the sound mode and song tasks.
// 080181FC-08018330, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "task.h"
#include "sound.h"

/**
 * Starts song *song* (m4aSongNumStart), first passing gSongSoundModes[song] << 8 to m4aSoundMode
 * when gSoundInfo.maxChans differs from it.
 */
void Sound_PlaySong(s32 song)
{
    if (gSoundInfo.maxChans != gSongSoundModes[song])
        m4aSoundMode(gSongSoundModes[song] << 8);
    m4aSongNumStart(song);
}
/**
 * Sound_PlaySong with m4aSongNumStartOrChange for the song instead of m4aSongNumStart.
 */
void Sound_PlaySongOrChange(s32 song)
{
    if (gSoundInfo.maxChans != gSongSoundModes[song])
        m4aSoundMode(gSongSoundModes[song] << 8);
    m4aSongNumStartOrChange(song);
}
void m4aSongNumStartOrContinue(u16 song);

/**
 * Sound_PlaySong with m4aSongNumStartOrContinue for the song instead of m4aSongNumStart.
 */
void Sound_PlaySongOrContinue(s32 song)
{
    if (gSoundInfo.maxChans != gSongSoundModes[song])
        m4aSoundMode(gSongSoundModes[song] << 8);
    m4aSongNumStartOrContinue(song);
}
/**
 * A task: while gGameState.paused is 0, counts unkA down a frame at a time; at 0 it calls
 * Sound_PlaySong(unk8) (which ends in m4aSongNumStart) and frees itself.
 */
void Sound_UpdateDelayedSong(struct Task *task)
{
    if (gGameState.paused == 0)
    {
        if (task->unkA == 0)
        {
            Sound_PlaySong(task->unk8);
            Task_Destroy(task);
        }
        else
            task->unkA--;
    }
}
/**
 * Like Sound_UpdateDelayedSong, but at 0 it starts unk8 with m4aSongNumStart directly.
 */
void Sound_UpdateDelayedSongOnly(struct Task *task)
{
    if (gGameState.paused == 0)
    {
        if (task->unkA == 0)
        {
            m4aSongNumStart(task->unk8);
            Task_Destroy(task);
        }
        else
            task->unkA--;
    }
}
/**
 * Waits until the music player *player* has no track playing (the low halfword of its status),
 * then counts delay down; at 0 it calls Sound_PlaySong(song) and frees itself. Paused while
 * gGameState.paused is set.
 */
void Sound_UpdateSongAfterPlayer(struct SongAfterPlayer *task)
{
    if (gGameState.paused == 0 && (u16)task->player->status == 0)
    {
        if (task->delay == 0)
        {
            Sound_PlaySong(task->song);
            Task_Destroy((struct Task *)task);
        }
        else
            task->delay--;
    }
}
