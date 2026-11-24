/*
 * routing.c - C home for persistence/gateway/routing.py.
 * Faithful port of the routing-key helpers and the runtime-route projection
 * (gateway_runtime / resume_route) that a resumed session runs.
 * PoP: w19_routing_b4d479_route_namespace @ persistence/gateway/routing.py:route_namespace
 * PoP: w19_routing_b4d479_normalize_topic_profile_name @ persistence/gateway/routing.py:normalize_topic_profile_name
 * PoP: w19_routing_b4d479_route @ persistence/gateway/routing.py:_route
 * PoP: w19_routing_b4d479_gateway_runtime @ persistence/gateway/routing.py:gateway_runtime
 * PoP: w19_routing_b4d479_resume_route @ persistence/gateway/routing.py:resume_route
 * PoP: w19_routing_b4d479_is_empty @ persistence/gateway/routing.py:GatewayRuntime.is_empty
 * PoP: w19_routing_b4d479_constants @ persistence/gateway/routing.py:<module>
 */
#include "noc_w19_routing_b4d479.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ROUTE_KEYS[] = {"provider", "base_url", "api_mode"};
static const char *BARE_BILLING[] = {"auto", "custom"};

/* Python str.strip() over ASCII whitespace. Caller frees. */
static char *py_strip(const char *s) {
    size_t n, a = 0;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    while (a < n && (unsigned char)s[a] <= ' ') a++;
    while (n > a && (unsigned char)s[n - 1] <= ' ') n--;
    out = (char *)malloc(n - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, n - a);
    out[n - a] = '\0';
    return out;
}

static char *py_strdup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* str(value): the string copy, "" for null/absent, a decimal for a number. */
static char *py_str_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return py_strdup("");
    if (v->type == JSON_STRING) return py_strdup(v->str_val);
    if (v->type == JSON_BOOL) return py_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%lld", (long long)d);
        else snprintf(buf, sizeof buf, "%.17g", d);
        return py_strdup(buf);
    }
    return py_strdup("");
}

/* Python truthiness of a JSON value. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

static void ascii_lower(char *s) {
    for (; s && *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* str.split(":") in place: returns the slot count, parts[] points into tmp. */
static int split_colon(char *tmp, char **parts, int max) {
    int n = 0;
    char *p = tmp;
    parts[n++] = p;
    while (*p && n < max) {
        if (*p == ':') {
            *p = '\0';
            parts[n++] = p + 1;
        }
        p++;
    }
    return n;
}

/* route_namespace: the <ns> slot of agent:<ns>:..., "main" outside that shape. */
const char *w19_routing_b4d479_route_namespace(const char *session_key) {
    static char buf[256];
    char tmp[1024];
    char *parts[64];
    int n;
    snprintf(tmp, sizeof tmp, "%s", session_key ? session_key : "");
    n = split_colon(tmp, parts, 64);
    buf[0] = '\0';
    if (n > 2 && !strcmp(parts[0], "agent") && parts[1][0]) snprintf(buf, sizeof buf, "%s", parts[1]);
    else snprintf(buf, sizeof buf, "%s", "main");
    return buf;
}

/* normalize_topic_profile_name: stripped name, empty or missing reads "default". */
const char *w19_routing_b4d479_normalize_topic_profile_name(const char *profile_name) {
    static char buf[512];
    char *s = py_strip(profile_name ? profile_name : "");
    if (!s) return "default";
    if (!s[0]) {
        free(s);
        return "default";
    }
    snprintf(buf, sizeof buf, "%s", s);
    free(s);
    return buf;
}

/* _route(members, keep_falsy): only string members survive; falsy ones only when kept. */
json_t *w19_routing_b4d479_route(json_t *members, int keep_falsy) {
    json_t *out = json_object();
    for (size_t i = 0; i < sizeof ROUTE_KEYS / sizeof ROUTE_KEYS[0]; i++) {
        json_t *v = members ? json_obj_get(members, ROUTE_KEYS[i]) : NULL;
        if (v && v->type == JSON_STRING && (keep_falsy || (v->str_val && v->str_val[0])))
            json_set(out, ROUTE_KEYS[i], json_string(v->str_val));
        else
            json_set(out, ROUTE_KEYS[i], json_null());
    }
    return out;
}

static int bare_billing(const char *provider_lower) {
    for (size_t i = 0; i < sizeof BARE_BILLING / sizeof BARE_BILLING[0]; i++)
        if (!strcmp(provider_lower, BARE_BILLING[i])) return 1;
    return 0;
}

