# Audit: decompiled functions with partly untested code

`make test` only proves the paths the recorded inputs reach (`docs/coverage.md`). This file
lists every function already in C (the game's modules, `src/<module>.c`: docs/modules.md; the
entries below predate the split and say `game`) whose coverage is below 80%, and what was done
to show that its untested paths behave like the original:

- **verified: byte-match**: `scripts/score.py <module> <fn>` is 100%, so every path is the
  original's code.
- **reviewed, not proven**: the C was compared with `asm/nonmatchings/<module>/<fn>.s` on every
  path the inputs never reach (signedness, overflow, comparisons, loop bounds, evaluation order,
  every branch); no behaviour difference was found, but the code does not byte-match, so the
  untested paths are not proven. Each entry names the uncovered paths, the in-game situation
  that would reach them, and the proposed input (`tests/proposed_inputs/`, not locked) for it.

The `/batch` rule (`.claude/commands/batch.md`): a function below 80% coverage is committed only
if it byte-matches or has the review above, recorded here.

## Audit of 2026-10-18

Coverage from `docs/coverage.md` (278 of 427 functions executed); smallest first.

| Function | Size | Coverage | Result |
| --- | ---: | ---: | --- |
| `IntrDummy` | 0x4 | 0.0% | verified: byte-match |
| `Main_ClearFrameCallback` | 0xC | 0.0% | verified: byte-match |
| `Random_Get` | 0x10 | 0.0% | verified: byte-match |
| `Text_GetDakutenChar` | 0x18 | 0.0% | verified: byte-match |
| `Text_GetHandakutenChar` | 0x18 | 0.0% | verified: byte-match |
| `Text_Convert` | 0x50 | 53.8% | verified: byte-match (added by `/batch` 2026-10-19) |
| `Sound_UpdateSongAfterPlayer` | 0x3C | 53.8% | verified: byte-match (takes `struct SongAfterPlayer *` directly, as `Object_SpringDown` does) |
| `Text_DrawNumber` | 0xC4 | 66.7% | verified: byte-match (declaration order changed: `n` before `p`) |
| `Text_DrawSmall` | 0xD8 | 69.4% | reviewed, not proven |
| `Text_Draw` | 0xDC | 70.0% | reviewed, not proven |
| `Text_PrintSmall` | 0x164 | 55.8% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Text_Print` | 0x168 | 58.2% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Text_DrawSmallName` | 0x140 | 77.4% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Text_DrawName` | 0x144 | 79.9% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Level_StartTasks` | 0x210 | 72.8% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `LevelMap_StartMarkedObject` | 0x1AC | 71.7% | reviewed, not proven |
| `Text_PrintProportional` | 0x180 | 55.4% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Lives_Init` | 0x198 | 34.5% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Collision_FindPointInShapes` | 0x184 | 54.4% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Object_RollerProjectile` | 0x240 | 71.9% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Camera_Update` | 0x298 | 33.7% | reviewed, not proven (paths covered by the locked input `see_map.txt`) |
| `Results_CheckRankUp` | 0x36C | 12.6% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Play_MainState` | 0x460 | 54.8% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Pause_Update` | 0x488 | 29.4% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Helirin_Update` | 0x8B8 | 70.3% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `ModeMenu_Run` | 0xB9C | 53.8% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Results_EnterRecord` | 0x29C | 71.0% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Collection_Run` | 0x364 | 60.5% | reviewed, not proven |
| `Play_RunLevel` | 0x5B8 | 48.1% | reviewed, not proven (partly covered by the locked input `see_map.txt`) |
| `Play_CheckLevelEnd` | 0x7A0 | 31.8% | reviewed, not proven (proposed input `timer_cap.txt`, partly) |
| `Main_RunTransfersBios` | 0xC4 | 45.1% | verified: byte-match (added by `/batch` 2026-10-19; the fill's temporary is `vu16`, as in the SDK's CpuFill16) |
| `Bg_ScrollMapClamped` | 0x260 | 64.2% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Helirin_SetSize` | 0x270 | 68.9% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Collection_UpdatePicture` | 0x37C | 36.0% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Save_LoadAtBoot` | 0x36C | 30.7% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Helirin_UpdatePreview` | 0xDC | 76.2% | verified: byte-match (added by `/batch` 2026-10-19) |
| `Effect_Firework` | 0x1C8 | 54.0% | verified: byte-match (added by `/batch` 2026-10-19; the random ranges are `rand * n / 256`) |
| `Link_Exchange` | 0xEC | 49.5% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Bg_ScrollMap` | 0x1FC | 67.9% | reviewed, not proven (added by `/batch` 2026-10-19) |
| `Pause_MakeUp` | 0x144 | 56.8% | verified: byte-match (added by `/batch` 2026-10-20) |
| `Object_ShooterShot` | 0x244 | 62.3% | reviewed, not proven (added by `/batch` 2026-10-20) |
| `Text_ConvertToU16` | 0xB0 | 42.7% | reviewed, not proven (added by `/batch` 2026-10-20) |
| `Text_PrintSmallProportional` | 0x17C | 58.4% | reviewed, not proven (added by `/batch` 2026-10-20) |

No behaviour difference was found in any of the fourteen, so no fix was needed. Every
"reviewed, not proven" function got three byte-match attempts (not kept: they only make sense
as a whole, and none reached 100%); what they showed is noted below for a later attempt.

Uncovered paths were found with the per-instruction coverage of a full `make test` run (the
`.cov` files), and the proposed inputs were checked on the original with
`build/test/kurutest --watch ... --trace ... --coverage ... baserom.gba` (their `expect` lines
hold), then measured against those `.cov` files.

### `Text_DrawSmall`, `Text_Draw` (draw a string as small / large sprites)

- **Uncovered:** every character from 0x80 up: the 0xA0-0xDF range and the 0x80-0x9F /
  0xE0-0xFF range (the original tests them separately, with the same body), the look-up of a
  following 0xDE in `gDakutenChars` and 0xDF in `gHandakutenChars` (index `ch - 0x80`, 0-127,
  inside both 0x100-byte tables), replacing the character and skipping the mark when the
  entry differs from it.
- **Reviewed:** same tables, same index, unsigned range tests, the mark is consumed only when
  the entry differs, and a full sprite list (`Sprite_Alloc` returns NULL) still stops the
  string. Same behaviour on every path.
- **In-game situation:** a string with a character from 0x80 up (followed by a 0xDE/0xDF mark
  for the look-up) drawn through these two functions. None found: the name-entry grid
  (0x0801E8E8) is ASCII on both pages (checked on screen). Codes 0x80-0xFF are not only kana in
  this (European) ROM: the 8x16 font's tiles for them (and for the codes the mark tables
  combine them into) hold English words, and the delete-saved-data screen's prompts are
  written that way. 0x0801E954 is "Which data do you want to delete?" (`Files_DeleteMenuItem`, item
  0x1FF), 0x0801E970 and 0x0801E990 are "Do you really want to / delete this data?"
  (`Files_DeleteConfirmItem`, item 0x100), mark pairs included (checked on screen, `save_files` frames
  4040 and 4100). They are drawn by `Text_Print`, whose 0xA0-0xDF mark look-up `save_files`
  does execute, not by `Text_DrawSmall` / `Text_Draw`, so they leave these two uncovered.
  **No proposed input.**
- **Byte-match attempts:** the two range tests, each with a `switch (*str)` on 0xDE/0xDF and a
  shared "replace and skip" tail, give 93% (`Text_DrawSmall`) and 91% (`Text_Draw`); what is
  left is register allocation (`str` in r5, the space and character paths adding 0x80000
  separately).

### `Text_DrawSmallName` (draw a string as 8x8 sprites, proportional lower case, plus a suffix)

- **Uncovered:** the space character (an 8-pixel gap); every character from 0x80 up and the
  0xDE/0xDF mark look-up, as in `Text_DrawSmall` (the same code, in the same two range tests).
- **Reviewed:** the space adds 0x80000 and masks with 0x1FF0000 like every other character;
  the mark look-up is the reviewed one of `Text_DrawSmall` (same tables, index `ch - 0x80`, the
  mark consumed only when the entry differs); the lower-case width test `ch - 'a' <= 25` is
  unsigned (`u32`), as the original's `bhi`; a NULL from `Sprite_Alloc` returns in both loops,
  skipping the suffix as the original does. Same behaviour on every path.
- **In-game situation:** a string with a space or a kana character drawn by its callers
  `MapScreen_LevelMenuItem` / `Challenge_MenuItem`; none of the strings the inputs reach has one (not
  investigated further). **No proposed input.**

### `Text_DrawName` (the same with Text_Draw's 8x16 cells)

- **Uncovered:** every character from 0x80 up and the 0xDE/0xDF mark look-up (as in
  `Text_Draw`).
- **Reviewed:** as `Text_DrawSmallName`: the mark look-up is `Text_Draw`'s, the width test
  (`gFontWidths`) unsigned, a NULL entry returns in both loops; the attribute word is
  `pos | 0x8000 | y` and the tile `ch * 2 + 0xE0` in both loops, as in the original. Same
  behaviour on every path.
- **In-game situation:** a string with a character from 0x80 up; none found (see `Text_DrawSmall`).
  **No proposed input.**

### `Text_PrintSmall` (draw text with a cursor and `#` escapes, 8x8 sprites)

- **Uncovered:** every `#` escape (the decimal number, `h`, `k`, `p` and any other final
  character); characters below 0x20 other than `'\n'`; every character from 0x80 up: the
  0xA0-0xDF shift after `#h` (0xA0-0xBF down by 0x20, 0xC0-0xDF up by 0x20) and the
  0xDE/0xDF mark look-up of `Text_DrawSmall`.
- **Reviewed:** the escape consumes its final character whatever it is (a NUL there runs on
  past the end of the string, as in the original); the number is `n * 10 + c - '0'` in `u32`;
  `p` stores the whole number in `p->palette` and uses `n & 0xF` as the palette; the shift is
  applied before the mark look-up, which then indexes with the shifted character; `y` is a
  `u32` that newlines can carry past 0xFF into the attribute word, and only its low byte is
  written back, as in the original; a full sprite list skips the character without stopping;
  `dx` is the signed `p->dx` << 16 masked to 9 bits. Same behaviour on every path.
- **In-game situation:** a string with a `#` escape, a control character or kana in the text
  `Debug_StageSelectItem` draws (`gDebugModeNames`); the strings the inputs reach have none (not
  investigated further). **No proposed input.**

### `Text_Print` (the same with 8x16 sprites)

- **Uncovered:** every character below 0x20 (`'\n'` included); every `#` escape; the
  0xA0-0xDF shift after `#h`; the 0xDE/0xDF mark look-up of the 0x80-0x9F / 0xE0-0xFF range.
  (The 0xA0-0xDF range's look-up of both marks, with the replacement, runs in `save_files`:
  the delete-saved-data prompts, see `Text_DrawSmall`.)
- **Reviewed:** the C is `Text_PrintSmall`'s with the attribute word `0x8000 | y | pos` and the
  tile `ch * 2 + 0xE0`, the only two differences in the original; the review above applies.
- **In-game situation:** as `Text_PrintSmall`, for the text of `Files_FileMenuItem` / `Files_DeleteConfirmItem`.
  **No proposed input.**

### `Text_PrintProportional` (`Text_Print` with proportional lower case)

- **Uncovered:** as `Text_Print`: characters below 0x20 (`'\n'` included), `#` escapes,
  the `#h` shift, the 0xDE/0xDF marks.
- **Reviewed:** the C is `Text_Print`'s with the advance of `Text_DrawName`: after a
  character, a space or a full sprite list, `'a'-'z'` (the possibly combined character,
  unsigned test) advance by `gFontWidths[ch - 'a']`, anything else by `dx`, then the 9-bit
  mask; newlines and escapes skip the advance, as in the original. Same behaviour on every
  path.
- **In-game situation:** as `Text_PrintSmall`, for the text `Save_LoadAtBoot` draws. **No proposed
  input.**

### `Level_StartTasks` (start a level's tasks)

- **Uncovered:** the world switch for worlds 4-11 and 13 (world 7's and 5's extra
  `Effect_AnimateCopies` task among them) and for a world above 13 (no case).
- **Reviewed:** the jump table, case by case: `unk17` = 13, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  0x16, 11, 12 for worlds 0-13; the extra task's frames and copies for worlds 2
  (`gUnk_080C6150`/`gUnk_080C6164`), 5 (`gUnk_080C6194`/`gUnk_080C61BC`) and 7
  (`gUnk_080C6204`/`gUnk_080C6228`); `world` is a `u8`, so above 13 nothing is set, as the
  original's unsigned `bls`; `LevelMap_Read` runs after every case, a failed allocation
  included. The list loop stops at a NULL entry or a failed allocation without consuming the
  pair, as the original (it only lays the loop out in another block order). Same behaviour on
  every path.
- **Byte-match attempts:** a `for` loop under `if (gWorldTileAnims != 0)` gives 94.3%; left:
  agbcc puts the loop's allocation after the test instead of before it.
