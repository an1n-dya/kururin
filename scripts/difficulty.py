#!/usr/bin/env python3
"""Estimate how hard each function is to match, from its control flow.

Features come from the same explorer the generator uses (scripts/romanalysis.py):
code size (without literal pools), conditional branches, loops (backward
branches), calls, jump tables, long-branch BLs, saved high registers (r8-r11:
heavy register pressure, where agbcc's allocation is hardest to reproduce) and
stack frames. Each function gets a tier:

  trivial  leaf, straight-line, <= 16 bytes of code
  easy     <= 64 bytes, at most 2 branches, no loop over 1 call, no jump table
  medium   <= 256 bytes, no jump table, no saved high registers
  hard     anything larger, or with a jump table / saved r8-r11 / long branches
  asm      hand-written asm (asm modules / asm regions): not a C target
  linked   libgcc/libc, linked from agbcc's own archives: nothing to do

Only `make compare` decides a match; this is a triage aid for picking the next
function. Already-decompiled functions (no INCLUDE_ASM left) are counted under done,
not under their tier, and --list leaves them out.

Usage:
  python3 scripts/difficulty.py              # summary per module and tier
  python3 scripts/difficulty.py --list easy  # functions of one tier, easiest first
"""

import argparse
import collections
import os
import signal
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_asm import c_definitions, function_names, load_config, modules  # noqa: E402
from romanalysis import Rom, explore_thumb  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TIERS = ("trivial", "easy", "medium", "hard", "asm", "linked")
COLUMNS = TIERS + ("done",)


def features(rom, e, end, stops, noreturn, farjumps):
    fi = explore_thumb(rom, e, end, stops, noreturn, farjumps)
    code = sum(i.size for i in fi.insns.values())
    branches = sum(1 for i in fi.insns.values() if i.kind == "bcond")
    loops = sum(1 for a, i in fi.insns.items() if i.kind in ("b", "bcond") and i.target <= a)
    calls = sum(1 for i in fi.insns.values() if i.kind == "bl" and i.target not in farjumps)
    far = sum(1 for i in fi.insns.values() if i.kind == "bl" and i.target in farjumps)
    # `mov rLow, rHigh` with rHigh in r8-r11: agbcc spilling into the high registers
    hiregs = any((i.raw & 0xFFC0) == 0x4640 and 8 <= (((i.raw >> 3) & 7) | 8) <= 11
                 for i in fi.insns.values() if i.size == 2)
    frame = any(i.kind == "other" and (i.raw & 0xFF80) == 0xB080 for i in fi.insns.values())
    return dict(code=code, branches=branches, loops=loops, calls=calls, jumptables=len(fi.jumptables),
                far=far, hiregs=hiregs, frame=frame)


def tier(f):
    if f["jumptables"] or f["hiregs"] or f["far"] or f["code"] > 256:
        return "hard"
    if f["code"] <= 16 and not f["branches"] and not f["calls"] and not f["loops"]:
        return "trivial"
    if f["code"] <= 64 and f["branches"] <= 2 and not (f["loops"] and f["calls"] > 1):
        return "easy"
    return "medium"


def decompiled(mods, names, funcs):
    """Function names that each C module's src file defines in C (as the generator sees it)."""
    done = set()
    for name, kind, lo, hi in mods:
        if kind == "c":
            done |= c_definitions(os.path.join(ROOT, "src", f"{name}.c"),
                                  [names[a] for a in funcs if lo <= a < hi])
    return done


def main():
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)  # `| head` is fine
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", choices=TIERS, help="list the functions of one tier")
    args = ap.parse_args()

    cfg, funcs, farjumps, noreturn = load_config()
    with open(os.path.join(ROOT, "kururin.toml"), "rb") as f:
        asm_regions = [(r["start"], r["end"]) for r in tomllib.load(f).get("asm_regions", [])]
    names = function_names(cfg, funcs)
    rom = Rom(open(os.path.join(ROOT, "baserom.gba"), "rb").read())
    mods = modules(cfg)
    done = decompiled(mods, names, funcs)
    stops = frozenset(funcs)
    rows = []
    for name, kind, lo, hi in mods:
        for a in sorted(x for x in funcs if lo <= x < hi):
            if kind == "lib":
                rows.append((name, a, "linked", None))
                continue
            if kind != "c" or any(r0 <= a < r1 for r0, r1 in asm_regions):
                rows.append((name, a, "asm", None))
                continue
            f = features(rom, a, cfg["rom"]["code_end"], stops - {a}, noreturn, farjumps)
            rows.append((name, a, tier(f), f))

    if args.list:
        sel = [r for r in rows if r[2] == args.list and names[r[1]] not in done]
        sel.sort(key=lambda r: (r[3]["code"] if r[3] else 0, r[1]))
        for mod, a, t, f in sel:
            extra = "" if f is None else (f"{f['code']:4d}B br={f['branches']} loops={f['loops']} "
                                          f"calls={f['calls']}")
            print(f"{mod:<10} {a:08X} {names[a]:<24} {extra}")
        return

    col = lambda r: "done" if names[r[1]] in done else r[2]  # noqa: E731
    by = collections.Counter((r[0], col(r)) for r in rows)
    print(f"{'module':<14}" + "".join(f"{t:>9}" for t in COLUMNS) + f"{'total':>8}")
    for name, kind, _, _ in mods:
        if kind in ("header", "data"):
            continue
        counts = [by[(name, t)] for t in COLUMNS]
        print(f"{name:<14}" + "".join(f"{c:9d}" for c in counts) + f"{sum(counts):8d}")
    totals = collections.Counter(col(r) for r in rows)
    print(f"{'all':<14}" + "".join(f"{totals[t]:9d}" for t in COLUMNS) + f"{len(rows):8d}")
    print(f"\ndecompiled: {totals['done']}/{len(rows)}")


if __name__ == "__main__":
    main()
