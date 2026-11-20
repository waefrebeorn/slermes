<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
