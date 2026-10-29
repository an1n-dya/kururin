# The PC port (SDL2)

A native build of the decompiled game for a PC: the game's C (`src/`) compiled with the host's
gcc, a small platform layer in `port/` standing in for the GBA hardware, the BIOS and the
hand-written asm, and the player's own `baserom.gba` loaded at run time for every byte of data.

This document is the design (written before the code, with the reasons; corrected where the
code showed otherwise) and the state of the work. Milestone 1 is a build that runs the game
headless with the same RAM as the original, frame by frame; graphics, sound and Windows come
later.

## Building and running

```sh
sudo apt install gcc-multilib libsdl2-dev:i386   # after: sudo dpkg --add-architecture i386 && sudo apt update
make -C port                # -> port/kururin_pc (or, from the root: make -f port/Makefile)
port/kururin_pc             # the game in a window, 3x; baserom.gba from the current directory
port/kururin_pc --rom path/to/baserom.gba --save my.sav --scale 4   # default save: kururin.sav next to the executable
port/kururin_pc --headless --frames 600 --dump - # the watched RAM after every frame, to stdout
port/kururin_pc --headless --frames 600 --shot 599 --shot-dir /tmp   # a screenshot (PPM)
port/kururin_pc --headless --frames 6000 --wav out.wav   # the sound, into a WAV file
port/kururin_pc --poke 700:0x03000DCA:5   # a byte written before a frame: here the debug stage select
make -C port test           # the port test: boot_attract and training1_moves against the original
python3 port/test/port_test.py --all              # every input of tests/gameplay/inputs
make -C port test-audio     # the audio test: m4a's RAM and PCM buffer against the original, 3 inputs
make -C port test-video     # the pixel test: 4 inputs; VIDEO_ARGS="--all -v" for all 14
make -C port survey         # which display features the original uses, on every input
make -C port sound-survey   # which sound hardware and m4a features the original uses
make -C port layouts        # struct layouts, agbcc vs gcc -m32
make -C port windows        # -> port/kururin_pc.exe + port/SDL2.dll (32-bit Windows; needs mingw-w64)
make -C port layouts-windows   # struct layouts, agbcc vs i686-w64-mingw32-gcc
make -C port dist-linux     # -> dist/kururin_pc_linux_x86.tar.gz ("The Linux package")
port/tools/build_linux_release.sh   # the same from a clean clone of HEAD, on Ubuntu 22.04 (Docker): the release
```

Keys: arrows, Z = A, X = B, A = L, S = R, Enter = Start, Backspace = Select, or a gamepad;
F11 or Alt+Enter switch fullscreen; all of it can be changed in `kururin.ini` next to the
executable ("Settings and controls"). The window shows
the GBA's 240x160 at 59.73 frames a second, scaled by a whole number (`--scale N`, default 3;
resizing the window keeps whole multiples, with black borders), with the sound through SDL
audio (`--mute`, `--volume N` 0-100, `--wav FILE`: "The sound output"). The
port test and the pixel test need the libmgba `./setup.sh` builds (`tools/mgba`), like `make
test`.

On Ubuntu 24.04 in the cloud container, installing the i386 packages re-pointed
`/usr/bin/python3` (3.13 -> 3.12), which the project's `.venv` links to, and `luvdis` stopped
importing; `./setup.sh` notices such a venv (made by another Python than it runs) and makes it
again.

## Constraints

- **The GBA build does not change.** `kururin.gba` keeps the SHA1 in README.md, `make test` and
  `make shift-test` pass. Port code lives in `port/`; the few changes to shared files are
  `#ifdef PLATFORM_PC` (or macros that expand to the same tokens without it), so agbcc
  preprocesses exactly what it did before.
- **No ROM content in the repository or the binary.** The port reads the player's
  `baserom.gba` at start-up and checks its SHA1 (`6025ef59...`); the binary holds only code
  compiled from `src/` and `port/`, and tables of *addresses* (where the original's symbols
  are), which are not ROM content.

## Design

### 1. A 32-bit build: the original's pointers and structs work unchanged

The game keeps pointers in RAM and in ROM data (4 bytes each) and lays its structs out for a
32-bit ARM. A 32-bit x86 build (`gcc -m32`) keeps `sizeof(void *) == 4`, little-endian, and
the same `int`/`short`/`char` sizes, so every struct in RAM and every table in the ROM is read
by the same C with the same layout. A 64-bit build would need every pointer in data rewritten.

agbcc pads every struct to a multiple of 4 (thumb.h: `STRUCTURE_SIZE_BOUNDARY 32`); gcc on x86
does not. `port/tools/check_layouts.py` compiles every struct of every module with both
compilers and compares sizes and field offsets: all 92 are the same (7 differ only in
alignment, which no layout here depends on), so no struct needs an attribute.

