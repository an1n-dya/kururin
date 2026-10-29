# The game functions that were still in asm

**Done (2026-10-21, final C pass): every one of the 356 C-module functions is C; no game
function is `INCLUDE_ASM` any more.** The last 16 below were all never executed by the inputs
(`docs/coverage.md`: 0.0%), so each had to byte-match (`scripts/verify.py`) to be converted.
By the maintainer's decision for these 16, functional C reviewed path by path against the asm was
accepted where a byte-match was not reached: `Sprite_QueueTileList` byte-matches, the other
15 are **functional, reviewed, not executable on one console** (each one's attempts and
review: `docs/audit.md`, "Final C pass"; `tools/next.py --progress` counts them). Their check
was `make test` + `make shift-test` (nothing reachable changed) and that review;
`scripts/verify.py` rejects them and was not changed.

This file, written while they were asm (when they were all in module `game`; their modules
now: docs/modules.md), records what blocked each match. Classified into three groups:

- **DEAD**: nothing in the ROM reaches it.
- **LINK**: reached only through the link cable: the linked game (mode 4), the wait for a
  partner, MultiBoot, the serial exchange.
- **OTHER**: reachable in principle, by no input.

**2026-10-20, first session** (25 attempts): 10 converted (9 LINK, 1 OTHER).

**2026-10-20, final push** (59 of 60 attempts; OTHER, LINK, DEAD smallest first): 20 more
byte-match in C (3 OTHER, 3 LINK, 14 DEAD), marked **in C** below. Total 333/356 (game
262/285); **23 still asm**: 2 OTHER, 11 LINK, 10 DEAD. No function needed its own `.rodata`
(none was skipped for it), so the `[[rodata]]` support unblocked nothing here. Not attempted:
the five LINK near-misses of 2026-10-19 and `Text_DrawSmallNumberAligned`, the two LINK functions over
2 KB, `sub_08011890`, and the six DEAD functions from 0x90 bytes up, for the budget.

**2026-10-20, final push round 2** (63 of 70 attempts; the near-misses, then the never-attempted
ones): 4 more byte-match in C (3 LINK, 1 DEAD), marked **round 2** below. Total 337/356 (game
266/285); **19 still asm** then: 2 OTHER, 8 LINK, 9 DEAD. Not attempted: `Text_DrawProportional` (the same
routine as `Text_DrawSmallProportional`, same blocker), `sub_08011890` (budget), the two LINK functions over
2 KB, and `Text_DrawSmallNumberAligned`, `Link_Receive`, `Debug_DrawHexWords`, `Bg_DrawLevelMinimap`, `Debug_StageSelect` (no new
idea against their recorded blockers).

**2026-10-20/21, permuter session** (Vs_ResultsMenuItem2, Pause_LinkMenuItem, Vs_ResultsMenuItem, Text_DrawSmallNumber5,
Lives_SettingsMenuItem, Sprite_QueueTileList from their recorded best C, decomp-permuter -j2 each; the container
was reclaimed whenever the session idled, so the six ran ~1 h in total, then the two closest
~3 h from their best output): 3 more byte-match in C, marked **permuter** below:
`Text_DrawSmallNumber5`, `Text_DrawSmallNumber3` (the same fix, not one of the six) and `Pause_LinkMenuItem`. Total
340/356 (game 269/285); **16 still asm**: 2 OTHER, 7 LINK, 7 DEAD. "Permuter" scores below are
decomp-permuter's (0 = match), not objdiff's.

**2026-10-21, final C pass** (single agent; up to three byte-match attempts each, smallest
first, `Vs_RunResults` and `Vs_RunMenus` last): 1 byte-match (`Sprite_QueueTileList`), 15
functional C reviewed against the asm. Total 356/356 (game 285/285); byte-identical: 191 of
the 356 (game 120, libraries 71).

Sizes are the function's bytes in `config/functions.txt` (literal pools included). "Best" is
the objdiff score (`scripts/score.py`) of the best C tried; the C of the near-misses is not
committed (it is described here and in `docs/skipped.md`).

## How the references were searched

Over `baserom.gba`, with the function boundaries of `config/functions.txt`:

1. **BL callers:** every Thumb `bl` pair (0xF000-0xF7FF, 0xF800-0xFFFF) inside every Thumb
   function, decoded to its target.
