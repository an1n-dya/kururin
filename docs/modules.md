# The game's modules

The game code (0x080002D8-0x08019BA0, 285 functions) was one provisional translation unit,
`src/game.c`. For readability it is split into one C module per subsystem. The rules:

- **Contiguous ranges in ROM order.** A module is `[start, next module's start)`; the functions
  stay in address order, so the linked ROM does not change. The boundaries are a readability
  choice: the original translation units are still unknown (nothing in the ROM marks them), and
  these ranges make no claim about them.
- **One compiler setting.** Every module keeps the game's `cc`/`cflags` (agbcc
  -fprologue-bugfix) and shares its evidence: `evidence = "game"` in `kururin.toml`, checked as
  one group against `tests/compiler_evidence/game/` (`scripts/check_compilers.py`).
- **No code changes.** The functions are moved as they are. The build must stay byte-identical to
  the build before the split (SHA1 `a31831a186ee435d82b3aced0a0a7c2bc47b0986`, which is not
  `baserom.gba`'s: the game is a functional decompilation), and `make test` must pass, after
  every module.
- **Headers.** `include/game.h` holds what more than one module uses: the shared structs, the
  RAM/ROM data externs and the library prototypes (it includes `global.h`, `main.h`, `m4a.h`
  and `gba/multiboot.h`, as `game.c` did). `include/<module>.h` declares the module's functions
  that other modules call (main's are in `include/main.h`, next to `struct Main`). What only
  one module uses (its structs, data, macros, its internal prototypes) stays in its `.c`.

## Evidence used for the boundaries

- What the functions do: the doc comment above each function, written from its code
  (what it reads, writes and calls), and the screens/states they belong to (`gMain.state`'s
  state functions, `Play_MainState`'s sub-states).
- The call graph: who calls each function (a screen's frame loop and the item callbacks and
  tasks it starts sit next to it in the ROM).
- `tools/next.py`'s subsystem tags (from literal pools: gHelirins, gCamera, gGameState, m4a and
  SRAM calls, KEYINPUT, VRAM/I/O): an ordering heuristic, used only as a hint. Its `level` tag
  in particular marks any function that reads `gGameState`.
- KururinTAS's `kkk.txt` (0x08013D0C "Collision Test", 0x080150E4's collision branches,
  0x08004104 "CREATE_OBJECT") and KuruTools' level format (`Level_Load`, `Decompress_LZ`).

## Modules

| Module | Range | Functions | What it does | Evidence |
| --- | --- | ---: | --- | --- |
| `main` | 080002D8-08000AEC | 20 | AgbMain, the frame end (`Main_EndFrame`), the VBlank handler, keys (`Main_ReadKeys`), the frame callbacks, the transfer queue (`Main_QueueCopy` and its kin) and the heap allocator (`Heap_Init`-`Heap_Alloc`) | crt0 jumps to AgbMain; gMain, the interrupt table, KEYINPUT, DMA 3; every module calls the queue and `Main_EndFrame` |
| `bg` | 08000AEC-08002438 | 12 | video modes (`Bg_SetVideoMode`), background maps and their layers, the window effect (`Bg_StartWindowCircle`, `Bg_DrawWindowCircle`), the backgrounds of each screen (`Bg_LoadScreen`) | DISPCNT/BGxCNT copies, gBgLayers layers, BG VRAM; callers are the screens |
| `sprite` | 08002438-08002848 | 11 | the 16 sprite lists and the OAM buffer (`Sprite_Alloc`, `Sprite_BuildOam`), the affine parameter sets (`Sprite_AllocAffine`-`Sprite_SetAffineRotScale`), an OBJ tile test grid | gOamBuffer, gSprites/gSpriteLists/gAffineParams |
| `text` | 08002848-08003C18 | 27 | times (`Text_FormatTime`), numbers and strings drawn as sprites (8x8 and 8x16, proportional), the character tables and text conversion (`#h`/`#k`) | all built on `Sprite_Alloc`; the font tables gDakutenChars.. |
| `link` | 08003C18-08004030 | 4 | the link cable: multi-player setup (`Link_Init`) and the per-frame exchange (`Link_Exchange`, `Link_Send`) | SIOCNT/RCNT, timer 3 and serial interrupts, gLinkState |
| `decompress` | 08004030-080040E0 | 2 | the run-length and LZ decompressors | KuruTools `LzCompression.cs` (`Decompress_LZ`) |
| `task` | 080040E0-080041F4 | 6 | the task heap: create (`Task_Create`), free, run every task, find by key | KururinTAS "CREATE_OBJECT" (0x08004104); gTaskHeap |
| `util` | 080041F4-0800459C | 17 | the random byte table (`Random_Next`, `Random_Shuffle`), screen positions relative to the camera, steps towards a value, the link player bits, the direction of a vector (`Util_GetDirection`), animation steppers (`Anim_Start`-`Anim_StepLong`) | small leaf helpers used across the game |
| `menu` | 0800459C-08004C24 | 19 | the menu driver (`Menu_RunWrap`, `Menu_Run`: item callbacks 0x100.., 0x1FF) and the key handlers that move an item or a value (each plays song 0x20) | every menu screen calls them |
| `level` | 08004C24-08005178 | 4 | a new game (`Game_Boot`), the running level number, loading a level (`Level_Load`) and starting its tasks (`Level_StartTasks`) | gGameState, gWorldEntries (KuruTools' level format) |
| `save` | 08005178-080057B8 | 5 | the save at boot (`Save_LoadAtBoot`), saving (`Save_Write`), resetting files and records | ReadSram/WriteSram/VerifySram; docs/save_format.md |
| `level_map` | 080057B8-08005F24 | 4 | reading a level map: numbers in the map, the start positions in gCamera, the objects it starts (`LevelMap_Read`, `LevelMap_StartMarkedObject`) | map codes 0xED-0xFF, gCamera; it starts the `objects` tasks |
| `intro` | 08005F24-080076FC | 9 | main state 1, the intro (`Intro_MainState`), its menu and tasks | gMainStates[1]; the tasks it starts |
| `ending` | 080076FC-08008200 | 5 | main state 4, the ending (`Ending_MainState`), its background, sprites and text | gMainStates[4]; save_ending input |
| `play` | 08008200-08009438 | 4 | main state 3 (`Play_MainState`: menus to levels), a level's loop (`Play_RunLevel`), its end (`Play_CheckLevelEnd`), a steering helper | gMainStates[3]; calls every screen below |
| `pause` | 08009438-0800AA0C | 7 | the pause menu (`Pause_Update`) and its item callbacks per mode | called once per frame of a level by `Play_RunLevel` |
| `results` | 0800AA0C-0800B8A0 | 4 | the result screen over the level (`Results_Run`), rank-ups (`Results_CheckRankUp`), the camera origin (`Camera_Update`), entering a level's result and records (`Results_EnterRecord`) | `Play_MainState`'s level end; gSaveBank records |
| `files` | 0800B8A0-0800C75C | 7 | main state 2: the file select (`Files_MainState`), name entry (`Files_EnterName`), the delete-data screens | gMainStates[2]; save files gSaveFiles |
| `mode_menu` | 0800C75C-0800E560 | 13 | the mode menu (`ModeMenu_Run`, with the MultiBoot send) and its sprites, then the Adventure map screen it leads to (`MapScreen_Run`), its level menu and map tasks | MultiBoot calls; `Play_MainState` sub-states 0-2 (see "Not clean") |
| `script` | 0800E560-0800EF44 | 3 | the screens that step through a script of pictures (`Script_Step`, `Script_Run`) and their two-item menu | struct ScriptPlayer |
| `collection` | 0800EF44-0800F624 | 2 | the bonus collection screen (`Collection_Run`) and its picture tasks | struct CollectionPicture ("one picture of the bonus / collection screen") |
| `level_intro` | 0800F624-0800FAAC | 2 | the screen before a level (`LevelIntro_Run`: tiles by world and level) and its animation | `Play_MainState` sub-state 3, before the level |
| `challenge` | 0800FAAC-0801016C | 2 | the Challenge course menu (`Challenge_Run`, `Challenge_MenuItem`) | world 12, levels 0x26..; save_challenge input |
| `vs` | 0801016C-080131F8 | 16 | the linked game: waiting for the other consoles, its menus and screens, a shuffled permutation of 0..54 | SIOCNT player number, gLinkStatus; mode 4 (`Vs_RunMenus`) |
| `makeup` | 080131F8-08013D0C | 7 | the lives per game mode (`Lives_Reset`, `Lives_Init`) and the Make Up screen (`MakeUp_Run`, `MakeUp_MenuItem`) | gGameState.saveFile->unlocks/unk18-1A; save_makeup input |
| `collision` | 08013D0C-080142DC | 6 | map collision (`Collision_TestWallPixel`, `Collision_GetTilePixel`), the Helirin's points and their tests against shapes | KururinTAS "Collision Test" (0x08013D0C); gPhysicalTiles |
| `helirin` | 080142DC-08015890 | 10 | the Helirin: setup, its frame (`Helirin_Update`, KururinTAS's physics description), the collision response (`Helirin_UpdateCollision`), drawing, pushes, the pieces it breaks into | gHelirins; KururinTAS kkk.txt (0x080150E4) |
| `objects` | 08015890-08016BF4 | 11 | the world objects a level map starts: springs, Rollers (and their projectiles and catchers), Pistons, Shooters and their shots | started by `LevelMap_StartMarkedObject` (`level_map`) |
| `hud` | 08016BF4-08017270 | 5 | the lives and status sprites and the times drawn during a level | gHudFaceAnim, the Helirin's lives |
| `effects` | 08017270-080181FC | 22 | effect tasks: the window effect, fades, BG rotation, sparks, bursts, palette cycles, queued copies, banners, the linked countdown | small `Task_Create` tasks, each freeing itself |
| `sound` | 080181FC-08018330 | 6 | song starts that set the sound mode first and the tasks that start songs | m4aSongNumStart*, m4aSoundMode, gSoundInfo |
| `bonus` | 08018330-08018D34 | 7 | the bonus items of a level (taken or switched on) and their tasks | gGameState.saveFile->unlocks/unk1A bits 0-9 |
| `practice` | 08018D34-080194D4 | 2 | Practice (mode 5, `Practice_Run`) and its area menu | the mode menu's result 5 (`Play_MainState`); save_practice input |
| `debug` | 080194D4-08019BA0 | 4 | code no state selects: a stage select (main state 5) and a MultiBoot send test (main state 6) | gMainStates[5], [6]; never executed |

## Names

The functions are named `Prefix_Verb` (docs/names.md). The prefix is the module's, except where
a module holds code of another subsystem, which keeps that subsystem's prefix: `MapScreen_`
(the map screen in `mode_menu`), `Camera_Update` (`results`), `Lives_` (`makeup`), `Random_`,
`Link_FindCommand*` and `Anim_` (`util`), `Debug_DrawHexWords` (`collision`), `Heap_` (`main`).

## Not clean

- **Code generation depends on the module boundaries.** agbcc's gcse orders expressions by a
  hash of their constant-pool label names (`*.LC<n>`, `n` counting up through the translation
  unit), and the order can change which registers a function gets. So a function can compile
  differently when the number of constants before it in its file changes (declarations do not
  matter). With a separate map screen module starting at 0x0800D4F4, `MapScreen_Run` swapped two
  registers. Its range was therefore merged into `mode_menu`, where the constants before it give
  it label numbers that compile it as before (the mode menu and the map screen are also
  consecutive steps of `Play_MainState`). A future split, merge or reordering of functions inside a module
  can change other functions the same way: such a change is behaviour-preserving (`make test`
  decides) but not byte-preserving.
- **`Camera_Update`** (the camera origin, called by the level loop) sits between the rank-up
  code and `Results_EnterRecord` in `results`: a contiguous range cannot move it to the level code.
- **`makeup`** starts with `Lives_Reset` and `Lives_Init` (the lives per mode, used by new
  games and every level), which come right before the Make Up screen. It ends with
  `Lives_SettingsScreen` and `Lives_SettingsMenuItem`, a screen and its menu function no code reaches
  (`Lives_SettingsMenuItem` only by a pointer from `Lives_SettingsScreen`, which nothing references).
- **`collision`** ends with `Debug_DrawHexWords`, which nothing references and which draws with
  `Text_DrawSmall` (debug output, by its look).
- **`main`** keeps the heap allocator (`Heap_Init`-`Heap_Alloc`), which the task heap
  (`task`) and the screens use, because it sits between the transfer queue and the video modes.
- **`util`** gathers the small helpers between the task heap and the menu driver, which have no
  single subsystem.
