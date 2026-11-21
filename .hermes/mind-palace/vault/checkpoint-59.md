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

# Checkpoint 59 — AG25 agent_runtime_owns_post_tool_hook ported

**Build v86. AG25: 8/24 → 9/24 public functions ported.**

## What Was Done

1. **`agent_runtime_owns_post_tool_hook`** ported to C:
   - File: `src/agent/agent_message_repair.c` (added after includes, before `message_deep_copy`)
   - Header: `include/hermes_agent.h` (declaration after `sanitize_tool_call_arguments`)
   - Checks function name against known post-hook tool names: `todo`, `session_search`, `memory`, `clarify`, `delegate_task`
   - Python original also checks `_context_engine_tool_names` and `memory_manager.has_tool()` — those C equivalents not yet implemented

2. **Walkway files updated**: state.md, battleship.md, roadmap.md, index.md, goal-mantra.md, prestige.md, plan.md, BANNER.md, mind-palace/index.md, mind-palace/state.md, mind-palace/goal-mantra.md, battleship-v40.md
   - All v85→v86, cp58→cp59
   - Battleship AG25 count: 8/24→9/24 (37%)

3. **Barnacle hunt**: no stale numbers in active walkway files

## Build/Test Status
- Build: clean, 0 errors
- Tests: 4/4 pass

## Classification Changes
- AG25 agent_runtime_helpers: 8/24→9/24 public functions in C (still REAL GAP)
