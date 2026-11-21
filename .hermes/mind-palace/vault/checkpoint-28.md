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

# Checkpoint 28 — EN08 SSH sync-back + MS03 compression lock

## Gaps closed

### EN08 (file_sync) — REAL GAP → PARTIAL
- `src/tools/terminal.c:330-369` — Added `ssh_sync_download()`: tar-over-ssh pipe to pull remote ~/.hermes back to host after command execution
- `src/tools/terminal.c:423-434` — Wired sync-back into `run_command_ssh()`: captures command result, runs sync-back, then returns
- SSH backend now has full round-trip file sync: upload before execution, download after
- Docker backend uses read-only volume mount (sync-back not needed — Python explicitly says Docker doesn't use file_sync)
- Modal backend still missing (requires Modal SDK)

### MS03 (conversation_compression) — REAL GAP → PARTIAL
- `lib/libdb/db.h:170-185` — Declared compression lock API: `db_try_acquire_compression_lock()`, `db_release_compression_lock()`, `db_get_compression_lock_holder()`
- `lib/libdb/db.c:1098-1197` — Implemented compression lock using `flock()` on per-session lock files
  - Lock file: `<sessions_dir>/.lock_<session_id>`
  - Holder ID written to lock file for diagnostics (format: `pid=<pid>:tid=main:session=<sid>`)
  - Non-blocking acquire (`LOCK_EX | LOCK_NB`) — skip compression if lock held
  - Static fd storage (single lock at a time, covers common case)
- `src/agent/agent_loop.c:1123-1147` — Wired lock acquisition before compression in agent loop
  - If lock not acquired, log holder and skip compression cycle
- `src/agent/agent_loop.c:1128-1133` — Wired lock release after compression (success or failure)
- Remaining MS03 gaps: memory provider notification, image shrinking (needs C image library)

## Build
- Clean compile, 0 errors (only pre-existing warning at agent_loop.c:864)
- 4/4 tests pass
