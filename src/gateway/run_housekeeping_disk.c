/*
 * run_housekeeping_disk.c — C port of upstream Hermes gateway/run_housekeeping_disk.py.
 *
 * PoP: w17_runhouseke_16c95d_clear_local_auto_prune_notice @ gateway/run_housekeeping_disk.py:clear_local_auto_prune_notice
 * PoP: w17_runhouseke_16c95d_maybe_post_auto_prune_prompt @ gateway/run_housekeeping_disk.py:maybe_post_auto_prune_prompt
 *
 * Seams (Platform, active_authority, prune_advisor, scheduling) are injected by
 * the oracle from the fixture; this file reproduces the decision logic only.
 */
#include "noc_w17_runhouseke_16c95d.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

#define RH_NOTICE_KEY "sessions.auto_prune"

/* ---- clear_local_auto_prune_notice ---- */
struct json_t *w17_runhouseke_16c95d_clear_local_auto_prune_notice(const struct json_t *fixture) {
    const char *platform = json_get_str(fixture, "platform", "");
    int has_authority = json_get_bool(fixture, "has_authority", false) ? 1 : 0;
    int has_live = json_get_bool(fixture, "has_live", false) ? 1 : 0;
    if (!platform || strcmp(platform, "local") != 0) return json_null();
    if (!has_authority || !has_live) return json_null();

    json_t *out = json_object();
    json_set(out, "chat_id", json_string(json_get_str(fixture, "chat_id", "")));
    json_set(out, "key", json_string(RH_NOTICE_KEY));
    json_set(out, "event_type", json_string("notification.clear"));
    return out;
}

/* ---- maybe_post_auto_prune_prompt ---- */
static int rh_in_reachable(const json_t *transports, const char *profile) {
    for (size_t i = 0; i < json_len(transports); i++) {
        const json_t *t = json_get(transports, i);
        const json_t *p = json_is_array(t) ? json_get(t, 0) : NULL;
        if (!profile) {
            /* Python: None in reachable only when a transport's profile is None */
            if (!p || p->type == JSON_NULL) return 1;
        } else if (p && p->type == JSON_STRING && strcmp(p->str_val, profile) == 0) {
            return 1;
        }
    }
    return 0;
}

static const json_t *rh_lookup(const json_t *obj, const char *key) {
    if (!json_is_object(obj) || !key) return NULL;
    return json_obj_get(obj, key);
}

struct json_t *w17_runhouseke_16c95d_maybe_post_auto_prune_prompt(const struct json_t *fixture) {
    json_t *out = json_object();
    json_t *scheduled = json_array();
    json_t *snoozed = json_array();
    json_set(out, "scheduled", scheduled);
    json_set(out, "snoozed", snoozed);

    if (json_get_bool(fixture, "runner_none", false) || json_get_bool(fixture, "loop_none", false)) return out;

    const json_t *transports = json_obj_get(fixture, "transports");
    const char *routing_home = json_get_str(fixture, "routing_home", "");
    const json_t *served = json_obj_get(fixture, "served_profile_homes");
    const json_t *should_ask = json_obj_get(fixture, "should_ask");
    const json_t *advice = json_obj_get(fixture, "advice");
    int future_none = json_get_bool(fixture, "future_none", false) ? 1 : 0;
    int future_result = json_get_bool(fixture, "future_result", false) ? 1 : 0;

    /* homes = {None: routing_home}; homes.update(served_profile_homes) */
    /* First entry has a null profile key (the routing process home). */
    const char *homes_key[64];
    const char *homes_val[64];
    size_t nh = 0;
    if (nh < 64) { homes_key[nh] = NULL; homes_val[nh] = routing_home; nh++; }
    if (json_is_object(served)) {
        for (size_t i = 0; i < json_object_size(served) && nh < 64; i++) {
            const char *k = json_object_get_key_at(served, i);
            const json_t *v = json_object_get_at(served, i);
            homes_key[nh] = k;
            homes_val[nh] = (v && v->type == JSON_STRING) ? v->str_val : "";
            nh++;
        }
    }

    for (size_t i = 0; i < nh; i++) {
        const char *profile = homes_key[i];
        const char *home = homes_val[i];
        if (!rh_in_reachable(transports, profile)) continue;
        const json_t *sa = rh_lookup(should_ask, home);
        if (!sa || sa->type == JSON_NULL) continue;
        const json_t *ad = rh_lookup(advice, home);
        if (!ad || ad->type == JSON_NULL) continue;

        json_append(scheduled, profile ? json_string(profile) : json_null());
        if (!future_none && future_result) json_append(snoozed, json_string(home));
    }
    return out;
}
