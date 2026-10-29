#!/usr/bin/env python3
"""Generate asm/, data/ and ldscript.txt from baserom.gba.

This script is the source of truth for every generated .s file: nothing it
writes is committed (see .gitignore). Inputs:

  baserom.gba             the cartridge (SHA1 checked against kururin.toml)
  kururin.toml            modules, ARM routines, asm regions, renames
  config/functions.txt    function entries, far jumps, noreturn
                          (written by scripts/discover_functions.py)
  config/data_pointers.txt absolute ROM pointers inside the data
                          (written by scripts/find_pointers.py)

Thumb code is disassembled by Luvdis. Luvdis's own control-flow analysis
cannot follow agbcc jump tables or long-branch BLs, so it is not used for
classification: scripts/romanalysis.py explores every function, and its
result (which bytes are code, literal pools, jump tables) is loaded into
Luvdis's flag map before Luvdis formats each function. ARM code (crt0,
interrupt handlers, m4a's ARM blocks) is emitted as `.inst` words, since
Luvdis is Thumb-only.

Outputs (all gitignored):
  asm/nonmatchings/<module>/<function>.s   one per function in C modules
  asm/<module>.s                           asm modules (crt0, m4a_1, ...)
  data/<module>.s                          incbin'd data, with every pointer of
                                           config/data_pointers.txt as `.4byte <symbol>`
                                           and a global label at every address the
                                           code or a pointer refers to
  ldscript.txt, modules.mk, objdiff.json   link order, per-module compilers, objdiff units
  src/<module>.c                           only if missing (or --init-src)

`lib` modules (libgcc, libc) generate nothing: their bytes are identical to
members of agbcc's own libgcc.a/libc.a, which the build links directly.

The script refuses to leave a usable broken tree (on any problem it exits 1 and
removes modules.mk and ldscript.txt, so no build can use what it wrote): a [renames] entry that is not a
function, a C module without cc/cflags, [overrides] that config/functions.txt
does not reflect, a [symbols] entry outside EWRAM/IWRAM/ROM data or named like a
function, a per-function file that uses a label another file defines or holds
data, a C-module function whose code falls into, branches into or reaches past
its neighbour, or a src/<module>.c that (as cpp sees it) does not list every
function in address order are all errors.

Every ROM address in the output is a symbol, so the linker can move code and data
(`make shift-test`, `--shift`): function entries by name, data addresses as `gUnk_<addr>`
(or their [symbols] name) labels in data/data.s. [symbols] in ROM data are such labels,
not linker-script constants.

Usage:
  python3 scripts/generate_asm.py [--init-src] [--shift N]
"""

from __future__ import annotations

import argparse
import contextlib
import glob
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys
import tomllib
import warnings

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from romanalysis import ROM_BASE, Rom, explore_arm, explore_thumb  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASEROM = os.path.join(ROOT, "baserom.gba")
# INCLUDE_ASM("asm/nonmatchings/<module>", <name>) as cpp expands it (include/include_asm.h)
INCLUDE_ASM_RE = re.compile(r'"\.include \\""\s*"asm/nonmatchings/(\w+)"\s*"/"\s*"(\w+)"\s*"\.s\\"')
# The C preprocessor and flags the Makefile uses (CPPFLAGS)
CPP = ["arm-none-eabi-cpp", "-std=gnu17", "-nostdinc", "-I", "tools/agbcc/include", "-iquote", "include"]
# Where [symbols] may point: EWRAM, IWRAM, and ROM data after the code
SYMBOL_RAM = ((0x02000000, 0x02040000), (0x03000000, 0x03008000))


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------


def load_config():
    with open(os.path.join(ROOT, "kururin.toml"), "rb") as f:
        cfg = tomllib.load(f)
    funcs, farjumps, noreturn = {}, set(), set()
    with open(os.path.join(ROOT, "config", "functions.txt")) as f:
        for line in f:
            line = line.split("#", 1)[0].split()
            if not line:
                continue
            if line[0] == "farjump":
                farjumps.add(int(line[1], 16))
            elif line[0] == "noreturn":
                noreturn.add(int(line[1], 16))
            else:
                funcs[int(line[0], 16)] = line[1]
    if "# made with --no-ghidra" in open(os.path.join(ROOT, "config", "functions.txt")).read():
        if os.environ.get("ALLOW_NO_GHIDRA") != "1":
            sys.exit("config/functions.txt was made with --no-ghidra (it misses m4a_1 labels): run "
                     "`make ghidra-functions` and discovery again, or set ALLOW_NO_GHIDRA=1 to "
                     "experiment (never commit that functions.txt)")
        print("warning: config/functions.txt was made with --no-ghidra; do not commit it", file=sys.stderr)
    return cfg, funcs, farjumps, noreturn


