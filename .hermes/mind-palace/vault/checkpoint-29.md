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

# Checkpoint 29 — EN08 SSH sync-back + MS03 compression lock + compression config fix

## Gaps closed

### EN08 (file_sync) — REAL GAP → PARTIAL
- `src/tools/terminal.c:330-369` — Added `ssh_sync_download()`: tar-over-ssh pipe pulls remote ~/.hermes back to host
- `src/tools/terminal.c:423-434` — Wired sync-back into `run_command_ssh()`: captures result, runs sync-back, returns
- SSH backend now has full round-trip file sync
- Docker uses read-only volume mount (sync-back N/A per Python design)
- Modal still missing (requires Modal SDK)

### MS03 (conversation_compression) — REAL GAP → PARTIAL
- `lib/libdb/db.h:170-185` — Declared compression lock API
- `lib/libdb/db.c:1098-1197` — Implemented compression lock using flock() on per-session lock files
- `src/agent/agent_loop.c:1123-1147` — Wired lock acquisition before compression
- `src/agent/agent_loop.c:1128-1133` — Wired lock release after compression
- `src/agent/llm_client.c:669-673` — **Bug fix**: removed erroneous `base_url[0]='\0'` and `api_key[0]='\0'` that would break compression API calls by clearing credentials

## Build
- Clean compile, 0 errors (only pre-existing warning at agent_loop.c:864)
- 4/4 tests pass
