/*
 * process_registry_notifications_flow2.c — tools/process_registry_notifications.py
 * UNCLAIMED remainder (C11 port, FLOW oracle process_registry_notifications_flow2).
 * LIVE authority: C:/Users/eman5/hermes-agent/tools/process_registry_notifications.py
 *
 * CRLF-aware PoP grep performed FIRST (pop_claims_full.json + src PoP comments):
 *   CLAIMED (seamed here, never re-ported):
 *     _format_age, _format_async_delegation, format_process_notification,
 *     _delegation_attribution_line, _model_not_found_patterns, _delegation_config,
 *     _delegation_model_not_found_notice, _process_accounting_lines
 *   UNCLAIMED remainder (28 defs by AST); this file ports 13 of them:
 *     is_child_process_notification, should_surface_notification, _is_truncated,
 *     delegation_wakes_parent, completion_resumes_parent, group_process_notifications,
 *     _short_title, _short_command, process_completion_display_text,
 *     _completion_status, heartbeat_output_tail, heartbeat_output_text,
 *     join_heartbeat_outputs
 * NOT fully claimed -> port proceeds.
 *
 * Self-contained, libjson-only. Every live effect that belongs to a CLAIMED sibling
 * enters through ONE universal seam (only should_surface_notification needs it:
 * ProcessRegistry._surface_child_process_notifications, seam name "surface_child").
 * Internal composition (process_completion_display_text -> _short_command,
 * join_heartbeat_outputs -> heartbeat_output_tail, should_surface_notification ->
 * is_child_process_notification) runs REAL on both sides.
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * args is BORROWED (seam records a copy); result is NEW (caller frees). NULL == None;
 * {"__raise__": cls, "msg": msg} == a raised exception. A raise with NO matching
 * except site propagates VERBATIM (BaseException trio included). Full builtin
 * exception hierarchy below models isinstance-style catch folding; with no catch in
 * this remainder the table is retained for parity/future catch sites and the
 * oracle's build_exc constructs the same hierarchy (subclasses like FileNotFoundError
 * are modeled as OSError children, SystemExit/KeyboardInterrupt/GeneratorExit stay
 * above Exception). Harness script_call is FAIL-CLOSED: a MISSING script returns a
 * RuntimeError raise spec; a scripted null is a valid None.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; {"__raise__":...} == propagated exception; tuple returns project to
 * JSON arrays; join_heartbeat_outputs returns {"output":str,"omitted":num}.
 *
 * Domain (identical on both sides): ASCII text; len()==strlen; integral JSON numbers
 * are Python ints (write them without a decimal point in fixtures); int() inputs are
 * ASCII numeric strings or numbers/bools (no non-integral float in a str()/==0
 * position); status/reason values are strings or absent/null; task_index is an int;
 * whitespace is the ASCII set (space \t \n \v \f \r).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool prn2_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool prn2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool prn2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool prn2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static char *prn2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct prn2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} prn2_seams_t;

static json_t *prn2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool prn2_is_raise(const json_t *r) {
    return prn2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *prn2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* pi2_call OWNS args (frees it after the call); seam BORROWS args. */
static json_t *prn2_call(const prn2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) { json_free(args); return prn2_raise("RuntimeError", "no seam installed"); }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *prn2_args0(void) { return json_object(); }
static void prn2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (child -> parent), BaseException trio
 *    above Exception so `except Exception` never swallows SystemExit /
 *    KeyboardInterrupt / GeneratorExit. Used for isinstance-style catch folding;
 *    retained here in full for parity even though this remainder has no catch site. ── */
typedef struct prn2_exc_pair { const char *child; const char *parent; } prn2_exc_pair_t;
static const prn2_exc_pair_t PRN2_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
};
static const char *prn2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(PRN2_EXC_TREE) / sizeof(PRN2_EXC_TREE[0]); i++)
        if (!strcmp(PRN2_EXC_TREE[i].child, c)) return PRN2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException.
 * __attribute__((unused)): retained for parity; no catch site in this remainder. */
static bool __attribute__((unused)) prn2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = prn2_exc_parent(c);
    }
    return false;
}

