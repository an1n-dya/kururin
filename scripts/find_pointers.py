#!/usr/bin/env python3
"""Find the absolute ROM pointers inside the data and write config/data_pointers.txt.

Everything after the code (0x0801CB64 to the end of the ROM) is incbin'd. Some of its words
are absolute addresses of functions or of other data; scripts/generate_asm.py emits each one
listed in config/data_pointers.txt as `.4byte <symbol>`, so the linker relocates it when code
or data moves. A pointer missed here keeps the old address after a move; a non-pointer listed
here changes by the move. Both are only visible when the layout changes (`make shift-test`).

Where pointers are looked for:

  ranges   kururin.toml [pointers] ranges: the game's own tables (and m4a's voice groups and
           song table). Every word-aligned word there whose value is a ROM address is a
           pointer, unless
             * its own bytes were read by ldrb/ldrh (it is part of a byte/halfword table:
               "narrow" in the report), or
             * its value is inside the code but not a function entry ("code-not-entry"), or
             * its target is not word-aligned and it is not part of a table: no candidate at
               the same stride on either side (two in a row) whose target is within 64 KiB
               ("unaligned-alone"; tables of string or palette pointers have such targets), or
             * kururin.toml [pointers] not_pointers lists it (each with a reason).
  m4a      the song table ([pointers] song_table) is parsed exactly: every song header (voice
           group and track pointers), and every track's command stream for the pointers of
           GOTO, PATT, REPT and XCMD xWAVE (these are not word-aligned).
  elsewhere graphics, maps and samples: nothing is a pointer.

The evidence from running the original (`make pointer-evidence`: tests/gameplay inputs run with
kurutest --rom-loads, written to build/test/*.loads) is required and checked: every ROM-valued
word the game's own code loads with ldr/ldm must be a pointer found above, and a word loaded
with ldrb/ldrh is never one. The script exits 1 when that check fails, when an m4a track does
not parse or a jump leaves its song, or when an m4a pointer lies in a range.

Outputs (committed):
  config/data_pointers.txt   location, value, kind (code/data/m4a) and evidence per pointer
  config/pointer_report.txt  statistics and every rejected candidate in the ranges

Usage:
  python3 scripts/find_pointers.py [--evidence DIR]
"""

from __future__ import annotations

import argparse
import collections
import glob
import os
import struct
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROM_BASE = 0x08000000

# m4a track commands (MusicPlayer2000's MPlayMain / gMPlayJumpTable, with MPlayExtender's
# additions: B9 MEMACC, CD XCMD): argument bytes after the command byte.
M4A_FINE = {0xB1, 0xB6, 0xB7, 0xB8, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB}
M4A_ONE_ARG = set(range(0xBA, 0xC6)) | {0xC8, 0xCC}
M4A_XCMD_WAVE = 0x01  # xWAVE: a 4-byte wave pointer; the other xcmds take one byte
M4A_XCMD_END = {0x00, 0x03}  # ply_xxx: ends the track like FINE


class Rom:
    def __init__(self, data: bytes):
        self.data = data
        self.end = ROM_BASE + len(data)

    def u8(self, a):
        return self.data[a - ROM_BASE]

    def u32(self, a):
        return struct.unpack_from("<I", self.data, a - ROM_BASE)[0]

    def is_rom(self, v):
        return ROM_BASE <= v <= self.end


def load_functions():
    funcs = {}
    for line in open(os.path.join(ROOT, "config", "functions.txt")):
        p = line.split("#", 1)[0].split()
        if p and p[0] not in ("farjump", "noreturn"):
            funcs[int(p[0], 16)] = p[1]
    return funcs


def load_evidence(directory):
    words, narrow = set(), set()
    files = sorted(glob.glob(os.path.join(directory, "*.loads")))
    for path in files:
        for line in open(path):
            kind, lo, hi = line.split()
            lo, hi = int(lo, 16), int(hi, 16)
            if kind == "w":
                words.update(range(lo, hi, 4))
            else:
                narrow.update(range(lo, hi))
    return files, words, narrow


# ---------------------------------------------------------------------------
# m4a songs
# ---------------------------------------------------------------------------


