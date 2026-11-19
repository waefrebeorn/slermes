/*
 * lsp_tools_flow.c — the LSP tools surface FLOW port (universal-seam convention).
 *
 * The task names "tools/lsp_tools.py"; the quarry has NO such module. The real
 * LSP tools surface is the LSP-diagnostics tier of the file tools plus the
 * unclaimed collaborators it composes with. This port covers 20 UNCLAIMED
 * callable units across three defining modules (PoP credits the DEFINING
 * module, per the annotation audit):
 *
 *   tools/file_operations_lint.py (7):
 *     _lsp_service, _lsp_call, _local_workspace_untrusted,
 *     _run_managed_node_linter, _format_lsp_block, _post_deferred,
 *     _queue_late_verdict
 *   agent/lsp/deferred.py (11):
 *     bind_owner, current_owner, post, take, restore, pop_notes,
 *     track_pending, settled, _owners, settle_for_agent, forward_to_agent
 *   agent/lsp/sync_diagnostics.py (2):
 *     supported, _from_tsserver   (async `request` is out of flow scope)
 *
 * Claimed siblings (_check_lint, _check_lint_delta, _lsp_local_only,
 * _lsp_handles_extension, _has_ancestor_tsconfig, _lsp_will_handle,
 * _maybe_lsp_diagnostics, _looks_like_linter_unusable, the _lint_*_inproc
 * leaves) are SEAMED where reached (_lsp_local_only is the only one the
 * ported defs call). Unclaimed ported siblings compose for real:
 * _post_deferred -> _queue_late_verdict -> _format_lsp_block / deferred.post /
 * deferred.restore; settle_for_agent / forward_to_agent -> pop_notes;
 * restore -> post.
 *
 * Every effectful call (claimed sibling, foreign collaborator, service method,
 * future method, ContextVar op, Condition op, logger.debug) enters through
 * lt_seams_t.call(ctx, name, args); args is OWNED by lt_call (freed after the
 * borrowed call); results are NEW json_t with NULL == Python None and
 * {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError) pass through
 * every except-Exception site. The exception-parent table carries the FULL
 * builtin hierarchy (+ the subprocess family used by TimeoutExpired).
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static
 * lt_*. Ordered trace is emitted by the harness (tests/t_port_lsp_tools_flow.c).
 * Fail-closed: an unscripted seam raises RuntimeError on both sides.
 *
 * State model (deferred queue), mutated in place and echoed by the harness:
 *   {"queued": {owner: {path: {"text": str, "baseline_text": str|null}}},
 *    "pending": {owner: [token ids]}, "next_token": int}
 * _QUEUED is an OrderedDict (insertion order preserved; pop+reinsert moves an
 * owner to newest; popitem(last=False) evicts the FIRST key). _PENDING values
 * are sets of opaque tokens, modeled as integer ids from a state counter;
 * identity == id equality (ids are unique). Sets echo SORTED on both sides.
 * _OWNER (ContextVar) and _LOCK/_SETTLED (threading) are seam doubles: set/
 * reset/get and wait_for/notify_all fire seams; `with _LOCK`/`with _SETTLED`
 * are inert. bind_owner is driven as a context manager over a scripted body
 * ({"read": bool, "raise": spec}); the finally-reset always fires, and a reset
 * raise replaces a body raise (live CM semantics).
 *
 * Object doubles (documented fixture domains):
 *  - self: {"env": object|null, "cwd": str|null}; `self.env` and `self.cwd`
 *    are DIRECT reads (absent -> AttributeError 'types.SimpleNamespace').
 *    env.cwd via getattr default None; `getattr(self.env,"cwd",None) or
 *    self.cwd` short-circuits on a truthy env.cwd.
 *  - svc (get_service result): object whose KEYS are the available method
 *    names; getattr(svc, method) succeeds iff the key is present, then the
 *    call fires seam svc_call {method, path}. A missing key is an
 *    AttributeError swallowed by _lsp_call's except -> default. Non-object
 *    svc (str/int/...) has none of the LSP method names -> default.
 *  - future: object -> double with cancelled()/result() seams; null/str ->
 *    the value verbatim (.cancelled() AttributeError, caught by
 *    _queue_late_verdict's except). future_result scripts are COMPLETE
 *    CheckResult records {superseded, diagnostics, carry, baseline_text}
 *    (SimpleNamespace doubles: a missing field is a direct-read
 *    AttributeError that PROPAGATES, outside the try).
 *  - client/agent: objects -> SimpleNamespace doubles; null -> None.
 *  - notes/carry: arrays of {"text": str, "baseline_text": str|null};
 *    null notes with a truthy owner -> TypeError 'NoneType' not iterable.
 *  - token: integer id (or null). owners elements: str|null.
 *  - message/category in _from_tsserver: str|number|bool (str() projection);
 *    line/offset: number|bool or absent (null/str/list/dict -> TypeError).
 *    Numeric error-message receivers are INTEGERS (int vs float type names).
 *  - truncate/report_for_file/with_hermes_node_path results: str (truncate,
 *    report_for_file) / object with "PATH" (with_hermes_node_path) or raises.
 *
 * ASCII scope; integral floats == ints (libjson serializes integral doubles
 * as integers; the oracle's canon normalizes integral floats to ints).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include "libjson/json.h"

typedef struct lt_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} lt_seams_t;

/* ── universal-seam helpers ── */
static json_t *lt_call(const lt_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool lt_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *lt_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *lt_emsg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *lt_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *lt_args(void) { return json_object(); }
static void lt_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void lt_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void lt_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void lt_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *lt_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *lt_sval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static bool lt_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static char *lt_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* ── exception-parent table (FULL builtin hierarchy + subprocess family) ── */
typedef struct { const char *kind; const char *parent; } lt_exc_ent;
static const lt_exc_ent LT_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"LookupError", "Exception"},
    {"ArithmeticError", "Exception"},
    {"OSError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"ImportError", "Exception"},
    {"Warning", "Exception"},
    {"RuntimeError", "Exception"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ReferenceError", "Exception"},
    {"ExceptionGroup", "Exception"},
    {"SubprocessError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FileNotFoundError", "OSError"},
    {"PermissionError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"InterruptedError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"FileExistsError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"TimeoutExpired", "SubprocessError"},
    {"CalledProcessError", "SubprocessError"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {NULL, NULL},
};
static const char *lt_exc_parent(const char *kind) {
    for (const lt_exc_ent *e = LT_EXC_PARENTS; e->kind; e++)
        if (strcmp(e->kind, kind) == 0) return e->parent;
    if (strcmp(kind, "BaseException") == 0) return NULL;
    return "Exception";
}
static bool lt_exc_is(const char *kind, const char *base) {
    const char *k = kind;
    for (int depth = 0; k && depth < 32; depth++) {
        if (strcmp(k, base) == 0) return true;
        k = lt_exc_parent(k);
    }
    return false;
}
/* BaseException trio + CancelledError pass through except Exception */
static bool lt_is_base_exc(const char *kind) {
    return !lt_exc_is(kind, "Exception");
}

/* ── JSON value helpers ── */

/* Python type name for a JSON value (numeric receivers must be INTEGERS in
 * error-message fixtures: JSON cannot distinguish 5 from 5.0). */
static const char *lt_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Detach a key from an object WITHOUT freeing its value (Python dict.pop).
 * Returns the owned value node, or NULL when absent. Preserves order. */
static json_t *lt_obj_pop(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < obj->c.count; i++) {
        if (obj->c.keys[i] && strcmp(obj->c.keys[i], key) == 0) {
            json_t *val = obj->c.items[i];
            free(obj->c.keys[i]);
            for (size_t j = i; j + 1 < obj->c.count; j++) {
                obj->c.keys[j] = obj->c.keys[j + 1];
                obj->c.items[j] = obj->c.items[j + 1];
            }
            obj->c.count--;
            return val;
        }
    }
    return NULL;
}
/* OrderedDict.popitem(last=False): drop the FIRST entry (free key+value). */
static void lt_obj_pop_first(json_t *obj) {
    if (!obj || obj->type != JSON_OBJECT || obj->c.count == 0) return;
    free(obj->c.keys[0]);
    json_free(obj->c.items[0]);
    for (size_t j = 0; j + 1 < obj->c.count; j++) {
        obj->c.keys[j] = obj->c.keys[j + 1];
        obj->c.items[j] = obj->c.items[j + 1];
    }
    obj->c.count--;
}

/* Python str() projection for the scalar domain (str|number|bool). Integral
 * numbers render as str(int) ("5"); the fixture domain uses INT literals for
 * integral message/category. Non-integral floats use shortest round-trip repr. */
static char *lt_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return lt_xsf("None");
    if (v->type == JSON_STRING) return lt_xsf("%s", v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return lt_xsf("%s", v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        /* Integral numbers project as Python str(int) ("5", not "5.0"); the
         * fixture domain uses INT literals for integral message/category. */
        if (d == (double)(long long)d && fabs(d) < 1e16)
            return lt_xsf("%.0f", d);
        char tmp[64];
        for (int prec = 15; prec <= 17; prec++) {
            snprintf(tmp, sizeof(tmp), "%.*g", prec, d);
            if (strtod(tmp, NULL) == d) break;
        }
        return lt_xsf("%s", tmp);
    }
    /* array/object outside the documented scalar domain: best-effort JSON */
    char *s = json_serialize(v);
    char *out = lt_xsf("%s", s ? s : "");
    free(s);
    return out;
}

/* AttributeError for a direct read of a missing field on a SimpleNamespace
 * double (the oracle materializes objects as types.SimpleNamespace). */
static json_t *lt_attr_err(const char *cls, const char *attr) {
    char *m = lt_xsf("'%s' object has no attribute '%s'", cls, attr);
    json_t *r = lt_raise("AttributeError", m);
    free(m);
    return r;
}

/* logger.debug(fmt, *args, exc_info=True) seam — exact method + shape. */
static json_t *lt_log_debug(const lt_seams_t *sp, const char *fmt,
                            const char *arg0, bool exc_info) {
    json_t *a = lt_args();
    lt_s(a, "fmt", fmt);
    json_t *args = json_array();
    json_append(args, json_string(arg0 ? arg0 : ""));
    json_set(a, "args", args);
    lt_b(a, "exc_info", exc_info);
    return lt_call(sp, "logger_debug", a);
}

/* ── deferred queue state helpers ── */

static json_t *lt_note_new(const json_t *text, const json_t *baseline) {
    json_t *n = json_object();
    json_set(n, "text", text ? json_copy(text) : json_null());
    json_set(n, "baseline_text", baseline ? json_copy(baseline) : json_null());
    return n;
}

/* agent.lsp.deferred.post — pure state mutation (no seams, cannot raise on
 * the documented domain). _QUEUED.pop(owner) or OrderedDict(); merge an older
 * note for the same path when its text differs; cap per-owner then owners. */
/* PoP: lt_deferred_post @ agent/lsp/deferred.py:post */
static void lt_deferred_post(json_t *state, const char *owner, const char *path,
                             const json_t *text, const json_t *baseline) {
    json_t *queued = (json_t *)lt_oget(state, "queued");
    if (!queued) return;
    json_t *notes = lt_obj_pop(queued, owner);
    if (!notes || notes->type != JSON_OBJECT || notes->c.count == 0) {
        json_free(notes);
        notes = json_object();
    }
    json_t *older = lt_obj_pop(notes, path);
    const json_t *use_text = text;
    const json_t *use_base = baseline;
    char *merged = NULL;
    if (older && older->type == JSON_OBJECT) {
        const json_t *ot = lt_oget(older, "text");
        if (ot && ot->type == JSON_STRING && text && text->type == JSON_STRING &&
            strcmp(ot->str_val, text->str_val) != 0) {
            merged = lt_xsf("%s\n\n%s", ot->str_val, text->str_val);
            use_base = lt_oget(older, "baseline_text");
        }
    }
    json_t *note;
    if (merged) {
        note = json_object();
        json_set(note, "text", json_string(merged));
        json_set(note, "baseline_text", use_base ? json_copy(use_base) : json_null());
        free(merged);
    } else {
        note = lt_note_new(use_text, use_base);
    }
    json_set(notes, path, note);
    while (notes->c.count > 32) lt_obj_pop_first(notes);
    json_set(queued, owner, notes);
    while (queued->c.count > 64) lt_obj_pop_first(queued);
    json_free(older);
}

/* agent.lsp.deferred.take */
/* PoP: lt_deferred_take @ agent/lsp/deferred.py:take */
static json_t *lt_deferred_take(json_t *state, const char *owner, const char *path) {
    if (!owner || !owner[0]) return NULL;
    json_t *queued = (json_t *)lt_oget(state, "queued");
    json_t *notes = (json_t *)lt_oget(queued, owner);
    if (!notes || notes->type != JSON_OBJECT || notes->c.count == 0) return NULL;
    return lt_obj_pop(notes, path);
}

/* agent.lsp.deferred.restore — re-post each carried note (oldest first). */
/* PoP: lt_deferred_restore @ agent/lsp/deferred.py:restore */
static json_t *lt_deferred_restore(json_t *state, const char *owner,
                                   const char *path, const json_t *notes) {
    if (owner && owner[0]) {
        if (!notes || notes->type == JSON_NULL) {
            char *m = lt_xsf("'NoneType' object is not iterable");
            json_t *r = lt_raise("TypeError", m);
            free(m);
            return r;
        }
        if (notes->type != JSON_ARRAY) {
            char *m = lt_xsf("'%s' object is not iterable", lt_type_name(notes));
            json_t *r = lt_raise("TypeError", m);
            free(m);
            return r;
        }
        for (size_t i = 0; i < notes->c.count; i++) {
            const json_t *n = notes->c.items[i];
            lt_deferred_post(state, owner, path, lt_oget(n, "text"), lt_oget(n, "baseline_text"));
        }
    }
    return NULL;
}

/* agent.lsp.deferred.pop_notes(*owners) — dedup truthy owners (first wins),
 * pop each owner's notes, extend (path, text) pairs in insertion order. */
/* PoP: lt_deferred_pop_notes @ agent/lsp/deferred.py:pop_notes */
static json_t *lt_deferred_pop_notes(json_t *state, const json_t *owners) {
    json_t *out = json_array();
    json_t *queued = (json_t *)lt_oget(state, "queued");
    if (!owners || owners->type != JSON_ARRAY || !queued) return out;
    /* seen list (strdup'd) for dict.fromkeys dedup */
    char **seen = NULL;
    size_t nseen = 0;
    for (size_t i = 0; i < owners->c.count; i++) {
        const json_t *o = owners->c.items[i];
        if (!lt_truthy(o)) continue;
        const char *os = lt_sval(o);
        bool dup = false;
        for (size_t j = 0; j < nseen; j++)
            if (strcmp(seen[j], os) == 0) { dup = true; break; }
        if (dup) continue;
        seen = (char **)realloc(seen, (nseen + 1) * sizeof(char *));
        seen[nseen++] = strdup(os);
        json_t *notes = lt_obj_pop(queued, os);
        if (notes && notes->type == JSON_OBJECT && notes->c.count > 0) {
            for (size_t k = 0; k < notes->c.count; k++) {
                const json_t *note = notes->c.items[k];
                json_t *pair = json_array();
                json_append(pair, json_string(notes->c.keys[k]));
                json_append(pair, json_copy(lt_oget(note, "text")));
                json_append(out, pair);
            }
        }
        json_free(notes);
    }
    for (size_t j = 0; j < nseen; j++) free(seen[j]);
    free(seen);
    return out;
}

/* agent.lsp.deferred.track_pending — token = object(); model as state id. */
/* PoP: lt_deferred_track_pending @ agent/lsp/deferred.py:track_pending */
static json_t *lt_deferred_track_pending(json_t *state, const char *owner) {
    double tid = json_get_num(state, "next_token", 0);
    json_set(state, "next_token", json_number(tid + 1));
    json_t *pending = (json_t *)lt_oget(state, "pending");
    if (!pending) return json_number(tid);
    json_t *arr = (json_t *)lt_oget(pending, owner);
    if (!arr || arr->type != JSON_ARRAY) {
        arr = json_array();
        json_set(pending, owner, arr);
    }
    json_append(arr, json_number(tid));
    return json_number(tid);
}

/* agent.lsp.deferred.settled — discard token, drop empty set, notify_all. */
/* PoP: lt_deferred_settled @ agent/lsp/deferred.py:settled */
static json_t *lt_deferred_settled(const lt_seams_t *sp, json_t *state,
                                   const char *owner, const json_t *token) {
    json_t *pending = (json_t *)lt_oget(state, "pending");
    json_t *arr = (json_t *)lt_oget(pending, owner);
    if (arr && arr->type == JSON_ARRAY) {
        if (token && token->type == JSON_NUMBER) {
            for (size_t i = 0; i < arr->c.count; i++) {
                json_t *t = arr->c.items[i];
                if (t && t->type == JSON_NUMBER && t->num_val == token->num_val) {
                    json_free(t);
                    for (size_t j = i; j + 1 < arr->c.count; j++)
                        arr->c.items[j] = arr->c.items[j + 1];
                    arr->c.count--;
                    break;
                }
            }
        }
        if (arr->c.count == 0) json_obj_del(pending, owner);
    }
    json_t *r = lt_call(sp, "settled_notify_all", lt_args());
    if (lt_raised(r)) return r;
    json_free(r);
    return NULL;
}

/* agent.lsp.deferred._owners(agent) — (session_id, _current_task_id), each
 * kept only when isinstance str, else None. Pure. */
/* PoP: lt_owners @ agent/lsp/deferred.py:_owners */
static json_t *lt_owners(const json_t *agent) {
    json_t *out = json_array();
    const char *keys[2] = {"session_id", "_current_task_id"};
    for (int i = 0; i < 2; i++) {
        const json_t *v = lt_oget(agent, keys[i]);
        if (v && v->type == JSON_STRING) json_append(out, json_copy(v));
        else json_append(out, json_null());
    }
    return out;
}

/* agent.lsp.deferred.settle_for_agent */
/* PoP: lt_settle_for_agent @ agent/lsp/deferred.py:settle_for_agent */
static json_t *lt_settle_for_agent(const lt_seams_t *sp, json_t *state,
                                   const json_t *agent, double timeout) {
    json_t *ow = lt_owners(agent);
    /* owners = [o for o in dict.fromkeys(_owners(agent)) if o] */
    json_t *owners = json_array();
    char **seen = NULL;
    size_t nseen = 0;
    for (size_t i = 0; i < ow->c.count; i++) {
        const json_t *o = ow->c.items[i];
        if (!lt_truthy(o)) continue;
        const char *os = lt_sval(o);
        bool dup = false;
        for (size_t j = 0; j < nseen; j++)
            if (strcmp(seen[j], os) == 0) { dup = true; break; }
        if (dup) continue;
        seen = (char **)realloc(seen, (nseen + 1) * sizeof(char *));
        seen[nseen++] = strdup(os);
        json_append(owners, json_copy(o));
    }
    json_free(ow);
    for (size_t j = 0; j < nseen; j++) free(seen[j]);
    free(seen);
    if (owners->c.count == 0) { json_free(owners); return json_array(); }
    /* pred = not any(_PENDING.get(o) for o in owners) */
    json_t *pending = (json_t *)lt_oget(state, "pending");
    bool pred = true;
    for (size_t i = 0; i < owners->c.count; i++) {
        json_t *arr = (json_t *)lt_oget(pending, lt_sval(owners->c.items[i]));
        if (arr && arr->type == JSON_ARRAY && arr->c.count > 0) { pred = false; break; }
    }
    json_t *a = lt_args();
    lt_n(a, "timeout", timeout);
    lt_b(a, "pred", pred);
    json_t *r = lt_call(sp, "settled_wait_for", a);
    if (lt_raised(r)) { json_free(owners); return r; }
    json_free(r);
    json_t *ret = lt_deferred_pop_notes(state, owners);
    json_free(owners);
    return ret;
}

/* agent.lsp.deferred.forward_to_agent */
/* PoP: lt_forward_to_agent @ agent/lsp/deferred.py:forward_to_agent */
static json_t *lt_forward_to_agent(const lt_seams_t *sp, json_t *state,
                                   const json_t *agent) {
    json_t *ow = lt_owners(agent);
    json_t *notes = lt_deferred_pop_notes(state, ow);
    json_free(ow);
    if (notes->c.count == 0) { json_free(notes); return NULL; }
    for (size_t i = 0; i < notes->c.count; i++) {
        json_t *pair = notes->c.items[i];
        const char *path = lt_sval(json_get(pair, 0));
        const json_t *text = json_get(pair, 1);
        char *key = lt_xsf("lsp:%s", path);
        json_t *a = lt_args();
        lt_j(a, "agent", agent);
        lt_s(a, "key", key);
        lt_s(a, "text", lt_sval(text));
        free(key);
        json_t *r = lt_call(sp, "queue_plugin_notice", a);
        if (lt_raised(r)) { json_free(notes); return r; }
        json_free(r);
    }
    json_free(notes);
    return NULL;
}

/* ── tools/file_operations_lint.py ported defs ── */

/* _lsp_service(self): None on non-local backend / any Exception. */
/* PoP: lt_lsp_service @ tools/file_operations_lint.py:_lsp_service */
static json_t *lt_lsp_service(const lt_seams_t *sp, const json_t *slf) {
    (void)slf;
    json_t *r = lt_call(sp, "lsp_local_only", lt_args());
    if (lt_raised(r)) return r;
    bool local = lt_truthy(r);
    json_free(r);
    if (!local) return NULL;
    json_t *g = lt_call(sp, "get_service", lt_args());
    if (lt_raised(g)) {
        if (lt_is_base_exc(lt_kind(g))) return g;
        json_free(g);
        return NULL;
    }
    return g;
}

/* _lsp_call(self, method, path, default): svc.<method>(path) or default. */
/* PoP: lt_lsp_call @ tools/file_operations_lint.py:_lsp_call */
static json_t *lt_lsp_call(const lt_seams_t *sp, const json_t *slf,
                           const char *method, const char *path,
                           const json_t *defval) {
    json_t *svc = lt_lsp_service(sp, slf);
    if (lt_raised(svc)) return svc;
    if (!svc || svc->type == JSON_NULL) { json_free(svc); return defval ? json_copy(defval) : NULL; }
    /* getattr(svc, method): succeeds iff svc is an object carrying the key.
     * A missing attr is an AttributeError swallowed by except -> default. */
    if (svc->type != JSON_OBJECT || !json_has(svc, method)) {
        json_free(svc);
        return defval ? json_copy(defval) : NULL;
    }
    json_t *a = lt_args();
    lt_s(a, "method", method);
    lt_s(a, "path", path);
    json_t *r = lt_call(sp, "svc_call", a);
    json_free(svc);
    if (lt_raised(r)) {
        if (lt_is_base_exc(lt_kind(r))) return r;
        json_free(r);
        return defval ? json_copy(defval) : NULL;
    }
    return r;
}

/* Read `getattr(self.env, "cwd", None) or self.cwd` with the live direct-read
 * AttributeError semantics. Returns a BORROWED node (from slf) or NULL; on a
 * missing direct attr, returns a raise envelope via *err. */
static const json_t *lt_self_cwd(const json_t *slf, json_t **err) {
    *err = NULL;
    const json_t *env = lt_oget(slf, "env");
    if (!env) {  /* self.env absent -> direct-read AttributeError */
        *err = lt_attr_err("types.SimpleNamespace", "env");
        return NULL;
    }
    const json_t *envcwd = lt_oget(env, "cwd");
    if (lt_truthy(envcwd)) return envcwd;
    const json_t *cwd = lt_oget(slf, "cwd");
    if (!cwd) {  /* self.cwd absent -> direct-read AttributeError */
        *err = lt_attr_err("types.SimpleNamespace", "cwd");
        return NULL;
    }
    return cwd;
}

/* _local_workspace_untrusted(self) */
/* PoP: lt_local_workspace_untrusted @ tools/file_operations_lint.py:_local_workspace_untrusted */
static json_t *lt_local_workspace_untrusted(const lt_seams_t *sp, const json_t *slf) {
    json_t *r = lt_call(sp, "lsp_local_only", lt_args());
    if (lt_raised(r)) return r;
    bool local = lt_truthy(r);
    json_free(r);
    if (!local) return json_bool(false);
    json_t *cfg = lt_call(sp, "load_config_readonly", lt_args());
    if (lt_raised(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        char *m = lt_xsf("'%s' object has no attribute 'get'", lt_type_name(cfg));
        json_t *e = lt_raise("AttributeError", m);
        free(m);
        json_free(cfg);
        return e;
    }
    const json_t *lsp_cfg = lt_oget(cfg, "lsp");
    const json_t *tw = NULL;
    if (lsp_cfg && lsp_cfg->type == JSON_OBJECT) tw = lt_oget(lsp_cfg, "trusted_workspaces");
    json_t *a = lt_args();
    lt_j(a, "arg", tw);
    json_t *trusted = lt_call(sp, "parse_trusted_workspaces", a);
    if (lt_raised(trusted)) { json_free(cfg); return trusted; }
    /* arg1: getattr(self.env,"cwd",None) or self.cwd (before operator_roots) */
    json_t *cerr = NULL;
    const json_t *cwd = lt_self_cwd(slf, &cerr);
    if (cerr) { json_free(cfg); json_free(trusted); return cerr; }
    json_t *roots = lt_call(sp, "operator_workspace_roots", lt_args());
    if (lt_raised(roots)) { json_free(cfg); json_free(trusted); return roots; }
    a = lt_args();
    lt_j(a, "cwd", cwd);
    lt_j(a, "trusted", trusted);
    lt_j(a, "roots", roots);
    json_t *res = lt_call(sp, "is_trusted_workspace", a);
    json_free(cfg);
    json_free(trusted);
    json_free(roots);
    if (lt_raised(res)) return res;
    json_t *ret = json_bool(!lt_truthy(res));
    json_free(res);
    return ret;
}

/* _run_managed_node_linter(self, ext, path) */
/* PoP: lt_run_managed_node_linter @ tools/file_operations_lint.py:_run_managed_node_linter */
static json_t *lt_run_managed_node_linter(const lt_seams_t *sp, const json_t *slf,
                                          const char *ext, const char *path,
                                          bool is_windows) {
    const char *tool = NULL;
    const char *margs[2] = {NULL, NULL};
    int nargs = 0;
    if (strcmp(ext, ".js") == 0) { tool = "node"; margs[0] = "--check"; nargs = 1; }
    else if (strcmp(ext, ".ts") == 0) { tool = "npx"; margs[0] = "tsc"; margs[1] = "--noEmit"; nargs = 2; }
    else {
        char *m = lt_xsf("'%s'", ext);
        json_t *e = lt_raise("KeyError", m);
        free(m);
        return e;
    }
    /* executable = shutil.which(tool, path=with_hermes_node_path({"PATH":""})["PATH"]) */
    json_t *env0 = json_object();
    json_set(env0, "PATH", json_string(""));
    json_t *a = lt_args();
    lt_j(a, "env", env0);
    json_free(env0);
    json_t *r1 = lt_call(sp, "with_hermes_node_path", a);
    if (lt_raised(r1)) return r1;
    if (!r1 || r1->type != JSON_OBJECT) {
        char *m;
        if (r1 && r1->type == JSON_ARRAY)
            m = lt_xsf("list indices must be integers or slices, not str");
        else
            m = lt_xsf("'%s' object is not subscriptable", lt_type_name(r1));
        json_t *e = lt_raise("TypeError", m);
        free(m);
        json_free(r1);
        return e;
    }
    if (!json_has(r1, "PATH")) {
        json_free(r1);
        return lt_raise("KeyError", "'PATH'");
    }
    const json_t *pathval = lt_oget(r1, "PATH");
    a = lt_args();
    lt_s(a, "tool", tool);
    lt_j(a, "path", pathval);
    json_t *exe = lt_call(sp, "shutil_which", a);
    json_free(r1);
    if (lt_raised(exe)) return exe;
    if (!exe || exe->type == JSON_NULL) { json_free(exe); return NULL; }
    /* cwd = getattr(self.env,"cwd",None) or self.cwd */
    json_t *cerr = NULL;
    const json_t *cwd_b = lt_self_cwd(slf, &cerr);
    if (cerr) { json_free(exe); return cerr; }
    json_t *cwd = cwd_b ? json_copy(cwd_b) : json_null();
    json_t *pathn = json_string(path);
    if (is_windows) {
        a = lt_args();
        lt_s(a, "path", path);
        json_t *p2 = lt_call(sp, "msys_to_windows_path", a);
        if (lt_raised(p2)) { json_free(exe); json_free(cwd); json_free(pathn); return p2; }
        json_free(pathn);
        pathn = p2;
        if (lt_truthy(cwd)) {
            a = lt_args();
            lt_j(a, "path", cwd);
            json_t *c2 = lt_call(sp, "msys_to_windows_path", a);
            if (lt_raised(c2)) { json_free(exe); json_free(cwd); json_free(pathn); return c2; }
            json_free(cwd);
            cwd = c2;
        }
    }
    /* env=with_hermes_node_path(hermes_subprocess_env()) */
    json_t *henv = lt_call(sp, "hermes_subprocess_env", lt_args());
    if (lt_raised(henv)) { json_free(exe); json_free(cwd); json_free(pathn); return henv; }
    a = lt_args();
    lt_j(a, "env", henv);
    json_t *env2 = lt_call(sp, "with_hermes_node_path", a);
    json_free(henv);
    if (lt_raised(env2)) { json_free(exe); json_free(cwd); json_free(pathn); return env2; }
    json_t *flags = lt_call(sp, "windows_hide_flags", lt_args());
    if (lt_raised(flags)) { json_free(exe); json_free(cwd); json_free(pathn); json_free(env2); return flags; }
    /* cmd = [executable, *args, path] */
    json_t *cmd = json_array();
    json_append(cmd, json_copy(exe));
    for (int i = 0; i < nargs; i++) json_append(cmd, json_string(margs[i]));
    json_append(cmd, json_copy(pathn));
    a = lt_args();
    json_set(a, "cmd", cmd);
    lt_j(a, "cwd", lt_truthy(cwd) ? cwd : NULL);
    lt_j(a, "env", env2);
    lt_n(a, "stdin", -3);
    lt_n(a, "stdout", -1);
    lt_n(a, "stderr", -2);
    lt_b(a, "text", true);
    lt_s(a, "encoding", "utf-8");
    lt_s(a, "errors", "replace");
    lt_n(a, "timeout", 30);
    lt_j(a, "creationflags", flags);
    lt_b(a, "check", false);
    json_t *proc = lt_call(sp, "subprocess_run", a);
    json_free(exe);
    json_free(cwd);
    json_free(pathn);
    json_free(env2);
    json_free(flags);
    if (lt_raised(proc)) {
        if (strcmp(lt_kind(proc), "TimeoutExpired") == 0) {
            json_free(proc);
            char *m = lt_xsf("%s timed out after 30s", tool);
            json_t *ret = json_object();
            json_set(ret, "stdout", json_string(m));
            json_set(ret, "exit_code", json_number(124));
            json_set(ret, "cwd_error", json_string(""));
            free(m);
            return ret;
        }
        return proc;  /* other Exception / BaseException propagates */
    }
    /* proc.stdout / proc.returncode (SimpleNamespace direct reads) */
    if (!proc || proc->type != JSON_OBJECT) {
        json_t *e = lt_attr_err(lt_type_name(proc), "stdout");
        json_free(proc);
        return e;
    }
    if (!json_has(proc, "stdout")) { json_free(proc); return lt_attr_err("types.SimpleNamespace", "stdout"); }
    if (!json_has(proc, "returncode")) { json_free(proc); return lt_attr_err("types.SimpleNamespace", "returncode"); }
    const json_t *so = lt_oget(proc, "stdout");
    const json_t *rc = lt_oget(proc, "returncode");
    json_t *ret = json_object();
    json_set(ret, "stdout", lt_truthy(so) ? json_copy(so) : json_string(""));
    json_set(ret, "exit_code", rc ? json_copy(rc) : json_null());
    json_set(ret, "cwd_error", json_string(""));
    json_free(proc);
    return ret;
}

/* _format_lsp_block(path, diagnostics, header) */
/* PoP: lt_format_lsp_block @ tools/file_operations_lint.py:_format_lsp_block */
static json_t *lt_format_lsp_block(const lt_seams_t *sp, const char *path,
                                   const json_t *diags, const char *header) {
    if (!lt_truthy(diags)) return json_string("");
    json_t *a = lt_args();
    lt_s(a, "path", path);
    lt_j(a, "diagnostics", diags);
    json_t *block = lt_call(sp, "report_for_file", a);
    if (lt_raised(block)) {
        if (lt_is_base_exc(lt_kind(block))) return block;
        json_free(block);
        goto except_path;
    }
    if (!lt_truthy(block)) { json_free(block); return json_string(""); }
    if (block->type != JSON_STRING) {
        /* header + block -> TypeError (caught); message is swallowed */
        json_free(block);
        goto except_path;
    }
    {
        char *text = lt_xsf("%s%s", header, block->str_val);
        json_free(block);
        a = lt_args();
        lt_s(a, "text", text);
        free(text);
        json_t *r = lt_call(sp, "truncate", a);
        if (lt_raised(r)) {
            if (lt_is_base_exc(lt_kind(r))) return r;
            json_free(r);
            goto except_path;
        }
        return r;
    }
except_path:
    {
        json_t *lr = lt_log_debug(sp, "formatting LSP diagnostics for %s failed", path, true);
        if (lt_raised(lr)) return lr;
        json_free(lr);
        return json_string("");
    }
}

/* _queue_late_verdict(owner, abs_path, future, carry) */
/* PoP: lt_queue_late_verdict @ tools/file_operations_lint.py:_queue_late_verdict */
static json_t *lt_queue_late_verdict(const lt_seams_t *sp, json_t *state,
                                     const char *owner, const char *abs_path,
                                     const json_t *future, const json_t *carry) {
    json_t *result = NULL;
    /* try: result = None if future.cancelled() else future.result() */
    if (!future || future->type != JSON_OBJECT) {
        /* future.cancelled() AttributeError -> caught by except Exception */
        goto except_path;
    }
    json_t *c = lt_call(sp, "future_cancelled", lt_args());
    if (lt_raised(c)) {
        if (lt_is_base_exc(lt_kind(c))) return c;
        json_free(c);
        goto except_path;
    }
    bool cancelled = lt_truthy(c);
    json_free(c);
    if (!cancelled) {
        result = lt_call(sp, "future_result", lt_args());
        if (lt_raised(result)) {
            if (lt_is_base_exc(lt_kind(result))) return result;
            json_free(result);
            result = NULL;
            goto except_path;
        }
    }
    /* post-try branches (direct SimpleNamespace reads PROPAGATE) */
    if (!result || result->type == JSON_NULL) {
        json_free(result);
        json_t *e = lt_deferred_restore(state, owner, abs_path, carry);
        if (lt_raised(e)) return e;
        return NULL;
    }
    if (result->type != JSON_OBJECT) {
        json_t *e = lt_attr_err(lt_type_name(result), "superseded");
        json_free(result);
        return e;
    }
    if (!json_has(result, "superseded")) { json_free(result); return lt_attr_err("types.SimpleNamespace", "superseded"); }
    if (lt_truthy(lt_oget(result, "superseded"))) { json_free(result); return NULL; }
    if (!json_has(result, "diagnostics")) { json_free(result); return lt_attr_err("types.SimpleNamespace", "diagnostics"); }
    const json_t *diags = lt_oget(result, "diagnostics");
    if (!diags || diags->type == JSON_NULL) {
        if (!json_has(result, "carry")) { json_free(result); return lt_attr_err("types.SimpleNamespace", "carry"); }
        const json_t *rcarry = lt_oget(result, "carry");
        json_t *e = lt_deferred_restore(state, owner, abs_path, rcarry);
        json_free(result);
        if (lt_raised(e)) return e;
        return NULL;
    }
    {
        char *header = lt_xsf(
            "LSP diagnostics introduced by your earlier edit of %s "
            "(the language server answered after that tool result returned):\n",
            abs_path);
        json_t *text = lt_format_lsp_block(sp, abs_path, diags, header);
        free(header);
        if (lt_raised(text)) { json_free(result); return text; }
        if (lt_truthy(text)) {
            if (!json_has(result, "baseline_text")) {
                json_free(text);
                json_free(result);
                return lt_attr_err("types.SimpleNamespace", "baseline_text");
            }
            lt_deferred_post(state, owner, abs_path, text, lt_oget(result, "baseline_text"));
        }
        json_free(text);
    }
    json_free(result);
    return NULL;
except_path:
    {
        json_t *lr = lt_log_debug(sp, "deferred LSP edit check failed for %s", abs_path, true);
        if (lt_raised(lr)) return lr;
        json_free(lr);
        json_t *e = lt_deferred_restore(state, owner, abs_path, carry);
        if (lt_raised(e)) return e;
        return NULL;
    }
}

/* _post_deferred(owner, abs_path, future, carry, token): try queue finally settled */
/* PoP: lt_post_deferred @ tools/file_operations_lint.py:_post_deferred */
static json_t *lt_post_deferred(const lt_seams_t *sp, json_t *state,
                                const char *owner, const char *abs_path,
                                const json_t *future, const json_t *carry,
                                const json_t *token) {
    json_t *r1 = lt_queue_late_verdict(sp, state, owner, abs_path, future, carry);
    json_t *r2 = lt_deferred_settled(sp, state, owner, token);
    if (lt_raised(r2)) { json_free(r1); return r2; }
    json_free(r2);
    return r1;
}

/* ── agent/lsp/deferred.py ContextVar flows ── */

/* bind_owner(owner) driven as a context manager over a scripted body. */
/* PoP: lt_bind_owner @ agent/lsp/deferred.py:bind_owner */
static json_t *lt_bind_owner(const lt_seams_t *sp, const char *owner,
                             const json_t *body) {
    json_t *a = lt_args();
    /* value = owner or None */
    if (owner && owner[0]) json_set(a, "value", json_string(owner));
    else json_set(a, "value", json_null());
    json_t *tok = lt_call(sp, "owner_set", a);
    if (lt_raised(tok)) return tok;  /* __enter__ raised: no reset */
    json_t *body_exc = NULL;
    if (body && body->type == JSON_OBJECT) {
        if (lt_truthy(lt_oget(body, "read"))) {
            json_t *g = lt_call(sp, "owner_get", lt_args());
            if (lt_raised(g)) body_exc = g;
            else json_free(g);
        }
        if (!body_exc) {
            const json_t *br = lt_oget(body, "raise");
            if (br && br->type == JSON_OBJECT && lt_raised(br)) body_exc = json_copy(br);
        }
    }
    /* finally: _OWNER.reset(token) — always fires; a reset raise wins */
    a = lt_args();
    lt_j(a, "token", tok);
    json_t *r = lt_call(sp, "owner_reset", a);
    json_free(tok);
    if (lt_raised(r)) { json_free(body_exc); return r; }
    json_free(r);
    return body_exc;
}

/* current_owner() */
/* PoP: lt_current_owner @ agent/lsp/deferred.py:current_owner */
static json_t *lt_current_owner(const lt_seams_t *sp) {
    return lt_call(sp, "owner_get", lt_args());
}

/* ── agent/lsp/sync_diagnostics.py leaves ── */

/* supported(client): client.server_id in _TS_SERVERS and not getattr(...) */
/* PoP: lt_sync_supported @ agent/lsp/sync_diagnostics.py:supported */
static json_t *lt_sync_supported(const json_t *client) {
    if (!client || client->type != JSON_OBJECT)
        return lt_attr_err(lt_type_name(client), "server_id");
    if (!json_has(client, "server_id"))
        return lt_attr_err("types.SimpleNamespace", "server_id");
    const json_t *sid = lt_oget(client, "server_id");
    if (!sid || sid->type != JSON_STRING || strcmp(sid->str_val, "typescript") != 0)
        return json_bool(false);
    const json_t *unsup = lt_oget(client, "_hermes_sync_diagnostics_unsupported");
    return json_bool(!lt_truthy(unsup));
}

/* _from_tsserver(item): tsserver item -> LSP diagnostic dict (or None). */
/* PoP: lt_from_tsserver @ agent/lsp/sync_diagnostics.py:_from_tsserver */
static json_t *lt_from_tsserver(const json_t *item) {
    if (!item || item->type != JSON_OBJECT) return NULL;
    const json_t *start = lt_oget(item, "startLocation");
    const json_t *end = lt_oget(item, "endLocation");
    if (!start || start->type != JSON_OBJECT || !end || end->type != JSON_OBJECT)
        return NULL;
    /* loc.get(key, 1) - 1; a present non-number raises TypeError */
    double coords[4];
    const json_t *locs[2] = {start, end};
    const char *fields[2] = {"line", "offset"};
    for (int li = 0; li < 2; li++) {
        for (int fi = 0; fi < 2; fi++) {
            const json_t *v = lt_oget(locs[li], fields[fi]);
            double base = 1.0;
            if (v) {
                if (v->type == JSON_NUMBER) base = v->num_val;
                else if (v->type == JSON_BOOL) base = v->bool_val ? 1.0 : 0.0;
                else {
                    char *m = lt_xsf("unsupported operand type(s) for -: '%s' and 'int'",
                                     lt_type_name(v));
                    json_t *e = lt_raise("TypeError", m);
                    free(m);
                    return e;
                }
            }
            coords[li * 2 + fi] = base - 1.0;
        }
    }
    json_t *range = json_object();
    json_t *s = json_object();
    json_set(s, "line", json_number(coords[0]));
    json_set(s, "character", json_number(coords[1]));
    json_t *e = json_object();
    json_set(e, "line", json_number(coords[2]));
    json_set(e, "character", json_number(coords[3]));
    json_set(range, "start", s);
    json_set(range, "end", e);
    /* message = str(item.get("message") or "") */
    const json_t *msg = lt_oget(item, "message");
    char *msgs;
    if (!lt_truthy(msg)) msgs = lt_xsf("");
    else msgs = lt_py_str(msg);
    /* severity = _TS_SEVERITY.get(str(item.get("category") or "error"), 1) */
    const json_t *cat = lt_oget(item, "category");
    char *cats;
    if (!lt_truthy(cat)) cats = lt_xsf("error");
    else cats = lt_py_str(cat);
    double sev = 1.0;
    if (strcmp(cats, "error") == 0) sev = 1;
    else if (strcmp(cats, "warning") == 0) sev = 2;
    else if (strcmp(cats, "message") == 0) sev = 3;
    else if (strcmp(cats, "suggestion") == 0) sev = 4;
    free(cats);
    /* source = item.get("source") or "typescript" */
    const json_t *src = lt_oget(item, "source");
    json_t *diag = json_object();
    json_set(diag, "range", range);
    json_set(diag, "message", json_string(msgs));
    free(msgs);
    json_set(diag, "severity", json_number(sev));
    json_set(diag, "source", lt_truthy(src) ? json_copy(src) : json_string("typescript"));
    const json_t *code = lt_oget(item, "code");
    if (code && code->type != JSON_NULL) json_set(diag, "code", json_copy(code));
    return diag;
}
