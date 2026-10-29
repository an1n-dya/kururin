// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_VARIABLES_H
#define GUARD_VARIABLES_H

#include "gba/types.h"

// IWRAM state named from KururinTAS's BizHawk memory watch
// (tools/research/KururinTAS/"Kuru Kuru Kururin (Europe).wch", IWRAM offsets) and confirmed by
// the code: each struct's base address is what the code loads from its literal pools, and the
// fields below are accessed at exactly these offsets from it. Unknown space stays unkXX.

struct Helirin;

// A save file (docs/save_format.md, 0x1F4 bytes): the 5 at gSaveFiles; gGameState.saveFile points
// to the one picked. MakeUp_Run (Make Up) starts one task per set bit 0-9 of unlocks and lets
// its menu edit type/color/items, restoring them when B cancels.
struct SaveFile {
    /*0x00*/ u16 number;      // the file's number, 0-4 (Save_ResetFile)
    /*0x02*/ char name[11];   // NUL-terminated (Files_FileMenuItem draws it); name[0] == 0: an empty file
    /*0x0D*/ u8 rank;         // 0-3: none, Trainee, Professor, Master (KuruTools); drawn after the name
    /*0x0E*/ u8 unkE[2];
    /*0x10*/ u32 flags;       // docs/save_format.md: bit 0 Adventure started, 1 the last world open, 2 the 11th
                              // Challenge row open, 3 Normal/Easy chosen, 4 Easy, 5 the ending seen, 6 a message shown
    /*0x14*/ u32 unlocks;     // one bit per bonus: 0-9 the items, 10-16 the colours, 17-31 the Helirin types
    /*0x18*/ u8 type;         // the Helirin type chosen in Make Up (0-14)
    /*0x19*/ u8 color;        // the Helirin colour chosen (0-6)
    /*0x1A*/ u16 items;       // the items worn, bits 0-9
    /*0x1C*/ u32 bonusTaken;  // bit n - 5 per level 5-34 (LevelMap_Read: no bonus task in it once set)
    /*0x20*/ u32 bestTimes[93]; // the best time per level, in frames (216000: never cleared)
    // the mark of each level: 0 not cleared, 1 Training skipped (Script_Run sets [0..4]), 2
    // cleared with another Helirin, 3 cleared, 4 cleared without a bump (Results_EnterRecord); Files_FileMenuItem
    // passes the number up to the last non-zero one of [0..0x25] to Level_SetFromIndex
    /*0x194*/ u8 marks[0x60];
};

