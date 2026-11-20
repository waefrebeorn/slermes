/*
 * session_transcript_subagent_flow2.c — FLOW port of gateway/
 * session_transcript_subagent.py (hermes-agent), the 5 unclaimed defs:
 *   unavailable, subagent_transcript, _child, roster (nested in _child), _read.
 *
 * Decision sequences only: every live collaborator call enters through ONE
 * universal seam sts2_call(ctx, name, args). The harness wires it to a
 * recording stub fed in.io; the oracle installs recording monkeypatch stubs
 * emitting the identical {"name","args","ret"} events from the SAME scripts.
 *
 * Seam table (name -> live call):
 *   authorize                       authorize(connection, ref, params,
 *                                   'session:read')
 *   to_thread                       asyncio.to_thread(_read, authority, live,
 *                                   ref, binding, held, limit, inline_images)
 *   owner_scope                     owner_scope(authority)      (CM factory)
 *   owner_scope.__enter__           cm.__enter__()
 *   owner_scope.__exit__            cm.__exit__(exc)
 *   authority_persistence           authority_persistence(authority)
 *   child                           _child(authority, live, ref, subagent_id)
 *                                   (ret = [child, running])
 *   delegated_child_lineage_ids     store.lineage.delegated_child_lineage_ids(child)
 *   read_transcript                 read_transcript(store, child, lineage_ids,
 *                                   binding, held, limit, profile_home=...,
 *                                   inline_images=...)
 *   locked_projection               locked_projection(authority, live, ref, roster)
 *                                   (ret = [records, target])
 *   require_current_route           _require_current_route(authority, ref)
 *   owner_objects                   _owner_objects(authority, ref,
 *                                   include_fallback_evicted=True)
 *   physical_target                 physical_target(authority, ref)
 *   compression_lineage             _compression_lineage(store, target)
 *   stored_child_session            stored_child_session(store, subagent_id,
 *                                   lineage)
 *
 * Cross-function calls are seamed, not composed: subagent_transcript -> _read
 * (to_thread), _read -> _child (child), _child -> the nested roster
 * (locked_projection), so each func's trace holds only its immediate
 * collaborators and roster is driven on its own.
 *
 * Representation contract (shared with the oracle):
 * - NULL json_t* == Python None (absent); a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; the FULL builtin
 *   hierarchy table decides Exception lineage (this module has no except, but
 *   the projection is uniform). Unknown kinds are synthetic Exception classes.
 * - Scripted seams: a plain value (reused), a bare array (per-call queue, last
 *   value does NOT repeat), or {"__script__": true, "ret"/"raise"}. A bare
 *   {"__raise__","message"} dict also raises. Missing/exhausted script is a
 *   fail-closed RuntimeError; no io channel is "no seam channel installed".
 * - None-valued arg keys are dropped on both sides. ASCII fixture corpus.
 * - owner_scope is a CM: an __enter__ raise skips the body; an __exit__ raise
 *   replaces a pending body raise; a truthy __exit__ suppresses it (never
 *   exercised with a pending raise — _read would read an unbound `page`).
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and sts2_-prefixed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct sts2_ctx {
    const json_t *io;    /* borrowed scripted seam results; NULL == no channel */
    json_t *cursors;     /* owned; per-name array cursors */
    json_t *trace;       /* owned; ordered seam events */
} sts2_ctx_t;

