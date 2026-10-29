#!/usr/bin/env python3
"""Discover every function in baserom.gba and write config/functions.txt.

Function discovery combines several independent sources of candidate entry
points, then validates every candidate with the same recursive-descent
explorer (scripts/romanalysis.py):

  manual    ARM routines and overrides from kururin.toml
  crt0      the AgbMain pointer loaded by the ARM start-up code
  bl        targets of BL instructions in accepted functions
  pool      odd ROM pointers in accepted functions' literal pools
  tail      bx to a pool-loaded function pointer
  ptr       odd ROM pointers anywhere in the ROM (function-pointer tables)
  luvdis    Luvdis's own call-count/prologue guesses
  ghidra    functions from a Ghidra headless auto-analysis of the ROM imported
            as ARM:LE:32:v4t (ghidra_out/functions.txt; `make ghidra-functions`)
  gap       unreferenced code at the start of a gap between accepted functions

Candidates with code-level evidence (manual, crt0, bl, pool, tail) are
"strong". The rest are "weak": they must be word-aligned (agbcc aligns every
function), explore cleanly, and fit in a gap without overlapping anything
already accepted. A weak candidate in the middle of a gap needs two
independent sources.

Two agbcc idioms need care:
  * Long branches. When a `b` is out of range agbcc emits `bl` (lr is saved
    in the prologue), so a BL target can be a label inside the caller. Such
    a target pops more than it pushes when explored on its own; it is
    classified as a far jump and the caller is re-explored following it.
  * Jump tables (`mov pc, rX` after a bounds check); see romanalysis.py.

The accepted set must tile the code region: every function is re-explored
with all other entries as stop points, and a function that falls through
into, or branches across, the next entry means one of the two is wrong.
Hand-written asm regions (kururin.toml [[asm_regions]], e.g. m4a_1.s) are
exempt: their routines share code and fall into each other by design.

Anything left between functions that is not alignment padding is data in
code; it must be declared in kururin.toml ([overrides] data_in_code).

Discovery refuses to write config/functions.txt (exit status 1, reasons in
config/discovery_report.txt) when the result is suspect:
  * the functions do not tile the code region;
  * an [overrides] entry contradicts the code: a functions entry that does
    not explore as a function or lies in an asm region, a farjumps entry
    that no BL targets or that explores as a function, a noreturn entry that
    is not a function or has a return path, a not_functions entry that a
    BL, literal pool or tail call names;
  * the data in code differs from what kururin.toml declares, or is code
    (it runs into the next function, or explores as a complete function);
  * a Ghidra or Luvdis function, or a candidate with two sources, is
    rejected (other than as a far-jump target or a declared not_function);
  * an accepted C function has exploration issues.

Outputs:
  config/functions.txt      committed; one line per function (generator input)
  config/data_in_code.txt   committed; data ranges inside the code region
  config/discovery_report.txt statistics and per-source agreement

Usage:
  python3 scripts/discover_functions.py [--no-luvdis] [--ghidra PATH]
"""

from __future__ import annotations

import argparse
import collections
import contextlib
import io
import os
import struct
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from romanalysis import ROM_BASE, FuncInfo, Rom, explore_arm, explore_thumb  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STRONG = {"manual", "crt0", "bl", "pool", "tail"}
SOURCES = ("manual", "crt0", "bl", "pool", "tail", "ptr", "luvdis", "ghidra", "gap")


def load_config():
    with open(os.path.join(ROOT, "kururin.toml"), "rb") as f:
        return tomllib.load(f)


def luvdis_guesses(rom_path: str, code_end: int) -> set[int]:
    """Run Luvdis's analysis (with guessing) over the code region only."""
    import warnings

    warnings.filterwarnings("ignore")
    from luvdis.analyze import State
    from luvdis.rom import ROM

    rom = ROM(rom_path, detect=False)
    state = State({}, 2, 3, ROM_BASE, code_end, None)
    with contextlib.redirect_stderr(io.StringIO()):
        state.analyze_rom(rom, True)
    return set(state.functions)