2. **Pointers in code:** every aligned word inside every function (literal pools; ARM crt0
   words too) equal to a function entry + 1.
3. **Pointers in data:** every 2-aligned word from 0x0801CB64 to the end of the ROM equal to a
   function entry + 1 (a superset of `config/data_pointers.txt`, which lists only aligned
   words in its `[pointers]` ranges).
4. **Any byte offset:** for each DEAD candidate, the whole ROM searched for the 4-byte
   little-endian address, with and without the Thumb bit: no hit for any of them.
5. **Reachability:** from the roots (crt0's ARM routines, AgbMain, and every function a data
   word points to, e.g. the interrupt table at 0x0801CB64 and AgbMain's state table
   0x080C28B4), following BL edges and code pointers (a function whose pool holds a pointer
   can store it as a callback). Everything a root does not reach is DEAD. (The same search
   finds 26 unreachable library functions in m4a and libgcc, which stay as they are.)

The DEAD functions also all entered `config/functions.txt` as gaps between other functions
(evidence `gap`, some `ghidra`), never from a `bl`, `ptr` or `pool`, except `Sprite_AllocMany`,
`Menu_MoveLeftRight`, `Menu_MoveSelect` and `Lives_SettingsMenuItem`, whose only references are from other DEAD
functions.

## The last 16 (in C since the final C pass)

Each row as it stood before the final C pass; the final pass's best scores and results are
in `docs/audit.md`. `Sprite_QueueTileList` byte-matched on its first attempt there (the source
pointer read in the `while` condition, the size a `u16` variable); the others are functional
C, not byte-identical.

