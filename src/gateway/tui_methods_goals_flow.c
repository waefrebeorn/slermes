/*
 * tui_methods_goals_flow.c -- FLOW + leaf port of the TUI /goal command methods
 * (hermes-agent): hermes_cli/goal_command.py (the CLI's /goal semantics shared
 * by every goal command surface -- the TUI's methods_tools.py:_cmd_goal calls
 * dispatch_goal_command) plus the UNCLAIMED gate/contract manager leaves of
 * hermes_cli/goals.py that the dispatcher composes (clear / add_gate /
 * remove_gate / clear_gates / render_gates / render_contract /
 * wait_on_session / _check_gates).
 *
 * MODULE RESOLUTION: the task named tui_gateway/methods_goals.py, which does
 * NOT exist in the quarry (the TUI method surface is the methods_*.py family).
 * The goal-command methods live in hermes_cli/goal_command.py (verified fully
 * UNCLAIMED: no PoP in src/ references it); the gate/contract leaves are the
 * unclaimed remainder of hermes_cli/goals.py (the claimed siblings -- pause /
 * resume / set / status_line / has_goal / stop_waiting / wait_on /
 * next_continuation_prompt / _save / _require_goal / _require_active /
 * _pop_item / _clear_items / _park / _pause_decision / state property /
 * parse_contract / draft_contract / goal_kick_prompt / is_empty /
 * render_block / has_contract / _gate_workspace / run_gate / _decision --
 * are SEAMED, never re-ported). PoP credit is to the REAL defining modules
 * (hermes_cli/goal_command.py:<fn>, hermes_cli/goals.py:<fn>).
 *
 * Ports the DECISION SEQUENCES -- branch order, error-message composition,
 * seam call order, f-string/dict-literal evaluation order, state mutation
 * history and exception paths. Every live external call enters through ONE
 * universal seam: tmg_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmg_-prefixed.
 *
 * The mgr carrier is state.mgr: {"session_id": str, "_state": <GoalState dict
 * or null>}. The GoalManager.state property is TRANSPARENT (returns _state;
 * no seam -- the audit-flow GoalManagerDouble precedent). Claimed mgr methods
 * are seams; the ported goals.py leaves read/mutate _state directly and fire
 * the claimed-helper seams (_save / _require_goal / _pop_item / ...).
 *
 * Seam name -> live call (args are informational projections):
 *   render                 render(key, default, **values)  [callback; the
 *                          _english default is ported separately as a leaf]
 *   progress               progress(message)  [callback; present flag in args]
 *   authorize_gate         authorize_gate() -> denial str or null  [callback]
 *   status_line            mgr.status_line() -> str  [claimed]
 *   pause                  mgr.pause(reason=...) -> state dict or null [claimed]
 *   resume                 mgr.resume() -> state dict or null  [claimed]
 *   set                    mgr.set(goal, contract=...) -> state dict [claimed]
 *   has_goal               mgr.has_goal() -> bool  [claimed]
 *   stop_waiting           mgr.stop_waiting() -> bool  [claimed]
 *   wait_on                mgr.wait_on(pid, reason=...) -> state dict [claimed]
 *   next_continuation_prompt mgr.next_continuation_prompt() -> str/null [claimed]
 *   mgr_state              NOT A SEAM: the state property reads _state directly
 *   save                   mgr._save()  [claimed _save]
 *   require_goal           mgr._require_goal() -> state dict  [claimed]
 *   require_active         mgr._require_active() -> state dict  [claimed]
 *   pop_item               mgr._pop_item(attr, index_1based) -> removed gate
 *                          dict  [claimed]
 *   clear_items            mgr._clear_items(attr) -> previous count  [claimed]
 *   park                   mgr._park(reason, **barrier) -> state dict [claimed]
 *   pause_decision         mgr._pause_decision(paused_reason, verdict, reason,
 *                          message) -> decision dict  [claimed]
 *   state_has_contract     state.has_contract() -> bool  [claimed GoalState
 *                          method; carrier must be a dict double, else
 *                          AttributeError naming has_contract BEFORE the seam]
 *   contract_is_empty      contract.is_empty() -> bool  [claimed GoalContract
 *                          method; carrier check as above]
 *   contract_render_block  contract.render_block() -> str  [claimed; carrier]
 *   draft_contract         goals.draft_contract(objective) -> contract dict/null
 *                          [claimed]
 *   parse_contract         goals.parse_contract(text) -> [headline, contract]
 *                          [claimed; list result scripted one level deeper]
 *   goal_kick_prompt       goals.goal_kick_prompt(goal, last_user_message) -> str
 *                          [claimed]
 *   logger_debug           logger.debug("goal draft failed: %s", exc)
 *                          -> {"msg": fmt % args}  (exc_info silent)
 *   gate_workspace         _gate_workspace() -> [cwd, refusal]  [claimed]
 *   run_gate               run_gate(gate, cwd=...) -> [passed, exit_code, tail]
 *                          [claimed; list result one level deeper]
 *   decision               _decision(status, should_continue, prompt, verdict,
 *                          reason, message) -> decision dict  [claimed]
 *
 * Representation contract (shared with the oracle):
 * - None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
 *   BaseException-only kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
 *   CancelledError) pass through every except-Exception / except-(RVI) site.
 * - GoalCommandResult projects to its field dict in dataclass order:
 *   {output, prompt, kickoff, clear_pending, error} (defaults null/false/null/
 *   false). GoalGate projects to {command, timeout_seconds, max_retries,
 *   attempts, last_exit_code, last_output_tail}.
 * - State/contract/gate doubles are dict carriers: a required field read on a
 *   non-dict raises the type-name AttributeError; on a dict missing the field,
 *   the SimpleNamespace AttributeError. Present-null is a valid null read.
 *   Scripted state results are restricted to {null, NON-EMPTY record} where
 *   the live test is TRUTHINESS (_pause's `if state`): a SimpleNamespace({})
 *   is truthy live but an empty JSON object is falsy here. `is None` sites
 *   (_resume) accept {} (it raises AttributeError 'goal' on both sides).
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - Fail-closed: a missing/exhausted script is a RECORDED RuntimeError raise
 *   {"no script for X" / "script exhausted for X"} on both sides. Inside
 *   _gate / dispatch the fail-closed RuntimeError IS caught by the live
 *   except-(RuntimeError, ValueError, IndexError) sites.
 * - List-valued seam RESULTS are wrapped one level deeper (bare array == a
 *   per-call queue): parse_contract / gate_workspace / run_gate.
 * - Tuples project to lists; integral floats to ints. str()/strip()/lower()
 *   fold the ASCII corpus only (str strip ws includes \x1c-\x1f; int() strips
 *   the NARROWER set without \x1c-\x1f). str.format templates carry plain
 *   {name} fields (+ {{/}} escapes); f-string rest!r uses Python repr quoting.
 * - status_line / render / render_block / goal_kick_prompt scripts stay in
 *   their natural str domain (f-string str() of containers is outside it).
 *
 * Ownership: tmg_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node. Seam
 * results are caller-freed. Borrowed state nodes are never freed.
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

/* -- universal seam context -- */
typedef struct tmg_ctx {
    json_t *state;      /* owned; {"mgr": carrier}; emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    bool last_raise;    /* the last tmg_call produced a scripted/fail-closed raise */
} tmg_ctx_t;

