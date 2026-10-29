# Save format (SRAM)

What Kuru Kuru Kururin (Europe) keeps in its cartridge SRAM, worked out from the decompiled
save code. Every field names the function (and its address) that is the evidence. Offsets are
hexadecimal. `tools/make_save.py` builds images in this format from the descriptions in
`tests/gameplay/saves/*.toml` and the ROM (the tests do, at test time: the images are not
committed), and `tools/make_save.py --show <file.sav>` decodes one (or an SRAM dump: kurutest
`--dump-save`).

Level index (`n` below), used by every per-level array: `world == 0 ? level : world * 3 +
level + 2` (`Play_MainState` 0x08008200, `Results_EnterRecord` 0x0800B604). With KuruTools' world list
(`KuruRomExtractor/Levels.cs`: 5 Training levels, 11 worlds of 3, 55 Challenge courses):

| n | Levels |
| --- | --- |
| 0-4 | Training 1-5 |
| 5-34 | worlds 1-10 (Grass, Ocean, Jungle, Cake, Cave, Cloud, Star, Ice, Machine, Ghost), 3 levels each |
| 35-37 | world 11, the last world ("The Last Land") |
| 38-92 | Challenge courses 1-55 (world 12; `Challenge_Run` 0x0800FAAC: level = row * 5 + column, `Challenge_MenuItem` 0x0800FDC8: index 0x26 + level) |

## SRAM layout

The game uses the SDK SRAM library (`ReadSram` 0x0801C5F4, `WriteSram`, `VerifySram`), 32 KiB
at 0x0E000000. Only the first 0x44C0 bytes are used; the rest stays as it was (0xFF on a blank
cartridge, and in mGBA).

| Offset | Size | Field | Evidence |
| --- | --- | --- | --- |
| 0x0000 | 0x10 | signature, `"KURURIN 1.01"` + NULs (`gSaveSignature`); compared with `strcmp` | `Save_LoadAtBoot` 0x08005178 |
| 0x0010 | 2 | bank in use, 0 or 1 | `Save_LoadAtBoot`, `Save_Write` 0x080054E4 |
| 0x0012 | 2 x 2 | checksum of bank 0, bank 1: the 16-bit sum of the bank's 0x2254 bytes | `Save_LoadAtBoot`, `Save_Write` |
| 0x0016 | 2 | unused (never written; 0xFF) | `struct SramSave` in `src/save.c` |
| 0x0018 | 0x2254 | bank 0 | `Save_LoadAtBoot` (`SRAM_DATA`) |
| 0x226C | 0x2254 | bank 1 | |

**Loading** (`Save_LoadAtBoot`, at boot): with A+B+Select+Start held it asks whether to erase
everything. Otherwise: if the signature differs, it formats. If the bank number is 0 or 1, it
reads that bank and checks its checksum, then the other bank; the first good one is the
data. If neither is good (or the bank number is above 1) it shows an error until A and
formats. Formatting resets the five files (`Save_ResetFiles` 0x0800562C) and the records
(`Save_ResetRecords` 0x08005644), writes the signature and bank 0, and saves twice. Every path
ends with a save (`Save_Write`), so a good image comes back with the other bank in use and
both banks holding the same data.

**Saving** (`Save_Write`): writes the 0x2254 bytes to the bank *not* in use, verifies them,
writes and verifies that bank's checksum, then writes and verifies the bank number. A failed
verification leaves the old bank in use. Saves happen at boot, after each level's result
(`Results_Run` 0x0800AA0C), after a rank-up (`Results_CheckRankUp` 0x0800B000), after the name entry
(`Files_EnterName` 0x0800BC70), when the Make Up screen is left with a choice (`MakeUp_Run`
0x080133E4), after the bonus message (`Play_MainState`) and the ending (`Ending_MainState`).

## A bank (0x2254 bytes)

In RAM the bank is the EWRAM buffer `gSaveData`; `gSaveFiles` (files) and
`gSaveBank` (the whole bank) both point to it (`Game_Boot` 0x08004C24).

| Offset | Size | Field |
| --- | --- | --- |
| 0x0000 | 5 x 0x1F4 | save files 0-4 |
| 0x09C4 | 93 x 3 x 0x10 | the records of the 93 Normal levels (Adventure Normal and Challenge) |
| 0x1B34 | 38 x 3 x 0x10 | the records of the 38 Adventure levels in Easy mode |

## A save file (0x1F4 bytes, `struct SaveFile`)

The file menu (`Files_MainState` 0x0800B8A0) lists files 1, 2, 3, 4, 0 (`gFileMenuFiles`); file 0
is always named `GUEST` (`Save_ResetFile` 0x080055C0 copies `gTextGuest`), the others are
named in the name entry (`Files_EnterName`, up to 8 characters). Its sixth item, "Delete saved
data", is `Files_DeleteScreen`. `gGameState.file` is the file picked,
`gGameState.saveFile` points to it.

