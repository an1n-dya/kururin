// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_GAME_H
#define GUARD_GAME_H

// Declarations shared by the game's modules (src/*.c but the libraries: docs/modules.md):
// the structs, RAM/ROM data and library functions more than one of them uses. A module's
// own functions are declared in include/<module>.h, its private declarations in its .c.

#include "global.h"
#include "main.h"
#include "m4a.h"
#include "gba/multiboot.h"

// 0x03000D50: four 0x18-byte entries, each enabled by one of bits 0-3 of gLinkStatus
// (Link_FindCommandExact, Link_FindCommand).
// Play_RunLevel reads +4..+0xA as another Helirin's x/y pixels, angle (high byte) and
// progress (low byte), and flags: the layout of what it writes to gLinkSendData.
struct LinkData {
    /*0x00*/ u16 command; // Link_FindCommand, Link_FindCommandExact
    /*0x02*/ u16 unk2;
    /*0x04*/ u16 unk4;
    /*0x06*/ u16 unk6;
    /*0x08*/ u16 unk8;
    /*0x0A*/ u16 unkA;
    /*0x0C*/ u16 unkC;
    /*0x0E*/ u16 unkE; // entry 0: player 0's pause menu item (Pause_Update)
    /*0x10*/ u8 unk10[8];
};

// The argument of Menu_Run and Menu_Move (callers build it on the stack).
struct Menu {
    /*0x00*/ s8 itemCount; // Menu_Run calls func for items 0x100..0x100 + itemCount
    /*0x01*/ s8 current;   // the current item: Menu_Move moves it within 1..itemCount on keys 0x40/0x80
    /*0x02*/ u8 x;         // where item 0x100 puts gTextCursor
    /*0x03*/ u8 y;
    /*0x04*/ u8 unk4; // copy of x (Menu_Run)
    /*0x05*/ u8 unk5; // copy of y
    /*0x06*/ u8 unk6[2];
    /*0x08*/ s32 (*func)(struct Menu *, s32); // the menu function: the current item, 0xFFFF, 0x100.., 0x1FF
    /*0x0C*/ u16 unkC; // the callers store the keys they pass here
    /*0x0E*/ u16 unkE;
    /*0x10*/ u16 unk10; // MakeUp_Run: copies of gGameState.saveFile->type/unk19/unk1A
    /*0x12*/ u16 unk12;
    /*0x14*/ u16 unk14;
};

// A task (Task_Create, from gTaskHeap): Task_RunAll calls func with it every frame. The
// tasks' own structs start with the same func, key and state; the 0x18 bytes here are the
// size Hud_Init asks for.
struct Task {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;   // Task_FindByKey, Task_DestroyByKey; cleared by Task_Create
    /*0x06*/ u16 state; // cleared by Task_Create; the tasks switch on it
    /*0x08*/ u16 unk8;
    /*0x0A*/ u16 unkA;
    /*0x0C*/ u16 unkC;
    /*0x0E*/ u8 unkE;
    /*0x0F*/ u8 unkF;
    /*0x10*/ u8 unk10[0x1A];
};

// gTextCursor (0x03003EE8): where the Text_Print* functions and the menus draw: a position that
// steps by (dx, dy) per character/column and line/row, set up on item 0x100 from the menu's
// x/y.
struct TextCursor {
    /*0x00*/ u8 x;
    /*0x01*/ u8 y;
    /*0x02*/ s8 dx;
    /*0x03*/ u8 dy;
    /*0x04*/ u16 palette; // the sprites' palette ('#p' sets it)
    /*0x06*/ u16 list;    // low byte: the sprite list (Sprite_Alloc); bits 2-3: the sprites' priority
};

// KuruTools (tools/research/KuruTools/KuruRomExtractor/KuruRomExtractor/Levels.cs):
// WorldEntry, WorldInfo and LevelEntry; offsets are into the world data, each block is
// compressed (Decompress_LZ) and the sizes are uncompressed.
struct LevelEntry {
    // [0] level data, [1] graphical data, [2] background data, [3] minimap
    /*0x00*/ struct { u32 offset, size; } layers[4];
};

