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

1. **GAP → PORT conversion: measured, and it is not a sweep.** 13,300 sentinels;
   4,097 name a function whose symbol exists in the port (exact or unambiguous
   suffix alias, `stub_shadowed` excluded). `scripts/convert_gap_gate.py` tries the
   three call shapes that take one `const char*` (long / bool / string) and keeps a
   conversion only when the cups equal the live twin AND they discriminate.
   Result on the first 25: 23 never matched a shape (the port functions take
   structs, several arguments, or no argument at all), and 2 that DID match — a
   config getter and a model-id lookup — were constant-answer functions: one cup
   for five inputs, i.e. zero evidence. Those two are back to their sentinels, and
   the acceptance rule now requires `len(set(cups)) > 1`. Net yield of the
   mechanical sweep: **0**. `libcron_next_run` is the one real conversion this turn,
   and it took a hand-written twin plus typed fixtures — that is the shape of the
   work: per-gate design, not a batch job.
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

### Addendum 2 — the largest gap, measured, and what can actually be filled

**GAP (13,300) is where the volume is, and it is not mechanically closable.**
8,451 of those gates name a (module, function) that a `PoP:` annotation in `src/`
claims to implement; 7,342 point at a symbol the built port exports; 3,247 even
have a signature the converter can bind (one `const char*`, or an out-buffer),
after dropping symbols that serve several gates at once (`cmd_mcp` is claimed by 16
gates — a dispatcher, not the function).

Automated conversion over a filtered batch of 80 of the best-looking candidates:
**0 converted.** The reason is uniform and structural: the live twin returns no
comparable value. `port__hermes_cli_banner_py___format_update_notice` and friends
take dictionaries, config objects or no argument at all, so `twin.call(input)` raises
for every probe. Their harness can feed a string; the Python function cannot accept
one. That is the same wall `libcron_next_run` climbed by hand: a twin that builds the
receiver and typed fixtures, plus a C side that emits the harness's branch ladder.
Any claim that these 13k gates can be swept is false; the work is per-gate design.

**MISSING (776) was two different defects and is now triaged (measured):**

- **634 harnesses name a symbol no source defines** — not a failing test, a
  placeholder. All rewritten as GAP sentinels, so they read as gaps instead of
  build failures in every fleet run (`scripts/triage_missing_symbols.py --apply-gap`).
- **100 call a function the port implements but declares `static`** — real code
  with no evidence, because a gate cannot link to it. 25 were de-static'd where the
  file's own `PoP:` annotation names the gate's (module, function) and the C symbol
  matches the Python name. The other 75 are `str_in_list`-style annotations on a
  generic helper, i.e. the gate's PoP is wrong, not the port.
- **42 are defined in a source the binary does not build** — `src/plugins/platforms/qqbot/main.c`
  and `infrastructure_stubs.c`. Those gates can never link; they need the plugin
  object or their own copy of the semantics.

### Addendum 3 — the gate archive was losing 572 objects

`_watercup_object_archive()` ran `ar rcs` over the object list. GNU ar stores a
member by BASENAME, and 233 basenames collide in this tree (`cron.o` x4,
`ansi_strip.o` x2, ...): 4,929 objects became 4,357 members, so the archive held
one object per name and any gate whose port object lost the collision failed to
link with "undefined reference" to symbols the port plainly defines. Measured on
25 gates after a rebuild: 13 BUILD FAIL from this cause, 2 after switching to
`ar rcsP` (full-path member names). The archive now carries 4,929 members and
35,720 symbols, matching the union of the objects; symbol audits read the objects
directly rather than the archive for the same reason.

### Addendum 4 — the Windows TZ trap was live in the port

`tests/win_semantics/probe.c` measures the traps against the real port objects.
First run, Windows 11 + mingw64 + msvcrt:

```
PASS binary round trip             256/256 bytes identical
INFO text mode translation active   wrote 3 bytes in text mode, on disk 4
PASS rename replaces existing      target holds 'new'      (util_rename_replace)
INFO unlink of open file           denied, errno=13 -- the POSIX idiom has no equivalent
PASS errno on missing parent       errno=2, GetLastError=3
INFO IANA TZ ignored               TZ=America/New_York -> offset -3600 (EDT is -14400)
INFO TZ=UTC ignored                TZ=UTC -> local hour 9 (UTC hour was 14)
INFO clock quantised to the tick   QPC 1ms-sleep delta max=16138us
```

TZ=UTC being ignored means every `setenv("TZ","UTC"); tzset(); mktime()` idiom in
the tree is the machine zone wearing a UTC label. `datetime_tz_offset()` and
`datetime_format_tz()` were built on exactly that idiom, so a named zone rendered
the machine wall clock. Both now resolve through a table (26 zones, US/EU/AU DST
rules computed from the date, plus fixed offsets) and never touch TZ. Verified by
the probe: 6/6 zone cases exact, including both DST directions and the southern
hemisphere, and `Asia/Kolkata` at +19800.

What the probe still cannot settle, and why it matters: text mode is ACTIVE
(3 bytes -> 4 on disk), and 474 `fopen()` calls carry no `b`. Blanket binary is
still the wrong answer (C text mode strips `\r` on read, matching Python's
universal newlines), so the sites need classifying one by one.

### Addendum 5 — devil's advocate: what this work does not prove

Written as an attack on my own numbers, in the order a reviewer would make them.

