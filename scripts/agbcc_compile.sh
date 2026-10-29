#!/usr/bin/env bash
# Compile one C file to an object exactly as the Makefile does for a C module.
#   scripts/agbcc_compile.sh [-I dir...] INPUT.c -o OUTPUT.o
# AGBCC_CC (agbcc or old_agbcc) and AGBCC_CFLAGS="..." must be set, to the module's
# kururin.toml cc/cflags: there is no default, because no single setting fits every
# module. scripts/permute_import.sh and scripts/check_compilers.py set both. -I flags are accepted (decomp-permuter
# reads include paths from them) but the Makefile's include paths are always used.
# Used by decomp-permuter (permuter_settings.toml) and for quick experiments.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IN=""; OUT=""
while [ $# -gt 0 ]; do
    case "$1" in
        -o) OUT="$2"; shift 2 ;;
        -I) shift 2 ;;
        -I*) shift ;;
        *) IN="$1"; shift ;;
    esac
done
: "${AGBCC_CC:?set AGBCC_CC to the cc of the module in kururin.toml (agbcc or old_agbcc)}"
: "${AGBCC_CFLAGS:?set AGBCC_CFLAGS to the cflags of the module in kururin.toml}"
IN="$(realpath "$IN")"; OUT="$(realpath -m "$OUT")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cd "$ROOT"
# -std=gnu17 as in the Makefile (GCC 15's cpp defaults to C23: src/m4a.c's other branch)
arm-none-eabi-cpp -std=gnu17 -nostdinc -I tools/agbcc/include -iquote include "$IN" -o "$TMP/in.i"
# shellcheck disable=SC2086 # AGBCC_CFLAGS is a list of words
"tools/agbcc/bin/$AGBCC_CC" $AGBCC_CFLAGS "$TMP/in.i" -o "$TMP/in.s"
arm-none-eabi-as -mcpu=arm7tdmi -mthumb-interwork "$TMP/in.s" -o "$OUT"