- **In-game situation:** a level of worlds 4-11 or 13; the inputs reach worlds 0-3 and 12.
  **No proposed input** (it needs a route through Adventure).

### `LevelMap_StartMarkedObject` (start the world-object tasks of map marker `id`)

- **Uncovered:** an empty map (width x height = 0); marker 0xF4 (a `Object_Shooter` task, 0x24
  bytes) and 0xF6 (a `Object_RollerCatcher` task, 0x14 bytes); 0xF7 with bit 0 of its angle value set
  (x = 8x + 8); the inner dispatch of tile values below 0xF5; a failed task allocation for 0xF7.
- **Reviewed:** order of the `LevelMap_ReadNumber` reads (they consume the map stream), shifts,
  offsets, `0x10000 / n` (`__divsi3`, signed), the allocation sizes. One difference in the code
  but not in behaviour: the original stores the 0xF4/0xF5 tasks' x/y words at +0xA/+0xE (a
  misaligned `str`, which the ARM7 performs at +0x8/+0xC), the C at +0x8/+0xC. Task blocks are
  always word-aligned (`Heap_Alloc` rounds every size up to 4 from an aligned heap), so the
  same bytes are written.
- **In-game situation:** an Adventure level whose map holds 0xF4 or 0xF6 objects, or a 0xF7
  object with an odd angle value. None of the levels the inputs play has one (Training,
  Grasslands, the first levels of world 3, the Challenge courses of the 100% TAS); it needs
  Adventure progress beyond what the TAS keeps in sync (frame ~9 700) and beyond what a
  hand-written script can reach. A failed allocation needs a full task heap. **No proposed
  input** yet: it needs a route into the later worlds.
- **Byte-match attempts:** reading `map->tiles` first and `id != LevelMap_ReadNumber()` (81%), writing
  the 0xF4/0xF5 words at +0xA/+0xE through a cast (85%); the rest is register allocation.

### `Lives_Init` (a Helirin's lives and unk5 by game mode)

- **Uncovered:** every mode but 1/7 and 3 (`gGameState.mode` 2, 4, 5, 6 and the default for
  0 or above 7); the demo branches (`GAME_FLAG_DEMO`); `gLivesSettings.unkB` bit 0 set.
- **Reviewed:** the switch value is `(s8)(unk16 - 1)`, and only 0-6 reach a case (the
  original's unsigned `bls 6`), so 0 and 8-255 change nothing; mode 6 returns before the
  `unkBC` tail, having set bit 4; mode 5 clears lives and unk53 first, then runs mode 2's code
  when `gGameState.saveFile->flags & 0x10`, mode 1's otherwise; mode 2 sets unk5 to 5 only
  outside the demo; mode 4 indexes `unk7[]`/`unk3[]` with the Helirin's `unk0` (unsigned),
  clears unk4/unk6 and sets `unkC0` bit 1; the tail sets or clears `unkBC` bit 4 from bit 0 of
  `unkB`. Same behaviour on every path.
- **In-game situation:** VS (mode 4, a link cable), the other game modes the inputs never
  start, a demo, and settings where `gLivesSettings.unkB` (copied from `gDefaultLivesSettings`) has
  bit 0. **No proposed input.**

### `Collision_FindPointInShapes` (the first Helirin point inside a shape list)

- **Uncovered:** shape type 2 (a circle: its box tests, the point loop and the
  `dx * dx + dy * dy <= h` test) and any type other than 1 and 2 (skipped); every shape the
  inputs reach is a rectangle.
- **Reviewed:** the Helirin's box against the shape's in the original's order (max x >= left,
  min x <= right, max y >= top, min y <= bottom, all signed); the points read as `s16` (the
  original's `ldsh`); `numPoints` signed; for a circle the box is `w` around the centre on
  each side and the squared distance is compared signed with `h`, in `s32` like the
  original's `mul`; the first matching point's index is returned; a bit 0 in `unkC0` returns
  -1 before any test. Same behaviour on every path.
- **In-game situation:** a level whose bonus (`Bonus_UpdateColor`, `Bonus_UpdateType`) or other caller
  (`Bonus_UpdateItem`) uses a circle shape; `gBonusShape`, the bonus's shape, is a rectangle.
  **No proposed input.**

### `Object_RollerProjectile` (the shot of `Object_Roller`)

- **Uncovered:** a shot hitting the Helirin (the push: `Util_GetDirection` towards it,
  `unkA8 |= hits`, `Helirin_Bump(h, dir, 0x300)` for points 0-2, `unk4A` = +/-0x400 by the
  side and `(s8)(dir - angle / 256)`); a shot while `gGameState.paused` is set; a state other
  than 0 and 1.
- **Reviewed:** the hit code is `Object_Piston`'s (covered there, and compared line by line:
  `angle / 256` signed, rounding towards zero; the direction narrowed to `s8`; the two
  sign tests, `>= 0` for 0x15554 and `< 0` for 0xAAAA, giving 0x400); `unk24` set skips the
  movement, the map test and the hit test but still draws; any other state returns without
  drawing, as the original. Same behaviour on every path.
- **In-game situation:** a level with a shooting object (`Object_Roller`, map tile 0xF7)
  whose shot reaches the Helirin; the inputs only see shots that hit walls.
  **No proposed input.**

### `Camera_Update` (camera position and background layers)

- **Uncovered:** the pause menu's "See Map" free camera (`gGameState.flags` bit 0): the speed
  keys (2; A: 4; B: double), the four directions, Select (back to the Helirin); all four bound
  clamps; the VS-mode exception (`unk16 == 4` while `unk24` is set); the layer set-ups of
  worlds 6 (layer 3 at 3/4 speed plus a drifting offset), 7 and 9 (3/4 speed) and 11 (1/2).
- **Reviewed:** signed clamps against the `s16` bounds, `* 6 / 8` and `/ 2` signed (rounding
  towards zero), `gCloudDriftX / 0x10000` signed, the order of the four key tests, the jump
  table (world 2: two `Bg_ScrollMap` layers; 3, 4, 5, 8, 10: default).
- **Input `tests/gameplay/inputs/see_map.txt`** (proposed, then locked by the maintainer): pause in Training 1, See Map, scroll
  to all four edges at every speed (B held from before the pause, since a new B press leaves
  the map), Select, leave, then "Play Again" and "Quit". On the original its `expect`s hold, it
  raises the function from 33.7% to 54.9%: every path but VS and worlds 6/7/9/11. The current
  rebuilt ROM is identical to the original on it (3 976 frames), and a deliberate break
  (A speed 4 -> 5) fails at frame 1 734.
- **Still uncovered:** VS mode needs a second console on a link cable (kurutest runs one);
  worlds 6, 7, 9 and 11 need a route through Adventure.
- **Byte-match attempts:** `gCamera.` instead of a local pointer (86%), `step` before `held`
  (87%), `default:` first in the switch (99.1%); left: `held` in r3, and the original reading
  `gCloudDriftX`/`gCloudDriftY` as +0xC of a base 0x03004460/0x030043E0 (a struct there).

### `Collection_Run` (the bonus / collection screen; modes 0-3)

- **Uncovered:** mode 0's returns for world > 10, `gCamera.unk18 == 0` and bonus not taken;
  mode 1 (sixteen and two `Collection_UpdatePicture` tasks, the timed sequence of frames 45-300, states
  4-5; called only by `Ending_MainState`, never executed); mode 2; mode 3 (from `Play_MainState`
  after `Script_Run(4)`).
- **Reviewed:** the early returns skip the clean-up as in the original, `frame % 24`,
  `frame / 24` signed, `(u32)(frame - 45) <= 0xC2`, palette/VRAM addresses and sizes, the task
  numbers 0x10-0x1B, `timer` set before state 5 is entered.
- **In-game situation:** clearing the third level of an Adventure world without taking its
  bonus (the 100% TAS takes every bonus), a level whose `gCamera.unk18` is 0, the end of a
  world with every bonus (mode 3), the ending (mode 1, `Ending_MainState`). **No proposed input**:
  all need a route through Adventure levels.
- **Byte-match attempts:** `switch (mode)` in states 0 and 2 (97.5%), separate `world > 10` /
  `world < 1` returns (97.9%), the frame test as `< 0x2C` / `== 0x2C` / `<= 0x3C` (98.8%); left:
  the original's unused 0x2C-byte stack frame, `world` compared signed, `frame` in r4.

### `Play_RunLevel` (the in-level main loop)

- **Uncovered:** all of VS mode (`gGameState.mode == 4`: four Helirins, the link exchange
  through `gLinkSendData`/`gLinkRecvData`, states 2, 5, 6); `Level_Load` returning non-zero;
  the demo task (`GAME_FLAG_DEMO`); the pause menu's results (`Pause_Update` returning 2, 3,
  -1: state 0, state 4, quit) and `Play_CheckLevelEnd` returning 2.
- **Reviewed:** state machine and jump table, the size argument of the two
  `Task_Create` calls that pass whatever r1 held (8 / 0xC, as the C), the VS packet layout
  (`gLinkRecvData` stride 0x18, `angle & 0xFF00`, `unkBC | 0x2000000`), the post-increment
  `gLinkTimer++ > 0x78`.
- **Input `see_map.txt`** (above, now locked) reaches "Play Again" (2) and "Quit" (-1): 7 more
  instructions. "Make Up" from the pause menu did not give 3 in the tries made.
- **Still uncovered:** VS needs a link cable; the demo flag is never set (20 000 idle frames on
  the title screen start no demo).
- **Byte-match attempts:** state 4 placed after state 0 (86%), the `gLinkSendData[1..7]`
  clears written out and `switch` on the two results (93.9%), `i <= 3` (93.9%); left: the
  original keeps `result`, the mask, `countdown` and `bit` on the stack.

### `Play_CheckLevelEnd` (end-of-level conditions: crash, goal, timer)

- **Uncovered:** VS mode (`unk16 == 4`) in every block; modes 5 and 6 (6: `unkBC & 0x20` ends
  with -1; 5: the 0x1E fade, result 2, `Play_MoveHelirinTo` -> `unk25`); the goal test under
  `gGameState.flags` bit 2; the `GAME_FLAG_DEMO` autopilot; the timer clamp at 0x34BBF.
- **Reviewed:** both jump tables (modes 1, 2, 3, 6 / 5 / 4), `timer` compared unsigned
  (`u32`), `POS_PIXEL` (`s16`) against the `u16` camera targets, `a = b = x` storing both rate
  fields, the order of `Bg_StartWindowCircle`/`Sound_PlaySong`/flag updates, `n >= unk1C` and
  `n >= unk1C - 1` signed.
- **Proposed input `tests/proposed_inputs/timer_cap.txt`:** start the clock in Training 1 and
  wait an hour (218 402 frames): on the original its `expect`s hold (the timer stops at
  0x34BBF) and it runs the clamp's store, the one uncovered instruction of the non-VS timer
  path.
- **Still uncovered:** VS needs a link cable; modes 5 and 6 are neither Adventure (1, 2),
  Challenge (3) nor VS (4) (probably Practice, locked on a new file); the demo flag as above.