Compiler options chosen to keep the ARM semantics the C was written for:
`-funsigned-char` (ARM's `char` is unsigned), `-fwrapv` (signed overflow wraps, as the ARM
does), `-fno-strict-aliasing` (the game reads data through casts). Known semantic differences
that options cannot fix are listed under [Risks](#risks).

Linux first (`gcc -m32`, needs `gcc-multilib` and `libsdl2-dev:i386`); Windows later with the
32-bit mingw toolchain (`i686-w64-mingw32-gcc`) and `VirtualAlloc`.

### 2. The GBA memory map at its own addresses

The C uses the GBA's addresses directly: globals are absolute symbols (`kururin.toml
[symbols]`: `gMain` is 0x03000DC0), I/O registers are `*(vu16 *)0x04000000`, VRAM writes go to
0x06000000, ROM data is read at 0x08xxxxxx. The port reserves host memory at exactly those
addresses at start-up with `mmap(MAP_FIXED_NOREPLACE)`:

| Region | Address | Reserved |
| --- | --- | --- |
| unmapped (open bus) | 0x00010000 | up to 0x02000000, read-only, reads 0 (see below) |
| EWRAM | 0x02000000 | 256 KiB, mirrored up to 0x03000000 as on the GBA (one `memfd` mapped 64 times) |
| IWRAM | 0x03000000 | 32 KiB, mirrored up to 0x04000000 |
| I/O | 0x04000000 | 4 KiB (plain memory: see [hardware](#4-hardware-bios-and-asm-replaced)) |
| Palette | 0x05000000 | 1 KiB (a page) |
| VRAM | 0x06000000 | 96 KiB |
| OAM | 0x07000000 | 1 KiB (a page) |
| ROM | 0x08000000 | 32 MiB reserved; `baserom.gba` (4 MiB) read in, read-only after start-up |
| SRAM | 0x0E000000 | 64 KiB, filled with 0xFF (a blank cartridge), loaded from / saved to a `.sav` file |

The GBA reads *open bus* (the last prefetched instruction) at unmapped addresses; the game
does that once on the tested paths: `LevelIntro_Run` decompresses `gLevelIntroPictureTiles[11]`
for The Last Land, an entry past the table's end that holds 0x000C0004. The port maps that
space read-only so it reads zeros: the garbage the original decompresses goes to a heap block
in EWRAM and to OBJ VRAM that is not shown, so the watched RAM is the same either way.
Addresses below the kernel's `mmap_min_addr` (0x10000 by default: the BIOS's 16 KiB and more)
cannot be mapped; nothing on the tested paths reads there.

Why fixed addresses rather than a base offset: every one of those addresses is a constant in
the compiled C (literal addresses, absolute symbols) and in the data (the ROM's 3 546 pointers,
RAM pointers the game stores). Translating them would mean an access macro on every pointer
dereference in the game. At the original addresses the game's C runs unmodified, and RAM can be
compared with the original's byte for byte.

The executable must not overlap those ranges: the i386 default load address, 0x08048000, is
inside the ROM. The port is linked non-PIE at 0x10000000 (`-Wl,-Ttext-segment=0x10000000`);
shared libraries and the heap are placed by the kernel far above. `MAP_FIXED_NOREPLACE` fails
instead of silently replacing a mapping.

**Fallback** when an address cannot be reserved (something already mapped there, or a kernel
older than 4.17 without `MAP_FIXED_NOREPLACE`): the port first tries plain `mmap` with the
address as a hint and checks the result, then, if a region is still taken, re-executes itself
once with address-space randomisation off (`personality(ADDR_NO_RANDOMIZE)`), which moves
whatever was randomly placed there. If that fails too it stops with a message naming the region
and what holds it (`/proc/self/maps`). There is no relocating fallback (the addresses are baked
into the code, see above). On Windows the same layout comes from `VirtualAlloc(MEM_RESERVE |
MEM_COMMIT)` at each address, with the executable based above 0x10000000 (`--image-base`).

### 3. Function pointers in ROM data: patched once at load

The ROM's data holds 67 pointers to code (`config/data_pointers.txt`, kind `code`): AgbMain's
state table `gMainStates`, the interrupt table `gIntrTableInit`, m4a's jump table. The game calls
through them (`gMainStates[gMain.state]()`), and copies them into RAM (`gIntrTable`). On the PC
those GBA addresses point at Thumb code that the host cannot run.

**Decision: patch at load.** After reading `baserom.gba`, the port overwrites each of those 67
words in its ROM image with the address of the host function of the same name, then makes the
ROM read-only. The table (GBA address of the word, host function) is generated at build time
from `config/data_pointers.txt`, `config/functions.txt` and `kururin.toml [renames]`
(`port/tools/gen_port_tables.py`).

Why not resolve at call time: every indirect call in the game (state functions, tasks, menu
functions, interrupt handlers, m4a's jump table, about 30 call sites) would need a wrapper macro
that maps a GBA address to a host function, and every place that stores a function's address
(the C's own `task->func = Object_Spring`, which is already a host address) would have to agree
on which kind of address is in RAM. Patching keeps one rule: **a function pointer in RAM is
always a host address**, whether the C took the address itself or read it from a ROM table.
The cost is that 67 ROM words are not the original's bytes; the game never reads them as
anything but pointers (the evidence `find_pointers.py` used).

The reverse table (host function -> GBA address, every function of `config/functions.txt` the
port defines) is used when RAM is dumped for comparison: a watched word that holds a host
function address is written out as the original's GBA address, the same way kurutest maps
ROM addresses between two builds (`--rom-map`).

Data symbols (`gMain`, `gSongTable`, `gUnk_08xxxxxx`, ...) are absolute symbols at the
original's addresses: a generated linker script (`kururin.toml [symbols]`, the `gUnk_<address>`
labels, `[linker_constants]`), so the ROM data is read where `baserom.gba` was loaded.

### 4. Hardware, BIOS and asm replaced

| Original | Port |
| --- | --- |
| `crt0` (start vector, stacks) | `port/main.c`: `main()` maps the memory, loads and patches the ROM, then calls `AgbMain` (the game's own, unchanged). The game runs on the host stack. |
| `IntrMain` (ARM, dispatches IE & IF through `gIntrTable`) | `Port_RaiseIrq` in C: the same priority order (VBlank, serial/timer 3, HBlank, VCount, DMA 0-3, keypad), only when IME and the IE bit are set. Interrupts are synchronous: raised at the frame boundary, never in the middle of the game's code. |
| The VBlank wait in `Main_EndFrame` (a busy loop on `gMain.intrFlags`, set by the interrupt) | `#ifdef PLATFORM_PC`: `Port_WaitVBlank()`, the frame boundary: the frame's lines (`Port_DrawFrame`: VCount matches, HBlank DMA, drawn if needed), dump the watched RAM (headless), present the frame and read the keys into KEYINPUT (SDL), raise VBlank. The only change to the game's control flow. |
| DMA (the `DmaSet` / `DMA3_SET` macros writing DMA registers) | under `PLATFORM_PC` the macros call `Port_Dma`, which does an immediate transfer at once (16/32-bit, source/destination increment, decrement or fixed) and loads a timed one's internal registers: HBlank channels run between the renderer's lines (`Port_RunHblankDma`); the sound FIFO channels (DMA 1 and 2) move 16 bytes whenever the sound output's FIFO asks (`Port_SoundDmaRequest`), restarted by `m4aSoundVSync` (`Port_SoundDmaRestart`); the game uses no VBlank DMA. Reads of I/O by DMA follow mGBA's open-bus model (see "The renderer"). `Bg_LoadMap` writes the DMA 3 registers through a pointer itself; it gets an `#ifdef PLATFORM_PC` that calls `Port_Dma` for the same transfers. |
| BIOS `CpuSet`, `CpuFastSet` (`libagbsyscall`) | C with the BIOS's rules (word/halfword, fill/copy, `CpuFastSet` in blocks of 8 words, the source/destination alignment the BIOS forces). |
| BIOS `MultiBoot`, the m4a BIOS jump-table copy (`MusicPlayerJumpTableCopy`) | stubs: MultiBoot fails (returns 1), there is no second console. |
| `m4a_1.s` (the mixer `SoundMain`, `MPlayMain`, the sequencer commands, `m4aSoundVSync`) | `port/src/m4a_1.c`: all of it in C, transcribed from the asm (milestone 3, "m4a_1.s in C"). Milestone 1 had silent stubs. |
| m4a's `SampleFreqSet` waits for VCOUNT 159 | `#ifdef PLATFORM_PC`: `Port_WaitForLine159()` (VCOUNT does not count in plain memory; the wait only lines up the sound DMA with the frame; the port test raises the VBlank that follows it). |
| The SDK's SRAM library (`agb_sram.c`) copies its read and verify loops to the stack and calls them (Thumb code, address bit 0) | `port/agb_sram_pc.c`: the same functions (`ReadSram`, `WriteSram`, `WriteSramEx`, `VerifySram`, `VerifySramEx`, `SetSramFastFunc`...) in plain C on the SRAM region; `src/agb_sram.c` is not compiled into the port. |
| `MultiBootWaitCycles` (inline asm in the SDK) | a C stub (MultiBoot never runs). |
| The ARM serial routines in `crt0` | stubs (no link cable). |
| Everything else in the asm modules | `port/tools/gen_port_tables.py` writes a weak empty stub for every function of `crt0`, `m4a_1`, `libagbsyscall` and the `[[asm_regions]]`; a definition in `port/src` replaces it. |
| A reset (BizHawk's "Power" in the TAS movies, kurutest's `reset`) | `longjmp` back to `main`, RAM, I/O, palette, VRAM and OAM cleared (SRAM and ROM kept), `AgbMain` again. |
| libgcc (`__divsi3`, ...), libc (`memcpy`, ...) | the host's: gcc emits native division, the C library is the host's. |

### 5. The window

`frame.c`: SDL2, a 240x160 streaming texture (`SDL_PIXELFORMAT_XBGR8888`: `gPortScreen`'s
0x00BBGGRR words as they are) drawn with nearest-neighbour scaling at a whole multiple of
240x160 (`SDL_RenderSetLogicalSize` and `SDL_RenderSetIntegerScale`: a resized window keeps
the largest multiple that fits, centred). Each frame waits for the GBA's frame period, 280 896
cycles of 16.78 MHz (59.73 Hz), against the performance counter, so a frame that comes late
does not make the next ones fast. The keys are read from SDL's key events into KEYINPUT at the
frame boundary. `--headless` opens no window and draws only the frames someone asks for
(`--video`, `--shot`).

Checked in the cloud container with Xvfb: Return and Z sent through XTEST took the game from
the title to the file select (`gMain.state` 1 to 2), and a grab of the X screen held the port's
screenshot of that frame at 3x, all 345 600 pixels; 600 frames take 10.26 s (10.05 s at
59.73 Hz, plus start-up). Drawing a frame costs about 0.6 ms on that machine; the game's own
work, about 15 µs.

### 6. Headless mode and the port test

`port/kururin_pc --headless --frames N --inputs file.inp [--dump file]` runs without a window:
the keys come from a kurutest `.inp` file (written by `tests/gameplay/run.py` from a scripted
input), and after every frame the values of every `tests/gameplay/watch.txt` entry are written
out, one line per frame, function pointers translated back to GBA addresses.

The port test (`make -C port test`, `port/test/port_test.py`) compares that with the original:

1. `port/test/refdump` (built on the libmgba `setup.sh` builds, mGBA's HLE BIOS, the same
   input semantics as kurutest) runs `baserom.gba` on the input and writes the same watch values
   after every emulator frame, in the same format.
2. **Lag frames.** The original sometimes takes more than one VBlank for a frame of work (a
   level or screen loading): its frame ends a VBlank later, the keys of the frame it missed are
   never read, and RAM sampled at that VBlank is half-way through the game's code. The port has
   no cycle timing, so it cannot know when that happens; the test gives it the original's
   schedule. A frame of the original is a *lag frame* when `gMain.frameCount` has not changed
   by the next frame's sample (its wait did not end at the next VBlank). The port test passes
   the list to the port (`--lag-frames`): at such a frame the port runs the VBlank interrupt the
   original took in the middle of the work, skips the sample (half-way states cannot be
   compared) and goes on to the next emulator frame's keys. In normal play there are no lag
   frames (a PC is fast enough), which is the one intended difference from the hardware.
   Whether that VBlank raised its interrupt comes from the original too (`refdump --timing`:
   DISPSTAT bit 3 at the sample): the first VBlanks after power-on come before `AgbMain`
   enables the interrupt, and the game does not count them (`gMain.vblankCount`).
3. The two dumps are compared frame by frame; the first difference is reported with the frame,
   the keys, the watch entry and both values, then every entry's first differing frame. Entries
   can be excluded with `--ignore` (milestone 1: the m4a clock, see the stubs above).

The reference is kurutest itself: `port/test/refdump.c` includes the locked
`tests/gameplay/kurutest.c` unchanged (its `main` renamed) and adds its own `main`, so the
original runs exactly as in `make test` (mGBA core, HLE BIOS, `save` images, resets). The
inputs are converted by `tests/gameplay/run.py`'s own functions.

The test was checked by breaking the port on purpose: with the Helirin's normal speed 0x181
instead of 0x180 (`Helirin_Update`), `training1_moves` fails at frame 1642 on `helirin0.x`,
`helirin0.xs` and `helirin0.speedNorm`.

## What the game draws with (the display survey, milestone 2)

The renderer implements what the game uses and nothing else. What it uses was measured, not
guessed: `make -C port survey` (`port/test/survey.c`, kurutest with its own `main`, like
`refdump`) runs the original in mGBA on all 14 inputs (167 937 frames) and records, at the end
of every frame, DISPCNT, BGxCNT, the affine, window, blending and mosaic registers, every
visible OAM entry, the DMA channels and the display interrupts; and, through hooks on mGBA's
renderer, every video register, VRAM, palette and OAM write made while a line is being drawn
(VCOUNT < 160) instead of in VBlank. The code agrees (`Bg_SetVideoMode`, `Effect_RotateBg2`,
`Bg_StartWindowCircle`, `Effect_BrightnessUp`/`Down`, `Ending`'s window 1).

| Feature | Used | Where (first seen) |
| --- | --- | --- |
| BG mode 0 (four text BGs) | yes: 137 391 frames | menus, levels |
| BG mode 1 (BG0-1 text, BG2 affine) | yes: 30 516 frames | the intro's and Challenge's rotating BG2 (`Effect_RotateBg2`, `Bg_SetVideoMode` 9 and 4) |
| BG modes 2-5 (affine BG3, bitmaps) | no | |
| Text BGs | 4bpp on BG0-3; 8bpp on BG1 and BG2; 256x256 only (screen size 0) | |
| Affine BG2 | 128x128 (size 0), wrap-around on, rotated and scaled | boot_attract 2599 |
| Mosaic | BG2's mosaic bit is set in some levels, always with MOSAIC = 0 (no effect); MOSAIC is non-zero only while no layer has the bit; OBJ mosaic never | save_adventure 1102 |
| Sprites | every shape and size of 4bpp; affine (all sizes) and double-size; semi-transparent mode; H and V flip; priorities 0-3; wrapping at x 512 / y 256; 1D tile mapping only | |
| 8bpp sprites, OBJ window mode, OBJ mosaic, 2D mapping | no | |
| OBJ cycle budget | exceeded once: 1264 > 1210 cycles on a line (tasvideos_6116M_100, frame 13886), so the per-line sprite limit matters | |
| Window 0 | yes (59 983 frames), shaped line by line: WIN0H written at every HBlank by DMA 0 | the circle window (`Bg_StartWindowCircle`), save_adventure 876 |
| Window 1 | yes (6 447 frames), a fixed rectangle | the ending (`gWin1hCopy`) |
| OBJ window | no | |
| Blending | alpha (BG1 or BG2 over everything, 8/8 and 11/5, and semi-transparent sprites), brighten and darken with every BLDY 0-16 (the fades) | |
| Forced blank | 30 frames (white): after power-on and the 100% TAS's reset | |
| HBlank interrupt | **never enabled** (IE 0x2F01 \| 4): the per-line effect is the HBlank DMA, not a handler | |
| VCount interrupt | line 227 only (`Bg_WindowLineIntr`, which restarts the window's HBlank DMA for the next frame) | |
| Timed DMA | DMA 0 at HBlank (repeat, 16-bit, to WIN0H); DMA 1/2 sound FIFO (special); no VBlank DMA | |

Writes while lines are drawn: WIN0H (the HBlank DMA, as intended); DISPCNT, BG0CNT, BG1CNT in
15 frames (`AgbMain` after power-on and reset, the screen blank); and VRAM (330 frames), BG
and OBJ palette (110, 75) in loading frames, where the game writes to the hardware directly
while the screen shows. Everything else reaches the hardware in VBlank, from the frame
callback (`Main_FrameCallback`: OAM, DISPCNT and 0x27 halfwords of registers from BG0CNT, the
transfer queue). So the port can draw a whole frame at once, from the registers and memory as
they are at the end of the frame's work, and run the HBlank DMA between its lines: only those
loading frames can differ (see the pixel test).

## The renderer (milestone 2)

**Licence:** `video.c` follows mGBA's software renderer closely (the pixel test needs its
pixels, quirks included) and is under mGBA's licence, the MPL 2.0, file by file: its header,
`port/LICENSE-MPL-2.0.txt`, and `port/NOTICE.md`, which also lists the port files compared with
mGBA and found independent. The packages ship the MPL and `source/video.c`.

`port/src/video.c` draws the 240x160 screen line by line from VRAM, palette RAM, OAM and the
video registers at their GBA addresses, implementing exactly the features of the survey above.
`Port_WaitVBlank` (the frame boundary) calls `Port_DrawFrame` after the frame's work, before
the VBlank interrupt:

1. the VCount matches of lines 161-227, the end of the last VBlank: the game's `Bg_WindowLineIntr`
   at line 227 sets WIN0H for line 0 and restarts DMA 0 (line 160's match comes with the VBlank);
2. the affine BGs' reference points are latched from BG2X/BG2Y (as at the start of a frame);
3. for each line 0-159: VCOUNT, a VCount match, the line drawn from the registers as they are,
   then HBlank: the channels started with HBlank timing run (`Port_RunHblankDma`, `hw.c`: the
   DMA unit now keeps each channel's internal source, destination and count, so a repeating
   channel walks through `gWindowLines` one halfword per line), and the HBlank interrupt is
   raised if DISPSTAT enables it (the game never does).

The lines are drawn only when someone looks (the window, the pixel test); the DMA and the
interrupts run either way, so the RAM is the same with or without a window.

How a line is drawn follows mGBA's software renderer, the reference the pixel test compares
with (kurutest's screens are mGBA's): the colours are its 8-bit channels (`c << 3 | c >> 2`);
the line is split into window segments (WINOUT, window 1, window 0 in front); the sprites of
the line are drawn in OAM order into a sprite line until the line's 1210 sprite cycles run out
(an affine sprite costs 10 + 2 x its width, a regular one its width), a later sprite replacing a
pixel only if its priority is higher; then per segment, for each priority 0-3, the sprites of
that priority and the BGs of that priority are composited through flags kept in each pixel's top
byte (priority, BG number, first and second blend target, semi-transparent); then a first target
with nothing under it blends with the backdrop, and the semi-transparent sprites get the
brightness effect. Brightness up and down use a brightened / darkened copy of the palette for
the first-target layers. The I/O registers start as the BIOS leaves them (`memory.c`: DISPCNT
forced blank, the affine identity matrices, as mGBA's `GBAIOInit`).

Two things the pixel test found, both reproduced so the frames are identical:

- **Text BGs are drawn tile by tile as mGBA draws them** (a first partial tile, whole tiles, a
  last partial tile per window segment). It matters only in window segments narrower than a
  tile, at the tips of the circle window: when a segment ends inside its first tile, mGBA takes
  an H-flipped tile's pixels from the wrong end (and an 8bpp tile's from its right half), and it
  draws 4bpp partial tiles even past BG VRAM. These are mGBA's, not the hardware's; they move a
  few pixels at the circle's top and bottom lines (`save_adventure` frame 2802: 7 pixels).
- **Write-only registers read as open bus.** `Main_InstallFrameCallback` copies the 0x2B
  halfwords of I/O from DISPCNT into RAM with the BIOS's CpuSet (`Main_InstallLinkFrameCallback`
  with DMA 3), write-only registers included, and the frame callback writes the copies back each
  frame until the game sets them: on the mode select the affine BG2 is drawn for two frames with
  whatever BG2PA-BG2Y read as (`save_makeup` frame 3947), and MOSAIC gets 0xFFFB or 0x30B2. The
  port's I/O is plain memory, so `hw.c` makes those reads as mGBA makes them (`GBAIORead`,
  `GBALoadBad`): readable video registers as the hardware keeps them (masked), write-only and
  unused ones as open bus: for CpuSet the low half of the ARM instruction two ahead of the load
  in mGBA's HLE BIOS (0xFFFB after a copy loop's load, 0x30B2 after a fill's), for DMA the last
  value the DMA unit moved. The real BIOS would give other values; the game's RAM copies of
  these registers are not in `watch.txt`, which is why the RAM test did not see it.

## The pixel test (`make -C port test-video`)

`port/test/video_test.py` compares the port's screen with the original's on recorded inputs
(default: `boot_attract`, `training1_moves`, `see_map`, `save_makeup`: the rotating BG, the
map view, menus, sprites; `VIDEO_ARGS=--all` for all 14, `-v` to list every difference):

1. `port/test/build/refdump` (kurutest's mGBA setup, the screens `make test` compares) runs the
   original with every frame rendered and writes a hash of each frame's screen (`--video`,
   FNV-1a over each pixel's 24-bit colour; `--fb-every N` for every Nth frame), the RAM and
   VBlank timing the port test uses for the original's lag frames, and (`--mid-frame`) what it
   changed while lines were being drawn: VRAM, palette RAM, OAM and video register writes at
   VCOUNT < 160 (hooks on mGBA's renderer), and HBlank DMA reads whose source had changed by the
   end of the frame;
2. `port/kururin_pc --headless --video` writes the same hashes of its own screen;
3. every frame both have is compared (lag frames have no port screen; the first frame after
   power-on and each reset is skipped: mGBA's HLE BIOS starts the cartridge at line 126, the
   screen blank anyway);
4. every differing frame is run again in both (`--shot`) for its two screenshots and the video
   state each was drawn from (`port/test/build/video/<input>/{ref_baserom,port}_frameNNNNNN.ppm`,
   `{ref,port}_stateNNNNNN.bin`: I/O, palette, VRAM, OAM). A difference is **expected** when
   the original changed the video state while drawing those lines (a write on line L can show
   on lines 0-L; a changed HBlank DMA source on its own lines): the port draws a frame at once,
   from the hardware as the frame's work left it, and cannot know how far the display had got.
   Any other difference fails, with the first differing pixel and which video memory differs
   (the same state and different pictures: the renderer).

Breaking the renderer on purpose fails it: brightness computed with `/ 15` instead of `/ 16`
fails `boot_attract` from frame 2615 (the title's fade-in), 2757 pixels differing.

**Results (2026-10-22), every frame:**

| Input | Screens compared | Identical | Differ (expected) |
| --- | ---: | ---: | ---: |
| boot_attract | 5 985 | 5 985 | 0 |
| training1_moves | 2 292 | 2 290 | 2 |
| see_map | 3 940 | 3 938 | 2 |
| save_makeup | 4 058 | 4 056 | 2 |
| menus | 2 066 | 2 064 | 2 |
| training1_crash | 3 475 | 3 473 | 2 |
| save_adventure | 9 421 | 9 420 | 1 |
| save_challenge | 4 121 | 4 120 | 1 |
| save_corrupt | 810 | 809 | 1 |
| save_ending | 9 628 | 9 626 | 2 |
| save_files | 6 725 | 6 717 | 8 |
| save_practice | 6 979 | 6 978 | 1 |
| tasvideos_3933M_any | 22 032 | 22 023 | 9 |
| tasvideos_6116M_100 | 84 367 | 84 349 | 18 |
| **all** | **165 899** | **165 848** | **51** |

No frame differs in any other way. The 51 remaining frames are two effects of timing, which the
port does not model (it has no cycle timing: see the lag frames):

- **48 screen changes**: when a screen is set up (the training's first frame, the mode select,
  file select, level starts), the game decompresses tiles straight into VRAM (once also the
  palette) while the top of the screen is being drawn; the original's first lines show the old
  tiles (e.g. frame 554 of most inputs: lines 8-17, 1 653 pixels), the port's the new ones. One
  frame each time.
- **3 circle-window frames** (frame 1137 of the training inputs, when the circle closes on the
  dead Helirin): `Bg_DrawWindowCircle` clears and redraws the window's line table while the
  HBlank DMA reads it out, so the original shows lines 77-82 without the window (42 pixels).

## What the game's sound uses (the sound survey, milestone 3)

As for the display, the port's sound implements what the game uses, measured rather than
guessed: `make -C port sound-survey` (`port/test/sound_survey.c`, kurutest with its own `main`)
runs the original in mGBA on all 14 inputs (167 937 frames) and samples at the end of every
frame the sound registers, m4a's settings in `gSoundInfo`, the twelve DirectSound channels,
the four GB-compatible channels (`gCgbChans`) and the songs each music player plays. The code
agrees (`m4aSoundInit`, `SoundInit`, `MPlayExtender`, `CgbSound` in `src/m4a.c`).

**m4a_1.s routines** (all of them run on the inputs except three sequencer commands;
`docs/coverage.md`):

| Routine | What | Run |
| --- | --- | --- |
| `SoundMain` (0x0801A538) | once a frame from `m4aSoundMain`: runs the players (`MPlayMainHead`), `CgbSound`, then jumps to the mixer's copy in IWRAM | yes |
| `SoundMainRAM` (0x0801A5BC, run from `SoundMainRAM_Buffer`) | the mixer: reverb or clear of the frame's buffer segment, then each DirectSound channel's envelope and its samples added into the segment, interpolated or at a fixed rate (ARM code) | yes |
| `m4aSoundVSync` | from the VBlank interrupt: counts `pcmDmaCounter` down and restarts the sound DMAs at the buffer's start every 7 VBlanks | yes |
| `MPlayMain` | a music player's tick: tempo, the tracks' commands, gate times, LFO, then volume and pitch of the playing channels | yes |
| `ply_note` | a note (commands 0xCF-0xFF): picks a channel (DirectSound by priority, or the tone's CGB channel) and starts it | yes |
| `TrackStop`, `ply_endtie`, `ply_lfos`, `ply_mod`, `MPlayJumpTableCopy` | named in the SDK's jump table | yes |
| the other jump-table entries (`sub_0801A998` ply_fine, `sub_0801AA08` ply_goto, ply_patt, ply_pend, ply_tempo, ply_keysh, ply_voice, ply_vol, ply_pan, ply_bend, ply_bendr, ply_lfodl, ply_modt, ply_tune; entries 34 and 35 `RealClearChain`, `Clear64byte`), `sub_0801B0CC` (the asm's ClearModM), the command-byte readers | pret's names, by their place in the table (`gUnk_0801F2D4`) | yes |
| `sub_0801AA58` (ply_rept), `sub_0801AA88` (ply_prio), `sub_0801AB74` (ply_port: writes a sound register) | entries 4, 9 and 27 | never |
| `umul3232H32` | `MidiKeyToFreq`'s 32x32 high word (ARM `umull`) | yes |

**The hardware**:

| Feature | Used | Notes |
| --- | --- | --- |
| DirectSound A | yes, **right** only | `SOUNDCNT_H` = 0x210E: A 100% to the right, B 100% to the left, both on timer 0, PSG at 100% |
| DirectSound B | yes, **left** only | |
| Sample rate | **13 379 Hz**, always | `m4aSoundMode(SOUND_MODE_FREQ_13379 ...)`: timer 0 reload 0xFB1A (1 254 cycles a sample), **224 samples per VBlank** exactly, `pcmFreq` 13379, `divFreq` 627 |
| Sound DMA | DMA 1 to FIFO A from `pcmBuffer` (0x03004BD0), DMA 2 to FIFO B from `pcmBuffer` + 0x630 (0x03005200); 32-bit, repeat, FIFO timing | restarted by `m4aSoundVSync` every **7 VBlanks** (`pcmDmaPeriod`: 1 568 of the buffer's 1 584 samples) |
| SOUNDBIAS | 0x4200: bias 0x200, **8-bit resolution, 65 536 Hz** PWM (`SOUND_MODE_DA_BIT_8`) | 0x0200 (the BIOS's) for the 30 frames before `SoundInit` |
| Master volume | 13 (of 15) | 15 for the first frame of `SoundInit` |
| DirectSound channels | `maxChans` per song (`gSongSoundModes`): 2, 4, 5, 8, 9, 11, 12; up to **12 at once** (save_ending) | resampled (type 0, linear interpolation) and fixed-rate samples (type 8, the drums: one sample per output sample); looped samples; releases |
| Reverb | **50** in every song after the first (`MPlayStart` takes the song header's), 0 before | the mixer's reverb mixes each segment with the next (the oldest) at 50/512 |
| Pseudo echo (`SOUND_CHANNEL_SF_IEC`) | never on a DirectSound channel | the code path is ported anyway |
| `maxLines` | 0 | the mixer's VCOUNT limit never applies |
| CGB channel 1 | square, duty 12.5/25/50%; **sweep** 0x4D (time 4, down, shift 5) in some sounds, else 0x08 (off) | |
| CGB channel 2 | square, duty 12.5/25/50% | |
| CGB channel 3 | wave: one 32-sample bank (`NR30` 0x00/0x80, bank 0, one bank), 4 waves (0x08020FD8, 0x08020FF8, 0x08021028, 0x08021048); volume 100/50/25%, and 75% (`NR32` bit 7) | `CgbSound` writes the wave RAM with bank 1 selected (bank 0 written) and plays bank 0 |
| CGB channel 4 | noise, `NR43` = 0x06 (15-bit, ratio 6, shift 0) | type 4 and 12 |
| `NR50` | 0x77 (both sides at 7) | |
| `NR51` | every channel to either or both sides (`CgbSound`'s pan) | |
| Envelope, length | the hardware envelope (`NRx2` step and direction, re-triggered with each change of volume) and the length counter (`NRx1`, `NRx4` bit 6) | `CgbSound` steps its own envelope at 60 Hz alongside |
| Music players | 4: `gMPlayInfo_BGM` (0x030059C0, 9 tracks, 20 songs on the inputs), 0x03005A00 (2 tracks, 18), `gMPlayInfo_SE2` (0x03005A40, 2 tracks, 5), 0x03005A90 (1 track, 9) | tracks at 0x03000030, 0x03000300, 0x030003A0, 0x03000440 |

So the port needs: the mixer, the sequencer and the VSync in C (exactly: they fill RAM that
the original's `pcmBuffer` and players can be compared with), the two FIFOs fed from the
buffer at 13 379 Hz, and the four GB channels (both squares with sweep, the wave channel with one
bank, noise) with envelope and length, mixed as the GBA mixes them.

## m4a_1.s in C (milestone 3)

`port/src/m4a_1.c` is the whole of `m4a_1.s` (0x0801A528-0x0801B11C) in C, transcribed from this
ROM's asm rather than from pret's newer revision (`src/m4a.c` notes how that revision differs;
`m4a_1.s` differs too: its mixer interpolates every resampled channel and has no VCOUNT fast
path for this game, `ply_rept` compares its counter before storing it as a byte, the LFO's
phase is used before it is stored as a byte). Each function does what its asm does, with the
same widths, signedness and order of reads and writes, so the RAM it fills is the original's
byte for byte (the audio test below):

- `SoundMain` (the frame's lock, the players, `CgbSound`, the segment from `pcmDmaCounter`) and
  the mixer `SoundMainRAM` (reverb or clearing, each channel's envelope, then its samples):
  the ARM mixer adds four samples into a word of each half, rotating it a byte at a time
  (`add rW, r1, rW, ror #8`); the C does the same rotations (`struct MixWords`), so a byte's
  sum wraps exactly as there, and a channel that ends inside a word stores that word
  rotated back as the asm does. Fixed-rate samples (type 8) go a word at a time while more
  than four samples are left, then one at a time; resampled ones interpolate linearly with a
  23-bit fraction stepped by `divFreq * frequency`, wrapping into the loop by as much as they
  went past its end.
- `MPlayMain` (ticks of 150 tempo units: gate times, the commands, the LFO; then volume and
  pitch), `ply_note` (picking a channel: CGB by priority and track, DirectSound free, else
  releasing, else lowest priority), `ply_endtie`, `TrackStop`, `ply_lfos`, `ply_mod`, every
  jump-table command (`sub_0801AA08` ply_goto ... `sub_0801AB74` ply_port), `RealClearChain`,
  `Clear64byte`, `MPlayJumpTableCopy`, `umul3232H32`; the asm's check that a song's data is not
  read from the BIOS (a value from below 0x02000000 reads as 0) is kept.
- `m4aSoundVSync`: counts the DMA period and restarts DMA 1 and 2; the port's DMA unit
  (`hw.c`, `Port_SoundDmaRestart`) reloads their sources, which the sound output reads (below).

The routines keep the names `config/functions.txt` gives them (`sub_...` for the jump table's
commands, with pret's name in a comment), which the port's tables use; `port/src/stubs.c` keeps
only the two SDK asm regions.

**Lag frames and m4a's VSync.** The port test's lag frames (the original took more than a
VBlank for a frame's work) were raised at the frame boundary, after the work; that was enough
for the game's RAM, but m4a's DMA period counter (`pcmDmaCounter`) counts every VBlank, and
which buffer segment `SoundMain` mixes depends on where in the work each VBlank came. Three
cases occur, each told by the original's state at that VBlank (`refdump --timing`):

1. most lag VBlanks come during the work, before `m4aSoundMain` ends it: the port's `SoundMain`
   raises them first (`Port_RunLagFrames`), so the counter is the original's when it chooses
   its segment;
2. one that came while the original's mixer ran (the CPU in `SoundMainRAM`'s IWRAM copy, or in
   `SoundMain` after it read the counter) is raised after the segment is chosen;
3. one that came while m4a's VSync was off (`gSoundInfo.ident` not `ID_NUMBER` or one more:
   between `m4aSoundVSyncOff` and `On`, which `Play_RunLevel` calls around a level's loading,
   or before `SoundInit`) is ignored by the port's `m4aSoundVSync` too.

And at power-on, `m4aSoundInit`'s two `SampleFreqSet` calls wait for line 159, which the port
cannot (its VCOUNT does not count): `src/m4a.c` calls `Port_WaitForLine159` there under
`PLATFORM_PC`, which raises the VBlank that comes a line after the wait in the original
(in the port test; in play there are no lag frames).

## The audio test (`make -C port test-audio`)

The mixer writes the frame's samples into `gSoundInfo.pcmBuffer`, which the sound DMAs then play;
so the exact test of the port's sound is that buffer, with all of m4a's state, against the
original's, after every frame. `make -C port test-audio` runs the port test
(`port/test/port_test.py`) on `boot_attract`, `save_ending` and `save_makeup`
(`AUDIO_INPUTS=...` for others) with the entries of `port/test/audio_watch.txt` after those of
`tests/gameplay/watch.txt` (`--extra-watch`): `gSoundInfo` (the lock, `pcmDmaCounter`, the
settings, the function pointers, the 12 DirectSound channels), both halves of `pcmBuffer`
(`raw`: bytes, never translated as a function pointer), `gMPlayJumpTable`, the 4 `gCgbChans`,
the 4 music players with their 14 tracks and `gMPlayMemAccArea`. The original's lag frames
come with the evidence of where their VBlank came ("m4a_1.s in C"): `refdump --timing` writes,
for each frame, whether m4a's VSync ignored that VBlank (`gSoundInfo.ident`) and whether the
CPU was in the mixer (its PC).

**Results (2026-10-22): identical, byte for byte, on every compared frame** of the three inputs,
and of all 14 (`python3 port/test/port_test.py --all --extra-watch port/test/audio_watch.txt`):

| Input | Frames | Original's lag frames | Compared, all identical |
| --- | ---: | ---: | ---: |
| boot_attract | 6 000 | 15 | 5 985 |
| save_ending | 9 669 | 41 | 9 628 |
| save_makeup | 4 096 | 38 | 4 058 |
| the other 11 | 148 172 | 1 944 | 146 228 |
| **all 14** | **167 937** | **2 038** | **165 899** |

Breaking the mixer on purpose fails it: one bit of the resampler's phase flipped
(`fw ^= 1`) fails `boot_attract` at frame 94, the first note.

## The sound output (milestone 3)

`port/src/audio.c` and `port/src/apu.c` do what the GBA's sound hardware does with what m4a
writes, and play it through SDL audio. At each frame boundary, before the VBlank interrupt,
`Port_AudioFrame` renders the frame that just ended (280 896 cycles), as the hardware played
it while the game did that frame's work:

- **DirectSound.** Timer 0 (its reload from TM0CNT_L: 1 254 cycles, 13 379 Hz) pops a byte from
  each 32-byte FIFO at every overflow; a FIFO holding 16 bytes or fewer asks its DMA for 16 more
  (GBATEK's rule), which `hw.c` reads from the PCM buffer where the DMA's source is
  (`Port_SoundDmaRequest`; `m4aSoundVSync` restarts it every 7 VBlanks, without emptying the
  FIFOs, as on the hardware). A sample is held until the next overflow: the mixed segment of
  frame N plays during frame N + 1, as on the GBA.
- **The GB channels** (`apu.c`): both squares (duty, envelope, length; channel 1's sweep), the
  wave channel (one 32-sample bank, 100/50/25/75% volume) and the noise channel (15- and 7-bit),
  stepped cycle by cycle, with the 512 Hz frame sequencer for length (256 Hz), sweep (128 Hz) and
  envelope (64 Hz). The registers are plain memory, so `Port_ApuSync` reads them once a frame:
  a write to NRx4 is how a channel restarts or changes frequency, and since NRx4's bits 3-5 do
  nothing on the hardware and `CgbSound` writes them 0, the port sets them after reading: when
  they read 0 again, the game wrote the register (even the same value). The sweep writes its
  frequency back into NR13, as the hardware's frequency register (`CgbOscOff` later writes NR14
  alone). The game runs a frame's work at once, so the port applies `CgbSound`'s writes 40 lines
  into the frame: where the original typically writes them (24 to 51 lines after the VBlank,
  median per input, measured with `refdump --psg-writes`).
- **The mix**, as the GBA (and mGBA, the tests' reference) computes it: each GB channel 0-15 to
  its sides (NR51), x (NR50 volume + 1) x 8, then SOUNDCNT_H's PSG volume (here 100%: / 4);
  DirectSound A to the right and B to the left (SOUNDCNT_H), x 4; SOUNDBIAS's clamp to the
  10-bit DAC; x 48 to 16 bits (mGBA's scale). Then a 10 Hz high-pass (the output capacitor:
  the GB channels are unipolar) and the volume.

The output has 896 samples a frame, 4 per DirectSound sample: 53 516 Hz, the frame rate times
896 (the game's 13 379 Hz times 4, so each DirectSound sample covers exactly four output samples,
held as the hardware holds it); each output sample is the average of the GB channels over its
313.5 cycles (a box filter against aliasing). SDL converts it to the device's rate.

**Frame loop and ring buffer.** The frames go into a 32 768-sample ring that SDL's callback
empties (512-sample callbacks). The frame loop (59.73 Hz on the performance counter) is the
clock; `Port_AudioPacing` moves each frame's deadline by up to 0.2%, in proportion to how far
the ring's fill (averaged over about a second) is from its target, so the device's clock and the
frame timer cannot drift apart.

- *Start.* The device is started as it is opened, and the game's first frame waits (3 s at
  most) until the device is running: a callback 5 ms or more after its first one. PulseAudio
  takes a buffer's worth at once and then nothing for 1-2 s (measured under a null sink: the
  first steady callback 1.2-2.0 s after opening); before, the game ran on meanwhile, the ring
  filled up, about a second of the game's first sound was dropped (`sound overrun`) and the
  ring stayed full, 0.3 s behind the picture, for minutes. The callback plays from the ring once
  it holds its target, 2 frames (33 ms) at first.
- *Underrun* (the ring short: the game late, or a device that takes more at once than the ring
  holds): the callback holds the last sample and stops playing until the ring is back at its
  target, one gap rather than a gap at every callback; the target is then raised to a frame
  more than the most the device has taken at once (callbacks less than 2 ms apart), up to 16
  frames (267 ms). Under a PulseAudio null sink, 20 s from start-up each: with the device's
  default latency nothing is logged (before: 11 000-38 000 samples dropped at the start); with
  `PULSE_LATENCY_MSEC` 40, 120 and 250 (bursts of up to 3 584 samples, as a long-latency server
  such as WSL's gives) one underrun at most, at the start, then none (before: 38 000 samples
  dropped at the start, then the ring stuck full, 0.3 s behind the picture, which hid bursts
  up to 120 ms; at 250 ms underruns and overruns went on).
- *Overrun* (the device stalled: more than the target, the largest burst and 4 frames): the
  oldest samples are dropped down to the target, so the sound never lags far behind.

Both are counted and logged, a line a second at most (`sound underrun: N samples short in 60
frames (now buffering M ms)`, `sound overrun: N samples dropped`).

Options: `--mute` (no device; the game runs the same), `--volume N` (0-100%), `--wav FILE` (the
output into a 16-bit stereo WAV, with `--headless` too; 6 000 frames take 1.6 s), and for
debugging `--sound-only psg|ds` and `--psg-trace FILE`. In test runs (headless, no `--wav`) the
sound output does not run at all; with it, the RAM is the same frame for frame (checked on
boot_attract and save_ending with the audio watch list).

**Compared with mGBA.** `port/test/build/refdump --audio FILE` writes mGBA's own mixed output
(65 536 Hz, before resampling; `--audio-only psg|ds` for one part), `--psg-trace FILE` its GB
channels' state after each frame (the port's `--psg-trace` writes the same format), and
`--psg-writes FILE` where in each frame the game wrote the GB registers. On boot_attract, with
the original's lag frames (`--lag-frames`, so the frames line up):

| | Result |
| --- | --- |
| levels | RMS within 0.5% (left 3 037 vs 3 024; right 2 889 vs 2 874); DirectSound alone 2 937 vs 2 960, GB channels alone 781 vs 778 |
| whole output | correlation 0.93 at a constant offset (one frame, the port renders a frame at its end, minus 0.77 ms); per frame median 0.968, 90th percentile 0.996 |
| DirectSound alone | correlation 0.968: the same samples; mGBA's stream is 10 samples (0.77 ms) later, from its word-based FIFO model (the port follows GBATEK's byte FIFO) |
| GB channels | state identical to mGBA's on 99.8% of frames (boot_attract 14 of 5 985 differ, save_ending 29 of 9 628, save_makeup 50 of 4 058): the others are an envelope or sweep step a tick apart, where the original wrote the registers earlier or later than 40 lines in; per-frame spectra correlate 0.990 (median); waveforms do not correlate sample for sample because a square's phase depends on the exact trigger time |
| clicks | no seam at frame boundaries (the steps there are those of any DirectSound sample edge); steps of more than 8 000 occur 41.8 times a second in both, the music's own (99.9th percentile step 7 498 vs mGBA's 7 104); no clipping (peak 24 672) |

Found on the way: **`make test`'s audio comparison compares silence.** kurutest never sets
mGBA's `volume` option, so mGBA's master volume is 0 and every sample it mixes is 0 (both ROMs'
audio hashes are of zeros; only the sample counts can differ). The harness is locked; the
maintainers may want to set `opts.volume` (0x100) in `emuInit`. Tried on 2026-10-22: every input then fails on the audio, because
the game's non-matching C changes when m4a writes the sound hardware (docs/audit.md, "Audio with
mGBA's volume on"); since then `make test` compares the writes to the sound registers instead
(CLAUDE.md). refdump sets the master volume
itself for `--audio`.

Checked in the container under Xvfb with SDL's `disk` audio driver (it sleeps whole
milliseconds per buffer, so it runs ~6% fast and underruns steadily: logged once a second,
the game unaffected): 1 200 frames take 20.2 s (nominal 20.09), with or without sound.

## Saving (milestone 3)

In play the save is **`kururin.sav` next to the executable** (`SDL_GetBasePath`, the same on
Windows), or `--save FILE`; `--no-save` plays without one. It is read at start-up (a missing
file is a blank cartridge, which the game formats at boot), and written whenever the game has
written SRAM (`port/src/sram.c`): after every frame `Port_SramFrame` compares the 32 KiB the game
uses with what the file holds (the game writes SRAM both through the SDK's `WriteSram` and
directly, `src/save.c`, so a comparison catches every write); once it differs and has then stayed
the same for 30 frames (0.5 s: a save is written over several frames) the file is written,
whole. At exit (the window closed, Ctrl+C or SIGTERM, which end the run at the next frame) a
change not written yet is written. Each write goes to `kururin.sav.tmp` and is renamed over the
old file, so a crash leaves the previous save or the new one, never half of one.

A run with `--inputs` (the tests) or `--headless` has no save file unless `--save` is given:
an input's `save` image is read and never written, as in kurutest.

Checked: `menus` (a new file named "M") with `--save` writes a file byte-identical to the
original's SRAM after the same input (kurutest `--dump-save`), decoded by `tools/make_save.py
--show`; a real-time run under Xvfb killed with SIGKILL after the game's save point leaves the
complete save (written during play, no temporary file left); SIGINT (windowed) and SIGTERM
(headless) end the run with status 0 through the save at exit; plain play creates
`port/kururin.sav` at boot.

## Settings and controls (milestone 4)

`kururin.ini` next to the executable (`--config FILE` for another) holds the player's settings;
the first windowed run writes it with the defaults and comments, and later runs read it at
start-up (`port/src/config.c`). `--headless` runs (the tests) never read it. The command line's
`--scale`, `--fullscreen`, `--volume` and `--mute` win over it.

| Section | Setting | Default |
| --- | --- | --- |
| `[video]` | `scale` (1-16: the window is 240x160 times it), `fullscreen` (yes/no) | 3, no |
| `[sound]` | `volume` (0-100), `mute` | 100, no |
| `[game]` | `pause_on_focus_loss` | yes |
| `[keyboard]` | `A B L R Start Select Up Down Left Right`: SDL key names, several separated by commas | Z, X, A, S, Return, Backspace, arrows |
| `[gamepad]` | the same buttons: SDL game controller buttons (`a b x y back start leftshoulder rightshoulder dpup ...`) and axis directions (`leftx- leftx+ lefty- lefty+ rightx- ... lefttrigger righttrigger`); `deadzone` (percent) | A = a, B = b, L = leftshoulder + lefttrigger, R = rightshoulder + righttrigger, Start = start, Select = back, directions = d-pad + left stick; 50 |

A setting the file does not have keeps its default; a button listed in the file replaces its
default bindings; a name that does not exist is reported on stderr and left out.

- **Keys** are what the keyboard and the gamepads hold at the frame's end, plus whatever was
  pressed during the frame (a tap shorter than a frame still counts for one frame). Gamepads
  use SDL's game controller database (Xbox, PlayStation, Switch Pro and most others have a
  mapping), are opened when plugged in, up to 4, all at once.
- **Fullscreen**: F11 or Alt+Enter switch it (`SDL_WINDOW_FULLSCREEN_DESKTOP`: the desktop's
  resolution, the picture scaled by a whole number with black borders, the mouse cursor
  hidden); Escape leaves it. Alt+Enter does not press Start.
- **Pause on focus loss**: when the window loses the focus, the frames, the sound and the frame
  clock stop until it comes back ("(paused)" in the title).

Checked under Xvfb with openbox (a window manager is what makes X11 fullscreen work), driven by
xdotool through XTEST: the first run writes `kururin.ini`; F11 makes the window 1280x1024 (the
screen) and Alt+Enter 720x480 again; focusing another window (xclock) stops the frame counter
(frame 967 two seconds later, the title "(paused)") and focusing back resumes it; with `Start =
Space` in the file, Space starts the game and Return does nothing. There is no gamepad (nor
`/dev/uinput`) in the container: `make -C port test-config` (`port/test/config_test.c`) checks
the file (defaults, a file with Windows line ends over them, the command line over it) and the
gamepad mapping with SDL's virtual joystick (every default and remapped button, the stick
directions, the deadzone, the triggers): all pass.

## Windows (milestone 4)

`make -C port windows` builds `port/kururin_pc.exe` with mingw-w64's 32-bit compiler
(`i686-w64-mingw32-gcc`, Ubuntu's `gcc-mingw-w64-i686`) from the same sources, and copies
`SDL2.dll` next to it. SDL2 is SDL's own MinGW development package (2.32.10), downloaded once
into `port/build/sdl2-mingw` by `port/tools/get_sdl2_mingw.sh` (pinned SHA-256).

- **The memory map**: the GBA's memory is part of the exe's own image ("The memory map on
  Windows: in the image", below).
- **The executable** is based at 0x00400000 with no ASLR (`--disable-dynamicbase`) and no
  relocation section, so it can only load there; its code and data start at 0x10000000, after
  the GBA's sections. It is large-address-aware (on 64-bit Windows the DLLs and heap may use
  2-4 GiB too) and a GUI program (`-mwindows`: no console window). It imports `SDL2.dll`,
  `KERNEL32.dll` and `msvcrt.dll` only (`-static-libgcc`). The GBA symbols are absolute symbols
  in the PE image as in the ELF one (`_gMain = 0x03000DC0`: 32-bit Windows C names have a
  leading underscore, `gen_port_tables.py --prefix _`).
- **Compiler options**: the Linux build's, plus `-mno-ms-bitfields` (mingw lays bitfields out
  as MSVC does by default; the game's structs must keep GCC's layout) and
  `-D__USE_MINGW_ANSI_STDIO=1` (C99 `printf`: `%zu`). `make -C port layouts-windows`: all 92
  structs have agbcc's layout.
- **Weak symbols**: GNU ld's PE target does not resolve weak definitions referenced from other
  objects, so `gen_port_tables.py` now reads which functions the objects define (`nm`) and
  writes plain stubs for the asm functions `port/src` does not define, and NULL in
  `gPortFunctions` for the functions the port has no code for (the same tables as before, on
  both systems).
- **Small platform code**: `Port_Die` also shows a message box on Windows (a GUI program has no
  console); started from a console, the program attaches to it (`AttachConsole`) so its
  messages appear there; `baserom.gba` is looked for in the current directory, then next to the
  executable; the save is already next to the executable (`SDL_GetBasePath`) and replaced
  safely (`sram.c`).

Checked under Wine 9.0 (Ubuntu's `wine`, `wine32:i386`): 600 headless frames of `boot_attract`
give the Linux build's watched RAM line for line; the port test (below) passes `save_adventure`
with the Windows build.

### The memory map on Windows: in the image

**What went wrong.** The first release reserved each GBA region with `VirtualAlloc` from `main`.
On a real Windows 11 machine it stopped at once: "cannot reserve the GBA's EWRAM at
0x02000000-0x03000000 (Windows error 487)" (`ERROR_INVALID_ADDRESS`: something was there
already). Under Wine those addresses are free when `main` runs; on Windows, by the time an exe's
code runs, the loader, ntdll's heaps, the C runtime, the system DLLs and whatever a security
tool or an overlay injects have all allocated memory, much of it low in the address space and
partly at random (bottom-up randomisation), so any fixed address below 0x10000000 may be taken.

**The fix: the GBA's memory is part of the exe's image.** The kernel maps an exe's image when it
creates the process, before ntdll's start-up code, before any DLL, heap or thread stack; an
image without relocations is mapped at its base or the process does not start at all. So
`port/tools/win_image.py` makes the GBA's memory sections of `kururin_pc.exe`:

| Section | Addresses | |
| --- | --- | --- |
| (headers) | 0x00400000-0x00400FFF | the image base |
| `.gbabus` | 0x00401000-0x01FFFFFF | the open bus: read-only zeros |
| `.ewram`, `.iwram`, `.gbaio`, `.pltt`, `.vram`, `.oam`, `.gbarom`, `.sram` | the GBA's regions (`port.h`) | read-write zeros (the ROM made read-only once loaded) |
| `.ewmirr`, `.iwmirr`, `.gap4` ... `.gape` | everything between them, up to 0x0FFFFFFF | read-only zeros (the mirrors: no code reaches them) |
| `.text`, `.data`, ... | from 0x10000000 | the exe's own code and data |

All of them are uninitialised: nothing in the file (the exe is still 1.5 MB), the pages zeroed
by the loader. PE sections must be contiguous, hence the fillers; they are read-only so they
cost no commit charge (the read-write ones, 33 MB, mostly the ROM's 32 MiB, do). The build
(`port/Makefile`): `win_image.py asm` writes the sections (`.space` in `"bw"` sections),
`win_image.py ld` takes the linker's own default script (`ld --verbose`) and puts them first,
with `.text` at 0x10000000; after the link `win_image.py patch` makes the fillers read-only
(gas has no flag for read-only uninitialised data: `"br"` makes initialised zeros, 220 MB of
them in the file) and checks every section's address and size, the base, no ASLR, no
relocations, or fails the build.

At run time `Port_MapMemory` checks that each region is committed memory of the exe's own image
(`VirtualQuery`: `MEM_IMAGE`, allocation base the exe's); a region that is not (an exe linked
without the sections) is allocated with `VirtualAlloc` if free. Below 0x00400000 the open bus is
handled as before (free blocks committed read-only, the exception handler for the rest:
`LevelIntro_Run`'s read of 0x000C0004 is there).

Why not the launcher (the exe starting itself suspended and reserving the ranges in the child
with `VirtualAllocEx` before resuming it): it works too, but it needs a second process at every
start, a way to hand the reservations over (the child must commit, not reserve), and leaves a
window in which the suspended child already has its stack and ntdll's early allocations; the
image needs nothing at run time and has no such window. Its one cost is the image's size in the
address space (0x00400000-0x10100000), which a 32-bit process has to spare (2 GiB, or 4 GiB
large-address-aware on 64-bit Windows).

**Diagnostics** (kept for good): when a region cannot be had, the message box, stderr and
`kururin.log` next to the exe hold the `VirtualQuery` map of the requested range: each region's
base, size, state (free, reserved, committed), type (image, mapped, private), protection,
allocation base and, for images and mapped files, the file (`GetMappedFileName`, else the
module's `GetModuleFileName`). Every fatal error (`Port_Die`) is written to `kururin.log` on
Windows, and the message box says so.

**Tested under Wine** by taking the ranges before the game runs: `port/test/occupy.c`
(`occupy.exe ADDR... -- PROGRAM`) starts the program suspended, reserves each address in it
with `VirtualAllocEx`, then resumes it. With `kururin_pc_noimage.exe` (the same objects linked
without the sections, the first release's layout; `port/Makefile`, tests only) and 0x02000000
taken, the game stops with the Windows 11 error, now with the map ("0x02000000 size 0x00010000
reserved private ..."); with `kururin_pc.exe` every reservation fails (the child holds those
addresses already: `MEM_IMAGE`, allocation 0x00400000) and the game runs. `make -C port
test-wine OCCUPIED=1` (and `test-audio-wine`, `test-video-wine`) runs the tests that way: the
start of each GBA region (EWRAM, IWRAM, I/O, palette, VRAM, OAM, ROM, SRAM) taken before
start-up. Results (2026-10-22, all 14 inputs): the RAM test identical on 165 899 frames, m4a's
RAM and PCM buffer identical, the screens identical but for the same 51 expected frames as
before; in a window (Xvfb) the keys, fullscreen and the pause behave as before.

### Testing the Windows build (under Wine)

The tests run the Windows build through Wine with `--port` (`port_test.py`, `video_test.py`):

```sh
sudo dpkg --add-architecture i386 && sudo apt install wine wine32:i386 mingw-w64
make -C port test-wine WINE_ARGS=--all        # the RAM test, every input
make -C port test-audio-wine WINE_ARGS=--all  # m4a's RAM and PCM buffer
make -C port test-video-wine WINE_ARGS=--all  # the pixel test
make -C port test-config-wine                 # kururin.ini and the gamepad (config_test.exe)
```

(`WINE` defaults to `env WINEDEBUG=-all WINEDLLOVERRIDES=mscoree,mshtml= wine`: quiet, and no
Mono/Gecko installers in a new prefix.) Results, Wine 9.0 in the cloud container, 2026-10-22:

| Test | Result |
| --- | --- |
| RAM (`test-wine`, all 14 inputs) | 165 899 compared frames, all identical to the original (the Linux build's count) |
| m4a's RAM and PCM buffer (`test-audio-wine`, all 14) | all identical |
| pixels (`test-video-wine`, all 14) | identical except the same 51 frames as the Linux build, where the original changed the video state while drawing them |
| sound output (`--wav`, boot_attract and save_ending) | byte-identical WAV files from the Linux and the Windows build (21 MB and 35 MB) |
| settings and gamepad (`test-config-wine`) | all pass (SDL's virtual joystick through SDL's Windows code) |
| the window (Xvfb + openbox, xdotool) | Start twice reaches the file select; F11 and Alt+Enter switch fullscreen (1280x1024 and back); focusing another window pauses (title "(paused)", frames stop), activating the game's window resumes; 60 frames a second |
| the package as a player gets it | unzipped into a new folder with `baserom.gba` next to the exe, started from another directory: the ROM found, `kururin.ini` and `kururin.sav` (a formatted blank cartridge, `make_save.py --show`) written next to the exe; without the ROM a message box names the path; with no sound device (none in the container) "no sound" and the game runs on |

Not checked here: a real Windows machine (Wine's memory layout differs from Windows's: the
open-bus handler is only exercised by an experiment, see above), real audio hardware, a real
gamepad. Under Wine the first run of a new prefix can take over 30 s (and once failed to create
the window while the prefix's desktop process was starting); later runs start in about a second.

### The Windows package

`make -C port dist-windows` builds `dist/kururin_pc_win32.zip` (`port/tools/package_windows.py`;
`dist/` is not tracked), a folder `kururin_pc/` holding:

| File | What |
| --- | --- |
| `kururin_pc.exe` | the game's code and the port (no ROM data) |
| `SDL2.dll` | SDL 2.32.10, SDL's own release build |
| `README.txt` | `port/dist/README.txt` with Windows line ends: where to put `baserom.gba`, controls, `kururin.ini`, saving, the command line, troubleshooting, licences |
| `LICENSE-SDL2.txt`, `README-SDL.txt` | SDL's zlib licence and the readme SDL asks to ship with the DLL |
| `LICENSE-mingw-w64.txt` | the mingw-w64 runtime's licences (ZPL 2.1 and others: the exe links its start-up code and libmingwex statically) |

Before writing the zip the script checks that none of these files holds ROM content: each
64-byte block of `baserom.gba` (at multiples of 64; blocks with fewer than 16 different byte
values, and counting tables such as 0, 1, 2, ... as bytes, halfwords or words, left out) is
looked for at every offset of every file, so any copied run of 127 bytes or more is found;
with 200 ROM bytes planted in the exe it refuses. (Without the counting-table rule it found
two in SDL2.dll that the ROM also has: the alphabet in UTF-16 and the halfwords 0x80-0xBF,
character tables, not the game's.) 38 257 blocks, 6 files, about a second.

The zip is downloaded from the repository's **GitHub Releases**: each release has the two
packages attached, outside every branch's history.

### The Linux package

`make -C port dist-linux` builds `dist/kururin_pc_linux_x86.tar.gz` (`port/tools/package_linux.py`),
a folder `kururin_pc/` holding:

| File | What |
| --- | --- |
| `kururin_pc` | the game's code and the port, 32-bit, `-O2`, stripped, RUNPATH `$ORIGIN` (no ROM data) |
| `libSDL2-2.0.so.0` | SDL 2.32.10, built from its source release by `port/tools/build_sdl2_linux.sh` (SHA-256 pinned), stripped |
| `kururin.sh` | the launcher, `port/dist/linux/kururin.sh` (below) |
| `README.txt` | `port/dist/linux/README.txt`: `baserom.gba`, the 32-bit libraries and the command per distribution, controls, `kururin.ini`, saving, troubleshooting, licences |
| `LICENSE-SDL2.txt`, `README-SDL.txt` | SDL's zlib licence and its readme |

**SDL, bundled.** The distributions' own `libSDL2` links X11, Wayland, PulseAudio, ALSA... directly,
so it would need every one of them installed (32-bit) just to load. The package's SDL is built with
every backend loaded at run time (`SDL_*_SHARED=ON`: X11, Wayland, KMS/DRM, ALSA, PulseAudio,
sndio, udev, D-Bus, libdecor; PipeWire's own API and libsamplerate left out), so it links only
libc and libm and uses whatever the system has. The program finds it next to itself (RUNPATH
`$ORIGIN`, so it can be started from anywhere, through a symlink too). What cannot be bundled
is the system's: glibc, X11's client libraries (they talk to the system's X server), the sound
client libraries (PulseAudio/PipeWire's server protocol, ALSA's configuration and plugins) and
OpenGL (Mesa's or NVIDIA's driver, which must match the system's kernel and X server). The game
needs glibc to start and X11 (`libX11`, `libXext`) for its window; without OpenGL SDL scales the
picture in software, without sound libraries it runs silent.

**glibc.** A program asks for the newest symbol versions of the glibc it was linked against: built
on Ubuntu 24.04 (glibc 2.39) the binary and SDL needed GLIBC_2.38 (`__isoc23_sscanf`,
`__isoc23_strtol`, `strlcpy`: glibc 2.38 redirects them under `_GNU_SOURCE`) and would not start
on Debian 12 (2.36) or Ubuntu 22.04 (2.35). The release is therefore built on Ubuntu 22.04:
`port/tools/build_linux_release.sh` clones HEAD into a temporary directory and runs
`make -C port dist-linux` in an `ubuntu:22.04` container (gcc 11, python3.11 from its
repository for `tomllib`, `libsdl2-dev:i386` for the headers SDL's build looks for); the result
needs GLIBC_2.34 at most. `package_linux.py` checks that (and fails above 2.34), along with: both
files i386 and stripped, the program's RUNPATH exactly `$ORIGIN`, its NEEDED only libSDL2 and
glibc's libraries, SDL's only glibc's; and no ROM content, with `package_windows.py`'s 64-byte
blocks (38 257 blocks, 6 files). The tarball is reproducible (fixed times, owner root, gzip
without a time stamp).

**The launcher**, `kururin.sh` (POSIX sh): checks the 32-bit loader (`/lib/ld-linux.so.2`, the
program's interpreter), then `ldd` on the program and SDL (`not found`), then the libraries SDL
loads at run time, which `ldd` cannot see: in `ldconfig -p`'s cache as 32-bit (`(libc6)`, not
`(libc6,x86-64)`), else as an i386 ELF file in the usual directories (`/usr/lib/i386-linux-gnu`,
`/usr/lib32`, `/usr/lib`...). Groups: to start (glibc), the window (`libX11`, `libXext`; or, on
Wayland without them, Wayland's libraries and EGL), recommended (`libpulse`, `libasound`,
`libGL` with Mesa's driver, X11's extras, `libudev`, `libdbus`). Missing ones give the install
command for the distribution (`/etc/os-release` `ID`/`ID_LIKE`, else whichever of
apt/dnf/pacman exists, else all three): Debian/Ubuntu with `dpkg --add-architecture i386` when
i386 is not enabled yet, Fedora's `.i686` packages, Arch's `lib32-` packages with the `sed` that
enables `[multilib]` when it is off. Without glibc it is one command for everything (a bare
64-bit system needs one step); a missing window is fatal; missing recommended libraries are a
warning and the game starts. Fatal messages also go to `kururin.log` and, with no terminal, to
a zenity/kdialog/xmessage dialog. `--check` lists every library; `--headless` needs only glibc;
`KURURIN_NO_CHECK=1` skips the check. The program writes `kururin.sav`, `kururin.ini` and
`kururin.log` next to itself (`SDL_GetBasePath`), as on Windows; on Linux its error messages go
to a message box only when stderr is not a terminal.

**Tested** (2026-10-22) in clean containers, each with the tarball unpacked as a normal user
into its home directory and `baserom.gba` copied next to the program:

| Distribution | glibc | Install command (printed by `kururin.sh`, run as printed) | Port tests through `kururin.sh` | Window (Xvfb) |
| --- | --- | --- | --- | --- |
| Debian 12 | 2.36 | `dpkg --add-architecture i386 && apt update && apt install libc6:i386 ...` | RAM all 14 inputs (165 899 frames), audio 3, pixels 4 | menu reached |
| Ubuntu 22.04 | 2.35 | the same | RAM 2, audio 1 | menu reached |
| Ubuntu 24.04 | 2.39 | the same (`libasound2:i386` resolves to `libasound2t64:i386`) | RAM 2, audio 1 | menu reached |
| Fedora 41 | 2.40 | `dnf install glibc.i686 libX11.i686 ...` | RAM 2, audio 1 | menu reached |
| Arch (2026-10) | 2.44 | `sed` enabling `[multilib]`, `pacman -Syu --needed lib32-glibc ...` | RAM 2, audio 1 | menu reached |

On each bare system `./kururin.sh` stopped with the command; after running it `./kururin.sh
--check` said everything was installed. (Container-only preparation, not part of the commands:
https apt sources for the session's proxy; on Arch a keyring, a mirror and pacman's
`DisableSandbox`, which Docker's seccomp needs.) The window run: `./kururin.sh` with
`DISPLAY` on the host's Xvfb, Enter twice through XTEST, the file-select screen drawn, exit
status 0, `kururin.ini` and `kururin.sav` written next to the program. With only
`libc6:i386` installed it printed the X11 + recommended command and `--headless` ran; with X11
alone it warned and ran (software scaling, no sound). Sound: under a PulseAudio server (null
sink) in the Debian container the game's stream plays (s16le stereo, 53 516 Hz); in the first
two seconds about a second of samples is dropped (`sound overrun`), the same with Debian's own
libSDL2, so it is the port's start-up against that virtual sink, not the package; real sound
hardware is not available here. A missing `baserom.gba` or watch file shows SDL's message box
on X11 and writes `kururin.log`. (The dropped start-up second was the port's: the device asks
for nothing for 1-2 s after it opens, and the game ran on meanwhile; fixed on 2026-10-29, "Frame
loop and ring buffer".)

The tarball is attached to the same GitHub Releases as the Windows zip.

## Changes outside port/

All of them preprocess to the same tokens for agbcc when `PLATFORM_PC` is not defined
(`make compare`: `kururin.gba` keeps its SHA1).

| File | Change |
| --- | --- |
| `include/include_asm.h` | `INCLUDE_ASM` declares nothing under `PLATFORM_PC` |
| `include/gba/macro.h`, `include/game.h` | `DmaSet`, `DMA3_SET` call `Port_Dma` under `PLATFORM_PC` |
| `src/main.c` | `Main_EndFrame`'s wait is `Port_WaitVBlank()` under `PLATFORM_PC` |
| `src/bg.c` | `Bg_LoadMap`'s direct DMA 3 register writes call `Port_Dma` under `PLATFORM_PC` |
| `src/m4a.c` | `SampleFreqSet`'s VCOUNT waits are `Port_WaitForLine159()` under `PLATFORM_PC` |
| `src/bg.c`, `src/main.c` | the I/O register copies reached from one base (`IO_COPY`, `IO_COPY_DISPCNT`/`IO_COPY_BG0CNT`) use `gDispcntCopy`'s address as a number or the globals themselves under `PLATFORM_PC`: GCC takes an index past one object as undefined (and as not aliasing the next one) |
| `include/game.h` | `memcpy`/`memset` take `__SIZE_TYPE__` under `PLATFORM_PC` (agbcc's built-ins need `unsigned long`) |
| `src/vs.c` | `Vs_DrawSpinner` calls `Sprite_SetAffineRotation` through its prototype under `PLATFORM_PC` (the GBA build keeps the original's call with an unnarrowed int, through a cast function type) |
| `src/m4a.c` | `ply_xwave` starts from 0 under `PLATFORM_PC` (pret's `UBFIX`; every byte is overwritten either way) |
| `src/vs.c` | `Vs_DrawSpinner` divides by the transfer's size only when it is not 0 under `PLATFORM_PC` (the GBA's libgcc gives 0; an x86 traps) |
| `src/level_map.c` | `LevelMap_Read`'s Piston and Shooter positions (`SET_POSITION`) are the fields under `PLATFORM_PC`; the GBA build keeps the original's misaligned word stores (to +0x0A/+0x0E, which the ARM7 writes at +0x08/+0x0C) |

### The port's build: port/Makefile only

The port has its own `port/Makefile`, and nothing else builds it: `make -C port`, or `make -f
port/Makefile` from the repository root (every path in it is absolute, from the file's own
location, so both are the same build, into `port/build/` and `port/kururin_pc`). The top
`Makefile` is locked (`tests/locked_files.txt`) and is not touched. There is no `make pc`: a
target added through the generated `modules.mk` would not be in any committed file (the maintainer's
decision, 2026-10-21; an earlier commit did that, and was reverted).

The list of game sources is read, not copied: `port/Makefile` takes the C modules from
`kururin.toml` (`[[modules]]` with `kind = "c"`, the list `scripts/generate_asm.py` writes into
`modules.mk` as `C_MODULES`), minus `agb_sram`, so a module added or split there is in the port
too, and the port does not need `modules.mk` to exist. The generated address tables
(`port/build/port_tables.c`, `gba_symbols.ld`) are read from `kururin.toml`,
`config/functions.txt` and `config/data_pointers.txt` the same way.

## Risks

Differences between the ARM and x86 that compiler options do not remove; none is known to be
hit yet, each would show as a divergence in the port test:

- **Shifts by 32 or more**: the ARM gives 0 (or the sign) for a register shift of 32-255; x86
  masks the count to 5 bits. Only matters for variable shift counts that can reach 32.
- **Division by zero**: agbcc's libgcc returns 0 (`Ldiv0` in `lib1thumb.asm`, after
  `__div0`), and `INT_MIN / -1` is `INT_MIN`; an x86 traps on both. `port/src/divide.c` catches
  the trap (SIGFPE, or Windows' `EXCEPTION_INT_DIVIDE_BY_ZERO` / `_OVERFLOW`), decodes the
  `div`/`idiv r/m32` that trapped and gives the GBA's quotient and remainder (logged once).
  Known divisors that could be 0: `Vs_DrawSpinner`'s total (0 when no MultiBoot guest needs the
  level data; also guarded in `src/vs.c` under `PLATFORM_PC`), and divisors read from the ROM's
  data (`Hud`'s heart animation, `LevelMap_Read`'s Piston period, `Sprite_SetAffineRotScale`),
  never 0 on the inputs.
- **Unaligned accesses**: the ARM rotates an unaligned word load and stores an unaligned word
  at the word below; x86 reads and writes the unaligned word. The one such store the game's C
  makes (`LevelMap_Read`, above) writes the fields under `PLATFORM_PC`.
- **Reads of address 0-0x3FFF** (the BIOS region): mapped on the GBA, not on the PC (the
  kernel's `mmap_min_addr`). The one known reader is the debug stage select's world TEST
  (`gWorldEntries[13]`, all zero: its world info is read from address 0); the port crashed
  there and now points that entry at an empty world at load (`port/src/rom.c`), so a TEST stage
  does not start (docs/unused_content.md, "The rebuilt ROM and the PC port in the stage
  select").
- **Uninitialised stack variables** hold different garbage on another compiler.
- **Timing**: lag frames (above), and anything the game derives from VCOUNT or timers.

## Status (deep review 3, 2026-10-29)

A review of `port/`, its build and packaging, after deep review 2's `src/` changes (the
PC-only fixes in `Vs_RunMenus`, `Vs_DrawSpinner`, the `bg.c`/`main.c` I/O copies,
`memcpy`/`memset`, `ply_xwave`).

**Fixed:**
- The debug stage select's world TEST crashed the port (a segmentation fault: its world info
  is read from address 0): an empty world now ("Risks"; docs/unused_content.md).
- An integer division by 0 (or `INT_MIN / -1`) in the game's C stopped the port with SIGFPE
  where the GBA gives 0: `port/src/divide.c` gives the GBA's results (Linux and Windows,
  checked with register and memory operands at -O0 and -O2, and under Wine);
  `Vs_DrawSpinner`'s total is also guarded in `src/vs.c` ("Risks").
- Up to a second of the game's first sound was dropped (the device starts 1-2 s after it
  opens), and the sound then stayed 0.3 s behind the picture for minutes; a device taking
  samples in bursts longer than that (a long-latency PulseAudio, as under WSL) ran short over
  and over. Now the game waits for the device, re-buffers once after an underrun with a target
  sized to the device's bursts (up to 267 ms) and drops the oldest samples when the device
  stalls ("Frame loop and ring buffer").
- `frame.c` read an input's `save <file>` with `%399s`: a path with a space (or longer than
  399 characters) was cut, and the run silently used a blank or another save. It now takes
  the rest of the line, as kurutest does, and stops when the image is missing; input lines up
  to 8 KiB, a negative lag frame number and failed allocations are handled.
- Licence: `video.c` is transcribed from mGBA's renderer: now under the MPL 2.0 with its
  notice, and the packages carry the MPL and the file's source (`port/NOTICE.md`).

**Added:** `--poke FRAME:ADDR:VALUE` (a byte written before a frame, e.g. the debug stage
select). **Removed:** `Port_FunctionName` (unused).

**Results** (Linux build, all 14 inputs): the RAM identical on 165 899 compared frames; m4a's
RAM and PCM buffer identical byte for byte on 165 899; the screens identical on 165 848 of
165 899, the other 51 the frames where the original changed the video state while drawing
them, as before. The Windows build under Wine: RAM on boot_attract and save_adventure, audio
on boot_attract, identical; its WAV output of boot_attract byte-identical to the Linux build's.
The debug stage select through all four modes (`--poke`): the RAM identical to the original on
every compared frame (5 911). The GBA build is unchanged (`kururin.gba` `d1fbc47f...`).

## Status (the Linux package, 2026-10-22)

**Done.** `dist/kururin_pc_linux_x86.tar.gz`: the 32-bit program built on
Ubuntu 22.04 (glibc 2.34 at most), SDL 2.32.10 with every backend loaded at run time, the
launcher `kururin.sh` that names the missing 32-bit libraries and the command that installs
them on Debian/Ubuntu, Fedora and Arch, and a README ("The Linux package"). Tested in clean
Debian 12, Ubuntu 22.04 and 24.04, Fedora 41 and Arch containers: each distribution's printed
command installs everything, the RAM, audio and pixel tests pass through the launcher, and the
window comes up under Xvfb.

**Not checked**: a real desktop (Wayland with XWayland, a real sound card, a gamepad, NVIDIA's
32-bit GL), glibc older than 2.35 (2.34 is what the symbols need), the start-up sound overrun
with a real PulseAudio/PipeWire device.

## Status (milestone 4, 2026-10-22)

**Done.** A Windows release: `make -C port windows` (32-bit, mingw-w64, the GBA memory map
with `VirtualAlloc`), `make -C port dist-windows` (the zip), and comfort
for both systems: `kururin.ini` (scale, fullscreen, volume, pause on focus loss, the keyboard
and gamepad mapping), gamepads, F11 / Alt+Enter fullscreen, pausing without the focus. Every
port test passes for the Windows build under Wine (RAM, m4a, pixels, all 14 inputs), its sound
output is byte-identical to the Linux build's, and the window and the unzipped package behave
as expected under Xvfb ("Testing the Windows build"). `make test`'s sound check now compares
what the game writes to the sound hardware (maintainer-approved; CLAUDE.md). The GBA build is
unchanged (`kururin.gba` `ceddea36...`).

**Not checked**: a real Windows machine since the fix below (the first release failed there:
EWRAM's addresses were taken, "The memory map on Windows: in the image"), where Windows puts
the main thread's stack (the open-bus handler covers it, tested only by an experiment), real
audio hardware and a real gamepad, Windows SmartScreen (the exe is not signed).

**Next**: a smoke test on real Windows (boot, a level, a save, a gamepad, fullscreen, sound);
a window icon; perhaps an installer-free 64-bit build (needs the pointer-width work milestone 1
avoided).

## Status (milestone 3, 2026-10-22)

**Done.** The game has its sound: `m4a_1.s` in C (`port/src/m4a_1.c`), exact: m4a's RAM and
the PCM buffer are the original's byte for byte on every compared frame of all 14 inputs
(`make -C port test-audio`); the hardware side (`audio.c`, `apu.c`) plays the buffer through the
two FIFOs and the four GB channels into SDL audio at 53 516 Hz, frame-paced, matching mGBA's
output in level and content ("The sound output"). The game saves by default to `kururin.sav`
next to the executable, as soon as it writes SRAM and at exit. The RAM, pixel and audio tests
pass; the GBA build is unchanged (`kururin.gba` `ceddea36...`, `make test`, `make shift-test`).

**Known limits of the sound.** The GB channels' register writes take effect 40 lines into the
frame, where the original typically makes them (the port runs a frame's work at once); 0.2% of
frames see an envelope or sweep step a 1/64 s tick from mGBA's. DirectSound is 0.77 ms earlier
than mGBA's (FIFO model). Not checked on real audio hardware here (the container has none; SDL's
`disk` driver runs fast and only shows that underruns are survived and logged).

**What is left for a Windows release.**
1. *The memory map*: `VirtualAlloc(MEM_RESERVE | MEM_COMMIT)` at each GBA address instead of
   `mmap(MAP_FIXED_NOREPLACE)` (EWRAM/IWRAM mirrors as views of one file mapping:
   `CreateFileMapping` + `MapViewOfFileEx`), the executable based above 0x10000000
   (`--image-base`), and no re-exec without address randomisation (`/DYNAMICBASE:NO` instead).
   The read-only open-bus region and the ROM's protection: `VirtualProtect`.
2. *The toolchain*: `i686-w64-mingw32-gcc` (32-bit, so pointers stay 4 bytes), SDL2's mingw
   development package, `port/Makefile` with a Windows target (`.exe`, `-mwindows` or a console),
   SDL2.dll shipped next to the executable.
3. *Small platform code*: `personality`/`/proc/self/maps` (Linux-only diagnostics) behind
   `#ifdef`; `signal` works; `sram.c` already removes the old save before `rename`; paths from
   `SDL_GetBasePath` (the default ROM path could use it too: `baserom.gba` next to the
   executable when it is not in the current directory).
4. *Testing on Windows*: the port test needs the libmgba tools (Linux here); a smoke test (boot,
   the title, a level, a save) and a listening test on real audio hardware.
5. *Comfort* (from milestone 2's list): a gamepad (SDL_GameController), remappable keys,
   fullscreen, a window icon.

## Status (milestone 2, 2026-10-22)

**Done.** The renderer (`port/src/video.c`) draws every display feature the game uses, the
HBlank DMA shapes the circle window line by line, and the window shows the frames at 59.73 Hz,
3x by default, with the keyboard as the GBA's keys. The pixel test (`make -C port test-video`)
finds every frame of all 14 inputs identical to the original's (mGBA's) screen, except 51
frames where the original changed VRAM or the window table while drawing them, which the port,
drawing a frame at once, cannot show (see "The pixel test"). The RAM still matches frame by
frame (`make -C port test`). The GBA build is unchanged.

**Before it is playable:**
1. *Sound*: `m4a_1.s` in C (`MPlayMain`, the `ply_*` sequencer commands, `SoundMain`'s mixer,
   the CGB channels) and an SDL audio stream; then the audio can be compared with kurutest's.
   The m4a clock (`m4aPlayer.clock`) then matches too.
2. *Saving by default*: SRAM is written only with `--save FILE`; a default file next to the
   ROM (and saving when the game writes SRAM, not only at exit) would keep progress.
3. *Comfort*: a gamepad (SDL_GameController), remappable keys, fullscreen, a window icon.
4. *Windows*: the same layout with `VirtualAlloc` and `i686-w64-mingw32-gcc` (milestone 1's
   plan, below).

## Status (milestone 1, 2026-10-21)

**Done.** `make -C port` builds `port/kururin_pc` (32-bit Linux, SDL2): a blank window at the GBA's
59.73 frames a second with the keyboard as the GBA's keys, or `--headless`. The GBA build is
unchanged (`kururin.gba` `ceddea36...`, `make test` and `make shift-test` pass).

**How far the RAM matches.** Every input of `tests/gameplay/inputs` (the 12 scripts and both
TASVideos movies, with the 100% movie's reset) is identical on every watched value on every
compared frame, with `m4aPlayer.clock` left out:

| Input | Frames | Original's lag frames (skipped) | Compared, all identical |
| --- | ---: | ---: | ---: |
| boot_attract | 6 000 | 15 | 5 985 |
| training1_moves | 2 319 | 27 | 2 292 |
| menus | 2 083 | 17 | 2 066 |
| training1_crash | 3 512 | 37 | 3 475 |
| see_map | 3 976 | 36 | 3 940 |
| save_corrupt | 824 | 14 | 810 |
| save_files | 6 766 | 41 | 6 725 |
| save_makeup | 4 096 | 38 | 4 058 |
| save_practice | 7 139 | 160 | 6 979 |
| save_adventure | 9 657 | 236 | 9 421 |
| save_challenge | 4 175 | 54 | 4 121 |
| save_ending | 9 669 | 41 | 9 628 |
| tasvideos_3933M_any | 22 194 | 162 | 22 032 |
| tasvideos_6116M_100 | 85 527 | 1 160 | 84 367 |
| **all** | **167 937** | **2 038** | **165 899** |

`m4aPlayer.clock` differs from frame 94 of `training1_moves` (when the first song plays): the
stubbed sequencer never advances it. The watch list covers the Helirins, `gMain`, `gGameState`,
`gCamera` and the save files; RAM outside it, the screen and the audio are not compared yet.

**Stubbed.** The m4a sequencer and mixer (`m4a_1.s`: no sound, songs do not advance), the
renderer (the window is blank; VRAM, OAM and palette RAM are filled as on the GBA), timed DMA
(HBlank: the window shapes; sound FIFO), MultiBoot and the link cable (`MultiBoot` fails, no
serial interrupts), the BIOS region below 0x10000.

**Next blockers.**
1. *Graphics*: a renderer from VRAM, OAM, palette RAM and the I/O registers: BG modes 0-2 with
   the affine BG 2 (`Effect_RotateBg2`), regular and affine sprites, window 0 shaped per line by
   HBlank DMA (`Bg_WindowLineIntr`), BLDY fades. The port test can then compare the screen with
   kurutest's framebuffer hash.
2. *Sound and the m4a state*: `m4a_1.s` in C (`MPlayMain` and the `ply_*` commands, which also
   make `m4aPlayer.clock` comparable; then `SoundMain`'s mixer and the CGB channels into SDL
   audio).
3. *Windows*: the same layout with `VirtualAlloc` and `i686-w64-mingw32-gcc`.

## Where I stopped / what's next

A short note at each commit of the port, newest last.

- 2026-10-21, design (`417577b`): docs/port.md written. Next: memory map, ROM loading, build.
- 2026-10-21, the port (`ddb28a4`): memory map, ROM loading and patching, the shims, headless
  mode and the dump, in one commit. Next: the port test.
- 2026-10-21, the port test (`a58ac7b`, docs `f18cbf1`): all 14 inputs identical. Next: graphics.
- 2026-10-21, build: `make pc` (through the generated `modules.mk`) removed; the port builds only
  with `port/Makefile` (`make -C port`, `make -f port/Makefile`), its game sources read from
  `kururin.toml`. The port test still passes (boot_attract, training1_moves). Next: the
  renderer (Next blockers, 1), then `m4a_1.s` in C, then Windows.
- 2026-10-22, milestone 2, step 1 (the display survey): `port/test/survey.c` (`make -C port
  survey`) and its results ("What the game draws with"). Next: the scanline renderer.
- 2026-10-22, milestone 2, step 2 (the renderer): `port/src/video.c`, the HBlank DMA and the
  VCount matches before the lines (`hw.c`), the BIOS's I/O values at power-on and reset
  (`memory.c`). The RAM port test still passes. Next: the pixel test (`make -C port
  test-video`).
- 2026-10-22, milestone 2, step 3 (the pixel test): `make -C port test-video`
  (`port/test/video_test.py`; refdump `--video`, `--shot` with states, `--mid-frame`; the port's
  `--video`, `--shot`, `--shot-dir`). Fixed on the way: text BGs drawn tile by tile as mGBA does,
  write-only registers read as open bus by CpuSet and DMA. All 14 inputs identical except 51
  frames where the original changed the video state while drawing them. Next: the SDL window.
- 2026-10-22, milestone 2, step 4 (the window): the frames in the SDL window, integer scaling
  (`--scale`, default 3), keys Z = A, X = B, A = L, S = R, Enter = Start, Backspace = Select;
  checked under Xvfb (keys through XTEST, the window's pixels). `--headless` unchanged.
  Milestone 2 is done. Next: sound (`m4a_1.s` in C and SDL audio), a default save file, then
  Windows.
- 2026-10-22, milestone 3, step 1 (the sound survey): `port/test/sound_survey.c` (`make -C port
  sound-survey`) and its results ("What the game's sound uses"): DirectSound A right / B left at
  13 379 Hz, 224 samples a frame, reverb 50, up to 12 channels; all four CGB channels (sweep on
  channel 1, one wave bank, 15-bit noise). Next: `m4a_1.s` in C.
- 2026-10-22, milestone 3, step 2 (`m4a_1.s` in C): `port/src/m4a_1.c` (the mixer, `SoundMain`,
  `MPlayMain`, `ply_note`, every sequencer command, `m4aSoundVSync`), the sound DMAs' restart in
  `hw.c`, the original's lag VBlanks raised where m4a's counter needs them, `SampleFreqSet`'s
  wait as `Port_WaitForLine159` (`src/m4a.c`, `#ifdef PLATFORM_PC`). `m4aPlayer.clock` now
  matches: the RAM port test passes without ignoring it. Next: the audio test.
- 2026-10-22, milestone 3, step 3 (the audio test): `make -C port test-audio`
  (`port/test/audio_watch.txt`, `port_test.py --extra-watch`, `refdump --timing`'s m4a columns).
  m4a's RAM and the PCM buffer are identical to the original's on every compared frame of all
  14 inputs. Next: the sound output (SDL audio, the FIFOs, the GB channels).
- 2026-10-22, milestone 3, step 4 (the sound output): `port/src/audio.c` (the FIFOs fed by the
  sound DMAs, the mix, the ring buffer and SDL audio, pacing, `--mute`, `--volume`, `--wav`) and
  `port/src/apu.c` (the four GB channels); `refdump --audio/--psg-trace/--psg-writes` to compare
  with mGBA: levels within 0.5%, DirectSound the same samples, GB channels' state identical on
  99.8% of frames. Found: `make test`'s audio hashes are of silence (kurutest's mGBA volume is 0).
  Next: saving by default.
- 2026-10-22, milestone 3, step 5 (saving by default): `kururin.sav` next to the executable,
  written 0.5 s after the game last wrote SRAM (temporary file and rename) and at exit (window,
  Ctrl+C, SIGTERM); `--no-save`; checked byte-identical to the original's SRAM after `menus`.
  Milestone 3 is done. Next: Windows ("Status (milestone 3)": the memory map with
  `VirtualAlloc`, mingw-w64 32-bit, SDL2.dll), then a gamepad and fullscreen.
- 2026-10-22, the sound check of `make test` (maintainer-approved): kurutest compares the writes to
  the sound registers (CLAUDE.md). The port's dump takes entries up to 4 KiB and treats
  `finished` entries as raw; refdump gets `audio_watch.txt` without `raw`; `port_test.py --port`.
- 2026-10-22, milestone 4, step 1 (the Windows build): `make -C port windows`
  (`port/kururin_pc.exe`, `SDL2.dll`), the memory map with `VirtualAlloc` and the open-bus
  exception handler, the PE image at 0x10000000 without ASLR or relocations, `gen_port_tables.py`
  without weak symbols, message boxes and the parent console. Runs under Wine. Next: comfort
  (gamepad, fullscreen, kururin.ini, pause on focus loss).
- 2026-10-22, milestone 4, step 2 (comfort): `port/src/config.c` (`kururin.ini`), gamepads,
  F11 / Alt+Enter fullscreen, pausing on focus loss, `--config`, `--fullscreen`;
  `make -C port test-config`; checked under Xvfb + openbox. Next: the Windows build under Wine
  through the RAM, video and audio tests.
- 2026-10-22, milestone 4, step 3 (Wine): `make -C port test-wine`, `test-audio-wine`,
  `test-video-wine`, `test-config-wine` (`--port` for `video_test.py`); every test passes under
  Wine on all 14 inputs, the WAV output is byte-identical, the window and the unzipped package
  behave as on Linux ("Testing the Windows build"). Next: the package (`dist-windows`).
- 2026-10-22, milestone 4, step 4 (the package): `make -C port dist-windows`
  (`port/tools/package_windows.py`, `port/dist/README.txt`), checked for ROM content. Milestone 4 is done ("Status (milestone 4)"). Next: real
  Windows.
- 2026-10-22, Windows 11: the first release stopped at start-up (EWRAM's addresses taken,
  error 487). The GBA's memory is now part of the exe's image (`port/tools/win_image.py`), with a
  `VirtualQuery` map of the range in every such error and in `kururin.log`; tested under Wine with
  the ranges taken before start-up (`port/test/occupy.c`, `OCCUPIED=1`): RAM, audio and pixel
  tests pass on all 14 inputs. The zip rebuilt. Next: real Windows again.
- 2026-10-22, the Linux package: `make -C port dist-linux` (`port/tools/build_sdl2_linux.sh`,
  `package_linux.py`, `port/dist/linux/`), `port/tools/build_linux_release.sh` (Ubuntu 22.04,
  for glibc 2.34); `Port_Die` writes `kururin.log` on Linux too and shows a box when stderr is
  not a terminal. Tested on five distributions ("The Linux package"); the tarball is built
  next to the Windows zip. Next: real hardware, both systems.
- 2026-10-29, deep review 3 ("Status (deep review 3)"): TEST and division-by-zero crashes fixed
  (`rom.c`, `divide.c`, `src/vs.c` under `PLATFORM_PC`), the sound's start-up and buffering
  (`audio.c`), `frame.c`'s `save` paths, `--poke`; `video.c` under the MPL 2.0
  (`port/NOTICE.md`); downloads through GitHub Releases. RAM, audio and pixel tests as before
  on all 14 inputs. Next: real hardware, both systems; the choice of licence.
