# Unused content: main states 5 and 6

AgbMain runs `gMainStates[gMain.state]` forever (`gMain.state` is the byte at
**0x03000DCA**). The game itself only ever stores 0-4 there (`docs/remaining.md`, OTHER), so
the state functions 5 (`Debug_StageSelect`) and 6 (`Debug_MultiBootTest`) are never entered. Both were
opened on the **original ROM** (`baserom.gba`) by writing the state byte from outside, and
explored over a few thousand frames with key presses (2026-10-20, see "How this was explored"
below). Summary:

| State | Function | What it is |
| ---: | --- | --- |
| 5 | `Debug_StageSelect` (menu: `Debug_StageSelectItem`) | **Debug stage select**: play any level in a debug mode, a free-camera "MAP TEST", and a key **recorder** / demo **player** for the recorded demos (Practice's See Demo, Training's instructor) |
| 6 | `Debug_MultiBootTest` (menu: `Debug_MultiBootTestItem`) | **MultiBoot send test**: sends the linked game's guest program to up to three consoles over the BIOS MultiBoot protocol in one piece |

Both are developer tools left in the release ROM: plain upper-case debug text (the menu font,
not the game's menu graphics), no background of their own (whatever the previous screen left
in VRAM stays on screen), and B leaves them to state 1 (the title).

## How to reach them (mGBA)

Write **5** or **6** to the byte **0x03000DCA** once, while the title screen ("PRESS START") is
showing or later. The write takes effect when the current state function next checks
`gMain.state`:

- **At the title screen**: within ~25 frames (the title's own loop checks the state once its
  current step ends).
- **In a level**: only when the level ends (death, Quit, a cleared level): the level loop
  (`Play_RunLevel`) does not look at `gMain.state`; the mode loop of state 3 does, after the
  level returns. Dying in Training 1 with the byte set opened state 5 at once.
- **Before the title** (power-on, intro): useless, state 0 ends with `gMain.state = 1` and
  overwrites it.

Ways to do the write in mGBA:

- **Memory viewer** (Tools > Game state views > View memory): go to `0x03000DCA`, type `05`
  (or `06`).
- **Debugger console** (Tools > Open debugger console, or `mgba -d`): `w/1 0x03000DCA 5`.
- **Cheat** (Tools > Cheats, a VBA-style line): `03000DCA:05`. Turn it on at the title screen,
  and **off again** once the menu is up: a cheat rewrites the byte every frame, so B (which
  stores 1) could not leave, and the game's own state changes would be undone.

No setup is needed beyond a normal boot to the title: the font tiles state 5 copies to OBJ VRAM
come from the ROM, the save buffer (`gSaveFiles`) is already set up by boot, and level
loading (`Play_RunLevel`) does the rest. State 6 relies on the font the title screen left in
OBJ VRAM (see below).

## State 5: debug stage select (`Debug_StageSelect`)

### The menu

Screenshot (opened from the title, frame ~700): the title screen's BG tiles stay on screen as a
garbled lilac pattern with yellow flowers; at the top left a white outline of the selected
level's **minimap** (Training 1's long horizontal corridor at first); in the middle three or
four rows of white 8x8 text, the row under the cursor in green:

```
GAME START
WORLD Z
STAGE  1
```

The rows are drawn 8 pixels apart with a font slightly taller than that, so they overlap a
little. Opened from a level instead, the background is that level's (e.g. Grasslands, or the
starry Training map BG after dying in Training 1).

Keys: Up/Down move between the rows (the cursor starts on STAGE), Left/Right change the value
(with key repeat), A starts, B goes back to the title (state 1, sound 0x22).

| Row | Values | Stored in (`gGameState`) |
| --- | --- | --- |
| mode (`gDebugModeNames`) | `GAME START`, `MAP TEST`, `RECORDING`, `PLAYBACK` | `flags`: 0, bit 0, bit 2, bit 3 (`GAME_FLAG_DEMO`) |
| world (`gDebugWorldNames`) | `WORLD Z` (Training), `WORLD A`-`WORLD K` (the 11 worlds), `MINI` (Challenge), `TEST` | `world` 0-13 |
| `STAGE n` | 1-5 in Z, 1-3 in A-K, 1-55 in MINI, 1-200 in TEST | `level` |
| `AREA n` (only for RECORDING / PLAYBACK, worlds Z-K) | 0-3 | `unk2` |

Each change of world/stage decompresses the level's minimap data and redraws the outline
(`Decompress_LZ` + `Bg_DrawLevelMinimap`; no minimap: the outline area is cleared). A sets
`gGameState.mode = 6` (a mode no menu of the game uses), `unk4 = 0` (file 1), and runs the
level with `Play_RunLevel`; when it returns, the menu comes back with the same settings.

