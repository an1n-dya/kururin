#!/usr/bin/env python3
"""Verify one decompiled function by the rule its coverage sets; /batch commits only on exit 0.

  executed by the test inputs (docs/coverage.md above 0%):
      `make test` and `make shift-test` must pass.
  never executed:
      objdiff must score it 100% (scripts/score.py: byte-identical code), and `make test`
      must pass (CLAUDE.md: every commit passes it). Below 100% it stays asm.

Before that it checks that the function really is C now (no INCLUDE_ASM left for it) and that
the test lock holds (tests/check_lock.py). Afterwards it runs `make compare`, for information
only (whether the whole ROM is still byte-identical; the commit message says so).

A partly covered function passes on its executed paths only: the coverage is printed so the
commit can say so.

Usage:
  python3 scripts/verify.py <function>
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from difficulty import decompiled  # noqa: E402
from generate_asm import function_names, load_config, modules  # noqa: E402
from next import coverage  # noqa: E402  (tools/next.py)

PYTHON = os.path.join(ROOT, ".venv", "bin", "python3")
if not os.path.exists(PYTHON):
    PYTHON = sys.executable


def run(what, cmd) -> bool:
    print(f"verify: {what}: {' '.join(cmd)}", flush=True)
    ok = subprocess.run(cmd, cwd=ROOT).returncode == 0
    print(f"verify: {what}: {'passed' if ok else 'FAILED'}", flush=True)
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("function")
    args = ap.parse_args()
    fn = args.function

    cfg, funcs, _, _ = load_config()
    names = function_names(cfg, funcs)
    addr = next((a for a, n in names.items() if n == fn), None)
    if addr is None:
        sys.exit(f"verify: {fn}: not a function of config/functions.txt")
    mods = modules(cfg)
    unit = next(name for name, kind, lo, hi in mods if lo <= addr < hi)
    if fn not in decompiled(mods, names, funcs):
        sys.exit(f"verify: {fn}: not defined in C in src/{unit}.c (still INCLUDE_ASM?)")
    if not run("test lock", [PYTHON, "tests/check_lock.py"]):
        sys.exit(1)

    covered = coverage().get(fn, 0.0)
    executed = covered > 0
    print(f"verify: {fn} ({unit}, 0x{addr:08X}): {covered:.1f}% executed by the inputs -> "
          + ("make test + make shift-test" if executed else "objdiff 100% + make test"), flush=True)
    if executed:
        ok = run("make test", ["make", "test"]) and run("make shift-test", ["make", "shift-test"])
    else:
        ok = run("objdiff 100%", [PYTHON, "scripts/score.py", unit, fn]) and run("make test", ["make", "test"])
    if not ok:
        print(f"verify: {fn}: FAILED")
        sys.exit(1)
    match = subprocess.run(["make", "compare"], cwd=ROOT, capture_output=True).returncode == 0
    print(f"verify: {fn}: VERIFIED ({'executed' if executed else 'never executed'}, "
          f"{covered:.1f}% covered); make compare: {'byte-identical' if match else 'does not match'}")


if __name__ == "__main__":
    main()