/* ── Python scalar coercion helpers (ASCII domain) ── */
static bool prn2_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
static const char *prn2_py_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
/* Python str(v) for the fixture domain (integral numbers -> decimal int). */
static char *prn2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == trunc(d) && fabs(d) < 9.0e15) return prn2_xsf("%lld", (long long)d);
            return prn2_xsf("%.17g", d);
        }
        case JSON_ARRAY:  return strdup("[...]");
        case JSON_OBJECT: return strdup("{...}");
        default:          return strdup("");
    }
}
/* Python int(v): 0 on success (*out); -1 with *err raise spec. */
static int prn2_py_int(const json_t *v, double *out, json_t **err) {
    *out = 0;
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return 0; }
    if (v->type != JSON_STRING) {
        char *m = prn2_xsf("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                           prn2_py_typename(v));
        *err = prn2_raise("TypeError", m); free(m); return -1;
    }
    const char *s = v->str_val ? v->str_val : "";
    const char *p = s; while (prn2_py_ws(*p)) p++;
    const char *e = s + strlen(s); while (e > p && prn2_py_ws(e[-1])) e--;
    bool neg = false;
    if (p < e && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
    double val = 0; bool any = false, prev_digit = false;
    for (const char *q = p; q < e; q++) {
        if (*q == '_') {
            if (!prev_digit || q + 1 >= e || q[1] < '0' || q[1] > '9') goto bad_int;
            continue;
        }
        if (*q >= '0' && *q <= '9') { val = val * 10.0 + (*q - '0'); any = true; prev_digit = true; }
        else goto bad_int;
    }
    if (!any) goto bad_int;
    *out = neg ? -val : val;
    return 0;
bad_int:
    { char *m = prn2_xsf("invalid literal for int() with base 10: '%s'", s);
      *err = prn2_raise("ValueError", m); free(m); return -1; }
}
/* ' '.join(str.split()) over ASCII whitespace. */
static char *prn2_ws_join(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    size_t j = 0; bool inword = false;
    for (size_t i = 0; i < n; i++) {
        if (prn2_py_ws(s[i])) { inword = false; }
        else {
            if (j > 0 && !inword) out[j++] = ' ';
            out[j++] = s[i]; inword = true;
        }
    }
    out[j] = '\0'; return out;
}
/* evt.get(key) or 0 -> int(v) (Python `int(x or 0)`). */
static int prn2_field_int(const json_t *o, const char *key, double *out, json_t **err) {
    const json_t *v = prn2_is_obj(o) ? json_obj_get(o, key) : NULL;
    *out = 0;
    if (v && prn2_jtruth(v)) return prn2_py_int(v, out, err);
    return 0;
}
/* evt.get(key) or "" -> str(v). */
static char *prn2_field_str(const json_t *o, const char *key) {
    const json_t *v = prn2_is_obj(o) ? json_obj_get(o, key) : NULL;
    return (v && prn2_jtruth(v)) ? prn2_py_str(v) : strdup("");
}
/* Python `value == 0` (0 / 0.0 / False are equal to 0; "0" is not). */
static bool prn2_eq_zero(const json_t *v) {
    if (!v) return false;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_BOOL)   return !v->bool_val;
    return false;
}
/* Python str() of a possibly-absent exit code ('?' default). */
static char *prn2_exit_str(const json_t *v) {
    return v ? prn2_py_str(v) : strdup("?");
}

/* ══════════════════ ported UNCLAIMED functions ══════════════════ */

/* PoP: prn2_is_child_process_notification @ tools/process_registry_notifications.py:is_child_process_notification */
json_t *prn2_is_child_process_notification(const json_t *evt) {
    const json_t *t = prn2_is_obj(evt) ? json_obj_get(evt, "type") : NULL;
    if (prn2_is_str(t)) {
        const char *s = t->str_val ? t->str_val : "";
        if (!strcmp(s, "async_delegation") || !strcmp(s, "async_delegation_work_closeout"))
            return json_bool(false);
    }
    const json_t *v = NULL;
    if (prn2_is_obj(evt)) {
        const json_t *o = json_obj_get(evt, "owner_task_id");
        if (prn2_jtruth(o)) v = o;
        else {
            const json_t *q = json_obj_get(evt, "task_id");
            if (prn2_jtruth(q)) v = q;
        }
    }
    char *s = v ? prn2_py_str(v) : strdup("");
    bool hit = strncmp(s, "sa-", 3) == 0;
    free(s);
    return json_bool(hit);
}

