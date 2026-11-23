/*
 * session_prune.c — C home for upstream Hermes gateway/session_prune.py.
 *
 * PoP: w8_sessionprun_f0679c_filters @ gateway/session_prune.py:_filters
 * PoP: w8_sessionprun_f0679c_prune_guard @ gateway/session_prune.py:prune
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w8_sessionprun_f0679c.h"

/* persistence.housekeeping.maintenance.PRUNE_FILTER_NAMES (live set, verbatim). */
static const char *const W8_PRUNE_FILTER_NAMES[] = {
    "last_active_before", "last_active_after", "started_before", "started_after",
    "source", "title_like", "end_reason", "cwd_prefix", "min_messages", "max_messages",
    "model_like", "provider", "user_id", "chat_id", "chat_type", "branch_like",
    "min_tokens", "max_tokens", "min_cost", "max_cost", "min_tool_calls",
    "max_tool_calls", "archived", "include_pinned", "lineage_tips_only"
};
#define W8_FILTER_N (sizeof(W8_PRUNE_FILTER_NAMES) / sizeof(W8_PRUNE_FILTER_NAMES[0]))

static bool w8_allowed_filter(const char *k) {
    if (!strcmp(k, "older_than_days")) return true;
    for (size_t i = 0; i < W8_FILTER_N; i++)
        if (!strcmp(k, W8_PRUNE_FILTER_NAMES[i])) return true;
    return false;
}

static bool w8_scalar(const json_t *v) {
    return v && (v->type == JSON_NULL || v->type == JSON_BOOL
                 || v->type == JSON_NUMBER || v->type == JSON_STRING);
}

/* _filters: dict, allowed keys only, scalar values only. */
static bool w8_filters_ok(const json_t *raw) {
    if (!raw || raw->type != JSON_OBJECT) return false;
    for (size_t i = 0; i < json_object_size(raw); i++) {
        const char *k = json_object_get_key_at(raw, i);
        json_t *v = json_object_get_at(raw, i);
        if (!k || !w8_allowed_filter(k) || !w8_scalar(v)) return false;
    }
    return true;
}

json_t *w8_sessionprun_f0679c_filters(const struct json_t *raw) {
    bool ok = w8_filters_ok(raw);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(ok));
    json_set(out, "value", ok ? json_copy(raw) : json_null());
    return out;
}

json_t *w8_sessionprun_f0679c_prune_guard(const struct json_t *native_owner,
        const struct json_t *capabilities, const struct json_t *actor_profile,
        const struct json_t *authority_profile, const struct json_t *params) {
    const char *verdict = "ok";
    bool owner = native_owner && native_owner->type == JSON_BOOL && native_owner->bool_val;
    bool has_op = false;
    if (capabilities && capabilities->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(capabilities); i++) {
            json_t *c = json_get(capabilities, i);
            if (c && c->type == JSON_STRING && !strcmp(c->str_val, "session:operator")) {
                has_op = true; break;
            }
        }
    }
    if (!owner || !has_op) verdict = "permission_denied";
    else {
        const char *ap = (actor_profile && actor_profile->type == JSON_STRING) ? actor_profile->str_val : "";
        const char *authp = (authority_profile && authority_profile->type == JSON_STRING) ? authority_profile->str_val : "";
        if (strcmp(ap, authp)) verdict = "profile_mismatch";
        else {
            bool has_filters = params && params->type == JSON_OBJECT
                               && json_obj_get(params, "filters") != NULL;
            json_t *na = (params && params->type == JSON_OBJECT)
                         ? json_obj_get(params, "never_active_days") : NULL;
            bool na_present = na && na->type != JSON_NULL; /* None (missing or null) */
            if (has_filters == na_present) verdict = "invalid_params";
            else if (na_present && (na->type == JSON_BOOL || na->type != JSON_NUMBER))
                verdict = "invalid_params";
            else if (na_present && na->num_val < 0) verdict = "invalid_params";
            else if (has_filters && !w8_filters_ok(json_obj_get(params, "filters")))
                verdict = "invalid_params";
        }
    }
    return json_string(verdict);
}
