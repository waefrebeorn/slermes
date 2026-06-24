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
