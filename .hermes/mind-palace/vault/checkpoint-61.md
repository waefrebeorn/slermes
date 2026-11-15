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
