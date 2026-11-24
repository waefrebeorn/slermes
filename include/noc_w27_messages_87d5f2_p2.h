/*
 * noc_w27_messages_87d5f2_p2.h — C surface for gate w27_messages_87d5f2_p2:
 * transcript row identity, the kept-prefix stamp and the clone statement
 * (upstream persistence/sqlite/conversation/_store/messages.py).
 *
 * This gate owns what a live message and a stored row must agree on before a
 * replace/rewind may keep the row instead of re-inserting it, plus the
 * pure-SQL clone an in-place compaction uses to re-sequence a concurrent tail.
 */
#ifndef NOC_W27_MESSAGES_87D5F2_P2_H
#define NOC_W27_MESSAGES_87D5F2_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _row_identity: [role, encoded content, tool_call_id, encoded tool_calls]. */
json_t *msg_row_identity(const json_t *role, const json_t *content, const json_t *tool_call_id,
                         const json_t *tool_calls);

/* _loaded_view_content: the loader's lens (sanitize + strip for user/assistant text). */
json_t *msg_loaded_view_content(const json_t *role, const json_t *content);

/* _stamp_kept_live_prefix(live, messages) -> {"kept": n, "messages": [...]}.
 * live: SELECT id, role, content, tool_call_id, tool_calls, message_uid rows. */
json_t *msg_stamp_kept_live_prefix(const json_t *live, const json_t *messages);

/* _clone_message_rows(columns, tail_ids, session_id) -> {"sql", "params"};
 * columns are PRAGMA table_info(messages) names, session_id null = in place. */
json_t *msg_clone_message_rows(const json_t *columns, const json_t *tail_ids,
                               const json_t *session_id);

#ifdef __cplusplus
}
#endif
#endif
