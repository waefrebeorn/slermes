/*
 * gateway_supervised_restart.c -- real C11 port of the decisions of upstream
 * Hermes hermes_cli/gateway_supervised_restart.py.
 *
 * PoP: w20_gatewaysup_735d69_declares_external_supervisor @ hermes_cli/gateway_supervised_restart.py:gateway_declares_external_supervisor
 * PoP: w20_gatewaysup_735d69_replacement_pid @ hermes_cli/gateway_supervised_restart.py:_wait_for_supervised_gateway_replacement
 * PoP: w20_gatewaysup_735d69_start_mode @ hermes_cli/gateway_supervised_restart.py:start_restart_replacement
 *
 * The control socket, psutil/`ps` argv capture, the pidfile and the restart CLI
 * stay upstream: their answers are injected, and the policy those answers feed
 * (which supervisor owns the PID, is a replacement fresh, how the fallback
 * starts) is reproduced here.
 */
#include "hermes_json.h"
#include "noc_w20_gatewaysup_735d69.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GW735D_VERIFY_TIMEOUT 15.0
#define GW735D_FLOOR_TIMEOUT 0.5
#define GW735D_EXTERNAL_MARKER "--external-supervisor"

double w20_gatewaysup_735d69_verify_timeout(void) {
    return GW735D_VERIFY_TIMEOUT;
}

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int gw735d_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* A PID-shaped value: `106.0` and 106 are the same PID; a bool or a string is
 * not a PID at all. */
static int gw735d_pid(const json_t *v, double *out) {
    if (!v || v->type != JSON_NUMBER) return 0;
    if (!isfinite(v->num_val)) return 0;
    *out = v->num_val;
    return 1;
}

/* launch_supervisor(identity): the first non-empty launch value that is not the
 * folded "none", else None. */
static const char *gw735d_launch_supervisor(const json_t *identity) {
    static const char *const KEYS[] = {"launch_supervisor", "supervisor"};
    int i;
    if (!identity || identity->type != JSON_OBJECT) return NULL;
    for (i = 0; i < 2; i++) {
        json_t *value = json_obj_get((json_t *)identity, KEYS[i]);
        if (value && value->type == JSON_STRING && value->str_val && value->str_val[0] &&
            strcmp(value->str_val, "none") != 0) {
            return value->str_val;
        }
    }
    return NULL;
}

static int gw735d_array_has(const json_t *arr, const char *needle) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < arr->c.count; i++) {
        const json_t *item = arr->c.items[i];
        if (item && item->type == JSON_STRING && item->str_val &&
            strcmp(item->str_val, needle) == 0) {
            return 1;
        }
    }
    return 0;
}

/* --- gateway_declares_external_supervisor --------------------------------- */

int w20_gatewaysup_735d69_declares_external_supervisor(const json_t *pid,
                                                        const json_t *identity,
                                                        const json_t *argv,
                                                        const json_t *record) {
    double pid_value = 0.0;
    const char *supervisor;
    const json_t *effective_argv = argv;

    if (!gw735d_pid(pid, &pid_value) || pid_value <= 1.0) return 0;

    /* identity.get("pid") == pid and launch_supervisor(identity) not in (None, "manual") */
    if (identity && identity->type == JSON_OBJECT) {
        json_t *answer_pid = json_obj_get((json_t *)identity, "pid");
        double other = 0.0;
        if (gw735d_pid(answer_pid, &other) && other == pid_value) {
            supervisor = gw735d_launch_supervisor(identity);
            if (supervisor && strcmp(supervisor, "manual") != 0) return 1;
        }
    }

    /* else the argv the gateway stamped into gateway_state.json */
    if (!effective_argv || effective_argv->type == JSON_NULL) {
        effective_argv = NULL;
        if (record && record->type == JSON_OBJECT) {
            json_t *record_pid = json_obj_get((json_t *)record, "pid");
            double other = 0.0;
            if (gw735d_pid(record_pid, &other) && other == pid_value) {
                effective_argv = json_obj_get((json_t *)record, "argv");
            }
        }
    }
    if (!gw735d_truthy(effective_argv)) return 0;   /* bool(argv) */
    return gw735d_array_has(effective_argv, GW735D_EXTERNAL_MARKER);
}

/* --- _wait_for_supervised_gateway_replacement ----------------------------- */

/* _fresh_pid: pid is not None and pid > 0 and pid != old_pid */
static int gw735d_fresh(const json_t *value, double old_pid, double *out) {
    double v = 0.0;
    if (!gw735d_pid(value, &v)) return 0;
    if (!(v > 0.0)) return 0;
    if (v == old_pid) return 0;
    *out = v;
    return 1;
}

json_t *w20_gatewaysup_735d69_replacement_pid(const json_t *old_pid,
                                              const json_t *observed,
                                              const json_t *timeout,
                                              const json_t *poll_interval) {
    double old = 0.0;
    double budget = GW735D_VERIFY_TIMEOUT;      /* timeout=None */
    double interval = 0.5;
    double deadline, now = 0.0;
    size_t index = 0;
    size_t total = (observed && observed->type == JSON_ARRAY) ? observed->c.count : 0;

    gw735d_pid(old_pid, &old);
    if (timeout && timeout->type == JSON_NUMBER && isfinite(timeout->num_val)) {
        budget = timeout->num_val;
    }
    if (budget < GW735D_FLOOR_TIMEOUT) budget = GW735D_FLOOR_TIMEOUT;  /* max(timeout, 0.5) */
    if (poll_interval && poll_interval->type == JSON_NUMBER &&
        isfinite(poll_interval->num_val)) {
        interval = poll_interval->num_val;
    }
    deadline = now + budget;

    for (;;) {
        const json_t *answer = NULL;
        double fresh = 0.0;
        double remaining;
        if (total > 0) {
            size_t at = index < total ? index : total - 1;   /* the last value repeats */
            answer = observed->c.items[at];
        }
        index++;
        if (gw735d_fresh(answer, old, &fresh)) return json_number(fresh);
        remaining = deadline - now;
        if (remaining <= 0.0) return json_null();           /* poll_until's falsy timeout */
        now += (interval < remaining) ? interval : remaining;
    }
}

/* --- start_restart_replacement ------------------------------------------- */

json_t *w20_gatewaysup_735d69_start_mode(const json_t *args, int force,
                                         const char *home, int detach_code) {
    json_t *out = json_object();
    int detach = 0;
    if (args && args->type == JSON_OBJECT) {
        detach = gw735d_truthy(json_obj_get((json_t *)args, "detach"));
    }
    if (detach) {
        json_set(out, "mode", json_string("detach"));
        json_set(out, "home", json_string(home ? home : ""));
        json_set(out, "force", json_bool(force ? 1 : 0));
        json_set(out, "code", json_number((double)detach_code));
    } else {
        json_set(out, "mode", json_string("foreground"));
        json_set(out, "force", json_bool(force ? 1 : 0));
        json_set(out, "verbose", json_number(0.0));
    }
    return out;
}
