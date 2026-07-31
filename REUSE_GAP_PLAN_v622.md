# Slermes v622+ Plan — Close ~1,000 REAL_GAPs via Code Reuse

**Date:** 2026-07-23 | **Author:** recovery session (post upstream re-pull)
**Baseline (post upstream re-pull):** PORTED 6,532 / REAL_GAP 4,619 / PARTIAL 23 / TOTAL 11,174

> **WARNING — shared-object fork hazard (do NOT push the Python parent fork):**
> `/home/wubu/hermes-agent-dev` (Python) and `slermes/` share git objects. The parent's
> `origin/main` remote-tracking ref is polluted with slermes C-commit SHAs
> (`aa5a39bf0` "v621: sync live parity…" sits on Python `waefrebeorn/hermes-agent`).
> `git push origin main` on the parent was **rejected as "stale info"** by force-with-lease —
> pushing would clobber the Python fork with C objects. The parent upstream-merge commit
> (`93e5012fb`) stays LOCAL. Only `slermes` (`waefrebeorn/slermes`) is pushed.

## The 1,000-gap thesis (grounded in live scanner data)

We have 4,619 REAL_GAP. Many are name-pattern twins of logic we already ported.
Re-closing them = reuse existing `port_*`/`lib/` helpers + add `/* PoP: c_fn @ mod.py:py_fn */`,
NOT rewriting. Real reusable surface present:

- `src/lib/` string/time/json/base64/hash/url/path/os helpers (381 headers, 769 .c)
- `src/agent/port_agent_tool_dispatch_helpers.h`, `port_agent_redact_helpers.h`, `port_markdown_tables.h`
- `src/tools/port_url_safety_helpers.h`, `src/hermes_cli/sqlite_util.h`

Name-bucket estimates of REAL_GAP that map to existing C utility families:
`config/get/set/env` ~526 · `http/request/client` ~429 · `validate/check/is_` ~312 ·
`time/date` ~288 · `base64/hash/crypto` ~126 · `format/render` ~109 · `json` ~62 ·
`url` ~55 · `string/normalize` ~71. **Subtotal directly reusable ≈ 1,978.**
After removing genuinely IO/network-heavy ones, a realistic **~1,000** are closable by reuse.

## Execution lanes (parallel-safe, each is a self-contained commit)

**Lane 0 — PARTIAL (immediate, 23 gaps):** add `/* PoP */` before the existing C fn.
Targets incl. `hermes_cli/console_engine.py` (`_status,_doctor,_logs,_sessions_list,
_config_path,_config_set,_config_migrate`), `hermes_cli/web_server.py` (`_is_sensitive_path`),
`tools/image_source.py` (`_get_active_env,_finalize`). No new logic — annotation only.

**Lane 1 — Pure-logic batch (highest yield):** `model_normalize`/`slug`/`base64`/`sha`/
`url_parse`/`json_field` families already exist in `lib/`. New `port_*.c` for
`hermes_cli/tools_config.py` (70), `hermes_cli/plugins.py` (59), `hermes_cli/plugins_cmd.py`
(47), `hermes_cli/commands.py` (41) reusing `lib/str`, `lib/json`, `lib/base64`.

**Lane 2 — CLI command mixins:** `hermes_cli/cli_commands_mixin.py` (43, 0 ported),
`hermes_cli/config.py` (47, reuse `sqlite_util` + `lib/config`), `hermes_cli/kanban.py` (49).

**Lane 3 — Gateway pure helpers:** `gateway/slash_commands.py` (60, 0 ported),
`gateway/session.py` (58), `gateway/stream_consumer.py` (35) — reuse `port_gateway_*`.

**Lane 4 — agent pure:** `agent/context_compressor.py` (37, already 56 ported → top up),
`agent/memory_manager.py` (30), `cron/scheduler.py` (37, reuse `suggestion_catalog`).

## Per-lane recipe (from slermes-pop-parity skill)
1. `python3 tests/slermes_parity_battleground.py --module <python_module>` → exact fn names.
2. Group by Python module; write `port_<mod>.c` with REAL implementations (>5 lines, correct sig).
3. Call existing `lib/`/`port_*` helpers (no re-implementation).
4. Annotate `/* PoP: c_fn @ module.py:py_fn */` immediately before each C fn (pattern at idx 0).
5. Register `.o` in `build/objects.mk` (append to `PHASE5_OBJ`, NOT a dead `*_PORT_REGEN`).
6. `rm -f slermes <touched>.o && make slermes`; run module oracle where one exists.
   **FAP note:** the scanner only checks statics. A ported fn can still diverge
   from LIVE Python at runtime — that is a **FAP (Functional Alignment Problem)**,
   found only by the oracle harness (`bash tests/oracle/run_oracles.sh` → any
   `cases: MISMATCH` is a FAP; see `docs/fap.md`). If a `t_port_<mod>.c` +
   `sta_oracle_<mod>.py` pair exists, it MUST be registered in
   `tests/oracle/registry.json` so its FAPs actually run; an unregistered port
   hides its FAPs. Run the oracle after every port touch — MATCH = behavioral done.
7. Re-run scanner; confirm `real_gaps` dropped and `c_function` populated (not None).
8. Commit all files together with `v622:` prefix; push `origin main` (force-with-lease).

## Definition of done
- PARTIAL → 0 (23 annotation fixes).
- ~1,000 REAL_GAP closed across Lanes 1–4 using reused helpers (no stubs; oracle-verified where a suite exists).
- Oracle harness wired for every ported module with a `t_port_*`/`sta_oracle_*`
  pair (no silent FAPs from unregistered ports).
- BANNER/parity-summary updated each lane; live parity re-run after each push.

## Out of scope (do NOT fake)
`hermes_cli/web_server.py` (444) / `gateway/run.py` (308) / `hermes_cli/main.py` (200) /
`auth.py` (153) — these need real IO/transport/process work; tackle in later, dedicated lanes.
