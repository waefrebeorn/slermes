<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# The Water-Cup Parity Oracle

**Status:** design + reference implementation (v1) + **project-wide framework (v2)**.
**Problem it solves:** the existing oracle (`tests/oracle/`) diffs *output*
fixtures. That is gameable: a C port can return a hardcoded `json_object`
(or a copy of the expected vector) and pass every fixture while doing *none* of
the real work. The parity scanner (`slermes_parity_battleground.py`) is even
weaker — it never executes anything, so a `/* PoP: */` annotation on an empty
`{ return json_object; }` body closes the gap.

The water-cup oracle makes parity *behavioral and path-dependent*, so faking
the output without flowing water through the real control structure fails.

## Metaphor

A function is a **cup**. Input is **water** poured in. As water flows through
the function's real control structure it carves a **shape** — the ordered
sequence of branch decisions, loop iterations, recursion steps, and the
commit-relevant intermediate-value hashes. Two cups (Python + C) are
**parity-matched** iff, over a stream of inputs, they carve the *same shape*.

A cheater who only fakes the *output* carves a *different shape*: a stub that
`return json_object` has no branch/loop/recursion trace, so its cup is empty
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
declared result. You cannot `return json_object` (or a copied vector) *and*
reproduce the branch/loop/recursion trace of the real algorithm, because that
trace is a function of the input in a way a stub does not compute.

## Reference implementation (this directory)

- `tests/watercup/watercup.h` — C tracer API (`WCUP_PUSH`, `WCUP_BRANCH`,
  `WCUP_LOOP`, `WCUP_VAL`, `WCUP_RECURSE`) + `wcup_finalize` returning the
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
| hardcoded `return json_object` | passes if fixture expects `{}` | **fails**: empty trace-cup |
| copied expected vector | passes on that fixture | **fails**: trace diverges on the *stream* |
| right output, wrong path | **passes** (the gap) | **fails**: branch/loop trace differs |
| right output, invariant-violating | **passes** | **fails**: equivalence-cup |

The only way to make the cup match is to **execute the same algorithm** — which
is exactly what a faithful port does. There is no shortcut.

## v2 — Project-wide framework (language-agnostic)

The water-cup is **not Python-specific**. The cup is a byte stream of
`B<depth>:<0|1>` / `L<depth>` / `R<depth>` / `r<depth>` / `V<hash>` events folded
with FNV-1a. Any source language translated to C11 — Python, TypeScript, Rust —
carves the **same shape** when the logic is identical. A stub that returns a
constant cannot reproduce the real trace, so the cup never matches.

This is what makes it scale "project wide": instead of 600 hand-built demos, a
**generic registry-driven engine** runs every gate.

### Layout
```
tests/watercup/
  watercup.h          # C tracer (no-op under WCUP_DISABLED)
  watercup_py.py      # Python tracer, byte-identical FNV-1a
  wc_run.py           # GENERIC engine: build each gate, run C + LIVE twin, diff cups
  gates/<name>/
    gate.json         # {name, subsystem, c_src, c_fn, py_module, py_fn, inputs}
    harness.c         # traced C copy of the ported fn (+ main dispatch)
    twin.py           # imports LIVE Python twin, traces SAME branch ladder
  run_watercup_all.sh # CI deep-gate (fail-closed)
```

### Adding a gate (no code in the runner)
1. `mkdir tests/watercup/gates/<fn>`
2. Write `gate.json` (name the LIVE python module + fn, list a correlated input
   walk; optional `cflags`/`extra_src` to link the real ported `.c`).
3. Write `harness.c` — a traced copy of the C11 port (or link the real `.c`
   via `extra_src` so the cup traces shipped code), with `main` dispatching
   `<c_fn>` over `argv`. Print EXACTLY one cup hash per input on stdout —
   any extra output (results, labels) breaks the line diff.
4. Write `twin.py` exposing `run(inputs) -> [cup, ...]`, tracing the **same
   branch ladder in the same order** as the C harness (so divergence = real
   logic difference, not tracing convention).

`wc_run.py` builds (with stale-binary detection), runs, and diffs
automatically. The `watercup_all` port in `tests/oracle/registry.json` makes
`run_oracles.sh` dispatch it in CI alongside the diff oracle.

