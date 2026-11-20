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

# Checkpoint 12 — Gateway Clarify + Reclassifications

**Changes:**
1. **TD19 CLOSED** — clarify_gateway: Added gateway-mode clarify with blocking wait, condvar, and session-scoped response matching. `src/tools/clarify.c` rewritten with dual CLI/gateway paths. `src/gateway/server.c` added `g_gw_clarify` state struct, `gw_clarify_begin/match/check_response/wire_response`. Wired into message processing path and init. `include/hermes.h` declarations added.
2. **MS82 N/A** — `agent/compression.py`: File does not exist in Python codebase. Battleship listed it erroneously.
3. **MS83 N/A** — `agent/caching.py`: File does not exist in Python codebase. Battleship listed it erroneously.
4. **TD20 N/A** — `tools/env_probe.py`: Python-specific environment probing tool (Python/pip/venv detection). C has no equivalent need — no Python runtime to probe.
5. **TD23 N/A** — `tools/openrouter_client.py`: 33-line Python convenience wrapper providing shared async OpenRouter client. C has native OpenRouter provider (`provider_openrouter.c`).

**Net gaps closed: 5 (1 implementation + 4 reclassifications)**