def function_names(cfg, funcs) -> dict[int, str]:
    names = {a: f"sub_{a:08X}" for a in funcs}
    for f in cfg.get("arm_functions", []):
        if "name" in f:
            names[f["start"]] = f["name"]
    stray = []
    for k, v in cfg.get("renames", {}).items():
        a = int(k, 16)
        if a not in funcs:
            stray.append(f"0x{a:08X} ({v})")
        names[a] = v
    if stray:
        sys.exit("[renames] entries that are not function entries in config/functions.txt: "
                 + ", ".join(stray))
    symbols = cfg.get("symbols", {})
    rom_data = (cfg["rom"]["code_end"], ROM_BASE + cfg["rom"]["size"])
    bad = [f"{n} = {a!r}" for n, a in symbols.items() if not isinstance(a, int)]
    if bad:
        sys.exit("[symbols] values must be integers (0x...): " + ", ".join(bad))
    bad = [f"{n} = 0x{a:08X}" for n, a in symbols.items()
           if not any(lo <= a < hi for lo, hi in (*SYMBOL_RAM, rom_data))]
    if bad:
        sys.exit("[symbols] is for EWRAM/IWRAM/ROM data (I/O registers go in include/gba/io_reg.h, "
                 "functions in [renames]): " + ", ".join(bad))
    dup = {n for n in names.values() if list(names.values()).count(n) > 1}
    dup |= set(names.values()) & set(symbols)
    dup |= {n for n, a in symbols.items() if list(symbols.values()).count(a) > 1}
    if dup:
        sys.exit(f"names used twice (functions, [symbols], or one address named twice): {sorted(dup)}")
    sizes = cfg.get("symbol_sizes", {})
    bad = [f"{n} = {v!r}" for n, v in sizes.items()
           if n not in symbols or not isinstance(v, int) or v <= 0
           or any(lo <= symbols[n] < hi and symbols[n] + v > hi for lo, hi in SYMBOL_RAM)
           or not any(lo <= symbols[n] < hi for lo, hi in SYMBOL_RAM)]
    if bad:
        sys.exit("[symbol_sizes] entries must name a RAM [symbols] entry and give a positive "
                 "size inside its region: " + ", ".join(bad))
    spans = sorted((symbols[n], symbols[n] + v, n) for n, v in sizes.items())
    overlap = [f"{a} and {b}" for (_, end, a), (start, _, b) in zip(spans, spans[1:]) if start < end]
    if overlap:
        sys.exit("[symbol_sizes] objects overlap: " + ", ".join(overlap))
    return names


def check_overrides(cfg, funcs, farjumps, noreturn) -> list[str]:
    """[overrides] and config/functions.txt must agree: discovery was rerun after each change."""
    ov = cfg.get("overrides", {})
    problems = [f"[overrides] functions entry 0x{a:08X} is not in config/functions.txt"
                for a in ov.get("functions", []) if a not in funcs]
    problems += [f"[overrides] not_functions entry 0x{a:08X} is in config/functions.txt"
                 for a in ov.get("not_functions", []) if a in funcs]
    problems += [f"[overrides] farjumps entry 0x{a:08X} is not a farjump line in config/functions.txt"
                 for a in ov.get("farjumps", []) if a not in farjumps]
    problems += [f"[overrides] noreturn entry 0x{a:08X} is not a noreturn line in config/functions.txt"
                 for a in ov.get("noreturn", []) if a not in noreturn]
    found = []
    for line in open(os.path.join(ROOT, "config", "data_in_code.txt")):
        parts = line.split("#", 1)[0].split()
        if parts:
            found.append((int(parts[0], 16), int(parts[1], 16)))
    if sorted(found) != sorted(tuple(r) for r in ov.get("data_in_code", [])):
        problems.append("[overrides] data_in_code differs from config/data_in_code.txt")
    if problems:
        problems.append("rerun scripts/discover_functions.py after changing [overrides]")
    return problems


def modules(cfg):
    """[(name, kind, start, end)] in link order."""
    mods = cfg["modules"]
    bad = [m["name"] for m in mods if m["kind"] == "c"
           and (m.get("cc") not in ("agbcc", "old_agbcc") or not isinstance(m.get("cflags"), str))]
    if bad:
        sys.exit(f"C modules without cc (agbcc/old_agbcc) and cflags in kururin.toml: {', '.join(bad)} "
                 "(a split-off module takes its parent's, with the parent's evidence comment)")
    out = []
    for i, m in enumerate(mods):
        end = mods[i + 1]["start"] if i + 1 < len(mods) else ROM_BASE + cfg["rom"]["size"]
        out.append((m["name"], m["kind"], m["start"], end))
    return out


# ---------------------------------------------------------------------------
# Analysis -> Luvdis
# ---------------------------------------------------------------------------