// 0x03004420: loaded by ~67 functions; +0/+1 read as bytes (World, Level), +0x20 masked with
// 0x08 (demo mode).
struct GameState {
    /*0x00*/ u8 world;  // .wch "World"
    /*0x01*/ u8 level;  // .wch "Level"
    /*0x02*/ u8 area;   // 0-2: the start point a level starts from (gCamera.startX[area]); Practice's area
                        // menu sets it; Play_CheckLevelEnd: which of the Helirin flags' bits 19/21/23 count
    /*0x03*/ u8 savedArea; // the player's area while Practice's See Demo runs (Pause_PracticeMenuItem
                           // saves it, Pause_DemoMenuItem restores it); 0 from the map screens
    /*0x04*/ u8 file;   // watch.txt "gGameState.file": the save file picked (Files_MainState, gFileMenuFiles)
    /*0x05*/ u8 unk5[3];
    /*0x08*/ struct SaveFile *saveFile; // the save file picked: gSaveFiles + 0x1F4 * file (Files_MainState)
    /*0x0C*/ u32 bestTime;    // the level's best record (Play_MainState), drawn during the level (Hud_DrawTimes)
    /*0x10*/ u32 timer;       // Play_RunLevel: helirin->timer, copied every frame
    /*0x14*/ u16 bumpCount;   // Play_RunLevel: helirin->bumpCount, copied every frame
    /*0x16*/ u8 mode;         // watch.txt "gGameState.mode", read signed: 1 Adventure, 2 Easy, 3 Challenge,
                              // 4 the linked game (four Helirins), 5 Practice, 6 the debug stage select's play
                              // (docs/unused_content.md), 7 Make Up (MakeUp_Run)
    /*0x17*/ u8 song;         // the level's song, by world (Level_StartTasks); started by Play_RunLevel
    /*0x18*/ struct Helirin *helirin; // the current player's: &gHelirins[n] (Play_RunLevel)
    /*0x1C*/ u32 playerCount; // 1 for one player (Play_RunLevel); the linked game ends when that many reach the goal
    /*0x20*/ u32 flags;       // .wch byte 0x4440: GAME_FLAG_DEMO, GAME_FLAG_BONUS_TAKEN
    /*0x24*/ u8 paused;       // toggled by the pause menu (Pause_Update): a level's tasks and Helirins stop
    /*0x25*/ u8 pauseRequest; // opens the pause menu once (Pause_Update): the end of a demo or Practice area
    /*0x26*/ u8 unk26[2];
    /*0x28*/ u32 pauseScreen; // 0 the pause menu, 1 See Map (Pause_SeeMap), 2 Make Up (Pause_MakeUp)
    /*0x2C*/ u8 unk2C;  // Play_CheckLevelEnd: picks Sound_PlaySong(0x12) over 0x13
    /*0x2D*/ u8 unk2D;  // Play_MainState: Vs_RunMenus(unk2D + 1) after Vs_RunResults returns 1
    /*0x2E*/ u16 unk2E; // Vs_RunResults: the final results once unk30 + 1 reaches it (0: never)
    /*0x30*/ u16 unk30; // Hud_Draw draws unk30 + 1 in the linked game (mode 4)
    /*0x32*/ u16 unk32; // Vs_RunMenus: the Challenge course rows the random courses come from (bit n: row n)
    /*0x34*/ u16 unk34[4]; // Vs_PlayerSettingsItem: drawn for each player (Vs_DrawNumberRight)
    /*0x3C*/ u8 unk3C[4];  // Effect_UpdateLinkResultBanner: one per player, 0: draws its sprite
};                         // 0x40 bytes: Game_Boot copies 0x40 bytes of gInitialGameState
#define GAME_FLAG_DEMO (1 << 3)         // .wch "Is demo mode (0b00001000)"
#define GAME_FLAG_BONUS_TAKEN (1 << 4)  // .wch "Has taken bonus (0b00010000)"

// 0x030043B0: the camera bounds are stored as halfwords at exactly +0x1C..+0x22.
struct Camera {
    // From the level map (LevelMap_Read): [0] the start zone (tiles 0xFB/0xFC), [1-3] the healing
    // zones 0xED-0xEF: where each area starts (Helirin_Init)
    /*0x00*/ u16 startX[4];
    /*0x08*/ u16 startY[4];
    /*0x10*/ u16 goalX;       // tile 0xFF: where Play_CheckLevelEnd moves the Helirin at the goal
    /*0x12*/ u16 goalY;
    /*0x14*/ u32 ccwStarts;   // bit n: start point n spins counter-clockwise (KuruTools: 0xFC "CCW")
    /*0x18*/ u32 bonusUnlock; // the unlock the level's bonus gives (docs/save_format.md); Collection_Run's
                              // mode 0 runs only when this is non-zero
    /*0x1C*/ s16 boundLeft;   // .wch "Cam Bound L"
    /*0x1E*/ s16 boundRight;  // .wch "Cam Bound R"
    /*0x20*/ s16 boundTop;    // .wch "Cam Bound T"
    /*0x22*/ s16 boundBottom; // .wch "Cam Bound B"
};