def parse_track(rom: Rom, start: int, problems: list[str]):
    """Commands of one track from *start* to its FINE: (pointer locations with kinds, command
    addresses, end)."""
    pos, status = start, None
    pointers, cmds = [], set()
    while True:
        if not (ROM_BASE <= pos < rom.end):
            problems.append(f"m4a track {start:08X}: runs off the ROM")
            return pointers, cmds, pos
        cmds.add(pos)
        c = rom.u8(pos)
        if c < 0x80:  # running status: the byte is an argument of the previous command
            if status is None:
                problems.append(f"m4a track {start:08X}: argument byte {c:02X} at {pos:08X} with no running status")
                return pointers, cmds, pos
            cmd = status
        else:
            pos += 1
            cmd = c
            if cmd >= 0xBD:
                status = cmd
        if cmd >= 0xCF or cmd == 0xCE:  # note (key, velocity, gate) / EOT (key): optional args
            for _ in range(3 if cmd >= 0xCF else 1):
                if rom.u8(pos) < 0x80:
                    pos += 1
                else:
                    break
        elif cmd <= 0xB0 or cmd == 0xB4:  # wait, PEND
            pass
        elif cmd in (0xB2, 0xB3):  # GOTO, PATT
            pointers.append((pos, "goto" if cmd == 0xB2 else "patt"))
            pos += 4
        elif cmd == 0xB5:  # REPT count, address
            pointers.append((pos + 1, "rept"))
            pos += 5
        elif cmd == 0xB9:  # MEMACC op, address, data
            pos += 3
        elif cmd in M4A_ONE_ARG:
            pos += 1
        elif cmd == 0xCD:  # XCMD type, args
            sub = rom.u8(pos)
            pos += 1
            if sub == M4A_XCMD_WAVE:
                pointers.append((pos, "xwave"))
                pos += 4
            elif sub in M4A_XCMD_END:
                return pointers, cmds, pos
            else:
                pos += 1
        elif cmd in M4A_FINE:
            return pointers, cmds, pos
        else:
            problems.append(f"m4a track {start:08X}: unknown command {cmd:02X} at {pos - 1:08X}")
            return pointers, cmds, pos


def parse_songs(rom: Rom, table: int, problems: list[str]):
    """Pointers of the song table, the song headers and their tracks.

    Returns ({location: (value, kind)}, [(lo, hi)] byte ranges the song data covers)."""
    found = {}
    entries = []
    a = table
    while rom.is_rom(rom.u32(a)) and (not entries or a < min(entries)):
        entries.append(rom.u32(a))
        found[a] = (rom.u32(a), "song")
        a += 8  # {SongHeader *, u16 ms, u16 me}
    covered = []
    for header in sorted(set(entries)):
        ntracks = rom.u8(header)
        tone = header + 4
        if rom.is_rom(rom.u32(tone)):
            found[tone] = (rom.u32(tone), "voicegroup")
        tracks = []
        for i in range(ntracks):
            loc = header + 8 + 4 * i
            found[loc] = (rom.u32(loc), "track")
            tracks.append(rom.u32(loc))
        covered.append((header, header + 8 + 4 * ntracks))
        cmds, jumps = set(), []
        for t in tracks:
            ptrs, c, end = parse_track(rom, t, problems)
            cmds |= c
            covered.append((t, end))
            for loc, kind in ptrs:
                found[loc] = (rom.u32(loc), kind)
                if kind != "xwave":
                    jumps.append((loc, kind, rom.u32(loc)))
        for loc, kind, target in jumps:
            if target not in cmds:
                problems.append(f"m4a song {header:08X}: {kind} at {loc:08X} to {target:08X}, "
                                "which is not a command of the song")
    return found, covered


# ---------------------------------------------------------------------------
# Ranges
# ---------------------------------------------------------------------------


def scan_ranges(rom: Rom, ranges, funcs, code_end, narrow, not_pointers):
    """Pointers in the configured ranges: ({location: (value, kind)}, {location: reason})."""
    cands = {}
    for lo, hi in ranges:
        for a in range(lo + (-lo) % 4, hi - 3, 4):
            v = rom.u32(a)
            if rom.is_rom(v):
                cands[a] = v
    found, rejected = {}, {}
    for a, v in sorted(cands.items()):
        if a in not_pointers:
            rejected[a] = "not_pointers"
        elif any(b in narrow for b in range(a, a + 4)):
            rejected[a] = "narrow"
        elif v < code_end:
            entry = v & ~1
            if entry in funcs and (v & 1) == (funcs[entry] == "thumb"):
                found[a] = (v, "code")
            else:
                rejected[a] = "code-not-entry"
        elif v % 4 and not in_table(a, v, cands):
            rejected[a] = "unaligned-alone"
        else:
            found[a] = (v, "data")
    return found, rejected


