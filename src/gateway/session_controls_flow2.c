/*
 * session_controls_flow2.c — REMAINING unclaimed FLOW cores of
 * gateway/session_controls.py (hermes-agent): the AuthorityConnection
 * construction / capability projection, the host-scope property, the
 * voice-stop key, the resume-transcript projection, create/describe/info,
 * the session-listing validation + stored/title/live branches, submit's
 * voice-stop chain, mutate's guard/fence/branch-attach ordering, receipt's
 * include_result branch, resolve_unknown/respond/respond_clarify keyset
 * gates, the response-frame entitlement closure, close's teardown ordering
 * and the spawn-notice task registration.
 *
 * Decision sequences only: every live I/O (authority verbs, store reads,
 * title validation, listing collaborators, mutation engine, server-request
 * registry, transport binding, task scheduling, logging) enters through ONE
 * universal seam: sct2_call(c, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording monkeypatch stubs emitting the identical event shape
 * {"name","args","ret"} from the SAME scripts. Refusals mirror the real
 * AuthorityRefusal (reason/field/detail, _one_line bounds included); the
 * AdmissionReceipt / SessionRef / Submission / SessionActor returns mirror
 * the live dataclass asdict shapes.
 *
 * Claimed siblings (ported in session_controls_flow.c) are SEAMED here,
 * never re-ported: self.ping, self._voice_stop_replay, self._typed_voice_stop,
 * self._record_voice_stop, self.submission. Unclaimed-but-unported callees
 * are seamed too: self._create, self.resume, sibling.close. Ported targets
 * compose through real C calls where the live code does (describe->ping is
 * a seam because ping is CLAIMED; submit->submission is a seam likewise).
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and sct2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_session_controls_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   in.state is copied by the harness and mutated in place; out.state echoes
 *   every untouched key. Presence vs null matters (`'title' in params`,
 *   `params.get(k, default)`): json_has tests presence.
 * - An exception is a {"__raise__": cls, "message": str(exc), ...} object
 *   RETURNED as ret (the oracle catches BaseException and projects the same
 *   shape). AuthorityRefusal adds reason/field/detail (field/detail
 *   _one_line-bound); mutate's fence enrichment adds revision /
 *   execution_generation to the refusal descriptor ONLY when the live
 *   handler sets them. BaseException kinds (SystemExit/KeyboardInterrupt/
 *   GeneratorExit) pass through every `except AuthorityRefusal` /
 *   `except MUTATION_GUARD_REFUSALS` site via the full builtin parent table.
 * - in.state carries the connection mirror: authority (aid), transport (id),
 *   actor {subject,profile_id,capabilities[],transport_id}, identity,
 *   operator, native_owner, subscriptions {sid:subid}, replay_cursors
 *   {sid:[epoch,seq]}, reconnects/handed_back (SORTED set arrays),
 *   siblings {pid:{authority,actor}}, notice_tasks (SORTED task-id array;
 *   ABSENT == the _notice_tasks attribute is absent), plus the world:
 *   authorities {aid:{profile_id,instance_id,epoch[,_titled_create_lock]}},
 *   sessions {sid:{subscribers:[subid...],...}}, events {tid:transport}.
 * - in.args carries the per-call arguments (ref {profile_id,session_id},
 *   params, ...). ref args project to exactly {profile_id,session_id}; actor
 *   args to {subject,profile_id,capabilities(SORTED),transport_id}.
 * - __init__ returns the constructed connection's projection (live returns
 *   None; the constructor's product is the observable). host_scoped reads
 *   state.identity. close iterates handed_back in SORTED order: fixtures
 *   keep <=1 handed-back id so the live set's hash order cannot diverge.
 * - The resolve seam (server-request registry) is scripted with an "asks"
 *   list: [{"session_id","events"}...] — the entitlement closure fires
 *   may_settle for self then each sibling (dict order, any() short-circuit)
 *   per ask BEFORE the resolve event records, on both sides.
 * - asyncio.to_thread runs its callable synchronously with no trace of its
 *   own; locks / owner_scope are no-ops; logger calls are silent (not
 *   traced). create_task schedules the publish coroutine without running
 *   it: one create_task seam event, task.add_done_callback is silent.
 * - Scripted seams: a bare JSON array is ONE RESULT PER CALL (last value
 *   does NOT repeat — exhausted is fail-closed); a list-valued result must
 *   be scripted {"__script__": true, "ret": [...]}. {"__script__": true,
 *   "raise": {type,...}} raises; missing/exhausted scripts are a fail-closed
 *   RuntimeError recorded in the trace before propagation.
 * - Fixture domain: ASCII only; integral floats == ints (libjson cannot
 *   tell 1 from 1.0); `is True` gates are JSON_BOOL-only; type(x) is int
 *   gates exclude bools; receipts script exactly the 7 asdict fields.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct sct2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sct2_ctx_t;

/* ── basic helpers ── */
static bool sct2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sct2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool sct2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sct2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sct2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sct2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sct2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static const char *sct2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool sct2_str_eq(const json_t *v, const char *s) {
    const char *t = sct2_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* `is True` / `is False` identity gates: JSON_BOOL only. */
static bool sct2_is_true(const json_t *v) { return sct2_is_bool(v) && v->bool_val; }

/* type(x) is int: integral NUMBER only (bool is NOT int for type()). */
static bool sct2_is_int_strict(const json_t *v) {
    return v && v->type == JSON_NUMBER
        && v->num_val == (double)(long long)v->num_val;
}

static bool sct2_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!sct2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !sct2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}

static json_t *sct2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* str(KeyError(k)) == repr(k) for the string ids the fixtures use. */
static json_t *sct2_keyerror(const char *key) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s'", key ? key : "");
    return sct2_err("KeyError", buf);
}

static char *sct2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python repr() for ASCII fixture strings (quote choice + escapes). */
static char *sct2_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *r = malloc(cap);
    if (!r) return sct2_strdup("''");
    size_t n = 0;
    r[n++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') { r[n++] = '\\'; r[n++] = '\\'; }
        else if (c == (unsigned char)q) { r[n++] = '\\'; r[n++] = (char)c; }
        else if (c == '\n') { r[n++] = '\\'; r[n++] = 'n'; }
        else if (c == '\r') { r[n++] = '\\'; r[n++] = 'r'; }
        else if (c == '\t') { r[n++] = '\\'; r[n++] = 't'; }
        else if (c < 0x20 || c == 0x7f)
            n += (size_t)snprintf(r + n, cap - n, "\\x%02x", c);
        else r[n++] = (char)c;
    }
    r[n++] = q;
    r[n] = '\0';
    return r;
}

/*
 * persistence.core.errors._one_line: whitespace collapses to single spaces
 * (str.split()/join), other non-printables drop, bounded; empty -> None.
 * ASCII fixture strings: bytes >= 0x80 are treated as printable.
 */
static char *sct2_one_line(const char *value, size_t limit) {
    if (!value) return NULL;
    size_t n = strlen(value);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    size_t k = 0;
    bool pending_space = false, started = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (isspace(ch)) { if (started) pending_space = true; continue; }
        if (pending_space) { tmp[k++] = ' '; pending_space = false; }
        started = true;
        tmp[k++] = (char)ch;
    }
    tmp[k] = '\0';
    size_t w = 0;
    for (size_t i = 0; i < k; i++) {
        unsigned char ch = (unsigned char)tmp[i];
        if (ch < 0x20 || (ch >= 0x7f && ch <= 0x9f)) continue;
        tmp[w++] = tmp[i];
    }
    tmp[w] = '\0';
    if (w == 0) { free(tmp); return NULL; }
    if (w > limit) w = limit;
    char *out = malloc(w + 1);
    if (!out) { free(tmp); return NULL; }
    memcpy(out, tmp, w);
    out[w] = '\0';
    free(tmp);
    return out;
}

