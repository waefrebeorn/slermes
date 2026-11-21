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

# Checkpoint 61 — AG25 drop_thinking_only_and_merge_users ported

**Build v88. AG25: 9/24 → 10/24 public functions ported.**

## What Was Done

1. **`drop_thinking_only_and_merge_users`** ported to C:
   - File: `src/agent/agent_message_repair.c` (after `agent_runtime_owns_post_tool_hook`)
   - Header: `include/hermes_agent.h`
   - Drops assistant messages that have reasoning but no visible content and no tool calls
   - Reuses `repair_message_sequence()` for Pass 2 (adjacent user message merge)
   - Port of Python `agent_runtime_helpers.drop_thinking_only_and_merge_users()`

2. **Walkway files updated**: v87→v88, cp60→cp61 across all files

3. **Barnacle hunt**: clean

## Build/Test Status
- Build: clean, 0 errors
- Tests: 4/4 pass

## Classification Changes
- AG25 agent_runtime_helpers: 9/24→10/24 public functions in C (42%) — still REAL GAP