def ghidra_functions(path: str) -> dict[int, str]:
    """Read ExportFunctions.java's output; it ends with `# end <n> functions`."""
    out = {}
    if not path:
        return out
    with open(path) as f:
        lines = f.read().splitlines()
    trailer = lines[-1].split() if lines else []
    if len(trailer) != 4 or trailer[:2] != ["#", "end"] or not trailer[2].isdigit():
        sys.exit(f"{path} is truncated (no `# end <n> functions` line): re-run `make ghidra-functions`")
    for line in lines[:-1]:
        parts = line.split()
        if len(parts) >= 2 and not line.startswith("#"):
            out[int(parts[0], 16)] = parts[1]
    if len(out) != int(trailer[2]) or not out:
        sys.exit(f"{path}: {len(out)} functions, but its trailer says {trailer[2]}: "
                 "re-run `make ghidra-functions`")
    return out


class Discovery:
    def __init__(self, rom: Rom, cfg: dict):
        self.rom = rom
        r = cfg["rom"]
        self.code_end = r["code_end"]
        self.thumb_start = r["thumb_start"]
        self.arm = {f["start"]: f for f in cfg.get("arm_functions", [])}
        self.asm_regions = [(x["start"], x["end"], x["name"]) for x in cfg.get("asm_regions", [])]
        ov = cfg.get("overrides", {})
        self.forced = set(ov.get("functions", []))
        self.not_functions = set(ov.get("not_functions", []))
        self.declared_data = sorted(tuple(r) for r in ov.get("data_in_code", []))
        self.noreturn = set(ov.get("noreturn", []))
        self.farjumps: set[int] = set(ov.get("farjumps", []))
        self.override_noreturn = set(self.noreturn)
        self.override_farjumps = set(self.farjumps)
        self.sources: dict[int, set[str]] = collections.defaultdict(set)
        self.ptr_refs: dict[int, set[int]] = collections.defaultdict(set)
        self.accepted: dict[int, FuncInfo] = {}
        self.rejected: dict[int, list[str]] = {}
        self._standalone: dict[int, FuncInfo] = {}
        self._resolving: set[int] = set()

    # -- helpers ------------------------------------------------------------

    def in_code(self, addr: int) -> bool:
        return self.thumb_start <= addr < self.code_end

    def asm_region(self, addr: int):
        for lo, hi, name in self.asm_regions:
            if lo <= addr < hi:
                return lo, hi, name
        return None

    def strong(self, e: int) -> bool:
        return bool(self.sources[e] & STRONG) or e in self.forced

    def scan_pointers(self):
        data = self.rom.data
        for off in range(0, self.rom.size - 3, 4):
            v = struct.unpack_from("<I", data, off)[0]
            if v & 1 and self.in_code(v - 1):
                self.sources[v - 1].add("ptr")
                self.ptr_refs[v - 1].add(ROM_BASE + off)

    # -- exploration --------------------------------------------------------

    def explore(self, entry: int, stops=frozenset()) -> FuncInfo:
        return explore_thumb(self.rom, entry, self.code_end, stops, self.noreturn, self.farjumps)

    @staticmethod
    def plausible(fi: FuncInfo) -> bool:
        """Could this be a function entry? (Clean flow, balanced stack.)"""
        return fi.ok and not fi.stack_underflow

    def standalone(self, t: int) -> FuncInfo:
        if t not in self._standalone:
            self._standalone[t] = self.explore_resolved(t)
        return self._standalone[t]

    def explore_resolved(self, e: int) -> FuncInfo:
        """Explore *e*, turning implausible nearby BL targets into far jumps."""
        self._resolving.add(e)
        try:
            for _ in range(32):
                fi = self.explore(e)
                new = set()
                for t in fi.calls:
                    if t in self.farjumps or t in self.accepted or t in self.arm or t in self._resolving:
                        continue
                    if not (e < t < e + 0x10000) or self.asm_region(t):
                        continue
                    if not self.plausible(self.standalone(t)):
                        new.add(t)
                if not new:
                    return fi
                self.farjumps |= new
                self._standalone.clear()
            return fi
        finally:
            self._resolving.discard(e)

    def coverage(self) -> dict[int, int]:
        """Map of byte address -> owning entry for every accepted function."""
        cov = {}
        for e, fi in self.accepted.items():
            for a, i in fi.insns.items():
                for x in range(a, a + i.size):
                    cov[x] = e
            for lo, hi, _ in fi.data_ranges():
                for x in range(lo, hi):
                    cov[x] = e
        for lo, hi, _ in self.asm_regions:
            for x in range(lo, hi):
                cov.setdefault(x, lo)
        return cov

    # -- acceptance ----------------------------------------------------------

    def accept_closure(self, entries):
        """Accept entries and everything they reference with code-level evidence."""
        work = list(entries)
        while work:
            e = work.pop()
            if e in self.accepted or e in self.not_functions or e in self.farjumps:
                continue
            fi = self.explore_resolved(e)
            if not self.plausible(fi) and e not in self.forced and not self.asm_region(e):
                self.rejected[e] = fi.issues or ["stack underflow when explored as a function"]
                continue
            self.accepted[e] = fi
            refs = [(t, "bl") for t in fi.calls] + [(t, "tail") for t in fi.tailcalls]
            for p in fi.pools:
                v = self.rom.u32(p)
                if v & 1 and self.in_code(v - 1):
                    refs.append((v - 1, "pool"))
            for t, why in refs:
                if self.in_code(t) and t not in self.farjumps:
                    self.sources[t].add(why)
                    if t not in self.accepted:
                        work.append(t)
        # Far jumps found along the way may have been accepted as functions earlier.
        for e in list(self.accepted):
            if e in self.farjumps:
                del self.accepted[e]
                self.rejected[e] = ["far-jump target (agbcc long branch), not a function"]
        for e in list(self.accepted):
            self.accepted[e] = self.explore(e)

    def fits_gap(self, fi: FuncInfo, cov) -> bool:
        body = list(fi.insns) + [x for lo, hi, _ in fi.data_ranges() for x in range(lo, hi)]
        return not any(x in cov for x in body)

    def skip_padding(self, lo: int, hi: int) -> int:
        """Skip one zero/nop halfword that pads to a word boundary."""
        if lo % 4 == 2 and lo + 2 <= hi and self.rom.u16(lo) in (0x0000, 0x46C0):
            return lo + 2
        return lo

    def gaps(self, cov=None):
        """Uncovered byte ranges between thumb_start and code_end."""
        cov = self.coverage() if cov is None else cov
        out = []
        a = self.thumb_start
        while a < self.code_end:
            if a in cov:
                a += 1
                continue
            b = a
            while b < self.code_end and b not in cov:
                b += 1
            out.append((a, b))
            a = b
        return out

    def is_padding(self, lo: int, hi: int) -> bool:
        return self.skip_padding(lo, hi) >= hi and hi % 4 == 0

    def scan_gap_one(self) -> int | None:
        """Accept one unreferenced function at the start of a gap."""
        cov = self.coverage()
        for lo, hi in self.gaps(cov):
            start = self.skip_padding(lo, hi)
            if start >= hi or start % 4 or start in self.accepted or start in self.rejected:
                continue
            if start in self.not_functions or start in self.farjumps:
                self.rejected[start] = ["[overrides] not_functions/farjumps"]
                continue
            fi = self.explore_resolved(start)
            if not self.plausible(fi) or not (fi.returns or fi.tailcalls or fi.jumptables):
                continue
            if not self.fits_gap(fi, cov):
                continue
            self.sources[start].add("gap")
            self.accept_closure([start])
            if start not in self.accepted:
                self.rejected.setdefault(start, ["not accepted"])
                continue
            return start
        return None

    def accept_weak_one(self) -> bool:
        cov = self.coverage()
        gap_starts = {self.skip_padding(lo, hi) for lo, hi in self.gaps(cov)}
        weak = [e for e in self.sources
                if e not in self.accepted and e not in self.not_functions and e not in self.rejected
                and e not in self.farjumps and not self.strong(e)]
        weak.sort(key=lambda e: (-len(self.sources[e]), -len(self.ptr_refs.get(e, ())), e))
        for e in weak:
            if e % 4:
                self.rejected[e] = ["not word-aligned"]
                continue
            if e in cov:
                self.rejected[e] = [f"inside accepted function {cov[e]:08X}"]
                continue
            support = len(self.sources[e]) + (len(self.ptr_refs.get(e, ())) > 1)
            if e not in gap_starts and support < 2:
                continue  # may become a gap start later
            fi = self.explore_resolved(e)
            if not self.plausible(fi) or not (fi.returns or fi.tailcalls or fi.jumptables):
                self.rejected[e] = fi.issues or ["stack underflow / no return"]
                continue
            if not self.fits_gap(fi, cov):
                self.rejected[e] = ["overlaps an accepted function"]
                continue
            self.accept_closure([e])
            return True
        return False

    def compute_noreturn(self) -> bool:
        """Functions with no return path never return; a BL to one ends the path."""
        changed = False
        for e, fi in self.accepted.items():
            if e not in self.noreturn and fi.returns == 0 and not fi.tailcalls and not fi.jumptables \
                    and not self.asm_region(e):
                self.noreturn.add(e)
                changed = True
        if changed:
            for e in list(self.accepted):
                self.accepted[e] = self.explore(e)
        return changed

    def label_asm_regions(self):
        """Give hand-written asm routines inside asm regions their own symbols.

        Inside a region, an address that Ghidra, a literal pool, a BL or two
        pointers name as an entry becomes a (global) label if it explores
        cleanly and is not in the middle of an instruction, a literal pool or
        ARM code. The region is still emitted as one file, so this only
        affects symbols.
        """
        def in_table(ref: int) -> bool:
            for n in (ref - 4, ref + 4):
                if self.rom.contains(n, 4):
                    v = self.rom.u32(n)
                    if v & 1 and self.in_code(v - 1):
                        return True
            return False

        for lo, hi, _ in self.asm_regions:
            changed = True
            while changed:
                changed = False
                inside = {e: fi for e, fi in self.accepted.items() if lo <= e < hi}
                blocked = set()
                for fi in inside.values():
                    blocked.update(a + 2 for a, i in fi.insns.items() if i.size == 4)
                    blocked.update(x for p in fi.pools for x in range(p, p + 4))
                    for t in fi.arm_targets:
                        ai = explore_arm(self.rom, t, hi)
                        blocked.update(x for a in ai.insns | ai.pools for x in range(a, a + 4))
                        for p in ai.pools:  # ARM literal pools point at Thumb routines too
                            v = self.rom.u32(p)
                            if v & 1 and lo <= v - 1 < hi:
                                self.sources[v - 1].add("pool")
                for c, src in sorted(self.sources.items()):
                    if not (lo <= c < hi) or c in self.accepted or c in blocked or c % 2:
                        continue
                    refs = self.ptr_refs.get(c, ())
                    if not (src & {"ghidra", "pool", "bl"} or len(refs) > 1 or any(map(in_table, refs))):
                        continue
                    fi = self.explore(c)
                    if fi.ok:
                        self.accepted[c] = fi
                        self.rejected.pop(c, None)
                        changed = True

    # -- tiling -------------------------------------------------------------

    def entries(self):
        return sorted(set(self.accepted) | set(self.arm))

    def check_tiling(self):
        entries = self.entries()
        stops = frozenset(entries)
        problems = {}
        for i, e in enumerate(entries):
            if e in self.arm or self.asm_region(e):
                continue
            nxt = entries[i + 1] if i + 1 < len(entries) else self.code_end
            fi = self.explore(e, stops)
            self.accepted[e] = fi
            bad = [x for x in fi.issues if "falls through" in x or "branches into" in x]
            past = [a for a in fi.insns if a >= nxt] + [p for p in fi.pools if p >= nxt]
            if past:
                bad.append(f"reaches {min(past):08X}, past next entry {nxt:08X}")
            before = [a for a in fi.insns if a < e]
            if before:
                bad.append(f"branches back before its entry to {min(before):08X}")
            if bad:
                problems[e] = (nxt, bad)
        return problems

    def fix_tiling(self):
        """Drop weak entries until no function falls into or across another."""
        log = []
        for _ in range(200):
            problems = self.check_tiling()
            if not problems:
                return log
            changed = False
            for e, (nxt, bad) in sorted(problems.items()):
                victim = None
                if nxt in self.accepted and not self.strong(nxt) and not self.asm_region(nxt):
                    victim = nxt  # a label inside e
                elif not self.strong(e):
                    victim = e  # a label inside its predecessor
                if victim is not None and victim in self.accepted:
                    del self.accepted[victim]
                    self.rejected[victim] = [f"interior label of a neighbour ({'; '.join(bad)})"]
                    log.append(f"{victim:08X} dropped ({'; '.join(bad)})")
                    changed = True
            if not changed:
                for e, (nxt, bad) in sorted(problems.items()):
                    log.append(f"UNRESOLVED {e:08X}: {'; '.join(bad)}")
                return log
        return log


