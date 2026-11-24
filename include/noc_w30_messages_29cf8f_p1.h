/*
 * noc_w30_messages_29cf8f_p1.h - C surface for upstream
 * persistence/sqlite/conversation/messages.py, gate p1 (wire shaping).
 *
 * The transcript repository's pure dict/record shapers: the rewind-refusal
 * reason table, the row-visibility rule, the NewMessage/HeldMessage message
 * dicts the store's writers take, the pruned-original merge, the stored-row
 * transcript projection, the JSON sidecar decode and the wire field lists.
 *
 * Every SQLite seam (StoreHolder, connections, transactions) stays in the
 * caller; these functions take plain JSON request objects and return JSON.
 */
#ifndef NOC_W30_MESSAGES_29CF8F_P1_H
#define NOC_W30_MESSAGES_29CF8F_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"text": <str>} -> the contract reason the store's rewind RuntimeError text
 * names, or null when the text names none (_REWIND_REFUSALS). */
json_t *w30_messages_29cf8f_p1_rewind_refusal_reason(const json_t *in);

/* {"active": <any>, "compacted": <any>} -> "active" |
 * "compaction_archived" | "rewound" (_visibility). */
json_t *w30_messages_29cf8f_p1_visibility(const json_t *in);

/* {"message": {NewMessage fields...}} -> the message dict the store writers
 * take (_message_dict). */
json_t *w30_messages_29cf8f_p1_message_dict(const json_t *in);

/* {"held": {HeldMessage fields...}} -> the held message dict the store
 * compares, retired_durable_rows recursed (_held_dict). */
json_t *w30_messages_29cf8f_p1_held_dict(const json_t *in);

/* {"before": {NewMessage}, "original": {HeldMessage}} -> before's dict minus
 * _row_id/_db_row_snapshot with the held original's fields over it
 * (_pruned_original). */
json_t *w30_messages_29cf8f_p1_pruned_original(const json_t *in);

/* {"row": {stored row dict...}} -> the TranscriptRow projection over
 * _WIRE_FIELDS (_transcript_row). */
json_t *w30_messages_29cf8f_p1_transcript_row(const json_t *in);

/* {"value": <any>} -> a JSON string decoded, any other value verbatim, an
 * undecodable string verbatim (_sidecar). */
json_t *w30_messages_29cf8f_p1_sidecar(const json_t *in);

/* {} -> {"message_optionals": [...], "wire_fields": [...],
 * "sidecars": [...]} (_MESSAGE_OPTIONALS/_WIRE_FIELDS/_SIDECARS). */
json_t *w30_messages_29cf8f_p1_wire_fields(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