/* The real AuthorityRefusal projection: reason/field/detail + message=reason. */
static json_t *sct2_refusal(const char *reason, const char *field, const char *detail) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "AuthorityRefusal");
    json_set_hdr(e, "message", reason ? reason : "");
    json_set_hdr(e, "reason", reason ? reason : "");
    char *f = sct2_one_line(field, 80);
    json_set(e, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = sct2_one_line(detail, 160);
    json_set(e, "detail", d ? json_string(d) : json_null());
    free(d);
    return e;
}

/* ── exception-parent table: FULL builtin hierarchy + the persistence
 * taxonomy classes the ported catch sites name. Single-parent walk suffices
 * for every except clause ported here (AuthorityRefusal /
 * MUTATION_GUARD_REFUSALS); the BaseException trio stays DIRECT children of
 * BaseException so they pass through every Exception-level catch. ── */
typedef struct { const char *name; const char *parent; } sct2_exc_ent_t;
static const sct2_exc_ent_t SCT2_EXC_PARENT[] = {
    {"BaseException", "object"}, {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"}, {"ExceptionGroup", "Exception"},
    {"GeneratorExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"}, {"CancelledError", "BaseException"},
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BufferError", "Exception"},
    {"EOFError", "Exception"}, {"EnvironmentError", "OSError"},
    {"IOError", "OSError"}, {"ImportError", "Exception"},
    {"LookupError", "Exception"}, {"MemoryError", "Exception"},
    {"NameError", "Exception"}, {"OSError", "Exception"},
    {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"}, {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
    {"TypeError", "Exception"}, {"ValueError", "Exception"},
    {"Warning", "Exception"}, {"WindowsError", "OSError"},
    {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"}, {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"}, {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"}, {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"}, {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"}, {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"}, {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
    {"BytesWarning", "Warning"}, {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"}, {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"}, {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"}, {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    /* persistence.core.errors taxonomy (catch-site classes) */
    {"PersistenceError", "Exception"}, {"AuthorityRefusal", "PersistenceError"},
    {"Conflict", "PersistenceError"}, {"_SessionConflict", "Conflict"},
    {"CompressionBusy", "_SessionConflict"}, {"CompressionInProgress", "CompressionBusy"},
    {"TurnLeaseLost", "_SessionConflict"}, {"TransientStoreError", "PersistenceError"},
    {"PermanentStoreError", "PersistenceError"}, {"CorruptStoreError", "PermanentStoreError"},
    {"StoreUnavailable", "PersistenceError"}, {"StoreReplaced", "PersistenceError"},
};

static const char *sct2_exc_parent(const char *kind) {
    if (!kind) return NULL;
    for (size_t i = 0; i < sizeof(SCT2_EXC_PARENT) / sizeof(SCT2_EXC_PARENT[0]); i++)
        if (strcmp(SCT2_EXC_PARENT[i].name, kind) == 0)
            return SCT2_EXC_PARENT[i].parent;
    return NULL;
}

/* isinstance(exc, names) via the parent walk (unknown kinds: not caught). */
static bool sct2_caught_by(const char *kind, const char *const *names, size_t n) {
    const char *cur = kind;
    for (int depth = 0; depth < 32 && cur; depth++) {
        for (size_t i = 0; i < n; i++)
            if (strcmp(cur, names[i]) == 0) return true;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0)
            return false;
        cur = sct2_exc_parent(cur);
    }
    return false;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *sct2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    if (strcmp(type, "AuthorityRefusal") == 0) {
        return sct2_refusal(json_get_str(spec, "reason", ""),
                            sct2_str(json_obj_get(spec, "field")),
                            sct2_str(json_obj_get(spec, "detail")));
    }
    if (strcmp(type, "TurnLeaseLost") == 0 || strcmp(type, "CompressionInProgress") == 0) {
        /* _SessionConflict.__init__: str(exc) is the formatted default_message
         * with {session_id!r}. */
        const char *sid = json_get_str(spec, "session_id", "");
        char *rep = sct2_repr_str(sid);
        char buf[512];
        if (strcmp(type, "TurnLeaseLost") == 0)
            snprintf(buf, sizeof buf,
                     "Session turn lease lost; refusing transcript write for %s", rep);
        else
            snprintf(buf, sizeof buf,
                     "Session %s is being compressed by another writer", rep);
        free(rep);
        return sct2_err(type, buf);
    }
    return sct2_err(type, json_get_str(spec, "message", ""));
}

/* ── seam dispatch ── */
static json_t *sct2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return sct2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* The CURRENT script entry for a name without consuming it (the resolve
 * seam's "asks" peek; mirrors the oracle resolve double's peek). */
static const json_t *sct2_peek(const sct2_ctx_t *c, const char *name) {
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        return json_get(entry, idx);
    }
    return entry;
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *sct2_call(sct2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = sct2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = sct2_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = sct2_entry_result(json_get(entry, idx));
        }
    } else {
        res = sct2_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* ── state helpers ── */
static json_t *sct2_sub(sct2_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *sct2_sub_ensure(sct2_ctx_t *c, const char *key) {
    json_t *v = sct2_sub(c, key);
    if (!v) { v = json_object(); json_set(c->state, key, v); }
    return v;
}

/* set-valued state (reconnects, handed_back, notice_tasks) is a SORTED ARRAY. */
static json_t *sct2_set_ensure(sct2_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    if (!v || v->type != JSON_ARRAY) { v = json_array(); json_set(c->state, key, v); }
    return v;
}

static bool sct2_set_has(const json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (sct2_str_eq(json_get(arr, i), id)) return true;
    return false;
}

/* set.add: insert keeping sorted order (no dup). Rebuilds in place. */
static void sct2_set_add(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return;
    if (sct2_set_has(arr, id)) return;
    size_t n = json_len(arr), pos = n;
    for (size_t i = 0; i < n; i++) {
        const char *s = sct2_str(json_get(arr, i));
        if (s && strcmp(s, id) > 0) { pos = i; break; }
    }
    json_t *copy = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i == pos) json_append(copy, json_string(id));
        json_append(copy, json_copy(json_get(arr, i)));
    }
    if (pos == n) json_append(copy, json_string(id));
    for (size_t i = 0; i < arr->c.count; i++) {
        json_free(arr->c.items[i]);
        free(arr->c.keys ? arr->c.keys[i] : NULL);
    }
    free(arr->c.items); free(arr->c.keys);
    arr->c.items = copy->c.items; arr->c.keys = copy->c.keys; arr->c.count = copy->c.count;
    copy->c.items = NULL; copy->c.keys = NULL; copy->c.count = 0;
    json_free(copy);
}

static bool sct2_dict_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

/* dict.pop(key, None): owned copy out, key removed. */
static json_t *sct2_dict_pop(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(obj, key);
    if (!v) return NULL;
    json_t *copy = json_copy(v);
    json_obj_del(obj, key);
    return copy;
}

static json_t *sct2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* Attribute read on a SimpleNamespace double: missing -> the py3.14
 * module-qualified AttributeError; a None receiver -> 'NoneType'. */
static const json_t *sct2_attr(const json_t *obj, const char *name, json_t **err) {
    *err = NULL;
    if (!obj || obj->type == JSON_NULL) {
        char buf[256];
        snprintf(buf, sizeof buf, "'NoneType' object has no attribute '%s'", name);
        *err = sct2_err("AttributeError", buf);
        return NULL;
    }
    json_t *v = (obj->type == JSON_OBJECT) ? json_obj_get(obj, name) : NULL;
    if (!v) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "'types.SimpleNamespace' object has no attribute '%s'", name);
        *err = sct2_err("AttributeError", buf);
        return NULL;
    }
    return v;
}

