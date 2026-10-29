#!/usr/bin/env python3
"""Build Kuru Kuru Kururin SRAM images (save files) from a short description.

The format is in docs/save_format.md (with the evidence for every field). In short: 32 KiB of
SRAM, a 16-byte signature, the bank in use (0/1), a 16-bit byte-sum checksum per bank, and two
banks of 0x2254 bytes, each: five 0x1F4-byte save files, then three records per level for the
93 Normal levels and the 38 Easy ones.

A description is a TOML file (tests/gameplay/saves/<name>.toml -> <name>.sav):

    [records]                  # optional; the leaderboards
    master = true              # enter the Master times as sub_0800B000 does once a file
                               # reaches rank 2 (default: true when a Normal file has rank >= 2)

    [sram]                     # optional; for testing the loader (sub_08005178)
    bank = 0                   # the bank in use (0/1; above 1 is invalid)
    bad_checksum = [0, 1]      # banks whose stored checksum is made wrong

    [[file]]                   # up to five; slots without a [[file]] are reset (empty)
    slot = 1                   # 0-4; 0 is GUEST (the fifth item of the file menu, always named
                               # "GUEST"), 1-4 the first four items
    name = "ANNA"              # 1-8 characters (ASCII)
    easy = false               # the file plays Adventure in Easy mode (unk10 bits 3 and 4)
    rank = 3                   # 0-3 (unkD): Trainee times beaten = 1, Professor = 2, Master = 3;
                               # default: what the best times give (sub_0800B000)
    adventure = 38             # levels 0..N-1 of Adventure cleared (0-38; 38 = all 12 worlds),
    challenge = 55             # courses 0..N-1 of Challenge cleared (0-55)
    mark = 4                   # mark of a cleared level: 3 cleared, 4 cleared without a bump
    marks = { 7 = 3 }          # per-level marks overriding the above (level index: 0-4
                               # Training, world w level l = w * 3 + l + 2, Challenge 38 + n)
    bonuses = 30               # bonuses taken: the first N of the 30 levels of worlds 1-10
                               # (unk1C), with the unlocks they give (unk14); or a list of levels
    times = 2                  # best times of cleared levels: just under gLevelTimes[level][n]
                               # (0 Trainee, 1 Professor, 2 Master; Easy files use the Easy
                               # times for Adventure), or 3: 0.9 x the Master time
    level_times = { 7 = 0 }    # per-level overrides of times
    helirin = 0                # unk18 (0-14), unk19 colour (0-6), unk1A items worn (bits 0-9)
    colour = 0
    items_worn = 0
    ending = false             # the ending has been seen (unk10 bit 5)
    flags = 0x...              # unk10 outright, instead of what the fields above give

Usage:
    python3 tools/make_save.py                 # every tests/gameplay/saves/*.toml
    python3 tools/make_save.py <desc.toml>...  # these
    python3 tools/make_save.py --check         # fail if a .sav differs from its description
    python3 tools/make_save.py --show <file.sav>   # decode an SRAM image (or a dump)

The ROM tables a save holds (signature, default names, level times) are read from the user's
baserom.gba (its SHA1 checked), so nothing ROM-derived is in this script or the repository: the
.sav images are not committed. tests/gameplay/run.py builds the image of an input's `save` line
from its .toml whenever it converts the input (make test, make shift-test, the port tests), so
an image always is what this script, its description and the ROM give; the script and the
descriptions are locked (tests/locked_files.txt).
"""

import argparse
import hashlib
import os
import pathlib
import signal
import struct
import sys
import threading
import tomllib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SAVES = ROOT / "tests" / "gameplay" / "saves"
BASEROM_SHA1 = "6025ef597db2684ae064ffe22b4fd1d37941f887"  # Kurukuru Kururin (Europe)

# The ROM tables a save holds, by address (their names in kururin.toml [symbols]). They are here
# rather than read from kururin.toml, which is not locked: an image depends on nothing but this
# script, its description and the ROM.
SIGNATURE = 0x080C2C50         # gSaveSignature: 16 bytes
GUEST_NAME = 0x0801E484        # gTextGuest: "GUEST", file 0's name (6 bytes)
RECORD_NAMES = 0x0801E48A      # gDefaultRecordNames: char[3][10]
NO_RECORD_NAME = 0x0801E4A8    # gTextNoRecordName: 9 bytes
LEVEL_TIMES = 0x080C293C       # gLevelTimes: u16[93][3], the Normal levels
EASY_LEVEL_TIMES = 0x080C2B6A  # gEasyLevelTimes: u16[38][3]