**1. "PORT 8,326" is not parity.** PORT means the harness calls a symbol the port
exports. It does not mean the port agrees with Python: a gate can call the port and
still pass because both sides return a constant for the fixtures chosen. The only
gate-level defence is the NON-DISCRIMINATING verdict, and until a fleet-wide sweep
runs, the discrimination rate of the 8,326 is UNMEASURED. A 120-gate random sample
is being measured for exactly this reason; the sample rate is the honest bound, not
36.8%.

**2. The MISSING 776 -> 116 cleanup is bookkeeping, not capability.** 634 harnesses
that named a non-existent symbol were rewritten as GAP sentinels. That removes a
false build failure from every fleet run; it does not make one more function work.
Reporting it as "gaps filled" would be dishonest. The 25 de-static symbols are the
only part that changed what can be tested, and when those 25 gates were run against
the real port, most did not turn green: 2 still fail to link, the rest report
mismatch, twin blind, or non-discriminating fixtures. That is the honest state: the
symbols are now reachable, the fixtures are not yet meaningful.

**3. GAP 13,160 is the largest class and the one with the least progress.** (13,937 before 777 stale-oracle gates were separated out: those name a quarry file that no longer exists, so no fixture work can ever close them.) 7,215 of
those gates name a function that is not module-level in the quarry file (methods,
nested helpers, or a name that moved), 819 name a function with no value return, 868
need two or more arguments, 777 name a file that does not exist at that path any
more. That leaves **3,710 gates (26.6%) that are shape-convertible**: module-level,
one argument or none, returns a value, under 60 lines. They are ranked in
`.testbin/gap_candidates_ast.json`. Even those need the C side to exist and the
fixtures to discriminate, which is where the conversion rate gets measured, not
assumed.

**4. The 41 COPY gates still measure transcriptions.** 14 of them name a symbol the
port exports, so they can be deputised; 27 cannot without port work. Until then
their green means "two hand-written copies agree", which is the failure mode this
whole audit exists to catch.

**5. What is actually verified this session** (and therefore the only claims worth
repeating): the Windows-ignores-TZ bug is real, measured, and fixed with a table
that the probe and a new gate both exercise; `ar` was silently dropping 572 objects
from the gate archive, now full-path members with 4,929/4,929; ISO parsing no longer
mutates TZ on any build; binary IO round-trips; `util_rename_replace` replaces.
Everything else is still a gap with a number on it.

### Addendum 6 — the PORT number measured: 0 clean matches in 40

A random sample of 40 PORT-class gates was run against the current binary
(`.testbin/port_sample40.txt`, seed 7):

```
NON-DISCRIMINATING  26   every fixture yields one cup on both sides
TWIN BLIND           5   C cups vary, the twinned Python side is constant
MISMATCH / other     9   placeholder fixtures ("hello", "world", "test123", "NULL")
clean MATCH          0
```

So the honest reading of "PORT 8,327" is: **8,327 gates link the port; a random one
has a ~0% chance of producing usable evidence today.** The 36.8% figure is a
linkage statistic and must not be quoted as parity. What the sample does prove is
that the fleet's weakness is systemic and mechanical -- the fixtures never move the
answer -- so it is attackable by automation rather than by 8,000 hand edits:
`scripts/refixture_gates.py` runs each gate over a 56-input battery (empty,
whitespace, digits, sign, JSON, arrays, paths, URLs, emails, credential shapes,
unicode, long input, control chars), keeps the smallest input set whose cups differ
on BOTH sides, and refuses the gate when the C side varies but the twin does not.
The refusals are the real remaining work: a blind twin cannot be fixed by fixtures.

### Addendum 7 — three attempts to convert the GAP class, and why each failed

The GAP class (13,160) is the largest and the only one that would move the evidence
count. Three mechanisms were tried, all measured:

**1. Shape screening (AST).** Of 13,937 GAP gates, 3,710 are shape-convertible in
principle: module-level function, one argument or none, returns a value, under 60
lines. 7,215 name something that is not module-level in the quarry file, 819 have no
value return, 868 need two or more arguments, 777 name a file that is gone.

**2. Symbol resolution for those 3,710.** `scripts/resolve_gap_symbols.py` adds a
symbol when the Python name matches exactly ONE exported C symbol and the defining
`.c` can be located. On a 40-gate slice: 11 unique matches, 14 ambiguous (several
`adapter.py`-style symbols), the rest already mapped. Converting those 11:
**0 converted.** 7 were refused because the twin returns no comparable value (the
functions are side-effecting: `_enable_multiplex_log_routing` returns None), 4 by a
map-key/name mismatch in the converter. A string corpus cannot observe a function
whose only output is global state.

**3. Refixturing the non-discriminating gates.** `scripts/refixture_gates.py` on the
26 NON-DISCRIMINATING gates from the sample: **1 refixtured, 4 twin-blind, 21 kept
because the port returns one cup for all 54 inputs.** The fixtures were not the
problem for those 21 -- the functions are input-independent. Static confirmation
across the fleet: **708 of the 4,295 statically resolvable PORT gates call a Python
function that never reads its own argument** (16.5%). Those gates cannot discriminate
no matter what fixtures they get.

Net: the largest class cannot be converted, and the second-largest class is partly
vacuous by construction. What is left is per-function work -- write the C
implementation, write a harness that observes its real effect, and twin that effect
in Python. The port has roughly 1,800-2,000 gates that could carry discriminating
evidence today; everything else is either unimplemented, side-effecting, or
input-independent.