/* -- basic helpers -- */
static bool tmg_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmg_raise_kind(const json_t *v) {
    return tmg_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmg_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* FULL builtin exception hierarchy (single-parent table). except-sites walk
 * this to the target class; the BaseException trio + CancelledError stop at
 * BaseException (NOT Exception) so they pass through. Unknown scripted kinds
 * default to the Exception family. */
static const char *tmg_exc_parent(const char *kind) {
    if (!kind) return NULL;
    /* BaseException direct children */
    if (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
        || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError")
        || !strcmp(kind, "Exception")) return "BaseException";
    if (!strcmp(kind, "BaseException")) return NULL;
    /* Exception direct children */
    if (!strcmp(kind, "RuntimeError") || !strcmp(kind, "ValueError")
        || !strcmp(kind, "TypeError") || !strcmp(kind, "AttributeError")
        || !strcmp(kind, "NameError") || !strcmp(kind, "LookupError")
        || !strcmp(kind, "ArithmeticError") || !strcmp(kind, "OSError")
        || !strcmp(kind, "StopIteration") || !strcmp(kind, "StopAsyncIteration")
        || !strcmp(kind, "AssertionError") || !strcmp(kind, "SyntaxError")
        || !strcmp(kind, "SystemError") || !strcmp(kind, "ImportError")
        || !strcmp(kind, "Warning") || !strcmp(kind, "MemoryError")
        || !strcmp(kind, "BufferError") || !strcmp(kind, "EOFError")
        || !strcmp(kind, "ReferenceError")) return "Exception";
    /* deeper subclasses */
    if (!strcmp(kind, "NotImplementedError") || !strcmp(kind, "RecursionError"))
        return "RuntimeError";
    if (!strcmp(kind, "UnicodeError")) return "ValueError";
    if (!strcmp(kind, "UnicodeDecodeError") || !strcmp(kind, "UnicodeEncodeError")
        || !strcmp(kind, "UnicodeTranslateError")) return "UnicodeError";
    if (!strcmp(kind, "IndexError") || !strcmp(kind, "KeyError")) return "LookupError";
    if (!strcmp(kind, "UnboundLocalError")) return "NameError";
    if (!strcmp(kind, "FloatingPointError") || !strcmp(kind, "OverflowError")
        || !strcmp(kind, "ZeroDivisionError")) return "ArithmeticError";
    if (!strcmp(kind, "ConnectionError") || !strcmp(kind, "InterruptedError")
        || !strcmp(kind, "BlockingIOError") || !strcmp(kind, "ChildProcessError")
        || !strcmp(kind, "FileExistsError") || !strcmp(kind, "FileNotFoundError")
        || !strcmp(kind, "IsADirectoryError") || !strcmp(kind, "NotADirectoryError")
        || !strcmp(kind, "PermissionError") || !strcmp(kind, "ProcessLookupError")
        || !strcmp(kind, "TimeoutError")) return "OSError";
    if (!strcmp(kind, "BrokenPipeError") || !strcmp(kind, "ConnectionAbortedError")
        || !strcmp(kind, "ConnectionRefusedError") || !strcmp(kind, "ConnectionResetError"))
        return "ConnectionError";
    if (!strcmp(kind, "IndentationError")) return "SyntaxError";
    if (!strcmp(kind, "TabError")) return "IndentationError";
    if (!strcmp(kind, "ModuleNotFoundError")) return "ImportError";
    if (!strcmp(kind, "DeprecationWarning") || !strcmp(kind, "PendingDeprecationWarning")
        || !strcmp(kind, "RuntimeWarning") || !strcmp(kind, "SyntaxWarning")
        || !strcmp(kind, "UserWarning") || !strcmp(kind, "FutureWarning")
        || !strcmp(kind, "ImportWarning") || !strcmp(kind, "UnicodeWarning")
        || !strcmp(kind, "BytesWarning") || !strcmp(kind, "ResourceWarning")
        || !strcmp(kind, "EncodingWarning")) return "Warning";
    return "Exception"; /* unknown scripted kinds default to the Exception family */
}

static bool tmg_exc_is(const char *kind, const char *target) {
    if (!kind) return false;
    const char *k = kind;
    for (int i = 0; i < 16 && k; i++) {
        if (!strcmp(k, target)) return true;
        k = tmg_exc_parent(k);
    }
    return false;
}

static bool tmg_exc_is_exception(const char *kind) {
    if (!kind || tmg_is_base_exc(kind)) return false;
    return tmg_exc_is(kind, "Exception");
}

/* except (RuntimeError, ValueError, IndexError) -- the _gate / dispatch site. */
static bool tmg_exc_is_rvi(const char *kind) {
    if (!kind || tmg_is_base_exc(kind)) return false;
    return tmg_exc_is(kind, "RuntimeError") || tmg_exc_is(kind, "ValueError")
        || tmg_exc_is(kind, "IndexError");
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmg_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmg_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmg_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmg_type_name(const json_t *v) {
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

static char *tmg_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* json_string copies its input; this adopts+free an owned char* buffer. */
static json_t *tmg_str_own(char *s) {
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). */
static char *tmg_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmg_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tmg_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tmg_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tmg_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Python repr() for strings (ASCII corpus; quote choice minimises escapes). */
static char *tmg_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmg_strdup("None");
    if (v->type == JSON_BOOL) return tmg_strdup(v->bool_val ? "True" : "False");
    if (v->type != JSON_STRING) return tmg_pystr(v);
    const char *s = v->str_val ? v->str_val : "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *o = malloc(n * 4 + 3);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c == '\a') { o[k++] = '\\'; o[k++] = 'a'; }
        else if (c == '\b') { o[k++] = '\\'; o[k++] = 'b'; }
        else if (c == '\f') { o[k++] = '\\'; o[k++] = 'f'; }
        else if (c < 0x20 || c == 0x7f) {
            k += (size_t)snprintf(o + k, 5, "\\x%02x", c);
        } else o[k++] = (char)c;
    }
    o[k++] = q;
    o[k] = '\0';
    return o;
}

/* Python str.strip() whitespace: ASCII White_Space INCLUDING \x1c-\x1f. */
static bool tmg_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || (c >= 0x1c && c <= 0x1f);
}

/* int() strips the NARROWER set: White_Space MINUS \x1c-\x1f. */
static bool tmg_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *tmg_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmg_is_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tmg_is_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *tmg_lower(const char *s) {
    char *o = tmg_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* str.split(None, 1): skip leading ws runs; token 1 up to ws; the remainder
 * (leading ws skipped, trailing ws KEPT) is token 2. Returns the token count
 * (0..2); tokens are owned char* (NULL when absent). */
static size_t tmg_split_none1(const char *s, char **t0, char **t1) {
    *t0 = *t1 = NULL;
    if (!s) return 0;
    size_t n = strlen(s), i = 0;
    while (i < n && tmg_is_ws((unsigned char)s[i])) i++;
    if (i >= n) return 0;
    size_t start = i;
    while (i < n && !tmg_is_ws((unsigned char)s[i])) i++;
    *t0 = malloc(i - start + 1);
    if (*t0) { memcpy(*t0, s + start, i - start); (*t0)[i - start] = '\0'; }
    if (i >= n) return 1;
    while (i < n && tmg_is_ws((unsigned char)s[i])) i++;
    if (i >= n) return 1;
    *t1 = tmg_strdup(s + i);
    return 2;
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tmg_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return false;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type != b->type) return false;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tmg_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tmg_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

static bool tmg_in_set(const char *s, const char *const *set) {
    if (!s) return false;
    for (size_t i = 0; set[i]; i++) if (!strcmp(s, set[i])) return true;
    return false;
}

/* -- raise-producing access helpers -- */

/* Attribute access on a dict double: borrowed value (or borrowed default when
 * has_default and absent); missing required attribute raises the live
 * SimpleNamespace AttributeError. Non-dict targets raise the type-name
 * message (or the default when has_default -- getattr swallows). */
static const json_t *tmg_attr(const json_t *obj, const char *key,
                              bool has_default, const json_t *def, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        if (has_default) return def;
        char msg[192];
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", key);
        *raise = tmg_err_spec("AttributeError", msg);
        return NULL;
    }
    if (has_default) return def;
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tmg_type_name(obj), key);
    *raise = tmg_err_spec("AttributeError", msg);
    return NULL;
}

/* attribute WRITE on a state/gate double: dict -> set; non-dict ->
 * AttributeError naming the written field. */
static json_t *tmg_attr_set(const json_t *obj, const char *key, json_t *val) {
    if (obj && obj->type == JSON_OBJECT) { json_set((json_t *)obj, key, val); return NULL; }
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tmg_type_name(obj), key);
    json_free(val);
    return tmg_err_spec("AttributeError", msg);
}

/* Carrier check for a seamed method on a result double: the receiver must be
 * a dict double; anything else raises AttributeError naming the METHOD before
 * any seam fires (the live attribute lookup precedes the call). */
static json_t *tmg_carrier_error(const json_t *obj, const char *method) {
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tmg_type_name(obj), method);
    return tmg_err_spec("AttributeError", msg);
}

/* a, b = v -- the live unpack messages (fixture domain: 2-lists, null/int/
 * bool/dict/str). */
static json_t *tmg_unpack2(const json_t *v, json_t **a, json_t **b) {
    *a = *b = NULL;
    if (v && v->type == JSON_ARRAY) {
        if (v->c.count < 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tmg_err_spec("ValueError", msg);
        }
        if (v->c.count > 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tmg_err_spec("ValueError", msg);
        }
        *a = json_copy(v->c.items[0]);
        *b = json_copy(v->c.items[1]);
        return NULL;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", tmg_type_name(v));
    return tmg_err_spec("TypeError", msg);
}

/* a, b, c = v -- the live 3-unpack messages. */
static json_t *tmg_unpack3(const json_t *v, json_t **a, json_t **b, json_t **c) {
    *a = *b = *c = NULL;
    if (v && v->type == JSON_ARRAY) {
        if (v->c.count < 3) {
            char msg[96];
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 3, got %zu)",
                     v->c.count);
            return tmg_err_spec("ValueError", msg);
        }
        if (v->c.count > 3) {
            char msg[96];
            snprintf(msg, sizeof msg, "too many values to unpack (expected 3, got %zu)",
                     v->c.count);
            return tmg_err_spec("ValueError", msg);
        }
        *a = json_copy(v->c.items[0]);
        *b = json_copy(v->c.items[1]);
        *c = json_copy(v->c.items[2]);
        return NULL;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", tmg_type_name(v));
    return tmg_err_spec("TypeError", msg);
}

/* int(x) on the fixture scalar domain. Returns an owned number node or a
 * raise spec. str: int-ws strip + sign + underscore-separated digits; the
 * ValueError message reprs the ORIGINAL token. */
static json_t *tmg_int_coerce(const json_t *v) {
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        return json_number((double)(long long)d); /* trunc toward zero */
    }
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        const char *orig = v->str_val ? v->str_val : "";
        const char *s = orig;
        while (*s && tmg_is_int_ws((unsigned char)*s)) s++;
        const char *end = s + strlen(s);
        while (end > s && tmg_is_int_ws((unsigned char)end[-1])) end--;
        const char *p = s;
        bool neg = false;
        if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        bool any = false, bad = false, prev_us = false;
        double acc = 0.0;
        for (; p < end; p++) {
            char c = *p;
            if (c == '_') {
                if (!any || prev_us || p + 1 == end) { bad = true; break; }
                prev_us = true;
                continue;
            }
            if (c < '0' || c > '9') { bad = true; break; }
            prev_us = false;
            any = true;
            acc = acc * 10.0 + (c - '0');
        }
        if (!bad && any) return json_number(neg ? -acc : acc);
        char *r = tmg_repr(v);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        return tmg_err_spec("ValueError", msg);
    }
    char msg[192];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             tmg_type_name(v));
    return tmg_err_spec("TypeError", msg);
}

/* str.format(**values) on plain {name} templates (+ {{ / }} escapes).
 * Returns an owned char* or NULL with *raise set (KeyError reprs the field). */