/* ── basic helpers ── */
static bool sts2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sts2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sts2_truthy(const json_t *v) {
    if (sts2_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *sts2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sts2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *sts2_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return sts2_strdup("");
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : sts2_strdup("");
}
static json_t *sts2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *sts2_val(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* Scalars-only equality (matches fixture-domain values). */
static bool sts2_json_eq(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (sts2_is_null(a) || sts2_is_null(b))
        return sts2_is_null(a) && sts2_is_null(b);
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        default: return false; /* records / lineage compared by identity only */
    }
}
static json_t *sts2_cm_error(const json_t *v) {
    const char *tn = sts2_is_null(v) ? "NoneType" :
        (v->type == JSON_OBJECT ? "dict" : v->type == JSON_ARRAY ? "list" :
         v->type == JSON_STRING ? "str" : v->type == JSON_BOOL ? "bool" : "int");
    char *msg = sts2_fmt("'%s' object does not support the context manager protocol "
                         "(missed __exit__ method)", tn);
    json_t *e = sts2_err("TypeError", msg);
    free(msg);
    return e;
}

/* ── exception lineage (FULL builtin hierarchy, py3.14) ── */
typedef struct { const char *name; const char *parent; } sts2_exc_row_t;
static const sts2_exc_row_t STS2_EXC_PARENTS[] = {
    {"ArithmeticError","Exception"},{"AssertionError","Exception"},
    {"AttributeError","Exception"},{"BaseException","object"},
    {"BaseExceptionGroup","BaseException"},{"BlockingIOError","OSError"},
    {"BrokenPipeError","ConnectionError"},{"BufferError","Exception"},
    {"BytesWarning","Warning"},{"CancelledError","BaseException"},
    {"ChildProcessError","OSError"},{"ConnectionAbortedError","ConnectionError"},
    {"ConnectionError","OSError"},{"ConnectionRefusedError","ConnectionError"},
    {"ConnectionResetError","ConnectionError"},{"DeprecationWarning","Warning"},
    {"EOFError","Exception"},{"EncodingWarning","Warning"},
    {"EnvironmentError","Exception"},{"Exception","BaseException"},
    {"ExceptionGroup","Exception"},{"FileExistsError","OSError"},
    {"FileNotFoundError","OSError"},{"FloatingPointError","ArithmeticError"},
    {"FutureWarning","Warning"},{"GeneratorExit","BaseException"},
    {"IOError","Exception"},{"ImportError","Exception"},{"ImportWarning","Warning"},
    {"IndentationError","SyntaxError"},{"IndexError","LookupError"},
    {"InterruptedError","OSError"},{"IsADirectoryError","OSError"},
    {"KeyError","LookupError"},{"KeyboardInterrupt","BaseException"},
    {"LookupError","Exception"},{"MemoryError","Exception"},
    {"ModuleNotFoundError","ImportError"},{"NameError","Exception"},
    {"NotADirectoryError","OSError"},{"NotImplementedError","RuntimeError"},
    {"OSError","Exception"},{"OverflowError","ArithmeticError"},
    {"PendingDeprecationWarning","Warning"},{"PermissionError","OSError"},
    {"ProcessLookupError","OSError"},{"PythonFinalizationError","RuntimeError"},
    {"RecursionError","RuntimeError"},{"ReferenceError","Exception"},
    {"ResourceWarning","Warning"},{"RuntimeError","Exception"},
    {"RuntimeWarning","Warning"},{"StopAsyncIteration","Exception"},
    {"StopIteration","Exception"},{"SyntaxError","Exception"},
    {"SyntaxWarning","Warning"},{"SystemError","Exception"},
    {"SystemExit","BaseException"},{"TabError","IndentationError"},
    {"TimeoutError","OSError"},{"TypeError","Exception"},
    {"UnboundLocalError","NameError"},{"UnicodeDecodeError","UnicodeError"},
    {"UnicodeEncodeError","UnicodeError"},{"UnicodeError","ValueError"},
    {"UnicodeTranslateError","UnicodeError"},{"UnicodeWarning","Warning"},
    {"UserWarning","Warning"},{"ValueError","Exception"},{"Warning","Exception"},
    {"WindowsError","Exception"},{"ZeroDivisionError","ArithmeticError"},
    {"_IncompleteInputError","SyntaxError"},
    /* project exceptions: quarry persistence.core.errors */
    {"PersistenceError","Exception"},{"AuthorityRefusal","PersistenceError"},
    {"Conflict","PersistenceError"},{"_SessionConflict","Conflict"},
    {"CompressionBusy","_SessionConflict"},{"CompressionInProgress","CompressionBusy"},
    {"TurnLeaseLost","_SessionConflict"},{"TransientStoreError","PersistenceError"},
    {"PermanentStoreError","PersistenceError"},{"CorruptStoreError","PermanentStoreError"},
    {"StoreUnavailable","PersistenceError"},{"StoreReplaced","PersistenceError"},
};

static const char *sts2_exc_parent(const char *name) {
    for (size_t i = 0; i < sizeof(STS2_EXC_PARENTS)/sizeof(STS2_EXC_PARENTS[0]); i++)
        if (strcmp(STS2_EXC_PARENTS[i].name, name) == 0)
            return STS2_EXC_PARENTS[i].parent;
    return "Exception"; /* synthetic kind == Exception subclass */
}

/* True when `except Exception` would catch the kind (Exception lineage). */
static bool sts2_exc_is_exception(const char *kind) {
    const char *cur = kind;
    for (int hops = 0; hops < 16; hops++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return false;
        cur = sts2_exc_parent(cur);
    }
    return false;
}

/* ── seam dispatch ── */
static json_t *sts2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT)
            return sts2_err(json_get_str(raise, "type", "RuntimeError"),
                            json_get_str(raise, "message", ""));
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The returned node is NEW; the caller frees it. */
static json_t *sts2_call(sts2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    if (!c->io) {
        json_t *ev = json_object();
        json_set_hdr(ev, "name", name);
        json_set(ev, "args", args);
        char *msg = sts2_fmt("no seam channel installed for %s", name);
        json_t *res = sts2_err("RuntimeError", msg);
        free(msg);
        json_set(ev, "ret", json_copy(res));
        json_append(c->trace, ev);
        return res;
    }
    const json_t *entry = json_obj_get(c->io, name);
    json_t *res;
    if (!entry) {
        char *msg = sts2_fmt("no script for %s", name);
        res = sts2_err("RuntimeError", msg);
        free(msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char *msg = sts2_fmt("script exhausted for %s", name);
            res = sts2_err("RuntimeError", msg);
            free(msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = sts2_entry_result(json_get(entry, idx));
        }
    } else {
        res = sts2_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* copy a borrowed value into an args object; NULL/JSON-NULL (None) drops the key */
static void sts2_jset(json_t *obj, const char *key, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(obj, key, json_copy(v));
}
static json_t *sts2_args1(const char *k1, const json_t *v1) {
    json_t *a = json_object();
    sts2_jset(a, k1, v1);
    return a;
}

/* ===========================================================================
 * Ported flows
 * =========================================================================== */

/* PoP: sts2_unavailable @ gateway/session_transcript_subagent.py:unavailable */
static json_t *sts2_unavailable(sts2_ctx_t *c, const json_t *args) {
    (void)c;
    json_t *out = json_object();
    json_set(out, "session_id", sts2_val(json_obj_get(args, "session_id")));
    json_set(out, "rows", json_array());
    json_set(out, "next_cursor", json_null());
    json_set(out, "reset", json_bool(true));
    json_set(out, "subagent_id", sts2_val(json_obj_get(args, "subagent_id")));
    json_set(out, "available", json_bool(false));
    json_set(out, "running", sts2_val(json_obj_get(args, "running")));
    json_set(out, "child_session_id", json_null());
    return out;
}

/* PoP: sts2_subagent_transcript @ gateway/session_transcript_subagent.py:subagent_transcript */
static json_t *sts2_subagent_transcript(sts2_ctx_t *c, const json_t *args) {
    const json_t *connection = json_obj_get(args, "connection");
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    const json_t *authority = json_obj_get(connection, "authority");
    const json_t *sessions = json_obj_get(authority, "sessions");
    const char *sid = sts2_str(json_obj_get(ref, "session_id"));

    /* Cold adoption belongs to session.resume, never a child poll. */
    if (!sid || !sessions || sessions->type != JSON_OBJECT || !json_has(sessions, sid))
        return sts2_err("AuthorityRefusal", "not_found");

    json_t *aa = json_object();
    sts2_jset(aa, "connection", connection);
    sts2_jset(aa, "ref", ref);
    sts2_jset(aa, "params", params);
    json_set(aa, "permission", json_string("session:read"));
    json_t *az = sts2_call(c, "authorize", aa);
    if (sts2_is_raise(az)) return az;
    json_free(az);

    json_t *ta = json_object();
    sts2_jset(ta, "authority", authority);
    sts2_jset(ta, "live", json_obj_get(sessions, sid));
    sts2_jset(ta, "ref", ref);
    sts2_jset(ta, "binding", json_obj_get(args, "binding"));
    sts2_jset(ta, "held", json_obj_get(args, "held"));
    sts2_jset(ta, "limit", json_obj_get(args, "limit"));
    sts2_jset(ta, "inline_images", json_obj_get(args, "inline_images"));
    return sts2_call(c, "to_thread", ta);
}

/* PoP: sts2_roster @ gateway/session_transcript_subagent.py:_child.<locals>.roster */
static json_t *sts2_roster(sts2_ctx_t *c, const json_t *args) {
    const json_t *authority = json_obj_get(args, "authority");
    const json_t *ref = json_obj_get(args, "ref");

    json_t *r1 = sts2_args1("authority", authority);
    sts2_jset(r1, "ref", ref);
    json_t *rc = sts2_call(c, "require_current_route", r1);
    if (sts2_is_raise(rc)) return rc;
    json_free(rc);

    json_t *r2 = json_object();
    sts2_jset(r2, "authority", authority);
    sts2_jset(r2, "ref", ref);
    json_set(r2, "include_fallback_evicted", json_bool(true));
    json_t *oo = sts2_call(c, "owner_objects", r2);
    if (sts2_is_raise(oo)) return oo;

    json_t *r3 = sts2_args1("authority", authority);
    sts2_jset(r3, "ref", ref);
    json_t *pt = sts2_call(c, "physical_target", r3);
    if (sts2_is_raise(pt)) { json_free(oo); return pt; }

    /* roster() returns (_owner_objects(...)[1], physical_target(...)) */
    const json_t *records = (oo->type == JSON_ARRAY && json_len(oo) >= 2)
                              ? json_get(oo, 1) : NULL;
    json_t *out = json_array();
    json_append(out, sts2_val(records));
    json_append(out, sts2_val(pt));
    json_free(oo);
    json_free(pt);
    return out;
}

/* PoP: sts2_child @ gateway/session_transcript_subagent.py:_child */
static json_t *sts2_child(sts2_ctx_t *c, const json_t *args) {
    const json_t *authority = json_obj_get(args, "authority");
    const json_t *live = json_obj_get(args, "live");
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *subid = json_obj_get(args, "subagent_id");

    json_t *la = json_object();
    sts2_jset(la, "authority", authority);
    sts2_jset(la, "live", live);
    sts2_jset(la, "ref", ref);
    json_t *lp = sts2_call(c, "locked_projection", la);
    if (sts2_is_raise(lp)) return lp;

    const json_t *records = (lp->type == JSON_ARRAY && json_len(lp) >= 1) ? json_get(lp, 0) : NULL;
    const json_t *target = (lp->type == JSON_ARRAY && json_len(lp) >= 2) ? json_get(lp, 1) : NULL;

    /* record = next((r for r in records if r.get('subagent_id') == subagent_id), None) */
    const json_t *record = NULL;
    if (records && records->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(records); i++) {
            const json_t *r = json_get(records, i);
            if (r && r->type == JSON_OBJECT &&
                sts2_json_eq(json_obj_get(r, "subagent_id"), subid)) { record = r; break; }
        }
    }
    /* child = getattr(record.get('agent'), 'session_id', None) if record else None */
    const json_t *child = NULL;
    if (record) {
        const json_t *agent = json_obj_get(record, "agent");
        if (agent && agent->type == JSON_OBJECT) child = json_obj_get(agent, "session_id");
    }
    if (child && child->type == JSON_STRING && child->str_val && child->str_val[0] != '\0') {
        json_t *out = json_array();
        json_append(out, json_copy(child));
        json_append(out, json_bool(true));
        json_free(lp);
        return out;
    }

    /* Fallthrough: stored child. store = authority_persistence(authority) */
    bool has_record = record != NULL;
    json_t *pa = sts2_args1("authority", authority);
    json_t *store = sts2_call(c, "authority_persistence", pa);
    if (sts2_is_raise(store)) { json_free(lp); return store; }

    /* stored_child_session(store, subagent_id, _compression_lineage(store, target))
     * -- compression_lineage is evaluated BEFORE stored_child_session. */
    json_t *ca = json_object();
    sts2_jset(ca, "store", store);
    sts2_jset(ca, "target", target);
    json_t *lin = sts2_call(c, "compression_lineage", ca);
    if (sts2_is_raise(lin)) { json_free(store); json_free(lp); return lin; }

    json_t *sa = json_object();
    sts2_jset(sa, "store", store);
    sts2_jset(sa, "subagent_id", subid);
    sts2_jset(sa, "lineage", lin);
    json_t *sch = sts2_call(c, "stored_child_session", sa);
    if (sts2_is_raise(sch)) { json_free(lin); json_free(store); json_free(lp); return sch; }

    json_t *out = json_array();
    json_append(out, sch ? sts2_val(sch) : json_null());
    json_append(out, json_bool(has_record));
    json_free(sch);
    json_free(lin);
    json_free(store);
    json_free(lp);
    return out;
}

/* PoP: sts2_read @ gateway/session_transcript_subagent.py:_read */
static json_t *sts2_read(sts2_ctx_t *c, const json_t *args) {
    const json_t *authority = json_obj_get(args, "authority");
    const json_t *live = json_obj_get(args, "live");
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *binding = json_obj_get(args, "binding");
    const json_t *held = json_obj_get(args, "held");
    const json_t *limit = json_obj_get(args, "limit");
    const json_t *inline_images = json_obj_get(args, "inline_images");

    /* subagent_id = binding[1] */
    const json_t *subid = (binding && binding->type == JSON_ARRAY && json_len(binding) >= 2)
                            ? json_get(binding, 1) : NULL;

    json_t *pending = NULL;
    json_t *result = NULL;

    /* with owner_scope(authority): */
    json_t *oa = sts2_args1("authority", authority);
    json_t *cm = sts2_call(c, "owner_scope", oa);
    bool entered = false;
    if (sts2_is_raise(cm)) { pending = cm; cm = NULL; }
    else if (!cm || cm->type != JSON_OBJECT) {
        pending = sts2_cm_error(cm);
        json_free(cm); cm = NULL;
    } else {
        json_t *ea = sts2_args1("cm", cm);
        json_t *er = sts2_call(c, "owner_scope.__enter__", ea);
        if (sts2_is_raise(er)) pending = er;
        else { entered = true; json_free(er); }
    }

    if (!pending) {
        /* child, running = _child(authority, live, ref, subagent_id)  [seamed] */
        json_t *ba = json_object();
        sts2_jset(ba, "authority", authority);
        sts2_jset(ba, "live", live);
        sts2_jset(ba, "ref", ref);
        sts2_jset(ba, "subagent_id", subid);
        json_t *cr = sts2_call(c, "child", ba);
        if (sts2_is_raise(cr)) pending = cr;
        else {
            const json_t *child = (cr->type == JSON_ARRAY && json_len(cr) >= 1) ? json_get(cr, 0) : NULL;
            const json_t *running = (cr->type == JSON_ARRAY && json_len(cr) >= 2) ? json_get(cr, 1) : NULL;
            if (sts2_is_null(child)) {
                /* return unavailable(ref.session_id, subagent_id, running) */
                json_t *ua = json_object();
                sts2_jset(ua, "session_id", json_obj_get(ref, "session_id"));
                sts2_jset(ua, "subagent_id", subid);
                sts2_jset(ua, "running", running);
                result = sts2_unavailable(c, ua);
                json_free(ua);
            } else {
                /* store = authority_persistence(authority) */
                json_t *pa = sts2_args1("authority", authority);
                json_t *store = sts2_call(c, "authority_persistence", pa);
                if (sts2_is_raise(store)) pending = store;
                else {
                    /* store.lineage.delegated_child_lineage_ids(child) */
                    const json_t *lineage = json_obj_get(store, "lineage");
                    json_t *da = json_object();
                    sts2_jset(da, "lineage", lineage);
                    sts2_jset(da, "child", child);
                    json_t *lin = sts2_call(c, "delegated_child_lineage_ids", da);
                    if (sts2_is_raise(lin)) pending = lin;
                    else {
                        /* page = read_transcript(..., profile_home=authority.profile_id,
                         *                        inline_images=inline_images) */
                        json_t *ra = json_object();
                        sts2_jset(ra, "store", store);
                        sts2_jset(ra, "child", child);
                        sts2_jset(ra, "lineage_ids", lin);
                        sts2_jset(ra, "binding", binding);
                        sts2_jset(ra, "held", held);
                        sts2_jset(ra, "limit", limit);
                        sts2_jset(ra, "profile_home", json_obj_get(authority, "profile_id"));
                        sts2_jset(ra, "inline_images", inline_images);
                        json_t *page = sts2_call(c, "read_transcript", ra);
                        if (sts2_is_raise(page)) pending = page;
                        else {
                            /* {'session_id': ref.session_id, **page, 'subagent_id': ...,
                             *  'available': True, 'running': running,
                             *  'child_session_id': child} */
                            json_t *out = json_object();
                            json_set(out, "session_id", sts2_val(json_obj_get(ref, "session_id")));
                            if (page && page->type == JSON_OBJECT)
                                for (size_t i = 0; i < page->c.count; i++)
                                    json_set(out, page->c.keys[i], json_copy(page->c.items[i]));
                            json_set(out, "subagent_id", sts2_val(subid));
                            json_set(out, "available", json_bool(true));
                            json_set(out, "running", sts2_val(running));
                            json_set(out, "child_session_id", sts2_val(child));
                            result = out;
                            json_free(page);
                        }
                        json_free(lin);
                    }
                    json_free(store);
                }
            }
            json_free(cr);
        }
    }

    /* __exit__(exc) fires after the body / before the return expression. */
    if (entered) {
        json_t *xa = json_object();
        if (cm) json_set(xa, "cm", json_copy(cm));
        if (pending) json_set(xa, "exc", json_copy(pending));
        json_t *xr = sts2_call(c, "owner_scope.__exit__", xa);
        if (sts2_is_raise(xr)) {
            json_free(result); result = NULL;
            json_free(pending); pending = xr;
        } else {
            if (sts2_truthy(xr)) { /* suppression of a pending raise: out of corpus */ }
            json_free(xr);
        }
    }
    json_free(cm);

    if (pending) { json_free(result); return pending; }
    return result ? result : json_null();
}
