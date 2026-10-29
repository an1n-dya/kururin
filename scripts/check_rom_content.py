#!/usr/bin/env python3
"""Check that no tracked file holds content of baserom.gba.

Every file `git ls-files` lists is read (a BizHawk .bk2 movie is a zip: its members are read
unpacked) and checked for:

  1. raw ROM bytes: any run of MIN_RUN (32) or more bytes that also occurs in baserom.gba;
  2. hex dumps: a line with 6 or more hex numbers of one width (2, 4 or 8 digits) whose bytes,
     in either byte order, occur in baserom.gba (runs of one repeated value are ignored);
  3. assembly listings: more than 5 consecutive lines that read as ARM/Thumb instructions, in
     any text file except the assembler macros (asm/macros.inc) and this script.

Each finding is printed as `path: what`, and the exit status is 1 if there is any. With
`--strings` it also lists (without failing) every printable-ASCII run of 10 or more characters
(4 or more of them different) that occurs in the ROM: game text quoted in docs and comments, for a reviewer to judge.

    python3 scripts/check_rom_content.py [--rom baserom.gba] [--strings] [paths...]

The ROM run check is exact: every 16-byte block at a 16-byte-aligned ROM offset is indexed, and
any 32-byte run of the ROM contains one such block, so each file offset is looked up and a hit
is extended both ways. Runs of a single repeated byte value (padding, zero fill) are not
reported.
"""

import argparse
import io
import re
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MIN_RUN = 32
BLOCK = 16
ROM_SHA1 = "6025ef597db2684ae064ffe22b4fd1d37941f887"
SKIP_ASM = {"asm/macros.inc", "scripts/check_rom_content.py"}

MNEMONIC = (r"(?:ldr|ldrb|ldrh|ldrsb|ldrsh|str|strb|strh|ldm|ldmia|stm|stmia|push|pop|mov|movs|"
            r"mvn|mvns|add|adds|adc|sub|subs|sbc|neg|negs|mul|muls|lsl|lsls|lsr|lsrs|asr|asrs|"
            r"ror|and|ands|orr|orrs|eor|eors|bic|bics|cmp|cmn|tst|b|bl|bx|blx|beq|bne|bcs|bcc|"
            r"bhs|blo|bmi|bpl|bvs|bvc|bhi|bls|bge|blt|bgt|ble|swi|svc|nop|teq|rsb|mrs|msr)")
# An instruction, optionally after a comment marker, a table bar, a backtick, an address and a
# label, and followed by an operand that looks like one (register, immediate, label, list).
ASM_LINE = re.compile(
    r"^\s*(?://+|/?\*+|#+|;+|\|)?\s*`?\s*(?:(?:0x)?[0-9A-Fa-f]{4,8}:?\s+)?"
    r"(?:[0-9A-Fa-f]{4}(?:\s[0-9A-Fa-f]{4})?\s+)?(?:[A-Za-z_.$][\w.$]*:\s*)?"
    + MNEMONIC + r"(?:\.[nw])?\s+(?:r\d{1,2}\b|sp\b|lr\b|pc\b|ip\b|\{|#|\[|_|0x|[A-Za-z_]\w*\s*$)",
    re.IGNORECASE)
LABEL_LINE = re.compile(r"^\s*(?://+|/?\*+|#+|;+|\|)?\s*`?[A-Za-z_.$][\w.$]*:\s*`?\s*$")
HEX_TOKEN = re.compile(r"(?<![\w.])(?:0x)?([0-9A-Fa-f]{2}|[0-9A-Fa-f]{4}|[0-9A-Fa-f]{8})(?![\w.])")
PRINTABLE = re.compile(rb"[\x20-\x7e]{10,}")


def git_files():
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, check=True,
                         capture_output=True).stdout
    return [p for p in out.decode().split("\0") if p]


def contents(path):
    """(name, bytes) for the file, or for each member of a .bk2 movie."""
    data = (ROOT / path).read_bytes()
    if path.endswith(".bk2"):
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for info in z.infolist():
                yield f"{path}:{info.filename}", z.read(info)
        return
    yield path, data


