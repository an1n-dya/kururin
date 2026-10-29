#!/usr/bin/env bash
# Launch REA (rea-agents) as this project's MCP server; registered in .mcp.json.
#
# REA needs absolute paths, so they are resolved here from the checkout. Ghidra is
# REA's native provider. The target to open is the build ELF (kururin.elf after
# `make compare`), which is byte-identical to baserom.gba at 0x08000000 and carries
# every function symbol plus Thumb/ARM mapping symbols; REA_GHIDRA_LANGUAGE_ID
# selects ARMv4T for it. (REA's raw-binary mode is fixed to ARM:LE:32:v7 and
# refuses a language override, so raw baserom.gba imports are not used.)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-$ROOT/tools/ghidra}"
if [ -z "${JAVA_HOME:-}" ] && command -v javac >/dev/null; then
    JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"
    export JAVA_HOME
fi
export REA_GHIDRA_LANGUAGE_ID="${REA_GHIDRA_LANGUAGE_ID:-ARM:LE:32:v4t}"
export REA_GHIDRA_COMPILER_SPEC_ID="${REA_GHIDRA_COMPILER_SPEC_ID:-default}"
export REA_OBJDUMP_COMMAND="${REA_OBJDUMP_COMMAND:-arm-none-eabi-objdump}"
if [ -x "$ROOT/tools/rizin/bin/rizin" ]; then
    export REA_RIZIN_COMMAND="${REA_RIZIN_COMMAND:-$ROOT/tools/rizin/bin/rizin}"
fi
export GHIDRA_HEADLESS_MAXMEM="${GHIDRA_HEADLESS_MAXMEM:-2G}"
# setup.sh's fallback Node, if it installed one
if [ -x "$ROOT/tools/node/bin/node" ]; then
    export PATH="$ROOT/tools/node/bin:$PATH"
fi

# The copy setup.sh installs (and checks with `rea doctor`), pinned by
# config/npm/package-lock.json; before setup.sh has run, the same version via npx.
REA="$ROOT/tools/npm/node_modules/.bin/rea"
if [ -x "$REA" ]; then
    exec "$REA" mcp
fi
exec npx -y rea-agents@6.5.0 mcp
