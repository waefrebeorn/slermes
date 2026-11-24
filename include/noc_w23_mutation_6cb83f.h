/*
 * noc_w23_mutation_6cb83f.h -- public C API for the receipted session
 * mutation port (gate w23_mutation_6cb83f).
 *
 * Upstream: persistence/sqlite/authority/_mutation.py (mutate_runtime_session,
 * receipt key derivation through persistence.authority.runtime).
 *
 * The sqlite handle is the seam: the caller hands in the row facts the write
 * would have read (the epoch singleton, the session row, the stored receipt,
 * the apply_action result) and this file owns the validation ladder, the
 * receipt identity and the commit arithmetic.
 */
#ifndef NOC_W23_MUTATION_6CB83F_H
#define NOC_W23_MUTATION_6CB83F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * (state_meta key, digest) of a mutation request's exact-retry receipt:
 * mutation_receipt_key(principal_id, session_id, request_id, operation=...,
 * payload=..., expected_revision=..., expected_generation=...).
 *
 * req keys: principal_id, session_id, request_id, operation, payload,
 *           expected_revision, expected_generation (absent or null = None).
 * Returns {"key": <str>, "digest": <str>}.
 */
json_t *w23_mutation_6cb83f_receipt_key(const json_t *req);

/*
 * mutate_runtime_session's closed write, over supplied row facts.
 *
 * req keys: epoch, principal_id, session_id, request_id, expected_revision,
 *           operation, payload, expected_generation (absent or null = None),
 *           store: {"epoch": <int>, "session": <row|null>,
 *                   "receipt": "none"|"exact"|"conflict",
 *                   "receipt_result": <stored result value>,
 *                   "affected": [<str>...], "projection": {<key>: <value>}}
 *
 * Returns {"status":"refused","reason":<code>} or
 *         {"status":"ok","result":{...},"gone":null|<event>,"replayed":<bool>}.
 */
json_t *w23_mutation_6cb83f_mutate(const json_t *req);

/*
 * parse_mutation over {operation, payload}: 1 when the pair names a command of
 * the closed union, 0 when it refuses (invalid_params).
 */
int w23_mutation_6cb83f_parse(const json_t *req);

/* _GENERATION_REQUIRED membership: the operations that need expected_generation. */
int w23_mutation_6cb83f_require_generation(const json_t *operation);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W23_MUTATION_6CB83F_H */
