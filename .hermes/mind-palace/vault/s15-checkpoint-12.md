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

# Checkpoint 12 — Gateway Clarify + Reclassifications

**Changes:**
1. **TD19 CLOSED** — clarify_gateway: Added gateway-mode clarify with blocking wait, condvar, and session-scoped response matching. `src/tools/clarify.c` rewritten with dual CLI/gateway paths. `src/gateway/server.c` added `g_gw_clarify` state struct, `gw_clarify_begin/match/check_response/wire_response`. Wired into message processing path and init. `include/hermes.h` declarations added.
2. **MS82 N/A** — `agent/compression.py`: File does not exist in Python codebase. Battleship listed it erroneously.
3. **MS83 N/A** — `agent/caching.py`: File does not exist in Python codebase. Battleship listed it erroneously.
4. **TD20 N/A** — `tools/env_probe.py`: Python-specific environment probing tool (Python/pip/venv detection). C has no equivalent need — no Python runtime to probe.
5. **TD23 N/A** — `tools/openrouter_client.py`: 33-line Python convenience wrapper providing shared async OpenRouter client. C has native OpenRouter provider (`provider_openrouter.c`).

**Net gaps closed: 5 (1 implementation + 4 reclassifications)**
