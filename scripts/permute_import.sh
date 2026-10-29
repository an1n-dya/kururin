#!/usr/bin/env bash
# Import one function into decomp-permuter (creates nonmatchings/<function>/).
#   scripts/permute_import.sh <module> <function>
# The C for the function must already be in src/<module>.c (replacing its INCLUDE_ASM).
# Then: .venv/bin/python tools/decomp-permuter/permuter.py nonmatchings/<function> -j4
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODULE="$1"; FN="$2"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
# The permuter finds the function name from a `.globl` line, which our macros hide.
{ printf '\t.text\n\t.globl %s\n' "$FN"; cat "$ROOT/asm/nonmatchings/$MODULE/$FN.s"; } > "$TMP/$FN.s"
cd "$ROOT"
PY="$( [ -x .venv/bin/python3 ] && echo .venv/bin/python3 || echo python3 )"
# The module's compiler and flags, as the Makefile uses them (kururin.toml cc/cflags).
read -r CC CFLAGS < <("$PY" - "$MODULE" <<'PYEOF'
import sys, tomllib
m = next(m for m in tomllib.load(open("kururin.toml", "rb"))["modules"] if m["name"] == sys.argv[1])
print(m["cc"], m["cflags"])
PYEOF
)
AGBCC_CC="$CC" AGBCC_CFLAGS="$CFLAGS" "$PY" tools/decomp-permuter/import.py "src/$MODULE.c" "$TMP/$FN.s"
# Bake them into the imported compile script so later permuter runs use them.
sed -i "s|^scripts/agbcc_compile.sh|AGBCC_CC=$CC AGBCC_CFLAGS='$CFLAGS' scripts/agbcc_compile.sh|" \
    "nonmatchings/$FN/compile.sh"
grep -q "^AGBCC_CC=$CC AGBCC_CFLAGS=" "nonmatchings/$FN/compile.sh" \
    || { echo "could not set the compiler in nonmatchings/$FN/compile.sh" >&2; exit 1; }
echo "compiler for $MODULE: $CC $CFLAGS"
