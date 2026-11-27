<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Gates

Every gate below is runnable from a clean checkout with the Windows toolchain
(`source build/winlibs-env.sh` first). Run them and read the exit code — do not
pipe gate output through `head`/`tail`, which corrupts exit status on this box.

## 1. Build health — the one to run first

```bash
python tests/verify_build_health.py            # sources + port-object gate + link
python tests/verify_build_health.py --no-link  # faster; skips `make -j4 all`
python tests/verify_build_health.py --regress  # compare against the saved baseline
python tests/verify_build_health.py --save     # write the baseline
```

`-fsyntax-only` proves a file parses; it does not prove symbols resolve. So the
gate also runs a real link and fails on `multiple definition` / `undefined
reference`. `HEALTH_JOBS` controls the worker count (4 is the safe maximum on
MSYS2 — 8 produces spurious failures from fork exhaustion, not real errors).

This gate now also runs the port-object check (below), so one command covers it.

## 2. Port-object gate

```bash
python tests/verify_port_objects.py
```

Parity is a claim about the **linked binary**. A `.c` file that is never named in
`build/objects.mk` cannot legitimately carry parity credit, so any port-style
file (`port_*.c`, `*_ports.c`, or a path with a `/ports/` segment) carrying a
`PoP:` annotation must either be linked or explicitly excused.

Excused files are listed in `tests/superseded_port_files.txt`. Regenerate it:

```bash
python scripts/audit_port_objects.py
```

Adding a new dead port file fails the build. Adding it to `objects.mk` is the
better fix — see `scripts/link_dead_ports.py`, which finds every unlinked twin
that links without a symbol collision.

## 3. Oracle suites

An oracle runs the real Python function and the C port over the same inputs and
compares outputs. Build the port object first.

```bash
bash tests/run_one_oracle.sh <name> <oracle.py> "" "<obj> [<obj> ...]"
bash tests/run_all_oracles.sh          # needs ORACLE_JOBS=1 on MSYS2
```

The object list is argument 4. Passing an empty string forces a full
project link (every `.o` in `src/` + `lib/`), which Smart App Control refuses
to run on this box — always pass the minimal object list. `tests/run_all_oracles.sh` carries the authoritative list
in its `DEPS` table. To converge a new oracle's object closure automatically
(link → read undefined refs → `nm` for definers → append → repeat, with the
SAC `-O0` hash substitution and stubs-last ordering):

```bash
python scripts/resolve_oracle_deps.py <name> [max_rounds]
```

A harness that cannot execute must be reported as an explicit skip with a
reason; "0 cases" is a failure, not a pass.

## 4. Slermes tests

```bash
bash tests/run_slv_tests.sh
```

## 4b. Watercup fleet interpreter

The trace-cup fleet imports LIVE Python twins that pull PyYAML transitively.
Neither the MSYS2 python nor the Hermes tools python has it; the quarry venv
does. Run the fleet with it (inside the MSYS2 env child so gcc resolves):

```bash
C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe tests/watercup/wc_run_parallel.py --port-only
```

The runner checks the interpreter up front and refuses with a named fix
rather than emitting misleading `No module named yaml` gate failures.
`make parity`'s watercup stage inherits whatever interpreter launched the
pipeline — launch it with the venv python for real watercup verdicts.

## 5. Parity truth

```bash
python scripts/parity_truth.py     # writes live_parity_scan.json
python scripts/gen_parity_walkway.py  # regenerates every PARITY:AUTO block
```

Numbers inside `` sentinels are generator-owned. Never
hand-edit them; change the generator or the scan instead.

Four counting systems exist and they disagree by design:

- the scanner (feature-level, PoP-annotated) — what `parity_truth.py` reports;
- `tests/parity_reality.py` (body-level, stricter — a `(void)x; return 0;` body
  is a placeholder, while the scanner credits the annotation);
- `tests/check_gate_evidence.py` (gate-level, measured — how many gates could
  actually discriminate; see section 6);
- any older hand-written figure in an archived document.

Quote which one you mean. The scanner counts annotations; only the third counts
anything the binary did.

## 6. Gate evidence — the workflow guard

```bash
python tests/check_gate_evidence.py --report   # print the classes, never fail
python tests/check_gate_evidence.py            # ratchet check (make workflow-check)
python tests/check_gate_evidence.py --save     # adopt current state as the baseline
```

`make workflow-check` runs it after the reality scoreboard and build health, so a
commit that makes the fleet look better without making a gate stronger fails.

The number it ratchets is **EVIDENCE**, not gate count:

```
EVIDENCE = PORT - (Python fn never reads its input)
                - (Python fn returns a tuple the harness cannot compare)
                - (C symbol ignores every parameter — a stub)
```

It is an upper bound on usable evidence, and it is deliberately conservative in
both directions: it can only fall if a gate is broken or a debt class
(COPY/SHIM/MISSING/BROKEN) grows, and it can only rise by making a gate real.

What each class means when you read a run:

- **PORT** — the harness calls a symbol the port exports. Necessary, not sufficient:
  a 40-gate random sample produced 26 NON-DISCRIMINATING and 0 clean matches, so
  never quote PORT as parity.
- **GAP** — a sentinel: the function has no C implementation. Real work, not a test.
- **STALE** — the gate names a quarry file that no longer exists. Unclosable.
- **COPY** — the harness defines the port's own function locally; the cups measure
  the transcription. Deputise it (`scripts/deput_copy_harness.py`) or the gate is
  decoration.
- **MISSING / BROKEN** — the gate cannot link or cannot run.

Two scripts exist to move gates between classes, and both are evidence-gated — they
keep a change only if the cups actually match live Python afterwards:

```bash
python scripts/refixture_gates.py --from-provenance --limit 40 --apply  # fixtures that move the answer
python scripts/resolve_gap_symbols.py --gates <gate>...                  # find the C symbol for a GAP
```

Neither can rescue a side-effecting function (no comparable return value) or an
input-independent one. Those need a hand-written harness that observes the real
effect and a twin that observes the same effect in Python. That is the only route
left for the bulk of the GAP class.

## Known environmental limits

- Smart App Control refuses to run freshly linked images containing
  `lib/libhash/hash.o` (SHA-256), `lib/libregex/hermes_regex.o`, or
  `src/agent/auxiliary_client_runtime.o`. **Reproducible workaround for
  hash:** `tests/ensure_oracle_deps.sh` builds `.testbin/hash_O0.o`
  (lib/libhash/hash.c at `-O0`) — the -O0 image is accepted where the -O3
  image is refused (verified at the latest run with a SHA-256 probe). DEPS entries
  that need SHA-256 name `.testbin/hash_O0.o`; the runners auto-build it.
  For other blocked dependencies, harnesses link a local no-op stub for the
  *dependency* — never for the code under test — and name the real object
  in DEPS.
- The sandbox strips `VAR=value cmd` prefixes. Pass per-invocation config as
  a script argument, never as an environment prefix.
- `VAR=... make` style invocations that need env must `export` inside the
  child shell.

## Gate integrity rules (enforced by the runner, not by memory)

A gate is evidence only when the number it prints came out of the port. Three
shapes break that promise and all three used to look green:

1. **The gate ships its own copy of a port symbol.** `shims.c` linked ahead of
   the prebuilt objects wins with `-Wl,--allow-multiple-definition`, so the cups
   measure the gate. `agent_markdown_tables_is_table_divider` did exactly this;
   a negative control (shim forced to `return true`) moved every cup, proving the
   port was never called. **Rule:** `wc_run.build_harness` now nms every
   gate-local object (anything under `tests/watercup/`) against the port symbol
   set and fails closed with `GATE SHADOWS THE PORT`. Vendored libs
   (`lib/libjson/json.c`) are exempt: the runner drops the matching `.o`.
2. **The harness calls a symbol the port never had.** It links only because a
   shim supplied it. Same refusal catches it when the shim is gate-local; the
   audit script `scripts/audit_gate_shims.py` classifies every remaining shim as
   FAKE / SHADOWED / DEAD and exits non-zero on the first two.
3. **The twin never receives the input.** A dotted/property `py_fn`
   (`Cls.available`) used to resolve to nothing -> `ERROR:TypeError` for every
   input, one constant cup that read like five port defects. Now: dotted paths
   are walked, a `property` resolves to its getter, `TwinBase.call()` is the hook
   for building a real receiver, and the runner reports `TWIN BLIND` (C cups vary,
   twin answered one) instead of N phantom mismatches.

## Out-of-domain inputs carry evidence, not silence

`"NULL"` is a C null pointer; Python `None` is a different value and a function
may legitimately raise on it. A bool/str cup cannot express "raises". A gate may
declare

```json
"inputs_out_of_domain": ["NULL"],
"out_of_domain_raises": "TypeError",
"out_of_domain_reason": "split_table_row(None) raises TypeError ..."
```

and the runner recomputes the twin's cup for that input every run, requiring it to
equal `Cup(label).valstr("ERROR:TypeError")`. If the live function stops raising,
the exclusion is stale and the gate fails closed. An exclusion without both
fields is a gate defect, not a pass.