struct WorldInfo {
    /*0x00*/ u32 tilesOffset;            // addr1: "Graphical Tiles"
    /*0x04*/ u32 tilesSize;
    /*0x08*/ u32 bgTilesOffset;          // addr2: "Background tiles"
    /*0x0C*/ u32 bgTilesSize;
    /*0x10*/ u32 unk10[4];
    /*0x20*/ u32 paletteOffset;          // addr5: "Palette for background layers"
    /*0x24*/ u32 paletteSize;
    /*0x28*/ u32 trainingPaletteOffset;  // addr6: "... for the training mode"
    /*0x2C*/ u32 trainingPaletteSize;
    /*0x30*/ u32 colorSetsOffset;        // addr7: "Other versions of the first color set (32
    /*0x34*/ u32 colorSetsSize;          // first bytes)" (Effect_CyclePalette copies them to BG palette 0)
    /*0x38*/ u32 unk38[0x12];
    /*0x80*/ struct LevelEntry levels[0];
};

struct WorldEntry {
    /*0x00*/ const struct WorldInfo *info; // world_info_mem_address
    /*0x04*/ const u8 *data;               // world_data_mem_address
    /*0x08*/ u32 tileAnims;                // dummy1: {frames, copies} pairs (Level_StartTasks)
};

// gBgLayers: one per hardware background layer (Bg_SetVideoMode); the frame callback copies
// hofs/vofs to BGxHOFS/BGxVOFS and, when dirty, buffer to mapVram.
struct BgLayer {
    /*0x00*/ u16 hofs;       // BGxHOFS, the fine scroll (Bg_ScrollMap); Files_MainState clears both
    /*0x02*/ u16 vofs;       // BGxVOFS
    /*0x04*/ void *mapVram;  // its map in VRAM; Bg_LoadMap copies a map's rows there (0x40 bytes apart)
    /*0x08*/ u16 mapSize;    // its map's bytes; cleared at buffer when a level layer is missing
    /*0x0A*/ u8 unkA[2];
    /*0x0C*/ void *charVram; // its tiles in VRAM; Bg_LoadMap copies the 0x20-byte tiles of its list there
    /*0x10*/ u16 width;      // Bg_SetVideoMode: 0x20, the width and height in tiles
    /*0x12*/ u16 height;
    /*0x14*/ u8 dirty;       // set when buffer changed; the frame callback copies it and clears this
    /*0x15*/ u8 unk15[3];
    /*0x18*/ void *buffer;   // its map buffer in gBgLayerBuffers
};

// A TileCopy list: {source, destination tile, size in tiles} copied to OBJ VRAM
// (0x06010000 + tile * 32), ended by a NULL source (gMakeUpSpriteTiles, gPauseMenuTiles, ...).
struct TileCopy {
    /*0x00*/ const void *src;
    /*0x04*/ u16 tile;
    /*0x06*/ u16 tiles;
};

// 0x03000020: a table pointer, a counter set to 1 and the pointer's [1], like a Helirin's
// colorAnim/colorTimer/colorValue (Helirin_SetColor, Hud_Init).
struct Anim {
    /*0x00*/ const u16 *next;
    /*0x04*/ u16 timer;
    /*0x06*/ u16 value;
};

// The animation Anim_StartLong and Anim_StepLong step: like Anim, with 8-byte entries
// {s16 frames, u32 value} (frames < 0: jump by that many entries; 0: the end).
struct LongAnimFrame {
    /*0x00*/ s16 frames;
    /*0x02*/ u16 unk2;
    /*0x04*/ u32 value;
};
struct LongAnim {
    /*0x00*/ const struct LongAnimFrame *next; // the next entry
    /*0x04*/ u16 timer;                         // frames left of the current one
    /*0x06*/ u16 unk6;
    /*0x08*/ u32 value;                         // the current entry's value
};

