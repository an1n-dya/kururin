#!/usr/bin/env python3
"""Coverage of the original ROM's code by the tests/gameplay inputs: docs/coverage.md.

`make test` runs baserom.gba traced (kurutest --coverage, one file per input:
build/test/<input>.cov), recording every halfword it executed, in ROM and in RAM. This script
merges the inputs and measures each function of config/functions.txt: the share of its
instruction bytes (as scripts/romanalysis.py explores them: not literal pools, jump tables or
padding) that ran. Code run from RAM (m4a's SoundMainRAM in IWRAM, the SRAM library's
routines copied to the stack) counts for the ROM function it was copied from, found by its
bytes as they were when they first ran.

tests/gameplay/run.py calls it after a full, passing run (every input, not --quick); it can
also be run by hand on the .cov files of an earlier run.

Usage:
  python3 scripts/coverage.py [--cov-dir build/test] [--out docs/coverage.md]
"""

from __future__ import annotations

import argparse
import collections
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_asm import ROOT, function_names, load_config, modules  # noqa: E402
from romanalysis import ROM_BASE, Rom, explore_arm, explore_thumb  # noqa: E402


def instruction_bytes(rom: Rom, cfg, funcs, farjumps, noreturn) -> dict[int, int]:
    """Every instruction's address -> size, from the same exploration generate_asm.py does."""
    code_end = cfg["rom"]["code_end"]
    stops = frozenset(funcs)
    insns: dict[int, int] = {}
    thumb_work = [a for a, m in funcs.items() if m == "thumb"]
    arm_work = [a for a, m in funcs.items() if m == "arm"]
    explored = set()
    while thumb_work or arm_work:
        while thumb_work:
            e = thumb_work.pop()
            if e in explored:
                continue
            explored.add(e)
            fi = explore_thumb(rom, e, code_end, stops - {e}, noreturn, farjumps)
            insns.update({a: ins.size for a, ins in fi.insns.items()})
            arm_work.extend(fi.arm_targets)
        while arm_work:
            e = arm_work.pop()
            if e in explored:
                continue
            explored.add(e)
            ai = explore_arm(rom, e, code_end)
            insns.update({a: 4 for a in ai.insns})
            thumb_work.extend(ai.thumb_targets)
    return insns


def load_coverage(paths, rom: Rom, code_end: int):
    """Executed ROM halfwords (merged over the inputs), and RAM runs that match no ROM code."""
    executed = set()
    code = rom.data[: code_end - ROM_BASE]
    episodes = []  # (RAM start, RAM end, code bytes, [(lo, hi) that ran])
    for path in paths:
        for line in open(path):
            parts = line.split()
            if parts[0] == "#":
                continue
            if parts[0] == "ram":  # ram <start> <end> <code start..end> <ran lo-hi,...>
                runs = [tuple(int(x, 16) for x in r.split("-")) for r in parts[4].split(",")]
                episodes.append((int(parts[1], 16), int(parts[2], 16), bytes.fromhex(parts[3]), runs))
            else:
                lo, hi = int(parts[0], 16), int(parts[1], 16)
                executed.update(range(lo, hi, 2))
    # Each episode's code, found in the ROM: where it occurs once, or else (a short piece, e.g.
    # the rest of a routine after an interrupt returns into it) where an episode found that
    # way puts the same RAM address.
    placed = []  # (RAM start, RAM end, ROM address of RAM start)
    later = []
    for ep in sorted(episodes, key=lambda e: e[0] - e[1]):  # longest first
        lo, hi, data, _ = ep
        at = code.find(data)
        if at >= 0 and code.find(data, at + 1) < 0:
            placed.append((lo, hi, ROM_BASE + at))
            mark(executed, ep, ROM_BASE + at)
        else:
            later.append(ep)
    unmatched = []
    for ep in later:
        lo, hi, data, _ = ep
        rom_lo = next((r + lo - plo for plo, phi, r in placed if plo <= lo and hi <= phi
                       and code[r + lo - plo - ROM_BASE: r + hi - plo - ROM_BASE] == data), None)
        if rom_lo is None:
            unmatched.append((lo, hi))
        else:
            mark(executed, ep, rom_lo)
    return executed, sorted(set(unmatched))


