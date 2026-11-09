/*
 * port_hosted_rooms.h -- pure helpers ported from gateway/hosted_rooms.py.
 *
 * The sqlite / transaction half stays in Python.
 */
#ifndef PORT_HOSTED_ROOMS_H
#define PORT_HOSTED_ROOMS_H

#include <stddef.h>

#include "../lib/libjson/json.h"

/* hosted_rooms_common.py:bounded_int -- reject bools, non-ints and values
 * outside [low, high]. `kind`: 0 = not int, 1 = int, 3 = bool (rejected
 * before the int check). Returns 0 / -1; *err receives the exact message. */
int hr_bounded_int(int kind, long long ival, const char *message,
                   long long low, int has_high, long long high,
                   long long *out, char **err);

/* hosted_rooms_common.py:canonical_json -- compact_json bounded by max_bytes
 * of UTF-8. Returns 0 / -1; *out receives the encoded JSON. */
int hr_canonical_json(const json_t *value, const char *label, int max_bytes,
                      char **out, char **err);

/* hosted_rooms.py:_claim_values -- {key: str(claims.get(key) or "")}. Returns
 * a flat [key, value, ...] array in the order given (caller frees). */
char **hr_claim_values(const char *const *claims, const char *const *keys,
                       int nkeys, int *out_count);

/* hosted_rooms.py:_room_grant_scope_key -- sha256 hex of the compact JSON of
 * the seven scope fields; NULL with *err when any field is empty. */
char *hr_grant_scope_key(const char *const *claims, char **err);

/* hosted_rooms.py:_reservation_superseded -- a newer epoch, or the same epoch
 * under another gateway, outranks this claim. */
int hr_reservation_superseded(long long row_epoch, const char *row_gateway_id,
                              const char *gateway_id, long long epoch);

/* hosted_rooms.py:_remote_run_identity -- the record's columns projected into
 * a tuple, in order. Caller frees the array and its strings. */
char **hr_remote_run_identity(const char *const *record,
                              const char *const *columns, int ncols,
                              int *out_count, char **err);

/* hosted_rooms.py:_validate_members -- at most MAX_MEMBERS (128) objects, then
 * the canonical JSON bounded by MAX_MEMBERS_JSON_BYTES (128 KiB). */
int hr_validate_members(json_t **members, int n, char **out_json, char **err);

/* hosted_rooms.py:user_event_id -- "user:" + sha256 hex of the validated event
 * id. Caller frees. */
char *hr_user_event_id(const char *client_event_id, char **err);

#endif /* PORT_HOSTED_ROOMS_H */