class Disassembly:
    def __init__(self, cfg, funcs, farjumps, noreturn):
        warnings.filterwarnings("ignore")
        from luvdis import analyze as lv
        from luvdis.rom import ROM

        self.lv = lv
        self.cfg = cfg
        self.code_end = cfg["rom"]["code_end"]
        self.rom = Rom(open(BASEROM, "rb").read())
        self.lrom = ROM(BASEROM, detect=False)
        self.funcs = funcs
        self.names = function_names(cfg, funcs)
        self.thumb = sorted(a for a, m in funcs.items() if m == "thumb")
        self.arm_entries = sorted(a for a, m in funcs.items() if m == "arm")

        self.arm_code: set[int] = set()  # word addresses of ARM instructions
        self.arm_pools: set[int] = set()
        self.jumptables: dict[int, list[int]] = {}  # table address -> targets
        self.thumb_pools: set[int] = set()
        self.data_symbols = {v: k for k, v in cfg.get("symbols", {}).items()}
        # RAM objects with a known size ([symbol_sizes]): start, end, name
        self.symbol_spans = sorted((cfg["symbols"][n], cfg["symbols"][n] + size, n)
                                   for n, size in cfg.get("symbol_sizes", {}).items())
        self.rom_end = ROM_BASE + cfg["rom"]["size"]
        # pool word address -> (linker constant, value): [linker_constants]
        self.pool_constants = {a: (n, c["value"]) for n, c in cfg.get("linker_constants", {}).items()
                               for a in c["pools"]}
        self.finfo = {}  # thumb entry -> FuncInfo (explored with every other entry as a stop)

        st = lv.State({}, start=ROM_BASE, stop=self.code_end)
        st.flags = lv.RomFlags(self.lrom.size)
        st.label_map = {}
        st.functions = {a: (self.names[a], None) for a in self.thumb}
        self.state = st
        self._analyse(farjumps, noreturn)
        st.make_labels(self.lrom)
        self._data_labels()

    def _data_labels(self):
        """A label for every data address the code or a data pointer refers to."""
        self.data_pointers = {}
        for line in open(os.path.join(ROOT, "config", "data_pointers.txt")):
            parts = line.split("#", 1)[0].split()
            if parts:
                self.data_pointers[int(parts[0], 16)] = int(parts[1], 16)
        targets = set(self.data_pointers.values())
        targets |= {self.rom.u32(p) for p in self.thumb_pools | self.arm_pools}
        targets |= {a for a in self.data_symbols if self.is_data(a)}
        targets = {t for t in targets if self.is_data(t)}
        self.labels = {}  # data address -> label
        inside = {}       # target inside a pointer word -> that word's address
        for t in targets:
            word = next((loc for loc in range(t - 3, t) if loc in self.data_pointers), None)
            if word is not None:
                inside[t] = word
                targets.add(word)  # noqa: B038 (word is a pointer location, never inside one)
        for t in sorted(targets):
            if t not in inside:
                self.labels[t] = self.data_symbols.get(t) or f"gUnk_{t:08X}"
        self.label_refs = {t: self.labels[t] for t in self.labels}
        self.label_refs.update({t: f"{self.labels[w]}+{t - w}" for t, w in inside.items()})

    def ram_ref(self, v: int) -> str | None:
        """The symbol (expression) for RAM address *v*: a [symbols] name, or `name+offset`
        for an address inside a [symbol_sizes] object (e.g. `gUnk_03003120+0x28` for
        &gUnk_03003120[2]), which is what C that indexes the object compiles to."""
        if self.is_data(v):
            return None
        if v in self.data_symbols:
            return self.data_symbols[v]
        for start, end, name in self.symbol_spans:
            if start < v < end:
                return f"{name}+0x{v - start:X}"
        return None

    def is_data(self, v: int) -> bool:
        return self.code_end <= v <= self.rom_end

    def rom_ref(self, v: int) -> str | None:
        """The symbol (expression) for ROM address *v*: a data label or a function entry
        (Thumb entries with bit 0 set, which the linker adds for a Thumb function symbol)."""
        if self.is_data(v):
            return self.label_refs[v]
        entry = v & ~1
        if entry in self.funcs and (v & 1) == (self.funcs[entry] == "thumb"):
            return self.names[entry]
        return None

    def _mark(self, lo, hi, flag):
        self.state.flags[lo:hi] = flag

    def _analyse(self, farjumps, noreturn):
        lv, st = self.lv, self.state
        stops = frozenset(self.thumb) | frozenset(self.arm_entries)
        thumb_work = list(self.thumb)
        explored = set()
        arm_work = list(self.arm_entries)
        while thumb_work or arm_work:
            while thumb_work:
                e = thumb_work.pop()
                if e in explored:
                    continue
                explored.add(e)
                fi = explore_thumb(self.rom, e, self.code_end, stops - {e}, noreturn, farjumps)
                self.finfo[e] = fi
                for a, ins in fi.insns.items():
                    self._mark(a, a + ins.size, lv.FLAG_EXEC)
                    if ins.kind in ("b", "bcond", "bl"):
                        st.label_map.setdefault(ins.target, lv.BRANCH)
                for p in fi.pools:
                    self._mark(p, p + 4, lv.FLAG_WORD)
                    st.label_map[p] = lv.WORD
                for table, targets in fi.jumptables.values():
                    self._mark(table, table + 4 * len(targets), lv.FLAG_WORD)
                    st.label_map[table] = lv.WORD
                    self.jumptables[table] = targets
                    for t in targets:
                        st.label_map.setdefault(t, lv.BRANCH)
                self.thumb_pools |= fi.pools
                arm_work.extend(fi.arm_targets)
            while arm_work:
                e = arm_work.pop()
                if e in explored:
                    continue
                explored.add(e)
                ai = explore_arm(self.rom, e, self.code_end)
                if not ai.ok:
                    sys.exit(f"ARM exploration of {e:08X} failed: {ai.issues}")
                self.arm_code |= ai.insns
                self.arm_pools |= ai.pools
                for t in ai.thumb_targets:
                    st.label_map.setdefault(t, lv.BRANCH)
                    thumb_work.append(t)
        # Padding nops (`bx rX; nop` in libgcc's _call_via_rX) read better as `nop`.
        for a in range(ROM_BASE + 0x2, self.code_end, 4):
            if self.rom.u16(a) == 0x46C0 and not st.flags[a] and st.flags[a - 2] & lv.FLAG_EXEC:
                self._mark(a, a + 2, lv.FLAG_EXEC)

    # -- emission ------------------------------------------------------------

    def luvdis(self, lo: int, hi: int) -> list[str]:
        """Luvdis's disassembly of [lo, hi) as lines."""
        st = self.state
        st.start, st.stop = lo, hi
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            st.dump(self.lrom, None, None, default_mode=self.lv.BYTE)
        warns = [w for w in err.getvalue().splitlines() if "Warning" in w]
        if warns:
            sys.exit(f"Luvdis warnings for {lo:08X}-{hi:08X}:\n" + "\n".join(warns))
        return out.getvalue().splitlines()

    def label(self, addr: int) -> str:
        return self.names.get(addr) or f"_{addr:08X}"

    def thumb_lines(self, lo: int, hi: int) -> list[str]:
        """Thumb code in [lo, hi), splicing in ARM blocks."""
        lines = []
        a = lo
        while a < hi:
            if a in self.arm_code or a in self.arm_pools:
                b = a
                while b < hi and (b in self.arm_code or b in self.arm_pools):
                    b += 4
                lines += ["\t.arm"] + self.arm_lines(a, b) + ["\t.thumb"]
                a = b
                continue
            b = a + 2
            while b < hi and b not in self.arm_code and b not in self.arm_pools:
                b += 2
            lines += self.luvdis(a, b)
            a = b
        return self.symbolize(lines)

    def symbolize(self, lines: list[str]) -> list[str]:
        """Pool words and jump tables as symbols.

        * jump-table entries, and pool words holding a jump table's address, become
          local labels -- only labels defined in the same chunk, so every generated
          file assembles on its own;
        * pool words equal to a [symbols] address in kururin.toml become that symbol,
          so a decompiled function that names the object matches its target exactly
          (I/O addresses are never symbols: C reaches them through literal-address macros);
          those inside a RAM object of known size ([symbol_sizes]) become `symbol+offset`;
        * pool words listed in [linker_constants] become that constant's name (a number the
          original build defined in its linker script, such as m4a's gNumMusicPlayers);
        * pool words holding a ROM address become its symbol: a data label (see
          _data_labels) or a function, so the linker relocates them.
        """
        defined = {int(m.group(1), 16) for m in (re.match(r"^_([0-9A-F]{8}):", x) for x in lines) if m}
        tables = set(self.jumptables)
        out = []
        in_table = False
        for line in lines:
            m = re.match(r"^(?:_([0-9A-F]{8}): )?\s*\.4byte 0x([0-9A-F]{8})$", line.strip())
            if not m:
                in_table = False
                out.append(line)
                continue
            addr = int(m.group(1), 16) if m.group(1) else None
            value = int(m.group(2), 16)
            if addr is not None:
                in_table = addr in tables
            pool_to_table = addr in self.thumb_pools and value in tables
            prefix = f"_{addr:08X}: " if addr is not None else "\t"
            if (in_table or pool_to_table) and value in defined:
                line = f"{prefix}.4byte _{value:08X}"
            elif addr in self.pool_constants:
                name, want = self.pool_constants[addr]
                if value != want:
                    sys.exit(f"[linker_constants] {name}: the pool word at {addr:08X} is 0x{value:X}, not 0x{want:X}")
                line = f"{prefix}.4byte {name}"
            elif addr in self.thumb_pools and self.ram_ref(value):
                line = f"{prefix}.4byte {self.ram_ref(value)}"
            elif addr in self.thumb_pools and self.rom_ref(value):
                line = f"{prefix}.4byte {self.rom_ref(value)}"
            out.append(line)
        return out

    def arm_lines(self, lo: int, hi: int) -> list[str]:
        """ARM words in [lo, hi) as `.inst` (code) or `.4byte` (pools/data)."""
        try:
            import capstone

            cs = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
        except ImportError:
            cs = None
        lines = []
        for a in range(lo, hi, 4):
            w = self.rom.u32(a)
            if a not in self.arm_code:
                ref = self.rom_ref(w) or self.ram_ref(w)
                lines.append(f"_{a:08X}: .4byte {ref or f'0x{w:08X}'}")
                continue
            comment = ""
            if cs is not None:
                ins = next(cs.disasm(self.rom.bytes(a, 4), a), None)
                if ins is not None:
                    comment = f" @ {ins.mnemonic} {ins.op_str}".rstrip()
            lines.append(f"\t.inst 0x{w:08X}{comment}")
        return lines


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

