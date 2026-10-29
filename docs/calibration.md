# Difficulty calibration (Phase 1)

To check what the tiers of `scripts/difficulty.py` mean in practice, one agent per sampled
function tried to match it under the rules in CLAUDE.md (asmlift/m2c first, clean C, no
inline asm/barriers/register pins/per-function flags), each in a scratch copy of the
repository, nothing committed. "Matched" means `make compare` printed `kururin.gba: OK`
with the C in place.

**These matches are not committed.** The C below is a starting point: commit each one the
normal way (one function per commit, after `make compare` in the real tree). The fixes they
needed are already in the repository (DMA macros in `include/gba/macro.h`, `REG_WAITCNT` in
`include/gba/io_reg.h`, libgcc names, the SRAM names, `agb_sram` built at `-O`), so the C below
compiles in the current tree as it stands. The per-function notes were written during the
runs; where they recommend a change (per-module `cflags`, a rename, new macros), it has been
made since. The functions were renamed after the runs; the notes keep the `sub_` names they
had then. The `[symbols]` entries the blocks mention are in `kururin.toml`.

| Function | Tier | Matched | Iterations | Notes |
| --- | --- | --- | --- | --- |
| `sub_0800456C` | easy | yes | 2 | early returns reorder blocks; a single `result` variable matches |
| `sub_08004250` | easy | yes | 2 | DMA3 copy (`DmaCopy32`); asmlift matched first try |
| `sub_080042F8` | medium | yes | 1 | screen-bounds check; asmlift draft matched |
| `sub_08004604` | medium | yes | 1 | menu callback calls via `_call_via_r2` (needs the libgcc names) |
| `sub_08013040` | hard | yes | 6 | decimal digits as sprites; `/` needs `__udivsi3`; operand order drives hoisting; asmlift declined, m2c draft used |
| `MPlayContinue` | library (m4a) | yes | 15 | not plain agbcc (needs the bare `bx lr`); SDK `ident = ID_NUMBER` write-back decides the register |
| `MPlayFadeOut` | library (m4a) | yes | 3 | same idiom as MPlayContinue |
| `sub_080016E0` | hard | yes | 24 | minimap render; needed gcc pass dumps; one contrived but legal construct (see below) |
| `ReadSram_Core` (`sub_0801C5D0` then) | library (agb_sram) | yes | 14 | natural SDK C matches at module -O |
| `WriteSram` (`sub_0801C658` then) | library (agb_sram) | yes | 14 | natural SDK C matches at module -O (99.67% at -O2) |

