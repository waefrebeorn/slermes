#ifndef AGENT_REPLAY_CLEANUP_H
#define AGENT_REPLAY_CLEANUP_H
#include <stdbool.h>
#include "hermes_json.h"

/* Pure replay-history sanitization (agent/replay_cleanup.py). */
bool agent_replay_cleanup_is_interrupted_tool_result(const char *content);

/* Strip interrupted assistant->tool blocks anywhere in the history.
 * Returns a NEW json_t* array; caller frees. NULL on alloc failure. */
json_t *agent_replay_cleanup_strip_interrupted_tool_tails(const json_t *history);

/* Strip a trailing unanswered assistant(tool_calls) with no tool answers.
 * Returns a NEW json_t* array; caller frees. */
json_t *agent_replay_cleanup_strip_dangling_tool_call_tail(const json_t *history);

/* Apply both strippers in canonical order. Returns a NEW json_t* array. */
json_t *agent_replay_cleanup_sanitize_replay_history(const json_t *history);

/* Expire stale dangerous-confirmation text in user messages (#59607).
 * Redacts in place (role preserved) when older than expiry_seconds and the
 * message carries a numeric "timestamp". Returns a NEW json_t* array. */
json_t *agent_replay_cleanup_strip_stale_dangerous_confirmations(
    const json_t *history, double now, double expiry_seconds);

#endif