/* ── projection helpers (shared arg shapes) ── */
static json_t *sct2_sorted_strs(const json_t *arr) {
    json_t *out = json_array();
    if (!sct2_is_arr(arr)) return out;
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        const char *s = sct2_str(json_get(arr, i));
        if (!s) continue;
        size_t pos = json_len(out);
        for (size_t j = 0; j < json_len(out); j++) {
            const char *t = sct2_str(json_get(out, j));
            if (t && strcmp(t, s) > 0) { pos = j; break; }
        }
        json_t *node = json_string(s);
        if (pos == json_len(out)) { json_append(out, node); }
        else {
            /* insert at pos: shift tail */
            json_append(out, json_copy(json_get(out, json_len(out) - 1)));
            for (size_t j = json_len(out) - 1; j > pos; j--) {
                json_free(out->c.items[j]);
                out->c.items[j] = out->c.items[j - 1];
            }
            out->c.items[pos] = node;
        }
    }
    return out;
}

/* actor arg: {subject, profile_id, capabilities(SORTED), transport_id}. */
static json_t *sct2_actor_arg(const json_t *actor) {
    json_t *a = json_object();
    json_set_hdr(a, "subject", json_get_str(actor, "subject", ""));
    json_set_hdr(a, "profile_id", json_get_str(actor, "profile_id", ""));
    json_set(a, "capabilities", sct2_sorted_strs(json_obj_get(actor, "capabilities")));
    json_set_hdr(a, "transport_id", json_get_str(actor, "transport_id", ""));
    return a;
}

/* ref arg: exactly {profile_id, session_id}. */
static json_t *sct2_ref_arg(const json_t *ref) {
    json_t *r = json_object();
    json_set_hdr(r, "profile_id", json_get_str(ref, "profile_id", ""));
    json_set_hdr(r, "session_id", json_get_str(ref, "session_id", ""));
    return r;
}

static const char *sct2_aid(const sct2_ctx_t *c) {
    return json_get_str(c->state, "authority", "");
}

static json_t *sct2_auth(sct2_ctx_t *c) {
    return json_obj_get(sct2_sub(c, "authorities"), sct2_aid(c));
}

static bool sct2_keyset_eq(const json_t *params, const char *const *req, size_t n) {
    if (!sct2_is_obj(params) || params->c.count != n) return false;
    for (size_t i = 0; i < n; i++)
        if (!json_has(params, req[i])) return false;
    return true;
}

static bool sct2_str_in(const char *s, const char *const *set, size_t n) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++)
        if (strcmp(set[i], s) == 0) return true;
    return false;
}

/* ── 1. __init__ ─────────────────────────────────────────────────────────── */
/* PoP: sct2_init @ gateway/session_controls.py:__init__ */
static json_t *sct2_init(sct2_ctx_t *c, const json_t *args) {
    const char *aid = json_get_str(args, "authority", "");
    const char *tid = json_get_str(args, "transport", "");
    const json_t *identity = json_obj_get(args, "identity");
    const json_t *operator = json_obj_get(args, "operator");
    const json_t *auth = json_obj_get(sct2_sub(c, "authorities"), aid);
    static const char *BASE_CAPS[] = {
        "session:read", "session:submit", "session:control",
        "session:approve", "session:respond",
    };
    json_t *caps = json_array();
    for (size_t i = 0; i < 5; i++) json_append(caps, json_string(BASE_CAPS[i]));
    if (sct2_truthy(identity)) sct2_set_add(caps, "session:create");
    if (sct2_dict_has(identity, "capabilities")) {
        const json_t *ic = json_obj_get(identity, "capabilities");
        if (!sct2_is_arr(ic)) {
            /* frozenset(non-iterable) raises TypeError at the call site. */
            json_free(caps);
            return sct2_err("TypeError", "argument of type 'NoneType' is not iterable");
        }
        json_free(caps);
        caps = json_copy(ic);
    }
    /* capabilities -= {'session:operator'} */
    for (size_t i = 0; i < json_len(caps); i++) {
        if (sct2_str_eq(json_get(caps, i), "session:operator")) {
            json_free(caps->c.items[i]);
            for (size_t j = i; j + 1 < caps->c.count; j++)
                caps->c.items[j] = caps->c.items[j + 1];
            caps->c.count--;
            break;
        }
    }
    if (sct2_is_true(operator)) sct2_set_add(caps, "session:operator");
    const json_t *user_id = json_obj_get(identity, "user_id");
    const json_t *auth_inst = json_obj_get(auth, "instance_id");
    const json_t *inst = sct2_dict_has(identity, "instance_id")
        ? json_obj_get(identity, "instance_id") : auth_inst;
    if (!sct2_truthy(user_id) || !sct2_json_eq(inst, auth_inst)) {
        json_free(caps);
        caps = json_array();
    }
    json_t *sorted_caps = sct2_sorted_strs(caps);
    json_free(caps);
    /* self.actor = SessionActor(principal_subject(identity),
     *     identity.get('profile_id', authority.profile_id), capabilities,
     *     uuid.uuid4().hex) */
    json_t *pa = json_object();
    json_set(pa, "identity", sct2_copy_or_null(identity));
    json_t *subject = sct2_call(c, "principal_subject", pa);
    if (sct2_is_raise(subject)) { json_free(sorted_caps); return subject; }
    const json_t *pid = sct2_dict_has(identity, "profile_id")
        ? json_obj_get(identity, "profile_id") : json_obj_get(auth, "profile_id");
    json_t *hex = sct2_call(c, "uuid4_hex", json_object());
    if (sct2_is_raise(hex)) { json_free(sorted_caps); json_free(subject); return hex; }
    json_t *actor = json_object();
    json_set(actor, "subject", subject);   /* adopts */
    json_set(actor, "profile_id", sct2_copy_or_null(pid));
    json_set(actor, "capabilities", sorted_caps);   /* adopts */
    json_set(actor, "transport_id", json_copy(hex));
    json_t *ba = json_object();
    json_set_hdr(ba, "authority", aid);
    json_set(ba, "actor", json_copy(actor));
    json_set(ba, "identity", sct2_copy_or_null(identity));
    json_t *bind = sct2_call(c, "bind_native_transport", ba);
    if (sct2_is_raise(bind)) { json_free(actor); json_free(hex); return bind; }
    json_free(bind);
    bool native_owner = sct2_is_true(json_obj_get(identity, "native_bootstrap"));
    /* authority.events[self.actor.transport_id] = transport */
    json_t *events = sct2_sub_ensure(c, "events");
    json_set(events, sct2_str(hex) ? sct2_str(hex) : "", json_string(tid));
    json_free(hex);
    /* The constructor's product is the observable (live returns None). */
    json_t *out = json_object();
    json_set_hdr(out, "authority", aid);
    json_set_hdr(out, "transport", tid);
    json_set(out, "actor", actor);   /* adopts */
    json_set(out, "native_owner", json_bool(native_owner));
    json_set(out, "identity", sct2_copy_or_null(identity));
    json_set(out, "operator", operator ? json_copy(operator) : json_bool(false));
    json_set(out, "subscriptions", json_object());
    json_set(out, "replay_cursors", json_object());
    json_set(out, "reconnects", json_array());
    json_set(out, "handed_back", json_array());
    json_set(out, "siblings", json_object());
    return out;
}