HEADER = "@ Generated by scripts/generate_asm.py from baserom.gba -- do not edit or commit.\n"


def tidy_function(lines: list[str], name: str | None, drop_tail_padding: bool) -> list[str]:
    """Clean up one function's Luvdis output."""
    out = list(lines)
    # trailing alignment padding: the next function's `.align 2, 0` recreates it
    if drop_tail_padding and out and out[-1].strip() == ".byte 0x00, 0x00":
        out.pop()
    # padding before a literal pool or an ARM block -> .align, as agbcc/as write it
    for i, line in enumerate(out[:-1]):
        if line.strip() == ".byte 0x00, 0x00" and (
                re.match(r"^_[0-9A-F]{8}: \.4byte", out[i + 1]) or out[i + 1].strip() == ".arm"):
            out[i] = "\t.align 2, 0"
    if name is not None:
        out.append(f"\tthumb_func_end {name}")
    return out


def write(path: str, text: str):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


def gen_header(name, lo, hi):
    return (HEADER + "\t.section .text\n\t.global RomHeader\nRomHeader:\n"
            f"\t.incbin \"baserom.gba\", 0x{lo - ROM_BASE:X}, 0x{hi - lo:X}\n")


def gen_data(dis: Disassembly, name, lo, hi, owned=()):
    """The data as incbin chunks, cut at every label and pointer.

    *owned* are the [[rodata]] regions a C module's .rodata provides: they are left out, and
    the data after each goes into its own section (`.rodata.after_<start>`), which the linker
    script places after that module's .rodata."""
    out = [HEADER + "\t.section .rodata"]
    owned = sorted((r["start"], r["end"]) for r in owned if lo <= r["start"] < hi)
    inside = lambda a: any(r0 <= a < r1 for r0, r1 in owned)  # noqa: E731
    pointers = {a: v for a, v in dis.data_pointers.items() if lo <= a < hi and not inside(a)}
    for r0, r1 in owned:
        held = [dis.labels[a] for a in dis.labels if r0 <= a < r1]
        if held:
            sys.exit(f"[[rodata]] {r0:08X}-{r1:08X}: labels inside it ({', '.join(held)}) would have "
                     "to be defined by the C module")
    cuts = sorted({a for a in dis.labels if lo <= a <= hi and not inside(a)} | set(pointers) | {lo, hi}
                  | {r1 for _, r1 in owned})
    ends = {r1: r0 for r0, r1 in owned}
    for i, a in enumerate(cuts):
        if a in ends:
            out.append(f"\t.section .rodata.after_{ends[a]:08X}")
        if a in dis.labels:
            out += [f"\t.global {dis.labels[a]}", f"{dis.labels[a]}:"]
        if a == hi:
            break
        if a in pointers:
            ref = dis.rom_ref(pointers[a])
            if ref is None:
                sys.exit(f"config/data_pointers.txt: {a:08X} = {pointers[a]:08X} is neither data nor a function entry")
            if cuts[i + 1] < a + 4:
                sys.exit(f"config/data_pointers.txt: a label at {cuts[i + 1]:08X} falls inside the pointer at {a:08X}")
            out.append(f"\t.4byte {ref}")
            start = a + 4
        else:
            start = a
        stop = min([cuts[i + 1]] + [r0 for r0, _ in owned if start <= r0 < cuts[i + 1]])
        if start < stop:
            out.append(f"\t.incbin \"baserom.gba\", 0x{start - ROM_BASE:X}, 0x{stop - start:X}")
    return "\n".join(out) + "\n"


