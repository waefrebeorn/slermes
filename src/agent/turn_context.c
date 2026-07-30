/*
 * turn_context.c — Per-turn context setup for run_conversation.
 *
 * Port of Python agent/turn_context.py (389 lines).
 * 1 module-level function + TurnContext dataclass.
 *
 * Port of Python: build_turn_context — INLINE in run_conversation() at
 *   agent_loop.c:842 (the turn prologue: lines 846-972+ handle user
 *   message sanitization, memory prefetch, steer queue, subdir hints,
 *   system prompt restore, todo hydration, nudge counters, etc.)
 * N/A: TurnContext — Python dataclass (C uses agent_state_t directly,
 *   no intermediate dataclass needed — state lives on the struct)
 */

#include "hermes_core_types.h"
