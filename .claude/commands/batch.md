---
description: Decompile the next N game functions (tools/next.py; default 10), verify, commit and push each, then report
argument-hint: "[N: batch size, 1-30, default 10]"
---

# /batch: decompile the next N functions

**Batch size N:** the argument is `$ARGUMENTS`. If it is a whole number from 1 to 30, that is
N. If it is empty, N = 10. If it is anything else (not a number, 0, or above 30), use N = 10 and
say so in the final report. Everywhere below, N means this number.

Read `CLAUDE.md` first; its hard rules apply to every step. This command authorises
decompiling the functions `tools/next.py` hands out, and nothing else.

**Work alone.** No subagents (no Agent/Task tool), no background workflows, no review rounds.
Run every step yourself, one function at a time.

**Never edit test files**: nothing under `tests/` (the harness, inputs, watch list, compiler
evidence, the lock), `scripts/rom_map.py`, `scripts/verify.py` or the `Makefile`. They are
locked (`tests/locked_files.txt`); `make test` fails if they change. If a function cannot pass
without changing one of them, it fails (see step 5).

## 0. Start

```sh
# baserom.gba is not in git: copy in your own dump (CLAUDE.md, Environment notes)
./setup.sh --no-ghidra        # only if tools/ or .venv/ is missing
export PATH="$PWD/tools/bin:$PATH"
git checkout main && git pull origin main
git status --short            # must be clean
python3 tools/next.py N       # the targets, in order (N = the batch size)
python3 tools/next.py --progress
```

Take the N functions `tools/next.py N` prints, in that order. Note each one's module (the
third column: `src/<module>.c`, `asm/nonmatchings/<module>/`; docs/modules.md) and its
`verify:` rule (`test`: executed by the inputs; `match`: never executed).

## Per function (repeat for each of the N)

1. **Draft.** `make expected`, then
   `asmlift asm/nonmatchings/<module>/<fn>.s --name <fn> --score-against expected/src/<module>.o`.
   If asmlift declines, `.venv/bin/m2c --target gba asm/nonmatchings/<module>/<fn>.s`. Read the
   `.s` yourself; the draft is not the answer.
2. **Readable C.** Rewrite the draft as idiomatic C in place of the function's `INCLUDE_ASM`
   line in `src/<module>.c` (address order), with real types and the existing structs of
   `include/variables.h` and `include/game.h`. Declarations go where CLAUDE.md says (Modules):
   used by this module only, in its `.c`; shared, in `include/game.h`; another module's
   function, through its `include/<module>.h`. Names (functions, globals, fields) only with evidence (CLAUDE.md rule
   6): KururinTAS (`tools/research/KururinTAS`: the `.wch` memory watch, `Misc/Game_analysis/`)
   and KuruTools (`tools/research/KuruTools`: data formats), or what the code itself does;
   record the evidence in a comment next to the name (`kururin.toml [renames]`/`[symbols]` or
   the header). No evidence: keep `sub_XXXXXXXX` / `gUnk_XXXXXXXX` / `unkXX`. Every new RAM/ROM
   global gets a `[symbols]` address; then `.venv/bin/python3 scripts/generate_asm.py`.
   Behaviour must be the same on every path, not just the tested ones (signedness, overflow,
   evaluation order, uninitialised reads). No inline asm. Skip a function whose C needs its own
   `.rodata` (string literals, `static const` tables): it fails at once (step 5, reason
   "needs .rodata").
3. **Score** (a hint, and the verdict for `match` functions):
   `python3 scripts/score.py <module> <fn>`.
4. **Verify:** `python3 scripts/verify.py <fn>`. It applies the rule:
   - executed by the inputs: `make test` and `make shift-test` must pass;
   - never executed: objdiff 100% (and `make test`), otherwise it stays asm.
   Only exit 0 (`VERIFIED`) counts.
   **Below 80% coverage** (`verify.py` prints it; `docs/coverage.md`): `make test` only saw part
   of the function, so before committing it must either byte-match (`scripts/score.py`: 100%)
   or get the review of `docs/audit.md`: compare the C with `asm/nonmatchings/<module>/<fn>.s` on
   every path the inputs never reach (signedness, overflow, comparisons, loop bounds,
   evaluation order, every branch) and fix any difference. Record the result in
   `docs/audit.md` (a row in the latest audit's table: "verified: byte-match", or "reviewed,
   not proven" with a section naming the uncovered paths and the in-game situation that
   reaches them) and commit that file with the function. No review, no commit: a difference
   found and not fixed within the attempts below fails the function (step 5).
5. **At most 3 fix attempts.** An attempt is one change to the C followed by step 4 (or
   step 3, for a `match` function that is not at 100% yet). After the third failed attempt,
   or at once for a reason no attempt can fix:
   - revert everything for this function: `git checkout -- . && git clean -fd src include`
     (and `.venv/bin/python3 scripts/generate_asm.py`);
   - add a row to the table in `docs/skipped.md`:
     `` | `<fn>` | <YYYY-MM-DD> | <rule> | <reason: what failed, with the first difference or the score> | ``
   - commit `docs/skipped.md` alone ("Skip <fn>: <reason>"), push it (below), and go on to
     the next function. Skipped functions stay out of `tools/next.py` from then on.
6. **Commit the success on its own**: the source, `kururin.toml`/header changes,
   `docs/coverage.md` if `make test` rewrote it, and `docs/audit.md` for a function below 80%
   coverage (step 4). Nothing else. Message:
   ```
   Decompile <fn>: <what it does, in one line>

   <verify rule> (<coverage>% covered); make compare: <byte-identical | does not match>.
   <evidence for any name you added>
   ```
   ending with the attribution lines the session asks for.
7. **Push to main** right after the commit, only after `verify.py` passed:
   `git push origin HEAD:main` (on a network error retry up to 4 times, waiting 2, 4, 8, 16 s).
   If the push is rejected, `git pull --no-rebase origin main`, rerun `verify.py <fn>`, push.

## End of the batch

After the Nth function (succeeded or skipped):

1. `python3 tools/next.py --progress` and the summary of `docs/coverage.md`.
2. Append one section to `docs/progress.md`:
   ```
   ## <YYYY-MM-DD>: /batch N

   - converted (count): `fn` (rule, coverage), ...
   - skipped (count): `fn` (reason), ...
   - total: X/356 functions in C (game: Y/285)
   - coverage: <functions executed> of <all> functions, <bytes>% of instruction bytes
     (docs/coverage.md)
   ```
   X is the "all C-module functions" count of `tools/next.py --progress`.
3. Commit `docs/progress.md` ("Batch progress: X/356"), push to main as above.
4. **Stop.** Report the converted and skipped functions and the totals in a few lines. Do not
   start another batch.