/* PoP: prn2_should_surface_notification @ tools/process_registry_notifications.py:should_surface_notification */
json_t *prn2_should_surface_notification(const prn2_seams_t *sp, const json_t *evt,
                                        const json_t *surface_child) {
    json_t *child = prn2_is_child_process_notification(evt);
    bool is_child = prn2_jtruth(child);
    json_free(child);
    if (!is_child) return json_bool(true);
    if (!surface_child || surface_child->type == JSON_NULL)
        return prn2_call(sp, "surface_child", prn2_args0());
    return json_copy(surface_child);
}

/* PoP: prn2_is_truncated @ tools/process_registry_notifications.py:_is_truncated */
json_t *prn2_is_truncated(const json_t *entry) {
    bool t = false;
    if (prn2_is_obj(entry)) {
        if (prn2_jtruth(json_obj_get(entry, "truncated"))) t = true;
        else {
            const json_t *r = json_obj_get(entry, "exit_reason");
            if (prn2_is_str(r) && !strcmp(r->str_val ? r->str_val : "", "max_iterations")) t = true;
        }
    }
    return json_bool(t);
}

/* PoP: prn2_delegation_wakes_parent @ tools/process_registry_notifications.py:delegation_wakes_parent */
json_t *prn2_delegation_wakes_parent(const json_t *evt) {
    const json_t *t = prn2_is_obj(evt) ? json_obj_get(evt, "type") : NULL;
    if (!prn2_is_str(t) || strcmp(t->str_val ? t->str_val : "", "async_delegation"))
        return json_bool(false);
    if (prn2_jtruth(json_obj_get(evt, "task_failure_notice"))) return json_bool(false);
    if (prn2_jtruth(json_obj_get(evt, "_sibling_failed"))) return json_bool(true);
    const json_t *role = json_obj_get(evt, "role");
    if (prn2_is_str(role) && !strcmp(role->str_val ? role->str_val : "", "cron_run"))
        return json_bool(false);
    /* results = [r for r in raw if isinstance(r, dict)] if isinstance(raw, list) else [] */
    json_t *results = json_array();
    const json_t *raw = json_obj_get(evt, "results");
    if (prn2_is_arr(raw)) {
        for (size_t i = 0; i < json_len(raw); i++) {
            const json_t *r = json_get(raw, i);
            if (prn2_is_obj(r)) json_append(results, json_copy(r));
        }
    }
    /* results = results or [evt] */
    if (json_len(results) == 0) { json_free(results); results = json_array(); json_append(results, json_copy(evt)); }
    /* stopped = () if stalled_after_quiet_seconds is not None else _STOPPED */
    const json_t *sv = json_obj_get(evt, "stalled_after_quiet_seconds");
    bool stopped_empty = (sv && sv->type != JSON_NULL);
    bool any = false;
    for (size_t i = 0; i < json_len(results) && !any; i++) {
        const json_t *r = json_get(results, i);
        const json_t *st = json_obj_get(r, "status");
        bool have_status = prn2_jtruth(st);
        const char *status_s = NULL;
        bool nonstr_truthy = false;
        if (have_status) {
            if (prn2_is_str(st)) status_s = st->str_val ? st->str_val : "";
            else nonstr_truthy = true;
        } else {
            status_s = prn2_jtruth(json_obj_get(r, "error")) ? "failed" : "completed";
        }
        bool allowed = false;
        if (!nonstr_truthy && status_s) {
            if (!strcmp(status_s, "completed") || !strcmp(status_s, "success")) allowed = true;
            else if (!stopped_empty && (!strcmp(status_s, "interrupted") || !strcmp(status_s, "cancelled")))
                allowed = true;
        }
        if (!allowed) any = true;
    }
    json_free(results);
    return json_bool(any);
}

/* PoP: prn2_completion_resumes_parent @ tools/process_registry_notifications.py:completion_resumes_parent */
json_t *prn2_completion_resumes_parent(const json_t *evt) {
    const json_t *t = prn2_is_obj(evt) ? json_obj_get(evt, "type") : NULL;
    bool is_deleg = prn2_is_str(t) && !strcmp(t->str_val ? t->str_val : "", "async_delegation");
    if (is_deleg) {
        if (prn2_jtruth(json_obj_get(evt, "task_failure_notice"))) return json_bool(false);
        bool sibling = prn2_jtruth(json_obj_get(evt, "_sibling_resumes"));
        const json_t *role = json_obj_get(evt, "role");
        bool not_cron = !(prn2_is_str(role) && !strcmp(role->str_val ? role->str_val : "", "cron_run"));
        return json_bool(sibling || not_cron);
    }
    return json_bool(prn2_is_str(t) && !strcmp(t->str_val ? t->str_val : "", "completion"));
}

