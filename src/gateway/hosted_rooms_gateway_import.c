/*
 * hosted_rooms_gateway_import.c — C home for upstream Hermes gateway/hosted_rooms_gateway_import.py.
 * PoP: w26_hostedroom_f8aecd_source_for @ gateway/hosted_rooms_gateway_import.py:source_for
 * PoP: w26_hostedroom_f8aecd_handoff_id @ gateway/hosted_rooms_gateway_import.py:handoff_gateway_id
 * PoP: w26_hostedroom_f8aecd_import_plan @ gateway/hosted_rooms_gateway_import.py:import_after_ready
 * PoP: w26_hostedroom_f8aecd_constants @ gateway/hosted_rooms_gateway_import.py:<module constants>
 *
 * A standalone backend's Group Chats move into the gateway in two crash-safe
 * steps (fence the source, then copy and claim).  Both stores, the root
 * ``shared-state.db`` file and the ready-import thread are seams; what is
 * ported here is what the values decide: whether a store has a source beside
 * it, which authority a fenced room is handed to, and which room services get
 * the post-ready pass (a service that is missing, or that already ran it, does
 * not).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_hostedroom_f8aecd.h"

/* Module-level literal constants mirrored from the Python source. */
static const char *const HOSTEDROOM_SOURCE_NAME = "shared-state.db";
static const char *const HOSTEDROOM_HANDOFF_SUFFIX = ":gateway-store";
static const char *const HOSTEDROOM_THREAD_NAME = "hosted-rooms-ready-import";
static const char *const HOSTEDROOM_WARN_TEMPLATE =
    "hosted rooms: could not import the standalone Group Chats from %s (%s); "
    "will retry on the next start";
static const char *const HOSTEDROOM_INFO_TEMPLATE =
    "hosted rooms: imported %d standalone Group Chat(s) from %s";

/* Python truthiness of a JSON value. */
static int hr_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

json_t *w26_hostedroom_f8aecd_source_for(const char *target_name, int sibling_exists) {
    if (!target_name || strcmp(target_name, "state.db") != 0) return json_null();
    if (!sibling_exists) return json_null();
    return json_string(HOSTEDROOM_SOURCE_NAME);
}

json_t *w26_hostedroom_f8aecd_handoff_id(const char *local_id) {
    size_t n = local_id ? strlen(local_id) : 0;
    size_t s = strlen(HOSTEDROOM_HANDOFF_SUFFIX);
    char *buf = (char *)malloc(n + s + 1);
    json_t *out;
    if (!buf) return json_null();
    if (n) memcpy(buf, local_id, n);
    memcpy(buf + n, HOSTEDROOM_HANDOFF_SUFFIX, s + 1);
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w26_hostedroom_f8aecd_import_plan(const json_t *authorities) {
    json_t *out = json_array();
    size_t i, n;
    if (!authorities || authorities->type != JSON_ARRAY) return out;
    n = json_len(authorities);
    for (i = 0; i < n; i++) {
        const json_t *entry = json_get(authorities, i);
        int has_service = 0, ready_started = 0, start;
        json_t *row;
        if (entry && entry->type == JSON_OBJECT) {
            has_service = hr_truthy(json_obj_get(entry, "has_service"));
            ready_started = hr_truthy(json_obj_get(entry, "ready_started"));
        }
        /* service is None -> skip; service._ready_import_started truthy -> skip */
        start = has_service && !ready_started;
        row = json_object();
        json_set(row, "index", json_number((double)i));
        json_set(row, "start", json_bool(start ? true : false));
        json_append(out, row);
    }
    return out;
}

json_t *w26_hostedroom_f8aecd_constants(void) {
    json_t *o = json_object();
    json_set(o, "source_name", json_string(HOSTEDROOM_SOURCE_NAME));
    json_set(o, "handoff_suffix", json_string(HOSTEDROOM_HANDOFF_SUFFIX));
    json_set(o, "thread_name", json_string(HOSTEDROOM_THREAD_NAME));
    json_set(o, "warn_template", json_string(HOSTEDROOM_WARN_TEMPLATE));
    json_set(o, "info_template", json_string(HOSTEDROOM_INFO_TEMPLATE));
    return o;
}
