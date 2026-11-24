/*
 * noc_w27_messages_87d5f2_p1.h — C surface for gate w27_messages_87d5f2_p1:
 * the transcript store's payload codecs and counter statement
 * (upstream persistence/sqlite/conversation/_store/messages.py).
 *
 * This gate owns the writes' value-shaping layer: display-metadata encode /
 * decode, the reasoning column codec, the repair-row decoder and the sessions
 * counter UPDATE the inserters run.  No connection, no clock: every function
 * here is a pure transformation of values.
 */
#ifndef NOC_W27_MESSAGES_87D5F2_P1_H
#define NOC_W27_MESSAGES_87D5F2_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _encode_display_metadata: value -> its TEXT column (or null). */
json_t *msg_encode_display_metadata(const json_t *display_metadata);

/* _decode_display_metadata: column value -> dict (or null), legacy double layer unwrapped. */
json_t *msg_decode_display_metadata(const json_t *raw);

/* _reasoning_json_text: reasoning field -> TEXT (strings verbatim, else json.dumps). */
json_t *msg_reasoning_json_text(const json_t *value);

/* _decoded_repair_row: one durable row -> the live message dict, no dedupe/repair. */
json_t *msg_decoded_repair_row(const json_t *row);

/* _bump_session_counters: {"sql": <text|null>, "params": [...]}. */
json_t *msg_bump_session_counters(const json_t *session_id, const json_t *inserted,
                                  const json_t *tool_calls, const json_t *unit);

/* _reaction_list: display metadata -> the well-formed (dict) reactions. */
json_t *msg_reaction_list(const json_t *meta);

#ifdef __cplusplus
}
#endif
#endif
