/*
 * telegram_adapter_flow8.c - Telegram adapter FLOW part 8 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the GROUP MENTION
 * GATING + OBSERVE/ROUTING DECISION surface left UNCLAIMED after
 * telegram_adapter_flow{,2,3,6,7}.c.  Every target below was confirmed
 * UNCLAIMED before this file was written; no flow 1-7 target is re-ported.
 *
 * Coherent theme: scoped config gates and chat/topic allowlists, mention
 * patterns, and the group observe/dispatch decision chain.  All live I/O and
 * injected callables enter through ONE universal seam: ta8_call(ctx,name,args).
 * Claimed siblings / unported collaborators are seamed.  Pure leaves are
 * transcribed inline (no seam, no PoP re-claim).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * Every symbol is static and ta8_-prefixed.
 *
 * SEAM NAME SET (must equal oracle SEAM_NAMES exactly):
 *   see tests/oracle_telegram_adapter_flow8.py SEAM_NAMES.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow8.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA8_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 * - Python `is False` gates are identity checks, not truthiness: only a real
 *   JSON_BOOL false satisfies them (ta8_is_false).
 * - Attribute-direct reads raise AttributeError when absent; getattr-with-
 *   default reads NULL.  Nested doubles are plain attribute doubles.
 * - into arrays: membership is by string equality.  ints render as %.0f.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct ta8_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta8_ctx_t;

/* ── basic helpers ── */
static bool ta8_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta8_raise_kind(const json_t *v) {
    return ta8_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta8_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta8_present(const json_t *v) { return v && v->type != JSON_NULL; }
static bool ta8_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta8_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta8_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const char *ta8_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 1e16) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static const json_t *ta8_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta8_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta8_attr(const json_t *obj, const char *key,
                              const char *tname, json_t **err) {
    char msg[220];
    if (!obj || obj->type == JSON_NULL) {
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", key);
        *err = ta8_err("AttributeError", msg);
        return NULL;
    }
    if (obj->type != JSON_OBJECT) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", ta8_type_name(obj), key);
        *err = ta8_err("AttributeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tname, key);
        *err = ta8_err("AttributeError", msg);
        return NULL;
    }
    return v;
}

/* ── string helpers ── */
static char *ta8_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta8_lower(const char *s) {
    char *o = ta8_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta8_exc_row_t;
static const ta8_exc_row_t TA8_EXC_HIER[] = {
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"}, {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"BufferError", "Exception"},
    {"BytesWarning", "Warning"}, {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"}, {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"}, {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"}, {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"}, {"GeneratorExit", "BaseException"},
    {"ImportError", "Exception"}, {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"}, {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"}, {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"}, {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"}, {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"}, {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"}, {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"}, {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"}, {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"}, {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"}, {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"}, {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"}, {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"}, {"SystemError", "Exception"},
    {"SystemExit", "BaseException"}, {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"}, {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"}, {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"}, {"ValueError", "Exception"}, {"Warning", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"}, {"_IncompleteInputError", "SyntaxError"},
};
static const char *ta8_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA8_EXC_HIER) / sizeof(TA8_EXC_HIER[0]); i++)
        if (strcmp(name, TA8_EXC_HIER[i].name) == 0) return TA8_EXC_HIER[i].parent;
    return NULL;
}
static bool ta8_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta8_builtin_parent(cur);
    }
    return false;
}
static bool ta8_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta8_builtin_parent(kind) == NULL) return true;
    return ta8_chain_has(kind, "Exception");
}

