#!/usr/bin/env python3
"""Packages the Windows build (docs/port.md, "The Windows package"): make -C port dist-windows.

usage: package_windows.py OUT.zip EXE SDL2_DIR

Writes OUT.zip with kururin_pc.exe, SDL2.dll, README.txt (port/dist/README.txt, with Windows
line ends) and the licence notices (SDL2's LICENSE.txt and README-SDL.txt, mingw-w64's
copyright file; the MPL 2.0 with the source of the one file under it, source/video.c:
port/NOTICE.md). Before that it checks that no ROM content is in what it packages: every
64-byte block of baserom.gba (at multiples of 64, leaving out blocks of fewer than 16 different
byte values: padding, fills) is looked for at every offset of each file, so any run of 127
bytes or more copied from the ROM would be found. Exits 1 (and writes nothing) if one is.
Blocks whose bytes step evenly (1, 2 or 4 interleaved arithmetic progressions: counting tables
of bytes, halfwords or words) are left out too: SDL2.dll has two such tables the ROM also holds (the alphabet in
UTF-16, 52 bytes, and the 16-bit values 0x80-0xBF in order, 149 bytes), which are not the
game's content.
"""
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BLOCK = 64


def counting(b):
    """Whether the bytes are 1, 2 or 4 interleaved even steps (a table of 0, 1, 2, ..., also as
    16- or 32-bit values): at a stride s, each of the s phases has one difference."""
    return any(all(len({(b[k + s] - b[k]) & 0xFF for k in range(r, len(b) - s, s)}) == 1 for r in range(s))
               for s in (1, 2, 4))


def rom_blocks(rom):
    blocks = set()
    for i in range(0, len(rom) - BLOCK + 1, BLOCK):
        b = rom[i:i + BLOCK]
        if len(set(b)) >= 16 and not counting(b):
            blocks.add(b)
    return blocks


def baserom_blocks():
    """rom_blocks of the user's baserom.gba, which the check needs (it is not in git)."""
    path = os.path.join(ROOT, "baserom.gba")
    if not os.path.isfile(path):
        sys.exit(f"{path} is missing: the packaging checks that none of the ROM is packaged, which needs it")
    return rom_blocks(open(path, "rb").read())


def rom_content(data, blocks):
    """The first offset of data where a ROM block starts, or None."""
    for i in range(len(data) - BLOCK + 1):
        if data[i:i + BLOCK] in blocks:
            return i
    return None


def main():
    out, exe, sdl = sys.argv[1:4]
    blocks = baserom_blocks()
    sdl_root = os.path.dirname(sdl.rstrip("/"))  # SDL2-<version>/ (sdl is its i686-w64-mingw32/)
    readme = open(os.path.join(ROOT, "port", "dist", "README.txt"), "rb").read()
    files = [
        ("kururin_pc.exe", open(exe, "rb").read()),
        ("SDL2.dll", open(os.path.join(sdl, "bin", "SDL2.dll"), "rb").read()),
        ("README.txt", readme.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")),
        ("LICENSE-SDL2.txt", open(os.path.join(sdl_root, "LICENSE.txt"), "rb").read()),
        ("README-SDL.txt", open(os.path.join(sdl_root, "README-SDL.txt"), "rb").read()),
        ("LICENSE-mingw-w64.txt", open("/usr/share/doc/mingw-w64-common/copyright", "rb").read()),
        ("LICENSE-MPL-2.0.txt", open(os.path.join(ROOT, "port", "LICENSE-MPL-2.0.txt"), "rb").read()),
        ("source/video.c", open(os.path.join(ROOT, "port", "src", "video.c"), "rb").read()),
    ]
    for name, data in files:
        at = rom_content(data, blocks)
        if at is not None:
            sys.exit(f"package_windows.py: {name} holds ROM content at offset 0x{at:X}: not packaged")
    print(f"no ROM content: {len(blocks)} blocks of baserom.gba looked for in {len(files)} files")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    tmp = out + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in files:
            info = zipfile.ZipInfo("kururin_pc/" + name, date_time=(2026, 10, 22, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
    os.replace(tmp, out)
    print(f"{out}: {os.path.getsize(out)} bytes, {', '.join(n for n, _ in files)}")


if __name__ == "__main__":
    main()
