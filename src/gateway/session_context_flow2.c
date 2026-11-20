/*
 * session_context_flow2.c — FLOW remainder of gateway/session_context.py
 * (hermes-agent): the unclaimed leaves left after the first-wave ports
 * (session_context.c / session_context_ports.c claim set_session_vars,
 * clear_session_vars, reset_session_vars, get_session_env,
 * set_current_session_id, scoped_current_session_id, session_context_engaged,
 * session_is_messaging_surface, declare_stateless_channel,
 * async_delivery_supported). Ported targets (7):
 *   _runtime_cwd, scoped_turn_actor, scoped_session_key, bound_session_key,
 *   source_route_metadata, _bound_session_value,
 *   session_history_delivery_supported.
 *
 * Decision sequences only: every live collaborator call enters through ONE
 * universal seam: scx2_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording monkeypatch stubs emitting the identical event shape
 * {"name","args","ret"} from the SAME scripts.
 *
 * Seam table (name -> live call):
 *   runtime_cwd.<func>                  getattr(agent.runtime_cwd, func)(*args)
 *                                       (in-function import; args = {"args":[...]})
 *   turn.running_follow_up              turn.running_follow_up(event)
 *                                       (args = {"event": event}; a dict result is
 *                                        a context-manager carrier, anything else
 *                                        passes through to the live `with` protocol)
 *   turn.running_follow_up.__enter__    cm.__enter__()          (args = {"cm": raw})
 *   turn.running_follow_up.__exit__     cm.__exit__(exc)        (args = {"cm": raw,
 *                                        "exc": raise-spec or absent})
 *   body                                the block driven inside the scoped CM
 *                                       (args = {})
 *
 * ContextVar state model (in.state, mutated in place, echoed as out.state):
 *   "vars": {<legacy name>: value}  — the 22 _VAR_MAP ContextVars; a MISSING
 *           key reads as _UNSET; the marker {"__unset__": true} is an explicit
 *           _UNSET value. Only the 22 known names are readable (a stray key
 *           for an unknown name is ignored, like _VAR_MAP.get -> None).
 *   "turn_event": event dict | null — _SESSION_TURN_EVENT (default None).
 *   "history_delivery": value       — _SESSION_HISTORY_DELIVERY (default _UNSET).
 *   "async_delivery": value         — _SESSION_ASYNC_DELIVERY (echo only).
 *   "engaged": bool                 — _session_context_engaged latch (echo only).
 * Only keys PRESENT in the fixture state are materialized/echoed; scoped flows
 * set tokens and restore them in finally (reverse order), so the final state
 * equals the initial state — a missing key stays missing (token reset deletes
 * it; a vars container created by the flow is removed when it empties).
 *
 * Representation contract (shared with tests/oracle_session_context_flow2.py):
 * - NULL json_t* == Python None (absent); a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; the BaseException
 *   line (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/
 *   BaseExceptionGroup) passes through the `except Exception` site in
 *   _runtime_cwd; the full builtin hierarchy table decides Exception lineage.
 * - Event/turn/source doubles travel as plain dicts; the oracle materializes
 *   them to SimpleNamespace recursively so getattr reads agree.
 * - Scripted seams: a plain value (reused), a bare array (per-call queue, last
 *   value does NOT repeat), or {"__script__": true, "ret": v, "raise": spec}.
 *   A bare {"__raise__","message"} dict result also raises (uniform spec shape
 *   only). A missing/exhausted script is a fail-closed RuntimeError on both
 *   sides; no io channel at all is "no seam channel installed for <name>".
 * - None-valued arg keys are dropped on both sides.
 * - Fixture corpus is ASCII. str() domains: anchor/actor attribute values are
 *   str/int/bool/null (floats and containers excluded — str(float)/str(dict)
 *   repr is out of domain); metadata is dict/null (truthy non-mapping raises
 *   TypeError when anchors exist); scripted raise kinds are plain-message
 *   classes (KeyError excluded: str(KeyError(k)) == repr(k)).
 * - CM flows return None on success (the CM drives the block); a raise spec
 *   otherwise. An __exit__ raise replaces the pending body raise; a truthy
 *   __exit__ result suppresses it; a failed cm expression or __enter__ skips
 *   body and __exit__; the finally token reset ALWAYS runs.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and scx2_-prefixed. Production default (no seam channel
 * installed) is fail-closed.
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
typedef struct scx2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results; NULL == no channel */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    const json_t *in;   /* borrowed whole case input (import_fail, attrs) */
} scx2_ctx_t;

