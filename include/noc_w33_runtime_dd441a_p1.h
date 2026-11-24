/*
 * noc_w33_runtime_dd441a_p1.h - C surface for upstream
 * persistence/authority/runtime.py, gate w33_runtime_dd441a_p1.
 *
 * The receipt-identity half of the session runtime authority: the exact-retry
 * receipt key and digest a mutation request names (``mutation_receipt_key``),
 * the chat-pick spelling of it with its fence validation (``chat_pick_key``),
 * the bounded identity text rule (``_text``) and the module constant clients
 * match on (``COMPACTED_TARGET_DETAIL``).  The store, the epoch and the
 * transactions are seams; nothing here opens a connection.
 */
#ifndef NOC_W33_RUNTIME_DD441A_P1_H
#define NOC_W33_RUNTIME_DD441A_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The receipt key prefix ``mutation_receipt_key`` prepends. */
#define W33_RUNTIME_DD441A_P1_RECEIPT_PREFIX "gateway.mutation.v1."

/* The identity text limit ``_text`` enforces. */
#define W33_RUNTIME_DD441A_P1_TEXT_MAX 1024

/* req: {principal_id, session_id, request_id, operation, payload,
 *       expected_revision, expected_generation?} ->
 * {"ok": true, "key": str, "digest": str}. */
json_t *w33_runtime_dd441a_p1_mutation_receipt_key(const json_t *req);

/* req: the same shape, fenced (both fences required and non-negative) ->
 * {"ok": true, "key": str, "digest": str} or the invalid_params refusal. */
json_t *w33_runtime_dd441a_p1_chat_pick_key(const json_t *req);

/* req: {"value": <any>} -> {"ok": true, "value": str, "length": int} or the
 * invalid_params refusal. */
json_t *w33_runtime_dd441a_p1_text(const json_t *req);

/* {"compacted_target_detail": str} */
json_t *w33_runtime_dd441a_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W33_RUNTIME_DD441A_P1_H */
