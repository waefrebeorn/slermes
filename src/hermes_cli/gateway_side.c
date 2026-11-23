/*
 * gateway_side.c — faithful C11 port of upstream Hermes hermes_cli/gateway_side.py.
 *
 * PoP: w9_gatewayside_3248c8_is_side_event @ hermes_cli/gateway_side.py:SIDE_EVENTS
 * PoP: w9_gatewayside_3248c8_connection_for @ hermes_cli/gateway_side.py:connection_for
 * PoP: w9_gatewayside_3248c8_start @ hermes_cli/gateway_side.py:start
 * PoP: w9_gatewayside_3248c8_answer @ hermes_cli/gateway_side.py:start.<locals>.wait
 *
 * The connection/store objects are fixture seams; the oracle calls the real
 * start()/wait() closures with a fake connection and inbox so no socket, queue
 * or event loop is involved.
 */
#define _GNU_SOURCE
#include "noc_w9_gatewayside_3248c8.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

static void py_str_into(char *buf, size_t cap, const json_t *v) {
    if (!v || !v->type || v->type == JSON_NULL) { snprintf(buf, cap, ""); return; }
    switch (v->type) {
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else snprintf(buf, cap, "%g", v->num_val);
            return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY: {
            size_t used = 1; buf[0] = '['; buf[1] = '\0';
            for (size_t i = 0; i < json_len(v) && used + 8 < cap; i++) {
                char tmp[256]; py_str_into(tmp, sizeof tmp, json_get(v, i));
                int n = snprintf(buf + used, cap - used, "%s%s", i ? ", " : "", tmp);
                if (n <= 0) break;
                used += (size_t)n;
            }
            snprintf(buf + used, cap - used, "]");
            return;
        }
        case JSON_OBJECT: snprintf(buf, cap, "{}"); return;
        default: snprintf(buf, cap, ""); return;
    }
}

int w9_gatewayside_3248c8_is_side_event(const char *name) {
    if (!name) return 0;
    return !strcmp(name, "background.complete") || !strcmp(name, "btw.complete");
}

int w9_gatewayside_3248c8_connection_for(int has_store, int session_open, int final) {
    return (has_store && session_open && !final) ? 1 : 0;
}

json_t *w9_gatewayside_3248c8_start(const json_t *result, const char *verb) {
    const char *v = (verb && *verb) ? verb : "bg";
    json_t *task_id = json_is_object(result) ? json_obj_get(result, "task_id") : NULL;
    if (!py_truthy(task_id)) {
        char msg[256];
        snprintf(msg, sizeof msg, "prompt.%s returned no task id", v);
        json_t *out = json_object();
        json_set(out, "error", json_string(msg));
        return out;
    }
    json_t *out = json_object();
    json_set(out, "task_id", json_copy(task_id));
    return out;
}

json_t *w9_gatewayside_3248c8_answer(const json_t *payload) {
    json_t *text = json_is_object(payload) ? json_obj_get(payload, "text") : NULL;
    char answer[2048];
    if (py_truthy(text)) py_str_into(answer, sizeof answer, text);
    else answer[0] = '\0';
    json_t *out = json_object();
    if (!strncmp(answer, "error: ", 7))
        json_set(out, "error", json_string(answer + 7));
    else
        json_set(out, "answer", json_string(answer));
    return out;
}

