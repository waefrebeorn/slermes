/*
 * noc_w24_live_416722.h - C surface for upstream tools/connectors/live.py
 * (gate w24_live_416722).
 *
 * The live-operations registry is a process-wide dict keyed by
 * ``(profile_key, session_key)``; what is ported here is everything that dict
 * decides from values: the key composition (``_profile_key``/``_key``), the four
 * lookups (``current`` / ``get`` / ``get_by_op_id`` / ``find_target`` including
 * its connector/alias/repair_id match), ``open``'s already-open conflict and its
 * message, and ``close``'s identity-guarded removal.
 *
 * Seams (passed in, never called): the registry snapshot itself, the process
 * home key and the active home key -- ``hermes_home_key`` touches the filesystem
 * (resolve/normcase) so its *result* is the input here; the threading lock and
 * the logger are the caller's.
 */
#ifndef W24_LIVE_416722_H
#define W24_LIVE_416722_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _profile_key(profile_home): the home text that keys the registry.  A falsy
 * profile_home means the process home. */
json_t *w24_live_416722_effective_profile_key(const json_t *profile_home,
                                              const json_t *process_key);

/* _key(session_key, profile_home) -> [profile_key, session_key]. */
json_t *w24_live_416722_key_pair(const json_t *session_key, const json_t *profile_home,
                                 const json_t *process_key);

/* ops: [{profile_key, session_key, op_id, settled, targets:[{name,kind,alias,
 * repair_id}]}]; query: {op, session_key, op_id, profile_home, process_key, name,
 * alias, repair_id}.  -> index of the selected operation, or null. */
json_t *w24_live_416722_select(const json_t *ops, const json_t *query);

/* open(operation): {raised, message, stored, op_ids}.  A settled operation at
 * the same key is replaced; an open one raises OperationAlreadyOpen. */
json_t *w24_live_416722_open_result(const json_t *ops, const json_t *query);

/* close(operation): {removed, op_ids}. */
json_t *w24_live_416722_close_result(const json_t *ops, const json_t *query);

/* OperationAlreadyOpen's message for a live op. */
json_t *w24_live_416722_conflict_message(const json_t *session_key, const json_t *op_id);

/* Literal: the op_id length the module mints. */
json_t *w24_live_416722_constants(void);

#ifdef __cplusplus
}
#endif
#endif
