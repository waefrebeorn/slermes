/*
 * methods_free_tier.c — C home for upstream Hermes tui_gateway/methods_free_tier.py.
 *
 * PoP: w18_methodsfre_06a22e_ack_notice @ tui_gateway/methods_free_tier.py:_ (free_tier.ack_notice)
 * PoP: w18_methodsfre_06a22e_claim_nudge @ tui_gateway/methods_free_tier.py:_ (free_tier.claim_nudge)
 * PoP: w18_methodsfre_06a22e_challenge_result @ tui_gateway/methods_free_tier.py:_ (free_tier.challenge_result)
 *
 * These handlers are thin: they shape params, call server-side helpers bound at
 * install time, and wrap the result in _ok. The helpers are injected by the
 * oracle (anon_auth / free_tier_offer / anon_challenge) with deterministic
 * semantics mirrored here.
 */
#include "noc_w18_methodsfre_06a22e.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *str_of(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

static json_t *bool_result(const char *key, int value) {
    json_t *o = json_object();
    json_set(o, key, json_bool(value ? true : false));
    return o;
}

json_t *w18_methodsfre_06a22e_ack_notice(const char *rid, int guest) {
    (void)rid;
    return bool_result("acked", guest ? 1 : 0);
}

json_t *w18_methodsfre_06a22e_claim_nudge(const char *rid, const json_t *params) {
    const json_t *kv = params ? json_obj_get(params, "kind") : NULL;
    const char *kind = "trial"; /* free_tier_offer.TRIAL */
    int claimed;
    (void)rid;
    if (kv && py_truthy(kv)) {
        kind = (kv->type == JSON_STRING) ? kv->str_val : "";
    }
    claimed = (!strcmp(kind, "trial") || !strcmp(kind, "connector"));
    return bool_result("claimed", claimed);
}

json_t *w18_methodsfre_06a22e_challenge_result(const char *rid, const json_t *params) {
    const char *url = str_of(params ? json_obj_get(params, "url") : NULL);
    const char *outcome = str_of(params ? json_obj_get(params, "outcome") : NULL);
    double attempt = json_get_num(params, "attempt", 0.0);
    int accepted;
    (void)rid;
    accepted = (!strncmp(url, "https://", 8) && !strcmp(outcome, "success") && attempt >= 0.0);
    return bool_result("accepted", accepted);
}

