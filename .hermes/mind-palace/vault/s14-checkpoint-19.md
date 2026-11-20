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

# Checkpoint 19 — TR02+TR05 Codex Transport + Directory Cleanup

**Date:** 2026-06-03
**Commit:** 05d3835d6

## Gaps Closed

### TR02 — codex_app_server.py (399 lines) → PORTED
**File:** `src/agent/codex_app_server_client.c`
- JSON-RPC 2.0 client for `codex app-server` over stdio
- Subprocess spawn via fork/pipe/exec (pattern from libmcp/mcp.c)
- Threaded stdout reader with line-delimited JSON dispatch
- Pending request queue with pthread_cond_timedwait
- Notification + server-request queues (bounded, pthread_cond)
- Stderr ring buffer (500 lines) for diagnostics
- Lifecycle: initialize handshake, close with SIGTERM→SIGKILL escalation

### TR05 — codex_event_projector.py (312 lines) → PORTED
**File:** `src/agent/codex_event_projector.c`
- Projects codex item/* notifications into Hermes messages
- Handles all item types: agentMessage, reasoning, commandExecution, fileChange, mcpToolCall, dynamicToolCall, userMessage, opaque
- Deterministic call IDs (codex_{type}_{item_id})
- Reasoning accumulation across items
- Tool call → assistant message + tool result pairs

## Directory Cleanup
- Removed `slermes/upstream-python-ref/` shallow clone directory
- Python reference now accessed via `upstream/main` remote: `git show upstream/main:<path>`
- Updated all walkway file references from shallow clone to remote
- Updated upstream commit ref to 39fee4f3b (latest)

## Build Status
- Both new files compile clean, 0 warnings
- Full binary link has pre-existing errors (hermes_get_home, hermes_config_load, hermes_config_load_env) — NOT caused by new code

## Battleship Update
- TR02: ❌ REAL GAP → ✅ PORTED
- TR05: ❌ REAL GAP → ✅ PORTED
- TR sector: 5→6 PORTED, 4→2 REAL GAP
- Overall: ~69% → ~70% PORTED, ~20% → ~19% REAL GAP
