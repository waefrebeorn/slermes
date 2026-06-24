# S14 #3: Gateway Protocol Parity — Methodology Comparison

**Date:** June 01 Session
**Methodology:** Function-level diff of Python `gateway/run.py` (19K lines), `gateway/session.py` (1.3K) vs C `gateway/server.c` (2.4K), `gateway/helpers.c` (377 lines)

## Core architecture

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Event loop | asyncio (async/await) | pthreads (sync) | PORTED (acceptable alternative) |
| Multi-platform | Platform enum + base adapter class | platform_def_t array + function ptrs | PORTED |
| Platform count | 20+ platforms | 19 platform types | PORTED (~95%) |
| Platform init | `BasePlatformAdapter.__init__()` | `setup_*()` per platform | PORTED |
| Platform poll | async coroutine per adapter | pthread per platform | PORTED |
| Session management | `SessionStore` with SQLite | Fixed-size pool with SQLite | PARTIAL |
| Agent reuse | LRU cache (128 cap, 1h TTL) | Per-session agent (evict oldest) | PARTIAL |
| Message queue | Per-adapter + /queue FIFO | Global circular buffer | PARTIAL |

## Python features NOT in C (REAL GAPS)

1. **Agent cache with idle TTL (P1)** — C session pool has no TTL eviction, no LRU `move_to_end()` ordering. Python: `_AGENT_CACHE_IDLE_TTL_SECS=3600`, `_AGENT_CACHE_MAX_SIZE=128`. C: simple "evict oldest by last_active" — no TTL for idle agents.

2. **Per-session model overrides (P1)** — `/model` command sets per-session model/provider/api_key/base_url/api_mode. C uses one agent config for all sessions. File: `run.py:1776-1778`.

3. **Per-session reasoning config (P1)** — `/reasoning` command persists reasoning effort per session. C doesn't have `/reasoning` overrides. File: `run.py:1779-1781`.

4. **DeliveryRouter abstraction (P2)** — Python has `DeliveryRouter(self.config)` for routing responses to correct platform. C sends directly via `gw_platform_send()`. File: `run.py:1713`.

5. **Status messages / typing indicators (P2)** — Python sends `_send_or_update_status_coro()` for busy states. C sends nothing during agent processing. File: `run.py:322-333`.

6. **Automatic platform reconnection (P2)** — Python tracks `_failed_platforms` with retry scheduling. C logs setup failure and moves on. File: `run.py:1790-1792`.

7. **HMAC webhook verification (P3)** — C logs "HMAC verification: disabled" (`server.c:2253` webhook setup). Python has HMAC verification via `webhook.py`.

8. **FIFO /queue semantics (P3)** — Python has `_queued_events: Dict[str, List[MessageEvent]]` for FIFO queue of /queue commands. C has a single slot per session.

9. **Busy-ack debounce (P3)** — Python tracks `_busy_ack_ts` per session to avoid spamming users. C doesn't send busy acknowledgments.

10. **Media handling (P3)** — Python has `_build_media_placeholder()`, `_probe_audio_duration()`. C processes text messages only.

11. **Prefill messages / ephemeral config (P3)** — Python loads `_prefill_messages`, `_ephemeral_system_prompt`, `_reasoning_config`, `_service_tier`. C doesn't support ephemeral config overrides.

12. **Last-resolved model fallback (P3)** — Python caches last working model per session to survive config-cache misses. C doesn't cache. File: `run.py:1733-1740`.

13. **Kanban notifier profile (P3)** — Python routes kanban events to gateway with profile tracking. C doesn't integrate kanban with gateway notifications.

14. **Pending /update prompt tracking (P3)** — Python tracks `_update_prompt_pending` per session. C doesn't support `/update` prompts via gateway.

15. **Session sources LRU cache (P3)** — Python caches `SessionSource` objects (512 max) for fallback routing. C doesn't cache session source metadata.

## C gateway features already ported

- ✅ Multi-platform support (19 types with setup/poll/shutdown)
- ✅ Per-session SQLite persistence
- ✅ Per-session agent (created on first message, reused)
- ✅ Message deduplication (`helpers.c:msg_dedup_*`)
- ✅ Approval request/response flow (poll + condvar)
- ✅ Rate limiting per platform
- ✅ Cron notification wiring
- ✅ Gateway log with rotation (10MB)
- ✅ Signal handling (SIGINT/SIGTERM)
- ✅ Thread-safe message queue (circular buffer)
- ✅ HTTP connection pool with keepalive config
- ✅ Platform registry (gw_platform_register/find)

## Overall classification

**Core gateway infrastructure: PORTED (~70%)** — multi-platform, session, agent, queue, logging, rate limiting, approval flow all present.

**Advanced gateway features: PARTIAL (~20%)** — Missing agent cache with TTL, per-session overrides, delivery routing, status indicators, platform reconnection, HMAC, /queue semantics, media handling, busy-ack, kanban integration.

**Overall: PARTIAL (~40%)** — Functional but lacks the polish and edge-case handling of the Python gateway.

**Evidence:**
- C gateway: `src/gateway/server.c:2158-2416` — hermes_gateway_main()
- C helpers: `src/gateway/helpers.c:1-377` — msg_dedup, etc.
- Python gateway: `/home/wubu/.hermes/hermes-agent/gateway/run.py:1663-19237` — GatewayRunner class
- Python session: `/home/wubu/.hermes/hermes-agent/gateway/session.py:1-1348` — SessionStore, SessionSource
- Battleship previously claimed 30% — corrected to 40% PARTIAL
