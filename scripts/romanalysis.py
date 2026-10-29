"""Control-flow analysis of the Kuru Kuru Kururin ROM (ARMv4T, Thumb).

This module is deliberately self-contained: a minimal ARMv4T Thumb decoder that
classifies instructions for control-flow purposes (it does not format them --
Luvdis does that), plus per-function recursive-descent exploration that finds
an entry point's reachable code, its literal pools and its jump tables.

Everything here is a pure function of the ROM bytes, so discovery is
reproducible.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

ROM_BASE = 0x08000000


def sx(value: int, bits: int) -> int:
    """Sign-extend the low *bits* of *value*."""
    sign = 1 << (bits - 1)
    return (value & (sign - 1)) - (value & sign)


@dataclass(slots=True)
class Insn:
    addr: int
    size: int
    kind: str  # see decode_thumb()
    raw: int
    target: int | None = None  # branch target / pc-relative load address
    rd: int | None = None
    rs: int | None = None
    imm: int | None = None


class Rom:
    def __init__(self, data: bytes):
        self.data = data
        self.size = len(data)

    def contains(self, addr: int, n: int = 1) -> bool:
        off = addr - ROM_BASE
        return 0 <= off and off + n <= self.size

    def u8(self, addr: int) -> int:
        return self.data[addr - ROM_BASE]

    def u16(self, addr: int) -> int:
        return struct.unpack_from("<H", self.data, addr - ROM_BASE)[0]

    def u32(self, addr: int) -> int:
        return struct.unpack_from("<I", self.data, addr - ROM_BASE)[0]

    def bytes(self, addr: int, n: int) -> bytes:
        off = addr - ROM_BASE
        return self.data[off:off + n]


# Instruction kinds produced by decode_thumb():
#   ill      undefined on ARMv4T (or a Thumb-2/ARMv5 encoding agbcc never emits)
#   bl       BL pair (4 bytes)
#   b        unconditional branch
#   bcond    conditional branch
#   bx       bx rs                      (return when rs == lr, or after pop {rX})
#   pop_pc   pop {..., pc}              (return)
#   mov_pc   mov pc, rs                 (jump table / computed jump)
#   add_pc   add pc, rs                 (computed jump)
#   ldr_pc   ldr rd, [pc, #imm]         (literal pool load)
#   adr      add rd, pc, #imm
#   swi      swi #n
#   push / pop / other
def decode_thumb(rom: Rom, addr: int) -> Insn:
    h = rom.u16(addr)
    top5 = h >> 11
    if top5 == 0b11110:  # BL prefix
        if rom.contains(addr + 2, 2):
            h2 = rom.u16(addr + 2)
            if (h2 >> 11) == 0b11111:
                off = (sx(h & 0x7FF, 11) << 12) | ((h2 & 0x7FF) << 1)
                return Insn(addr, 4, "bl", h | (h2 << 16), target=addr + 4 + off)
        return Insn(addr, 2, "ill", h)
    if top5 == 0b11111:  # BL suffix on its own
        return Insn(addr, 2, "ill", h)
    if top5 == 0b11100:
        return Insn(addr, 2, "b", h, target=addr + 4 + (sx(h & 0x7FF, 11) << 1))
    if top5 == 0b11101:  # BLX suffix (ARMv5)
        return Insn(addr, 2, "ill", h)
    if (h >> 12) == 0b1101:
        cond = (h >> 8) & 0xF
        if cond == 0xE:
            return Insn(addr, 2, "ill", h)
        if cond == 0xF:
            return Insn(addr, 2, "swi", h, imm=h & 0xFF)
        return Insn(addr, 2, "bcond", h, target=addr + 4 + (sx(h & 0xFF, 8) << 1), imm=cond)
    if (h >> 10) == 0b010001:  # hi register ops / bx
        op = (h >> 8) & 3
        rd = (h & 7) | ((h >> 4) & 8)
        rs = (h >> 3) & 0xF
        if op == 3:
            if h & 0x80 or h & 7:  # blx (v5) / nonzero SBZ bits
                return Insn(addr, 2, "ill", h)
            return Insn(addr, 2, "bx", h, rs=rs)
        if op == 2 and rd == 15:
            return Insn(addr, 2, "mov_pc", h, rd=rd, rs=rs)
        if op == 0 and rd == 15:
            return Insn(addr, 2, "add_pc", h, rd=rd, rs=rs)
        if op == 1 and rd == 15:
            return Insn(addr, 2, "ill", h)  # cmp pc, rX: unpredictable
        return Insn(addr, 2, "other", h, rd=rd, rs=rs, imm=op)
    if (h >> 11) == 0b01001:
        imm = (h & 0xFF) * 4
        return Insn(addr, 2, "ldr_pc", h, target=((addr + 4) & ~3) + imm, rd=(h >> 8) & 7)
    if (h >> 11) == 0b10100:
        imm = (h & 0xFF) * 4
        return Insn(addr, 2, "adr", h, target=((addr + 4) & ~3) + imm, rd=(h >> 8) & 7)
    if (h >> 8) == 0xB0:
        return Insn(addr, 2, "other", h)  # add/sub sp, #imm
    if (h >> 9) == 0b1011010:
        return Insn(addr, 2, "push", h, imm=h & 0x1FF)
    if (h >> 9) == 0b1011110:
        if h & 0x100:
            return Insn(addr, 2, "pop_pc", h, imm=h & 0x1FF)
        return Insn(addr, 2, "pop", h, imm=h & 0x1FF)
    if (h >> 12) == 0b1011:  # remaining misc space: v5+/Thumb-2 only
        return Insn(addr, 2, "ill", h)
    if (h >> 12) == 0b1100:  # ldmia/stmia
        if (h & 0xFF) == 0:
            return Insn(addr, 2, "ill", h)
        return Insn(addr, 2, "other", h)
    # Formats 1-4, 7-11: always defined.
    return Insn(addr, 2, "other", h, rd=h & 7, rs=(h >> 3) & 7)


# ---------------------------------------------------------------------------
# Function exploration
# ---------------------------------------------------------------------------


@dataclass
class FuncInfo:
    entry: int
    ok: bool = True
    insns: dict[int, Insn] = field(default_factory=dict)
    pools: set[int] = field(default_factory=set)        # addresses of 4-byte pool words
    adr_targets: set[int] = field(default_factory=set)
    jumptables: dict[int, tuple[int, list[int]]] = field(default_factory=dict)  # mov pc addr -> (table, targets)
    calls: set[int] = field(default_factory=set)         # bl targets
    tailcalls: set[int] = field(default_factory=set)     # bx to a pool-loaded odd ROM address
    arm_targets: set[int] = field(default_factory=set)  # `adr rX, L; bx rX` switches to ARM at L
    returns: int = 0
    stack_underflow: bool = False  # some path pops more than the function pushed
    issues: list[str] = field(default_factory=list)

    @property
    def code_end(self) -> int:
        """End of the highest instruction, pool word or jump table."""
        end = self.entry
        for a, i in self.insns.items():
            end = max(end, a + i.size)
        for p in self.pools:
            end = max(end, p + 4)
        for table, targets in self.jumptables.values():
            end = max(end, table + 4 * len(targets))
        return end

    def data_ranges(self) -> list[tuple[int, int, str]]:
        out = [(p, p + 4, "pool") for p in self.pools]
        for table, targets in self.jumptables.values():
            out.append((table, table + 4 * len(targets), "jumptable"))
        return sorted(out)


def _find_jumptable(rom: Rom, fi: FuncInfo, block: list[Insn], region_end: int):
    """Recognise agbcc's switch idiom ending at block[-1] (mov pc, rX).

        cmp     rI, #N          @ optional bound check (gives N+1 entries)
        bhi     default
        lsls    rI, rI, #2
        ldr     rT, =table
        adds    rI, rI, rT
        ldr     rI, [rI]
        mov     pc, rI
        .align  2, 0
    table:
        .4byte  case0, case1, ...
    """
    table = None
    count = None
    for ins in reversed(block[-8:-1]):
        if ins.kind == "ldr_pc" and table is None and rom.contains(ins.target, 4):
            v = rom.u32(ins.target)
            if rom.contains(v, 4) and v % 4 == 0:
                table = v
    if table is None:
        return None
    for ins in reversed(block[-14:-1]):
        # cmp rX, #imm8 : 0x28xx-0x2Fxx
        if (ins.raw >> 11) == 0b00101 and ins.size == 2:
            count = (ins.raw & 0xFF) + 1
            break
    targets = []
    limit = count if count is not None else 512
    a = table
    while len(targets) < limit and rom.contains(a, 4):
        t = rom.u32(a)
        if t % 2 or not (fi.entry <= t < region_end):
            break
        if targets and a >= min(targets):
            break
        targets.append(t)
        a += 4
    if count is not None and len(targets) != count:
        fi.issues.append(f"jumptable@{table:08X}: expected {count} entries, decoded {len(targets)}")
    return (table, targets) if targets else None


def _popcount(x: int) -> int:
    return bin(x).count("1")


def explore_thumb(rom: Rom, entry: int, region_end: int, stop_at: set[int] | frozenset = frozenset(),
                  noreturn: set[int] | frozenset = frozenset(),
                  farjumps: set[int] | frozenset = frozenset()) -> FuncInfo:
    """Recursive-descent exploration of the Thumb function at *entry*.

    BL targets are recorded as calls and not followed; a BL to a function in
    *noreturn* ends the path. A BL to an address in *farjumps* is agbcc's long
    unconditional branch (used when a `b` is out of range) and is followed
    like `b`. *stop_at* holds known function starts: a straight-line path that
    runs into one of them (rather than branching to it) is recorded as a
    fall-through issue.

    The stack-pointer delta is tracked along every path; a return reached
    with more popped than pushed sets `stack_underflow` -- the entry is then an
    interior label of some other function, not a function.
    """
    fi = FuncInfo(entry)
    work = [(entry, 0)]
    seen_starts = set()
    while work:
        a, sp = work.pop()
        if a in seen_starts:
            continue
        seen_starts.add(a)
        block: list[Insn] = []
        while True:
            if a in fi.insns:
                break
            if a != entry and a in stop_at:
                if block and block[-1].kind != "b":  # straight-line flow into another function
                    fi.issues.append(f"falls through into {a:08X}")
                    fi.ok = False
                else:  # reached by a branch: agbcc never branches into another function
                    fi.issues.append(f"branches into {a:08X}")
                break
            if not (ROM_BASE <= a < region_end) or a % 2:
                fi.issues.append(f"flow leaves code region at {a:08X}")
                fi.ok = False
                break
            if any(p <= a < p + 4 for p in fi.pools):
                fi.issues.append(f"executes pool word at {a:08X}")
                fi.ok = False
                break
            ins = decode_thumb(rom, a)
            if ins.kind == "ill":
                fi.issues.append(f"illegal instruction {ins.raw:04X} at {a:08X}")
                fi.ok = False
                break
            fi.insns[a] = ins
            block.append(ins)
            k = ins.kind
            if k == "ldr_pc":
                if rom.contains(ins.target, 4):
                    fi.pools.add(ins.target)
                else:
                    fi.issues.append(f"pool outside ROM at {a:08X}")
                    fi.ok = False
            elif k == "adr":
                fi.adr_targets.add(ins.target)
            elif k == "bl":
                if ins.target in farjumps:
                    work.append((ins.target, sp))
                    break
                fi.calls.add(ins.target)
                if ins.target in noreturn:
                    break
            elif k == "bcond":
                work.append((ins.target, sp))
            elif k == "b":
                work.append((ins.target, sp))
                break
            elif k == "push":
                if sp is not None:
                    sp -= 4 * _popcount(ins.imm)
            elif k == "pop":
                if sp is not None:
                    sp += 4 * _popcount(ins.imm)
            elif k == "other" and (ins.raw >> 8) == 0xB0 and sp is not None:
                imm = (ins.raw & 0x7F) * 4
                sp += -imm if ins.raw & 0x80 else imm
            elif k == "other" and (ins.raw & 0xFF87) == 0x4685:  # mov sp, rX
                sp = None
            elif k == "other" and (ins.raw & 0xFF87) == 0x4485:  # add sp, rX
                sp = None
            if k in ("bx", "pop_pc") or (k == "mov_pc" and ins.rs == 14):
                if k == "pop_pc" and sp is not None:
                    sp += 4 * _popcount(ins.imm)
                if sp is not None and sp > 0:
                    fi.stack_underflow = True
            if k == "mov_pc" and ins.rs == 14:  # mov pc, lr: old-style return
                fi.returns += 1
                break
            if k == "bx":
                adr = next((p for p in reversed(block[:-1]) if p.rd == ins.rs and p.kind in ("adr", "ldr_pc")), None)
                if adr is not None and adr.kind == "adr":  # mode switch into ARM code
                    fi.arm_targets.add(adr.target)
                    break
                fi.returns += 1
                # bx rX where rX came from a pool word holding an odd ROM address: tail call
                for prev in reversed(block[:-1]):
                    if prev.kind == "ldr_pc" and prev.rd == ins.rs and rom.contains(prev.target, 4):
                        v = rom.u32(prev.target)
                        if v & 1 and rom.contains(v - 1, 2):
                            fi.tailcalls.add(v - 1)
                        break
                break
            elif k == "pop_pc":
                fi.returns += 1
                break
            elif k in ("mov_pc", "add_pc"):
                jt = _find_jumptable(rom, fi, block, region_end) if k == "mov_pc" else None
                if jt is None:
                    fi.issues.append(f"unresolved computed jump at {a:08X}")
                else:
                    fi.jumptables[a] = jt
                    work.extend((t, sp) for t in jt[1])
                break
            elif k == "swi":
                pass
            a += ins.size
    # Instructions must not overlap pool words or jump tables.
    for lo, hi, what in fi.data_ranges():
        for x in range(lo - 2, hi, 2):
            i = fi.insns.get(x)
            if i is not None and x + i.size > lo:
                fi.issues.append(f"instruction at {x:08X} overlaps {what} at {lo:08X}")
                fi.ok = False
                break
    return fi


# ---------------------------------------------------------------------------
# ARM (only hand-written code: crt0, interrupt handlers, m4a's ARM blocks)
# ---------------------------------------------------------------------------


@dataclass
class ArmInfo:
    entry: int
    ok: bool = True
    insns: set[int] = field(default_factory=set)
    pools: set[int] = field(default_factory=set)
    calls: set[int] = field(default_factory=set)
    thumb_targets: set[int] = field(default_factory=set)  # `add rX, pc, #n+1; bx rX` back to Thumb
    issues: list[str] = field(default_factory=list)

    @property
    def end(self) -> int:
        return max([a + 4 for a in self.insns] + [p + 4 for p in self.pools] + [self.entry])


def explore_arm(rom: Rom, entry: int, region_end: int) -> ArmInfo:
    """Recursive descent over ARM code: finds its instructions and literal pools."""
    ai = ArmInfo(entry)
    work = [entry]
    while work:
        a = work.pop()
        adr = {}  # register -> value of a preceding `add rX, pc, #imm`
        while True:
            if a in ai.insns:
                break
            if not (ROM_BASE <= a < region_end) or a % 4 or a in ai.pools:
                ai.issues.append(f"ARM flow leaves code at {a:08X}")
                ai.ok = False
                break
            w = rom.u32(a)
            cond = w >> 28
            if cond == 0xF:
                ai.issues.append(f"undefined ARM instruction {w:08X} at {a:08X}")
                ai.ok = False
                break
            ai.insns.add(a)
            always = cond == 0xE
            op = (w >> 25) & 7
            if (w & 0x0FFFFFF0) == 0x012FFF10:  # bx
                t = adr.get(w & 15)
                if t is not None and t & 1:
                    ai.thumb_targets.add(t - 1)
                called = a - 4 in ai.insns and rom.u32(a - 4) == 0xE1A0E00F  # mov lr, pc; bx rX
                if always and not called:
                    break
            elif (w & 0x0FFF0000) == 0x028F0000:  # add rX, pc, #imm (adr)
                rot = ((w >> 8) & 15) * 2
                imm = w & 0xFF
                imm = ((imm >> rot) | (imm << (32 - rot))) & 0xFFFFFFFF
                adr[(w >> 12) & 15] = a + 8 + imm
            elif op == 0b101:  # b / bl
                t = a + 8 + (sx(w & 0xFFFFFF, 24) << 2)
                if w & (1 << 24):
                    ai.calls.add(t)
                else:
                    work.append(t)
                    if always:
                        break
            elif op == 0b100:  # ldm/stm
                if (w & (1 << 20)) and (w & 0x8000) and always:
                    break
            elif op in (0b010, 0b011):  # ldr/str
                load = bool(w & (1 << 20))
                rn, rd = (w >> 16) & 15, (w >> 12) & 15
                if load and op == 0b010 and rn == 15:
                    off = w & 0xFFF
                    ai.pools.add(a + 8 + (off if w & (1 << 23) else -off))
                if load and rd == 15 and always:
                    break
            elif op in (0b000, 0b001):
                is_msr = (w & 0x0DB0F000) == 0x0120F000
                is_mul_or_xfer = op == 0 and (w & 0x90) == 0x90
                opcode = (w >> 21) & 15
                is_test = 8 <= opcode <= 11
                if not (is_msr or is_mul_or_xfer or is_test) and ((w >> 12) & 15) == 15 and always:
                    break  # writes pc: return / computed jump
            elif op == 0b111 and not (w & (1 << 24)):
                ai.issues.append(f"coprocessor instruction {w:08X} at {a:08X}")
                ai.ok = False
                break
            a += 4
    for p in ai.pools:
        if p in ai.insns:
            ai.issues.append(f"ARM pool word {p:08X} is also executed")
            ai.ok = False
    return ai
