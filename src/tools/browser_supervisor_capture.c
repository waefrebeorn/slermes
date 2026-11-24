/*
 * browser_supervisor_capture.c - real C11 port of the pure admission logic in upstream Hermes
 *   tools/browser_supervisor_capture.py.
 * The event loop, WebSocket and thread scheduling stay in Python; what is ported is the
 * connection-validity predicate, the invalid-handle message, call()'s ordered admission checks
 * and capture()'s supervisor/page preconditions.
 *
 * PoP: w19_browsersup_1c8a2d_invalid_message @ tools/browser_supervisor_capture.py:CapturedCDP._invalid
 * PoP: w19_browsersup_1c8a2d_is_valid @ tools/browser_supervisor_capture.py:CapturedCDP.is_valid
 * PoP: w19_browsersup_1c8a2d_call_gate @ tools/browser_supervisor_capture.py:CapturedCDP.call
 * PoP: w19_browsersup_1c8a2d_capture_error @ tools/browser_supervisor_capture.py:capture
 */
#include "hermes_json.h"
#include "noc_w19_browsersup_1c8a2d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s) + 1;
    o = (char *)malloc(n);
    if (!o) return NULL;
    memcpy(o, s, n);
    return o;
}

/* Python repr() of a str: single quotes unless the text itself carries one. */
static char *repr_str(const char *s) {
    const char *src = s ? s : "";
    int has_single = strchr(src, '\'') != NULL;
    int has_double = strchr(src, '"') != NULL;
    char quote = (has_single && !has_double) ? '"' : '\'';
    size_t n = strlen(src);
    char *out = (char *)malloc(n * 2 + 3);
    size_t i, p = 0;
    if (!out) return dupstr("''");
    out[p++] = quote;
    for (i = 0; i < n; i++) {
        char c = src[i];
        if (c == quote || c == '\\') out[p++] = '\\';
        out[p++] = c;
    }
    out[p++] = quote;
    out[p] = '\0';
    return out;
}

static char *format_task(const char *fmt_prefix, const char *task_id) {
    char *repr = repr_str(task_id);
    int n = snprintf(NULL, 0, "%s%s", fmt_prefix, repr ? repr : "''");
    char *buf;
    if (n < 0) { free(repr); return dupstr(""); }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { free(repr); return dupstr(""); }
    snprintf(buf, (size_t)n + 1, "%s%s", fmt_prefix, repr ? repr : "''");
    free(repr);
    return buf;
}

char *w19_browsersup_1c8a2d_invalid_message(const char *task_id) {
    char *tail = format_task("captured CDP connection for task ", task_id);
    int n = snprintf(NULL, 0,
                     "%s is gone (supervisor reconnected, stopped or was replaced); capture again",
                     tail ? tail : "");
    char *out;
    if (n < 0) { free(tail); return dupstr(""); }
    out = (char *)malloc((size_t)n + 1);
    if (!out) { free(tail); return dupstr(""); }
    snprintf(out, (size_t)n + 1,
             "%s is gone (supervisor reconnected, stopped or was replaced); capture again",
             tail ? tail : "");
    free(tail);
    return out;
}

json_t *w19_browsersup_1c8a2d_is_valid(int registry_same, int active, int stop_requested,
                                       int ws_same, int loop_running) {
    int ok = registry_same && active && !stop_requested && ws_same && loop_running;
    return json_bool(ok != 0);
}

/* Python str(number) for the timeout message. */
static char *num_str(double d) {
    char tmp[64];
    if (d == (double)(long long)d && fabs(d) < 1e15) snprintf(tmp, sizeof tmp, "%lld", (long long)d);
    else snprintf(tmp, sizeof tmp, "%.15g", d);
    return dupstr(tmp);
}

static json_t *gate_result(const char *error_class, const char *message) {
    json_t *out = json_object();
    json_set(out, "error", error_class ? json_string(error_class) : json_null());
    json_set(out, "message", message ? json_string(message) : json_null());
    return out;
}

json_t *w19_browsersup_1c8a2d_call_gate(const json_t *timeout, double timeout_max,
                                        int before_send_kind, int valid, int loop_misuse,
                                        const char *method, const char *task_id) {
    if (timeout && timeout->type != JSON_NULL) {
        int bad = 0;
        if (timeout->type == JSON_BOOL) bad = 1;
        else if (timeout->type != JSON_NUMBER) bad = 1;
        else {
            double t = timeout->num_val;
            if (t != t || t > 1.7e308 || t <= 0 || t > timeout_max - 1.0) bad = 1;
        }
        if (bad) return gate_result("ValueError", "Captured CDP timeout must be positive and finite, or None");
    }
    if (before_send_kind == 2) return gate_result("TypeError", "before_send must be callable");
    if (loop_misuse)
        return gate_result("RuntimeError",
                           "CapturedCDP.call() blocks and cannot run on the supervisor loop");
    if (!valid) {
        char *msg = w19_browsersup_1c8a2d_invalid_message(task_id);
        json_t *out = gate_result("CapturedCDPInvalid", msg);
        free(msg);
        return out;
    }
    {
        char *t = num_str(timeout ? timeout->num_val : 10.0);
        int n = snprintf(NULL, 0, "CDP %s got no reply within %ss", method ? method : "", t ? t : "");
        char *msg;
        json_t *out;
        if (n < 0) { free(t); return gate_result("TimeoutError", ""); }
        msg = (char *)malloc((size_t)n + 1);
        if (!msg) { free(t); return gate_result("TimeoutError", ""); }
        snprintf(msg, (size_t)n + 1, "CDP %s got no reply within %ss", method ? method : "", t ? t : "");
        out = gate_result("TimeoutError", msg);
        free(msg);
        free(t);
        return out;
    }
}

json_t *w19_browsersup_1c8a2d_capture_error(int found, int loop_present, int loop_running,
                                            int active, int ws_present, int page_present,
                                            int schedule_unavailable, const char *task_id) {
    json_t *out = json_object();
    char *msg = NULL;
    if (!found || !loop_present || !loop_running || schedule_unavailable) {
        msg = format_task("no running browser supervisor for task ", task_id);
    } else if (!active || !ws_present || !page_present) {
        msg = format_task("browser supervisor for task ", task_id);
        if (msg) {
            char *joined;
            size_t n = strlen(msg) + strlen(" has no attached page") + 1;
            joined = (char *)malloc(n);
            if (joined) {
                snprintf(joined, n, "%s has no attached page", msg);
                free(msg);
                msg = joined;
            }
        }
    }
    json_set(out, "error", msg ? json_string(msg) : json_null());
    free(msg);
    return out;
}
