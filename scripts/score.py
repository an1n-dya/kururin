#!/usr/bin/env python3
"""Score functions with objdiff: match % and a side-by-side diff.

Builds build/src/<unit>.o and expected/src/<unit>.o, runs `objdiff-cli diff`
in one-shot JSON mode and prints, for each requested function, its match
percentage and (unless it matches) the target and current instructions side
by side. A score is evidence only: `make compare` is the verdict (objdiff
compares one symbol; it cannot see changes that move other functions).

Usage:
  python3 scripts/score.py <unit> <function> [<function>...]
  python3 scripts/score.py <unit> --all      # every function below 100%
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDIFF = os.environ.get("OBJDIFF", os.path.join(ROOT, "tools", "objdiff-cli"))


def run_objdiff(unit: str, symbol: str) -> dict:
    with tempfile.NamedTemporaryFile(suffix=".json") as out:
        subprocess.run([OBJDIFF, "diff", "-p", ROOT, "-u", unit, symbol, "-o", out.name],
                       check=True, capture_output=True)
        return json.load(open(out.name))


def symbols(side: dict) -> dict:
    return {s["name"]: s for s in side["symbols"] if s.get("kind") == "SYMBOL_FUNCTION"}


def first_function(unit: str) -> str:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from generate_asm import function_names, load_config, modules

    cfg, funcs, _, _ = load_config()
    names = function_names(cfg, funcs)
    for name, _, lo, hi in modules(cfg):
        if name == unit:
            return names[min(a for a in funcs if lo <= a < hi)]
    sys.exit(f"unknown unit {unit}")


def fmt(ins):
    if "instruction" not in ins:
        return ""
    return ins["instruction"]["formatted"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("unit")
    ap.add_argument("functions", nargs="*")
    ap.add_argument("--all", action="store_true", help="list every function below 100%%")
    args = ap.parse_args()
    if not args.functions and not args.all:
        ap.error("name the functions to score, or --all")

    subprocess.run(["make", "-s", f"build/src/{args.unit}.o", f"expected/src/{args.unit}.o"],
                   cwd=ROOT, check=True)
    if args.all:
        d = run_objdiff(args.unit, args.functions[0] if args.functions else first_function(args.unit))
        target, built = symbols(d["left"]), symbols(d["right"])
        # objdiff's JSON omits `address` when it is 0 (the unit's first symbol)
        below = 0
        for name, s in sorted(built.items(), key=lambda kv: int(kv[1].get("address", 0))):
            pct = s.get("match_percent", 0.0)
            if pct < 100:
                print(f"{pct:6.2f}%  {name}")
                below += 1
        print(f"{args.unit}: {len(built) - below}/{len(target)} functions at 100%")
        if not built or set(built) != set(target):
            sys.exit(f"{args.unit}: the built and target objects have different functions")
        sys.exit(1 if below else 0)
    status = 0
    for fn in args.functions:
        d = run_objdiff(args.unit, fn)
        tgt, cur = symbols(d["left"]).get(fn), symbols(d["right"]).get(fn)
        if cur is None or tgt is None:
            print(f"{fn}: not in {'build' if cur is None else 'expected'}/src/{args.unit}.o")
            status = 1
            continue
        pct = cur.get("match_percent", 0.0)
        print(f"{fn}: {pct:.2f}%")
        if pct < 100:
            status = 1
            for a, b in zip(tgt["instructions"], cur["instructions"]):
                mark = " " if "diff_kind" not in a and "diff_kind" not in b else "|"
                print(f"   {fmt(a):<40} {mark} {fmt(b)}")
    sys.exit(status)


if __name__ == "__main__":
    main()
