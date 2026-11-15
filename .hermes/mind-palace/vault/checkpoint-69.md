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

# Checkpoint 69 — CP69/v96: AG25 try_recover_primary_transport ported (18/24)

## Changes

**Ported `try_recover_primary_transport()` from `agent/agent_runtime_helpers.py:724-807` (84L)** to `llm_client.c` + `agent_loop.c`.

### Implementation
- `llm_client.c:1219-1254` — core function: gives primary provider one more attempt after retries exhausted, using a fresh HTTP connection. Skips OpenRouter/Nous (server-side retries).
- `agent_loop.c:1465-1489` — wired between retry exhaustion and fallback: calls `try_recover_primary_transport()`, if success -> `goto retry_done`, if failure -> proceed to fallback.

### Evidence
- `src/agent/llm_client.c:42-46` — forward declaration
- `src/agent/llm_client.c:1219-1254` — implementation
- `src/agent/agent_loop.c:1465-1489` — wired call site
- `include/hermes_agent.h:292-301` — header declaration

### Impact
- **AG25:** 17/24 -> 18/24 (75%)
- **Build:** Clean, 0 errors
- **Tests:** 4/4 pass
- **Commit:** `5f0759842`