def gen_asm_module(dis: Disassembly, name, lo, hi):
    """A whole module as one .s file (ARM routines and/or hand-written Thumb)."""
    text = [HEADER.rstrip(), '\t.include "asm/macros.inc"', "\t.syntax unified", "\t.text", ""]
    entries = sorted(a for a in dis.funcs if lo <= a < hi)
    bounds = entries + [hi]
    if entries and entries[0] != lo:
        bounds = [lo] + bounds
    for i in range(len(bounds) - 1):
        a, b = bounds[i], bounds[i + 1]
        if dis.funcs.get(a) == "arm":
            text += [f"\tarm_func_start {dis.names[a]}", f"{dis.names[a]}:"]
            text += dis.arm_lines(a, b)
            text += [f"\tarm_func_end {dis.names[a]}", ""]
        else:
            name_a = dis.names[a] if dis.funcs.get(a) == "thumb" else None
            # the next chunk re-aligns only if it is a word-aligned function (thumb_func_start)
            realigns = b == hi or (b % 4 == 0 and dis.funcs.get(b) is not None)
            text += tidy_function(dis.thumb_lines(a, b), name_a, realigns) + [""]
    text.append("\t.align 2, 0")
    return "\n".join(text) + "\n"


LABEL_DEF_RE = re.compile(r"^(_[0-9A-F]{8}):", re.M)
DATA_RE = re.compile(r"^\s*(?:_[0-9A-F]{8}:\s*)?\.(?:byte|2byte|hword|short|space|skip|fill|zero|incbin|ascii|asciz|string)\b")
LABEL_REF_RE = re.compile(r"(?<![\w.])(_[0-9A-F]{8})\b")


