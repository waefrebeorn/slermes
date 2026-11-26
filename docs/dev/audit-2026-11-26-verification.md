# Verification audit — 2026-11-26

Trigger: resume after a crashed session, then attack the gate fleet the way the
field does, not the way we had been doing it. Two research passes (port-verification
SOTA, Windows silent-divergence traps) plus probes against this tree.

## 1. What the fleet actually measures

`scripts/audit_gate_provenance.py` classifies all 22,603 gates by where the C cup
comes from — measured against a freshly rebuilt object archive, not a stale one:

```
GAP       13300   58.8%   real__ sentinel: harness returns the fn's name for every input
PORT       8303   36.7%   calls a symbol the built port exports
MISSING     776    3.4%   calls a symbol the built port does not export
UNKNOWN     182    0.8%
COPY         41    0.2%   harness defines the port's own function locally
BROKEN        1    0.0%
```

One gate is non-discriminating BY CONSTRUCTION: `gateway_acp_acp_get_id` calls
`acp_get_id(NULL)` for every input, so no fixture can ever make it discriminate
(counted by the audit's `input_used` check).

Only the PORT class is parity evidence today. That is the number to quote, not
"22.6k gates". The first run of this audit was wrong in both directions because it
nm'd the gate archive without rebuilding it: `backup_should_exclude` is `T` in
`src/hermes_cli/backup.o` and was reported absent. Both audits now rebuild when a
member object is newer (`scripts/_gate_audit_common.py`).

## 2. Three ways a gate measured the wrong thing (now refused by the runner)

| shape | evidence it was real | fix |
|---|---|---|
| gate-local shim wins the link | forcing `shims.c` to `return true` moved every cup of `agent_markdown_tables_is_table_divider` | `build_harness` nms gate-local objects against the port symbol set and fails closed (`GATE SHADOWS THE PORT`) |
| twin never receives the input | `AccountUsageSnapshot.available` (a property) resolved to nothing → one constant `ERROR:TypeError` cup for 5 inputs | dotted paths walked, properties unwrapped, `TwinBase.call()` receiver hook, `TWIN BLIND` verdict; gate now 5/5 |
| exclusion by silence | `NULL` fed to a fn whose Python raises | `inputs_out_of_domain` + `out_of_domain_raises`, re-verified every run against the twin's cup |

Also closed: 20 unlinked `shims.c` files deleted (dead trip hazards), `fmt_usd`
re-pointed at the port (5/5), unit twins get `PYTHONPATH`, no-oracle verdicts for
gates whose `py_module` is a label/.cjs/.c (23 gates previously printed
`TWIN FAIL (exit 1):` with nothing after it), traceback tails preserved, `tzset`
no-op on Windows CPython.

## 3. Research pass — how the field verifies a port

Sources and the concrete change each implies. Status is honest: ADOPTED means it
landed in this tree; OPEN means the leg is named with its next action.

| technique | who | status |
|---|---|---|
| upstream suite in differential mode (run the real tests on both sides) | MicroPython `run-tests.py` | OPEN — our 22.6k cups are synthesised; Hermes' pytest suite is an untapped free-twin source |
| inverted divergence assertions: every intentional difference is a file that must differ | MicroPython `tests/cpydiff` | OPEN — would stop `out_of_domain` exclusions rotting; the verified-`TypeError` check is a first step |
| approval testing: keep the serialised trace, diff it, cup as fast path | ApprovalTests | OPEN — a 64-bit mismatch still names nothing |
| live shadow execution against real receivers | GitHub Scientist | OPEN — `TwinBase.call()` is the mechanism; fixtures need real instances |
| stateful sequence generation (constructor → mutator → accessor) | Hypothesis stateful | OPEN — most gates are single calls |
| record-replay of real runs as fixtures | rr | OPEN |
| frozen clock / seeded RNG on both sides | libfaketime, `register_random` | OPEN — clock-dependent gates are the flakiest class |
| coverage-guided differential fuzzing, corpus merge + shrink | libFuzzer / OSS-Fuzz | PARTIAL — `scripts/gate_fuzz.py` exists; no merge/minimise pass |
| equivalence-modulo-inputs pruning | Le/Su PLDI'14 | OPEN — cheap version: delete unexecuted branches in a COPY harness and require the cup to move |
| non-optimising reference engine (C-vs-C oracle) | NoREC | OPEN |
| mutation testing of the port, require a gate to fail | SQLite | OPEN — the honest measure of gate coverage |
| fault injection (OOM at Nth allocation, I/O errors) | SQLite | OPEN — those paths have no Python twin at all |
| exported-symbol/signature manifest diff | libabigail | ADOPTED in spirit: the shadow refusal + `MISSING` class come from the port's symbol set |
| content-addressed cache keys for gate objects | Bazel | PARTIAL — cache invalidation on `watercup.h` was already added for exactly this reason |

## 4. Research pass — Windows silent-divergence traps

Ranked by "wrong answer, exit 0", not by crash. Probes measured in this tree:

- **Text mode.** 474 `fopen()` calls without a `b` flag vs 444 with one, and 4
  `_setmode`/`_O_BINARY` uses. Blanket binary is NOT the fix: C text mode strips
  `\r` on read, which matches Python's universal newlines, while binary preserves
  it and silently changes every config parser. The real task is classifying
  binary-data sites (images, zip, sqlite blobs) — OPEN, detector named below.
- **CRT identity.** `objdump -p slermes.exe` shows **msvcrt.dll**, not UCRT:
  `printf`/`snprintf` edge cases (e.g. `%s` with NULL), `errno` values and
  `_set_output_format` behaviour follow msvcrt. Any gate asserting formatted
  output on the C side is asserting msvcrt, so a Linux twin may differ for a
  reason that is not the port.
- **Three error spaces** (errno / GetLastError / WSAGetLastError), **rename over
  an existing file**, **unlink of an open file**, **case-insensitive names**,
  **MAX_PATH**, **`system()` via cmd.exe**, **CRT argv vs CommandLineToArgvW**,
  **sockets are not fds** (`select` nfds ignored, 64-slot fd_set), **mmap fixed at
  creation**, **per-thread locale**, **`clock()` is wall-clock**, **`TZ` ignores
  IANA names**, **DLL search order with 20 shipped DLLs**, **MSYS2 argv/env path
  rewriting**. Each is cited in the run log of this audit; none has a probe yet.
- The port already has the right primitive for one of them: `util_rename_replace`
  (`src/hermes_cli/fs_replace.c`) exists because `rename()` does not replace on
  Windows. That is the pattern to copy for the rest: one wrapper, one test.

## 5. Open legs, in the order they pay

1. **GAP → PORT conversion.** 13,303 sentinels; ~3,457 name a function whose
   symbol exists in the port (suffix match, collisions included). `scripts/convert_gap_gate.py`
   converts one gate only when a call shape compiles/links AND its cups equal the
   live twin. Pilot blocked: the first 9 candidates were not sentinels at all, so
   the candidate list must be filtered by `GAP` class from the provenance audit.
2. **COPY → PORT.** 41 measured (+182 UNKNOWN to triage): the harness carries its
   own implementation, so green means the transcription agrees with Python. The
   runner now prints `COPY HARNESS` so none of them can pass as ordinary evidence.
3. **MISSING (776).** Harnesses call symbols the built port does not export —
   either dead code (`static`), an unbuilt object, or a real gap. Triage per file.
4. **Windows probes.** One probe per silent trap in §4, starting with binary
   round-trip, errno mapping, and `util_rename_replace`.

### Addendum — one real conversion landed, one research-backed pattern

`libcron_next_run` was `ERROR: No such file or directory: croniter.py`: a bare
third-party `py_module` was resolved as a path inside the quarry, its fixtures were
the generic `hello`/`world` placeholders, and `croniter.get_next` is a method, not a
module function. All three fixed — bare names now `import_module` (with the quarry's
own venv `site-packages` on `sys.path`), inputs are `expr|from_epoch` probes
(`*/5 * * * *`, weekday ranges, `@daily`, an impossible Feb 30, an invalid
expression), and the twin emits the harness's three `WCUP_BRANCH` points itself
because `TwinBase.branch()` can only emit one. Result: **5/5 cups match live
croniter** — the port's `cron_next` now has real evidence behind it, not a sentinel.
