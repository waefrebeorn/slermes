# Water-Cup Parity Oracle — test harness

A behavioral, path-dependent parity oracle that is **harder to game than the
output-diff oracle**. The water-cup metaphor: a function is a cup; input is water;
as water flows through the real control structure it carves a *shape* (the ordered
trace of branches, loops, recursion, and significant intermediate values). Two cups
(Python + C, or any two translations) are parity-matched iff they carve the same
shape over a stream of inputs.

**Language-agnostic:** the cup is a byte stream folded with FNV-1a. Any source
translated to C11 — Python, TypeScript, Rust — carves the same shape when the
logic is identical. A stub that returns a constant cannot reproduce the real trace.

## Files

- `watercup.h` — C tracer (`WCUP_BEGIN/_END/_BRANCH/_LOOP_TICK/_RECURSE_IN/OUT/_VAL/_VALSTR/_FINALIZE`). No-op under `WCUP_DISABLED`.
- `watercup_py.py` — Python tracer, byte-identical FNV-1a fold + event alphabet.
- `wc_run.py` — **generic project-wide engine.** Builds every gate under `gates/`, runs the traced C harness + the LIVE Python twin, diffs cups. Fail-closed.
- `gates/<name>/` — one gate per audited function: `gate.json` + `harness.c` (traced C copy) + `twin.py` (LIVE Python twin tracing the same ladder).
- `audit_web_auth.*` / `audit_json_escape.*` — the two WEB-system deep-gates (CI-wired in `tests/oracle/registry.json`).
- `demo/` — the original cheater-vs-faithful demonstration (`match.py`, `match_c_vs_py.py`, `demo_c.c`, `watercup_py.py`).

## Run

    python3 tests/watercup/wc_run.py                 # all gates
    python3 tests/watercup/wc_run.py <gate_name>    # one gate
    python3 tests/watercup/wc_run.py --list         # list gates
    bash tests/oracle/run_watercup_all.sh            # CI deep-gate (fail-closed)

## Adding a gate (no runner changes needed)

1. `mkdir tests/watercup/gates/<fn>`
2. `gate.json` — `{name, subsystem, c_src, c_fn, py_module, py_fn, inputs}`
3. `harness.c` — faithful traced copy of the C11 port, `main` dispatches `<c_fn>`
4. `twin.py` — `run(inputs) -> [cup,...]`, imports the LIVE python source, traces
   the SAME branch ladder so divergence = real logic difference, not convention.

## Design notes (anti-gaming)

- The cup is derived from *execution*, not the declared result. You cannot
  `return json_object()` and also reproduce the real trace.
- Three nesting cups: trace-cup, stream-cup (correlated input walk), equivalence-cup.
- Canonical value encoding: doubles via `"%.17g"` so Python and C carve identical bytes.

See `docs/watercup-parity-oracle.md` (v2 section) and `docs/WORK_DASHBOARD.md`.