### GAME START: debug play

Screenshot (World A, stage 1): the normal Grasslands level, HUD with RECORD 00'00"00 and the
timer, three hearts and the Kururin portrait.

- No instructor Helirin in Training (world Z), no intro text, no pause menu.
- Start shows a blinking `PAUSE` in the middle of the screen (the game freezes), Start again
  resumes. **Select + Start** leaves the level back to the stage select.
- **The Helirin cannot die**: the hearts empty after three bumps, then it keeps flying and
  bumping (bumpCount went past 30, +3 s on the timer each time, as in the normal game).
- Reaching the goal returns to the stage select.
- The RECORD shown is file 1's (`gGameState.saveFile` = the save buffer): 00'00"00 after a boot
  with a blank save, the real record once a file has been played.

### MAP TEST: free camera

`flags` bit 0: the Helirin stays frozen at the start (no input reaches it, `unkC0` bit 1),
the timer stays at 00'00"00, and the D-pad scrolls the camera instead (`Camera_Update`): 2
pixels a frame, 4 with A, doubled with B, clamped to the level's camera bounds. Screenshots:
the level's grass and decorations far from the Helirin (e.g. the Grasslands start area's
bushes and a red-yellow tile pad at the right edge). Select + Start leaves. The same branch of
`Camera_Update` also runs when the Helirin's `unkBC` bit 30 is set.

### RECORDING: input recorder

