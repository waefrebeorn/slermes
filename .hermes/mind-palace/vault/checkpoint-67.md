<!-- PARITY:AUTO -->
| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Checkpoint 67 — CP67/v94: AG25 dump_api_request_debug ported (16/24)

## Changes

**Ported `dump_api_request_debug()` from `agent/agent_runtime_helpers.py:1072-1150` (81L)** to `src/agent/llm_client.c`.

### Implementation
- Creates JSON debug dump files in `~/.slermes/logs/` with request body, error context, provider info, and timestamps
- Guarded by `SLERMES_DEBUG` env var — no-op when unset
- Wired into provider ops error path on 4xx HTTP responses
- Also supports `SLERMES_DEBUG_DUMP_STDOUT` to print dump to stdout

### Evidence
- `src/agent/llm_client.c:34-38` — forward declaration
- `src/agent/llm_client.c:1015-1022` — save body/url copy before freeing
- `src/agent/llm_client.c:1044-1049` — call in error path
- `src/agent/llm_client.c:1631-1719` — implementation
- `include/hermes_agent.h:282-292` — header declaration

### Impact
- **AG25:** 15/24 → 16/24 (67%)
- **Build:** Clean, 0 errors
- **Tests:** 4/4 pass
- **Commit:** `64fe13819`
