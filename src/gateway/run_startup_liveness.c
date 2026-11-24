/*
 * run_startup_liveness.c — C port of upstream Hermes gateway/run_startup_liveness.py.
 *
 * PoP: w16_runstartup_6b6bfb_liveness_plan @ gateway/run_startup_liveness.py:start_loop_liveness_guards
 *
 * The selector-floor/watchdog arming is I/O; the gated slice is the deterministic
 * decision plan (config gate, handle presence, float()/int() parameter coercion).
 */
#include "noc_w16_runstartup_6b6bfb.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}
static double coerce_float(const json_t *v, double def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) return strtod(v->str_val ? v->str_val : "", NULL);
    return def;
}
static long long coerce_int(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val ? v->str_val : "", NULL, 10);
    return def;
}

json_t *w16_runstartup_6b6bfb_liveness_plan(const json_t *config, const json_t *floor_handle,
                                            const json_t *watchdog) {
    json_t *out = json_object();
    int enabled = 1;
    if (json_is_object(config)) {
        const json_t *lv = json_obj_get(config, "loop_watchdog");
        if (lv && !py_truthy(lv)) enabled = 0;
    }
    json_set(out, "enabled", json_bool(enabled));
    if (!enabled) {
        json_set(out, "arm_floor", json_bool(false));
        json_set(out, "arm_watchdog", json_bool(false));
        json_set(out, "probe_interval", json_null());
        json_set(out, "probe_timeout", json_null());
        json_set(out, "max_strikes", json_null());
        return out;
    }
    int arm_floor = (!floor_handle || json_is_null(floor_handle)) ? 1 : 0;
    int alive = json_is_object(watchdog) && json_get_bool(watchdog, "alive", false) ? 1 : 0;
    int arm_watchdog = !alive;
    json_set(out, "arm_floor", json_bool(arm_floor));
    json_set(out, "arm_watchdog", json_bool(arm_watchdog));
    if (arm_watchdog) {
        const json_t *cfg = json_is_object(config) ? config : NULL;
        double interval = coerce_float(cfg ? json_obj_get(cfg, "loop_watchdog_probe_interval_s") : NULL, 30.0);
        double timeout  = coerce_float(cfg ? json_obj_get(cfg, "loop_watchdog_probe_timeout_s") : NULL, 10.0);
        long long strikes = coerce_int(cfg ? json_obj_get(cfg, "loop_watchdog_max_strikes") : NULL, 3);
        json_set(out, "probe_interval", json_number(interval));
        json_set(out, "probe_timeout", json_number(timeout));
        json_set(out, "max_strikes", json_number((double)strikes));
    } else {
        json_set(out, "probe_interval", json_null());
        json_set(out, "probe_timeout", json_null());
        json_set(out, "max_strikes", json_null());
    }
    return out;
}
