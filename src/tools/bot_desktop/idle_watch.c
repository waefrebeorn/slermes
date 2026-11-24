/*
 * idle_watch.c - C11 port of upstream tools/bot_desktop/idle_watch.py
 * (gate w25_idlewatch_8d8b25).
 *
 * PoP: w25_idlewatch_8d8b25_next_check_in @ tools/bot_desktop/idle_watch.py:next_check_in
 * PoP: w25_idlewatch_8d8b25_own_launcher @ tools/bot_desktop/idle_watch.py:_own_launcher
 * PoP: w25_idlewatch_8d8b25_constants @ tools/bot_desktop/idle_watch.py:<constants>
 *
 * The launcher loop, the config file, the process table and os.setsid() are
 * seams.  What is ported here is the arithmetic that decides when the launcher
 * sleeps next and which (pid, create_time) identity it may stop -- a pure
 * function of the limit, the current idle age and the launcher's own argv.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_idlewatch_8d8b25.h"

/* Python's numeric view of a JSON value: int/float/bool; None is none of them. */
static int iw_num(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

json_t *w25_idlewatch_8d8b25_next_check_in(const json_t *req) {
    const json_t *v_idle, *v_def;
    double limit, idle = 0.0, candidate, floor_s, default_minutes;
    int have_idle;

    if (!req || req->type != JSON_OBJECT) return json_null();
    if (!iw_num(json_obj_get(req, "limit"), &limit)) return json_null();

    /* The window is off: the setting is re-read every default window. */
    if (limit <= 0.0) {
        v_def = json_obj_get(req, "default_minutes");
        if (!iw_num(v_def, &default_minutes)) default_minutes =
            (double)W25_IDLEWATCH_8D8B25_DEFAULT_IDLE_STOP_MINUTES;
        return json_number(default_minutes * 60.0);
    }

    /* `idle = idle or 0.0`: None and every falsy value become 0.0. */
    v_idle = json_obj_get(req, "idle");
    have_idle = iw_num(v_idle, &idle);
    if (!have_idle || idle == 0.0) idle = 0.0;

    /* limit - idle if idle < limit else limit */
    candidate = (idle < limit) ? (limit - idle) : limit;
    /* math.ceil(max(min(_MIN_DELAY_S, limit), candidate)) */
    floor_s = (W25_IDLEWATCH_8D8B25_MIN_DELAY_S < limit)
                  ? W25_IDLEWATCH_8D8B25_MIN_DELAY_S : limit;
    if (candidate > floor_s) floor_s = candidate;
    return json_number(ceil(floor_s));
}

json_t *w25_idlewatch_8d8b25_own_launcher(const json_t *req) {
    const json_t *argv, *a0, *v_born;
    const char *pid_s;
    double born = -1.0, b = 0.0;
    long long pid;
    char *end = NULL;
    size_t i, n;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_null();
    argv = json_obj_get(req, "argv");
    n = (argv && argv->type == JSON_ARRAY) ? json_len(argv) : 0;
    a0 = n > 0 ? json_get(argv, 0) : NULL;
    pid_s = (a0 && a0->type == JSON_STRING && a0->str_val) ? a0->str_val : "";
    if (!*pid_s) return json_null();
    /* str.isdigit() */
    for (i = 0; pid_s[i]; i++)
        if (pid_s[i] < '0' || pid_s[i] > '9') return json_null();

    pid = strtoll(pid_s, &end, 10);
    if (end == pid_s) return json_null();
    /* runtime._create_time(pid) returned None -> an identity nothing matches */
    v_born = json_obj_get(req, "born");
    if (iw_num(v_born, &b)) born = b;

    out = json_array();
    json_append(out, json_number((double)pid));
    json_append(out, json_number(born));
    return out;
}

json_t *w25_idlewatch_8d8b25_constants(void) {
    json_t *o = json_object();
    json_set(o, "min_delay_s", json_number(W25_IDLEWATCH_8D8B25_MIN_DELAY_S));
    json_set(o, "default_idle_stop_minutes",
             json_number((double)W25_IDLEWATCH_8D8B25_DEFAULT_IDLE_STOP_MINUTES));
    json_set(o, "default_idle_stop_seconds",
             json_number((double)W25_IDLEWATCH_8D8B25_DEFAULT_IDLE_STOP_MINUTES * 60.0));
    return o;
}
