/*
 * free_tier_offer.c — C port of upstream Hermes hermes_cli/free_tier_offer.py.
 *
 * PoP: w13_freetiero_db4384_constants @ hermes_cli/free_tier_offer.py (module constants)
 * PoP: w13_freetiero_db4384_due_at @ hermes_cli/free_tier_offer.py:_due_at
 * PoP: w13_freetiero_db4384_due_in @ hermes_cli/free_tier_offer.py:_due_in
 * PoP: w13_freetiero_db4384_eligible @ hermes_cli/free_tier_offer.py:_eligible
 *
 * _clock(), _eligible() and anon_auth state are injected by the fixture; no
 * process clock or environment is read directly.
 */
#include "noc_w13_freetiero_db4384.h"
#include "hermes_json.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Module-level literal constants mirrored from the Python source. */
#define OFFER_DELAY_S 180.0
#define REOFFER0 3600.0
#define REOFFER1 14400.0
#define REOFFER2 86400.0

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        default: return v->c.count > 0;
    }
}

static double as_double(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) return strtod(v->str_val, NULL);
    return 0.0;
}

struct json_t *w13_freetiero_db4384_constants(void) {
    json_t *o = json_object();
    json_set(o, "OFFER_DELAY_S", json_number(OFFER_DELAY_S));
    json_t *a = json_array();
    json_append(a, json_number(REOFFER0));
    json_append(a, json_number(REOFFER1));
    json_append(a, json_number(REOFFER2));
    json_set(o, "REOFFER_AFTER_S", a);
    json_set(o, "TRIAL", json_string("trial"));
    json_set(o, "CONNECTOR", json_string("connector"));
    json_set(o, "CONNECTED_APP_FLAG", json_string("has_connected_app"));
    return o;
}

struct json_t *w13_freetiero_db4384_due_at(const struct json_t *state) {
    if (!json_is_object(state) || json_object_size(state) == 0) return json_null();
    const json_t *first = json_obj_get(state, "first_task_at");
    double first_v = as_double(first);
    const json_t *ov = json_obj_get(state, "offers");
    long long offers = 0;
    if (ov && ov->type == JSON_NUMBER) offers = (long long)ov->num_val;
    else if (ov && ov->type == JSON_STRING) offers = strtoll(ov->str_val, NULL, 10);
    if (offers == 0) return json_number(first_v + OFFER_DELAY_S);
    const json_t *lt = json_obj_get(state, "last_task_at");
    double last_task = truthy(lt) ? as_double(lt) : first_v;
    const json_t *lo = json_obj_get(state, "last_offered_at");
    double last_offer = truthy(lo) ? as_double(lo) : 0.0;
    if (last_task <= last_offer) return json_null();
    int idx = (int)(offers < 3 ? offers : 3) - 1;
    double reoffer = REOFFER2;
    if (idx == 0) reoffer = REOFFER0;
    else if (idx == 1) reoffer = REOFFER1;
    double a = last_task + OFFER_DELAY_S, b = last_offer + reoffer;
    return json_number(a > b ? a : b);
}

struct json_t *w13_freetiero_db4384_due_in(const struct json_t *state, int eligible, double now) {
    json_t *due = w13_freetiero_db4384_due_at(state);
    if (due->type == JSON_NULL || !eligible) { json_free(due); return json_null(); }
    double c = ceil(due->num_val - now);
    json_free(due);
    return json_number(c < 0.0 ? 0.0 : c);
}

struct json_t *w13_freetiero_db4384_eligible(const char *kind, const struct json_t *flag,
                                             int is_guest, int guest_allowed, int free_active) {
    if (!strcmp(kind, "trial")) return json_bool(free_active ? true : false);
    int flag_true = flag && flag->type == JSON_BOOL && flag->bool_val;
    return json_bool(is_guest && flag_true && guest_allowed && !free_active ? true : false);
}
