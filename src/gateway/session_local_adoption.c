/*
 * session_local_adoption.c — C home for upstream Hermes
 * gateway/session_local_adoption.py.
 *
 * PoP: w18_sessionloc_866cb3_binding @ gateway/session_local_adoption.py:_binding
 *
 * Self-contained C11 port of the receipt -> SessionBinding normalization.
 * The SessionBinding constructor is a pure field recorder in the oracle seam;
 * its observable behaviour is the field mapping and defaulting done here.
 */
#include "noc_w18_sessionloc_866cb3.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

struct json_t *w18_sessionloc_866cb3_binding(const struct json_t *receipt) {
    static const char *keys[] = {
        "profile_id", "principal_id", "request_id", "session_id",
        "route", "entry", "policy"
    };
    json_t *out = json_object();
    size_t i;

    if (!receipt || receipt->type != JSON_OBJECT) return out;

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        json_t *v = json_obj_get(receipt, keys[i]);
        /* Python indexes receipt[...] directly; mirror a missing key as null. */
        json_set(out, keys[i], v ? json_copy(v) : json_null());
    }

    /* lineage = tuple(receipt.get('lineage', [receipt['session_id']])) */
    {
        json_t *lin = json_obj_get(receipt, "lineage");
        if (lin && lin->type == JSON_ARRAY) {
            json_set(out, "lineage", json_copy(lin));
        } else {
            json_t *arr = json_array();
            json_t *sid = json_obj_get(receipt, "session_id");
            json_append(arr, sid ? json_copy(sid) : json_null());
            json_set(out, "lineage", arr);
        }
    }

    /* legacy_session_id = receipt.get('legacy_session_id') */
    {
        json_t *legacy = json_obj_get(receipt, "legacy_session_id");
        json_set(out, "legacy_session_id", legacy ? json_copy(legacy) : json_null());
    }
    return out;
}