| Function | Group | Size | Best | Blocking difference |
| --- | --- | ---: | ---: | --- |
| `Sprite_AllocMany` | DEAD | 0x68 | 94.2% (5 attempts; round 2: 94.0%) | Sprite_Alloc for *count* entries in a row (`for (i = count; i != 0; i--)`, the range check `(u32)(counter + count - 1) > 127`). Only the registers of the entry block differ: the original narrows *count* straight into r2 and keeps the counter in r1; agbcc keeps *count* in r1 (its incoming register) and copies it to r2 for the loop. Operand order and the loop's form do not change it. (Its prototype must return `struct Sprite *`; `Sprite_DebugTileGrid` then casts.) |
| `Debug_DrawHexWords` | DEAD | 0x84 | 79.0% (3 attempts) | 8 words as 8-digit hex (lowest digit first). Register pressure: the original spills the *values* pointer and `i + 1` to the stack (frame 0x14) and keeps 0xF, *x* and *palette* in r8-r10; agbcc keeps them all in registers (frame 0xC). A buffer pointer and a `mask` variable do not change it |
| `Sprite_QueueTileList` | DEAD | 0x90 | 95.6% (round 2, 6 attempts) | queues rows of OBJ tile copies (entries {src, tile, width, rows}, NULL-ended) in gTransferQueue. Found: `last = rows - 1; if (last + gTransferCount > 0x3F)` (in one expression fold moves the - 1 onto the count), `e++` after the inner loop (keeps `e + 1` in r12 over it). Left: *src*/*dst* swap r4/r5, and *e*/`&gTransferCount` swap r1/r2 (agbcc copies the count's address into another register for the inner loop); declaration order does not change it Permuter session: ~1 h of permuter from 95.6% (score 305): best 250, no idea that closes the swaps; dropped. |
| `Text_DrawSmallProportional` | DEAD | 0xF0 | 59.4% (round 2, 8 attempts) | Text_DrawSmallName without the suffix (proportional text, 0xA0-0xDF tested before 0x80-0xFF for the 0xDE/0xDF combining tables). The original rotates the string loop (exit test copied in front) and hoists the two table addresses and 0x1FF0000 into r2/r3/r10 (spilled around the call); agbcc leaves the loop unrotated and loop.c finds every move "not desirable" (`-dL`: savings 1, life 1, 83 insns), so the tables fold into `gDakutenChars-0x100` pool words. `Text_DrawSmallName`'s C (functional only) gives 52%; `for (ch = *str++; ...; ch = *str++)`, `break` for the return and an index variable do not rotate it |
| `Text_DrawProportional` | DEAD | 0xF4 | not attempted | the same routine as `Text_DrawSmallProportional` (Text_DrawName's variant: tiles `c * 2 + 0xE0`, widths gFontWidths), the same blocker |
| `Lives_SettingsScreen` | DEAD | 0x104 | 91.8% (round 2, 4 attempts) | a settings screen with its own frame loop over a copy of gLivesSettings (menu Lives_SettingsMenuItem). Needs gLivesSettings's byte 0xB as a 1-bit bitfield (the original reads it with `lsl #31; lsr #31` and stores it with a 32-bit `& ~1`) and a 0x2C-byte menu (the frame; Vs_RunResults's menu struct has that size); neither is committed. Left: loop.c's second pass hoists `&gDefaultLivesSettings` out of the frame loop (`-dL`: life 10 in 125 insns, moved), the original reloads it; the bitfield store reads unkE with `ldrb` where the original uses `ldrh` |
| `Lives_SettingsMenuItem` | DEAD | 0x1B0 | 97.8% (round 2, 4 attempts) | Lives_SettingsScreen's menu function. Only the switch's decision tree differs: with cases 1, 2, 3, 5, 0x100-0x106 agbcc's root is 0x101; adding `case 4: break;` (or `case 4: default:`) gives the original's root 0x100 and subtree root 3, but agbcc then also tests 4, where the original tests only 5 above 3 Permuter session (~1 h): best 5 swaps the bodies of cases 4 and 5 (changes behaviour, rejected); the best that keeps behaviour is 205 (a contrived inline helper); dropped. |
| `Text_DrawSmallNumberAligned` | LINK | 0x78 | 79.9% (2026-10-19, 5 attempts; skipped) | not re-attempted: strength reduction of the power-of-ten index (`docs/skipped.md`) |
| `Link_Receive` | LINK | 0x16C | 88.0% (3 attempts) | the receive side of the link exchange. Found: the IWRAM routine is called through a local function pointer (`bl _call_via_r0`, a cast-and-call gives `bl gUnk_03004210`), the received flags are a `u8 ready[4]` (stack order), the checksum a `u32` compared as `(s16)sum == -1`. Left: the registers of the player loop (the original keeps `&ready`/the fill temp in r10/r9, `i` in r4 and spills `i + 1`; agbcc puts `i` in r5 and `&ready` on the stack) |
| `Vs_ResultsMenuItem` | LINK | 0x2A8 | 99.2% (round 2, 4 attempts) | Vs_RunResults's first results menu (struct Unk08011FF8: ranks, times and numbers per player at +0x1C/+0x20/+0x24). `tile = gUnk_080C4868[r]; tile \|= 0xB000;` gives the original's SImode OR. Left: *p* and `&gTextCursor` swap r4/r5 (and the palette / cursor address in the row loop): global-alloc priority, `-dg`: the cursor address (8 refs over 120 insns, 3 * 8 / 120 = 0.200) beats *p* (6 refs over 63, 2 * 6 / 63 = 0.190) for r4; `switch` for the single item does not change it Permuter session (recreated at 98.9%, score 315 -> 180 in ~1 h, then 3 h from that output -> 100): the best outputs are contrived but equivalent (`0x40000000` through a local, the header sprites through separate variables); not 0. |
| `Vs_ResultsMenuItem2` | LINK | 0x378 | 99.8% (round 2, ~11 compiles + 20 min of permuter) | the twin of `Vs_ResultsMenuItem` (items 0x101/0x102, Vs_DrawNumberRight). `tile = gUnk_080C4868[r]; tile \|= 0xB000; ((u16 *)spr)[2] = tile;` fixes the HImode copy of 0xB000; the palette as `?:` (14/15) fixed the second palette. Left: the first rank sprite ORs into the table value's register (`orr r1, r0`) where the original ORs into the constant's (`orr r0, r1`), the second rank sprite's pointer is r2 instead of r1, and 0x1E4 is built in r0 instead of r3. The permuter's better scores are contrived (`spr->unk0 = K; spr->unk0 = x \| spr->unk0;`) Permuter session: an earlier draft had 0x4000 for item 0x102's `0x8000` (fixed); score 210 -> 115 in ~1 h, then 3 h from that output -> 80 (`tile = (t = table[r] + 4)`: equivalent, contrived); not 0. |
| `sub_08011890` | LINK | 0x348 | not attempted | (budget) a scrolling menu function (items 0x101-0x10B, unk10/unk12 scroll state) |
| `Vs_RunResults` | LINK | 0x940 | not attempted | size |
| `Vs_RunMenus` | LINK | 0x1280 | not attempted | size |
| `Bg_DrawLevelMinimap` | OTHER | 0x134 | 86.0% (5 attempts + 10 min of permuter) | the minimap from level data. agbcc's loop.c does not hoist `x * 2` out of the row loop (`-dL`: "savings 1 not desirable"), so *x* stays live in r12 and 0x3FF is not hoisted into r10 as in the original; `tiles + x + y * width` hoists `tiles + x * 2` instead. The permuter's better scores all change behaviour (they reuse `x` or `col` across rows) |
| `Debug_StageSelect` | OTHER | 0x238 | 87.9% (3 attempts) | main state 5 (debug stage select). The original derives the tiles address gLevelSpritePalettes - 0x6400 from the palette's register (`r10 = r9 + 0xFFFF9C00`, both hoisted out of the state loop); agbcc's CSE relates the two, but loop.c leaves the add in the loop ("not desirable": lifetime 1 against 244 insns) and it is folded back into a pool word `gLevelSpritePalettes-0x6400`. A small loop does hoist it (checked), so the original's loop body is smaller or its register needs differ; a zero constant agbcc hoists (23 uses) takes a register as well |

## Converted before the final C pass

| Function | Group | Size | When | Attempts | Notes |
| --- | --- | ---: | --- | ---: | --- |
| `Menu_StepValueReversed`, `Vs_DrawSpinner`, `Effect_UpdateTimeUpBanner`, `Vs_WaitForConsoles`, `Vs_CheckMenuLink`, `Vs_CheckLevelLink`, `ModeMenu_DrawMultiBootLabel`, `Link_Send`, `Effect_UpdateLinkEndBanner` | LINK | | 2026-10-20 (first session) | | |
| `Menu_StepValueBy10` | OTHER | 0x7C | 2026-10-20 (first session) | 1 | |
| `Debug_MultiBootTestItem` | OTHER | 0x108 | final push | 3 | main state 6's menu function; `switch`es for the item and probe_count |
| `Debug_MultiBootTest` | OTHER | 0x194 | final push | 2 | main state 6 (MultiBoot send test); its menu is a union with a `u16 results[16]` log (the original's 0x2C-byte frame) |
| `Debug_StageSelectItem` | OTHER | 0x1F8 | final push | 4 | main state 5's menu function; the cases in the original's order (0x100, 0x101, 1, 0x102, 2, ...) |
| `sub_08011614` | LINK | 0xEC | final push | 5 | `if / else` for the palette (the related `subs r2, #0x20`), `((u16 *)spr)[2] = K`, one block-scoped `spr` per sprite |
| `Effect_UpdateLinkCountdown` | LINK | 0x118 | final push | 2 | `x` a variable (the OR'd first word), block-scoped `spr` |
| `sub_08011700` | LINK | 0x190 | final push | 2 | `(u16)((palette << 12) \| 0x800) \| tile`: the cast stops the reassociation of 0x800 |
| `Random_Next2` | DEAD | 0x1C | final push | 1 | |
| `Sound_PlaySongOrChange` | DEAD | 0x30 | final push | 1 | |
| `Main_BlockCopy` | DEAD | 0x40 | final push | 2 | |
| `Menu_MoveSelect` | DEAD | 0x44 | final push | 2 | test `p->current` (not the saved `old`): CSE reuses `old` and the increment uses the fresh `ldrb`; committed with `Menu_MoveLeftRight` (9efc4c2) |
| `Menu_StepValueUpDown` | DEAD | 0x44 | final push | 1 | |
| `Menu_StepValueLR` | DEAD | 0x48 | final push | 1 | committed with `Menu_StepValueUpDown` (7d3b970) |
| `Decompress_RLE` | DEAD | 0x54 | final push | 2 | the run's byte read inside the loop (`*dst++ = *src`) and `src++` after it |
| `Menu_MoveLeftRight` | DEAD | 0x58 | final push | 2 | as `Menu_MoveSelect` |
| `Sprite_SetAffineRotScale` | DEAD | 0x64 | final push | 2 | `dir` an `int` narrowed in the index |
| `Menu_RunLeftRightWrap`, `Menu_RunLeftRight`, `Menu_RunSelect` | DEAD | 0x68 each | final push | 1 each | `Menu_RunWrap` with another key handler |
| `Bonus_DrawColorIcon`, `Bonus_DrawTypeIcon` | DEAD | 0x94, 0x74 | final push | 1 each | |
| `Effect_UpdateLinkResultBanner` | LINK | 0x220 | round 2 | 2 | a banner task with an icon pair per waiting player; block-scoped sprites, a `switch` with `goto next`/`goto draw` (case 2 as `if (unkC++ <= 0x78) goto draw; goto next;`), gGameState grown to its 0x40 bytes (`unk3C[4]`) |
| `Hud_DrawLinkProgress` | LINK | 0x11C | round 2 | 4 | the affine set and the rest of the Helirin's first word in separate statements (`set = unk3C << 25; attr = (x part) \| K; unk0 = set \| attr;`); `(s8)unkC1 * 2 + 0x50` |
| `Vs_PlayerSettingsItem` | LINK | 0x420 | round 2 | 5 | its menu as its own struct (`u16 settings[4]` at +0xC, read freshly per case), empty `case 0x101: case 0x102:` (the switch tree), `s32 ready` (asr), the palette destination as `0x05000000 + (i + 5) * 0x20`, `(u16)(palette \| 0x400) \| table[i]`, the arrows' colour as `if / else` stores to gTextCursor.palette (re-read per call); gGameState `unk34[4]` |
| `Sprite_DebugTileGrid` | DEAD | 0xB8 | round 2 | 1 | the C of `tests/compiler_evidence/game/Sprite_DebugTileGrid.c` |
| `Text_DrawSmallNumber5` | DEAD | 0xC4 | permuter | 5 + permuter | `**&p = pad;` for the tens digit's padding: taking p's address stops gcse's PRE of `p + 1` |
| `Text_DrawSmallNumber3` | DEAD | 0x6C | permuter | 1 | `Text_DrawSmallNumber5`'s fix |
| `Pause_LinkMenuItem` | LINK | 0x2D4 | permuter | 6 + permuter | the cursor's y read through `attr` (set again before every sprite uses it): sharing the variable gives idx r5 and the cursor r4 as in the original, a temporary of its own does not |

Mode 4 is entered from the mode menu (`ModeMenu_Run`) only when a link partner answers;
`kurutest` runs one console per ROM, so none of the LINK code can be tested here. A DEAD
function cannot affect gameplay: no code runs it.

## The OTHER functions

- **`Bg_DrawLevelMinimap`** draws a minimap from a level's decompressed level data. The Adventure map
  (`MapScreen_Run`), Challenge (`Challenge_Run`), Practice (`Practice_Run`) and the linked
  game call it only for a level whose minimap layer (`layers[3]`) is empty and whose level
  data (`layers[0]`) is not. No level in the ROM is like that: in `gWorldEntries` every
  level entry of the 13 worlds has a minimap (checked: the first 12 entries of each world,
  58 of the Challenge world; in worlds 1-10 the fourth entry, after the three levels, is
  empty in both). Reaching it needs other level data, which no input can give.
- **`Debug_StageSelect` / `Debug_MultiBootTest`** are AgbMain's state functions 5 and 6
  (`gMainStates[5]`, `[6]`), screens with their own menus (`Debug_StageSelectItem`, which calls
  `Menu_StepValueBy10`; `Debug_MultiBootTestItem`) that leave to state 1 on B. No code sets `gMain.state` to 5
  or 6: every store to `gMain + 0x0A` in the ROM (all functions' asm) writes 0, 1, 2, 3 or 4,
  except the two in these functions themselves. They are leftovers that only a RAM write of
  `gMain.state` could open; `kurutest` has no way to
  poke RAM, so no input can reach them either. What they are (a debug stage select with a
  recorder and demo player; a MultiBoot send test) and how to open them in mGBA:
  [`unused_content.md`](unused_content.md).

None of the OTHER functions deserves a new test input: none is reachable by key presses.
