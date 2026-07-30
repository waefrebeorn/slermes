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
