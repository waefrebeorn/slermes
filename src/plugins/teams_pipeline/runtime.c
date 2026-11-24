/*
 * runtime.c -- real C11 port of upstream Hermes plugins/teams_pipeline/runtime.py.
 *
 * PoP: w22_runtime_49223c_delivery_configured @ plugins/teams_pipeline/runtime.py:_teams_delivery_is_configured
 * PoP: w22_runtime_49223c_config @ plugins/teams_pipeline/runtime.py:build_pipeline_runtime_config
 * PoP: w22_runtime_49223c_bind @ plugins/teams_pipeline/runtime.py:bind_gateway_runtime
 *
 * `build_pipeline_runtime` itself constructs TeamsMeetingPipeline over a live
 * graph client and sqlite store (other C homes), so only the ladder around it
 * is ported: the notification scheduler choice, the recorded runtime error and
 * the three early-return decisions.
 */
#include "hermes_json.h"
#include "noc_w22_runtime_49223c.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Teams platform settings mirrored into teams_delivery (pipeline keys win). */
static const char *const DELIVERY_KEYS[] = {
    "incoming_webhook_url", "access_token", "team_id", "channel_id", "chat_id", NULL
};

static int rt_truthy(const json_t *v) {
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

static json_t *rt_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get((json_t *)obj, key);
}

/* str(value or "") then .strip(): the scalar shapes a config field can hold. */
static char *rt_str_strip(const json_t *v) {
    char buf[64];
    const char *src = "";
    const char *a;
    const char *b;
    size_t n;
    char *out;
    if (rt_truthy(v)) {
        switch (v->type) {
        case JSON_STRING: src = v->str_val ? v->str_val : ""; break;
        case JSON_BOOL:   src = v->bool_val ? "True" : "False"; break;
        case JSON_NUMBER:
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            src = buf;
            break;
        case JSON_ARRAY:  src = "[]"; break;
        case JSON_OBJECT: src = "{}"; break;
        default: src = ""; break;
        }
    }
    a = src;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static char *rt_lower(char *s) {
    char *p;
    for (p = s; p && *p; p++) *p = (char)tolower((unsigned char)*p);
    return s;
}

/* pick(key) = teams_delivery.get(key) or teams_extra.get(key) */
static json_t *rt_pick(const json_t *teams_extra, const json_t *teams_delivery, const char *key) {
    json_t *v = rt_get(teams_delivery, key);
    if (rt_truthy(v)) return v;
    return rt_get(teams_extra, key);
}

static int rt_delivery_configured(const json_t *teams_extra, const json_t *teams_delivery) {
    json_t *mode = rt_get(teams_delivery, "mode");
    char *text;
    int result = 0;

    if (!rt_truthy(mode)) mode = rt_pick(teams_extra, teams_delivery, "delivery_mode");
    text = rt_str_strip(mode);
    if (!text) return 0;
    rt_lower(text);
    if (!strcmp(text, "incoming_webhook")) {
        result = rt_truthy(rt_pick(teams_extra, teams_delivery, "incoming_webhook_url"));
    } else if (!strcmp(text, "graph")) {
        result = rt_truthy(rt_pick(teams_extra, teams_delivery, "chat_id")) ||
                 (rt_truthy(rt_pick(teams_extra, teams_delivery, "team_id")) &&
                  rt_truthy(rt_pick(teams_extra, teams_delivery, "channel_id")));
    }
    free(text);
    return result;
}

json_t *w22_runtime_49223c_delivery_configured(const json_t *teams_extra,
                                              const json_t *teams_delivery) {
    return json_bool(rt_delivery_configured(teams_extra, teams_delivery) ? 1 : 0);
}

json_t *w22_runtime_49223c_config(const json_t *platforms) {
    json_t *teams = rt_get(platforms, "teams");
    json_t *teams_extra = rt_get(teams, "extra");
    json_t *pipeline = rt_get(teams_extra, "meeting_pipeline");
    json_t *out;
    int i;

    if (pipeline && pipeline->type == JSON_OBJECT) {
        out = json_copy(pipeline);            /* dict(pipeline_config_raw or {}) */
    } else {
        out = json_object();                  /* falsy -> {} */
    }
    if (!teams) return out;

    /* `if teams_config and teams_config.enabled:` */
    if (!rt_truthy(rt_get(teams, "enabled"))) return out;
    {
        json_t *raw_delivery = rt_get(out, "teams_delivery");
        json_t *delivery = (raw_delivery && raw_delivery->type == JSON_OBJECT)
                               ? json_copy(raw_delivery) : json_object();
        char *mode = rt_str_strip(rt_get(teams_extra, "delivery_mode"));

        if (mode && mode[0]) json_set(delivery, "mode", json_string(mode));
        free(mode);

        for (i = 0; DELIVERY_KEYS[i]; i++) {
            json_t *value = rt_get(teams_extra, DELIVERY_KEYS[i]);
            /* `value not in {None, ""}`: 0 and False are copied, null/"" are not */
            if (!value || value->type == JSON_NULL) continue;
            if (value->type == JSON_STRING && (!value->str_val || !value->str_val[0])) continue;
            json_set(delivery, DELIVERY_KEYS[i], json_copy(value));
        }
        if (json_object_size(delivery) > 0) {
            json_set(delivery, "enabled",
                     json_bool(rt_delivery_configured(teams_extra, delivery) ? 1 : 0));
            json_set(out, "teams_delivery", delivery);
        } else {
            json_free(delivery);
        }
    }
    return out;
}

json_t *w22_runtime_49223c_bind(const json_t *req) {
    json_t *out = json_object();
    json_t *adapter = rt_get(req, "adapter");
    json_t *runtime = rt_get(req, "runtime");
    json_t *build_error = rt_get(req, "build_error");
    int has_adapter = rt_truthy(adapter);
    int has_runtime = rt_truthy(runtime);
    int failed = build_error && build_error->type != JSON_NULL;

    if (!has_adapter) {
        /* `if adapter is None: return False` -- nothing is touched. */
        json_set(out, "result", json_bool(0));
        json_set(out, "scheduler", json_null());
        json_set(out, "error", json_null());
        json_set(out, "runtime_installed", json_bool(has_runtime ? 1 : 0));
        return out;
    }
    if (has_runtime) {
        json_set(out, "result", json_bool(1));
        json_set(out, "scheduler", json_null());
        json_set(out, "error", json_null());
        json_set(out, "runtime_installed", json_bool(1));
        return out;
    }
    if (failed) {
        /* the drop-scheduler path: Graph still gets its prompt 202 */
        json_set(out, "result", json_bool(0));
        json_set(out, "scheduler", json_string("_drop"));
        json_set(out, "error", json_copy(build_error));
        json_set(out, "runtime_installed", json_bool(0));
        return out;
    }
    json_set(out, "result", json_bool(1));
    json_set(out, "scheduler", json_string("_schedule"));
    json_set(out, "error", json_null());
    json_set(out, "runtime_installed", json_bool(1));
    return out;
}