/* ── 2. host_scoped (property) ───────────────────────────────────────────── */
/* PoP: sct2_host_scoped @ gateway/session_controls.py:host_scoped */
static json_t *sct2_host_scoped(sct2_ctx_t *c, const json_t *args) {
    (void)args;
    const json_t *identity = json_obj_get(c->state, "identity");
    const json_t *scope = json_obj_get(identity, "profile_scope");
    return json_bool(sct2_str_eq(scope, "host"));
}

/* ── 3. _voice_stop_key ──────────────────────────────────────────────────── */
/* PoP: sct2_voice_stop_key @ gateway/session_controls.py:_voice_stop_key */
static json_t *sct2_voice_stop_key(sct2_ctx_t *c, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *rid = json_obj_get(params, "submission_id");
    if (!sct2_truthy(rid)) rid = json_obj_get(params, "input_id");
    if (!sct2_is_str(rid) || !sct2_truthy(rid)) return json_null();
    const json_t *actor = json_obj_get(c->state, "actor");
    json_t *key = json_array();
    json_append(key, json_string(json_get_str(actor, "subject", "")));
    json_append(key, json_string(json_get_str(ref, "profile_id", "")));
    json_append(key, json_string(json_get_str(ref, "session_id", "")));
    json_append(key, json_copy(rid));
    return key;
}

/* ── 4. _resume_transcript ───────────────────────────────────────────────── */
/* PoP: sct2_resume_transcript @ gateway/session_controls.py:_resume_transcript */
static json_t *sct2_resume_transcript(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *snapshot = json_obj_get(args, "snapshot");
    const json_t *history = json_obj_get(args, "history");
    const json_t *omit = json_obj_get(args, "omit");
    if (!sct2_truthy(omit)) {
        json_t *ta = json_object();
        json_set(ta, "history", sct2_copy_or_null(history));
        json_t *todo = sct2_call(c, "_todo_state_from_history", ta);
        if (sct2_is_raise(todo)) return todo;
        json_t *out = json_object();
        json_set(out, "messages", sct2_copy_or_null(history));
        json_set(out, "message_count", json_number((double)json_len(history)));
        json_set(out, "todo_state", todo);   /* adopts */
        return out;
    }
    /* asyncio.to_thread(authority.unless_deleted, ref, partial(
     *     omitted_history_state, store, snapshot.transcript_target)) */
    json_t *ua = json_object();
    json_set(ua, "ref", sct2_ref_arg(ref));
    json_set(ua, "transcript_target",
             sct2_copy_or_null(json_obj_get(snapshot, "transcript_target")));
    json_t *pair = sct2_call(c, "authority.unless_deleted", ua);
    if (sct2_is_raise(pair)) return pair;
    if (!sct2_is_arr(pair) || json_len(pair) != 2) {
        size_t got = sct2_is_arr(pair) ? json_len(pair) : 0;
        bool is_arr = sct2_is_arr(pair);
        char buf[128];
        if (!is_arr)
            snprintf(buf, sizeof buf, "cannot unpack non-iterable NoneType object");
        else if (got < 2)
            snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", got);
        else
            snprintf(buf, sizeof buf, "too many values to unpack (expected 2)");
        json_free(pair);
        return sct2_err(is_arr ? "ValueError" : "TypeError", buf);
    }
    json_t *out = json_object();
    json_set(out, "messages", json_array());
    json_set(out, "messages_omitted", json_bool(true));
    json_set(out, "message_count", json_copy(json_get(pair, 1)));
    json_set(out, "todo_state", json_copy(json_get(pair, 0)));
    json_free(pair);
    return out;
}

/* ── 5. create ───────────────────────────────────────────────────────────── */
/* PoP: sct2_create @ gateway/session_controls.py:create */
static json_t *sct2_create(sct2_ctx_t *c, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    json_t *p = sct2_is_obj(params) ? json_copy(params) : json_object();
    json_t *title = NULL;   /* NULL == None */
    if (json_has(p, "title")) {
        json_t *raw = sct2_dict_pop(p, "title");
        json_t *va = json_object();
        json_set(va, "title", raw);   /* adopts */
        title = sct2_call(c, "validate_title", va);
        if (sct2_is_raise(title)) { json_free(p); return title; }
    }
    if (!title || sct2_is_null(title)) {
        if (title) json_free(title);
        json_t *ca = json_object();
        json_set(ca, "params", p);   /* adopts */
        json_set(ca, "title", json_null());
        return sct2_call(c, "self._create", ca);
    }
    /* lock = getattr(authority, '_titled_create_lock', None); if None: create
     * (asyncio.Lock() is a silent no-op double; the attribute IS set). */
    json_t *auth = sct2_auth(c);
    if (auth && !json_has(auth, "_titled_create_lock"))
        json_set(auth, "_titled_create_lock", json_bool(true));
    json_t *ca = json_object();
    json_set(ca, "params", p);   /* adopts */
    json_set(ca, "title", title);   /* adopts */
    return sct2_call(c, "self._create", ca);
}

/* ── 6. describe ─────────────────────────────────────────────────────────── */
/* PoP: sct2_describe @ gateway/session_controls.py:describe */
static json_t *sct2_describe(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    json_t *pa = json_object();
    json_set(pa, "ref", sct2_ref_arg(ref));
    json_set(pa, "params", sct2_copy_or_null(params));
    json_t *pong = sct2_call(c, "self.ping", pa);
    if (sct2_is_raise(pong)) return pong;
    json_free(pong);
    json_t *skin = sct2_call(c, "resolved_skin", json_object());
    if (sct2_is_raise(skin)) return skin;
    const json_t *auth = sct2_auth(c);
    static const char *CAPS[] = {
        "durable-admission-v1", "event-replay-v1", "local-cli-create-v1",
        "acp-editor-policy-v1", "acp-session-mcp-v1", "clarify-batch-v1",
        "plugin-rpc-v1", "session-client-v1", "session-transcript-v1",
        "session-list-picker-v1", "session-list-cursor-v1",
    };
    static const char *SOURCES[] = { "acp", "cli", "gui", "oneshot", "tool", "tui" };
    static const char *PARAMETERS[] = {
        "accept_hooks", "api_key", "base_url", "checkpoints", "client", "client_env",
        "cwd", "cwd_explicit", "editor", "follow_profile_config", "ignore_rules",
        "ignore_user_config", "max_turns", "model", "pass_session_id", "provider",
        "reasoning", "request_id", "row_cwd", "run_budget", "safe_mode", "service_tier",
        "skills", "source", "title", "toolsets", "verbose", "yolo",
    };
    json_t *out = json_object();
    json_set(out, "instance_id", sct2_copy_or_null(json_obj_get(auth, "instance_id")));
    json_set(out, "profile_id", sct2_copy_or_null(json_obj_get(auth, "profile_id")));
    json_set(out, "authority_epoch", sct2_copy_or_null(json_obj_get(auth, "epoch")));
    json_t *caps = json_array();
    for (size_t i = 0; i < sizeof(CAPS) / sizeof(CAPS[0]); i++)
        json_append(caps, json_string(CAPS[i]));
    json_set(out, "capabilities", caps);
    json_t *sc = json_object();
    json_t *sources = json_array();
    for (size_t i = 0; i < sizeof(SOURCES) / sizeof(SOURCES[0]); i++)
        json_append(sources, json_string(SOURCES[i]));
    json_set(sc, "sources", sources);
    json_t *parameters = json_array();
    for (size_t i = 0; i < sizeof(PARAMETERS) / sizeof(PARAMETERS[0]); i++)
        json_append(parameters, json_string(PARAMETERS[i]));
    json_set(sc, "parameters", parameters);
    json_set(out, "session_create", sc);
    json_set(out, "skin", skin);   /* adopts */
    return out;
}