SRAM_SIZE = 0x8000
BANK_SIZE = 0x2254
FILE_SIZE = 0x1F4
FILES = 5
NORMAL_LEVELS = 93      # 38 Adventure (5 Training + 11 worlds x 3) + 55 Challenge
EASY_LEVELS = 38
RECORDS = 0x9C4         # 5 * FILE_SIZE
EASY_RECORDS = 0x1B34   # RECORDS + 93 * 3 * 0x10
NO_TIME = 216000        # 60 minutes: an empty record / an uncleared level (sub_08005644)
ADVENTURE = 38
CHALLENGE = 55
BONUS_LEVELS = 30       # levels 5-34 (worlds 1-10) hold one bonus each


def bonus_bit(i):
    """unk14 bit of the bonus in the i-th bonus level (0-29). Each level's map names its own
    bit (sub_0800580C); which level has which is not known here, so the unlocks are assigned
    in kind order: 10 items (bits 0-9), 6 colours (11-16), 14 Helirins (18-31)."""
    if i < 10:
        return i
    if i < 16:
        return 11 + i - 10
    return 18 + i - 16


class Rom:
    def __init__(self, path=ROOT / "baserom.gba"):
        try:
            self.data = pathlib.Path(path).read_bytes()
        except FileNotFoundError:
            sys.exit(f"{path} is missing: the save images are built from your own dump of the game")
        if hashlib.sha1(self.data).hexdigest() != BASEROM_SHA1:
            sys.exit(f"{path} is not the Kurukuru Kururin (Europe) dump (SHA1 {BASEROM_SHA1}): "
                     "the save images are built from its tables")

    def at(self, addr, size, offset=0):
        a = addr - 0x08000000 + offset
        return self.data[a:a + size]

    def signature(self):
        return self.at(SIGNATURE, 16)

    def times(self, level, easy=False):
        """gLevelTimes[level] (Normal) or gEasyLevelTimes[level] (Easy): 3 u16 thresholds."""
        return struct.unpack("<3H", self.at(EASY_LEVEL_TIMES if easy else LEVEL_TIMES, 6, level * 6))


def cstr(text, size):
    b = text.encode("ascii") if isinstance(text, str) else bytes(text)
    return b[:size].ljust(size, b"\0")


class Bank:
    """The 0x2254 bytes of one bank (gUnk_030043A0 / gUnk_0300439C in RAM)."""

    def __init__(self, rom):
        self.rom = rom
        self.b = bytearray(BANK_SIZE)
        for n in range(FILES):
            self.reset_file(n)
        self.reset_records()

    # sub_080055C0
    def reset_file(self, n):
        o = n * FILE_SIZE
        f = bytearray(FILE_SIZE)
        struct.pack_into("<H", f, 0, n)
        struct.pack_into("<I", f, 0x14, 0x20400)
        for i in range(NORMAL_LEVELS):
            struct.pack_into("<I", f, 0x20 + i * 4, NO_TIME)
        if n == 0:
            f[2:8] = self.rom.at(GUEST_NAME, 6)
        self.b[o:o + FILE_SIZE] = f

    def rec(self, table, level, j):
        return table + (level * 3 + j) * 0x10

    def get_rec(self, off):
        t, = struct.unpack_from("<I", self.b, off)
        return t, bytes(self.b[off + 4:off + 15]), self.b[off + 15]

    def set_rec(self, off, time, name, rank):
        struct.pack_into("<I", self.b, off, time)
        self.b[off + 4:off + 15] = name
        self.b[off + 15] = rank

    def strcpy(self, off, src):
        s = src.split(b"\0")[0] + b"\0"
        self.b[off:off + len(s)] = s

    # sub_08005644
    def reset_records(self):
        dashes = self.rom.at(NO_RECORD_NAME, 9)
        names = [self.rom.at(RECORD_NAMES, 10, 10 * k) for k in range(3)]
        for table, count in ((RECORDS, NORMAL_LEVELS * 3), (EASY_RECORDS, EASY_LEVELS * 3)):
            for k in range(count):
                o = table + k * 0x10
                struct.pack_into("<I", self.b, o, NO_TIME)
                self.b[o + 4:o + 13] = dashes
        for table, levels, easy in ((RECORDS, list(range(35)) + list(range(38, 88)), False),
                                    (EASY_RECORDS, range(35), True)):
            for i in levels:
                t = self.rom.times(i, easy)
                for j, (k, rank) in enumerate(((1, 2), (0, 1))):
                    o = self.rec(table, i, j)
                    struct.pack_into("<I", self.b, o, t[k])
                    self.strcpy(o + 4, names[k])
                    self.b[o + 15] = rank

    def insert(self, table, level, time, name, rank, unless_name=False):
        """Puts (time, name) into the level's three records, sorted by time (sub_0800B604's
        new-record path; with unless_name, sub_0800B000's: not when it is already there)."""
        for j in range(3):
            t, n, _ = self.get_rec(self.rec(table, level, j))
            if unless_name and t == time and n.split(b"\0")[0] == name.split(b"\0")[0]:
                return
            if t > time:
                for k in range(2, j, -1):
                    t2, n2, r2 = self.get_rec(self.rec(table, level, k - 1))
                    o = self.rec(table, level, k)
                    struct.pack_into("<I", self.b, o, t2)
                    self.strcpy(o + 4, n2)
                    self.b[o + 15] = r2
                o = self.rec(table, level, j)
                struct.pack_into("<I", self.b, o, time)
                self.strcpy(o + 4, name)
                self.b[o + 15] = rank
                return

    # sub_0800B000, when a file's rank reaches 2
    def enter_master(self, easy):
        name = self.rom.at(RECORD_NAMES, 10, 20)
        if easy:
            for i in range(0x23):
                self.insert(EASY_RECORDS, i, self.rom.times(i, True)[2], name, 3, unless_name=True)
        else:
            for i in range(0x23):
                self.insert(RECORDS, i, self.rom.times(i)[2], name, 3, unless_name=True)
        for i in range(0x26, 0x58):
            self.insert(RECORDS, i, self.rom.times(i)[2], name, 3, unless_name=True)


