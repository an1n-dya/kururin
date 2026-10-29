# kururin

A decompilation of **Kuru Kuru Kururin** (Game Boy Advance, Europe, `AKRP`): C source that
compiles, with the original SDK's compiler (agbcc), into a ROM that **plays identically** to the
original cartridge. It is a *functional* decompilation: the game's C does not have to produce
the same bytes, but it must behave the same in every case, and an automated test plays the
original and the rebuilt ROM side by side to prove it on recorded play. The SDK libraries
(sound, MultiBoot, serial, save) are kept byte-identical to the original.

> **Not affiliated with Nintendo. No game data is included.** This repository holds source
> code, tools and tests only: no ROM, no assembly or data generated from it, no graphics, sound
> or text. To build or test anything you need **your own** dump of *Kuru Kuru Kururin*
> (Europe, `AKRP`), SHA1 `6025ef597db2684ae064ffe22b4fd1d37941f887`. Please do not post ROMs,
> generated assembly or game assets in issues or pull requests.

New here? Read **[docs/GUIDE.md](docs/GUIDE.md)**, a tour of the game's code: the main loop,
the Helirin's physics, collision, levels, game modes, graphics, sound and save data.

## Status

| | |
| --- | --- |
| Game functions in C | **285 of 285** (34 modules, all named except 3) |
| SDK library functions in C (m4a, MultiBoot, SIO, SRAM) | **71 of 71**, all byte-identical |
| All C-module functions | **356 of 356** |
| Byte-identical to the original (objdiff 100%) | 191 of the 356 (game 120, libraries 71) |
| Still asm | no game function. The last 16, none of them run by the tests (7 unreachable, 7 link-cable only, 2 debug/fallback), became C on 2026-10-21: 1 byte-identical, 15 functional C reviewed path by path against the asm, as the maintainer decided for them ([docs/audit.md](docs/audit.md), final C pass; [docs/remaining.md](docs/remaining.md)) |
| Test coverage | 304 of 427 functions executed, 67.9% of the instruction bytes ([docs/coverage.md](docs/coverage.md)) |
| `make test` | 14 inputs (2 TASVideos movies, 12 scripts), ~168 000 frames, all identical |

Besides those 356 functions, the ROM has 47 hand-written asm functions (crt0, the m4a mixer
`m4a_1.s`, the BIOS call stubs, and two SDK functions that were inline asm) and 24 functions
linked unchanged from agbcc's libgcc and libc.

## Build and test from a fresh clone

You need an x86-64 Linux with Python 3.11 or newer (`setup.sh` installs the system packages it
needs with apt-get, dnf or pacman; tested from a fresh clone on Ubuntu 24.04, Fedora 44 and Arch,
on Debian 12 with `--no-ghidra` (it packages no JDK 21) and on Ubuntu 22.04 after `apt install
python3.11 python3.11-venv`) and your own dump of the game (it is not in the repository).

```sh
git clone https://github.com/an1n-dya/kururin.git && cd kururin
cp /path/to/your/dump.gba baserom.gba     # your own dump: it is not in the repository
sha1sum baserom.gba                       # must be 6025ef597db2684ae064ffe22b4fd1d37941f887
./setup.sh                                # or ./setup.sh --no-ghidra (skips the ~450 MB Ghidra download)
export PATH="$PWD/tools/bin:$PATH"
```