def check_function(dis: Disassembly, a: int, b: int, fname: str, lines: list[str]) -> list[str]:
    """A C-module function must be self-contained: it is compiled on its own once decompiled."""
    problems = []
    fi = dis.finfo.get(a)
    if fi is not None:
        problems += [f"{fname}: {x}" for x in fi.issues if "falls through" in x or "branches into" in x]
        outside = [x for x in list(fi.insns) + list(fi.pools) if not (a <= x < b)]
        if outside:
            problems.append(f"{fname}: code or pool at {min(outside):08X}, outside its range {a:08X}-{b:08X}")
    defined = set(LABEL_DEF_RE.findall("\n".join(lines)))
    used = set()
    for line in lines:
        operands = LABEL_DEF_RE.sub("", line.split("@", 1)[0], count=1)  # drop `_X:` and comments
        used.update(LABEL_REF_RE.findall(operands))
    missing = sorted(used - defined)
    if missing:
        problems.append(f"{fname}: uses labels defined in another function's file: {', '.join(missing)}")
    data = [line.strip() for line in lines if DATA_RE.match(line)]
    if data:  # only pools (.4byte) and jump tables belong to a function; see discovery's data_in_code
        problems.append(f"{fname}: holds data, not code: {data[0]}")
    return problems


def gen_c_module(dis: Disassembly, name, lo, hi, problems: list[str]):
    """One .s per function; returns [(addr, function name)]."""
    entries = sorted(a for a in dis.funcs if lo <= a < hi)
    if entries and entries[0] != lo:
        sys.exit(f"module {name} starts at {lo:08X}, but its first function is at {entries[0]:08X}")
    funcs = []
    for i, a in enumerate(entries):
        b = entries[i + 1] if i + 1 < len(entries) else hi
        fname = dis.names[a]
        lines = dis.thumb_lines(a, b)
        lines = tidy_function(lines, fname, drop_tail_padding=b % 4 == 0)
        problems += check_function(dis, a, b, fname, lines)
        write(os.path.join(ROOT, "asm", "nonmatchings", name, f"{fname}.s"),
              HEADER + "\t.syntax unified\n" + "\n".join(lines) + "\n")
        funcs.append((a, fname))
    return funcs


def c_source_stub(name, funcs, cfg_mod):
    lines = [
        f"// {name}: provisional translation unit, {funcs[0][0]:08X}-.",
        "// Generated once by scripts/generate_asm.py --init-src, then maintained by hand:",
        "// replace an INCLUDE_ASM with matching C, one function at a time.",
        "",
        '#include "global.h"',
        "",
    ]
    lines += [f'INCLUDE_ASM("asm/nonmatchings/{name}", {fname});' for _, fname in funcs]
    return "\n".join(lines) + "\n"


def preprocess(path: str) -> str:
    """The C file as the compiler sees it (no comments, #if'd-out code or unexpanded macros),
    keeping only the lines that come from the file itself, not from its headers."""
    r = subprocess.run(CPP + [path], cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"{path}: cpp failed:\n{r.stderr}")
    out, current = [], None
    for line in r.stdout.splitlines():
        m = re.match(r'^# \d+ "([^"]*)"', line)
        if m:
            current = m.group(1)
            out.append("")
        else:
            out.append(line if current == path else "")
    return "\n".join(out)


# what may precede a function's name in its definition: types, `*`, __attribute__((...))
DECL_PREFIX = r"(?:[\w\s\*]|__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\))*?"
# K&R parameter declarations between `)` and `{` (`int a; char *b;`)
KR_PARAMS = r"(?:\s*\w[^;{}()]*;)*"


def definition_re(fname: str) -> re.Pattern:
    """A C definition of *fname* (not a prototype or a call) in preprocess() output."""
    return re.compile(rf"^(?=\w){DECL_PREFIX}\b{re.escape(fname)}\s*\([^;{{}}]*\){KR_PARAMS}\s*{DECL_PREFIX}\{{",
                      re.M)


def c_definitions(path: str, fnames) -> set[str]:
    """The functions among *fnames* that src file *path* defines in C."""
    src = preprocess(path)
    return {f for f in fnames if definition_re(f).search(src)}


def check_c_source(path, name, funcs):
    """Every function of the module must be INCLUDE_ASM'd or defined in C, in address order."""
    src = preprocess(path)
    problems = []
    known = {f for _, f in funcs}
    found = []  # (offset in file, function name)
    for m in INCLUDE_ASM_RE.finditer(src):
        if m.group(1) != name or m.group(2) not in known:
            problems.append(f"{path}: INCLUDE_ASM of unknown function {m.group(1)}/{m.group(2)}")
        else:
            found.append((m.start(), m.group(2)))
    for _, fname in funcs:
        # a definition: the name, its parameter list, then a body (not a prototype or a call)
        m = definition_re(fname).search(src)
        if m:
            found.append((m.start(), fname))
    names = [f for _, f in sorted(found)]
    for _, fname in funcs:
        n = names.count(fname)
        if n == 0:
            problems.append(f"{path}: {fname} is neither INCLUDE_ASM'd nor defined")
        elif n > 1:
            problems.append(f"{path}: {fname} appears {n} times (INCLUDE_ASM and C?)")
    order = [f for _, f in funcs if f in names]
    seen = [f for i, f in enumerate(names) if f not in names[:i]]
    for want, got in zip(order, seen):
        if want != got:
            problems.append(f"{path}: functions out of address order: expected {want}, found {got}")
            break
    return problems