/* ── exception-graph registry ── */
static const json_t *ta8_err_obj(const ta8_ctx_t *ctx, const char *eid) {
    if (!eid || !ctx->errors || ctx->errors->type != JSON_OBJECT) return NULL;
    return json_obj_get(ctx->errors, eid);
}
static const char *ta8_err_class(const ta8_ctx_t *ctx, const char *eid) {
    const json_t *e = ta8_err_obj(ctx, eid);
    return e ? json_get_str(e, "class", "Exception") : NULL;
}
static const char *ta8_err_text(const ta8_ctx_t *ctx, const char *eid) {
    const json_t *e = ta8_err_obj(ctx, eid);
    return e ? json_get_str(e, "message", "") : "";
}
static const json_t *ta8_err_attr(const ta8_ctx_t *ctx, const char *eid, const char *key) {
    const json_t *e = ta8_err_obj(ctx, eid);
    return e ? json_obj_get(e, key) : NULL;
}
static const char *ta8_err_link(const ta8_ctx_t *ctx, const char *eid, const char *key) {
    return ta8_str(ta8_err_attr(ctx, eid, key));
}
/* canon(live exception) == {"__raise__": class, "message": str(exc)} */
static json_t *ta8_canon_err(const ta8_ctx_t *ctx, const char *eid) {
    if (!eid) return json_null();
    json_t *o = json_object();
    const char *cls = ta8_err_class(ctx, eid);
    json_set_hdr(o, "__raise__", cls ? cls : "Exception");
    json_set_hdr(o, "message", ta8_err_text(ctx, eid));
    return o;
}
/* DFS (cycle-safe) graph traversal; fills ids[] with eids in yield order.
 * stack.extend(cause, context) then pop -> context is visited before cause. */
static size_t ta8_graph(const ta8_ctx_t *ctx, const char *eid,
                        const char **ids, size_t cap) {
    const char *stack[256]; size_t sp = 0;
    const char *seen[256]; size_t ns = 0; size_t out = 0;
    if (eid && sp < 256) stack[sp++] = eid;
    while (sp && out < cap) {
        const char *cur = stack[--sp];
        bool dup = false;
        for (size_t i = 0; i < ns; i++) if (strcmp(seen[i], cur) == 0) { dup = true; break; }
        if (dup) continue;
        if (ns < 256) seen[ns++] = cur;
        ids[out++] = cur;
        const char *cause = ta8_err_link(ctx, cur, "cause");
        const char *context = ta8_err_link(ctx, cur, "context");
        if (cause && sp < 256) stack[sp++] = cause;
        if (context && sp < 256) stack[sp++] = context;
    }
    return out;
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *ta8_call(ta8_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta8_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta8_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta8_err(json_get_str(raise, "type", "RuntimeError"),
                              json_get_str(raise, "message", ""));
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}


/* ── flow8-extra helpers ── */
#include <stdarg.h>
static bool ta8_is_false(const json_t *v) {
    return v && v->type == JSON_BOOL && !v->bool_val;
}
static char *ta8_fmt(const char *fmt, ...) {
    char tmp[128]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta8_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta8_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
static char *ta8_strip(const char *s) {
    if (!s) return ta8_strdup("");
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return ta8_strdup("");
    memcpy(o, s + a, b - a); o[b - a] = '\0';
    return o;
}
/* Python str() of a JSON scalar; integral doubles render as ints. */
static char *ta8_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta8_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta8_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta8_fmt("%.0f", d);
            return ta8_fmt("%.17g", d);
        }
        case JSON_STRING: return ta8_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta8_strdup(""); }
    }
}
static bool ta8_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *x = ta8_str(json_get(arr, i));
        if (x && strcmp(x, s) == 0) return true;
    }
    return false;
}
static json_t *ta8_sarg(const char *k, const char *v) {
    json_t *a = json_object(); json_set_hdr(a, k, v); return a;
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta8_telegram_observe_unmentioned_group_messages @ plugins/platforms/telegram/adapter.py:_telegram_observe_unmentioned_group_messages */
static json_t *ta8_telegram_observe_unmentioned_group_messages(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "observe_unmentioned_group_messages");
    json_set_hdr(a, "env_name", "TELEGRAM_OBSERVE_UNMENTIONED_GROUP_MESSAGES");
    json_set_hdr(a, "default", "false");
    json_t *fk = json_array();
    json_append(fk, json_string("ingest_unmentioned_group_messages"));
    json_set(a, "fallback_keys", fk);
    return ta8_call(ctx, "_extra_bool", a);
}

