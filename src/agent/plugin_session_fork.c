/*
 * plugin_session_fork.c — C home for upstream Hermes agent/plugin_session_fork.py.
 *
 * Faithful C11 ports of the module's pure numeric helpers.  Python's int/bool
 * distinction is honoured (bool is NOT an int), ints may arrive as JSON numbers
 * or decimal strings, and the timeout clamp mirrors float()/min() semantics.
 *
 * PoP: w11_pluginsess_a37456_bounded_int @ agent/plugin_session_fork.py:_bounded_int
 * PoP: w11_pluginsess_a37456_clamped_timeout @ agent/plugin_session_fork.py:_clamped_timeout
 * PoP: w11_pluginsess_a37456_constants @ agent/plugin_session_fork.py:<module>
 */
#include "noc_w11_pluginsess_a37456.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TIMEOUT_SECONDS 1800.0

/* Python: isinstance(value, int) and not isinstance(value, bool).
 * JSON numbers that are integral map to Python int; decimal strings are
 * parsed with strtoll (int64-as-decimal-string pitfall). */
static int py_int(json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL || v->type == JSON_NULL ||
        v->type == JSON_ARRAY || v->type == JSON_OBJECT) return 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d) || d != trunc(d)) return 0;   /* float, not int */
        if (d > 9.22e18 || d < -9.22e18) return 0;
        *out = (long long)d;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        if (!s) return 0;
        errno = 0;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end != '\0') return 0;                    /* partial parse */
        *out = x;
        return 1;
    }
    return 0;
}

/* Python: value clamped to [low, high]; high=None disables the upper bound. */
json_t *w11_pluginsess_a37456_bounded_int(json_t *a) {
    long long value, low = 0, high = 0;
    json_t *jv = a ? json_obj_get(a, "value") : NULL;
    json_t *jl = a ? json_obj_get(a, "low") : NULL;
    json_t *jh = a ? json_obj_get(a, "high") : NULL;
    if (!py_int(jv, &value)) return json_null();
    if (!py_int(jl, &low)) return json_null();
    int has_high = py_int(jh, &high);
    if (value < low) value = low;
    if (has_high && value > high) value = high;
    return json_number((double)value);
}

/* Python: float(timeout); None on TypeError/ValueError; min(v,1800) when
 * v > 0, else None (NaN and <=0 included). */
json_t *w11_pluginsess_a37456_clamped_timeout(json_t *a) {
    json_t *v = a ? json_obj_get(a, "timeout") : NULL;
    double d;
    if (!v) return json_null();
    if (v->type == JSON_NULL || v->type == JSON_ARRAY || v->type == JSON_OBJECT)
        return json_null();
    if (v->type == JSON_BOOL) {
        d = v->bool_val ? 1.0 : 0.0;             /* Python float(True) == 1.0 */
    } else if (v->type == JSON_NUMBER) {
        d = v->num_val;
    } else if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        d = strtod(s, &end);
        if (end == s) return json_null();
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end != '\0') return json_null();
    } else {
        return json_null();
    }
    if (!(d > 0.0)) return json_null();               /* NaN / <= 0 */
    return json_number(d < MAX_TIMEOUT_SECONDS ? d : MAX_TIMEOUT_SECONDS);
}

/* Module-level constants (mirrored from plugin_session_fork.py). */
json_t *w11_pluginsess_a37456_constants(json_t *a) {
    (void)a;
    json_t *o = json_object();
    json_set(o, "PLUGIN_FORK_TASK", json_string("plugin_fork"));
    json_set(o, "DEFAULT_MAX_ITERATIONS", json_number(8));
    json_set(o, "MAX_ITERATIONS", json_number(16));
    json_set(o, "MAX_PROMPT_CHARS", json_number(8192));
    json_set(o, "MAX_TIMEOUT_SECONDS", json_number(1800.0));
    return o;
}
