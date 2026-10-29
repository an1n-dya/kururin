#!/usr/bin/env python3
"""Compare struct layouts between agbcc (the GBA build) and the PC port's compiler.

The port shares RAM and ROM data with the original byte for byte, so every struct the game's C
uses must have the same size, alignment and field offsets on both compilers. agbcc pads every
struct to a multiple of 4 bytes and aligns it to 4 (thumb.h: STRUCTURE_SIZE_BOUNDARY 32); gcc on
x86 does not. A struct whose size or field offsets differ would need fixing for the port; one
whose alignment alone differs is reported only with -v (it changes no layout here).

For each C module this preprocesses the source both ways, finds every struct definition, and
appends sizeof/__alignof__/offsetof constants for each struct and each named field; agbcc and
`gcc -m32` compile them to data, and the two lists must be equal. Exits 1 on any difference.

usage: python3 port/tools/check_layouts.py [-v] [--cc 'gcc -m32']
       (the Windows build: --cc 'i686-w64-mingw32-gcc -mno-ms-bitfields')
"""
import argparse, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def preprocess(cmd, src):
    return subprocess.run(cmd + [src], cwd=ROOT, check=True, capture_output=True, text=True).stdout

def strip_lines(text):
    return "\n".join(l for l in text.split("\n") if not l.startswith("#"))

def find_structs(text):
    """{name: [field names]} for every 'struct Name {...}' definition (named fields only)."""
    out = {}
    for m in re.finditer(r"\bstruct\s+([A-Za-z_]\w*)\s*\{", text):
        name, i, depth = m.group(1), m.end(), 1
        start = i
        while depth:
            c = text[i]
            depth += c == "{"
            depth -= c == "}"
            i += 1
        body = text[start:i - 1]
        fields, d, cur = [], 0, ""
        for c in body:
            if c in "{(":
                d += 1
            elif c in "})":
                d -= 1
            if c == ";" and d == 0:
                fields.append(cur)
                cur = ""
            else:
                cur += c
        names = []
        for f in fields:
            f = f.strip()
            if not f or "{" in f:   # nested anonymous struct/union: skip
                continue
            for decl in f.split(","):
                decl = re.sub(r"\[.*?\]", "", decl.split(":")[0])
                if "(" in decl:      # function pointer: (*name)(...)
                    mm = re.search(r"\(\s*\*\s*(\w+)\s*\)", decl)
                    if mm:
                        names.append((mm.group(1), ":" in f))
                    continue
                mm = re.search(r"(\w+)\s*$", decl)
                if mm:
                    names.append((mm.group(1), ":" in f))
        out.setdefault(name, [n for n, bit in names if not bit])
    return out

def probe(text, structs):
    lines = [text, "\n"]
    for s, fields in sorted(structs.items()):
        lines.append(f"int __lay_{s}[] = {{ sizeof(struct {s}), __alignof__(struct {s})")
        for f in fields:
            lines.append(f", (int)&((struct {s} *)0)->{f}")
        lines.append(" };\n")
    return "".join(lines)

def values(asm):
    """{symbol: [ints]} from a compiled .s (agbcc: .word; gcc: .long)."""
    out, cur = {}, None
    for l in asm.split("\n"):
        m = re.match(r"^_?(__lay_\w+):", l.strip())  # mingw: a leading underscore
        if m:
            cur = out.setdefault(m.group(1), [])
            continue
        m = re.match(r"^\s*\.(word|long|4byte)\s+(-?\w+)", l)
        if m and cur is not None:
            cur.append(int(m.group(2), 0))
        elif l.strip() and not l.strip().startswith(".") and cur is not None and ":" in l:
            cur = None
    return out

def module_flags():
    """{module: (cc, cflags)} from modules.mk."""
    flags = {}
    for l in open(os.path.join(ROOT, "modules.mk")):
        m = re.match(r"\$\(BUILD_DIR\)/src/(\w+)\.o: CC1 := (.*)", l.strip())
        if m:
            flags.setdefault(m.group(1), [None, None])[0] = m.group(2).strip()
        m = re.match(r"\$\(BUILD_DIR\)/src/(\w+)\.o: CC1FLAGS := (.*)", l.strip())
        if m:
            flags.setdefault(m.group(1), [None, None])[1] = m.group(2).strip()
    return flags

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-v", action="store_true")
    ap.add_argument("--cc", default="gcc -m32")
    args = ap.parse_args()
    flags = module_flags()
    bad = 0
    checked = set()
    with tempfile.TemporaryDirectory() as tmp:
        for mod in sorted(flags):
            src = f"src/{mod}.c"
            cc, cflags = flags[mod]
            agb = strip_lines(preprocess(["arm-none-eabi-cpp", "-std=gnu17", "-nostdinc", "-I", "tools/agbcc/include",
                                          "-iquote", "include"], src))
            pc = strip_lines(preprocess(args.cc.split() + ["-E", "-std=gnu17", "-DPLATFORM_PC", "-iquote", "include"], src))
            # INCLUDE_ASM's top-level asm is not wanted in a data-only probe
            agb = re.sub(r"__asm__\s*\((?:[^()]|\([^()]*\))*\)\s*;", "", agb)
            structs = find_structs(pc)
            structs = {k: v for k, v in structs.items() if k in find_structs(agb)}
            pa, pp = os.path.join(tmp, mod + "_agb.i"), os.path.join(tmp, mod + "_pc.c")
            open(pa, "w").write(probe(agb, structs))
            open(pp, "w").write(probe(pc, structs))
            cc = {"$(AGBCC)": "tools/agbcc/bin/agbcc", "$(OLD_AGBCC)": "tools/agbcc/bin/old_agbcc"}.get(cc, cc)
            subprocess.run([os.path.join(ROOT, cc)] + cflags.split() + ["-o", pa + ".s", pa], check=True,
                           capture_output=True)
            subprocess.run(args.cc.split() + ["-std=gnu17", "-w", "-S", "-o", pp + ".s", pp], check=True)  # as port/Makefile GAME_STD
            va, vp = values(open(pa + ".s").read()), values(open(pp + ".s").read())
            for s in sorted(structs):
                key = f"__lay_{s}"
                if s in checked:
                    continue
                checked.add(s)
                a, p = va.get(key), vp.get(key)
                if a and p and a[0] == p[0] and a[2:] == p[2:] and a[1] != p[1]:
                    # Alignment alone: harmless while every containing struct still has the
                    # same offsets (each is checked too) and globals are absolute symbols.
                    if args.v:
                        print(f"note struct {s}: alignment {a[1]} (agbcc) vs {p[1]} (pc), same layout")
                    continue
                if a != p:
                    bad += 1
                    fields = ["sizeof", "alignof"] + structs[s]
                    diff = [f"{f}: {x} vs {y}" for f, x, y in zip(fields, a or [], p or []) if x != y]
                    print(f"struct {s} ({mod}.c): agbcc != pc: " + "; ".join(diff))
                elif args.v:
                    print(f"ok struct {s}: size {a[0]}, align {a[1]}, {len(a) - 2} fields")
    print(f"{len(checked)} structs checked, {bad} differ")
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
