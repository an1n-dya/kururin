#!/usr/bin/env python3
"""`make test`: gameplay equivalence of the rebuilt ROM with baserom.gba.

Turns every input in tests/gameplay/inputs/ into kurutest's .inp format and runs kurutest
(tests/gameplay/kurutest.c) on baserom.gba and kururin.gba side by side: the watch list
(tests/gameplay/watch.txt) is compared after every frame, the framebuffer every --fb-every
frames. Exits 1 if any input shows a difference, with the first frame, address and both values.
The original runs traced (kurutest --coverage); after a full passing run (every input, not
--quick) scripts/coverage.py turns that into docs/coverage.md.

Inputs:
  *.bk2  BizHawk movies (TASVideos publications for this ROM). Only the "Input Log.txt" is
         read: one line per frame, |tilt x,tilt y,tilt z,light,UDLRSsBALRP|; P (Power) resets
         the console before that frame.
  *.txt  scripted inputs, one step per line:
           <keys> <frames>       hold <keys> for <frames> frames; keys are letters joined by
                                 '+' (A B Select Start Right Left Up Down R L) or "-" for none
           tap <keys> [<gap>]    hold <keys> for one frame, then nothing for <gap> frames
                                 (default 30)
           expect <watch name> <op> <value>
                                 check the ORIGINAL ROM at this point (op: == != < <= > >=),
                                 so a script fails loudly when it stops doing what it says
           include <file.txt>    the steps of another script (e.g. a shared "boot to title")
           save <file.sav>       only as a script's first step: both consoles boot with this
                                 SRAM image (a path from the repository root) instead of a
                                 blank SRAM; without it they boot with none. When <file>.toml
                                 is next to it (tests/gameplay/saves/), the image is built
                                 from it and baserom.gba by tools/make_save.py each time the
                                 script is converted: the images are not committed
           # comment
         A script whose name starts with "_" is only included, never run on its own.
"""

import argparse
import concurrent.futures
import functools
import os
import pathlib
import re
import signal
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
HERE = ROOT / "tests" / "gameplay"
sys.path.insert(0, str(ROOT / "tools"))
import make_save  # noqa: E402  (tools/make_save.py: the save images, from their .toml and the ROM)

KEY_BITS = {"A": 0, "B": 1, "SELECT": 2, "START": 3, "RIGHT": 4, "LEFT": 5, "UP": 6, "DOWN": 7, "R": 8, "L": 9}
# BizHawk GBA input log button order (after the four analog fields) -> GBA KEYINPUT bit
BK2_ORDER = ["UP", "DOWN", "LEFT", "RIGHT", "START", "SELECT", "B", "A", "L", "R"]


def parse_keys(text, where):
    if text == "-":
        return 0
    mask = 0
    for k in text.upper().split("+"):
        if k not in KEY_BITS:
            sys.exit(f"{where}: unknown key {k!r}")
        mask |= 1 << KEY_BITS[k]
    return mask


def compact(frames):
    """[(keys)] per frame -> run-length .inp lines."""
    out, i = [], 0
    while i < len(frames):
        if frames[i] == "reset":
            out.append("reset")
            i += 1
            continue
        j = i
        while j < len(frames) and frames[j] == frames[i]:
            j += 1
        out.append(f"{frames[i]:03X}*{j - i}" if j - i > 1 else f"{frames[i]:03X}")
        i = j
    return out


def convert_bk2(path):
    with zipfile.ZipFile(path) as z:
        log = z.read("Input Log.txt").decode()
    frames = []
    for n, line in enumerate(log.splitlines(), 1):
        if not line.startswith("|"):
            continue
        buttons = line.strip("|").split(",")[-1]
        if len(buttons) != 11:
            sys.exit(f"{path}: Input Log line {n}: unexpected format {line!r}")
        if buttons[10] != ".":
            frames.append("reset")
        frames.append(sum(1 << KEY_BITS[k] for k, c in zip(BK2_ORDER, buttons) if c != "."))
    return [f"# {path.name}: {sum(f != 'reset' for f in frames)} frames"] + compact(frames)


@functools.cache
def save_rom():
    """baserom.gba for tools/make_save.py: read once, its SHA1 checked."""
    return make_save.Rom()