static char *tmg_format(const char *fmt, const json_t *values, json_t **raise) {
    *raise = NULL;
    size_t cap = strlen(fmt) * 2 + 64, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    size_t i = 0, n = strlen(fmt);
    while (i < n) {
        char c = fmt[i];
        if (c == '{') {
            if (i + 1 < n && fmt[i + 1] == '{') { o[len++] = '{'; i += 2; continue; }
            size_t j = i + 1;
            while (j < n && fmt[j] != '}') j++;
            if (j >= n) {
                free(o);
                *raise = tmg_err_spec("ValueError", "Single '{' encountered in format string");
                return NULL;
            }
            size_t flen = j - (i + 1);
            char *field = malloc(flen + 1);
            if (!field) { free(o); return NULL; }
            memcpy(field, fmt + i + 1, flen);
            field[flen] = '\0';
            const json_t *val = (values && values->type == JSON_OBJECT)
                                ? json_obj_get(values, field) : NULL;
            if (!val) {
                char msg[256];
                snprintf(msg, sizeof msg, "'%s'", field); /* str(KeyError) == repr(key) */
                free(field);
                free(o);
                *raise = tmg_err_spec("KeyError", msg);
                return NULL;
            }
            char *vs = tmg_pystr(val);
            free(field);
            size_t vn = vs ? strlen(vs) : 0;
            if (len + vn + 1 >= cap) {
                cap = (len + vn + 1) * 2;
                char *nb = realloc(o, cap);
                if (!nb) { free(vs); free(o); return NULL; }
                o = nb;
            }
            if (vs) { memcpy(o + len, vs, vn); len += vn; }
            free(vs);
            i = j + 1;
            continue;
        }
        if (c == '}') {
            if (i + 1 < n && fmt[i + 1] == '}') { o[len++] = '}'; i += 2; continue; }
            free(o);
            *raise = tmg_err_spec("ValueError", "Single '}' encountered in format string");
            return NULL;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(o, cap);
            if (!nb) { free(o); return NULL; }
            o = nb;
        }
        o[len++] = c;
        i++;
    }
    o[len] = '\0';
    return o;
}

/* a + b string concatenation with the live TypeError messages. b is always a
 * str here; a non-str left operand gives "unsupported operand type(s)". */
static json_t *tmg_concat(const json_t *a, const char *b) {
    const char *as = tmg_str(a);
    if (!as) {
        char msg[160];
        if (a && a->type == JSON_STRING) as = "";
        else {
            snprintf(msg, sizeof msg, "unsupported operand type(s) for +: '%s' and 'str'",
                     tmg_type_name(a));
            return tmg_err_spec("TypeError", msg);
        }
    }
    size_t an = strlen(as), bn = strlen(b);
    char *o = malloc(an + bn + 1);
    if (!o) return NULL;
    memcpy(o, as, an);
    memcpy(o + an, b, bn + 1);
    return tmg_str_own(o);
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tmg_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmg_err_spec(type, msg);
}