// 0x03004530: the Helirins (the players' spinning sticks), four of 0xCC bytes: Play_RunLevel
// indexes them with `n * 0xCC` and initialises 0, +0xCC, +0x198 and +0x264; Play_CheckLevelEnd loops
// over the four at +0xBC with stride 0xCC. The .wch watches player 0 (gHelirins[0]). Code
// writes X Pixel / Y Pixel at +0x16 / +0x1A through this base. Positions are 16.16 fixed point
// (the .wch lists the subpixel byte at +1 and the pixel halfword at +2 of each).
struct Helirin {
    /*0x00*/ u32 player;           // Helirin_Init(h, n): n; Play_RunLevel compares it with this console's
    /*0x04*/ u8 type;              // the save file's Helirin type (docs/save_format.md 0x018), 0-14
    /*0x05*/ u8 size;              // gHelirinSizes[size]: 64..8 pixels long; Select in a zone changes it
    /*0x06*/ s8 color;             // the save file's colour (0x019): gHelirinColorAnims[color]
    /*0x07*/ u8 unk7;
    /*0x08*/ const s8 *pointDistances; // Collision_UpdatePoints: the distance of each of points[] from the centre
    /*0x0C*/ s8 numPoints;         // entries of points[] in use (Helirin_UpdateCollision)
    // From gHelirinSizes: Helirin_Draw draws the Helirin as a sprite (when length > 8) with
    // attribute bits spriteAttr at (screen x + spriteDx, screen y + spriteDy), on screen while
    // within length of its edges
    /*0x0D*/ s8 length;
    /*0x0E*/ s8 spriteDx;
    /*0x0F*/ u8 spriteDy;
    /*0x10*/ u32 spriteAttr;
    /*0x14*/ s32 x;                // .wch "X" (pixel = x >> 16)
    /*0x18*/ s32 y;                // .wch "Y"
    /*0x1C*/ s32 xb;               // .wch "XB"
    /*0x20*/ s32 yb;               // .wch "YB"
    /*0x24*/ s32 xs;               // .wch "XS"
    /*0x28*/ s32 ys;               // .wch "YS"
    /*0x2C*/ s16 screen[2];        // Helirin_Draw: the pixel position - (gCameraX, gCameraY)
    // colorAnim, colorTimer and colorValue have the layout of a struct Anim: the colour animation
    /*0x30*/ const u16 *colorAnim; // Helirin_SetColor: gHelirinColorAnims[color] (or gHelirinHealColorAnim)
    /*0x34*/ u16 colorTimer;       // .wch "Helirin Color Timer"
    /*0x36*/ u16 colorValue;       // the palette row Helirin_Draw copies (kkk.txt 0x03004566: "non null in zone/dmg")
    /*0x38*/ u16 tile;             // its first OBJ tile: Helirin_Init: 0x30D + 0x20 * player
    /*0x3A*/ u16 palette;          // its OBJ palette << 12: Helirin_Init: 0, 0x1000, 0x9000, 0xA000 by player
    /*0x3C*/ u8 affineSet;         // Helirin_Draw: its sprite's affine set (Sprite_AllocAffine, or the angle in eighths)
    /*0x3D*/ u8 unk3D;
    /*0x3E*/ s16 inputDir;         // Helirin_Update: the D-pad's direction (gKeyDirections), negative: none
    /*0x40*/ u16 speedNorm;        // .wch "Speed norm"
    /*0x42*/ s16 angle;            // .wch "Angle"
    /*0x44*/ s16 angleRate;        // .wch "Angle Rate"
    /*0x46*/ s16 angleDefaultRate; // .wch "Angle Default Rate"
    /*0x48*/ u16 angleRateStep;    // 0x5B: kkk.txt "Rotation +- 91 until default value" (Util_StepTowards)
    /*0x4A*/ s16 nextAngleRate;    // kkk.txt 0x0300457A "Turn rate (with bump)": angleRate's next value
    /*0x4C*/ s16 nextAngleDefaultRate; // kkk.txt 0x0300457C "Turn rate (at sleep)": a spring sets -angleDefaultRate
    /*0x4E*/ u16 bumpCount;        // .wch "Bump count"
    /*0x50*/ u16 regenTimer;       // Practice: +0x44 a frame outside zones, a life at 0x1000 (Helirin_Update); cleared on damage
    /*0x52*/ u8 lives;             // .wch "Lives"
    /*0x53*/ s8 maxLives;          // the lives a zone refills (Helirin_Update); row of gHudFaceAnims (Hud_Init)
    /*0x54*/ u8 frame;             // .wch "Frame"
    /*0x55*/ u8 invulnerability;   // .wch "Invulnerability"
    /*0x56*/ u8 unk56[2];
    // Collision_UpdatePoints: the smaller and the larger x and y of the last two points[] (the stick's ends)
    /*0x58*/ s16 minX;
    /*0x5A*/ s16 minY;
    /*0x5C*/ s16 maxX;
    /*0x5E*/ s16 maxY;
    // Helirin_UpdateCollision tests each point against the map (Collision_TestWallPixel) and sets bit n of
    // collisionMask for points[n]; 17 points fill the space up to collisionMask.
    /*0x60*/ struct { u16 x, y; } points[17];
    /*0xA4*/ u32 collisionMask;    // .wch "Collision Mask" (0x030045D4)
    /*0xA8*/ u32 objectCollisionMask; // kkk.txt "Collision mask 2?": the points the moving objects hit, or'd with collisionMask (Helirin_UpdateCollision)
    /*0xAC*/ u16 keys[5];          // Helirin_Update: the keys held, the last frame's, new, repeated, the repeat timer
    /*0xB6*/ u8 unkB6[2];
    /*0xB8*/ u32 timer;            // .wch "Timer (frame)" (0x030045E8, the low halfword);
                                   // Helirin_UpdateCollision adds 0xB4 per hit, capped at 0x34BBF
    /*0xBC*/ u32 flags;            // kkk.txt "Flags..." (bit 11: "Damaging"); Play_CheckLevelEnd tests 0x8008 of all four
    /*0xC0*/ u8 disableFlags;      // bit 0: no collisions (Helirin_UpdateCollision), bit 1: no keys (Helirin_Update)
    /*0xC1*/ u8 progress;          // map tiles 0xA0-0xBF: 1-0x20, the goal 0x21 (Helirin_Update); the linked
                                   // game's progress bar (Hud_DrawLinkProgress), sent with angle's high byte (Play_RunLevel)
    /*0xC2*/ u8 unkC2[2];
    /*0xC4*/ const u8 *demoKeys;   // gDemoKeys + the level's gDemoKeyOffsets offset; Helirin_Update replays the keys from it (0xFF: the end)
    /*0xC8*/ u8 *recordKeys;       // Helirin_Init: gKeyRecording; Helirin_Update records the keys there (a byte a frame, 0xFF after the last)
};

