/*
 * run_turn_hygiene.c — C home for upstream Hermes gateway/run_turn_hygiene.py.
 *
 * PoP: w10_runturnhy_0e89de_read_config @ gateway/run_turn_hygiene.py:GatewayTurnHygieneMixin._hmwa_hygiene_read_config
 * PoP: w10_runturnhy_0e89de_bound_payload @ gateway/run_turn_hygiene.py:GatewayTurnHygieneMixin._bound_hygiene_payload
 *
 * The config-to-hygiene-settings projection and the fail-closed payload bound.
 * `hs_in` injects the caller's current HygieneSettings; `data` is the gateway
 * config object; both mirror the live static methods exactly (invalid knob
 * values keep the current value; ceiling can never be tighter than the idle
 * timeout).
 */
#include "noc_w10_runturnhy_0e89de.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val[0] != '\0';
    if (v->type == JSON_ARRAY) return json_len(v) != 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) != 0;
    return 0;
}
static int py_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long long n = strtoll(v->str_val, &end, 10);
        if (end == v->str_val) return 0;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end != '\0') return 0;
        *out = n;
        return 1;
    }
    return 0;
}
static int py_float(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end == v->str_val) return 0;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end != '\0') return 0;
        *out = d;
        return 1;
    }
    return 0;
}
/* Python str(value).lower() for the values compression.enabled may take. */
static int enabled_truthy(const json_t *v) {
    const char *s = NULL;
    char num[64];
    if (!v) s = "true";                       /* default True */
    else if (v->type == JSON_NULL) s = "none";
    else if (v->type == JSON_BOOL) s = v->bool_val ? "true" : "false";
    else if (v->type == JSON_STRING) s = v->str_val;
    else if (v->type == JSON_NUMBER) {
        snprintf(num, sizeof(num), "%.17g", v->num_val);
        s = num;
    } else return 0;
    char low[128];
    size_t i = 0;
    for (; s[i] && i + 1 < sizeof(low); i++) low[i] = (char)tolower((unsigned char)s[i]);
    low[i] = '\0';
    return (strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0);
}
static long long knob_int(const json_t *cc, const char *key, long long current) {
    long long p;
    if (!py_int(json_obj_get(cc, key), &p)) return current;
    return (p > 0) ? p : current;
}
static double knob_float(const json_t *cc, const char *key, double current, int allow_zero) {
    double p;
    if (!py_float(json_obj_get(cc, key), &p)) return current;
    return ((allow_zero ? p >= 0.0 : p > 0.0)) ? p : current;
}

struct json_t *w10_runturnhy_0e89de_read_config(const struct json_t *data, const struct json_t *hs_in) {
    json_t *out = json_object();
    if (!hs_in || hs_in->type != JSON_OBJECT) return out;
    /* copy the incoming settings */
    for (size_t i = 0; i < json_object_size(hs_in); i++)
        json_set(out, json_object_get_key_at(hs_in, i), json_copy(json_object_get_at(hs_in, i)));

    if (data && data->type == JSON_OBJECT) {
        const json_t *mc = json_obj_get(data, "model");
        if (mc && mc->type == JSON_STRING) {
            json_set(out, "model", json_copy((json_t *)mc));
        } else if (mc && mc->type == JSON_OBJECT) {
            const json_t *d = json_obj_get(mc, "default");
            const json_t *m = json_obj_get(mc, "model");
            const json_t *pick = truthy(d) ? d : (truthy(m) ? m : NULL);
            if (pick) json_set(out, "model", json_copy((json_t *)pick));
            const json_t *rc = json_obj_get(mc, "context_length");
            long long iv;
            if (rc && rc->type != JSON_NULL && py_int(rc, &iv))
                json_set(out, "config_context_length", json_number((double)iv));
            const json_t *pv = json_obj_get(mc, "provider");
            json_set(out, "provider",
                     (pv && truthy(pv) && pv->type == JSON_STRING) ? json_string(pv->str_val) : json_null());
            const json_t *bu = json_obj_get(mc, "base_url");
            json_set(out, "base_url",
                     (bu && truthy(bu) && bu->type == JSON_STRING) ? json_string(bu->str_val) : json_null());
        }
    }

    const json_t *cc = data ? json_obj_get(data, "compression") : NULL;
    if (!cc || cc->type != JSON_OBJECT) {
        /* invalid compression config: model knobs already applied, return early */
        return out;
    }
    json_set(out, "compression_enabled", json_bool(enabled_truthy(json_obj_get(cc, "enabled"))));

    double hard = (double)knob_int(cc, "hygiene_hard_message_limit", (long long)json_get_num(hs_in, "hard_msg_limit", 0));
    double timeout = knob_float(cc, "hygiene_timeout_seconds", json_get_num(hs_in, "timeout_seconds", 0), 0);
    double ceiling = knob_float(cc, "hygiene_total_ceiling_seconds", json_get_num(hs_in, "total_ceiling_seconds", 0), 0);
    if (ceiling < timeout) ceiling = timeout;
    double hold = knob_float(cc, "hygiene_max_turn_hold_seconds", json_get_num(hs_in, "max_turn_hold_seconds", 0), 0);
    double cooldown = knob_float(cc, "hygiene_failure_cooldown_seconds", json_get_num(hs_in, "failure_cooldown_seconds", 0), 1);

    json_set(out, "hard_msg_limit", json_number(hard));
    json_set(out, "timeout_seconds", json_number(timeout));
    json_set(out, "total_ceiling_seconds", json_number(ceiling));
    json_set(out, "max_turn_hold_seconds", json_number(hold));
    json_set(out, "failure_cooldown_seconds", json_number(cooldown));
    return out;
}

struct json_t *w10_runturnhy_0e89de_bound_payload(const struct json_t *history, long long limit) {
    json_t *out = json_array();
    if (!history || history->type != JSON_ARRAY) return out;
    size_t len = json_len(history);
    if ((long long)len <= limit) return json_copy((json_t *)history);
    long long head_end = 0;
    while (head_end < (long long)len) {
        const json_t *row = json_get(history, (size_t)head_end);
        if (!row || row->type != JSON_OBJECT) break;
        const char *role = json_get_str(row, "role", "");
        if (strcmp(role, "system") != 0 && strcmp(role, "session_meta") != 0) break;
        head_end++;
    }
    long long cap = limit - 1;
    if (head_end > cap) head_end = cap;
    long long tail_start = (long long)len - (limit - head_end);
    while (tail_start < (long long)len) {
        const json_t *row = json_get(history, (size_t)tail_start);
        if (!row || row->type != JSON_OBJECT) break;
        if (strcmp(json_get_str(row, "role", ""), "tool") != 0) break;
        tail_start++;
    }
    for (long long i = 0; i < head_end; i++) json_append(out, json_copy(json_get(history, (size_t)i)));
    for (long long i = tail_start; i < (long long)len; i++) json_append(out, json_copy(json_get(history, (size_t)i)));
    return out;
}