/* ── basic helpers ── */
static bool scx2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool scx2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool scx2_truthy(const json_t *v) {
    if (scx2_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *scx2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool scx2_str_eq(const json_t *v, const char *s) {
    const char *t = scx2_str(v);
    return t && s && strcmp(t, s) == 0;
}

static char *scx2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static char *scx2_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return scx2_strdup("");
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : scx2_strdup("");
}

static json_t *scx2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static const char *scx2_typename(const json_t *v) {
    if (scx2_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *scx2_attr_error(const json_t *v, const char *attr) {
    char *msg = scx2_fmt("'%s' object has no attribute '%s'", scx2_typename(v), attr);
    json_t *e = scx2_err("AttributeError", msg);
    free(msg);
    return e;
}

/* py3.14 `with <non-carrier>:` — TypeError, missed __exit__ wording. */
static json_t *scx2_cm_error(const json_t *v) {
    char *msg = scx2_fmt("'%s' object does not support the context manager protocol "
                         "(missed __exit__ method)", scx2_typename(v));
    json_t *e = scx2_err("TypeError", msg);
    free(msg);
    return e;
}

/* {**<truthy non-mapping>} */
static json_t *scx2_mapping_error(const json_t *v) {
    char *msg = scx2_fmt("'%s' object is not a mapping", scx2_typename(v));
    json_t *e = scx2_err("TypeError", msg);
    free(msg);
    return e;
}

/* str() over the fixture domain: str/int/bool (null handled by callers). */
static char *scx2_pystr(const json_t *v) {
    if (scx2_is_null(v)) return scx2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return scx2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                return scx2_fmt("%lld", (long long)v->num_val);
            return scx2_fmt("%.17g", v->num_val); /* out of domain fallback */
        case JSON_STRING: return scx2_strdup(v->str_val ? v->str_val : "");
        default: return scx2_strdup("");          /* out of domain fallback */
    }
}

/* str(x or "") — falsy operands collapse to the empty string. */
static char *scx2_pystr_or_empty(const json_t *v) {
    if (!scx2_truthy(v)) return scx2_strdup("");
    return scx2_pystr(v);
}

/* ── exception lineage (FULL builtin hierarchy, py3.14) ──
 * ExceptionGroup's first base is BaseExceptionGroup but it IS an Exception
 * subclass; the table stores its Exception-lineage parent. CancelledError is
 * asyncio's BaseException subclass. Unknown kinds are synthetic Exception
 * subclasses on the oracle side, so they walk to Exception here too. */
typedef struct { const char *name; const char *parent; } scx2_exc_row_t;
static const scx2_exc_row_t SCX2_EXC_PARENTS[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"CancelledError", "BaseException"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "Exception"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "Exception"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
};

static const char *scx2_exc_parent(const char *name) {
    for (size_t i = 0; i < sizeof(SCX2_EXC_PARENTS) / sizeof(SCX2_EXC_PARENTS[0]); i++)
        if (strcmp(SCX2_EXC_PARENTS[i].name, name) == 0)
            return SCX2_EXC_PARENTS[i].parent;
    return "Exception"; /* synthetic kind == Exception subclass */
}

/* True when `except Exception` catches the kind (Exception lineage). */
static bool scx2_exc_is_exception(const char *kind) {
    const char *cur = kind;
    for (int hops = 0; hops < 16; hops++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return false;
        cur = scx2_exc_parent(cur);
    }
    return false;
}

/* ── seam dispatch ── */
static json_t *scx2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return scx2_err(type, msg);
}

static json_t *scx2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return scx2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees. */
static json_t *scx2_call(scx2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    if (!c->io) {
        /* fail-closed: no seam channel installed */
        json_t *ev = json_object();
        json_set_hdr(ev, "name", name);
        json_set(ev, "args", args);
        char *msg = scx2_fmt("no seam channel installed for %s", name);
        json_t *res = scx2_err("RuntimeError", msg);
        free(msg);
        json_set(ev, "ret", json_copy(res));
        json_append(c->trace, ev);
        return res;
    }
    const json_t *entry = json_obj_get(c->io, name);
    json_t *res;
    if (!entry) {
        char *msg = scx2_fmt("no script for %s", name);
        res = scx2_err("RuntimeError", msg);
        free(msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char *msg = scx2_fmt("script exhausted for %s", name);
            res = scx2_err("RuntimeError", msg);
            free(msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = scx2_entry_result(json_get(entry, idx));
        }
    } else {
        res = scx2_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* copy a borrowed value into an args object; NULL (None) drops the key —
 * including a present JSON_NULL node (Python None), on both sides. */
static void scx2_jset(json_t *obj, const char *key, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(obj, key, json_copy(v));
}

/* ── ContextVar state model ── */
static const char *SCX2_VAR_NAMES[] = {
    "HERMES_SESSION_PLATFORM", "HERMES_SESSION_SOURCE", "HERMES_SESSION_CHAT_ID",
    "HERMES_SESSION_CHAT_TYPE", "HERMES_SESSION_CHAT_NAME", "HERMES_SESSION_THREAD_ID",
    "HERMES_SESSION_USER_ID", "HERMES_SESSION_USER_ID_ALT", "HERMES_SESSION_USER_NAME",
    "HERMES_SESSION_SCOPE_ID", "HERMES_SESSION_KEY", "HERMES_SESSION_ID",
    "HERMES_UI_SESSION_ID", "HERMES_SESSION_MESSAGE_ID", "HERMES_SESSION_PROFILE",
    "HERMES_CRON_SESSION", "HERMES_SESSION_PARENT_CHAT_ID",
    "HERMES_SESSION_ACTOR_ID", "HERMES_SESSION_ACTOR_NAME",
    "HERMES_CRON_AUTO_DELIVER_PLATFORM", "HERMES_CRON_AUTO_DELIVER_CHAT_ID",
    "HERMES_CRON_AUTO_DELIVER_THREAD_ID",
};

static bool scx2_var_known(const char *name) {
    for (size_t i = 0; i < sizeof(SCX2_VAR_NAMES) / sizeof(SCX2_VAR_NAMES[0]); i++)
        if (strcmp(SCX2_VAR_NAMES[i], name) == 0) return true;
    return false;
}

static bool scx2_is_unset(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__unset__", false);
}

static json_t *scx2_unset(void) {
    json_t *o = json_object();
    json_set(o, "__unset__", json_bool(true));
    return o;
}

/* borrowed read; NULL == _UNSET (missing key or missing container). */
static json_t *scx2_var_get(scx2_ctx_t *c, const char *name) {
    json_t *vars = json_obj_get(c->state, "vars");
    if (!vars || vars->type != JSON_OBJECT) return NULL;
    return json_obj_get(vars, name);
}

/* token: {"kind","name","present","prev","made_vars"} — prev is a copy or null. */
static json_t *scx2_token(const char *kind, const char *name, bool present,
                          json_t *prev, bool made_vars) {
    json_t *t = json_object();
    json_set_hdr(t, "kind", kind);
    json_set_hdr(t, "name", name);
    json_set(t, "present", json_bool(present));
    json_set(t, "prev", prev ? prev : json_null());
    json_set(t, "made_vars", json_bool(made_vars));
    return t;
}

/* var.set(value) — value OWNED. Returns the reset token. */
static json_t *scx2_set_var(scx2_ctx_t *c, const char *name, json_t *value) {
    json_t *vars = json_obj_get(c->state, "vars");
    bool made_vars = false;
    if (!vars || vars->type != JSON_OBJECT) {
        vars = json_object();
        json_set(c->state, "vars", vars);
        made_vars = true;
    }
    bool present = json_has(vars, name);
    json_t *prev = json_copy(json_obj_get(vars, name));
    json_set(vars, name, value);
    return scx2_token("var", name, present, prev, made_vars);
}

/* _SESSION_TURN_EVENT.set(value) — value OWNED. */
static json_t *scx2_set_turn_event(scx2_ctx_t *c, json_t *value) {
    bool present = json_has(c->state, "turn_event");
    json_t *prev = json_copy(json_obj_get(c->state, "turn_event"));
    json_set(c->state, "turn_event", value);
    return scx2_token("turn_event", "turn_event", present, prev, false);
}

/* var.reset(token) — restore presence AND value; drop a flow-created vars
 * container once it empties so the state echo matches the fixture shape. */
static void scx2_reset_token(scx2_ctx_t *c, json_t *t) {
    const char *kind = json_get_str(t, "kind", "");
    const char *name = json_get_str(t, "name", "");
    bool present = json_get_bool(t, "present", false);
    bool made_vars = json_get_bool(t, "made_vars", false);
    json_t *prev = json_obj_get(t, "prev");
    if (strcmp(kind, "var") == 0) {
        json_t *vars = json_obj_get(c->state, "vars");
        if (vars && vars->type == JSON_OBJECT) {
            if (present) json_set(vars, name, json_copy(prev));
            else json_obj_del(vars, name);
            if (made_vars && vars->c.count == 0) json_obj_del(c->state, "vars");
        }
    } else {
        if (present) json_set(c->state, "turn_event", json_copy(prev));
        else json_obj_del(c->state, "turn_event");
    }
    json_free(t);
}

/* getattr(obj, attr, None) over the JSON double model: only objects carry
 * attributes; a present null reads as None. Borrowed. */
static json_t *scx2_getattr(const json_t *obj, const char *attr) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, attr);
}

/* ===========================================================================
 * Ported flows
 * =========================================================================== */

/* PoP: scx2_runtime_cwd @ gateway/session_context.py:_runtime_cwd */
/* try: from agent import runtime_cwd; getattr(runtime_cwd, func)(*args)
 * except Exception: pass. import_fail models the halted in-function import
 * (ModuleNotFoundError, swallowed, no seam event); runtime_cwd_attrs lists
 * the module double's attributes — a missing one raises AttributeError
 * (swallowed, no seam event). BaseException kinds pass through. */
static json_t *scx2_runtime_cwd(scx2_ctx_t *c, const json_t *args) {
    const char *func = json_get_str(args, "func", "");
    if (json_get_bool(c->in, "import_fail", false)) return NULL;
    const json_t *attrs = json_obj_get(c->in, "runtime_cwd_attrs");
    if (attrs && attrs->type == JSON_ARRAY) {
        bool found = false;
        for (size_t i = 0; i < json_len(attrs); i++)
            if (scx2_str_eq(json_get(attrs, i), func)) { found = true; break; }
        if (!found) return NULL; /* AttributeError swallowed by except Exception */
    }
    const json_t *argv = json_obj_get(args, "args");
    json_t *a = json_object();
    json_set(a, "args", (argv && argv->type == JSON_ARRAY) ? json_copy(argv) : json_array());
    char *name = scx2_fmt("runtime_cwd.%s", func);
    json_t *r = scx2_call(c, name, a);
    free(name);
    if (scx2_is_raise(r)) {
        const char *kind = json_get_str(r, "__raise__", "");
        if (scx2_exc_is_exception(kind)) { json_free(r); return NULL; }
        return r; /* BaseException passthrough */
    }
    json_free(r);
    return NULL;
}

/* PoP: scx2_scoped_turn_actor @ gateway/session_context.py:scoped_turn_actor */
/* CM flow: bind the event's sender (else its source) as actor, run the turn's
 * running_follow_up CM around the body, restore the three tokens in reverse
 * order in finally. See the header for the enter/exit raise semantics. */
static json_t *scx2_scoped_turn_actor(scx2_ctx_t *c, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    const json_t *turn = json_obj_get(args, "turn");
    /* actor = event if getattr(event,"user_id",None) is not None
     *         else getattr(event,"source",None) */
    const json_t *uid = scx2_getattr(event, "user_id");
    const json_t *actor = !scx2_is_null(uid) ? event : scx2_getattr(event, "source");
    char *aid = scx2_pystr_or_empty(scx2_getattr(actor, "user_id"));
    char *aname = scx2_pystr_or_empty(scx2_getattr(actor, "user_name"));
    json_t *t1 = scx2_set_var(c, "HERMES_SESSION_ACTOR_ID", json_string(aid));
    json_t *t2 = scx2_set_var(c, "HERMES_SESSION_ACTOR_NAME", json_string(aname));
    json_t *t3 = scx2_set_turn_event(c, event ? json_copy(event) : json_null());
    free(aid); free(aname);

    json_t *pending = NULL;
    json_t *cm = NULL;
    bool entered = false;
    if (!turn || turn->type != JSON_OBJECT) {
        pending = scx2_attr_error(turn, "running_follow_up");
    } else {
        json_t *a = json_object();
        scx2_jset(a, "event", event);
        cm = scx2_call(c, "turn.running_follow_up", a);
        if (scx2_is_raise(cm)) {
            pending = cm; cm = NULL;
        } else if (!cm || cm->type != JSON_OBJECT) {
            pending = scx2_cm_error(cm);
            json_free(cm); cm = NULL;
        } else {
            json_t *ea = json_object();
            json_set(ea, "cm", json_copy(cm));
            json_t *er = scx2_call(c, "turn.running_follow_up.__enter__", ea);
            if (scx2_is_raise(er)) pending = er;
            else { entered = true; json_free(er); }
        }
    }
    if (!pending) {
        json_t *body = scx2_call(c, "body", json_object());
        if (scx2_is_raise(body)) pending = body;
        else json_free(body);
    }
    if (entered) {
        json_t *xa = json_object();
        json_set(xa, "cm", json_copy(cm));
        if (pending) json_set(xa, "exc", json_copy(pending));
        json_t *xr = scx2_call(c, "turn.running_follow_up.__exit__", xa);
        if (scx2_is_raise(xr)) {
            json_free(pending); pending = xr;   /* exit raise replaces pending */
        } else {
            if (scx2_truthy(xr)) { json_free(pending); pending = NULL; }
            json_free(xr);
        }
    }
    /* finally: reversed(tokens) */
    scx2_reset_token(c, t3);
    scx2_reset_token(c, t2);
    scx2_reset_token(c, t1);
    json_free(cm);
    return pending; /* NULL == the CM drove the block; ret None */
}

/* PoP: scx2_scoped_session_key @ gateway/session_context.py:scoped_session_key */
/* CM flow: token = _SESSION_KEY.set(session_key); body; finally reset. */
static json_t *scx2_scoped_session_key(scx2_ctx_t *c, const json_t *args) {
    const json_t *key = json_obj_get(args, "session_key");
    json_t *t = scx2_set_var(c, "HERMES_SESSION_KEY", key ? json_copy(key) : json_null());
    json_t *body = scx2_call(c, "body", json_object());
    scx2_reset_token(c, t); /* finally — runs on every path */
    if (scx2_is_raise(body)) return body;
    json_free(body);
    return NULL;
}

/* PoP: scx2_bound_session_key @ gateway/session_context.py:bound_session_key */
/* value = _SESSION_KEY.get(); "" if value is _UNSET else value. */
static json_t *scx2_bound_session_key(scx2_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *v = scx2_var_get(c, "HERMES_SESSION_KEY");
    if (!v || scx2_is_unset(v)) return json_string("");
    return json_copy(v);
}

/* PoP: scx2_source_route_metadata @ gateway/session_context.py:source_route_metadata */
/* anchors = {key: str(value) for key in ("scope_id","parent_chat_id")
 *            if (value := getattr(source, key, None))}
 * return {**(metadata or {}), **anchors} if anchors else metadata */
static json_t *scx2_source_route_metadata(scx2_ctx_t *c, const json_t *args) {
    (void)c;
    const json_t *source = json_obj_get(args, "source");
    const json_t *meta = json_obj_get(args, "metadata");
    static const char *keys[2] = { "scope_id", "parent_chat_id" };
    json_t *anchors = json_object();
    for (int i = 0; i < 2; i++) {
        const json_t *v = scx2_getattr(source, keys[i]);
        if (scx2_truthy(v)) {
            char *s = scx2_pystr(v);
            json_set(anchors, keys[i], json_string(s));
            free(s);
        }
    }
    if (anchors->c.count > 0) {
        if (scx2_truthy(meta) && meta->type != JSON_OBJECT) {
            json_free(anchors);
            return scx2_mapping_error(meta);
        }
        json_t *out = (meta && meta->type == JSON_OBJECT) ? json_copy(meta) : json_object();
        for (size_t i = 0; i < anchors->c.count; i++)
            json_set(out, anchors->c.keys[i], json_copy(anchors->c.items[i]));
        json_free(anchors);
        return out;
    }
    json_free(anchors);
    return meta ? json_copy(meta) : json_null();
}

/* PoP: scx2_bound_session_value @ gateway/session_context.py:_bound_session_value */
/* Actor names read "" while the running turn event's user_id is "" (another
 * sender folded into the turn); otherwise the _VAR_MAP value or _UNSET. */
static json_t *scx2_bound_session_value(scx2_ctx_t *c, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    const json_t *te = json_obj_get(c->state, "turn_event");
    const json_t *uid = scx2_getattr(te, "user_id");
    if ((strcmp(name, "HERMES_SESSION_ACTOR_ID") == 0
         || strcmp(name, "HERMES_SESSION_ACTOR_NAME") == 0)
        && scx2_str_eq(uid, ""))
        return json_string("");
    if (!scx2_var_known(name)) return scx2_unset();
    json_t *v = scx2_var_get(c, name);
    return v ? json_copy(v) : scx2_unset();
}

/* PoP: scx2_session_history_delivery_supported @ gateway/session_context.py:session_history_delivery_supported */
/* return _SESSION_HISTORY_DELIVERY.get() == "1" — fail closed on _UNSET. */
static json_t *scx2_session_history_delivery_supported(scx2_ctx_t *c, const json_t *args) {
    (void)args;
    const json_t *v = json_obj_get(c->state, "history_delivery");
    return json_bool(scx2_str_eq(v, "1"));
}
