# Water-Cup Parity Oracle — test harness

A behavioral, path-dependent parity oracle that is **harder to game than the
output-diff oracle**. The water-cup metaphor: a function is a cup; input is
water; as water flows through the real control structure it carves a *shape*
(the ordered trace of branches, loops, recursion, and significant
intermediate values). Two cups (Python + C) are parity-matched iff they carve
the same shape over a stream of inputs.

## Files

- `watercup.h` — C tracer (`WCUP_BEGIN/_END/_BRANCH/_LOOP_TICK/_RECURSE_IN/
  OUT/_VAL/_VALSTR/_FINALIZE`). Compiles to a no-op under `WCUP_DISABLED`.
- `watercup_py.py` — Python tracer, byte-identical FNV-1a fold + event
  alphabet (`B<d>:<0|1>`, `L<d>`, `R<d>`, `r<d>`, `V<hash>`).
- `match.py` — self-test: a faithful port vs a no-water CHEATER. The cheater
  returns identical strings (passes an output oracle) but carves a degenerate
  cup -> water-cup catches it.
- `demo_c.c` — C side of `format_cost_label`, traced with watercup.h.
- `match_c_vs_py.py` — proves Python and C carve IDENTICAL cups across the
  language boundary (the whole point of "matching water cups").

## Run

    python3 tests/watercup/match.py          # cheater demonstration
    python3 tests/watercup/match_c_vs_py.py  # Python-vs-C identity proof

## Design notes (anti-gaming properties)

- The cup is derived from *execution*, not from the declared result. You
  cannot `return json_object()` and also reproduce the branch/loop/recursion
  trace of the real algorithm.
- Three nesting cups: trace-cup (per-input signature), stream-cup (ordered
  signature sequence over a correlated input walk — catches gradient fakes),
  equivalence-cup (declared invariants satisfied on both sides).
- Canonical value encoding: doubles via `"%.17g"`, bools via `"True"/"False"`
  (so Python `valstr` and C `WCUP_VALSTR` carve identical bytes).

See `docs/watercup-parity-oracle.md` for the full design and the comparison
table vs the output-diff oracle.