/* gateway_runtime: nested truthy-provider route wins whole; else the top-level picks. */
json_t *w19_routing_b4d479_gateway_runtime(json_t *model_config, const char *billing_provider,
                                           const char *billing_base_url) {
    json_t *raw = (model_config && model_config->type == JSON_OBJECT) ? model_config : NULL;
    json_t *nested = raw ? json_obj_get(raw, "gateway_runtime") : NULL;
    json_t *runtime = (nested && nested->type == JSON_OBJECT) ? nested : NULL;
    json_t *top;
    json_t *out;
    char *provider;

    if (runtime && py_truthy(json_obj_get(runtime, "provider")))
        return w19_routing_b4d479_route(runtime, 1);

    top = json_object();
    if (raw) {
        for (size_t i = 0; i < sizeof ROUTE_KEYS / sizeof ROUTE_KEYS[0]; i++) {
            json_t *v = json_obj_get(raw, ROUTE_KEYS[i]);
            if (py_truthy(v)) json_set(top, ROUTE_KEYS[i], json_copy(v));
        }
    }
    provider = py_strip(billing_provider ? billing_provider : "");
    ascii_lower(provider);
    if (provider[0] && !bare_billing(provider)) {
        char *raw_provider = py_strip(billing_provider ? billing_provider : "");
        if (!json_obj_get(top, "provider")) json_set(top, "provider", json_string(raw_provider));
        free(raw_provider);
    } else if (!strcmp(provider, "custom") && !py_truthy(json_obj_get(top, "provider"))) {
        char *base_url = py_strip(billing_base_url ? billing_base_url : "");
        if (base_url[0]) {
            if (!json_obj_get(top, "base_url")) json_set(top, "base_url", json_string(base_url));
            json_set(top, "provider", json_string("custom"));
        }
        free(base_url);
    }
    free(provider);

    if (json_object_size(top) > 0) out = w19_routing_b4d479_route(top, 0);
    else if (!runtime) out = w19_routing_b4d479_route(NULL, 0);
    else out = w19_routing_b4d479_route(runtime, 1);
    json_free(top);
    return out;
}

static json_t *config_from(json_t *model_config) {
    if (model_config && model_config->type == JSON_OBJECT) return model_config;
    if (model_config && model_config->type == JSON_STRING) {
        json_t *parsed = json_parse(model_config->str_val, NULL);
        if (parsed && parsed->type == JSON_OBJECT) return parsed;
        json_free(parsed);
    }
    return NULL;
}

/* resume_route: the (model, route) pair a resumed session runs, both from one record. */
json_t *w19_routing_b4d479_resume_route(json_t *model_config, const char *model,
                                        const char *billing_provider, const char *billing_base_url) {
    json_t *parsed = NULL;
    json_t *config = config_from(model_config);
    if (config != model_config) parsed = config;
    char *column = py_strip(model ? model : "");
    char *picked = py_strip(py_str_value(config ? json_obj_get(config, "model") : NULL));
    json_t *nested = config ? json_obj_get(config, "gateway_runtime") : NULL;
    json_t *lock = config ? json_obj_get(config, "browser_model_lock") : NULL;
    char *nested_model = py_strip(py_str_value(
        (nested && nested->type == JSON_OBJECT) ? json_obj_get(nested, "model") : NULL));
    char *lock_model = py_strip(py_str_value(
        (lock && lock->type == JSON_OBJECT) ? json_obj_get(lock, "model") : NULL));
    int lock_confirmed = (lock && lock->type == JSON_OBJECT) &&
                         py_truthy(json_obj_get(lock, "confirmed"));
    int same, nested_provider = (nested && nested->type == JSON_OBJECT) &&
                                py_truthy(json_obj_get(nested, "provider"));
    char *chosen = NULL;
    json_t *out = json_object();
    json_t *route;

    if (nested_provider) {
        if (nested_model[0]) chosen = py_strdup(nested_model);
        else if (column[0]) chosen = py_strdup(column);
        else chosen = py_strdup(picked);
    } else if (lock_confirmed && !strcmp(lock_model, column)) {
        if (column[0]) chosen = py_strdup(column);
        else chosen = py_strdup(picked);
    } else {
        if (picked[0]) chosen = py_strdup(picked);
        else chosen = py_strdup(column);
    }
    same = !strcmp(chosen, column);
    route = w19_routing_b4d479_gateway_runtime(config, same ? billing_provider : NULL,
                                               same ? billing_base_url : NULL);
    json_set(out, "model", json_string(chosen));
    json_set(out, "route", route);

    free(column); free(picked); free(nested_model); free(lock_model); free(chosen);
    if (parsed) json_free(parsed);
    return out;
}

/* GatewayRuntime.is_empty: no member is set. */
int w19_routing_b4d479_is_empty(json_t *runtime) {
    for (size_t i = 0; i < sizeof ROUTE_KEYS / sizeof ROUTE_KEYS[0]; i++) {
        json_t *v = runtime ? json_obj_get(runtime, ROUTE_KEYS[i]) : NULL;
        if (v && v->type != JSON_NULL) return 0;
    }
    return 1;
}

/* The module vocabulary the routing verbs are written against. */
json_t *w19_routing_b4d479_constants(void) {
    /* sorted(), as the ported vocabulary is compared: frozensets have no order. */
    static const char *END_REASONS[] = {"daily", "idle", "resume_pending_expired",
                                        "session_reset", "session_switch", "suspended"};
    json_t *o = json_object();
    json_t *reasons = json_array();
    json_t *bare = json_array();
    for (size_t i = 0; i < sizeof END_REASONS / sizeof END_REASONS[0]; i++)
        json_append(reasons, json_string(END_REASONS[i]));
    for (size_t i = 0; i < sizeof BARE_BILLING / sizeof BARE_BILLING[0]; i++)
        json_append(bare, json_string(BARE_BILLING[i]));
    json_set(o, "peer_reset_end_reasons", reasons);
    json_set(o, "end_reason_superseded_by_repair", json_string("superseded_by_repair"));
    json_set(o, "orphan_adoption_max_gap_s", json_number(900.0));
    json_set(o, "bare_billing_providers", bare);
    json_set(o, "topic_schema_meta_key", json_string("telegram_dm_topic_schema_version"));
    return o;
}
