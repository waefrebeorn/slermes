/*
 * port_hosted_rooms.c
 *
 * Pure helpers ported from gateway/hosted_rooms.py (67 defs). The module is the
 * hosted-room coordination store; its sqlite / transaction half stays in
 * Python. What is portable:
 *
 *   hr_bounded_int       <- hosted_rooms_common.py:bounded_int (via _bounded_int)
 *   hr_canonical_json    <- hosted_rooms_common.py:canonical_json
 *   hr_claim_values      <- hosted_rooms.py:_claim_values
 *   hr_grant_scope_key   <- hosted_rooms.py:_room_grant_scope_key
 *   hr_reservation_superseded <- hosted_rooms.py:_reservation_superseded
 *   hr_remote_run_identity     <- hosted_rooms.py:_remote_run_identity
 *   hr_validate_members  <- hosted_rooms.py:_validate_members
 *   hr_user_event_id     <- hosted_rooms.py:user_event_id
 *
 * The room-level `_room_id` / `_event_id` / `_actor_json` / `_payload_json`
 * are `partial` aliases over hosted_rooms_common's identifier / canonical_json
 * with fixed labels and bounds - they are covered by those two primitives plus
 * the constants below, so they are not re-implemented here.
 */

#include "port_hosted_rooms.h"

#include <stdlib.h>
#include <string.h>

#include "../lib/libhash/hash.h"
#include "../lib/libjson/json.h"
#include "port_hosted_rooms_common.h"

/* hosted_rooms.py:MAX_MEMBERS / MAX_MEMBERS_JSON_BYTES */
#define HR_MAX_MEMBERS 128
#define HR_MAX_MEMBERS_JSON_BYTES (128 * 1024)
#define HR_MAX_EVENT_JSON_BYTES (256 * 1024)

/* hosted_rooms.py: _actor_json / _payload_json bounds */
#define HR_ACTOR_JSON_MAX_BYTES (4 * 1024)

/* ===========================================================================
 *  bounded_int  (hosted_rooms_common.py:bounded_int)
 *
 *      if isinstance(value, bool) or not isinstance(value, int)
 *         or value < low or (high is not None and value > high):
 *          raise error(message)
 *      return value
 *
 *  `kind` carries the Python type: 0 = not int, 1 = int, 3 = bool (rejected
 *  before the int check - bool is a subclass of int in Python).
 *  Returns 0 / -1; *out receives the value.
 * =========================================================================== */

/* PoP: bounded_int @ gateway/hosted_rooms_common.py:bounded_int */
int hr_bounded_int(int kind, long long ival, const char *message,
                   long long low, int has_high, long long high,
                   long long *out, char **err)
{
    if (out) *out = 0;
    if (err) *err = NULL;
    if (kind == 3 || kind != 1 || ival < low || (has_high && ival > high)) {
        if (err) *err = hrc_fmt("%s", message ? message : "invalid integer");
        return -1;
    }
    if (out) *out = ival;
    return 0;
}

/* ===========================================================================
 *  canonical_json  (hosted_rooms_common.py:canonical_json)
 *
 *      try: encoded = compact_json(value, ensure_ascii=ensure_ascii)
 *      except (TypeError, ValueError, RecursionError) as exc:
 *          raise error(f"{label} must be JSON-serializable") from exc
 *      if len(encoded.encode("utf-8")) > max_bytes:
 *          raise error(f"{label} is too large")
 *      return encoded
 *
 *  `in` is a json_t (already parsed); the "unserializable" case cannot arise
 *  here, so only the byte bound is enforced. Returns 0 / -1.
 * =========================================================================== */

/* PoP: canonical_json @ gateway/hosted_rooms_common.py:canonical_json */
int hr_canonical_json(const json_t *value, const char *label, int max_bytes,
                      char **out, char **err)
{
    if (out) *out = NULL;
    if (err) *err = NULL;
    /* Python's canonical_json(None) -> compact_json(None) -> "null": a NULL
     * input is the JSON null literal, not an unserializable value. */
    char *encoded = value ? hrc_compact_json(value) : strdup("null");
    if (!encoded) {
        if (err) *err = hrc_fmt("%s must be JSON-serializable", label);
        return -1;
    }
    size_t bytes = strlen(encoded);
    if (bytes > (size_t)max_bytes) {
        free(encoded);
        if (err) *err = hrc_fmt("%s is too large", label);
        return -1;
    }
    if (out) *out = encoded; else free(encoded);
    return 0;
}

