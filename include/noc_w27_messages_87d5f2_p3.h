/*
 * noc_w27_messages_87d5f2_p3.h — C surface for gate w27_messages_87d5f2_p3:
 * the display projection (upstream
 * persistence/sqlite/conversation/_store/messages.py).
 *
 * This gate owns everything between a stored row and a caller's transcript:
 * the per-row projection, the model-only predicate the display SQL mirrors,
 * the summary-carrier fold, the three statements a display / conversation read
 * runs, and get_messages' own argument gate + row assembly.
 */
#ifndef NOC_W27_MESSAGES_87D5F2_P3_H
#define NOC_W27_MESSAGES_87D5F2_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _is_model_only_row: display_metadata's "model_only" flag, as a bool. */
json_t *msg_is_model_only_row(const json_t *row);

/* _fold_summary_carrier(row, key, first_by_uid) -> {"key", "first_by_uid"}. */
json_t *msg_fold_summary_carrier(const json_t *row, const json_t *key, const json_t *first_by_uid);

/* _row_to_message_dict(row, summary_flag): dict(row) with the payloads decoded. */
json_t *msg_row_to_message_dict(const json_t *row, const json_t *summary_flag);

/* _display_rows_from_conn -> {"sql", "params"}. */
json_t *msg_display_rows_from_conn(const json_t *session_id, const json_t *limit,
                                   const json_t *offset, const json_t *latest, const json_t *after);

/* _active_clause(include_inactive, include_compacted). */
json_t *msg_active_clause(const json_t *include_inactive, const json_t *include_compacted);

/* _fetch_conversation_rows -> {"sql", "params"} (the plain, non-compacted read). */
json_t *msg_fetch_conversation_rows(const json_t *session_ids, const json_t *active_clause,
                                    const json_t *with_session_id);

/* get_messages' single-segment read -> {"sql", "params", "messages"} or {"error"}. */
json_t *msg_get_messages(const json_t *session_id, const json_t *include_inactive,
                         const json_t *include_compacted, const json_t *limit, const json_t *offset,
                         const json_t *latest, const json_t *after_id, const json_t *include_ancestors,
                         const json_t *rows);

#ifdef __cplusplus
}
#endif
#endif
