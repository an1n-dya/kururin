# Names

The readability phase renames the game's placeholders (`sub_<address>`, `gUnk_<address>`,
`Unk<address>` types, `unkXX` fields) to descriptive names, one module per commit, without
changing the build (the ROM stays byte-identical, SHA1 `a31831a186ee435d82b3aced0a0a7c2bc47b0986`).
A name needs evidence (CLAUDE.md rule 6): what the code reads, writes and calls, KururinTAS's
memory watch and `Misc/Game_analysis` notes, KuruTools' format docs and level editor, or
kurutest `--trace` runs of the original. Without evidence the placeholder stays.

Style: functions `Subsystem_Verb...` (PascalCase with the module's prefix), globals
`gCamelCase`, types `PascalCase`, fields `camelCase`.

This file maps each old name to its new one, so older docs (docs/*.md written before the
renames), commit messages and comments stay searchable. Functions keep their address in
`kururin.toml [renames]`, globals in `[symbols]`, each with its evidence.

## helirin

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080142DC` | `Helirin_SetColor` | sets color, the save file's colour (docs/save_format.md 0x019), and its colour animation gHelirinColorAnims[color] |
| `sub_08014324` | `Helirin_SetSize` | loads entry size of gHelirinSizes (17..3 points, 64..8 pixels long: the stick's size, which Select changes in a zone) and its OBJ tiles |
| `sub_08014594` | `Helirin_Init` | sets Helirin n up for a level: the start point, type and colour from the save file, lives (sub_08013210), size |
| `sub_0801475C` | `Helirin_ResetToStart` | puts the Helirin back on the start point and restarts the demo keys (its only caller: the end of a looping demo) |
| `sub_0801482C` | `Helirin_Update` | the Helirin's frame: KururinTAS kkk.txt 'Global description of physics' steps 1-5 (keys, input speed, bump decay, zones) |
| `sub_080150E4` | `Helirin_UpdateCollision` | KururinTAS kkk.txt 0x08015160-0x0801531E: the collision mask, damage, invulnerability, the 3 s timer penalty, the wall bumps |
| `sub_08015468` | `Helirin_Draw` | copies its palette, picks an affine set rotated by its angle and draws its sprites (sub_08002528) |
| `sub_08015638` | `Helirin_Bump` | sets xb/yb (.wch 'XB'/'YB', kkk.txt 'X bump') from a direction and a speed and moves by them |
| `sub_08015684` | `Helirin_UpdatePiece` | the task of one of the 16 pieces Helirin_Update starts when the Helirin crashes (flags bit 6) |
| `sub_080157B4` | `Helirin_UpdatePreview` | spins in place with the keys and draws, no physics: the Make Up screen's Helirin (src/makeup.c) |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_0801EDDC` | `gHelirinHealColorAnim` | Helirin colour animation started when a zone refills the lives (flags bit 9, Helirin_UpdateCollision) |
| `gUnk_0801EDD0` | `gHelirinZoneColorAnim` | Helirin colour animation while in a zone (flags bit 18, Helirin_UpdateCollision) |
| `gUnk_0801EDB8` | `gHelirinHurtColorAnim` | Helirin colour animation after losing a life (Helirin_UpdateCollision) |
| `gUnk_080C48DC` | `gHelirinColorAnims` | const u16 *[7], the colour animation of each colour (Helirin_SetColor) |
| `gUnk_0801EE8C` | `gHelirinSizes` | struct HelirinSize[15], by size (Helirin_SetSize) |
| `gUnk_08391C20` | `gHelirinTypeGfx` | 0x80-byte entries by Helirin type: offsets into gHelirinGfx (Helirin_Draw: its palettes) |
| `gUnk_08391C28` | `gHelirinSizeGfx` | gHelirinTypeGfx + 8: per type, the OBJ tiles of each size (Helirin_SetSize) |
| `gUnk_08392398` | `gHelirinGfx` | the Helirins' OBJ tiles and palettes (Helirin_SetSize, Helirin_Draw) |
| `gUnk_08133548` | `gVsHelirinPalettes` | u8[4][0x20], the Helirin palette of each player in the linked game (Helirin_Draw) |
| `gUnk_080C48FC` | `gHelirinPieceTiles` | 8 pointers to the OBJ tile of each piece (Helirin_UpdatePiece) |
| `gUnk_0801ED48` | `gKeyDirections` | s16[16], the direction of each D-pad combination (KEYINPUT bits 4-7; -1: none; 0x40: Right) |
| `gUnk_0801EF40` | `gTextPlayback` | "PLAYBACK", drawn when the demo keys end (Helirin_Update) |
| `gUnk_0801EF4C` | `gTextDataEmpty` | "DATA EMPTY" (Helirin_Update) |
| `gUnk_0801EF58` | `gTextRecArea` | "REC AREA", drawn when the key recording is full (Helirin_Update) |
| `gUnk_0801EF64` | `gTextEmpty` | "EMPTY" (Helirin_Update) |
| `gUnk_083B3900` | `gDemoKeyOffsets` | per level and area, the offset of its demo keys in gDemoKeys (Helirin_Init: replayed with GAME_FLAG_DEMO) |
| `gUnk_083B3DC0` | `gDemoKeys` | the demos' keys, a byte a frame, 0xFF at the end (Helirin_Update) |
| `gUnk_02039DAC` | `gKeyRecording` | u8[0x2000], the keys Helirin_Update records with gGameState.flags bit 2 (a byte a frame, 0xFF after the last) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08015684` | `HelirinPiece` | the task Helirin_UpdatePiece runs |
| `Unk0801EE8C` | `HelirinSize` | an entry of gHelirinSizes |
| `Unk08391C20` | `HelirinTypeGfx` | an entry of gHelirinTypeGfx |
| `Unk08391C28` | `HelirinSizeGfx` | an entry of gHelirinSizeGfx |
| `Unk083B3900` | `DemoKeyOffset` | an entry of gDemoKeyOffsets |
| `Unk03000020` | `Anim` | an animation sub_080044CC/sub_08004504 step: {next entry, frames left, value} |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `Helirin` | `unk0` | `player` | Helirin_Init(h, n): n; compared with this console's player number |
| `Helirin` | `unk4` | `type` | the save file's Helirin type (save_format.md 0x018), Helirin_Init |
| `Helirin` | `unk5` | `size` | index into gHelirinSizes: 64..8 pixels long; Select in a zone changes it |
| `Helirin` | `unk6` | `color` | the save file's colour (save_format.md 0x019), Helirin_SetColor |
| `Helirin` | `unk8` | `pointDistances` | the distance of each point from the centre (gHelirinSizes) |
| `Helirin` | `unkD` | `length` | 64..8 by size (gHelirinSizes): its on-screen margin, a sprite above 8 |
| `Helirin` | `unkE` | `spriteDx` | the sprite's offset from the centre (Helirin_Draw) |
| `Helirin` | `unkF` | `spriteDy` | the sprite's y offset (Helirin_Draw) |
| `Helirin` | `unk10` | `spriteAttr` | the sprite's OAM attribute bits (Helirin_Draw) |
| `Helirin` | `unk30` | `colorAnim` | with colorTimer (.wch) and colorValue an Anim: the colour animation |
| `Helirin` | `unk36` | `colorValue` | the colour animation's value: the palette row (Helirin_Draw) |
| `Helirin` | `unk38` | `tile` | its first OBJ tile, 0x30D + 0x20 * player (Helirin_Init, Helirin_Draw) |
| `Helirin` | `unk3A` | `palette` | its OBJ palette << 12 (Helirin_Draw) |
| `Helirin` | `unk3C` | `affineSet` | its sprite's affine parameter set (Helirin_Draw) |
| `Helirin` | `unk3E` | `inputDir` | gKeyDirections of the D-pad, negative: none (Helirin_Update) |
| `Helirin` | `unk48` | `angleRateStep` | 91: kkk.txt 'Rotation +- 91 until default value' (0x03004578) |
| `Helirin` | `unk4A` | `nextAngleRate` | kkk.txt 0x0300457A 'Turn rate (with bump)': angleRate's next value |
| `Helirin` | `unk4C` | `nextAngleDefaultRate` | kkk.txt 0x0300457C 'Turn rate (at sleep)': angleDefaultRate's next value |
| `Helirin` | `unk50` | `regenTimer` | Practice: +0x44 a frame outside zones, a life at 0x1000 (Helirin_Update) |
| `Helirin` | `unk53` | `maxLives` | the lives a zone refills (Helirin_Update), the life sprites (sub_08016BF4) |
| `Helirin` | `unk58` | `minX` | the box of the stick's two ends (sub_08013E38) |
| `Helirin` | `unk5A` | `minY` | the box of the stick's two ends (sub_08013E38) |
| `Helirin` | `unk5C` | `maxX` | the box of the stick's two ends (sub_08013E38) |
| `Helirin` | `unk5E` | `maxY` | the box of the stick's two ends (sub_08013E38) |
| `Helirin` | `unkA8` | `objectCollisionMask` | kkk.txt 'Collision mask 2?' (0x030045D8): the points the moving objects hit |
| `Helirin` | `unkAC` | `keys` | held, previous, new, repeated keys and the repeat timer (Helirin_Update) |
| `Helirin` | `unkBC` | `flags` | kkk.txt 'Flags...' (bit 11: damaging) |
| `Helirin` | `unkC0` | `disableFlags` | bit 0: no collisions (Helirin_UpdateCollision), bit 1: no keys (Helirin_Update) |
| `Helirin` | `unkC1` | `progress` | map tiles 0xA0-0xBF give 1-0x20, the goal 0x21; the linked game's progress bar (sub_08017154) |
| `Helirin` | `unkC4` | `demoKeys` | the demo keys replayed (gDemoKeys) |
| `Helirin` | `unkC8` | `recordKeys` | where the keys are recorded (gKeyRecording) |
| `HelirinPiece` | `unk1C` | `tile` | the piece (0-15), then its sprite's attribute 2 |
| `HelirinPiece` | `unk1E` | `affineSet` | dir in eighths: its sprite's affine set |
| `HelirinSize` | `unk0` | `pointDistances` | copied to Helirin.pointDistances |
| `HelirinSize` | `unk5` | `length` | copied to Helirin.length |
| `HelirinSize` | `unk6` | `spriteDx` | copied to Helirin.spriteDx; 0: no OBJ tiles |
| `HelirinSize` | `unk7` | `spriteDy` | copied to Helirin.spriteDy |
| `HelirinSize` | `unk8` | `spriteAttr` | copied to Helirin.spriteAttr |
| `Anim` | `unk0` | `next` | the next entry {frames, value} (sub_080044CC) |
| `Anim` | `unk4` | `timer` | frames left of the current entry (sub_08004504) |
| `Anim` | `unk6` | `value` | the current entry's value |

## collision

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08013D0C` | `Collision_TestWallPixel` | KururinTAS kkk.txt 'Collision Test' (0x08013D0C): the 4-bit pixel of a wall tile (1-0x9F) at a map position, 0 for any other tile |
| `sub_08013DA8` | `Collision_GetTilePixel` | the same pixel for any tile (Helirin_Update tests the diagonal zone tiles 0xF2/0xF3 with it) |
| `sub_08013E38` | `Collision_UpdatePoints` | places points[] along the stick by angle (kkk.txt: 'Helirin separated in 17 parts') and the box of its ends |
| `sub_08013F14` | `Collision_TestShapes` | the mask of the Helirin's points inside a list of shapes (the springs, the moving objects) |
| `sub_080140D4` | `Collision_FindPointInShapes` | the first of the Helirin's points inside a list of shapes, or -1 |
| `sub_08014258` | `Debug_DrawHexWords` | draws 8 words as hexadecimal digits ('0'-'9', 'A'-'F'), one line each, with sub_080028EC; nothing references it |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03003134` | `gWallMap` | struct Map, map 1 of gUnk_03003120: the level's first layer (KuruTools: 'Walls (layer 1)', sub_08004CE0); .wch "MapW IW"/"MapH IW" at +8/+0xA; the Helirin's points are tested against it |
| `gUnk_0801CF34` | `gSineTable` | s16[256]: 256 * sin(2 pi dir / 256) ([0] 0, [64] 256): the x step of byte direction dir (0 up, 0x40 right) |
| `gUnk_0801D134` | `gCosineTable` | s16[256]: 256 * cos(2 pi dir / 256) ([0] 256, [128] -256): the (upward) y step of byte direction dir |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk080140D4Shape` | `CollisionShape` | a shape Collision_TestShapes and Collision_FindPointInShapes test the Helirin against |

## level

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08004C24` | `Game_Boot` | the game's main state 0 (AgbMain's state table gUnk_080C28B4[0]): sound, keys, the initial gGameState, the save at boot (sub_08005178), then gMain.state 1 |
| `sub_08004C8C` | `Level_SetFromIndex` | sets gGameState.world/level from a level index 0-92 (docs/save_format.md: 0-4 Training, then 3 a world, 38-92 Challenge) |
| `sub_08004CE0` | `Level_Load` | decompresses the level's world graphics and its three layers (KuruTools Levels.cs: WorldEntry/WorldInfo/LevelEntry) into VRAM and the maps |
| `sub_08004F68` | `Level_StartTasks` | starts a level's tasks: the world's tile animations (gWorldTileAnims), the effects, the song, the map's objects (sub_0800580C) |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03004390` | `gWorldTileAnims` | gWorldEntries[world].tileAnims (KuruTools "dummy1"), set by Level_Load: {frames, copies} pairs, NULL-terminated, one sub_08017BA8 task each (Level_StartTasks) |
| `gUnk_0801E334` | `gLevelSpriteTiles` | {src, tile, tiles} OBJ VRAM copies every level loads, NULL-terminated (Level_Load) |
| `gUnk_081DC788` | `gLevelSpritePalettes` | 0x1C0 bytes copied to OBJ palette RAM 0x05000200 at every level load (Level_Load) |
| `gUnk_0801E3CC` | `gInitialGameState` | struct GameState: the 0x40 bytes Game_Boot copies to gGameState |
| `gUnk_0203BDAC` | `gSaveData` | u8[0x2254] EWRAM: the save data in RAM, one SRAM bank (docs/save_format.md: 5 files, then the records); gSaveFiles and gSaveBank point here (Game_Boot) |
| `gUnk_0300439C` | `gSaveFiles` | u8 *, gSaveData: the 5 save files of 0x1F4 bytes (docs/save_format.md) |
| `gUnk_030043A0` | `gSaveBank` | u8 *, gSaveData, the whole bank: + 0x9C4 the Normal records, + 0x1B34 the Easy ones, 0x30 bytes a level (docs/save_format.md) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `WorldEntry` | `unk8` | `tileAnims` | copied to gWorldTileAnims (Level_Load): the world's tile animations (Level_StartTasks) |

## play

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08008200` | `Play_MainState` | the game's main state 3 (gUnk_080C28B4[3]): the mode menu, the maps and menus of each mode and their levels, in sub-states gMain.subState |
| `sub_08008660` | `Play_RunLevel` | a level's loop, one state a frame: load (Level_Load), start, the linked players' wait, play (the Helirins, the pause menu), restart, the linked game's end |
| `sub_08008C18` | `Play_CheckLevelEnd` | once a frame of play: the crash (flags bit 6), the goal (bits 2, 3), the demo's and Practice's ends; 1 when the level is finished, -1/2 when it is left |
| `sub_080093B8` | `Play_MoveHelirinTo` | moves the Helirin towards a pixel by a speed (to the goal, a demo's targets); 0 once there |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03000004` | `gLevelEndTimer` | s32 frames to wait at a level's end before the window closes (Play_CheckLevelEnd: 0x3C, 0x1E, 0x78, counted down) |
| `gUnk_030031E4` | `gCameraX` | s32, the map pixel at the screen's left edge: the Helirin's x - 0x78 at a level start (Play_RunLevel), subtracted from every sprite's x (Helirin_Draw) |
| `gUnk_030031E8` | `gCameraY` | s32, the map pixel at the screen's top edge: the Helirin's y - 0x50 at a level start (Play_RunLevel) |
| `gUnk_0801EF6C` | `gSongSoundModes` | u32[], by gGameState.song: the m4aSoundMode setting (<< 8) of a level's song (Play_RunLevel) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `GameStateUnk8` | `SaveFile` | a save file, 0x1F4 bytes (docs/save_format.md) |
| `Unk030043A0Record` | `LevelRecord` | a level record, 0x10 bytes (docs/save_format.md: time, name, rank) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `GameState` | `unk2` | `area` | the area a level starts from (0-2): Practice's area menu sets it; Helirin_Init starts at gCamera.startX[area] |
| `GameState` | `unk3` | `savedArea` | the player's area while Practice's See Demo runs: Pause_PracticeMenuItem saves area there, Pause_DemoMenuItem restores it; deep review 2026-10-23 |
| `GameState` | `unk4` | `file` | the save file picked (watch.txt gGameState.file) |
| `GameState` | `unk8` | `saveFile` | &gSaveFiles[file] (sub_0800B8A0) |
| `GameState` | `unkC` | `bestTime` | the level's best record time (Play_MainState), drawn during the level (sub_08017090) |
| `GameState` | `unk10` | `timer` | the current Helirin's timer, copied every frame (Play_RunLevel) |
| `GameState` | `unk14` | `bumpCount` | the current Helirin's bumpCount, copied every frame (Play_RunLevel) |
| `GameState` | `unk16` | `mode` | watch.txt gGameState.mode: 1 Adventure, 2 Easy, 3 Challenge, 4 linked (sub_08010394), 5 Practice (sub_08018D34), 7 Make Up (sub_080133E4); 6 is not identified |
| `GameState` | `unk17` | `song` | the level's song, by world (Level_StartTasks), started 0x1E frames into the level (Play_RunLevel) |
| `GameState` | `unk1C` | `playerCount` | 1 for one player (Play_RunLevel); the linked game ends when that many Helirins reached the goal (Play_CheckLevelEnd) |
| `GameState` | `unk24` | `paused` | toggled by the pause menu (sub_08009438); everything in a level stops while it is set |
| `GameState` | `unk25` | `pauseRequest` | opens the pause menu once (sub_08009438): the end of a demo or a Practice area (Play_CheckLevelEnd) |
| `Camera` | `unk0` | `startX` | [0] the start zone (map tiles 0xFB/0xFC), [1-3] the healing zones 0xED-0xEF (sub_0800580C): where each area starts |
| `Camera` | `unk8` | `startY` | the y of startX |
| `Camera` | `unk10` | `goalX` | map tile 0xFF (sub_0800580C): where the Helirin is moved at the goal (Play_CheckLevelEnd) |
| `Camera` | `unk12` | `goalY` | the y of goalX |
| `Camera` | `unk14` | `ccwStarts` | bit n: start point n spins counter-clockwise (tile 0xFC, KuruTools' 'CCW' start zone, or a flipped 0xFB/0xED-0xEF) |
| `Camera` | `unk18` | `bonusUnlock` | the unlock bit the level's bonus gives (docs/save_format.md: sub_0800AA0C ORs it in) |
| `SaveFile` | `unk0` | `number` | docs/save_format.md 0x000 |
| `SaveFile` | `unk2` | `name` | 0x002 |
| `SaveFile` | `unkD` | `rank` | 0x00D |
| `SaveFile` | `unk10` | `flags` | 0x010 |
| `SaveFile` | `unk14` | `unlocks` | 0x014 |
| `SaveFile` | `unk18` | `type` | 0x018, the Helirin type |
| `SaveFile` | `unk19` | `color` | 0x019, the Helirin colour |
| `SaveFile` | `unk1A` | `items` | 0x01A, the items worn |
| `SaveFile` | `unk1C` | `bonusTaken` | 0x01C |
| `SaveFile` | `unk20` | `bestTimes` | 0x020 |
| `SaveFile` | `unk194` | `marks` | 0x194 |
| `LevelRecord` | `unk0` | `time` | docs/save_format.md: time in frames |
| `LevelRecord` | `unk4` | `name` | name |
| `LevelRecord` | `unkF` | `rank` | the holder's rank |

## main

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080003FC` | `Main_EndFrame` | ends a frame: m4aSoundMain, waits for the VBlank interrupt (gMain.intrFlags), the frame callback, gMain.frameCount, the keys |
| `sub_08000450` | `Main_VBlankIntr` | the VBlank handler: entry 0 of the interrupt table AgbMain installs (IE bit 0); sets gMain.intrFlags for Main_EndFrame |
| `sub_08000478` | `Main_ClearKeys` | clears gMain's key state (heldKeys..repeatTimer) |
| `sub_080004A0` | `Main_ReadKeys` | reads KEYINPUT into gMain.heldKeys/newKeys/prevKeys/repeatKeys |
| `ClearFrameCallback` | `Main_ClearFrameCallback` | gMain.frameCallback = NULL; named in Phase 1, given the module prefix in the deep review (2026-10-23) |
| `sub_080004F8` | `Main_InstallLinkFrameCallback` | copies the I/O registers to their copies (DMA 3) and installs Main_LinkFrameCallback |
| `sub_08000528` | `Main_LinkFrameCallback` | the frame callback with the link cable: the frame's link exchange (sub_08003D14) when the serial interrupt is on, then OAM, I/O copies, transfers, layers |
| `sub_0800065C` | `Main_InstallFrameCallback` | copies the I/O registers to their copies (BIOS CpuSet) and installs Main_FrameCallback |
| `sub_08000680` | `Main_FrameCallback` | the frame callback after the VBlank wait: OAM from gOamBuffer, the I/O register copies, the transfer queue, the layers' maps |
| `sub_080006F8` | `Main_RunTransfersDma` | runs the transfer queue gTransferQueue with DMA 3 and empties it |
| `sub_080007CC` | `Main_RunTransfersBios` | runs the transfer queue with the BIOS (CpuSet/CpuFastSet) and empties it |
| `sub_08000890` | `Main_QueueCopy` | queues a copy (transfer mode 0); -1 when the queue is full |
| `sub_080008F4` | `Main_QueueFill` | queues a halfword fill (transfer mode 1); -1 when the queue is full |
| `sub_08000958` | `Main_QueueBlockCopy` | queues blocks of tiles, each to the next 0x400 bytes (transfer mode 2) |
| `sub_080009C8` | `Main_BlockCopy` | Main_QueueBlockCopy's copy done at once (DMA 3) |
| `sub_08000A08` | `Heap_Init` | sets a heap up on a memory block: one free block in a ring with the heap's head |
| `sub_08000A2C` | `Heap_Free` | frees a block of a heap, merging free neighbours |
| `sub_08000A70` | `Heap_Alloc` | the first free block big enough from the rover, split, cleared; NULL when none |
| `sub_0801AB8C` | `m4aSoundVSync` | m4a_1.s's m4aSoundVSync (pret): checks SOUND_INFO_PTR's 'Smsh' ident, counts pcmDmaCounter down and restarts the sound DMA; called from Main_VBlankIntr, as the SDK expects |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03000E10` | `gTransferQueue` | 0x40 transfers {src, dst, size, mode} (Main_QueueCopy & co.) that Main_RunTransfersDma/Bios run |
| `gUnk_03000E00` | `gTransferCount` | u16, the transfers queued in gTransferQueue (at most 0x40); 0 after they run |
| `gUnk_0801CB64` | `gIntrTableInit` | 14 interrupt handlers AgbMain copies to gIntrTable ([0] Main_VBlankIntr, the rest IntrDummy or 0) |
| `gUnk_030008B0` | `gIntrTable` | the interrupt handler table IntrMain dispatches through (AgbMain fills it from gIntrTableInit); sub_0800C75C sets [1] to the IWRAM routines at 0x03003EF0/1 |
| `gUnk_03000950` | `gIntrMainBuffer` | 0x400 bytes of IWRAM: AgbMain copies IntrMain there and makes it the BIOS interrupt vector (0x03007FFC) |
| `gUnk_080C28B4` | `gMainStates` | AgbMain's state functions, indexed by gMain.state: Game_Boot, the intro, the file menu, Play_MainState, the ending, two debug states |
| `gUnk_030008F0` | `gDispcntCopy` | u16, the copy of DISPCNT (0x04000000) and the start of the I/O register copies: Main_InstallFrameCallback copies 0x2B halfwords of I/O registers here, the frame callback writes them back |
| `gUnk_030008F8` | `gBgCntCopies` | u16[4], gDispcntCopy + 8: the copies of BG0CNT-BG3CNT |
| `gUnk_03000900` | `gBgOfsCopies` | u32[4], gDispcntCopy + 0x10: the copies of BG0HOFS/VOFS-BG3HOFS/VOFS, from the layers' positions (Main_FrameCallback) |
| `gUnk_03000910` | `gBg2AffineCopy` | gDispcntCopy + 0x20: the copies of BG2PA-PD, BG2X, BG2Y (sub_080173CC writes a rotation there) |
| `gUnk_03000930` | `gWin0hCopy` | u16, gDispcntCopy + 0x40: the copy of WIN0H |
| `gUnk_03000932` | `gWin1hCopy` | u16, gDispcntCopy + 0x42: the copy of WIN1H |
| `gUnk_03000934` | `gWin0vCopy` | u16, gDispcntCopy + 0x44: the copy of WIN0V |
| `gUnk_03000936` | `gWin1vCopy` | u16, gDispcntCopy + 0x46: the copy of WIN1V |
| `gUnk_03000938` | `gWininCopy` | u16, gDispcntCopy + 0x48: the copy of WININ |
| `gUnk_0300093A` | `gWinoutCopy` | u16, gDispcntCopy + 0x4A: the copy of WINOUT |
| `gUnk_0300093C` | `gMosaicCopy` | u16, gDispcntCopy + 0x4C: the copy of MOSAIC |
| `gUnk_03000940` | `gBldcntCopy` | u16, gDispcntCopy + 0x50: the copy of BLDCNT |
| `gUnk_03000942` | `gBldalphaCopy` | u16, gDispcntCopy + 0x52: the copy of BLDALPHA |
| `gUnk_03000944` | `gBldyCopy` | u16, gDispcntCopy + 0x54: the copy of BLDY |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk03000E10` | `Transfer` | an entry of gTransferQueue |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `Main` | `unkB` | `subState` | the sub-state the main states switch on (Play_MainState, the ending); AgbMain clears it |
| `Main` | `unkC` | `vblankCount` | counted up by Main_VBlankIntr, cleared by Main_EndFrame |
| `Main` | `unk10` | `ewramCursor` | Level_Load: the next free byte of the EWRAM buffer it decompresses into |
| `Main` | `unk14` | `heap` | a struct Heap (0x18 bytes) the screens allocate from (Heap_Init on 0x02000000) |

## task

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080040E0` | `Task_Init` | sets the task heap gTaskHeap up on EWRAM 0x0203E000 (0x2000 bytes), no tasks |
| `sub_08004104` | `Task_Create` | KururinTAS kkk.txt PROC0 (08004104) CREATE_OBJECT: allocates a task from gTaskHeap and sets its function |
| `sub_08004134` | `Task_Destroy` | frees a task (Heap_Free) and counts it off gTaskCount |
| `sub_08004154` | `Task_DestroyByKey` | frees every task whose key matches |
| `sub_08004198` | `Task_RunAll` | calls every task's function, round the heap's ring |
| `sub_080041C0` | `Task_FindByKey` | the first task whose key matches, or NULL |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03004260` | `gTaskHeap` | struct Heap the tasks come from: Task_Init sets it up on 0x0203E000 (0x2000 bytes), Task_Create/Task_Destroy allocate and free |
| `gUnk_03004250` | `gTaskCount` | s32, the tasks allocated from gTaskHeap: +1 in Task_Create, -1 in Task_Destroy, 0 in Task_Init |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08004104` | `Task` | a task (Task_Create): its function, then its own data |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `Task` | `unk4` | `key` | Task_FindByKey and Task_DestroyByKey match it; cleared by Task_Create |
| `Task` | `unk6` | `state` | cleared by Task_Create; the tasks switch on it (e.g. Helirin_UpdatePiece) and the task structs built on Task call it state |

## sprite

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08002438` | `Sprite_ClearLists` | empties the 16 sprite lists and frees every sprite entry and affine set but the first 8 |
| `sub_0800246C` | `Sprite_BuildOam` | builds gOamBuffer from the 16 sprite lists in order, hides the rest, stores the 32 affine parameter sets |
| `sub_08002528` | `Sprite_Alloc` | hands out the next sprite entry and pushes it on a list; NULL when all 128 are taken |
| `sub_08002580` | `Sprite_AllocMany` | hands out count consecutive sprite entries on a list (Sprite_DebugTileGrid) |
| `sub_080025E8` | `Sprite_InitRotations` | affine sets 0-7: the rotations by the 8 directions 0, 0x20, ... 0xE0 |
| `sub_08002628` | `Sprite_AllocAffine` | hands out the next of the 32 affine parameter sets, -1 when none is left |
| `sub_08002648` | `Sprite_SetAffine` | sets affine set n to {pa, pb, pc, pd} |
| `sub_08002664` | `Sprite_SetAffineRotation` | sets affine set n to the rotation by a direction (gCosineTable, gSineTable) |
| `sub_0800269C` | `Sprite_SetAffineRotScale` | Sprite_SetAffineRotation scaled by 256 / scale |
| `sub_08002700` | `Sprite_QueueTileList` | queues the OBJ tile copies of a NULL-terminated list {src, tile, size, rows} (Main_QueueCopy's queue, each row 0x800 bytes on); -1 when the queue fills; nothing references it |
| `sub_08002790` | `Sprite_DebugTileGrid` | draws a 128x128 grid of OBJ tiles, shifted by Right/Down: an OBJ VRAM viewer no state reaches |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03003AC0` | `gSpriteLists` | s8[16], the head of each sprite list (-1: empty), linked through Sprite.next (Sprite_Alloc, Sprite_BuildOam) |
| `gUnk_030033C0` | `gSprites` | struct Sprite[128], the entries Sprite_Alloc hands out |
| `gUnk_03003EE0` | `gSpriteCount` | u8, the sprite entries handed out so far (Sprite_Alloc; 0 in Sprite_ClearLists) |
| `gUnk_030039C0` | `gAffineParams` | u16[32][4], the OBJ affine parameter sets (pa, pb, pc, pd) Sprite_BuildOam stores in OAM |
| `gUnk_03003AD0` | `gAffineCount` | u8, the next free affine set: Sprite_AllocAffine hands out 8-31 (Sprite_ClearLists sets 8) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08002528` | `Sprite` | a sprite entry (Sprite_Alloc): its OAM attributes and its list link |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `Sprite` | `unk0` | `attr01` | OAM attributes 0 (low half) and 1 (high half), copied to gOamBuffer (Sprite_BuildOam) |
| `Sprite` | `unk4` | `attr2` | OAM attribute 2: tile and palette |
| `OamEntry` | `unk0` | `attr01` | OAM attributes 0 and 1 |
| `OamEntry` | `unk4` | `attr2` | OAM attribute 2 |
| `OamEntry` | `unk6` | `affineParam` | the OAM's fourth halfword: one parameter of an affine set (Sprite_BuildOam) |

## bg

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08000AEC` | `Bg_SetVideoMode` | sets one of 10 video setups: the DISPCNT/BGxCNT/blend register copies and each layer's VRAM, size and buffer |
| `sub_08001258` | `Bg_InitMap` | sets a map up on a layer with its tiles, width and height, and draws it from (0, 0) |
| `sub_08001284` | `Bg_ScrollMap` | moves a map to (x, y), wrapping round its edges, and redraws its layer's buffer |
| `sub_08001480` | `Bg_ScrollMapClamped` | moves a map to (x, y) and redraws its layer's buffer, blank outside the map |
| `sub_080016E0` | `Bg_DrawLevelMinimap` | draws a minimap from a level's data (the Challenge menu when the level has no minimap layer, src/challenge.c; Practice, the Adventure map) |
| `sub_08001814` | `Bg_LoadPicture` | map 2 as a 30x20 map of tiles 0-599 in order, its tiles and palette: a full-screen picture |
| `sub_08001888` | `Bg_WindowLineIntr` | the VCount handler Bg_StartWindowCircle installs: starts DMA 0 copying gWindowLines to WIN0H at each HBlank |
| `sub_080018B8` | `Bg_StartWindowCircle` | starts the circular window effect at (x, y), radius r: window 0, the VCount handler Bg_WindowLineIntr |
| `sub_08001930` | `Bg_DrawWindowCircle` | draws a circle of radius r around (x, y) into gWindowLines (midpoint circle algorithm): the window shape |
| `sub_08001ACC` | `Bg_LoadScreen` | sets up the background maps, tiles and palettes of screen n (0-18) of the menus |
| `sub_08002240` | `Bg_LoadMap8bpp` | Bg_LoadMap for 8bpp tiles |
| `sub_0800233C` | `Bg_LoadMap` | decompresses a tile map into a layer's VRAM row by row and copies the tiles its list names |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03003170` | `gBgLayers` | struct BgLayer[4], one per BG layer (Bg_SetVideoMode); the frame callback copies their offsets and buffers to the hardware |
| `gUnk_03003120` | `gMaps` | struct Map[4], one per layer (Bg_InitMap); Level_Load sets 1-3 up from the level's three layers |
| `gUnk_03001110` | `gBgLayerBuffers` | u8[0x2000]: the 0x800-byte map buffers of the four gBgLayers (buffer, Bg_SetVideoMode) |
| `gUnk_03003110` | `gWindowRadius` | s32, the radius of the window circle (Bg_StartWindowCircle, Bg_DrawWindowCircle): the level's opening and closing |
| `gUnk_030033B8` | `gWindowX` | s32, the centre x of the window circle (Bg_StartWindowCircle) |
| `gUnk_030031E0` | `gWindowY` | s32, the centre y of the window circle (Bg_StartWindowCircle) |
| `gUnk_030031F0` | `gWindowLines` | u16[]: WIN0H, then one value per line that DMA 0 copies to WIN0H at each HBlank (Bg_WindowLineIntr) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk03003170` | `BgLayer` | one hardware background layer: offsets, VRAM, size, map buffer |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `BgLayer` | `unk0` | `hofs` | copied to BGxHOFS by the frame callback (with vofs, as one word) |
| `BgLayer` | `unk2` | `vofs` | copied to BGxVOFS |
| `BgLayer` | `unk4` | `mapVram` | the layer's map in VRAM, where its buffer is copied (Bg_SetVideoMode) |
| `BgLayer` | `unk8` | `mapSize` | the bytes of its map (0x800, 0x200, 0) |
| `BgLayer` | `unkC` | `charVram` | the layer's tiles in VRAM (Bg_SetVideoMode) |
| `BgLayer` | `unk10` | `width` | 0x20, its width in tiles (Bg_SetVideoMode) |
| `BgLayer` | `unk12` | `height` | its height in tiles |
| `BgLayer` | `unk14` | `dirty` | set when buffer changed: the frame callback copies it to mapVram and clears it |
| `BgLayer` | `unk18` | `buffer` | the layer's map buffer in gBgLayerBuffers |
| `Map` | `unk0` | `x` | the map's x position, wrapped (Bg_ScrollMap) |
| `Map` | `unk2` | `y` | the map's y position |
| `Map` | `unk10` | `layer` | the BgLayer it draws through (Bg_InitMap) |

## objects

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08015890` | `Object_SpringUp` | map tile 0xF8 (sub_0800580C): KuruTools' SPRING_IDS, KururinTAS spring.txt 'UP 248'; bumps the Helirin up (Helirin_Bump dir 0, 0x100 = spring.txt's 65536) |
| `sub_08015A30` | `Object_SpringDown` | tile 0xF8 flipped vertically (spring.txt 'DOWN 2296' = 0xF8 | 0x800): bumps down (dir 0x80); in world 0 both Helirins |
| `sub_08015D50` | `Object_SpringRight` | tile 0xF9 (spring.txt 'RIGHT 249'): bumps right (dir 0x40) |
| `sub_08015EEC` | `Object_SpringLeft` | tile 0xF9 flipped horizontally (spring.txt 'LEFT 1273' = 0xF9 | 0x400): bumps left (dir 0xC0) |
| `sub_0801608C` | `Object_Roller` | tile 0xF7, KuruTools' Roller (Dir, StartTime, Period, Speed): fires an Object_RollerProjectile every period |
| `sub_080161A0` | `Object_RollerCatcher` | tile 0xF6, KuruTools' Roller Catcher (Dir only): one sprite |
| `sub_08016264` | `Object_RollerProjectile` | a Roller's projectile (KuruTools: 'Speed of the projectiles'): flies until the map ahead is solid, pushes a Helirin it hits |
| `sub_080164A4` | `Object_Piston` | tile 0xF5, KuruTools' Piston (Dir 0-3, StartTime, WaitPeriod, MovePeriod): swings out and back along its direction |
| `sub_080166E0` | `Object_TurnInArc` | turns a byte direction towards another by a step, kept inside an arc (the Shooter's MinDir..MaxDir) |
| `sub_0801677C` | `Object_Shooter` | tile 0xF4, KuruTools' Shooter (MinDir, MaxDir, StartTime, Period): turns towards the Helirin within its arc and fires Object_ShooterShot |
| `sub_080169B0` | `Object_ShooterShot` | a Shooter's shot: flies along its direction, pushes a Helirin it hits |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_080C491C` | `gSpringAnim` | u16 animation frames of a spring at rest (Object_Spring*) |
| `gUnk_080C4920` | `gSpringHitAnim` | u16 animation frames of a spring that bumps (Object_Spring*) |
| `gUnk_080C4940` | `gSpringShape` | struct CollisionShape[] of a spring (Object_Spring*) |
| `gUnk_080C4984` | `gPistonShape` | struct CollisionShape[] of a piston (Object_Piston) |
| `gUnk_080C496C` | `gRollerProjectileShape` | struct CollisionShape[] of a roller's projectile (Object_RollerProjectile) |
| `gUnk_080C4958` | `gRollerProjectileAnim` | u16 animation frames of a roller's projectile |
| `gUnk_080C499C` | `gShooterAnim` | u16 animation frames a shooter starts with (Object_Shooter) |
| `gUnk_080C49A0` | `gShooterFireAnim` | u16 animation frames of a shooter firing (a shot on its frame 0x29C) |
| `gUnk_080C49E0` | `gShooterShotAnim` | u16 animation frames of a shooter's shot (Object_ShooterShot) |
| `gUnk_080C49EC` | `gShooterShotShape` | struct CollisionShape[] of a shooter's shot (Object_ShooterShot) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08015A30` | `AnimObject` | the task of a spring (Object_Spring*) and of the effects with its layout (Effect_UpdateCrash, Effect_UpdateHitSpark, Effect_UpdateZoneHitRipple, Effect_UpdateSpringSpark); named `SpringObject` until the deep review of 2026-10-23 |
| `Unk0801608C` | `RollerObject` | the task of a roller (Object_Roller) |
| `Unk080161A0` | `RollerCatcherObject` | the task of a roller catcher (Object_RollerCatcher) |
| `Unk08016264` | `RollerProjectile` | the task of a roller's projectile (Object_RollerProjectile) |
| `Unk080164A4` | `PistonObject` | the task of a piston (Object_Piston) |
| `Unk0801677C` | `ShooterObject` | the task of a shooter (Object_Shooter) |
| `Unk080169B0` | `ShooterShot` | the task of a shooter's shot (Object_ShooterShot) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `RollerObject` | `unk10` | `affineSet` | dir in eighths: its sprite's affine set |
| `RollerObject` | `unk11` | `dir` | KuruTools Roller 'Dir' << 5: a byte direction |
| `RollerObject` | `unk12` | `speed` | KuruTools 'Speed' of the projectiles (0xC0 from a map code) |
| `RollerObject` | `unk14` | `timer` | KuruTools 'StartTime', then frames to the next projectile |
| `RollerObject` | `unk16` | `period` | KuruTools 'Period': what timer restarts from |
| `RollerCatcherObject` | `unk10` | `affineSet` | dir in eighths |
| `RollerCatcherObject` | `unk11` | `dir` | KuruTools 'Dir' << 5 |
| `RollerProjectile` | `unk1C` | `affineSet` | its sprite's affine set (0: none) |
| `RollerProjectile` | `unk1D` | `dir` | the roller's dir |
| `RollerProjectile` | `unk1E` | `speed` | the roller's speed |
| `RollerProjectile` | `unk28` | `aheadX` | 15/256 of the x step: the map is tested that far ahead of x |
| `RollerProjectile` | `unk2A` | `aheadY` | and of y |
| `PistonObject` | `unk1C` | `dirEighths` | dir in eighths (0-7) |
| `PistonObject` | `unk1D` | `dir` | KuruTools Piston 'Dir' (0-3) << 6 |
| `PistonObject` | `unk1E` | `attr2` | its sprite's OAM attribute 2 |
| `PistonObject` | `unk20` | `waitPeriod` | KuruTools 'WaitPeriod' (0x78 from a map code) |
| `PistonObject` | `unk22` | `waitTimer` | KuruTools 'StartTime', then frames until the next move |
| `PistonObject` | `unk24` | `phaseStep` | the phase's step a frame: 0x10000 / KuruTools 'MovePeriod' |
| `ShooterObject` | `unk14` | `affineSet` | its sprite's affine set |
| `ShooterObject` | `unk16` | `arcMin` | KuruTools Shooter 'MinDir' << 5 (with arcMax 0: unconstrained) |
| `ShooterObject` | `unk17` | `arcMax` | KuruTools 'MaxDir' << 5 |
| `ShooterObject` | `unk20` | `timer` | KuruTools 'StartTime', then frames before the next shot |
| `ShooterObject` | `unk22` | `period` | KuruTools 'Period' (0x90 from a map code) |
| `ShooterShot` | `unk2A` | `aheadX` | 1/32 of the x step: the map is tested that far ahead |
| `ShooterShot` | `unk2C` | `aheadY` | and of y |
| `ShooterShot` | `unk2E` | `inMap` | bit 0: in the map at the last test (it started inside) |

## hud

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08016BF4` | `Hud_Init` | a level's HUD: the face animation by maxLives and lives, and one Hud_UpdateLifeIcon task per life |
| `sub_08016C6C` | `Hud_UpdateLifeIcon` | task n: the icon of life n, shown (growing in, then still) while the current Helirin has at least n lives |
| `sub_08016DD4` | `Hud_Draw` | the HUD sprites: the face (its animation's OBJ tiles), the lives, the hurt marks, the linked game's number |
| `sub_08017090` | `Hud_DrawTimes` | draws gGameState.timer and gGameState.bestTime on their frames |
| `sub_08017154` | `Hud_DrawLinkProgress` | the linked game's progress bar: each player's Helirin at x 0x50 + 2 * progress |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03000020` | `gHudFaceAnim` | struct Anim: the HUD face's animation; each new frame copies gHudFaceTiles[value] to OBJ VRAM 0x06010B40, the face sprite's tiles (Hud_Draw) |
| `gUnk_03000018` | `gHudHurtAnim` | struct Anim: runs gHudHurtFrames when the Helirin is damaging (flags 0x808 == 0x800; KururinTAS kkk.txt bit 11 'Damaging (set the sprite of the face)'), with the hurt face gHudFaceTiles[6] (Hud_Draw) |
| `gUnk_080C4B50` | `gHudFaceAnims` | const u16 *[][4], the face animation by [maxLives][lives] (Hud_Init, Hud_Draw) |
| `gUnk_080C4B38` | `gHudGoalFaceAnim` | u16 animation frames of the face at the goal (Helirin flags bit 2, Hud_Draw) |
| `gUnk_080C4AA0` | `gHudHurtFrames` | u16 animation frames of gHudHurtAnim (Hud_Draw) |
| `gUnk_080C4A50` | `gHudFaceTiles` | const void *[]: 0x200 bytes of OBJ tiles per face frame (Hud_Draw) |
| `gUnk_080C4A04` | `gHudLifePositions` | {s16 x, y}[]: where life icon n is drawn (index n - 1, Hud_UpdateLifeIcon) |
| `gUnk_080C4A14` | `gHudLifeScaleAnim` | u16 animation frames: the scale of a life icon growing in (Hud_UpdateLifeIcon) |
| `gUnk_081E5748` | `gHudLifeTiles` | 0x80 bytes of OBJ tiles of the life icon (Hud_UpdateLifeIcon) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08016C6C` | `HudLifeIcon` | the task Hud_UpdateLifeIcon runs |

## save

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08005178` | `Save_LoadAtBoot` | the save at boot (Game_Boot): the erase question (A+B+Select+Start), the signature, each SRAM bank's checksum (ReadSram), the damaged-save message, the format |
| `sub_080054E4` | `Save_Write` | writes gSaveBank to the SRAM bank not in use, its checksum, then makes it the bank in use (WriteSram, VerifySram); 0 when saved, -1 otherwise |
| `sub_080055C0` | `Save_ResetFile` | resets save file n (docs/save_format.md): number, empty name ("GUEST" for file 0), unlocks 0x20400, times 216000, marks 0 |
| `sub_0800562C` | `Save_ResetFiles` | Save_ResetFile for files 0-4 |
| `sub_08005644` | `Save_ResetRecords` | resets every level record to 216000 frames "--------", then the two "Hare" records of each level from gLevelTimes / gEasyLevelTimes |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_080C2C50` | `gSaveSignature` | "KURURIN 1.01", compared with SRAM 0x0E000000 and written there at a format (Save_LoadAtBoot) |
| `gUnk_0801E40C` | `gTextDeleteSave1` | "Do you want to delete" (Save_LoadAtBoot) |
| `gUnk_0801E424` | `gTextDeleteSave2` | "all the saved data?" |
| `gUnk_0801E43C` | `gTextYes` | "Yes" |
| `gUnk_0801E444` | `gTextNo` | "No" |
| `gUnk_0801E448` | `gTextSaveDamaged1` | "This saved data" (Save_LoadAtBoot, no bank's checksum good) |
| `gUnk_0801E45C` | `gTextSaveDamaged2` | "is damaged." |
| `gUnk_0801E46C` | `gTextSaveDamaged3` | "It will be deleted." |
| `gUnk_0801E484` | `gTextGuest` | "GUEST", file 0's name after a reset (Save_ResetFile) |
| `gUnk_0801E48A` | `gDefaultRecordNames` | char[3][10], "Hare": the names of the default records (Save_ResetRecords) |
| `gUnk_0801E4A8` | `gTextNoRecordName` | "--------", the name of an empty record (Save_ResetRecords) |
| `gUnk_080C2B6A` | `gEasyLevelTimes` | gLevelTimes[93]: KuruTools' Easy Mode times, the Easy records' defaults (Save_ResetRecords) |

## files

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800B8A0` | `Files_MainState` | the game's main state 2 (gMainStates[2]): the file select, then name entry or the delete screen |
| `sub_0800BAA4` | `Files_FileMenuItem` | the file select menu's items (Menu protocol 0x100..): each file's name, mode, world and level; item 6 "delete saved data" |
| `sub_0800BC70` | `Files_EnterName` | the name entry screen: edits a file's name (8 characters, Start fills in "Kururin"), then Save_Write |
| `sub_0800BD98` | `Files_NameEntryItem` | the name entry menu's items: the name, the three kinds (upper, lower case, kana) and the character grid |
| `sub_0800C2F8` | `Files_DeleteScreen` | the "delete saved data" screen: pick a file, confirm, Save_ResetFile and Save_Write |
| `sub_0800C410` | `Files_DeleteMenuItem` | the delete screen's file menu items (as Files_FileMenuItem) |
| `sub_0800C60C` | `Files_DeleteConfirmItem` | the delete confirmation's items: the file's name, two text lines, Yes (1) / No (-1) |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_0801E89C` | `gTextNoSavedData` | "No Saved Data": the file menu's text for an empty file (Files_FileMenuItem, Files_DeleteMenuItem) |
| `gUnk_0801E8AC` | `gTextEasy` | "Easy": drawn for a file in Easy mode (flags bit 4, docs/save_format.md; Files_FileMenuItem) |
| `gUnk_0801E8B4` | `gTextNormal` | "Normal": drawn for a file in Normal mode (flags bit 4 clear; Files_FileMenuItem) |
| `gUnk_0801E8BC` | `gTextEnd` | "End": drawn instead of the world and level once the file is past world 10 (11 with flags bit 1, the last world open; Files_FileMenuItem) |
| `gUnk_0801E8C0` | `gTextDeleteSavedData` | glyphs 0xBB-0xC6 (a label drawn as consecutive tiles): file menu item 6, which opens Files_DeleteScreen (save_files input: "Delete saved data") |
| `gUnk_0801E944` | `gNameEntryColumnKinds` | u8 by column: the kind under the name entry's cursor on the kind row (Files_NameEntryItem) |
| `gUnk_0801E94F` | `gNameEntryKindColumns` | u8 by kind: the column the name entry puts the cursor on (Files_NameEntryItem) |
| `gUnk_0801E954` | `gTextDeleteMenuTitle` | glyphs 0xC7-0xDE (a label): item 0x1FF of the delete screen's file menu (Files_DeleteMenuItem) |
| `gUnk_0801E970` | `gTextDeleteConfirm1` | first line of the delete confirmation (Files_DeleteConfirmItem, item 0x100) |
| `gUnk_0801E990` | `gTextDeleteConfirm2` | second line of the delete confirmation (Files_DeleteConfirmItem, item 0x100) |
| `gUnk_0801E9AC` | `gTextDeleteYes` | "Yes": the delete confirmation's item 1, which clears the file (Files_DeleteConfirmItem) |
| `gUnk_0801E9B0` | `gTextDeleteNo` | "No": the delete confirmation's item 2 (Files_DeleteConfirmItem) |
| `gUnk_080C3B88` | `gWorldShortNames` | const u8 *[12]: "Training", "Grass", "Ocean", ... "Last": the world a file has reached (Files_FileMenuItem) |
| `gUnk_080C3BB8` | `gFileMenuFiles` | u8[5] {1, 2, 3, 4, 0}: the save file of file menu items 0x101-0x105 (Files_FileMenuItem) |
| `gUnk_080C3BD0` | `gNameEntryRows` | const u8 *[3][6]: the name entry's character rows per kind: upper case, lower case, kana (Files_NameEntryItem) |
| `gUnk_080C3C00` | `gDefaultNames` | const u8 *[4], each "Kururin": the name the name entry fills in on Start (Files_EnterName) |
| `gUnk_080C6324` | `gMenuCursorColors` | 4 pointers to the colour the menu cursor cycles through, written to OBJ palette 14 colour 15 (Files_NameEntryItem, sub_080135E8) |
| `gUnk_08130908` | `gMenuObjPalettes` | 0x1C0 bytes, OBJ palettes 0-13 of the menu screens (Files_MainState, the mode menu, Challenge, Practice) |
| `gUnk_0838D420` | `gMenuObjTiles` | 0x4680 bytes of OBJ tiles from tile 0x120 (VRAM 0x06012400), loaded by the menu screens (Files_MainState, Save_LoadAtBoot, the mode menu, Practice) |
| `gUnk_081DC948` | `gTextPalettes` | 0x40 bytes: palettes 14-15 of both BG (0x050001C0) and OBJ (0x050003C0), the colours text is drawn in (cursor colour 14/15) |
| `gUnk_083AA618` | `gMenuCursorTiles` | 0x200 bytes of OBJ tiles: the 32x32 menu cursor (the name entry's tile 0x354 at 0x06016A80, Files_EnterName; the bonus tasks load it at 0x060169A0) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk0800BC70` | `NameEntryMenu` | the name entry menu of Files_EnterName: its Menu and the name it edits |

## text

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08002848` | `Text_FormatTime` | writes a frame count as the time MM'SS"CC (60 frames a second, gFrameHundredths) |
| `sub_080028EC` | `Text_DrawSmall` | draws a string in the 8x8 font (tile c - 0x20); 0xDE/0xDF marks combine (gDakutenChars, gHandakutenChars) |
| `sub_080029C4` | `Text_DrawSmallHex` | draws the low digits nibbles of a value in hexadecimal with Text_DrawSmall |
| `sub_08002A20` | `Text_DrawSmallBinary` | draws the low digits bits of a value in binary with Text_DrawSmall |
| `sub_08002A6C` | `Text_DrawSmallNumber` | draws a signed number in decimal with Text_DrawSmall, no leading zeros |
| `sub_08002B30` | `Text_DrawSmallNumber5` | draws a u16 as five right-aligned decimal digits with Text_DrawSmall |
| `sub_08002BF4` | `Text_DrawSmallNumber3` | draws a u8 as three right-aligned decimal digits with Text_DrawSmall |
| `sub_08002C60` | `Text_DrawSmallNumberAligned` | Text_DrawNumberAligned drawn with Text_DrawSmall (docs/skipped.md: the same code, gPowersOf10) |
| `sub_08002CD8` | `Text_DrawSmallTime` | draws a frame count as a time (Text_FormatTime) with Text_DrawSmall; 60 minutes: "--'--\"--" |
| `sub_08002D20` | `Text_DrawTime` | draws a frame count as a time (Text_FormatTime) with Text_Draw; 60 minutes: "--'--\"--" |
| `sub_08002D68` | `Text_Draw` | draws a string in the 8x16 font (tile 0xE0 + 2 * c); 0xDE/0xDF marks combine |
| `sub_08002E44` | `Text_DrawNumber` | draws a signed number in decimal with Text_Draw, no leading zeros |
| `sub_08002F08` | `Text_DrawNumberAligned` | draws a number right-aligned in digits characters with Text_Draw (gPowersOf10) |
| `sub_08002F80` | `Text_PrintSmall` | prints a string in the 8x8 font from the text cursor: '\n', the escapes #h, #k, #p, the marks |
| `sub_080030E4` | `Text_Print` | Text_PrintSmall in the 8x16 font |
| `sub_0800324C` | `Text_DrawDigitTime` | draws a frame count as a time with Text_DrawDigits; 60 minutes: "--'--\"--" |
| `sub_080032AC` | `Text_DrawDigits` | draws the digits, ' " and - of a string with the digit tiles 0x40 + 2 * digit, 0x54-0x58 |
| `sub_08003384` | `Text_GetDakutenChar` | gDakutenChars[code - 0x80]: the character a dakuten mark (0xDE, JIS X 0201) makes of code |
| `sub_0800339C` | `Text_GetHandakutenChar` | gHandakutenChars[code - 0x80]: the character a handakuten mark (0xDF, JIS X 0201) makes of code |
| `sub_080033B4` | `Text_ConvertToU16` | copies a string as halfword characters: #h/#k (hiragana) applied, the marks combined |
| `sub_08003464` | `Text_Convert` | copies a string with #h/#k applied (hiragana: 0xA6-0xBF down, 0xC0-0xDD up by 0x20); the name entry's kana |
| `sub_080034B4` | `Text_DrawSmallName` | Text_DrawSmall with proportional lower case (gSmallFontWidths), then the rank mark gSmallRankMarks[rank] (callers pass a file's or record's name and rank) |
| `sub_080035F4` | `Text_DrawName` | Text_Draw with proportional lower case (gFontWidths), then the rank mark gRankMarks[rank] (callers pass a file's or record's name and rank) |
| `sub_08003738` | `Text_DrawSmallProportional` | Text_DrawSmall with proportional lower case (gSmallFontWidths), without a rank mark; nothing calls it |
| `sub_08003828` | `Text_DrawProportional` | Text_Draw with proportional lower case (gFontWidths), without a rank mark; nothing calls it |
| `sub_0800391C` | `Text_PrintSmallProportional` | Text_PrintSmall with proportional lower case |
| `sub_08003A98` | `Text_PrintProportional` | Text_Print with proportional lower case (gFontWidths) |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_0801CB9C` | `gFrameHundredths` | u8[60], the hundredths of a second of each frame of a second (Text_FormatTime) |
| `gUnk_0801CBD8` | `gDakutenChars` | u16[128] by code - 0x80: the character code + dakuten (0xDE, JIS X 0201's mark) gives, or code itself (Text_Draw, Text_GetDakutenChar) |
| `gUnk_0801CCD8` | `gHandakutenChars` | u16[128] by code - 0x80: the character code + handakuten (0xDF) gives, or code itself (Text_Draw, Text_GetHandakutenChar) |
| `gUnk_0801CDD8` | `gSmallFontWidths` | u8[26], the width in pixels of 'a'-'z' in the 8x8 font (Text_DrawSmallName) |
| `gUnk_0801CDF2` | `gFontWidths` | u8[26], the width in pixels of 'a'-'z' in the 8x16 font (Text_DrawName, Text_PrintProportional) |
| `gUnk_0801CE0C` | `gTextNoTime` | "--'--\"--": drawn instead of a time of 216000 frames (60 minutes, no record; Text_DrawTime) |
| `gUnk_080C28D0` | `gPowersOf10` | u32[11]: 0, 1, 10, ..., 10^9 (Text_DrawNumberAligned indexes it by digit count) |
| `gUnk_080C28FC` | `gSmallRankMarks` | u16[][4], 0-terminated tiles Text_DrawSmallName draws after a name, by its rank |
| `gUnk_080C291C` | `gRankMarks` | u16[][4], 0-terminated characters Text_DrawName draws after a name, by its rank (= gSmallRankMarks[4]) |
| `gUnk_03003EE8` | `gTextCursor` | struct TextCursor: where the Text_Print* functions draw: {x, y, dx, dy, palette, list}, left after the text |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk03003EE8` | `TextCursor` | the cursor the Text_Print* functions draw from and leave after the text (x, y, dx, dy per character and line, palette, sprite list) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `TextCursor` | `unk4` | `palette` | the sprites' palette (Text_PrintSmall: '#p' sets it; 14/15 for the blinking current item) |
| `TextCursor` | `unk6` | `list` | the sprite list passed to Sprite_Alloc (low byte); bits 2-3 the sprites' priority (Text_PrintSmall) |

## menu

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800459C` | `Menu_RunWrap` | Menu_Run with Menu_MoveWrap: up/down move the current item, wrapping around |
| `sub_08004604` | `Menu_RunLeftRightWrap` | Menu_Run with Menu_MoveLeftRightWrap: left/right, wrapping around |
| `sub_0800466C` | `Menu_Run` | a menu's frame: moves the current item on the keys (Menu_Move), calls its function with it (the result), then draws: 0xFFFF, 0x100 + each item, 0x1FF |
| `sub_080046D4` | `Menu_RunLeftRight` | Menu_Run with Menu_MoveLeftRight: left/right, stopping at the ends |
| `sub_0800473C` | `Menu_RunSelect` | Menu_Run with Menu_MoveSelect: Select steps through the items |
| `sub_080047A4` | `Menu_Draw` | Menu_Run's drawing calls alone: 0xFFFF, 0x100 + each item, 0x1FF |
| `sub_080047F8` | `Menu_MoveWrap` | moves the current item on up/down within 1..itemCount, wrapping around; song 0x20 when it moved |
| `sub_08004858` | `Menu_MoveLeftRightWrap` | moves the current item on left/right, wrapping around; song 0x20 |
| `sub_080048B8` | `Menu_Move` | moves the current item on up/down within 1..itemCount, stopping at the ends; song 0x20 |
| `sub_08004910` | `Menu_MoveLeftRight` | moves the current item on left/right, stopping at the ends; song 0x20 |
| `sub_08004968` | `Menu_MoveSelect` | moves the current item to the next on Select, back to 1 after the last; song 0x20 |
| `sub_080049AC` | `Menu_StepValueWrap` | steps a value within 0..max on left/right, wrapping around; song 0x20 when it changed |
| `sub_080049F8` | `Menu_StepValueUpDownWrap` | steps a value within 0..max on up/down, wrapping around; song 0x20 |
| `sub_08004A44` | `Menu_StepValue` | steps a value within 0..max on left (down) / right (up), stopping at the ends; song 0x20 |
| `sub_08004A88` | `Menu_StepValueReversed` | Menu_StepValue with right down and left up |
| `sub_08004ACC` | `Menu_StepValueUpDown` | Menu_StepValue on up (down) / down (up) |
| `sub_08004B10` | `Menu_StepValueLR` | Menu_StepValue on L (down) / R (up) |
| `sub_08004B58` | `Menu_StepValueLRWrap` | Menu_StepValueWrap on L / R |
| `sub_08004BA8` | `Menu_StepValueBy10` | steps a value within 0..max by 10 on L/R and by 1 on left/right, wrapping around; song 0x20 |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk0800466C` | `Menu` | a menu: Menu_Run* call its function with the current item, then for drawing with 0xFFFF and 0x100 + each item, then 0x1FF |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `Menu` | `unk0` | `itemCount` | the number of items: Menu_Run calls func for 0x100..0x100 + itemCount, Menu_Move keeps current within 1..itemCount |
| `Menu` | `unk1` | `current` | the current item, 1..itemCount (Menu_Move); func is called with it |
| `Menu` | `unk2` | `x` | where item 0x100 puts the text cursor (gTextCursor.x = p->x in every menu function) |
| `Menu` | `unk3` | `y` | gTextCursor.y on item 0x100 |
| `Menu` | `unk8` | `func` | the menu function: called with the current item (its result is Menu_Run's), 0xFFFF, 0x100.., 0x1FF |

## mode_menu

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800C75C` | `ModeMenu_Run` | the mode menu screen with its own frame loop (Play_MainState): Practice, Adventure (Normal/Easy), Challenge, the linked game (waiting for a partner, the MultiBoot send), Make Up; returns the mode, -1 on B |
| `sub_0800D2F8` | `ModeMenu_DrawCursor` | the mode menu's cursor at an item: a 64x64 sprite through the six frames of gModeMenuCursorFrames |
| `sub_0800D390` | `ModeMenu_DrawDescription` | the description of the current mode menu item: its OBJ tiles (gModeMenuDescriptionTiles) copied when the item changed, drawn as 15 sprites |
| `sub_0800D434` | `ModeMenu_DrawMultiBootLabel` | the five 32x16 sprites (gMultiBootLabelTiles) ModeMenu_Run draws, blinking, while it sends the MultiBoot image |
| `sub_0800D4F4` | `MapScreen_GetLevelDataSize` | the uncompressed size of the current level's data (KuruTools LevelEntry, layer 0): 0 for a level that does not exist |
| `sub_0800D518` | `MapScreen_ShowPicture` | shows a window of an 8bpp picture at a pixel: its tile rows to a layer's tiles, the layer scrolled by the rest |
| `sub_0800D58C` | `MapScreen_ShowMap` | the map picture on BG 2: the training map (gTrainingMapPicture) for levels 0-4, else the world map (gWorldMapPicture) scrolled to the level |
| `sub_0800D690` | `MapScreen_Run` | the Adventure map screen with its own frame loop: the level menu (MapScreen_LevelMenuItem), the path, the level cursor and world tasks; returns 0 when a level starts, -1 on B |
| `sub_0800DB44` | `MapScreen_LevelMenuItem` | the map screen's level menu items: item 1 picks the level (Menu_StepValue); 0x101 the file's name and time and the level's "TOP3" records |
| `sub_0800DD54` | `MapScreen_UpdatePath` | a map screen task: a marker per level along the path (gTrainingMapPositions, gMapLevelPositions) by its mark, dots between levels |
| `sub_0800E0D8` | `MapScreen_UpdateCursor` | a map screen task at the current level's position: the animated cursor (gMapCursorAnim) and the mark of a level whose bonus is not taken |
| `sub_0800E22C` | `MapScreen_UpdateSlidingCursor` | MapScreen_UpdateCursor that slides from the old level to a newly opened one in 60 frames (MapScreen_Run in mode 1) |
| `sub_0800E4BC` | `MapScreen_UpdateWorldIcon` | a map screen task: the current world's icon (gWorldIconFrames, two frames a world) at (0xA8, 0x80), sliding between worlds |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03000008` | `gModeMenuCursorFrame` | u16, the mode menu cursor's animation (ModeMenu_DrawCursor: + 0x40 a frame, 0x100 a frame of gModeMenuCursorFrames, modulo 0x600) |
| `gUnk_0300000C` | `gModeMenuDescriptionItem` | s32, the mode menu item whose description ModeMenu_DrawDescription copied last (negative: none) |
| `gUnk_03004480` | `gMultiBootParam` | struct MultiBootParam passed to the SDK's MultiBootInit/Main/StartMaster/CheckComplete (ModeMenu_Run, the debug MultiBoot test) |
| `gUnk_030044D8` | `gMultiBootImageSize` | s32, the size of the MultiBoot image (ModeMenu_Run) |
| `gUnk_030044DC` | `gMultiBootImage` | const u8 *, the MultiBoot image gMultiBootProgram (ModeMenu_Run) |
| `gUnk_080E2668` | `gMultiBootProgram` | the MultiBoot image sent to link partners, up to gUnk_080E3B60 (ModeMenu_Run): the linked game's client program |
| `gUnk_0801E9B4` | `gMapScreenSpriteTiles` | {src, tile, tiles} OBJ VRAM copies, NULL-terminated: the Adventure map screen's (MapScreen_Run, Practice) |
| `gUnk_0801EA94` | `gTrainingMapPositions` | {u16 x, y} per training level (world 0): where the map screen's cursor and path are |
| `gUnk_0801EAAC` | `gMapLevelPositions` | {u16 x, y} per level number world * 3 + level + 2 (worlds 1+): where the map screen's cursor and path are, the world map's scroll (MapScreen_ShowMap) |
| `gUnk_0801EB44` | `gTextTop3` | "  TOP3": drawn above the level's three records (MapScreen_LevelMenuItem) |
| `gUnk_080C3C10` | `gModeMenuCursorPositions` | {u8 x, y}[]: where the mode menu's cursor is for each item, from 1 (ModeMenu_DrawCursor) |
| `gUnk_080C3C20` | `gModeMenuDescriptionTiles` | const void *[]: the 0x780-byte OBJ tiles of each mode menu item's description (ModeMenu_DrawDescription) |
| `gUnk_080C470C` | `gMapCursorAnim` | Unk08004534Frame[]: the map screen cursor's animation (0x400 bytes of OBJ tiles per frame, MapScreen_UpdateCursor) |
| `gUnk_08139DDC` | `gMultiBootLabelTiles` | 0x500 bytes copied to OBJ VRAM 0x06017A00 when the MultiBoot send starts: ModeMenu_DrawMultiBootLabel's sprites |
| `gUnk_083AAC18` | `gWorldIconFrames` | struct WorldIconFrame[]: {offset into gIconTiles, ?}, two per world: 0x400 bytes MapScreen_UpdateWorldIcon copies to OBJ VRAM 0x06016A80 |
| `gUnk_083AACD0` | `gModeMenuCursorFrames` | gWorldIconFrames[23]: six entries, the 0x800-byte frames of the mode menu's cursor (ModeMenu_DrawCursor) |
| `gUnk_083AAD00` | `gIconTiles` | the OBJ tiles gWorldIconFrames' offsets point into (MapScreen_UpdateWorldIcon, ModeMenu_DrawCursor) |
| `gUnk_080F7980` | `gWorldMapPicture` | an 8bpp picture 0x87 tiles wide, its palettes 0x2A300 and 0x33D00 bytes on: the world map MapScreen_ShowMap scrolls to the level (gMapLevelPositions) |
| `gUnk_08121E80` | `gTrainingMapPicture` | an 8bpp picture of 30 x 20 tiles, its palette 0x9600 bytes on: the map of the training levels 0-4 (MapScreen_ShowMap) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk083AAC18` | `WorldIconFrame` | an entry of gWorldIconFrames: an offset into gIconTiles |
| `Unk0800E0D8` | `MapCursor` | the task MapScreen_UpdateCursor runs |
| `Unk0800E22C` | `MapSlidingCursor` | the task MapScreen_UpdateSlidingCursor runs |
| `Unk0800DD54` | `MapPath` | the task MapScreen_UpdatePath runs |

## pause

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08009438` | `Pause_Update` | the pause menu, once a frame of a level (Play_RunLevel): Start opens/closes it (gGameState.paused), runs the menu, See Map or Make Up; the linked game's shared menu; its result |
| `sub_080098C0` | `Pause_MenuItem` | the pause menu's items in modes 1-3 (see_map input: See Map, Play Again, Make Up, Quit): 2 play again, -1 quit |
| `sub_08009C44` | `Pause_PracticeMenuItem` | the pause menu's items in Practice (mode 5): See Demo (sets GAME_FLAG_DEMO), See Map, Play Again, Make Up, Quit |
| `sub_0800A008` | `Pause_DemoMenuItem` | the pause menu's items in a Practice demo (save_practice input: next area, See Map, in-place Make Up, Quit Demo) |
| `sub_0800A48C` | `Pause_LinkMenuItem` | the linked game's pause menu items (player 0's keys): item 1 returns 2, item 2 -1 |
| `sub_0800A760` | `Pause_SeeMap` | See Map (pauseScreen 1): gGameState.flags bit 0 (the free camera, see_map input), four blinking arrows; B returns to the menu |
| `sub_0800A88C` | `Pause_MakeUp` | the in-place Make Up (pauseScreen 2, save_practice input): L steps the Helirin's type, R its colour among the unlocked ones, saved (Save_Write); B returns |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_030043F0` | `gPauseMenu` | struct PauseMenu: the menu Pause_Update runs (Menu_RunWrap), then at +0x1C its OBJ VRAM copy list |
| `gUnk_0801E768` | `gPauseMenuTiles` | {src, tile, tiles} OBJ VRAM copies, NULL-terminated: the pause menu of modes 1-3 (Pause_Update) |
| `gUnk_0801E790` | `gPracticePauseMenuTiles` | {src, tile, tiles} OBJ VRAM copies: the pause menu of mode 5, Practice (Pause_Update) |
| `gUnk_0801E7A8` | `gDemoPauseMenuTiles` | {src, tile, tiles} OBJ VRAM copies: the pause menu of a Practice demo (mode 5 with GAME_FLAG_DEMO, Pause_Update) |
| `gUnk_0801E7D8` | `gLinkPauseMenuTiles` | {src, tile, tiles} OBJ VRAM copies: the pause menu of mode 4, the linked game (Pause_Update) |
| `gUnk_0801E7F0` | `gTextPause` | "PAUSE": blinks while mode 6 is paused (Pause_Update, Text_DrawSmall) |
| `gUnk_081E5268` | `gSeeMapArrowTiles` | 0x200 bytes copied to OBJ VRAM 0x060171A0: the four arrows Pause_SeeMap draws at the screen's edges |
| `gUnk_081E5468` | `gMakeUpLRTiles` | 0x100 bytes copied to OBJ VRAM 0x060171A0 while bit 4 or 5 of the frame counter is set: Pause_MakeUp's two sprites (L, R) |
| `gUnk_081E5568` | `gMakeUpLRTilesBlink` | 0x100 bytes copied there otherwise (Pause_MakeUp) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk030043F0` | `PauseMenu` | gPauseMenu: the Menu Pause_Update runs, then the OBJ tiles of its items |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `PauseMenu` | `unk1C` | `tiles` | the OBJ VRAM copies of the menu's items (gPauseMenuTiles..), copied again when See Map or Make Up returns to the menu |
| `GameState` | `unk28` | `pauseScreen` | 0 the pause menu, 1 See Map (Pause_SeeMap), 2 Make Up (Pause_MakeUp): set by the pause menu's items, cleared on B (Pause_Update) |

## results

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800AA0C` | `Results_Run` | the result screen over the level (Play_MainState at a level's end): Results_EnterRecord, the bonus taken, saves; the times, the records (the new one blinking); A or B closes the window |
| `sub_0800B000` | `Results_CheckRankUp` | the rank-ups (save_ending input): while every level's best time beats the next rank's gLevelTimes, shows the rank-up text, raises the file's rank, enters the Master records; saves |
| `sub_0800B36C` | `Camera_Update` | the camera origin gCameraX/gCameraY: centred on the Helirin, or moved by the D-pad (See Map, flags bit 0); clamped to gCamera's bounds; scrolls maps 1-3 (map 3 parallax) |
| `sub_0800B604` | `Results_EnterRecord` | enters the level's result in the save file (marks[], flags bits 1-2) and its three records (sorted by time); returns the place, 4 \| place for a new one, 8 for mark 2 |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_030043EC` | `gCloudDriftY` | s32 16.16 y offset of map 3 in world 6 (Cloud, gWorldShortNames[6]), += 0x2AAA a frame (Camera_Update): the drifting background |
| `gUnk_0300446C` | `gCloudDriftX` | s32 16.16 x offset of map 3 in world 6 (Cloud), += 0x4000 a frame (Camera_Update) |
| `gUnk_0801E7F8` | `gResultsSpriteTiles` | {src, tile, tiles} OBJ VRAM copies, NULL-terminated: the result screen's sprites (Results_Run) |
| `gUnk_0801E818` | `gTextBestTime` | "BESTTIME": drawn by Results_Run at (0x38, 0x34) (Text_Draw) |
| `gUnk_081E3B88` | `gResultsOtherHelirinTiles` | 0x800 bytes copied to OBJ VRAM 0x06016A80 by Results_Run for mark 2 (Results_EnterRecord's 8: cleared with another Helirin size, no record), drawn blinking |

## level_map

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080057B8` | `LevelMap_ParseNumber` | the number at the start of a map string in the game's digits 0xE0-0xE9 |
| `sub_080057DC` | `LevelMap_ReadNumber` | LevelMap_ParseNumber at gMapReadCursor, which it moves past the number |
| `sub_0800580C` | `LevelMap_Read` | reads a level map (Level_StartTasks): the start, healing and goal tiles (0xED-0xFF) into gCamera, the camera bounds, the objects and the bonus it starts |
| `sub_08005D78` | `LevelMap_StartMarkedObject` | starts the object after marker 0xF1 with a given number (map code 0xF0, LevelMap_Read) at a tile |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03000000` | `gMapReadCursor` | const u16 *, the map position LevelMap_ReadNumber reads from (LevelMap_Read, LevelMap_StartMarkedObject) |

## level_intro

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800F624` | `LevelIntro_Run` | the screen before a level (Play_MainState sub-state 3): OBJ tiles by world and level; for the first level of worlds 1-10 a picture and its animation (A/B); then LevelIntro_PlayStartAnim |
| `sub_0800F954` | `LevelIntro_PlayStartAnim` | the animation that ends LevelIntro_Run: gLevelStartAnim's pictures on BG 2 with songs 0x4A-0x4D, until it ends or A/B |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_080C4734` | `gLevelIntroWorldNameTiles` | const u8 *[] by world: 0x400 bytes of compressed OBJ tiles (LevelIntro_Run, to 0x06012400): the world's name (screenshot; first named `gLevelIntroWorldTiles`) |
| `gUnk_080C4764` | `gLevelIntroLevelNumberTiles` | const u8 *[] by level: 0x200 bytes of compressed OBJ tiles (LevelIntro_Run, to 0x06012800): the level's number (screenshot; first named `gLevelIntroLevelTiles`) |
| `gUnk_080C4778` | `gLevelIntroPictureTiles` | const u8 *[] by world: 0xC80 bytes of compressed OBJ tiles (LevelIntro_Run, to 0x06012A00: tile 0x150, the 5x5 sprite picture) |
| `gUnk_080C47A4` | `gLevelIntroAnim` | u16 animation frames of LevelIntro_Run's picture |
| `gUnk_080C47F0` | `gLevelIntroEndAnim` | u16 animation frames LevelIntro_Run switches to on the last A/B |
| `gUnk_080C4810` | `gLevelIntroAnimTiles` | const u8 *[] by frame value of gLevelIntroAnim/gLevelIntroEndAnim: 0x200 bytes of compressed OBJ tiles (LevelIntro_Run) |
| `gUnk_08181940` | `gLevelIntroWorld5Picture1` | 0xC80 bytes of compressed OBJ tiles LevelIntro_Run swaps in for gLevelIntroPictureTiles[5] (the first A/B in world 5) |
| `gUnk_08181E60` | `gLevelIntroWorld5Picture2` | 0xC80 bytes of compressed OBJ tiles LevelIntro_Run swaps in (the second A/B in world 5) |
| `gUnk_080C45D0` | `gLevelStartAnim` | u16 animation frames: LevelIntro_PlayStartAnim's pictures (gLevelStartPictures) on BG 2 |
| `gUnk_080C4490` | `gLevelStartPictures` | {map, tile list}[]: the pictures of gLevelStartAnim's frames (Bg_LoadMap8bpp, LevelIntro_PlayStartAnim) |

## challenge

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800FAAC` | `Challenge_Run` | the Challenge course menu (world 12, mode 3; save_challenge input): rows of five courses, the selected one's minimap; A starts it (0), B -1 |
| `sub_0800FDC8` | `Challenge_MenuItem` | the Challenge course menu's items: the column, the scrolled rows of courses with their marks, the selected course's best time and records |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_0801EB4C` | `gChallengeSpriteTiles` | {src, tile, tiles} OBJ VRAM copies, NULL-terminated: the Challenge course menu's (Challenge_Run) |
| `gUnk_0801EB74` | `gTextChallengeTop3` | "  TOP3": drawn above the course's three records (Challenge_MenuItem; a copy of gTextTop3) |

## practice

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08018D34` | `Practice_Run` | Practice (mode 5, save_practice input): the map screen's level menu and tasks, then the area menu of the level; A picks the area (0), B -1 |
| `sub_080191F8` | `Practice_AreaMenuItem` | Practice's area menu items: a frame around the rows, "Area1".."Area4" one per area (gLevelAreaCounts) |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_0801F180` | `gTextArea1` | "Area1": item 0x101 of Practice's area menu (Practice_AreaMenuItem, Text_PrintSmallProportional) |
| `gUnk_0801F188` | `gTextArea2` | "Area2": item 0x102 (Practice_AreaMenuItem) |
| `gUnk_0801F190` | `gTextArea3` | "Area3": item 0x103 (Practice_AreaMenuItem) |
| `gUnk_0801F198` | `gTextArea4` | "Area4": item 0x104 (Practice_AreaMenuItem) |

## bonus

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08018330` | `Bonus_UpdateColor` | the level's colour bonus (a LevelMap_Read task): touched, it swaps its colour with the Helirin's for a while (Helirin_SetColor), sets GAME_FLAG_BONUS_TAKEN |
| `sub_080184DC` | `Bonus_UpdateType` | the level's type bonus: touched (Collision_FindPointInShapes), it swaps its Helirin type with the Helirin's for 0x79 frames (Helirin_SetSize), song 0x47, GAME_FLAG_BONUS_TAKEN |
| `sub_0801866C` | `Bonus_UpdateItem` | the level's item bonus (unlocks bits 0-9): touched, GAME_FLAG_BONUS_TAKEN, song 0x47, and a Bonus_UpdateWornItem task takes over |
| `sub_080187EC` | `Bonus_UpdateWornItem` | an item the Helirin wears (taken, or the save file's items): follows a Helirin point, flies off when the Helirin is hit and homes back |
| `sub_08018B34` | `Bonus_DrawColorIcon` | a task drawing a colour bonus's 16x16 sprite (gColorBonusTilesA/B); nothing starts it |
| `sub_08018BC8` | `Bonus_DrawTypeIcon` | Bonus_DrawColorIcon with a type bonus's tiles (gTypeBonusTiles); nothing starts it |
| `sub_08018C3C` | `Bonus_UpdateMakeUpIcon` | the Make Up screen's icon of an unlocked item (one task per set bit 0-9), animated (gItemAnims) while the item is worn |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_0801ECE7` | `gMakeUpIconPalettes` | u8[10], the OBJ palette of each item's Make Up icon (Bonus_UpdateMakeUpIcon, the Make Up screen) |
| `gUnk_0801EFCC` | `gItemAnims` | Unk08004534Frame[10][5]: each item's animation (0x80 bytes of OBJ tiles per frame; Bonus_UpdateItem, Bonus_UpdateMakeUpIcon) |
| `gUnk_0801F15C` | `gWornItemPalettes` | u8[10]: the OBJ palette of each item worn (Bonus_UpdateWornItem) |
| `gUnk_0801F168` | `gWornItemFollowAnim` | u16 animation frames of a worn item while it follows the Helirin (Bonus_UpdateWornItem) |
| `gUnk_0801F16C` | `gWornItemFlyAnim` | u16 animation frames of a worn item while it flies (Bonus_UpdateWornItem) |
| `gUnk_080C62B4` | `gColorBonusPalettes` | const void *[]: the 0x20-byte OBJ palette of each colour bonus (Bonus_UpdateColor) |
| `gUnk_080C62D0` | `gTypeBonusTiles` | const void *[]: 0x80 bytes of OBJ tiles per Helirin type (Bonus_UpdateType) |
| `gUnk_080C630C` | `gBonusShape` | struct CollisionShape of the colour and type bonuses, tested against the Helirin (Bonus_UpdateType) |
| `gUnk_080C6334` | `gItemPalettes` | const void *[10]: the 0x20-byte OBJ palette of each item (Bonus_UpdateItem) |
| `gUnk_080C635C` | `gWornItemTiles` | const void *[4][4]: 0x20 bytes of OBJ tiles by (item & 3, frame) (Bonus_UpdateWornItem) |
| `gUnk_080C639C` | `gWornItemShape` | struct CollisionShape of a worn item's sprite, tested against the Helirin when it flies back (Bonus_UpdateWornItem) |
| `gUnk_083A8798` | `gColorBonusTilesA` | 0x80 bytes of OBJ tiles of a colour bonus: Bonus_UpdateColor copies them for an odd colour, Bonus_DrawColorIcon for an even one |
| `gUnk_083A8998` | `gColorBonusTilesB` | 0x80 bytes of OBJ tiles of a colour bonus: the other parity (Bonus_UpdateColor, Bonus_DrawColorIcon) |
| `gUnk_083A8B98` | `gTypeBonusGfx` | 0x20 bytes copied to OBJ palette 0x05000300, and from + 0x1A80 0x200 bytes to OBJ VRAM 0x060169A0: the type bonus (Bonus_UpdateType) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk080184DC` | `BonusTask` | the level's colour or type bonus, the task Bonus_UpdateColor / Bonus_UpdateType runs |
| `Unk08018330` | `MapBonusTask` | the bonus tasks as LevelMap_Read sets them up (x, y, the unlock bit) |
| `Unk0801866C` | `ItemBonus` | the level's item bonus, the task Bonus_UpdateItem runs |
| `Unk080187EC` | `WornItem` | an item worn by the Helirin, the task Bonus_UpdateWornItem runs |
| `Unk08018C3C` | `MakeUpIcon` | an item's icon on the Make Up screen, the task Bonus_UpdateMakeUpIcon runs |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `BonusTask` | `unk10` | `value` | the colour (Bonus_UpdateColor) or type (Bonus_UpdateType) it gives: swapped with the Helirin's for a while |
| `ItemBonus` | `unk10` | `item` | the item, bit 0-9 of the save file's unlocks (gItemPalettes[item], gItemAnims[item]) |
| `WornItem` | `unk2A` | `item` | the item, bit 0-9 (tile 0x371 + item, gWornItemPalettes[item]) |
| `WornItem` | `unk2C` | `point` | the Helirin point it sits on (points[point]) |
| `MakeUpIcon` | `unkC` | `item` | the item, bit 0-9 of the save file's unlocks (tile 0x164 + 4 * item) |
| `MapBonusTask` | `unk10` | `unlock` | which bit of the save file's unlocks (within its group): the colour, type or item (LevelMap_Read) |

## collection

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800EF44` | `Collection_Run` | the bonus collection screen: mode 0 after a level whose bonus was taken (one picture per item unlocked), 1 the ending's pictures 0x10-0x1B, 2-3 variants; the window opens, A/B or 300 frames, closes |
| `sub_0800F2A8` | `Collection_UpdatePicture` | the task of one picture of the collection screen: its OBJ tiles, position and animation by picture, drawn as one or two sprites |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_080C43D8` | `gCollectionAnims` | the animations (sub_0800456C frames) of the collection pictures by picture (Collection_UpdatePicture) |
| `gUnk_080C4448` | `gCollectionPositions` | u8 {x, y} by picture of the collection pictures (Collection_UpdatePicture) |
| `gUnk_080C4460` | `gCollectionTiles` | u16 {first OBJ tile, tiles a frame} by picture of the collection pictures (Collection_UpdatePicture) |
| `gUnk_0817AD68` | `gCollectionPalettes` | 0x200 bytes copied to OBJ palette RAM by Collection_Run (modes 0, 3: the items' pictures) |
| `gUnk_0817B368` | `gCollectionEndingPalettes` | 0x200 bytes copied to OBJ palette RAM by Collection_Run (mode 1, the ending) |
| `gUnk_08186E8C` | `gCollectionBgPalettes` | compressed (sub_08004084) 0x200 bytes; the second half goes to BG palette 0x05000100 (Collection_Run) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk0800F2A8` | `CollectionPicture` | one picture of the collection screen, the task Collection_UpdatePicture runs |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `CollectionPicture` | `unk20` | `picture` | the picture: 0-9 an item (unlocks bits 0-9), 0x10-0x1B the ending's (Collection_Run) |

## makeup

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080131F8` | `Lives_Reset` | resets gLivesSettings to gDefaultLivesSettings (Game_Boot) |
| `sub_08013210` | `Lives_Init` | sets a Helirin's lives (and maxLives) and size by game mode from gLivesSettings (Helirin_Init, Play_RunLevel) |
| `sub_080133A8` | `MakeUp_StartWornItems` | starts a Bonus_UpdateWornItem task for each item worn (the save file's items bits 0-9) |
| `sub_080133E4` | `MakeUp_Run` | the Make Up screen (mode 7, save_makeup input): the Helirin types, colours and items unlocked; B restores them, a choice saves (Save_Write) |
| `sub_080135E8` | `MakeUp_MenuItem` | the Make Up menu's items: three rows of five types, a row of seven colours and End, the items toggled on and off |
| `sub_08013A58` | `Lives_SettingsScreen` | a screen with its own frame loop over a copy of gLivesSettings (docs/remaining.md); nothing references it |
| `sub_08013B5C` | `Lives_SettingsMenuItem` | Lives_SettingsScreen's menu function: the lives of each mode stepped with Menu_StepValueWrap |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03004520` | `gLivesSettings` | struct LivesSettings (12 bytes), set from gDefaultLivesSettings by Lives_Reset; Lives_Init gives a Helirin its lives and size from it by mode |
| `gUnk_080C4870` | `gDefaultLivesSettings` | struct LivesSettings, the initial gLivesSettings (Lives_Reset) |
| `gUnk_03000010` | `gMakeUpIconTile` | u32 tile number of the next Make Up icon, from 0x120 in steps of 4 (MakeUp_MenuItem) |
| `gUnk_0801ECC0` | `gMakeUpSpriteTiles` | {src, tile, tiles} OBJ VRAM copies, NULL-terminated (MakeUp_Run) |
| `gUnk_0801ECE0` | `gMakeUpColorPalettes` | u8[7], the palette of each icon of the colour row (MakeUp_MenuItem, row 4) |
| `gUnk_0801ECF1` | `gMakeUpTypeColumns` | u8[], the column of the type rows 1-3 by cursor (MakeUp_MenuItem) |
| `gUnk_0801ECFB` | `gMakeUpColorColumns` | u8[], the column of the colour row 4 by cursor (MakeUp_MenuItem) |
| `gUnk_080C4884` | `gMakeUpIconTiles` | 22 pointers to 0x80-byte OBJ tiles, copied to 0x06012400..: the Make Up screen's icons (MakeUp_Run) |
| `gUnk_080F3B70` | `gMakeUpObjPalettes` | 0x1C0 bytes copied to OBJ palette RAM 0x05000200 (MakeUp_Run) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk03004520` | `LivesSettings` | gLivesSettings: the lives (and the linked game's sizes) each game mode gives a Helirin (Lives_Init) |
| `Unk0801ECC0` | `TileCopy` | an entry {src, tile, tiles} of a NULL-terminated list of OBJ VRAM copies (each copied to 0x06010000 + tile * 32, e.g. gMakeUpSpriteTiles) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `LivesSettings` | `unk0` | `lives` | the lives of modes 1, 2, 5 and 7 (Adventure, Easy, Practice, Make Up; Lives_Init) |
| `LivesSettings` | `unk1` | `challengeLives` | the lives of mode 3, Challenge (Lives_Init) |
| `LivesSettings` | `unk3` | `linkLives` | the lives of Helirin n in mode 4, the linked game (Lives_Init) |
| `LivesSettings` | `unk7` | `linkSizes` | the size of Helirin n in mode 4 (Lives_Init) |
| `LivesSettings` | `unk2` | `debugLives` | the lives of mode 6, the debug stage select's play (Lives_Init); deep review 2026-10-23 |
| `LivesSettings` | `unkB` | `invincible` | bit 0: the line "#h" muteki (むてき, invincible) of Lives_SettingsScreen; Lives_Init copies it to the Helirin's flags bit 4, which keeps it from crashing at 0 lives (Helirin_UpdateCollision); deep review 2026-10-23 |

## effects

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08017270` | `Effect_OpenWindow` | a task: the circular window (Bg_StartWindowCircle) grows by 8 a frame up to 0xA0, then the window is turned off |
| `sub_080172C8` | `Effect_CloseWindow` | a task: the circular window shrinks from 0xA0 by 8 a frame to 0, then BG0 and the window are turned off |
| `sub_08017320` | `Effect_BrightnessUp` | a task: BLDY (gBldyCopy) from 0 up to 16, 0x80/0x100 a frame |
| `sub_0801737C` | `Effect_BrightnessDown` | a task: BLDY from 16 down to 0, 0x80/0x100 a frame |
| `sub_080173CC` | `Effect_RotateBg2` | a task: turns BG2 by half the frame counter around the screen's centre |
| `sub_08017464` | `Effect_UpdateCrash` | the crash (Helirin flags bit 6, Helirin_Update): a 64x64 animation (gCrashAnim) at the Helirin |
| `sub_08017520` | `Effect_UpdateHitSpark` | the spark Helirin_UpdateCollision starts at each point that hits a wall (gHitSparkAnim) |
| `sub_080175BC` | `Effect_UpdateZoneHitRipple` | Effect_UpdateHitSpark with gZoneHitRippleAnim: for a Helirin whose flags have 0x40008 (bit 18 in a zone, bit 3); blue rings on screen (first named `Effect_UpdateZoneHitSpark`) |
| `sub_08017658` | `Effect_UpdateSpringSpark` | the spark Object_SpringDown starts when a Helirin bounces off it (gSpringSparkAnim) |
| `sub_080176F4` | `Effect_UpdateTimePenalty` | the 3 s penalty (Helirin_UpdateCollision adds 0xB4 frames to the timer): a sprite rising for 0x1E frames |
| `sub_0801776C` | `Effect_Firework` | a one-frame task: 32 Effect_UpdateFireworkSpark sprites thrown from a random point, song 0x50 (the goal, the results, the ending) |
| `sub_08017934` | `Effect_UpdateFireworkSpark` | one sprite thrown by Effect_Firework: slowed and falling, for a number of frames |
| `sub_080179C8` | `Effect_UpdateGoal` | the goal's palette animation (BG palettes 11-12, gGoalAnim, gGoalReachedAnim) and, once a Helirin is there (flags bit 3), its fireworks |
| `sub_08017AC8` | `Effect_CyclePalette` | a level task: cycles the first BG colours through the world's colour sets (KuruTools addr7, gPaletteCycleAnim) |
| `sub_08017B48` | `Effect_AnimateCopies` | a task: an animation whose frames queue a copy each (Main_QueueCopy) |
| `sub_08017BA8` | `Effect_AnimateBlockCopies` | a task: an animation whose frames queue a block copy each (Main_QueueBlockCopy): the world's tile animations (gWorldTileAnims) |
| `sub_08017C08` | `Effect_UpdateGoalBanner` | a banner (gGoalBannerTiles) moving left across the screen when a Helirin reaches the goal (flags bit 2, Play_CheckLevelEnd) |
| `sub_08017CCC` | `Effect_UpdateLinkEndBanner` | the linked game's banner (gLinkEndBannerTiles) once all but one Helirin reached the goal (Play_CheckLevelEnd) |
| `sub_08017D90` | `Effect_UpdateTimeUpBanner` | the linked game's banner (gTimeUpBannerTiles) shown for 0x78 frames when a Helirin's timer passes 0x2A2F (Play_CheckLevelEnd) |
| `sub_08017E3C` | `Effect_UpdateLinkResultBanner` | the linked game's banner that slides in with the players' numbers under it |
| `sub_0801805C` | `Effect_DrawDemoLabel` | a task in a demo (GAME_FLAG_DEMO, Play_RunLevel): two blinking sprites at the top right (gDemoLabelTiles) |
| `sub_080180E4` | `Effect_UpdateLinkCountdown` | the linked game's countdown before the start (Play_RunLevel state 2): songs 0x3F, 0x40, then the Helirins go |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_080C5ED4` | `gCrashAnim` | u16 animation frames of the crash (Effect_UpdateCrash; Helirin_Update starts it) |
| `gUnk_080C5EF0` | `gCrashTiles` | const void *[6]: 0x800 bytes of OBJ tiles per frame of gCrashAnim, copied to 0x06015980 (Effect_UpdateCrash) |
| `gUnk_080C5F08` | `gHitSparkAnim` | u16 animation frames of the spark at a colliding point (Effect_UpdateHitSpark) |
| `gUnk_080C5F1C` | `gZoneHitRippleAnim` | u16 animation frames of the hit effect of a Helirin with flags 0x40008 (Effect_UpdateZoneHitRipple; first named `gZoneHitSparkAnim`) |
| `gUnk_080C5F30` | `gSpringSparkAnim` | u16 animation frames of a spring's spark (Effect_UpdateSpringSpark; Object_SpringDown starts it) |
| `gUnk_080C5F44` | `gGoalAnim` | u16 animation frames of the goal's palette (Effect_UpdateGoal, state 1) |
| `gUnk_080C5F58` | `gGoalReachedAnim` | u16 animation frames of the goal's palette once the Helirin's flags have bit 3 (Effect_UpdateGoal) |
| `gUnk_080C5F6C` | `gGoalPalettes` | const u8 *[8]: the palettes Effect_UpdateGoal copies by frame (0x20 bytes to 0x05000160, 0x20 from +0x100 to 0x05000180) |
| `gUnk_080C5F8C` | `gPaletteCycleAnim` | u16 animation frames: which colour set Effect_CyclePalette copies to BG palette 0 |
| `gUnk_081DD188` | `gGoalBannerTiles` | 0x1000 bytes of OBJ tiles Effect_UpdateGoalBanner copies to 0x06011400 (two 64x64 sprites) |
| `gUnk_081DE188` | `gLinkEndBannerTiles` | 0x1000 bytes of OBJ tiles Effect_UpdateLinkEndBanner copies to 0x06011400 (two 64x64 sprites) |
| `gUnk_081E1F88` | `gTimeUpBannerTiles` | 0x1000 bytes of OBJ tiles Effect_UpdateTimeUpBanner copies to 0x06011400 (two 64x64 sprites) |
| `gUnk_081DF188` | `gCountdownTiles` | OBJ tiles the linked game's countdown loads with song 0x3F (Effect_UpdateLinkCountdown) |
| `gUnk_081E0188` | `gCountdownGoTiles` | OBJ tiles the countdown loads 30 frames before the start, with songs 0x40 and 0x2F (Effect_UpdateLinkCountdown) |
| `gUnk_081E4FC8` | `gDemoLabelTiles` | 0x180 bytes copied to OBJ VRAM 0x06011400: the sprites Effect_DrawDemoLabel draws during a demo |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk080176F4` | `TimePenalty` | the task Effect_UpdateTimePenalty runs |
| `Unk08017934` | `FireworkSpark` | one thrown sprite of Effect_Firework, the task Effect_UpdateFireworkSpark runs |
| `Unk080179C8` | `GoalEffect` | the task Effect_UpdateGoal runs |
| `Unk08017AC8` | `PaletteCycle` | the task Effect_CyclePalette runs |
| `Unk08017B48` | `CopyAnim` | the task Effect_AnimateCopies runs: an animation whose frames are queued copies |
| `Unk08017BA8` | `BlockCopyAnim` | the task Effect_AnimateBlockCopies runs: an animation whose frames are block copies |
| `Unk08017BA8Copy` | `BlockCopy` | one frame's copy of a BlockCopyAnim (Main_QueueBlockCopy) |
| `Unk080C6284` | `QueuedCopy` | one frame's copy of a CopyAnim {src, dst, size} (Main_QueueCopy) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `CopyAnim` | `unk8` | `copies` | the copy of each animation frame, indexed by anim.value (Effect_AnimateCopies) |
| `BlockCopyAnim` | `unk8` | `copies` | the copy of each animation frame, indexed by anim.value (Effect_AnimateBlockCopies) |

## intro

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08005F24` | `Intro_MainState` | the game's main state 1 (gMainStates[1]): the intro and the title screen (boot_attract input), its tasks and the "PRESS START" menu |
| `sub_08006068` | `Intro_MenuItem` | the intro's one-item menu: "PRESS START" blinking; A or Start on the title goes to main state 2, earlier it skips to the title |
| `sub_080060F4` | `Intro_StartBackgroundTasks` | starts Effect_RotateBg2 and three Effect_AnimateCopies tasks: the animated background of the menu screens (file select, mode menu, Make Up) |
| `sub_08006178` | `Intro_Update` | the intro's timeline task on gMain.frameCount: its backgrounds, scrolling layers, fades, the sprites and tasks it starts at set frames, the title (sub-state 4-5) |
| `sub_08006B08` | `Intro_UpdateBgAnim` | an intro task: an animation of BG map 1 pictures (gIntroBgAnim) from frame 0x3FC to 0x7E9 |
| `sub_08006BA4` | `Intro_UpdateTiltedHelirin` | an intro task from frame 0x7EA: a 64x32 sprite of 18 frames moved by a direction animation (gIntroTiltedHelirinPath); on screen a flattened spinning Helirin (first named `Intro_UpdateMovingSprite`) |
| `sub_08006CA8` | `Intro_UpdateHelirin` | an intro task from the title (frame 0x9DC): a rotating Helirin (OBJ tiles from gHelirinGfx) as a double-size affine sprite |
| `sub_08006E2C` | `Intro_UpdateMovingHelirin` | Intro_UpdateHelirin with its own tiles, moved by a direction animation (gIntroHelirinPath); at the title it takes the Helirin's tiles |
| `sub_0800709C` | `Intro_UpdateSprite` | one of the intro's 15 sprites: its own timeline of the frame counter, animated tiles, freed off screen |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_0801E4B4` | `gTextPressStart` | "PRESS START": the title's blinking text (Intro_MenuItem) |
| `gUnk_080C38C4` | `gIntroTiltedHelirinPath` | u16 animation frames of Intro_UpdateTiltedHelirin (the value: a direction, 0xFF none; first named `gIntroMovingSpritePath`) |
| `gUnk_080C38E8` | `gIntroHelirinPath` | u16 animation frames of Intro_UpdateMovingHelirin (the value: a direction, 0xFF none) |
| `gUnk_080C5FA4` | `gBackgroundAnim1` | u16 animation frames of the first menu background Effect_AnimateCopies task (Intro_StartBackgroundTasks) |
| `gUnk_080C5FC0` | `gBackgroundAnim2` | u16 animation frames of the second (Intro_StartBackgroundTasks) |
| `gUnk_080C5FDC` | `gBackgroundAnim3` | u16 animation frames of the third (Intro_StartBackgroundTasks) |
| `gUnk_080C5FF8` | `gBackgroundCopies` | the copies of Intro_StartBackgroundTasks's three Effect_AnimateCopies tasks (+8) |
| `gUnk_0817AF68` | `gIntroObjPalette` | 0x200 bytes of OBJ palette of the intro (Intro_Update) |
| `gUnk_081B1C40` | `gIntroBgTiles` | compressed (sub_08004084) 0x8000 bytes of BG tiles of the intro (Intro_Update) |
| `gUnk_081B6730` | `gIntroBgPalette` | 0x200 bytes of BG palette of the intro (Intro_Update) |
| `gUnk_081B6D34` | `gIntroLayer3Map` | 0x800 bytes: the intro's layer 3 map (Intro_Update, DMA) |
| `gUnk_081B7538` | `gIntroLayer2Map` | 0x800 bytes: the intro's layer 2 map (Intro_Update, DMA) |
| `gUnk_081B7D3C` | `gIntroLayer1Map` | 0x800 bytes: the intro's layer 1 map (Intro_Update, DMA) |
| `gUnk_081BCFE8` | `gTitleObjPalette` | 0x200 bytes of OBJ palette of the intro from frame 0x3FC and of the title (Intro_Update) |
| `gUnk_081C22E8` | `gIntroTiltedHelirinTiles` | 0x4800 bytes of OBJ tiles: the 18 64x32 frames of Intro_UpdateTiltedHelirin (first named `gIntroMovingSpriteTiles`) |
| `gUnk_081C6AE8` | `gIntroHelirinTiles` | OBJ tiles of Intro_UpdateMovingHelirin (eight rows of 0x40 bytes, then 0x20 at +0x200) |
| `gUnk_081E5668` | `gIntroHelirinCenterTile` | 0x20 bytes of OBJ tiles Intro_UpdateHelirin copies to 0x06016180: the 8x8 sprite at the Helirin's centre (tile 0x30C) |
| `gUnk_081D6388` | `gIntroObjTiles` | 0x2400 bytes of OBJ tiles the intro (Intro_MainState) copies to 0x06010000; also the ending's and Practice's |
| `gUnk_080C382C` | `gIntroBgAnim` | u16 animation frames of Intro_UpdateBgAnim (the value: a picture of gIntroBgAnimPictures) |
| `gUnk_080C3794` | `gIntroBgAnimPictures` | {map, tile list}[]: the BG map 1 pictures of gIntroBgAnim's frames (Bg_LoadMap, Intro_UpdateBgAnim) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08006178` | `IntroTask` | the intro's timeline task (Intro_Update) |
| `Unk08006B08` | `IntroBgAnim` | the task Intro_UpdateBgAnim runs |
| `Unk08006BA4` | `IntroTiltedHelirin` | the task Intro_UpdateTiltedHelirin runs (first named `IntroMovingSprite`) |
| `Unk08006CA8` | `IntroHelirin` | the task Intro_UpdateHelirin and Intro_UpdateMovingHelirin run: a rotating Helirin |
| `Unk0800709C` | `IntroSprite` | one of the intro's sprites, the task Intro_UpdateSprite runs |
| `Unk0800709CFrame` | `IntroSpriteFrame` | a frame of an IntroSprite's animation: its tiles, attributes and offset |

## ending

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080076FC` | `Ending_MainState` | the game's main state 4 (gMainStates[4]): the ending (save_ending input) after Collection_Run(1): its tasks, the music; sets flags bit 5 (the ending seen) and saves; A/Start returns to the intro |
| `sub_08007868` | `Ending_UpdateBackground` | the ending's background task: the intro's BG tiles and layers scrolled left, blending, the windows, the twelve Ending_UpdateSprite tasks |
| `sub_08007ADC` | `Ending_UpdateSprite` | one of the ending's 12 sprites: Intro_UpdateSprite going the other way (walking left) |
| `sub_08007EEC` | `Ending_UpdateText` | the ending's text: a page of gEndingText at a time, its letters started column by column (Ending_UpdateLetter) |
| `sub_080080AC` | `Ending_UpdateLetter` | a letter of the ending's text: runs in bouncing, stands, runs on |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_080C3348` | `gEndingSprite10Anim` | u16 animation frames sprite 10 of the ending switches to at frame 0x1680 (Ending_UpdateSprite) |
| `gUnk_080C3910` | `gEndingText` | struct EndingTextLine[]: the ending's text pages (Ending_UpdateText) |
| `gUnk_0817B168` | `gEndingObjPalette` | 0x200 bytes of OBJ palette of the ending (Ending_UpdateBackground) |
| `gUnk_081B6B30` | `gEndingBgPalette` | 0x200 bytes of BG palette of the ending (Ending_UpdateBackground) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08007EEC` | `EndingText` | the ending's text task (Ending_UpdateText): a page of lines |
| `Unk080080AC` | `EndingLetter` | a letter of the ending's text, the task Ending_UpdateLetter runs |
| `Unk080C3910` | `EndingTextLine` | an entry of gEndingText: a page's line count, or a line's position and text |

## sound

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080181FC` | `Sound_PlaySong` | m4aSongNumStart, after m4aSoundMode(gSongSoundModes[song] << 8) when gSoundInfo.maxChans differs |
| `sub_0801822C` | `Sound_PlaySongOrChange` | Sound_PlaySong with m4aSongNumStartOrChange |
| `sub_0801825C` | `Sound_PlaySongOrContinue` | Sound_PlaySong with m4aSongNumStartOrContinue |
| `sub_0801828C` | `Sound_UpdateDelayedSong` | a task: counts unkA frames down (not while paused), then Sound_PlaySong(unk8) and frees itself |
| `sub_080182C0` | `Sound_UpdateDelayedSongOnly` | Sound_UpdateDelayedSong with m4aSongNumStart (no sound mode) |
| `sub_080182F4` | `Sound_UpdateSongAfterPlayer` | a task: waits for a music player to stop, counts a delay down, then Sound_PlaySong and frees itself |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_030059C0` | `gMPlayInfo_BGM` | m4a struct MusicPlayerInfo of gMPlayTable[0] (9 tracks: the music; pret's m4a name); Play_CheckLevelEnd fades it out |
| `gUnk_03005A40` | `gMPlayInfo_SE2` | m4a struct MusicPlayerInfo of gMPlayTable[2] (2 tracks; pret's m4a name), stopped by m4aMPlayStop (Play_CheckLevelEnd, Pause_Update) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk080182F4` | `SongAfterPlayer` | the task Sound_UpdateSongAfterPlayer runs: a song started once a music player has stopped |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `SongAfterPlayer` | `unk8` | `player` | the music player waited for (its status' low halfword: tracks playing) |
| `SongAfterPlayer` | `unkC` | `song` | the song started (Sound_PlaySong) |
| `SongAfterPlayer` | `unkE` | `delay` | frames counted down once the player stopped |

## util

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080041F4` | `Random_SetIndices` | sets both indices into gRandomTable (gRandomIndex, gRandomIndex2) to a value (Level_Load) |
| `sub_08004208` | `Random_Next` | the next byte of gRandomTable at gRandomIndex (wrapping at 256): the game's random numbers (Effect_Firework, the linked game) |
| `sub_08004224` | `Random_Get` | entry n of gRandomTable |
| `sub_08004234` | `Random_Next2` | Random_Next at the second index gRandomIndex2 |
| `sub_08004250` | `Random_Reset` | copies gRandomTableInit over gRandomTable (DMA 3) |
| `sub_08004274` | `Random_Shuffle` | shuffles gRandomTable from a seed (Intro_MainState seeds it with the frame counter) |
| `sub_080042AC` | `Util_GetScreenPos` | a map position relative to the camera (gCameraX, gCameraY); 0 when within a margin of the screen, else -1 |
| `sub_080042F8` | `Util_IsOnScreen` | Util_GetScreenPos without storing the screen position |
| `sub_08004348` | `Util_StepTowards` | moves a value towards another by at most a step (at least 1) |
| `sub_08004374` | `Util_StepTowardsS8` | Util_StepTowards for signed bytes |
| `sub_080043C4` | `Link_FindCommand` | the linked players (bits of a mask) whose received entry of gUnk_03000D50 has a command's high byte |
| `sub_08004428` | `Link_FindCommandExact` | Link_FindCommand comparing both bytes of the command |
| `sub_08004478` | `Util_GetDirection` | the byte direction (gSineTable's index) of a vector, from gDirectionTable |
| `sub_080044CC` | `Anim_Start` | starts an animation entry {frames, value} on an Anim: 1, -1 at the end, 3 after a jump (a negative count) |
| `sub_08004504` | `Anim_Step` | advances an Anim by a frame: -1 at the end, 0 while the frame lasts, else Anim_Start's result for the next entry |
| `sub_08004534` | `Anim_StartLong` | Anim_Start for a LongAnim (8-byte entries with u32 values) |
| `sub_0800456C` | `Anim_StepLong` | Anim_Step for a LongAnim |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03004280` | `gRandomTable` | u8[256]: the random byte table (Random_Next, Random_Shuffle), reset from gRandomTableInit |
| `gUnk_03004380` | `gRandomIndex` | u8, the next entry of gRandomTable Random_Next returns |
| `gUnk_03004384` | `gRandomIndex2` | u8, the second index into gRandomTable (Random_Next2) |
| `gUnk_0801CE34` | `gRandomTableInit` | u8[256], ROM copy of gRandomTable (Random_Reset DMAs it over it; Random_Shuffle's swap partners) |
| `gUnk_0801D334` | `gDirectionTable` | s8[64][64], the direction of (x, y) at [y][x] (Util_GetDirection) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08004534Frame` | `LongAnimFrame` | an entry {s16 frames, u32 value} of a LongAnim (frames < 0: jump; 0: the end) |
| `Unk08004534` | `LongAnim` | an animation Anim_StartLong / Anim_StepLong run: Anim with 8-byte entries (u32 values: tile pointers) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `LongAnim` | `unk0` | `next` | the next entry (Anim_StartLong) |
| `LongAnim` | `unk4` | `timer` | frames left of the current entry (Anim_StepLong) |
| `LongAnim` | `unk8` | `value` | the current entry's value |

## decompress

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08004030` | `Decompress_RLE` | decompresses run-length data: 0-0x7F literals, 0x80 + n a byte repeated n + 3 times; size or 0 |
| `sub_08004084` | `Decompress_LZ` | decompresses KuruTools' LZ format (KuruRomExtractor/LzCompression.cs): literals and back references; size or 0 |

## script

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0800E560` | `Script_Step` | steps a script of pictures to its next stop: BG palettes and maps, OBJ tiles and their palette; -1 at its end |
| `sub_0800E664` | `Script_Run` | a screen showing a script of pictures (Play_MainState, Results_CheckRankUp), stepped by A/B: mode 0 before a level, 1 after one, 2 the first Adventure (with the "skip training" menu), 3 all ten items, 4 the items missing, 5 a rank-up |
| `sub_0800EDA8` | `Script_SkipTrainingMenuItem` | the two-item menu of Script_Run's mode 2: the second item marks levels 0-4 as skipped (marks 1, save_files input "skip training") |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_080C3C3C` | `gAdventureStartScript` | u32 script of Script_Run mode 2: the first Adventure of a file (Play_MainState, flags bit 0 clear) |
| `gUnk_080C3ED0` | `gMissingItemsScript` | u32 script of Script_Run mode 4: world 10 level 3 cleared without all ten items (once, flags bit 6) |
| `gUnk_080C3EF8` | `gAllItemsScript` | u32 script of Script_Run mode 3: a world's level 3 cleared with all ten items, before the ending |
| `gUnk_080C3FA0` | `gLevelStartScripts` | const u32 *[], Script_Run mode 0's script by gGameState.level (before a level) |
| `gUnk_080C3FB4` | `gLevelEndScripts` | const u32 *[], Script_Run mode 1's script by gGameState.world (after a level) |
| `gUnk_080C3FE4` | `gRankUpScripts` | const u32 *[], Script_Run mode 5's script by the save file's rank (Results_CheckRankUp) |
| `gUnk_0818BC9C` | `gScriptBgPalettes` | the compressed BG palettes Script_Step's scripts name (up to gScriptObjTiles) |
| `gUnk_0818D1C4` | `gScriptObjTiles` | the compressed OBJ tiles Script_Step's scripts name (up to gScriptDataEnd) |
| `gUnk_0819FE40` | `gScriptDataEnd` | the end of Script_Step's OBJ tiles (gScriptBgPalettes + 0x141A4 in the original) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk0800E560` | `ScriptPlayer` | what Script_Run passes Script_Step: the script, the two blocks it decompresses into, the sprites' palette |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `ScriptPlayer` | `unk8` | `bgBlock` | the 0x200-byte block a BG palette is decompressed into (Script_Step) |
| `ScriptPlayer` | `unkC` | `objBlock` | the 0x1000-byte block OBJ tiles are decompressed into (Script_Step) |
| `ScriptPlayer` | `unk10` | `palette` | the palette Script_Run draws the sprites with, set by the script after OBJ tiles (Script_Step) |

## vs

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_0801016C` | `Vs_WaitForConsoles` | a frame of waiting for the linked consoles after the MultiBoot transfer: "ID ERROR!" or "TIME OUT!" and -1, 1 while waiting, 0 once all answer |
| `sub_0801021C` | `Vs_CheckMenuLink` | Vs_CheckLevelLink's code for the linked game's menus (Vs_RunMenus, Vs_RunResults) |
| `sub_080102D8` | `Vs_CheckLevelLink` | the link check of a linked level (Play_RunLevel states 2-3): "ID ERROR!", "TIME OUT!" (restarts the link), else whether every console answered |
| `sub_08010394` | `Vs_RunMenus` | the linked game's menus before a level (mode 4, Play_MainState): the settings, the course (Vs_NextCourse picks a Challenge course row by 5) |
| `sub_08011614` | `Vs_MatchMenuItem` | the first menu of Vs_RunMenus (state 5): item 1 a match of random courses whose rounds its unkE picks (gUnk_080C4844: 3, 5, 7, no end, into gGameState.unk2E, which ends the match in Vs_RunResults), item 2 the course list; named in the deep review (2026-10-23) |
| `sub_08011700` | `Vs_CourseRowsMenuItem` | Vs_RunMenus states 6-7: two values 0-9, the first and last row of Challenge courses the random courses come from (gGameState.unk32); deep review |
| `sub_08011890` | `Vs_CourseListMenuItem` | Vs_RunMenus states 9-10: rows 1-11 of five Challenge courses, the course (row - 1) * 5 + column becomes the level, as in Challenge_MenuItem; deep review |
| `sub_08011BD8` | `Vs_PlayerSettingsItem` | a linked game menu function: each player's Helirin size and lives, a row per player with its Helirin |
| `sub_08011FF8` | `Vs_RunResults` | the linked game's results after a level (Play_MainState): 1 plays another round (Vs_RunMenus) |
| `sub_08012938` | `Vs_ResultsMenuItem` | Vs_RunResults's first results menu function: ranks, times and numbers per player |
| `sub_08012BE0` | `Vs_ResultsMenuItem2` | the twin of Vs_ResultsMenuItem (items 0x101/0x102, Vs_DrawNumberRight) |
| `sub_08012F58` | `Vs_DrawNumberRight` | Vs_DrawNumber right-aligned in five cells, then a 16x8 sprite (tile 0x1D8) |
| `sub_08013040` | `Vs_DrawNumber` | draws a number in decimal with the digit tiles 0x40 + 2 * d (Sprite_Alloc(3)), no leading zeros |
| `sub_080130F8` | `Vs_DrawSpinner` | a spinning sprite (tile 0x6AC) at x = a * 240 / b: a progress marker |
| `sub_08013168` | `Vs_ShuffleCourses` | fills gVsCourseOrder with 0..54 (the 55 Challenge courses) and shuffles it (Vs_NextCourse) |
| `sub_0801319C` | `Vs_NextCourse` | the next course of gVsCourseOrder, swapping two entries (Random_Next) as it goes; Vs_RunMenus divides it by 5 (a row of Challenge courses) |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_030044E0` | `gVsCourseOrder` | u8[55], a permutation of 0..54, the Challenge courses (Vs_ShuffleCourses), swapped by Vs_NextCourse |
| `gUnk_03004518` | `gVsCourseIndex` | s32, index into gVsCourseOrder, 0..54 (Vs_NextCourse) |
| `gUnk_0801EC04` | `gTextIdError` | "ID ERROR!" (Vs_WaitForConsoles, Vs_CheckLevelLink) |
| `gUnk_0801EC10` | `gTextTimeOut` | "TIME OUT!" (Vs_WaitForConsoles, Vs_CheckLevelLink) |
| `gUnk_0801EC24` | `gTextLeftArrow` | "<", drawn left of the cursor (Vs_CourseRowsMenuItem) |
| `gUnk_0801EC28` | `gTextRightArrow` | ">", drawn right of the cursor (Vs_CourseRowsMenuItem) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk08011BD8` | `VsSettingsMenu` | the menu of Vs_PlayerSettingsItem: a Menu up to unkC, then each player's settings |

## link

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_08003C18` | `Link_Init` | sets up the link cable in multi-player mode (SIOCNT 0x4003, 115200 bps) and gLinkState, copies crt0's ARM serial routines to IWRAM |
| `sub_08003D14` | `Link_Exchange` | a frame of the link exchange (Main's linked frame callback): starts the transfers (the parent), Link_Receive into the received data, Link_Send; returns the status (gLinkStatus) |
| `sub_08003E00` | `Link_Send` | hands this frame's data to the exchange: a packet (frame counter, player bits, checksum) in gLinkState's send buffer |
| `sub_08003EC4` | `Link_Receive` | takes what each player sent from gLinkState's receive buffers (Link_Exchange) |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03003FF0` | `gLinkState` | struct LinkState, 0x220 bytes: the link cable state of the serial routines sub_0800019C/sub_080001DC (Link_Init) |
| `gUnk_03000890` | `gLinkSendData` | u16[8] sent to the other players each frame (Link_Exchange): command, -, x, y, angle \| progress, flags (Play_RunLevel) |
| `gUnk_03000D50` | `gLinkRecvData` | struct LinkData[4]: what each player sent (Link_Exchange), each enabled by a bit 0-3 of gLinkStatus (Link_FindCommand) |
| `gUnk_030008EC` | `gLinkStatus` | u32, Link_Exchange's result: bits 0-3 the players whose data arrived, 4-6 and 0x80 (the parent) the link's state, bits 8-11 Link_Init's argument |
| `gUnk_030008A8` | `gLinkPlayer` | u8, this console's player number in a linked game, from SIOCNT (ModeMenu_Run; Play_RunLevel, Vs_WaitForConsoles) |
| `gUnk_030008AC` | `gLinkWaitFrames` | u16, frames without a link partner (ModeMenu_Run: over 60 switches between waiting and MultiBoot; Vs_WaitForConsoles) |
| `gUnk_030008E8` | `gLinkTimer` | s32 frame counter: frames the linked consoles have not all answered (Vs_CheckLevelLink: "TIME OUT!" past 0x3C); Play_RunLevel's states 5 and 6 |
| `gUnk_03000DB0` | `gLinkPlayers` | u8, the mask of the players in the linked game (Play_RunLevel, Vs_WaitForConsoles, Effect_UpdateLinkResultBanner) |

Types:

| Old | New | Evidence |
| --- | --- | --- |
| `Unk03003FF0` | `LinkState` | gLinkState: the link cable state of the serial interrupt routines (crt0's ARM sub_0800019C / sub_080001DC), set up by Link_Init |
| `Unk03000D50` | `LinkData` | an entry of gLinkRecvData: what a player sent this frame (the layout of gLinkSendData) |

Fields:

| Struct | Old | New | Evidence |
| --- | --- | --- | --- |
| `LinkData` | `unk0` | `command` | the command Link_FindCommand / Link_FindCommandExact compare (gLinkSendData[0]) |

## debug

Functions:

| Old | New | Evidence |
| --- | --- | --- |
| `sub_080194D4` | `Debug_StageSelect` | AgbMain's state function 5 (gMainStates[5]), which no code selects: a stage select ("GAME START", "MAP TEST", "RECORDING", "PLAYBACK"; "WORLD Z".."TEST", "STAGE", "AREA") |
| `sub_0801970C` | `Debug_StageSelectItem` | Debug_StageSelect's menu function: the mode, the world, the stage and the area |
| `sub_08019904` | `Debug_MultiBootTest` | AgbMain's state function 6 (gMainStates[6]), which no code selects: a MultiBoot send test (MultiBootInit/Main/StartMaster) |
| `sub_08019A98` | `Debug_MultiBootTestItem` | Debug_MultiBootTest's menu function: each client's state ("NOT CONNECTED", "CHECKING...", "READY") and the transfer's ("PUSH START", "NOW SENDING...", "BOOT CHECKING...") |

Globals:

| Old | New | Evidence |
| --- | --- | --- |
| `gUnk_03004470` | `gDebugMultiBootResult` | s32, the last result of MultiBootMain in main state 6 (Debug_MultiBootTest) |
| `gUnk_030044D0` | `gDebugProbeCounts` | u8[8], the last 8 different MultiBoot probe_count values, newest first (Debug_MultiBootTest) |
| `gUnk_0801F240` | `gTextStage` | "STAGE " (Debug_StageSelectItem, item 0x103) |
| `gUnk_0801F248` | `gTextArea` | "AREA " (Debug_StageSelectItem, item 0x104) |
| `gUnk_0801F250` | `gTextNotConnected` | "NOT CONNECTED": a client whose response_bit is clear (Debug_MultiBootTestItem) |
| `gUnk_0801F260` | `gTextChecking` | "CHECKING...": a client that answered, its client_bit clear (Debug_MultiBootTestItem) |
| `gUnk_0801F270` | `gTextReady` | "READY": a client ready (Debug_MultiBootTestItem) |
| `gUnk_0801F280` | `gTextPushStart` | "PUSH START": probe_count 0 (Debug_MultiBootTestItem) |
| `gUnk_0801F294` | `gTextNowSending` | "NOW SENDING...": probe_count 0xD1 (Debug_MultiBootTestItem) |
| `gUnk_0801F2A8` | `gTextBootChecking` | "BOOT CHECKING...": probe_count above 0xDF (Debug_MultiBootTestItem) |
| `gUnk_080C63B4` | `gDebugModeNames` | const u8 *[4]: "GAME START", "MAP TEST", "RECORDING", "PLAYBACK" (Debug_StageSelectItem) |
| `gUnk_080C63C4` | `gDebugWorldNames` | const u8 *[14]: "WORLD Z", "WORLD A".."WORLD K", "MINI", "TEST" (Debug_StageSelectItem) |
| `gUnk_080C63FC` | `gDebugMultiBootImage` | the image Debug_MultiBootTest sends, up to gMultiBootProgram |

## Checked with screenshots (2026-10-21)

Seven names of the naming session were uncertain; each was checked on the **original ROM** with
kurutest `--shot`/`--trace` (or, where the code cannot run on one console, from its callers):

| Name | What the screenshots show | Result |
| --- | --- | --- |
| `Intro_UpdateMovingSprite` | boot_attract frames 2160-2300: a spinning Helirin drawn flattened (18 pre-drawn rotation frames) flying along the canyon of the title map | renamed `Intro_UpdateTiltedHelirin` (its struct, path and tiles too) |
| `Effect_UpdateZoneHitSpark` | training1_moves frames 1755-1769: blue rings at the hit point inside a healing zone (a normal hit shows a yellow star and "+3 SEC") | renamed `Effect_UpdateZoneHitRipple` (and `gZoneHitRippleAnim`) |
| `gLevelIntro*` | tasvideos_6116M_100 frames 592-630: "Grasslands" (the world tiles), "1" (the level tiles), a small animated circle (`gLevelIntroAnim`), then a 5x5-sprite picture of a Helirin craft (`gLevelIntroPictureTiles`) | `gLevelIntroWorldTiles` / `gLevelIntroLevelTiles` renamed `gLevelIntroWorldNameTiles` / `gLevelIntroLevelNumberTiles`; the rest kept |
| `Effect_DrawDemoLabel` | save_practice (See Demo) frames 2000-2032: the word "Demo" at the top right, green / white | kept |
| `gColorBonusTilesA/B` | the Grasslands 1 colour bonus (tasvideos_6116M_100 frame 895) and both sets drawn offline with each colour's palette: the same paint-tube icon, A and B using different palette slots; odd/even names would be wrong, the two users pick them with opposite parity | kept |
| `Vs_DrawSpinner` | link only (no screenshot): Vs_RunMenus calls it with the bytes of a link transfer sent and their total, under a blinking "WAIT" | kept (comment completed) |
| `Effect_UpdateLinkResultBanner` | link only (no screenshot): started by Vs_RunResults | kept |

## Kept placeholders

After the last module (debug), 282 of the game's 285 functions had names; the deep review
(2026-10-23) named the last three (`Vs_MatchMenuItem`, `Vs_CourseRowsMenuItem`,
`Vs_CourseListMenuItem`, from what Vs_RunMenus does with their values, reviewed in the final C
pass), so all 285 have names. Kept for lack of evidence:

- Globals: 113 `gUnk_` symbols the game's code still uses, mostly graphics, palettes and maps
  known only by where they are copied (bg 37, intro 45: the frame tables of the intro's and
  the ending's 15 sprites, the backgrounds of set frames), the effect tables a level loads
  (level 12), the script screens' pictures (script 6) and a few link and result screen
  tables.
- crt0's ARM serial routines `sub_0800019C` / `sub_080001DC` and their IWRAM copies (asm, not
  a game module).
- Fields: the `unkXX` fields of the shared structs whose use differs per caller (`Menu`'s
  `unkC`-`unk14`, `Task`'s), and those no code explains.
