#!/usr/bin/env python3
"""Map original ROM addresses to the rebuilt ROM's, for kurutest --rom-map.

The game keeps ROM addresses in RAM (gMain's frame callback, level and graphics pointers).
When code or data moves (`make shift-test`, or a decompiled function of another size), the
rebuilt ROM legitimately holds a different value there, so make test compares a watched word
that is a ROM address through this map.

Every global symbol of the rebuilt ELF whose original address is known -- a function
(config/functions.txt and kururin.toml names), a data label (gUnk_<address>, or a [symbols]
name in ROM) -- anchors the map: an original address maps to its rebuilt address plus its
offset from the nearest anchor at or below it. Output: one line per run of anchors with the
same delta, "<original start> <original end> <delta>" in hex (end exclusive).

Usage:
  python3 scripts/rom_map.py kururin.elf > build/test/rom_map.txt
"""

import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_asm import ROM_BASE, function_names, load_config  # noqa: E402


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    cfg, funcs, _, _ = load_config()
    functions = {name: addr for addr, name in function_names(cfg, funcs).items()}
    orig = dict(functions)
    orig.update({n: a for n, a in cfg.get("symbols", {}).items() if a >= ROM_BASE})
    nm = subprocess.run(["arm-none-eabi-nm", sys.argv[1]], capture_output=True, text=True, check=True).stdout
    anchors = {}
    for line in nm.splitlines():
        parts = line.split()
        if len(parts) != 3:
            continue
        value, name = int(parts[0], 16), parts[2]
        if name in functions:
            value &= ~1  # a Thumb function's symbol has bit 0 set
        m = re.fullmatch(r"gUnk_([0-9A-F]{8})", name)
        a = int(m.group(1), 16) if m else orig.get(name)
        if a is not None and a >= ROM_BASE and value >= ROM_BASE:
            anchors[a] = value - a
    rom_end = ROM_BASE + cfg["rom"]["size"]
    starts = sorted(anchors)
    runs = []
    for i, a in enumerate(starts):
        if runs and runs[-1][2] == anchors[a]:
            continue
        runs.append([a, None, anchors[a]])
    for i, r in enumerate(runs):
        r[1] = runs[i + 1][0] if i + 1 < len(runs) else rom_end + 1
    if runs and runs[0][0] > ROM_BASE:
        runs.insert(0, [ROM_BASE, runs[0][0], 0])
    for lo, hi, delta in runs:
        print(f"{lo:08X} {hi:08X} {delta:X}" if delta >= 0 else f"{lo:08X} {hi:08X} -{-delta:X}")


if __name__ == "__main__":
    main()