/* PoP: ta8_telegram_guest_mode @ plugins/platforms/telegram/adapter.py:_telegram_guest_mode */
static json_t *ta8_telegram_guest_mode(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "guest_mode");
    json_set_hdr(a, "env_name", "TELEGRAM_GUEST_MODE");
    json_set_hdr(a, "default", "false");
    json_set(a, "fallback_keys", json_array());
    return ta8_call(ctx, "_extra_bool", a);
}

/* PoP: ta8_telegram_free_response_chats @ plugins/platforms/telegram/adapter.py:_telegram_free_response_chats */
static json_t *ta8_telegram_free_response_chats(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = ta8_sarg("key", "free_response_chats");
    json_set_hdr(a, "env_name", "TELEGRAM_FREE_RESPONSE_CHATS");
    return ta8_call(ctx, "_extra_str_set", a);
}

/* PoP: ta8_telegram_free_response_topics @ plugins/platforms/telegram/adapter.py:_telegram_free_response_topics */
static json_t *ta8_telegram_free_response_topics(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = ta8_sarg("key", "free_response_topics");
    json_set_hdr(a, "env_name", "TELEGRAM_FREE_RESPONSE_TOPICS");
    return ta8_call(ctx, "_extra_str_set", a);
}

/* PoP: ta8_telegram_allowed_chats @ plugins/platforms/telegram/adapter.py:_telegram_allowed_chats */
static json_t *ta8_telegram_allowed_chats(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = ta8_sarg("key", "allowed_chats");
    json_set_hdr(a, "env_name", "TELEGRAM_ALLOWED_CHATS");
    return ta8_call(ctx, "_extra_str_set", a);
}

/* PoP: ta8_telegram_group_allowed_chats @ plugins/platforms/telegram/adapter.py:_telegram_group_allowed_chats */
static json_t *ta8_telegram_group_allowed_chats(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = ta8_sarg("key", "group_allowed_chats");
    json_set_hdr(a, "env_name", "TELEGRAM_GROUP_ALLOWED_CHATS");
    return ta8_call(ctx, "_extra_str_set", a);
}

/* PoP: ta8_telegram_allowed_topics @ plugins/platforms/telegram/adapter.py:_telegram_allowed_topics */
static json_t *ta8_telegram_allowed_topics(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = ta8_sarg("key", "allowed_topics");
    json_set_hdr(a, "env_name", "TELEGRAM_ALLOWED_TOPICS");
    return ta8_call(ctx, "_extra_str_set", a);
}

/* PoP: ta8_reactions_enabled @ plugins/platforms/telegram/adapter.py:_reactions_enabled */
static json_t *ta8_reactions_enabled(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_t *cfg = json_obj_get(ctx->state, "config");
    json_t *extra = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "extra") : NULL;
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set_hdr(a, "key", "reactions");
    json_set_hdr(a, "env_name", "TELEGRAM_REACTIONS");
    json_set(a, "default", json_null());
    json_t *r = ta8_call(ctx, "_extra_or_secret", a);
    if (ta8_is_raise(r)) return r;
    if (ta8_is_null(r)) { json_free(r); return json_bool(false); }
    char *s = ta8_lower(ta8_py_str(r));
    bool hit = s && (strcmp(s, "false") == 0 || strcmp(s, "0") == 0 || strcmp(s, "no") == 0);
    free(s); json_free(r);
    return json_bool(!hit);
}