- **Byte-match attempts:** `angleRate = angleDefaultRate = x` (stores in the original's order),
  `case 5` before `case 4`, an empty `case 6` (91% at best); left: the case-tree shape of the
  `unkBC & 4` / `& 8` switches and register allocation.

### `Results_EnterRecord` (enter a level's result in the save file and the records)

- **Uncovered:** the `gGameState.mode == 2` records (+0x1B34) and its mark test
  (`helirin->unk5 == 5`); mark 2 (the Helirin's unk5 not 0 / 5: return 8); the file's
  unk10 bit 1 (every level 5-0x22 marked above 3); the file's time already among the
  records (the first loop's match: moving it up to its new place, or returning its place
  when it is not faster); a time slower than the first record in the second loop.
- **Reviewed:** the level number (`world * 3 + level + 2`), the mark (`unk14`, u16, 0: 4)
  and `unk194[n] < mark`, unk5 read signed, the mark tests `<= 3` unsigned on bytes 5-0x22 /
  0x26-0x57 and the loop bounds, every time comparison unsigned (`u32`, `>` / `<=` as the
  original's `bhi`/`bls`), `gGameState.timer` re-read in each shift test, the shifts (unk0,
  the name with `strcpy`, unkF) from the top down, the file's best time stored before the
  record, and the results `(place & 3) | 4`, `place & 3`, 8.
- **In-game situation:** finishing a level with a time that is already the file's record
 , Easy mode (`unk16 == 2`), a Helirin with
  unk5 set (a level finished in another mode), the last Adventure level getting its mark.
  **No proposed input.**
- **Byte-match attempts:** the shift loop as `for (j = i; j > 0 && ...)` (91.1%), with the
  test inside the loop and the `mark = 3; if (...) mark = 4;` form (93.9%, kept); left: the
  shift loop's pointers (the original steps pointers to `rec[j - 1]`) and the stack slots of
  `&save->unk20[n]` / `save->unk20`.

### `Results_CheckRankUp` (a save file's rank unkD raised when every best time beats the table)

- **Uncovered:** all but the entry and the first test loop's return: the Easy Mode table
  (`unk10` bit 4: `gEasyLevelTimes`), the Challenge levels 0x26-0x57, `Script_Run(5)` and
  `unkD++`, and the records of `gLevelTimes[i][2]` named `gDefaultRecordNames[2]` (inserted into
  each level's three records unless already there) when unkD reaches 2, the save
  (`Save_Write`) and the loop back for the next rank.
- **Reviewed:** unkD read once per round (byte, `<= 2` unsigned), every time comparison
  unsigned (u32 against the u16 table: `>=` returns, as the original's `bcs`), the level
  ranges 5-0x22 and 0x26-0x57 inclusive, the Easy table only for levels 5-0x22 and only in
  the test (the records of 0x26-0x57 always use gLevelTimes and + 0x9C4), unkD tested after
  its increment, the insertion (equal time and name: none; else before the first slower
  record, shifting the later ones down from the bottom; unkF 3), counters compared unsigned.
- **In-game situation:** a file whose best time of every Adventure level 5-0x22 and every
  Challenge level is below that level's gLevelTimes entry (the inputs never finish the
  Challenge courses with record times on one file). **No proposed input.**
- **Byte-match attempts:** the straightforward loops (74.5%), unkD read once into a local as
  the original does (76.5%, kept); left: the original keeps the counters in r9/r10 and the
  table entry as a pointer stepped by 6, and a 0x1C-byte frame against 0x24.

### `Play_MainState` (main state 3: the menus, modes and levels in gMain.unkB)

- **Uncovered:** sub-states 8-13 (`ModeMenu_Run` results 4 and 5: `Vs_RunMenus`,
  `Vs_RunResults`, `Practice_Run` and their levels, including `Vs_RunMenus(unk2D + 1)`);
  the Easy Mode records (+0x1B34) for gGameState.bestTime; after an Adventure level the world 10
  level 2 screens (`Script_Run(4)`, unk10 bit 6, `Save_Write`, `Collection_Run(3)`), the end
  after level 2 of worlds 1-10 with all ten bonus bits (`Script_Run(3)`, gMain.state 4,
  leaving without the frame wait) and `MapScreen_Run(1)` returning non-zero; world 0 in the
  Challenge sub-state; unkB above 13.
- **Reviewed:** both jump tables (unkB 0-13 read signed and compared unsigned, so a negative
  value only waits a frame; `ModeMenu_Run` results 1-5), every next sub-state, the level
  number for the records (`world * 3 + level + 2`, the first record's time), the halfword test
  of world 10 / level 2, the bonus bits `& 0x3FF` against 0x3FF, `(u8)(world - 1) <= 9`, the
  results of `Play_RunLevel` (0, 1, else) and `Vs_RunResults` (1, 2, else), the early return.
- **In-game situation:** the modes behind `ModeMenu_Run`'s results 4 and 5 (not reached by the
  inputs), Easy Mode, Adventure world 10 and the bonus bits of a whole world (the 100% TAS is
  in sync only up to world 3). **No proposed input.**
- **Byte-match attempts:** a `switch` with shared code for the records (78.3%, kept); left:
  the original repeats the level-number code in both record branches and keeps gMain in r2.

### `Pause_Update` (the pause menu of a level, by mode)

- **Uncovered:** gGameState.pauseRequest (a request to open the menu); mode 5's two menus
  (`Pause_DemoMenuItem` with GAME_FLAG_DEMO, else `Pause_PracticeMenuItem`); unk28 == 2 (`Pause_MakeUp`);
  mode 6 (Select + Start returns -1, Start pauses with a blinking text); all of mode 4, the
  linked game: player 0's menu (`Pause_LinkMenuItem`) and the results it sends in
  `gLinkSendData[1]` / `[7]` (1, 2, 3 for 0, 2, -1), the other players following
  `gLinkRecvData[0]` (open, close, the item `unkE`), and 2 / 3 from player 0 returning 2 / -1;
  a mode outside 1-6.
- **Reviewed:** the mode switch (`(s8)(unk16 - 1)` compared unsigned with 5, the same as cases
  1-6), the open/close conditions (Start; B only while open and unk28 == 0; nothing without
  flags bit 7), what opening and closing do in each mode and in what order (the music players,
  the menu fields, the OBJ copies, song 0x24, `flags &= ~1`, song unk17), the item sent as
  `menu.current` sign-extended, player numbers above 3 returning 0, every return value.
- **In-game situation:** VS mode (two to four consoles on a link cable), and the modes 5 and 6
  (see `Play_CheckLevelEnd`: probably Practice, locked on a new file). **No proposed input.**
- **Byte-match attempts:** gotos for the shared open/close code of modes 1-5 (87.3%, kept);
  left: the original's block order in mode 4 and its register use for gGameState / the menu.

### `Helirin_Update` (a Helirin's frame: keys, input speed, bumps, angle, zone tiles)

- **Uncovered:** the paused path drawing the recording's names (`unkBC` bit 17, `unkC8`);
  a replay running out without bit 27 (the two texts; with it `Helirin_ResetToStart` restarts it,
  which the attract demo reaches);
  recording the keys into `unkC8` (A/B folded to 1, both to 3) and the full-buffer texts;
  the linked game's crash song 0x3A; Select in a start zone or on a spring (Helirin unk5 by
  mode 1-3, 5, 6, `Helirin_SetSize`); the spring tiles 0xEC/0xEF; mode 5's slow refill of
  lives (`unk50 += 0x44` up to 0xFFF).
- **Reviewed:** the key bookkeeping (new = held & ~previous; repeat after 20 then every 4
  frames; nothing read or recorded while `unkC0` bit 1 or `unkBC` bit 30), every unkBC mask
  (0xDB57FCBA, bits 28 -> 29, ~0x3000, 0x4000000 for L/R, the spring pairs 0x180000 /
  0x600000 / 0x1800000, 0x40000 for the zones), the speeds (signed table times the u16 norm,
  y negated), the bump decay `* 192 / 256` rounded towards 0, the tile index (signed `/ 8`
  and `%` with the u16 map size), the tile cases (0xEA-0xFF, 0xF2/0xF3 only with
  `Collision_GetTilePixel`, 0xA0-0xBF setting unkC1), unk5's wraps (`(s8)` > 3 / 10 / 13), lives
  compared signed, the order of the end (`Collision_UpdatePoints`, the masks cleared, unk4A/unk4C).
- **In-game situation:** a recording (unkBC bit 17), a replay ending without bit 27, Select
  in a start zone, the linked game, mode 5 (see `Play_CheckLevelEnd`).
  **No proposed input.**
- **Byte-match attempts:** the tile `switch` with gotos for the shared zone code (75.8%,
  kept); left: the original's block order (two jump tables) and register use for the key
  fields.

### `ModeMenu_Run` (the mode menu: link partners, MultiBoot, modes 1-5)

- **Uncovered:** everything that needs a link partner: SIOCNT's player number with an answer
  (`Link_FindCommand` 3, 7, 0xF), the partner starting the linked game (`gLivesSettings`
  unk3/unk7, result 4), player 0's packets in `gLinkSendData`, item 4 (0x101, or the
  MultiBoot start), the MultiBoot clients answering (`client_bit` 2, 6, 0xE), states 12-16
  (`MultiBootStartMaster` with the image `gMultiBootProgram` less its 0xC0-byte header, the
  transfer loop of state 15 with `Link_Exchange` / `sub_0801A250` / `sub_0801A2E8` and its
  60 retries, the wait for the booted partners in 14); item 1 with the save file's level 4
  marked (mode 5) and without (song 0x36); Easy Mode (state 8's second item, unk10 bits 3
  and 4).
- **Reviewed:** the state machine (every next state, the fall-throughs 3 -> 4, 5 -> 6,
  12 -> 13, 15 -> 16), the timeouts (`++gLinkWaitFrames > 0x3C` on a u16), the item switch
  (items 1-5 of a signed byte), `result > 0` setting the mode and state 10 after the keys, the
  linked test order (the second `Link_FindCommandExact` call only when the first fails or no partner
  answers), the low byte of `gLinkRecvData[0].unk4` for 3 / 1, the packets' words, the
  MultiBoot fields (`masterp`, `server_type`, `probe_count`, `client_bit & 0xF`), the
  transfer loop's sub-states 0-5 and its byte counter `gLinkSendData` + 5, the interrupt
  handler `gIntrTable[1]`, the REG_IE masks, the exit (`IE &= 0xFF3F` unless mode 4).
- **In-game situation:** two to four consoles on a link cable, with or without the game
  cartridge in the others (MultiBoot); mode 5 needs the mark of level 4; Easy Mode is the
  second choice of the Adventure question. **No proposed input** (kurutest runs one console).
- **Byte-match attempts:** a `switch` with the shared key code behind a label (68.2%, kept);
  left: the original's register allocation (state in r8, result in r10, the link flag in r9)
  and the block order of the jump tables.

### `Bg_ScrollMapClamped` (move a map and redraw its 32 x 21 tiles into the layer buffer)

- **Uncovered:** every path with the serial interrupt on (`REG_IE & 0xC0`), where it copies
  with the BIOS (`CpuSet`) instead of DMA 3: the clear of the whole buffer when the map is
  out of view (0x150 words), the clear of a row above or below the map (0x10 words), the
  blank columns left of the map, the copied columns and the blank ones right of it.
- **Reviewed:** the out-of-view test (`x > -32`, `x < width`, `y > -21`, `y < height`, all
  signed, on `x >> 3` / `y >> 3` arithmetic shifts), the column counts (`skip = -x` when
  `x < 0`, `min(width - x, 32) - skip` copied, the rest blank), the source (`tiles + y *
  width + x + skip` halfwords, `+ width` a row), the 21 rows, the row test (`y < 0 || y >=
  height`), each CpuSet control word (`0x05000150`, `0x05000010`, `skip | 0x01000000`,
  `count`, `rest | 0x01000000`, each masked with 0x1FFFFF as the original does), the buffer
  steps (0x40 bytes a blank row, 2 bytes a column), the zero halfword and word sources, and
  `unk14 = 1` after either copy but not when the map has no tiles.
- **In-game situation:** a map scrolled during a linked game (two consoles, the serial
  interrupt on). **No proposed input** (kurutest runs one console).
- **Byte-match attempts:** the SDK's CpuFill/CpuCopy and DmaFill/DmaCopy macros (78.9%, kept);
  left: the original tests `REG_IE` before computing the source and holds the tile pointer
  in r9.

### `Helirin_SetSize` (a Helirin's size entry and OBJ tiles)

- **Uncovered:** the copies for sizes (`unk5`) 8-11 (four rows of 0x40 bytes) and 12 and up
  (0x80 bytes in one copy, from tile `unk38`).
- **Reviewed:** `unk4` and `unk5` read signed, as the original's `ldsb`; the size tests
  (`<= 7`, `<= 11`, signed); the entry's `unk6` tested signed for 0 before any copy; the
  source `gHelirinGfx + gHelirinSizeGfx[unk4][unk5].offset` (the table at `0x08391C28 +
  (unk4 * 16 + unk5) * 8`) reloaded for each copy as the original does, `+ 0x40` a row; the
  destinations `0x06010000 + (unk38 + 1 + 4 * row) * 32`, and `unk38 * 32` for the one copy;
  the 0x400-byte clear before; `Helirin_SetColor(h, unk6)` after every copy path, not after the
  early return.
- **In-game situation:** a Helirin grown past size 7 (Select on a start or healing zone in
  the modes where it changes `unk5`, `Helirin_Update`, up to 14). **No proposed input.**
- **Byte-match attempts:** none kept: the original unrolls the copies (34.3% with loops).

### `Collection_UpdatePicture` (a picture of the bonus / collection screen, `Collection_Run`'s tasks)

- **Uncovered:** no 0x800-byte block (the task frees itself); every picture 0x10 and up
  (`Collection_Run` mode 1, the ending): the restart of the animation at frame 0x3C and the
  two-sprite pictures 0x1A / 0x1B; the pictures 4, 5, 8 and 9 (and 0x14, 0x15, 0x18, 0x19);
  an unk20 above 0x1B. The inputs only draw pictures 0, 1, 2, 3, 6, 7.
- **Reviewed:** the state test (0 falls into 1, others return); the table index `unk20 & 0xF`
  for tile/tiles/x/y and the first animation, the full `unk20` for the restart; `unk20 >
  0xF` unsigned and the frame counter read at entry; `Anim_StepLong > 0` signed and the copy
  size `tiles * 32` as a u16; per case the sprite list (8 or 12), attribute 0 (`0xC0008000`,
  `0xC0000000`, `0x80000000`, `0xC0004000`), the x from the signed halfword, the y as its
  low byte (+ 0x40 for the second sprite of 0x1A/0x1B), attribute 2 (`(unk20 & 0xF) + 1`
  palette, priority 0x800 or 0xC00, 0xB800 for 0x1B); picture 5's bob `(gSineTable[low
  byte of the frame counter] + 0x20) / 64`, a signed division rounding to zero as the
  original's `asr` with `+ 0x3F` for negatives; the second sprite of 0x1A/0x1B drawn even
  when the first gets none.
- **In-game situation:** the ending's collection screen (mode 1, after all ten bonuses of
  Adventure), and the bonus screen with the bonus bits 4, 5, 8 or 9 collected; a full heap.
  **No proposed input.**
- **Byte-match attempts:** none kept (72.8%: one switch with the cases grouped by sprite).

### `Save_LoadAtBoot` (load the save at boot: erase question, checksums, format)

- **Uncovered:** the erase question (A, B, Select and Start held at boot: its loop until A,
  the two blinking answers, both outcomes); a bad checksum of the bank in use, the other
  bank's check, a bank number above 1, and the message of a bad save (its loop until A) with
  the format after it. The inputs start without a save (the signature differs: format) and
  later boot with a good bank 0.
- **Reviewed:** `(heldKeys & 0xF) == 0xF`; the first choice 1 and `Menu_StepValueUpDownWrap(&choice, 1,
  newKeys)`; each text's position and palette (0xE, the answer blinking with frame counter
  bit 3); the `newKeys & A` test before each loop's first frame; song 0x21 and the 60
  frames; choice 0 formats and returns, 1 goes on to the load; `bank <= 1` unsigned; the
  addresses `0x0E000018 + bank * 0x2254` and `0x0E000012 + bank * 2`; the u16 sum of bytes
  0-0x2253; the other bank `bank == 0 ? 1 : 0`, and that its success also returns after one
  `Save_Write`; the format (records, signature, bank 0, `Save_Write` twice).
- **In-game situation:** booting with the four buttons held, or with a save whose bank in
  use was cut off while being written (a bad checksum). **No proposed input** (kurutest
  boots without a save file).
- **Byte-match attempts:** none kept (89.4%): the original keeps `0x0E000010` in a stack
  slot from the start and the 60-frame counters in r6.

### `Link_Exchange` (a frame of the link cable exchange)

- **Uncovered:** the start of a parent (multi-player ID 0) whose cable is ready (RCNT SD
  set, SI clear, SC set, `gLinkState.unk14 == -1`): the interrupt switch from serial to
  timer 3 and the start of the transfer; the 0x80 of the return value for a parent. The
  inputs run one console, so the cable never becomes ready.
- **Reviewed:** the state byte (`unk1`: 0, 1, anything else skips to the end); SIOCNT's low
  byte read once (`& 0x30` for the ID) and written back from that read with bit 7 set, its
  high byte read-modified with `& ~0x40`; RCNT's low byte read once, `& 2` (nothing yet,
  state stays 0) and `(rcnt & 5) == 1`; `unk14` compared signed with -1; the order of the
  writes (IE `&= ~0x80`, IE `|= 0x40` as two halfword read-modify-writes, SIOCNT high byte,
  IF = 0xC0 halfword, TM3CNT = 0xC352 one word, `unk0 = 8`, SIOCNT low byte); state 1 set
  for a child and after the start; `Link_Receive(recv)` before `Link_Send(send, a)`; the
  byte `unkB` counted up on every path; the return `unk3 | (unk9 & 0x70) | unk2 << 8`, plus
  0x80 when `unk0 == 8`.
- **In-game situation:** two consoles linked by cable, this one the parent (player 1), in
  the linked game. **No proposed input** (kurutest runs one console).
- **Byte-match attempts:** none kept (85.7%): the original holds the SIOCNT address and the
  byte it read in other registers (probably the SDK's SioMultiCnt bitfields).

### `Bg_ScrollMap` (move a wrapping map and redraw its 32 x 21 tiles into the layer buffer)

- **Uncovered:** the paths with the serial interrupt on (`REG_IE & 0xC0`), where it copies
  with the BIOS (`CpuSet`) instead of DMA 3: the 21 whole rows of 32 tiles, and the rows
  split at the map's right edge (`count` tiles, then `32 - count` from the row's start).
- **Reviewed:** `x % pixelWidth` and `y % pixelHeight` signed (`__modsi3` of the u16 size),
  plus the size when negative, stored to `unk0`/`unk2`; `x & 7`, `y & 7` to the layer
  before the `tiles == NULL` return; `x >> 3`, `y >> 3` logical; `count = width - tx` and
  its signed test `> 31`; the row wrap `ty >= height` unsigned, before each row's copy; the
  source `tiles + ty * width + tx` (halfwords), advanced by `count - width` to the row's
  start; each CpuSet control word (`0x20`, `count & 0x1FFFFF`, `(32 - count) & 0x1FFFFF`,
  16-bit copies), the buffer steps (32, `count`, `32 - count` halfwords); `unk14 = 1` after
  every copy path but not on the early return.
- **In-game situation:** a level's map scrolled during a linked game (two consoles, the
  serial interrupt on). **No proposed input** (kurutest runs one console).
- **Byte-match attempts:** none kept (80.2%): the original keeps the layer pointer and the
  map's width in stack slots and the row counter in r7.

### `Object_ShooterShot` (the shot of the turret `Object_Shooter`)

- **Uncovered:** a state other than 0 or 1; a negative `gSineTable[dir]` in the 1/32
  step ahead; the frames while `gGameState.paused` is set (no move, no test, only the draw);
  `Task_Create` returning NULL for the `Effect_UpdateHitSpark` mark; the whole Helirin hit (the push,
  `unkA8`, `unk4A`) and the whole draw (the shot within 0x10 pixels of the screen).
- **Reviewed:** the state switch returns for any other state; `unk2A`/`unk2C` are the s16
  table values divided by 32 truncating towards zero (the original adds 0x1F before `asr`
  when negative); the map test passes through while bit 0 of `unk2E` is set or `timer` is
  non-zero, else leaves the mark (also when NULL) and frees the shot; `timer` decremented
  only while positive (signed); the hit: `angle = h->angle / 256` (signed, truncating),
  `dir` the s8 of `Util_GetDirection` of the signed pixel differences Helirin - shot,
  `unkA8 |= hits`, `Helirin_Bump(h, dir, 0x300)` on `hits & 7`, `unk4A` 0x400 / -0x400 by
  the sign of `(s8)(dir - angle)` (>= 0 for `0x15554`, < 0 for `0xAAAA`, tested in that
  order), and the shot freed on every hit; the draw: attribute word
  `((screen x - 8) << 16 & 0x1FF0000) | 0x40000000` with the low byte `screen y - 8`,
  `anim.unk6 | 0x4400`, and nothing when `Sprite_Alloc` returns NULL. Same behaviour on
  every path.
- **In-game situation:** a turret (map object 0xF4 of `LevelMap_Read`) whose shot flies onto
  the screen or into the Helirin, or a pause while a shot is flying. **No proposed input**
  (no level with such a turret in sight was looked for).
- **Byte-match attempts:** 84.3%: the original computes the table index `dir * 2` once for
  both velocities, and allocates the hit path's registers differently.

### `Text_ConvertToU16` (a string to halfword characters, with "#h"/"#k" and kana marks)

- **Uncovered:** every escape (`#` and the byte after it), every character from 0x80 up
  (the 0xA0-0xDF branch with its hiragana shift, the 0x80-0x9F / 0xE0-0xFF branch) and
  the 0xDE/0xDF mark look-up; the inputs only pass it plain ASCII (its one caller is the
  ending's `Ending_UpdateText`).
- **Reviewed:** `#` always consumes the next byte (also a terminating 0, as the original
  does), `h` sets and `k` clears the hiragana flag, any other byte is dropped; the range
  tests `ch - 0xA0 <= 0x3F` and `ch - 0x80 <= 0x7F` are unsigned (`u32`, the original's
  `bhi`); in hiragana `ch <= 0xBF` (unsigned) lowers by 0x20, else raises by 0x20, before
  the look-up; the mark (not consumed unless the character changes) picks the u16 tables
  `gDakutenChars` / `gHandakutenChars` at `ch - 0x80` (0-127 after the shift); a differing
  entry replaces the character and skips the mark; each character is stored as a halfword
  and counted, the escapes are not; the 0 terminator is stored and the count returned.
  Same behaviour on every path.
- **In-game situation:** none known in this (European) ROM: the ending's texts would have
  to hold `#` escapes or kana. **No proposed input.**
- **Byte-match attempts:** 98.9%: the same code with the character in r3 instead of r2
  (and the mark byte in r2 instead of r3); declaration order does not change it.

### `Text_PrintSmallProportional` (`Text_PrintProportional` with 8x8 characters)

- **Uncovered:** every `#` escape (the number, `h`, `k`, `p`) and every character from 0x80
  up (the 0xA0-0xDF hiragana shift, the 0xDE/0xDF marks of both ranges).
- **Reviewed:** the C is `Text_PrintProportional`'s (reviewed above) with the original's two
  differences: the tile is `ch - 0x20` (in the low halfword with the palette and the
  priority bits, as the original ORs them) and the attribute word is `pos | y` without
  0x8000. The escape: digits `'0'-'9'` (unsigned tests) build `n`, the byte after them is
  consumed; `h` / `k` set and clear the shift, `p` stores `n` in `p->palette` and makes the
  palette `(n & 0xF) << 12` (the original's `n << 28 >> 16`), anything else does nothing; an
  escape draws nothing and does not advance. Characters 0xA0-0xDF are shifted (<= 0xBF
  down 0x20, else up 0x20, unsigned) before the mark look-up, 0x80-0x9F and 0xE0-0xFF are
  looked up unshifted; a differing table entry replaces the character and consumes the
  mark. A full sprite list (NULL) still advances. Same behaviour on every path.
- **In-game situation:** text with escapes or kana drawn through it; none known in this
  (European) ROM. **No proposed input.**
- **Byte-match attempts:** none kept (46.9%; `Text_PrintProportional`'s C, which this follows, is at
  47.5%): the original keeps the line start, the y step and a copy of y on the stack and
  tests the characters in another order.

## Timing (2026-10-19, coverage session)

The save-file inputs start levels of The Last Land at a frame's very end: in the original the
level start misses VBlank by a few cycles, so the rebuilt ROM ends that frame one way or the
other depending on every cycle of the C on the path (`gMain.intrFlags` differs at frame end).
Bisecting (halves of the C functions back to `INCLUDE_ASM` in a scratch copy) found six; all
were made cycle-exact in the commit "Make the DMA queue, heap, OAM and video-mode setup
cycle-exact":

| Function | Was | Now | What differed |
| --- | ---: | ---: | --- |
| `Main_QueueCopy`, `Main_QueueFill` | 94% | 99.5% | the count was read once instead of once per field (an r5/r6 swap remains: same instructions) |
| `Heap_Init` | 72% | 100% | declared `void`; the original returns an (unset) value, so r0 stays live: a push, a pop and a move more |
| `Heap_Alloc` | 60% | 100% | register use of the spare size and the result |
| `Sprite_BuildOam` | 60% | 100% | per-field sprite copies instead of an 8-byte struct copy; byte AND/OR for the affine mode |
| `Bg_SetVideoMode` | 39% | 98.3% | separate register-copy globals instead of one base; case order; the layer sizes; BLDY |

`Bg_SetVideoMode` is still not byte-identical: in the tail shared by its cases the original keeps
the `gBgLayers` pointer in r2/r3 where the C uses r12 and r1 (a few `mov`s more in mode 1).
All inputs pass with it; a future input that starts a level at such a frame edge may still show
a one-frame difference there, and the fix is to finish matching it.

**A seventh (deep review, 2026-10-23): `LevelMap_Read`.** A local scenario (`all_unlocked.sav`,
Adventure: The Last Land 3, then 2, then 1, each entered from the map; now
`tests/proposed_inputs/last_land_levels.txt`) started The Last Land 1 a frame late (frame 2 713,
`gMain.frameCount` one behind from then on): its level start also ends within a few cycles of
VBlank, and `save_adventure.txt` enters level 3 of each world only. Bisecting the 165
non-matching C functions (ddmin, each set back to `INCLUDE_ASM` in a scratch copy) left
`LevelMap_Read`, the map scan of every level start. It went from 90.4% to 99.95%: the same size
and the same instructions, except the registers of two `and`/`cmp` pairs (rollers' direction
test), so the same cycles on every path. What differed: the order of the first loads (`tiles`
before `&gCamera`); one variable for the bonus's unlock bit and each tile (the register the scan
keeps the tile in); the bonus test made in each world branch; the start positions cleared with
`startX[i] = startY[i] = 0`; the 0xF1 skip loop reading into that variable; one `Task_Create`
call per spring direction; the rollers' x and y set in each branch; the default case masking
the variable itself; and the Piston and Shooter positions stored as the original does, through
a pointer to +0x0A/+0x0E (`SET_POSITION` in `src/level_map.c`: the ARM7 stores a misaligned word
at the word below, +0x08/+0x0C; the PC port writes the fields). Local scenarios entering each
level of worlds 1-11 from the map then passed except one frame of World 10 level 1's load (its
map takes more than a frame to read): at that VBlank the original had already stored the start
position (`gCamera.startX[0]`), the rebuilt ROM not yet. Bisecting again left
`LevelMap_StartMarkedObject`, which the scan calls for each 0xF0 code: 77.6% to 99.66% (only a
register pair swapped, and the same `and`/`cmp` registers as above), with the marker's positions
computed after the marker is found (not before the loop, which had pushed the id to the stack),
an unsigned switch on the code read before it is masked, the rollers' x and y in each branch,
and `SET_POSITION`. All those scenarios pass now.

