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

## Failure modes (fixed — do not regress)

These were live bugs that made the fleet lie about its own results. Each is
guarded by a test-shaped check; keep them.

**Empty selection returned SUCCESS.** `wc_run_parallel.py` filtered by
`--port-only` / `--gap-only` / `--legacy-only` (or a typo'd gate name) down to
zero gates and then `return 0`. A CI gate wired to it went green having
verified nothing. Now exits 2 with the reason. Reproduce:

    python3 tests/watercup/wc_run_parallel.py --port-only <a non-port gate>   # exit 2

**A SystemExit in one gate killed the whole run.** `SystemExit` derives from
`BaseException`, so the per-gate `except Exception` sailed past it. Two paths
reach it: a gate over a CLI entry point (`py_fn: "main"`, 20 of them) calling
`argparse.parse_args()`, and a twin whose *module-level* code exits before the
runner's guard is even installed. Both runners now catch `SystemExit`
specifically — **not** `BaseException`, so `KeyboardInterrupt` still aborts the
run on Ctrl+C.

**An unbounded gate hung the run forever.** `ProcessPoolExecutor` had no
timeout; one wedged worker blocked the report indefinitely. Now bounded by
`WATERCUP_GATE_TIMEOUT` (default 120s), recorded as a `TIMEOUT` result. The
pool is shut down with `wait=False, cancel_futures=True` on abandon — the
`with` form of the executor blocks until in-flight gates finish, which measured
66s for 4 gates on 2 workers against a 3s timeout.

Caveat, stated plainly: a worker blocked inside a C call cannot be killed from
Python. `future.cancel()` is a no-op once the worker has picked the gate up, so
a truly wedged worker runs until the process exits — it just stops blocking the
report. A hard guarantee needs per-gate subprocess isolation, which costs a
process spawn per gate and is not worth it for a fleet that is ~0.1s/gate when
healthy.

## Operator-state isolation (`HERMES_HOME`)

Both runners export `SLERMES_HOME` / `HERMES_HOME` to a throwaway dir
(`build/home_watercup/`) before any LIVE module import — the same convention
`tests/oracle/runners/run_oracle.sh` uses. This is required, not cosmetic:
several LIVE twins own global operator state whose mutation escapes the test
process. `agent/estop.py` writes the ESTOP sentinel at `$HERMES_HOME/ESTOP`,
and `hermes pause` reads that same file — so a watercup run launched from
inside a live Hermes session used to engage the real pause on the operator's
own gateway, and every subsequent gateway turn answered with the
"Hermes is paused" notice.

To trace the real home instead (e.g. to debug a sentinel-path gate):

    WATERCUP_REAL_HERMES_HOME=1 python3 tests/watercup/wc_run.py <gate>

The sandbox is only effective for gates whose traced value embeds the home
path; those cups legitimately change when the home changes. Per-gate
PASS/FAIL verdicts are unaffected — the C harness for those gates is a
GAP-sentinel constant, so it never matched anyway.

## DO NOT run the full fleet from inside a Hermes session

Several gates import LIVE entry points **in-process**: `real__gateway_run_py__main`,
`real__hermes_cli_container_boot_py__main`,
`real__agent_transports_hermes_tools_mcp_server_py__main`,
`real__tools_neutts_synth_py__main`, `real__cli_py__main`, and friends. Those
bind ports, spawn servers, and register signal handlers. Run inside an agent
session this crashes the host (observed: five WSL deaths).

Run the fleet from a plain shell, never from an agent:

    cd /home/wubu/hermes-agent-dev/slermes
    /home/wubu/hermes-agent-dev/venv/bin/python tests/watercup/wc_run.py

To iterate on a subset from inside a session, exclude the entry-point gates.
They are the `py_fn: "main"` set:

    python3 tests/watercup/wc_run.py --list | grep -vE \
      'gateway_run_py__main|container_boot|mcp_server_py__main|neutts_synth|cli_py__main|hermes_cli_main_py__main|death_supervisor|scan_venv_blockers|uninstall_py__main|update_restart_recovery|windows_ssh_runtime|hermes_cli_stderr_timestamp|cgroup_cleanup|acp_adapter_entry_py__main'

If a run behaves oddly, check for orphans left by a crash — a forkserver
survives and holds stale argv:

    ps aux | grep -E 'forkserver|wc_run' | awk '{print $2}' | xargs -r kill -9

## Entry-point isolation (why the full fleet is now safe to run)

20 of the 22,604 gates declare `py_fn: "main"`, meaning they execute a REAL
CLI/server entry point. Several of those targets are process-level programs:

| module | why it is dangerous in-process |
|---|---|
| `gateway/run.py` | binds ports, runs a polling loop |
| `agent/transports/hermes_tools_mcp_server.py` | spawns/supervises child processes |
| `tools/mcp_death_supervisor.py` | process supervisor — kills things |
| `tools/neutts_synth.py` | spawns audio subprocesses |
| `hermes_cli/container_boot.py` | starts services |
| `cli.py`, `hermes_cli/main.py` | full CLI entry |

Imported by the runner these crashed the host — observed as five WSL deaths.

`run_cup_gate` now routes every `py_fn: "main"` gate to `_twin_driver.py` in a
**subprocess** with a timeout. The child inherits the sandbox env, so operator
state still lands in `build/home_watercup/`. Any failure — crash, hang,
`SystemExit` — comes back as a JSON `{"error": ...}` and is reported as a
failing gate. The worst a gate can now do is die in its own process.

Verified: `real__gateway_run_py__main`, the gate that killed WSL five times,
now returns `ERROR: No module named 'yaml'` as data while the runner completes
normally. The remaining 22,584 gates run in-process as before.

`WATERCUP_GATE_TIMEOUT` bounds both the isolated child and the parallel pool.

## Build: two-step compile + ccache

`build_harness` compiles each translation unit with `-c` to its own `.o`, then
links. A single `cc -o out a.c b.c` is a *link* step, and ccache refuses to
cache those (`No -c option found` → `called_for_link`), so the cold-build cost
of a 22.6k-gate fleet stayed entirely uncached. Splitting compile from link
makes every TU individually cacheable.

ccache is used automatically when on PATH (`shutil.which`), so a machine
without it is unaffected. Override the compiler with `WATERCUP_CC`:

    WATERCUP_CC=cc python3 tests/watercup/wc_run.py ...   # force bare cc

`.o` files are gate-prefixed (`<gate>.harness.o`, `<gate>.x0.json.c`) because
most harnesses share the basename `harness.c` and the same `extra_src`, which
would collide in the shared `build/` dir.

## Interpreter requirements

The fleet runs under whatever `python3` resolves to. The twins import LIVE
Hermes modules, some of which pull in PyYAML transitively (context compressor,
gate status). On an interpreter without it those gates fail with a bare
`ERROR: No module named 'yaml'` — indistinguishable from a real parity failure.

The runner now checks up front and names the fix. A venv with the deps:

    /home/wubu/hermes-agent-dev/venv/bin/python tests/watercup/wc_run.py ...

Bypass the check (to run the yaml-free subset deliberately):

    WATERCUP_SKIP_DEP_CHECK=1 python3 tests/watercup/wc_run.py <gates>

## Gates over CLI entry points (`py_fn: "main"`)

20 gates trace a real `main()`. Those call `argparse.parse_args()`, which reads
the *runner's* argv and calls `sys.exit(2)` on a missing required flag — so the
first such gate used to abort the entire run with
`error: the following arguments are required: --criteria`.

`run_cup_gate` now wraps every twin invocation in `_neutral_argv()` (bare argv
+ `ArgumentParser.exit` → `SystemExit`) and `_run_twin_inputs()` converts that
`SystemExit` to a `ValueError` — `SystemExit` is a `BaseException`, so the
per-twin `except Exception` would sail past it. The gate then traces an ordinary
`ERROR:SystemExit(2)` cup: visible in the report, never fatal to the fleet.

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
