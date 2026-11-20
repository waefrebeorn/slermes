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

# Checkpoint 54 — AG25 sanitize_api_messages ported

**Battleship:** v80→v81

## What was done

### AG25: sanitize_api_messages → Pass 3 in repair_message_sequence

- Extended `repair_message_sequence()` in `src/agent/agent_message_repair.c` with Pass 3
- Pass 3 injects stub `MSG_TOOL` messages for any assistant `tool_call_id` that lacks a corresponding tool result
- Port of Python `agent/agent_runtime_helpers.py:sanitize_api_messages()` stub injection logic
- Stub content: `"[Result unavailable — see context summary above]"` matching Python
- Also added `msg_new()` static helper for creating heap-allocated message_t structs
- Previously `repair_message_sequence` only dropped orphaned tool results (Pass 1) and merged users (Pass 2)
- Now also ensures every tool call has a result before the API call — matching Python sanitize_api_messages behavior

### File:line evidence
- Stub injection logic: `src/agent/agent_message_repair.c:162-229`
- msg_new helper: `src/agent/agent_message_repair.c:35-45`
- TOOL_RESULT_STUB_MARKER: `src/agent/agent_message_repair.c:48-49`

### Build/Test
- Clean compile, 0 errors
- 4/4 tests passing

### Classification Changes
- AG25 agent_runtime_helpers: 6/24 → 7/24 functions ported (25% → 29%)