def mark(executed, episode, rom_lo):
    lo, _, _, runs = episode
    for rlo, rhi in runs:
        executed.update(range(rom_lo + rlo - lo, rom_lo + rhi - lo, 2))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cov-dir", default=os.path.join(ROOT, "build", "test"))
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "coverage.md"))
    args = ap.parse_args()

    paths = sorted(glob.glob(os.path.join(args.cov_dir, "*.cov")))
    if not paths:
        sys.exit(f"no coverage files in {args.cov_dir} (run make test)")
    cfg, funcs, farjumps, noreturn = load_config()
    names = function_names(cfg, funcs)
    rom = Rom(open(os.path.join(ROOT, "baserom.gba"), "rb").read())
    code_end = cfg["rom"]["code_end"]
    insns = instruction_bytes(rom, cfg, funcs, farjumps, noreturn)
    executed, unmatched = load_coverage(paths, rom, code_end)

    mods = [(n, k, lo, hi) for n, k, lo, hi in modules(cfg) if k not in ("header", "data")]
    entries = sorted(funcs)
    rows = []  # (module, addr, name, size, code bytes, covered bytes)
    for i, a in enumerate(entries):
        b = entries[i + 1] if i + 1 < len(entries) else code_end
        mod = next(n for n, _, lo, hi in mods if lo <= a < hi)
        b = min(b, next(hi for n, _, lo, hi in mods if n == mod))
        code = {x: s for x, s in insns.items() if a <= x < b}
        total = sum(code.values())
        covered = sum(s for x, s in code.items() if x in executed)
        rows.append((mod, a, names[a], b - a, total, covered))

    def pct(c, t):
        return f"{100 * c / t:.1f}%" if t else "-"

    inputs = ", ".join(f"`{os.path.basename(p)[:-4]}`" for p in paths)
    tot_code = sum(r[4] for r in rows)
    tot_cov = sum(r[5] for r in rows)
    ran = [r for r in rows if r[5]]
    out = [
        "# Code coverage of the gameplay inputs",
        "",
        "Generated by `scripts/coverage.py` from a full `make test` run -- do not edit by hand.",
        "",
        f"What the original ROM executes on the inputs of `tests/gameplay/inputs/` ({inputs}). "
        "A function's coverage is the share of its instruction bytes that ran (literal pools, jump "
        "tables and padding excluded); code run from RAM counts for the ROM function it was copied from. "
        "A function that never runs is not tested by `make test` at all; a partly covered one only on "
        "the paths shown here.",
        "",
        "## Summary",
        "",
        f"- functions executed: **{len(ran)} of {len(rows)}** ({pct(len(ran), len(rows))})",
        f"- instruction bytes executed: **{tot_cov} of {tot_code}** ({pct(tot_cov, tot_code)})",
    ]
    if unmatched:
        out.append(f"- RAM code that matches no ROM code (not counted): "
                   + ", ".join(f"{lo:08X}-{hi:08X}" for lo, hi in unmatched[:20]))
    out += ["", "| Module | Functions | Executed | Never executed | Code bytes | Covered |",
            "| --- | ---: | ---: | ---: | ---: | ---: |"]
    by_mod = collections.defaultdict(list)
    for r in rows:
        by_mod[r[0]].append(r)
    for n, _, _, _ in mods:
        rs = by_mod.get(n, [])
        if not rs:
            continue
        e = sum(1 for r in rs if r[5])
        t, c = sum(r[4] for r in rs), sum(r[5] for r in rs)
        out.append(f"| {n} | {len(rs)} | {e} | {len(rs) - e} | {t} | {pct(c, t)} |")

    never = [r for r in rows if r[4] and not r[5]]
    out += ["", "## Biggest uncovered code", "",
            "Instruction bytes that never ran, largest first: never-executed functions and the "
            "unexecuted parts of partly covered ones.", "",
            "| Function | Module | Size | Uncovered bytes | Covered |", "| --- | --- | ---: | ---: | ---: |"]
    for r in sorted(rows, key=lambda r: -(r[4] - r[5]))[:25]:
        out.append(f"| `{r[2]}` | {r[0]} | 0x{r[3]:X} | {r[4] - r[5]} | {pct(r[5], r[4])} |")

    out += ["", f"## Never-executed functions ({len(never)})", "",
            "| Address | Function | Module | Size |", "| --- | --- | --- | ---: |"]
    for r in never:
        out.append(f"| 0x{r[1]:08X} | `{r[2]}` | {r[0]} | 0x{r[3]:X} |")

    out += ["", "## Every function", "", "| Address | Function | Module | Size | Covered |",
            "| --- | --- | --- | ---: | ---: |"]
    for r in rows:
        out.append(f"| 0x{r[1]:08X} | `{r[2]}` | {r[0]} | 0x{r[3]:X} | {pct(r[5], r[4])} |")
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w") as f:
        f.write("\n".join(out) + "\n")
    print(f"coverage: {len(ran)}/{len(rows)} functions executed, {pct(tot_cov, tot_code)} of the code "
          f"({os.path.relpath(args.out, ROOT)})")


if __name__ == "__main__":
    main()