## Final C pass (2026-10-21): the never-executed functions

The last 16 game functions in asm (`docs/remaining.md`) are all never executed by the inputs:
dead code (nothing in the ROM reaches it), the link-cable game (needs a second console), and
the two OTHER functions no key press reaches. Until now such a function could only become C
by byte-matching (`scripts/verify.py`). **Maintainer decision (2026-10-21):** for these, functional
C reviewed against the asm is accepted when a byte-match is not reached. Each got up to three
byte-match attempts first; the ones that did not match were written as readable C and
reviewed path by path against `asm/nonmatchings/<module>/<fn>.s` (signedness, widths,
overflow and wrap-around, evaluation order of side effects, every branch and loop bound).

The check for such a function is: `make test` and `make shift-test` pass (they prove that
nothing reachable changed: the function's size changes, so all the code and data after it
moves) plus the review below. `scripts/verify.py` rejects them (never executed, not 100%) and
was not changed. Result: **functional, reviewed, not executable on one console**.

| Function | Size | Group | Best match tried | Result |
| --- | ---: | --- | ---: | --- |
| `Sprite_AllocMany` | 0x68 | DEAD | 87.9% (3 attempts; 94.2% earlier) | functional, reviewed, not executable on one console |
| `Text_DrawSmallNumberAligned` | 0x78 | LINK | 95.6% (3 attempts; 79.9% earlier) | functional, reviewed, not executable on one console |
| `Debug_DrawHexWords` | 0x84 | DEAD | 79.0% (3 attempts; 79.0% earlier) | functional, reviewed, not executable on one console |
| `Sprite_QueueTileList` | 0x90 | DEAD | 100% (1st attempt; 95.6% earlier) | verified: byte-match |
| `Text_DrawSmallProportional` | 0xF0 | DEAD | 57.3% (3 attempts; 59.4% earlier) | functional, reviewed, not executable on one console |
| `Text_DrawProportional` | 0xF4 | DEAD | 59.2% (3 attempts; not attempted earlier) | functional, reviewed, not executable on one console |
| `Lives_SettingsScreen` | 0x104 | DEAD | 91.8% (3 attempts; 91.8% earlier) | functional, reviewed, not executable on one console |
| `Bg_DrawLevelMinimap` | 0x134 | OTHER | 86.9% (3 attempts; 86.0% earlier) | functional, reviewed, not executable on one console |
| `Link_Receive` | 0x16C | LINK | 95.4% (3 attempts; 88.0% earlier) | functional, reviewed, not executable on one console |
| `Lives_SettingsMenuItem` | 0x1B0 | DEAD | 97.6% (3 attempts; 97.8% earlier) | functional, reviewed, not executable on one console |
| `Debug_StageSelect` | 0x238 | OTHER | 94.2% (3 attempts; 87.9% earlier) | functional, reviewed, not executable on one console |
| `Vs_ResultsMenuItem` | 0x2A8 | LINK | 98.2% (3 attempts; 99.2% earlier) | functional, reviewed, not executable on one console |
| `Vs_CourseListMenuItem` (`sub_08011890`) | 0x348 | LINK | 92.1% (3 attempts; not attempted earlier) | functional, reviewed, not executable on one console |
| `Vs_ResultsMenuItem2` | 0x378 | LINK | 91.8% (3 attempts; 99.8% earlier) | functional, reviewed, not executable on one console |
| `Vs_RunResults` (`sub_08011FF8`) | 0x940 | LINK | 87.4% (3 attempts; not attempted earlier) | functional, reviewed, not executable on one console |
| `Vs_RunMenus` (`sub_08010394`) | 0x1280 | LINK | 61.6% (1 attempt; not attempted earlier) | functional, reviewed, not executable on one console |

