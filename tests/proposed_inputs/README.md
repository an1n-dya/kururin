# Proposed gameplay inputs (not locked)

Drafts from the audit of partly covered C functions (`docs/audit.md`). Nothing here is run by
`make test` and nothing here is locked. `see_map.txt`, the first draft, was approved and is now
locked as `tests/gameplay/inputs/see_map.txt`.

## Running one

`make test TEST_ARGS=<name>` cannot run them: `tests/gameplay/run.py` (locked) only takes inputs
from `tests/gameplay/inputs/`, and copying a file there trips the lock. Run one by hand after a
`make test` (which builds `kururin.gba` and `build/test/kurutest`), with `NAME` its file name
without `.txt`:

```sh
NAME=last_land_levels
python3 -c "import sys, pathlib; sys.path.insert(0, 'tests/gameplay'); import run; \
  print('\n'.join(run.convert_script(pathlib.Path('tests/proposed_inputs/$NAME.txt').resolve())))" \
  > build/test/$NAME.inp
.venv/bin/python3 scripts/rom_map.py kururin.elf > build/test/rom_map.txt
build/test/kurutest --watch tests/gameplay/watch.txt --inputs build/test/$NAME.inp \
  --name $NAME --dump-dir build/test --rom-map build/test/rom_map.txt baserom.gba kururin.gba
```

It must end with `ok   <name>: <n> frames identical`.

## `last_land_levels.txt`: a level start at the edge of VBlank

`all_unlocked.sav`: Adventure, The Last Land 3, 2 and 1 in a row (enter from the map, move, pause,
Quit), 3 796 frames. Entering The Last Land 1 after The Last Land 2 ends the level-start frame a
few cycles before VBlank; with the former (90%) `LevelMap_Read` the rebuilt ROM missed it and
stayed a frame behind from frame 2 713 on (docs/audit.md, Timing). `save_adventure.txt` enters
level 3 of each world only. Proposed for locking: it guards the cycle-identical `LevelMap_Read`.

## `timer_cap.txt`: a manual check

Start the clock in Training 1 and wait an hour (218 402 frames): the level timer stops at
0x34BBF, the clamp in `Play_CheckLevelEnd`. It holds its `expect`s on the original and the
rebuilt ROM is identical on it, but it takes about 5 minutes per ROM, too slow for every
`make test`; run it as above with `NAME=timer_cap`.

## What no draft reaches, and why (details per function in `docs/audit.md`)

The save-file inputs (2026-10-20) now reach most of what the first audit listed here: the later
worlds' background layers (`Camera_Update`), the map objects 0xF4/0xF6 and odd-angle 0xF7
(`LevelMap_StartMarkedObject`), the bonus screen's modes (`Collection_Run`), Practice (mode 5)
and its demo (`GAME_FLAG_DEMO`, set by its See Demo). Still out of reach (`docs/coverage.md`):

- **The linked game** (`gGameState.mode == 4`: most of the uncovered code of `Play_RunLevel`
  and `Play_CheckLevelEnd`): needs a second console on a link cable; kurutest runs one console.
- **The debug stage select** (main state 5, mode 6: `docs/unused_content.md`): nothing in the
  game enters it; only a RAM write does, and inputs cannot write RAM.
- **Katakana text** in `Text_DrawSmall` / `Text_Draw`: no screen of this European ROM was found
  to draw such bytes; the name entry offers ASCII only.
