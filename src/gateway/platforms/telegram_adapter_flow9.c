/*
 * telegram_adapter_flow9.c - Telegram adapter FLOW part 9 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the remaining
 * UNCLAIMED small surface left after telegram_adapter_flow{,2,3,6,7,8}.c.
 * Every target below was confirmed UNCLAIMED (not one of the 120 PoPs in
 * flows 1-8); no flow 1-8 target is re-ported.
 *
 * Coherent theme: HTML/catalog formatting (translated labels, exec-approval
 * header, picker buttons), small pure attribute/data helpers, config bridging
 * getters and the adapter factory.  All live I/O and injected callables enter
 * through ONE universal seam: ta9_call(ctx, name, args).  Pure leaves are
 * transcribed inline (no seam, no PoP re-claim).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * Every symbol is static and ta9_-prefixed.
 *
 * SEAM NAME SET (must equal oracle SEAM_NAMES exactly):
 *   see tests/oracle_telegram_adapter_flow9.py SEAM_NAMES.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow9.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA9_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 * - Python `not` truthiness is ta9_truthy; attribute reads with a default
 *   yield NULL when absent.  Nested doubles are plain attribute doubles.
 * - ints render as %.0f.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct ta9_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta9_ctx_t;

/* ── basic helpers ── */
static bool ta9_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta9_raise_kind(const json_t *v) {
    return ta9_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta9_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta9_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta9_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta9_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const char *ta9_type_name(const json_t *v) {
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
static const json_t *ta9_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta9_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta9_state_get(const ta9_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static double ta9_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* ── string helpers ── */
static char *ta9_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta9_lower(const char *s) {
    char *o = ta9_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta9_fmt(const char *fmt, ...) {
    char tmp[256]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta9_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta9_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
/* Python str() of a JSON scalar; integral doubles render as ints. */
static char *ta9_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta9_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta9_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta9_fmt("%.0f", d);
            return ta9_fmt("%.17g", d);
        }
        case JSON_STRING: return ta9_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta9_strdup(""); }
    }
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta9_exc_row_t;
static const ta9_exc_row_t TA9_EXC_HIER[] = {
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
static const char *ta9_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA9_EXC_HIER) / sizeof(TA9_EXC_HIER[0]); i++)
        if (strcmp(name, TA9_EXC_HIER[i].name) == 0) return TA9_EXC_HIER[i].parent;
    return NULL;
}
static bool ta9_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta9_builtin_parent(cur);
    }
    return false;
}
static bool ta9_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta9_builtin_parent(kind) == NULL) return true;
    return ta9_chain_has(kind, "Exception");
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *ta9_call(ta9_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta9_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta9_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta9_err(json_get_str(raise, "type", "RuntimeError"),
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

/* ── flow9 seam-arg helpers ── */
static json_t *ta9_sarg(const char *k, const char *v) {
    json_t *a = json_object(); json_set_hdr(a, k, v); return a;
}
/* t(key, **kwargs) */
static json_t *ta9_t(ta9_ctx_t *ctx, const char *key, json_t *kwargs) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set(a, "kwargs", kwargs ? kwargs : json_object());
    return ta9_call(ctx, "t", a);
}
static json_t *ta9_kwargs1(const char *k, const json_t *v) {
    json_t *a = json_object();
    json_set(a, k, v ? json_copy((json_t *)v) : json_null());
    return a;
}
static json_t *ta9_t1(ta9_ctx_t *ctx, const char *key, const char *k, const json_t *v) {
    return ta9_t(ctx, key, ta9_kwargs1(k, v));
}
/* _html.escape(value): record the RAW argument (Python passes it unaltered). */
static json_t *ta9_escape(ta9_ctx_t *ctx, const json_t *value) {
    json_t *a = json_object();
    json_set(a, "value", value ? json_copy((json_t *)value) : json_null());
    return ta9_call(ctx, "html_escape", a);
}
/* InlineKeyboardButton(text, callback_data=...) */
static json_t *ta9_ikb(ta9_ctx_t *ctx, const char *text, const char *cb) {
    json_t *a = json_object();
    json_set_hdr(a, "text", text);
    json_set_hdr(a, "callback_data", cb);
    return ta9_call(ctx, "InlineKeyboardButton", a);
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta9_bold_label_html @ plugins/platforms/telegram/adapter.py:_bold_label_html */
static json_t *ta9_bold_label_html(ta9_ctx_t *ctx, const json_t *args) {
    const char *line = ta9_str(ta9_arg(args, "line"));
    if (!line) line = "";
    const char *colon = strchr(line, ':');
    char *out = NULL;
    if (!colon) {
        json_t *e = ta9_escape(ctx, json_string(line));
        if (ta9_is_raise(e)) return e;
        char *s = ta9_py_str(e); json_free(e);
        out = ta9_fmt("<b>%s</b>", s ? s : ""); free(s);
    } else {
        size_t nl = (size_t)(colon - line);
        char *label = malloc(nl + 1);
        if (label) { memcpy(label, line, nl); label[nl] = '\0'; }
        const char *rest = colon + 1;
        json_t *e1 = ta9_escape(ctx, json_string(label ? label : ""));
        if (ta9_is_raise(e1)) { free(label); return e1; }
        json_t *e2 = ta9_escape(ctx, json_string(rest));
        if (ta9_is_raise(e2)) { free(label); json_free(e1); return e2; }
        char *s1 = ta9_py_str(e1), *s2 = ta9_py_str(e2);
        out = ta9_fmt("<b>%s:</b>%s", s1 ? s1 : "", s2 ? s2 : "");
        free(s1); free(s2); free(label); json_free(e1); json_free(e2);
    }
    json_t *r = json_string(out ? out : ""); free(out); return r;
}

/* PoP: ta9_append_observed_note @ plugins/platforms/telegram/adapter.py:_append_observed_note */
static json_t *ta9_append_observed_note(ta9_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *existing = ta9_arg(args, "existing");
    const json_t *note = ta9_arg(args, "note");
    if (!ta9_truthy(note))
        return ta9_truthy(existing) ? json_copy((json_t *)existing) : json_string("");
    if (ta9_truthy(existing)) {
        char *a = ta9_py_str(existing), *b = ta9_py_str(note);
        char *out = ta9_fmt("%s\n\n%s", a ? a : "", b ? b : "");
        free(a); free(b);
        json_t *r = json_string(out ? out : ""); free(out); return r;
    }
    return json_copy((json_t *)note);
}

/* PoP: ta9_ea_header @ plugins/platforms/telegram/adapter.py:_EA_HEADER */
static json_t *ta9_ea_header(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *t = ta9_t(ctx, "gateway.exec_approval.header", NULL);
    if (ta9_is_raise(t)) return t;
    json_t *e = ta9_escape(ctx, t); json_free(t);
    if (ta9_is_raise(e)) return e;
    char *s = ta9_py_str(e); json_free(e);
    char *out = ta9_fmt("⚠️ <b>%s</b>\n\n", s ? s : "");
    free(s);
    json_t *r = json_string(out ? out : ""); free(out); return r;
}

/* PoP: ta9_ea_reason_label @ plugins/platforms/telegram/adapter.py:_EA_REASON_LABEL */
static json_t *ta9_ea_reason_label(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *t = ta9_t(ctx, "gateway.exec_approval.reason_label", NULL);
    if (ta9_is_raise(t)) return t;
    json_t *e = ta9_escape(ctx, t); json_free(t);
    if (ta9_is_raise(e)) return e;
    char *s = ta9_py_str(e); json_free(e);
    char *out = ta9_fmt("%s: ", s ? s : "");
    free(s);
    json_t *r = json_string(out ? out : ""); free(out); return r;
}

/* PoP: ta9_ea_escape @ plugins/platforms/telegram/adapter.py:_ea_escape */
static json_t *ta9_ea_escape(ta9_ctx_t *ctx, const json_t *args) {
    const json_t *text = ta9_arg(args, "text");
    return ta9_escape(ctx, text);
}

/* PoP: ta9_ea_action_labels @ plugins/platforms/telegram/adapter.py:_EA_ACTION_LABELS */
static json_t *ta9_ea_action_labels(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *const choices[] = {"once", "session", "always", "deny"};
    json_t *o = json_object();
    for (int i = 0; i < 4; i++) {
        char key[96];
        snprintf(key, sizeof key, "platform.telegram.approval.action_%s", choices[i]);
        json_t *v = ta9_t(ctx, key, NULL);
        if (ta9_is_raise(v)) { json_free(o); return v; }
        json_set(o, choices[i], v);
    }
    return o;
}

/* PoP: ta9_ea_smart_deny_line @ plugins/platforms/telegram/adapter.py:_EA_SMART_DENY_LINE */
static json_t *ta9_ea_smart_deny_line(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *t = ta9_t(ctx, "gateway.exec_approval.smart_deny_line", NULL);
    if (ta9_is_raise(t)) return t;
    json_t *inner_args = json_object();
    json_set(inner_args, "line", t);  /* adopted */
    json_t *b = ta9_bold_label_html(ctx, inner_args);
    if (ta9_is_raise(b)) return b;
    char *s = ta9_py_str(b); json_free(b);
    char *out = ta9_fmt("\n\n%s", s ? s : ""); free(s);
    json_t *r = json_string(out ? out : ""); free(out); return r;
}

/* PoP: ta9_provider_list_text @ plugins/platforms/telegram/adapter.py:_provider_list_text */
static json_t *ta9_provider_list_text(ta9_ctx_t *ctx, const json_t *args) {
    const json_t *cm = ta9_arg(args, "current_model");
    const json_t *pl = ta9_arg(args, "provider_label");
    const json_t *pi = ta9_arg(args, "page_info");
    char *model;
    if (ta9_truthy(cm)) {
        char *s = ta9_py_str(cm);
        model = ta9_fmt("`%s`", s ? s : ""); free(s);
    } else {
        json_t *u = ta9_t(ctx, "platform.telegram.picker.unknown_model", NULL);
        if (ta9_is_raise(u)) return u;
        char *s = ta9_py_str(u); json_free(u);
        model = ta9_fmt("`%s`", s ? s : ""); free(s);
    }
    json_t *t1 = ta9_t(ctx, "platform.telegram.picker.title", NULL);
    if (ta9_is_raise(t1)) { free(model); return t1; }
    json_t *o_model = json_string(model ? model : ""); free(model);
    json_t *t2 = ta9_t1(ctx, "platform.telegram.picker.current_model", "model", o_model);
    json_free(o_model);
    if (ta9_is_raise(t2)) { json_free(t1); return t2; }
    json_t *t3 = ta9_t1(ctx, "platform.telegram.picker.provider_label", "provider", pl);
    if (ta9_is_raise(t3)) { json_free(t1); json_free(t2); return t3; }
    json_t *t4 = ta9_t(ctx, "platform.telegram.picker.select_provider", NULL);
    if (ta9_is_raise(t4)) { json_free(t1); json_free(t2); json_free(t3); return t4; }
    char *s1 = ta9_py_str(t1), *s2 = ta9_py_str(t2);
    char *s3 = ta9_py_str(t3), *s4 = ta9_py_str(t4);
    char *pis = ta9_py_str(pi);
    char *out = ta9_fmt("⚙ *%s*\n\n%s\n%s\n\n%s%s",
                        s1 ? s1 : "", s2 ? s2 : "", s3 ? s3 : "",
                        s4 ? s4 : "", pis ? pis : "");
    free(s1); free(s2); free(s3); free(s4); free(pis);
    json_free(t1); json_free(t2); json_free(t3); json_free(t4);
    json_t *r = json_string(out ? out : ""); free(out); return r;
}

/* PoP: ta9_provider_button @ plugins/platforms/telegram/adapter.py:_provider_button */
static json_t *ta9_provider_button(ta9_ctx_t *ctx, const json_t *args) {
    const json_t *p = ta9_arg(args, "p");
    const json_t *total = ta9_getattr(p, "total_models");
    double count_num;
    char *count_s;
    if (total) {
        count_s = ta9_py_str(total);
    } else {
        const json_t *models = ta9_getattr(p, "models");
        count_num = (models && models->type == JSON_ARRAY) ? (double)json_len(models) : 0.0;
        count_s = ta9_fmt("%.0f", count_num);
    }
    const json_t *nm = ta9_getattr(p, "name");
    char *name = ta9_py_str(nm);
    char *label = ta9_fmt("%s (%s)", name ? name : "", count_s ? count_s : "");
    free(name); free(count_s);
    if (ta9_truthy(ta9_getattr(p, "is_current"))) {
        char *l2 = ta9_fmt("✓ %s", label); free(label); label = l2;
    }
    const json_t *slug = ta9_getattr(p, "slug");
    char *ss = ta9_py_str(slug);
    char *cb = ta9_fmt("mp:%s", ss ? ss : "");
    free(ss);
    json_t *r = ta9_ikb(ctx, label ? label : "", cb ? cb : "");
    free(label); free(cb); return r;
}

/* PoP: ta9_picker_back_cancel_row @ plugins/platforms/telegram/adapter.py:_picker_back_cancel_row */
static json_t *ta9_picker_back_cancel_row(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *row = json_array();
    json_t *b = ta9_t(ctx, "platform.telegram.picker.back", NULL);
    if (ta9_is_raise(b)) { json_free(row); return b; }
    char *bs = ta9_py_str(b); json_free(b);
    json_t *x1 = ta9_ikb(ctx, bs ? bs : "", "mb"); free(bs);
    if (ta9_is_raise(x1)) { json_free(row); return x1; }
    json_append(row, x1);
    json_t *c = ta9_t(ctx, "platform.telegram.picker.cancel", NULL);
    if (ta9_is_raise(c)) { json_free(row); return c; }
    char *cs = ta9_py_str(c); json_free(c);
    json_t *x2 = ta9_ikb(ctx, cs ? cs : "", "mx"); free(cs);
    if (ta9_is_raise(x2)) { json_free(row); return x2; }
    json_append(row, x2);
    return row;
}

/* PoP: ta9_picker_nav_row @ plugins/platforms/telegram/adapter.py:_picker_nav_row */
static json_t *ta9_picker_nav_row(ta9_ctx_t *ctx, const json_t *args) {
    double page = ta9_num(ta9_arg(args, "page"));
    double total = ta9_num(ta9_arg(args, "total_pages"));
    const char *prefix = ta9_str(ta9_arg(args, "prefix"));
    if (!prefix) prefix = "";
    json_t *nav = json_array();
    if (page > 0) {
        json_t *pv = ta9_t(ctx, "platform.telegram.picker.prev", NULL);
        if (ta9_is_raise(pv)) { json_free(nav); return pv; }
        char *ps = ta9_py_str(pv); json_free(pv);
        char *cb = ta9_fmt("%s:%.0f", prefix, page - 1);
        json_t *x = ta9_ikb(ctx, ps ? ps : "", cb ? cb : "");
        free(ps); free(cb);
        if (ta9_is_raise(x)) { json_free(nav); return x; }
        json_append(nav, x);
    }
    char *mid = ta9_fmt("%.0f/%.0f", page + 1, total);
    json_t *xm = ta9_ikb(ctx, mid ? mid : "", "mx:noop"); free(mid);
    if (ta9_is_raise(xm)) { json_free(nav); return xm; }
    json_append(nav, xm);
    if (page < total - 1) {
        json_t *nx = ta9_t(ctx, "platform.telegram.picker.next", NULL);
        if (ta9_is_raise(nx)) { json_free(nav); return nx; }
        char *ns = ta9_py_str(nx); json_free(nx);
        char *cb = ta9_fmt("%s:%.0f", prefix, page + 1);
        json_t *x = ta9_ikb(ctx, ns ? ns : "", cb ? cb : "");
        free(ns); free(cb);
        if (ta9_is_raise(x)) { json_free(nav); return x; }
        json_append(nav, x);
    }
    return nav;
}

/* PoP: ta9_teardown_started @ plugins/platforms/telegram/adapter.py:_teardown_started */
static json_t *ta9_teardown_started(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *v = ta9_state_get(ctx, "_polling_teardown_started");
    return v ? json_copy((json_t *)v) : json_bool(false);
}

/* PoP: ta9_send_path_degraded @ plugins/platforms/telegram/adapter.py:send_path_degraded */
static json_t *ta9_send_path_degraded(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *v = ta9_state_get(ctx, "_send_path_degraded");
    return json_bool(ta9_truthy(v));
}

/* PoP: ta9_callback_ctx @ plugins/platforms/telegram/adapter.py:_callback_ctx */
static json_t *ta9_callback_ctx(ta9_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *q = ta9_arg(args, "query");
    const json_t *qm = ta9_getattr(q, "message");
    const json_t *qc = ta9_getattr(qm, "chat");
    const json_t *fu = ta9_getattr(q, "from_user");
    json_t *o = json_object();
    const json_t *cid = ta9_getattr(qm, "chat_id");
    json_set(o, "chat_id", cid ? json_copy((json_t *)cid) : json_null());
    const json_t *ct = ta9_getattr(qc, "type");
    json_set(o, "chat_type", ct ? json_copy((json_t *)ct) : json_null());
    const json_t *tid = ta9_getattr(qm, "message_thread_id");
    json_set(o, "thread_id", tid ? json_copy((json_t *)tid) : json_null());
    const json_t *un = ta9_getattr(fu, "first_name");
    json_set(o, "user_name", un ? json_copy((json_t *)un) : json_null());
    return o;
}

/* PoP: ta9_supports_draft_streaming @ plugins/platforms/telegram/adapter.py:supports_draft_streaming */
static json_t *ta9_supports_draft_streaming(ta9_ctx_t *ctx, const json_t *args) {
    const json_t *bot = ta9_state_get(ctx, "_bot");
    if (!bot || bot->type == JSON_NULL || !ta9_truthy(bot)) return json_bool(false);
    const json_t *sd = ta9_getattr(bot, "send_message_draft");
    if (!sd) return json_bool(false);
    const json_t *ct = ta9_arg(args, "chat_type");
    if (!ta9_truthy(ct)) return json_bool(false);
    char *s = ta9_lower(ta9_str(ct) ? ta9_str(ct) : ta9_py_str(ct));
    bool yes = s && (strcmp(s, "dm") == 0 || strcmp(s, "private") == 0);
    free(s);
    return json_bool(yes);
}

/* PoP: ta9_fallback_ips @ plugins/platforms/telegram/adapter.py:_fallback_ips */
static json_t *ta9_fallback_ips(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *cfg = ta9_state_get(ctx, "config");
    const json_t *extra = ta9_getattr(cfg, "extra");
    json_t *configured = NULL;  /* owned array or scalar copy */
    if (ta9_truthy(extra)) {
        const json_t *v = ta9_getattr(extra, "fallback_ips");
        configured = v ? json_copy((json_t *)v) : json_array();
    } else {
        configured = json_array();
    }
    /* isinstance(configured, str) -> split(",") */
    if (configured->type == JSON_STRING) {
        const char *s = configured->str_val ? configured->str_val : "";
        json_t *arr = json_array();
        const char *p = s;
        while (1) {
            const char *comma = strchr(p, ',');
            size_t n = comma ? (size_t)(comma - p) : strlen(p);
            char *part = malloc(n + 1);
            if (part) { memcpy(part, p, n); part[n] = '\0'; }
            json_append(arr, json_string(part ? part : ""));
            free(part);
            if (!comma) break;
            p = comma + 1;
        }
        json_free(configured); configured = arr;
    }
    bool nonempty = ta9_truthy(configured);
    char *joined = NULL;
    if (nonempty && configured->type == JSON_ARRAY) {
        size_t n = json_len(configured);
        size_t cap = 64; for (size_t i = 0; i < n; i++) { const char *x = ta9_str(json_get(configured, i)); cap += x ? strlen(x) + 1 : 20; }
        joined = malloc(cap);
        size_t off = 0;
        if (joined) joined[0] = '\0';
        for (size_t i = 0; i < n && joined; i++) {
            char *x = ta9_py_str(json_get(configured, i));
            if (i) { joined[off] = ','; off++; }
            size_t l = x ? strlen(x) : 0;
            if (off + l + 1 > cap) break;
            memcpy(joined + off, x ? x : "", l); off += l; joined[off] = '\0';
            free(x);
        }
    }
    json_t *a = json_object();
    json_set(a, "value", (joined && nonempty) ? json_string(joined) : json_null());
    free(joined); json_free(configured);
    return ta9_call(ctx, "parse_fallback_ip_env", a);
}

/* PoP: ta9_link_preview_kwargs @ plugins/platforms/telegram/adapter.py:_link_preview_kwargs */
static json_t *ta9_link_preview_kwargs(ta9_ctx_t *ctx, const json_t *args) {
    (void)args;
    if (!ta9_truthy(ta9_state_get(ctx, "_disable_link_previews"))) return json_object();
    if (ta9_truthy(ta9_state_get(ctx, "LinkPreviewOptions_present"))) {
        json_t *a = json_object();
        json_set(a, "is_disabled", json_bool(true));
        json_t *r = ta9_call(ctx, "LinkPreviewOptions", a);
        if (ta9_is_raise(r)) return r;
        json_t *o = json_object();
        json_set(o, "link_preview_options", r);
        return o;
    }
    json_t *o = json_object();
    json_set(o, "disable_web_page_preview", json_bool(true));
    return o;
}

/* PoP: ta9_build_adapter @ plugins/platforms/telegram/adapter.py:_build_adapter */
static json_t *ta9_build_adapter(ta9_ctx_t *ctx, const json_t *args) {
    const json_t *config = ta9_arg(args, "config");
    json_t *a0 = json_object();
    json_set(a0, "config", config ? json_copy((json_t *)config) : json_null());
    json_t *adapter = ta9_call(ctx, "TelegramAdapter", a0);
    if (ta9_is_raise(adapter)) return adapter;
    json_t *r = ta9_call(ctx, "resolve_notifications_mode", json_object());
    if (ta9_is_raise(r)) {
        if (!ta9_is_exception(ta9_raise_kind(r))) { json_free(adapter); return r; }
        json_free(r);
        json_set_hdr(adapter, "_notifications_mode", "important");
        return adapter;
    }
    json_set(adapter, "_notifications_mode", r);
    return adapter;
}
