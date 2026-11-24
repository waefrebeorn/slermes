/*
 * run_restored_delivery.c — C port of upstream Hermes gateway/run_restored_delivery.py.
 *
 * PoP: w17_runrestore_0acd81_resume_pending_candidates @ gateway/run_restored_delivery.py:resume_pending_candidates
 *
 * All runner seams (session store, primary adapters, restart-loop guard) are
 * injected by the oracle from fixture data; this file reproduces the exact
 * filter / guard decision and short-circuit order.
 */
#include "noc_w17_runrestore_0acd81.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static int rr_in_strings(const json_t *arr, const char *v) {
    if (!v || !json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *x = json_get(arr, i);
        if (x && x->type == JSON_STRING && strcmp(x->str_val, v) == 0) return 1;
    }
    return 0;
}

struct json_t *w17_runrestore_0acd81_resume_pending_candidates(const struct json_t *fx) {
    const json_t *pnode = json_obj_get(fx, "platform");
    const char *platform = (pnode && pnode->type == JSON_STRING) ? pnode->str_val : NULL;
    int is_relay = platform && strcmp(platform, "relay") == 0;
    int has_fronts = is_relay && json_get_bool(fx, "relay_fronts", false) ? 1 : 0;
    const json_t *fronts_platforms = json_obj_get(fx, "fronts_platforms");
    const json_t *entries = json_obj_get(fx, "entries");
    const json_t *reasons = json_obj_get(fx, "auto_reasons");

    /* with runner.session_store._lock: _ensure_loaded_locked() -- failures -> None */
    if (json_get_bool(fx, "lock_raises", false) || json_get_bool(fx, "ensure_raises", false))
        return json_null();

    json_t *candidates = json_array();
    size_t n = json_is_array(entries) ? json_len(entries) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        if (!json_is_object(e)) continue;
        if (!json_get_bool(e, "resume_pending", false)) continue;
        if (json_get_bool(e, "suspended", false)) continue;
        const json_t *origin = json_obj_get(e, "origin");
        if (!origin || origin->type == JSON_NULL) continue;   /* entry.origin is None */
        const char *oplatform;
        if (origin->type == JSON_STRING) oplatform = origin->str_val;
        else if (json_is_object(origin)) oplatform = json_get_str(origin, "platform", "");
        else continue;
        if (!rr_in_strings(reasons, json_get_str(e, "resume_reason", ""))) continue;

        int platform_ok = 1;
        if (platform != NULL && !(oplatform && strcmp(oplatform, platform) == 0)) {
            platform_ok = has_fronts && rr_in_strings(fronts_platforms, oplatform);
        }
        if (!platform_ok) continue;
        json_append(candidates, json_string(json_get_str(e, "session_key", "")));
    }

    /* Only boots with interrupted sessions count; a tripped boot still serves inbound. */
    if (json_len(candidates) > 0 && !json_get_bool(fx, "guard_raises", false)) {
        if (json_get_bool(fx, "guard_trip", false)) {
            json_free(candidates);
            return json_null();
        }
    }
    return candidates;
}