/* PoP: prn2_group_process_notifications @ tools/process_registry_notifications.py:group_process_notifications */
json_t *prn2_group_process_notifications(const json_t *notifications) {
    json_t *out = json_array();
    json_t *batch = json_array();
    if (prn2_is_arr(notifications)) {
        for (size_t i = 0; i < json_len(notifications); i++) {
            const json_t *pair = json_get(notifications, i);
            const json_t *evt  = prn2_is_arr(pair) ? json_get(pair, 0) : NULL;
            const json_t *text = prn2_is_arr(pair) ? json_get(pair, 1) : NULL;
            const json_t *t = prn2_is_obj(evt) ? json_obj_get(evt, "type") : NULL;
            /* event.get("type", "completion") == "completion" — default only when ABSENT */
            bool is_completion = !t || (prn2_is_str(t) && !strcmp(t->str_val ? t->str_val : "", "completion"));
            json_t *p = json_array();
            json_append(p, evt ? json_copy(evt) : json_null());
            json_append(p, text ? json_copy(text) : json_null());
            if (is_completion) {
                json_append(batch, p);
            } else {
                if (json_len(batch) > 0) { json_append(out, batch); batch = json_array(); }
                json_t *g = json_array();
                json_append(g, p);
                json_append(out, g);
            }
        }
    }
    if (json_len(batch) > 0) json_append(out, batch); else json_free(batch);
    return out;
}

/* PoP: prn2_short_title @ tools/process_registry_notifications.py:_short_title */
json_t *prn2_short_title(const json_t *title) {
    const char *s = prn2_is_str(title) ? (title->str_val ? title->str_val : "") : "";
    size_t n = strlen(s);
    if (n > 200) { char b[201]; memcpy(b, s, 197); memcpy(b + 197, "...", 4); return json_string(b); }
    return json_string(s);
}

/* PoP: prn2_short_command @ tools/process_registry_notifications.py:_short_command */
json_t *prn2_short_command(const json_t *command) {
    char *raw = (command && prn2_jtruth(command)) ? prn2_py_str(command) : strdup("");
    char *cmd = prn2_ws_join(raw);
    free(raw);
    size_t n = strlen(cmd);
    json_t *r;
    if (n > 80) { char b[81]; memcpy(b, cmd, 77); memcpy(b + 77, "...", 4); b[80] = '\0'; r = json_string(b); }
    else r = json_string(cmd);
    free(cmd);
    return r;
}

/* PoP: prn2_process_completion_display_text @ tools/process_registry_notifications.py:process_completion_display_text */
json_t *prn2_process_completion_display_text(const json_t *events) {
    size_t n = prn2_is_arr(events) ? json_len(events) : 0;
    if (n != 1) {
        char *s = prn2_xsf("%zu Background Processes Finished", n);
        json_t *r = json_string(s); free(s); return r;
    }
    const json_t *evt = json_get(events, 0);
    /* reason = evt.get("completion_reason") or "exited" */
    const json_t *cr = json_obj_get(evt, "completion_reason");
    enum { R_OTHER, R_KILLED, R_LOST } reason = R_OTHER;
    if (prn2_jtruth(cr) && prn2_is_str(cr)) {
        const char *s = cr->str_val ? cr->str_val : "";
        if (!strcmp(s, "killed")) reason = R_KILLED;
        else if (!strcmp(s, "lost")) reason = R_LOST;
    }
    const json_t *ec = json_obj_get(evt, "exit_code");   /* default '?' when absent */
    bool zero = prn2_eq_zero(ec);
    const char *outcome = reason == R_KILLED ? "Terminated"
                        : reason == R_LOST   ? "Lost"
                        : (zero ? "Finished" : "Failed");
    char *ecs = prn2_exit_str(ec);
    char *detail = NULL;
    if (reason == R_OTHER && !zero) detail = prn2_xsf(" (exit %s)", ecs);
    json_t *cmdj = prn2_short_command(json_obj_get(evt, "command"));
    const char *cmd = (cmdj && cmdj->type == JSON_STRING && cmdj->str_val) ? cmdj->str_val : "";
    json_t *r;
    if (cmd[0]) {
        char *s = prn2_xsf("Background Process %s%s: %s", outcome, detail ? detail : "", cmd);
        r = json_string(s); free(s);
    } else {
        char *s = prn2_xsf("Background Process %s%s", outcome, detail ? detail : "");
        r = json_string(s); free(s);
    }
    free(detail); free(ecs); json_free(cmdj);
    return r;
}