static json_t *tmg_call(tmg_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = tmg_err_spec("RuntimeError", msg);
        ctx->last_raise = true;
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmg_err_spec("RuntimeError", msg);
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
                ret = tmg_make_exc(raise);
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
static json_t *tmg_args0(void) { return json_object(); }
static void tmg_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmg_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmg_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmg_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* -- GoalCommandResult projection (dataclass field order) -- */
/* PoP: tmg_result @ hermes_cli/goal_command.py:GoalCommandResult */
static json_t *tmg_result(json_t *output, json_t *prompt, bool kickoff,
                          json_t *clear_pending, bool error) {
    json_t *r = json_object();
    json_set(r, "output", output ? output : json_null());
    json_set(r, "prompt", prompt ? prompt : json_null());
    json_set(r, "kickoff", json_bool(kickoff));
    json_set(r, "clear_pending", clear_pending ? clear_pending : json_null());
    json_set(r, "error", json_bool(error));
    return r;
}

static json_t *tmg_result_simple(json_t *output) {
    return tmg_result(output, NULL, false, NULL, false);
}

static json_t *tmg_result_error(json_t *output) {
    return tmg_result(output, NULL, false, NULL, true);
}

/* render(key, default, **values) callback seam. values adopted. */
static json_t *tmg_render(tmg_ctx_t *ctx, const char *key, const char *def, json_t *values) {
    json_t *a = values ? values : json_object();
    /* key/default first in the live call order: rebuild with them in front */
    json_t *o = json_object();
    json_set_hdr(o, "key", key);
    json_set_hdr(o, "default", def);
    for (size_t i = 0; i < a->c.count; i++)
        json_set(o, a->c.keys[i], json_copy(a->c.items[i]));
    json_free(a);
    return tmg_call(ctx, "render", o);
}

/* mgr carrier (borrowed; NULL when absent/null). */
static json_t *tmg_mgr(tmg_ctx_t *ctx) {
    return json_obj_get(ctx->state, "mgr");
}

/* Claimed mgr-method call carrier check: a dict double carries every bound
 * seam method; a non-dict receiver raises AttributeError naming the method. */
static json_t *tmg_mgr_check(const json_t *mgr, const char *method) {
    if (mgr && mgr->type == JSON_OBJECT) return NULL;
    return tmg_carrier_error(mgr, method);
}

/* -- goal_command.py handlers -- */

/* PoP: tmg_english @ hermes_cli/goal_command.py:_english */
static json_t *tmg_english(tmg_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *def = json_obj_get(args, "default");
    const char *ds = tmg_str(def);
    if (!ds) return tmg_carrier_error(def, "format");
    const json_t *values = json_obj_get(args, "values");
    json_t *raise = NULL;
    char *out = tmg_format(ds, values, &raise);
    if (raise) return raise;
    return tmg_str_own(out);
}

/* PoP: tmg_status @ hermes_cli/goal_command.py:_status */
static json_t *tmg_status(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *ck = tmg_mgr_check(mgr, "status_line");
    if (ck) return ck;
    json_t *sl = tmg_call(ctx, "status_line", tmg_args0());
    if (tmg_is_raise(sl)) return sl;
    return tmg_result_simple(sl);
}

/* PoP: tmg_show @ hermes_cli/goal_command.py:_show */
static json_t *tmg_show(tmg_ctx_t *ctx, json_t *mgr);

/* PoP: tmg_pause @ hermes_cli/goal_command.py:_pause */
static json_t *tmg_pause(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *ck = tmg_mgr_check(mgr, "pause");
    if (ck) return ck;
    json_t *a = tmg_args0();
    tmg_asets(a, "reason", "user-paused");
    json_t *state = tmg_call(ctx, "pause", a);
    if (tmg_is_raise(state)) return state;
    json_t *out = NULL;
    json_t *cp = NULL;
    if (tmg_truthy(state)) {
        json_t *raise = NULL;
        const json_t *goal = tmg_attr(state, "goal", false, NULL, &raise);
        if (raise) { json_free(state); return raise; }
        json_t *vals = json_object();
        tmg_aset(vals, "goal", goal);
        out = tmg_render(ctx, "gateway.goal.paused", "\xe2\x8f\xb8 Goal paused: {goal}", vals);
        cp = json_string("pause");
    } else {
        out = tmg_render(ctx, "gateway.goal.no_goal_set", "No goal set.", NULL);
    }
    json_free(state);
    if (tmg_is_raise(out)) { json_free(cp); return out; }
    return tmg_result(out, NULL, false, cp, false);
}

/* PoP: tmg_resume @ hermes_cli/goal_command.py:_resume */
static json_t *tmg_resume(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *ck = tmg_mgr_check(mgr, "resume");
    if (ck) return ck;
    json_t *state = tmg_call(ctx, "resume", tmg_args0());
    if (tmg_is_raise(state)) return state;
    if (!state || state->type == JSON_NULL) {
        json_free(state);
        json_t *out = tmg_render(ctx, "gateway.goal.no_resume", "No goal to resume.", NULL);
        if (tmg_is_raise(out)) return out;
        return tmg_result_simple(out);
    }
    json_t *raise = NULL;
    const json_t *goal = tmg_attr(state, "goal", false, NULL, &raise);
    if (raise) { json_free(state); return raise; }
    json_t *vals = json_object();
    tmg_aset(vals, "goal", goal);
    json_t *out = tmg_render(ctx, "gateway.goal.resumed", "\xe2\x96\xb6 Goal resumed: {goal}", vals);
    if (tmg_is_raise(out)) { json_free(state); return out; }
    /* the prompt kwarg evaluates AFTER the positional render call */
    json_t *prompt = tmg_call(ctx, "next_continuation_prompt", tmg_args0());
    json_free(state);
    if (tmg_is_raise(prompt)) { json_free(out); return prompt; }
    return tmg_result(out, prompt, false, NULL, false);
}

/* the goals.py clear leaf (below) composes here */
static json_t *tmg_mgr_clear(tmg_ctx_t *ctx, json_t *mgr);

/* PoP: tmg_clear @ hermes_cli/goal_command.py:_clear */
static json_t *tmg_clear(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *ck = tmg_mgr_check(mgr, "has_goal");
    if (ck) return ck;
    json_t *had = tmg_call(ctx, "has_goal", tmg_args0());
    if (tmg_is_raise(had)) return had;
    json_t *cl = tmg_mgr_clear(ctx, mgr);
    if (tmg_is_raise(cl)) { json_free(had); return cl; }
    json_free(cl);
    json_t *out;
    if (tmg_truthy(had))
        out = tmg_render(ctx, "gateway.goal_cleared", "\xe2\x9c\x93 Goal cleared.", NULL);
    else
        out = tmg_render(ctx, "gateway.no_active_goal", "No active goal.", NULL);
    json_free(had);
    if (tmg_is_raise(out)) return out;
    return tmg_result(out, NULL, false, json_string("clear"), false);
}

/* PoP: tmg_unwait @ hermes_cli/goal_command.py:_unwait */
static json_t *tmg_unwait(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *ck = tmg_mgr_check(mgr, "stop_waiting");
    if (ck) return ck;
    json_t *sw = tmg_call(ctx, "stop_waiting", tmg_args0());
    if (tmg_is_raise(sw)) return sw;
    const char *msg = tmg_truthy(sw)
        ? "\xe2\x96\xb6 Wait barrier cleared \xe2\x80\x94 goal loop resumes."
        : "No wait barrier set.";
    json_free(sw);
    return tmg_result_simple(json_string(msg));
}

/* PoP: tmg_wait @ hermes_cli/goal_command.py:_wait */
static json_t *tmg_wait(tmg_ctx_t *ctx, json_t *mgr, const json_t *arg) {
    if (!tmg_truthy(arg))
        return tmg_result_error(json_string("Usage: /goal wait <pid> [reason]"));
    const char *as = tmg_str(arg);
    if (!as) return tmg_carrier_error(arg, "split");
    char *t0 = NULL, *t1 = NULL;
    size_t nt = tmg_split_none1(as, &t0, &t1);
    if (nt == 0) {
        free(t0); free(t1);
        return tmg_err_spec("IndexError", "list index out of range");
    }
    json_t *pidn = tmg_int_coerce(json_string(t0));
    free(t0);
    if (tmg_is_raise(pidn)) {
        free(t1);
        if (!strcmp(tmg_raise_kind(pidn), "ValueError")) {
            json_free(pidn);
            return tmg_result_error(json_string("/goal wait: <pid> must be an integer process id."));
        }
        return pidn;
    }
    char *reason = nt > 1 ? tmg_strip(t1) : tmg_strdup("");
    free(t1);
    ck_wait:;
    json_t *ck = tmg_mgr_check(mgr, "wait_on");
    if (ck) { json_free(pidn); free(reason); return ck; }
    json_t *a = tmg_args0();
    json_set(a, "pid", pidn); /* adopted */
    tmg_asets(a, "reason", reason);
    json_t *st = tmg_call(ctx, "wait_on", a);
    if (tmg_is_raise(st)) { free(reason); return st; }
    json_free(st);
    char *ps = tmg_pystr(pidn);
    size_t cap = strlen(ps) + strlen(reason) + 128;
    char *out = malloc(cap);
    if (out) {
        if (reason[0])
            snprintf(out, cap, "\xe2\x8f\xb3 Goal parked on pid %s (%s). Loop pauses until it exits.", ps, reason);
        else
            snprintf(out, cap, "\xe2\x8f\xb3 Goal parked on pid %s. Loop pauses until it exits.", ps);
    }
    free(ps);
    free(reason);
    return tmg_result_simple(tmg_str_own(out));
}

/* the goals.py gate leaves (below) compose here */
static json_t *tmg_mgr_add_gate(tmg_ctx_t *ctx, json_t *mgr, const json_t *command,
                                const json_t *timeout_seconds, const json_t *max_retries);
static json_t *tmg_mgr_remove_gate(tmg_ctx_t *ctx, json_t *mgr, const json_t *index);
static json_t *tmg_mgr_clear_gates(tmg_ctx_t *ctx, json_t *mgr);
static json_t *tmg_mgr_render_gates(tmg_ctx_t *ctx, json_t *mgr);

/* PoP: tmg_gate_add @ hermes_cli/goal_command.py:_gate_add */
static json_t *tmg_gate_add(tmg_ctx_t *ctx, json_t *mgr, const json_t *arg) {
    json_t *gate = tmg_mgr_add_gate(ctx, mgr, arg, NULL, NULL);
    if (tmg_is_raise(gate)) return gate;
    json_t *raise = NULL;
    const json_t *command = tmg_attr(gate, "command", false, NULL, &raise);
    if (!raise) {
        const json_t *mr = tmg_attr(gate, "max_retries", false, NULL, &raise);
        if (!raise) {
            const json_t *ts = tmg_attr(gate, "timeout_seconds", false, NULL, &raise);
            if (!raise) {
                char *cs = tmg_pystr(command);
                char *mrs = tmg_pystr(mr);
                char *tss = tmg_pystr(ts);
                size_t cap = strlen(cs) + strlen(mrs) + strlen(tss) + 160;
                char *out = malloc(cap);
                if (out)
                    snprintf(out, cap,
                             "\xe2\x9a\xbf Gate added: $ %s (%s retries, %ss timeout). "
                             "It must pass before the goal can complete.", cs, mrs, tss);
                free(cs); free(mrs); free(tss);
                json_free(gate);
                return tmg_result_simple(tmg_str_own(out));
            }
        }
    }
    json_free(gate);
    return raise;
}

/* PoP: tmg_gate_remove @ hermes_cli/goal_command.py:_gate_remove */
static json_t *tmg_gate_remove(tmg_ctx_t *ctx, json_t *mgr, const json_t *arg) {
    json_t *idx = tmg_int_coerce(arg);
    if (tmg_is_raise(idx)) return idx;
    json_t *removed = tmg_mgr_remove_gate(ctx, mgr, idx);
    json_free(idx);
    if (tmg_is_raise(removed)) return removed;
    char *rs = tmg_pystr(removed);
    size_t cap = strlen(rs) + 64;
    char *out = malloc(cap);
    if (out) snprintf(out, cap, "\xe2\x9c\x93 Gate removed: $ %s", rs);
    free(rs);
    json_free(removed);
    return tmg_result_simple(tmg_str_own(out));
}

/* PoP: tmg_gate_clear @ hermes_cli/goal_command.py:_gate_clear */
static json_t *tmg_gate_clear(tmg_ctx_t *ctx, json_t *mgr, const json_t *arg) {
    (void)arg;
    json_t *count = tmg_mgr_clear_gates(ctx, mgr);
    if (tmg_is_raise(count)) return count;
    json_t *one = json_number(1.0);
    bool plural = !tmg_json_eq(count, one);
    json_free(one);
    char *cs = tmg_pystr(count);
    size_t cap = strlen(cs) + 48;
    char *out = malloc(cap);
    if (out) snprintf(out, cap, "\xe2\x9c\x93 Cleared %s gate%s.", cs, plural ? "s" : "");
    free(cs);
    json_free(count);
    return tmg_result_simple(tmg_str_own(out));
}

/* _EXACT_HANDLERS / _GATE_HANDLERS / _BARE_CONTROL_VERBS tables */
typedef enum {
    TMG_H_NONE = 0, TMG_H_STATUS, TMG_H_SHOW, TMG_H_PAUSE, TMG_H_RESUME,
    TMG_H_CLEAR, TMG_H_UNWAIT
} tmg_handler_id;

static tmg_handler_id tmg_exact_handler(const char *key) {
    if (!key) return TMG_H_NONE;
    if (!strcmp(key, "") || !strcmp(key, "status")) return TMG_H_STATUS;
    if (!strcmp(key, "show")) return TMG_H_SHOW;
    if (!strcmp(key, "pause")) return TMG_H_PAUSE;
    if (!strcmp(key, "resume") || !strcmp(key, "continue") || !strcmp(key, "unpause"))
        return TMG_H_RESUME;
    if (!strcmp(key, "clear") || !strcmp(key, "stop") || !strcmp(key, "done"))
        return TMG_H_CLEAR;
    if (!strcmp(key, "unwait")) return TMG_H_UNWAIT;
    return TMG_H_NONE;
}

static const char *const TMG_BARE_VERBS[] = {
    "resume", "continue", "unpause", "pause", "status", "show", "unwait", NULL
};
static const char *const TMG_CONTROL_FIRST[] = {
    "resume", "continue", "unpause", "pause", "status", "show", "unwait",
    "wait", "gate", NULL
};

static json_t *tmg_run_exact(tmg_ctx_t *ctx, json_t *mgr, tmg_handler_id h) {
    switch (h) {
        case TMG_H_STATUS: return tmg_status(ctx, mgr);
        case TMG_H_SHOW: return tmg_show(ctx, mgr);
        case TMG_H_PAUSE: return tmg_pause(ctx, mgr);
        case TMG_H_RESUME: return tmg_resume(ctx, mgr);
        case TMG_H_CLEAR: return tmg_clear(ctx, mgr);
        case TMG_H_UNWAIT: return tmg_unwait(ctx, mgr);
        default: return tmg_err_spec("KeyError", "''");
    }
}

/* PoP: tmg_bare_control @ hermes_cli/goal_command.py:_bare_control */
static json_t *tmg_bare_control(tmg_ctx_t *ctx, json_t *mgr, const json_t *verb,
                                const json_t *rest) {
    const char *vs = tmg_str(verb);
    tmg_handler_id h = tmg_exact_handler(vs);
    if (h == TMG_H_NONE) {
        char *r = tmg_repr(verb);
        char msg[256];
        snprintf(msg, sizeof msg, "%s", r ? r : "''");
        free(r);
        return tmg_err_spec("KeyError", msg);
    }
    json_t *result = tmg_run_exact(ctx, mgr, h);
    if (tmg_is_raise(result)) return result;
    char *rr = tmg_repr(rest);
    size_t cap = strlen(rr ? rr : "") + 256;
    char *suffix = malloc(cap);
    if (suffix)
        snprintf(suffix, cap,
                 "\n(ignored %s: a control command never sets goal text \xe2\x80\x94 use /goal -- <text> "
                 "when the goal really starts with a control word.)", rr ? rr : "");
    free(rr);
    json_t *raise = NULL;
    const json_t *old_out = tmg_attr(result, "output", false, NULL, &raise);
    if (raise) { free(suffix); json_free(result); return raise; }
    json_t *joined = tmg_concat(old_out, suffix ? suffix : "");
    free(suffix);
    if (tmg_is_raise(joined)) { json_free(result); return joined; }
    /* dataclasses.replace: copy every field, replace output */
    json_t *nr = json_copy(result);
    json_set(nr, "output", joined);
    json_free(result);
    return nr;
}

/* PoP: tmg_gate @ hermes_cli/goal_command.py:_gate */
static json_t *tmg_gate(tmg_ctx_t *ctx, json_t *mgr, const json_t *arg) {
    bool list_path = !tmg_truthy(arg);
    if (!list_path) {
        const char *as = tmg_str(arg);
        if (!as) return tmg_carrier_error(arg, "lower");
        char *lo = tmg_lower(as);
        list_path = lo && !strcmp(lo, "list");
        free(lo);
    }
    if (list_path) {
        json_t *rg = tmg_mgr_render_gates(ctx, mgr);
        if (tmg_is_raise(rg)) return rg;
        return tmg_result_simple(rg);
    }
    const char *as = tmg_str(arg);
    char *t0 = NULL, *t1 = NULL;
    size_t nt = tmg_split_none1(as, &t0, &t1);
    if (nt == 0) {
        free(t0); free(t1);
        return tmg_err_spec("IndexError", "list index out of range");
    }
    char *verb = tmg_lower(t0);
    free(t0);
    char *rest = nt > 1 ? tmg_strip(t1) : tmg_strdup("");
    free(t1);
    /* _GATE_HANDLERS.get(verb) */
    int handler = 0; /* 1 add, 2 remove, 3 clear */
    if (!strcmp(verb, "add")) handler = 1;
    else if (!strcmp(verb, "remove") || !strcmp(verb, "rm")) handler = 2;
    else if (!strcmp(verb, "clear")) handler = 3;
    bool usage = !handler
        || (!strcmp(verb, "clear") && rest[0])
        || (strcmp(verb, "clear") && !rest[0]);
    if (usage) {
        free(verb); free(rest);
        return tmg_result_error(json_string(
            "Usage: /goal gate [list | add <command> | remove <N> | clear]"));
    }
    if (!strcmp(verb, "add")) {
        json_t *denial = tmg_call(ctx, "authorize_gate", tmg_args0());
        if (tmg_is_raise(denial)) { free(verb); free(rest); return denial; }
        if (tmg_truthy(denial)) {
            free(verb); free(rest);
            return tmg_result_error(denial);
        }
        json_free(denial);
    }
    json_t *ret;
    if (handler == 1) ret = tmg_gate_add(ctx, mgr, json_string(rest));
    else if (handler == 2) ret = tmg_gate_remove(ctx, mgr, json_string(rest));
    else ret = tmg_gate_clear(ctx, mgr, json_string(rest));
    if (tmg_is_raise(ret)) {
        const char *kind = tmg_raise_kind(ret);
        if (tmg_exc_is_rvi(kind)) {
            const char *operation = !strcmp(verb, "rm") ? "remove" : verb;
            const char *em = json_get_str(ret, "message", "");
            size_t cap = strlen(operation) + strlen(em) + 32;
            char *out = malloc(cap);
            if (out) snprintf(out, cap, "/goal gate %s: %s", operation, em);
            json_free(ret);
            free(verb); free(rest);
            return tmg_result_error(tmg_str_own(out));
        }
        free(verb); free(rest);
        return ret;
    }
    free(verb); free(rest);
    return ret;
}

/* PoP: tmg_set @ hermes_cli/goal_command.py:_set */
static json_t *tmg_set(tmg_ctx_t *ctx, json_t *mgr, const json_t *arg, bool drafting,
                       const json_t *last_user_message, bool has_progress) {
    json_t *headline = NULL;
    json_t *contract = NULL;
    if (drafting) {
        if (!tmg_truthy(arg))
            return tmg_result_error(json_string("Usage: /goal draft <objective in plain language>"));
        if (has_progress) {
            json_t *a = tmg_args0();
            tmg_asets(a, "message", "Drafting completion contract\xe2\x80\xa6");
            json_t *pr = tmg_call(ctx, "progress", a);
            if (tmg_is_raise(pr)) return pr;
            json_free(pr);
        }
        json_t *a = tmg_args0();
        tmg_aset(a, "objective", arg);
        contract = tmg_call(ctx, "draft_contract", a);
        if (tmg_is_raise(contract)) {
            if (!tmg_exc_is_exception(tmg_raise_kind(contract))) return contract;
            /* logger.debug("goal draft failed: %s", exc) */
            const char *em = json_get_str(contract, "message", "");
            size_t cap = strlen(em) + 32;
            char *msg = malloc(cap);
            if (msg) snprintf(msg, cap, "goal draft failed: %s", em);
            json_t *la = tmg_args0();
            json_set(la, "msg", tmg_str_own(msg));
            json_t *lg = tmg_call(ctx, "logger_debug", la);
            json_free(contract);
            if (tmg_is_raise(lg)) return lg;
            json_free(lg);
            contract = json_null();
        }
        headline = json_copy(arg);
    } else {
        json_t *a = tmg_args0();
        tmg_aset(a, "text", arg);
        json_t *pc = tmg_call(ctx, "parse_contract", a);
        if (tmg_is_raise(pc)) return pc;
        json_t *raise = tmg_unpack2(pc, &headline, &contract);
        json_free(pc);
        if (raise) return raise;
        /* contract = contract if not contract.is_empty() else None */
        if (!contract || contract->type != JSON_OBJECT) {
            json_free(headline);
            return tmg_carrier_error(contract, "is_empty");
        }
        json_t *ie = tmg_call(ctx, "contract_is_empty", tmg_args0());
        if (tmg_is_raise(ie)) { json_free(headline); json_free(contract); return ie; }
        if (tmg_truthy(ie)) { json_free(contract); contract = json_null(); }
        json_free(ie);
    }
    /* previous = mgr.state.goal if mgr.has_goal() else "" */
    json_t *ck = tmg_mgr_check(mgr, "has_goal");
    if (ck) { json_free(headline); json_free(contract); return ck; }
    json_t *had = tmg_call(ctx, "has_goal", tmg_args0());
    if (tmg_is_raise(had)) { json_free(headline); json_free(contract); return had; }
    json_t *previous = json_string("");
    if (tmg_truthy(had)) {
        json_t *raise = NULL;
        const json_t *st = tmg_attr(mgr, "_state", false, NULL, &raise);
        if (!raise) {
            const json_t *goal = tmg_attr(st, "goal", false, NULL, &raise);
            if (!raise) { json_free(previous); previous = json_copy(goal); }
        }
        if (raise) {
            json_free(had); json_free(headline); json_free(contract); json_free(previous);
            return raise;
        }
    }
    json_free(had);
    /* state = mgr.set(headline or arg, contract=contract) */
    ck = tmg_mgr_check(mgr, "set");
    if (ck) { json_free(headline); json_free(contract); json_free(previous); return ck; }
    json_t *sa = tmg_args0();
    tmg_aset(sa, "goal", tmg_truthy(headline) ? headline : arg);
    tmg_aset(sa, "contract", contract);
    json_t *state = tmg_call(ctx, "set", sa);
    if (tmg_is_raise(state)) {
        json_free(headline); json_free(contract); json_free(previous);
        return state;
    }
    json_t *raise = NULL;
    const json_t *max_turns = tmg_attr(state, "max_turns", false, NULL, &raise);
    const json_t *goal = NULL;
    if (!raise) goal = tmg_attr(state, "goal", false, NULL, &raise);
    if (raise) {
        json_free(headline); json_free(contract); json_free(previous); json_free(state);
        return raise;
    }
    json_t *vals = json_object();
    tmg_aset(vals, "budget", max_turns);
    tmg_aset(vals, "goal", goal);
    json_t *out = tmg_render(ctx, "gateway.goal.set",
                           "\xe2\x8a\x99 Goal set ({budget}-turn budget): {goal}", vals);
    if (tmg_is_raise(out)) {
        json_free(headline); json_free(contract); json_free(previous); json_free(state);
        return out;
    }
    /* if previous and previous != state.goal: replaced suffix */
    if (tmg_truthy(previous) && !tmg_json_eq(previous, goal)) {
        char *ps = tmg_pystr(previous);
        size_t cap = strlen(ps) + 64;
        char *suffix = malloc(cap);
        if (suffix) snprintf(suffix, cap, "\n(replaced the previous goal)\n  was: %s", ps);
        free(ps);
        json_t *joined = tmg_concat(out, suffix ? suffix : "");
        free(suffix);
        json_free(out);
        if (tmg_is_raise(joined)) {
            json_free(headline); json_free(contract); json_free(previous); json_free(state);
            return joined;
        }
        out = joined;
    }
    /* if state.has_contract(): label + contract render block */
    if (!state || state->type != JSON_OBJECT) {
        json_free(headline); json_free(contract); json_free(previous); json_free(state); json_free(out);
        return tmg_carrier_error(state, "has_contract");
    }
    json_t *hc = tmg_call(ctx, "state_has_contract", tmg_args0());
    if (tmg_is_raise(hc)) {
        json_free(headline); json_free(contract); json_free(previous); json_free(state); json_free(out);
        return hc;
    }
    bool has_contract = tmg_truthy(hc);
    json_free(hc);
    if (has_contract) {
        const char *label = drafting ? "Drafted completion contract:" : "Completion contract:";
        json_t *craise = NULL;
        const json_t *sc = tmg_attr(state, "contract", false, NULL, &craise);
        if (craise) {
            json_free(headline); json_free(contract); json_free(previous); json_free(state); json_free(out);
            return craise;
        }
        if (!sc || sc->type != JSON_OBJECT) {
            json_free(headline); json_free(contract); json_free(previous); json_free(state); json_free(out);
            return tmg_carrier_error(sc, "render_block");
        }
        json_t *rb = tmg_call(ctx, "contract_render_block", tmg_args0());
        if (tmg_is_raise(rb)) {
            json_free(headline); json_free(contract); json_free(previous); json_free(state); json_free(out);
            return rb;
        }
        char *rbs = tmg_pystr(rb);
        size_t cap = strlen(label) + strlen(rbs) + 8;
        char *suffix = malloc(cap);
        if (suffix) snprintf(suffix, cap, "\n%s\n%s", label, rbs);
        free(rbs);
        json_free(rb);
        json_t *joined = tmg_concat(out, suffix ? suffix : "");
        free(suffix);
        json_free(out);
        if (tmg_is_raise(joined)) {
            json_free(headline); json_free(contract); json_free(previous); json_free(state);
            return joined;
        }
        out = joined;
    }
    /* the drafting/non-drafting tail calls has_contract a SECOND time */
    hc = tmg_call(ctx, "state_has_contract", tmg_args0());
    if (tmg_is_raise(hc)) {
        json_free(headline); json_free(contract); json_free(previous); json_free(state); json_free(out);
        return hc;
    }
    has_contract = tmg_truthy(hc);
    json_free(hc);
    const char *tail;
    if (drafting) {
        tail = has_contract
            ? "\nTighten any field by re-setting the goal with inline lines "
              "(e.g. verify: <command>), then /goal resume. Use /goal show to review."
            : "\nCouldn't draft a contract (aux model unavailable) \xe2\x80\x94 running as a "
              "free-form goal. The per-turn judge still applies.";
    } else {
        const char *against = has_contract ? " against the contract above" : "";
        size_t cap = strlen(against) + 256;
        char *buf = malloc(cap);
        if (buf)
            snprintf(buf, cap,
                     "\nAfter each turn, a judge model checks if the goal is done%s. "
                     "Hermes keeps working until it is, you pause/clear it, or the budget is "
                     "exhausted. Use /goal status, /goal show, /goal pause, /goal resume, /goal clear.",
                     against);
        json_t *joined = tmg_concat(out, buf ? buf : "");
        free(buf);
        json_free(out);
        if (tmg_is_raise(joined)) {
            json_free(headline); json_free(contract); json_free(previous); json_free(state);
            return joined;
        }
        out = joined;
        tail = NULL;
    }
    if (tail) {
        json_t *joined = tmg_concat(out, tail);
        json_free(out);
        if (tmg_is_raise(joined)) {
            json_free(headline); json_free(contract); json_free(previous); json_free(state);
            return joined;
        }
        out = joined;
    }
    /* return GoalCommandResult(output, goals.goal_kick_prompt(state.goal, lum), kickoff=True) */
    raise = NULL;
    goal = tmg_attr(state, "goal", false, NULL, &raise);
    if (raise) {
        json_free(headline); json_free(contract); json_free(previous); json_free(state); json_free(out);
        return raise;
    }
    json_t *ka = tmg_args0();
    tmg_aset(ka, "goal", goal);
    tmg_aset(ka, "last_user_message", last_user_message);
    json_t *kick = tmg_call(ctx, "goal_kick_prompt", ka);
    json_free(headline); json_free(contract); json_free(previous); json_free(state);
    if (tmg_is_raise(kick)) { json_free(out); return kick; }
    return tmg_result(out, kick, true, NULL, false);
}

/* PoP: tmg_show @ hermes_cli/goal_command.py:_show */
static json_t *tmg_show(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *ck = tmg_mgr_check(mgr, "status_line");
    if (ck) return ck;
    json_t *sl = tmg_call(ctx, "status_line", tmg_args0());
    if (tmg_is_raise(sl)) return sl;
    json_t *rc = tmg_mgr_render_contract(ctx, mgr);
    if (tmg_is_raise(rc)) { json_free(sl); return rc; }
    char *sls = tmg_pystr(sl);
    char *rcs = tmg_pystr(rc);
    size_t cap = strlen(sls) + strlen(rcs) + 4;
    char *out = malloc(cap);
    if (out) snprintf(out, cap, "%s\n%s", sls, rcs);
    free(sls); free(rcs);
    json_free(sl); json_free(rc);
    return tmg_result_simple(tmg_str_own(out));
}

/* PoP: tmg_is_goal_control @ hermes_cli/goal_command.py:is_goal_control */
static json_t *tmg_is_goal_control(tmg_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *arg = json_obj_get(args, "arg");
    const char *as = tmg_str(arg);
    if (!as) return tmg_carrier_error(arg, "strip");
    char *st = tmg_strip(as);
    char *normalized = tmg_lower(st);
    free(st);
    bool hit = tmg_exact_handler(normalized) != TMG_H_NONE;
    if (!hit && normalized[0]) {
        char *t0 = NULL, *t1 = NULL;
        tmg_split_none1(normalized, &t0, &t1);
        hit = tmg_in_set(t0, TMG_CONTROL_FIRST);
        free(t0); free(t1);
    }
    free(normalized);
    return json_bool(hit);
}

/* PoP: tmg_dispatch @ hermes_cli/goal_command.py:dispatch_goal_command */
static json_t *tmg_dispatch(tmg_ctx_t *ctx, json_t *mgr, const json_t *arg,
                            const json_t *last_user_message, bool has_progress) {
    const char *as = tmg_str(arg);
    if (!as) return tmg_carrier_error(arg, "strip");
    char *stripped = tmg_strip(as);
    char *t0 = NULL, *t1 = NULL;
    size_t nt = tmg_split_none1(stripped, &t0, &t1);
    char *verb = nt > 0 ? tmg_lower(t0) : tmg_strdup("");
    char *rest = nt > 1 ? tmg_strip(t1) : tmg_strdup("");
    free(t0); free(t1);
    const char *prefix = "Invalid goal";
    json_t *ret = NULL;
    if (!strcmp(verb, "--")) {
        ret = tmg_set(ctx, mgr, json_string(rest), false, last_user_message, has_progress);
    } else {
        char *arglo = tmg_lower(stripped);
        tmg_handler_id h = tmg_exact_handler(arglo);
        free(arglo);
        if (h != TMG_H_NONE) {
            ret = tmg_run_exact(ctx, mgr, h);
        } else if (tmg_in_set(verb, TMG_BARE_VERBS)) {
            ret = tmg_bare_control(ctx, mgr, json_string(verb), json_string(rest));
        } else if (!strcmp(verb, "wait")) {
            prefix = "/goal wait";
            ret = tmg_wait(ctx, mgr, json_string(rest));
        } else if (!strcmp(verb, "gate")) {
            prefix = "/goal gate";
            ret = tmg_gate(ctx, mgr, json_string(rest));
        } else {
            bool drafting = !strcmp(verb, "draft");
            ret = tmg_set(ctx, mgr, json_string(drafting ? rest : stripped), drafting,
                          last_user_message, has_progress);
        }
    }
    free(stripped); free(verb); free(rest);
    if (tmg_is_raise(ret)) {
        const char *kind = tmg_raise_kind(ret);
        if (!tmg_exc_is_rvi(kind)) return ret;
        const char *em = json_get_str(ret, "message", "");
        json_t *out;
        if (!strcmp(prefix, "Invalid goal")) {
            json_t *vals = json_object();
            tmg_asets(vals, "error", em);
            out = tmg_render(ctx, "gateway.goal.invalid", "Invalid goal: {error}", vals);
            if (tmg_is_raise(out)) { json_free(ret); return out; }
        } else {
            size_t cap = strlen(prefix) + strlen(em) + 8;
            char *buf = malloc(cap);
            if (buf) snprintf(buf, cap, "%s: %s", prefix, em);
            out = tmg_str_own(buf);
        }
        json_free(ret);
        return tmg_result_error(out);
    }
    return ret;
}

/* -- hermes_cli/goals.py UNCLAIMED manager leaves -- */

/* PoP: tmg_mgr_clear @ hermes_cli/goals.py:clear */
static json_t *tmg_mgr_clear(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *raise = NULL;
    const json_t *state = tmg_attr(mgr, "_state", false, NULL, &raise);
    if (raise) return raise;
    if (!state || state->type == JSON_NULL) return json_null();
    json_t *wr = tmg_attr_set(state, "status", json_string("cleared"));
    if (wr) return wr;
    json_t *ck = tmg_mgr_check(mgr, "_save");
    if (ck) return ck;
    json_t *sv = tmg_call(ctx, "save", tmg_args0());
    if (tmg_is_raise(sv)) return sv;
    json_free(sv);
    json_set(mgr, "_state", json_null());
    return json_null();
}

/* PoP: tmg_mgr_add_gate @ hermes_cli/goals.py:add_gate */
static json_t *tmg_mgr_add_gate(tmg_ctx_t *ctx, json_t *mgr, const json_t *command,
                                const json_t *timeout_seconds, const json_t *max_retries) {
    json_t *ck = tmg_mgr_check(mgr, "_require_goal");
    if (ck) return ck;
    json_t *state = tmg_call(ctx, "require_goal", tmg_args0());
    if (tmg_is_raise(state)) return state;
    char *cs = tmg_truthy(command) ? tmg_pystr(command) : tmg_strdup("");
    char *cmd = tmg_strip(cs);
    free(cs);
    if (!cmd[0]) {
        free(cmd);
        json_free(state);
        return tmg_err_spec("ValueError", "gate command is empty");
    }
    json_t *tsn;
    if (tmg_truthy(timeout_seconds)) {
        tsn = tmg_int_coerce(timeout_seconds);
        if (tmg_is_raise(tsn)) { free(cmd); json_free(state); return tsn; }
    } else {
        tsn = json_number(300.0); /* DEFAULT_GATE_TIMEOUT_SECONDS */
    }
    json_t *mrn;
    if (tmg_truthy(max_retries)) {
        mrn = tmg_int_coerce(max_retries);
        if (tmg_is_raise(mrn)) { free(cmd); json_free(tsn); json_free(state); return mrn; }
    } else {
        mrn = json_number(3.0); /* DEFAULT_GATE_MAX_RETRIES */
    }
    /* GoalGate(command=..., timeout_seconds=..., max_retries=...) + defaults */
    json_t *gate = json_object();
    json_set(gate, "command", tmg_str_own(cmd));
    json_set(gate, "timeout_seconds", tsn);
    json_set(gate, "max_retries", mrn);
    json_set(gate, "attempts", json_number(0.0));
    json_set(gate, "last_exit_code", json_null());
    json_set(gate, "last_output_tail", json_string(""));
    json_t *raise = NULL;
    const json_t *gates = tmg_attr(state, "gates", false, NULL, &raise);
    if (raise) { json_free(gate); json_free(state); return raise; }
    if (!gates || gates->type != JSON_ARRAY) {
        json_free(gate); json_free(state);
        return tmg_carrier_error(gates, "append");
    }
    json_append((json_t *)gates, json_copy(gate));
    ck = tmg_mgr_check(mgr, "_save");
    if (ck) { json_free(gate); json_free(state); return ck; }
    json_t *sv = tmg_call(ctx, "save", tmg_args0());
    json_free(state);
    if (tmg_is_raise(sv)) { json_free(gate); return sv; }
    json_free(sv);
    return gate;
}

/* PoP: tmg_mgr_remove_gate @ hermes_cli/goals.py:remove_gate */
static json_t *tmg_mgr_remove_gate(tmg_ctx_t *ctx, json_t *mgr, const json_t *index) {
    json_t *ck = tmg_mgr_check(mgr, "_pop_item");
    if (ck) return ck;
    json_t *a = tmg_args0();
    tmg_asets(a, "attr", "gates");
    tmg_aset(a, "index_1based", index);
    json_t *item = tmg_call(ctx, "pop_item", a);
    if (tmg_is_raise(item)) return item;
    json_t *raise = NULL;
    const json_t *command = tmg_attr(item, "command", false, NULL, &raise);
    if (raise) { json_free(item); return raise; }
    json_t *out = json_copy(command);
    json_free(item);
    return out;
}

/* PoP: tmg_mgr_clear_gates @ hermes_cli/goals.py:clear_gates */
static json_t *tmg_mgr_clear_gates(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *ck = tmg_mgr_check(mgr, "_clear_items");
    if (ck) return ck;
    json_t *a = tmg_args0();
    tmg_asets(a, "attr", "gates");
    return tmg_call(ctx, "clear_items", a);
}

/* PoP: tmg_mgr_render_gates @ hermes_cli/goals.py:render_gates */
static json_t *tmg_mgr_render_gates(tmg_ctx_t *ctx, json_t *mgr) {
    (void)ctx;
    json_t *raise = NULL;
    const json_t *state = tmg_attr(mgr, "_state", false, NULL, &raise);
    if (raise) return raise;
    if (!state || state->type == JSON_NULL) return json_string("(no active goal)");
    const json_t *gates = tmg_attr(state, "gates", false, NULL, &raise);
    if (raise) return raise;
    if (!tmg_truthy(gates))
        return json_string("(no quality gates \xe2\x80\x94 use /goal gate add <command> to require one)");
    if (gates->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmg_type_name(gates));
        return tmg_err_spec("TypeError", msg);
    }
    size_t cap = 64, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    o[0] = '\0';
    for (size_t i = 0; i < gates->c.count; i++) {
        const json_t *g = gates->c.items[i];
        const json_t *lec = tmg_attr(g, "last_exit_code", false, NULL, &raise);
        if (raise) { free(o); return raise; }
        char *status = tmg_strdup("");
        json_t *zero = json_number(0.0);
        bool passing = tmg_json_eq(lec, zero);
        json_free(zero);
        if (passing) {
            free(status);
            status = tmg_strdup(" \xe2\x9c\x93 passing");
        } else if (lec && lec->type != JSON_NULL) {
            const json_t *attempts = tmg_attr(g, "attempts", false, NULL, &raise);
            if (!raise) {
                const json_t *mr = tmg_attr(g, "max_retries", false, NULL, &raise);
                if (!raise) {
                    char *ls = tmg_pystr(lec);
                    char *as = tmg_pystr(attempts);
                    char *ms = tmg_pystr(mr);
                    size_t scap = strlen(ls) + strlen(as) + strlen(ms) + 64;
                    status = realloc(status, scap);
                    if (status)
                        snprintf(status, scap, " \xe2\x9c\x97 failing (exit %s, attempt %s/%s)", ls, as, ms);
                    free(ls); free(as); free(ms);
                }
            }
            if (raise) { free(status); free(o); return raise; }
        }
        const json_t *command = tmg_attr(g, "command", false, NULL, &raise);
        if (raise) { free(status); free(o); return raise; }
        char *cs = tmg_pystr(command);
        char idx[32];
        snprintf(idx, sizeof idx, "%zu", i + 1);
        size_t need = len + strlen(idx) + strlen(cs) + strlen(status ? status : "") + 16;
        if (need >= cap) {
            cap = need * 2;
            char *nb = realloc(o, cap);
            if (!nb) { free(cs); free(status); free(o); return NULL; }
            o = nb;
        }
        len += (size_t)snprintf(o + len, cap - len, "%s- %s. $ %s%s",
                                i > 0 ? "\n" : "", idx, cs, status ? status : "");
        free(cs);
        free(status);
    }
    return tmg_str_own(o);
}