def run_discovery(rom_path: str, cfg: dict, use_luvdis=True, ghidra_path=None):
    rom = Rom(open(rom_path, "rb").read())
    d = Discovery(rom, cfg)
    for a in d.arm:
        d.sources[a].add("manual")
    for a in d.forced:
        d.sources[a].add("manual")
    d.sources[cfg["rom"]["agbmain"]].add("crt0")
    for lo, hi, name in d.asm_regions:
        d.sources[lo].add("manual")
    d.scan_pointers()
    luv = luvdis_guesses(rom_path, d.code_end) if use_luvdis else set()
    for a in luv:
        if d.in_code(a):
            d.sources[a].add("luvdis")
    ghidra = ghidra_functions(ghidra_path)
    for a, mode in ghidra.items():
        if mode == "thumb" and d.in_code(a):
            d.sources[a].add("ghidra")

    d.accept_closure([cfg["rom"]["agbmain"], *d.forced, *(lo for lo, _, _ in d.asm_regions)])
    gap_funcs = []
    for _ in range(100000):
        if d.compute_noreturn():
            continue
        g = d.scan_gap_one()
        if g is not None:
            gap_funcs.append(g)
            continue
        if d.accept_weak_one():
            continue
        break
    else:
        sys.exit("discovery does not converge (a bug in scripts/discover_functions.py)")
    tiling_log = d.fix_tiling()
    d.label_asm_regions()
    cov = d.coverage()
    data = [(lo, hi) for lo, hi in d.gaps(cov) if not d.is_padding(lo, hi)]
    return d, luv, ghidra, gap_funcs, tiling_log, data


