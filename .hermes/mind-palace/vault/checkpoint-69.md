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
