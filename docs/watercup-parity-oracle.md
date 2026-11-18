<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# The Water-Cup Parity Oracle

**Status:** project-wide fleet — **22,603 gates** under `tests/watercup/gates/`,
run by the generic engine `wc_run.py` / `wc_run_parallel.py`.

**Problem it solves:** the diff oracle (`tests/oracle/`) compares *output*
fixtures. That is gameable: a C port can return a hardcoded `json_object`
(or a copy of the expected vector) and pass every fixture while doing *none*
of the real work. The parity scanner is weaker still — it never executes
anything, so a `/* PoP: */` annotation on an empty body closes the gap.

The water-cup oracle makes parity *behavioral and path-dependent*: faking
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

1. **Trace-cup (control-flow signature).** Record, in execution order:
   `B<depth>` per significant branch (T/F as 1/0), `L<depth>` per loop
   iteration, `R<depth>`/`r<depth>` per recursion entry/exit, `V<hash>` per
   significant intermediate value. The stream folds into one **cup hash**
   per (function, input).
2. **Stream-cup (response-surface coupling).** Feed a *correlated* input
   stream (fuzz walk / Markov step). The cup is the *ordered* sequence of
   cup-hashes — a faithful port matches step-by-step, because the gradient
   of the response surface is determined by the real algorithm.
3. **Equivalence-cup (invariant shape).** Behavioral invariants the water
   must satisfy on every input: idempotence, monotonicity, symmetry,
   token-conservation, bounds. Both sides must satisfy the same invariants.

**Anti-gaming property:** the shape is *derived from execution*, not from the
declared result. You cannot `return json_object` *and* reproduce the real
trace. The only way to make the cup match is to **execute the same
algorithm** — which is exactly what a faithful port does.

## Language-agnostic

The cup is a byte stream of events folded with **FNV-1a**. Any source
translated to C11 — Python, TypeScript, Rust — carves the same shape when
the logic is identical. Doubles encode via `"%.17g"` so Python and C carve
identical bytes. This is what lets the fleet scale project-wide: a generic
registry-driven engine runs every gate, not hand-built demos.

## Layout

```
tests/watercup/
  watercup.h          # C tracer (WCUP_* macros; no-op under WCUP_DISABLED)
  watercup_py.py      # Python tracer, byte-identical FNV-1a fold
  wc_run.py           # generic engine: build gate, run C + LIVE twin, diff cups
  wc_run_parallel.py  # parallel fleet runner (bounded workers, per-gate timeout)
  _twin_driver.py     # subprocess isolation for entry-point (py_fn: "main") gates
  gates/<name>/
    gate.json         # {name, subsystem, c_src, c_fn, py_module, py_fn, inputs}
    harness.c         # traced C copy of the port (or extra_src links the real .c)
    twin.py           # imports LIVE Python, traces the SAME branch ladder
  build/              # harness binaries + results.jsonl (never /tmp)
  demo/               # the original cheater-vs-faithful demonstration
```

## Run

```bash
python tests/watercup/wc_run.py                  # all gates, fail-closed
python tests/watercup/wc_run.py <gate> [...]     # selected gates
python tests/watercup/wc_run.py --list           # gate names
python tests/watercup/wc_run.py --coverage       # subsystem coverage map
python tests/watercup/wc_run.py --json           # machine-readable report
python tests/watercup/wc_run_parallel.py --port-only   # structural subset (fast)
```

`make parity` runs the `--port-only` structural subset as its watercup stage
(the full 22k sweep is the documented batch op: `--full`). Every run appends
per-gate verdicts to `tests/watercup/build/results.jsonl`; the parity
pipeline truncates it first so a stale run cannot poison the walkway stats.
Builds are cached: a harness recompiles only when its sources, `watercup.h`,
or `gate.json` are newer than the binary (stale binaries were the #1
historical source of false REDs). Compile and link are split so ccache can
cache every TU.

## Adding a gate (no runner changes needed)

1. `mkdir tests/watercup/gates/<fn>`
2. `gate.json` — name the LIVE python module + fn, list a correlated input
   walk; optional `cflags`/`extra_src` to link the real ported `.c`.