def in_table(a, v, cands):
    """Two more candidates at the same stride before, after or around *a*, with targets
    within 64 KiB of *v*: a table, not a stray word."""
    def near(b):
        return b in cands and abs(cands[b] - v) < 0x10000
    for stride in range(4, 0x24, 4):
        if (near(a - stride) and near(a - 2 * stride)) or (near(a + stride) and near(a + 2 * stride)) \
                or (near(a - stride) and near(a + stride)):
            return True
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--evidence", default=os.path.join(ROOT, "build", "test"),
                    help="directory with kurutest --rom-loads files (*.loads)")
    args = ap.parse_args()

    with open(os.path.join(ROOT, "kururin.toml"), "rb") as f:
        cfg = tomllib.load(f)
    rom = Rom(open(os.path.join(ROOT, "baserom.gba"), "rb").read())
    code_end = cfg["rom"]["code_end"]
    pcfg = cfg["pointers"]
    ranges = [tuple(r) for r in pcfg["ranges"]]
    not_pointers = set(pcfg.get("not_pointers", []))
    funcs = load_functions()
    files, words, narrow = load_evidence(args.evidence)
    if not files:
        sys.exit(f"no evidence in {args.evidence} (*.loads): run `make pointer-evidence` first")

    problems: list[str] = []
    m4a, covered = parse_songs(rom, pcfg["song_table"], problems)
    ranged, rejected = scan_ranges(rom, ranges, funcs, code_end, narrow, not_pointers)
    for loc in m4a:
        if any(lo <= loc < hi for lo, hi in ranges) and m4a[loc][1] in ("goto", "patt", "rept", "xwave"):
            problems.append(f"m4a track pointer at {loc:08X} lies in a [pointers] range")
    for lo, hi in covered:
        for rlo, rhi in ranges:
            if lo < rhi and rlo < hi:
                problems.append(f"m4a song data {lo:08X}-{hi:08X} overlaps the range {rlo:08X}-{rhi:08X}")
    pointers = dict(ranged)
    for loc, (v, kind) in m4a.items():
        if loc in pointers and pointers[loc][0] != v:
            problems.append(f"{loc:08X}: m4a and range scan disagree")
        pointers[loc] = (v, "m4a" if kind in ("goto", "patt", "rept", "xwave") else pointers.get(loc, (v, "data"))[1])
    for loc, (v, kind) in pointers.items():
        if not (code_end <= loc and loc + 4 <= rom.end):
            problems.append(f"pointer at {loc:08X} is outside the data")
        if kind == "m4a" and not (code_end <= v <= rom.end):
            problems.append(f"m4a pointer at {loc:08X} to {v:08X} is outside the data")

    # The original's own loads: every ROM-valued word it loads is a pointer, no pointer is
    # read byte by byte.
    loaded = {a for a in words if code_end <= a < rom.end and rom.is_rom(rom.u32(a))}
    for a in sorted(loaded - set(pointers)):
        problems.append(f"{a:08X} = {rom.u32(a):08X} is loaded with ldr/ldm by the original but is not a pointer "
                        f"({rejected.get(a, 'outside the ranges and m4a')})")
    for loc, (v, kind) in sorted(pointers.items()):
        if kind != "m4a" and any(b in narrow for b in range(loc, loc + 4)):
            problems.append(f"{loc:08X} = {v:08X} ({kind}) is read with ldrb/ldrh by the original")

    report = [
        "# Generated by scripts/find_pointers.py -- do not edit by hand.",
        "#",
        f"# evidence: {len(files)} --rom-loads files from {os.path.relpath(args.evidence, ROOT)}",
        f"# pointers: {len(pointers)}",
    ]
    kinds = collections.Counter(k for _, k in pointers.values())
    report += [f"#   {k:5} {n}" for k, n in sorted(kinds.items())]
    report.append(f"#   loaded by the original's ldr/ldm: {len(loaded & set(pointers))}")
    report.append("# ranges:")
    for lo, hi in ranges:
        n = sum(1 for a in pointers if lo <= a < hi)
        report.append(f"#   {lo:08X}-{hi:08X}: {n} pointers")
    report.append(f"# m4a song data: {len(covered)} headers and tracks, {sum(hi - lo for lo, hi in covered)} bytes")
    report.append(f"# rejected candidates in the ranges: {len(rejected)}")
    for a, why in sorted(rejected.items()):
        report.append(f"{a:08X} {rom.u32(a):08X} {why}")
    if problems:
        report.append("# PROBLEMS (config/data_pointers.txt not written):")
        report += [f"#   {p}" for p in problems]
    with open(os.path.join(ROOT, "config", "pointer_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")
    if problems:
        print("\n".join(problems[:40]), file=sys.stderr)
        sys.exit(f"find_pointers: {len(problems)} problems (config/pointer_report.txt)")

    lines = [
        "# Generated by scripts/find_pointers.py -- do not edit by hand; change kururin.toml",
        "# [pointers] and re-run it (after `make pointer-evidence`).",
        "#",
        "# location    value       kind  evidence (loaded: the original loads it with ldr/ldm)",
    ]
    for loc, (v, kind) in sorted(pointers.items()):
        ev = "loaded" if loc in loaded else ""
        lines.append(f"0x{loc:08X}  0x{v:08X}  {kind:4}  {ev}".rstrip())
    with open(os.path.join(ROOT, "config", "data_pointers.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"find_pointers: {len(pointers)} pointers "
          + ", ".join(f"{n} {k}" for k, n in sorted(kinds.items())))


if __name__ == "__main__":
    main()
