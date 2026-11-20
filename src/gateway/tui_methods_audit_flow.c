/*
 * tui_methods_audit_flow.c -- FLOW + leaf port of tui_gateway/methods_session_control.py
 * (hermes-agent): the structured Desktop session-control audit surface --
 * session.control.read (the stable allowlisted audit view of one live session's
 * goal/loop/heartbeat state), session.control (the closed intent-action set),
 * the safe snapshot projections, the canonical revision digest, the publish
 * pipeline and the manager/dispatch action leaves.
 *
 * MODULE RESOLUTION: the task named tui_gateway/methods_audit.py, which does
 * NOT exist in the quarry (no methods_audit.py anywhere; the TUI method surface
 * is the methods_*.py family). The fully UNCLAIMED audit/read surface is
 * tui_gateway/methods_session_control.py -- session.control.read is literally
 * the allowlisted audit view of persisted session state. PoP credit is to that
 * REAL defining module (tui_gateway/methods_session_control.py:<fn>). Verified
 * unclaimed: no PoP in src/ references methods_session_control.py.
 *
 * The module's functions are rebound onto server.py's globals at install time
 * (method_ctx.bind_module), so _err / _ok / _emit / _sess_nowait /
 * _session_profile_runtime_scope / _methods / logger are bare free variables.
 * ALL of them are seams here (external to methods_session_control.py). _err /
 * _ok / _sess_nowait are CLAIMED by tui_server_flow.c / tui_methods_sessions_flow.c
 * -- seamed, never re-ported. Ported siblings compose LIVE (_safe_*_snapshot /
 * _extract_wait_barrier / _snapshot_revision / _snapshot_updated_at inside
 * _snapshot_control; _load_* / _goal_blocks_loop_tick inside _snapshot_control;
 * _session_publish_lock + _snapshot_control inside _publish_session_control_snapshot;
 * _validate_action_args / _dispatch_command / _execute_manager_action /
 * _manager_error_message / _dispatch_envelope / _publish_session_control_snapshot
 * inside the session.control handler; _execute_subgoal_action /
 * _execute_heartbeat_action inside _execute_manager_action).
 *
 * Ports the DECISION SEQUENCES -- branch order, error-code/message composition,
 * seam call order, dict-literal evaluation order, session-dict mutation history
 * and exception paths. Every live external call enters through ONE universal
 * seam: tma_call(ctx, name, args). The harness wires it to a recording stub fed
 * the case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tma_-prefixed. SHA-256 and the canonical
 * json.dumps(sort_keys=True, ensure_ascii=False, separators=(",",":"))
 * serializer are hand-rolled below (the gate links libjson only).
 *
 * Decorated handlers (def _ under @method / @_profile_scoped) are ported BARE
 * (registry decorators stripped): tma_control_read (session.control.read),
 * tma_control (session.control). register is ported as the bind_module seam.
 *
 * Seam name -> live call (args are informational projections):
 *   err                          _err(rid, code, msg)  [server global; claimed]
 *   ok                           _ok(rid, result)  [claimed sibling]
 *   emit                         _emit("session.control.update", sid, {"control": control})
 *   sess_nowait                  _sess_nowait(params, rid) -> [session, err]  [claimed]
 *   session_profile_runtime_scope _session_profile_runtime_scope(session) -> CM
 *                                marker (dict -> silent CM; non-dict raises
 *                                TypeError at the live `with` site)
 *   methods_get                  _methods.get("command.dispatch") -> handler
 *                                marker (dict) or null; a non-dict non-null
 *                                marker is NOT callable at the call site
 *   dispatch_handler             handler(rid, {"session_id","name","arg"})
 *   logger_debug                 logger.debug(fmt, *args, exc_info=True)
 *                                -> {"msg": fmt % args}  (exc_info is silent)
 *   load_goal                    hermes_cli.goals.load_goal(session_key)
 *   load_loop                    hermes_cli.loops.load_loop(session_key)
 *   load_heartbeat               hermes_cli.heartbeat.load_heartbeat(session_key)
 *   goal_blocks_loop_tick        hermes_cli.loops.goal_blocks_loop_tick(session_key)
 *   contract_to_dict             goal_state.contract.to_dict()  (carrier must be
 *                                a dict double; null/str raise AttributeError
 *                                naming to_dict BEFORE any seam fires)
 *   time_time                    time.time()
 *   new_lock                     threading.Lock() -> lock marker {"__lock__": true}
 *                                (_PUBLISH_LOCK_GUARD is silent; setdefault
 *                                evaluates the factory EAGERLY even on a
 *                                present-null key)
 *   goal_manager_new             GoalManager(session_id=session_key) -> manager
 *                                marker {"state": {"subgoals": [...]}}
 *   manager_add_subgoal          manager.add_subgoal(text) -> text (the double
 *                                APPENDS the returned text to state.subgoals,
 *                                modeling the live append; len is read after)
 *   manager_remove_subgoal       manager.remove_subgoal(index) -> removed text
 *                                (the double pops index-1 when in range)
 *   manager_clear_subgoals       manager.clear_subgoals() -> previous count
 *                                (the double empties state.subgoals)
 *   heartbeat_manager_new        HeartbeatManager(session_id=session_key)
 *   manager_pause                manager.pause() -> heartbeat state dict or null
 *   manager_resume               manager.resume() -> heartbeat state dict or null
 *   manager_clear                manager.clear() -> bool
 *   format_interval              hermes_cli.heartbeat.format_interval(interval_seconds)
 *   bind_module                  method_ctx.bind_module(globals(), server, skip=("_",))
 *                                (server is an opaque handle; only skip projects)
 *
 * Representation contract (shared with tests/oracle_tui_methods_audit_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. The session.control action try catches
 *   ONLY (RuntimeError, ValueError, IndexError) -- the FULL builtin hierarchy
 *   table below walks kinds to those bases (NotImplementedError -> RuntimeError
 *   is caught; KeyError -> LookupError is NOT; unknown scripted kinds are
 *   Exception subclasses and are NOT caught by the narrow tuple).
 * - State doubles (goal/loop/heartbeat/manager/contract): scripted dicts become
 *   SimpleNamespaces (attribute access; a missing required attr raises
 *   "'types.SimpleNamespace' object has no attribute '<key>'"); non-dict targets
 *   raise the live type-name AttributeError at the access site. getattr with a
 *   default (_snapshot_updated_at) never raises. Heartbeat pause/resume state
 *   scripts are null or NON-EMPTY dicts ({} is outside the live domain: the
 *   live HeartbeatState object is always truthy when present).
 * - Locks: threading.Lock() scripts the marker {"__lock__": true}; the session
 *   dict carries the marker (canon projects the live LockDouble back to it).
 *   `with <non-marker>:` raises the py3.14 CM-protocol TypeError.
 * - Session state mirror: ctx.state.session is the live session dict. Direct
 *   _session_publish_lock / _publish cases read it from in.state; handler cases
 *   register the sess_nowait session dict into it (only when it IS a dict).
 *   Mutations (control_publish_lock setdefault, control_revision) land in place.
 * - Manager markers: {"state": {"subgoals": [str, ...]}}; add appends the seam
 *   return, remove pops index-1 (in range), clear empties. Subgoal scripts are
 *   lists of ASCII strings.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints.
 *   Fixture numbers are INTS (the revision digest is float-repr sensitive;
 *   json.dumps writes 1.0 != 1). ASCII corpus only (strip ws set includes
 *   \x1c-\x1f). str() of containers is outside the fixture domain (session_key
 *   / session_id / prompt / text are str/int/null). list() accepts list/str/dict
 *   (keys); subgoals scripts are lists. type(index) is int admits JSON numbers
 *   (fixture ints) and REJECTS bool (type(True) is bool).
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - Fail-closed: a missing/exhausted script is a RECORDED RuntimeError raise
 *   {"no script for X" / "script exhausted for X"} on both sides.
 *
 * Ownership: tma_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node. Seam
 * results are caller-freed. Borrowed nodes (json_obj_get / json_get) are copied
 * before storage.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct tma_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    bool last_raise;    /* the last tma_call produced a scripted/fail-closed raise */
} tma_ctx_t;

/* -- basic helpers -- */
static bool tma_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tma_raise_kind(const json_t *v) {
    return tma_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

static const char *tma_raise_msg(const json_t *v) {
    return tma_is_raise(v) ? json_get_str(v, "message", "") : "";
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tma_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool tma_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static bool tma_is_str(const json_t *v) {
    return v && v->type == JSON_STRING;
}

static const char *tma_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tma_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tma_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static char *tma_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). */
static char *tma_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tma_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tma_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tma_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tma_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* str(v or ""): every falsy value collapses to the empty string. */
static char *tma_pystr_or_empty(const json_t *v) {
    return tma_truthy(v) ? tma_pystr(v) : tma_strdup("");
}

/* Python str.strip() whitespace: ASCII White_Space INCLUDING \x1c-\x1f. */
static bool tma_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || (c >= 0x1c && c <= 0x1f);
}

static char *tma_strip(const char *s) {
    if (!s) s = "";
    while (*s && tma_is_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tma_is_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static bool tma_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tma_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return false;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return tma_str_eq(a->str_val, b->str_val);
    if (a->type != b->type) return false;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tma_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tma_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* -- exception hierarchy: FULL builtin table -- */
typedef struct { const char *child; const char *parent; } tma_exc_rel_t;
static const tma_exc_rel_t TMA_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"TypeError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ArithmeticError", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"OSError", "Exception"},
    {"IOError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FileExistsError", "OSError"},
    {"PermissionError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"InterruptedError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ReferenceError", "Exception"},
    {"Warning", "Exception"},
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
    {"ExceptionGroup", "Exception"},
    {"BaseExceptionGroup", "BaseException"},
};

static const char *tma_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(TMA_EXC_PARENTS) / sizeof(TMA_EXC_PARENTS[0]); i++)
        if (tma_str_eq(TMA_EXC_PARENTS[i].child, kind))
            return TMA_EXC_PARENTS[i].parent;
    /* unknown scripted kind: the oracle builds type(kind, (Exception,), {}) */
    return "Exception";
}

