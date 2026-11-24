/*
 * noc_w26_rewind_057872.h - C surface for upstream
 * persistence/sqlite/conversation/_store/rewind.py (gate w26_rewind_057872).
 *
 * The user-turn rewind's pure decisions: which user turn a (possibly negative)
 * ordinal names and how many turns that undoes, which warm rows are throwaway
 * recovery scaffolding, the lossless retry text /retry may resend (with its
 * fail-closed errors), the display flattening a prefill uses, and the module's
 * message-shape constants.  The store handle, the reads, the write transaction
 * and the session rules' row shapes are seams.
 */
#ifndef NOC_W26_REWIND_057872_H
#define NOC_W26_REWIND_057872_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* plan ordinal: {"ok": true, "ordinal": n, "turns_undone": k} or
 * {"ok": false, "error": "target user message is no longer in session history"}.
 * count = len(durable_user) of the repaired transcript. */
json_t *w26_rewind_057872_plan_ordinal(const json_t *count, const json_t *ordinal);

/* retryable_user_text(content): {"ok": true, "text": ...} or
 * {"ok": false, "error": <the ValueError message>}. */
json_t *w26_rewind_057872_retry_text(const json_t *content);

/* flatten_message_text(content): the visible text of a content shape, sep "\n". */
json_t *w26_rewind_057872_flatten_text(const json_t *content);

/* [m for m in messages if not _is_ephemeral_scaffolding(m)] */
json_t *w26_rewind_057872_non_scaffolding(const json_t *messages);

/* The rewind message-shape constants, as an object. */
json_t *w26_rewind_057872_constants(void);

#ifdef __cplusplus
}
#endif
#endif