/* PoP: tmg_mgr_render_contract @ hermes_cli/goals.py:render_contract */
static json_t *tmg_mgr_render_contract(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *raise = NULL;
    const json_t *state = tmg_attr(mgr, "_state", false, NULL, &raise);
    if (raise) return raise;
    if (!state || state->type == JSON_NULL) return json_string("(no active goal)");
    if (state->type != JSON_OBJECT) return tmg_carrier_error(state, "has_contract");
    json_t *hc = tmg_call(ctx, "state_has_contract", tmg_args0());
    if (tmg_is_raise(hc)) return hc;
    bool has = tmg_truthy(hc);
    json_free(hc);
    if (!has)
        return json_string("(no completion contract \xe2\x80\x94 set one with /goal draft <objective> "
                           "or inline field: value lines)");
    const json_t *contract = tmg_attr(state, "contract", false, NULL, &raise);
    if (raise) return raise;
    if (!contract || contract->type != JSON_OBJECT)
        return tmg_carrier_error(contract, "render_block");
    return tmg_call(ctx, "contract_render_block", tmg_args0());
}

/* PoP: tmg_mgr_wait_on_session @ hermes_cli/goals.py:wait_on_session */
static json_t *tmg_mgr_wait_on_session(tmg_ctx_t *ctx, json_t *mgr,
                                       const json_t *session_id, const json_t *reason) {
    json_t *ck = tmg_mgr_check(mgr, "_require_active");
    if (ck) return ck;
    json_t *ra = tmg_call(ctx, "require_active", tmg_args0());
    if (tmg_is_raise(ra)) return ra;
    json_free(ra);
    char *ss = tmg_truthy(session_id) ? tmg_pystr(session_id) : tmg_strdup("");
    char *sid = tmg_strip(ss);
    free(ss);
    if (!sid[0]) {
        free(sid);
        return tmg_err_spec("ValueError", "session_id must be a non-empty string");
    }
    ck = tmg_mgr_check(mgr, "_park");
    if (ck) { free(sid); return ck; }
    json_t *a = tmg_args0();
    tmg_aset(a, "reason", reason);
    tmg_asets(a, "waiting_on_session", sid);
    free(sid);
    return tmg_call(ctx, "park", a);
}