def convert_script(path, seen=()):
    if path in seen:
        sys.exit(f"{path}: include loop")
    out = [f"# {path.name}"]
    steps = 0
    for n, raw in enumerate(path.read_text().splitlines(), 1):
        where = f"{path}:{n}"
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        words = line.split()
        steps += 1
        if words[0] == "save" and len(words) == 2:
            save = ROOT / words[1]
            if steps != 1 or seen:
                sys.exit(f"{where}: 'save' must be the first step of a script that is run, not included")
            desc = save.with_suffix(".toml")
            if desc.is_file():
                make_save.write_image(desc, save_rom())  # what the description and the ROM give, now
            elif not save.is_file():
                sys.exit(f"{where}: no save file {words[1]} (and no {desc.name} to build it from)")
            out.append(f"save {save}")
        elif words[0] == "include" and len(words) == 2:
            out += convert_script(path.parent / words[1], seen + (path,))
        elif words[0] == "expect" and len(words) == 4:
            out.append(f"! {words[1]} {words[2]} {words[3]}")
        elif words[0] == "tap" and len(words) in (2, 3):
            out.append(f"{parse_keys(words[1], where):03X}")
            gap = int(words[2]) if len(words) == 3 else 30
            if gap:
                out.append(f"000*{gap}")
        elif len(words) == 2 and re.fullmatch(r"\d+", words[1]):
            out.append(f"{parse_keys(words[0], where):03X}*{int(words[1])}")
        else:
            sys.exit(f"{where}: cannot parse {raw!r}")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--kurutest", default=str(ROOT / "build/test/kurutest"))
    ap.add_argument("--baserom", default=str(ROOT / "baserom.gba"))
    ap.add_argument("--rom", default=str(ROOT / "kururin.gba"))
    ap.add_argument("--fb-every", type=int, default=4, help="compare the framebuffer every N frames (0: never)")
    ap.add_argument("--out", default=str(ROOT / "build/test"), help="where .inp files and mismatch screenshots go")
    ap.add_argument("--quick", action="store_true", help="skip the .bk2 movies (scripted inputs only)")
    ap.add_argument("--no-coverage", action="store_true",
                    help="do not record coverage (by default each input writes <out>/<input>.cov, and a full run "
                         "writes docs/coverage.md through scripts/coverage.py)")
    ap.add_argument("--rom-map", help="kurutest --rom-map: where the original's ROM addresses are in the rebuilt "
                                      "ROM (scripts/rom_map.py)")
    ap.add_argument("--rom-loads", action="store_true",
                    help="also write <out>/<input>.loads: the ROM words the original loads with ldr/ldm "
                         "(evidence for scripts/find_pointers.py)")
    ap.add_argument("-j", "--jobs", type=int, default=max(1, (os.cpu_count() or 2) // 2),
                    help="inputs run at once (each kurutest uses two threads)")
    ap.add_argument("only", nargs="*", help="run only these inputs (file names or stems)")
    args = ap.parse_args()

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if not os.access(args.kurutest, os.X_OK):
        sys.exit(f"{args.kurutest} is missing: make build/test/kurutest (it needs the tools/mgba ./setup.sh builds)")
    inputs = sorted(p for p in (HERE / "inputs").iterdir()
                    if p.suffix in (".bk2", ".txt") and not p.name.startswith("_"))
    unknown = [n for n in args.only if not any(n in (p.name, p.stem) for p in inputs)]
    if unknown:
        sys.exit(f"no such input in tests/gameplay/inputs: {', '.join(unknown)}")
    if args.quick:
        inputs = [p for p in inputs if p.suffix != ".bk2"]
    if args.only:
        inputs = [p for p in inputs if p.name in args.only or p.stem in args.only]
        if not inputs:
            sys.exit("--quick leaves out the .bk2 movies: nothing left to run")

    def run_one(p):
        lines = convert_bk2(p) if p.suffix == ".bk2" else convert_script(p)
        inp = out / (p.stem + ".inp")
        inp.write_text("\n".join(lines) + "\n")
        trace = [] if args.no_coverage else ["--coverage", str(out / (p.stem + ".cov"))]
        if args.rom_map:
            trace += ["--rom-map", args.rom_map]
        if args.rom_loads:
            trace += ["--rom-loads", str(out / (p.stem + ".loads"))]
        return subprocess.run([args.kurutest, "--watch", str(HERE / "watch.txt"), "--inputs", str(inp),
                               "--name", p.stem, "--fb-every", str(args.fb_every), "--dump-dir", str(out),
                               *trace, args.baserom, args.rom], capture_output=True, text=True)

    # Longest first (the movies), so the short scripts fill in around them.
    inputs.sort(key=lambda p: -p.stat().st_size if p.suffix == ".bk2" else 0)
    failed = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for p, r in zip(inputs, pool.map(run_one, inputs)):
            print(r.stdout, end="", flush=True)
            # kurutest's own errors (exit status 2), with the input they belong to
            for line in r.stderr.splitlines():
                print(f"{p.stem}: {line}", file=sys.stderr, flush=True)
            if r.returncode < 0:
                try:
                    sig = signal.Signals(-r.returncode).name
                except ValueError:
                    sig = f"signal {-r.returncode}"
                print(f"FAIL {p.stem}: kurutest was killed by {sig}", flush=True)
            if r.returncode != 0:
                failed.append(p.name)
    if failed:
        print(f"make test: FAILED: {', '.join(failed)}")
        return 1
    print(f"make test: all {len(inputs)} inputs identical")
    if not args.no_coverage and not args.quick and not args.only:
        # every input ran, so the .cov files are this run's: docs/coverage.md
        r = subprocess.run([sys.executable, str(ROOT / "scripts" / "coverage.py"), "--cov-dir", str(out)])
        if r.returncode:
            return r.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
