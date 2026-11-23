/*
 * stream_max_lifetime.c — C home for upstream Hermes agent/stream_max_lifetime.py.
 *
 * Faithful C11 ports of the stream-lifetime cap resolution and the
 * StreamLifetimeExceeded user-facing message.  Config values and the stale
 * timeout are injected through the args object; no process config is read.
 *
 * PoP: w11_streammax_7580a7_resolve @ agent/stream_max_lifetime.py:resolve_stream_max_lifetime
 * PoP: w11_streammax_7580a7_message @ agent/stream_max_lifetime.py:StreamLifetimeExceeded.__init__
 */
#include "noc_w11_streammax_7580a7.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Python float('inf')/nan have no JSON literal; the fixture injects sentinels. */
static double json_to_double(json_t *v, double def) {
    if (!v) return def;
    if (json_is_number(v)) return v->num_val;
    if (json_is_string(v) && v->str_val) {
        if (!strcmp(v->str_val, "__inf__")) return INFINITY;
        if (!strcmp(v->str_val, "__ninf__")) return -INFINITY;
        if (!strcmp(v->str_val, "__nan__")) return NAN;
    }
    return def;
}

/* PoP: w11_streammax_7580a7_resolve @ agent/stream_max_lifetime.py:resolve_stream_max_lifetime */
json_t *w11_streammax_7580a7_resolve(json_t *a) {
    double configured = 0.0;
    json_t *cfg = a ? json_obj_get(a, "agent_cfg") : NULL;
    if (json_is_object(cfg)) {
        json_t *v = json_obj_get(cfg, "stream_max_lifetime");
        /* isinstance(value, (int, float)) and not isinstance(value, bool) */
        if (json_is_number(v)) configured = v->num_val;
        else if (json_is_string(v)) configured = json_to_double(v, 0.0);
    }
    if (configured <= 0.0 || !isfinite(configured)) return json_number(0.0);
    json_t *st = a ? json_obj_get(a, "stale_timeout") : NULL;
    if (st && !json_is_null(st)) {
        double stale = json_to_double(st, NAN);
        if (isfinite(stale)) return json_number(configured > stale ? configured : stale);
    }
    return json_number(configured);
}

/* PoP: w11_streammax_7580a7_message @ agent/stream_max_lifetime.py:StreamLifetimeExceeded.__init__ */
json_t *w11_streammax_7580a7_message(json_t *a) {
    double elapsed = a ? json_get_num(a, "elapsed", 0.0) : 0.0;
    double cap = a ? json_get_num(a, "cap", 0.0) : 0.0;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "Response still streaming after %.0fs, cut by agent.stream_max_lifetime (%gs). "
        "Raise agent.stream_max_lifetime, or set it to 0 to turn the cap off, if this model "
        "legitimately streams this long.", elapsed, cap);
    return json_string(buf);
}