All ten matched. `WriteSram` reached 100% only once `agb_sram` was built at `-O` (now the
module's `cflags`). Iteration counts are compile/score rounds; the hard ones also needed
reading agbcc's RTL dumps (`-dl -dg -dL`).

## What this says about the tiers

Counts are from before the 7 functions decompiled since (`scripts/difficulty.py` now lists
those under done).

- **trivial / easy** (81 functions): expect a match in 1–3 rounds, usually straight from
  asmlift's best candidate rewritten with real types.
- **medium** (131): both samples matched on the first compile; the tier is conservative.
- **hard** (144): both matched, in 6 and 24 rounds. Large functions need knowledge of
  agbcc's register allocator and loop optimizer; plan for them to be slow, not impossible.
- **library code**: the compiler and flags per translation unit matter more than the C
  (m4a: old_agbcc; agb_sram: -O; see below). SDK sources reconstructed by pret are a reliable guide.

## Which compiler (found after the runs)

The runs above used plain agbcc for game, and every game function matched with it. Two code
shapes tell the three settings apart, and the samples contained neither:

- a *leaf function with a branch*: plain agbcc saves lr there (`push {lr} … pop {rN}; bx rN`),
  old_agbcc and `agbcc -fprologue-bugfix` return with a bare `bx lr`;
- a *single-use byte/halfword load feeding an and/orr/add*: old_agbcc folds it and loads it right
  before the operation, agbcc keeps it where the source has it.

Game has seven leaves with a branch that save no registers, and all seven end in a bare
`bx lr` (leaves that save r4 and up push lr with every setting). Two functions settle it:

- `sub_08004348` (moves `a` towards `b` by at most `step`) matches with old_agbcc and with
  `agbcc -fprologue-bugfix`, never with plain agbcc (it gains `push {lr}`):

  ```c
  s32 sub_08004348(s32 a, s32 b, u32 step)
  {
      s32 diff = b - a;

      if (diff != 0)
      {
          if (step == 0)
              step = 1;
          if (diff > 0)
          {
              if (diff > step)
                  return a + step;
          }
          else if (-diff > step)
          {
              return a - step;
          }
          return b;
      }
      return a;
  }
  ```

- `sub_08002790` (16 four-sprite OAM blocks) matches with agbcc in natural C and reaches only
  90.91% with old_agbcc. old_agbcc's `s_register_operand` (thumb.c, `OLD_COMPILER`) accepts
  `(subreg (mem))`, so combine folds each single-use `ldrh` of an OAM halfword into its `and`
  and reload emits the load right next to it; agbcc keeps the load first, as the ROM does.
  (`sub_080016E0` below also matches only with agbcc as written, but old_agbcc matches it
  too once its reads are made `volatile`, so it is weaker evidence.)

So game is `agbcc -fprologue-bugfix`: all six game functions below plus `sub_08004348` match
together with it. The same two tests settle the libraries:

- multiboot: the SDK's `MultiBootCheckComplete` (a leaf with a branch) rules out plain agbcc;
  `MultiBootHandShake` keeps the `ldrb` of `masterp[0xAC]` before its `orr`, which rules out
  old_agbcc (96.06%). multiboot is `agbcc -fprologue-bugfix`, like game.
- m4a: `MPlayContinue`/`MPlayFadeOut` rule out plain agbcc; the SDK's `CgbModVol` has
  old_agbcc's folded `ldrb r2, [r1, #3]; ldrb r3, [r1, #2]; adds r0, r2, r3`, which rules out
  `agbcc -fprologue-bugfix` (88.02%). m4a is `old_agbcc`.

The C for all of these is in `tests/compiler_evidence/`, and `make compare` checks it against
each module's setting (`scripts/check_compilers.py`).

## agbcc lessons


### `sub_0800456C` (easy)

This was easy: it took 2 score iterations and about 5 minutes. The only problem was block order. The natural early-return form (if (x<=0) return -1; if (--x > 0) return 0; return call();) scored 81.36%. agbcc kept the bl call on the fall-through path and moved 'return 0' after it (bgt to 'mov r0,#0'). Writing it as one if/else-if/else chain that assigns a result variable, with a single return, gave the target layout (ble to the call; 'mov r0,#0; b end' first) and matched 100%. Lesson for CLAUDE.md: when the target puts a short constant return before a call block, try a result variable with a single return instead of early returns. Other codegen notes: the s16 field shows up as ldrh (kept for the decrement) plus a separate ldsh for the compare. '--field > 0' on an s16 gives sub/strh/lsl #16/cmp. Both are the plain signed-short idioms and needed no tricks. The setup command in the task uses rsync, which is not installed. I used 'tar --exclude=./tools --exclude=./.venv --exclude=./build --exclude=./expected -cf - . | tar -xf - -C $D' instead.

*asmlift/m2c:* Yes, completely. asmlift's winning candidate ('unsigned/setup-args') scored 0/22 (an exact match), and it already had the right shape: one result variable with if/else. My clean rewrite with structs and names only had to keep that shape. My first rewrite used early returns and lost the match (81%); going back to asmlift's single-return structure fixed it. m2c could not decode 'ldsh' with a register offset and emitted M2C_ERROR, and it used early returns, which is the non-matching shape. asmlift was far more useful than m2c here.

### `sub_08004250` (easy)

This was trivial. It is the standard pret DmaSet/DmaCopy32 pattern: write SAD, DAD and CNT to DMA3 through a vu32 pointer, then do a volatile read-back of CNT. The read-back is the trailing 'ldr r0, [r1, #8]', so the function returns void; it does not return a value. CNT 0x84000040 is DMA_ENABLE | DMA_32BIT | 0x40 words, which is 0x100 bytes. That fits gUnk_03004280 being a u8[256] (sub_08004224 indexes it with a u8). The source 0x0801CE34 is a pool word into the data incbin. Adding it to [symbols] only gives it a name, because the bytes are the same either way. sub_08004274 uses the same table. The repo has no DMA helpers yet, so a shared gba/macro.h with DmaSet/DmaCopy32 (plus REG_ADDR_DMA3 and the DMA_* flags in io_reg.h) is worth adding for real, since more DMA functions will need it. Setup note: the task's rsync command fails because rsync is not installed in this container. 'tar --exclude=./tools --exclude=./.venv --exclude=./build --exclude=./expected -cf - . | tar -xf - -C $D' does the same job. In the copy, git diff also lists tools/ghidra_scripts/* as deleted, but that only happens because tools is a symlink there; nothing was deleted (fixed since: the Ghidra scripts live in scripts/ghidra and the symlinks are gitignored). Calibration verdict: 'easy' overstates this one. It is trivial, and it took 1 asmlift run, 2 score runs and 1 make compare.

*asmlift/m2c:* Yes, directly. Its 'unsigned/livebase/volatile' candidate (a volatile s32* at 0x040000D4 with three stores and a read-back) scored as a full match on the first run. I rewrote it as idiomatic C with the pret DmaCopy32 macro, a named ROM symbol and a void return, and that also scored 100% on the first try. I did not need m2c.

### `sub_080042F8` (medium)

This turned out easy, not really medium. It is a 38-instruction screen-bounds check (x - camX and y - camY compared against -margin and margin+240 / margin+160) and needed no data splitting. A pattern worth recording: the asm zero-extends the second parameter on entry (lsl/lsr into r4) and sign-extends it again (lsl/asr) at its later use. That is what agbcc does for an s16 parameter whose first use comes after other code, so it does not mean the parameter was u16. Declaring it s16 matched, and so did asmlift's (s16)u16 cast form. The first parameter is sign-extended right away because it is used immediately. Writing it as early returns, with the dy computation after the first check, gave the same code as asmlift's single || chain. The two globals are probably the camera or screen origin (s32), since the window is 240x160, but I kept the gUnk_ names because there is no outside evidence for them yet. Callers: sub_08016264 and sub_0801677C (called twice there).

*asmlift/m2c:* Yes. Its top candidate (unsigned/flip-branch/derived-home) already matched, 0/38 diff. It used magic-number pointer derefs (*(s32 *)50344420) and a (s16)a1 cast on a u16 parameter. I only rewrote it as clean C with named globals and an s16 parameter, and it matched on the first compile. I did not need m2c.

### `sub_08004604` (medium)

For a medium-tier function this was easy: it matched on the first compile. What made it match:
(1) The 'bl sub_0801C734' calls are function-pointer calls. 0x0801C734 is libgcc's _call_via_r2 ('bx r2; nop'), and agbcc emits 'bl _call_via_r2' for any call through a pointer whose address lands in r2. Clean C (a->unk8(a, x)) only links to the same bytes once 0x0801C734 is renamed to _call_via_r2 in [renames]. The project should name the whole stub block now: 0x0801C72C..0x0801C764 are _call_via_r0..r9, sl, fp, ip, sp, lr (15 four-byte 'bx rN; nop' stubs; done, see [renames]). Many game functions will need these names.
(2) The function takes a second parameter it never uses itself. r1 is never written before 'bl sub_08004858', which tests r1 & 0x20 and r1 & 0x10, so the parameter is passed straight through. agbcc leaves an untouched pass-through argument in r1, so declaring (struct *, int) gives the same code as the one-argument version asmlift guessed, and it is the correct prototype.
(3) The s8 fields at +0 and +1 come from ldsb, and the u8 copies at +2..+5 from ldrb/strb. An int loop counter compared with <= against the s8 field gives the exact 'b to test; ble' loop shape.
The sibling sub_0800466C has the same code except that it calls sub_080048B8, so the same template should match it, and probably other neighbours. The struct looks like a menu (count at +0, wrapping cursor at +1, draw callback at +8; sub_08004858 calls m4aSongNumStart(0x20), probably a sound). I kept unk names because there is no outside evidence for real ones yet.

*asmlift/m2c:* Yes, almost completely. Its top candidate scored 0/45 diff (a match) on the first try. It treated sub_0801C734 as an ordinary 3-argument function (sub_0801C734(a0, x, a0->field_8)) and left offsets 0 and 1 as raw casts. The only manual work was rewriting that into a struct with a function-pointer field, adding the pass-through parameter and the _call_via_r2 rename. I did not need m2c.

### `sub_08013040` (hard)

For the hard tier this took only about 6 compile/score rounds; the tier label overstated it. Four things decided the codegen:

1. **Write a division as C `/`.** The library routine at 0x0801C8D0 is __udivsi3. It needs a [renames] entry so that `/ 10` on an unsigned value links to it. The project should add the libgcc renames (__divsi3, __modsi3, __udivsi3, __umodsi3) once, for every function that divides.

2. **Operand order controls loop-invariant hoisting.** The target computes `palette << 12` once before the loop, keeps it in r10, and keeps palette itself in a stack slot. agbcc only did that when the source said `(palette << 12) | (digit*2 + 0x40)`. With the operands the other way round, nothing was hoisted and the score fell to 83-86%. Reading the order from the copy of the expression that is not in the loop gave the right answer.

3. **Initialisation order decides register allocation.** Writing `n = value; divisor = 10000; started = 0;` in that order put y in r9 and started in r8, as in the target, and fixed the scheduling of `ldr` and `movs #0`. A do-while and a `for` loop give the same loop shape here.

4. **One variable reused across two blocks changes register allocation.** A single `oam` pointer used in both places came out in r3 instead of r2 (99.43%). Declaring a separate pointer inside each block made it 100%.

Also: a u16 parameter copied into a u32 local gives one `lsl/lsr` at entry and no narrowing inside the loop.

*asmlift/m2c:* No. asmlift declined the function: it read `str r3, [sp]` (palette spilled to the stack) as a possible outgoing stack argument of the bl ('stack pointer used as data'). m2c gave a structurally correct draft, close to the final C apart from operand order, variable scoping and the struct, which would score about 83-86% as is. The real work was the four codegen points above.

### `MPlayContinue` (library (m4a))

The obvious C, if (ident == ID_NUMBER) status &= ~PAUSE;, scores 99.17%: the ROM loads ident into r3, the obvious C into r1. Neither source variations (early return, local temp, swapped compare, volatile, signed types, bitfield, raw u32*) nor flags (-O1/-O2/-O3, -fno-expensive-optimizations, -fno-cse-*, -fregmove) changed it. An old_agbcc -dl/-dg RTL dump explained it: in the obvious C the ident pseudo lives in one basic block, so local-alloc gives it the first free register (r1). The ROM's r3 is what global-alloc picks when ident is live through the then-block (where r0 and r1 hold status and the mask). The fix is pret's lock pattern minus the ident++: add mplayInfo->ident = ID_NUMBER; at the end of the then-block. CSE turns the constant into the already-loaded ident register (as in pokeemerald's str r3,[r2,#0x34]), which keeps that register live across the block. The store itself is still present in the .combine and .lreg dumps but gone in .greg, so reload deletes it as a store of a REG_EQUIV pseudo back into its own memory. Net effect: no store in the output, only the register choice. General lesson: an unexplained higher register (r3 instead of r1) for a value used only in the condition suggests the source used it again later in a way that was optimised out (a redundant write-back). This kururin m4a is an older SDK revision with no ident++ lock but with the ident restore. Difficulty: a 10-instruction function was only 99%, and closing the gap needed RTL-dump-level reasoning; the template-guessing alone did not find it.

*asmlift/m2c:* Barely. asmlift ran against agbcc, not old_agbcc: it takes its target from decomp.yaml (tools.asmlift.target = agbcc) and objdiff.json has no unit for these symbols, so it ignores the module's cc. (Fixed since: asmlift now takes each module's compiler and flags from objdiff.json.) Its winner scored 10/16, using s32* indexing (a0[13]) and a spurious return value. m2c gave correct structure immediately (99.17% under old_agbcc). Neither tool found the register-allocation fix.

### `MPlayFadeOut` (library (m4a))

Same root cause as MPlayContinue. In the plain C, local-alloc gave the block-local ident pseudo r1 first, which pushed the zero-extended u16 speed (a global pseudo) to r3 (lsr r3,r1,#16 instead of lsrs r1,r1,#16), making 94.67%. The trailing mplayInfo->ident = ID_NUMBER; makes ident a global pseudo; speed then takes r1 (its hard-reg preference) and ident r3, exactly as in the ROM. The fix from MPlayContinue transferred without further iteration, so expect the whole older-revision m4a API family (MPlayStop/MPlayContinue/MPlayFadeOut style 'if ident == ID_NUMBER {...; ident = ID_NUMBER;}') to follow this pattern. The u16 speed parameter is genuinely u16: the ROM zero-extends it at entry. Field order and offsets match pret's MusicPlayerInfo (fadeOI 0x24, fadeOC 0x26, fadeOV 0x28, ident 0x34), with stores in the order fadeOC, fadeOI, fadeOV.

*asmlift/m2c:* Barely: winner 9/19 against agbcc (wrong compiler from decomp.yaml), with an invented struct and a spurious return value. m2c gave the correct body (94.67% under old_agbcc). Neither found the write-back trick.

### `sub_080016E0` (hard)

Difficulty: hard tier, and it earned it. The logic was clear from the asm in minutes: the function finds the first map tile whose value is 0xFB/0xFC, steps back 16 columns and 32 rows (clamped at 0), and renders a 64x64 4bpp minimap into OBJ VRAM at 0x060174A0. Every point from 83% up was agbcc codegen detail, and I had to read gcc's loop.c, flow.c and global.c dumps (-dL, -dl, -dg) to close it. Lessons for agbcc:
1. Loop-invariant motion needs threshold*savings*lifetime >= insn_count (threshold = 2*(1+n_non_fixed_regs)). In a big loop, a short-lived invariant such as x<<1 inside `src = map + y*width + x` is NOT hoisted. The target had x*2 hoisted into a stack slot, and only identical computations in both branches of the if/else made it happen: loop.c merges matching movables (combine_movables) and adds their savings and lifetimes. So a duplicated `src = ...` in the zero-fill branch, dead there and removed later by flow, is what hoists it. Without it: 88.7%. With it: 95-97%. Writing `map + x + y*width` instead hoists map+x*2 as a whole and gives a single add, which is wrong.
2. The pointer form decides the induction-variable shape. `row = map + y*width; *row++` gives the target's byte-offset giv (stride width<<1 kept in a register, with row = map + off computed before x = 0). `map[y*width + x]` gives an index giv scaled later. A separate `row` pointer for the search loop, instead of reusing `src`, fixed the register numbering (96.86%).
3. The last diff was which hoisted constant got r10: 0x3FF or 0xE0000000. global.c priority is floor_log2(refs)*refs/live_length (live length counts notes too), and the constant 0xE0000000 had the shorter live range, so it won. Writing `u32 color = 0xE0000000; ... bits |= color;` makes it a function-long pseudo. thumb's ior needs a register, so CSE cannot fold it back to a constant. It gets lowest priority, is spilled, and is rematerialized inline as mov #0xE0; lsl #24 through REG_EQUIV, which gives 100%.
4. Small notes: the range checks `t == 0xFB || t == 0xFC` and `t >= 1 && t <= 0x9F` fold to sub + unsigned cmp. An inner loop whose counter is unused is reversed to the subs/cmp #0/bge countdown. Unsigned compares against height/width (bcs/bcc) together with signed clamps (bge) mean u32 width/height and int x/y.
The two tricks (dead duplicate src, constant held in a variable) are legal C under the CLAUDE.md rules: no asm, no barriers, no register pins, no flags. The dead duplicate looks unnatural, though. The original code may have had some other structure that produces the same pair of movables.

*asmlift/m2c:* No. asmlift refused to structure the function ('unrecovered back-edge into block #2'): the goto out of the doubly nested search loop defeats its loop recovery. m2c's draft was structurally correct (goto spaghetti, but the right ops and constants) and confirmed my own reading of the asm, but it did not help with matching. My first hand-written C scored 71%, and two changes to structure and pointer form took it to 88%. Everything after that was compiler-internals work that neither tool can do.

### `ReadSram_Core` (library (agb_sram))

Strong evidence that the SRAM library was compiled with -O, not -O2. This is the same setup as pret's agb_flash (pokeemerald: `agb_flash.o: CFLAGS := -O -mthumb-interwork`). At -O, natural SDK C matches three of the five agb_sram functions by objdiff with both compilers: ReadSram_Core, WriteSram and VerifySram_Core. The two stack-copy wrappers ReadSram (sub_0801C5F4) and VerifySram (sub_0801C6C8) reach 95.33% with C I wrote quickly (out of scope). Their remaining diffs are a CSE'd pool load of the core function address, and the bl target. The Makefile should get per-module CFLAGS, e.g. `cflags = "-O"` for agb_sram in kururin.toml, mirroring `cc` (done). The libgcc symbol at 0x0801C738 should be renamed `_call_via_r3` (the known libgcc routine), because agbcc emits `bl _call_via_r3` for function-pointer calls and the link fails while it is still `sub_0801C738` (done). The library cc (old_agbcc vs agbcc) cannot be decided from agb_sram. Every function there saves r4 or more, and none loads a single-use byte or halfword into an and/orr/add, so neither shape that tells the compilers apart (see "Which compiler") shows up. Evidence for cc has to come from m4a or libc. Difficulty: trivial once the -O hypothesis was tried. At -O2 it took reading jump.c and loop.c dumps to understand why it fails.

*asmlift/m2c:* Partly. asmlift reported its own `v2 = a2 - 1; if (a2 != 0) do {...} while (v2 != -1)` draft as a match with the agbcc -O2 command, which gave me the contrived -O2 form. That draft is not idiomatic and hides that the real cause is -O. m2c gave the same structure. Neither suggested the SDK source.

### `WriteSram` (library (agb_sram))

The -O2 r0/r1 swap on `(REG & ~3) | 3` is a pass difference between -O and -O2 (likely regmove or the expensive optimizations). No C rewrite I tried changed it. The same RMW in ReadSram and VerifySram also gets the -O register choice (those two wrappers still differ elsewhere; see above). Recommendation: add per-module cflags to kururin.toml and the generator, set agb_sram to -O, and leave cc as it is (unverifiable here). (Done.) Difficulty under the right flags: trivial, since the SDK source matches as-is.

*asmlift/m2c:* Somewhat. asmlift's best candidate scored 21/32 (it emitted the RMW as a literal-address expression and restructured the loop test). m2c gave correct structure. Both pointed at the SDK WriteSram shape, but neither could close the -O2 register swap, because that comes from the flags, not from the C.

## The C (uncommitted)

### `sub_0800456C`

```c
// 8-byte entries walked by sub_08004534: a signed count, then a word.
struct Unk08004534Entry {
    s16 unk0;
    u32 unk4;
};

struct Unk08004534 {
    struct Unk08004534Entry *unk0;
    s16 unk4;
    u32 unk8;
};

s32 sub_08004534(struct Unk08004534 *obj, struct Unk08004534Entry *entry);

/**
 * Counts down unk4; when it reaches zero, hands the current entry (unk0) to
 * sub_08004534. Returns -1 if unk4 was already <= 0, 0 while still counting.
 */
s32 sub_0800456C(struct Unk08004534 *obj)
{
    s32 result;

    if (obj->unk4 <= 0)
        result = -1;
    else if (--obj->unk4 > 0)
        result = 0;
    else
        result = sub_08004534(obj, obj->unk0);
    return result;
}
```

### `sub_08004250`

```c
/* src/game.c (DmaCopy32 is in include/gba/macro.h) */
extern const u8 gUnk_0801CE34[];

/**
 * Resets the 256-byte table gUnk_03004280 from its ROM copy gUnk_0801CE34 (DMA3, 32-bit).
 */
void sub_08004250(void)
{
    DmaCopy32(3, gUnk_0801CE34, gUnk_03004280, 0x100);
}

/* kururin.toml [symbols] */
gUnk_0801CE34 = 0x0801CE34    # u8[256], ROM copy of gUnk_03004280 (sub_08004250 DMAs it over)
```

### `sub_080042F8`

```c
extern s32 gUnk_030031E4;
extern s32 gUnk_030031E8;

/**
 * Returns 0 if the point (x, y) lies within margin pixels of the 240x160 screen
 * window whose top-left is (gUnk_030031E4, gUnk_030031E8), -1 otherwise.
 */
s32 sub_080042F8(s16 x, s16 y, u8 margin)
{
    s32 dx = x - gUnk_030031E4;
    s32 dy;

    if (dx <= -margin || dx >= margin + 240)
        return -1;
    dy = y - gUnk_030031E8;
    if (dy <= -margin || dy >= margin + 160)
        return -1;
    return 0;
}

/* kururin.toml [symbols]: gUnk_030031E4 = 0x030031E4, gUnk_030031E8 = 0x030031E8 */
```

### `sub_08004604`

```c
struct Unk_08004604 {
    s8 unk0;
    s8 unk1;
    u8 unk2;
    u8 unk3;
    u8 unk4;
    u8 unk5;
    int (*unk8)(struct Unk_08004604 *, int);
};

int sub_08004858(struct Unk_08004604 *, int);

int sub_08004604(struct Unk_08004604 *a, int b)
{
    int ret;
    int i;

    a->unk4 = a->unk2;
    a->unk5 = a->unk3;
    sub_08004858(a, b);
    ret = a->unk8(a, a->unk1);
    for (i = 0; i <= a->unk0; i++)
    {
        if (i == a->unk1)
            a->unk8(a, 0xFFFF);
        a->unk8(a, i + 0x100);
    }
    a->unk8(a, 0x1FF);
    return ret;
}

/* plus, in kururin.toml [renames]:  0x0801C734 = "_call_via_r2"   # libgcc: bx r2; nop */
```

### `sub_08013040`

```c
extern void *sub_08002528(u8 layer);

// A sprite entry handed out by sub_08002528: GBA OAM attributes 0-2 (attr0's low byte
// is Y, attr1's low 9 bits are X, attr2 holds tile and palette).
struct OamEntry {
    union {
        u32 attr01;
        u8 y;
    } a;
    u16 attr2;
};

/**
 * Draws `value` in decimal with 8x16 sprites (shape bit set in attr0, two tiles per
 * digit from tile 0x40), left to right from `x`, without leading zeros.
 */
void sub_08013040(s32 x, s32 y, u16 value, s32 palette)
{
    u32 n = value;
    u32 divisor = 10000;
    s32 started = FALSE;

    do {
        s32 digit = 0;

        while (n >= divisor) {
            digit++;
            n -= divisor;
        }
        if (digit != 0 || started) {
            struct OamEntry *oam;

            started = TRUE;
            oam = sub_08002528(3);
            if (oam != NULL) {
                oam->a.attr01 = ((x << 16) & 0x01FF0000) | 0x8000;
                oam->attr2 = (palette << 12) | (digit * 2 + 0x40);
                oam->a.y = y;
            }
            x += 8;
        }
        divisor /= 10;
    } while (divisor >= 10);

    {
        struct OamEntry *oam = sub_08002528(3);

        if (oam != NULL) {
            oam->a.attr01 = ((x << 16) & 0x01FF0000) | 0x8000;
            oam->attr2 = (palette << 12) | (n * 2 + 0x40);
            oam->a.y = y;
        }
    }
}

/* plus, in kururin.toml [renames]: 0x0801C8D0 = "__udivsi3"  (libgcc order: _call_via_r0..r14, __divsi3, __div0, __modsi3, __udivsi3, __umodsi3), so that `divisor /= 10` links to the existing routine */
```

### `MPlayContinue`

```c
#include "m4a.h" /* struct MusicPlayerInfo, ID_NUMBER, MUSICPLAYER_STATUS_PAUSE */

void MPlayContinue(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->status &= ~MUSICPLAYER_STATUS_PAUSE;
        mplayInfo->ident = ID_NUMBER;
    }
}
```

### `MPlayFadeOut`

```c
#include "m4a.h" /* struct MusicPlayerInfo, ID_NUMBER, FADE_VOL_SHIFT */

void MPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->fadeOC = speed;
        mplayInfo->fadeOI = speed;
        mplayInfo->fadeOV = (64 << FADE_VOL_SHIFT);
        mplayInfo->ident = ID_NUMBER;
    }
}
```

### `sub_080016E0`

```c
void sub_080016E0(u16 *map)
{
    u32 width, height;
    int x, y, i, j, k;
    u32 *dst;
    u16 *src;
    u16 *row;
    u32 bits;
    u32 color = 0xE0000000;

    width = *map++;
    height = *map++;
    for (y = 0; y < height; y++)
    {
        row = map + y * width;
        for (x = 0; x < width; x++)
        {
            u32 t = *row++ & 0x3FF;
            if (t == 0xFB || t == 0xFC)
                goto found;
        }
    }
    y = 0;
    x = 0;
found:
    x -= 16;
    if (x < 0)
        x = 0;
    y -= 32;
    if (y < 0)
        y = 0;

    for (i = 0; i < 64; i++)
    {
        if (y >= height)
        {
            src = map + y * width + x;
            dst = (u32 *)(0x060174A0 + (i / 8) * 0x100 + (i % 8) * 4);
            for (k = 0; k < 8; k++)
            {
                *dst = 0;
                dst += 8;
            }
        }
        else
        {
            src = map + y * width + x;
            dst = (u32 *)(0x060174A0 + (i / 8) * 0x100 + (i % 8) * 4);
            for (k = 0; k < 8; k++)
            {
                bits = 0;
                for (j = 0; j < 8; j++)
                {
                    u32 t = *src++ & 0x3FF;
                    bits >>= 4;
                    if (t >= 1 && t <= 0x9F)
                        bits |= color;
                }
                *dst = bits;
                dst += 8;
            }
        }
        y++;
    }
}
```

### `ReadSram_Core`

```c
/* The SDK form. Matches with the agb_sram module at -O (its configured cflags); make
   compare -> kururin.gba: OK with old_agbcc and with agbcc. (At -O2 only a contrived
   `n = size - 1; if (size != 0) do ... while (n-- != 0);` matched: the flags were wrong,
   not the C.) */
void ReadSram_Core(const u8 *src, u8 *dest, u32 size)
{
    while (--size != -1)
        *dest++ = *src++;
}
```

### `WriteSram`

```c
/* REG_WAITCNT and WAITCNT_SRAM_* are in include/gba/io_reg.h */
void WriteSram(const u8 *src, u8 *dest, u32 size)
{
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    while (--size != -1)
        *dest++ = *src++;
}
```
