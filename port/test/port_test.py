#!/usr/bin/env python3
"""The PC port test (docs/port.md, "Headless mode and the port test").

For each input (default: boot_attract and training1_moves; --all: every input of
tests/gameplay/inputs), from the repository root:
  1. turns tests/gameplay/inputs/<name>.txt (or .bk2) into kurutest's .inp, with
     tests/gameplay/run.py's own converter;
  2. runs the original (port/test/build/refdump: kurutest's mGBA setup) and dumps the
     tests/gameplay/watch.txt values after every frame;
  3. finds the original's lag frames: frame n is one when gMain.frameCount has not changed by
     frame n + 1's sample (its frame's work did not end before the next VBlank), whether
     the VBlank in the middle of the work raised its interrupt (refdump --timing: DISPSTAT
     bit 3; the first VBlanks after power-on come before AgbMain enables it), whether m4a's
     VSync ignored it (m4aSoundVSyncOff during a level's loading) and whether it came while
     the mixer ran (after SoundMain chose its buffer segment);
  4. runs port/kururin_pc --headless on the same keys with those lag frames;
  5. compares the two dumps frame by frame (the port skips the lag frames' samples: the
     original was half-way through its code there) and reports the first difference, and for
     every watch entry the first frame it differs.

--survey runs port/test/build/survey on the inputs instead: which display features the original
uses (docs/port.md, "What the game draws with"); --sound-survey runs port/test/build/sound_survey:
what its sound uses (docs/port.md, "What the game's sound uses").

--port CMD runs that instead of port/kururin_pc (split like a shell command line): the Windows
build under Wine is --port "wine port/kururin_pc.exe" (make -C port test-wine).

Exits 1 if a compared entry differs within --frames frames (default: the whole input).
--ignore NAME (repeatable) leaves an entry out. --extra-watch FILE compares that list's entries
too (after tests/gameplay/watch.txt's, which the inputs' checks use): port/test/audio_watch.txt
is the audio test (make -C port test-audio: m4a's RAM, the PCM buffer included).
"""
import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests" / "gameplay"))
import run as gameplay  # noqa: E402  (tests/gameplay/run.py: the .inp converters)

OUT = ROOT / "port" / "test" / "build"
DEFAULT_INPUTS = ["boot_attract", "training1_moves"]
DEFAULT_IGNORE = []


def load_watch(path):
    out = []
    for line in open(path):
        p = line.split("#", 1)[0].split()
        if len(p) in (3, 4):  # a fourth word: "raw" (port/test/audio_watch.txt)
            out.append((int(p[0], 16), int(p[1]), p[2]))
    return out


def read_dump(path):
    """[(frame, keys, [hex per entry] or None for a lag frame)]"""
    rows = []
    for line in open(path):
        p = line.split()
        rows.append((int(p[0]), p[1], None if p[2:] == ["lag"] else p[2:]))
    return rows


def lag_frames(ref, counter):
    """Frames whose sample is half-way through the original's frame of work."""
    counts = [int.from_bytes(bytes.fromhex(r[2][counter]), "little") for r in ref]
    return [n for n in range(len(counts) - 1) if counts[n + 1] == counts[n]]


def describe(watch, i, a, b):
    """The entry's name; for a struct, the first differing byte and the words around it."""
    addr, size, name = watch[i]
    if size <= 4:
        return f"{name} (0x{addr:08X}): original 0x{int.from_bytes(bytes.fromhex(a), 'little'):X}, " \
               f"port 0x{int.from_bytes(bytes.fromhex(b), 'little'):X}"
    ba, bb = bytes.fromhex(a), bytes.fromhex(b)
    off = next(k for k in range(size) if ba[k] != bb[k])
    w = off & ~3
    return (f"{name}+0x{off:X} (0x{addr + off:08X}): original {ba[w:w + 4].hex().upper()}, "
            f"port {bb[w:w + 4].hex().upper()} (the word at +0x{w:X}, bytes in memory order)")


def refdump_watch(path):
    """The watch list for refdump: kurutest's reader takes no fourth word but "finished", so
    "raw" (port/test/audio_watch.txt: bytes the port must not translate) is dropped."""
    lines = open(path).read().split("\n")
    out = [" ".join(l.split()[:3]) if len(l.split("#", 1)[0].split()) == 4 and l.split()[3] == "raw" else l
           for l in lines]
    if out == lines:
        return path
    ref = OUT / f"{Path(path).stem}_refdump.txt"
    ref.write_text("\n".join(out))
    return ref


def port_command(args):
    """The port to run: --port, split like a shell command line, or port/kururin_pc."""
    return shlex.split(args.port) if args.port else [str(ROOT / "port" / "kururin_pc")]


def write_inp(name):
    """tests/gameplay/inputs/<name>.txt or .bk2 as kurutest's .inp (run.py's converters)."""
    src = next((p for p in (ROOT / "tests/gameplay/inputs").iterdir() if p.stem == name), None)
    if not src:
        sys.exit(f"no input {name}")
    lines = gameplay.convert_bk2(src) if src.suffix == ".bk2" else gameplay.convert_script(src)
    inp = OUT / f"{name}.inp"
    inp.write_text("\n".join(lines) + "\n")
    return inp


