# Slermes Session Prestige — 2026-08-22

Wrap-up record for the "fix EVERYTHING, 1:1 parity" session. Next session
should read this first plus `docs/bootleg-audit-2026-08-22.md`.

## Where the project stands

| Metric | Value |
|---|---|
| Python modules (quarry) | 534 scanned |
| Ported to C11 | **524** |
| Remaining unported | **10** (in flight — see below) |
| Exported C functions | 18,827 |
| Total commits | 25,951 |
| Water-cup gates | 21, 0 failing |
| Build | green (45M binary) |

## What this session accomplished

1. **Bootleg audit + fix campaign.** New linear-time `scripts/stub_hunter.py`
   found 497 empty/const-return bodies (236 PoP-annotated false gaps across
   85 modules). Five parallel workstreams closed every high-value cluster
   (~218 fns fixed). See the audit doc for the full table.
2. **TUI boot-appearance parity** (`0b79407ba5`): byte-exact logo art,
   two-column Rich-style panel with tools/skills/MCP sections, compact
   banner path, skin hooks — verified by running the binary in a PTY.
3. **Dependency-lib audit** (`docs/lib_audit_2026-08-22.md`): all 79 libs
   catalogued; libregex/libdifflib/libcsv/libglob/libtextwrap FIXED with a
   36-case live-Python oracle (`make test-lib-audit`).
4. **Water-cup v3 runner** + new gates: repetition_guard, prompt_cache_scope,
   loops_format_interval, loops_parse_loop_args → 21 gates all MATCH.
5. **Upstream quarry re-sync** (2,852 commits merged into
   /home/wubu/hermes-agent-dev) and full oracle-drift repair:
   provider_auth regenerated from LIVE PROVIDER_REGISTRY (+vertex type),
   turn_retry_state native_compaction field, toolsets browser_exec +
   kanban_request_* alignment, `_enable_recently_shipped_toolsets` (bfl)
   ported, run_pure name-parity rename.
6. **8 of 10 post-sync unported modules closed** by parallel agents
   (compaction_display, error_surface, reasoning_effort, annotate/close/
   drive_preview, bot_mode_dm).

## In flight right now (next session: verify, don't redo)

One agent is porting the final 10 modules: browser_extension_router,
process_identity, relay_plugin_cutover, update_inventory, update_receipt,
worktree_cmd, worktree_gc, browser_control_artifacts, browser_control_broker,
control_socket. Note `include/worktree_gc.h` was committed prematurely —
verify it matches the implementation.

## Next-session queue (priority order)

1. Verify the final-10 agent's work (build + nm + battleground 534/534).
2. Long-tail no-ops: `tests/unexplained_noops.json` (~185 entries; many are
   documented-legit — reclassify with the comment convention
   `/* Python default is also a no-op */`, implement the rest).
3. Full oracle sweep (`tests/oracle/run_oracles.sh`) — last sweep had 56
   failing ports, of which I personally repaired usage_pricing,
   provider_auth, turn_retry_state, run_pure, toolsets, schema_sanitizer;
   the rest need triage (many are pre-existing harness rot from before the
   upstream sync).
4. Re-run water-cup suite + push everything.
5. TUI boot parity remaining diffs (documented in the banner commit):
   update-check notice line, lazy-tool yellow vs red distinction, MCP
   connected-state at boot.

## Hard-won conventions (do not relearn these)

- PoP annotation: `/* PoP: <c_name> @ <py_file>:<py_name> */` standalone
  line, nothing after the py name. Multi-line form: PoP on its own line
  before `*/`. Prose after the name silently breaks the scanner regex.
- Twin-oracle rule: rung ORDER is the contract; V-events on both sides or
  neither; harness stdout = cup stream only.
- libjson: `json_get_num` on a boolean returns default 0 — use bool accessors.
- Never `git checkout` a file holding uncommitted work (loops_manager.c was
  lost that way once; rewritten better).
- Upstream quarry wins on merge conflicts (`git checkout --theirs .`);
  slermes repo is the only push target.
- Concurrent agents editing one file = lost edits; re-grep before patching,
  re-verify builds after any parallel run (toolsets.c was overwritten once
  mid-fix and had to be re-patched).