// A task at (x, y) with an animation (Anim_Start, Anim_Step), drawn as one sprite at its screen
// position (Util_GetScreenPos): a spring (map codes 0xF8/0xF9, Object_Spring*) and the effects
// with the same layout (Effect_UpdateCrash, Effect_UpdateHitSpark, Effect_UpdateZoneHitRipple,
// Effect_UpdateSpringSpark).
struct AnimObject {
    /*0x00*/ void (*func)(struct AnimObject *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ s16 y;
    /*0x0C*/ s16 screen[2]; // x - gCameraX, y - gCameraY (Util_GetScreenPos)
    /*0x10*/ struct Anim anim;
};

// A sprite entry of gSprites (Sprite_Alloc): its first 8 bytes are an OAM entry, which
// Sprite_BuildOam copies to gOamBuffer; next links the entries of one of the 16 lists.
struct Sprite {
    /*0x00*/ u32 attr01; // OAM attributes 0 (low half: y, ...) and 1 (high half: x, ...)
    /*0x04*/ u16 attr2;  // OAM attribute 2: tile, priority, palette
    /*0x06*/ u16 unk6;
    /*0x08*/ s32 next; // the list's previous head, -1 at its end (Sprite_Alloc)
};
extern u32 gLinkStatus;
extern struct LinkData gLinkRecvData[4];

// The heaps Heap_Init sets up: a ring of blocks, each a 0x10-byte header followed by its
// memory (Heap_Alloc hands out header + 0x10, Heap_Free takes it back and merges free
// neighbours). The heap starts with a block of its own (size 0, always in use) that ends
// the ring.
struct HeapBlock {
    /*0x00*/ struct HeapBlock *next;
    /*0x04*/ struct HeapBlock *prev;
    /*0x08*/ u32 size; // bytes, header included
    /*0x0C*/ u32 used;
};
struct Heap {
    /*0x00*/ struct HeapBlock head;
    /*0x10*/ struct HeapBlock *rover; // where Heap_Alloc starts looking
    /*0x14*/ u32 size;
};
extern u16 gAffineParams[][4];
// gLivesSettings (0x03004520): 12 bytes copied from gDefaultLivesSettings (Lives_Reset); bytes,
// as the code reads them: Lives_Init sets a Helirin's lives from lives (modes 1, 2, 5, 7),
// challengeLives (3), debugLives (6) or linkLives[n] (4, Helirin n), its size from
// linkSizes[n] (4), and its flags bit 4 from bit 0 of invincible; ModeMenu_Run indexes
// linkLives[] by gLinkPlayer (the player number).
struct LivesSettings {
    /*0x00*/ u8 lives;
    /*0x01*/ u8 challengeLives;
    /*0x02*/ u8 debugLives; // mode 6, the debug stage select's play (Lives_Init)
    /*0x03*/ u8 linkLives[4];
    /*0x07*/ u8 linkSizes[4];
    /*0x0B*/ u8 invincible; // bit 0: Lives_SettingsScreen's "#h" muteki (invincible) line; Lives_Init
                            // gives the Helirin flags bit 4, which keeps it from crashing at 0 lives
};
extern struct LivesSettings gLivesSettings;
extern const s16 gSineTable[256];
extern const s16 gCosineTable[256];
extern s32 gCameraX;
extern s32 gCameraY;
extern const u8 gUnk_083A7C98[];
// The shapes Collision_TestShapes and Collision_FindPointInShapes test a Helirin against: 0xC-byte entries ended
// by type 0.
// Type 1: the rectangle from (x + dx, y + dy), w x h pixels; type 2: the circle of radius w
// around (x + dx, y + dy), h being the square of its radius.
struct CollisionShape {
    /*0x00*/ u8 type;
    /*0x01*/ u8 unk1[3];
    /*0x04*/ s16 dx;
    /*0x06*/ s16 dy;
    /*0x08*/ s16 w;
    /*0x0A*/ s16 h;
};
extern struct Map gWallMap; // gMaps[1]
// 4 bits per pixel, a u32 per row of 8 pixels (Collision_GetTilePixel)
extern const u32 gPhysicalTilesChallenge[][8];
extern struct TextCursor gTextCursor;
extern const u16 *const gMenuCursorColors[4];
extern u8 gLinkPlayer;
extern s32 gLinkTimer;
extern u16 gDispcntCopy;
extern u16 gBgCntCopies[4];
extern u16 gMosaicCopy;
extern u16 gBldalphaCopy;
extern u16 gBldyCopy;
extern u16 gWin0hCopy;
extern u16 gWin0vCopy;
extern u16 gWininCopy;
extern u16 gLinkSendData[12]; // 6 words: Link_Send copies them all (Vs_RunMenus fills [8..11] too)
extern u8 gLinkPlayers;
extern u8 gUnk_03000DB4;
extern s32 gWindowRadius;
extern struct Map gMaps[4];
extern const u32 gSongSoundModes[];
extern s32 gWindowY;
extern s32 gWindowX;
extern struct MusicPlayerInfo gMPlayInfo_BGM;
extern struct MusicPlayerInfo gMPlayInfo_SE2;
// Worlds 0-12; [13], all 0 in the ROM, is the debug stage select's world 13 (Debug_StageSelect,
// Level_StartTasks's song 12): its NULL info is read as is (the BIOS region on the GBA).
extern const struct WorldEntry gWorldEntries[14];
extern const struct WorldInfo *gWorldInfo;
extern const u8 *gWorldData;
extern struct BgLayer gBgLayers[4];
extern const u16 gLevelSpritePalettes[];
extern u16 gBldcntCopy;
extern const u16 gCollectionPalettes[];
extern const u8 gCollectionBgPalettes[];
extern const u16 gTextPalettes[];

// The moving objects' tasks LevelMap_StartMarkedObject starts (Task_Create) from the level map's codes
// (KuruTools KuruKuruKururin.md "Special Objects": their parameters); x/y are pixels, or 16.16
// fixed point in the two tasks that hold them as words.
// A Roller (map code 0xF7): fires an Object_RollerProjectile every period frames.
struct RollerObject {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ u16 x;
    /*0x0A*/ u16 y;
    /*0x0C*/ s16 screen[2];  // from Util_GetScreenPos
    /*0x10*/ u8 affineSet;   // dir rounded to eighths (0-7), as in Object_RollerCatcher
    /*0x11*/ s8 dir;         // a byte direction: KuruTools "Dir" (0-7) << 5
    /*0x12*/ u16 speed;      // KuruTools "Speed" of the projectiles (0xC0 from a map code)
    /*0x14*/ u16 timer;      // KuruTools "StartTime", then frames until the next projectile
    /*0x16*/ u16 period;     // KuruTools "Period": what timer restarts from
};
// A Roller Catcher (map code 0xF6): one sprite turned by dir.
struct RollerCatcherObject {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ u16 x;
    /*0x0A*/ u16 y;
    /*0x0C*/ s16 screen[2];  // from Util_GetScreenPos
    /*0x10*/ u8 affineSet;   // dir rounded to eighths (0-7), its sprite's affine set
    /*0x11*/ s8 dir;         // a byte direction: KuruTools "Dir" << 5
    /*0x12*/ u8 unk12[2];
};
// A Piston (map code 0xF5): Object_Piston swings x/y about (x0, y0) along dir (dirEighths: only
// 0, 2, 4 and 6 move), by gCosineTable[phase / 256] << 13, the phase advancing by phaseStep a
// frame and pausing waitPeriod frames (counted down in waitTimer) after each turn.
struct PistonObject {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
    /*0x10*/ s32 x0;
    /*0x14*/ s32 y0;
    /*0x18*/ s16 screen[2];  // from Util_GetScreenPos
    /*0x1C*/ u8 dirEighths;  // dir in eighths (0-7)
    /*0x1D*/ s8 dir;         // a byte direction: KuruTools "Dir" (0-3) << 6
    /*0x1E*/ u16 attr2;      // the sprite's OAM attribute 2
    /*0x20*/ u16 waitPeriod; // KuruTools "WaitPeriod" (0x78 from a map code)
    /*0x22*/ s16 waitTimer;  // KuruTools "StartTime", then the wait between moves
    /*0x24*/ s32 phaseStep;  // 0x10000 / KuruTools "MovePeriod"
    /*0x28*/ s32 phase;
};
// A Shooter (map code 0xF4): Object_Shooter turns towards the Helirin (within the arc arcMin..
// arcMax unless both are 0) and fires an Object_ShooterShot every period frames; x/y are 16.16
// fixed point.
struct ShooterObject {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
    /*0x10*/ s16 screen[2];  // from Util_GetScreenPos
    /*0x14*/ u8 affineSet;   // its sprite's affine set (Sprite_AllocAffine, or dir in eighths)
    /*0x15*/ s8 dir;         // a byte direction
    /*0x16*/ u8 arcMin;      // KuruTools "MinDir" << 5: the arc dir turns in (Object_TurnInArc)
    /*0x17*/ u8 arcMax;      // KuruTools "MaxDir" << 5
    /*0x18*/ struct Anim anim; // gShooterAnim, then gShooterFireAnim for each shot
    /*0x20*/ u16 timer;      // KuruTools "StartTime", then frames before the next shot
    /*0x22*/ u16 period;     // KuruTools "Period" (0x90 from a map code): timer after a shot
};
// The m4a calls the game makes beyond m4a.h (pret's m4a_internal.h): its gba/m4a.h API
void m4aSongNumStart(u16 song);
void m4aSongNumStartOrChange(u16 song);
void m4aMPlayAllStop(void);
void m4aMPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed);

