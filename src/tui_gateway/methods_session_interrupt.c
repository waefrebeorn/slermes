/*
 * methods_session_interrupt.c — C home for upstream Hermes tui_gateway/methods_session_interrupt.py.
 * Gate w26_methodsses_f994df.
 *
 * PoP: w26_methodsses_f994df_correction @ tui_gateway/methods_session_interrupt.py:_correction_method("session.steer"/"session.redirect")
 * PoP: w26_methodsses_f994df_interrupt @ tui_gateway/methods_session_interrupt.py:@method("session.interrupt")
 * PoP: w26_methodsses_f994df_constants @ tui_gateway/methods_session_interrupt.py:_registry
 *
 * The handlers close over server.py globals through method_ctx.bind_module: the
 * session dict, the agent object, the queues, the TTS cut and the compute-host
 * supervisor are all seams.  What remains, and what is ported here, is the
 * branch tree those seams feed: which params produce a 4002 / 4010 / 5000, when
 * a correction is queued instead of applied (mid-build, compression in flight,
 * idle steer), and which interrupt answers "not_interrupted".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_methodsses_f994df.h"

/* Python truthiness of a JSON value. */
static int mm_truthy(const json_t *v) {
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

static int mm_flag(const json_t *session, const char *key, int def) {
    const json_t *v = session && session->type == JSON_OBJECT ? json_obj_get(session, key) : NULL;
    if (!v || v->type == JSON_NULL) return def;
    return mm_truthy(v);
}

/* str(v) for the shapes params/session hold. */
static char *mm_str_dup(const json_t *v) {
    char buf[64];
    if (!v) return strdup("");
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    case JSON_NULL:   return strdup("");
    case JSON_ARRAY:
    case JSON_OBJECT: return strdup("");
    }
    return strdup("");
}

static int mm_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over ASCII whitespace. */
static void mm_strip(char *s) {
    size_t n;
    char *start = s;
    if (!s) return;
    while (*s && mm_space((unsigned char)*s)) s++;
    n = strlen(s);
    while (n > 0 && mm_space((unsigned char)s[n - 1])) n--;
    memmove(start, s, n);
    start[n] = '\0';
}

/* str(params.get(key) or "").strip() -> caller frees. */
static char *mm_param_str(const json_t *params, const char *key) {
    const json_t *v = params && params->type == JSON_OBJECT ? json_obj_get(params, key) : NULL;
    char *out = mm_truthy(v) ? mm_str_dup(v) : strdup("");
    if (out) mm_strip(out);
    return out;
}

static json_t *mm_ok(json_t *result) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "result", result ? result : json_object());
    return o;
}

static json_t *mm_err(int code, const char *message) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "code", json_number((double)code));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

static json_t *mm_status_result(const char *status, const char *text) {
    json_t *r = json_object();
    json_set(r, "status", json_string(status));
    if (text) json_set(r, "text", json_string(text));
    return r;
}

json_t *w26_methodsses_f994df_correction(const char *verb, const json_t *params,
                                         const json_t *session) {
    int is_redirect = verb && strcmp(verb, "redirect") == 0;
    const json_t *agent = session && session->type == JSON_OBJECT
        ? json_obj_get(session, "agent") : NULL;
    /* `session.get("agent") is None` -- None/absent, not falsiness. */
    int agent_present = agent && agent->type != JSON_NULL;
    int running = mm_flag(session, "running", 0);
    int compressed = mm_flag(session, "compression_in_flight", 0);
    int accepted = mm_flag(session, "accepted", 1);
    int supported = agent_present
        && mm_flag(session, is_redirect ? "supports_redirect" : "supports_steer", 0);
    const char *accepted_status = is_redirect ? "redirected" : "queued";
    const char *unsupported = is_redirect ? "agent does not support active-turn redirect"
                                          : "agent does not support steer";
    const json_t *agent_error;
    char *text = mm_param_str(params, "text");
    json_t *out;

    if (!text) text = strdup("");
    if (!text || !text[0]) {
        free(text);
        return mm_err(4002, "text is required");
    }
    /* Redirect during the turn-build window: queue for the next turn. */
    if (is_redirect && !agent_present && running) {
        out = mm_ok(mm_status_result("queued", text));
        free(text);
        return out;
    }
    /* Compression in flight: a correction now would abort the compression. */
    if (compressed) {
        out = mm_ok(mm_status_result("queued", text));
        free(text);
        return out;
    }
    if (!supported) {
        free(text);
        return mm_err(4010, unsupported);
    }
    /* An idle agent accepts steer() but only the next turn drains it. */
    if (!is_redirect && !running) {
        out = mm_ok(mm_status_result("rejected", text));
        free(text);
        return out;
    }
    /* _apply_correction: agent.<verb>(text) raises -> 5000 "<verb> failed: <exc>". */
    agent_error = session && session->type == JSON_OBJECT ? json_obj_get(session, "agent_error") : NULL;
    if (agent_error && agent_error->type == JSON_STRING && agent_error->str_val
        && agent_error->str_val[0]) {
        size_t need = strlen(verb ? verb : "") + strlen(" failed: ")
                      + strlen(agent_error->str_val) + 1;
        char *msg = (char *)malloc(need);
        if (msg) {
            snprintf(msg, need, "%s failed: %s", verb ? verb : "", agent_error->str_val);
            out = mm_err(5000, msg);
            free(msg);
        } else {
            out = mm_err(5000, "agent call failed");
        }
        free(text);
        return out;
    }
    out = mm_ok(mm_status_result(accepted ? accepted_status : "rejected", text));
    free(text);
    return out;
}

/* task.get("task_id") == expected; a non-str task_id never equals a str. */
static int mm_task_matches(const json_t *task, const char *expected) {
    const json_t *tid;
    if (!task || task->type != JSON_OBJECT) return 0;
    tid = json_obj_get(task, "task_id");
    if (!tid || tid->type != JSON_STRING || !tid->str_val) return 0;
    return strcmp(tid->str_val, expected) == 0;
}

json_t *w26_methodsses_f994df_interrupt(const json_t *params, const json_t *session) {
    char *expected = mm_param_str(params, "expected_hosted_task_id");
    int running = mm_flag(session, "running", 0);
    const json_t *task = session && session->type == JSON_OBJECT
        ? json_obj_get(session, "_hosted_room_task") : NULL;
    int compute_host = mm_flag(session, "compute_host", 0);
    json_t *r;

    if (expected && expected[0]) {
        int matching = running && task && task->type == JSON_OBJECT
                       && mm_task_matches(task, expected);
        free(expected);
        if (!matching) {
            r = json_object();
            json_set(r, "status", json_string("not_interrupted"));
            json_set(r, "interrupted", json_bool(false));
            return mm_ok(r);
        }
    } else {
        free(expected);
    }
    r = mm_status_result("interrupted", NULL);
    if (compute_host) json_set(r, "turn_isolation", json_bool(true));
    return mm_ok(r);
}

json_t *w26_methodsses_f994df_constants(void) {
    json_t *o = json_object();
    json_t *methods = json_array();
    json_append(methods, json_string("session.interrupt"));
    json_append(methods, json_string("session.redirect"));
    json_append(methods, json_string("session.steer"));
    json_set(o, "methods", methods);
    return o;
}
