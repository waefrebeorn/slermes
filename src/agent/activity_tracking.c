/*
 * activity_tracking.c — C home for upstream Hermes agent/activity_tracking.py.
 *
 * Faithful C11 models of ActivityTrackingMixin's observable state transitions.
 * Time, description binding, provenance normalisation and store availability
 * are injected through the args object so the differential oracle can replay
 * the exact Python branches without live I/O.
 *
 * PoP: w9_activitytra_4f9ab0_touch @ agent/activity_tracking.py:ActivityTrackingMixin._touch_activity
 * PoP: w9_activitytra_4f9ab0_persist_if_due @ agent/activity_tracking.py:ActivityTrackingMixin._persist_session_activity_if_due
 * PoP: w9_activitytra_4f9ab0_reset_labels @ agent/activity_tracking.py:ActivityTrackingMixin._reset_activity_labels_after_turn
 */
#include "noc_w9_activitytra_4f9ab0.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v)
{
    if (!v || json_is_null(v)) return 0;
    if (json_is_bool(v)) return json_bool_value(v) ? 1 : 0;
    if (json_is_number(v)) return json_number_value(v) != 0;
    if (json_is_string(v)) { const char *s = json_string_value(v); return s && *s; }
    if (json_is_array(v) || json_is_object(v)) return json_len(v) != 0;
    return 0;
}

/* PoP: w9_activitytra_4f9ab0_touch @ agent/activity_tracking.py:ActivityTrackingMixin._touch_activity */
json_t *w9_activitytra_4f9ab0_touch(json_t *a)
{
    long long gen = 0;
    json_t *g = json_obj_get(a, "gen");
    if (json_is_number(g)) gen = (long long)json_number_value(g);
    gen++; /* getattr(..., 0) + 1 */

    double ts = json_get_num(a, "ts", 0);
    const char *bound = json_get_str(a, "bound_desc", json_get_str(a, "desc", ""));
    const char *prov = json_get_str(a, "resolved_provenance", "unknown");
    int force = json_get_bool(a, "force_persist", 0) ? 1 : 0;
    int terminal = json_get_bool(a, "is_terminal", 0) ? 1 : 0;

    json_t *out = json_object();
    json_set(out, "generation", json_number((double)gen));
    json_set(out, "last_activity_ts", json_number(ts));
    json_set(out, "last_activity_desc", json_string(bound));
    json_set(out, "last_activity_provenance", json_string(prov));
    json_set(out, "abort_claim", json_null());
    json_set(out, "reset_window", json_bool(force || terminal));
    json_set(out, "persist_called", json_bool(1));
    return out;
}

/* PoP: w9_activitytra_4f9ab0_persist_if_due @ agent/activity_tracking.py:ActivityTrackingMixin._persist_session_activity_if_due */
json_t *w9_activitytra_4f9ab0_persist_if_due(json_t *a)
{
    int session_ok = py_truthy(json_obj_get(a, "session_id"));
    int has_db = json_get_bool(a, "has_session_db", 0) ? 1 : 0;
    int has_touch = json_get_bool(a, "has_touch", 0) ? 1 : 0;
    double now_mono = json_get_num(a, "now_mono", 0);
    double last_mono = json_get_num(a, "last_mono", 0);
    double interval = json_get_num(a, "interval", 60);

    int touched = 0;
    double new_last = last_mono;
    if (session_ok && has_db && has_touch) {
        if (!last_mono || (now_mono - last_mono) >= interval) {
            touched = 1;
            new_last = now_mono;
        }
    }
    json_t *out = json_object();
    json_set(out, "touched", json_bool(touched));
    json_set(out, "new_last_mono", json_number(new_last));
    return out;
}

/* PoP: w9_activitytra_4f9ab0_reset_labels @ agent/activity_tracking.py:ActivityTrackingMixin._reset_activity_labels_after_turn */
json_t *w9_activitytra_4f9ab0_reset_labels(json_t *a)
{
    int session_ok = py_truthy(json_obj_get(a, "session_id"));
    int has_db = json_get_bool(a, "has_session_db", 0) ? 1 : 0;
    int has_clear = json_get_bool(a, "has_clear", 0) ? 1 : 0;
    int clear_called = session_ok && has_db && has_clear;

    json_t *out = json_object();
    json_set(out, "last_activity_desc", json_string(""));
    json_set(out, "last_activity_provenance", json_string("unknown"));
    json_set(out, "clear_called", json_bool(clear_called));
    return out;
}
