#!/usr/bin/env python3
"""Print the next functions to decompile, in the order /batch takes them.

Only the game's own code (the modules of evidence group `game` in kururin.toml, one per
subsystem: docs/modules.md): the library C modules (m4a, the SRAM library, MultiBoot, serial)
are decompiled separately, byte-matching only (CLAUDE.md); --progress counts them per module.
Already-decompiled functions (as the generator sees src/<module>.c) and functions listed in
docs/skipped.md are left out. The rest is ordered:

  1. functions the test inputs execute (docs/coverage.md, from the last full `make test`)
     before functions they never execute;
  2. within each, by subsystem: physics, collision, camera, objects, level, graphics, sound,
     save, input, system, other;
  3. within a subsystem, smallest first (instruction bytes, without literal pools).

Each line also gives the function's module (src/<module>.c, asm/nonmatchings/<module>/) and
says how scripts/verify.py will judge the function: `test` (executed: `make test` and `make
shift-test` must pass) or `match` (never executed: objdiff 100%, else it stays asm).

Subsystems are an ordering heuristic, not names (nothing here is evidence for a name). A
function's subsystem comes from, in this order:
  - KururinTAS's analysis of this ROM (tools/research/KururinTAS/Misc/Game_analysis/kkk.txt):
    0x08013D0C "Collision Test", 0x080150E4 (holds 0x08015160-0x0801531E: the per-frame
    collision, damage, penalty and life-loss branches), 0x08004104 "CREATE_OBJECT";
  - what its literal pool loads (first rule that matches): the physical tile tables
    (collision), the Helirins (physics), gCamera (camera), m4a calls (sound), SRAM library calls
    (save), KEYINPUT (input), VRAM/OAM/palette or display registers (graphics), gGameState and
    the KuruTools level tables (level), gMain or the interrupt registers (system);
  - otherwise its callers' most common subsystem, then its callees', else `other`.

Usage:
  python3 tools/next.py [N]          # the next N targets (default 10)
  python3 tools/next.py --all        # every remaining target
  python3 tools/next.py N --names    # names only, one per line
  python3 tools/next.py --progress   # decompiled functions: game, and all C-module functions (and
                                     # how many are reviewed-only, docs/audit.md final C pass)
"""

import argparse
import collections
import os
import re
import signal
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
from difficulty import decompiled, features, tier  # noqa: E402
from generate_asm import function_names, load_config, modules  # noqa: E402
from romanalysis import Rom, explore_thumb  # noqa: E402

GAME = "game"  # the evidence group of the game's modules (kururin.toml `evidence`)
SUBSYSTEMS = ("physics", "collision", "camera", "objects", "level", "graphics", "sound", "save",
              "input", "system", "other")

# KururinTAS Misc/Game_analysis/kkk.txt (addresses of this ROM: they fall on our function
# entries and inside 0x080150E4).
SEEDS = {
    0x08013D0C: "collision",  # "Collision Test: Beginning: 08013D0C", reads tile ID & 0x3FF
    0x080150E4: "physics",    # 08015160..0801531E: COLLISIONS, damage, +180 penalty, lose life
    0x08004104: "objects",    # "PROC0 (08004104) CREATE_OBJECT"
}


def pool_rules(cfg):
    sym = cfg["symbols"]
    within = lambda lo, n: (lambda v: lo <= v < lo + n)  # noqa: E731
    return [
        ("collision", within(sym["gPhysicalTiles"], 0x4000)),  # gPhysicalTiles + ...Challenge
        ("physics", within(sym["gHelirins"], 4 * 0xCC)),
        ("camera", within(sym["gCamera"], 0x24)),
        ("input", lambda v: v == 0x04000130),
        ("graphics", lambda v: 0x05000000 <= v < 0x08000000 or 0x04000000 <= v < 0x04000060),
        ("level", lambda v: within(sym["gGameState"], 0x24)(v)
         or v in (sym["gLevelAreaCounts"], sym["gLevelTimes"], sym["gWorldEntries"])),
        ("system", lambda v: within(sym["gMain"], 0x40)(v) or 0x04000200 <= v < 0x04000210
         or v == 0x03007FFC),
    ]


def coverage():
    """Function name -> executed share (0.0-100.0), from docs/coverage.md's "Every function"."""
    cov, on = {}, False
    for line in open(os.path.join(ROOT, "docs", "coverage.md")):
        if line.startswith("## "):
            on = line.strip() == "## Every function"
            continue
        m = on and re.match(r"\| 0x[0-9A-F]+ \| `([^`]+)` \| \w+ \| 0x[0-9A-F]+ \| ([\d.]+)% \|", line)
        if m:
            cov[m.group(1)] = float(m.group(2))
    if not cov:
        sys.exit("docs/coverage.md has no \"Every function\" table: run a full `make test`")
    return cov


def reviewed_only():
    """Functions accepted as functional C reviewed against the asm (docs/audit.md, "Final C
    pass": never executed, not byte-identical; maintainer decision 2026-10-21)."""
    path = os.path.join(ROOT, "docs", "audit.md")
    if not os.path.exists(path):
        return set()
    text = open(path).read()
    start = text.find("## Final C pass")
    if start < 0:
        return set()
    end = text.find("\n## ", start + 1)
    names = set()
    for line in text[start:end if end >= 0 else None].splitlines():
        m = re.match(r"\| `(\w+)`.*\| functional, reviewed, not executable on one console \|$", line)
        if m:
            names.add(m.group(1))
    return names