def gen_ldscript(cfg, mods, shift=0, owned=()):
    lines = [
        "/* Generated by scripts/generate_asm.py from kururin.toml -- do not edit. */",
        "OUTPUT_ARCH(arm)",
        "ENTRY(RomHeader)",
        "",
        "/* Data symbols ([symbols] in kururin.toml) in RAM; those in ROM are labels in data/. */",
    ]
    for name, addr in sorted(cfg.get("symbols", {}).items(), key=lambda kv: kv[1]):
        if addr < ROM_BASE:
            lines.append(f"{name} = 0x{addr:08X};")
    lines += ["", "/* Constants the code loads from a literal pool ([linker_constants]). */"]
    for name, c in sorted(cfg.get("linker_constants", {}).items()):
        lines.append(f"{name} = 0x{c['value']:X};")
    lines += [
        "",
        "SECTIONS",
        "{",
        "    . = 0x08000000;",
        "    .text :",
        "    {",
    ]
    for m in cfg["modules"]:
        sec = ".rodata" if m["kind"] == "data" else ".text"
        for obj in module_objects(m):
            lines.append(f"        {obj}({sec});")
            if m["kind"] != "data":
                continue
            # [[rodata]] regions whose C provides them: the module's .rodata, then the rest
            for r in sorted(owned, key=lambda r: r["start"]):
                sym = f"__rodata_{r['start']:08X}"
                lines += [
                    f"        {sym} = .; /* [[rodata]] {r['start']:08X}-{r['end']:08X} */",
                    f"        src/{r['module']}.o(.rodata);",
                    f"        ASSERT(. - {sym} == 0x{r['end'] - r['start']:X}, "
                    f"\"src/{r['module']}.o .rodata is not the 0x{r['end'] - r['start']:X} bytes of "
                    f"[[rodata]] {r['start']:08X}\");",
                    f"        {obj}(.rodata.after_{r['start']:08X});",
                ]
        if shift and m["name"] == SHIFT_AFTER:
            lines.append(f"        . = . + 0x{shift:X}; /* --shift: moves everything after {SHIFT_AFTER} */")
    lines += [
        "    } =0",
        "",
        "    /DISCARD/ :",
        "    {",
        "        *(.comment);",
        "        *(.ARM.attributes);",
        "        *(.debug*) *(.stab*) *(.line);",
        "    }",
        "",
        "    /* Every other input section (a C data definition, .data/.bss before RAM sections",
        "       exist, a named section such as EWRAM data) lands here and fails the link. */",
        "    .stray :",
        "    {",
        "        *(*) *(COMMON)",
        "    }",
        "    ASSERT(SIZEOF(.stray) == 0, \"sections outside .text: see kururin.map (.stray)\")",
        "}",
    ]
    return "\n".join(lines) + "\n"


def owned_rodata(cfg, mods, problems: list[str]) -> list[dict]:
    """The [[rodata]] regions whose module's C provides them: every function of the region's
    `functions` is defined in C (their literal constants and the module's own data are its
    .rodata). Until then the region stays incbin'd and the module must emit no .rodata."""
    owned = []
    data = [(lo, hi) for _, kind, lo, hi in mods if kind == "data"]
    for r in cfg.get("rodata", []):
        name = r["module"]
        if not any(n == name and k == "c" for n, k, _, _ in mods):
            problems.append(f"[[rodata]] {r['start']:08X}: {name} is not a C module")
            continue
        if not any(lo <= r["start"] < r["end"] <= hi for lo, hi in data):
            problems.append(f"[[rodata]] {r['start']:08X}-{r['end']:08X} is not inside the data")
            continue
        path = os.path.join(ROOT, "src", f"{name}.c")
        if os.path.exists(path) and c_definitions(path, r["functions"]) == set(r["functions"]):
            owned.append(r)
    return owned


# --shift pads here: the start of the C code, so every function and all the data move.
SHIFT_AFTER = "crt0"


def gen_modules_mk(cfg, mods):
    """Make variables for the Makefile: objects in link order, per-module compilers."""
    lines = ["# Generated by scripts/generate_asm.py from kururin.toml -- do not edit."]
    objs = [f"$(BUILD_DIR)/{o}" for m in cfg["modules"] for o in module_objects(m)]
    lines.append("OBJS := " + " ".join(objs))
    for m in cfg["modules"]:
        if m["kind"] == "lib":
            stem = os.path.splitext(m["archive"])[0]
            lines.append(f"$(BUILD_DIR)/lib/{stem}/%.o: tools/agbcc/lib/{m['archive']}")
            # quoted: $(CURDIR) may hold a space
            lines.append('\t@mkdir -p $(@D) && cd $(@D) && $(AR) x "$(CURDIR)/$<" $(@F)')
    for m in cfg["modules"]:
        if m["kind"] != "c":
            continue
        cc = {"agbcc": "$(AGBCC)", "old_agbcc": "$(OLD_AGBCC)"}[m["cc"]]
        lines.append(f"$(BUILD_DIR)/src/{m['name']}.o: CC1 := {cc}")
        lines.append(f"$(BUILD_DIR)/src/{m['name']}.o: CC1FLAGS := {m['cflags']}")
    lines.append("C_MODULES := " + " ".join(n for n, k, _, _ in mods if k == "c"))
    return "\n".join(lines) + "\n"


