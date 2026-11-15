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

# Checkpoint 63 — AG25 extract_reasoning ported (12/24)

## Analysis: Python `agent_/agent_runtime_helpers.py:extract_reasoning()` → C `extract_reasoning()`

### What
Extracts reasoning/thinking content from an assistant message. Checks the structured `reasoning` field first, then scans `content` for think/reasoning XML blocks (`<think>`, `<thinking>`, `<reasoning>`, `<thought>`, `<REASONING_SCRATCHPAD>`) using the same tag definitions and scanner already in `agent_message_sanitize.c`. Returns combined reasoning text or NULL.

### Evidence
- **Implementation:** `src/agent/agent_message_sanitize.c:849-958` — `extract_reasoning(content, reasoning)`
- **Header:** `include/hermes_agent.h:253-255`
- Python original: `agent/agent_runtime_helpers.py:990-1068` (82 lines)

### Impact
- **AG25:** 11/24→12/24 public functions ported (50%)
- **Name parity:** Matches Python `extract_reasoning()` exactly

### Build & Test
- **Build:** Clean compile, 0 errors (1 pre-existing warning: sanitize_structure_surrogates_json unused)
- **Tests:** 4/4 pass
- **Battleship:** v90
