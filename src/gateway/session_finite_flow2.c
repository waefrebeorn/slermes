/*
 * session_finite_flow2.c - FLOW remainder of gateway/session_finite.py
 * (hermes-agent): the UNCLAIMED admission-scoped finite consumers left after
 * tool_dispatch_flow2 claimed finite_turn_required (the ContextVar read).
 * Ported targets (6 - the module's full unclaimed ceiling; 7 top-level defs
 * minus the 1 claimed sibling):
 *   finite_clarify_reply, unattended_turn, finite_turn_scope, admit_finite,
 *   _may_run_unattended, execute_finite_admission.
 *
 * Claimed sibling finite_turn_required is NOT re-ported. Foreign collaborator
 * leaves (other modules, out of this port's scope) are SEAMED, never re-ported:
 *   gateway/session_policy.py:policy_for_source
 *   gateway/session_ingress.py:execute_admission
 *   gateway/session_display.py:display_turn_scope / submission_turn_scope
 *   gateway/session_surface.py:surface_turn_scope
 *   gateway/session_cli_turn.py:cli_turn_scope
 * Ported siblings compose for REAL: _may_run_unattended inside admit_finite;
 * finite_turn_scope inside execute_finite_admission.
 *
 * Decision sequences only: every live collaborator call enters through ONE
 * universal seam: sf2_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording monkeypatch stubs emitting the identical event shape
 * {"name","args","ret"} from the SAME scripts.
 *
 * Seam table (name -> live call):
 *   body                          the block driven inside finite_turn_scope
 *   policy_for_source             policy_for_source(runner, source)
 *                                 args = {runner?, source:{user_id}}
 *   surface_turn_scope            surface_turn_scope(surface_v1)
 *                                 args = {surface?}
 *   display_turn_scope            display_turn_scope(admission_id, display_v1)
 *                                 args = {admission_id?, display?}
 *   submission_turn_scope         submission_turn_scope(admission_id, request_id)
 *                                 args = {admission_id?, request_id?}
 *   cli_turn_scope                cli_turn_scope(admission_id, cli_turn_v1)
 *                                 args = {admission_id?, cli_turn?}
 *   <factory>.__enter__           cm.__enter__()      args = {cm: raw}
 *   <factory>.__exit__            cm.__exit__(exc)    args = {cm: raw, exc?}
 *   execute_admission             await execute_admission(authority, ref, row)
 *                                 args = {authority?, ref?, row?}
 *
 * ContextVar state model (in.state, mutated in place, echoed as out.state):
 *   "finite_turn":     value of _finite_turn (default None when absent).
 *   "unattended_turn": value of _unattended_turn (default False when absent).
 *   Only keys PRESENT in the fixture state are echoed. finite_turn_scope and
 *   execute_finite_admission set tokens and restore them in finally (reverse
 *   order), so the final state equals the initial state - a missing key stays
 *   missing (token reset deletes it).
 *
 * Representation contract (shared with tests/oracle_session_finite_flow2.py):
 * - NULL json_t* == Python None (absent); a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}. NONE of the six
 *   ported defs has an `except` clause (finite_turn_scope is try/FINALLY only),
 *   so EVERY raise - including the BaseException line (SystemExit /
 *   KeyboardInterrupt / GeneratorExit / CancelledError) - propagates unchanged.
 *   No exception-parent table is hand-rolled here (there is no except-Exception
 *   site to classify for); the oracle materializes scripted raises with the REAL
 *   builtin classes (full hierarchy via a builtins scan) plus the real
 *   AuthorityRefusal (PersistenceError, ValueError) AST-extracted from
 *   persistence/core/errors.py.
 * - Attribute doubles: connection/ref/source/actor/entry/policy dicts are
 *   materialized by the oracle to SimpleNamespace with their domain keys
 *   injected (absent key == None). C reads fields with sf2_attr (absent -> null),
 *   so absent == present-null on both sides; a non-object carrier raises the
 *   deterministic '<type>' object has no attribute '<name>' AttributeError.
 * - Scripted seams: a plain value (reused), a bare array (per-call queue, last
 *   value does NOT repeat), or {"__script__": true, "ret": v, "raise": spec}.
 *   A bare {"__raise__","message"} dict result also raises (uniform spec shape).
 *   A missing/exhausted script is a fail-closed RuntimeError on both sides; no
 *   io channel at all is "no seam channel installed for <name>".
 * - None-valued TOP-LEVEL arg keys are dropped on both sides; nested nulls are
 *   kept verbatim (canon).
 * - CM flows return None on success (the CM drives the block); a raise spec
 *   otherwise. A dict factory result is a CM carrier (enter/exit fire seams);
 *   any other result passes through to the live `with` protocol (py3.14
 *   TypeError). An __exit__ raise replaces the pending body raise; a truthy
 *   __exit__ result suppresses it (outer CMs then see exc=None); a failed cm
 *   expression / __enter__ skips that CM's body+exit; the finally token reset
 *   ALWAYS runs. `with A, B, ...` is nested: enter in order, exit in REVERSE,
 *   each CM's args evaluated just before its factory call (a KeyError there
 *   exits the already-entered CMs).
 *
 * Domain (ASCII scope, documented for the generator):
 * - fixture text is ASCII. params: dict-or-null (null -> py3.14 membership
 *   TypeError). submitter: null / list / str / dict (iterable splat); a
 *   non-iterable scalar's call-splat TypeError carries an oracle-injection-
 *   dependent module prefix and is OUT of the corpus. A wrong element count
 *   raises the py3.14 call-splat BINDING TypeError (missing/too-many positional
 *   arguments, NO module prefix), not an unpack ValueError.
 * - connection/ref/source/actor/entry/policy carriers: dict (domain keys read
 *   as None when absent) or null/scalar (deterministic AttributeError surface).
 *   user_id/subject: str-or-null (no bool/int cross-equality). session_id:
 *   str/int/bool/null (KeyError str() == repr(key); str sid is ASCII without
 *   quotes/backslashes). row: dict-or-null; payload: dict/null/str/int/bool/list
 *   (float excluded - libjson has no int/float identity for the '<T>' typename).
 * - integral floats normalize to ints on both sides; no integral float sits in
 *   a str()/type()-sensitive position.
 * - scripted raise kinds are plain-message classes (KeyError excluded from
 *   SCRIPTED raises: str(KeyError(k)) == repr(k)); real subscript KeyErrors are
 *   transcribed with the repr quoting. AuthorityRefusal('invalid_params') is the
 *   live refusal raised by admit_finite.
 *
 * Self-contained: the harness #includes this .c directly; libjson only (the gate
 * links lib/libjson/json.o). All symbols are static and sf2_-prefixed.
 * Production default (no seam channel installed) is fail-closed.
 *
 * Gate: bash tests/run_one_oracle.sh session_finite_flow2 \
 *           oracle_session_finite_flow2.py "" "lib/libjson/json.o"
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

/* -- live module constant (byte-pinned) -- */
/* gateway/session_finite.py:FINITE_CLARIFY_NOTICE */
#define SF2_FINITE_CLARIFY_NOTICE \
    "single-query mode: no user available to answer. Pick the best choices " \
    "using your own judgment, or make the most reasonable assumption you can, " \
    "and continue."

