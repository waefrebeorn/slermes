#ifndef NOC_W11_TURNITERAT_F63754_H
#define NOC_W11_TURNITERAT_F63754_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_turniterat_f63754_anchors_current_turn @ agent/turn_iteration_prep.py:_anchors_current_turn
 * idx is a decimal STRING (int64): "true"/"false" model Python bool-as-int.
 * `view` is the injected user_originated_turn_view(msg) result (json object or
 * NULL); it is consulted only after the verbatim content check fails. */
int w11_turniterat_f63754_anchors_current_turn(
    const json_t *messages, const char *idx, const char *user_message, const json_t *view);

/* PoP: w11_turniterat_f63754_previous_tool_round @ agent/turn_iteration_prep.py:_previous_tool_round
 * Returns the newest assistant tool_calls batch joined to its tool-result rows. */
json_t *w11_turniterat_f63754_previous_tool_round(const json_t *messages);

/* PoP: w11_turniterat_f63754_neutralise_replay_echo_ghosts @ agent/turn_iteration_prep.py:_neutralise_replay_echo_ghosts
 * Returns {"messages": [...], "neutralised": n}. The three marker strings are
 * injected seams for the lazily-imported placeholder constants. */
json_t *w11_turniterat_f63754_neutralise_replay_echo_ghosts(
    const json_t *seq, const char *scaffold_marker, const char *legacy_placeholder,
    const char *current_placeholder);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TURNITERAT_F63754_H */