3. `harness.c` — traced copy of the C11 port (or link the real `.c` via
   `extra_src` so the cup traces shipped code), `main` dispatches `<c_fn>`.
   Print EXACTLY one 16-hex cup per input on stdout — results/diagnostics go
   to stderr; any extra stdout breaks the line diff.
4. `twin.py` — `run(inputs) -> [cup, ...]`, importing the LIVE python source
   and tracing the **same branch ladder in the same order** (so divergence
   = real logic difference, not tracing convention).

## Runner hardening (fixed — do not regress)

- **Empty selection ≠ success.** Filtering to zero gates exits 2 with the
  reason; a CI gate wired to it can never go green having verified nothing.
- **SystemExit is caught per gate** (not BaseException — Ctrl+C still
  aborts). CLI-entry twins calling `argparse` exit into a traced
  `ERROR:SystemExit(2)` cup, visible in the report, never fatal.
- **Per-gate timeout** (`WATERCUP_GATE_TIMEOUT`, default 120s) bounds wedged
  workers; the pool abandons with `wait=False, cancel_futures=True`.
- **Entry-point isolation.** The 20 gates with `py_fn: "main"` execute REAL
  CLI/server entry points (gateway run, MCP server, container boot, …).
  In-process they crashed the host (five WSL deaths observed); they now run
  via `_twin_driver.py` in a subprocess with the sandbox env. The worst a
  gate can do is die in its own process.
- **Operator-state isolation.** Both runners export `SLERMES_HOME` /
  `HERMES_HOME` to a throwaway dir (`build/home_watercup/`) before any LIVE
  import — twins like `agent/estop.py` write sentinels that otherwise engage
  the real operator's gateway pause. Override with
  `WATERCUP_REAL_HERMES_HOME=1` to trace the real home deliberately.
- **Interpreter dependency check.** Twins import LIVE modules that pull
  PyYAML transitively; the runner checks up front and names the fix instead
  of letting gates fail with a bare `No module named 'yaml'` indistinguishable
  from a parity failure. Bypass: `WATERCUP_SKIP_DEP_CHECK=1`.

**Do not run the full fleet from inside an agent session** — run it from a
plain shell. To iterate on a subset inside a session, exclude the
entry-point gates (the `py_fn: "main"` set; see `tests/watercup/README.md`
for the exclusion list).

## Tracer-convention pitfalls (learned the hard way)

1. **Rung order is the contract.** A mismatch can be a real FAP *or*
   tracing-convention drift. Align the twin's ladder to the harness's; if
   the cup then matches, the drift was convention (fix the twin). If it
   still mismatches, you found a real divergence.
2. **V events on both sides or neither.** `WCUP_VALSTR` in C with no matching
   `c.valstr` in the twin mismatches every cup.
3. **Harness stdout is the cup stream.** One 16-hex cup per input, nothing
   else.
4. **Boolean fixture fields**: `json_get_num` on a JSON boolean returns the
   default — use `json_bool_value`.

## Historical regression pins

The first project-wide gates (billing money format/parse, `format_size`,
loops parse/format, model vendor detection, prompt-cache scope, repetition
guard, registry arg validation, tier-row rendering, tool dispatch retry /
idempotency / result classification, usage-pricing cost labels + Anthropic /
Bedrock normalization, web auth allowlist, JSON control-char escape) were
each born from a real RED finding that was then fixed. They stay as
regression pins. The live fleet status is never hand-typed here — run
`wc_run.py --coverage` / `--json`, or read the Watercup line of any
`` sentinel after `make parity`.

## How it sits on the diff oracle

The water-cup is the **deep gate** layered on top of `tests/oracle/` (the
coarse output-diff gate). It catches the "right output, wrong path" class
the diff oracle can never see:

| Vector | Diff oracle | Water-cup |
|---|---|---|
| hardcoded `return json_object` | passes if fixture expects `{}` | **fails**: empty trace-cup |
| copied expected vector | passes on that fixture | **fails**: trace diverges on the stream |
| right output, wrong path | **passes** (the gap) | **fails**: branch/loop trace differs |
| right output, invariant-violating | **passes** | **fails**: equivalence-cup |

**A port is parity-closed only when BOTH the diff oracle AND the water-cup
say MATCH.**