/* ── 7. info ─────────────────────────────────────────────────────────────── */
/* PoP: sct2_info @ gateway/session_controls.py:info */
static json_t *sct2_info(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    static const char *REQ[] = { "session_id" };
    if (!sct2_keyset_eq(params, REQ, 1))
        return sct2_refusal("invalid_params", NULL, NULL);
    json_t *ra = json_object();
    json_set(ra, "actor", sct2_actor_arg(json_obj_get(c->state, "actor")));
    json_set(ra, "ref", sct2_ref_arg(ref));
    json_t *r = sct2_call(c, "authority.resolve", ra);
    if (sct2_is_raise(r)) return r;
    json_free(r);
    json_t *sa = json_object();
    json_set_hdr(sa, "authority", sct2_aid(c));
    json_set(sa, "ref", sct2_ref_arg(ref));
    return sct2_call(c, "snapshot_info", sa);
}

/* ── 8. list_sessions ────────────────────────────────────────────────────── */
/* PoP: sct2_list_sessions @ gateway/session_controls.py:list_sessions */
static json_t *sct2_list_sessions(sct2_ctx_t *c, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    const json_t *actor = json_obj_get(c->state, "actor");
    const json_t *caps = json_obj_get(actor, "capabilities");
    if (!sct2_set_has(caps, "session:read"))
        return sct2_refusal("permission_denied", NULL, NULL);
    /* limit, workspace, picker = params.get('limit', 200),
     * params.get('workspace'), params.get('picker', False) */
    const json_t *limitv = json_has(params, "limit") ? json_obj_get(params, "limit") : NULL;
    double limit = 200;
    bool limit_ok;
    if (!limitv) { limit_ok = true; }
    else if (sct2_is_int_strict(limitv)) { limit = limitv->num_val; limit_ok = true; }
    else { limit_ok = false; }
    const json_t *workspace = json_obj_get(params, "workspace");
    const json_t *pickerv = json_has(params, "picker") ? json_obj_get(params, "picker") : NULL;
    bool picker = sct2_is_true(pickerv);
    bool picker_ok = !pickerv || sct2_is_bool(pickerv);
    static const char *ALLOWED[] = {
        "limit", "title", "include_hidden", "scope", "workspace", "picker", "query", "cursor",
    };
    bool bad_keys = false;
    if (sct2_is_obj(params)) {
        for (size_t i = 0; i < params->c.count; i++)
            if (!sct2_str_in(params->c.keys[i], ALLOWED, 8)) { bad_keys = true; break; }
    } else {
        bad_keys = true;   /* set(params) on a non-mapping raises; fail closed */
    }
    bool ws_ok = sct2_is_null(workspace) || (sct2_is_str(workspace) && sct2_truthy(workspace));
    if (bad_keys || !limit_ok || !(limit >= 1 && limit <= 200) || !ws_ok || !picker_ok)
        return sct2_refusal("invalid_params", NULL, NULL);
    const json_t *scopev = json_has(params, "scope") ? json_obj_get(params, "scope") : NULL;
    bool scope_ok = !scopev ? true : (sct2_str_eq(scopev, "live") || sct2_str_eq(scopev, "stored"));
    bool ws_not_none = json_has(params, "workspace") && !sct2_is_null(workspace);
    if (!scope_ok || (json_has(params, "title")
                      && (json_has(params, "scope") || ws_not_none || picker)))
        return sct2_refusal("invalid_params", NULL, NULL);
    json_t *cpa = json_object();
    json_set(cpa, "params", sct2_copy_or_null(params));
    json_t *cp = sct2_call(c, "check_page_params", cpa);
    if (sct2_is_raise(cp)) return cp;
    json_free(cp);
    const char *aid = sct2_aid(c);
    if (sct2_str_eq(scopev, "stored")) {
        json_t *sa = json_object();
        json_set_hdr(sa, "authority", aid);
        json_set(sa, "actor", sct2_actor_arg(actor));
        json_set(sa, "limit", json_number(limit));
        const json_t *ih = json_obj_get(params, "include_hidden");
        json_set(sa, "include_hidden", json_bool(sct2_truthy(ih)));
        json_set(sa, "workspace", sct2_copy_or_null(workspace));
        json_set(sa, "picker", json_bool(picker));
        json_set(sa, "query", sct2_copy_or_null(json_obj_get(params, "query")));
        json_set(sa, "cursor", sct2_copy_or_null(json_obj_get(params, "cursor")));
        return sct2_call(c, "stored_picker_sessions", sa);
    }
    if (json_has(params, "title")) {
        json_t *ta = json_object();
        json_set_hdr(ta, "authority", aid);
        json_set(ta, "actor", sct2_actor_arg(actor));
        json_set(ta, "title", json_copy(json_obj_get(params, "title")));
        json_set(ta, "missing_ok", json_bool(true));
        json_t *found = sct2_call(c, "resolve_titled_session", ta);
        if (sct2_is_raise(found)) return found;
        if (sct2_is_null(found)) {
            json_free(found);
            json_t *out = json_object();
            json_set(out, "sessions", json_array());
            json_set_hdr(out, "scope", "stored");
            return out;
        }
        json_t *err = NULL;
        const json_t *fsidv = sct2_attr(found, "session_id", &err);
        if (err) { json_free(found); return err; }
        const char *fsid = sct2_str(fsidv) ? sct2_str(fsidv) : "";
        json_t *ga = json_object();
        json_set_hdr(ga, "session_id", fsid);
        json_t *row = sct2_call(c, "store.sessions.get", ga);
        if (sct2_is_raise(row)) { json_free(found); return row; }
        json_t *rta = json_object();
        json_set(rta, "title", json_copy(json_obj_get(params, "title")));
        json_t *rt = sct2_call(c, "store.sessions.resolve_by_title", rta);
        if (sct2_is_raise(rt)) { json_free(found); json_free(row); return rt; }
        json_t *ga2 = json_object();
        if (sct2_truthy(rt)) json_set(ga2, "session_id", json_copy(rt));
        else json_set_hdr(ga2, "session_id", fsid);
        json_free(rt);
        json_t *root0 = sct2_call(c, "store.sessions.get", ga2);
        if (sct2_is_raise(root0)) { json_free(found); json_free(row); return root0; }
        json_t *root;
        if (sct2_truthy(root0)) { root = root0; json_free(row); }
        else { root = row; json_free(root0); }
        /* The reply dict, in live literal evaluation order. */
        json_t *sess = json_object();
        json_set_hdr(sess, "session_id", fsid);
        const json_t *rid = sct2_is_null(root) ? NULL : sct2_attr(root, "session_id", &err);
        if (err) { json_free(found); json_free(root); json_free(sess); return err; }
        if (!sct2_truthy(rid)) rid = fsidv;
        json_set(sess, "id", json_copy(rid));
        json_set_hdr(sess, "resolved_id", fsid);
        const json_t *rtitle = sct2_is_null(row) ? NULL : sct2_attr(row, "title", &err);
        if (err) { json_free(found); json_free(root); json_free(sess); return err; }
        json_set(sess, "title", sct2_truthy(rtitle) ? json_copy(rtitle) : json_string(""));
        const json_t *rotitle = sct2_is_null(root) ? NULL : sct2_attr(root, "title", &err);
        if (err) { json_free(found); json_free(root); json_free(sess); return err; }
        json_set(sess, "root_title", sct2_truthy(rotitle) ? json_copy(rotitle) : json_string(""));
        const json_t *rsource = sct2_is_null(row) ? NULL : sct2_attr(row, "source", &err);
        if (err) { json_free(found); json_free(root); json_free(sess); return err; }
        json_set(sess, "source", sct2_copy_or_null(rsource));
        const json_t *rstarted = sct2_is_null(row) ? NULL : sct2_attr(row, "started_at", &err);
        if (err) { json_free(found); json_free(root); json_free(sess); return err; }
        json_set(sess, "started_at", sct2_copy_or_null(rstarted));
        const json_t *rmc = sct2_is_null(row) ? NULL : sct2_attr(row, "message_count", &err);
        if (err) { json_free(found); json_free(root); json_free(sess); return err; }
        json_set(sess, "message_count", rmc ? json_copy(rmc) : json_number(0));
        json_set(sess, "running", json_bool(sct2_dict_has(sct2_sub(c, "sessions"), fsid)));
        json_free(found);
        json_free(root);
        json_t *out = json_object();
        json_t *arr = json_array();
        json_append(arr, sess);
        json_set(out, "sessions", arr);
        json_set_hdr(out, "scope", "stored");
        return out;
    }
    /* live branch */
    json_t *la = json_object();
    json_set_hdr(la, "authority", aid);
    json_set(la, "actor", sct2_actor_arg(actor));
    json_t *entries = sct2_call(c, "live_entries", la);
    if (sct2_is_raise(entries)) return entries;
    static const char *INTERNAL[] = { "kanban", "tool", "oneshot" };
    bool include_hidden = sct2_truthy(json_obj_get(params, "include_hidden"));
    json_t *sessions = json_array();
    if (sct2_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const json_t *entry = json_get(entries, i);
            json_t *err = NULL;
            const json_t *esid = sct2_attr(entry, "session_id", &err);
            if (err) { json_free(entries); json_free(sessions); return err; }
            const json_t *row = sct2_attr(entry, "row", &err);
            if (err) { json_free(entries); json_free(sessions); return err; }
            const json_t *closed = sct2_attr(entry, "closed", &err);
            if (err) { json_free(entries); json_free(sessions); return err; }
            const json_t *source = json_obj_get(row, "source");
            if (sct2_str_in(sct2_str(source), INTERNAL, 3) || sct2_truthy(closed)
                    || (!include_hidden && (sct2_truthy(json_obj_get(row, "hidden"))
                                            || sct2_truthy(json_obj_get(row, "archived")))))
                continue;
            const json_t *handle = sct2_attr(entry, "handle", &err);
            if (err) { json_free(entries); json_free(sessions); return err; }
            const json_t *estate = sct2_attr(handle, "execution_state", &err);
            if (err) { json_free(entries); json_free(sessions); return err; }
            json_t *s = json_object();
            json_set(s, "session_id", json_copy(esid));
            json_set(s, "id", json_copy(esid));
            const json_t *title = json_obj_get(row, "title");
            json_set(s, "title", sct2_truthy(title) ? json_copy(title) : json_string(""));
            json_set(s, "source", sct2_copy_or_null(source));
            json_set(s, "started_at", sct2_copy_or_null(json_obj_get(row, "started_at")));
            const json_t *mc = json_obj_get(row, "message_count");
            json_set(s, "message_count", mc ? json_copy(mc) : json_number(0));
            json_set(s, "model", sct2_copy_or_null(json_obj_get(row, "model")));
            json_set(s, "running", json_bool(sct2_str_eq(estate, "running")));
            json_append(sessions, s);
        }
    }
    json_free(entries);
    /* sessions.sort(key=lambda row: row['started_at'] or 0, reverse=True) —
     * STABLE: equal keys keep their original order (insertion sort). */
    size_t n = json_len(sessions);
    for (size_t i = 1; i < n; i++) {
        json_t *cur = sessions->c.items[i];
        const json_t *sa = json_obj_get(cur, "started_at");
        double key = (sa && sa->type == JSON_NUMBER && sct2_truthy(sa)) ? sa->num_val : 0;
        size_t j = i;
        while (j > 0) {
            const json_t *prev = sessions->c.items[j - 1];
            const json_t *pa = json_obj_get(prev, "started_at");
            double pkey = (pa && pa->type == JSON_NUMBER && sct2_truthy(pa)) ? pa->num_val : 0;
            if (pkey < key) { sessions->c.items[j] = sessions->c.items[j - 1]; j--; }
            else break;
        }
        sessions->c.items[j] = cur;
    }
    json_t *lpa = json_object();
    json_set_hdr(lpa, "authority", aid);
    json_set(lpa, "sessions", sessions);   /* adopts */
    json_set(lpa, "limit", json_number(limit));
    json_set(lpa, "workspace", sct2_copy_or_null(workspace));
    json_set(lpa, "picker", json_bool(picker));
    json_t *page = sct2_call(c, "live_picker_page", lpa);
    if (sct2_is_raise(page)) return page;
    json_t *out = json_object();
    json_set(out, "sessions", page);   /* adopts */
    json_set_hdr(out, "scope", "live");
    return out;
}