def run_input(name, args, watch):
    inp = write_inp(name)
    ref, timing, lag, pc = (OUT / f"{name}.{ext}" for ext in ("ref", "timing", "lag", "pc"))
    subprocess.run([str(OUT / "refdump"), "--watch", str(refdump_watch(args.watch)), "--inputs", str(inp), "--dump", str(ref),
                    "--timing", str(timing), str(ROOT / "baserom.gba")], check=True)
    timing_rows = [l.split() for l in timing.read_text().splitlines()]
    irq = [r[1] == "1" for r in timing_rows]
    sound_off = [len(r) > 2 and r[2] == "1" for r in timing_rows]
    in_mixer = [len(r) > 3 and r[3] == "1" for r in timing_rows]
    refrows = read_dump(ref)
    counter = next(i for i, w in enumerate(watch) if w[2] == "gMain.frameCounter")
    lags = lag_frames(refrows, counter)
    lag.write_text("".join(f"{n}{'' if irq[n] else ' noirq'}{' soundoff' if sound_off[n] else ''}"
                           f"{' mixer' if in_mixer[n] else ''}\n" for n in lags))
    frames = min(args.frames or len(refrows), len(refrows))
    res = subprocess.run(port_command(args) + ["--headless", "--inputs", str(inp), "--frames",
                          str(frames), "--lag-frames", str(lag), "--watch", str(args.watch), "--dump", str(pc),
                          "--rom", str(ROOT / "baserom.gba")])
    pcrows = read_dump(pc)
    # a run that crashed can end in a half-written line
    while pcrows and pcrows[-1][2] is not None and (len(pcrows[-1][2]) != len(watch) or
                                                     len(pcrows[-1][2][-1]) != 2 * watch[-1][1]):
        pcrows.pop()
    ignore = set(args.ignore)
    compared, first, per_entry = 0, None, {}
    for n in range(min(frames, len(pcrows))):
        fr, keys, a = refrows[n]
        _, _, b = pcrows[n]
        if b is None:
            continue
        compared += 1
        for i, (_, _, wname) in enumerate(watch):
            if wname in ignore or a[i] == b[i]:
                continue
            if i not in per_entry:
                per_entry[i] = (n, describe(watch, i, a[i], b[i]))
            if first is None:
                first = (n, keys, i)
    ok = first is None and res.returncode == 0 and len(pcrows) >= frames
    print(f"{'ok  ' if ok else 'FAIL'} {name}: {frames} frames, {len(lags)} lag frames of the original skipped, "
          f"{compared} compared" + ("" if ok else ""))
    if res.returncode != 0 or len(pcrows) < frames:
        print(f"     the port stopped after {len(pcrows)} frames (exit status {res.returncode})")
    if first:
        n, keys, i = first
        print(f"     first difference at frame {n} (keys {keys}), "
              f"identical for the {sum(1 for r in pcrows[:n] if r[2] is not None)} compared frames before it:")
        for i2, (n2, text) in sorted(per_entry.items(), key=lambda kv: kv[1][0]):
            print(f"       frame {n2:6}: {text}")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="*", default=DEFAULT_INPUTS)
    ap.add_argument("--all", action="store_true", help="every input of tests/gameplay/inputs")
    ap.add_argument("--frames", type=int, default=0, help="compare only the first N frames")
    ap.add_argument("--ignore", action="append", default=None, help="a watch entry to leave out")
    ap.add_argument("--watch", type=Path, default=ROOT / "tests/gameplay/watch.txt")
    ap.add_argument("--port", help='the port to run (default port/kururin_pc), e.g. "wine port/kururin_pc.exe"')
    ap.add_argument("--extra-watch", type=Path, help="more entries to compare (port/test/audio_watch.txt)")
    ap.add_argument("--survey", action="store_true",
                    help="instead of the test: the display survey of the original on the inputs (port/test/survey.c)")
    ap.add_argument("--sound-survey", action="store_true",
                    help="instead of the test: the sound survey of the original on the inputs (port/test/sound_survey.c)")
    args = ap.parse_args()
    if args.ignore is None:
        args.ignore = DEFAULT_IGNORE
    os.chdir(ROOT)
    OUT.mkdir(parents=True, exist_ok=True)
    if args.extra_watch:
        combined = OUT / f"{args.extra_watch.stem}_combined.txt"
        combined.write_text(args.watch.read_text() + "\n" + args.extra_watch.read_text())
        args.watch = combined
    watch = load_watch(args.watch)
    if args.all:
        args.inputs = sorted(p.stem for p in (ROOT / "tests/gameplay/inputs").iterdir()
                             if p.suffix in (".txt", ".bk2") and not p.name.startswith("_"))
    if args.survey or args.sound_survey:
        inps = [str(write_inp(n)) for n in args.inputs]
        tool = OUT / ("survey" if args.survey else "sound_survey")
        return subprocess.run([str(tool), str(args.watch), str(ROOT / "baserom.gba")] + inps).returncode
    results = [run_input(n, args, watch) for n in args.inputs]
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
