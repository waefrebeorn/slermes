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