/* ── 9. submit ───────────────────────────────────────────────────────────── */
/* PoP: sct2_submit @ gateway/session_controls.py:submit */
static json_t *sct2_submit(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    /* if self._voice_stop_replay(ref, params) or await self._typed_voice_stop(...):
     * the `or` short-circuits — a truthy replay skips the typed seam. */
    json_t *va = json_object();
    json_set(va, "ref", sct2_ref_arg(ref));
    json_set(va, "params", sct2_copy_or_null(params));
    json_t *replay = sct2_call(c, "self._voice_stop_replay", va);
    if (sct2_is_raise(replay)) return replay;
    bool stop = sct2_truthy(replay);
    json_free(replay);
    if (!stop) {
        json_t *ta = json_object();
        json_set(ta, "ref", sct2_ref_arg(ref));
        json_set(ta, "params", sct2_copy_or_null(params));
        json_t *typed = sct2_call(c, "self._typed_voice_stop", ta);
        if (sct2_is_raise(typed)) return typed;
        stop = sct2_truthy(typed);
        json_free(typed);
    }
    if (stop) {
        json_t *ra = json_object();
        json_set(ra, "ref", sct2_ref_arg(ref));
        json_set(ra, "params", sct2_copy_or_null(params));
        json_t *rec = sct2_call(c, "self._record_voice_stop", ra);
        if (sct2_is_raise(rec)) return rec;
        json_free(rec);
        json_t *out = json_object();
        json_set(out, "voice_stopped", json_bool(true));
        return out;
    }
    /* authority.submit(self.actor, self.submission(ref, params),
     *                    queue_while_restarting=self._operator is True) */
    json_t *sa = json_object();
    json_set(sa, "ref", sct2_ref_arg(ref));
    json_set(sa, "params", sct2_copy_or_null(params));
    json_t *submission = sct2_call(c, "self.submission", sa);
    if (sct2_is_raise(submission)) return submission;
    bool qwr = sct2_is_true(json_obj_get(c->state, "operator"));
    json_t *aba = json_object();
    json_set(aba, "actor", sct2_actor_arg(json_obj_get(c->state, "actor")));
    json_set(aba, "submission", submission);   /* adopts */
    json_set(aba, "queue_while_restarting", json_bool(qwr));
    json_t *receipt = sct2_call(c, "authority.submit", aba);
    if (sct2_is_raise(receipt)) return receipt;
    return receipt;   /* asdict(receipt): the scripted 7-field shape */
}

