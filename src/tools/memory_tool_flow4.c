/*
 * memory_tool_flow4.c — tools/memory_tool.py final UNCLAIMED-remainder FLOW core.
 *
 * A CRLF-aware PoP scan over src/ leaves exactly nine tools/memory_tool.py defs
 * unclaimed (the rest are credited to memory_tool.c, memory_tool_ports.c and
 * memory_tool_pure.c):
 *
 *   _pin_matched_entries, _gate_or_stage, _batch_op_line,
 *   _validate_single_op, destructive_ops, _background_delete_gate,
 *   _applied, _invalid, _memory_tool
 *
 * This port closes those gaps. Each function is a direct port; the FLOW trace
 * is the ordered, scripted-seam event list (universal seam `mt4_seam`) that
 * the differential oracle drives identically on the live Python side.
 *
 * Full builtin exception hierarchy: MT4_EXC_PARENTS (py3.14 builtins + the
 * local MT4TestExc -> RuntimeError edge). Scripted {"__raise__": Kind} seam
 * results are resolved through mt4_exc_is(): Python's own `except Exception`
 * / bare re-raise on the live side mirrors the parent-table walk here.
 * EnvironmentError/IOError/WindowsError canonicalise to OSError (real Python
 * aliases). Unknown kinds default to an Exception subclass.
 *
 * Projection: json.dumps(...) / tool_error(...) return values are projected
 * to their parsed JSON objects (the oracle applies the same parse on the live
 * side). Payload-carrying functions echo the (possibly pinned) payload.
 *
 * Self-contained: libjson only. All helpers static mt4_*. No live I/O.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "libjson/json.h"

/* ── exception-parent table (FULL builtin hierarchy, py3.14) ── */
typedef struct { const char *kind; const char *parent; } mt4_exc_ent;
static const mt4_exc_ent MT4_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"}, {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"}, {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"}, {"TypeError", "Exception"},
    {"ValueError", "Exception"}, {"AttributeError", "Exception"},
    {"NameError", "Exception"}, {"LookupError", "Exception"},
    {"ArithmeticError", "Exception"}, {"OSError", "Exception"},
    {"EnvironmentError", "OSError"}, {"IOError", "OSError"},
    {"WindowsError", "OSError"}, {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"}, {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
    {"ImportError", "Exception"}, {"Warning", "Exception"},
    {"RuntimeError", "Exception"}, {"MemoryError", "Exception"},
    {"BufferError", "Exception"}, {"EOFError", "Exception"},
    {"ReferenceError", "Exception"}, {"_IncompleteInputError", "SyntaxError"},
    {"PythonFinalizationError", "RuntimeError"}, {"MT4TestExc", "RuntimeError"},
    {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
    {"UnboundLocalError", "NameError"}, {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"}, {"IndexError", "LookupError"},
    {"KeyError", "LookupError"}, {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"}, {"FloatingPointError", "ArithmeticError"},
    {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
    {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
    {"InterruptedError", "OSError"}, {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"}, {"ProcessLookupError", "OSError"},
    {"FileExistsError", "OSError"}, {"ChildProcessError", "OSError"},
    {"BlockingIOError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"ModuleNotFoundError", "ImportError"}, {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"}, {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    {NULL, NULL},
};

static const char *mt4_exc_canon(const char *k) {
    if (!k) return "";
    if (strcmp(k, "EnvironmentError") == 0 || strcmp(k, "IOError") == 0 ||
        strcmp(k, "WindowsError") == 0) return "OSError";
    return k;
}

static const char *mt4_exc_parent(const char *kind) {
    const char *k = mt4_exc_canon(kind);
    for (const mt4_exc_ent *e = MT4_EXC_PARENTS; e->kind; e++)
        if (strcmp(e->kind, k) == 0) return e->parent;
    if (strcmp(k, "BaseException") == 0) return NULL;
    return "Exception";
}

/* same-as-or-descendant-of (full walk). */
static bool mt4_exc_is(const char *kind, const char *base) {
    const char *k = kind;
    for (int depth = 0; k && depth < 32; depth++) {
        if (strcmp(mt4_exc_canon(k), mt4_exc_canon(base)) == 0) return true;
        k = mt4_exc_parent(k);
    }
    return false;
}

/* ── generic helpers ── */
static char *mt4_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) return NULL;
    va_start(ap, fmt);
    vsnprintf(b, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return b;
}

static char *mt4_prefix(const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = strlen(s);
    if (n < len) len = n;
    char *b = (char *)malloc(len + 1);
    if (!b) return NULL;
    memcpy(b, s, len);
    b[len] = '\0';
    return b;
}

static bool mt4_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return v->c.count > 0;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        default:          return false;
    }
}

static const char *mt4_gets(const json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}

static json_t *mt4_obj_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
    return o;
}

/* ── universal scripted seam + FLOW trace ── */
typedef struct {
    const json_t *script;
    json_t *idx;
    json_t *trace;
} mt4_ctx;

static bool mt4_is_null(const json_t *v);
static json_t *mt4_destructive_ops_payload(const json_t *payload);
static json_t *mt4_bg_deny(mt4_ctx *c);

/* Set by the seam when the scripted result is {"__raise__": Kind}. */
static bool MT4_RAISED;
static char MT4_RAISE_KIND[96];

static void mt4_clear_raise(void) { MT4_RAISED = false; MT4_RAISE_KIND[0] = '\0'; }

/* Mirror the oracle's seam(): drop None-valued args before recording. */
static json_t *mt4_args_filtered(const json_t *args) {
    json_t *o = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type != JSON_NULL)
                json_set(o, args->c.keys[i], json_copy(v));
        }
    }
    return o;
}

static json_t *mt4_seam(mt4_ctx *c, const char *name, const json_t *args) {
    MT4_RAISED = false;
    MT4_RAISE_KIND[0] = '\0';
    const json_t *sc = json_obj_get(c->script, name);
    json_t *r = NULL;
    if (sc && sc->type == JSON_OBJECT && json_obj_get(sc, "__seq__")) {
        const json_t *seq = json_obj_get(sc, "__seq__");
        size_t i = (size_t)json_get_num(c->idx, name, 0.0);
        json_set(c->idx, name, json_number((double)(i + 1)));
        if (seq && seq->type == JSON_ARRAY && json_len(seq) > 0) {
            size_t j = i < json_len(seq) ? i : json_len(seq) - 1;
            r = json_copy(json_get(seq, j));
        } else {
            r = json_null();
        }
    } else if (sc) {
        r = json_copy(sc);
    } else {
        r = json_null();
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", mt4_args_filtered(args));
    json_set(e, "ret", json_copy(r));
    json_append(c->trace, e);
    if (r && r->type == JSON_OBJECT) {
        const json_t *k = json_obj_get(r, "__raise__");
        if (k && k->type == JSON_STRING) {
            MT4_RAISED = true;
            snprintf(MT4_RAISE_KIND, sizeof(MT4_RAISE_KIND), "%s", k->str_val);
        }
    }
    return r;
}

static json_t *mt4_unknown_uncaught(void) {
    json_t *o = json_object();
    json_set(o, "__uncaught__", json_string(MT4_RAISE_KIND));
    return o;
}

static bool mt4_caught_by(const char *base) {
    return MT4_RAISED && mt4_exc_is(MT4_RAISE_KIND, base);
}

/* tool_error(): {"error": <msg>, "success": false} (pure formatter, no seam). */
static json_t *mt4_tool_error(const char *msg, bool success) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg ? msg : ""));
    json_set(o, "success", json_bool(success));
    return o;
}

