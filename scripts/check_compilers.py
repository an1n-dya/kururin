#!/usr/bin/env python3
"""Check every C module's compiler setting against committed evidence.

The candidate settings are agbcc, agbcc -fprologue-bugfix and old_agbcc, each at -O
and -O2, with the module's other flags. tests/compiler_evidence/<module>/<function>.c
holds C for one function, plus the declarations it needs, that tells some of them
apart. Modules split off one unit share its evidence: `evidence = "<group>"` in
kururin.toml makes them one group, checked as one module (the game's modules are the
group `game`, tests/compiler_evidence/game/), and they must all have the same cc/cflags.
For every C module (or group) in kururin.toml:

  * each of its evidence files, compiled on its own with the module's cc/cflags, must
    match the ROM (objdiff 100% against expected/src/<module>.o, the module of the group
    that holds the function);
  * every other candidate must be ruled out: some evidence file must not match with it.

A module whose setting the evidence cannot decide yet says so in kururin.toml with
`unverified = "<reason>"`: the candidates still possible are listed instead of failing.
-O3 (-O2 plus -finline-functions) is not a candidate. It differs from -O2 only where a
function calls a small function of the same file, which no evidence covers yet.

The evidence is not decompiled source: src/ keeps each function as INCLUDE_ASM until
it is decompiled the normal way (it may then reuse this C).

Usage:
  make check-compilers                          (make compare runs it too)
  python3 scripts/check_compilers.py [-v]       regenerates the asm first; -v shows each compile
"""

import glob
import json
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_asm import ROOT, function_names, load_config  # noqa: E402

OBJDIFF = os.environ.get("OBJDIFF", os.path.join(ROOT, "tools", "objdiff-cli"))
OPT_LEVELS = {"-O": "-O", "-O1": "-O", "-O2": "-O2"}
BUGFIX = "-fprologue-bugfix"


def key(cc: str, cflags: str):
    """What tells candidates apart: compiler, -fprologue-bugfix, optimization level."""
    flags = cflags.split()
    opts = [f for f in flags if re.fullmatch(r"-O\d?", f)]
    return cc, BUGFIX in flags, OPT_LEVELS.get(opts[-1]) if opts else None


def short(cc: str, cflags: str) -> str:
    cc, bugfix, opt = key(cc, cflags)
    return f"{cc} {opt}" + (f" {BUGFIX}" if bugfix else "")


def candidates(cflags: str) -> list[tuple[str, str]]:
    base = [f for f in cflags.split() if f != BUGFIX and not re.fullmatch(r"-O\d?", f)]
    out = []
    for cc, bugfix in (("agbcc", False), ("agbcc", True), ("old_agbcc", False)):
        for opt in ("-O", "-O2"):
            out.append((cc, " ".join(base + [opt] + ([BUGFIX] if bugfix else []))))
    return out


class Checker:
    def __init__(self, tmp: str, verbose: bool):
        self.tmp, self.verbose, self.cache = tmp, verbose, {}

    def match_percent(self, src: str, module: str, fn: str, cc: str, cflags: str) -> float:
        if (src, cc, cflags) in self.cache:
            return self.cache[src, cc, cflags]
        obj = os.path.join(self.tmp, f"{fn}.o")
        env = dict(os.environ, AGBCC_CC=cc, AGBCC_CFLAGS=cflags)
        r = subprocess.run([os.path.join(ROOT, "scripts", "agbcc_compile.sh"), src, "-o", obj],
                           env=env, capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"{src}: {cc} {cflags} failed:\n{r.stderr}")
        out = os.path.join(self.tmp, "diff.json")
        subprocess.run([OBJDIFF, "diff", "-1", os.path.join(ROOT, "expected", "src", f"{module}.o"),
                        "-2", obj, fn, "-o", out], check=True, capture_output=True)
        pct = next((s.get("match_percent", 0.0) for s in json.load(open(out))["right"]["symbols"]
                    if s["name"] == fn), None)
        if pct is None:
            sys.exit(f"{src}: does not define {fn}")
        if self.verbose:
            print(f"     {module}/{fn}: {pct:6.2f}% with {short(cc, cflags)}")
        self.cache[src, cc, cflags] = pct
        return pct