`flags` bit 2 (Helirin `unkBC` bit 17): every frame the Helirin's keys are written to the
EWRAM buffer **0x02039DAC** (one byte per frame: D-pad in the high nibble, bit 0 A or B, bits
0-1 both, the format of the game's built-in demos), with an 0xFF end marker after the last
byte. Pausing (Start) shows the buffer's start and current end in hex instead of `PAUSE`;
screenshot after ~150 frames of play: `02039DAC 02039F19` in white over the level. When the
buffer is full (0x1FFF bytes, ~2 min 16 s) `REC AREA` / `EMPTY` is drawn and recording stops.
Nothing saves the recording: the developer would read that memory range out with a debugger and
put it in the ROM's demo data.

### PLAYBACK: demo player

`flags` bit 3 (`GAME_FLAG_DEMO`): the Helirin plays the ROM's recorded keys for the level and
`AREA` (`gDemoKeys + gDemoKeyOffsets[level][area].offset`, the data Training's instructor Helirin and
Practice's "See Demo" use) with the `Demo` label at the top right. World A stage 1 area 0 flew
to the goal without a bump in 1267 frames (21.1 s) and returned to the menu. When a demo runs
out before the goal, `PLAYBACK` / `DATA EMPTY` is drawn and the Helirin gets no more keys. The demo does
not replay what RECORDING wrote (it reads the ROM, not 0x02039DAC).

### MINI and TEST

- `MINI` is the Challenge world (world 12): stage 1 is the first Challenge course.
- `TEST` (world 13) reads `gWorldEntries[13]`, which is all zero in this ROM (the table has 13
  real entries; the 14th is empty). The level loads as nothing: a black screen with the HUD,
  the Helirin spinning in the void and one stray tile. Its 200 stage numbers suggest a test
  world that was removed before release.

### The rebuilt ROM and the PC port in the stage select

Checked on 2026-10-29 with a scratch copy of kurutest that writes 5 to `0x03000DCA` at frame
700 in both ROMs: one scenario of 5 980 frames through all four modes (GAME START in a level
with bumps, pause and Select + Start; MAP TEST scrolling at every speed; RECORDING; PLAYBACK
of a whole demo), and one with TEST.

- **The rebuilt ROM** (`kururin.gba`), in the four modes: the RAM and the sound are the
  original's on every frame. The screen differs on single frames where the minimap is redrawn
  after a change of world or stage (frame 4107 of that scenario: 101 pixels of the outline,
  lines 61-71): `Bg_DrawLevelMinimap` draws the outline straight into OBJ VRAM while the screen
  is being drawn, and its C (85.2% of the original's bytes, not byte-identical) takes other
  cycles than the original's asm, so the screen catches the drawing at a different point; the
  next frame is the same in both. With TEST the same function also moves a frame's end: its
  minimap is drawn from garbage and takes more than a frame (frame 781 is a lag frame of the
  original), so the menu sound the original writes in that frame comes a frame later in the
  rebuilt ROM. **Bisected:** with `Bg_DrawLevelMinimap` back to `INCLUDE_ASM` (everything else
  as on `main`), the four-mode scenario is identical on all 5 980 frames, screen included, and
  the TEST scenario's RAM and sound on all 2 221. Not fixed: it is reached only through this
  hidden state, and fixing it means matching the function's cycles (the same kind of timing
  difference as `docs/audit.md`, "Timing"). TEST's level itself still looks different (stray
  tiles in the void, from its first frame): its maps are read from unmapped addresses, where
  the GBA returns the instructions it is executing, and the rebuilt ROM's instructions are other
  ones; only byte-identical code on that path could show the same tiles.
- **The PC port** (`--poke 700:0x03000DCA:5` opens the stage select the same way): the RAM is
  the original's on every compared frame of the four-mode scenario (5 911, port/test's
  comparison). **TEST is different**: the original reads its world info and level table from
  address 0 (the BIOS, whose reads give the BIOS's last fetched opcode) and plays a level from
  whatever EWRAM still holds; a PC cannot map address 0, and the port stopped there with a
  segmentation fault. The port now gives TEST an empty world (all offsets and sizes 0,
  `port/src/rom.c`): `Level_Load` finds no level data, so A on a TEST stage goes straight back
  to the stage select instead of the void level. Following the original further is not
  possible: with the BIOS's words as sizes it decompresses nothing but moves its EWRAM cursor
  by them and reads maps from far outside the GBA's memory, which only its open bus answers.

## State 6: MultiBoot send test (`Debug_MultiBootTest`)

Screenshot (opened from the title, frame ~900): the title screen still there, its logo now a
flat white silhouette (only the Helirin stick in it keeps its colours); three white rows over the lower half:

```
NOT CONNECTED
NOT CONNECTED
NOT CONNECTED
```

one per possible client (players 2-4, `gMultiBootParam.client_bit` bits 1-3: `NOT CONNECTED`,
`CHECKING...`, `READY`), and below them, once a client answers, `PUSH START`,
`NOW SENDING...` or `BOOT CHECKING...` (`gTextNotConnected`-`gTextBootChecking`).

What it does: `MultiBootInit` with `gMultiBootParam`, then `MultiBootMain` every frame; A or
Start, when a client has answered and nothing is being sent, starts `MultiBootStartMaster`
(and calls the m4a functions at 0x0801B38C and 0x0801B7F0 and clears REG_IE bits 2 and 8-11,
VCount and DMA). When `MultiBootCheckComplete` says the transfer is
done it goes to state 1 (title). B also goes to state 1. Up/Right/Select do nothing.

The image it sends is **`gDebugMultiBootImage`, 0x1C26C bytes, up to `gMultiBootProgram`**: the same
data the real linked game (mode 4, `ModeMenu_Run`) streams in 0x7200-byte pieces over the SIO32
loader (`gUnk_0801E82C[0..3]` point at 0x080C63FC, 0x080CD5FC, 0x080D47FC, 0x080DB9FC) after
MultiBooting the small 0x14F8-byte loader at `gMultiBootProgram`. So the guest program of the
linked game is a complete MultiBoot image by itself, and state 6 sends it the plain BIOS way, in
one go. Its contents (read with a disassembler, not run): an ARM entry that relocates the image,
then LZ77-decompresses three streams (0x210: a 0x23564-byte program to 0x02000000; 0x190B4:
0x5000 bytes to OBJ VRAM; 0x1B32C: 0x2000 bytes to BG VRAM) and jumps to 0x02000000. The program
carries the strings `MultiSio4Sio32Load010208`, `Sio32MultiLoad010208`, `GUEST`, `ID ERROR!`
and `TIME OUT!`. Its header has no Nintendo logo (unlike `gMultiBootProgram`'s).

Opened from a level (Training 1, after dying with the byte set to 6): only the starry Training
map background, no text: state 6 loads no font of its own and the level had replaced the
title's OBJ tiles.

Nothing beyond the "NOT CONNECTED" screen can be seen here: `kurutest` runs one console, with no
link cable.

## How this was explored

`kurutest` (locked, rule 7) has no way to write RAM, so a scratch copy of
`tests/gameplay/kurutest.c` outside the repository got two options: `--poke
FRAME:ADDR:SIZE:VALUE` (a `busWrite` on the original before that frame) and `--shot-every N`.
Scripted inputs (`run.py`'s format, e.g. the title prefix `- 400`, `tap Start 60`, `tap A 60`,
`- 200`) were run on `baserom.gba` with `--poke 600:3000DCA:1:5` (or `:6`) and `--trace
gMain.state,gMain.unkB,gGameState.world,gGameState.level,gGameState.flags,helirin0.lives,...`;
from in-game, `_boot_to_training1.txt` plus `Right 300` (crash until dead) with the poke at
frame 1700. Neither state is reached by `make test`'s inputs, and since no input can write RAM,
none can be added for them.
