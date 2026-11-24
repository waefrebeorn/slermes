/*
 * noc_w24_runtimeret_4ec6b1.h - C surface for upstream
 * persistence/sqlite/authority/_runtime_retirement.py (gate w24_runtimeret_4ec6b1).
 *
 * The retirement reads' value layer: the durable identity keys a retried
 * request is recognised by (SHA-256 over the canonical admission fingerprint),
 * the mutation-receipt key + digest the exact-retry check looks up, the
 * generation walk a new derived session id is chosen with, and the fence-keeping
 * verdict a bind records for the generations its walk passed.
 *
 * Every read is a seam: the state_meta / sessions rows arrive as values (the
 * retired id list, the stored identity keys, the session rows, the sweep
 * markers a generation still holds).  No connection is opened here.
 */
#ifndef W24_RUNTIMERET_4EC6B1_H
#define W24_RUNTIMERET_4EC6B1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* identity_key(principal_id, session_id, request_id) ->
 * {"key": "gateway.terminal_identity.v1.<sha256>"} or {"error":"invalid_params"}
 * for a value _text refuses (not a str, empty, or longer than 1024 chars). */
json_t *w24_runtimeret_4ec6b1_identity_key(const json_t *principal_id, const json_t *session_id,
                                           const json_t *request_id);

/* mutation_receipt_key(operation="model", payload={}, expected_revision=0) ->
 * {"key": "gateway.mutation.v1.<sha256>", "digest": "<sha256>"}. */
json_t *w24_runtimeret_4ec6b1_receipt_key(const json_t *principal_id, const json_t *session_id,
                                          const json_t *request_id);

/* first_generation(generations, retired, pinned, identity_keys, rows, start) ->
 * {"generation","sid","row"}: the first non-retired generation from
 * *start* (or a retired one whose pinned identity is stored, which returns with
 * row null).  {"error":"IndexError"} when the walk runs past *generations*. */
json_t *w24_runtimeret_4ec6b1_first_generation(const json_t *generations, const json_t *retired,
                                               const json_t *pinned, const json_t *identity_keys,
                                               const json_t *rows, const json_t *start);

/* keep_passed_fences(generations, session_id, marked) ->
 * {"ok": bool, "unexpired": [<generation ids>], "generations": <count>};
 * ok is False when a passed generation lost its marker. */
json_t *w24_runtimeret_4ec6b1_keep_passed_fences(const json_t *generations, const json_t *session_id,
                                                 const json_t *marked);

/* The durable key prefixes, as an object. */
json_t *w24_runtimeret_4ec6b1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
