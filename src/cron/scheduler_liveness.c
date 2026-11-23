/*
 * scheduler_liveness.c — C home for upstream Hermes cron/scheduler_liveness.py.
 *
 * PoP: w9_schedulerli_102fcc_watchdog @ cron/scheduler_liveness.py:_inactivity_watchdog_loop
 * PoP: w9_schedulerli_102fcc_stamper @ cron/scheduler_liveness.py:ExecutionProgressStamper.tick
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w9_schedulerli_102fcc.h"

/* Integers cross the C/Python boundary as decimal strings (numbers are double). */
static long long dec_i(const json_t *v, long long def) {
    if (!v || v->type != JSON_STRING || !v->str_val) return def;
    char *end = NULL;
    long long x = strtoll(v->str_val, &end, 10);
    return (end && *end == '\0') ? x : def;
}
static double jan_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* Mirrors: while not stop.wait(poll_s): if future_done(): return False;
 *   idle = float(get_idle_seconds() or 0.0) [except -> 0.0]
 *   if meter.measure(idle) >= limit_s: return True; return False.
 * The fixture injects the fake stop's false-wait count, the future-done index,
 * the idle samples and the limit; the meter is the identity function. */
json_t *w9_schedulerli_102fcc_watchdog(const json_t *in) {
    long long stop_after = dec_i(json_obj_get(in, "stop_after"), 0);
    long long future_at = dec_i(json_obj_get(in, "future_at"), -1);
    double limit = jan_num(json_obj_get(in, "limit"), 0.0);
    const json_t *idles = json_obj_get(in, "idles");
    size_t n = (idles && idles->type == JSON_ARRAY) ? json_len(idles) : 0;
    int result = 0;
    for (long long i = 0; i < stop_after; i++) {
        if (i == future_at) { result = 0; break; }
        double idle = 0.0;
        if (idles && idles->type == JSON_ARRAY && (size_t)i < n) {
            const json_t *sample = json_get(idles, (size_t)i);
            if (sample && sample->type == JSON_NUMBER) idle = sample->num_val;
        }
        if (idle >= limit) { result = 1; break; }
    }
    return json_bool(result != 0);
}

/* Mirrors ExecutionProgressStamper.tick's decision: empty id -> no stamp;
 * now-last < every -> no stamp; idle >= every -> no stamp; otherwise stamp. */
json_t *w9_schedulerli_102fcc_stamper(const json_t *in) {
    const char *eid = json_get_str(in, "execution_id", "");
    double elapsed = jan_num(json_obj_get(in, "elapsed"), 0.0);
    double every = jan_num(json_obj_get(in, "every"), 0.0);
    double idle = jan_num(json_obj_get(in, "idle"), 0.0);
    int stamp = (eid && eid[0]) && !(elapsed < every) && !(idle >= every);
    return json_bool(stamp != 0);
}
