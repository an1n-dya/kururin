#!/usr/bin/env python3
"""Refuse to test with a changed test: the files of tests/locked_files.txt must hash as in
tests/lock.sha256.

`make test` and `make shift-test` run this first. The locked files are the gameplay harness,
the input recordings, the save images' descriptions and their generator, the comparison list, the
ROM address map, the Makefile and this lock itself (see tests/locked_files.txt); a changed, added
or removed file fails with its name. A test that can be edited to pass proves nothing, so
decompilation work never changes these files.

Usage:
  python3 tests/check_lock.py            # check (exit 1 on any difference)
  python3 tests/check_lock.py --update   # re-lock: the repository owner only, in its own commit
"""

import argparse
import glob
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIST = os.path.join(ROOT, "tests", "locked_files.txt")
LOCK = os.path.join(ROOT, "tests", "lock.sha256")


def locked_files() -> list[str]:
    files = set()
    for line in open(LIST):
        pattern = line.split("#", 1)[0].strip()
        if not pattern:
            continue
        # relative to ROOT (whose path may hold glob characters), and hidden files too: run.py
        # runs an input named .x.txt
        matches = glob.glob(pattern, root_dir=ROOT, include_hidden=True)
        if not matches:
            sys.exit(f"tests/locked_files.txt: {pattern} matches no file")
        files.update(p for p in matches if os.path.isfile(os.path.join(ROOT, p)))
    return sorted(files)


def sha256(path: str) -> str:
    with open(os.path.join(ROOT, path), "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--update", action="store_true", help="rewrite tests/lock.sha256 (owner only)")
    args = ap.parse_args()

    current = {p: sha256(p) for p in locked_files()}
    if args.update:
        with open(LOCK, "w") as f:
            f.write("".join(f"{h}  {p}\n" for p, h in current.items()))
        print(f"tests/lock.sha256: {len(current)} files locked")
        return
    if not os.path.exists(LOCK):
        sys.exit("tests/lock.sha256 is missing: the tests are not locked")
    stored = {}
    for line in open(LOCK):
        h, p = line.rstrip("\n").split("  ", 1)
        stored[p] = h
    problems = [f"changed: {p}" for p in current if p in stored and current[p] != stored[p]]
    problems += [f"not locked (new file): {p}" for p in current if p not in stored]
    problems += [f"missing: {p}" for p in stored if p not in current]
    if problems:
        sys.exit("the test files are locked (tests/locked_files.txt) and differ from "
                 "tests/lock.sha256:\n  " + "\n  ".join(problems)
                 + "\nRestore them (git checkout -- <file>); only the repository owner re-locks.")
    print(f"test lock: {len(current)} files unchanged")


if __name__ == "__main__":
    main()