def main():
    verbose = "-v" in sys.argv[1:]
    if "--generated" not in sys.argv[1:]:  # make compare has just run it
        subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "generate_asm.py")], check=True)
    cfg, funcs, _, _ = load_config()
    names = {name: a for a, name in function_names(cfg, funcs).items()}
    starts = [m["start"] for m in cfg["modules"]] + [cfg["rom"]["code_end"]]
    home = {}  # C module name -> its address range
    for m, end in zip(cfg["modules"], starts[1:]):
        if m["kind"] == "c":
            home[m["name"]] = (m["start"], end)
    # group name -> its modules (a module without `evidence` is a group of its own)
    groups = {}
    for m in cfg["modules"]:
        if m["kind"] == "c":
            groups.setdefault(m.get("evidence", m["name"]), []).append(m)
    mods = {}
    for group, members in groups.items():
        settings = {(m["cc"], m["cflags"], m.get("unverified")) for m in members}
        if len(settings) != 1:
            sys.exit(f"kururin.toml: the modules of evidence group {group} "
                     f"({', '.join(m['name'] for m in members)}) differ in cc/cflags/unverified")
        mods[group] = members[0]
    evidence = {}
    for src in sorted(glob.glob(os.path.join(ROOT, "tests", "compiler_evidence", "*", "*.c"))):
        group, fn = src.split(os.sep)[-2], os.path.splitext(os.path.basename(src))[0]
        module = next((m["name"] for m in groups.get(group, [])
                       if fn in names and home[m["name"]][0] <= names[fn] < home[m["name"]][1]), None)
        if module is None:
            sys.exit(f"{src}: no function {fn} in C module (or evidence group) {group}")
        evidence.setdefault(group, []).append((src, fn, module))
    subprocess.run(["make", "-s", "expected"], cwd=ROOT, check=True)

    failed, verified, unverified = [], [], []
    with tempfile.TemporaryDirectory() as tmp:
        chk = Checker(tmp, verbose)
        for module, m in mods.items():  # a module or an evidence group
            setting = (m["cc"], m["cflags"])
            n = len(groups[module])
            label = f"{module} ({f'{n} modules, ' if n > 1 else ''}{short(*setting)})"
            if key(*setting) not in {key(*c) for c in candidates(m["cflags"])}:
                failed.append(f"{label}: not one of the candidate settings this script can check")
                continue
            files = evidence.get(module, [])
            if not files:
                if "unverified" in m:
                    unverified.append(module)
                    if verbose:
                        print(f"{label}: unverified, no evidence ({m['unverified']})")
                else:
                    failed.append(f"{label}: no tests/compiler_evidence/{module}/ and no `unverified` "
                                  "reason in kururin.toml")
                continue
            bad = [f"{fn} {pct:.2f}%" for src, fn, home_mod in files
                   if (pct := chk.match_percent(src, home_mod, fn, *setting)) != 100.0]
            if bad:
                failed.append(f"{label}: does not match its own evidence: {', '.join(bad)}")
                continue
            survivors = []
            for cand in candidates(m["cflags"]):
                if key(*cand) == key(*setting):
                    continue
                if all(chk.match_percent(src, home_mod, fn, *cand) == 100.0 for src, fn, home_mod in files):
                    survivors.append(short(*cand))
            if survivors and "unverified" not in m:
                failed.append(f"{label}: the evidence does not rule out: {'; '.join(survivors)}")
            elif survivors:
                unverified.append(module)
                if verbose:
                    print(f"{label}: unverified ({m['unverified']}); still possible: {'; '.join(survivors)}")
            elif "unverified" in m:
                failed.append(f"{label}: the evidence decides it: remove `unverified` from kururin.toml")
            else:
                verified.append(module)
                if verbose:
                    print(f"{label}: verified by {len(files)} evidence files")
    print(f"compiler evidence: verified {', '.join(verified) or '-'}; unverified (see kururin.toml) "
          f"{', '.join(unverified) or '-'}" + (f"; FAILED {len(failed)}" if failed else ""))
    if failed:
        sys.exit("compiler evidence check failed:\n  " + "\n  ".join(failed))


if __name__ == "__main__":
    main()
