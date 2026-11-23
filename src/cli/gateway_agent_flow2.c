/*
 * gateway_agent_flow2.c — UNCLAIMED event-handler / settings / server-request
 * FLOW remainder of hermes_cli/gateway_agent.py (hermes-agent), second wave
 * after port_gateway_agent_flow.c / gateway_agent_cli_flow.c claimed the
 * reasoning-route, submit-staging, interrupt, CLI-summary and rate-limit
 * cores. 36 methods of GatewayAgent (PoP annotations credit each definer):
 *
 *   tool_progress_mode(set), pin_reasoning, stage_turn_reasoning,
 *   restore_reasoning, reasoning_callback(set), _close_segment, _on_delta,
 *   _on_interim, _on_tool_generating, _on_tool_progress, _on_tool_start,
 *   _on_tool_complete, _todo_from_result, _set_todos, _on_todo,
 *   _on_control_update, _on_thinking, _on_reasoning, _on_reasoning_available,
 *   _on_notice, _on_notice_clear, _on_reaction, _on_connection_request,
 *   _on_connection_update, _restore_connection_card, connection_respond,
 *   connection_retry, _on_request_withdrawn, _ask_clarify, _ask_sudo,
 *   _ask_vault_unlock, _ask_vault_save_login, _ask_vault_code, _answer_request,
 *   _voice_stopped, _refresh_stats.
 *
 * Decision sequences only: every live I/O (connection RPC/reply, callback
 * sinks, i18n, clocks, tool-preview builder, todo-store restore, diff printer,
 * server-request modal getters) enters through ONE universal seam:
 * ga2_call(ctx, name, args). The harness feeds it the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts. Claimed siblings (flow1
 * ports) and unported collaborators are SEAMED as "self.<name>" (or module
 * globals _effort_word / clarify_reply); ported siblings compose directly
 * (C calls C, live class methods compose).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ga2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_gateway_agent_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; GatewayRPCError
 *   adds code/data; OSError adds errno when set.
 * - Callback attributes travel as seam-name STRINGS (or null); presence is
 *   `is not None` (jpresent). Markers are non-empty strings in the corpus.
 * - self._turn travels as an attribute dict; seen/answered are SORTED string
 *   arrays (Python sets project sorted); close_segment unions the stripped
 *   shown/spoken joins into seen and resets the segment lists.
 * - The connection / TodoStore doubles are silent; only call/reply/restore
 *   seams trace. self._state is a traced get-or-create accessor returning a
 *   borrowed view whose apply_usage fires "state.apply_usage".
 * - The _lost(turn) abandon closure is a pure factory: it projects to
 *   {"__callable__": "_lost", "turn": <session_id>} and is never invoked.
 *   `turn and self._lost(turn)` uses the _Turn OBJECT's truthiness (always
 *   true when present), so a present turn dict (even empty) yields the closure.
 * - diff_printer(_cprint) projects its arg to {"__callable__": "_cprint"};
 *   its scripted result is a seam-name string fired once per diff line.
 * - logger calls are silent on both sides (not traced).
 * - bool counts as int for the bare isinstance(x, int) gates (Python semantics);
 *   integral JSON numbers are ints. ASCII text; integral numbers only.
 * - _ask_vault_save_login returns Python json.dumps({...}) with default
 *   separators (", ", ": ") and ensure_ascii escaping (ga2_py_dumps2).
 *
 * Fixture domain (documented exclusions):
 * - ASCII text; callback markers non-empty seam-name strings or null; integral
 *   numbers only (no integral-float str() sites); identifier/password values
 *   printable ASCII (ga2_pyquote matches json.dumps ensure_ascii for ASCII).
 * - _answer_request "secret" dispatches to the self._ask_secret seam (the heavy
 *   skills_tool/callbacks chain is never imported). Unknown methods KeyError.
 * - diff strings split on \n / \r\n / \r only; rstrip("\n") trailing newlines.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct ga2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ga2_ctx_t;

/* ── basic helpers ── */
static bool ga2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool ga2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ga2_present(const json_t *v) { return v && v->type != JSON_NULL; }
static bool ga2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ga2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ga2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *ga2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool ga2_str_eq(const json_t *v, const char *s) {
    const char *t = ga2_str(v);
    return t && s && strcmp(t, s) == 0;
}
/* Python isinstance(x, int): bool IS int; integral floats are NOT int. */
static bool ga2_is_int(const json_t *v) {
    if (!v) return false;
    if (v->type == JSON_BOOL) return true;
    if (v->type == JSON_NUMBER) return v->num_val == floor(v->num_val);
    return false;
}
static char *ga2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ga2_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return ga2_strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : ga2_strdup("");
}
/* str.strip(): ASCII whitespace both ends. */
static char *ga2_strip(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static size_t ga2_utf8_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}
static size_t ga2_utf8_prefix(const char *s, size_t n) {
    size_t count = 0, i = 0;
    while (s[i] && count < n) {
        if (((unsigned char)s[i] & 0xc0) != 0x80) count++;
        i++;
    }
    return i;
}
static const char *ga2_py_type(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── exception model ── */
static json_t *ga2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
/*
 * FULL builtin exception hierarchy (py3.14) + the quarry gateway family.
 * ga2_exc_parent walks to the root; `except Exception` catches exactly the
 * kinds whose chain reaches Exception. SystemExit / KeyboardInterrupt /
 * GeneratorExit parent to BaseException ONLY, so they pass through every
 * `except Exception` here. Unknown custom kinds parent to Exception (the
 * oracle's make_exc builds type(kind, (Exception,), {}) for them).
 */
static const char *ga2_exc_parent(const char *t) {
    static const struct { const char *child, *parent; } TAB[] = {
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"},
        {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        {"ArithmeticError", "Exception"},
        {"AssertionError", "Exception"},
        {"AttributeError", "Exception"},
        {"BufferError", "Exception"},
        {"LookupError", "Exception"},
        {"MemoryError", "Exception"},
        {"NameError", "Exception"},
        {"OSError", "Exception"},
        {"ReferenceError", "Exception"},
        {"RuntimeError", "Exception"},
        {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"},
        {"SyntaxError", "Exception"},
        {"SystemError", "Exception"},
        {"TypeError", "Exception"},
        {"ValueError", "Exception"},
        {"Warning", "Exception"},
        {"ImportError", "Exception"},
        {"ExceptionGroup", "Exception"},
        {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"UnboundLocalError", "NameError"},
        {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"},
        {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"},
        {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"ModuleNotFoundError", "ImportError"},
        {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"},
        {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"},
        {"UserWarning", "Warning"},
        {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        {"ResourceWarning", "Warning"},
        /* quarry gateway family */
        {"GatewayClientError", "ValueError"},
        {"GatewayRPCError", "GatewayClientError"},
        {"CallStopped", "GatewayClientError"},
        {"Empty", "Exception"},
    };
    for (size_t i = 0; i < sizeof(TAB) / sizeof(TAB[0]); i++)
        if (strcmp(t, TAB[i].child) == 0) return TAB[i].parent;
    return NULL;
}
static bool ga2_chain_reaches(const json_t *r, const char *ancestor) {
    if (!ga2_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    for (int depth = 0; depth < 24; depth++) {
        if (strcmp(t, ancestor) == 0) return true;
        if (strcmp(t, "BaseException") == 0) return false;
        const char *p = ga2_exc_parent(t);
        if (!p) return false;   /* unknown custom kind: bases Exception, stops */
        t = p;
    }
    return false;
}
/* except Exception: chain reaches Exception (unknown custom kinds do). */
static bool ga2_is_exception(const json_t *r) {
    if (!ga2_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    for (int depth = 0; depth < 24; depth++) {
        if (strcmp(t, "Exception") == 0) return true;
        if (strcmp(t, "BaseException") == 0) return false;
        const char *p = ga2_exc_parent(t);
        if (!p) return true;
        t = p;
    }
    return true;
}
static bool ga2_caught_gce(const json_t *r) { return ga2_chain_reaches(r, "GatewayClientError"); }
static bool ga2_is_kind(const json_t *r, const char *kind) {
    return ga2_is_raise(r) && ga2_str_eq(json_obj_get(r, "__raise__"), kind);
}
static json_t *ga2_keyerror(const char *key) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s'", key ? key : "");
    return ga2_err("KeyError", buf);
}
static json_t *ga2_attr_err(const json_t *v, const char *method) {
    char buf[256];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", ga2_py_type(v), method);
    return ga2_err("AttributeError", buf);
}
static bool ga2_is_oserror_type(const char *t) {
    for (int depth = 0; depth < 24; depth++) {
        if (strcmp(t, "OSError") == 0) return true;
        const char *p = ga2_exc_parent(t);
        if (!p) return false;
        t = p;
    }
    return false;
}
static json_t *ga2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    if (strcmp(type, "GatewayRPCError") == 0) {
        json_t *e = ga2_err("GatewayRPCError", json_get_str(spec, "message", ""));
        json_t *code = json_obj_get(spec, "code");
        json_set(e, "code", code ? json_copy(code) : json_null());
        json_t *data = json_obj_get(spec, "data");
        json_set(e, "data", (data && data->type == JSON_OBJECT) ? json_copy(data) : json_object());
        return e;
    }
    if (strcmp(type, "queue.Empty") == 0) return ga2_err("Empty", json_get_str(spec, "message", ""));
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && ga2_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = ga2_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return ga2_err(type, msg);
}

/* ── seam dispatch ── */
static json_t *ga2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ga2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}
/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *ga2_call(ga2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        size_t mlen = strlen(name) + 24;
        char *msg = malloc(mlen);
        if (!msg) return ga2_err("RuntimeError", "no script");
        snprintf(msg, mlen, "no script for %s", name);
        res = ga2_err("RuntimeError", msg);
        free(msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = ga2_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = ga2_entry_result(json_get(entry, idx));
        }
    } else {
        res = ga2_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}
#define GA2_CHK(v) do { if (ga2_is_raise(v)) return (v); } while (0)

/* ── small builders ── */
static void ga2_set_s(json_t *o, const char *k, const char *v) {
    json_set(o, k, v ? json_string(v) : json_null());
}
static void ga2_set_j(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, v ? json_copy(v) : json_null());
}
static json_t *ga2_args_str(const char *k, const char *v) {
    json_t *a = json_object();
    json_set_hdr(a, k, v ? v : "");
    return a;
}
/* str(x or ""): falsy collapses to "" (NOT "None"). */
static char *ga2_str_or_empty(const json_t *v) {
    if (!ga2_truthy(v)) return ga2_strdup("");
    const char *s = ga2_str(v);
    if (s) return ga2_strdup(s);
    if (v->type == JSON_BOOL) return ga2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val)) return ga2_xsf("%.0f", v->num_val);
        return ga2_xsf("%g", v->num_val);
    }
    return ga2_strdup("");
}
/* Python json.dumps ensure_ascii quoting for ASCII strings (short escapes). */
static char *ga2_pyquote(const char *s) {
    if (!s) s = "";
    size_t cap = strlen(s) * 6 + 3;
    char *o = malloc(cap);
    size_t k = 0;
    o[k++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char ch = *p;
        switch (ch) {
            case '"': o[k++] = '\\'; o[k++] = '"'; break;
            case '\\': o[k++] = '\\'; o[k++] = '\\'; break;
            case '\b': o[k++] = '\\'; o[k++] = 'b'; break;
            case '\f': o[k++] = '\\'; o[k++] = 'f'; break;
            case '\n': o[k++] = '\\'; o[k++] = 'n'; break;
            case '\r': o[k++] = '\\'; o[k++] = 'r'; break;
            case '\t': o[k++] = '\\'; o[k++] = 't'; break;
            default:
                if (ch < 0x20) { k += (size_t)snprintf(o + k, cap - k, "\\u%04x", ch); }
                else o[k++] = (char)ch;
        }
    }
    o[k++] = '"';
    o[k] = '\0';
    return o;
}
/* json.dumps({"identifier": a, "password": b}) default separators. */
static char *ga2_py_dumps2(const char *a, const char *b) {
    char *qa = ga2_pyquote(a), *qb = ga2_pyquote(b);
    char *o = ga2_xsf("{\"identifier\": %s, \"password\": %s}", qa, qb);
    free(qa); free(qb);
    return o;
}

/* ── sorted string-set helpers (seen / answered) ── */
static int ga2_strcmp_p(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* Insert s into a sorted string array (set semantics: dedup, strcmp order).
 * Rewrites arr in place so borrowed pointers (turn.seen) stay valid. */
static void ga2_set_add(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *t = ga2_str(json_get(arr, i));
        if (t && strcmp(t, s) == 0) return;   /* already present */
    }
    size_t n = json_len(arr);
    char **tmp = malloc((n + 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) tmp[i] = ga2_strdup(ga2_str(json_get(arr, i)));
    tmp[n] = ga2_strdup(s);
    qsort(tmp, n + 1, sizeof(char *), ga2_strcmp_p);
    for (size_t i = 0; i < n; i++) json_free(arr->c.items[i]);
    arr->c.count = 0;
    for (size_t i = 0; i < n + 1; i++) {
        json_append(arr, json_string(tmp[i]));
        free(tmp[i]);
    }
    free(tmp);
}
static json_t *ga2_sorted_copy(const json_t *arr) {
    json_t *o = json_array();
    if (ga2_is_arr(arr)) {
        for (size_t i = 0; i < json_len(arr); i++) {
            const char *s = ga2_str(json_get(arr, i));
            if (s) ga2_set_add(o, s);
        }
    }
    return o;
}

/* ── state defaults (mirror oracle setup) ── */
static void ga2_def(json_t *st, const char *k, json_t *v) {
    if (!json_obj_get(st, k)) json_set(st, k, v); else json_free(v);
}
static void ga2_init_defaults(json_t *st) {
    ga2_def(st, "_session_id", json_null());
    ga2_def(st, "_tool_progress_mode", json_string("all"));
    ga2_def(st, "_reasoning_config", json_null());
    ga2_def(st, "_turn_reasoning", json_null());
    ga2_def(st, "_show_reasoning_sent", json_null());
    ga2_def(st, "_service_tier", json_null());
    ga2_def(st, "suppress_status_output", json_bool(false));
    ga2_def(st, "_usage_stale", json_bool(false));
    ga2_def(st, "_open_request", json_null());
    ga2_def(st, "_generation", json_null());
    ga2_def(st, "session_api_calls", json_number(0));
    ga2_def(st, "session_input_tokens", json_number(0));
    ga2_def(st, "session_output_tokens", json_number(0));
    ga2_def(st, "_voice_turn_pending", json_bool(false));
    ga2_def(st, "_turn", json_null());
}

/* ── turn helpers ── */
static json_t *ga2_turn(json_t *st) {
    json_t *t = json_obj_get(st, "_turn");
    return ga2_is_obj(t) ? t : NULL;
}
static char *ga2_join(const json_t *arr) {
    size_t cap = 1;
    if (ga2_is_arr(arr)) for (size_t i = 0; i < json_len(arr); i++) {
        const char *s = ga2_str(json_get(arr, i));
        cap += s ? strlen(s) : 0;
    }
    char *o = malloc(cap);
    o[0] = '\0';
    if (ga2_is_arr(arr)) for (size_t i = 0; i < json_len(arr); i++) {
        const char *s = ga2_str(json_get(arr, i));
        if (s) strcat(o, s);
    }
    return o;
}
/* _Turn.close_segment: seen |= {shown.strip(), spoken.strip()}; reset lists. */
static void ga2_turn_close(json_t *t) {
    json_t *shown = json_obj_get(t, "_shown");
    json_t *spoken = json_obj_get(t, "_spoken");
    char *sh = ga2_join(shown), *sp = ga2_join(spoken);
    char *shs = ga2_strip(sh), *sps = ga2_strip(sp);
    json_t *seen = json_obj_get(t, "seen");
    if (!ga2_is_arr(seen)) { seen = json_array(); json_set(t, "seen", seen); }
    ga2_set_add(seen, shs);
    ga2_set_add(seen, sps);
    free(sh); free(sp); free(shs); free(sps);
    json_set(t, "_shown", json_array());
    json_set(t, "_spoken", json_array());
}
static json_t *ga2_proj_turn(const json_t *t) {
    if (!ga2_is_obj(t)) return json_null();
    json_t *o = json_copy(t);
    json_set(o, "seen", ga2_sorted_copy(json_obj_get(t, "seen")));
    json_set(o, "answered", ga2_sorted_copy(json_obj_get(t, "answered")));
    return o;
}

/* ── connection / state seams ── */
static json_t *ga2_conn_call(ga2_ctx_t *c, json_t *st, const char *method, json_t *kw) {
    if (!kw) kw = json_object();
    json_set_hdr(kw, "method", method);
    return ga2_call(c, "connection.call", kw);
}
static json_t *ga2_self_state(ga2_ctx_t *c) {
    json_t *node = json_obj_get(c->state, "state");
    if (!ga2_is_obj(node)) { node = json_object(); json_set(c->state, "state", node); }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", "self._state");
    json_set(ev, "args", json_object());
    json_set(ev, "ret", json_copy(node));
    json_append(c->trace, ev);
    return node;
}
/* fire a callback marker (seam-name string) if present; args owned. */
static json_t *ga2_fire(ga2_ctx_t *c, json_t *st, const char *key, json_t *args) {
    json_t *marker = json_obj_get(st, key);
    if (!ga2_present(marker)) { json_free(args); return json_null(); }
    const char *name = ga2_str(marker);
    return ga2_call(c, name ? name : "", args);
}
static bool ga2_cb_present(json_t *st, const char *key) {
    return ga2_present(json_obj_get(st, key));
}

/* ── tool_progress_callback uniform kwargs builder ── */
static json_t *ga2_tp_args(const char *kind, const json_t *name, const json_t *message,
                           const json_t *args) {
    json_t *a = json_object();
    ga2_set_s(a, "kind", kind);
    ga2_set_j(a, "name", name);
    ga2_set_j(a, "message", message);
    ga2_set_j(a, "args", args);
    return a;
}

/* ════════════════ ported methods ════════════════ */

/* ── tool_progress_mode setter ── */
/* PoP: ga2_tool_progress_mode_set @ hermes_cli/gateway_agent.py:tool_progress_mode */
static json_t *ga2_tool_progress_mode_set(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *mode = json_obj_get(args, "mode");
    const char *ms = ga2_str(mode);
    json_t *cur = json_obj_get(st, "_tool_progress_mode");
    bool same = ga2_is_str(cur) && ms && strcmp(ga2_str(cur), ms) == 0;
    bool valid = ms && (!strcmp(ms, "off") || !strcmp(ms, "new") ||
                        !strcmp(ms, "all") || !strcmp(ms, "verbose"));
    json_set(st, "_tool_progress_mode", mode ? json_copy(mode) : json_null());
    if (same || !valid) return json_null();
    json_t *kw = json_object();
    ga2_set_s(kw, "session_id", json_get_str(st, "_session_id", ""));
    ga2_set_s(kw, "key", "verbose");
    ga2_set_s(kw, "value", ms);
    json_t *r = ga2_conn_call(c, st, "config.set", kw);
    if (ga2_caught_gce(r)) { json_free(r); return json_null(); }
    GA2_CHK(r);
    json_free(r);
    return json_null();
}

/* ── pin_reasoning ── */
/* PoP: ga2_pin_reasoning @ hermes_cli/gateway_agent.py:pin_reasoning */
static json_t *ga2_pin_reasoning(ga2_ctx_t *c, json_t *args) {
    json_t *a = json_object();
    ga2_set_j(a, "config", json_obj_get(args, "config"));
    json_set(a, "explicit", json_bool(true));
    return ga2_call(c, "self._set_reasoning", a);
}

/* ── stage_turn_reasoning ── */
/* PoP: ga2_stage_turn_reasoning @ hermes_cli/gateway_agent.py:stage_turn_reasoning */
static json_t *ga2_stage_turn_reasoning(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *config = json_obj_get(args, "config");
    json_set(st, "_reasoning_config", config ? json_copy(config) : json_null());
    json_t *a = json_object();
    ga2_set_j(a, "config", config);
    json_t *effort = ga2_call(c, "_effort_word", a);
    GA2_CHK(effort);
    json_set(st, "_turn_reasoning", effort);
    return json_null();
}

/* ── restore_reasoning ── */
/* PoP: ga2_restore_reasoning @ hermes_cli/gateway_agent.py:restore_reasoning */
static json_t *ga2_restore_reasoning(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *config = json_obj_get(args, "config");
    json_set(st, "_reasoning_config", config ? json_copy(config) : json_null());
    json_set(st, "_turn_reasoning", json_null());
    return json_null();
}

/* ── reasoning_callback setter ── */
/* PoP: ga2_reasoning_callback_set @ hermes_cli/gateway_agent.py:reasoning_callback */
static json_t *ga2_reasoning_callback_set(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *cb = json_obj_get(args, "callback");
    json_set(st, "_reasoning_callback", cb ? json_copy(cb) : json_null());
    json_t *r = ga2_call(c, "self._sync_show_reasoning", json_object());
    GA2_CHK(r);
    json_free(r);
    return json_null();
}

/* ── _close_segment ── */
/* PoP: ga2_close_segment @ hermes_cli/gateway_agent.py:_close_segment */
static json_t *ga2_close_segment(ga2_ctx_t *c, json_t *starts, bool separator) {
    json_t *st = c->state;
    json_t *turn = ga2_turn(st);
    if (ga2_cb_present(st, "stream_delta_callback")) {
        bool any_shown = false;
        if (!separator) {
            if (!turn) return ga2_attr_err(json_null(), "_shown");
            json_t *shown = json_obj_get(turn, "_shown");
            if (ga2_is_arr(shown)) for (size_t i = 0; i < json_len(shown); i++) {
                const char *s = ga2_str(json_get(shown, i));
                if (s && s[0]) { any_shown = true; break; }
            }
        }
        if (separator || any_shown) {
            json_t *a = json_object();
            json_set(a, "text", json_null());
            json_t *r = ga2_fire(c, st, "stream_delta_callback", a);
            GA2_CHK(r);
            json_free(r);
        }
    }
    if (!turn) return ga2_attr_err(json_null(), "close_segment");
    ga2_turn_close(turn);
    return json_null();
}

/* ── _on_delta ── */
/* PoP: ga2_on_delta @ hermes_cli/gateway_agent.py:_on_delta */
static json_t *ga2_on_delta(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *payload = json_obj_get(args, "payload");
    json_t *text = ga2_is_obj(payload) ? json_obj_get(payload, "text") : NULL;
    json_t *turn = ga2_turn(st);
    if (!ga2_is_str(text)) {
        json_t *starts = json_obj_get(args, "starts");
        return ga2_close_segment(c, ga2_is_obj(starts) ? starts : NULL, true);
    }
    if (!turn) return ga2_attr_err(json_null(), "gapped");
    if (ga2_truthy(json_obj_get(turn, "gapped"))) return json_null();
    if (ga2_cb_present(st, "stream_delta_callback")) {
        json_t *a = json_object(); ga2_set_j(a, "text", text);
        json_t *r = ga2_fire(c, st, "stream_delta_callback", a);
        GA2_CHK(r); json_free(r);
        json_t *shown = json_obj_get(turn, "_shown");
        if (!ga2_is_arr(shown)) { shown = json_array(); json_set(turn, "_shown", shown); }
        json_append(shown, json_copy(text));
    }
    json_t *sc = json_obj_get(args, "stream_callback");
    if (ga2_present(sc)) {
        json_t *a = json_object(); ga2_set_j(a, "text", text);
        json_t *r = ga2_call(c, ga2_str(sc) ? ga2_str(sc) : "", a);
        GA2_CHK(r); json_free(r);
        json_t *spoken = json_obj_get(turn, "_spoken");
        if (!ga2_is_arr(spoken)) { spoken = json_array(); json_set(turn, "_spoken", spoken); }
        json_append(spoken, json_copy(text));
    }
    return json_null();
}

/* ── _on_interim ── */
/* PoP: ga2_on_interim @ hermes_cli/gateway_agent.py:_on_interim */
static json_t *ga2_on_interim(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *payload = json_obj_get(args, "payload");
    json_t *text = ga2_is_obj(payload) ? json_obj_get(payload, "text") : NULL;
    json_t *starts = json_obj_get(args, "starts");
    json_t *r = ga2_close_segment(c, ga2_is_obj(starts) ? starts : NULL, false);
    GA2_CHK(r); json_free(r);
    json_t *turn = ga2_turn(st);
    if (!turn) return ga2_attr_err(json_null(), "seen");
    char *raw = ga2_str_or_empty(text);
    char *stripped = ga2_strip(raw);
    json_t *seen = json_obj_get(turn, "seen");
    if (!ga2_is_arr(seen)) { seen = json_array(); json_set(turn, "seen", seen); }
    ga2_set_add(seen, stripped);
    free(raw); free(stripped);
    if (ga2_cb_present(st, "interim_assistant_callback")) {
        json_t *a = json_object();
        ga2_set_j(a, "text", text);
        json_set(a, "already_streamed", json_bool(ga2_truthy(ga2_is_obj(payload) ? json_obj_get(payload, "already_streamed") : NULL)));
        json_t *rr = ga2_fire(c, st, "interim_assistant_callback", a);
        GA2_CHK(rr); json_free(rr);
    } else if (ga2_truthy(text)) {
        char *s = ga2_str_or_empty(text);
        char *ss = ga2_strip(s);
        bool nonempty = ss && ss[0];
        free(s); free(ss);
        if (nonempty) {
            json_t *a = json_object(); ga2_set_j(a, "text", text);
            json_t *rr = ga2_call(c, "self._emit_print", a);
            GA2_CHK(rr); json_free(rr);
        }
    }
    return json_null();
}

/* ── _on_tool_generating ── */
/* PoP: ga2_on_tool_generating @ hermes_cli/gateway_agent.py:_on_tool_generating */
static json_t *ga2_on_tool_generating(ga2_ctx_t *c, json_t *args) {
    json_t *payload = json_obj_get(args, "payload");
    json_t *a = json_object();
    ga2_set_j(a, "name", ga2_is_obj(payload) ? json_obj_get(payload, "name") : NULL);
    json_t *r = ga2_fire(c, c->state, "tool_gen_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}

/* ── _on_tool_progress ── */
/* PoP: ga2_on_tool_progress @ hermes_cli/gateway_agent.py:_on_tool_progress */
static json_t *ga2_on_tool_progress(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    if (!ga2_cb_present(st, "tool_progress_callback")) return json_null();
    json_t *p = json_obj_get(args, "payload");
    if (!ga2_is_obj(p)) p = NULL;
    json_t *a = ga2_tp_args("tool.progress", p ? json_obj_get(p, "name") : NULL,
                            p ? json_obj_get(p, "message") : NULL, NULL);
    ga2_set_j(a, "tool_call_id", p ? json_obj_get(p, "tool_id") : NULL);
    ga2_set_j(a, "progress", p ? json_obj_get(p, "progress") : NULL);
    ga2_set_j(a, "total", p ? json_obj_get(p, "total") : NULL);
    ga2_set_j(a, "percent", p ? json_obj_get(p, "percent") : NULL);
    json_t *r = ga2_fire(c, st, "tool_progress_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}

/* ── _refresh_stats ── */
/* PoP: ga2_refresh_stats @ hermes_cli/gateway_agent.py:_refresh_stats */
static json_t *ga2_refresh_stats(ga2_ctx_t *c, json_t *turn) {
    json_t *st = c->state;
    json_t *kw = json_object();
    ga2_set_s(kw, "session_id", json_get_str(st, "_session_id", ""));
    json_set(kw, "timeout", json_number(10));
    if (ga2_present(turn)) {
        json_t *ab = json_object();
        json_set_hdr(ab, "__callable__", "_lost");
        ga2_set_s(ab, "turn", ga2_is_obj(turn) ? json_get_str(turn, "session_id", "") : "");
        json_set(kw, "abandon", ab);
    } else {
        json_set(kw, "abandon", json_null());
    }
    json_t *stats = ga2_conn_call(c, st, "session.stats", kw);
    if (ga2_caught_gce(stats)) { json_free(stats); return json_bool(false); }
    GA2_CHK(stats);
    if (ga2_is_obj(stats) && ga2_is_obj(json_obj_get(stats, "usage"))) {
        json_t *u = json_copy(json_obj_get(stats, "usage"));
        json_free(stats);
        stats = u;
    }
    if (!(ga2_is_obj(stats) && ga2_is_int(json_obj_get(stats, "api_calls")))) {
        json_free(stats);
        return json_bool(false);
    }
    bool apply = false;
    if (!ga2_present(turn)) {
        apply = true;
    } else {
        json_t *a = json_object();
        json_set(a, "turn", ga2_proj_turn(turn));
        json_t *owns = ga2_call(c, "self._owns", a);
        GA2_CHK(owns);
        apply = ga2_truthy(owns);
        json_free(owns);
    }
    if (apply) {
        ga2_self_state(c);
        json_t *a = json_object();
        ga2_set_j(a, "stats", stats);
        json_t *r = ga2_call(c, "state.apply_usage", a);
        GA2_CHK(r); json_free(r);
    }
    json_free(stats);
    return json_bool(true);
}

/* ── _on_tool_start ── */
/* PoP: ga2_on_tool_start @ hermes_cli/gateway_agent.py:_on_tool_start */
static json_t *ga2_on_tool_start(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *starts = json_obj_get(args, "starts");
    if (!ga2_is_obj(starts)) { starts = json_object(); json_set(args, "starts", starts); }
    json_t *r = ga2_close_segment(c, starts, false);
    GA2_CHK(r); json_free(r);
    json_t *p = json_obj_get(args, "payload");
    if (!ga2_is_obj(p)) p = NULL;
    json_t *name = p ? json_obj_get(p, "name") : NULL;
    if (!ga2_truthy(name)) name = p ? json_obj_get(p, "tool_name") : NULL;
    const char *name_s = ga2_truthy(name) ? ga2_str(name) : NULL;
    if (!name_s) name_s = "tool";
    json_t *args_v = (p && ga2_is_obj(json_obj_get(p, "args"))) ? json_obj_get(p, "args") : NULL;
    json_t *call_id = p ? json_obj_get(p, "tool_id") : NULL;
    if (!ga2_truthy(call_id)) call_id = p ? json_obj_get(p, "tool_call_id") : NULL;
    const char *cid = ga2_truthy(call_id) ? ga2_str(call_id) : NULL;
    if (!cid) cid = "";
    json_t *mono = ga2_call(c, "time.monotonic", json_object());
    GA2_CHK(mono);
    json_set(starts, cid, mono);
    if (ga2_truthy(json_obj_get(st, "_usage_stale"))) {
        json_set(st, "_usage_stale", json_bool(false));
        json_t *rs = ga2_refresh_stats(c, NULL);
        GA2_CHK(rs); json_free(rs);
    }
    if (ga2_cb_present(st, "tool_progress_callback")) {
        json_t *ba = json_object();
        ga2_set_s(ba, "name", name_s);
        ga2_set_j(ba, "args", args_v ? args_v : json_object());
        if (!args_v) { /* ga2_set_j copied null; replace with {} */
            json_set(ba, "args", json_object());
        }
        json_t *preview = ga2_call(c, "build_tool_preview", ba);
        GA2_CHK(preview);
        json_t *a = ga2_tp_args("tool.started", json_string(name_s), preview,
                                args_v ? args_v : json_object());
        if (!args_v) json_set(a, "args", json_object());
        json_free(preview);
        json_t *rr = ga2_fire(c, st, "tool_progress_callback", a);
        GA2_CHK(rr); json_free(rr);
    }
    return json_null();
}

/* ── _set_todos ── */
/* PoP: ga2_set_todos @ hermes_cli/gateway_agent.py:_set_todos */
static json_t *ga2_set_todos(ga2_ctx_t *c, json_t *snapshot) {
    if (!ga2_is_obj(snapshot)) return json_null();
    json_t *todos = json_obj_get(snapshot, "todos");
    if (!ga2_is_arr(todos)) return json_null();
    json_t *a = json_object();
    ga2_set_j(a, "todos", todos);
    ga2_set_j(a, "revision", json_obj_get(snapshot, "revision"));
    json_t *r = ga2_call(c, "todo_store.restore", a);
    if (ga2_chain_reaches(r, "ValueError") || ga2_chain_reaches(r, "TypeError")) {
        json_free(r);
        return json_null();
    }
    GA2_CHK(r);
    json_free(r);
    return json_null();
}

/* ── _todo_from_result ── */
/* PoP: ga2_todo_from_result @ hermes_cli/gateway_agent.py:_todo_from_result */
static json_t *ga2_todo_from_result(ga2_ctx_t *c, json_t *args) {
    json_t *result = json_obj_get(args, "result");
    json_t *data = NULL;
    bool owned = false;
    if (ga2_is_str(result)) {
        data = json_parse(result->str_val ? result->str_val : "", NULL);
        if (!data) return json_null();   /* json.loads ValueError -> caught */
        owned = true;
    } else {
        data = result;
    }
    json_t *r = json_null();
    if (ga2_is_obj(data)) r = ga2_set_todos(c, data);
    if (owned) json_free(data);
    return r;
}

/* ── _on_todo ── */
/* PoP: ga2_on_todo @ hermes_cli/gateway_agent.py:_on_todo */
static json_t *ga2_on_todo(ga2_ctx_t *c, json_t *args) {
    return ga2_set_todos(c, json_obj_get(args, "payload"));
}

/* ── _on_tool_complete ── */
/* PoP: ga2_on_tool_complete @ hermes_cli/gateway_agent.py:_on_tool_complete */
static json_t *ga2_on_tool_complete(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *p = json_obj_get(args, "payload");
    if (!ga2_is_obj(p)) p = NULL;
    json_t *name = p ? json_obj_get(p, "name") : NULL;
    if (!ga2_truthy(name)) name = p ? json_obj_get(p, "tool_name") : NULL;
    const char *name_s = ga2_truthy(name) ? ga2_str(name) : NULL;
    if (!name_s) name_s = "tool";
    json_t *args_v = (p && ga2_is_obj(json_obj_get(p, "args"))) ? json_obj_get(p, "args") : NULL;
    json_t *call_id = p ? json_obj_get(p, "tool_id") : NULL;
    if (!ga2_truthy(call_id)) call_id = p ? json_obj_get(p, "tool_call_id") : NULL;
    const char *cid = ga2_truthy(call_id) ? ga2_str(call_id) : NULL;
    if (!cid) cid = "";
    json_t *result = p ? json_obj_get(p, "result") : NULL;
    json_t *duration = p ? json_obj_get(p, "duration_s") : NULL;
    bool dur_owned = false;
    if (!ga2_present(duration)) {
        json_t *starts = json_obj_get(args, "starts");
        json_t *started = ga2_is_obj(starts) ? json_obj_get(starts, cid) : NULL;
        if (ga2_present(started)) {
            json_t *mono = ga2_call(c, "time.monotonic", json_object());
            GA2_CHK(mono);
            double d = mono->num_val - (started->type == JSON_NUMBER ? started->num_val : 0);
            json_free(mono);
            duration = json_number(d);
            dur_owned = true;
            if (ga2_is_obj(starts)) json_obj_del(starts, cid);
        } else {
            duration = json_number(0.0);
            dur_owned = true;
        }
    }
    if (ga2_cb_present(st, "tool_progress_callback")) {
        json_t *a = ga2_tp_args("tool.completed", json_string(name_s), NULL, NULL);
        ga2_set_j(a, "duration", duration);
        json_set(a, "is_error", json_bool(ga2_truthy(p ? json_obj_get(p, "is_error") : NULL)));
        ga2_set_j(a, "result", result);
        json_t *r = ga2_fire(c, st, "tool_progress_callback", a);
        if (dur_owned) json_free(duration);
        GA2_CHK(r); json_free(r);
    } else if (dur_owned) {
        json_free(duration);
    }
    json_set(st, "_usage_stale", json_bool(true));
    json_t *md = ga2_call(c, "mark_dirty", json_object());
    GA2_CHK(md); json_free(md);
    if (!strcmp(name_s, "todo")) {
        json_t *ta = json_object();
        ga2_set_j(ta, "result", result);
        json_t *r = ga2_todo_from_result(c, ta);
        json_free(ta);
        GA2_CHK(r); json_free(r);
    }
    json_t *diff = p ? json_obj_get(p, "inline_diff") : NULL;
    if (ga2_is_str(diff) && diff->str_val && diff->str_val[0]) {
        json_t *da = json_object();
        json_t *printer = json_object();
        json_set_hdr(printer, "__callable__", "_cprint");
        json_set(da, "printer", printer);
        json_t *emit = ga2_call(c, "diff_printer", da);
        GA2_CHK(emit);
        const char *emit_name = ga2_str(emit);
        char *emit_dup = ga2_strdup(emit_name ? emit_name : "diff_emit");
        json_free(emit);
        /* diff.rstrip("\n").splitlines() */
        char *src = ga2_strdup(diff->str_val);
        size_t n = strlen(src);
        while (n > 0 && src[n - 1] == '\n') src[--n] = '\0';
        const char *q = src;
        while (*q) {
            const char *nl = q;
            while (*nl && *nl != '\n' && *nl != '\r') nl++;
            size_t ln = (size_t)(nl - q);
            char *line = malloc(ln + 1);
            memcpy(line, q, ln); line[ln] = '\0';
            json_t *la = json_object();
            json_set_hdr(la, "line", line);
            free(line);
            json_t *r = ga2_call(c, emit_dup, la);
            GA2_CHK(r); json_free(r);
            q = nl;
            if (*q == '\r') { q++; if (*q == '\n') q++; }
            else if (*q == '\n') q++;
        }
        free(src);
        if (!strcmp(name_s, "delegate_task") && ga2_cb_present(st, "tool_complete_callback")) {
            json_t *a = json_object();
            ga2_set_s(a, "call_id", cid);
            ga2_set_s(a, "name", name_s);
            ga2_set_j(a, "args", args_v ? args_v : json_object());
            if (!args_v) json_set(a, "args", json_object());
            ga2_set_j(a, "result", result);
            json_t *r = ga2_fire(c, st, "tool_complete_callback", a);
            free(emit_dup);
            GA2_CHK(r); json_free(r);
        } else {
            free(emit_dup);
        }
    } else if (ga2_cb_present(st, "tool_complete_callback")) {
        json_t *a = json_object();
        ga2_set_s(a, "call_id", cid);
        ga2_set_s(a, "name", name_s);
        ga2_set_j(a, "args", args_v ? args_v : json_object());
        if (!args_v) json_set(a, "args", json_object());
        ga2_set_j(a, "result", result);
        json_t *r = ga2_fire(c, st, "tool_complete_callback", a);
        GA2_CHK(r); json_free(r);
    }
    return json_null();
}

/* ── simple callback forwarders ── */
/* PoP: ga2_on_control_update @ hermes_cli/gateway_agent.py:_on_control_update */
static json_t *ga2_on_control_update(ga2_ctx_t *c, json_t *args) {
    (void)args;
    json_t *r = ga2_fire(c, c->state, "control_update_callback", json_object());
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_thinking @ hermes_cli/gateway_agent.py:_on_thinking */
static json_t *ga2_on_thinking(ga2_ctx_t *c, json_t *args) {
    json_t *p = json_obj_get(args, "payload");
    json_t *text = ga2_is_obj(p) ? json_obj_get(p, "text") : NULL;
    char *s = ga2_str_or_empty(text);
    json_t *a = ga2_args_str("text", s);
    free(s);
    json_t *r = ga2_fire(c, c->state, "thinking_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_reasoning @ hermes_cli/gateway_agent.py:_on_reasoning */
static json_t *ga2_on_reasoning(ga2_ctx_t *c, json_t *args) {
    json_t *p = json_obj_get(args, "payload");
    json_t *text = ga2_is_obj(p) ? json_obj_get(p, "text") : NULL;
    char *s = ga2_str_or_empty(text);
    json_t *a = ga2_args_str("text", s);
    free(s);
    json_t *r = ga2_fire(c, c->state, "_reasoning_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_reasoning_available @ hermes_cli/gateway_agent.py:_on_reasoning_available */
static json_t *ga2_on_reasoning_available(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    if (!ga2_cb_present(st, "tool_progress_callback")) return json_null();
    json_t *p = json_obj_get(args, "payload");
    json_t *text = ga2_is_obj(p) ? json_obj_get(p, "text") : NULL;
    char *s = ga2_str_or_empty(text);
    size_t pre = ga2_utf8_prefix(s, 500);
    char *cut = malloc(pre + 1);
    memcpy(cut, s, pre); cut[pre] = '\0';
    free(s);
    json_t *a = ga2_tp_args("reasoning.available", json_string("_thinking"), json_string(cut), NULL);
    free(cut);
    json_t *r = ga2_fire(c, st, "tool_progress_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_notice @ hermes_cli/gateway_agent.py:_on_notice */
static json_t *ga2_on_notice(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    if (!ga2_cb_present(st, "notice_callback")) return json_null();
    json_t *p = json_obj_get(args, "payload");
    if (!ga2_is_obj(p)) p = NULL;
    json_t *notice = json_object();
    char *text = ga2_str_or_empty(p ? json_obj_get(p, "text") : NULL);
    json_set_hdr(notice, "text", text);
    free(text);
    json_t *level = p ? json_obj_get(p, "level") : NULL;
    json_set_hdr(notice, "level", ga2_truthy(level) ? (ga2_str(level) ? ga2_str(level) : "info") : "info");
    json_t *kind = p ? json_obj_get(p, "kind") : NULL;
    json_set_hdr(notice, "kind", ga2_truthy(kind) ? (ga2_str(kind) ? ga2_str(kind) : "sticky") : "sticky");
    ga2_set_j(notice, "ttl_ms", p ? json_obj_get(p, "ttl_ms") : NULL);
    ga2_set_j(notice, "key", p ? json_obj_get(p, "key") : NULL);
    ga2_set_j(notice, "id", p ? json_obj_get(p, "id") : NULL);
    json_t *a = json_object();
    json_set(a, "notice", notice);
    json_t *r = ga2_fire(c, st, "notice_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_notice_clear @ hermes_cli/gateway_agent.py:_on_notice_clear */
static json_t *ga2_on_notice_clear(ga2_ctx_t *c, json_t *args) {
    json_t *p = json_obj_get(args, "payload");
    json_t *a = json_object();
    ga2_set_j(a, "key", ga2_is_obj(p) ? json_obj_get(p, "key") : NULL);
    json_t *r = ga2_fire(c, c->state, "notice_clear_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_reaction @ hermes_cli/gateway_agent.py:_on_reaction */
static json_t *ga2_on_reaction(ga2_ctx_t *c, json_t *args) {
    json_t *p = json_obj_get(args, "payload");
    json_t *a = json_object();
    ga2_set_j(a, "kind", ga2_is_obj(p) ? json_obj_get(p, "kind") : NULL);
    json_t *r = ga2_fire(c, c->state, "reaction_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_connection_request @ hermes_cli/gateway_agent.py:_on_connection_request */
static json_t *ga2_on_connection_request(ga2_ctx_t *c, json_t *args) {
    json_t *a = json_object();
    ga2_set_j(a, "payload", json_obj_get(args, "payload"));
    json_t *r = ga2_fire(c, c->state, "connection_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_on_connection_update @ hermes_cli/gateway_agent.py:_on_connection_update */
static json_t *ga2_on_connection_update(ga2_ctx_t *c, json_t *args) {
    json_t *a = json_object();
    ga2_set_j(a, "payload", json_obj_get(args, "payload"));
    json_t *r = ga2_fire(c, c->state, "connection_update_callback", a);
    GA2_CHK(r); json_free(r);
    return json_null();
}
/* PoP: ga2_restore_connection_card @ hermes_cli/gateway_agent.py:_restore_connection_card */
static json_t *ga2_restore_connection_card(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *pending = json_obj_get(args, "pending");
    if (ga2_truthy(pending)) {
        if (ga2_cb_present(st, "connection_callback")) {
            json_t *a = json_object(); ga2_set_j(a, "payload", pending);
            json_t *r = ga2_fire(c, st, "connection_callback", a);
            GA2_CHK(r); json_free(r);
        }
    } else if (ga2_cb_present(st, "connection_update_callback")) {
        json_t *a = json_object();
        json_t *payload = json_object();
        json_set(payload, "settled", json_bool(true));
        json_set(a, "payload", payload);
        json_t *r = ga2_fire(c, st, "connection_update_callback", a);
        GA2_CHK(r); json_free(r);
    }
    return json_null();
}

/* ── connection_respond / connection_retry ── */
/* PoP: ga2_connection_respond @ hermes_cli/gateway_agent.py:connection_respond */
static json_t *ga2_connection_respond(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *kw = json_object();
    ga2_set_j(kw, "op_id", json_obj_get(args, "op_id"));
    ga2_set_j(kw, "result", json_obj_get(args, "result"));
    json_t *owner = json_object();
    json_set_hdr(owner, "type", "session");
    ga2_set_s(owner, "session_id", json_get_str(st, "_session_id", ""));
    json_set(kw, "owner", owner);
    return ga2_conn_call(c, st, "connection.respond", kw);
}
/* PoP: ga2_connection_retry @ hermes_cli/gateway_agent.py:connection_retry */
static json_t *ga2_connection_retry(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *kw = json_object();
    json_t *conn = json_array();
    json_append(conn, json_copy(json_obj_get(args, "name") ? json_obj_get(args, "name") : json_null()));
    json_set(kw, "connectors", conn);
    json_set(kw, "reconnect", json_bool(true));
    json_t *owner = json_object();
    json_set_hdr(owner, "type", "session");
    ga2_set_s(owner, "session_id", json_get_str(st, "_session_id", ""));
    json_set(kw, "owner", owner);
    return ga2_conn_call(c, st, "connectors.connect", kw);
}

/* ── _on_request_withdrawn ── */
/* PoP: ga2_on_request_withdrawn @ hermes_cli/gateway_agent.py:_on_request_withdrawn */
static json_t *ga2_on_request_withdrawn(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *current = json_obj_get(st, "_open_request");
    if (!ga2_is_arr(current) || json_len(current) < 2) return json_null();
    json_t *payload = json_obj_get(args, "payload");
    json_t *pid = ga2_is_obj(payload) ? json_obj_get(payload, "id") : NULL;
    json_t *cur_id = json_get(current, 0);
    bool id_eq = ga2_is_str(pid) && ga2_is_str(cur_id) &&
                 strcmp(ga2_str(pid), ga2_str(cur_id)) == 0;
    if (!id_eq) return json_null();
    if (!ga2_cb_present(st, "modal_withdraw_callback")) return json_null();
    json_t *a = json_object();
    ga2_set_j(a, "method", json_get(current, 1));
    json_t *r = ga2_fire(c, st, "modal_withdraw_callback", a);
    if (ga2_is_exception(r)) { json_free(r); return json_null(); }
    GA2_CHK(r);
    json_free(r);
    return json_null();
}

/* ── _ask_clarify ── */
/* PoP: ga2_ask_clarify @ hermes_cli/gateway_agent.py:_ask_clarify */
static json_t *ga2_ask_clarify(ga2_ctx_t *c, json_t *params) {
    json_t *a = json_object();
    json_set(a, "callback_present", json_bool(ga2_cb_present(c->state, "clarify_callback")));
    ga2_set_j(a, "params", params);
    return ga2_call(c, "clarify_reply", a);
}
/* ── _ask_sudo ── */
/* PoP: ga2_ask_sudo @ hermes_cli/gateway_agent.py:_ask_sudo */
static json_t *ga2_ask_sudo(ga2_ctx_t *c, json_t *params) {
    (void)params;
    json_t *getter = ga2_call(c, "_get_sudo_password_callback", json_object());
    GA2_CHK(getter);
    const char *name = ga2_str(getter);
    if (!name) { json_free(getter); return json_string(""); }
    char *dup = ga2_strdup(name);
    json_free(getter);
    json_t *r = ga2_call(c, dup, json_object());
    free(dup);
    GA2_CHK(r);
    if (ga2_truthy(r)) return r;
    json_free(r);
    return json_string("");
}
/* ── _ask_vault_unlock ── */
/* PoP: ga2_ask_vault_unlock @ hermes_cli/gateway_agent.py:_ask_vault_unlock */
static json_t *ga2_ask_vault_unlock(ga2_ctx_t *c, json_t *params) {
    json_t *getter = ga2_call(c, "get_unlock_prompt_callback", json_object());
    GA2_CHK(getter);
    const char *name = ga2_str(getter);
    if (!name) { json_free(getter); return json_string(""); }
    char *dup = ga2_strdup(name);
    json_free(getter);
    char *backend = ga2_str_or_empty(ga2_is_obj(params) ? json_obj_get(params, "backend") : NULL);
    char *display = ga2_str_or_empty(ga2_is_obj(params) ? json_obj_get(params, "display_name") : NULL);
    json_t *a = json_object();
    json_set_hdr(a, "backend", backend);
    json_set_hdr(a, "display_name", display);
    free(backend); free(display);
    json_t *r = ga2_call(c, dup, a);
    free(dup);
    GA2_CHK(r);
    if (ga2_truthy(r)) return r;
    json_free(r);
    return json_string("");
}
/* ── _ask_vault_save_login ── */
/* PoP: ga2_ask_vault_save_login @ hermes_cli/gateway_agent.py:_ask_vault_save_login */
static json_t *ga2_ask_vault_save_login(ga2_ctx_t *c, json_t *params) {
    json_t *getter = ga2_call(c, "get_save_login_prompt_callback", json_object());
    GA2_CHK(getter);
    const char *name = ga2_str(getter);
    if (!name) { json_free(getter); return json_string(""); }
    char *dup = ga2_strdup(name);
    json_free(getter);
    char *origin = ga2_str_or_empty(ga2_is_obj(params) ? json_obj_get(params, "origin") : NULL);
    char *site = ga2_str_or_empty(ga2_is_obj(params) ? json_obj_get(params, "site") : NULL);
    json_t *a = json_object();
    json_set_hdr(a, "origin", origin);
    json_set_hdr(a, "site", site);
    free(origin); free(site);
    json_t *answer = ga2_call(c, dup, a);
    free(dup);
    GA2_CHK(answer);
    if (!ga2_is_obj(answer) || !ga2_truthy(json_obj_get(answer, "password"))) {
        json_free(answer);
        return json_string("");
    }
    char *ident = ga2_str_or_empty(json_obj_get(answer, "identifier"));
    char *pw = ga2_str_or_empty(json_obj_get(answer, "password"));
    char *ser = ga2_py_dumps2(ident, pw);
    free(ident); free(pw);
    json_free(answer);   /* finally: answer.clear() — local copy, freed */
    json_t *out = json_string(ser);
    free(ser);
    return out;
}
/* ── _ask_vault_code ── */
/* PoP: ga2_ask_vault_code @ hermes_cli/gateway_agent.py:_ask_vault_code */
static json_t *ga2_ask_vault_code(ga2_ctx_t *c, json_t *params) {
    json_t *getter = ga2_call(c, "get_code_prompt_callback", json_object());
    GA2_CHK(getter);
    const char *name = ga2_str(getter);
    if (!name) { json_free(getter); return json_string(""); }
    char *dup = ga2_strdup(name);
    json_free(getter);
    char *site = ga2_str_or_empty(ga2_is_obj(params) ? json_obj_get(params, "site") : NULL);
    char *hint = ga2_str_or_empty(ga2_is_obj(params) ? json_obj_get(params, "hint") : NULL);
    json_t *a = json_object();
    json_set_hdr(a, "site", site);
    json_set_hdr(a, "hint", hint);
    free(site); free(hint);
    json_t *r = ga2_call(c, dup, a);
    free(dup);
    GA2_CHK(r);
    if (ga2_truthy(r)) return r;
    json_free(r);
    return json_string("");
}

/* ── _answer_request ── */
/* PoP: ga2_answer_request @ hermes_cli/gateway_agent.py:_answer_request */
static json_t *ga2_answer_request(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *frame = json_obj_get(args, "frame");
    if (!ga2_is_obj(frame)) frame = NULL;
    json_t *method = frame ? json_obj_get(frame, "method") : NULL;
    if (!method) return ga2_keyerror("method");
    json_t *id = frame ? json_obj_get(frame, "id") : NULL;
    if (!id) return ga2_keyerror("id");
    const char *ms = ga2_str(method);
    json_t *open = json_array();
    json_append(open, json_copy(id));
    json_append(open, json_copy(method));
    json_set(st, "_open_request", open);
    json_t *params = frame ? json_obj_get(frame, "params") : NULL;
    json_t *pempty = json_object();
    if (!ga2_truthy(params)) params = pempty;   /* frame.get("params") or {} */
    json_t *answer = NULL;
    if (ms && !strcmp(ms, "sudo")) answer = ga2_ask_sudo(c, params);
    else if (ms && !strcmp(ms, "secret")) {
        json_t *a = json_object(); ga2_set_j(a, "params", params);
        answer = ga2_call(c, "self._ask_secret", a);
    }
    else if (ms && !strcmp(ms, "vault.unlock_prompt")) answer = ga2_ask_vault_unlock(c, params);
    else if (ms && !strcmp(ms, "vault.save_login")) answer = ga2_ask_vault_save_login(c, params);
    else if (ms && !strcmp(ms, "vault.code")) answer = ga2_ask_vault_code(c, params);
    else if (ms && !strcmp(ms, "clarify")) answer = ga2_ask_clarify(c, params);
    else answer = ga2_keyerror(ms ? ms : "");
    if (ga2_is_raise(answer)) {
        if (ga2_is_exception(answer)) {
            json_free(answer);
            if (ms && !strcmp(ms, "clarify")) {
                answer = json_object();
                json_set_hdr(answer, "outcome", "cancelled");
            } else {
                answer = json_string("");
            }
        } else {
            json_set(st, "_open_request", json_null());   /* finally */
            json_free(pempty);
            return answer;   /* BaseException passthrough */
        }
    }
    json_set(st, "_open_request", json_null());   /* finally */
    json_t *reply = json_object();
    ga2_set_j(reply, "id", id);
    if (ms && !strcmp(ms, "clarify")) {
        ga2_set_j(reply, "result", answer);
    } else {
        json_t *val = json_object();
        json_set_hdr(val, "value", ga2_is_str(answer) ? ga2_str(answer) : "");
        json_set(reply, "result", val);
    }
    json_free(answer);
    json_t *r = ga2_call(c, "connection.reply", reply);
    GA2_CHK(r);
    json_free(r);
    json_free(pempty);
    return json_null();
}

/* ── _voice_stopped ── */
/* PoP: ga2_voice_stopped @ hermes_cli/gateway_agent.py:_voice_stopped */
static json_t *ga2_voice_stopped(ga2_ctx_t *c, json_t *args) {
    json_t *st = c->state;
    json_t *r = ga2_call(c, "self._give_back_turn_route", json_object());
    GA2_CHK(r); json_free(r);
    json_t *ta = ga2_args_str("key", "cli.voice.stop_phrase_typed");
    json_t *tr = ga2_call(c, "t", ta);
    GA2_CHK(tr);
    char *ts = ga2_strdup(ga2_str(tr) ? ga2_str(tr) : "");
    json_free(tr);
    json_t *pa = ga2_args_str("text", ts);
    free(ts);
    json_t *pr = ga2_call(c, "self._emit_print", pa);
    GA2_CHK(pr); json_free(pr);
    json_t *out = json_object();
    json_set_hdr(out, "final_response", "");
    json_set(out, "failed", json_bool(false));
    json_set(out, "partial", json_bool(false));
    json_set(out, "error", json_null());
    json_set(out, "failure_reason", json_null());
    json_set(out, "completed", json_bool(true));
    json_set(out, "interrupted", json_bool(false));
    json_set(out, "interrupt_message", json_null());
    json_set(out, "api_calls", json_number(0));
    json_set(out, "pending_steer", json_null());
    json_set(out, "last_reasoning", json_null());
    json_set(out, "response_previewed", json_bool(false));
    json_set(out, "response_transformed", json_bool(false));
    json_set(out, "pre_transform_response", json_null());
    json_set(out, "billing_block", json_null());
    json_set(out, "auxiliary_usage", json_object());
    json_t *history = json_obj_get(args, "history");
    json_set(out, "messages", ga2_is_arr(history) ? json_copy(history) : json_array());
    json_set(out, "voice_stopped", json_bool(true));
    ga2_set_s(out, "session_id", json_get_str(st, "_session_id", ""));
    return out;
}

/* ── dispatch ── */
typedef json_t *(*ga2_fn_t)(ga2_ctx_t *, json_t *);
typedef struct { const char *func; ga2_fn_t fn; } ga2_case_t;
static const ga2_case_t GA2_CASES[] = {
    {"tool_progress_mode", ga2_tool_progress_mode_set},
    {"pin_reasoning", ga2_pin_reasoning},
    {"stage_turn_reasoning", ga2_stage_turn_reasoning},
    {"restore_reasoning", ga2_restore_reasoning},
    {"reasoning_callback", ga2_reasoning_callback_set},
    {"_close_segment", NULL},   /* special: starts/separator args */
    {"_on_delta", ga2_on_delta},
    {"_on_interim", ga2_on_interim},
    {"_on_tool_generating", ga2_on_tool_generating},
    {"_on_tool_progress", ga2_on_tool_progress},
    {"_on_tool_start", ga2_on_tool_start},
    {"_on_tool_complete", ga2_on_tool_complete},
    {"_todo_from_result", ga2_todo_from_result},
    {"_set_todos", NULL},       /* special: snapshot arg */
    {"_on_todo", ga2_on_todo},
    {"_on_control_update", ga2_on_control_update},
    {"_on_thinking", ga2_on_thinking},
    {"_on_reasoning", ga2_on_reasoning},
    {"_on_reasoning_available", ga2_on_reasoning_available},
    {"_on_notice", ga2_on_notice},
    {"_on_notice_clear", ga2_on_notice_clear},
    {"_on_reaction", ga2_on_reaction},
    {"_on_connection_request", ga2_on_connection_request},
    {"_on_connection_update", ga2_on_connection_update},
    {"_restore_connection_card", ga2_restore_connection_card},
    {"connection_respond", ga2_connection_respond},
    {"connection_retry", ga2_connection_retry},
    {"_on_request_withdrawn", ga2_on_request_withdrawn},
    {"_ask_clarify", NULL},     /* special: params arg */
    {"_ask_sudo", NULL},
    {"_ask_vault_unlock", NULL},
    {"_ask_vault_save_login", NULL},
    {"_ask_vault_code", NULL},
    {"_answer_request", ga2_answer_request},
    {"_voice_stopped", ga2_voice_stopped},
    {"_refresh_stats", NULL},   /* special: turn arg */
};
#define GA2_CASE_COUNT (sizeof(GA2_CASES) / sizeof(GA2_CASES[0]))

static json_t *ga2_run(const char *func, ga2_ctx_t *c, json_t *args) {
    if (!strcmp(func, "_close_segment")) {
        json_t *starts = json_obj_get(args, "starts");
        return ga2_close_segment(c, ga2_is_obj(starts) ? starts : NULL,
                                 json_get_bool(args, "separator", false));
    }
    if (!strcmp(func, "_set_todos")) return ga2_set_todos(c, json_obj_get(args, "snapshot"));
    if (!strcmp(func, "_refresh_stats")) return ga2_refresh_stats(c, json_obj_get(args, "turn"));
    if (!strcmp(func, "_ask_clarify")) return ga2_ask_clarify(c, json_obj_get(args, "params"));
    if (!strcmp(func, "_ask_sudo")) return ga2_ask_sudo(c, json_obj_get(args, "params"));
    if (!strcmp(func, "_ask_vault_unlock")) return ga2_ask_vault_unlock(c, json_obj_get(args, "params"));
    if (!strcmp(func, "_ask_vault_save_login")) return ga2_ask_vault_save_login(c, json_obj_get(args, "params"));
    if (!strcmp(func, "_ask_vault_code")) return ga2_ask_vault_code(c, json_obj_get(args, "params"));
    for (size_t i = 0; i < GA2_CASE_COUNT; i++) {
        if (GA2_CASES[i].fn && !strcmp(GA2_CASES[i].func, func))
            return GA2_CASES[i].fn(c, args);
    }
    return ga2_err("RuntimeError", "unknown func");
}