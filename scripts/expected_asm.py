#!/usr/bin/env python3
"""Print the objdiff target for a C module: all of its functions' generated asm, in order.

`make expected` assembles this into expected/src/<module>.o -- the object the
module must produce once every function is decompiled. objdiff compares it,
symbol by symbol, against build/src/<module>.o.

Usage: python3 scripts/expected_asm.py <module>
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_asm import function_names, load_config, modules  # noqa: E402


def main():
    module = sys.argv[1]
    cfg, funcs, _, _ = load_config()
    names = function_names(cfg, funcs)
    for name, kind, lo, hi in modules(cfg):
        if name == module:
            break
    else:
        sys.exit(f"unknown module {module}")
    if kind != "c":
        sys.exit(f"{module} is not a C module")
    print('\t.include "asm/macros.inc"')
    print("\t.syntax unified")
    print("\t.text")
    for a in sorted(a for a in funcs if lo <= a < hi):
        print(f'\t.include "asm/nonmatchings/{module}/{names[a]}.s"')
    print("\t.align 2, 0")


if __name__ == "__main__":
    main()