| Offset | Size | Field | Evidence |
| --- | --- | --- | --- |
| 0x000 | 2 | the file's number (0-4) | `Save_ResetFile` |
| 0x002 | 11 | name, NUL-terminated (bytes after the NUL keep the name entry's `_`s); `[0] == 0`: an empty file | `Save_ResetFile`, `Files_FileMenuItem` 0x0800BAA4, `Files_EnterName` |
| 0x00D | 1 | rank 0-3 (0 none, 1 Trainee, 2 Professor, 3 Master: KuruTools' TimesEditor names), drawn after the name; raised by `Results_CheckRankUp` while every best time of levels 5-34 and 38-87 beats `gLevelTimes[n][rank]` (Easy files: `gEasyLevelTimes` for 5-34) | `Results_CheckRankUp`, `Files_FileMenuItem` |
| 0x00E | 2 | unused | |
| 0x010 | 4 | flags (below) | |
| 0x014 | 4 | unlocks (below); reset value 0x20400 | `Save_ResetFile` |
| 0x018 | 1 | Helirin type chosen (0-14) | `MakeUp_MenuItem` 0x080135E8, `Helirin_Init` 0x08014594 |
| 0x019 | 1 | Helirin colour chosen (0-6) | `MakeUp_MenuItem`, `Helirin_Init` |
| 0x01A | 2 | items worn: bits 0-9, one per collected item | `MakeUp_MenuItem`, `MakeUp_StartWornItems` 0x080133A8 |
| 0x01C | 4 | bonus taken, bit `n - 5` per level 5-34 (each of these 30 levels has one bonus) | `Results_Run`, `LevelMap_Read` 0x0800580C |
| 0x020 | 93 x 4 | best time per level, in frames; 216000 (60 minutes) when never cleared | `Save_ResetFile`, `Results_EnterRecord`, `MapScreen_LevelMenuItem` |
| 0x194 | 0x60 | mark per level (only the first 93 are used): 0 not cleared, 1 Training skipped (the story's second choice sets 0-4 to 1), 2 cleared with another Helirin (not the file's own: `helirin->unk5` not 0, or not 5 in Easy), 3 cleared, 4 cleared without a bump | `Results_EnterRecord`, `Script_Run` 0x0800E664 |

Flags (offset 0x010):

| Bit | Meaning | Set by | Read by |
| --- | --- | --- | --- |
| 0 | Adventure started (the story was shown) | `Play_MainState` | `Play_MainState`, the file menu draws the progress only with it (`Files_FileMenuItem`) |
| 1 | the last world is open: every level 5-34 has mark 4 | `Results_EnterRecord` | Adventure map (`MapScreen_Run` 0x0800D690: up to level 37 instead of 34), file menu |
| 2 | the 11th Challenge row (courses 51-55) is open: every course 1-50 has mark 4 | `Results_EnterRecord` | `Challenge_Run` (11 rows instead of 10) |
| 3 | Normal or Easy has been chosen (asked once) | `ModeMenu_Run` 0x0800C75C | `ModeMenu_Run` |
| 4 | Easy mode (with bit 3) | `ModeMenu_Run` | records table, times, marks, file menu text |
| 5 | the ending has been seen | `Ending_MainState` 0x080076FC | `Play_MainState` (no more bonus message or ending) |
| 6 | the "not all bonuses" message after world 10 level 3 was shown | `Play_MainState` | `Play_MainState` |

Unlocks (offset 0x014), one bit per bonus; a level's map names the bit its bonus gives
(`LevelMap_Read`: map number 1-10 -> bits 0-9, 11-19 -> bits 11-19, 20 on -> bits 17 on), and
`Results_Run` ORs it in (`gCamera.unk18`) when the bonus was taken:

| Bits | What | Chosen in Make Up (`MakeUp_MenuItem`) as |
| --- | --- | --- |
| 0-9 | the 10 items (one per world 1-10); all ten after level 3 of worlds 1-10 start the ending (`Play_MainState`) | items worn (0x01A) |
| 10-16 | the 7 colours; bit 10 (colour 0) is set from the start | colour (0x019) |
| 17-31 | the 15 Helirin types, 3 rows of 5; bit 17 (type 0) is set from the start | type (0x018) |

30 bonuses for 30 levels: 10 items, 6 colours, 14 types.

## A record (0x10 bytes, `struct LevelRecord`)

Three per level, best first.

| Offset | Size | Field |
| --- | --- | --- |
| 0x0 | 4 | time in frames (216000: empty) |
| 0x4 | 11 | name, NUL-terminated |
| 0xF | 1 | the holder's rank (the file's rank at the time; the built-in records: 1, 2, 3) |

Reset (`Save_ResetRecords`): every record 216000 frames named `"--------"` (`gTextNoRecordName`); then
for Normal levels 0-34 and 38-87 and Easy levels 0-34, the first two records are the level's
Professor and Trainee times (`gLevelTimes[n][1]`, `[n][0]`; Easy: `gEasyLevelTimes`), named
`"Hare"` (`gDefaultRecordNames[1]`, `[0]`) with rank 2 and 1. Levels 35-37 and 88-92 start empty.
When a file reaches rank 2, `Results_CheckRankUp` enters the Master time (`[n][2]`, `"Hare"`
`gDefaultRecordNames[2]`, rank 3) into the records of levels 0-34 (of its mode) and 38-87 where it
is not yet. After each level `Results_EnterRecord` puts the new time into the level's records, sorted
(the file's own entry moves up when it improves), and keeps the file's best time (0x020).

## What the menus show from it

- File menu (`Files_FileMenuItem`): the name and rank badge; with flag 0 the mode (bit 4) and the
  level after the last one with a mark among 0-37 (`Level_SetFromIndex`), as world name and level
  ("End" beyond the last world).
- Mode menu (`ModeMenu_Run`): Practice (mode 5, `Practice_Run`) needs a mark on Training 5
  (level 4); VS needs a link partner.
- Adventure map (`MapScreen_Run`): the levels up to the first without a mark (at most 34, or 37
  with flag 1).
- Challenge (`Challenge_Run`): all courses of 10 rows, 11 with flag 2; the marks and best times
  of the file, the course's three records.
- Make Up (`MakeUp_Run`): the unlocked types, colours and items.