// Results_Run's table: three records per level, from *gSaveBank + 0x9C4 (or + 0x1B34
// when gGameState.mode is 2); it draws name with Text_DrawName and time with Text_DrawTime.
struct LevelRecord {
    /*0x00*/ u32 time;
    /*0x04*/ char name[11];
    /*0x0F*/ u8 rank;
};
// gUnk_080C6284: the copies Effect_AnimateCopies queues with Main_QueueCopy, one per animation frame.
struct QueuedCopy {
    /*0x00*/ const void *src;
    /*0x04*/ void *dst;
    /*0x08*/ u16 size;
    /*0x0A*/ u16 unkA;
};
// The task Results_Run starts with Effect_AnimateCopies (0x14 bytes).
struct CopyAnim {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ const struct QueuedCopy *copies; // indexed by anim.value
    /*0x0C*/ struct Anim anim;
};
// The task Results_Run starts with Sound_UpdateSongAfterPlayer (0x10 bytes).
struct SongAfterPlayer {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ struct MusicPlayerInfo *player; // waited for until no track plays
    /*0x0C*/ u16 song;  // then started (Sound_PlaySong)
    /*0x0E*/ u16 delay; // frames after the player stopped
};
extern u8 *gSaveBank;
extern u8 *gSaveFiles;
extern u16 gTransferCount;
// One entry of the transfer queue Main_RunTransfersDma / Main_RunTransfersBios run at VBlank
// (gTransferCount entries): mode 0 copies size bytes of halfwords from src to dst
// (Main_QueueCopy), 1 fills them with the low halfword of the src value itself
// (Main_QueueFill), 2 copies size >> 8 blocks of (size & 0xFF) * 32 bytes, each to the next
// 0x400 bytes of dst (Main_QueueBlockCopy).
struct Transfer {
    /*0x00*/ const void *src;
    /*0x04*/ void *dst;
    /*0x08*/ u16 size;
    /*0x0A*/ u16 mode;
};
extern struct Transfer gTransferQueue[0x40];
// The C library functions the game calls (agbcc's libc.a)
char *strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
#ifdef PLATFORM_PC
void *memcpy(void *dst, const void *src, __SIZE_TYPE__ n);
void *memset(void *dst, int c, __SIZE_TYPE__ n);
#else
// agbcc's built-in memcpy/memset take an unsigned long size: with another type it warns and
// no longer expands them inline (constant-size copies become calls)
void *memcpy(void *dst, const void *src, unsigned long n);
void *memset(void *dst, int c, unsigned long n);
#endif
extern const u16 gLevelTimes[93 + 38][3];
extern const u16 gEasyLevelTimes[38][3]; // gLevelTimes[93]: the Easy Mode times, a label of their own
extern const char gDefaultRecordNames[3][10];
extern u16 gWinoutCopy;
extern const u16 gUnk_080C6270[];
extern const struct QueuedCopy gUnk_080C6284[];
// The copies Effect_AnimateBlockCopies queues with Main_QueueBlockCopy, one per animation frame (12 bytes each,
// by its index).
struct BlockCopy {
    /*0x00*/ const void *src;
    /*0x04*/ void *dst;
    /*0x08*/ u8 unk8;
    /*0x09*/ u8 unk9;
    /*0x0A*/ u16 unkA;
};
// The tasks Level_StartTasks starts with Effect_AnimateBlockCopies (0x14 bytes, one per pair of pointers in
// gWorldTileAnims's list), laid out like CopyAnim.
struct BlockCopyAnim {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ const struct BlockCopy *copies; // indexed by anim.value
    /*0x0C*/ struct Anim anim;
};
// gOamBuffer: what Main_FrameCallback copies to OAM. Each entry holds a sprite's attributes 0 and 1
// (attr01) and 2 (attr2); the affineParam of four entries in a row are one affine parameter set.
struct OamEntry {
    /*0x00*/ u32 attr01;
    /*0x04*/ u16 attr2;
    /*0x06*/ u16 affineParam;
};
extern struct OamEntry gOamBuffer[128];
extern const struct { u16 x, y; } gTrainingMapPositions[], gMapLevelPositions[];
// The tasks LevelMap_Read starts with Bonus_UpdateColor and Bonus_UpdateType (0x18 bytes, laid out as
// MapBonusTask): the level's bonus (touching it sets GAME_FLAG_BONUS_TAKEN), whose unk10 is
// swapped for a while with the Helirin's color (Bonus_UpdateColor) or type (Bonus_UpdateType).
struct BonusTask {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ s16 y;
    /*0x0C*/ s16 screen[2]; // from Util_GetScreenPos
    /*0x10*/ u16 value;     // the colour (its palette gColorBonusPalettes) or type (its tiles gTypeBonusTiles)
    /*0x12*/ u16 blink;     // 0-15: the colour of OBJ palette 0x050003DE, gMenuCursorColors[blink / 4]
    /*0x14*/ s16 timer;     // frames until the swap is undone (state 2)
};
extern const u8 gHelirinGfx[];
extern const u8 gMenuObjTiles[];
extern const u8 gMenuObjPalettes[];
extern const u8 gMenuCursorTiles[];
// The task LevelMap_Read starts with Bonus_UpdateItem (0x20 bytes, laid out as MapBonusTask): the
// bonus of bit unk10 (0-9) of gGameState.saveFile->unlocks, which Bonus_UpdateWornItem takes over when
// the Helirin touches it.
struct ItemBonus {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ s16 y;
    /*0x0C*/ s16 screen[2];
    /*0x10*/ u16 item;  // bit 0-9 of the save file's unlocks
    /*0x12*/ u16 blink; // 0-15: the colour of OBJ palette 0x050003DE, gMenuCursorColors[blink / 4]
    /*0x14*/ struct LongAnim anim; // gItemAnims[item]; value: the tiles to copy
};
// The tasks MakeUp_Run starts with Bonus_UpdateMakeUpIcon (0x1C bytes), one per set bit 0-9 of
// gGameState.saveFile->unlocks.
struct MakeUpIcon {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ u16 state;
    /*0x08*/ s16 x;
    /*0x0A*/ u16 y;
    /*0x0C*/ u16 item;   // the item, bit 0-9
    /*0x0E*/ u8 tile;    // its OBJ tile (0x164 + 4 * item)
    /*0x0F*/ u8 palette; // gMakeUpIconPalettes[item]
    /*0x10*/ struct LongAnim anim; // value: the tiles to copy
};
// The task Bonus_UpdateWornItem (0x34 bytes, started by MakeUp_StartWornItems, MakeUp_MenuItem and
// Bonus_UpdateItem with item = a bit of gGameState.saveFile->unlocks and point = a Helirin point);
// x/y and the velocity are 16.16 fixed point.
struct WornItem {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s32 x;
    /*0x0C*/ s32 y;
    /*0x10*/ s32 vx;    // +-0x400 a frame towards the Helirin, within +-0x10000 (state 4)
    /*0x14*/ s32 vy;
    /*0x18*/ s16 screen[2]; // from Util_GetScreenPos
    /*0x1C*/ struct Anim anim; // gWornItemFollowAnim (following), gWornItemFlyAnim (flying)
    /*0x24*/ u8 unk24;  // its sprite's affine set (Sprite_AllocAffine, or dir in eighths)
    /*0x25*/ s8 dir;    // a byte direction (Util_GetDirection of the velocity in state 4)
    /*0x26*/ s8 unk26;  // dir - the Helirin's angle (byte) while it follows
    /*0x27*/ u8 unk27;
    /*0x28*/ u16 unk28;
    /*0x2A*/ u16 item;  // the item, bit 0-9
    /*0x2C*/ u16 point; // the Helirin point it sits on (points[point])
    /*0x2E*/ u16 tile;  // 0x371 + item: its OBJ tile
    /*0x30*/ u16 attr2; // its tile/palette halfword (palette gWornItemPalettes[item])
    /*0x32*/ s16 timer; // frames before the Helirin can catch it again (state 4)
};
extern const struct TileCopy gMapScreenSpriteTiles[];
extern const u8 gUnk_080E3B60[];
extern const u8 gUnk_080EA560[];
extern const u8 gUnk_080F661A[];
extern const u8 gUnk_08133948[];
extern const u8 gIntroBgTiles[];
extern const u8 gIntroLayer3Map[];
extern const u8 gIntroLayer2Map[];
extern const u8 gIntroLayer1Map[];
extern const u8 gUnk_0813A3DC[];
// The intro task Intro_MainState starts with Intro_Update (0x34 bytes): 8.8 fixed point
// layer positions (layer 1 x: unkA, layer 2: unkC/unk14, layer 3: unkE/unk16).
struct IntroTask {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ s16 unk8;  // map 0's x (Bg_ScrollMapClamped)
    /*0x0A*/ s16 unkA;
    /*0x0C*/ s16 unkC;
    /*0x0E*/ s16 unkE;
    /*0x10*/ u16 unk10;
    /*0x12*/ s16 unk12;
    /*0x14*/ s16 unk14;
    /*0x16*/ s16 unk16;
    /*0x18*/ u8 unk18[2];
    /*0x1A*/ s16 unk1A;  // Ending_UpdateBackground: added to unkA every frame
    /*0x1C*/ u16 unk1C;  // added to unkC every frame
    /*0x1E*/ s16 unk1E;  // Ending_UpdateBackground: added to unkE every frame
    /*0x20*/ u8 unk20[2];
    /*0x22*/ u16 unk22;
    /*0x24*/ u16 unk24;
    /*0x26*/ u16 unk26;
    /*0x28*/ s16 unk28;  // BLDY << 8
    /*0x2A*/ u16 unk2A;
    /*0x2C*/ void *block; // the tiles' buffer on gMain.heap
    /*0x30*/ u32 unk30;
};
// 0x030008B0: the interrupt handlers AgbMain copies to IWRAM (IntrMain's table)
extern void (*gIntrTable[])(void);
extern u16 gLinkWaitFrames;
extern struct MultiBootParam gMultiBootParam;
extern s32 gMultiBootImageSize;
extern const u8 *gMultiBootImage;
extern const u8 gMultiBootProgram[];
#ifdef PLATFORM_PC
#define DMA3_SET(src, dst, control) Port_Dma(3, (const volatile void *)(src), (volatile void *)(dst), (u32)(control))
#else
// The DMA 3 registers written in order, without the SDK DmaSet's read-back of the control.
#define DMA3_SET(src, dst, control)                  \
    {                                                \
        vu32 *dmaRegs = (vu32 *)REG_ADDR_DMA3;       \
        dmaRegs[0] = (u32)(src);                     \
        dmaRegs[1] = (u32)(dst);                     \
        dmaRegs[2] = (u32)(control);                 \
    }
