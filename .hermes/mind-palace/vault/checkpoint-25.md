<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Checkpoint 25 — Session split on compression, file_sync SSH wiring

## Gaps closed

### MS03 (conversation_compression) — REAL GAP → PARTIAL
- `src/agent/agent_loop.c:1160-1200` — Session split on compression
- After successful compression: ends old session (`ended_at`, `end_reason="compression"`), creates new child session with `parent_id` link and `branch_point`, saves compressed messages, updates `state->session_id`
- Still missing: compression lock, memory provider notification, image shrinking

### EN08 (file_sync) — REAL GAP → PARTIAL
- `src/tools/terminal.c:275-328` — `ssh_sync_upload()` helper function
- Wires file_sync into SSH backend: collects `~/.hermes` files via `file_sync_collect()`, creates remote dirs via ssh `mkdir -p`, uploads each file via scp before command execution
- Still missing: Modal/Docker wiring, sync-back on cleanup

## Build
- Clean compile, 0 errors
- 4/4 tests pass

## Battleship impact
- MS: 7→6 REAL GAP (MS03 moved to PARTIAL)
- EN: 2→1 REAL GAP (EN08 moved to PARTIAL, EN04 Daytona remains)
- Overall: ~78%→~79% PORTED, ~11%→~10% REAL GAP
