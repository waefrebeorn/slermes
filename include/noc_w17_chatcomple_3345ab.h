#ifndef NOC_W17_CHATCOMPLE_3345AB_H
#define NOC_W17_CHATCOMPLE_3345AB_H

#include "hermes_json.h"

/* Replay of agent/chat_completion_headers.py::_chat_completion_capturing_headers.
 *
 * The external effects (SDK client, agent capture callbacks, credits tracker)
 * are supplied by the caller as data; this function reproduces exactly the
 * branch/decision logic of the live function.
 *
 *   provider          - agent.provider (NULL / "" means "not nous")
 *   has_state         - whether agent._credits_state is present (non-None)
 *   is_usage_window   - the JSON value of state.is_usage_window; only JSON
 *                       boolean true satisfies the live `is not True` guard
 *   has_raw_response  - whether chat.completions.with_raw_response exists
 *   response_is_window- credits_tracker.response_is_usage_window(raw)
 *   plain_result      - value returned by the plain create() call
 *   parse_result      - value returned by raw.parse()
 *
 * Returns a new object {"path":str,"raw_created":bool,"captures":int,
 * "result":str}; caller owns it. */
json_t *w17_chatcomple_3345ab_capture_headers(
    const char *provider, int has_state, json_t *is_usage_window,
    int has_raw_response, int response_is_window,
    const char *plain_result, const char *parse_result);

#endif