/* CONTINUATION_PROMPT_GATE_FAILED_TEMPLATE (goals.py, verbatim) */
static const char TMG_GATE_FAILED_TEMPLATE[] =
    "[Continuing toward your standing goal \xe2\x80\x94 a quality gate failed]\n"
    "Goal: {goal}\n\n"
    "The quality gate command below must pass before this goal can be "
    "declared done, and it just failed (attempt {attempt}/{max_retries}):\n"
    "  $ {command}\n"
    "Exit code: {exit_code}\n"
    "Output (tail):\n"
    "```\n"
    "{output}\n"
    "```\n\n"
    "Fix the underlying problem so this gate passes, then re-run it to "
    "confirm. Do not declare the goal complete while any gate fails. If the "
    "gate itself is wrong or cannot pass, say so clearly and stop.";

/* PoP: tmg_mgr_check_gates @ hermes_cli/goals.py:_check_gates */
static json_t *tmg_mgr_check_gates(tmg_ctx_t *ctx, json_t *mgr) {
    json_t *raise = NULL;
    const json_t *state = tmg_attr(mgr, "_state", false, NULL, &raise);
    if (raise) return raise;
    if (!state || state->type == JSON_NULL) return json_null();
    const json_t *gates = tmg_attr(state, "gates", false, NULL, &raise);
    if (raise) return raise;
    if (!tmg_truthy(gates)) return json_null();
    if (gates->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmg_type_name(gates));
        return tmg_err_spec("TypeError", msg);
    }
    json_t *gw = tmg_call(ctx, "gate_workspace", tmg_args0());
    if (tmg_is_raise(gw)) return gw;
    json_t *cwd = NULL, *refusal = NULL;
    raise = tmg_unpack2(gw, &cwd, &refusal);
    json_free(gw);
    if (raise) return raise;
    if (tmg_truthy(refusal)) {
        char *rs = tmg_pystr(refusal);
        size_t cap = strlen(rs) * 3 + 256;
        char *pr = malloc(cap), *re = malloc(cap), *me = malloc(cap);
        if (pr) snprintf(pr, cap, "quality gates not run: %s", rs);
        if (re) snprintf(re, cap, "gates not run: %s", rs);
        if (me) snprintf(me, cap,
                         "\xe2\x8f\xb8 Goal paused \xe2\x80\x94 quality gates not run: %s. Fix the workspace or "
                         "/goal gate remove the gates, then /goal resume.", rs);
        free(rs);
        json_t *a = tmg_args0();
        json_set(a, "paused_reason", tmg_str_own(pr));
        tmg_asets(a, "verdict", "gate_failed");
        json_set(a, "reason", tmg_str_own(re));
        json_set(a, "message", tmg_str_own(me));
        json_free(cwd); json_free(refusal);
        return tmg_call(ctx, "pause_decision", a);
    }
    for (size_t i = 0; i < gates->c.count; i++) {
        json_t *g = gates->c.items[i];
        json_t *ga = tmg_args0();
        tmg_aset(ga, "gate", g);
        tmg_aset(ga, "cwd", cwd);
        json_t *rg = tmg_call(ctx, "run_gate", ga);
        if (tmg_is_raise(rg)) { json_free(cwd); json_free(refusal); return rg; }
        json_t *passed = NULL, *exit_code = NULL, *tail = NULL;
        raise = tmg_unpack3(rg, &passed, &exit_code, &tail);
        json_free(rg);
        if (raise) { json_free(cwd); json_free(refusal); return raise; }
        raise = tmg_attr_set(g, "last_exit_code", json_copy(exit_code));
        if (!raise) raise = tmg_attr_set(g, "last_output_tail", json_copy(tail));
        if (raise) {
            json_free(passed); json_free(exit_code); json_free(tail);
            json_free(cwd); json_free(refusal);
            return raise;
        }
        if (tmg_truthy(passed)) {
            raise = tmg_attr_set(g, "attempts", json_number(0.0));
            json_free(passed); json_free(exit_code); json_free(tail);
            if (raise) { json_free(cwd); json_free(refusal); return raise; }
            continue;
        }
        json_free(passed);
        const json_t *attempts = tmg_attr(g, "attempts", false, NULL, &raise);
        if (raise) { json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal); return raise; }
        double an;
        if (attempts && attempts->type == JSON_NUMBER) an = attempts->num_val + 1.0;
        else if (attempts && attempts->type == JSON_BOOL) an = (attempts->bool_val ? 1.0 : 0.0) + 1.0;
        else {
            char msg[160];
            snprintf(msg, sizeof msg, "unsupported operand type(s) for +: '%s' and 'int'",
                     tmg_type_name(attempts));
            json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal);
            return tmg_err_spec("TypeError", msg);
        }
        raise = tmg_attr_set(g, "attempts", json_number(an));
        if (raise) { json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal); return raise; }
        const json_t *mr = tmg_attr(g, "max_retries", false, NULL, &raise);
        if (raise) { json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal); return raise; }
        if (mr && mr->type == JSON_NUMBER && an > mr->num_val) {
            const json_t *command = tmg_attr(g, "command", false, NULL, &raise);
            if (raise) { json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal); return raise; }
            char *cs = tmg_pystr(command);
            char *es = tmg_pystr(exit_code);
            char *ms = tmg_pystr(mr);
            char retries[64];
            snprintf(retries, sizeof retries, "%.17g", an - 1.0);
            if (an - 1.0 == (double)(long long)(an - 1.0))
                snprintf(retries, sizeof retries, "%lld", (long long)(an - 1.0));
            size_t cap = strlen(cs) * 3 + strlen(es) + strlen(ms) + 256;
            char *pr = malloc(cap), *re = malloc(cap), *me = malloc(cap);
            if (pr) snprintf(pr, cap, "quality gate exhausted %s retries: $ %s", retries, cs);
            if (re) snprintf(re, cap, "gate exhausted retries: $ %s", cs);
            if (me) snprintf(me, cap,
                             "\xe2\x8f\xb8 Goal paused \xe2\x80\x94 quality gate still failing after "
                             "%s retries: $ %s (exit %s). Fix it manually or /goal gate remove it, "
                             "then /goal resume.", ms, cs, es);
            free(cs); free(es); free(ms);
            json_t *a = tmg_args0();
            json_set(a, "paused_reason", tmg_str_own(pr));
            tmg_asets(a, "verdict", "gate_failed");
            json_set(a, "reason", tmg_str_own(re));
            json_set(a, "message", tmg_str_own(me));
            json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal);
            return tmg_call(ctx, "pause_decision", a);
        }
        if (!mr || mr->type != JSON_NUMBER) {
            char msg[160];
            snprintf(msg, sizeof msg, "'>' not supported between instances of 'float' and '%s'",
                     tmg_type_name(mr));
            json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal);
            return tmg_err_spec("TypeError", msg);
        }
        json_t *sv = tmg_call(ctx, "save", tmg_args0());
        if (tmg_is_raise(sv)) { json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal); return sv; }
        json_free(sv);
        /* template kwargs evaluate in order: goal, command, exit_code,
         * attempt, max_retries, output */
        const json_t *goal = tmg_attr(state, "goal", false, NULL, &raise);
        const json_t *command = NULL;
        if (!raise) command = tmg_attr(g, "command", false, NULL, &raise);
        if (raise) { json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal); return raise; }
        json_t *fv = json_object();
        tmg_aset(fv, "goal", goal);
        tmg_aset(fv, "command", command);
        tmg_aset(fv, "exit_code", exit_code);
        tmg_asetn(fv, "attempt", an);
        tmg_aset(fv, "max_retries", mr);
        tmg_aset(fv, "output", tmg_truthy(tail) ? tail : json_string("(no output)"));
        json_t *fraise = NULL;
        char *prompt = tmg_format(TMG_GATE_FAILED_TEMPLATE, fv, &fraise);
        json_free(fv);
        if (fraise) {
            json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal);
            return fraise;
        }
        const json_t *turns = tmg_attr(state, "turns_used", false, NULL, &raise);
        const json_t *maxt = NULL;
        if (!raise) maxt = tmg_attr(state, "max_turns", false, NULL, &raise);
        if (raise) {
            free(prompt);
            json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal);
            return raise;
        }
        char *es = tmg_pystr(exit_code);
        char *cs = tmg_pystr(command);
        char *ts = tmg_pystr(turns);
        char *mts = tmg_pystr(maxt);
        char *ms = tmg_pystr(mr);
        char att[64];
        snprintf(att, sizeof att, "%lld", (long long)an);
        size_t cap = strlen(es) + strlen(cs) * 2 + strlen(ts) + strlen(mts) + strlen(ms) + 160;
        char *re = malloc(cap), *me = malloc(cap);
        if (re) snprintf(re, cap, "gate failed (exit %s): $ %s", es, cs);
        if (me) snprintf(me, cap,
                         "\xe2\x9c\x97 Quality gate failed (%s/%s turns, attempt %s/%s): $ %s",
                         ts, mts, att, ms, cs);
        free(es); free(cs); free(ts); free(mts); free(ms);
        json_t *a = tmg_args0();
        tmg_asets(a, "status", "active");
        tmg_asetb(a, "should_continue", true);
        json_set(a, "prompt", tmg_str_own(prompt));
        tmg_asets(a, "verdict", "gate_failed");
        json_set(a, "reason", tmg_str_own(re));
        json_set(a, "message", tmg_str_own(me));
        json_free(exit_code); json_free(tail); json_free(cwd); json_free(refusal);
        return tmg_call(ctx, "decision", a);
    }
    json_free(cwd); json_free(refusal);
    json_t *sv = tmg_call(ctx, "save", tmg_args0());
    if (tmg_is_raise(sv)) return sv;
    json_free(sv);
    return json_null();
}