/* PoP: ta8_telegram_auth_env_configured @ plugins/platforms/telegram/adapter.py:_telegram_auth_env_configured */
static json_t *ta8_telegram_auth_env_configured(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *const keys[] = {
        "TELEGRAM_ALLOWED_USERS", "TELEGRAM_GROUP_ALLOWED_USERS",
        "TELEGRAM_GROUP_ALLOWED_CHATS", "TELEGRAM_ALLOW_ALL_USERS",
        "GATEWAY_ALLOWED_USERS", "GATEWAY_ALLOW_ALL_USERS"
    };
    for (size_t i = 0; i < 6; i++) {
        json_t *a = json_object();
        json_set_hdr(a, "key", keys[i]);
        json_set_hdr(a, "default", "");
        json_t *r = ta8_call(ctx, "_scoped_gate_env", a);
        if (ta8_is_raise(r)) return r;
        char *s = ta8_strip(ta8_str(r) ? ta8_str(r) : ta8_py_str(r));
        bool hit = s && s[0] != '\0';
        free(s); json_free(r);
        if (hit) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: ta8_topic_id_or_general @ plugins/platforms/telegram/adapter.py:_topic_id_or_general */
static json_t *ta8_topic_id_or_general(ta8_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *tid = ta8_arg(args, "thread_id");
    if (ta8_is_null(tid)) return json_string("1");
    char *s = ta8_py_str(tid);
    json_t *r = json_string(s ? s : ""); free(s); return r;
}

/* PoP: ta8_telegram_group_observe_attributed_text @ plugins/platforms/telegram/adapter.py:_telegram_group_observe_attributed_text */
static json_t *ta8_telegram_group_observe_attributed_text(ta8_ctx_t *ctx, const json_t *args) {
    const json_t *ev = ta8_arg(args, "event");
    const json_t *src = ta8_getattr(ev, "source");
    const json_t *uidv = ta8_getattr(src, "user_id");
    json_t *uid_val = ta8_truthy(uidv) ? json_copy((json_t *)uidv) : json_string("unknown");
    const json_t *unv = ta8_getattr(src, "user_name");
    json_t *tagarg = ta8_truthy(unv) ? json_copy((json_t *)unv) : json_copy(uid_val);
    json_t *a = json_object();
    json_set(a, "value", tagarg);
    json_t *tag = ta8_call(ctx, "tag_safe_display_name", a);
    if (ta8_is_raise(tag)) { json_free(uid_val); return tag; }
    char *tagstr = ta8_py_str(tag);
    char *uids = ta8_py_str(uid_val);
    const json_t *tv = ta8_getattr(ev, "text");
    char *text = ta8_truthy(tv) ? ta8_py_str(tv) : ta8_strdup("");
    char *out = ta8_fmt("[%s|%s]\n%s", tagstr ? tagstr : "", uids ? uids : "", text ? text : "");
    free(tagstr); free(uids); free(text); json_free(tag); json_free(uid_val);
    json_t *r = json_string(out ? out : ""); free(out); return r;
}

/* PoP: ta8_telegram_group_observe_shared_source @ plugins/platforms/telegram/adapter.py:_telegram_group_observe_shared_source */
static json_t *ta8_telegram_group_observe_shared_source(ta8_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *src = ta8_arg(args, "source");
    json_set(a, "source", src ? json_copy(src) : json_null());
    json_set(a, "user_id", json_null());
    json_set(a, "user_name", json_null());
    json_set(a, "user_id_alt", json_null());
    json_set(a, "shared_session", json_bool(true));
    return ta8_call(ctx, "dataclasses.replace", a);
}

/* PoP: ta8_apply_telegram_group_observe_attribution @ plugins/platforms/telegram/adapter.py:_apply_telegram_group_observe_attribution */
static json_t *ta8_apply_telegram_group_observe_attribution(ta8_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *ev = ta8_arg(args, "event");
    json_set(a, "event", ev ? json_copy(ev) : json_null());
    return ta8_call(ctx, "apply_group_observe_attribution", a);
}

/* PoP: ta8_telegram_group_observe_channel_prompt @ plugins/platforms/telegram/adapter.py:_telegram_group_observe_channel_prompt */
static json_t *ta8_telegram_group_observe_channel_prompt(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *u = ta8_call(ctx, "_current_bot_username", json_object());
    if (ta8_is_raise(u)) return u;
    char *username = ta8_truthy(u) ? ta8_py_str(u) : ta8_strdup("unknown");
    const json_t *bot = json_obj_get(ctx->state, "_bot");
    const json_t *idv = (bot && bot->type == JSON_OBJECT) ? json_obj_get(bot, "id") : NULL;
    char *bid = ta8_truthy(idv) ? ta8_py_str(idv) : ta8_strdup("unknown");
    char *out = ta8_fmt(
        "You are handling a Telegram group chat message.\n"
        "- Your identity: user_id=%s, @-mention name in this group=@%s\n"
        "- observed Telegram group context may be provided in a separate context-only block "
        "before the current message; it is not necessarily addressed to you.\n"
        "- Treat only the current new message as a request explicitly directed at you, "
        "and use observed context only when the current message asks for it.",
        bid ? bid : "unknown", username ? username : "unknown");
    free(username); free(bid); json_free(u);
    json_t *r = json_string(out ? out : ""); free(out); return r;
}

/* PoP: ta8_message_matches_mention_patterns @ plugins/platforms/telegram/adapter.py:_message_matches_mention_patterns */
static json_t *ta8_message_matches_mention_patterns(ta8_ctx_t *ctx, const json_t *args) {
    const json_t *pats = json_obj_get(ctx->state, "_mention_patterns");
    if (!ta8_truthy(pats)) return json_bool(false);
    const json_t *msg = ta8_arg(args, "message");
    json_t *a0 = json_object();
    json_set(a0, "message", msg ? json_copy(msg) : json_null());
    json_t *tv = ta8_call(ctx, "addressing_text", a0);
    if (ta8_is_raise(tv)) return tv;
    char *text = ta8_truthy(tv) ? ta8_py_str(tv) : ta8_strdup("");
    json_free(tv);
    if (!text || text[0] == '\0') { free(text); return json_bool(false); }
    for (size_t i = 0; i < json_len(pats); i++) {
        const json_t *pat = json_get(pats, i);
        json_t *a = json_object();
        json_set(a, "pattern", pat ? json_copy(pat) : json_null());
        json_set_hdr(a, "text", text);
        json_t *hit = ta8_call(ctx, "pattern.search", a);
        if (ta8_is_raise(hit)) { free(text); return hit; }
        bool yes = ta8_truthy(hit);
        json_free(hit);
        if (yes) { free(text); return json_bool(true); }
    }
    free(text);
    return json_bool(false);
}

/* PoP: ta8_compile_mention_patterns @ plugins/platforms/telegram/adapter.py:_compile_mention_patterns */
static json_t *ta8_compile_mention_patterns(ta8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a0 = ta8_sarg("key", "TELEGRAM_MENTION_PATTERNS");
    json_set_hdr(a0, "default", "");
    json_t *ev = ta8_call(ctx, "_scoped_gate_env", a0);
    if (ta8_is_raise(ev)) return ev;
    char *raw = ta8_strip(ta8_str(ev) ? ta8_str(ev) : ta8_py_str(ev));
    json_free(ev);
    json_t *patterns = NULL;  /* borrowed-ish: owned when we create it */
    bool patterns_owned = false;
    if (raw && raw[0] != '\0') {
        json_t *a1 = ta8_sarg("s", raw);
        json_t *p = ta8_call(ctx, "json.loads", a1);
        if (ta8_is_raise(p)) {
            if (!ta8_is_exception(ta8_raise_kind(p))) { free(raw); return p; }
            json_free(p);
            json_t *arr = json_array();
            char *save = raw;
            for (char *ln = save; ln && *ln;) {
                char *nl = strchr(ln, '\n');
                if (nl) *nl = '\0';
                char *part = ta8_strip(ln);
                if (part && part[0]) json_append(arr, json_string(part));
                free(part);
                ln = nl ? nl + 1 : NULL;
            }
            if (json_len(arr) == 0) {
                char *tok = strtok(save, ",");
                while (tok) {
                    char *part = ta8_strip(tok);
                    if (part && part[0]) json_append(arr, json_string(part));
                    free(part);
                    tok = strtok(NULL, ",");
                }
            }
            patterns = arr; patterns_owned = true;
        } else {
            patterns = p; patterns_owned = true;
        }
    } else {
        json_t *cfg = json_obj_get(ctx->state, "config");
        json_t *extra = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "extra") : NULL;
        const json_t *p = extra ? json_obj_get(extra, "mention_patterns") : NULL;
        patterns = p ? json_copy((json_t *)p) : json_null();
        patterns_owned = true;
    }
    free(raw);
    if (ta8_is_null(patterns)) { if (patterns_owned) json_free(patterns); return json_array(); }
    json_t *a2 = json_object();
    json_set(a2, "patterns", patterns);
    {
        const json_t *nm = json_obj_get(ctx->state, "name");
        json_set_hdr(a2, "log_prefix", ta8_str(nm) ? ta8_str(nm) : "adapter");
    }
    json_set_hdr(a2, "platform_label", "telegram");
    json_set_hdr(a2, "display_label", "Telegram");
    json_t *r = ta8_call(ctx, "compile_mention_patterns", a2);
    return r;
}

/* PoP: ta8_gate_or_observe @ plugins/platforms/telegram/adapter.py:_gate_or_observe */
static json_t *ta8_gate_or_observe(ta8_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta8_arg(args, "msg");
    json_t *a0 = json_object();
    json_set(a0, "msg", msg ? json_copy(msg) : json_null());
    json_t *b0 = ta8_call(ctx, "_should_process_message", a0);
    if (ta8_is_raise(b0)) return b0;
    if (ta8_truthy(b0)) { json_free(b0); return json_bool(true); }
    json_free(b0);
    json_t *a1 = json_object();
    json_set(a1, "msg", msg ? json_copy(msg) : json_null());
    json_t *b1 = ta8_call(ctx, "_should_observe_unmentioned_group_message", a1);
    if (ta8_is_raise(b1)) return b1;
    if (!ta8_truthy(b1)) { json_free(b1); return json_bool(false); }
    json_free(b1);
    json_t *a2 = json_object();
    json_set(a2, "msg", msg ? json_copy(msg) : json_null());
    const json_t *mt = ta8_arg(args, "msg_type");
    json_set(a2, "msg_type", mt ? json_copy(mt) : json_null());
    const json_t *upd = ta8_arg(args, "update");
    const json_t *uid = ta8_getattr(upd, "update_id");
    json_set(a2, "update_id", uid ? json_copy((json_t *)uid) : json_null());
    json_t *b2 = ta8_call(ctx, "_observe_unmentioned_group_message", a2);
    if (ta8_is_raise(b2)) return b2;
    json_free(b2);
    return json_bool(false);
}

/* PoP: ta8_should_observe_unmentioned_group_message @ plugins/platforms/telegram/adapter.py:_should_observe_unmentioned_group_message */
static json_t *ta8_should_observe_unmentioned_group_message(ta8_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta8_arg(args, "message");
    /* line 1: own | not observe_gate | not group -> False (or short-circuits) */
    json_t *a0 = json_object();
    json_set(a0, "message", msg ? json_copy(msg) : json_null());
    json_t *b0 = ta8_call(ctx, "_is_own_message", a0);
    if (ta8_is_raise(b0)) return b0;
    if (ta8_truthy(b0)) { json_free(b0); return json_bool(false); }
    json_free(b0);
    json_t *b1 = ta8_call(ctx, "_telegram_observe_unmentioned_group_messages", json_object());
    if (ta8_is_raise(b1)) return b1;
    if (!ta8_truthy(b1)) { json_free(b1); return json_bool(false); }
    json_free(b1);
    json_t *a2 = json_object();
    json_set(a2, "message", msg ? json_copy(msg) : json_null());
    json_t *b2 = ta8_call(ctx, "_is_group_chat", a2);
    if (ta8_is_raise(b2)) return b2;
    if (!ta8_truthy(b2)) { json_free(b2); return json_bool(false); }
    json_free(b2);
    /* topic gates: `is False` identity check */
    const json_t *tid = ta8_getattr(msg, "message_thread_id");
    json_t *a3 = json_object();
    json_set(a3, "thread_id", tid ? json_copy((json_t *)tid) : json_null());
    json_set(a3, "warn_non_numeric", json_bool(false));
    json_set(a3, "message", msg ? json_copy(msg) : json_null());
    json_t *b3 = ta8_call(ctx, "_topic_gates_pass", a3);
    if (ta8_is_raise(b3)) return b3;
    bool gated = ta8_is_false(b3);
    json_free(b3);
    if (gated) return json_bool(false);
    /* chat id string (ONCE; kept alive for both membership checks) */
    json_t *a4 = json_object();
    json_set(a4, "message", msg ? json_copy(msg) : json_null());
    json_t *cid = ta8_call(ctx, "_chat_id_str", a4);
    if (ta8_is_raise(cid)) return cid;
    char *cids = ta8_strdup(ta8_str(cid) ? ta8_str(cid) : "");
    json_free(cid);
    /* exclusive mentions && exclude-self */
    json_t *b5 = ta8_call(ctx, "_telegram_exclusive_bot_mentions", json_object());
    if (ta8_is_raise(b5)) { free(cids); return b5; }
    if (ta8_truthy(b5)) {
        json_free(b5);
        json_t *a6 = json_object();
        json_set(a6, "message", msg ? json_copy(msg) : json_null());
        json_set(a6, "log_skip", json_bool(false));
        json_t *b6 = ta8_call(ctx, "_explicit_bot_mentions_exclude_self", a6);
        if (ta8_is_raise(b6)) { free(cids); return b6; }
        if (ta8_truthy(b6)) { json_free(b6); free(cids); return json_bool(false); }
        json_free(b6);
    } else { json_free(b5); }
    /* observe allowlist (non-empty, membership) */
    json_t *al = ta8_call(ctx, "_telegram_observe_allowed_chats", json_object());
    if (ta8_is_raise(al)) { free(cids); return al; }
    bool allowed_ok = ta8_truthy(al) && ta8_arr_has_str(al, cids);
    json_free(al);
    if (!allowed_ok) { free(cids); return json_bool(false); }
    /* suppressed sender -> observe */
    json_t *a7 = json_object();
    json_set(a7, "message", msg ? json_copy(msg) : json_null());
    json_t *b7 = ta8_call(ctx, "_bot_sender_suppressed", a7);
    if (ta8_is_raise(b7)) { free(cids); return b7; }
    if (ta8_truthy(b7)) { json_free(b7); free(cids); return json_bool(true); }
    json_free(b7);
    /* free-response chat -> not observed */
    json_t *fr = ta8_call(ctx, "_telegram_free_response_chats", json_object());
    if (ta8_is_raise(fr)) { free(cids); return fr; }
    bool free_chat = ta8_arr_has_str(fr, cids);
    json_free(fr);
    if (free_chat) { free(cids); return json_bool(false); }
    json_t *a9 = json_object();
    json_set(a9, "message", msg ? json_copy(msg) : json_null());
    json_t *b9 = ta8_call(ctx, "_telegram_is_free_response_topic", a9);
    if (ta8_is_raise(b9)) { free(cids); return b9; }
    if (ta8_truthy(b9)) { json_free(b9); free(cids); return json_bool(false); }
    json_free(b9); free(cids);
    /* require_mention off | reply-to-bot | mentions-bot -> False */
    json_t *b10 = ta8_call(ctx, "_telegram_require_mention", json_object());
    if (ta8_is_raise(b10)) return b10;
    if (!ta8_truthy(b10)) { json_free(b10); return json_bool(false); }
    json_free(b10);
    json_t *a11 = json_object();
    json_set(a11, "message", msg ? json_copy(msg) : json_null());
    json_t *b11 = ta8_call(ctx, "_is_reply_to_bot", a11);
    if (ta8_is_raise(b11)) return b11;
    if (ta8_truthy(b11)) { json_free(b11); return json_bool(false); }
    json_free(b11);
    json_t *a12 = json_object();
    json_set(a12, "message", msg ? json_copy(msg) : json_null());
    json_t *b12 = ta8_call(ctx, "_message_mentions_bot", a12);
    if (ta8_is_raise(b12)) return b12;
    if (ta8_truthy(b12)) { json_free(b12); return json_bool(false); }
    json_free(b12);
    /* return not matches_patterns */
    json_t *a13 = json_object();
    json_set(a13, "message", msg ? json_copy(msg) : json_null());
    json_t *b13 = ta8_call(ctx, "_message_matches_mention_patterns", a13);
    if (ta8_is_raise(b13)) return b13;
    bool yes = ta8_truthy(b13);
    json_free(b13);
    return json_bool(!yes);
}