// 0x03003120: four 0x14-byte maps; Level_Load sets up 1-3 from the level's three
// layers (KuruTools LevelEntry: level data, graphical, background) through Bg_InitMap.
// 0x03003134 (map 1): a tile map; the .wch watches its width and height ("MapW IW" 0x0300313C,
// "MapH IW" 0x0300313E). Collision_GetTilePixel reads tiles[] at (y / 8 % height) * width +
// x / 8 % width: bits 0-9 are a tile of gPhysicalTilesChallenge, bits 10 and 11 flip it.
struct Map {
    /*0x00*/ u16 x;        // Bg_ScrollMap: its x argument modulo pixelWidth
    /*0x02*/ u16 y;        // and y modulo pixelHeight
    /*0x04*/ u16 pixelWidth;  // width * 8 (Bg_InitMap)
    /*0x06*/ u16 pixelHeight; // height * 8
    /*0x08*/ u16 width;  // .wch "MapW IW"
    /*0x0A*/ u16 height; // .wch "MapH IW"
    /*0x0C*/ const u16 *tiles;
    /*0x10*/ void *layer;   // the struct BgLayer it draws through (Bg_InitMap)
};

extern struct GameState gGameState;
extern struct Camera gCamera;
extern struct Helirin gHelirins[4];

// The pixel (high) halfword of a 16.16 position such as Helirin.x / .y: .wch "X Pixel" and
// "Y Pixel" are the halfwords at +2 of X and Y, which code writes on their own.
#define POS_PIXEL(pos) (((s16 *)&(pos))[1])

// Watched but not yet tied to a base the code loads (keep for naming later):
//   0x030059CC u32 "Laggy Level Frame
//   Counter" (+0xC, `clock`, of the m4a MusicPlayerInfo at 0x030059C0 that the game passes to
//   m4aMPlayFadeOut and other m4a calls).

#endif // GUARD_VARIABLES_H