/* -- case wrappers (uniform harness signature) -- */

static json_t *tmg_status_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_status(ctx, tmg_mgr(ctx));
}
static json_t *tmg_show_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_show(ctx, tmg_mgr(ctx));
}
static json_t *tmg_pause_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_pause(ctx, tmg_mgr(ctx));
}
static json_t *tmg_resume_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_resume(ctx, tmg_mgr(ctx));
}
static json_t *tmg_clear_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_clear(ctx, tmg_mgr(ctx));
}
static json_t *tmg_unwait_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_unwait(ctx, tmg_mgr(ctx));
}
static json_t *tmg_wait_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_wait(ctx, tmg_mgr(ctx), json_obj_get(args, "arg"));
}
static json_t *tmg_gate_add_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_gate_add(ctx, tmg_mgr(ctx), json_obj_get(args, "arg"));
}
static json_t *tmg_gate_remove_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_gate_remove(ctx, tmg_mgr(ctx), json_obj_get(args, "arg"));
}
static json_t *tmg_gate_clear_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_gate_clear(ctx, tmg_mgr(ctx), json_obj_get(args, "arg"));
}
static json_t *tmg_bare_control_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_bare_control(ctx, tmg_mgr(ctx), json_obj_get(args, "verb"),
                            json_obj_get(args, "rest"));
}
static json_t *tmg_gate_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_gate(ctx, tmg_mgr(ctx), json_obj_get(args, "arg"));
}
static json_t *tmg_set_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_set(ctx, tmg_mgr(ctx), json_obj_get(args, "arg"),
                   json_get_bool(args, "drafting", false),
                   json_obj_get(args, "last_user_message"),
                   json_get_bool(args, "progress", false));
}
static json_t *tmg_dispatch_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_dispatch(ctx, tmg_mgr(ctx), json_obj_get(args, "arg"),
                        json_obj_get(args, "last_user_message"),
                        json_get_bool(args, "progress", false));
}
static json_t *tmg_mgr_clear_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_mgr_clear(ctx, tmg_mgr(ctx));
}
static json_t *tmg_mgr_add_gate_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_mgr_add_gate(ctx, tmg_mgr(ctx), json_obj_get(args, "command"),
                            json_obj_get(args, "timeout_seconds"),
                            json_obj_get(args, "max_retries"));
}
static json_t *tmg_mgr_remove_gate_case(tmg_ctx_t *ctx, const json_t *args) {
    return tmg_mgr_remove_gate(ctx, tmg_mgr(ctx), json_obj_get(args, "index_1based"));
}
static json_t *tmg_mgr_clear_gates_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_mgr_clear_gates(ctx, tmg_mgr(ctx));
}
static json_t *tmg_mgr_render_gates_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_mgr_render_gates(ctx, tmg_mgr(ctx));
}
static json_t *tmg_mgr_render_contract_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_mgr_render_contract(ctx, tmg_mgr(ctx));
}
static json_t *tmg_mgr_wait_on_session_case(tmg_ctx_t *ctx, const json_t *args) {
    const json_t *reason = json_obj_get(args, "reason");
    return tmg_mgr_wait_on_session(ctx, tmg_mgr(ctx), json_obj_get(args, "session_id"),
                                   reason ? reason : json_string(""));
}
static json_t *tmg_mgr_check_gates_case(tmg_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmg_mgr_check_gates(ctx, tmg_mgr(ctx));
}