/* ===========================================================================
 *  _claim_values  (hosted_rooms.py:_claim_values)
 *
 *      return {key: str(claims.get(key) or "") for key in keys}
 *
 *  `claims` is a flat [key, value, ...] array; a NULL value (or a missing key)
 *  becomes "". The result is a flat [key, string, ...] array, keys in the
 *  order given. Caller frees the array and its strings.
 * =========================================================================== */

/* PoP: _claim_values @ gateway/hosted_rooms.py:_claim_values */
char **hr_claim_values(const char *const *claims, const char *const *keys,
                       int nkeys, int *out_count)
{
    char **out = (char **)calloc((size_t)nkeys * 2 + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }

    int k = 0;
    for (int i = 0; i < nkeys; i++) {
        const char *val = NULL;
        for (int j = 0; claims && claims[j]; j += 2) {
            if (strcmp(claims[j], keys[i]) == 0) { val = claims[j + 1]; break; }
        }
        /* str(value or "") - None / "" / 0 / false all become "" */
        out[k++] = strdup(keys[i]);
        out[k++] = strdup((val && val[0]) ? val : "");
    }
    if (out_count) *out_count = k;
    return out;
}

/* ===========================================================================
 *  _room_grant_scope_key  (hosted_rooms.py:_room_grant_scope_key)
 *
 *      fields = _claim_values(claims, ("room_id", "home_install_id",
 *              "authority_gateway_id", "authority_epoch", "member_id",
 *              "target_install_id", "target_profile"))
 *      if not all(fields.values()): raise HostedRoomError("room grant scope is incomplete")
 *      return hashlib.sha256(compact_json(fields).encode("utf-8")).hexdigest()
 *
 *  `claims` is a flat [key, value, ...] array. Returns malloc'd hex, or NULL
 *  with *err set when a field is empty.
 * =========================================================================== */

/* PoP: _room_grant_scope_key @ gateway/hosted_rooms.py:_room_grant_scope_key */
char *hr_grant_scope_key(const char *const *claims, char **err)
{
    if (err) *err = NULL;
    static const char *const fields[] = {
        "room_id", "home_install_id", "authority_gateway_id", "authority_epoch",
        "member_id", "target_install_id", "target_profile",
    };
    const int nf = (int)(sizeof(fields) / sizeof(fields[0]));

    char **vals = hr_claim_values(claims, fields, nf, NULL);
    if (!vals) return NULL;
    for (int i = 0; i < nf; i++) {
        if (!vals[i * 2 + 1] || !vals[i * 2 + 1][0]) {
            if (err) *err = hrc_fmt("room grant scope is incomplete");
            for (int j = 0; j < nf * 2; j++) free(vals[j]);
            free(vals);
            return NULL;
        }
    }
    /* build the compact_json of the fields object */
    json_t *obj = json_object();
    for (int i = 0; i < nf; i++) json_set(obj, fields[i], json_string(vals[i * 2 + 1]));
    char *compact = hrc_compact_json(obj);
    for (int j = 0; j < nf * 2; j++) free(vals[j]);
    free(vals);
    json_free(obj);
    if (!compact) return NULL;

    char *hex = hash_sha256_hex((const unsigned char *)compact, strlen(compact));
    free(compact);
    return hex;
}

/* ===========================================================================
 *  _reservation_superseded  (hosted_rooms.py:_reservation_superseded)
 *
 *      row_epoch = int(row["authority_epoch"])
 *      return row_epoch > epoch
 *          or (row_epoch == epoch and str(row["authority_gateway_id"]) != gateway_id)
 * =========================================================================== */

/* PoP: _reservation_superseded @ gateway/hosted_rooms.py:_reservation_superseded */
int hr_reservation_superseded(long long row_epoch, const char *row_gateway_id,
                              const char *gateway_id, long long epoch)
{
    return row_epoch > epoch ||
           (row_epoch == epoch && (!row_gateway_id || !gateway_id ||
                                   strcmp(row_gateway_id, gateway_id) != 0));
}