static json_t *mt4_fc(mt4_ctx *c, const char *value) {
    json_t *args = json_object();
    mt4_obj_str(args, "value", value);
    json_t *r = mt4_seam(c, "failure_class.set", args);
    json_free(args);
    json_free(r);
    return NULL;
}

/* ── PoP: mt4_batch_op_line @ tools/memory_tool.py:_batch_op_line ── */
static json_t *mt4_batch_op_line(const json_t *a) {
    const json_t *op = json_obj_get(a, "op");
    if (!op || op->type != JSON_OBJECT) op = NULL;
    const char *act = op ? mt4_gets(op, "action", "?") : "?";
    const char *content = op ? json_get_str(op, "content", NULL) : NULL;
    if (!content || !content[0]) content = op ? mt4_gets(op, "new_text", "") : "";
    const char *old = op ? mt4_gets(op, "old_text", "") : "";
    char *s;
    if (strcmp(act, "remove") == 0)
        s = mt4_fmt("- remove: %s", old);
    else if (strcmp(act, "patch") == 0)
        s = mt4_fmt("- patch span '%s' -> '%s' (rest of the entry is kept)", old, content);
    else
        s = mt4_fmt("- %s: %s", act, content);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* ── PoP: mt4_destructive_ops @ tools/memory_tool.py:destructive_ops ── */
static json_t *mt4_destructive_ops(const json_t *a) {
    const json_t *payload = json_obj_get(a, "payload");
    const json_t *ops = NULL;
    size_t single = 0;
    bool batch = payload && payload->type == JSON_OBJECT &&
                 strcmp(mt4_gets(payload, "action", ""), "batch") == 0;
    if (batch) {
        const json_t *o = json_obj_get(payload, "operations");
        if (o && o->type == JSON_ARRAY) ops = o;
    } else {
        single = 1;
    }
    json_t *out = json_array();
    size_t n = ops ? json_len(ops) : single;
    for (size_t i = 0; i < n; i++) {
        const json_t *op = ops ? json_get(ops, i) : payload;
        if (!op || op->type != JSON_OBJECT) continue;
        const char *act = mt4_gets(op, "action", "");
        if (strcmp(act, "replace") == 0 || strcmp(act, "remove") == 0 ||
            strcmp(act, "patch") == 0)
            json_append(out, json_copy(op));
    }
    return out;
}

/* ── PoP: mt4_applied @ tools/memory_tool.py:_applied ── */
static json_t *mt4_applied(const json_t *a) {
    const json_t *result = json_obj_get(a, "result");
    bool ok = result && mt4_truthy(json_obj_get(result, "success"));
    json_t *o = json_object();
    json_set(o, "outcome", json_string(ok ? "success" : "failed"));
    json_set(o, "result", result ? json_copy(result) : json_object());
    return o;
}

/* ── PoP: mt4_invalid @ tools/memory_tool.py:_invalid ── */
static json_t *mt4_invalid(mt4_ctx *c, const char *message) {
    mt4_fc(c, "invalid_args");
    json_t *o = json_object();
    json_set(o, "outcome", json_string("rejected"));
    json_set(o, "result", mt4_tool_error(message, false));
    return o;
}

/* ── PoP: mt4_validate_single_op @ tools/memory_tool.py:_validate_single_op ── */
static json_t *mt4_validate_single_op(mt4_ctx *c, const json_t *a) {
    const char *action = mt4_gets(a, "action", "");
    const char *target = mt4_gets(a, "target", "memory");
    const json_t *cv = json_obj_get(a, "content");
    const json_t *ov = json_obj_get(a, "old_text");
    bool content = mt4_truthy(cv);
    bool old = mt4_truthy(ov);
    const char *content_s = (cv && cv->type == JSON_STRING) ? cv->str_val : "";
    const char *old_s = (ov && ov->type == JSON_STRING) ? ov->str_val : "";
    if (strcmp(action, "add") == 0 && !content) {
        mt4_fc(c, "missing_content");
        return mt4_tool_error("Content is required for 'add' action.", false);
    }
    if ((strcmp(action, "remove") == 0 || strcmp(action, "patch") == 0) && !old) {
        mt4_fc(c, "missing_old_text");
        char *msg = mt4_fmt(
            "'%s' needs old_text -- a short unique substring of the entry to %s. "
            "None was provided. Reissue the %s with old_text set to part of one of "
            "the current_entries below.", action, action, action);
        json_t *args = json_object();
        mt4_obj_str(args, "target", target);
        json_t *entries = mt4_seam(c, "store._entries_for", args);
        json_t *usage = mt4_seam(c, "store._usage", args);
        json_free(args);
        json_t *o = json_object();
        json_set(o, "success", json_bool(false));
        json_set(o, "error", json_string(msg ? msg : ""));
        json_set(o, "current_entries", entries ? entries : json_null());
        json_set(o, "usage", usage ? usage : json_null());
        free(msg);
        return o;
    }
    if (strcmp(action, "patch") == 0 && !content) {
        mt4_fc(c, "missing_content");
        char *msg = mt4_fmt(
            "content (the new text for the old_text span) is required for 'patch' action. "
            "%s", "To drop text inside an entry, patch a longer span that contains it; "
                  "'remove' deletes the whole entry.");
        json_t *r = mt4_tool_error(msg ? msg : "", false);
        free(msg);
        return r;
    }
    (void)content_s;
    (void)old_s;
    return json_null();
}

/* pin_matched_entries — mutates `payload` in place (the harness works on a copy). */
static json_t *mt4_pin(mt4_ctx *c, json_t *payload) {
    const char *target = mt4_gets(payload, "target", "memory");
    const char *action = mt4_gets(payload, "action", "");
    json_t *r = NULL;
    if (strcmp(action, "batch") == 0) {
        json_t *args = json_object();
        mt4_obj_str(args, "target", target);
        const json_t *ops = json_obj_get(payload, "operations");
        json_set(args, "operations", ops ? json_copy(ops) : json_array());
        r = mt4_seam(c, "resolve_batch_entries", args);
        json_free(args);
        if (MT4_RAISED) return mt4_unknown_uncaught();
        if (r && mt4_truthy(json_obj_get(r, "success"))) {
            const json_t *ops2 = json_obj_get(payload, "operations");
            const json_t *ents = json_obj_get(r, "matched_entries");
            size_t nops = (ops2 && ops2->type == JSON_ARRAY) ? json_len(ops2) : 0;
            size_t nent = (ents && ents->type == JSON_ARRAY) ? json_len(ents) : 0;
            size_t n = nops < nent ? nops : nent;
            json_t *newops = json_array();
            for (size_t i = 0; i < n; i++) {
                const json_t *op = json_get(ops2, i);
                const json_t *ent = json_get(ents, i);
                if (ent && ent->type != JSON_NULL) {
                    json_t *d = (op && op->type == JSON_OBJECT) ? json_copy(op) : json_object();
                    json_set(d, "matched_entry", json_copy(ent));
                    json_append(newops, d);
                } else {
                    json_append(newops, op ? json_copy(op) : json_object());
                }
            }
            json_set(payload, "operations", newops);
            json_free(r);
            return json_null();
        }
    } else if (strcmp(action, "replace") == 0 || strcmp(action, "remove") == 0 ||
               strcmp(action, "patch") == 0) {
        json_t *args = json_object();
        mt4_obj_str(args, "target", target);
        const char *old = mt4_gets(payload, "old_text", "");
        mt4_obj_str(args, "old_text", old);
        mt4_obj_str(args, "action", action);
        r = mt4_seam(c, "resolve_entry", args);
        json_free(args);
        if (MT4_RAISED) return mt4_unknown_uncaught();
        if (r && mt4_truthy(json_obj_get(r, "success"))) {
            const json_t *me = json_obj_get(r, "matched_entry");
            json_set(payload, "matched_entry", me ? json_copy(me) : json_null());
            json_free(r);
            return json_null();
        }
    } else {
        return json_null();
    }
    json_t *out = json_copy(r ? r : json_null());
    json_free(r);
    return out;
}

/* PoP: mt4_pin_matched_entries @ tools/memory_tool.py:_pin_matched_entries */
static json_t *mt4_pin_matched_entries(mt4_ctx *c, const json_t *a, json_t **payload_out) {
    const json_t *src = json_obj_get(a, "payload");
    json_t *payload = (src && src->type == JSON_OBJECT) ? json_copy(src) : json_object();
    if (payload_out) *payload_out = payload;
    json_t *r = mt4_pin(c, payload);
    if (MT4_RAISED) mt4_clear_raise();
    return r;
}

static void mt4_gate_text(const char *action, const char *label,
                          const char *content, const char *old,
                          char **summary, char **detail) {
    if (strcmp(action, "add") == 0) {
        *summary = mt4_fmt("add to %s", label);
        *detail = mt4_fmt("%s", content ? content : "");
    } else if (strcmp(action, "replace") == 0) {
        *summary = mt4_fmt("replace in %s", label);
        *detail = mt4_fmt("entry matching: %s\nwhole entry becomes: %s", old, content);
    } else if (strcmp(action, "remove") == 0) {
        *summary = mt4_fmt("remove from %s", label);
        *detail = mt4_fmt("%s", (old && old[0]) ? old : "");
    } else if (strcmp(action, "patch") == 0) {
        *summary = mt4_fmt("patch in %s", label);
        *detail = mt4_fmt("entry matching: %s\nspan '%s' becomes: %s (rest of the entry is kept)",
                          old, old, content);
    } else {
        *summary = mt4_fmt("");
        *detail = mt4_fmt("");
    }
}

/* PoP: mt4_gate_or_stage @ tools/memory_tool.py:_gate_or_stage */
static json_t *mt4_gate_or_stage(mt4_ctx *c, const char *summary, const char *detail,
                                json_t *payload) {
    json_t *empty = json_object();
    json_t *imp = mt4_seam(c, "import write_approval", empty);
    json_free(empty);
    json_free(imp);
    if (MT4_RAISED) {
        json_t *u = mt4_unknown_uncaught();
        bool caught = mt4_caught_by("Exception");
        mt4_clear_raise();
        if (caught) {
            json_free(u);
            return json_null();
        }
        return u;
    }
    /* evaluate_gate uses the summary/detail passed by the caller verbatim. */
    json_t *ga = json_object();
    mt4_obj_str(ga, "kind", "memory");
    mt4_obj_str(ga, "inline_summary", summary);
    mt4_obj_str(ga, "inline_detail", detail);
    json_t *dec = mt4_seam(c, "evaluate_gate", ga);
    json_free(ga);
    if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(dec); return u; }
    bool allow = dec && mt4_truthy(json_obj_get(dec, "allow"));
    bool blocked = dec && mt4_truthy(json_obj_get(dec, "blocked"));
    const char *msg = mt4_gets(dec, "message", "");
    if (allow) { json_free(dec); return json_null(); }
    if (blocked) {
        mt4_fc(c, "gate_refused");
        json_t *r = mt4_tool_error(msg, false);
        json_free(dec);
        return r;
    }
    json_t *unmatched = mt4_pin(c, payload);
    if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(unmatched); json_free(dec); return u; }
    if (!mt4_is_null(unmatched)) { json_free(dec); return unmatched; }
    json_free(unmatched);
    mt4_fc(c, "staged");
    json_t *ca = json_object();
    mt4_obj_str(ca, "kind", "memory");
    json_set(ca, "payload", json_copy(payload));
    char *pfx = mt4_prefix(detail ? detail : "", 120);
    char *sum = mt4_fmt("%s: %s", summary ? summary : "", pfx ? pfx : "");
    mt4_obj_str(ca, "summary", sum);
    json_t *origin = mt4_seam(c, "current_origin", json_object());
    json_set(ca, "origin", origin ? origin : json_null());
    char *ojson = NULL;
    (void)ojson;
    json_t *st = mt4_seam(c, "stage_write", ca);
    json_free(ca);
    free(pfx);
    free(sum);
    if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(st); json_free(dec); return u; }
    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "staged", json_bool(true));
    const json_t *pid = st ? json_obj_get(st, "id") : NULL;
    json_set(out, "pending_id", pid ? json_copy(pid) : json_null());
    mt4_obj_str(out, "message", msg);
    json_free(st);
    json_free(dec);
    return out;
}

