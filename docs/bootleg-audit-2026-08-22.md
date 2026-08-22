# Bootleg Audit — 2026-08-22

**Scanner:** `scripts/stub_hunter.py` (new, linear-time). The old
`tests/recursive_false_gap_hunter.py` predates the port_-prefix purge and
scans 0 files — it has been superseded.

## Headline numbers

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