/* ── 10. mutate ──────────────────────────────────────────────────────────── */
/* PoP: sct2_mutate @ gateway/session_controls.py:mutate */
static json_t *sct2_mutate(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    const json_t *actor = json_obj_get(c->state, "actor");
    const char *aid = sct2_aid(c);
    json_t *ma = json_object();
    json_set_hdr(ma, "authority", aid);
    json_set(ma, "actor", sct2_actor_arg(actor));
    json_set(ma, "ref", sct2_ref_arg(ref));
    json_set(ma, "params", sct2_copy_or_null(params));
    json_t *r = sct2_call(c, "mutate_session", ma);
    if (sct2_is_raise(r)) {
        const char *kind = json_get_str(r, "__raise__", "");
        static const char *GUARDS[] = { "TurnLeaseLost", "CompressionInProgress" };
        static const char *AR[] = { "AuthorityRefusal" };
        static const char *CIP[] = { "CompressionInProgress" };
        if (sct2_caught_by(kind, GUARDS, 2)) {
            /* raise busy_refusal(exc) from exc */
            bool compression = sct2_caught_by(kind, CIP, 1);
            json_free(r);
            return sct2_refusal("session_busy", compression ? "compression" : NULL, NULL);
        }
        if (sct2_caught_by(kind, AR, 1)) {
            const char *reason = json_get_str(r, "reason", "");
            if (strcmp(reason, "revision_conflict") == 0
                    || strcmp(reason, "stale_generation") == 0) {
                json_t *ga = json_object();
                json_set_hdr(ga, "session_id", json_get_str(ref, "session_id", ""));
                json_t *row = sct2_call(c, "store.sessions.get", ga);
                if (sct2_is_raise(row)) { json_free(r); return row; }
                if (!sct2_is_null(row)) {
                    json_t *err = NULL;
                    const json_t *rev = sct2_attr(row, "runtime_revision", &err);
                    if (err) { json_free(r); json_free(row); return err; }
                    const json_t *gen = sct2_attr(row, "runtime_generation", &err);
                    if (err) { json_free(r); json_free(row); return err; }
                    json_set(r, "revision", json_copy(rev));
                    json_set(r, "execution_generation", json_copy(gen));
                }
                json_free(row);
            }
            return r;   /* bare `raise` re-raises the (enriched) refusal */
        }
        return r;   /* BaseException / other: pass through untouched */
    }
    /* child = result.get('branched_session_id') if isinstance(result, dict) else None */
    const json_t *child = sct2_is_obj(r) ? json_obj_get(r, "branched_session_id") : NULL;
    if (sct2_truthy(child)) {
        const char *csid = sct2_str(child);
        bool subscribed = csid && sct2_dict_has(sct2_sub(c, "subscriptions"), csid);
        if (!subscribed) {
            json_t *ra = json_object();
            json_t *nref = json_object();
            json_set_hdr(nref, "profile_id", json_get_str(ref, "profile_id", ""));
            json_set(nref, "session_id", json_copy(child));
            json_set(ra, "ref", nref);
            json_set(ra, "params", json_object());
            json_t *rr = sct2_call(c, "self.resume", ra);
            if (sct2_is_raise(rr)) { json_free(r); return rr; }
            json_free(rr);
        }
    }
    return r;
}

/* ── 11. receipt ─────────────────────────────────────────────────────────── */
/* PoP: sct2_receipt @ gateway/session_controls.py:receipt */
static json_t *sct2_receipt(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    const json_t *admid = json_obj_get(params, "admission_id");
    json_t *ra = json_object();
    json_set(ra, "actor", sct2_actor_arg(json_obj_get(c->state, "actor")));
    json_set(ra, "ref", sct2_ref_arg(ref));
    json_set(ra, "admission_id", sct2_copy_or_null(admid));
    json_t *rc = sct2_call(c, "authority.receipt", ra);
    if (sct2_is_raise(rc)) return rc;
    if (!sct2_is_obj(rc)) {
        /* asdict() on a non-dataclass raises TypeError. */
        json_free(rc);
        return sct2_err("TypeError", "asdict() should be called on dataclass instances");
    }
    json_t *out = json_copy(rc);
    if (sct2_is_true(json_obj_get(params, "include_result"))
            && sct2_str_eq(json_obj_get(rc, "status"), "terminal")) {
        json_t *aa = json_object();
        json_set(aa, "admission_id", sct2_copy_or_null(json_obj_get(rc, "admission_id")));
        json_t *saved = sct2_call(c, "admission_result", aa);
        if (sct2_is_raise(saved)) { json_free(rc); json_free(out); return saved; }
        if (!sct2_is_null(saved)) {
            if (!sct2_is_obj(saved) || !json_has(saved, "result")) {
                json_free(saved); json_free(rc); json_free(out);
                return sct2_keyerror("result");
            }
            json_set(out, "result", json_copy(json_obj_get(saved, "result")));
            const json_t *usage = json_obj_get(saved, "usage");
            json_set(out, "usage", sct2_truthy(usage) ? json_copy(usage) : json_object());
        }
        json_free(saved);
    }
    json_free(rc);
    return out;
}

/* ── 12. resolve_unknown ─────────────────────────────────────────────────── */
/* PoP: sct2_resolve_unknown @ gateway/session_controls.py:resolve_unknown */
static json_t *sct2_resolve_unknown(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    static const char *REQ[] = { "session_id", "admission_id", "execution_generation" };
    if (!sct2_keyset_eq(params, REQ, 3))
        return sct2_refusal("invalid_params", NULL, NULL);
    json_t *ra = json_object();
    json_set(ra, "actor", sct2_actor_arg(json_obj_get(c->state, "actor")));
    json_set(ra, "ref", sct2_ref_arg(ref));
    json_set(ra, "admission_id", json_copy(json_obj_get(params, "admission_id")));
    json_set(ra, "execution_generation", json_copy(json_obj_get(params, "execution_generation")));
    return sct2_call(c, "authority.resolve_unknown", ra);
}

/* ── 13/14. respond + respond_clarify ────────────────────────────────────── */
static json_t *sct2_do_respond(sct2_ctx_t *c, const json_t *args, bool clarify) {
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *params = json_obj_get(args, "params");
    static const char *REQ_C[] = { "session_id", "execution_generation", "prompt_id", "choice" };
    static const char *REQ_A[] = { "session_id", "execution_generation", "prompt_id", "answer" };
    if (!sct2_keyset_eq(params, clarify ? REQ_A : REQ_C, 4))
        return sct2_refusal("invalid_params", NULL, NULL);
    json_t *payload = json_object();
    json_set(payload, clarify ? "answer" : "choice",
             json_copy(json_obj_get(params, clarify ? "answer" : "choice")));
    json_t *ra = json_object();
    json_set(ra, "actor", sct2_actor_arg(json_obj_get(c->state, "actor")));
    json_set(ra, "ref", sct2_ref_arg(ref));
    json_set(ra, "execution_generation", json_copy(json_obj_get(params, "execution_generation")));
    json_set(ra, "prompt_id", json_copy(json_obj_get(params, "prompt_id")));
    json_set(ra, "payload", payload);   /* adopts */
    if (clarify) json_set_hdr(ra, "kind", "clarify");
    else json_set(ra, "kind", json_null());
    return sct2_call(c, "authority.respond", ra);
}

/* PoP: sct2_respond @ gateway/session_controls.py:respond */
static json_t *sct2_respond(sct2_ctx_t *c, const json_t *args) {
    return sct2_do_respond(c, args, false);
}

/* PoP: sct2_respond_clarify @ gateway/session_controls.py:respond_clarify */
static json_t *sct2_respond_clarify(sct2_ctx_t *c, const json_t *args) {
    return sct2_do_respond(c, args, true);
}