def best_time(rom, level, easy, times):
    """A best time for a cleared level, just under the threshold *times* asks for."""
    t = rom.times(level, easy and level < ADVENTURE)
    if times >= 3 or t[min(times, 2)] == 0:
        base = t[2] or t[1] or t[0] or 3600
        return max(1, base * 9 // 10)
    return max(1, t[times] - 1 - level % 7)


def rank_of(rom, unk20, easy):
    """sub_0800B000: the rank the file's best times give."""
    rank = 0
    while rank <= 2:
        adv = all(unk20[i] < rom.times(i, easy)[rank] for i in range(5, 0x23))
        cha = all(unk20[i] < rom.times(i)[rank] for i in range(0x26, 0x58))
        if not (adv and cha):
            break
        rank += 1
    return rank


def build(desc, rom):
    bank = Bank(rom)
    files = desc.get("file", [])
    slots = [f["slot"] for f in files]
    if len(set(slots)) != len(slots) or any(not 0 <= s < FILES for s in slots):
        sys.exit("file slots must be distinct, 0-4")
    entries = []
    for f in files:
        n = f["slot"]
        o = n * FILE_SIZE
        easy = f.get("easy", False)
        name = rom.at(GUEST_NAME, 6) if n == 0 else cstr(f["name"], 9)
        if n != 0 and not 1 <= len(f["name"]) <= 8:
            sys.exit(f"slot {n}: a name has 1-8 characters")
        adventure, challenge = f.get("adventure", 0), f.get("challenge", 0)
        mark = f.get("mark", 4)
        marks = [0] * 0x60
        for i in range(adventure):
            marks[i] = mark
        for i in range(challenge):
            marks[ADVENTURE + i] = mark
        for k, v in f.get("marks", {}).items():
            marks[int(k)] = v
        times = f.get("times", 0)
        unk20 = [NO_TIME] * NORMAL_LEVELS
        for i in range(NORMAL_LEVELS):
            if marks[i] >= 2:
                unk20[i] = best_time(rom, i, easy, f.get("level_times", {}).get(str(i), times))
        bonuses = f.get("bonuses", 0)
        levels = list(range(bonuses)) if isinstance(bonuses, int) else list(bonuses)
        unk1c = 0
        unk14 = 0x20400
        for i in levels:
            unk1c |= 1 << i
            unk14 |= 1 << bonus_bit(i)
        flags = 0
        if adventure or easy or f.get("mode_chosen", False):
            flags |= 1 | 8          # the story was shown, Normal/Easy was asked
        if easy:
            flags |= 0x10
        if all(marks[i] > 3 for i in range(5, 0x23)):
            flags |= 2              # the last world (sub_0800B604)
        if all(marks[i] > 3 for i in range(0x26, 0x58)):
            flags |= 4              # the 11th Challenge row (sub_0800B604)
        if f.get("ending", False):
            flags |= 0x20
        flags = f.get("flags", flags)
        rank = f.get("rank", rank_of(rom, unk20, easy))

        b = bank.b
        b[o + 2:o + 13] = cstr(name, 11)
        b[o + 0xD] = rank
        struct.pack_into("<IIBBHI", b, o + 0x10, flags, unk14, f.get("helirin", 0), f.get("colour", 0),
                         f.get("items_worn", 0), unk1c)
        struct.pack_into(f"<{NORMAL_LEVELS}I", b, o + 0x20, *unk20)
        b[o + 0x194:o + 0x1F4] = bytes(marks)
        entries.append((name, rank, easy, unk20))

    rec = desc.get("records", {})
    master = rec.get("master", any(r >= 2 and not e for _, r, e, _ in entries))
    if master:
        bank.enter_master(False)
    if rec.get("master_easy", any(r >= 2 and e for _, r, e, _ in entries)):
        bank.enter_master(True)
    for name, rank, easy, unk20 in entries:
        for i, t in enumerate(unk20):
            if t == NO_TIME:
                continue
            if easy and i < EASY_LEVELS:
                bank.insert(EASY_RECORDS, i, t, name, rank)
            else:
                bank.insert(RECORDS, i, t, name, rank)

    data = bytes(bank.b)
    s = sum(data) & 0xFFFF
    sram = bytearray(b"\xFF" * SRAM_SIZE)
    sram[0:16] = rom.signature()
    opts = desc.get("sram", {})
    sums = [(s ^ 0x5A5A) if k in opts.get("bad_checksum", []) else s for k in range(2)]
    struct.pack_into("<HHH", sram, 0x10, opts.get("bank", 0), *sums)   # both banks the same
    sram[0x18:0x18 + BANK_SIZE] = data
    sram[0x18 + BANK_SIZE:0x18 + 2 * BANK_SIZE] = data
    return bytes(sram)


def write_image(desc, rom):
    """Builds <desc>.sav from the description <desc>.toml and returns its path. The file is
    replaced in one step (inputs run in parallel read an image while another run writes it)."""
    desc = pathlib.Path(desc)
    out = desc.with_suffix(".sav")
    sram = build(tomllib.loads(desc.read_text()), rom)
    if not (out.exists() and out.read_bytes() == sram):
        tmp = out.with_name(f".{out.name}.{os.getpid()}.{threading.get_ident()}.tmp")
        tmp.write_bytes(sram)
        os.replace(tmp, out)
    return out


def show(path, rom):
    d = pathlib.Path(path).read_bytes()
    bank, = struct.unpack_from("<H", d, 0x10)
    sig_ok = d[:16].split(b"\0")[0] == rom.signature().split(b"\0")[0]
    print(f"{path}: {len(d)} bytes, signature {'ok' if sig_ok else 'BAD'} {d[:16]!r}, bank {bank}")
    for k in range(2):
        data = d[0x18 + k * BANK_SIZE:0x18 + (k + 1) * BANK_SIZE]
        stored, = struct.unpack_from("<H", d, 0x12 + 2 * k)
        print(f"  bank {k}: checksum {stored:04X}, sum {sum(data) & 0xFFFF:04X}"
              f" {'ok' if stored == sum(data) & 0xFFFF else 'BAD'}")
    if bank > 1:
        return
    data = d[0x18 + bank * BANK_SIZE:0x18 + (bank + 1) * BANK_SIZE]
    for n in (1, 2, 3, 4, 0):
        f = data[n * FILE_SIZE:(n + 1) * FILE_SIZE]
        num, = struct.unpack_from("<H", f, 0)
        flags, unk14, h, c, worn, unk1c = struct.unpack_from("<IIBBHI", f, 0x10)
        marks = f[0x194:0x194 + NORMAL_LEVELS]
        name = f[2:13].split(b"\0")[0].decode("latin-1")
        print(f"  file {n} (#{num}) {name!r:12} rank {f[0xD]} flags {flags:02X} unlocks {unk14:08X}"
              f" bonuses {unk1c:08X} helirin {h} colour {c} worn {worn:03X}")
        print(f"    marks {marks[:ADVENTURE].hex()} | {marks[ADVENTURE:].hex()}")


def main():
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)  # `--show ... | head` is fine
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if a .sav differs from its description")
    ap.add_argument("--show", action="store_true", help="decode the given SRAM images")
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    rom = Rom()
    if args.show:
        for p in args.files:
            show(p, rom)
        return 0
    descs = [pathlib.Path(p) for p in args.files] or sorted(SAVES.glob("*.toml"))
    bad = 0
    for p in descs:
        if args.check:
            out = p.with_suffix(".sav")
            if not out.exists() or out.read_bytes() != build(tomllib.loads(p.read_text()), rom):
                print(f"{out}: differs from {p.name} (run tools/make_save.py)")
                bad += 1
            continue
        out = write_image(p, rom)
        print(f"wrote {out.relative_to(ROOT) if out.is_relative_to(ROOT) else out}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
