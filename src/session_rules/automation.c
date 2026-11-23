/*
 * automation.c — C port of upstream Hermes session_rules/automation.py.
 *
 * PoP: w11_automation_a6ffec_descriptor @ session_rules/automation.py:automation_descriptor
 * PoP: w11_automation_a6ffec_folded_payload @ session_rules/automation.py:folded_payload
 * PoP: w11_automation_a6ffec_goal_contract @ session_rules/automation.py:GoalContract.from_dict
 * PoP: w11_automation_a6ffec_goal_contract_empty @ session_rules/automation.py:GoalContract.is_empty
 * PoP: w11_automation_a6ffec_goal_gate @ session_rules/automation.py:GoalGate.from_dict
 * PoP: w11_automation_a6ffec_heartbeat_state @ session_rules/automation.py:HeartbeatState.from_json
 */
#include "noc_w11_automation_a6ffec.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const CONTRACT_FIELDS[5] = {
    "outcome", "verification", "constraints", "boundaries", "stop_when"
};

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static const char *trimmed(const char *s) {
    static char buf[8192];
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s); if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n); buf[n] = 0;
    return buf;
}

/* Python str(value) for the scalar types the fixtures use. */
static const char *py_str(const json_t *v) {
    static char buf[256];
    if (!v || v->type == JSON_NULL || !truthy(v)) { buf[0] = 0; return buf; }
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return buf;
    }
    buf[0] = 0; return buf;
}

static long long py_int(const json_t *v, long long def) {
    if (!v || !truthy(v)) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}

static double py_float(const json_t *v, double def) {
    if (!v || !truthy(v)) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) return strtod(v->str_val, NULL);
    return def;
}

struct json_t *w11_automation_a6ffec_descriptor(const struct json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return json_object();
    const json_t *local = json_obj_get(payload, "local_automation_v1");
    if (truthy(local)) return json_copy(local);
    const json_t *native = json_obj_get(payload, "native_text_v1");
    if (truthy(native) && native->type == JSON_OBJECT) {
        const json_t *autom = json_obj_get(native, "automation");
        if (truthy(autom)) return json_copy(autom);
    }
    return json_object();
}

struct json_t *w11_automation_a6ffec_folded_payload(const struct json_t *payload) {
    json_t *out = json_copy(payload ? payload : json_object());
    if (!out || out->type != JSON_OBJECT) { if (out && out->type != JSON_OBJECT) json_free(out); return json_object(); }
    json_set(out, "text", json_string("[folded into a later completion batch]"));
    const json_t *local = json_obj_get(out, "local_automation_v1");
    if (truthy(local) && local->type == JSON_OBJECT) {
        json_obj_del((json_t *)local, "identities");
    } else {
        const json_t *native = json_obj_get(out, "native_text_v1");
        if (truthy(native) && native->type == JSON_OBJECT) {
            const json_t *autom = json_obj_get(native, "automation");
            if (truthy(autom) && autom->type == JSON_OBJECT) json_obj_del((json_t *)autom, "identities");
        }
    }
    return out;
}

struct json_t *w11_automation_a6ffec_goal_contract(const struct json_t *data) {
    json_t *out = json_object();
    for (int i = 0; i < 5; i++) {
        const char *v = (data && data->type == JSON_OBJECT) ? trimmed(py_str(json_obj_get(data, CONTRACT_FIELDS[i]))) : "";
        json_set(out, CONTRACT_FIELDS[i], json_string(v));
    }
    return out;
}

int w11_automation_a6ffec_goal_contract_empty(const struct json_t *data) {
    for (int i = 0; i < 5; i++) {
        const char *v = (data && data->type == JSON_OBJECT) ? trimmed(py_str(json_obj_get(data, CONTRACT_FIELDS[i]))) : "";
        if (*v) return 0;
    }
    return 1;
}

struct json_t *w11_automation_a6ffec_goal_gate(const struct json_t *data) {
    json_t *out = json_object();
    const char *command = "";
    long long timeout_s = 300, max_retries = 3, attempts = 0;
    int has_exit = 0;
    long long exit_code = 0;
    const char *tail = "";
    if (data && data->type == JSON_OBJECT) {
        command = py_str(json_obj_get(data, "command"));
        timeout_s = py_int(json_obj_get(data, "timeout_seconds"), 300);
        max_retries = py_int(json_obj_get(data, "max_retries"), 3);
        attempts = py_int(json_obj_get(data, "attempts"), 0);
        const json_t *ec = json_obj_get(data, "last_exit_code");
        if (ec && ec->type != JSON_NULL) { has_exit = 1; exit_code = py_int(ec, 0); }
        tail = py_str(json_obj_get(data, "last_output_tail"));
    }
    json_set(out, "command", json_string(command));
    json_set(out, "timeout_seconds", json_number((double)timeout_s));
    json_set(out, "max_retries", json_number((double)max_retries));
    json_set(out, "attempts", json_number((double)attempts));
    json_set(out, "last_exit_code", has_exit ? json_number((double)exit_code) : json_null());
    json_set(out, "last_output_tail", json_string(tail));
    return out;
}

struct json_t *w11_automation_a6ffec_heartbeat_state(const char *raw) {
    json_t *data = json_parse(raw ? raw : "{}", NULL);
    json_t *out = json_object();
    if (data && data->type == JSON_OBJECT) {
        const char *prompt = py_str(json_obj_get(data, "prompt"));
        long long interval = py_int(json_obj_get(data, "interval_seconds"), 0);
        const json_t *st = json_obj_get(data, "status");
        const char *status = truthy(st) ? py_str(st) : "active";
        double created = py_float(json_obj_get(data, "created_at"), 0.0);
        double fired = py_float(json_obj_get(data, "last_fired_at"), 0.0);
        long long count = py_int(json_obj_get(data, "fire_count"), 0);
        json_set(out, "prompt", json_string(prompt));
        json_set(out, "interval_seconds", json_number((double)interval));
        json_set(out, "status", json_string(status));
        json_set(out, "created_at", json_number(created));
        json_set(out, "last_fired_at", json_number(fired));
        json_set(out, "fire_count", json_number((double)count));
    } else {
        json_set(out, "prompt", json_string(""));
        json_set(out, "interval_seconds", json_number(0));
        json_set(out, "status", json_string("active"));
        json_set(out, "created_at", json_number(0.0));
        json_set(out, "last_fired_at", json_number(0.0));
        json_set(out, "fire_count", json_number(0));
    }
    json_free(data);
    return out;
}