/* true when `kind` is `target` or walks to it through the builtin hierarchy. */
static bool tma_kind_is(const char *kind, const char *target) {
    const char *k = kind;
    for (int depth = 0; k && depth < 16; depth++) {
        if (tma_str_eq(k, target)) return true;
        if (tma_str_eq(k, "Exception") || tma_str_eq(k, "BaseException")) return false;
        k = tma_exc_parent(k);
    }
    return false;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tma_is_base_kind(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

static bool tma_passthrough(const json_t *r) {
    const char *k = tma_raise_kind(r);
    return tma_is_base_kind(k) || !tma_kind_is(k, "Exception");
}

/* the session.control action try: except (RuntimeError, ValueError, IndexError) */
static bool tma_action_catch(const json_t *r) {
    const char *k = tma_raise_kind(r);
    return tma_kind_is(k, "RuntimeError") || tma_kind_is(k, "ValueError")
        || tma_kind_is(k, "IndexError");
}

/* -- live-error spec builders (the exact py3.14 messages) -- */
static json_t *tma_attr_error(const json_t *obj, const char *key) {
    char msg[192];
    if (obj && obj->type == JSON_OBJECT)
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", key);
    else
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 tma_type_name(obj), key);
    return tma_err_spec("AttributeError", msg);
}

static json_t *tma_get_error(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tma_type_name(v));
    return tma_err_spec("AttributeError", msg);
}

static json_t *tma_cm_error(const json_t *v) {
    char msg[192];
    snprintf(msg, sizeof msg,
             "'%s' object does not support the context manager protocol "
             "(missed __exit__ method)", tma_type_name(v));
    return tma_err_spec("TypeError", msg);
}

static json_t *tma_iter_error(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", tma_type_name(v));
    return tma_err_spec("TypeError", msg);
}

static json_t *tma_not_callable_error(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not callable", tma_type_name(v));
    return tma_err_spec("TypeError", msg);
}

static json_t *tma_membership_error(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "argument of type '%s' is not iterable", tma_type_name(v));
    return tma_err_spec("TypeError", msg);
}

static json_t *tma_len_error(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "object of type '%s' has no len()", tma_type_name(v));
    return tma_err_spec("TypeError", msg);
}

/* Attribute access on a SimpleNamespace double: borrowed value (or borrowed
 * default when has_default and absent); missing required attribute raises the
 * live AttributeError. Non-dict targets raise the type-name message (getattr
 * with a default never raises). */
static const json_t *tma_attr(const json_t *obj, const char *key,
                              bool has_default, const json_t *def, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        if (has_default) return def;
        *raise = tma_attr_error(obj, key);
        return NULL;
    }
    if (has_default) return def;
    *raise = tma_attr_error(obj, key);
    return NULL;
}

/* d.get(key) -- borrowed value or NULL (absent == None); non-dict raises. */
static const json_t *tma_get(const json_t *obj, const char *key, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) return json_obj_get(obj, key);
    *raise = tma_get_error(obj);
    return NULL;
}

/* d[key] -- borrowed value or the live KeyError spec (str(KeyError) == repr(key)). */
static const json_t *tma_subscript(const json_t *obj, const char *key, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        char msg[192];
        snprintf(msg, sizeof msg, "'%s'", key);
        *raise = tma_err_spec("KeyError", msg);
        return NULL;
    }
    char msg[160];
    if (!obj || obj->type == JSON_NULL)
        snprintf(msg, sizeof msg, "'NoneType' object is not subscriptable");
    else if (obj->type == JSON_STRING)
        snprintf(msg, sizeof msg, "string indices must be integers, not 'str'");
    else if (obj->type == JSON_ARRAY)
        snprintf(msg, sizeof msg, "list indices must be integers or slices, not str");
    else
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable", tma_type_name(obj));
    *raise = tma_err_spec("TypeError", msg);
    return NULL;
}

/* a, b = v -- the live unpack messages (fixture domain: 2-lists, null/scalars). */
static json_t *tma_unpack2(const json_t *v, json_t **a, json_t **b) {
    *a = *b = NULL;
    if (v && v->type == JSON_ARRAY) {
        if (v->c.count < 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tma_err_spec("ValueError", msg);
        }
        if (v->c.count > 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tma_err_spec("ValueError", msg);
        }
        *a = json_copy(v->c.items[0]);
        *b = json_copy(v->c.items[1]);
        return NULL;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", tma_type_name(v));
    return tma_err_spec("TypeError", msg);
}

/* list(v) -- array copy / str chars (ASCII) / dict keys; else TypeError. */
static json_t *tma_list_copy(const json_t *v, json_t **raise) {
    *raise = NULL;
    if (!v || v->type == JSON_NULL) { *raise = tma_iter_error(v); return NULL; }
    if (v->type == JSON_ARRAY) return json_copy(v);
    if (v->type == JSON_OBJECT) {
        json_t *o = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(o, json_string(v->c.keys[i]));
        return o;
    }
    if (v->type == JSON_STRING) {
        json_t *o = json_array();
        const char *s = v->str_val ? v->str_val : "";
        for (size_t i = 0; s[i]; i++) {
            char ch[2] = { s[i], '\0' };
            json_append(o, json_string(ch));
        }
        return o;
    }
    *raise = tma_iter_error(v);
    return NULL;
}

/* len(v) on the fixture domain (list/str/dict). */
static long tma_len(const json_t *v, json_t **raise) {
    *raise = NULL;
    if (v && v->type == JSON_ARRAY) return (long)v->c.count;
    if (v && v->type == JSON_OBJECT) return (long)v->c.count;
    if (v && v->type == JSON_STRING) return (long)strlen(v->str_val ? v->str_val : "");
    *raise = tma_len_error(v);
    return -1;
}

/* a < b -- numeric (bool is int); else the live TypeError. */
static bool tma_lt(const json_t *a, const json_t *b, json_t **raise) {
    *raise = NULL;
    bool an = a && (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    bool bn = b && (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av < bv;
    }
    char msg[160];
    snprintf(msg, sizeof msg, "'<' not supported between instances of '%s' and '%s'",
             tma_type_name(a), tma_type_name(b));
    *raise = tma_err_spec("TypeError", msg);
    return false;
}

/* "error" in v -- dict key / str substring / list element; else TypeError. */
static json_t *tma_contains_error(const json_t *v, bool *out) {
    *out = false;
    if (v && v->type == JSON_OBJECT) { *out = json_has(v, "error"); return NULL; }
    if (v && v->type == JSON_STRING) {
        *out = strstr(v->str_val ? v->str_val : "", "error") != NULL;
        return NULL;
    }
    if (v && v->type == JSON_ARRAY) {
        json_t *needle = json_string("error");
        for (size_t i = 0; i < v->c.count; i++)
            if (tma_json_eq(v->c.items[i], needle)) { *out = true; break; }
        json_free(needle);
        return NULL;
    }
    return tma_membership_error(v);
}

/* type(v) is int -- JSON numbers are fixture ints; bool is NOT int. */
static bool tma_type_is_int(const json_t *v) {
    return v && v->type == JSON_NUMBER;
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tma_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tma_err_spec(type, msg);
}

static json_t *tma_call(tma_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    ctx->last_raise = false;
    /* drop null-valued arg keys on both sides (absent-vs-null parity) */
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tma_err_spec("RuntimeError", msg);
        ctx->last_raise = true;
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tma_err_spec("RuntimeError", msg);
            ctx->last_raise = true;
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tma_make_exc(raise);
                ctx->last_raise = true;
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

/* args builders (values borrowed; copied into the args object) */
static json_t *tma_args0(void) { return json_object(); }
static void tma_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tma_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tma_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tma_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _err(rid, code, msg) / _ok(rid, result) -- server-global envelope seams. */
static json_t *tma_err(tma_ctx_t *ctx, const json_t *rid, int code, const char *msg) {
    json_t *a = tma_args0();
    tma_aset(a, "rid", rid);
    tma_asetn(a, "code", (double)code);
    tma_asets(a, "message", msg);
    return tma_call(ctx, "err", a);
}

static json_t *tma_ok(tma_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tma_args0();
    tma_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null()); /* adopted */
    return tma_call(ctx, "ok", a);
}

/* logger.debug(fmt, *args, exc_info=True) -> {"msg": fmt % args} */
static json_t *tma_logger_debug(tma_ctx_t *ctx, const char *msg) {
    json_t *a = tma_args0();
    tma_asets(a, "msg", msg);
    return tma_call(ctx, "logger_debug", a);
}

/* -- SHA-256 (hand-rolled; the gate links libjson only) -- */
static uint32_t tma_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static const uint32_t TMA_SHA_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
};

static char *tma_sha256_hex(const unsigned char *data, size_t len) {
    uint32_t h[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u,
    };
    size_t newlen = len + 1;
    while (newlen % 64 != 56) newlen++;
    unsigned char *msg = calloc(newlen + 8, 1);
    if (!msg) return NULL;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) msg[newlen + (size_t)i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xff);
    for (size_t off = 0; off < newlen + 8; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[off + (size_t)i * 4] << 24)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 1] << 16)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 2] << 8)
                 | ((uint32_t)msg[off + (size_t)i * 4 + 3]);
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = tma_rotr(w[i-15],7) ^ tma_rotr(w[i-15],18) ^ (w[i-15] >> 3);
            uint32_t s1 = tma_rotr(w[i-2],17) ^ tma_rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = tma_rotr(e,6) ^ tma_rotr(e,11) ^ tma_rotr(e,25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + TMA_SHA_K[i] + w[i];
            uint32_t S0 = tma_rotr(a,2) ^ tma_rotr(a,13) ^ tma_rotr(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(msg);
    char *out = malloc(65);
    if (!out) return NULL;
    for (int i = 0; i < 8; i++)
        snprintf(out + i * 8, 9, "%08x", h[i]);
    out[64] = '\0';
    return out;
}

/* -- Python float repr (shortest round-trip digits, rendered with Python's
 * notation rule: exponential iff decimal exp < -4 or >= 16) -- */
static char *tma_pyfloat_repr(double d) {
    if (isnan(d)) return tma_strdup("NaN");
    if (isinf(d)) return tma_strdup(d > 0 ? "Infinity" : "-Infinity");
    char buf[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    /* parse the shortest round-trip %g output into sign / digits / exponent */
    const char *p = buf;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    else if (*p == '+') p++;
    long exp = 0;
    char *e = strchr(p, 'e');
    if (!e) e = strchr(p, 'E');
    char mant[48];
    if (e) {
        exp = strtol(e + 1, NULL, 10);
        size_t mn = (size_t)(e - p);
        if (mn >= sizeof mant) mn = sizeof mant - 1;
        memcpy(mant, p, mn);
        mant[mn] = '\0';
    } else {
        snprintf(mant, sizeof mant, "%s", p);
    }
    char *dot = strchr(mant, '.');
    char digits[48];
    size_t dn = 0;
    long ip_len = dot ? (long)(dot - mant) : (long)strlen(mant);
    for (const char *q = mant; *q; q++)
        if (*q != '.') digits[dn++] = *q;
    digits[dn] = '\0';
    size_t nz = 0;
    while (nz < dn && digits[nz] == '0') nz++;
    if (nz == dn) return tma_strdup(neg ? "-0.0" : "0.0");
    long E = (ip_len - 1) - (long)nz + exp; /* decimal exp of the first digit */
    const char *D = digits + nz;
    size_t Dn = dn - nz;
    char out[80];
    size_t k = 0;
    if (neg) out[k++] = '-';
    if (E >= -4 && E < 16) {
        long point = E + 1;
        if (point <= 0) {
            out[k++] = '0'; out[k++] = '.';
            for (long i = 0; i < -point; i++) out[k++] = '0';
            memcpy(out + k, D, Dn); k += Dn;
        } else if ((size_t)point >= Dn) {
            memcpy(out + k, D, Dn); k += Dn;
            for (long i = 0; i < point - (long)Dn; i++) out[k++] = '0';
            out[k++] = '.'; out[k++] = '0';
        } else {
            memcpy(out + k, D, (size_t)point); k += (size_t)point;
            out[k++] = '.';
            memcpy(out + k, D + point, Dn - (size_t)point); k += Dn - (size_t)point;
        }
    } else {
        out[k++] = D[0];
        if (Dn > 1) {
            out[k++] = '.';
            memcpy(out + k, D + 1, Dn - 1); k += Dn - 1;
        }
        k += (size_t)snprintf(out + k, sizeof out - k, "e%c%02ld",
                              E < 0 ? '-' : '+', E < 0 ? -E : E);
    }
    out[k] = '\0';
    return tma_strdup(out);
}

/* -- canonical json.dumps(sort_keys=True, ensure_ascii=False, separators=(",",":")) -- */
typedef struct { char *buf; size_t len, cap; } tma_sbuf_t;

static bool tma_sb_reserve(tma_sbuf_t *sb, size_t extra) {
    if (sb->len + extra + 1 <= sb->cap) return true;
    size_t nc = sb->cap ? sb->cap : 128;
    while (nc < sb->len + extra + 1) nc *= 2;
    char *nb = realloc(sb->buf, nc);
    if (!nb) return false;
    sb->buf = nb;
    sb->cap = nc;
    return true;
}

static void tma_sb_putc(tma_sbuf_t *sb, char c) {
    if (tma_sb_reserve(sb, 1)) { sb->buf[sb->len++] = c; sb->buf[sb->len] = '\0'; }
}

static void tma_sb_puts(tma_sbuf_t *sb, const char *s) {
    size_t n = strlen(s);
    if (tma_sb_reserve(sb, n)) { memcpy(sb->buf + sb->len, s, n); sb->len += n; sb->buf[sb->len] = '\0'; }
}

/* Python json string escaping (ensure_ascii=False): ", \, the short control
 * escapes, \u00xx for other C0; every other byte (UTF-8) passes through raw. */
static void tma_sb_json_string(tma_sbuf_t *sb, const char *s) {
    if (!s) s = "";
    tma_sb_putc(sb, '"');
    for (size_t i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"': tma_sb_puts(sb, "\\\""); break;
            case '\\': tma_sb_puts(sb, "\\\\"); break;
            case '\b': tma_sb_puts(sb, "\\b"); break;
            case '\f': tma_sb_puts(sb, "\\f"); break;
            case '\n': tma_sb_puts(sb, "\\n"); break;
            case '\r': tma_sb_puts(sb, "\\r"); break;
            case '\t': tma_sb_puts(sb, "\\t"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof esc, "\\u%04x", c);
                    tma_sb_puts(sb, esc);
                } else {
                    tma_sb_putc(sb, (char)c);
                }
        }
    }
    tma_sb_putc(sb, '"');
}

