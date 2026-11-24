/*
 * agent_runtime_helpers_dead_connections.c - C11 port of upstream
 * agent/agent_runtime_helpers_dead_connections.py (gate w25_agentrunti_e1ad40).
 *
 * PoP: w25_agentrunti_e1ad40_socket_probe @ agent/agent_runtime_helpers_dead_connections.py:_socket_is_dead
 * PoP: w25_agentrunti_e1ad40_cleanup @ agent/agent_runtime_helpers_dead_connections.py:cleanup_dead_connections
 * PoP: w25_agentrunti_e1ad40_constants @ agent/agent_runtime_helpers_dead_connections.py:logger
 *
 * The pools are a seam: httpcore's private socket lists, the non-blocking
 * recv(MSG_PEEK|MSG_DONTWAIT) probe and the two teardown hooks stay in the
 * caller.  What is ported is the sweep: which pools exist (the request cache's
 * client is skipped when it IS the primary), what a probe classified a socket
 * as, how an exception in one pool must not stop the other, the advisory that
 * names the dead pools, and which ownership-aware teardown each one gets.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_agentrunti_e1ad40.h"

#define DC_PRIMARY 0
#define DC_REQUEST 1

static int dc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Membership in the plan's ["primary"|"request", ...] error fans. */
static int dc_has(const json_t *list, const char *label) {
    size_t i, n;
    if (!list || list->type != JSON_ARRAY) return 0;
    n = json_len(list);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(list, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, label) == 0) return 1;
    }
    return 0;
}

static void dc_log(json_t *logs, const char *level, const char *message) {
    json_t *row = json_array();
    json_append(row, json_string(level));
    json_append(row, json_string(message));
    json_append(logs, row);
}

json_t *w25_agentrunti_e1ad40_socket_probe(const json_t *kind) {
    const char *k;
    if (!kind || kind->type != JSON_STRING || !kind->str_val) return json_null();
    k = kind->str_val;
    if (!strcmp(k, "eof")) return json_bool(1);              /* recv peek == b"": peer FIN */
    if (!strcmp(k, "data")) return json_bool(0);             /* bytes waiting: healthy */
    if (!strcmp(k, "blocking")) return json_bool(0);         /* BlockingIOError: healthy */
    if (!strcmp(k, "oserror")) return json_bool(1);          /* OSError: treat as dead */
    if (!strcmp(k, "setblocking_error")) return json_bool(1); /* raised before the peek */
    return json_null();
}

json_t *w25_agentrunti_e1ad40_cleanup(const json_t *plan) {
    static const char *const LABEL[2] = {"primary", "request"};
    const char *dead_labels[2];
    int dead_counts[2];
    int present[2];
    int ndead = 0, total = 0, cleaned = 0, i;
    const json_t *dead;
    json_t *out, *actions, *logs;

    if (!plan || plan->type != JSON_OBJECT) return json_null();
    out = json_object();
    actions = json_array();
    logs = json_array();

    present[DC_PRIMARY] = dc_truthy(json_obj_get(plan, "primary")) ? 1 : 0;
    present[DC_REQUEST] = (dc_truthy(json_obj_get(plan, "request_client")) &&
                           !dc_truthy(json_obj_get(plan, "same"))) ? 1 : 0;
    dead = json_obj_get(plan, "dead");

    for (i = 0; i < 2; i++) {
        char msg[128];
        int count;
        if (!present[i]) continue;
        if (dc_has(json_obj_get(plan, "probe_error"), LABEL[i])) {
            snprintf(msg, sizeof msg, "Dead connection check error for %s client", LABEL[i]);
            dc_log(logs, "DEBUG", msg);
            continue;
        }
        count = dead && dead->type == JSON_OBJECT
                    ? (int)json_get_num(dead, LABEL[i], 0.0) : 0;
        if (count > 0) {
            dead_labels[ndead] = LABEL[i];
            dead_counts[ndead] = count;
            ndead++;
        }
    }

    if (ndead > 0) {
        char msg[256];
        char joined[64] = "";
        size_t used = 0;
        for (i = 0; i < ndead; i++) {
            size_t need = strlen(dead_labels[i]);
            if (i > 0) { snprintf(joined + used, sizeof joined - used, ", "); used += 2; }
            snprintf(joined + used, sizeof joined - used, "%s", dead_labels[i]);
            used += need;
            total += dead_counts[i];
        }
        /* Find %d dead connection(s) in %s client pool(s) — cleaning up */
        snprintf(msg, sizeof msg,
                 "Found %d dead connection(s) in %s client pool(s) \xE2\x80\x94 cleaning up",
                 total, joined);
        dc_log(logs, "WARNING", msg);

        for (i = 0; i < ndead; i++) {
            if (dc_has(json_obj_get(plan, "cleanup_error"), dead_labels[i])) {
                char err[128];
                snprintf(err, sizeof err, "Dead connection cleanup error for %s client", dead_labels[i]);
                dc_log(logs, "DEBUG", err);
                continue;
            }
            if (strcmp(dead_labels[i], LABEL[DC_PRIMARY]) == 0) {
                json_append(actions, json_string(
                    "primary:_replace_primary_openai_client:dead_connection_cleanup"));
            } else {
                json_append(actions, json_string(
                    "request:_close_cached_request_openai_client:dead_connection_cleanup"));
            }
        }
        cleaned = 1;
    }

    json_set(out, "cleaned", json_bool(cleaned));
    json_set(out, "actions", actions);
    json_set(out, "logs", logs);
    return out;
}

json_t *w25_agentrunti_e1ad40_constants(void) {
    json_t *o = json_object();
    json_set(o, "logger", json_string("run_agent"));
    json_set(o, "cleanup_reason", json_string("dead_connection_cleanup"));
    return o;
}
