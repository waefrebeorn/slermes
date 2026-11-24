/*
 * gateway_runtime_progress.c -- real C11 port of the adaptive startup deadline
 * of upstream Hermes hermes_cli/gateway_runtime_progress.py.
 *
 * PoP: w20_gatewayrun_1cb73c_run @ hermes_cli/gateway_runtime_progress.py:StartRunway
 * PoP: w20_gatewayrun_1cb73c_constants @ hermes_cli/gateway_runtime_progress.py:MAX_ADAPTIVE_TIMEOUT
 *
 * StartRunway is the whole module's arithmetic: a base deadline, a hard cap,
 * and a progress window that pushes the deadline forward one lead at a time
 * while the start's owner accrues CPU.  The only I/O it does is read two seams
 * -- the monotonic clock and the owner's CPU seconds (plus which pid is the
 * owner) -- and this port takes both as data, which is exactly what makes the
 * class unit-testable on any host.
 */
#include "noc_w20_gatewayrun_1cb73c.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define GR1CB73C_MAX_ADAPTIVE_TIMEOUT 300.0
#define GR1CB73C_PROGRESS_WINDOW_S 15.0
#define GR1CB73C_MIN_CPU_PER_WINDOW_S 0.1
#define GR1CB73C_LEAD_S (GR1CB73C_PROGRESS_WINDOW_S + 5.0)
#define GR1CB73C_SAMPLE_EVERY_S 1.0

typedef struct {
    int adaptive;
    double deadline;
    double cap;
    double sampled;
    int has_window;
    double win_pid;      /* exact integer pids ride here as doubles < 2^53 */
    double win_when;
    double win_cpu;
} gr1cb73c_runway;

static int gr1cb73c_truthy(const json_t *v) {
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

/* A pid/CPU seam value: absent or null means "no reading". */
static int gr1cb73c_num(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end != v->str_val) { *out = d; return 1; }
    }
    return 0;
}

json_t *w20_gatewayrun_1cb73c_constants(void) {
    json_t *out = json_object();
    json_set(out, "max_adaptive_timeout", json_number(GR1CB73C_MAX_ADAPTIVE_TIMEOUT));
    json_set(out, "progress_window_s", json_number(GR1CB73C_PROGRESS_WINDOW_S));
    json_set(out, "min_cpu_per_window_s", json_number(GR1CB73C_MIN_CPU_PER_WINDOW_S));
    json_set(out, "lead_s", json_number(GR1CB73C_LEAD_S));
    json_set(out, "sample_every_s", json_number(GR1CB73C_SAMPLE_EVERY_S));
    return out;
}

/* __init__: -> 0, -1 for its ValueError, -2 for the TypeError math.isfinite raises. */
static int gr1cb73c_init(gr1cb73c_runway *r, const json_t *timeout, double default_s,
                         double now) {
    int adaptive = (!timeout || timeout->type == JSON_NULL);
    double t = default_s;

    if (!adaptive) {
        if (timeout->type == JSON_BOOL) {
            t = timeout->bool_val ? 1.0 : 0.0;
        } else if (timeout->type == JSON_NUMBER) {
            t = timeout->num_val;
        } else {
            return -2;   /* math.isfinite(<non-number>) */
        }
    }
    if (!isfinite(t) || t <= 0) return -1;

    memset(r, 0, sizeof(*r));
    r->adaptive = adaptive;
    r->deadline = now + t;
    r->cap = now + (t > GR1CB73C_MAX_ADAPTIVE_TIMEOUT ? t : GR1CB73C_MAX_ADAPTIVE_TIMEOUT);
    r->sampled = 0.0;
    r->has_window = 0;
    return 0;
}

/* extend(): -> the deadline after one pending iteration. */
static double gr1cb73c_extend(gr1cb73c_runway *r, int homes_nonempty, int has_child,
                              int has_pid, double pid, int has_cpu, double cpu,
                              double now) {
    int throttled = (now - r->sampled < GR1CB73C_SAMPLE_EVERY_S) && (now < r->deadline);

    if (!r->adaptive || (!homes_nonempty && !has_child) || throttled) return r->deadline;
    r->sampled = now;

    if (!has_pid || !has_cpu || !r->has_window || r->win_pid != pid) {
        /* No reading, or a new owner: open a fresh window, claim nothing yet. */
        if (has_pid && has_cpu) {
            r->has_window = 1;
            r->win_pid = pid;
            r->win_when = now;
            r->win_cpu = cpu;
        } else {
            r->has_window = 0;
        }
    } else if (now - r->win_when >= GR1CB73C_PROGRESS_WINDOW_S) {
        if (cpu - r->win_cpu >= GR1CB73C_MIN_CPU_PER_WINDOW_S) {
            double target = now + GR1CB73C_LEAD_S;
            double pushed = r->deadline > target ? r->deadline : target;
            r->deadline = pushed < r->cap ? pushed : r->cap;
        }
        r->win_when = now;
        r->win_cpu = cpu;
    }
    return r->deadline;
}

json_t *w20_gatewayrun_1cb73c_run(const json_t *timeout, double default_s,
                                  const json_t *steps) {
    gr1cb73c_runway r;
    json_t *out;
    double init_now = 0.0;
    size_t i;

    /* The clock reading at construction: the first step's "now", else 0. */
    if (steps && steps->type == JSON_ARRAY && steps->c.count > 0) {
        json_t *first = steps->c.items[0];
        if (first && first->type == JSON_OBJECT) {
            json_t *n = json_obj_get(first, "now");
            if (!gr1cb73c_num(n, &init_now)) init_now = 0.0;
        }
    }
    {
        int status = gr1cb73c_init(&r, timeout, default_s, init_now);
        if (status != 0) {
            const char *tname = "str";
            char msg[96];
            out = json_object();
            json_set(out, "status", json_string("raised"));
            if (status == -2) {
                switch (timeout ? timeout->type : JSON_NULL) {
                case JSON_OBJECT: tname = "dict"; break;
                case JSON_ARRAY:  tname = "list"; break;
                default:          tname = "str"; break;
                }
                snprintf(msg, sizeof(msg), "must be real number, not %s", tname);
                json_set(out, "type", json_string("TypeError"));
                json_set(out, "message", json_string(msg));
            } else {
                json_set(out, "type", json_string("ValueError"));
                json_set(out, "message", json_string("timeout must be finite and positive"));
            }
            return out;
        }
    }

    out = json_object();
    json_set(out, "status", json_string("ok"));
    {
        json_t *arr = json_array();
        for (i = 0; steps && steps->type == JSON_ARRAY && i < steps->c.count; i++) {
            json_t *st = steps->c.items[i];
            double now = 0.0, pid = 0.0, cpu = 0.0;
            int has_pid, has_cpu, homes_nonempty, has_child = 0;
            json_t *child;
            if (!st || st->type != JSON_OBJECT) continue;
            gr1cb73c_num(json_obj_get(st, "now"), &now);
            has_pid = gr1cb73c_num(json_obj_get(st, "owner_pid"), &pid);
            has_cpu = gr1cb73c_num(json_obj_get(st, "cpu"), &cpu);
            homes_nonempty = gr1cb73c_truthy(json_obj_get(st, "homes"));
            child = json_obj_get(st, "child");
            if (child && child->type != JSON_NULL) has_child = 1;
            json_append(arr, json_number(gr1cb73c_extend(&r, homes_nonempty, has_child,
                                                        has_pid, pid, has_cpu, cpu, now)));
        }
        json_set(out, "deadlines", arr);
    }
    return out;
}