/* ===========================================================================
 *  _remote_run_identity  (hosted_rooms.py:_remote_run_identity)
 *
 *      return tuple(record[column] for column in _REMOTE_RUN_IDENTITY_COLUMNS)
 *
 *  `record` is a flat [key, value, ...] array; `columns` the projection order.
 *  Caller frees the array and its strings.
 * =========================================================================== */

/* PoP: _remote_run_identity @ gateway/hosted_rooms.py:_remote_run_identity */
char **hr_remote_run_identity(const char *const *record,
                              const char *const *columns, int ncols,
                              int *out_count, char **err)
{
    if (err) *err = NULL;
    char **out = (char **)calloc((size_t)ncols + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }
    for (int i = 0; i < ncols; i++) {
        const char *val = NULL;
        for (int j = 0; record && record[j]; j += 2) {
            if (strcmp(record[j], columns[i]) == 0) { val = record[j + 1]; break; }
        }
        /* record[column] - a missing column is a KeyError in Python */
        if (!val) {
            if (err) *err = hrc_fmt("%s", columns[i]);
            for (int j = 0; j < i; j++) free(out[j]);
            free(out);
            if (out_count) *out_count = 0;
            return NULL;
        }
        out[i] = strdup(val);
    }
    if (out_count) *out_count = ncols;
    return out;
}

/* ===========================================================================
 *  _validate_members  (hosted_rooms.py:_validate_members)
 *
 *      if not isinstance(value, list): raise HostedRoomError("members must be a list")
 *      if len(value) > MAX_MEMBERS: raise HostedRoomError("too many room members")
 *      if not all(isinstance(member, dict) for member in value):
 *          raise HostedRoomError("each room member must be an object")
 *      members = [dict(member) for member in value]
 *      return members, _canonical_json(members, label="members",
 *                                      max_bytes=MAX_MEMBERS_JSON_BYTES)
 *
 *  `members` is a flat array of json_t* (JSON_OBJECT elements); a NULL entry is
 *  the "not an object" case. *out_json receives the canonical JSON.
 * =========================================================================== */

/* PoP: _validate_members @ gateway/hosted_rooms.py:_validate_members */
int hr_validate_members(json_t **members, int n, char **out_json, char **err)
{
    if (out_json) *out_json = NULL;
    if (err) *err = NULL;

    if (n > HR_MAX_MEMBERS) {
        if (err) *err = hrc_fmt("too many room members");
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (!members[i] || members[i]->type != JSON_OBJECT) {
            if (err) *err = hrc_fmt("each room member must be an object");
            return -1;
        }
    }
    /* build the array copy: [dict(member) for member in value] */
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        char *s = hrc_compact_json(members[i]);
        if (!s) continue;
        json_t *copy = json_parse(s, NULL);
        free(s);
        if (copy) json_append(arr, copy);
    }
    int rc = hr_canonical_json(arr, "members", HR_MAX_MEMBERS_JSON_BYTES, out_json, err);
    json_free(arr);
    return rc;
}

/* ===========================================================================
 *  user_event_id  (hosted_rooms.py:user_event_id)
 *
 *      return f"user:{hashlib.sha256(_event_id(client_event_id).encode('utf-8')).hexdigest()}"
 *
 *  `_event_id` is `identifier(..., label="event_id", max_chars=128)`, i.e. the
 *  stripped, shape-checked value. Caller frees.
 * =========================================================================== */

/* PoP: user_event_id @ gateway/hosted_rooms.py:user_event_id */
char *hr_user_event_id(const char *client_event_id, char **err)
{
    if (err) *err = NULL;
    char *validated = NULL, *verr = NULL;
    if (hrc_identifier(2, client_event_id, "event_id", 128, 1, NULL,
                       &validated, &verr) != 0) {
        if (err) *err = verr ? verr : hrc_fmt("invalid event_id");
        if (!verr) free(*err == NULL ? NULL : NULL);
        return NULL;
    }
    free(verr);
    char *hex = hash_sha256_hex((const unsigned char *)validated, strlen(validated));
    free(validated);
    if (!hex) return NULL;
    size_t n = strlen(hex) + 8;
    char *out = (char *)malloc(n);
    if (!out) { free(hex); return NULL; }
    snprintf(out, n, "user:%s", hex);
    free(hex);
    return out;
}