/* PoP: mt4_background_delete_gate @ tools/memory_tool.py:_background_delete_gate */
static json_t *mt4_background_delete_gate(mt4_ctx *c, const char *action, const char *target,
                                          const char *content, const char *old,
                                          const json_t *ops) {
    json_t *empty = json_object();
    json_t *imp = mt4_seam(c, "import skill_provenance", empty);
    json_free(empty);
    json_free(imp);
    if (MT4_RAISED) {
        json_t *u = mt4_unknown_uncaught();
        mt4_clear_raise();
        return u;
    }
    json_t *un = mt4_seam(c, "is_unattended_review", json_object());
    if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(un); return u; }
    bool attended = mt4_truthy(un);
    json_free(un);
    if (!attended) return json_null();

    json_t *payload = json_object();
    mt4_obj_str(payload, "action", ops ? "batch" : action);
    mt4_obj_str(payload, "target", target);
    if (ops) {
        json_set(payload, "operations", json_copy(ops));
    } else {
        json_set(payload, "content", content ? json_string(content) : json_null());
        json_set(payload, "old_text", old ? json_string(old) : json_null());
    }
    json_t *dest = mt4_destructive_ops_payload(payload);
    bool any_dest = dest && dest->type == JSON_ARRAY && json_len(dest) > 0;
    json_free(dest);
    if (!any_dest) { json_free(payload); return json_null(); }

    char *detail;
    const char *kind;
    if (ops) {
        json_t *parts = json_array();
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *wa = json_object();
            json_set(wa, "op", json_copy(json_get(ops, i)));
            json_t *line = mt4_batch_op_line(wa);
            json_free(wa);
            json_append(parts, line);
        }
        /* join with "; " */
        size_t total = 1;
        for (size_t i = 0; i < json_len(parts); i++) {
            const json_t *v = json_get(parts, i);
            total += (v && v->type == JSON_STRING) ? strlen(v->str_val) : 0;
            if (i + 1 < json_len(parts)) total += 2;
        }
        detail = (char *)malloc(total + 1);
        size_t pos = 0;
        if (detail) {
            detail[0] = '\0';
            for (size_t i = 0; i < json_len(parts); i++) {
                const json_t *v = json_get(parts, i);
                const char *sv = (v && v->type == JSON_STRING) ? v->str_val : "";
                size_t sl = strlen(sv);
                memcpy(detail + pos, sv, sl);
                pos += sl;
                if (i + 1 < json_len(parts)) { detail[pos++] = ';'; detail[pos++] = ' '; }
            }
            detail[pos] = '\0';
        }
        kind = "batch";
        json_free(parts);
    } else {
        json_t *wa = json_object();
        json_t *op = json_object();
        mt4_obj_str(op, "action", action);
        if (content) mt4_obj_str(op, "content", content);
        if (old) mt4_obj_str(op, "old_text", old);
        json_set(wa, "op", op);
        json_t *line = mt4_batch_op_line(wa);
        json_free(wa);
        const char *sv = (line && line->type == JSON_STRING) ? line->str_val : "";
        detail = mt4_fmt("%s", sv);
        json_free(line);
        kind = action;
    }

    /* try: */
    json_t *unmatched = mt4_pin(c, payload);
    if (MT4_RAISED) {
        bool caught = mt4_caught_by("Exception");
        json_t *u = mt4_unknown_uncaught();   /* capture BEFORE clearing */
        mt4_clear_raise();
        json_free(unmatched);
        json_t *res = caught ? mt4_bg_deny(c) : u;
        if (caught) json_free(u);
        free(detail);
        json_free(payload);
        return res;
    }
    if (!mt4_is_null(unmatched)) { free(detail); json_free(payload); return unmatched; }
    json_free(unmatched);
    json_t *imp2 = mt4_seam(c, "import write_approval", json_object());
    json_free(imp2);
    if (MT4_RAISED) {
        bool caught = mt4_caught_by("Exception");
        json_t *u = mt4_unknown_uncaught();
        mt4_clear_raise();
        json_t *res = caught ? mt4_bg_deny(c) : u;
        if (caught) json_free(u);
        free(detail); json_free(payload);
        return res;
    }
    mt4_fc(c, "staged");
    char *base = mt4_fmt("background review consolidation (%s on %s): %s", kind, target, detail ? detail : "");
    char *sum = mt4_prefix(base ? base : "", 200);
    json_t *sa = json_object();
    mt4_obj_str(sa, "kind", "memory");
    json_set(sa, "payload", json_copy(payload));
    mt4_obj_str(sa, "summary", sum ? sum : "");
    json_t *origin = mt4_seam(c, "current_origin", json_object());
    json_set(sa, "origin", origin ? origin : json_null());
    json_t *st = mt4_seam(c, "stage_write", sa);
    json_free(sa);
    free(base); free(sum); free(detail);
    if (MT4_RAISED) {
        bool caught = mt4_caught_by("Exception");
        json_t *u = mt4_unknown_uncaught();
        mt4_clear_raise();
        json_free(st); json_free(payload);
        json_t *res = caught ? mt4_bg_deny(c) : u;
        if (caught) json_free(u);
        return res;
    }
    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "staged", json_bool(true));
    json_set(out, "proposal_staged", json_bool(true));
    const json_t *pid = st ? json_obj_get(st, "id") : NULL;
    json_set(out, "pending_id", pid ? json_copy(pid) : json_null());
    char *msg = mt4_fmt(
        "Background review may not delete memory entries unattended. The proposed "
        "%s was staged for your approval — review it with /memory pending "
        "(approve to apply, discard to drop).", kind);
    mt4_obj_str(out, "message", msg);
    free(msg);
    json_free(st);
    json_free(payload);
    return out;
}

