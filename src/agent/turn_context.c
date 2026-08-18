/*
 * turn_context.c — Per-turn context setup for run_conversation.
 *
 * Port of Python agent/turn_context.py (389 lines) — 1 module-level function
 * (build_turn_context) + a TurnContext dataclass.
 *
 * The Python TurnContext dataclass collapses in the C port: the turn context
 * is materialized as a JSON object (build_turn_context) consumed by the
 * conversation loop, rather than a separate struct. build_turn_context() is
 * implemented for real here, delegating to tctx_build_turn_context() in
 * port_agent_core_helpers.c (the canonical PoP). No "N/A".
 */
#include "hermes_core_types.h"

/* Forward-declare the canonical implementation (port_agent_core_helpers.c). */
int tctx_build_turn_context(const char *user_message, const char *system_message,
                            const char *conversation_history_json,
                            const char *task_id, char *out, size_t outsz);

/* Port of Python: turn_context.build_turn_context() */
int build_turn_context(const char *user_message, const char *system_message,
                       const char *conversation_history_json,
                       const char *task_id, char *out, size_t outsz)
{
    return tctx_build_turn_context(user_message, system_message,
                                   conversation_history_json, task_id, out, outsz);
}