#endif
extern const u8 gIntroObjTiles[];
// gHelirinTypeGfx + 8: per Helirin type (a 0x80-byte gHelirinTypeGfx entry), 16 pairs whose
// first word is an offset into gHelirinGfx, the OBJ tiles of each Helirin size unk5
// (Helirin_SetSize); Intro_UpdateHelirin copies [0][0]'s.
struct HelirinSizeGfx {
    /*0x00*/ u32 offset;
    /*0x04*/ u32 unk4;
};
extern const struct HelirinSizeGfx gHelirinSizeGfx[][16];
// The frames of a Intro_UpdateSprite task (16 bytes each, by its animation's value): the OBJ tiles
// to copy (NULL: the end), how many, and the sprite's attributes and offset.
struct IntroSpriteFrame {
    /*0x00*/ const void *src;
    /*0x04*/ u32 tiles;
    /*0x08*/ u32 attr;   // attributes 0 and 1, without the x
    /*0x0C*/ u16 attr2;  // | the task's tile
    /*0x0E*/ s8 dx;
    /*0x0F*/ u8 dy;
};
// The intro's sprite tasks Intro_UpdateSprite (0x20 bytes, numbered unk8 by Intro_Update): x and y
// are 16.16 fixed point.
struct IntroSprite {
    /*0x00*/ void (*func)(struct Task *);
    /*0x04*/ u16 key;
    /*0x06*/ s16 state;
    /*0x08*/ u16 n;      // which one, 0-14
    /*0x0A*/ u16 tile;   // its first OBJ tile
    /*0x0C*/ const struct IntroSpriteFrame *frames;
    /*0x10*/ s32 x;
    /*0x14*/ s32 y;
    /*0x18*/ struct Anim anim; // the value: the index into frames
};
extern const struct IntroSpriteFrame gUnk_080C2F38[];
extern const struct IntroSpriteFrame gUnk_080C2FCC[];
extern const struct IntroSpriteFrame gUnk_080C3020[];
extern const struct IntroSpriteFrame gUnk_080C306C[];
extern const struct IntroSpriteFrame gUnk_080C30D0[];
extern const struct IntroSpriteFrame gUnk_080C314C[];
extern const struct IntroSpriteFrame gUnk_080C31D0[];
extern const struct IntroSpriteFrame gUnk_080C323C[];
extern const struct IntroSpriteFrame gUnk_080C32B0[];
extern const struct IntroSpriteFrame gUnk_080C32FC[];
extern const struct IntroSpriteFrame gUnk_080C335C[];
extern const struct IntroSpriteFrame gUnk_080C3424[];
extern const u16 gUnk_080C2F10[];
extern const u16 gUnk_080C2FB8[];
extern const u16 gUnk_080C300C[];
extern const u16 gUnk_080C3050[];
extern const u16 gUnk_080C30BC[];
extern const u16 gUnk_080C3120[];
extern const u16 gUnk_080C31AC[];
extern const u16 gUnk_080C3220[];
extern const u16 gUnk_080C329C[];
extern const u16 gUnk_080C32E0[];
extern const u16 gUnk_080C333C[];
extern const u16 gUnk_080C338C[];
extern const u8 gLevelAreaCounts[38]; // KuruTools NUMBER_AREAS_TABLE_OFFSET (kururin.toml)

#endif // GUARD_GAME_H