/* helper declarations used above */
static bool mt4_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool mt4_is_uncaught_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__uncaught__") != NULL;
}
static json_t *mt4_destructive_ops_payload(const json_t *payload) {
    json_t *a = json_object();
    json_set(a, "payload", json_copy(payload));
    json_t *r = mt4_destructive_ops(a);
    json_free(a);
    return r;
}
static json_t *mt4_bg_deny(mt4_ctx *c) {
    json_t *lg = json_object();
    mt4_obj_str(lg, "fmt", "Failed to stage background-review consolidation; denying");
    json_set(lg, "args", json_array());
    json_set(lg, "exc_info", json_bool(true));
    json_t *x = mt4_seam(c, "logger.warning", lg);
    json_free(lg); json_free(x);
    mt4_fc(c, "gate_refused");
    return mt4_tool_error(
        "Background review may not delete memory entries ('patch'/'remove', including in a "
        "batch); 'add' is still available.", false);
}

/* PoP: mt4_memory_tool @ tools/memory_tool.py:_memory_tool */
static json_t *mt4_memory_tool(mt4_ctx *c, json_t *a) {
    const json_t *av = json_obj_get(a, "action");
    const char *action = (av && av->type == JSON_STRING) ? av->str_val : NULL;
    const char *target = mt4_gets(a, "target", "memory");
    const json_t *cv = json_obj_get(a, "content");
    const json_t *ov = json_obj_get(a, "old_text");
    const char *content = (cv && cv->type == JSON_STRING) ? cv->str_val : NULL;
    const char *old = (ov && ov->type == JSON_STRING) ? ov->str_val : NULL;
    const json_t *ops = json_obj_get(a, "operations");

    if ((!content || !content[0])) {
        const json_t *nv = json_obj_get(a, "new_text");
        if (nv && nv->type == JSON_STRING && nv->str_val && nv->str_val[0]) {
            content = nv->str_val;
            json_set(a, "content", json_string(content));  // live: content = new_text
        }
    }
    json_t *tea = json_object();
    mt4_obj_str(tea, "target", target);
    if (ops) json_set(tea, "operations", json_copy(ops));
    json_t *terr = mt4_seam(c, "_memory_target_error", tea);
    json_free(tea);
    if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(terr); return u; }
    if (!mt4_is_null(terr)) {
        json_t *o = json_object();
        json_set(o, "outcome", json_string("rejected"));
        json_set(o, "result", json_copy(terr));
        json_free(terr);
        return o;
    }
    json_free(terr);

    if (ops && mt4_truthy(ops)) {
        if (ops->type != JSON_ARRAY)
            return mt4_invalid(c, "operations must be a list of {action, content?, old_text?} objects.");
        for (size_t i = 0; i < json_len(ops); i++) {
            const json_t *op = json_get(ops, i);
            if (op && op->type == JSON_OBJECT &&
                strcmp(mt4_gets(op, "action", ""), "replace") == 0)
                return mt4_invalid(c,
                    "'replace' is not available; use 'patch'. old_text is the exact span to change and "
                    "content is its new text; the rest of the entry is kept. To rewrite a whole entry, "
                    "set old_text to the whole entry. Nothing was written.");
        }
        json_t *denied = mt4_background_delete_gate(c, action ? action : "", target, NULL, NULL, ops);
        if (mt4_is_uncaught_obj(denied)) {   /* uncaught exception escapes _memory_tool */
            json_t *o = json_object();
            const json_t *k = json_obj_get(denied, "__uncaught__");
            json_set(o, "__uncaught__", k ? json_copy(k) : json_null());
            json_free(denied);
            return o;
        }
        if (!mt4_is_null(denied)) {
            json_t *o = json_object();
            json_set(o, "outcome", json_string("rejected"));
            json_set(o, "result", json_copy(denied));
            json_free(denied);
            return o;
        }
        json_free(denied);
        json_t *ga = json_object();
        mt4_obj_str(ga, "action", "batch");
        mt4_obj_str(ga, "target", target);
        json_set(ga, "operations", json_copy(ops));
        json_t *gate = mt4_seam(c, "_apply_write_gate", ga);
        json_free(ga);
        if (!mt4_is_null(gate)) {
            json_t *o = json_object();
            json_set(o, "outcome", json_string("rejected"));
            json_set(o, "result", json_copy(gate));
            json_free(gate);
            return o;
        }
        json_free(gate);
        json_t *ba = json_object();
        mt4_obj_str(ba, "target", target);
        json_set(ba, "operations", json_copy(ops));
        json_t *r = mt4_seam(c, "apply_batch", ba);
        json_free(ba);
        if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(r); return u; }
        json_t *ra = json_object();
        json_set(ra, "result", r ? r : json_object());
        json_t *out = mt4_applied(ra);
        json_free(ra);
        return out;
    }

    if ((!action || !action[0]) && !(ops && mt4_truthy(ops)))
        return mt4_invalid(c,
            "Missing required parameter: 'operations', a list of {action: add/patch/remove, "
            "content?, old_text?} changes; a single change is a one-item list.");
    if (action && strcmp(action, "replace") == 0)
        return mt4_invalid(c,
            "'replace' is not available; use 'patch'. old_text is the exact span to change and "
            "content is its new text; the rest of the entry is kept. To rewrite a whole entry, "
            "set old_text to the whole entry. Nothing was written.");
    if (!action || (strcmp(action, "add") != 0 && strcmp(action, "remove") != 0 &&
                    strcmp(action, "patch") != 0)) {
        char *m = mt4_fmt("Unknown action '%s'. Use: add, patch, remove", action ? action : "None");
        json_t *r = mt4_invalid(c, m);
        free(m);
        return r;
    }
    /* short-circuit OR: validate -> background gate -> apply gate */
    json_t *inv = mt4_validate_single_op(c, a);
    if (mt4_is_null(inv)) {
        inv = mt4_background_delete_gate(c, action, target, content, old, NULL);
        if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(inv); return u; }
        if (mt4_is_uncaught_obj(inv)) {   /* uncaught exception escapes _memory_tool */
            json_t *o = json_object();
            const json_t *k = json_obj_get(inv, "__uncaught__");
            json_set(o, "__uncaught__", k ? json_copy(k) : json_null());
            json_free(inv);
            return o;
        }
    }
    if (mt4_is_null(inv)) {
        json_t *ga = json_object();
        mt4_obj_str(ga, "action", action);
        mt4_obj_str(ga, "target", target);
        if (content) mt4_obj_str(ga, "content", content);
        if (old) mt4_obj_str(ga, "old_text", old);
        inv = mt4_seam(c, "_apply_write_gate", ga);
        json_free(ga);
        if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(inv); return u; }
    }
    if (!mt4_is_null(inv)) {
        json_t *o = json_object();
        json_set(o, "outcome", json_string("rejected"));
        json_set(o, "result", json_copy(inv));
        json_free(inv);
        return o;
    }
    json_free(inv);
    char *seam_name = mt4_fmt("store.%s", action);
    json_t *sa = json_object();
    mt4_obj_str(sa, "target", target);
    if (content) mt4_obj_str(sa, "content", content);
    if (old) mt4_obj_str(sa, "old_text", old);
    json_t *r = mt4_seam(c, seam_name, sa);
    json_free(sa);
    free(seam_name);
    if (MT4_RAISED) { json_t *u = mt4_unknown_uncaught(); mt4_clear_raise(); json_free(r); return u; }
    json_t *ra = json_object();
    json_set(ra, "result", r ? r : json_object());
    json_t *out = mt4_applied(ra);
    json_free(ra);
    return out;
}

