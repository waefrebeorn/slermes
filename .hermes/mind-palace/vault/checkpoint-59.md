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
