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

# Checkpoint 66 — AG25 copy_reasoning_content_for_api ported (15/24)

## Analysis: Python `agent/agent_runtime_helpers.py:copy_reasoning_content_for_api()` (71L) → C `copy_reasoning_content_for_api()`

### What
Copies provider-facing reasoning fields from stored messages onto API replay messages. Handles 5 edge cases:
1. Explicit reasoning_content → preserve verbatim (empty→" " upgrade for pad providers)
2. Cross-provider poisoned history → inject " " on tool-call turns with reasoning
3. Promote internal 'reasoning' field to 'reasoning_content'
4. DeepSeek/Kimi/MiMo thinking mode → inject " " on all assistant messages
5. No reasoning content → leave API message unchanged

### Evidence
- **Implementation:** `src/agent/llm_client.c:2025-2086` — `copy_reasoning_content_for_api()`
- **Header:** `include/hermes_agent.h:273-278`
- Python original: `agent/agent_runtime_helpers.py:1987-2055` (71 lines)

### Impact
- **AG25:** 14/24→15/24 public functions ported (62%)
- **Name parity:** Matches Python `copy_reasoning_content_for_api()` exactly

### Build & Test
- **Build:** Clean compile, 0 errors
- **Tests:** 4/4 pass
- **Battleship:** v93
