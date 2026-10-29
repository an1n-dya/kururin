#!/usr/bin/env python3
"""Packages the Linux build (docs/port.md, "The Linux package"): make -C port dist-linux.

usage: package_linux.py OUT.tar.gz BINARY SDL2_DIR

Writes OUT.tar.gz, one directory kururin_pc/ with the program kururin_pc, libSDL2-2.0.so.0
(SDL2_DIR/prefix/lib, stripped here), the launcher kururin.sh and README.txt (port/dist/linux/),
SDL's licence notices (LICENSE-SDL2.txt, README-SDL.txt from SDL2_DIR's source), and the MPL 2.0
(LICENSE-MPL-2.0.txt) with the source of the one file under it, source/video.c (port/NOTICE.md:
the MPL asks for the Source Code Form to go with the program). The archive
is reproducible: fixed times, owner root, files in a fixed order.

Before that it checks what it packages, and exits 1 (writing nothing) if one check fails:
- no ROM content, as package_windows.py checks it (its 64-byte blocks of baserom.gba);
- both ELF files are 32-bit i386 and stripped; the program finds libSDL2 next to itself
  (RUNPATH $ORIGIN) and needs nothing but libSDL2 and glibc's libraries; libSDL2 needs only
  glibc's (SDL's backends are loaded at run time);
- no symbol version newer than glibc MAX_GLIBC: the oldest glibc the package runs on, which
  README.txt names (a build on a newer system picks up newer versions, e.g. glibc 2.38's
  __isoc23_strtol: port/tools/build_linux_release.sh builds on Ubuntu 22.04, whose glibc 2.35
  gives 2.34).
"""
import gzip
import io
import os
import re
import subprocess
import sys
import tarfile
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from package_windows import ROOT, baserom_blocks, rom_content  # noqa: E402

MAX_GLIBC = (2, 34)  # Fedora 35, RHEL 9; Ubuntu 22.04 has 2.35, Debian 12 2.36
GLIBC_LIBS = {"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1", "ld-linux.so.2"}
MTIME = 1792627200  # 2026-10-22 00:00 UTC


def readelf(path, *args):
    return subprocess.run(["readelf", "-W", *args, path], check=True, capture_output=True, text=True).stdout


def check_elf(name, path, needed_ok, runpath):
    errors = []
    header = readelf(path, "-h")
    if "ELF32" not in header or "Intel 80386" not in header:
        errors.append("not a 32-bit i386 ELF file")
    if ".symtab" in readelf(path, "-S"):
        errors.append("not stripped")
    dynamic = readelf(path, "-d")
    needed = set(re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", dynamic))
    if needed - needed_ok:
        errors.append(f"needs {', '.join(sorted(needed - needed_ok))}")
    rp = re.findall(r"\((?:RUNPATH|RPATH)\)\s+Library (?:runpath|rpath): \[([^\]]*)\]", dynamic)
    if rp != ([runpath] if runpath else []):
        errors.append(f"RUNPATH {rp}, want {runpath or 'none'}")
    versions = {tuple(map(int, v)) for v in re.findall(r"GLIBC_(\d+)\.(\d+)", readelf(path, "-V"))}
    newest = max(versions, default=(0, 0))
    if newest > MAX_GLIBC:
        errors.append(f"needs glibc {newest[0]}.{newest[1]} (at most {MAX_GLIBC[0]}.{MAX_GLIBC[1]} allowed)")
    if errors:
        sys.exit(f"package_linux.py: {name}: " + "; ".join(errors))
    return newest


def stripped(path):
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "lib")
        subprocess.run(["strip", "--strip-all", "-o", out, path], check=True)
        return open(out, "rb").read()


def main():
    out, binary, sdl = sys.argv[1:4]
    blocks = baserom_blocks()
    sdl = os.path.abspath(sdl)
    sources = [d for d in os.listdir(sdl) if d.startswith("SDL2-") and os.path.isdir(os.path.join(sdl, d))]
    if len(sources) != 1:
        sys.exit(f"package_linux.py: {sdl}: expected one SDL2-<version>/ source directory, found {sources}")
    sdl_src = os.path.join(sdl, sources[0])
    dist = os.path.join(ROOT, "port", "dist", "linux")
    files = [  # (name, data, mode)
        ("kururin_pc", open(binary, "rb").read(), 0o755),
        ("libSDL2-2.0.so.0", stripped(os.path.join(sdl, "prefix", "lib", "libSDL2-2.0.so.0")), 0o644),
        ("kururin.sh", open(os.path.join(dist, "kururin.sh"), "rb").read(), 0o755),
        ("README.txt", open(os.path.join(dist, "README.txt"), "rb").read(), 0o644),
        ("LICENSE-SDL2.txt", open(os.path.join(sdl_src, "LICENSE.txt"), "rb").read(), 0o644),
        ("README-SDL.txt", open(os.path.join(sdl_src, "README-SDL.txt"), "rb").read(), 0o644),
        ("LICENSE-MPL-2.0.txt", open(os.path.join(ROOT, "port", "LICENSE-MPL-2.0.txt"), "rb").read(), 0o644),
        ("source/video.c", open(os.path.join(ROOT, "port", "src", "video.c"), "rb").read(), 0o644),
    ]
    with tempfile.TemporaryDirectory() as tmp:
        newest = []
        for name, data, _ in files[:2]:
            path = os.path.join(tmp, name)
            open(path, "wb").write(data)
            if name == "kururin_pc":
                newest.append(check_elf(name, path, GLIBC_LIBS | {"libSDL2-2.0.so.0"}, "$ORIGIN"))
            else:
                newest.append(check_elf(name, path, GLIBC_LIBS, None))
    newest = max(newest)
    print(f"ELF checks: i386, stripped, RUNPATH $ORIGIN, glibc {newest[0]}.{newest[1]} at most")
    for name, data, _ in files:
        at = rom_content(data, blocks)
        if at is not None:
            sys.exit(f"package_linux.py: {name} holds ROM content at offset 0x{at:X}: not packaged")
    print(f"no ROM content: {len(blocks)} blocks of baserom.gba looked for in {len(files)} files")

    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.GNU_FORMAT) as tar:
        def add(name, data, mode, kind=tarfile.REGTYPE):
            info = tarfile.TarInfo("kururin_pc/" + name if name else "kururin_pc")
            info.type, info.mode, info.mtime, info.size = kind, mode, MTIME, len(data)
            info.uid = info.gid = 0
            info.uname = info.gname = "root"
            tar.addfile(info, io.BytesIO(data) if data else None)
        add("", b"", 0o755, tarfile.DIRTYPE)
        add("source", b"", 0o755, tarfile.DIRTYPE)
        for name, data, mode in files:
            add(name, data, mode)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    tmp = out + ".tmp"
    with open(tmp, "wb") as f, gzip.GzipFile(filename="", mode="wb", fileobj=f, compresslevel=9, mtime=0) as gz:
        gz.write(raw.getvalue())
    os.replace(tmp, out)
    print(f"{out}: {os.path.getsize(out)} bytes, {', '.join(n for n, _, _ in files)}")


if __name__ == "__main__":
    main()
