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

# Checkpoint 62 — AG25 apply_pending_steer_to_tool_results ported

## What Was Done

Ported `apply_pending_steer_to_tool_results` from Python `agent_runtime_helpers.py` (lines 2284-2348) to C `agent_message_repair.c`.

### New Function

**`apply_pending_steer_to_tool_results(message_t **messages, int *count, int num_tool_msgs, char *pending_steer)`**

Appends pending /steer text to the last MSG_TOOL message in the recent tail. Drains `pending_steer`, finds the last tool result backwards from the end, and appends with "\n\nUser guidance: " marker. If no tool result found, re-queues the steer.

Key behaviors ported from Python:
- **Drain**: Captures steer text, clears pending_steer buffer
- **Find**: Walks backward through the recent tail (last `num_tool_msgs` entries) for MSG_TOOL
- **Append**: Concatenates with marker "\n\nUser guidance: " to existing content
- **Re-queue**: If no MSG_TOOL found, prepends steer back to pending_steer for next-turn delivery
- **OOM safety**: On allocation failure, re-queues steer to pending_steer instead of losing it

### Files Changed

- `src/agent/agent_message_repair.c` — Added function implementation (92 lines)
- `include/hermes_agent.h` — Added declaration

### Evidence

- Build: Clean compile, 0 errors
- Tests: 4/4 pass (binary exists, help, version, no crash)
- Python function: `agent/agent_runtime_helpers.py:2284-2348`

### Classification

- AG25: 11/24 public functions ported (46%)
- Battleship: v89

CP62/v89
