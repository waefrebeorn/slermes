/*
 * noc_w32_hostedroom_be7d8d_p2.h - C surface for upstream
 * persistence/sqlite/gateway/hosted_room_policy.py (gate w32_hostedroom_be7d8d_p2).
 *
 * The projection's text: the ``_text`` coercion every payload read goes
 * through, the kind -> handler dispatch, the byte-stable statements and schema
 * DDL, the reopened-kind placeholder list, the publication existence query the
 * deferred/terminal branch picks, the admitted event-seq set, the settled
 * message predicate, the terminal receipt coercion and the transcript
 * retention cutoff.  The connection that runs the statements stays in the
 * caller.
 */
#ifndef NOC_W32_HOSTEDROOM_BE7D8D_P2_H
#define NOC_W32_HOSTEDROOM_BE7D8D_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* mapping.get(key) or "" -> str(...); value_type names the Python type of a
 * numeric value ("int" by default, "float" for a float). */
json_t *w32_hostedroom_be7d8d_p2_text(const json_t *mapping, const char *key, const char *value_type);

/* kind -> the _APPLY_BY_KIND handler's name, or null. */
json_t *w32_hostedroom_be7d8d_p2_apply_handler(const char *kind);

/* "delete_active" | "transcript" | "room_columns" -> the statement text. */
json_t *w32_hostedroom_be7d8d_p2_named_sql(const char *name);

/* The checkpoint schema DDL, in creation order. */
json_t *w32_hostedroom_be7d8d_p2_schema_ddl(void);

/* {"kinds", "placeholders", "count"} for the Continue reopened-event select. */
json_t *w32_hostedroom_be7d8d_p2_reopened(void);

/* status -> {"sql", "kinds", "uses_generation"} for publication_exists. */
json_t *w32_hostedroom_be7d8d_p2_publication_query(const char *status);

/* source_event_seq + context event_seqs -> {"seqs", "placeholders", "count"}. */
json_t *w32_hostedroom_be7d8d_p2_event_seqs(const json_t *source_event_seq, const json_t *event_seqs);

/* a hosted_room_events row (or null) + the discussion id -> the _settled_message verdict. */
json_t *w32_hostedroom_be7d8d_p2_settled_check(const json_t *row, const char *discussion_event_id);

/* kind + payload -> the terminal receipt coercion. */
json_t *w32_hostedroom_be7d8d_p2_receipt(const json_t *kind, const json_t *payload);

/* [{seq}] of message events -> {"cutoff", "delete_below", "offset"}. */
json_t *w32_hostedroom_be7d8d_p2_transcript_retention(const json_t *seqs);

#ifdef __cplusplus
}
#endif
#endif
