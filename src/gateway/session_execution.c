/*
 * session_execution.c — C port of upstream Hermes gateway/session_execution.py.
 *
 * PoP: w12_sessionexe_68345d_skip_context_files @ gateway/session_execution.py:GatewaySessionAgentMixin._skip_context_files
 * PoP: w12_sessionexe_68345d_current_message_count @ gateway/session_execution.py:GatewaySessionAgentMixin._current_message_count
 * PoP: w12_sessionexe_68345d_cached_sid_is_dead @ gateway/session_execution.py:GatewaySessionAgentMixin._cached_sid_is_dead
 *
 * Pure decision seams off the turn-execution mixin. The runner/store/agent
 * objects are replaced by JSON seams from the fixture; no process is spawned.
 */
#include "noc_w12_sessionexe_68345d.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return json_object_size(v) > 0 || (json_is_array(v) && json_len(v) > 0);
    }
    return 0;
}

json_t *w12_sessionexe_68345d_skip_context_files(const struct json_t *policy,
                                                   const struct json_t *platforms_cfg,
                                                   const struct json_t *platform_key) {
    /* policy_for_source returns None for every falsy fixture value. */
    if (policy && policy->type == JSON_OBJECT && json_object_size(policy) > 0)
        return json_bool(truthy(json_obj_get(policy, "ignore_rules")) ? 1 : 0);
    /* platforms_cfg = (user_config.get("gateway") or {}).get("platforms") or {} */
    if (!json_is_object(platforms_cfg)) return json_bool(0);
    const char *key = (platform_key && platform_key->type == JSON_STRING) ? platform_key->str_val : "";
    const json_t *entry = json_obj_get(platforms_cfg, key ? key : "");
    if (!json_is_object(entry)) return json_bool(0);
    return json_bool(truthy(json_obj_get(entry, "skip_context_files")) ? 1 : 0);
}

json_t *w12_sessionexe_68345d_current_message_count(const struct json_t *has_store,
                                                      const struct json_t *session_id,
                                                      const struct json_t *row) {
    if (!has_store || has_store->type != JSON_BOOL || !has_store->bool_val) return json_null();
    if (!session_id || session_id->type != JSON_STRING || !session_id->str_val || !session_id->str_val[0])
        return json_null();
    if (!row || row->type != JSON_OBJECT) return json_null();
    const json_t *count = json_obj_get(row, "message_count");
    if (!count || count->type == JSON_NULL) return json_null();
    if (count->type == JSON_NUMBER) return json_number(count->num_val);
    if (count->type == JSON_BOOL) return json_number(count->bool_val ? 1.0 : 0.0);
    if (count->type == JSON_STRING) return json_number(strtod(count->str_val ? count->str_val : "0", NULL));
    return json_null();
}

json_t *w12_sessionexe_68345d_cached_sid_is_dead(const struct json_t *has_lock,
                                                   const struct json_t *cache_entry,
                                                   const struct json_t *session_key,
                                                   const struct json_t *session_id,
                                                   const struct json_t *dead_check) {
    (void)session_key;  /* the seam supplies the already-looked-up entry */
    json_t *peek = json_null();
    if (has_lock && has_lock->type == JSON_BOOL && has_lock->bool_val &&
        json_is_array(cache_entry) && json_len(cache_entry) > 3) {
        const json_t *v = json_get(cache_entry, 3);
        if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) peek = json_copy(v);
    }
    int dead = 0;
    const char *pv = (peek->type == JSON_STRING) ? peek->str_val : NULL;
    const char *sv = (session_id && session_id->type == JSON_STRING && session_id->str_val) ? session_id->str_val : NULL;
    if (pv && sv && strcmp(pv, sv) &&
        dead_check && dead_check->type == JSON_BOOL && dead_check->bool_val)
        dead = 1;
    json_t *out = json_object();
    json_set(out, "peek_sid", peek);
    json_set(out, "dead", json_bool(dead));
    return out;
}