def suspect(d: Discovery, luv, ghidra, tiling_log, data) -> list[str]:
    """Reasons not to trust this result (see the module docstring)."""
    errors = [x for x in tiling_log if x.startswith("UNRESOLVED")]
    acc = set(d.entries())
    for e in sorted(d.forced):
        fi = d.explore(e)
        if d.asm_region(e):
            errors.append(f"[overrides] functions entry {e:08X} is inside asm region {d.asm_region(e)[2]}: "
                          "labels there come from Ghidra/BL/pool evidence (label_asm_regions)")
        elif not d.plausible(fi):
            errors.append(f"[overrides] functions entry {e:08X} does not explore as a function: "
                          f"{'; '.join(fi.issues) or 'stack underflow'}")
    bl_targets = {i.target for e in acc if e in d.accepted for i in d.accepted[e].insns.values()
                  if i.kind == "bl"}
    for t in sorted(d.override_farjumps):
        if t not in bl_targets:
            errors.append(f"[overrides] farjumps entry {t:08X} is not the target of any BL")
        elif d.plausible(d.explore(t)):  # automatic far jumps are exactly the implausible BL targets
            errors.append(f"[overrides] farjumps entry {t:08X} explores as a function of its own")
    for t in sorted(d.override_noreturn):
        if t not in acc:
            errors.append(f"[overrides] noreturn entry {t:08X} is not a function entry")
        elif d.explore(t).returns:
            errors.append(f"[overrides] noreturn entry {t:08X} has a return path")
    for e in sorted(d.not_functions):
        strong = d.sources.get(e, set()) & STRONG
        if strong:
            errors.append(f"[overrides] not_functions entry {e:08X} is a function by code evidence "
                          f"({','.join(sorted(strong))})")
    for lo, hi in data:  # code is a lost function, not data
        fi = d.explore(d.skip_padding(lo, hi), frozenset(acc))
        if fi.issues and all("falls through" in x or "branches into" in x for x in fi.issues):
            errors.append(f"data {lo:08X}-{hi:08X} is code that runs into the next function "
                          f"({fi.issues[0]}): a function boundary is wrong")
        elif d.plausible(fi) and (fi.returns or fi.tailcalls):
            errors.append(f"data {lo:08X}-{hi:08X} explores as a complete function: if it really is "
                          "data, this check needs an exception for it")
    if data != d.declared_data:
        errors.append(f"data in code {[(f'{lo:08X}', f'{hi:08X}') for lo, hi in data]} differs from "
                      f"[overrides] data_in_code {[(f'{lo:08X}', f'{hi:08X}') for lo, hi in d.declared_data]}")
    guesses = luv | {a for a, m in ghidra.items() if m == "thumb" and d.in_code(a)}
    for a in sorted(set(d.rejected) | guesses):
        if a in acc or a in d.farjumps or a in d.not_functions:  # far jumps checked above
            continue
        src = d.sources.get(a, set())
        if a in guesses or len(src) >= 2:
            why = d.rejected.get(a, ["not a candidate"])
            errors.append(f"{a:08X} ({','.join(sorted(src))}) rejected: {'; '.join(why[:2])}")
    for e in sorted(acc):
        if e in d.accepted and not d.asm_region(e) and d.accepted[e].issues:
            errors.append(f"{e:08X}: {'; '.join(d.accepted[e].issues)}")
    return errors