/* PoP: prn2_completion_status @ tools/process_registry_notifications.py:_completion_status */
json_t *prn2_completion_status(const json_t *evt) {
    const json_t *cr = json_obj_get(evt, "completion_reason");
    const char *reason = "exited";
    if (prn2_jtruth(cr) && prn2_is_str(cr)) reason = cr->str_val ? cr->str_val : "";
    char *out;
    if (!strcmp(reason, "killed")) {
        const json_t *ts = json_obj_get(evt, "termination_source");
        char *src = (ts && prn2_jtruth(ts)) ? prn2_py_str(ts) : strdup("Hermes");
        out = prn2_xsf("terminated by %s", src);
        free(src);
    } else if (!strcmp(reason, "lost")) {
        out = strdup("marked lost because the process backend disappeared");
    } else {
        out = strdup(prn2_eq_zero(json_obj_get(evt, "exit_code")) ? "completed normally" : "exited");
    }
    json_t *r = json_string(out); free(out); return r;
}

/* PoP: prn2_heartbeat_output_tail @ tools/process_registry_notifications.py:heartbeat_output_tail */
json_t *prn2_heartbeat_output_tail(const json_t *output, const json_t *omitted) {
    const char *s = prn2_is_str(output) ? (output->str_val ? output->str_val : "") : "";
    size_t n = strlen(s);
    double om = 0;
    if (omitted && omitted->type == JSON_NUMBER) om = omitted->num_val;
    size_t keep = n > 2000 ? 2000 : n;
    size_t skip = n - keep;
    if (n > 2000) om += (double)(n - 2000);
    char *buf = malloc(keep + 1); memcpy(buf, s + skip, keep); buf[keep] = '\0';
    json_t *r = json_array();
    json_append(r, json_string(buf));
    json_append(r, json_number(om));
    free(buf);
    return r;
}

/* PoP: prn2_heartbeat_output_text @ tools/process_registry_notifications.py:heartbeat_output_text */
json_t *prn2_heartbeat_output_text(const json_t *evt) {
    double omit = 0; json_t *err = NULL;
    if (prn2_field_int(evt, "omitted", &omit, &err) != 0) return err;
    char *out = prn2_field_str(evt, "output");
    json_t *r;
    if (omit != 0) {
        char *s = prn2_xsf("...(%lld earlier characters omitted)\n%s", (long long)omit, out);
        r = json_string(s); free(s);
    } else r = json_string(out);
    free(out);
    return r;
}

/* PoP: prn2_join_heartbeat_outputs @ tools/process_registry_notifications.py:join_heartbeat_outputs */
json_t *prn2_join_heartbeat_outputs(const json_t *earlier, const json_t *later) {
    double ecut = 0, lcut = 0; json_t *err = NULL;
    if (prn2_field_int(earlier, "omitted", &ecut, &err) != 0) return err;
    char *etext = prn2_field_str(earlier, "output");
    if (prn2_field_int(later, "omitted", &lcut, &err) != 0) { free(etext); return err; }
    char *ltext = prn2_field_str(later, "output");
    char *combined; json_t *tail;
    if (lcut != 0) {
        combined = NULL;
        json_t *on = json_string(ltext);
        json_t *om = json_number(ecut + (double)strlen(etext) + lcut);
        tail = prn2_heartbeat_output_tail(on, om);
        json_free(on); json_free(om);
    } else {
        combined = malloc(strlen(etext) + strlen(ltext) + 1);
        strcpy(combined, etext); strcat(combined, ltext);
        json_t *on = json_string(combined);
        json_t *om = json_number(ecut);
        tail = prn2_heartbeat_output_tail(on, om);
        json_free(on); json_free(om);
    }
    json_t *out = json_object();
    json_set(out, "output", json_copy(json_get(tail, 0)));
    json_set(out, "omitted", json_copy(json_get(tail, 1)));
    json_free(tail);
    free(combined); free(etext); free(ltext);
    return out;
}
