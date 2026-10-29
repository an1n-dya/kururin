#!/usr/bin/env python3
"""The PC port's pixel test (docs/port.md, "The pixel test"): `make -C port test-video`.

For each input (default: boot_attract, training1_moves, see_map, save_makeup; --all: every
input of tests/gameplay/inputs), from the repository root:
  1. runs the original in mGBA (port/test/build/refdump, kurutest's setup) and writes a hash of
     its screen every --fb-every frames (default 1: every frame), with the watched RAM and the
     VBlank timing the port test uses to find the original's lag frames;
  2. runs port/kururin_pc --headless on the same keys with those lag frames, writing the same
     hashes of its own screen (port/src/video.c);
  3. compares the hashes of every frame both have (the port has none for a lag frame of the
     original: the original was half-way through its work there, see port_test.py);
  4. for each differing frame (at most --shots), runs both again to write the two screens
     (port/test/build/video/<input>/{ref_baserom,port}_frameNNNNNN.ppm) and the video state
     they were drawn from ({ref,port}_stateNNNNNN.bin), and decides why it differs.

The port draws a whole frame at once, from the hardware as the frame's work left it; the
original's screen is drawn line by line while the game works. A frame where the original
changed VRAM, palette RAM, OAM or a video register while lines were drawn, or changed the data
its HBlank DMA reads (refdump --mid-frame), can differ on the lines drawn before the change:
such a difference is expected and reported (-v lists them). Any other difference fails the
test, with the first differing pixel, the number of pixels, and which of the video registers,
palette RAM, VRAM and OAM differ (the same state and different pictures: the renderer).

A pixel is its 24-bit colour as mGBA makes it (8 bits a channel, 0x00BBGGRR).

The first frame after power-on and after a reset is not compared: mGBA's HLE BIOS starts the
cartridge at line 126 (the original's first screen has lines 0-125 from before the reset, or
black), and the screen is blank (forced blank) then anyway.

Exits 1 if a compared frame differs other than as expected, or the port stopped early.
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import port_test  # noqa: E402  (the .inp converter, the dumps, the lag frames)

ROOT = port_test.ROOT
OUT = port_test.OUT
DEFAULT_INPUTS = ["boot_attract", "training1_moves", "see_map", "save_makeup"]


def read_hashes(path):
    out = {}
    for line in open(path):
        p = line.split()
        if len(p) == 2:
            out[int(p[0])] = p[1]
    return out


def read_ppm(path):
    data = path.read_bytes()
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3][:w * h * 3]


def first_frames(inp):
    """Frame 0 and the first frame after each reset (the .inp's 'reset' lines)."""
    frames, frame = {0}, 0
    for line in inp.read_text().splitlines():
        line = line.strip()
        if not line or line[0] in "#!" or line.startswith("save"):
            continue
        if line == "reset":
            frames.add(frame)
            continue
        keys, _, count = line.partition("*")
        frame += int(count) if count else 1
    return frames


def diff_shots(ref, pc):
    """(number of differing pixels, first differing index, its two colours, lines) of two PPMs."""
    w, h, a = read_ppm(ref)
    _, _, b = read_ppm(pc)
    diff = [i for i in range(w * h) if a[3 * i:3 * i + 3] != b[3 * i:3 * i + 3]]
    if not diff:
        return None
    i = diff[0]
    rgb = lambda d: "#%02X%02X%02X" % tuple(d[3 * i:3 * i + 3])
    lines = sorted({k // w for k in diff})
    return len(diff), (i % w, i // w), rgb(a), rgb(b), lines


def read_mid(path):
    """refdump --mid-frame: {frame: [(what, first line, last line)]}"""
    out = {}
    for line in open(path):
        f, what, a, b = line.split()
        out.setdefault(int(f), []).append((what, int(a), int(b)))
    return out


MID_TEXT = {"vram": "wrote VRAM", "palette": "wrote palette RAM", "oam": "wrote OAM",
            "register": "wrote video registers", "hblank-dma-source": "changed the window table the HBlank DMA reads"}


def explain(lines, mid):
    """Why the original's screen differs on *lines*, if it changed the video state while drawing them:
    a write on line L shows on the lines drawn before it (0..L), a changed HBlank DMA source on its lines."""
    if not mid:
        return None
    reach = set()
    for what, a, b in mid:
        reach |= set(range(a, b + 1)) if what == "hblank-dma-source" else set(range(0, b + 1))
    if not set(lines) <= reach:
        return None
    return "; ".join(f"the original {MID_TEXT[w]} while drawing lines {a}-{b}" for w, a, b in mid)


# The state files (refdump / kururin_pc --shot): I/O, palette RAM, VRAM, OAM
STATE = [("I/O", 0x04000000, 0x400), ("palette", 0x05000000, 0x400), ("VRAM", 0x06000000, 0x18000),
         ("OAM", 0x07000000, 0x400)]
# I/O the comparison leaves out: DISPSTAT and VCOUNT (status bits), KEYINPUT, and everything
# from the sound registers on (the port's sound is stubbed); the video registers count
IO_COMPARED = range(0x000, 0x060)


# How mGBA keeps the video registers it is written (the port keeps what the game wrote)
IO_MASKS = {0x000: 0xFFF7, 0x008: 0xDFFF, 0x00A: 0xDFFF, 0x048: 0x3F3F, 0x04A: 0x3F3F, 0x050: 0x3FFF,
            0x052: 0x1F1F, **{o: 0x01FF for o in range(0x010, 0x020, 2)}}


def masked_io(state):
    io = bytearray(state[:0x400])
    for ofs, mask in IO_MASKS.items():
        v = int.from_bytes(io[ofs:ofs + 2], "little") & mask
        io[ofs:ofs + 2] = v.to_bytes(2, "little")
    bldy = min(io[0x54] & 0x1F, 0x10)
    io[0x54:0x56] = bytes([bldy, 0])
    return bytes(io) + state[0x400:]


def compare_states(ref, pc):
    """Which video memory differs: what the two screens were drawn from."""
    a, b = ref.read_bytes(), masked_io(pc.read_bytes())
    out, ofs = [], 0
    for name, base, size in STATE:
        if name == "I/O":
            diff = [i for i in IO_COMPARED if i not in (4, 5, 6, 7) and a[ofs + i] != b[ofs + i]]
        else:
            diff = [i for i in range(size) if a[ofs + i] != b[ofs + i]]
        if diff:
            out.append(f"{name} {len(diff)} bytes (0x{base + diff[0]:08X}-0x{base + diff[-1]:08X})")
        ofs += size
    return "drawn from different " + ", ".join(out) if out else "drawn from the same video state"


def run_input(name, args):
    inp = port_test.write_inp(name)
    ref, timing, lag, refvid, pcvid, mid = (OUT / f"{name}.{ext}"
                                            for ext in ("ref", "timing", "lag", "refvid", "pcvid", "mid"))
    refdump = [str(OUT / "refdump"), "--watch", str(args.watch), "--inputs", str(inp), "--dump", str(ref),
               "--timing", str(timing), "--fb-every", str(args.fb_every)]
    subprocess.run(refdump + ["--video", str(refvid), "--mid-frame", str(mid), str(ROOT / "baserom.gba")],
                   check=True)
    watch = port_test.load_watch(args.watch)
    irq = [l.split()[1] == "1" for l in timing.read_text().splitlines()]
    refrows = port_test.read_dump(ref)
    counter = next(i for i, w in enumerate(watch) if w[2] == "gMain.frameCounter")
    lags = port_test.lag_frames(refrows, counter)
    lag.write_text("".join(f"{n}\n" if irq[n] else f"{n} noirq\n" for n in lags))
    frames = min(args.frames or len(refrows), len(refrows))
    port = port_test.port_command(args) + ["--headless", "--inputs", str(inp), "--frames", str(frames),
            "--lag-frames", str(lag), "--fb-every", str(args.fb_every), "--rom", str(ROOT / "baserom.gba")]
    res = subprocess.run(port + ["--video", str(pcvid)])

    a, b = read_hashes(refvid), read_hashes(pcvid)
    skip = first_frames(inp)
    compared = sorted(f for f in b if f in a and f < frames and f not in skip)
    differ = [f for f in compared if a[f] != b[f]]
    expected = len([f for f in range(0, frames, args.fb_every) if f not in skip]) - \
        len([n for n in lags if n < frames and n % args.fb_every == 0 and n not in skip])
    stopped = res.returncode != 0 or len(compared) < expected

    # Every differing frame: its two screens and states, then why (if the original changed the
    # video state while drawing those lines)
    shots = differ[:args.shots]
    explained, unexplained = [], []
    if shots:
        shotdir = OUT / "video" / name
        shotdir.mkdir(parents=True, exist_ok=True)
        shotargs = sum((["--shot", str(f)] for f in shots), []) + ["--shot-dir", str(shotdir)]
        subprocess.run(refdump + shotargs + [str(ROOT / "baserom.gba")], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(port + shotargs, stdout=subprocess.DEVNULL)
        mids = read_mid(mid)
        for f in shots:
            r, p = shotdir / f"ref_baserom_frame{f:06}.ppm", shotdir / f"port_frame{f:06}.ppm"
            d = diff_shots(r, p)
            if d is None:
                unexplained.append((f, "the screenshots are the same (the hashes differ?)", None))
                continue
            n, (x, y), ca, cb, lines = d
            text = (f"{n} pixels on lines {lines[0]}-{lines[-1]}, first ({x},{y}): original {ca}, port {cb}")
            why = explain(lines, mids.get(f))
            if why:
                explained.append((f, text, why))
            else:
                unexplained.append((f, text, compare_states(shotdir / f"ref_state{f:06}.bin",
                                                            shotdir / f"port_state{f:06}.bin") +
                                    f" ({p.relative_to(ROOT)})"))
    unchecked = len(differ) - len(shots)
    ok = not stopped and not unexplained and not unchecked
    print(f"{'ok  ' if ok else 'FAIL'} {name}: {len(compared)} screens compared, {len(compared) - len(differ)} identical, "
          f"{len(explained)} differ where the original changed the video state while drawing them, "
          f"{len(unexplained) + unchecked} other differences")
    if stopped:
        print(f"     the port stopped early (exit status {res.returncode}, {len(compared)} of {expected} screens)")
    for f, text, why in unexplained:
        print(f"     frame {f:6}: {text}\n                   {why}")
    if unchecked:
        print(f"     {unchecked} more differing frames not checked (--shots {args.shots}): from frame {differ[len(shots)]}")
    if args.verbose:
        for f, text, why in explained:
            print(f"     frame {f:6}: {text}\n                   {why}")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="*", default=DEFAULT_INPUTS)
    ap.add_argument("--all", action="store_true", help="every input of tests/gameplay/inputs")
    ap.add_argument("--frames", type=int, default=0, help="compare only the first N frames")
    ap.add_argument("--fb-every", type=int, default=1, help="compare every Nth frame's screen (default 1)")
    ap.add_argument("--shots", type=int, default=64, help="check at most N differing frames per input (64)")
    ap.add_argument("-v", "--verbose", action="store_true", help="also list the expected differences")
    ap.add_argument("--watch", type=Path, default=ROOT / "tests/gameplay/watch.txt")
    ap.add_argument("--port", help='the port to run (default port/kururin_pc), e.g. "wine port/kururin_pc.exe"')
    args = ap.parse_args()
    os.chdir(ROOT)
    OUT.mkdir(parents=True, exist_ok=True)
    if args.all:
        args.inputs = sorted(p.stem for p in (ROOT / "tests/gameplay/inputs").iterdir()
                             if p.suffix in (".txt", ".bk2") and not p.name.startswith("_"))
    results = [run_input(n, args) for n in args.inputs]
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