static int tma_key_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void tma_canon_rec(const json_t *v, tma_sbuf_t *sb) {
    if (!v || v->type == JSON_NULL) { tma_sb_puts(sb, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: tma_sb_puts(sb, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18) {
                char buf[32];
                snprintf(buf, sizeof buf, "%lld", (long long)d);
                tma_sb_puts(sb, buf);
            } else {
                char *r = tma_pyfloat_repr(d);
                if (r) { tma_sb_puts(sb, r); free(r); }
            }
            return;
        }
        case JSON_STRING: tma_sb_json_string(sb, v->str_val); return;
        case JSON_ARRAY:
            tma_sb_putc(sb, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) tma_sb_putc(sb, ',');
                tma_canon_rec(v->c.items[i], sb);
            }
            tma_sb_putc(sb, ']');
            return;
        case JSON_OBJECT: {
            tma_sb_putc(sb, '{');
            size_t n = v->c.count;
            if (n) {
                const char **keys = malloc(n * sizeof(char *));
                if (keys) {
                    for (size_t i = 0; i < n; i++) keys[i] = v->c.keys[i];
                    qsort(keys, n, sizeof(char *), tma_key_cmp);
                    for (size_t i = 0; i < n; i++) {
                        if (i) tma_sb_putc(sb, ',');
                        tma_sb_json_string(sb, keys[i]);
                        tma_sb_putc(sb, ':');
                        tma_canon_rec(json_obj_get(v, keys[i]), sb);
                    }
                    free(keys);
                }
            }
            tma_sb_putc(sb, '}');
            return;
        }
        default: tma_sb_puts(sb, "null"); return;
    }
}

static char *tma_canonical(const json_t *v) {
    tma_sbuf_t sb = { NULL, 0, 0 };
    tma_canon_rec(v, &sb);
    if (!sb.buf) return tma_strdup("");
    return sb.buf;
}

/* -- action tables (module constants) -- */
typedef struct { const char *action; const char *name; const char *arg; } tma_cmd_t;
static const tma_cmd_t TMA_ACTION_COMMAND_MAP[] = {
    {"goal.pause", "goal", "pause"},
    {"goal.resume", "goal", "resume"},
    {"goal.clear", "goal", "clear"},
    {"goal.unwait", "goal", "unwait"},
    {"loop.pause", "loop", "pause"},
    {"loop.resume", "loop", "resume"},
    {"loop.stop", "loop", "stop"},
};
static const char *TMA_MANAGER_ACTIONS[] = {
    "subgoal.add", "subgoal.remove", "subgoal.clear",
    "heartbeat.pause", "heartbeat.resume", "heartbeat.clear",
};

static const tma_cmd_t *tma_action_map_find(const char *action) {
    for (size_t i = 0; i < sizeof(TMA_ACTION_COMMAND_MAP) / sizeof(TMA_ACTION_COMMAND_MAP[0]); i++)
        if (tma_str_eq(TMA_ACTION_COMMAND_MAP[i].action, action))
            return &TMA_ACTION_COMMAND_MAP[i];
    return NULL;
}

static bool tma_action_valid(const char *action) {
    if (tma_action_map_find(action)) return true;
    for (size_t i = 0; i < sizeof(TMA_MANAGER_ACTIONS) / sizeof(TMA_MANAGER_ACTIONS[0]); i++)
        if (tma_str_eq(TMA_MANAGER_ACTIONS[i], action)) return true;
    return false;
}

/* -- claimed-sibling seam helpers -- */

/* _sess_nowait(params, rid) -> [session, err]. At seam return, element 0 (when
 * the pair is a non-empty array) is mirrored into ctx.state.session BEFORE the
 * unpack (the oracle's build registers the same mirror at the same point, so a
 * bad-unpack case still mirrors element 0 on both sides). The flow then uses
 * the BORROWED state node; *err_out is owned. Returns a raise spec or NULL. */
