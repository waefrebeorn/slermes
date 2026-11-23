#ifndef NOC_W11_TURNFINAL_45CEB6_H
#define NOC_W11_TURNFINAL_45CEB6_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_turnfinal_45ceb6_bare_marker_after_reply @ agent/turn_final_response.py:_bare_marker_after_reply
 * needs_reply is tri-state: 1 == literal Python True (only value that passes the
 * `is not True` guard), 0 == False, other == truthy-but-not-True. truncated_parts
 * and promoted use Python truthiness. `reply` is the injected housekeeping_reply_line
 * result; reply_is_silence / final_is_silence are the two injected
 * is_intentional_silence_response results (in call order). */
int w11_turnfinal_45ceb6_bare_marker_after_reply(
    int needs_reply, int truncated_parts, int promoted, int interrupt_requested,
    const char *reply, int reply_is_silence, int final_is_silence);

/* PoP: w11_turnfinal_45ceb6_absorb_continuation_fragments @ agent/turn_final_response.py:_absorb_continuation_fragments
 * Mutates `messages` (json array of objects) by popping the two ephemeral
 * continuation flags from every row, exactly like the Python loop. `parts` is a
 * json array of {"text": str, "is_partial_stub": bool} (the (str,bool) tuples).
 * Returns the joined string (caller frees); sets had_parts to whether parts was
 * non-empty. The join mirrors conversation_loop._join_truncated_parts. */
char *w11_turnfinal_45ceb6_absorb_continuation_fragments(
    const char *final_response, const json_t *parts, json_t *messages, int *had_parts);

/* PoP: w11_turnfinal_45ceb6_pop_ephemeral_scaffolding @ agent/turn_final_response.py:finish_text_response
 * The trailing-scaffolding pop loop from finish_text_response (the
 * _EPHEMERAL_SCAFFOLDING_FLAGS while-loop). Mutates `messages`, returns how many
 * trailing rows were popped. */
int w11_turnfinal_45ceb6_pop_ephemeral_scaffolding(json_t *messages);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TURNFINAL_45CEB6_H */
