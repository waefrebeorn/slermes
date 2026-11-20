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

/* -- Python float repr (shortest round-trip; json.dumps float formatting) -- */
static char *tma_pyfloat_repr(double d) {
    if (isnan(d)) return tma_strdup("NaN");
    if (isinf(d)) return tma_strdup(d > 0 ? "Infinity" : "-Infinity");
    char buf[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    /* normalise the exponent to Python's shape: e+NN / e-NN (min 2 digits) */
    char *e = strchr(buf, 'e');
    if (!e) e = strchr(buf, 'E');
    if (e) {
        char *p = e + 1;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        long exp = strtol(p, NULL, 10);
        snprintf(e, sizeof buf - (size_t)(e - buf), "e%c%02ld", neg ? '-' : '+', exp);
    } else if (!strpbrk(buf, ".nNiI")) {
        /* integral float keeps the .0 (json.dumps(2.0) == "2.0") */
        size_t n = strlen(buf);
        if (n + 2 < sizeof buf) { buf[n] = '.'; buf[n+1] = '0'; buf[n+2] = '\0'; }
    }
    return tma_strdup(buf);
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