/* -- universal seam context -- */
typedef struct sf2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results; NULL == no channel */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    const json_t *in;   /* borrowed whole case input (import_fail, ...) */
} sf2_ctx_t;

/* -- basic helpers -- */
static bool sf2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sf2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sf2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sf2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sf2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sf2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool sf2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool sf2_truth(const json_t *v) {
    if (sf2_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *sf2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool sf2_str_eq(const json_t *v, const char *s) {
    const char *t = sf2_str(v);
    return t && s && strcmp(t, s) == 0;
}
static char *sf2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *sf2_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return sf2_strdup("");
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : sf2_strdup("");
}
static json_t *sf2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
/* Python type name for the fixture domain (integral float reads as int -
 * libjson has no int/float identity; non-integral floats are out of domain). */
static const char *sf2_typename(const json_t *v) {
    if (sf2_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static json_t *sf2_attr_error(const json_t *v, const char *attr) {
    char *msg = sf2_fmt("'%s' object has no attribute '%s'", sf2_typename(v), attr);
    json_t *e = sf2_err("AttributeError", msg);
    free(msg);
    return e;
}
/* py3.14 `with <non-carrier>:` - TypeError, missed __exit__ wording. */
static json_t *sf2_cm_error(const json_t *v) {
    char *msg = sf2_fmt("'%s' object does not support the context manager protocol "
                        "(missed __exit__ method)", sf2_typename(v));
    json_t *e = sf2_err("TypeError", msg);
    free(msg);
    return e;
}
/* py3.14 `<non-subscriptable>[k]` (null carrier). */
static json_t *sf2_subscript_error(const json_t *v) {
    char *msg = sf2_fmt("'%s' object is not subscriptable", sf2_typename(v));
    json_t *e = sf2_err("TypeError", msg);
    free(msg);
    return e;
}
/* py3.14 `k in <non-container>` membership TypeError. */
static json_t *sf2_membership_error(const json_t *v) {
    char *msg = sf2_fmt("argument of type '%s' is not a container or iterable",
                        sf2_typename(v));
    json_t *e = sf2_err("TypeError", msg);
    free(msg);
    return e;
}
/* repr() of a scalar KeyError key (str/int/bool/null domain). */
static char *sf2_repr_scalar(const json_t *v) {
    if (sf2_is_null(v)) return sf2_strdup("None");
    if (sf2_is_bool(v)) return sf2_strdup(v->bool_val ? "True" : "False");
    if (sf2_is_num(v)) return sf2_fmt("%lld", (long long)v->num_val);
    if (sf2_is_str(v)) return sf2_fmt("'%s'", v->str_val ? v->str_val : "");
    return sf2_strdup(""); /* out of domain */
}
static json_t *sf2_key_error(const json_t *key) {
    char *r = sf2_repr_scalar(key);
    json_t *e = sf2_err("KeyError", r);
    free(r);
    return e;
}
static json_t *sf2_key_error_str(const char *k) {
    char *m = sf2_fmt("'%s'", k);
    json_t *e = sf2_err("KeyError", m);
    free(m);
    return e;
}
/* py3.14 call-splat binding TypeError for _may_run_unattended(*submitter) with
 * a wrong element count (these message forms carry NO module prefix). */
static json_t *sf2_splat_count_error(size_t got) {
    char *m;
    if (got == 0)
        m = sf2_strdup("_may_run_unattended() missing 2 required positional arguments: "
                       "'connection' and 'ref'");
    else if (got == 1)
        m = sf2_strdup("_may_run_unattended() missing 1 required positional argument: 'ref'");
    else
        m = sf2_fmt("_may_run_unattended() takes 2 positional arguments but %zu were given", got);
    json_t *e = sf2_err("TypeError", m);
    free(m);
    return e;
}
/* The live refusal admit_finite raises. */
static json_t *sf2_refusal(void) { return sf2_err("AuthorityRefusal", "invalid_params"); }

/* str-or-null equality (user_id != subject domain). */
static bool sf2_scalar_eq(const json_t *a, const json_t *b) {
    if (sf2_is_null(a) && sf2_is_null(b)) return true;
    if (sf2_is_str(a) && sf2_is_str(b))
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return false;
}

/* -- seam dispatch -- */
static json_t *sf2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sf2_err(type, msg);
}
static json_t *sf2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return sf2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}
/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees. */
static json_t *sf2_call(sf2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    if (!c->io) {
        json_t *ev = json_object();
        json_set_hdr(ev, "name", name);
        json_set(ev, "args", args);
        char *msg = sf2_fmt("no seam channel installed for %s", name);
        json_t *res = sf2_err("RuntimeError", msg);
        free(msg);
        json_set(ev, "ret", json_copy(res));
        json_append(c->trace, ev);
        return res;
    }
    const json_t *entry = json_obj_get(c->io, name);
    json_t *res;
    if (!entry) {
        char *msg = sf2_fmt("no script for %s", name);
        res = sf2_err("RuntimeError", msg);
        free(msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char *msg = sf2_fmt("script exhausted for %s", name);
            res = sf2_err("RuntimeError", msg);
            free(msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = sf2_entry_result(json_get(entry, idx));
        }
    } else {
        res = sf2_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}
/* copy a borrowed value into a TOP-LEVEL args key; NULL/None drops the key. */
static void sf2_jset(json_t *obj, const char *key, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(obj, key, json_copy(v));
}

/* -- attribute-double read --
 * getattr(v, name): owned copy of the value, or a raise spec. Only objects
 * carry attributes; a null/scalar carrier raises AttributeError. An absent key
 * on an object reads as None (the oracle injects None defaults), matching the
 * C absent->null read. */
static json_t *sf2_attr(const json_t *v, const char *name) {
    if (!sf2_is_obj(v)) return sf2_attr_error(v, name);
    json_t *x = json_obj_get(v, name);
    return x ? json_copy(x) : json_null();
}

/* -- in-function import gate -- */
static bool sf2_import_halted(sf2_ctx_t *c, const char *full) {
    const json_t *lst = json_obj_get(c->in, "import_fail");
    if (!sf2_is_arr(lst)) return false;
    for (size_t i = 0; i < lst->c.count; i++) {
        const char *m = sf2_str(lst->c.items[i]);
        if (m && strcmp(m, full) == 0) return true;
    }
    return false;
}
static json_t *sf2_halted_err(const char *full) {
    char *m = sf2_fmt("import of %s halted; None in sys.modules", full);
    json_t *e = sf2_err("ModuleNotFoundError", m);
    free(m);
    return e;
}

/* -- ContextVar state model -- */
/* var.set(value) - value OWNED. Returns the reset token {name,present,prev}. */
static json_t *sf2_cv_set(sf2_ctx_t *c, const char *name, json_t *value) {
    bool present = json_has(c->state, name);
    json_t *pv = json_obj_get(c->state, name);
    json_t *prev = pv ? json_copy(pv) : NULL;
    json_set(c->state, name, value);
    json_t *t = json_object();
    json_set_hdr(t, "name", name);
    json_set(t, "present", json_bool(present));
    json_set(t, "prev", prev ? prev : json_null());
    return t;
}
/* var.reset(token) - restore presence AND value (delete a flow-created key). */
static void sf2_cv_reset(sf2_ctx_t *c, json_t *t) {
    const char *name = json_get_str(t, "name", "");
    bool present = json_get_bool(t, "present", false);
    json_t *prev = json_obj_get(t, "prev");
    if (present) json_set(c->state, name, prev ? json_copy(prev) : json_null());
    else json_obj_del(c->state, name);
    json_free(t);
}

/* -- foreign context-manager frame -- */
typedef struct sf2_cm {
    const char *seam;   /* factory seam name */
    json_t *raw;        /* owned dict carrier (factory result) or NULL */
    bool entered;       /* __enter__ succeeded */
} sf2_cm_t;

/* Open a foreign CM: fire the factory seam, then __enter__ if a dict carrier.
 * Returns NULL on success (cm filled), or a raise spec (pending). */
static json_t *sf2_cm_open(sf2_ctx_t *c, sf2_cm_t *cm, const char *seam, json_t *fargs) {
    cm->seam = seam; cm->raw = NULL; cm->entered = false;
    json_t *res = sf2_call(c, seam, fargs);
    if (sf2_is_raise(res)) return res;
    if (!sf2_is_obj(res)) {
        json_t *e = sf2_cm_error(res);
        json_free(res);
        return e;
    }
    cm->raw = res; /* owned dict */
    json_t *ea = json_object();
    json_set(ea, "cm", json_copy(res));
    char *en = sf2_fmt("%s.__enter__", seam);
    json_t *er = sf2_call(c, en, ea);
    free(en);
    if (sf2_is_raise(er)) return er;
    json_free(er);
    cm->entered = true;
    return NULL;
}
/* Close an entered foreign CM: fire __exit__ with the pending exc. An exit
 * raise replaces pending; a truthy exit result suppresses it. Returns the
 * (possibly new) pending raise (owned) or NULL. */
static json_t *sf2_cm_close(sf2_ctx_t *c, sf2_cm_t *cm, json_t *pending) {
    if (!cm->entered) return pending;
    json_t *xa = json_object();
    json_set(xa, "cm", json_copy(cm->raw));
    if (pending) json_set(xa, "exc", json_copy(pending));
    char *xn = sf2_fmt("%s.__exit__", cm->seam);
    json_t *xr = sf2_call(c, xn, xa);
    free(xn);
    if (sf2_is_raise(xr)) {
        json_free(pending);
        return xr;
    }
    if (sf2_truth(xr)) {
        json_free(pending);
        json_free(xr);
        return NULL;
    }
    json_free(xr);
    return pending;
}

/* ===========================================================================
 * Ported flows
 * =========================================================================== */

/* PoP: sf2_finite_clarify_reply @ gateway/session_finite.py:finite_clarify_reply */
/* return {"answers": {}, "outcome": "undelivered", "notice": FINITE_CLARIFY_NOTICE} */
static json_t *sf2_finite_clarify_reply(sf2_ctx_t *c, const json_t *args) {
    (void)c; (void)args;
    json_t *out = json_object();
    json_set(out, "answers", json_object());
    json_set_hdr(out, "outcome", "undelivered");
    json_set_hdr(out, "notice", SF2_FINITE_CLARIFY_NOTICE);
    return out;
}

/* PoP: sf2_unattended_turn @ gateway/session_finite.py:unattended_turn */
/* return _unattended_turn.get()  (default False) */
static json_t *sf2_unattended_turn(sf2_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *v = json_obj_get(c->state, "unattended_turn");
    return v ? json_copy(v) : json_bool(false);
}

/* PoP: sf2_finite_turn_scope @ gateway/session_finite.py:finite_turn_scope */
/* @contextmanager: token=_finite_turn.set(finite);
 * unattended_token=_unattended_turn.set(bool(finite and unattended));
 * try: yield  finally: reset(unattended_token); reset(token). */
static json_t *sf2_finite_turn_scope(sf2_ctx_t *c, const json_t *args) {
    const json_t *finite = json_obj_get(args, "finite");
    const json_t *unattended = json_obj_get(args, "unattended"); /* default False */
    bool fa_truth = sf2_truth(finite);
    bool ua = fa_truth && sf2_truth(unattended);
    json_t *t1 = sf2_cv_set(c, "finite_turn", finite ? json_copy(finite) : json_null());
    json_t *t2 = sf2_cv_set(c, "unattended_turn", json_bool(ua));
    json_t *body = sf2_call(c, "body", json_object());
    /* finally - reverse order, runs on every path */
    sf2_cv_reset(c, t2);
    sf2_cv_reset(c, t1);
    if (sf2_is_raise(body)) return body;
    json_free(body);
    return NULL; /* the CM drove the block; ret None */
}

/* -- _may_run_unattended (composed by admit_finite) -- */
/* PoP: sf2_may_run_unattended @ gateway/session_finite.py:_may_run_unattended */
/* from gateway.session_policy import policy_for_source
 * source = connection.authority.sessions[ref.session_id].source
 * if not connection.native_owner or source is None
 *        or source.user_id != connection.actor.subject: return False
 * policy = policy_for_source(connection.authority.runner, source)
 * return policy is not None and policy.platform == 'cli' */
static json_t *sf2_may_run_unattended(sf2_ctx_t *c, const json_t *conn, const json_t *ref) {
    if (sf2_import_halted(c, "gateway.session_policy"))
        return sf2_halted_err("gateway.session_policy");

    json_t *authority = sf2_attr(conn, "authority");
    if (sf2_is_raise(authority)) return authority;
    json_t *sessions = sf2_attr(authority, "sessions");
    if (sf2_is_raise(sessions)) { json_free(authority); return sessions; }
    json_t *sid = sf2_attr(ref, "session_id");
    if (sf2_is_raise(sid)) { json_free(authority); json_free(sessions); return sid; }

    /* sessions[ref.session_id] - sessions is dict-or-null; only a STRING sid
     * can match a JSON object key (int/bool/null keys never equal a str key). */
    if (!sf2_is_obj(sessions)) {
        json_t *e = sf2_subscript_error(sessions);
        json_free(authority); json_free(sessions); json_free(sid);
        return e;
    }
    const json_t *entry = sf2_is_str(sid) ? json_obj_get(sessions, sid->str_val) : NULL;
    if (!entry) {
        json_t *e = sf2_key_error(sid);
        json_free(authority); json_free(sessions); json_free(sid);
        return e;
    }
    /* entry.source - read while sessions (entry's owner) is alive */
    json_t *source = sf2_attr(entry, "source");
    json_free(sessions); json_free(sid);
    if (sf2_is_raise(source)) { json_free(authority); return source; }

    /* guard: not connection.native_owner or source is None or user_id != subject */
    json_t *no = sf2_attr(conn, "native_owner");
    if (sf2_is_raise(no)) { json_free(authority); json_free(source); return no; }
    bool falsy_owner = !sf2_truth(no);
    json_free(no);
    if (falsy_owner) { json_free(authority); json_free(source); return json_bool(false); }
    if (sf2_is_null(source)) { json_free(authority); json_free(source); return json_bool(false); }

    json_t *uid = sf2_attr(source, "user_id");
    if (sf2_is_raise(uid)) { json_free(authority); json_free(source); return uid; }
    json_t *actor = sf2_attr(conn, "actor");
    if (sf2_is_raise(actor)) { json_free(authority); json_free(source); json_free(uid); return actor; }
    json_t *subject = sf2_attr(actor, "subject");
    json_free(actor);
    if (sf2_is_raise(subject)) { json_free(authority); json_free(source); json_free(uid); return subject; }
    bool neq = !sf2_scalar_eq(uid, subject);
    json_free(uid); json_free(subject);
    if (neq) { json_free(authority); json_free(source); return json_bool(false); }

    /* policy_for_source(connection.authority.runner, source) */
    json_t *runner = sf2_attr(authority, "runner");
    if (sf2_is_raise(runner)) { json_free(authority); json_free(source); return runner; }
    json_t *a = json_object();
    sf2_jset(a, "runner", runner);
    /* source projects to its double field {user_id} (nested null kept) */
    json_t *source_proj = json_object();
    json_t *suid = json_obj_get(source, "user_id");
    json_set(source_proj, "user_id", suid ? json_copy(suid) : json_null());
    json_set(a, "source", source_proj);
    json_t *policy = sf2_call(c, "policy_for_source", a);
    json_free(runner); json_free(authority); json_free(source);
    if (sf2_is_raise(policy)) return policy;

    /* policy is not None and policy.platform == 'cli' */
    if (sf2_is_null(policy)) { json_free(policy); return json_bool(false); }
    json_t *platform = sf2_attr(policy, "platform");
    json_free(policy);
    if (sf2_is_raise(platform)) return platform;
    bool is_cli = sf2_str_eq(platform, "cli");
    json_free(platform);
    return json_bool(is_cli);
}

/* splat submitter into exactly (connection, ref). Returns NULL on success
 * (conn/ref OWNED), or a raise spec. Domain: list/str/dict (iterable). */
static json_t *sf2_splat2(const json_t *sub, json_t **conn, json_t **ref) {
    *conn = NULL; *ref = NULL;
    if (sf2_is_arr(sub)) {
        size_t n = sub->c.count;
        if (n != 2) return sf2_splat_count_error(n);
        *conn = json_copy(sub->c.items[0]);
        *ref = json_copy(sub->c.items[1]);
        return NULL;
    }
    if (sf2_is_str(sub)) {
        const char *s = sub->str_val ? sub->str_val : "";
        size_t n = strlen(s); /* ASCII domain: bytes == codepoints */
        if (n != 2) return sf2_splat_count_error(n);
        char a[2] = { s[0], 0 }, b[2] = { s[1], 0 };
        *conn = json_string(a); *ref = json_string(b);
        return NULL;
    }
    if (sf2_is_obj(sub)) {
        size_t n = sub->c.count;
        if (n != 2) return sf2_splat_count_error(n);
        *conn = json_string(sub->c.keys[0]);
        *ref = json_string(sub->c.keys[1]);
        return NULL;
    }
    /* non-iterable scalar: out of the documented corpus (the live call-splat
     * message carries an oracle-injection-dependent module prefix). */
    char *m = sf2_fmt("'%s' object is not iterable", sf2_typename(sub));
    json_t *e = sf2_err("TypeError", m);
    free(m);
    return e;
}

/* PoP: sf2_admit_finite @ gateway/session_finite.py:admit_finite */
/* if 'unattended' in params and (type(params['unattended']) is not bool
 *        or (params['unattended'] and params.get('finite') is not True)):
 *     raise AuthorityRefusal('invalid_params')
 * if params.get('unattended') and submitter is not None
 *        and not _may_run_unattended(*submitter): raise AuthorityRefusal(...)
 * if 'finite' not in params: return {}
 * if type(params['finite']) is not bool: raise AuthorityRefusal(...)
 * return {'finite': params['finite'],
 *         **({'unattended': True} if params.get('unattended') else {})} */
static json_t *sf2_admit_finite(sf2_ctx_t *c, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    const json_t *submitter = json_obj_get(args, "submitter");

    /* 'unattended' in params - params is dict-or-null */
    if (!sf2_is_obj(params)) return sf2_membership_error(params);

    if (json_has(params, "unattended")) {
        const json_t *u = json_obj_get(params, "unattended");
        if (!sf2_is_bool(u)) return sf2_refusal();
        if (u->bool_val) {
            const json_t *f = json_obj_get(params, "finite");
            bool is_true = sf2_is_bool(f) && f->bool_val; /* `is True` identity */
            if (!is_true) return sf2_refusal();
        }
    }
    /* params.get('unattended') and submitter is not None and not _may_run... */
    if (sf2_truth(json_obj_get(params, "unattended")) && !sf2_is_null(submitter)) {
        json_t *conn = NULL, *ref = NULL;
        json_t *se = sf2_splat2(submitter, &conn, &ref);
        if (se) return se;
        json_t *r = sf2_may_run_unattended(c, conn, ref);
        json_free(conn); json_free(ref);
        if (sf2_is_raise(r)) return r;
        bool ok = sf2_truth(r);
        json_free(r);
        if (!ok) return sf2_refusal();
    }
    if (!json_has(params, "finite")) return json_object();
    const json_t *f = json_obj_get(params, "finite");
    if (!sf2_is_bool(f)) return sf2_refusal();
    json_t *out = json_object();
    json_set(out, "finite", json_copy(f));
    if (sf2_truth(json_obj_get(params, "unattended")))
        json_set(out, "unattended", json_bool(true));
    return out;
}

/* PoP: sf2_execute_finite_admission @ gateway/session_finite.py:execute_finite_admission */
/* from gateway.session_ingress import execute_admission
 * from gateway.session_display import display_turn_scope, submission_turn_scope
 * from gateway.session_surface import surface_turn_scope
 * from gateway.session_cli_turn import cli_turn_scope
 * with finite_turn_scope(payload.get('finite', False),
 *                        payload.get('unattended') is True),
 *      surface_turn_scope(payload.get('surface_v1')),
 *      display_turn_scope(row['admission_id'], payload.get('display_v1')),
 *      submission_turn_scope(row['admission_id'], row.get('request_id')),
 *      cli_turn_scope(row['admission_id'], payload.get('cli_turn_v1')):
 *     return await execute_admission(authority, ref, row) */
static json_t *sf2_execute_finite_admission(sf2_ctx_t *c, const json_t *args) {
    static const char *imports[] = {
        "gateway.session_ingress", "gateway.session_display",
        "gateway.session_surface", "gateway.session_cli_turn",
    };
    for (int i = 0; i < 4; i++)
        if (sf2_import_halted(c, imports[i])) return sf2_halted_err(imports[i]);

    const json_t *authority = json_obj_get(args, "authority");
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *row = json_obj_get(args, "row");

    /* row['payload'] - row is dict-or-null */
    if (!sf2_is_obj(row)) return sf2_subscript_error(row);
    const json_t *payload = json_obj_get(row, "payload");
    if (!payload) return sf2_key_error_str("payload");
    if (!sf2_is_obj(payload)) return sf2_attr_error(payload, "get");

    /* finite_turn_scope enter (real ported sibling): set the two tokens */
    const json_t *fv = json_obj_get(payload, "finite");
    json_t *finite_arg = fv ? json_copy(fv) : json_bool(false); /* .get('finite', False) */
    const json_t *uv = json_obj_get(payload, "unattended");
    bool unatt_arg = sf2_is_bool(uv) && uv->bool_val; /* `is True` identity */
    bool fa_truth = sf2_truth(finite_arg);
    json_t *t1 = sf2_cv_set(c, "finite_turn", finite_arg);
    json_t *t2 = sf2_cv_set(c, "unattended_turn", json_bool(fa_truth && unatt_arg));

    json_t *pending = NULL;
    json_t *retval = NULL;
    sf2_cm_t cms[4]; /* surface, display, submission, cli */
    for (int i = 0; i < 4; i++) { cms[i].seam = NULL; cms[i].raw = NULL; cms[i].entered = false; }
    int opened = 0;

    /* surface_turn_scope(payload.get('surface_v1')) */
    json_t *fa = json_object();
    sf2_jset(fa, "surface", json_obj_get(payload, "surface_v1"));
    pending = sf2_cm_open(c, &cms[0], "surface_turn_scope", fa);
    opened = 1;
    if (pending) goto cleanup;

    /* display_turn_scope(row['admission_id'], payload.get('display_v1')) */
    const json_t *aid = json_obj_get(row, "admission_id");
    if (!aid) { pending = sf2_key_error_str("admission_id"); goto cleanup; }
    fa = json_object();
    sf2_jset(fa, "admission_id", aid);
    sf2_jset(fa, "display", json_obj_get(payload, "display_v1"));
    pending = sf2_cm_open(c, &cms[1], "display_turn_scope", fa);
    opened = 2;
    if (pending) goto cleanup;

    /* submission_turn_scope(row['admission_id'], row.get('request_id')) */
    fa = json_object();
    sf2_jset(fa, "admission_id", aid);
    sf2_jset(fa, "request_id", json_obj_get(row, "request_id"));
    pending = sf2_cm_open(c, &cms[2], "submission_turn_scope", fa);
    opened = 3;
    if (pending) goto cleanup;

    /* cli_turn_scope(row['admission_id'], payload.get('cli_turn_v1')) */
    fa = json_object();
    sf2_jset(fa, "admission_id", aid);
    sf2_jset(fa, "cli_turn", json_obj_get(payload, "cli_turn_v1"));
    pending = sf2_cm_open(c, &cms[3], "cli_turn_scope", fa);
    opened = 4;
    if (pending) goto cleanup;

    /* body: return await execute_admission(authority, ref, row) */
    json_t *ba = json_object();
    sf2_jset(ba, "authority", authority);
    sf2_jset(ba, "ref", ref);
    sf2_jset(ba, "row", row);
    json_t *body = sf2_call(c, "execute_admission", ba);
    if (sf2_is_raise(body)) { pending = body; goto cleanup; }
    retval = body;

cleanup:
    /* exit entered CMs in REVERSE (cli, submission, display, surface) */
    for (int i = opened - 1; i >= 0; i--)
        pending = sf2_cm_close(c, &cms[i], pending);
    /* finite_turn_scope finally: reset tokens reverse */
    sf2_cv_reset(c, t2);
    sf2_cv_reset(c, t1);
    for (int i = 0; i < 4; i++) json_free(cms[i].raw);
    if (pending) { json_free(retval); return pending; }
    return retval;
}
