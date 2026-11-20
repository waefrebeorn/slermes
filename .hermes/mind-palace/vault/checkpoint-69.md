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