/* ── 15. _resolve_response ───────────────────────────────────────────────── */
/* The entitled closure: asked.append(session_id), then any(may_settle(...))
 * over (self, *self._siblings.values()) — dict order, first truthy wins. */
static json_t *sct2_entitled(sct2_ctx_t *c, const json_t *ask, bool *ok) {
    *ok = false;
    const char *sid = json_get_str(ask, "session_id", "");
    const json_t *events = json_obj_get(ask, "events");
    json_t *a = json_object();
    json_set_hdr(a, "authority", sct2_aid(c));
    json_set(a, "actor", sct2_actor_arg(json_obj_get(c->state, "actor")));
    json_set_hdr(a, "session_id", sid);
    json_set(a, "events", sct2_copy_or_null(events));
    json_t *r = sct2_call(c, "may_settle", a);
    if (sct2_is_raise(r)) return r;
    if (sct2_truthy(r)) { json_free(r); *ok = true; return NULL; }
    json_free(r);
    const json_t *siblings = json_obj_get(c->state, "siblings");
    if (sct2_is_obj(siblings)) {
        for (size_t i = 0; i < siblings->c.count; i++) {
            const json_t *sib = siblings->c.items[i];
            json_t *sa = json_object();
            json_set_hdr(sa, "authority", json_get_str(sib, "authority", ""));
            json_set(sa, "actor", sct2_actor_arg(json_obj_get(sib, "actor")));
            json_set_hdr(sa, "session_id", sid);
            json_set(sa, "events", sct2_copy_or_null(events));
            json_t *sr = sct2_call(c, "may_settle", sa);
            if (sct2_is_raise(sr)) return sr;
            if (sct2_truthy(sr)) { json_free(sr); *ok = true; return NULL; }
            json_free(sr);
        }
    }
    return NULL;
}

/* PoP: sct2_resolve_response @ gateway/session_controls.py:_resolve_response */
static json_t *sct2_resolve_response(sct2_ctx_t *c, const json_t *args) {
    const json_t *frame = json_obj_get(args, "frame");
    /* The resolve seam's script carries the registry's asks: the entitled
     * closure fires BEFORE the resolve event records (peek, no consume). */
    const json_t *entry = sct2_peek(c, "resolve");
    const json_t *asks = entry ? json_obj_get(entry, "asks") : NULL;
    bool asked = false;
    if (sct2_is_arr(asks)) {
        for (size_t i = 0; i < json_len(asks); i++) {
            asked = true;
            bool ok = false;
            json_t *err = sct2_entitled(c, json_get(asks, i), &ok);
            if (err) return err;
        }
    }
    json_t *ra = json_object();
    json_set(ra, "frame", sct2_copy_or_null(frame));
    json_t *r = sct2_call(c, "resolve", ra);
    if (sct2_is_raise(r)) return r;
    bool settled = sct2_truthy(r);
    json_free(r);
    if (!settled) {
        /* logging.getLogger(__name__).debug('dropping response for %s id=%r',
         * what, frame.get('id')) — silent on both sides (not traced).
         * what = asked ? 'a server request this connection may not settle'
         *              : 'unknown server request' */
        (void)asked;
    }
    return json_null();
}

/* ── 16. close ───────────────────────────────────────────────────────────── */
/* PoP: sct2_close @ gateway/session_controls.py:close */
static json_t *sct2_close(sct2_ctx_t *c, const json_t *args) {
    (void)args;
    const char *aid = sct2_aid(c);
    const json_t *actor = json_obj_get(c->state, "actor");
    /* for sibling in self._siblings.values(): await sibling.close() */
    const json_t *siblings = json_obj_get(c->state, "siblings");
    if (sct2_is_obj(siblings)) {
        for (size_t i = 0; i < siblings->c.count; i++) {
            const json_t *sib = siblings->c.items[i];
            json_t *a = json_object();
            json_set_hdr(a, "authority", json_get_str(sib, "authority", ""));
            json_t *r = sct2_call(c, "sibling.close", a);
            if (sct2_is_raise(r)) return r;   /* siblings NOT cleared on raise */
            json_free(r);
        }
    }
    json_set(c->state, "siblings", json_object());   /* self._siblings.clear() */
    json_t *fr = sct2_call(c, "forget", json_object());
    if (sct2_is_raise(fr)) return fr;
    json_free(fr);
    /* with owner_scope(self.authority): — silent no-op CM */
    const json_t *subs = json_obj_get(c->state, "subscriptions");
    if (sct2_is_obj(subs)) {
        for (size_t i = 0; i < subs->c.count; i++) {
            const char *sid = subs->c.keys[i];
            const json_t *sub = subs->c.items[i];
            const json_t *live = json_obj_get(sct2_sub(c, "sessions"), sid);
            if (sct2_is_null(live)) continue;
            json_t *err = NULL;
            const json_t *subscribers = sct2_attr(live, "subscribers", &err);
            if (err) return err;
            if (!sct2_set_has(subscribers, sct2_str(sub))) continue;
            json_t *da = json_object();
            json_set(da, "actor", sct2_actor_arg(actor));
            json_set(da, "subscription_id", json_copy(sub));
            json_set(da, "orphaned", json_bool(true));
            json_t *dr = sct2_call(c, "authority.detach", da);
            if (sct2_is_raise(dr)) return dr;
            json_free(dr);
        }
    }
    /* for session_id in self.handed_back: arm_detached_orphan_close.
     * SORTED iteration; fixtures keep <=1 id (live set hash order). */
    const json_t *hb = json_obj_get(c->state, "handed_back");
    if (sct2_is_arr(hb)) {
        for (size_t i = 0; i < json_len(hb); i++) {
            const char *sid = sct2_str(json_get(hb, i));
            if (!sid) continue;
            json_t *aa = json_object();
            json_set_hdr(aa, "authority", aid);
            json_set_hdr(aa, "session_id", sid);
            json_t *ar = sct2_call(c, "arm_detached_orphan_close", aa);
            if (sct2_is_raise(ar)) return ar;
            json_free(ar);
        }
    }
    json_t *ua = json_object();
    json_set_hdr(ua, "authority", aid);
    json_set(ua, "actor", sct2_actor_arg(actor));
    json_t *ur = sct2_call(c, "unbind_native_transport", ua);
    if (sct2_is_raise(ur)) return ur;
    json_free(ur);
    json_set(c->state, "subscriptions", json_object());   /* subscriptions.clear() */
    json_t *events = json_obj_get(c->state, "events");
    if (sct2_is_obj(events))
        json_obj_del(events, json_get_str(actor, "transport_id", ""));
    return json_null();
}

/* ── 17. _spawn_notice ───────────────────────────────────────────────────── */
/* PoP: sct2_spawn_notice @ gateway/session_controls.py:_spawn_notice */
static json_t *sct2_spawn_notice(sct2_ctx_t *c, const json_t *args) {
    const json_t *ref = json_obj_get(args, "ref");
    /* loop.create_task(publish_local_auto_prune_notice(authority, ref)): the
     * coroutine is created but never run — one create_task seam event. */
    json_t *ta = json_object();
    json_set_hdr(ta, "authority", sct2_aid(c));
    json_set(ta, "ref", sct2_ref_arg(ref));
    json_t *task = sct2_call(c, "create_task", ta);
    if (sct2_is_raise(task)) return task;
    /* tasks = self.__dict__.setdefault('_notice_tasks', set()); tasks.add(task) */
    json_t *tasks = sct2_set_ensure(c, "notice_tasks");
    const char *tid = sct2_str(task);
    if (tid) sct2_set_add(tasks, tid);
    json_free(task);
    /* task.add_done_callback(tasks.discard): silent on both sides. */
    return json_null();
}