### Runner flags (v3)
```
python3 tests/watercup/wc_run.py                    # all gates, fail-closed
python3 tests/watercup/wc_run.py <gate> [<gate>...]  # selected gates
python3 tests/watercup/wc_run.py --list             # gate names
python3 tests/watercup/wc_run.py --verbose <gate>    # per-gate detail
python3 tests/watercup/wc_run.py --json             # machine-readable report
python3 tests/watercup/wc_run.py --coverage          # subsystem coverage map
python3 tests/watercup/wc_run.py --force             # rebuild all harnesses
```
Every run appends per-gate pass/fail to `tests/watercup/build/results.jsonl`.
Builds are cached: a harness recompiles only when its sources, `watercup.h`,
or `gate.json` are newer than the binary (the #1 historical source of false
REDs was a stale binary).

### Tracer-convention pitfalls (learned the hard way)
1. **Rung order is the contract.** The cup encodes *which rung fired, in
   position order*. A mismatch can be a real FAP **or** a tracing-convention
   drift (twin traces the same set in a different order). Always verify
   fixability: align the twin's ladder to the C harness's; if the cup then
   matches, the drift was convention (fix the twin). If it still mismatches,
   you found a real divergence.
2. **V events must exist on both sides or neither.** `WCUP_VALSTR` in C with
   no matching `c.valstr` in the twin (or vice versa) mismatches every cup.
   For pure branch-ladder gates, emit V events on NEITHER side.
3. **Harness stdout is the cup stream.** Print one 16-hex cup per input and
   nothing else — route results/diagnostics to stderr.
4. **Boolean fixture fields**: reading a JSON boolean via `json_get_num`
   returns the default (0) — use `json_bool_value`. Same class of bug as the
   loops oracle's `--times` fixture.

### Current gates (v3 runner, all GREEN as of )

| gate | subsystem | cups | what it pins |
|------|-----------|------|--------------|
| `billing_format_money` | agent | 13 | money formatting (Decimal vs C rounding) |
| `billing_parse_money` | agent | 10 | money parsing ladder |
| `cch_status_code_from_value` | agent | 16 | status-code mapping |
| `format_size` | tools | 12 | byte sizes B..TB (GB-cap gap fixed) |
| `loops_format_interval` | hermes_cli | 13 | h/m/s assembly, `0s` fallback |
| `loops_parse_loop_args` | hermes_cli | 13 | 7-rung /loop argument parser |
| `model_normalize_detect_vendor` | hermes_cli | 10 | vendor detection incl. empty-guard |
| `prompt_cache_scope` | agent | 6 | rotation-stable scope (#79017): memo, lineage, fallback |
| `repetition_guard` | agent | 5 | repetition-loop guard (#86581): fail-open, fast+general paths |
| `registry_validate_args` | tools | unit | registry arg validation contract |
| `subscription_view_format_tier_row` | agent | 7 | tier-row rendering |
| `tool_dispatch_retry` | tools | unit | retry classification contract |
| `tool_idempotency` (+persist) | tools | unit | idempotency contract |
| `tool_result_classification` | tools | unit | result classification contract |
| `usage_pricing_format_cost_label` | agent | 14 | cost labels (rounding parity) |
| `usage_pricing_normalize_anthropic_model` | agent | 8 | Anthropic model names |
| `usage_pricing_normalize_bedrock_model` | agent | 9 | Bedrock model names |
| `watercup_json_escape` | web | unit | `<0x20` escape (C11-vs-C11 drift caught) |
| `watercup_web_auth` | web | unit | auth allowlist vs LIVE PUBLIC_API_PATHS |

Earlier RED findings (rounding FAP, empty-string guard, auth allowlist,
control-char escape) were fixed after the cup flagged them; the gates stay
as regression pins. Run `python3 tests/watercup/wc_run.py --coverage` for
the live subsystem map, and `--json`/`--verbose` for machine/human detail.

A port is parity-closed only when **both** the diff oracle AND the water-cup say
MATCH. The cup is the deep gate that catches the "right output, wrong path" class.
