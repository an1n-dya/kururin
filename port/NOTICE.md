# The PC port: third-party code and licences

What the port (`port/`) and its packages contain that is not the project's own work, and the
terms that come with it. The licences of the rest of the repository are in
the top-level `LICENSE`, `LICENSES/` and `NOTICE.md`; nothing here grants rights in Kuru Kuru Kururin, which is
(c) Nintendo / Eighting, or in the Nintendo SDK code that `src/` and `port/src/m4a_1.c`
reconstruct.

## mGBA (MPL-2.0): `port/src/video.c`

`port/src/video.c`, the renderer, is derived from mGBA's software renderer (mGBA 0.10.5,
https://mgba.io, Copyright (c) 2013-2015 Jeffrey Pfau): its window splitting
(`_breakWindowInner`), compositing and pixel flags (`_compositeBlendNoObjwin`,
`FLAG_PRIORITY` ... `FLAG_TARGET_2`), colour arithmetic (`_brighten`, `_darken`,
`mColorMix5Bit`), text BG partial-tile drawing (`software-mode0.c`) and sprite drawing
(`software-obj.c`, including its texel addressing and the skip at the start of an affine
sprite) follow mGBA's code closely, because the pixel test compares the port with mGBA's
pixels, quirks included (docs/port.md, "The renderer"). A comparison on 2026-10-29 found it
transcribed, not independently written, so the file is **Covered Software under the Mozilla
Public License 2.0**:

- the file carries the MPL's notice (Exhibit A) and mGBA's copyright line;
- the MPL's text is `port/LICENSE-MPL-2.0.txt`;
- the MPL is a file-level copyleft: `video.c` stays under it, with its source available to
  whoever gets the program (section 3.2); the other files of the port and the game are a
  "Larger Work" (section 3.3) and are not affected;
- both packages ship `LICENSE-MPL-2.0.txt` and the file's source as `source/video.c`
  (`port/tools/package_windows.py`, `package_linux.py`), so the Source Code Form goes with
  every copy of the program.

Changes to `video.c` stay under the MPL 2.0. Rewriting it from a description instead was
considered and not done: its output must stay identical to mGBA's on every frame, including
mGBA's own quirks, so a rewrite by someone who has read mGBA's renderer would not be
independent in any useful sense, and the MPL's terms are easy to meet.

**Compared and independent** (2026-10-29, the same comparison): `hw.c` (DMA, the I/O reads,
CpuSet/CpuFastSet, interrupts), `memory.c` (the memory map; the I/O start values are the
hardware's, as mGBA's `GBAIOInit` also sets them), `apu.c` (the GB channels: a cycle-stepped
model unlike mGBA's event-driven `gb/audio.c`), `audio.c` (the FIFOs and the mix: the
hardware's mixing rules, the same formula as mGBA's `GBAAudioSample`, written differently, in
floating point with its own resampling and filter), `frame.c`, `main.c`, `config.c`, `sram.c`,
`rom.c`, `divide.c`. They reproduce mGBA's *behaviour* where the tests compare with it
(register masks, open-bus values of its HLE BIOS, the mixing scale), which is not its code.
`m4a_1.c` is transcribed from this ROM's own `m4a_1.s` (Nintendo's sound library), like `src/`.
`port/test/refdump.c`, `survey.c` and `sound_survey.c` link libmgba but are local test tools,
never distributed.

## SDL2 (zlib)

The packages ship SDL 2.32.10 (`SDL2.dll`, `libSDL2-2.0.so.0`), https://www.libsdl.org/, under
the zlib licence, with `LICENSE-SDL2.txt` and `README-SDL.txt`.

## mingw-w64 runtime and libgcc (Windows), glibc (Linux)

`kururin_pc.exe` links the mingw-w64 runtime's start-up code and libmingwex statically (ZPL 2.1
and others: `LICENSE-mingw-w64.txt` in the zip) and GCC's libgcc (GPL with the GCC Runtime
Library Exception). The Linux program links libgcc statically (the same exception) and uses
the system's glibc (LGPL) dynamically.