### `Sprite_AllocMany` (Sprite_Alloc for *count* entries in a row)

- **Attempts:** decrementing *count* itself puts it in r2 as in the original but re-narrows
  it every iteration (82.6%); a `u32` copy of it 87.9%; `first` as `s32`/`u32`/`int` makes no
  difference (agbcc hoists `first * 2` and keeps gSprites in r12).
- **Review:** *list* and *count* are `u8` (both narrowed at entry). The check is
  `gSpriteCount + count - 1` compared unsigned (`bls`) with 127: `(u32)(first + count - 1) >
  127` in C, the same for every value, including *count* 0 with gSpriteCount 0 (-1, so NULL in
  both). The loop runs *count* times (`cmp r2, #0; beq` before it, `subs; bne` at its end:
  `for (i = 0; i < count; i++)`); each iteration re-reads gSpriteCount, stores the
  sign-extended (`ldrsb`) head `gSpriteLists[list]` as the 32-bit `next` (`s8` to `s32` in C),
  stores the count byte as the new head and increments the count byte (no reads after writes
  that could reorder). Returns `gSprites + first * 12` from the count read before the loop.
  No difference.

### `Text_DrawSmallNumberAligned` (Text_DrawNumberAligned drawn with Text_DrawSmall)

- **Attempts:** `Text_DrawNumberAligned`'s C with `Text_DrawSmallNumber5`'s `**&p = pad` (one
  `p++` after the `if`) 92.9%; with `gPowersOf10[*&digits]` as well, which stops loop.c's
  strength reduction of the table load, 95.6% (left: *value* and *p* swap r2/r4, and the
  original increments *p* in each branch); `*p++` in both branches with that index falls back
  to 79.1%. The committed C is the plain form (79.9%), `Text_DrawNumberAligned`'s.
