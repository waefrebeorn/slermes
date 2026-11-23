/*
 * scheduler_gateway_gate.c — C port of upstream Hermes cron/scheduler_gateway_gate.py.
 *
 * PoP: w14_schedulerg_cccc6c_gateway_unavailable @ cron/scheduler_gateway_gate.py:_gateway_unavailable
 * PoP: w14_schedulerg_cccc6c_refuse_jobs @ cron/scheduler_gateway_gate.py:refuse_agent_jobs_without_gateway
 *
 * The env read (HERMES_TUI_GATEWAY_URL) and discover_gateway_endpoint() are
 * injected seams: the fixture supplies env_value / state / reason_code.
 */
#include "noc_w14_schedulerg_cccc6c.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOWN_HINT \
    "the gateway is not running, so agent jobs cannot fire; they run at the next " \
    "tick once it is up. Start it with `hermes gateway start` " \
    "(or `hermes gateway install` for a service)."

/* Python str.strip() for the ASCII whitespace this gate cares about. */
static int blank(const char *s) {
    if (!s) return 1;
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == '\0';
}

const char *w14_schedulerg_cccc6c_gateway_unavailable(const char *env_value,
                                                     const char *state,
                                                     const char *reason_code) {
    static char buf[4096];
    if (!blank(env_value)) return NULL;      /* explicit remote gateway */
    if (state && !strcmp(state, "ready")) return NULL;
    if (state && !strcmp(state, "absent")) return DOWN_HINT;
    if (state && *state) {
        size_t n = strlen(state);
        if (reason_code && *reason_code) n += strlen(reason_code) + 3;
        n += strlen(", so agent jobs cannot fire; they run at the next tick once it is ready. "
                    "Check it with `hermes gateway status`.") + 16;
        if (n < sizeof(buf)) {
            if (reason_code && *reason_code)
                snprintf(buf, sizeof buf,
                    "the gateway is %s (%s), so agent jobs cannot fire; they run at the next "
                    "tick once it is ready. Check it with `hermes gateway status`.",
                    state, reason_code);
            else
                snprintf(buf, sizeof buf,
                    "the gateway is %s, so agent jobs cannot fire; they run at the next tick "
                    "once it is ready. Check it with `hermes gateway status`.", state);
            return buf;
        }
    }
    return NULL;
}

static int py_truthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && *v->str_val;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* str(id) for the JSON job ids used in fixtures (strings and integral numbers). */
static char *id_str(const struct json_t *v, char *buf, size_t cap) {
    if (!v) { snprintf(buf, cap, "None"); return buf; }
    switch (v->type) {
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); break;
        case JSON_BOOL: snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else snprintf(buf, cap, "%g", v->num_val);
            break;
        case JSON_NULL: snprintf(buf, cap, "None"); break;
        default: snprintf(buf, cap, "null"); break;
    }
    return buf;
}

struct json_t *w14_schedulerg_cccc6c_refuse_jobs(const struct json_t *due_jobs,
                                                const char *env_value,
                                                const char *state,
                                                const char *reason_code) {
    struct json_t *result = json_object();
    struct json_t *jobs = json_array();
    struct json_t *ids = json_array();
    struct json_t *detail = NULL;
    const char *hint;
    size_t i, n = due_jobs && due_jobs->type == JSON_ARRAY ? json_len(due_jobs) : 0;
    size_t agent_count = 0;

    if (!result || !jobs) { json_free(result); json_free(jobs); return NULL; }

    for (i = 0; i < n; i++) {
        const struct json_t *job = json_get(due_jobs, i);
        if (!py_truthy(job ? json_obj_get(job, "no_agent") : NULL)) agent_count++;
    }
    hint = agent_count ? w14_schedulerg_cccc6c_gateway_unavailable(env_value, state, reason_code) : NULL;

    for (i = 0; i < n; i++) {
        const struct json_t *job = json_get(due_jobs, i);
        int no_agent = py_truthy(job ? json_obj_get(job, "no_agent") : NULL);
        if (!hint || no_agent) json_append(jobs, job ? json_copy(job) : json_null());
        if (hint && !no_agent) {
            char idbuf[256];
            json_append(ids, json_string(id_str(job ? json_obj_get(job, "id") : NULL,
                                                idbuf, sizeof idbuf)));
        }
    }
    if (hint) {
        size_t cap = strlen("Skipped: ") + strlen(hint) + 1;
        char *s = (char *)malloc(cap);
        if (s) {
            snprintf(s, cap, "Skipped: %s", hint);
            detail = json_string(s);
            free(s);
        }
    }
    json_set(result, "jobs", jobs);
    if (hint) {
        json_set(result, "refused_ids", ids);
    } else {
        json_free(ids);
        json_set(result, "refused_ids", json_null());
    }
    json_set(result, "detail", detail ? detail : json_null());
    return result;
}
