# Notices and licences

This project is not affiliated with or endorsed by Nintendo or Eighting. Kuru Kuru Kururin is
(c) Nintendo / Eighting. The repository holds **no game data**: no ROM, no assembly or data
generated from it, no graphics, sound or text beyond a few short menu labels quoted as
evidence for names (`scripts/check_rom_content.py` checks every tracked file). To build or test
anything you need your own dump of the game.

The C in `src/` and `include/` re-expresses the game's program and the Nintendo SDK libraries
it was built with. The licences below cover **only what this project's contributors added**;
nothing in this repository grants any right in the game, in Nintendo's SDK, or in any other
third party's work.

## Licences by path

| Path | Licence |
| --- | --- |
| `src/`, `include/` (the game's code, `agb_sram.c`, `sio.c`, the GBA register headers) | **CC0-1.0** ([`LICENSES/CC0-1.0.txt`](LICENSES/CC0-1.0.txt)), except the files listed under "pret" below. Each file says so in its first line |
| `tests/compiler_evidence/` | the same terms as the `src/` code each file copies: CC0-1.0 for `game/` and `agb_sram/`; no licence for `m4a/` and `multiboot/` (pret, below) |
| everything else: `scripts/`, `tools/next.py`, `tools/make_save.py`, `tests/`, `port/`, `setup.sh`, the `Makefile`, `docs/`, `config/`, `kururin.toml` and the other build files | **GPL-3.0-only** ([`LICENSE`](LICENSE)), except `port/src/video.c` and the TASVideos movies (below) |

Copyright (C) 2026 an1n-dya and contributors, for the GPL-3.0 parts.

## Exceptions

### pret's SDK reconstructions: no licence

`src/m4a.c`, `include/m4a.h` (MusicPlayer2000, from pret's `m4a.c` and `m4a_internal.h`),
`src/multiboot.c`, `include/gba/multiboot.h` (the MultiBoot library) and their copies in
`tests/compiler_evidence/m4a/` and `tests/compiler_evidence/multiboot/` started from pret's
reconstructions of Nintendo SDK code in [pokeemerald](https://github.com/pret/pokeemerald) and
[pokeruby](https://github.com/pret/pokeruby), adapted to this ROM's older SDK revision. Those
projects carry no licence and the code is Nintendo's, so **no licence is claimed or granted**
for these files. The register names and macros of `include/gba/io_reg.h`, `macro.h` and
`types.h` follow pret's headers' style; the files were written for this project.

### mGBA: `port/src/video.c` (MPL-2.0)

The PC port's renderer is derived from [mGBA](https://mgba.io)'s software renderer (mGBA
0.10.5, Copyright (c) 2013-2015 Jeffrey Pfau) and is under the **Mozilla Public License 2.0**
([`port/LICENSE-MPL-2.0.txt`](port/LICENSE-MPL-2.0.txt)); the file carries the MPL notice.
[`port/NOTICE.md`](port/NOTICE.md) has the details, and what the port's release packages ship
(SDL2 under zlib, the mingw-w64 runtime, libgcc). The rest of the port is GPL-3.0-only;
`port/src/m4a_1.c` is a C transcription of the game's own `m4a_1.s` (Nintendo's sound
library), and like `src/` its licence covers only this project's contribution.

### TASVideos movies (CC BY 2.0)

`tests/gameplay/inputs/tasvideos_6116M_100.bk2` is [TASVideos movie #6116M](https://tasvideos.org/6116M)
(Kuru Kuru Kururin, 100%, BizHawk 2.9.1) by **mohoc**, and
`tests/gameplay/inputs/tasvideos_3933M_any.bk2` is [TASVideos movie #3933M](https://tasvideos.org/3933M)
(any%, BizHawk 2.3.1) by **mohoc and E-Sh4rk**. Both are published by TASVideos under the
[Creative Commons Attribution 2.0](https://creativecommons.org/licenses/by/2.0/) licence
([site licence](https://tasvideos.org/SiteLicense)) and are included unmodified.
`tests/gameplay/inputs/save_ending.txt` and `save_challenge.txt` splice frames of #6116M into
scripted inputs (each says which); those frames are under the same licence.

### The toolchain and linked libraries

Nothing of the toolchain is in the repository: `setup.sh` fetches and builds it, pinned.
[pret's agbcc](https://github.com/pret/agbcc) is GPL-2.0 (GCC); the members of its libgcc
(GPL with the GCC Runtime Library Exception) and libc (newlib, BSD-style licences) that the
build links into your `kururin.gba` stay under their own licences, as do mGBA (MPL-2.0, linked
into the local test harness), Luvdis, m2c, decomp-permuter, objdiff, asmlift, REA, Ghidra and
the other tools `setup.sh` installs. Your rebuilt ROM is made from your own dump; do not
distribute it.

## Credits

- [KururinTAS](https://github.com/E-Sh4rk/KururinTAS) by E-Sh4rk (MIT): the memory watch and
  physics notes many names come from.
- [KuruTools](https://github.com/E-Sh4rk/KuruTools) by E-Sh4rk: the level and data formats.
  Its terms ask for credit: thank you.
- [pret](https://github.com/pret): agbcc, and the m4a and MultiBoot reconstructions.
- [mGBA](https://mgba.io) by Jeffrey Pfau and contributors: the emulator the tests run on, and
  the PC port's renderer.
- [TASVideos](https://tasvideos.org), mohoc and E-Sh4rk: the movies above.