- **Review:** *digits* is compared signed (`cmp r3, #1; ble`/`bgt`): `s32 digits`, `while
  (digits > 1)`. The power is loaded with the index before the decrement (`gPowersOf10[digits]`,
  then `digits--`). The subtraction loop compares unsigned (`bcc`/`bcs`): *value* and *pow*
  are `u32`. The digit is compared with `'0'`: equal stores *pad* (space until a digit was
  drawn), else stores the digit and sets *pad* to `'0'`; both store one byte and advance *p*
  (a "digit" above `'9'`, when *value* has more digits than *digits* allows, is stored as its
  low byte in both). Then `value + '0'` and a 0 byte; Text_DrawSmall gets x as `u16` (`lsl/lsr
  #16`), y as `u8` (the parameter's own narrowing), the buffer and the narrowed palette. No
  difference.

### `Debug_DrawHexWords` (8 words as 8-digit hex lines)

- **Attempts:** a pointer into the buffer 78.4%; an index (`buf[j]`, then `buf[j] = 0`) and
  `if (digit > 9)` (the original's branch order) 79.0%; all variables at function scope and a
  9-byte buffer 79.0%. agbcc keeps the *values* pointer and `i + 1` in registers where the
  original spills both (frame 0x14 against 0xC).
- **Review:** x is `u16`, y and the palette `u8` (narrowed at entry). The outer loop runs 8
  times (`i + 1 <= 7`, unsigned), loading one word per line (`ldm r2!`: `*values++`). The inner
  loop runs 8 times: the low nibble, `<= 9` gives `+ 0x30` (`'0'`) else `+ 0x37` (`'A' - 10`),
  stored as a byte at `buf[j]`, then a logical shift right by 4 (`lsrs`: `u32`); then a 0 at
  `buf[8]`. Text_DrawSmall gets x, y, the buffer and the palette; y then advances by 8 as a
  byte (`lsl/lsr #24`: `u8` wrap). No difference.

### `Text_DrawSmallProportional` (Text_DrawSmallName without the rank mark)

- **Attempts:** Text_DrawSmallName's loop as a `do`/`while` behind an explicit first test
  52.1% (agbcc still jumps to the bottom test and does not hoist the table addresses); the
  original's two range tests (0xA0-0xDF, then 0x80-0xFF, each with its own 0xDE/0xDF tests,
  `goto` to the drawing) 57.3%, the same with a plain `while` 57.3%. The committed C is
  Text_DrawSmallName's form (52.1%).
- **Review:** x is used as `(x << 16) & 0x1FF0000` (no narrowing), y is `u8`, the palette
  `(palette << 28) >> 16`, i.e. `(palette & 0xF) << 12`. The loop reads a byte and stops at
  0 (also before the first character). A space advances by 8 pixels (`0x80000`) without a
  sprite. The original tests 0xA0-0xDF and then 0x80-0xFF, both doing the same (the next byte
  0xDE: `gDakutenChars[ch - 0x80]`, 0xDF: `gHandakutenChars[ch - 0x80]`, else no table), so
  the union `ch >= 0x80` (ch is a byte, at most 0xFF) is the same test; the table's halfword
  replaces ch and skips the mark only when it differs from ch (`cmp r4, r0; beq`). Sprite_Alloc(1);
  NULL returns. attr01 is `pos | y` (32-bit store), attr2 `(ch - 0x20) | pal` (halfword
  store: the low 16 bits in both). The advance is `gSmallFontWidths[ch - 'a'] << 16` (byte
  table) for `ch - 0x61 <= 0x19` unsigned, else 8 pixels; then `pos &= 0x1FF0000`. No
  difference.

### `Text_DrawProportional` (Text_DrawName without the rank mark)

- **Attempts:** Text_DrawName's form 51.2%; the original's two range tests with the two
  tables in locals (their registers r3/r2, saved around Sprite_Alloc as in the original)
  59.2%; the same as a `do`/`while` 59.2%. The committed C is Text_DrawName's form.
- **Review:** the same instructions as `Text_DrawSmallProportional` (above: the asm of the two
  differs only in these lines) except: attr01 is `0x8000 | y | pos` (`movs r0, #0x80; lsls
  #8; orr y; orr pos`: the 8x16 shape bit), attr2 `(ch * 2 + 0xE0) | pal` (`lsls r0, r4, #1;
  adds #0xE0`), and the widths come from gFontWidths (byte table, `ldrb`). No difference.

### `Lives_SettingsScreen` (a lives settings screen with its own frame loop)

- **Attempts:** plain C with a `do`/`while` state loop and `struct Menu` 89.6% (the
  original's loop shape; gDefaultLivesSettings is reloaded as in the original); bit 0 of
  `unkB` as a 1-bit bitfield through a cast 91.8% (the `lsl #31; lsr #31` reads and the
  32-bit `& ~1` store appear, but loop.c then hoists `&gDefaultLivesSettings`); with a 0x2C-byte
  frame and the menu's unkE as a `u16` bitfield 87.2%. The committed C is the plain form.
- **Review:** state 0 fills the menu (func, 6 items, item 1, x 0x40, y 0x20; unkE bit 0 of
  `unkB`; unk10/unk12 `lives - 1`/`challengeLives - 1` stored as halfwords, so 0 becomes
  0xFFFF in both; unk14 0; the other fields untouched in both) and goes to state 1. State 1:
  Sprite_ClearLists, Menu_RunWrap with gMain.newKeys (its result unused), Sprite_BuildOam;
  B leaves (state -1) before anything else. Otherwise, with gMain.newKeys re-read for each
  test as the original does (nothing in between writes it): A on item 4 reloads the three
  values from gDefaultLivesSettings, A on item 5 plays song unk14, and A on item 6 or Start
  stores them back (`unkB = (unkB & ~1) | (unkE & 1)` as bytes; `lives`/`challengeLives` the
  low byte of the value + 1, as the original's `ldrb; adds #1; strb`) and leaves; A on 4 or 5
  together with Start does both, as in the original's fall-through to its Start test. The
  current item is compared with 4/5/6 (`ldrb` there, `s8` here: equal for every byte). Every
  frame ends with Main_EndFrame, the loop stops after the frame that set -1, then
  m4aMPlayAllStop. Lives_SettingsMenuItem reads the menu only up to unk14, inside `struct
  Menu`. No difference.

### `Bg_DrawLevelMinimap` (a 64x64 minimap from the level data)

- **Attempts:** one destination computed before the `if` 67.5%; the header read through an
  advancing pointer and the destination computed in each branch, as the original does, 85.2%;
  with *x* turned into a byte offset before the row loop (the original keeps `x * 2` on the
  stack) 86.9%. The committed C is the second (`x * 2` stays a contortion).
- **Review:** width and height are the first two halfwords, the tiles follow. The search runs
  rows `y < height` and columns `x < width` (both unsigned, `bcs`), each row from `tiles + y *
  width`, and stops at the first tile whose 10 bits are 0xFB or 0xFC (`(t & 0x3FF) - 0xFB <= 1`
  unsigned); not found gives x = y = 0. Then `x - 16` and `y - 32`, each clamped at 0 (signed
  `bge`). 64 rows (`row <= 0x3F`, signed), each with y + row: the destination word is
  `0x060174A0 + (row / 8) * 0x100 + (row % 8) * 4` (signed division by 8, as the original's
  `+ 7` for negatives; row is never negative); a row with `y >= height` (unsigned) gets 8 zero
  words 0x20 bytes apart, any other 8 words of 8 tiles each from `tiles + y * width + x` (the
  32-bit product of both): the word shifted right by 4 (logical, `u32`) per tile and 0xE in the
  top nibble for `(t & 0x3FF) - 1 <= 0x9E` unsigned (tiles 1-0x9F), so the first tile ends in
  the lowest nibble. The reads run on past the end of a row into the next, unclamped, in both.
  No difference.

### `Link_Receive` (take the packets each player sent)

- **Attempts:** clean C with a packet pointer and a union for the four flags 84.3%; the
  packet re-read through `gLinkState.unk50[i]` every time and `*(u32 *)ready = poll()` 89.7%;
  the flags as the second of two words whose first is set to 0 (the original stores a 0 at
  sp+0 that nothing reads, and its flags are at sp+4) 95.4% (left: *recv* is spilled to the
  stack and `i + 1` lands at sp+0x10 in the original). The committed C is the first: the
  dead word only reproduces the frame.
- **Review:** the IWRAM routine at gUnk_03004210 (Link_Init's copy of sub_0800019C) is called
  through a pointer, its 32-bit result kept as four flag bytes (little-endian: player n's is
  byte n). unk3 is cleared. For players 0-3 (signed `ble 3`): the 14 halfwords of
  `unk50[n]` are summed in 32 bits and compared as `(s16)sum == -1`; with player n's flag
  non-zero too: `unk3 |= 1 << n` (byte), `unk9_0 = (unk9_0 | (unk1_5 << n)) & 0xF` (byte 1 bit
  5 of the packet: the original's `lsl #26; lsr #31`), and the packet's 6 words from +4 copied
  to `recv[n]` (24 bytes, CpuSet 0x04000006: 32-bit copy). Every player's 6 words are then
  cleared (a 0 word, CpuSet 0x05000006: 32-bit fill), `unk50[n]` read again after the call in
  both. Then `unk2 |= unk3`. Only if bit 0 of unk3: for the parent (unk0 == 8), unk9_4 set when
  `unk3 & 3` and unk3 equals the new unk2, then (unk9 re-read) unk9_6 set when bits 1-3 of
  unk9_0 and unk2 agree; any other console `unk9_6 |= unk1_6` of player 0's packet (bit 6); both
  `unk9_5 |= unk1_4` of player 0's packet (bit 4). Returns unk3 (the prototype now says `u32`:
  the original returns it in r0; Link_Exchange ignores it, its code is unchanged). No
  difference.

### `Lives_SettingsMenuItem` (Lives_SettingsScreen's menu function)

- **Attempts:** plain C (Debug_StageSelectItem's form) 92.8%; an empty `case 0: break;`
  makes the switch's root 0x100 as in the original (97.6%) but its left subtree root 2
  (original: 3); `case 4: case 5:` 91.5% (and changes behaviour: rejected); an empty case
  above 0x106 (0x107, 0x1FF, 0xFFFF) 91.5%. The committed C is the plain form: the empty
  case only steers the decision tree.
- **Review:** the palette is 15 when the item is `current + 0x100` (`ldrsb`: `s8 current`),
  else 14, stored before the switch. 0x100 sets gTextCursor to the menu's x/y, dx 8, dy 0x10,
  palette 14, list 0. 0x101-0x106 print their line with Text_Print (0x101 then the
  `gUnk_080C487C[unkE]` string; 0x102/0x103 Text_DrawNumber of `unk10 + 1`/`unk12 + 1`
  narrowed to `s16`, 0x105 of unk14 read as `s16` (`ldrsh`): the same bits as the `u16` passed
  to an `s16` parameter; x and y read from gTextCursor after the print, the palette's low
  byte), then x back to the menu's x and `y += dy` as bytes. 1, 2, 3 and 5 step unkE (max 1),
  unk10, unk12 (max 5) with gMain.newKeys and unk14 (max 99) with gMain.repeatKeys; 4, 6 and
  any other item do nothing. Returns 0 for every item. No difference.

### `Debug_StageSelect` (main state 5: the debug stage select)

- **Attempts:** plain C with gIntroObjTiles for the OBJ tiles 84.4%; the tiles as
  `gLevelSpritePalettes - 0x6400` (the same address: the original derives it from the palettes'
  register, and both are hoisted out of the state loop then, as in the original) and the
  mode switch as `case 0: default:` first (the original's tree: root 1, nothing tested below
  it) 94.2%; with a 0x2C-byte frame and `unk10 * 3 + (unk12 + 2)` 92.7%. The committed C has
  the switch of the second and gIntroObjTiles (88.3%).
- **Review:** subState is cleared and the loop runs while `gMain.state == 5` (byte), the
  sub-state read signed. 0: ewramCursor 0x02000000, video mode 3, sprites cleared and built,
  DMA 3 copies of 0x200 bytes of gLevelSpritePalettes to OBJ palette RAM and 0x2400 bytes of
  gIntroObjTiles (= gLevelSpritePalettes - 0x6400, the ROM data in between moves with it) to
  0x06010000 (16-bit, 0x80000100 / 0x80001200, each control register read back), DISPCNT copy
  `|= 0x1100`, the menu (Debug_StageSelectItem, 3 items, item 3, (0x40, 0x50), unkE `flags &
  1`, unk10-14 world, level, area), the shown level -1, sub-state 1. 1: Sprite_ClearLists,
  Menu_RunWrap; B plays song 0x22 and sets main state 1 (the frame still finishes); A plays
  0x21 and sets file 0, saveFile `*gSaveFiles`, mode 6, flags 0, world and level (the low
  bytes), then by unkE: 1 flags 1 and area 0, 2 flags 4 and 3 flags 8 with area unk14's low
  byte, 0 and anything else area 0; sub-state 2. In the same frame, whatever was pressed: the
  level number (unk12 in world 0, else `unk10 * 3 + unk12 + 2`, 32-bit) against the shown one;
  when it differs, `gWorldEntries[unk10].info->levels[unk12].layers[0]`: a non-zero size
  decompresses `data + offset` to ewramCursor (Decompress_LZ with that size) and draws it
  (Bg_DrawLevelMinimap, ewramCursor re-read), a zero size fills 0x800 bytes at 0x060174A0 with
  0x11111111 (32-bit DMA fill from a stack word); the shown number is then recomputed. A sprite
  from list 0 (if any): word 0xC0080000, attr2 0xE3A5, then its y byte 8; Sprite_BuildOam. 2:
  Play_RunLevel (result unused), sub-state 1. Any other sub-state does nothing. Every pass ends
  with Main_EndFrame. No difference. (Play_RunLevel's prototype moved to a new
  `include/play.h`; play.c includes it.)

### `Vs_ResultsMenuItem` (Vs_RunResults's first results menu function)

- **Attempts:** plain C 95.5%; the constant attribute words as halfword stores and the rank
  tile as `tile = table[r]; tile |= 0xB000;` (the round-2 findings) 97.6%; one block-scoped
  sprite pointer per sprite as well 98.2% (left: the menu and gTextCursor's address swap
  r4/r5, global-alloc priority). The committed C is the plain form. Its menu is a new `struct
  VsResultsMenu` (a struct Menu, then the ranks, times and numbers pointers at +0x1C/+0x20/+0x24).
- **Review:** only item 0x100 does anything (any other returns 0 at once). It sets
  gTextCursor (the menu's x/y, dx 8, dy 0x16, list 1, palette 14), draws three header sprites
  (each: the word, the halfword, then the y byte over the word's low byte) and
  Text_DrawSmallNumber(0xD8, 0x18, unk30 + 1 as `s16`, 14). The three pointers are read once.
  Places 0-4 (signed), players 0-3: a player is drawn when its bit of gLinkPlayers (a byte,
  shifted) is set and its place (a byte) is the current one. Places 0-3 get two sprites (words
  0x40100000/0x40200000, tiles `gUnk_080C4868[place] | 0xB000` and `(... + 4) | 0xB000`), all
  sprites take gTextCursor.y re-read after their Sprite_Alloc. The mark's palette is 0x5000 when
  bit 3 of gMain.frameCount is set and the player is gLinkPlayer, else gHelirins[n].palette
  (`ldrh` +0x3A); the mark's two sprites at x 0x40 and 0x50 (`(x << 16) & 0x1FF0000 |
  0x40000000`, tiles `palette | 0x1E4` and `palette | gUnk_080C4860[n]`). The colour is 15
  under the same test (re-read), else 14: Text_DrawDigitTime(3, 0x70, y, times[n], colour), a
  sprite at 0xC0 with `(colour << 12) | 0x1E0`, Vs_DrawNumber(0xD0, y, numbers[n], colour),
  then `y += dy` as a byte. Returns 0. No difference.

### `Vs_CourseListMenuItem` (`sub_08011890`: the course list of the linked game's menus)

- **Attempts:** `if`/`else` on the item 72.8%; a `switch` with GNU case ranges (`case 1 ...
  11`, `case 0x101 ... 0x10B`: the original's range tests) 89.9%; with `current - 1` and
  `&gGameState` in locals read before the switch (the original loads both at entry) 92.1%.
  The committed C is the second.
- **Review:** 0x100: when `current - 1` (`s8`) is below unk10, unk10 goes down by one and unk12
  is 0xFFE0 (-0x20); then (both re-read) when `current - 1 >= unk10 + 4`, unk10 goes up and
  unk12 is 0x20. A sprite from list 2 (`(x << 16) | 0xC0000000`, no 9-bit mask in the
  original either; 0xC3A5; the y byte the menu's y), gTextCursor at (x, y + 0x42) with dx 8,
  dy 9, list 1, palette 14, a sprite from list 3 at the cursor (0x40004000, 0xE3FB),
  Text_DrawSmallNumber(x + 0x20, y, unk30 + 1, 14), `y += dy`. Places 0-3, players 0-3 (bit
  of gLinkPlayers set and `unk3C[n]` the place): palette 15 / 14 (frame counter bit 3 and
  gLinkPlayer) stored in gTextCursor, x back to the menu's x, Text_DrawSmall "P", the
  player's number n + 1 (1 digit) right of it, `y += dy`, `unk34[n]` (3 digits), a sprite from
  gTextCursor.list (`((x + 0x20) << 16) | 0x40004000`; `(palette << 12) | ((list >> 2) << 10) |
  0xA0` from the halfwords), `y += dy`. Then x 4 and the rows' y: unk12 > 0 (signed) goes down
  by 4, y 0x28 when it reaches 0 (a 16-bit test) else `unk12 + 8`; unk12 < 0 goes up by 4, y
  `unk12 + 0x28`; 0: y 0x28 (all as bytes). 0x101-0x10B (row = item - 0x101): drawn only
  when `unk10 <= row + 1` (unk12 > 0) or `unk10 <= row` (otherwise), and `row < unk10 + 4`
  (`+ 5` when unk12 < 0), all signed; a sprite from list 9 (`(x << 16) | 0x80004000`, `(row * 8
  + 0x120) | 0x4800`), then five from list 9 at x + 0x28 + 0x18 * j - 8 (masked to 9 bits,
  `| 0x80000000`), y - 8, tile `(palette << 12) | 0x800 | (j * 16 + 0x178)`, palette 4, or 4 +
  1 while bit 3 of the frame counter is clear for column unkE of the current row (`bics`);
  `y += 0x20`. 1-11: Menu_StepValueWrap(&unkE, 4, unkC). Anything else returns 0, as all do.
  No difference. (Text_DrawSmallNumberAligned is now declared in `include/text.h`.)

### `Vs_ResultsMenuItem2` (Vs_RunResults's second results menu function)

- **Attempts:** plain C 90.8%; the round-2 findings (halfword stores of the constant tiles,
  `tile |= 0xB000` in a variable, the colour as `?:`) 91.8%; items 1-2 as a GNU case range
  91.8% (gcc already merges them; the switch's tree still differs: the original tests the
  range below 0x100 before 0x101/0x102, agbcc the other way round). Round 2's 99.8% C was not
  committed and could not be rebuilt in three attempts. The committed C is the plain form. Its
  per-player numbers are the pointer at +0x28 of `struct VsResultsMenu` (`totals`).
- **Review:** the palette is 14 when bit 3 of gMain.frameCount is set and the item is
  `current + 0x100` (`s8`), else 15, stored before the switch. 1 and 2: with bit 0 of unkC
  song 0x21 and the item is returned, else 0. 0x100: gTextCursor as in Vs_ResultsMenuItem
  (palette 14 again), the same three header sprites and Text_DrawSmallNumber; places 0-3
  only (`r <= 3`), players 0-3 present with that place: the two place sprites (always, no
  place-4 test here), the mark at x 0x40/0x50 as in Vs_ResultsMenuItem, the colour (15 /
  14), Vs_DrawNumberRight(0x50, y, totals[n], colour), `y += dy`. 0x101: gTextCursor at (0x68,
  0x80), a sprite `(x << 16) | 0x80004000` with `(palette << 12) | 0x218` and one at x + 0x20
  with `| 0x40000000` and `| 0x220` (the palette halfword re-read, the sum stored as a
  halfword), `y += 0x10`. 0x102: `(x << 16) | 0x40000000` with `| 0x224` and x + 0x10 with
  `| 0x8000` and `| 0x228`, y unchanged. Every other item returns 0. No difference.

### `Vs_RunResults` (`sub_08011FF8`: the linked game's results, a frame loop of 8 states)

- **Attempts:** plain C 85.0% (84.8% with the send macro as `do`/`while (0)`); the times as a
  local initializer `{216000, ...}` placed through a `[[rodata]]` region 86.7%, but the
  generator refuses the region (the original's literal pool names the data, so it has a label
  `gUnk_0801ECA8` the C would have to define), so it was dropped; a struct copy from
  `gUnk_0801ECA8` as the original does (`ldm`/`stm`) 87.4%, contrived. The committed C sets
  the four times in a loop (the same values: the ROM words are constant).
- **Review, section by section** (the state is `sp+0x44`, compared unsigned for the jump
  table: 0-7, anything else only ends the frame; the result `sp+0x48`, 0 at first; the
  times at `sp+0x2C`, the numbers at `sp+0x3C` (memset 0, 8 bytes), `noneFinished` at
  `sp+0x4C`; every pass ends with Main_EndFrame and the loop stops at state -1; "next" is the
  shared tail Task_RunAll + Sprite_BuildOam):
  - **0** (falls into 1, as the jump table's code does): m4aMPlayFadeOut(BGM, 2),
    Sprite_ClearLists, Sprite_BuildOam, Main_EndFrame; WINOUT copy 0x11; the gUnk_0801EC30
    TileCopy list (`tile * 32`, `tiles * 32` narrowed by Main_QueueCopy's `u16`, as the
    original's `lsl #21; lsr #16`) and 0x1C0 bytes of palettes to 0x05000200; the current
    Helirin's regenTimer 0 and `lives = maxLives` (byte copy); the menu (Vs_ResultsMenuItem, 0
    items, item 1, (0, 0x28), unkC 0, ranks unk3C, the local times and numbers, totals
    unk34); Task_Init; the CopyAnim task (0x14 bytes: timer 1, next/value from
    gUnk_080C6270, copies gUnk_080C6284) and the SongAfterPlayer task (0x10: BGM, song 0x14,
    delay 0x3C), each only if allocated; send {0x900, 0, 0, timer >> 16, timer, bumpCount,
    0, 0}.
  - **1:** Sprite_ClearLists; Vs_CheckMenuLink(0x900): -1 state 6 and Menu_Draw, 1 Menu_Draw,
    anything but 0 then "next". Link_FindCommandExact(1, 0x900, 1) (the parent's 0x901):
    places from `gLinkRecvData[0].unkC`, a nibble each, state 2. Otherwise players 1-3 send
    {0x900, 0, newKeys, time, 0, 0} (4 and up nothing); the parent, once
    Link_FindCommand(gLinkPlayers, 0x900) is all of gLinkPlayers (re-read): per answering
    player `times[n] = unk6 << 16 | unk8`, `numbers[n] = unkA`; places cleared; for each
    answering i and other answering j, `times[i] < times[j]` (unsigned) counts j down a
    place; an answering player with Helirin flag 0x400 (time up) gets place 4, the others are
    counted, `noneFinished = (count == 0)`; send {0x901, 0, current (s8), time, places as
    nibbles 3..0, 0}.
  - **2:** the same link check; the times and numbers again from Link_FindCommand(.., 0x900)
    (for every console); players 1-3 send their time (0x900) and draw the menu, 4 and up go
    straight to the totals check; the parent re-runs Link_FindCommand, stores newKeys in
    unkC, and when all answered: A or B (`& 3`) plays song 0x21, adds (unless noneFinished)
    `gUnk_080C4848[place + 4 - playerCount]` (u32 index) to each present player's unk34,
    capped at 999 after the 16-bit add (`> 999`), and sends {0xA00, 0, unk34[0..3], unk30,
    noneFinished}; otherwise 0x901 again; then Menu_Draw. The totals check
    (Link_FindCommand(.., 0xA00) bit 0): unk34[0..3], unk30 and noneFinished from player 0's
    data, places cleared and recounted with `unk34[i] > unk34[j]` (unsigned); `unk2E != 0 &&
    unk2E <= unk30 + 1` (signed) gives Vs_ResultsMenuItem2 with 0 items, item 0, the
    Effect_UpdateLinkResultBanner task and state 4, else 2 items, item 1 and state 3.
  - **3, 4** (one jump-table target): link check with 0xA00; Link_FindCommand(.., 0xA00)
    (result unused). Players 1-3 send {0xA01, 0, newKeys, 0...}, take `current` from player
    0's unk4 low byte when Link_FindCommandExact(gLinkPlayers, 0xA00, 2), and draw the
    menu. The parent: unkC = newKeys; not all answered: Menu_Draw and {0xA01, 0, current,
    0...}; state 3: Menu_RunWrap(menu, unkC) 0 sends {0xA02, 0, current, 0...}, 1 {0x201,
    0...}, 2 {0x102, 0...}, anything else nothing; state 4: Menu_Draw, then A or B {0x102,
    0...} else {0xA02, 0...} (no current). Then player 0's 0x200 raises unk30 when
    `!noneFinished && unk30 <= 997` (0x3E5) and gives result 1, else its 0x100 result 2; both
    state 5.
  - **5:** `gWindowRadius--` tested before the decrement (`<= 0`: DISPCNT copy `(& 0xE0FF) |
    0x100`, state -1), Bg_DrawWindowCircle with the decremented radius and gWindowX/Y,
    Sprite_ClearLists, "next".
  - **6, 7:** gLinkTimer 0 and state 7 (falling into 7); `gLinkTimer++ > 0x78` (signed,
    before the increment): DISPCNT copy `|= 0x2100`, Bg_DrawWindowCircle(gWindowRadius - 8,
    0x78, 0x50) (which stores the radius it gets), and at radius `<= 0` result and state -1.
    No "next" here, as in the original.
  - Returns the result. No difference. Declarations: `gGameState.unk2E` is now the `u16` the
    code reads (it was `u8 unk2E[2]`, used nowhere), `memset` and
    Effect_UpdateLinkResultBanner are declared in `game.h` / `effects.h`, `struct
    VsResultsMenu` moved above this function.

### `Vs_RunMenus` (`sub_08010394`: the linked game's menus, a frame loop of 17 states)

- **Attempts:** one: the readable C reaches 61.6%; the original keeps the wait counter in r10,
  the menus at sp+0x00/0x2C/0x58 and the transfer state at sp+0x90-0xA8, and cross-jumps
  the many 8-halfword sends into shared tails. At that distance (0x1280 bytes) two more
  variants would not have closed it, so none was tried.
- **Review, section by section** (the state is `sp+0x88`, compared unsigned for the jump
  table: 0-16, 3 and anything above 16 only end the frame; the result `sp+0x8C`, 0 at first;
  the countdown `wait` is r10, 5 after each menu's setup: while not every player answers, the
  parent sends its menu state only once it reaches 0; "next" is Task_RunAll +
  Sprite_BuildOam; every pass ends with Main_EndFrame, the loop stops at -1; the link check
  of each menu state: Vs_CheckMenuLink -1 state 15 (no draw), 1 the menu drawn, anything but
  0 "next"; every send writes the 8 halfwords in order, the menu's current item sign-extended):
  - **0:** ewramCursor 0x02000000, video mode 4, the gUnk_0801EB7C TileCopy list and 0x1C0
    bytes of palettes, screen 0x11, Task_Init, Effect_RotateBg2 (8 bytes). *a* 1 jumps to
    state 8's code, 2 to state 9's; else mode 4, playerCount the bits 0-3 of gLinkPlayers,
    world 12, level 0, DISPCNT copy 0x3741, gLinkWaitFrames and gLinkTimer 0, state 1 (frame
    end without "next").
  - **1, 4 and the first menu's setup:** 1: sprites cleared and built, window circle from 0
    at (0x78, 0x50), song 0x29, state 2; 4: state 5; both then: ewramCursor, screen 4, the
    menu (Vs_MatchMenuItem, 2 items, item 1, (0x40, 0x40), unkC/unkE 0), wait 5, Sound_PlaySong(0xE).
  - **2, 5** (2 falls into 5's code, as the original's): 2 draws the circle at `gWindowRadius
    + 8` and at a radius above 0x9F clears DISPCNT bits (`& 0xDEFF`) and sets 5. Then
    Sprite_ClearLists; any state but 5 draws the menu, "next". 5: the link check (0x200); the
    parent's 0x201: result -1, state 13; its 0x202: current and unkE from its data, unk2D =
    `current - 1` (byte), unk2E 0 and state 9 when non-zero, else unk2E = gUnk_080C4844[unkE],
    Random_Shuffle(frame counter), Vs_ShuffleCourses and state 6; unk30, unk34[] and unk3C[]
    cleared; the menu drawn. Players 1-3 send {0x200, 0, newKeys, 0...}, follow the parent's
    0x200 data, draw the menu (4 and up nothing). The parent: unkC = newKeys; all answered:
    Menu_RunWrap, then B song 0x22 and {0x201, 0, current, unkE, 0...}, else A song 0x21 and
    0x202, else 0x200, the same words (no draw); not all: draw, and 0x200 at wait 0 else
    `wait--`.
  - **6, 7:** 6: screen 0x10, the menu (Vs_CourseRowsMenuItem, 2 items, item 1, (0x68, 0x38), unkC 0,
    unkE 0, unk10 9), wait 5, song 0xF (continued), state 7. 7: as 5 with 0x300 and the
    repeat keys, the sends carrying unkE and unk10 in [3]/[4]; the parent's 0x301 goes back to
    4, its 0x302 takes current/unkE/unk10 and sets unk32 to `(2^(|unkE - unk10| + 1) - 1) <<
    min(unkE, unk10)` (the original's loop `m = m << 1 | 1`), state 8.
  - **8 (falls into 11):** world 12; the parent draws Vs_NextCourse until bit `n / 5` (signed
    division) of unk32 (re-read each time) is set, and stores it as the level.
  - **11:** screen 0xF, DISPCNT copy 0x1641, the settings menu (Vs_PlayerSettingsItem, 2
    items, item 1, (0, 4)), `settings[n] = (linkLives[n] - 1) << 4 | linkSizes[n]`, each
    Helirin Helirin_Init(n), angle 0xC000, pixel x 0x60 and pixel y 0xC + 0x28 * n (halfword
    stores into the high halves), the camera and the link counters 0, wait 5; the parent with
    unk2C: from `gWorldEntries[world].info->levels[level]`: layer 0's size, layer 1's and
    layer 2's offsets minus layer 0's (the second one the total), the data at `data + layer
    0's offset`, the sent bytes and the done frames 0; song 0xF (continued), state 12.
  - **9:** ewramCursor, screen 5, DISPCNT copy 0x1641, the course list (Vs_CourseListMenuItem, 10
    items, (0xA8, 8), unkC/unk12 0, item `level / 5 + 1` and unkE `level % 5`, both unsigned;
    unk10 6 for an item above 7 (signed) else item - 1), song 0xF, wait 5, the shown course -1,
    state 10.
  - **10:** link check (0x400). The parent's 0x401: state 4 and draw; its 0x402: current
    (low byte) and unkE (high byte) of its unk2, world 12, level `unkE + (current - 1) * 5`,
    state 11, draw. Players 1-3 send {0x400, repeatKeys, 0...} (the keys in [1] here), follow
    the parent's 0x400 and draw; the parent runs Menu_Run (not RunWrap) with the repeat keys
    and sends 0x401 / 0x402 / 0x400 with `current | unkE << 8` in [1] (or draws and counts
    down as in 5). Then for every console: the course `(current - 1) * 5 + unkE`; when it
    changes: Challenge world 12's level: a minimap layer (size non-zero) is queued to
    0x060174A0 (0x800 bytes), else level data is decompressed to ewramCursor and drawn
    (Bg_DrawLevelMinimap), else 0x11111111 is queued as a fill; the sent count 0 and the total
    0x200. The parent with unk2C and wait 0: a receiver whose bit is in unk2C (players 1-3)
    and whose packet's unk4 is non-zero steps the count back by 0x20 (to 0 below 0x20, both
    unsigned); [2] = total; while the count is below it (unsigned): [3] = count + 1, and
    [4]-[11] each 16 bits from 8 bytes of the minimap from `offset + count * 4`: per byte its
    low then high nibble, a 1 unless the nibble is 1 or 7, shifted in from the right (16-bit),
    the count + 2 per halfword. ([8]-[11] lie past the 8 halfwords gLinkSendData was declared
    with: it is 12, as Link_Send copies 6 words; the declaration now says 12.)
  - **12:** link check (0x500). Each answering player not on this console (`~gUnk_03000DB4`)
    has its settings from its unk2. The parent's 0x501: state 6 (9 when unk2D); its 0x502: the
    sizes (`& 0xF`) and lives (`((s & 0x70) >> 4) + 1`) into gLivesSettings, mode 4, world and
    level from its unkE's two bytes, result 0, state 13. Players 1-3: the menu (drawn only when
    ready, bit 7), A sets ready (song 0x21), B clears it (song 0x22, only if set), then
    {0x500, settings, 0...}. The parent: `ready` = the answering players with bit 7; all
    answered: the menu as above; a player's 0x501 sends {0x501, settings, 0...} and ends the
    frame; A sets ready; B (song 0x22) sends 0x501 when not ready, else clears it and sends
    0x500; otherwise all ready and (no unk2C, or the data done for more than 10 frames,
    signed) sends {0x502, settings, total, 0, layer 1's start, layer 0's size, 0, world |
    level << 8}, else 0x500. Not all answered: draw, 0x500 at wait 0 else `wait--`. Then
    with unk2C: at wait 0 the same resend step (also clearing the done frames), [2] = total,
    and while the sent bytes are below it: [3] = sent + 1 and halfwords from `data + sent`
    into [4].. until [11] or the total (the test after each), 2 bytes each; at the total the
    done frames count up. "WAIT" at (0x68, 0x4C) in colour 14/15 (frame counter bit 3) while
    all are ready and the data is not done; Vs_DrawSpinner(sent, total).
  - **13, 14:** BGM fade-out (speed 1), window circle from 0xA0, DISPCNT copy `|= 0x2100`, song
    0x2A, state 14; 14 draws the circle at `gWindowRadius - 8` (Bg_DrawWindowCircle stores the
    radius) and at `<= 0` stops all music and leaves (-1, the result unchanged).
  - **15, 16:** as Vs_RunResults's 6/7 (result -1).
  - **Uninitialised values:** the 0x502 send carries the total and the two layer values even
    when unk2C is 0 and they were never set (the total is 0x200 after the course list); the
    original sends whatever its stack held. Nothing reads them: every console that runs this
    code takes only the course from [7]. The C first sent whatever its locals held (reading
    an uninitialised variable, which a PC compiler may treat as undefined); since the deep
    review (2026-10-29) the three start at 0, so those words are 0. No other difference. Declarations: `gGameState.unk32` is the `u16` the code reads (it was `u8
    unk32[2]`, used nowhere), `struct VsSettingsMenu` moved above this function.

## Differential calls (deep review, 2026-10-29)

The "reviewed, not proven" functions that can be called on their own (no frame loop, no
interrupts needed) were run in both ROMs at once, outside `make test`, with a local harness on
libmgba: both ROMs boot the same input to a frame inside a level, then for each call the same
RAM/IO state is written into both (random values for the function's inputs, within what the
code can be handed, and many outside it), the function is called with the same arguments, and
r0 and the whole of EWRAM, IWRAM (but the stack), palette, VRAM and OAM are compared afterwards
(words holding ROM addresses are mapped as `make test` maps them). The harness also records which instructions of
the original ran. No difference was found; the coverage is of the original's instructions,
these calls together with `make test`'s:

| Function(s) | Calls | Coverage |
| --- | ---: | --- |
| `Text_DrawSmall`, `Text_Draw`, `Text_DrawSmallName`, `Text_DrawName`, `Text_PrintSmall`, `Text_Print`, `Text_PrintProportional`, `Text_PrintSmallProportional`, `Text_ConvertToU16` (and `Text_Convert`, the number/hex/proportional drawers, `Sprite_AllocMany`) | 1 088 | 100% each |
| `Link_Exchange`, `Link_Receive` (and `Link_Init`, `Link_Send`) | 1 000 | 100% |
| `Lives_Init`, `Lives_SettingsMenuItem` | 6 690 | 100%, 99.4% |
| `Collision_FindPointInShapes`, `Collision_TestShapes`, `Collision_UpdatePoints`, `Helirin_SetSize` | 8 000 | 99.5%, 100%, 100%, 100% |
| `Bg_ScrollMap`, `Bg_ScrollMapClamped` (wrapping/clamped maps of every size, any scroll) | 930 | 100% |
| `Bg_DrawLevelMinimap` (every level's layer 0, then random tiles) | 188 | 98.6% |
| `Object_ShooterShot`, `Object_RollerProjectile` (random Helirin positions, sizes, angles, flags) | 23 100 | 97.3%, 99.6% |
| `Results_EnterRecord` (every mode, level, time and record table) | 1 500 | 100% |
| `Level_StartTasks` | 224 | 100% |
| `Camera_Update` (every world, mode, pause, See Map keys, bounds, cloud drift) | 1 200 | 100% |

What stays unexecuted: a `b` after a path that always returns (`Lives_SettingsMenuItem`,
`Collision_FindPointInShapes`, `Object_RollerProjectile`, `Object_ShooterShot`), the rounding
`adds #7` of a signed division in `Bg_DrawLevelMinimap` for a negative operand (its operands are
never negative), and in `Object_ShooterShot` the `-0x400` arm of the 0x15554 (inner-side) hit's
angle-rate choice, read against the asm: `(s8)(dir - angle) >= 0 ? 0x400 : -0x400` is its
`lsl #24; cmp; bge`. These functions stay "reviewed, not proven" in the table above (the C
does not byte-match), but each of their paths has now run against the original.

Not callable this way (frame loops that wait for VBlank, or state machines across frames):
`Play_RunLevel`, `Play_CheckLevelEnd`, `Play_MainState`, `Pause_Update`, `ModeMenu_Run`,
`Results_CheckRankUp`, `Collection_Run`, `Collection_UpdatePicture`, `Save_LoadAtBoot`,
`Helirin_Update`, the `Vs_*` loops. They were re-read against their asm only.

## Audio with mGBA's volume on (2026-10-22)

`make test`'s audio comparison compares silence: kurutest never sets mGBA's `volume` option, so
mGBA's master volume is 0 and every sample is 0 (found by the port's audio work, docs/port.md).
The maintainer-approved fix is one line in `emuInit` (`emu->core->opts.volume = 0x100;` before
`reloadConfigOption`; checked: 92% of the samples of the first 1 200 frames are then non-zero,
peak 21 700). **With it, `make test` and `make shift-test` fail on all 14 inputs** (first
differences at frame 95, or 33 for the inputs that boot from a save), so the change was not
committed (rule 4).

Why: the audio depends on the cycle at which the game writes the sound hardware, not only on
what it writes.
- `m4aSoundMain` runs at the end of each frame's work (`Main_EndFrame`), and its `CgbSound`
  writes the GB channels' registers (a channel's restart resets its waveform's phase at that
  cycle). In levels the rebuilt ROM's C is faster: on `training1_moves` the writes come 1-2
  lines earlier on 164 frames (port/test/refdump `--psg-writes`: line 26 instead of 28...).
- DirectSound differs first (frame ~93, the first note): single samples at the edges of
  DirectSound samples, 6 331 of 5 085 008 values on `training1_moves`: the sample timer's
  phase is a few cycles off (`SampleFreqSet` starts it after a VCOUNT polling loop, whose exit
  depends on the cycles of the code before it).

The m4a library itself byte-matches; the game's C that runs before it decides the timing.

Evidence, from a scratch copy: all 165 game functions below 100% (objdiff) put back to
`INCLUDE_ASM` give the original's SHA1, and that build passes `make shift-test` with the
volume on (all 14 inputs, everything moved by 0x100 bytes): code position does not matter,
only the code. Then one module's non-matching *executed* functions at a time in C (150 in all,
docs/coverage.md), everything else as the original's asm, on 13 inputs (`bg` and `bonus`: 14,
with `tasvideos_6116M_100`):

| Module | Non-matching executed functions | Inputs whose audio differs | Earliest frame |
| --- | ---: | ---: | ---: |
| `bg` | 7 | 14 | 379 |
| `bonus` | 5 | 4 | 636 |
| `challenge` | 2 | 2 | 929 |
| `collection` | 2 | 2 | 959 |
| `collision` | 5 | 10 | 637 |
| `effects` | 9 | 13 | 381 |
| `ending` | 4 | 1 | 3202 |
| `files` | 7 | 12 | 444 |
| `helirin` | 8 | 10 | 636 |
| `hud` | 3 | 10 | 636 |
| `intro` | 8 | 13 | 34 |
| `level` | 2 | 0 | - |
| `level_intro` | 2 | 7 | 591 |
| `level_map` | 3 | 4 | 1264 |
| `link` | 1 | 11 | 468 |
| `main` | 5 | 8 | 636 |
| `makeup` | 4 | 1 | 1057 |
| `menu` | 3 | 3 | 1191 |
| `mode_menu` | 12 | 11 | 466 |
| `objects` | 10 | 5 | 1269 |
| `pause` | 5 | 5 | 1571 |
| `play` | 4 | 11 | 466 |
| `practice` | 2 | 1 | 970 |
| `results` | 4 | 10 | 636 |
| `save` | 2 | 0 | - |
| `script` | 3 | 7 | 491 |
| `sprite` | 3 | 13 | 34 |
| `task` | 3 | 13 | 34 |
| `text` | 12 | 13 | 45 |
| `util` | 10 | 13 | 34 |

28 of 30 modules break the audio on their own. Function by function in four of them (`intro`,
`sprite`, `task`, `text`; 27 functions, on `training1_moves`, `save_corrupt`, `save_makeup`),
19 do so alone; the earliest (frame 95 / 34) are `Task_RunAll`, `Sprite_AllocAffine`,
`Intro_Update` and `Intro_UpdateSprite` (and something in `util`). Functions that pass alone:
`Sprite_InitRotations`, `Text_Draw`, `Text_DrawNumberAligned`, `Text_ConvertToU16`,
`Text_PrintSmallProportional`, `Task_FindByKey`, `Intro_MainState`, `Intro_UpdateBgAnim`,
`Intro_UpdateTiltedHelirin`, `Intro_UpdateMovingHelirin`, and the modules `level` and `save`.

So there is no single culprit: for the audio to compare equal, roughly 100 of the 150 executed
non-matching functions (many at 40-75%, e.g. `Collision_UpdatePoints` 45%) would have to take
exactly the original's cycles, which in practice means byte-matching them. Options for the
maintainer: (1) that matching campaign, the volume fix landing when it is done; (2) a harness that
compares what the game writes (m4a's RAM and PCM buffer, the sound registers at frame end, as
`make -C port test-audio` does) instead of mGBA's mixed samples; (3) keep the comparison as it is.

**Decision (maintainer, 2026-10-22): (2).** kurutest now compares what each ROM writes to the sound
registers (0x04000060-0x040000A7), in order with the values, the sound DMAs' FIFO words as
streams and m4a's PCM buffer, not when in the frame (commit "Maintainer-approved: kurutest compares
sound register writes"; CLAUDE.md, "The gameplay equivalence test"). mGBA's volume stays 0.
A first version compared the PCM buffer every frame and the FIFO streams without breaks: 6
inputs passed and 8 failed, each on a frame where the original's work ran past the VBlank:

| Input | Frame | First difference | At that VBlank the original was |
| --- | ---: | --- | --- |
| training1_moves, training1_crash, see_map | 1342 | `pcmBuffer+0x1D0`: 0xF0 / 0xEE | mixing (the buffer half filled) |
| tasvideos_6116M_100 | 1363 | `pcmBuffer+0x1D0`: 0xF0 / 0xEE | mixing |
| tasvideos_3933M_any | 1107 | `pcmBuffer+0x470`: 0xF0 / 0xEE | mixing |
| save_makeup | 2316 | `pcmBuffer+0x321`: 0xEF / 0xEE | mixing |
| save_adventure | 2987 | FIFO A word 162 298: 0xFEFDFEFF / 0 | just after a load (m4aSoundVSyncOff, then On) |
| save_practice | 1680 | FIFO A word 90 460: 0xFF000000 / 0 | just after a load |

The FIFO cases, in a dump of both streams with the DMA 1 control writes: the rebuilt ROM restarts
the DMA (`m4aSoundVSync`: CNT_H 0, then 0xB600) after word 162 296, the original after 162 300:
the original's timer asked for one more transfer before the restart. The words after the
restart are the same in both. So the exclusions are: at each restart of a FIFO's DMA, at most
one transfer (4 words) more in one ROM since the last restart is not compared; the PCM buffer
is compared only for frames the original finished before the next VBlank. With them, all 14
inputs pass `make test` and `make shift-test`:

| | Compared | Left out |
| --- | ---: | ---: |
| CPU writes to the sound registers | 201 612 | 0 |
| FIFO DMA restarts (both FIFOs) | 67 959 | 72 transfers (save_adventure 10, save_practice 6, tasvideos_6116M_100 56) |
| PCM buffer, frames | 165 884 | 2 038 (the original's lag frames: the port test's 2 038) |

Broken on purpose (each reverted): the wall-bump sound effect in `Helirin_UpdateCollision`
(0x34 -> 0x35) fails training1_crash at frame 1819 (FIFO A word 100 943); the title's music
fade-out speed in `Intro_MenuItem` (2 -> 3) fails menus at frame 402 (FIFO A word 22 294); the
menu cursor sound in `Menu_MoveWrap` (0x20 -> 0x21) fails menus at frame 666 (CPU write 2 of the
frame: the original writes 0x04000060 = 0x4D, GB channel 1's sweep). (A first try,
`Effect_UpdateLinkCountdown`'s countdown sound, passed: that function never runs, docs/coverage.md;
`Menu_Move`'s cursor sound passed `menus` too: `menus` runs `Menu_MoveWrap`, not `Menu_Move`.)