def skipped():
    """Function names in docs/skipped.md (first column of its table: | `name` | ...)."""
    path = os.path.join(ROOT, "docs", "skipped.md")
    if not os.path.exists(path):
        return set()
    return {m.group(1) for m in re.finditer(r"^\| `([^`]+)` \|", open(path).read(), re.M)}


def classify(rom, cfg, mods, funcs, farjumps, noreturn, lo, hi):
    """Address -> subsystem for every function of [lo, hi)."""
    stops = frozenset(funcs)
    by_module = {a: name for a in funcs for name, _, mlo, mhi in mods if mlo <= a < mhi}
    rules = pool_rules(cfg)
    sub, calls = {}, {}
    for a in sorted(x for x in funcs if lo <= x < hi):
        fi = explore_thumb(rom, a, cfg["rom"]["code_end"], stops - {a}, noreturn, farjumps)
        callees = {t for t in fi.calls | fi.tailcalls if t not in farjumps}
        calls[a] = {t for t in callees if lo <= t < hi}
        if a in SEEDS:
            sub[a] = SEEDS[a]
            continue
        words = {rom.u32(p) for p in fi.pools}
        targets = {by_module.get(t) for t in callees}
        found = [s for s, test in rules if any(test(w) for w in words)]
        if "m4a" in targets or "m4a_1" in targets:
            found.append("sound")
        if "agb_sram" in targets:
            found.append("save")
        if found:
            sub[a] = min(found, key=SUBSYSTEMS.index)
    callers = collections.defaultdict(set)
    for a, cs in calls.items():
        for t in cs:
            callers[t].add(a)
    for neighbours in (callers, calls):
        changed = True
        while changed:
            changed = False
            for a in calls:
                if a in sub:
                    continue
                votes = collections.Counter(sub[n] for n in neighbours[a] if n in sub)
                if votes:
                    sub[a] = min(votes, key=lambda s: (-votes[s], SUBSYSTEMS.index(s)))
                    changed = True
    return {a: sub.get(a, "other") for a in calls}


def main():
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("n", nargs="?", type=int, default=10, help="how many targets (default 10)")
    ap.add_argument("--all", action="store_true", help="every remaining target")
    ap.add_argument("--names", action="store_true", help="print names only")
    ap.add_argument("--progress", action="store_true", help="print decompiled counts and exit")
    args = ap.parse_args()

    cfg, funcs, farjumps, noreturn = load_config()
    names = function_names(cfg, funcs)
    mods = modules(cfg)
    done = decompiled(mods, names, funcs)
    with open(os.path.join(ROOT, "kururin.toml"), "rb") as f:
        asm_regions = [(r["start"], r["end"]) for r in tomllib.load(f).get("asm_regions", [])]
    c_funcs = [a for name, kind, lo, hi in mods if kind == "c" for a in funcs
               if lo <= a < hi and not any(r0 <= a < r1 for r0, r1 in asm_regions)]
    game_mods = {m["name"] for m in cfg["modules"] if m.get("evidence") == GAME}
    ranges = [(lo, hi) for name, _, lo, hi in mods if name in game_mods]
    lo, hi = min(r[0] for r in ranges), max(r[1] for r in ranges)  # contiguous
    game = [a for a in c_funcs if lo <= a < hi]
    module = {a: name for a in game for name, _, mlo, mhi in mods if mlo <= a < mhi}

    if args.progress:
        n_game = sum(names[a] in done for a in game)
        n_all = sum(names[a] in done for a in c_funcs)
        print(f"game ({len(game_mods)} modules): {n_game}/{len(game)} decompiled")
        for name, kind, mlo, mhi in mods:
            if kind == "c" and name not in game_mods:
                mod = [a for a in c_funcs if mlo <= a < mhi]
                print(f"{name} (library, byte-matching): {sum(names[a] in done for a in mod)}/{len(mod)} decompiled")
        print(f"all C-module functions (not asm regions): {n_all}/{len(c_funcs)} decompiled")
        reviewed = reviewed_only()
        n_rev = sum(names[a] in done and names[a] in reviewed for a in game)
        if n_rev:
            print(f"  of the game's: {n_rev} never executed and not byte-identical, accepted as functional C "
                  "reviewed against the asm (docs/audit.md, final C pass)")
        return

    rom = Rom(open(os.path.join(ROOT, "baserom.gba"), "rb").read())
    cov, skip = coverage(), skipped()
    sub = classify(rom, cfg, mods, funcs, farjumps, noreturn, lo, hi)
    stops = frozenset(funcs)
    rows = []
    for a in game:
        n = names[a]
        if n in done or n in skip:
            continue
        f = features(rom, a, cfg["rom"]["code_end"], stops - {a}, noreturn, farjumps)
        executed = cov.get(n, 0.0) > 0
        rows.append(((not executed, SUBSYSTEMS.index(sub[a]), f["code"], a), a, n, sub[a], f, cov.get(n, 0.0)))
    rows.sort()
    if not args.all:
        rows = rows[:args.n]
    for key, a, n, s, f, c in rows:
        if args.names:
            print(n)
        else:
            rule = "match" if key[0] else "test"
            print(f"{a:08X} {n:<16} {module[a]:<11} {s:<10} {f['code']:5d}B {tier(f):<7} covered {c:5.1f}%  "
                  f"verify: {rule}")


if __name__ == "__main__":
    main()