static json_t *tma_sess_nowait(tma_ctx_t *ctx, const json_t *params, const json_t *rid,
                               json_t **session_out, json_t **err_out) {
    *session_out = *err_out = NULL;
    json_t *a = tma_args0();
    tma_aset(a, "params", params);
    tma_aset(a, "rid", rid);
    json_t *pair = tma_call(ctx, "sess_nowait", a);
    if (tma_is_raise(pair)) return pair;
    if (pair && pair->type == JSON_ARRAY && pair->c.count >= 1)
        json_set(ctx->state, "session", json_copy(pair->c.items[0]));
    json_t *u[2];
    json_t *bad = tma_unpack2(pair, u, u + 1);
    json_free(pair);
    if (bad) return bad;
    json_free(u[0]); /* the flow reads the state mirror, not this copy */
    *err_out = u[1];
    *session_out = json_obj_get(ctx->state, "session"); /* borrowed */
    return NULL;
}

/* printf-style malloc'd formatter (live f-string / %-format composition). */
static char *tma_xfmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* a > b -- numeric (bool is int); else the live TypeError (max() compares
 * each candidate AGAINST the running max: item > maxval). */
static bool tma_gt(const json_t *a, const json_t *b, json_t **raise) {
    *raise = NULL;
    bool an = a && (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    bool bn = b && (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av > bv;
    }
    char msg[160];
    snprintf(msg, sizeof msg, "'>' not supported between instances of '%s' and '%s'",
             tma_type_name(a), tma_type_name(b));
    *raise = tma_err_spec("TypeError", msg);
    return false;
}

/* {"result": {"type": "exec", "output": output}} -- the manager-action envelope. */
static json_t *tma_exec_result(char *output) {
    json_t *inner = json_object();
    json_set_hdr(inner, "type", "exec");
    json_set_hdr(inner, "output", output ? output : "");
    free(output);
    json_t *o = json_object();
    json_set(o, "result", inner);
    return o;
}

/* == _extract_wait_barrier == */
static json_t *tma_extract_wait_barrier_v(tma_ctx_t *ctx, const json_t *state) {
    json_t *raise = NULL;
    const json_t *wr = tma_attr(state, "waiting_reason", false, NULL, &raise);
    if (raise) return raise;
    json_t *reason = tma_truthy(wr) ? json_copy(wr) : json_string("");
    const json_t *wu = tma_attr(state, "waiting_until", false, NULL, &raise);
    if (raise) { json_free(reason); return raise; }
    if (tma_truthy(wu)) {
        json_t *now = tma_call(ctx, "time_time", tma_args0());
        if (tma_is_raise(now)) { json_free(reason); return now; }
        json_t *lt_raise = NULL;
        bool less = tma_lt(now, wu, &lt_raise);
        json_free(now);
        if (lt_raise) { json_free(reason); return lt_raise; }
        if (less) {
            json_t *o = json_object();
            json_set_hdr(o, "type", "until");
            json_set(o, "until_at", json_copy(wu));
            json_set(o, "reason", reason); /* adopted */
            return o;
        }
    }
    const json_t *ws = tma_attr(state, "waiting_on_session", false, NULL, &raise);
    if (raise) { json_free(reason); return raise; }
    if (!tma_is_none(ws)) {
        json_t *o = json_object();
        json_set_hdr(o, "type", "session");
        json_set(o, "target", json_copy(ws));
        json_set(o, "reason", reason);
        return o;
    }
    const json_t *wp = tma_attr(state, "waiting_on_pid", false, NULL, &raise);
    if (raise) { json_free(reason); return raise; }
    if (!tma_is_none(wp)) {
        json_t *o = json_object();
        json_set_hdr(o, "type", "pid");
        json_set(o, "target", json_copy(wp));
        json_set(o, "reason", reason);
        return o;
    }
    json_free(reason);
    return NULL; /* None */
}

/* PoP: tma_extract_wait_barrier @ tui_gateway/methods_session_control.py:_extract_wait_barrier */
static json_t *tma_extract_wait_barrier(tma_ctx_t *ctx, const json_t *args) {
    return tma_extract_wait_barrier_v(ctx, json_obj_get(args, "state"));
}

/* == _safe_goal_snapshot == */
static json_t *tma_safe_goal_snapshot_v(tma_ctx_t *ctx, const json_t *state) {
    if (tma_is_none(state)) return NULL;
    json_t *raise = NULL;
    const json_t *status = tma_attr(state, "status", false, NULL, &raise);
    if (raise) return raise;
    if (tma_is_str(status) && tma_str_eq(status->str_val, "cleared")) return NULL;
    json_t *o = json_object();
    /* dict-literal evaluation order: title, status, turns_used, max_turns,
     * contract.to_dict(), list(subgoals), the gates comprehension */
    const json_t *goal = tma_attr(state, "goal", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    json_set(o, "title", json_copy(goal));
    const json_t *st2 = tma_attr(state, "status", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    json_set(o, "status", json_copy(st2));
    const json_t *tu = tma_attr(state, "turns_used", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    json_set(o, "turns_used", json_copy(tu));
    const json_t *mt = tma_attr(state, "max_turns", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    json_set(o, "max_turns", json_copy(mt));
    const json_t *contract = tma_attr(state, "contract", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    /* carrier check BEFORE the seam: non-dict contract raises to_dict-named */
    if (!contract || contract->type != JSON_OBJECT) {
        json_free(o);
        return tma_attr_error(contract, "to_dict");
    }
    json_t *cd = tma_call(ctx, "contract_to_dict", tma_args0());
    if (tma_is_raise(cd)) { json_free(o); return cd; }
    json_set(o, "contract", cd);
    const json_t *sg = tma_attr(state, "subgoals", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    json_t *sgl = tma_list_copy(sg, &raise);
    if (raise) { json_free(o); return raise; }
    json_set(o, "subgoals", sgl);
    const json_t *gates = tma_attr(state, "gates", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    json_t *gl = tma_list_copy(gates, &raise);
    if (raise) { json_free(o); return raise; }
    json_t *garr = json_array();
    static const char *GATE_FIELDS[5] = {
        "command", "timeout_seconds", "max_retries", "attempts", "last_exit_code"
    };
    for (size_t i = 0; i < json_len(gl); i++) {
        const json_t *gate = json_get(gl, i);
        json_t *gd = json_object();
        for (int k = 0; k < 5; k++) {
            const json_t *gv = tma_attr(gate, GATE_FIELDS[k], false, NULL, &raise);
            if (raise) {
                json_free(gd); json_free(garr); json_free(gl); json_free(o);
                return raise;
            }
            json_set(gd, GATE_FIELDS[k], json_copy(gv));
        }
        json_append(garr, gd);
    }
    json_free(gl);
    json_set(o, "gates", garr);
    /* truthy-only optional fields, in live order */
    const json_t *ca = tma_attr(state, "created_at", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    if (tma_truthy(ca)) json_set(o, "created_at", json_copy(ca));
    const json_t *lta = tma_attr(state, "last_turn_at", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    if (tma_truthy(lta)) json_set(o, "updated_at", json_copy(lta));
    const json_t *pr = tma_attr(state, "paused_reason", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    if (tma_truthy(pr)) json_set(o, "paused_reason", json_copy(pr));
    const json_t *lv = tma_attr(state, "last_verdict", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    if (tma_truthy(lv)) json_set(o, "last_verdict", json_copy(lv));
    const json_t *lr = tma_attr(state, "last_reason", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    if (tma_truthy(lr)) json_set(o, "last_reason", json_copy(lr));
    /* if barrier := _extract_wait_barrier(state) */
    json_t *barrier = tma_extract_wait_barrier_v(ctx, state);
    if (tma_is_raise(barrier)) { json_free(o); return barrier; }
    if (tma_truthy(barrier)) json_set(o, "wait_barrier", barrier);
    else json_free(barrier);
    return o;
}

/* PoP: tma_safe_goal_snapshot @ tui_gateway/methods_session_control.py:_safe_goal_snapshot */
static json_t *tma_safe_goal_snapshot(tma_ctx_t *ctx, const json_t *args) {
    return tma_safe_goal_snapshot_v(ctx, json_obj_get(args, "state"));
}

/* == _safe_loop_snapshot == */
static json_t *tma_safe_loop_snapshot_v(tma_ctx_t *ctx, const json_t *state, bool deferred) {
    if (tma_is_none(state)) return NULL;
    json_t *raise = NULL;
    const json_t *status = tma_attr(state, "status", false, NULL, &raise);
    if (raise) return raise;
    if (tma_is_str(status) && tma_str_eq(status->str_val, "cleared")) return NULL;
    json_t *o = json_object();
    static const char *LOOP_FIELDS[13] = {
        "prompt", "status", "mode", "interval_seconds", "current_delay", "times",
        "until", "max_ticks", "ticks_fired", "created_at", "last_fired_at",
        "next_due_at", "awaiting_response"
    };
    for (int k = 0; k < 13; k++) {
        const json_t *v = tma_attr(state, LOOP_FIELDS[k], false, NULL, &raise);
        if (raise) { json_free(o); return raise; }
        json_set(o, LOOP_FIELDS[k], json_copy(v));
    }
    json_set(o, "deferred_by_goal", json_bool(deferred));
    const json_t *pr = tma_attr(state, "paused_reason", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    if (tma_truthy(pr)) json_set(o, "paused_reason", json_copy(pr));
    const json_t *lsr = tma_attr(state, "last_stop_reason", false, NULL, &raise);
    if (raise) { json_free(o); return raise; }
    if (tma_truthy(lsr)) json_set(o, "last_stop_reason", json_copy(lsr));
    return o;
}

/* PoP: tma_safe_loop_snapshot @ tui_gateway/methods_session_control.py:_safe_loop_snapshot */
static json_t *tma_safe_loop_snapshot(tma_ctx_t *ctx, const json_t *args) {
    return tma_safe_loop_snapshot_v(ctx, json_obj_get(args, "state"),
                                    json_get_bool(args, "deferred_by_goal", false));
}

/* == _safe_heartbeat_snapshot == */
static json_t *tma_safe_heartbeat_snapshot_v(tma_ctx_t *ctx, const json_t *state) {
    if (tma_is_none(state)) return NULL;
    json_t *raise = NULL;
    const json_t *status = tma_attr(state, "status", false, NULL, &raise);
    if (raise) return raise;
    if (tma_is_str(status) && tma_str_eq(status->str_val, "cleared")) return NULL;
    json_t *o = json_object();
    static const char *HB_FIELDS[6] = {
        "prompt", "status", "interval_seconds", "created_at", "last_fired_at", "fire_count"
    };
    for (int k = 0; k < 6; k++) {
        const json_t *v = tma_attr(state, HB_FIELDS[k], false, NULL, &raise);
        if (raise) { json_free(o); return raise; }
        json_set(o, HB_FIELDS[k], json_copy(v));
    }
    return o;
}

/* PoP: tma_safe_heartbeat_snapshot @ tui_gateway/methods_session_control.py:_safe_heartbeat_snapshot */
static json_t *tma_safe_heartbeat_snapshot(tma_ctx_t *ctx, const json_t *args) {
    return tma_safe_heartbeat_snapshot_v(ctx, json_obj_get(args, "state"));
}

/* == _snapshot_revision (canonical json.dumps + sha256) == */
static json_t *tma_snapshot_revision_v(tma_ctx_t *ctx, const json_t *goal,
                                       const json_t *loop, const json_t *heartbeat) {
    (void)ctx;
    if (tma_is_none(goal) && tma_is_none(loop) && tma_is_none(heartbeat))
        return json_string("");
    json_t *o = json_object();
    json_set(o, "goal", goal ? json_copy(goal) : json_null());
    json_set(o, "loop", loop ? json_copy(loop) : json_null());
    json_set(o, "heartbeat", heartbeat ? json_copy(heartbeat) : json_null());
    char *canon = tma_canonical(o);
    json_free(o);
    char *hex = tma_sha256_hex((const unsigned char *)canon, strlen(canon));
    free(canon);
    json_t *r = json_string(hex ? hex : "");
    free(hex);
    return r;
}

/* PoP: tma_snapshot_revision @ tui_gateway/methods_session_control.py:_snapshot_revision */
static json_t *tma_snapshot_revision(tma_ctx_t *ctx, const json_t *args) {
    return tma_snapshot_revision_v(ctx, json_obj_get(args, "goal"),
                                   json_obj_get(args, "loop"),
                                   json_obj_get(args, "heartbeat"));
}

/* == _snapshot_updated_at (persisted timestamps only; getattr default 0) == */
static json_t *tma_snapshot_updated_at_v(tma_ctx_t *ctx, const json_t *goal_state,
                                         const json_t *loop_state,
                                         const json_t *heartbeat_state) {
    (void)ctx;
    const json_t *states[3] = { goal_state, loop_state, heartbeat_state };
    static const char *FIELDS[3][2] = {
        {"created_at", "last_turn_at"},
        {"created_at", "last_fired_at"},
        {"created_at", "last_fired_at"},
    };
    json_t *zero = json_number(0);
    json_t *maxnode = NULL;
    json_t *err = NULL;
    for (int r = 0; r < 3 && !err; r++) {
        if (tma_is_none(states[r])) continue;
        for (int f = 0; f < 2 && !err; f++) {
            json_t *raise = NULL;
            const json_t *v = tma_attr(states[r], FIELDS[r][f], true, zero, &raise);
            if (raise) { err = raise; break; }
            if (!tma_truthy(v)) continue;
            if (!maxnode) { maxnode = json_copy(v); continue; }
            json_t *gr = NULL;
            bool greater = tma_gt(v, maxnode, &gr);
            if (gr) { err = gr; break; }
            if (greater) { json_free(maxnode); maxnode = json_copy(v); }
        }
    }
    json_free(zero);
    if (err) { json_free(maxnode); return err; }
    return maxnode ? maxnode : json_number(0);
}

/* PoP: tma_snapshot_updated_at @ tui_gateway/methods_session_control.py:_snapshot_updated_at */
static json_t *tma_snapshot_updated_at(tma_ctx_t *ctx, const json_t *args) {
    return tma_snapshot_updated_at_v(ctx, json_obj_get(args, "goal_state"),
                                     json_obj_get(args, "loop_state"),
                                     json_obj_get(args, "heartbeat_state"));
}

/* == the load wrappers (thin in-function-import seams) == */
static json_t *tma_load_goal_state_v(tma_ctx_t *ctx, const char *session_key) {
    json_t *a = tma_args0();
    tma_asets(a, "session_key", session_key);
    return tma_call(ctx, "load_goal", a);
}

/* PoP: tma_load_goal_state @ tui_gateway/methods_session_control.py:_load_goal_state */
static json_t *tma_load_goal_state(tma_ctx_t *ctx, const json_t *args) {
    return tma_load_goal_state_v(ctx, json_get_str(args, "session_key", ""));
}

static json_t *tma_load_loop_state_v(tma_ctx_t *ctx, const char *session_key) {
    json_t *a = tma_args0();
    tma_asets(a, "session_key", session_key);
    return tma_call(ctx, "load_loop", a);
}

/* PoP: tma_load_loop_state @ tui_gateway/methods_session_control.py:_load_loop_state */
static json_t *tma_load_loop_state(tma_ctx_t *ctx, const json_t *args) {
    return tma_load_loop_state_v(ctx, json_get_str(args, "session_key", ""));
}

static json_t *tma_load_heartbeat_state_v(tma_ctx_t *ctx, const char *session_key) {
    json_t *a = tma_args0();
    tma_asets(a, "session_key", session_key);
    return tma_call(ctx, "load_heartbeat", a);
}

/* PoP: tma_load_heartbeat_state @ tui_gateway/methods_session_control.py:_load_heartbeat_state */
static json_t *tma_load_heartbeat_state(tma_ctx_t *ctx, const json_t *args) {
    return tma_load_heartbeat_state_v(ctx, json_get_str(args, "session_key", ""));
}

static json_t *tma_goal_blocks_loop_tick_v(tma_ctx_t *ctx, const char *session_key) {
    json_t *a = tma_args0();
    tma_asets(a, "session_key", session_key);
    return tma_call(ctx, "goal_blocks_loop_tick", a);
}

/* PoP: tma_goal_blocks_loop_tick @ tui_gateway/methods_session_control.py:_goal_blocks_loop_tick */
static json_t *tma_goal_blocks_loop_tick(tma_ctx_t *ctx, const json_t *args) {
    return tma_goal_blocks_loop_tick_v(ctx, json_get_str(args, "session_key", ""));
}

/* == _snapshot_control (the audit read composition) == */
static json_t *tma_snapshot_control_v(tma_ctx_t *ctx, const char *session_key) {
    json_t *gs = tma_load_goal_state_v(ctx, session_key);
    if (tma_is_raise(gs)) return gs;
    json_t *ls = tma_load_loop_state_v(ctx, session_key);
    if (tma_is_raise(ls)) { json_free(gs); return ls; }
    json_t *hs = tma_load_heartbeat_state_v(ctx, session_key);
    if (tma_is_raise(hs)) { json_free(gs); json_free(ls); return hs; }
    /* deferred_by_goal: strict left-to-right short-circuit */
    bool deferred = false;
    json_t *raise = NULL;
    if (!tma_is_none(ls)) {
        const json_t *lst = tma_attr(ls, "status", false, NULL, &raise);
        if (raise) { json_free(gs); json_free(ls); json_free(hs); return raise; }
        if (tma_is_str(lst) && tma_str_eq(lst->str_val, "active") && !tma_is_none(gs)) {
            const json_t *gst = tma_attr(gs, "status", false, NULL, &raise);
            if (raise) { json_free(gs); json_free(ls); json_free(hs); return raise; }
            if (tma_is_str(gst) && tma_str_eq(gst->str_val, "active")) {
                json_t *blocks = tma_goal_blocks_loop_tick_v(ctx, session_key);
                if (tma_is_raise(blocks)) {
                    json_free(gs); json_free(ls); json_free(hs);
                    return blocks;
                }
                deferred = tma_truthy(blocks);
                json_free(blocks);
            }
        }
    }
    json_t *goal = tma_safe_goal_snapshot_v(ctx, gs);
    if (tma_is_raise(goal)) { json_free(gs); json_free(ls); json_free(hs); return goal; }
    json_t *loop = tma_safe_loop_snapshot_v(ctx, ls, deferred);
    if (tma_is_raise(loop)) {
        json_free(goal); json_free(gs); json_free(ls); json_free(hs);
        return loop;
    }
    json_t *hb = tma_safe_heartbeat_snapshot_v(ctx, hs);
    if (tma_is_raise(hb)) {
        json_free(goal); json_free(loop); json_free(gs); json_free(ls); json_free(hs);
        return hb;
    }
    /* dict-literal order: revision BEFORE updated_at */
    json_t *rev = tma_snapshot_revision_v(ctx, goal, loop, hb);
    json_t *upd = tma_snapshot_updated_at_v(ctx, gs, ls, hs);
    if (tma_is_raise(upd)) {
        json_free(rev); json_free(goal); json_free(loop); json_free(hb);
        json_free(gs); json_free(ls); json_free(hs);
        return upd;
    }
    json_t *o = json_object();
    json_set(o, "goal", goal ? goal : json_null());
    json_set(o, "loop", loop ? loop : json_null());
    json_set(o, "heartbeat", hb ? hb : json_null());
    json_set(o, "revision", rev);
    json_set(o, "updated_at", upd);
    json_free(gs); json_free(ls); json_free(hs);
    return o;
}

/* PoP: tma_snapshot_control @ tui_gateway/methods_session_control.py:_snapshot_control */
static json_t *tma_snapshot_control(tma_ctx_t *ctx, const json_t *args) {
    return tma_snapshot_control_v(ctx, json_get_str(args, "session_key", ""));
}

/* == _session_publish_lock (setdefault evaluates the factory EAGERLY) == */
static json_t *tma_session_publish_lock_v(tma_ctx_t *ctx, json_t *session) {
    json_t *raise = NULL;
    const json_t *lock = tma_get(session, "control_publish_lock", &raise);
    if (raise) return raise;
    if (tma_is_none(lock)) {
        /* _PUBLISH_LOCK_GUARD is silent (single-threaded) */
        json_t *nl = tma_call(ctx, "new_lock", tma_args0());
        if (tma_is_raise(nl)) return nl;
        if (json_has(session, "control_publish_lock")) {
            /* present-null (or any present value): setdefault keeps it */
            json_free(nl);
            return json_copy(json_obj_get(session, "control_publish_lock"));
        }
        json_set(session, "control_publish_lock", nl); /* adopted */
        return json_copy(nl);
    }
    return json_copy(lock);
}

/* PoP: tma_session_publish_lock @ tui_gateway/methods_session_control.py:_session_publish_lock */
static json_t *tma_session_publish_lock(tma_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tma_session_publish_lock_v(ctx, json_obj_get(ctx->state, "session"));
}

/* == _publish_session_control_snapshot == */
static json_t *tma_publish_v(tma_ctx_t *ctx, const json_t *sid, bool only_if_present,
                             bool only_if_changed, json_t *session) {
    if (!tma_truthy(session)) return NULL;
    json_t *raise = NULL;
    const json_t *skv = tma_get(session, "session_key", &raise);
    if (raise) return raise; /* OUTSIDE the try */
    char *sk = tma_pystr_or_empty(skv);
    if (!sk[0]) { free(sk); return NULL; }
    json_t *exc = NULL;
    json_t *lock = NULL;
    json_t *scope = NULL;
    json_t *control = NULL;
    json_t *er = NULL;
    lock = tma_session_publish_lock_v(ctx, session);
    if (tma_is_raise(lock)) {
        if (tma_passthrough(lock)) { free(sk); return lock; }
        exc = lock; goto catch;
    }
    /* `with lock:` -- only the lock marker is a CM */
    if (!(lock && lock->type == JSON_OBJECT && json_has(lock, "__lock__"))) {
        exc = tma_cm_error(lock);
        json_free(lock);
        goto catch;
    }
    json_free(lock); /* silent CM */
    {
        json_t *sa = tma_args0();
        tma_aset(sa, "session", session);
        scope = tma_call(ctx, "session_profile_runtime_scope", sa);
    }
    if (tma_is_raise(scope)) {
        if (tma_passthrough(scope)) { free(sk); return scope; }
        exc = scope; goto catch;
    }
    if (!(scope && scope->type == JSON_OBJECT)) {
        exc = tma_cm_error(scope);
        json_free(scope);
        goto catch;
    }
    json_free(scope); /* silent CM enter/exit */
    control = tma_snapshot_control_v(ctx, sk);
    if (tma_is_raise(control)) {
        if (tma_passthrough(control)) { free(sk); return control; }
        exc = control; goto catch;
    }
    {
        const json_t *rev = json_obj_get(control, "revision");
        bool skip = false;
        if (only_if_present && !tma_truthy(rev)) skip = true;
        if (!skip && only_if_changed) {
            const json_t *prev = json_obj_get(session, "control_revision");
            json_t *def = json_string("");
            if (tma_json_eq(rev, prev ? prev : def)) skip = true;
            json_free(def);
        }
        if (skip) { json_free(control); free(sk); return NULL; }
        json_set(session, "control_revision", json_copy(rev));
    }
    {
        json_t *payload = json_object();
        json_set(payload, "control", control); /* adopted */
        json_t *ea = tma_args0();
        tma_asets(ea, "event", "session.control.update");
        tma_aset(ea, "sid", sid);
        json_set(ea, "payload", payload);
        er = tma_call(ctx, "emit", ea);
    }
    if (tma_is_raise(er)) {
        if (tma_passthrough(er)) { free(sk); return er; }
        exc = er; goto catch;
    }
    json_free(er);
    free(sk);
    return NULL;
catch:
    {
        char *sids = tma_pystr(sid);
        char *msg = tma_xfmt("session.control.update publish failed for %s", sids);
        free(sids);
        json_t *lgr = tma_logger_debug(ctx, msg ? msg : "");
        free(msg);
        json_free(exc);
        if (tma_is_raise(lgr)) { free(sk); return lgr; } /* logger raise propagates */
        json_free(lgr);
    }
    free(sk);
    return NULL;
}

/* PoP: tma_publish_session_control_snapshot @ tui_gateway/methods_session_control.py:_publish_session_control_snapshot */
static json_t *tma_publish_session_control_snapshot(tma_ctx_t *ctx, const json_t *args) {
    return tma_publish_v(ctx, json_obj_get(args, "sid"),
                         json_get_bool(args, "only_if_present", false),
                         json_get_bool(args, "only_if_changed", false),
                         json_obj_get(ctx->state, "session"));
}

/* == _validate_action_args -> (validated, error) == */
static json_t *tma_validate_v(tma_ctx_t *ctx, const json_t *rid, const char *action,
                              const json_t *pargs, json_t **validated, json_t **verr) {
    *validated = *verr = NULL;
    if (tma_str_eq(action, "subgoal.add")) {
        json_t *raise = NULL;
        const json_t *text = tma_get(pargs, "text", &raise);
        if (raise) return raise;
        char *stripped = tma_is_str(text) ? tma_strip(text->str_val) : NULL;
        if (!tma_is_str(text) || !stripped || !stripped[0]) {
            free(stripped);
            *verr = tma_err(ctx, rid, 4004, "subgoal text is required");
            if (tma_is_raise(*verr)) return *verr;
            return NULL;
        }
        json_t *v = json_object();
        json_set_hdr(v, "text", stripped);
        free(stripped);
        *validated = v;
        return NULL;
    }
    if (tma_str_eq(action, "subgoal.remove")) {
        json_t *raise = NULL;
        const json_t *index = tma_get(pargs, "index", &raise);
        if (raise) return raise;
        if (!tma_type_is_int(index)) {
            *verr = tma_err(ctx, rid, 4004, "subgoal index must be an integer");
            if (tma_is_raise(*verr)) return *verr;
            return NULL;
        }
        if (index->num_val < 1) {
            *verr = tma_err(ctx, rid, 4004, "subgoal index must be >= 1");
            if (tma_is_raise(*verr)) return *verr;
            return NULL;
        }
        json_t *v = json_object();
        json_set(v, "index", json_copy(index));
        *validated = v;
        return NULL;
    }
    *validated = json_object();
    return NULL;
}

/* PoP: tma_validate_action_args @ tui_gateway/methods_session_control.py:_validate_action_args */
static json_t *tma_validate_action_args(tma_ctx_t *ctx, const json_t *args) {
    json_t *validated = NULL, *verr = NULL;
    json_t *bad = tma_validate_v(ctx, json_obj_get(args, "rid"),
                                 json_get_str(args, "action", ""),
                                 json_obj_get(args, "args"), &validated, &verr);
    if (bad) {
        json_free(validated);
        if (verr != bad) json_free(verr); /* bad may BE verr (err-seam raise) */
        return bad;
    }
    json_t *arr = json_array();
    json_append(arr, validated ? validated : json_null());
    json_append(arr, verr ? verr : json_null());
    return arr;
}

/* == _dispatch_command (the command.dispatch delegation) == */
static json_t *tma_dispatch_command_v(tma_ctx_t *ctx, const json_t *rid,
                                      const json_t *session_id, const char *name,
                                      const char *arg) {
    json_t *ma = tma_args0();
    tma_asets(ma, "name", "command.dispatch");
    json_t *handler = tma_call(ctx, "methods_get", ma);
    if (tma_is_raise(handler)) return handler; /* OUTSIDE the try */
    if (tma_is_none(handler))
        return tma_err(ctx, rid, 5031, "command.dispatch unavailable");
    json_t *r = NULL;
    if (handler->type != JSON_OBJECT) {
        /* live handler(rid, ...) on a non-callable: TypeError INSIDE the try */
        r = tma_not_callable_error(handler);
    } else {
        json_t *pa = json_object();
        tma_aset(pa, "session_id", session_id);
        tma_asets(pa, "name", name);
        tma_asets(pa, "arg", arg);
        json_t *ha = tma_args0();
        tma_aset(ha, "rid", rid);
        json_set(ha, "params", pa);
        r = tma_call(ctx, "dispatch_handler", ha);
    }
    json_free(handler);
    if (tma_is_raise(r)) {
        if (tma_passthrough(r)) return r;
        char *msg = tma_xfmt("command.dispatch %s %s failed: %s", name, arg, tma_raise_msg(r));
        json_t *lgr = tma_logger_debug(ctx, msg ? msg : "");
        if (tma_is_raise(lgr)) { free(msg); json_free(r); return lgr; }
        json_free(lgr);
        char *em = tma_xfmt("dispatch failed: %s", tma_raise_msg(r));
        json_t *er = tma_err(ctx, rid, 5031, em ? em : "");
        free(em); free(msg); json_free(r);
        return er;
    }
    return r;
}

/* PoP: tma_dispatch_command @ tui_gateway/methods_session_control.py:_dispatch_command */
static json_t *tma_dispatch_command(tma_ctx_t *ctx, const json_t *args) {
    return tma_dispatch_command_v(ctx, json_obj_get(args, "rid"),
                                  json_obj_get(args, "session_id"),
                                  json_get_str(args, "name", ""),
                                  json_get_str(args, "arg", ""));
}

/* == _execute_subgoal_action == */
static json_t *tma_execute_subgoal_action_v(tma_ctx_t *ctx, const char *session_key,
                                            const char *action, const json_t *pargs) {
    json_t *ma = tma_args0();
    tma_asets(ma, "session_id", session_key);
    json_t *manager = tma_call(ctx, "goal_manager_new", ma);
    if (tma_is_raise(manager)) return manager;
    bool is_add = tma_str_eq(action, "subgoal.add");
    bool is_remove = tma_str_eq(action, "subgoal.remove");
    const char *method = is_add ? "add_subgoal" : is_remove ? "remove_subgoal" : "clear_subgoals";
    /* the OUTER attribute resolves before the argument subscript */
    if (!manager || manager->type != JSON_OBJECT) {
        json_t *e = tma_attr_error(manager, method);
        json_free(manager);
        return e;
    }
    if (is_add) {
        json_t *raise = NULL;
        const json_t *text = tma_subscript(pargs, "text", &raise);
        if (raise) { json_free(manager); return raise; }
        json_t *aa = tma_args0();
        tma_aset(aa, "text", text);
        json_t *ret = tma_call(ctx, "manager_add_subgoal", aa);
        if (tma_is_raise(ret)) { json_free(manager); return ret; }
        /* model the live append: state.subgoals grows by the returned text */
        const json_t *mstate = tma_attr(manager, "state", false, NULL, &raise);
        if (raise) { json_free(ret); json_free(manager); return raise; }
        const json_t *sg = tma_attr(mstate, "subgoals", false, NULL, &raise);
        if (raise) { json_free(ret); json_free(manager); return raise; }
        if (!sg || sg->type != JSON_ARRAY) {
            json_t *e = tma_attr_error(sg, "append");
            json_free(ret); json_free(manager);
            return e;
        }
        json_append((json_t *)sg, json_copy(ret));
        long count = (long)sg->c.count;
        char *texts = tma_pystr(ret);
        char *out = tma_xfmt("\xe2\x9c\x93 Added subgoal %ld: %s", count, texts);
        free(texts); json_free(ret); json_free(manager);
        return tma_exec_result(out);
    }
    if (is_remove) {
        json_t *raise = NULL;
        const json_t *index = tma_subscript(pargs, "index", &raise);
        if (raise) { json_free(manager); return raise; }
        json_t *ra = tma_args0();
        tma_aset(ra, "index", index);
        json_t *ret = tma_call(ctx, "manager_remove_subgoal", ra);
        if (tma_is_raise(ret)) { json_free(manager); return ret; }
        /* the post-remove subgoals list is never read: the pop is not modeled */
        char *idxs = tma_pystr(index);
        char *texts = tma_pystr(ret);
        char *out = tma_xfmt("\xe2\x9c\x93 Removed subgoal %s: %s", idxs, texts);
        free(idxs); free(texts); json_free(ret); json_free(manager);
        return tma_exec_result(out);
    }
    /* subgoal.clear (and any other subgoal.* action: live falls through) */
    json_t *ret = tma_call(ctx, "manager_clear_subgoals", tma_args0());
    if (tma_is_raise(ret)) { json_free(manager); return ret; }
    char *out = NULL;
    if (tma_truthy(ret)) {
        char *cs = tma_pystr(ret);
        json_t *one = json_number(1);
        bool singular = tma_json_eq(ret, one);
        json_free(one);
        out = tma_xfmt("\xe2\x9c\x93 Cleared %s subgoal%s.", cs, singular ? "" : "s");
        free(cs);
    } else {
        out = tma_strdup("No subgoals to clear.");
    }
    json_free(ret); json_free(manager);
    return tma_exec_result(out);
}

/* PoP: tma_execute_subgoal_action @ tui_gateway/methods_session_control.py:_execute_subgoal_action */
static json_t *tma_execute_subgoal_action(tma_ctx_t *ctx, const json_t *args) {
    return tma_execute_subgoal_action_v(ctx, json_get_str(args, "session_key", ""),
                                        json_get_str(args, "action", ""),
                                        json_obj_get(args, "args"));
}

/* == _execute_heartbeat_action == */
static json_t *tma_execute_heartbeat_action_v(tma_ctx_t *ctx, const char *session_key,
                                              const char *action) {
    json_t *ma = tma_args0();
    tma_asets(ma, "session_id", session_key);
    json_t *manager = tma_call(ctx, "heartbeat_manager_new", ma);
    if (tma_is_raise(manager)) return manager;
    bool is_pause = tma_str_eq(action, "heartbeat.pause");
    bool is_resume = tma_str_eq(action, "heartbeat.resume");
    bool is_clear = tma_str_eq(action, "heartbeat.clear");
    const char *method = is_pause ? "pause" : is_resume ? "resume" : is_clear ? "clear" : NULL;
    if (!method) {
        json_free(manager);
        char *em = tma_xfmt("unknown heartbeat action: %s", action);
        json_t *er = tma_err(ctx, NULL, 4004, em ? em : "");
        free(em);
        return er;
    }
    if (!manager || manager->type != JSON_OBJECT) {
        json_t *e = tma_attr_error(manager, method);
        json_free(manager);
        return e;
    }
    json_t *ret = tma_call(ctx, is_pause ? "manager_pause" : is_resume ? "manager_resume" : "manager_clear",
                           tma_args0());
    if (tma_is_raise(ret)) { json_free(manager); return ret; }
    char *out = NULL;
    json_t *raise = NULL;
    if (is_pause) {
        if (tma_truthy(ret)) {
            const json_t *prompt = tma_attr(ret, "prompt", false, NULL, &raise);
            if (raise) { json_free(ret); json_free(manager); return raise; }
            char *ps = tma_pystr(prompt);
            out = tma_xfmt("\xe2\x8f\xb8 Heartbeat paused: %s", ps);
            free(ps);
        } else {
            out = tma_strdup("No heartbeat set.");
        }
    } else if (is_resume) {
        if (tma_truthy(ret)) {
            /* f-string order: format_interval(state.interval_seconds) FIRST */
            const json_t *iv = tma_attr(ret, "interval_seconds", false, NULL, &raise);
            if (raise) { json_free(ret); json_free(manager); return raise; }
            json_t *fa = tma_args0();
            tma_aset(fa, "interval_seconds", iv);
            json_t *fi = tma_call(ctx, "format_interval", fa);
            if (tma_is_raise(fi)) { json_free(ret); json_free(manager); return fi; }
            const json_t *prompt = tma_attr(ret, "prompt", false, NULL, &raise);
            if (raise) { json_free(fi); json_free(ret); json_free(manager); return raise; }
            char *fis = tma_pystr(fi);
            char *ps = tma_pystr(prompt);
            out = tma_xfmt("\xe2\x96\xb6 Heartbeat resumed (every %s): %s", fis, ps);
            free(fis); free(ps); json_free(fi);
        } else {
            out = tma_strdup("No heartbeat to resume.");
        }
    } else {
        out = tma_strdup(tma_truthy(ret) ? "\xe2\x9c\x93 Heartbeat cleared." : "No heartbeat set.");
    }
    json_free(ret); json_free(manager);
    return tma_exec_result(out);
}

/* PoP: tma_execute_heartbeat_action @ tui_gateway/methods_session_control.py:_execute_heartbeat_action */
static json_t *tma_execute_heartbeat_action(tma_ctx_t *ctx, const json_t *args) {
    return tma_execute_heartbeat_action_v(ctx, json_get_str(args, "session_key", ""),
                                          json_get_str(args, "action", ""));
}

/* == _execute_manager_action (pure dispatch) == */
static json_t *tma_execute_manager_action_v(tma_ctx_t *ctx, const char *session_key,
                                            const char *action, const json_t *pargs) {
    if (strncmp(action, "subgoal.", 8) == 0)
        return tma_execute_subgoal_action_v(ctx, session_key, action, pargs);
    return tma_execute_heartbeat_action_v(ctx, session_key, action);
}

/* PoP: tma_execute_manager_action @ tui_gateway/methods_session_control.py:_execute_manager_action */
static json_t *tma_execute_manager_action(tma_ctx_t *ctx, const json_t *args) {
    return tma_execute_manager_action_v(ctx, json_get_str(args, "session_key", ""),
                                        json_get_str(args, "action", ""),
                                        json_obj_get(args, "args"));
}

/* == _manager_error_message (pure) == */
static json_t *tma_manager_error_message_v(tma_ctx_t *ctx, const char *action,
                                           const json_t *exc) {
    (void)ctx;
    const char *prefix = action;
    if (tma_str_eq(action, "subgoal.add")) prefix = "/subgoal";
    else if (tma_str_eq(action, "subgoal.remove")) prefix = "/subgoal remove";
    else if (tma_str_eq(action, "subgoal.clear")) prefix = "/subgoal clear";
    char *es = tma_is_raise(exc) ? tma_strdup(tma_raise_msg(exc)) : tma_pystr(exc);
    char *out = tma_xfmt("%s: %s", prefix, es ? es : "");
    free(es);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* PoP: tma_manager_error_message @ tui_gateway/methods_session_control.py:_manager_error_message */
static json_t *tma_manager_error_message(tma_ctx_t *ctx, const json_t *args) {
    return tma_manager_error_message_v(ctx, json_get_str(args, "action", ""),
                                       json_obj_get(args, "exc"));
}

/* == _dispatch_envelope (pure) == */
static json_t *tma_dispatch_envelope_v(tma_ctx_t *ctx, const json_t *response) {
    (void)ctx;
    json_t *raise = NULL;
    const json_t *result = tma_get(response, "result", &raise);
    if (raise) return raise;
    const json_t *r = tma_truthy(result) ? result : NULL; /* result or {} */
    if (r && r->type != JSON_OBJECT) return tma_get_error(r);
    json_t *o = json_object();
    static const char *KEYS[5] = { "type", "output", "notice", "message", "display" };
    for (int k = 0; k < 5; k++) {
        const json_t *v = r ? json_obj_get(r, KEYS[k]) : NULL;
        json_set(o, KEYS[k], v ? json_copy(v) : json_null());
    }
    return o;
}

/* PoP: tma_dispatch_envelope @ tui_gateway/methods_session_control.py:_dispatch_envelope */
static json_t *tma_dispatch_envelope(tma_ctx_t *ctx, const json_t *args) {
    return tma_dispatch_envelope_v(ctx, json_obj_get(args, "response"));
}

/* == session.control.read handler (bare; registry decorators stripped) == */
/* PoP: tma_control_read @ tui_gateway/methods_session_control.py:_ */
static json_t *tma_control_read(tma_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *session = NULL, *err = NULL;
    json_t *bad = tma_sess_nowait(ctx, params, rid, &session, &err);
    if (bad) return bad;
    if (tma_truthy(err)) return err; /* adopted as the return */
    json_free(err);
    json_t *raise = NULL;
    const json_t *skv = tma_get(session, "session_key", &raise);
    if (raise) return raise; /* OUTSIDE the try */
    char *sk = tma_pystr_or_empty(skv);
    if (!sk[0]) {
        free(sk);
        return tma_err(ctx, rid, 4001, "session has no stored key");
    }
    /* try: return _ok(rid, {"control": _snapshot_control(session_key)}) */
    json_t *control = tma_snapshot_control_v(ctx, sk);
    if (tma_is_raise(control)) {
        if (tma_passthrough(control)) { free(sk); return control; }
        char *msg = tma_xfmt("session.control.read failed: %s", tma_raise_msg(control));
        json_t *lgr = tma_logger_debug(ctx, msg ? msg : "");
        if (tma_is_raise(lgr)) { free(msg); free(sk); json_free(control); return lgr; }
        json_free(lgr);
        json_t *er = tma_err(ctx, rid, 5031, msg ? msg : "");
        free(msg); free(sk); json_free(control);
        return er;
    }
    json_t *result = json_object();
    json_set(result, "control", control);
    json_t *okr = tma_ok(ctx, rid, result); /* INSIDE the try */
    free(sk);
    if (tma_is_raise(okr)) {
        if (tma_passthrough(okr)) return okr;
        char *msg = tma_xfmt("session.control.read failed: %s", tma_raise_msg(okr));
        json_t *lgr = tma_logger_debug(ctx, msg ? msg : "");
        if (tma_is_raise(lgr)) { free(msg); json_free(okr); return lgr; }
        json_free(lgr);
        json_t *er = tma_err(ctx, rid, 5031, msg ? msg : "");
        free(msg); json_free(okr);
        return er;
    }
    return okr;
}

/* == session.control handler (bare; registry decorators stripped) == */
/* PoP: tma_control @ tui_gateway/methods_session_control.py:_ */
static json_t *tma_control(tma_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *raise = NULL;
    const json_t *raw_action = tma_get(params, "action", &raise);
    if (raise) return raise;
    if (!tma_is_str(raw_action))
        return tma_err(ctx, rid, 4004, "action is required");
    char *action = tma_strip(raw_action->str_val);
    if (!action || !action[0]) {
        free(action);
        return tma_err(ctx, rid, 4004, "action is required");
    }
    if (strncmp(action, "goal.gate", 9) == 0) {
        free(action);
        return tma_err(ctx, rid, 4004,
                       "gate actions are not allowed through session.control");
    }
    if (!tma_action_valid(action)) {
        char *m = tma_xfmt("unknown action: %s", action);
        json_t *e = tma_err(ctx, rid, 4004, m ? m : "");
        free(m); free(action);
        return e;
    }
    json_t *empty_args = json_object();
    const json_t *pargs = empty_args;
    if (json_has(params, "args")) {
        const json_t *pa = json_obj_get(params, "args");
        if (!pa || pa->type != JSON_OBJECT) {
            free(action); json_free(empty_args);
            return tma_err(ctx, rid, 4004, "args must be an object");
        }
        pargs = pa;
    }
    json_t *validated = NULL, *verr = NULL;
    json_t *vbad = tma_validate_v(ctx, rid, action, pargs, &validated, &verr);
    if (vbad) {
        free(action); json_free(empty_args);
        json_free(validated);
        if (verr != vbad) json_free(verr);
        return vbad;
    }
    if (tma_truthy(verr)) {
        free(action); json_free(empty_args); json_free(validated);
        return verr; /* adopted as the return */
    }
    json_free(verr);
    json_t *session = NULL, *err = NULL;
    json_t *sbad = tma_sess_nowait(ctx, params, rid, &session, &err);
    if (sbad) {
        free(action); json_free(empty_args); json_free(validated);
        return sbad;
    }
    if (tma_truthy(err)) {
        free(action); json_free(empty_args); json_free(validated);
        return err;
    }
    json_free(err);
    const json_t *skv = tma_get(session, "session_key", &raise);
    if (raise) {
        free(action); json_free(empty_args); json_free(validated);
        return raise;
    }
    char *sk = tma_pystr_or_empty(skv);
    if (!sk[0]) {
        free(sk); free(action); json_free(empty_args); json_free(validated);
        return tma_err(ctx, rid, 4001, "session has no stored key");
    }
    /* try: dispatch (map actions) or manager action;
     * except (RuntimeError, ValueError, IndexError) ONLY */
    const tma_cmd_t *cmd = tma_action_map_find(action);
    json_t *action_result = NULL;
    json_t *empty_sid = json_string("");
    if (cmd) {
        const json_t *siv = json_obj_get(params, "session_id");
        const json_t *sid = tma_truthy(siv) ? siv : empty_sid;
        action_result = tma_dispatch_command_v(ctx, rid, sid, cmd->name, cmd->arg);
    } else {
        action_result = tma_execute_manager_action_v(ctx, sk, action, validated);
    }
    if (tma_is_raise(action_result)) {
        if (tma_action_catch(action_result)) {
            json_t *mm = tma_manager_error_message_v(ctx, action, action_result);
            json_t *e = tma_err(ctx, rid, 4004, mm->str_val ? mm->str_val : "");
            json_free(mm); json_free(action_result); json_free(empty_sid);
            free(sk); free(action); json_free(empty_args); json_free(validated);
            return e;
        }
        json_free(empty_sid);
        free(sk); free(action); json_free(empty_args); json_free(validated);
        return action_result; /* everything else propagates */
    }
    /* if "error" in action_result: return action_result */
    bool has_err = false;
    json_t *memb = tma_contains_error(action_result, &has_err);
    if (memb) {
        json_free(action_result); json_free(empty_sid);
        free(sk); free(action); json_free(empty_args); json_free(validated);
        return memb;
    }
    if (has_err) {
        json_free(empty_sid);
        free(sk); free(action); json_free(empty_args); json_free(validated);
        return action_result;
    }
    /* try: control = _snapshot_control(session_key); except Exception */
    json_t *control = tma_snapshot_control_v(ctx, sk);
    if (tma_is_raise(control)) {
        if (tma_passthrough(control)) {
            json_free(action_result); json_free(empty_sid);
            free(sk); free(action); json_free(empty_args); json_free(validated);
            return control;
        }
        char *msg = tma_xfmt("session.control snapshot after %s failed: %s",
                             action, tma_raise_msg(control));
        json_t *lgr = tma_logger_debug(ctx, msg ? msg : "");
        if (tma_is_raise(lgr)) {
            free(msg); json_free(control); json_free(action_result); json_free(empty_sid);
            free(sk); free(action); json_free(empty_args); json_free(validated);
            return lgr;
        }
        json_free(lgr);
        char *em = tma_xfmt("session.control snapshot failed: %s", tma_raise_msg(control));
        json_t *e = tma_err(ctx, rid, 5031, em ? em : "");
        free(em); free(msg); json_free(control); json_free(action_result); json_free(empty_sid);
        free(sk); free(action); json_free(empty_args); json_free(validated);
        return e;
    }
    /* manager actions publish through the one revision-recording publisher */
    if (!cmd) {
        const json_t *siv = json_obj_get(params, "session_id");
        const json_t *sid = tma_truthy(siv) ? siv : empty_sid;
        json_t *pr = tma_publish_v(ctx, sid, false, false, session);
        if (tma_is_raise(pr)) {
            json_free(control); json_free(action_result); json_free(empty_sid);
            free(sk); free(action); json_free(empty_args); json_free(validated);
            return pr;
        }
        json_free(pr);
    }
    json_free(empty_sid);
    json_t *envelope = tma_dispatch_envelope_v(ctx, action_result);
    if (tma_is_raise(envelope)) {
        json_free(control); json_free(action_result);
        free(sk); free(action); json_free(empty_args); json_free(validated);
        return envelope;
    }
    json_t *result = json_object();
    json_set(result, "control", control);
    json_set(result, "dispatch", envelope);
    json_t *okr = tma_ok(ctx, rid, result);
    json_free(action_result);
    free(sk); free(action); json_free(empty_args); json_free(validated);
    return okr;
}

/* == register (bind_module seam; server is an opaque handle) == */
/* PoP: tma_register @ tui_gateway/methods_session_control.py:register */
static json_t *tma_register(tma_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = tma_args0();
    json_t *skip = json_array();
    json_append(skip, json_string("_"));
    json_set(a, "skip", skip);
    json_t *r = tma_call(ctx, "bind_module", a);
    if (tma_is_raise(r)) return r;
    json_free(r); /* live register is an expression statement: returns None */
    return NULL;
}