`setup.sh` installs everything into `tools/` and `.venv/` (gitignored; a few system packages:
arm-none-eabi binutils and cpp, cmake, a JDK for Ghidra), with every download pinned: pret's
**agbcc**, Luvdis, m2c, decomp-permuter, objdiff-cli, asmlift, **mGBA** (libmgba, for the tests),
Ghidra and REA (optional, for exploring), and the research references
[KuruTools](https://github.com/E-Sh4rk/KuruTools) and
[KururinTAS](https://github.com/E-Sh4rk/KururinTAS). It ends by building the ROM and running
the tests. It is idempotent: run it again in any new checkout.

Then:

```sh
make rom          # build kururin.gba (incremental)
make test         # THE check: rebuild from scratch, play both ROMs side by side (~1.5 min)
make shift-test   # the same with all code and data moved by 0x100 bytes
make compare      # SHA1 against the original (informational: the game's C is not byte-identical)
```

| ROM | |
| --- | --- |
| Game | Kurukuru Kururin (Europe), No-Intro; game code `AKRP`, maker `01`, revision 0 |
| `baserom.gba` | 4 MiB, SHA1 `6025ef597db2684ae064ffe22b4fd1d37941f887`, CRC32 `c4dab956` |
| `kururin.gba` today | SHA1 `d1fbc47f3801b2a4a701e083fcd89b89a40c0c9f` (differs from the original by design) |

### What `make test` checks

`tests/gameplay/kurutest.c` runs `baserom.gba` and the rebuilt `kururin.gba` in mGBA with the
same keys every frame, from recorded inputs in `tests/gameplay/inputs/` (two TASVideos movies and
scripted play: menus, every mode, Training, all worlds, the ending, Make Up, save files that
`tools/make_save.py` builds from your ROM and the descriptions in `tests/gameplay/saves/`). After every frame it compares the game's state in RAM (the Helirins, the
main loop, the game state, the camera, ...), the save data and what the game writes to the
sound hardware (the register writes, the PCM samples, m4a's buffer); every fourth frame the
screen. The first difference fails, with screenshots of both. Code that no input reaches is not
tested (see coverage); such code must byte-match instead (15 such functions that did not, the
last ones, are C reviewed path by path against the asm by the maintainer's decision: docs/audit.md).
The inputs and the harness are locked (`tests/check_lock.py`): a change to them is its own pull
request, and the maintainers re-lock.

## PC port (Linux and Windows)

`make -C port` (or `make -f port/Makefile` from here) builds `port/kururin_pc`, the same C
compiled for a 32-bit Linux PC with SDL2, and `make -C port windows` builds
`port/kururin_pc.exe` for 32-bit Windows with mingw-w64
([docs/port.md](docs/port.md): the design and its status). It loads your own `baserom.gba` at
run time (SHA1 checked); nothing from the ROM is in the repository or the binary. The game runs
in a window (3x, keyboard: arrows, Z = A, X = B, A/S = L/R, Enter = Start, Backspace = Select,
or a gamepad; F11 or Alt+Enter for fullscreen; it pauses when the window loses the focus) with
its sound, and saves to `kururin.sav` next to the executable; `kururin.ini` next to it holds
the settings and the key and button mapping. Headless runs serve the tests: its RAM matches the
original frame by frame on all 14 test inputs (`make -C port test` runs two of them,
`python3 port/test/port_test.py --all` every one), and so does its screen,
every frame, except 51 frames where the original draws while it loads (`make -C port
test-video`), and m4a's sound RAM and PCM buffer, byte for byte (`make -C port test-audio`);
the Windows build passes the same tests under Wine (`make -C port test-wine`,
`test-video-wine`, `test-audio-wine`). It needs `gcc-multilib` and `libsdl2-dev:i386` (Fedora:
`glibc-devel.i686`, `libatomic.i686`, `sdl2-compat-devel.i686`; Arch packages no 32-bit SDL2;
Windows: `mingw-w64`; Wine for its tests); the GBA build is unaffected (the port's few hooks in `src/`
are `#ifdef PLATFORM_PC`).

**Downloads**: prebuilt packages are attached to the repository's
[**GitHub Releases**](https://github.com/an1n-dya/kururin/releases) (each checked to hold no ROM
data). They contain no game data either: you supply your own `baserom.gba`.

- `kururin_pc_win32.zip` (`make -C port dist-windows`: the exe, SDL2.dll, a README and the
  licences). Unzip it, put your own `baserom.gba` next to `kururin_pc.exe` and start it.
- `kururin_pc_linux_x86.tar.gz` (`port/tools/build_linux_release.sh`: `make -C port
  dist-linux` on Ubuntu 22.04, so it runs with glibc 2.34 or newer): the 32-bit program, SDL2
  next to it, the launcher `kururin.sh` and a README. Unpack it (`tar -xzf`), put your own
  `baserom.gba` in `kururin_pc/` and run `./kururin.sh`: it checks the 32-bit system libraries
  and prints the command that installs any missing ones (Debian/Ubuntu, Fedora, Arch).

Each README.txt lists the controls and settings.

## Layout

| Path | What |
| --- | --- |
| `src/` | the C: 34 game modules (`main.c`, `helirin.c`, `collision.c`, `play.c`, ...; [docs/modules.md](docs/modules.md)) and the SDK libraries (`m4a.c`, `multiboot.c`, `sio.c`, `agb_sram.c`); a function not yet in C is an `INCLUDE_ASM` line |
| `include/` | headers: `game.h` (shared structs and data), `variables.h` (the game state, the Helirin, the camera, save files), one per module |
| `kururin.toml` | the modules in link order with their compiler and flags, every name with its evidence, the data symbols |
| `config/` | `functions.txt` (every function's address and size) and `data_pointers.txt` (every pointer in the data), generated and committed |
| `scripts/` | the asm/data generator, function discovery, pointer finding, scoring, `verify.py`, coverage, `check_rom_content.py` (no ROM content in the repository) |
| `tests/gameplay/` | the equivalence test: harness, inputs, the save images' descriptions, the watched RAM |
| `tests/compiler_evidence/` | C that pins each module's compiler setting |
| `port/` | the PC port: its platform layer and renderer (`port/src`), the address-table generator, the port test, the pixel test |
| `tools/next.py`, `tools/make_save.py` | the next decompilation targets; the save image builder |
| `docs/` | the documentation (below) |
| `LICENSE`, `LICENSES/`, `NOTICE.md` | the licences, their exceptions and the attributions |
| `asm/`, `data/`, `ldscript.txt`, `modules.mk`, `objdiff.json` | **generated** from the ROM by `scripts/generate_asm.py`: never committed |

## Documentation

| Document | What |
| --- | --- |
| [docs/GUIDE.md](docs/GUIDE.md) | **start here**: a tour of the game's code |
| [docs/port.md](docs/port.md) | the PC port (SDL2): design, status, what is stubbed |
| [CLAUDE.md](CLAUDE.md) | the project's rules, workflow and tools (written for the AI agents that do most of the work, and for people) |
| [docs/modules.md](docs/modules.md) | the 34 game modules |
| [docs/names.md](docs/names.md) | every name given to a placeholder, with its evidence |
| [docs/save_format.md](docs/save_format.md) | the SRAM save format |
| [docs/unused_content.md](docs/unused_content.md) | the hidden debug stage select and MultiBoot test, and how to open them |
| [docs/coverage.md](docs/coverage.md) | what the test inputs execute |
| [docs/remaining.md](docs/remaining.md) | the last functions that were asm, and what blocked their matches |
| [docs/audit.md](docs/audit.md) | decompiled functions with paths the tests do not reach |
| [docs/calibration.md](docs/calibration.md) | agbcc code generation, with worked matching examples |
| [docs/progress.md](docs/progress.md), [docs/skipped.md](docs/skipped.md) | the history of the decompilation |

## Contributing

Read [CLAUDE.md](CLAUDE.md): the project's rules, workflow and tools (written for the AI
agents that did most of the work, and for people). Every change must pass `make test`, and
`python3 scripts/check_rom_content.py` must report nothing before you push.

## Licence

| Path | Licence |
| --- | --- |
| `src/`, `include/` (the decompiled C) | [CC0-1.0](LICENSES/CC0-1.0.txt) for this project's contributions, except the pret-derived SDK files `src/m4a.c`, `src/multiboot.c`, `include/m4a.h`, `include/gba/multiboot.h` (no licence) |
| everything else: `scripts/`, `tools/`, `tests/`, `port/`, the build files, the docs | [GPL-3.0-only](LICENSE), except `port/src/video.c` ([MPL-2.0](port/LICENSE-MPL-2.0.txt), from mGBA) and the TASVideos movies (CC BY 2.0) |

[NOTICE.md](NOTICE.md) has the details and the attributions. None of it grants any right in the
game, which is (c) Nintendo / Eighting.

## Credits

- [KururinTAS](https://github.com/E-Sh4rk/KururinTAS) by E-Sh4rk (memory watch, physics notes)
  and [KuruTools](https://github.com/E-Sh4rk/KuruTools) by E-Sh4rk (level and data formats):
  names and formats come from the code itself and from these.
- [pret](https://github.com/pret): the toolchain, [agbcc](https://github.com/pret/agbcc); the m4a
  and MultiBoot C started from pret's pokeemerald/pokeruby versions of the SDK sources.
- [mGBA](https://mgba.io): the emulator the tests run on; the PC port's renderer follows its
  software renderer.
- [TASVideos](https://tasvideos.org): movies [#6116M](https://tasvideos.org/6116M) by mohoc and
  [#3933M](https://tasvideos.org/3933M) by mohoc and E-Sh4rk, used as test inputs (CC BY 2.0).

Project by an1n-dya.