def gen_objdiff(cfg):
    """objdiff project: each C module's build object against its all-asm target.

    `scratch` carries the module's compiler and flags: asmlift reads them (through
    {{cc}} and {{flags}} in decomp.yaml) to compile candidates the way the Makefile does.
    """
    import json

    units = [{
        "name": m["name"],
        "target_path": f"expected/src/{m['name']}.o",
        "base_path": f"build/src/{m['name']}.o",
        "scratch": {"platform": "gba", "compiler": m["cc"], "c_flags": m["cflags"]},
        "metadata": {"source_path": f"src/{m['name']}.c"},
    } for m in cfg["modules"] if m["kind"] == "c"]
    return json.dumps({
        "$schema": "https://raw.githubusercontent.com/encounter/objdiff/main/config.schema.json",
        "custom_make": "make",
        "build_target": True,
        "build_base": True,
        "watch_patterns": ["*.c", "*.h", "*.s", "*.inc", "*.toml"],
        "units": units,
    }, indent=2) + "\n"


def module_objects(m) -> list[str]:
    """Objects (relative to build/) a module links, in order."""
    name, kind = m["name"], m["kind"]
    if kind == "lib":
        stem = os.path.splitext(m["archive"])[0]
        return [f"lib/{stem}/{member}" for member in m["members"]]
    return [{"c": f"src/{name}.o", "asm": f"asm/{name}.o", "header": f"asm/{name}.o",
             "data": f"data/{name}.o"}[kind]]


def write_tree(args, cfg, funcs, farjumps, noreturn):
    """Everything main() writes once asm/nonmatchings is gone."""
    dis = Disassembly(cfg, funcs, farjumps, noreturn)
    mods = modules(cfg)
    problems = check_overrides(cfg, funcs, farjumps, noreturn)
    owned = owned_rodata(cfg, mods, problems)
    nfuncs = 0
    for name, kind, lo, hi in mods:
        if kind == "header":
            write(os.path.join(ROOT, "asm", f"{name}.s"), gen_header(name, lo, hi))
        elif kind == "data":
            write(os.path.join(ROOT, "data", f"{name}.s"), gen_data(dis, name, lo, hi, owned))
        elif kind == "asm":
            write(os.path.join(ROOT, "asm", f"{name}.s"), gen_asm_module(dis, name, lo, hi))
            nfuncs += sum(1 for a in funcs if lo <= a < hi)
        elif kind == "lib":
            nfuncs += sum(1 for a in funcs if lo <= a < hi)  # linked from agbcc's archives
        elif kind == "c":
            fl = gen_c_module(dis, name, lo, hi, problems)
            nfuncs += len(fl)
            path = os.path.join(ROOT, "src", f"{name}.c")
            if args.init_src or not os.path.exists(path):
                write(path, c_source_stub(name, fl, None))
            problems += check_c_source(path, name, fl)
        else:
            sys.exit(f"unknown module kind {kind}")
    write(os.path.join(ROOT, "ldscript.txt"), gen_ldscript(cfg, mods, args.shift, owned))
    write(os.path.join(ROOT, "modules.mk"), gen_modules_mk(cfg, mods))
    write(os.path.join(ROOT, "objdiff.json"), gen_objdiff(cfg))
    if nfuncs != len(funcs):
        problems.append(f"{len(funcs) - nfuncs} functions are outside every module")
    # ld lets a linker-script symbol silently replace an object's global of the same name
    asm_globals = set()
    for path in glob.glob(os.path.join(ROOT, "asm", "*.s")):
        asm_globals |= set(re.findall(r"^\s*\.glob(?:a)?l\s+(\w+)", open(path).read(), re.M))
    clash = sorted(asm_globals & set(cfg.get("symbols", {})))
    if clash:
        problems.append(f"[symbols] names that asm modules already define: {', '.join(clash)}")
    if problems:
        sys.exit("\n".join(problems))  # main() removes modules.mk and ldscript.txt
    shifted = f", shifted by 0x{args.shift:X} after {SHIFT_AFTER}" if args.shift else ""
    print(f"generated {nfuncs} functions in {len(mods)} modules, {len(dis.data_pointers)} data pointers{shifted}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--init-src", action="store_true",
                    help="(re)write src/<module>.c stubs (destroys decompiled C!)")
    ap.add_argument("--shift", type=lambda x: int(x, 0), default=0,
                    help=f"pad the linker script with N bytes after {SHIFT_AFTER} (make shift-test)")
    args = ap.parse_args()

    cfg, funcs, farjumps, noreturn = load_config()
    data = open(BASEROM, "rb").read()
    sha1 = hashlib.sha1(data).hexdigest()
    if sha1 != cfg["rom"]["sha1"]:
        sys.exit(f"baserom.gba SHA1 {sha1} != expected {cfg['rom']['sha1']}")
    if len(data) != cfg["rom"]["size"]:
        sys.exit("baserom.gba has the wrong size")

    for d in ("asm/nonmatchings",):
        shutil.rmtree(os.path.join(ROOT, d), ignore_errors=True)
    try:
        write_tree(args, cfg, funcs, farjumps, noreturn)
    except BaseException:
        # Whatever stopped it (a problem it found, a bug, Ctrl-C), no build may use the tree it
        # began to write: without these the Makefile stops on a missing modules.mk.
        for f in ("modules.mk", "ldscript.txt"):
            with contextlib.suppress(FileNotFoundError):
                os.remove(os.path.join(ROOT, f))
        raise


if __name__ == "__main__":
    main()