def write_atomic(path: str, text: str):
    with open(path + ".tmp", "w") as f:
        f.write(text)
    os.replace(path + ".tmp", path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-luvdis", action="store_true", help="skip Luvdis guessing (faster)")
    ap.add_argument("--ghidra", default=os.path.join(ROOT, "ghidra_out", "functions.txt"),
                    help="Ghidra export from scripts/ghidra/ExportFunctions.java "
                         "(`make ghidra-functions`)")
    ap.add_argument("--no-ghidra", action="store_true",
                    help="run without the Ghidra export (finds fewer m4a_1 labels; do not commit the result)")
    args = ap.parse_args()
    if not args.no_ghidra and not os.path.exists(args.ghidra):
        sys.exit(f"{args.ghidra} is missing: run `make ghidra-functions` first (or pass --no-ghidra "
                 "to experiment; committed config/functions.txt is made with it)")

    cfg = load_config()
    d, luv, ghidra, gap_funcs, tiling_log, data = run_discovery(
        os.path.join(ROOT, "baserom.gba"), cfg, not args.no_luvdis, None if args.no_ghidra else args.ghidra)
    entries = d.entries()
    errors = suspect(d, luv, ghidra, tiling_log, data)
    write_report(d, entries, data, luv, ghidra, gap_funcs, tiling_log, errors)
    if errors:
        sys.exit("discovery result is suspect; config/functions.txt was not written "
                 "(see config/discovery_report.txt):\n  " + "\n  ".join(errors))
    write_functions(d, entries, args.no_ghidra)
    write_data(data)


def func_size(d: Discovery, entries: list[int], i: int) -> int:
    nxt = entries[i + 1] if i + 1 < len(entries) else d.code_end
    return nxt - entries[i]


NO_GHIDRA_MARK = "# made with --no-ghidra: do not commit"


def write_functions(d: Discovery, entries, no_ghidra: bool):
    os.makedirs(os.path.join(ROOT, "config"), exist_ok=True)
    out = ["# Generated by scripts/discover_functions.py -- do not edit by hand;",
           "# put overrides in kururin.toml and re-run the script."]
    if no_ghidra:
        out.append(NO_GHIDRA_MARK)  # scripts/generate_asm.py warns about it
    out.append("#\n# address    mode   size     evidence")
    for t in sorted(d.farjumps):
        out.append(f"farjump   0x{t:08X}  # agbcc long-branch BL target inside its caller")
    for t in sorted(d.noreturn):
        out.append(f"noreturn  0x{t:08X}")
    for i, e in enumerate(entries):
        mode = "arm" if e in d.arm else "thumb"
        src = ",".join(s for s in SOURCES if s in d.sources.get(e, ()))
        region = d.asm_region(e)
        if region:
            src += f" asm:{region[2]}"
        out.append(f"0x{e:08X}  {mode:<5}  0x{func_size(d, entries, i):05X}  {src}")
    write_atomic(os.path.join(ROOT, "config", "functions.txt"), "\n".join(out) + "\n")


def write_data(data):
    out = ["# Generated by scripts/discover_functions.py -- data that sits inside the code",
           "# region but belongs to no function's code, literal pools or jump tables.\n#",
           "# start       end         size"]
    out += [f"0x{lo:08X}  0x{hi:08X}  0x{hi - lo:X}" for lo, hi in data]
    write_atomic(os.path.join(ROOT, "config", "data_in_code.txt"), "\n".join(out) + "\n")


def write_report(d: Discovery, entries, data, luv, ghidra, gap_funcs, tiling_log, errors):
    acc = set(entries)
    lines = [f"functions: {len(entries)} ({len(d.arm)} ARM, {len(entries) - len(d.arm)} Thumb)"]
    by, only = collections.Counter(), collections.Counter()
    for e in entries:
        s = d.sources.get(e, set())
        by.update(s)
        if len(s) == 1:
            only[next(iter(s))] += 1
    lines.append("evidence per source: functions supported (functions supported by that source only)")
    for k in SOURCES:
        lines.append(f"  {k:<7} {by[k]:5d}  ({only[k]})")
    lines.append(f"far-jump targets (agbcc long branches, not functions): {len(d.farjumps)}")
    lines.append(f"luvdis guesses: {len(luv)}; not accepted: {len(luv - acc)}")
    for a in sorted(luv - acc):
        lines.append(f"  luvdis-only {a:08X}: {'; '.join(d.rejected.get(a, ['not a candidate'])[:2])}")
    gt = {a for a, m in ghidra.items() if m == "thumb" and d.in_code(a)}
    lines.append(f"ghidra thumb functions in code region: {len(gt)}; not accepted: {len(gt - acc)}")
    for a in sorted(gt - acc):
        why = d.rejected.get(a) or (["far-jump target"] if a in d.farjumps else ["not a candidate"])
        lines.append(f"  ghidra-only {a:08X}: {'; '.join(why[:2])}")
    lines.append(f"unreferenced functions found at gap starts: {len(gap_funcs)}")
    lines.append(f"tiling fixes: {len(tiling_log)}")
    lines.extend("  " + x for x in tiling_log)
    lines.append(f"data ranges inside code: {len(data)} ({sum(h - l for l, h in data)} bytes)")
    for lo, hi in data:
        lines.append(f"  data {lo:08X}-{hi:08X} ({hi - lo} bytes)")
    notes = [(e, x) for e in entries if e in d.accepted for x in d.accepted[e].issues]
    lines.append(f"exploration notes on accepted functions: {len(notes)}")
    for e, x in notes:
        lines.append(f"  {e:08X}: {x}")
    lines.append(f"errors (functions.txt is written only when there are none): {len(errors)}")
    lines.extend("  " + x for x in errors)
    text = "# Generated by scripts/discover_functions.py\n" + "\n".join(lines) + "\n"
    write_atomic(os.path.join(ROOT, "config", "discovery_report.txt"), text)
    print(text)


if __name__ == "__main__":
    main()
