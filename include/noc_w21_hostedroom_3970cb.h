/*
 * noc_w21_hostedroom_3970cb.h -- public C API for the port of
 *   gateway/hosted_room_authority_history.py (wave 21 batch 5).
 *
 * Bounded replay context for committed authority changes: the pure half of the
 * module. `validate_history` (the span chain a room carries), `at_sequence`
 * (which span owns an event sequence) and `origin_gateway_id` (the namespace a
 * chair change must not move) all run here; `read_history_locked` stays
 * upstream because it executes SQL inside the caller's room-state transaction.
 *
 * Every entry point returns {"status":"ok","value":...} on success and
 * {"status":"error","type":"<ExcName>","message":"<str(exc)>"} on the error the
 * live code raises (AuthorityHistoryError, or KeyError from a room lacking the
 * key it indexes).
 */
#ifndef NOC_W21_HOSTEDROOM_3970CB_H
#define NOC_W21_HOSTEDROOM_3970CB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* hosted_room_authority_history.MAX_AUTHORITY_SPANS */
#define W21_HOSTEDROOM_3970CB_MAX_SPANS 1024

/* PoP: w21_hostedroom_3970cb_identifier @ gateway/hosted_rooms_common.py:identifier
 * `value.strip()`, non-empty, at most max_chars long and fullmatch
 * ^[A-Za-z0-9][A-Za-z0-9._:-]*$ (pattern=None skips the shape check). Returns
 * the stripped text, or the error text identifier() raises for `label`. */
json_t *w21_hostedroom_3970cb_identifier(const json_t *value, const char *label,
                                         long long max_chars, int check_shape);

/* PoP: w21_hostedroom_3970cb_validate_history @ gateway/hosted_room_authority_history.py:validate_history
 * `value` is the room's authority_history (missing/None -> one span at
 * from_seq 0); `gateway_id`/`epoch` are the current owner. Value on success is
 * the list of {"gateway_id","epoch","from_seq"} mappings. */
json_t *w21_hostedroom_3970cb_validate_history(const json_t *value, const json_t *gateway_id,
                                               const json_t *epoch);

/* PoP: w21_hostedroom_3970cb_at_sequence @ gateway/hosted_room_authority_history.py:at_sequence
 * The last span whose from_seq is <= seq; error when seq precedes them all. */
json_t *w21_hostedroom_3970cb_at_sequence(const json_t *history, const json_t *seq);

/* PoP: w21_hostedroom_3970cb_origin_gateway_id @ gateway/hosted_room_authority_history.py:origin_gateway_id
 * The first span's gateway id of room["authority_history"], validated against
 * room["authority_gateway_id"]/room["authority_epoch"] (KeyError when a key the
 * live code indexes is absent). */
json_t *w21_hostedroom_3970cb_origin_gateway_id(const json_t *room);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_HOSTEDROOM_3970CB_H */