class Rom:
    def __init__(self, data):
        self.data = data
        self.blocks = {}
        for off in range(0, len(data) - BLOCK + 1, BLOCK):
            self.blocks.setdefault(data[off:off + BLOCK], []).append(off)

    def runs(self, buf):
        """(file offset, length, rom offset) of each ROM run of MIN_RUN+ bytes in buf."""
        rom, blocks, found = self.data, self.blocks, []
        i, end = 0, len(buf) - BLOCK + 1
        while i < end:
            offs = blocks.get(buf[i:i + BLOCK])
            if offs is None or len(set(buf[i:i + BLOCK])) == 1:
                i += 1
                continue
            best = None
            for r in offs:
                lo = 0
                while i - lo > 0 and r - lo > 0 and buf[i - lo - 1] == rom[r - lo - 1]:
                    lo += 1
                hi = BLOCK
                while i + hi < len(buf) and r + hi < len(rom) and buf[i + hi] == rom[r + hi]:
                    hi += 1
                if best is None or lo + hi > best[1]:
                    best = (i - lo, lo + hi, r - lo)
            if best[1] >= MIN_RUN and len(set(buf[best[0]:best[0] + best[1]])) > 1:
                if not found or best[0] >= found[-1][0] + found[-1][1]:
                    found.append(best)
                i = best[0] + best[1]
            else:
                i += 1
        return found


def hex_dumps(text, rom):
    for lineno, line in enumerate(text.splitlines(), 1):
        tokens = HEX_TOKEN.findall(line)
        for width in (2, 4, 8):
            run = [t for t in tokens if len(t) == width]
            if len(run) < 6 or len(set(run)) < 2:
                continue
            for order in ("little", "big"):
                blob = b"".join(int(t, 16).to_bytes(width // 2, order) for t in run)
                if blob in rom:
                    yield lineno, f"{len(run)} hex numbers ({len(blob)} bytes, {order}-endian) found in the ROM"
                    break


def asm_listings(text):
    run_start, count = None, 0
    lines = text.splitlines()
    for lineno, line in enumerate(lines + [""], 1):
        if ASM_LINE.match(line):
            if run_start is None:
                run_start = lineno
            count += 1
        elif run_start is not None and LABEL_LINE.match(line):
            continue
        else:
            if count > 5:
                yield run_start, f"{count} lines of assembly (lines {run_start}-{lineno - 1})"
            run_start, count = None, 0


def rom_strings(data, rom_data, windows):
    """(line, text) of each maximal printable run of 10+ bytes of data that the ROM holds."""
    i = 0
    while i <= len(data) - 10:
        if data[i:i + 10] not in windows:
            i += 1
            continue
        n = 10
        while i + n < len(data) and data[i:i + n + 1] in rom_data:
            n += 1
        yield data.count(b"\n", 0, i) + 1, data[i:i + n].decode("ascii")
        i += n


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--rom", default=str(ROOT / "baserom.gba"))
    ap.add_argument("--strings", action="store_true",
                    help="also list ASCII runs of 10+ characters that occur in the ROM")
    ap.add_argument("paths", nargs="*", help="files to check (default: every tracked file)")
    args = ap.parse_args()

    import hashlib
    rom_data = Path(args.rom).read_bytes()
    if hashlib.sha1(rom_data).hexdigest() != ROM_SHA1:
        sys.exit(f"{args.rom}: not the expected ROM (SHA1 {ROM_SHA1})")
    rom = Rom(rom_data)

    windows = None
    if args.strings:
        # windows of 4+ different characters: runs such as "||||....||" are not text
        windows = {w for m in PRINTABLE.finditer(rom_data)
                   for k in range(len(m.group()) - 9)
                   if len(set(w := m.group()[k:k + 10])) >= 4}
    paths = args.paths or git_files()
    findings, strings, scanned = [], [], 0
    for path in paths:
        for name, data in contents(path):
            scanned += 1
            for at, length, r in rom.runs(data):
                findings.append(f"{name}: {length} bytes at 0x{at:X} are the ROM's at 0x{0x08000000 + r:08X}")
            try:
                text = data.decode("utf-8")
            except UnicodeDecodeError:
                continue
            for lineno, what in hex_dumps(text, rom_data):
                findings.append(f"{name}:{lineno}: {what}")
            if path not in SKIP_ASM:
                for lineno, what in asm_listings(text):
                    findings.append(f"{name}:{lineno}: {what}")
            if args.strings:
                strings.extend(f"{name}:{line}: {hit!r}" for line, hit in rom_strings(data, rom_data, windows))
    for f in findings:
        print(f)
    if args.strings:
        print(f"-- ASCII runs of 10+ characters that occur in the ROM (review, not a failure): {len(strings)}")
        for s in strings:
            print(s)
    print(f"check_rom_content: {scanned} files checked, {len(findings)} findings")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
