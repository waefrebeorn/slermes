# Functionality mapping and matching — devil's advocate review

## Evidence ladder (what counts as "ported")

1. A **dispatched C implementation** runs in a harness with an oracle that
   extracts the LIVE Python body and compares outputs/traces. Green = parity
   for the covered cases only.
2. A **real production caller** in `src/` invokes the C function. Without one,
   the port is body-verified but not integrated.
3. A **PoP annotation** or a PORTED count is not evidence: the scanner matches
   comments. Empty stubs move the same counters as working code.

Anything below level 1 must be reported as unverified, never as parity.

## Mapping method used for this review

- Parsed every Python file in `gateway/`, `hermes_cli/`, `tools/`, `agent/`,
  `utils/`, `memory/`, `cron/` of the quarry with `ast`, collecting function
  defs of 15 lines or more.
- Searched all `src/**/*.c` and `lib/**/*.c/.h` for each function name as a
  quoted string (the convention used by harness and PoP comments).
- Result: **13,622 functions** with bodies >= 15 lines; **11,330** have no C
  reference at all. That number is not a work queue — most are process/UI/
  network code that cannot be faithfully verified without their seams. It is
  a reminder that annotation-driven parity metrics describe a tiny fraction
  of the reference surface.

## Highest-density unported clusters (candidate batches)

Ranked by number of medium (12–70 line) functions with no C reference:

1. `agent/auxiliary_client.py` — 128
2. `hermes_cli/gateway.py` — 104
3. `gateway/run.py` — 93
4. `agent/chat_completion_helpers.py` — 77
5. `gateway/platforms/base.py` — 63
6. `hermes_cli/kanban_db.py` — 62
7. `agent/agent_runtime_helpers.py` — 62
8. `cron/jobs.py` — 62
9. `cron/scheduler.py` — 60
10. `hermes_cli/gateway_windows.py` — 46

`hermes_cli/gateway_windows.py` was chosen first for this wave: it is the
Windows-specific launcher layer, its pure helpers are small, and they feed
the cmd/VBS scripts and Scheduled Task XML that the Windows port needs. Seven
leaf helpers are now ported with a LIVE-AST oracle and 46 fixtures:
`_normalize_windows_path`, `_quote_cmd_script_arg`, `_quote_vbs_string`,
`_sanitize_filename`, `_gateway_run_argv`, `_is_access_denied`,
`_should_fall_back`.

## Devil's advocate: how a green suite can still lie

- **Harness-only wiring.** A port called only by `tests/t_port_*.c` can be
  perfect and still never run in the product. Check for a real caller after
  every gate.
- **Oracle drift.** The quarry moves functions between modules. Three suite9
  failures were stale oracle source paths, not C regressions. Re-extract from
  the live file, never from a copied snapshot.
- **Script semantics.** An entry list is a sequence of successive returns, not
  a list value. A one-row seam script unwraps to a dict; both sides must agree
  or the oracle invents exceptions that the C port cannot reproduce.
- **Seam-vs-body mismatch.** If the oracle replaces a method with a recording
  seam but the C port implements the real body (or vice versa), the trace
  diverges even when both are faithful to LIVE. The seam boundary must be the
  same on both sides.
- **Duplicate symbols.** `--allow-multiple-definition` means link order picks
  a definition. Archive resolution picked a different one for
  `process_identity_flow2` (2 mismatches); that gate is pinned to explicit
  object linking.
- **UTF-8 boundaries.** Python `s[:n]` ends at a codepoint. A C prefix helper
  must consume the nth codepoint's continuation bytes.
- **Goto-skipped initializers.** A `goto cleanup` past `x = NULL` makes
  cleanup free garbage. This caused the last suite11 segfault in
  `tmk_run_task`.

Recommended automatable guardrails: a real-caller check for every new gate,
quarry-path assertions in every AST oracle, duplicate-symbol detection at
link time, and a periodic explicit-link spot check of archive-linked gates.

## Speed work (measured, not extrapolated)

| Change | Before | After |
|---|---|---|
| Gate with cached harness | 6.8 s | 1.37 s |
| 8-harness batch link | 20.3 s | 13.4 s |
| Full 580-gate suite | ~12 min | 230–289 s |
| Archive link of `gateway_revive` | ~9 s, 19 MB | 2.5 s, 217 KB |

Root causes: all 2241 registered objects were forced into every FULL image,
and the cache-freshness `find` walked ~19,400 `.o` files inside a fixture
submodule. Fixes: `tests/make_harness_archive.sh` packs the objects once and
FULL gates link against the archive (ld pulls only referenced objects), and
freshness checks scan `src lib` only.
