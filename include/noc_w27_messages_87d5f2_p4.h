/*
 * noc_w27_messages_87d5f2_p4.h — C surface for gate w27_messages_87d5f2_p4:
 * replay dedupe and lineage predicates (upstream
 * persistence/sqlite/conversation/_store/messages.py).
 *
 * This gate owns what an ancestor-lineage replay does with a user row it has
 * already emitted (the exact-clone key and the adjacent-duplicate scan) and the
 * fork predicate a resume uses to refuse crossing a branch/delegate boundary.
 */
#ifndef NOC_W27_MESSAGES_87D5F2_P4_H
#define NOC_W27_MESSAGES_87D5F2_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _canonical_replayed_user_content(msg) -> {"content", "composite"}. */
json_t *msg_canonical_replayed_user_content(const json_t *msg);

/* _exact_replayed_user_clone_key(timestamp, content) -> [ts, canonical json] | null. */
json_t *msg_exact_replayed_user_clone_key(const json_t *timestamp, const json_t *content);

/* _find_duplicate_replayed_user_message(messages, msg) -> [index, prefer] | null. */
json_t *msg_find_duplicate_replayed_user_message(const json_t *messages, const json_t *msg);

/* _is_explicit_fork_child_row(session, include_reset) -> bool. */
json_t *msg_is_explicit_fork_child_row(const json_t *session, const json_t *include_reset);

#ifdef __cplusplus
}
#endif
#endif
