# The Water-Cup Parity Oracle

**Status:** design + reference implementation (v1).
**Problem it solves:** the existing oracle (`tests/oracle/`) diffs *output*
fixtures. That is gameable: a C port can return a hardcoded `json_object()`
(or a copy of the expected vector) and pass every fixture while doing *none* of
the real work. The parity scanner (`slermes_parity_battleground.py`) is even
weaker — it never executes anything, so a `/* PoP: */` annotation on an empty
`{ return json_object(); }` body closes the gap.

The water-cup oracle makes parity *behavioral and path-dependent*, so faking
the output without flowing water through the real control structure fails.

## Metaphor

A function is a **cup**. Input is **water** poured in. As water flows through
the function's real control structure it carves a **shape** — the ordered
sequence of branch decisions, loop iterations, recursion steps, and the
commit-relevant intermediate-value hashes. Two cups (Python + C) are
**parity-matched** iff, over a stream of inputs, they carve the *same shape*.

A cheater who only fakes the *output* carves a *different shape*: a stub that
`return json_object()` has no branch/loop/recursion trace, so its cup is empty
or degenerate and never matches the Python cup.

## Three nesting cups (deeper = harder to game)

1. **Trace-cup (control-flow signature).** Record, in execution order, a
   compact event stream:
     - `B<depth>` when a significant branch is taken (T/F as 1/0),
     - `L<depth>` per loop iteration,
     - `R<depth>` per recursion entry/exit,
     - `V<hash>` the hash of each significant intermediate value
       (arguments, loop accumulators, the compared operand of each branch).
   The whole stream is folded into one **cup hash** per (function, input).

2. **Stream-cup (response-surface coupling).** Feed a *correlated* input
   stream (a fuzz walk or a Markov step over the input domain). The cup is the
   *ordered* sequence of cup-hashes. A faithful port matches Python's hash
   sequence step-by-step — not just at isolated sample points — because the
   gradient of the response surface (how the trace changes as the input
   changes) is determined by the real algorithm, which a stub cannot fake.

3. **Equivalence-cup (invariant shape).** Assert *behavioral invariants* the
   water must satisfy on every input in the stream, independent of path:
   idempotence, monotonicity, symmetry, token-conservation, bounds. Both sides
   must satisfy the *same* invariants. A cheater that coincidentally matches
   some outputs but violates an invariant is caught.

**Anti-gaming property:** the shape is *derived from execution*, not from the
declared result. You cannot `return json_object()` (or a copied vector) *and*
reproduce the branch/loop/recursion trace of the real algorithm, because that
trace is a function of the input in a way a stub does not compute.

## Reference implementation (this directory)

- `tests/watercup/watercup.h` — C tracer API (`WCUP_PUSH`, `WCUP_BRANCH`,
  `WCUP_LOOP`, `WCUP_VAL`, `WCUP_RECURSE`) + `wcup_finalize()` returning the
  cup-hash hex string. Compiles to a no-op when `WCUP_DISABLED` is defined, so
  it is zero-cost in production builds.
- `tests/watercup/watercup_py.py` — Python equivalent tracer using
  `sys.settrace` + value hashing, emitting the identical cup-hash format.
- `tests/watercup/match.py` — given a fixture stream, runs BOTH tracers,
  compares trace-cup + stream-cup + equivalence-cup, emits
  `cups: MATCH | MISMATCH` with the divergence point.
- `tests/watercup/registry.json` — per-function invariant declarations
  (the equivalence-cup rules) and the fixture streams.

## How it sits on the existing harness

It is **contract C** layered *on top of* `tests/oracle/` (the diff oracle).
The diff oracle stays as the coarse gate; the water-cup is the deep gate that
catches FAPs the diff oracle misses (the "returns right JSON but wrong path"
class). A port is parity-closed only when BOTH say MATCH.

## What makes it "harder than the oracle"

| Vector | Diff oracle | Water-cup |
|---|---|---|
| hardcoded `return json_object()` | passes if fixture expects `{}` | **fails**: empty trace-cup |
| copied expected vector | passes on that fixture | **fails**: trace diverges on the *stream* |
| right output, wrong path | **passes** (the gap) | **fails**: branch/loop trace differs |
| right output, invariant-violating | **passes** | **fails**: equivalence-cup |

The only way to make the cup match is to **execute the same algorithm** — which
is exactly what a faithful port does. There is no shortcut.
