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

/* === merged from port_agent_turn_context.c === */
/* Slermes C port — agent/turn_context.py (pure helper) */

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"
#include "provider_metadata.h"

/* PoP: agent_turn_context_compression_made_progress @ agent/turn_context.py:compression_made_progress */
bool agent_turn_context_compression_made_progress(
    long orig_len, long new_len, long orig_tokens, long new_tokens)
{
    if (new_len < orig_len) return true;
    return orig_tokens > 0 && new_tokens < orig_tokens * 0.95;
}

/* PoP: the @ agent/turn_context.py:_should_run_preflight_estimate */
/* Cheap gate for the (expensive) full preflight token estimate.
 * Returns True when EITHER:
 *   (a) message count exceeds the protected ranges (historical gate), OR
 *   (b) a cheap char-based estimate already crosses the threshold (the
 *       few-but-huge case from #27405 that the count-only gate skips).
 * Branch (b) uses estimate_messages_tokens_rough so a single large base64
 * image isn't mistaken for ~250K tokens. */
bool agent_turn_context__should_run_preflight_estimate(
    const json_t *messages, long protect_first_n, long protect_last_n,
    long threshold_tokens)
{
    if (!messages) return false;
    long count = (long)json_len(messages);
    if (count > protect_first_n + protect_last_n + 1) return true;
    long est = estimate_messages_tokens_rough(messages);
    return est >= threshold_tokens;
}