/* Synthetic FLOW helper: ordered except-clause dispatch over the full hierarchy. */
static json_t *mt4_except_dispatch(const json_t *a, json_t *trace) {
    const json_t *raised = json_obj_get(a, "raised");
    const char *kind = (raised && raised->type == JSON_STRING) ? raised->str_val : "RuntimeError";
    const json_t *clauses = json_obj_get(a, "clauses");
    size_t n = (clauses && clauses->type == JSON_ARRAY) ? json_len(clauses) : 0;
    const char *caught = NULL;
    for (size_t i = 0; i < n; i++) {
        const json_t *node = json_get(clauses, i);
        const char *name = (node && node->type == JSON_STRING) ? node->str_val : "";
        bool matched = mt4_exc_is(kind, name);
        json_t *e = json_object();
        json_set(e, "ev", json_string("isinstance"));
        json_t *ea = json_object();
        mt4_obj_str(ea, "name", name);
        json_set(e, "args", ea);
        json_set(e, "ret", json_bool(matched));
        json_append(trace, e);
        if (matched && !caught) caught = name;
    }
    json_t *o = json_object();
    json_set(o, "caught_by", caught ? json_string(caught) : json_null());
    return o;
}

/* ── dispatch ── */
json_t *mt4_run(const char *func, const json_t *in) {
    mt4_clear_raise();
    json_t *trace = json_array();
    json_t *idx = json_object();
    const json_t *script = json_obj_get(in, "script");
    mt4_ctx c = { script, idx, trace };
    json_t *args = json_copy(json_obj_get(in, "args"));
    if (!args || args->type != JSON_OBJECT) {
        json_free(args);
        args = json_object();
    }
    json_t *value = NULL;
    json_t *payload = NULL;

    if (strcmp(func, "batch_op_line") == 0) {
        value = mt4_batch_op_line(args);
    } else if (strcmp(func, "destructive_ops") == 0) {
        value = mt4_destructive_ops(args);
    } else if (strcmp(func, "applied") == 0) {
        value = mt4_applied(args);
    } else if (strcmp(func, "invalid") == 0) {
        value = mt4_invalid(&c, mt4_gets(args, "message", ""));
    } else if (strcmp(func, "validate_single_op") == 0) {
        value = mt4_validate_single_op(&c, args);
    } else if (strcmp(func, "pin_matched_entries") == 0) {
        value = mt4_pin_matched_entries(&c, args, &payload);
    } else if (strcmp(func, "gate_or_stage") == 0) {
        const json_t *src = json_obj_get(args, "payload");
        payload = (src && src->type == JSON_OBJECT) ? json_copy(src) : json_object();
        value = mt4_gate_or_stage(&c, mt4_gets(args, "summary", ""),
                                  mt4_gets(args, "detail", ""), payload);
    } else if (strcmp(func, "background_delete_gate") == 0) {
        value = mt4_background_delete_gate(&c,
            mt4_gets(args, "action", "add"), mt4_gets(args, "target", "memory"),
            json_get_str(args, "content", NULL), json_get_str(args, "old_text", NULL),
            json_obj_get(args, "operations"));
        const json_t *at = json_obj_get(args, "target");
        (void)at;
        payload = json_object();
        mt4_obj_str(payload, "action", mt4_gets(args, "action", "add"));
        mt4_obj_str(payload, "target", mt4_gets(args, "target", "memory"));
        json_set(payload, "content", json_get_str(args, "content", NULL) ?
                 json_string(json_get_str(args, "content", NULL)) : json_null());
        json_set(payload, "old_text", json_get_str(args, "old_text", NULL) ?
                 json_string(json_get_str(args, "old_text", NULL)) : json_null());
        if (json_obj_get(args, "operations"))
            json_set(payload, "operations", json_copy(json_obj_get(args, "operations")));
    } else if (strcmp(func, "memory_tool") == 0) {
        value = mt4_memory_tool(&c, args);
    } else if (strcmp(func, "except_dispatch") == 0) {
        value = mt4_except_dispatch(args, trace);
    } else {
        value = json_object();
        mt4_obj_str(value, "error", "unknown func");
    }
    if (MT4_RAISED) { json_free(value); value = mt4_unknown_uncaught(); mt4_clear_raise(); }

    json_t *out = json_object();
    json_set(out, "value", value ? value : json_null());
    json_set(out, "payload", payload ? payload : json_null());
    json_set(out, "trace", trace);   /* ownership transferred */
    json_free(args);
    json_free(idx);
    return out;
}
