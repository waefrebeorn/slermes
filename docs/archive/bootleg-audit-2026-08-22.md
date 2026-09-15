<!-- PARITY:AUTO -->
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | 313 PASS / 22,604 (1.4%) · 0/6230 ported match · 11511/11511 gap mismatch |
**Phase (v672):** PORT phase

**Upstream sync:** 2,072 ahead / 2,639 behind (last merge 2026-09-15 (upstream fetched))

_Generated 2026-09-15T06:22:36Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Bootleg Audit —  (RESOLVED — see Resolution below)

**Scanner:** `scripts/stub_hunter.py` (new, linear-time). The old
`tests/recursive_false_gap_hunter.py` predates the port_-prefix purge and
scans 0 files — it has been superseded.

## Resolution (same day)

The full fix campaign ran as five parallel workstreams plus follow-up
passes. **497 suspicious → 279**, and the remaining 279 are triaged:
~94 legitimate (platform-conditional windowing, backend-selection no-ops,
`_stubs.c` placeholders, documented Python-default no-op seams) + ~185
long-tail entries queued in `tests/unexplained_noops.json` for the next
pass. Every high-value cluster is now real code:

| Cluster | Fns fixed | Commit |
|---|---|---|
| gw_session transcript persistence | 13 | `c9ce510336` |
| gateway_runner adapters/scale-to-zero | 15 | `86cd89a072` |
| stream_consumer notification/delta | 24 | `43a12faa37` |
| mcp_tool reconnect/session | 12 | `69f9080f59` |
| tirith security chain | 13 | `2b88dc8932` |
| ports_wrappers dashboard/journey/bundles | 17 | `66db1d0fb8` |
| cron cluster | 12 | `f647be2bca` |
| final_gaps cli surface | 25 | `2107b3c` |
| util_helpers tools+cli | 19 | `3d1f7d88f0` |
| memory vtables | 11 | `a4b655ab42` |
| provider_bedrock converse | 5 | `e65293d932` |
| platforms/base (3 real + 18 verified Python no-ops) | 21 | `98deadb47c` |
| clarify/async_delegation/wake_word/kanban/service_manager + long tail | ~40 | various |

Also closed the same day: TUI boot-appearance parity (`0b79407ba5` —
byte-exact logo art, two-column panel, compact path), the 79-lib dependency
audit (`5e0099e6f9`, 5 libs FIXED with a 36-case oracle), and the upstream
quarry re-sync (2,852 commits) with all oracle drift repaired
(`e284c8d582`: provider_auth 55-provider table, turn_retry_state new field,
toolsets browser_exec/kanban_request_* alignment, bfl recently-shipped
auto-enable, run_pure name parity).

## Original findings (pre-fix)

| Metric | Count |
|---|---|
| C functions scanned | 23,013 |
| Suspicious (empty / const-return bodies) | 497 |
| …of which PoP-annotated (**false gaps**: scanner credits them PORTED) | **236** |
| Modules affected by false gaps | 85 |
| Likely-legitimate no-op defaults (`noop_*`, `*_default`, platform placeholders) | ~23 |

The 497 breaks down as:
- `*_gaps.c` unfilled gap placeholders — 90
- `*_stubs.c` / platform-conditional placeholders (window_wayland on
  non-Wayland builds, computer_use noop backends, win32-only paths) — 11 +
  most of window_wayland.c's 22 (build-target stubs; real impls live in the
  platform files)
- **396 silent no-ops inside otherwise-real modules** — the dangerous class:
  `gw_session.c::_save_entries` (transcript persistence!), 
  `gateway_runner.c::gateway_runner_connect_adapter`,
  `mcp_tool.c::mcp_reconnect_server`, `stream_consumer.c` notification
  callbacks, `provider_bedrock.c::call_converse*`.

## Top modules by false-gap count

| Python module | False gaps |
|---|---|
| gateway/stream_consumer.py | 24 |
| gateway/run.py | 20 |
| tools/mcp_tool.py | 16 |
| tools/tirith_security.py | 16 |
| gateway/session.py | 13 |
| gateway/platforms/base.py | 9 |
| hermes_cli/commands.py | 7 |
| tools/computer_use/cua_backend.py | 6 |
| tools/wake_word.py | 6 |
| hermes_cli/dashboard_auth/base.py | 5 |

Full queue: `tests/stub_fix_queue.json` (all suspicious) and
`/tmp/false_gap_noops.json` (PoP-annotated subset).

## What this means for "1:1 parity"

The function-level scanner said 516/516 modules ported. That is *file-level*
truth. But **236 functions that the scanner credits are empty bodies** — they
are counted as parity yet pour no water. The honest number is:

> 516 modules ported at file level; ~85 of those modules contain at least one
> credited-but-empty function. True behavioral parity requires closing these.

## Triage classes

1. **Platform-conditional stubs (LEGITIMATE)** — `window_wayland.c` compiled
   without Wayland dev libs, `computer_use.c` noop/x11/win backends selected
   by platform, `_stubs.c` files. Fix = build with the platform deps, not code.
2. **vtable no-op defaults (LEGITIMATE)** — `default_*`,
   `gw_base_platform_adapter_default_*`: the base-class default the Python
   also defines as no-op. Verify against Python per-case.
3. **FALSE GAPS (must fix)** — PoP-annotated empties where LIVE Python does
   real work: persistence, adapters, MCP reconnect, gate checks, transcript
   writes. 236 of them.

## Plan

Priority order = user-visible impact:
1. `gw_session.c` transcript persistence cluster (_save_entries,
   _append_transcript_message, rewrite_transcript, rewind_session)
2. `gateway_runner.c` adapter connect/disconnect + scale-to-zero cluster
3. `stream_consumer.c` notification/delta callbacks (24 fns)
4. `mcp_tool.c` reconnect/session cluster (16 fns)
5. `tirith.c` security-scan install/verify chain (13 fns)
6. Remaining modules in ranked order from `tests/stub_fix_queue.json`

Each fix: read LIVE Python, implement faithfully, re-run module oracle,
re-run `stub_hunter.py --quiet` until the entry disappears.

## Tooling shipped

- `scripts/stub_hunter.py` v2.1 — linear-time whole-tree scan (the old
  hunter's regex was catastrophically backtracking and scanned 0 files).
  Flags: `--json`, `--quiet`, `--root`.
