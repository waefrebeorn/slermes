/*
 * gateway_inbound_flow2.c — UNCLAIMED command-dispatch / admission FLOW
 * remainder of gateway/run_inbound.py (hermes-agent), second wave after
 * gateway_inbound_flow.c / run_inbound_flow.c claimed the ingress, busy-path
 * and authority cores. 21 methods of GatewayInboundMixin:
 *
 *   _hm_resolve_command, _hm_confirm_destructive, _hm_cmd_new, _hm_cmd_start,
 *   _hm_cmd_egress, _hm_rewrite_turn_to_prompt, _hm_cmd_learn, _hm_cmd_plan,
 *   _hm_cmd_init, _hm_cmd_blueprint, _hm_cmd_undo, _hm_cmd_queue,
 *   _hm_cmd_steer, _hm_dispatch_canonical_command, _hm_skill_slash_rewrite,
 *   _hm_pending_reply_intercepts, _hm_dispatch_idle_commands,
 *   _hm_rescue_orphaned_fifo, _hm_authority_admission, _hm_admit_to_fifo,
 *   _handle_message.
 *
 * NOTE: the task brief named hermes_cli/gateway_inbound.py; that module does
 * not exist in the quarry — the defining module is gateway/run_inbound.py
 * (PoP annotations credit the real definer).
 *
 * Decision sequences only: every live I/O (command resolution, quick/plugin
 * handlers, skill scans, prompt builders, authority admission, busy metrics,
 * adapter lookups, i18n, clocks) enters through ONE universal seam:
 * gi2_call(ctx, name, args). The harness feeds it the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts. Claimed siblings
 * (flow1 ports) and unported collaborators are SEAMED as "self.<name>";
 * ported siblings compose directly (C calls C, live class methods compose).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and gi2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_gateway_inbound_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc), ...};
 *   AuthorityRefusal adds reason/field/detail; TurnLeaseTimeoutError is
 *   built from {session_id, owner_key, generation, wait_seconds}.
 * - Platform / MessageType members travel as their .value strings; sources
 *   and events travel as plain attribute dicts (the oracle materializes
 *   SimpleNamespace / EventDouble and projects back identically).
 * - Authorities / adapters travel as id strings (or null).
 * - logger calls are silent on both sides (not traced).
 * - executing_admission.get() reads state.executing_admission silently.
 * - asyncio.to_thread and _run_in_executor_with_context run their callable
 *   synchronously (no trace of their own); a `build` callable travels as
 *   {"fn": <seam name>, "args": {...}} and fires that one seam.
 * - self._session_state is a get-or-create STATE accessor: records the event
 *   (ret = node copy at access time) and returns a borrowed pointer so the
 *   claim writes (turn.lease/agent/event/started_ts) persist into out.state.
 * - _AGENT_PENDING_SENTINEL travels as the token string
 *   "__agent_pending_sentinel__" (identity is marker equality).
 * - The `execute` closure of _hm_confirm_destructive projects to
 *   {"__callable__": "execute"}; scripts never invoke it (documented domain).
 * - out.event echoes the (possibly text-mutated) INPUT event; a flow that
 *   rebinds its local event (admission/rescue) leaves the input untouched.
 *
 * Fixture domain (documented exclusions):
 * - ASCII text inputs; the U+2026 ellipsis appears only as the live plan-ack
 *   suffix literal. Integral numbers only (no integral-float str() sites).
 * - resolve_command scripts: null or {"name": str}; cmd_def.name is read
 *   unguarded. get_command_args scripts: str. build_init_prompt_for_cwd /
 *   prompt-builder scripts: str. skill_cmds: object of {name: str} entries.
 * - _fifo_only events carry _stop_epoch; allow_gateway_control is present.
 * - int() tokens in /undo: optional sign + digits (no underscores),
 *   magnitude <= 2^31. Handler values in the plain/idle handler maps are
 *   non-empty seam-name strings. The execute closure is never invoked.
 * - session_states[key].turn exists when _handle_message reaches the claim.
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

/* ── module constants ── */
/* gateway/run.py:_AGENT_PENDING_SENTINEL marker (identity == token equality) */
#define GI2_SENTINEL "__agent_pending_sentinel__"
/* run_inbound._HM_CANONICAL_COMMANDS (frozenset; "moa" lives in the MoA
 * mixin, which is a stub base here -> AttributeError, transcribed below). */
static const char *GI2_CANONICAL[] = {
    "new", "start", "egress", "learn", "plan", "init", "blueprint",
    "undo", "queue", "steer", "moa",
};
#define GI2_N_CANONICAL (sizeof(GI2_CANONICAL) / sizeof(GI2_CANONICAL[0]))

/* ── universal seam context ── */
typedef struct gi2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} gi2_ctx_t;

/* ── basic helpers ── */
static bool gi2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool gi2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool gi2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *gi2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool gi2_str_eq(const json_t *v, const char *s) {
    const char *t = gi2_str(v);
    return t && s && strcmp(t, s) == 0;
}

static char *gi2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* str.strip(): ASCII whitespace both ends (fixtures are ASCII). */
static char *gi2_strip(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* UTF-8 codepoint count (Python len counts characters). */
static size_t gi2_utf8_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}

/* byte offset of the first n codepoints (Python s[:n]). */
static size_t gi2_utf8_prefix(const char *s, size_t n) {
    size_t count = 0, i = 0;
    while (s[i] && count < n) {
        if (((unsigned char)s[i] & 0xc0) != 0x80) count++;
        i++;
    }
    return i;
}

/* ── exception model ── */
static json_t *gi2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/*
 * FULL builtin exception hierarchy (py3.14). gi2_exc_parent walks to the
 * root; `except Exception` catches exactly the kinds whose chain reaches
 * Exception. SystemExit / KeyboardInterrupt / GeneratorExit parent to
 * BaseException ONLY, so they pass through every `except Exception` here.
 * Unknown custom kinds parent to Exception (the oracle's make_exc builds
 * type(kind, (Exception,), {}) for them).
 */
static const char *gi2_exc_parent(const char *t) {
    static const struct { const char *child, *parent; } TAB[] = {
        /* BaseException direct children */
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"},
        {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        /* Exception direct children */
        {"ArithmeticError", "Exception"},
        {"AssertionError", "Exception"},
        {"AttributeError", "Exception"},
        {"BufferError", "Exception"},
        {"LookupError", "Exception"},
        {"MemoryError", "Exception"},
        {"NameError", "Exception"},
        {"OSError", "Exception"},
        {"ReferenceError", "Exception"},
        {"RuntimeError", "Exception"},
        {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"},
        {"SyntaxError", "Exception"},
        {"SystemError", "Exception"},
        {"TypeError", "Exception"},
        {"ValueError", "Exception"},
        {"Warning", "Exception"},
        {"ImportError", "Exception"},
        {"ExceptionGroup", "Exception"},
        /* ArithmeticError */
        {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        /* LookupError */
        {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        /* NameError */
        {"UnboundLocalError", "NameError"},
        /* OSError family (incl. the old EnvironmentError/IOError aliases) */
        {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"},
        {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"},
        /* RuntimeError */
        {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        /* SyntaxError */
        {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        /* ValueError */
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        /* ImportError */
        {"ModuleNotFoundError", "ImportError"},
        /* Warning family */
        {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"},
        {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"},
        {"UserWarning", "Warning"},
        {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        {"ResourceWarning", "Warning"},
        /* quarry custom families */
        {"PersistenceError", "Exception"},
        {"TransientStoreError", "PersistenceError"},
        {"PermanentStoreError", "PersistenceError"},
        {"StoreUnavailable", "TransientStoreError"},
        {"StoreReplaced", "TransientStoreError"},
        {"CorruptStoreError", "PermanentStoreError"},
        {"AuthorityRefusal", "PersistenceError"},
        {"TurnLeaseTimeoutError", "TimeoutError"},
    };
    for (size_t i = 0; i < sizeof(TAB) / sizeof(TAB[0]); i++)
        if (strcmp(t, TAB[i].child) == 0) return TAB[i].parent;
    return NULL;
}

/* except Exception: the kind's chain reaches Exception (not the trio). */
static bool gi2_is_exception(const json_t *r) {
    if (!gi2_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    for (int depth = 0; depth < 16; depth++) {
        if (strcmp(t, "Exception") == 0) return true;
        if (strcmp(t, "BaseException") == 0) return false;
        const char *p = gi2_exc_parent(t);
        if (!p) return true;  /* unknown custom kind: make_exc bases Exception */
        t = p;
    }
    return true;
}

static bool gi2_is_kind(const json_t *r, const char *kind) {
    return gi2_is_raise(r) && gi2_str_eq(json_obj_get(r, "__raise__"), kind);
}

/* str(KeyError(k)) == repr(k) for the string keys the fixtures use. */
static json_t *gi2_keyerror(const char *key) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s'", key ? key : "");
    return gi2_err("KeyError", buf);
}

/* Python type name of a JSON value (for AttributeError/TypeError messages). */
static const char *gi2_py_type(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* obj.method(...) on a non-carrier: AttributeError naming the method. */
static json_t *gi2_attr_err(const json_t *v, const char *method) {
    char buf[256];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'",
             gi2_py_type(v), method);
    return gi2_err("AttributeError", buf);
}

/*
 * persistence.core.errors._one_line: whitespace collapses to single spaces,
 * other non-printables drop, bounded; empty -> None (ASCII fixture corpus).
 */
static char *gi2_one_line(const char *value, size_t limit) {
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
static json_t *gi2_refusal(const char *reason, const char *field, const char *detail) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "AuthorityRefusal");
    json_set_hdr(e, "message", reason ? reason : "");
    json_set_hdr(e, "reason", reason ? reason : "");
    char *f = gi2_one_line(field, 80);
    json_set(e, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = gi2_one_line(detail, 160);
    json_set(e, "detail", d ? json_string(d) : json_null());
    free(d);
    return e;
}

static bool gi2_is_oserror_type(const char *t) {
    /* walk the parent chain: anything under OSError */
    for (int depth = 0; depth < 16; depth++) {
        if (strcmp(t, "OSError") == 0) return true;
        const char *p = gi2_exc_parent(t);
        if (!p) return false;
        t = p;
    }
    return false;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *gi2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    if (strcmp(type, "AuthorityRefusal") == 0) {
        return gi2_refusal(json_get_str(spec, "reason", ""),
                           gi2_str(json_obj_get(spec, "field")),
                           gi2_str(json_obj_get(spec, "detail")));
    }
    if (strcmp(type, "TurnLeaseTimeoutError") == 0) {
        const char *sid = json_get_str(spec, "session_id", "");
        const char *okey = json_get_str(spec, "owner_key", "");
        double gen = json_get_num(spec, "generation", 0);
        double wait = json_get_num(spec, "wait_seconds", 0);
        char buf[512];
        snprintf(buf, sizeof buf,
                 "turn lease wait timed out after %.0fs on session %s "
                 "for routing key %s (gen %.0f)", wait, sid, okey, gen);
        return gi2_err(type, buf);
    }
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && gi2_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = gi2_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return gi2_err(type, msg);
}

/* ── seam dispatch ── */
/*
 * One scripted entry: a plain value (reused), or {"__script__": true,
 * "ret": v, "raise": spec}. A missing/exhausted script is a fail-closed
 * RuntimeError on both sides. A bare JSON array is a SEQUENCE: one result
 * per call, indexed by a per-name cursor.
 */
static json_t *gi2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return gi2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *gi2_call(gi2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = gi2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = gi2_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = gi2_entry_result(json_get(entry, idx));
        }
    } else {
        res = gi2_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* Propagate a raise result unless it is NULL (success). */
#define GI2_CHK(v) do { if (gi2_is_raise(v)) return (v); } while (0)

/* ── small arg builders ── */
static json_t *gi2_arg1(const char *k1, const json_t *v1) {
    json_t *a = json_object();
    json_set(a, k1, v1 ? json_copy(v1) : json_null());
    return a;
}
static json_t *gi2_arg2(const char *k1, const json_t *v1,
                        const char *k2, const json_t *v2) {
    json_t *a = gi2_arg1(k1, v1);
    json_set(a, k2, v2 ? json_copy(v2) : json_null());
    return a;
}
static json_t *gi2_arg3(const char *k1, const json_t *v1,
                        const char *k2, const json_t *v2,
                        const char *k3, const json_t *v3) {
    json_t *a = gi2_arg2(k1, v1, k2, v2);
    json_set(a, k3, v3 ? json_copy(v3) : json_null());
    return a;
}
static json_t *gi2_args_str(const char *k, const char *v) {
    json_t *a = json_object();
    json_set_hdr(a, k, v ? v : "");
    return a;
}

/* tuple/list builders for the (handled, result[, ...]) returns */
static json_t *gi2_pair(const json_t *a, const json_t *b) {
    json_t *t = json_array();
    json_append(t, a ? json_copy(a) : json_null());
    json_append(t, b ? json_copy(b) : json_null());
    return t;
}
static json_t *gi2_triple(const json_t *a, const json_t *b, const json_t *d) {
    json_t *t = gi2_pair(a, b);
    json_append(t, d ? json_copy(d) : json_null());
    return t;
}
static json_t *gi2_quad(const json_t *a, const json_t *b,
                        const json_t *d, const json_t *e) {
    json_t *t = gi2_triple(a, b, d);
    json_append(t, e ? json_copy(e) : json_null());
    return t;
}

/* a, b = result  /  a, b, c = result — ValueError messages per py3.14. */
static bool gi2_unpack(const json_t *r, size_t n, json_t **out) {
    if (r && r->type == JSON_ARRAY && json_len(r) == n) {
        for (size_t i = 0; i < n; i++) out[i] = json_get(r, i);
        return true;
    }
    size_t got = (r && r->type == JSON_ARRAY) ? json_len(r) : 0;
    char buf[128];
    if (!r || r->type != JSON_ARRAY) {
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object",
                 gi2_py_type(r));
    } else if (got < n) {
        snprintf(buf, sizeof buf,
                 "not enough values to unpack (expected %zu, got %zu)", n, got);
    } else {
        snprintf(buf, sizeof buf, "too many values to unpack (expected %zu)", n);
    }
    out[0] = gi2_err("ValueError", buf);
    return false;
}

/* ── state helpers ── */
static json_t *gi2_sub(gi2_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

/* self._session_state: get-or-create STATE accessor. Records the event
 * (ret = the node copy at access time) and returns a BORROWED pointer so
 * the claim writes persist into out.state. */
static json_t *gi2_session_state(gi2_ctx_t *c, const char *key) {
    json_t *states = gi2_sub(c, "session_states");
    if (!states) {
        states = json_object();
        json_set(c->state, "session_states", states);
    }
    json_t *node = key ? json_obj_get(states, key) : NULL;
    if (!node || node->type != JSON_OBJECT) {
        node = json_object();
        json_set(states, key ? key : "", node);
    }
    json_t *a = gi2_args_str("key", key);
    json_t *ev = json_object();
    json_set_hdr(ev, "name", "self._session_state");
    json_set(ev, "args", a);
    json_set(ev, "ret", json_copy(node));
    json_append(c->trace, ev);
    return node;
}

/* event.get_command() / event.get_command_args(): EventDouble seams. */
static json_t *gi2_ev_command(gi2_ctx_t *c) {
    return gi2_call(c, "event.get_command", json_object());
}
static json_t *gi2_ev_command_args(gi2_ctx_t *c) {
    return gi2_call(c, "event.get_command_args", json_object());
}

/* str member read with a default (dict.get semantics on object nodes). */
static const char *gi2_field_str(const json_t *obj, const char *key, const char *dflt) {
    if (!obj || obj->type != JSON_OBJECT) return dflt;
    const char *s = gi2_str(json_obj_get(obj, key));
    return s ? s : dflt;
}

static bool gi2_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (gi2_str_eq(json_get(arr, i), s)) return true;
    return false;
}

/* JSON value equality for the _stop_epoch != route_stop_epoch comparison
 * (numeric epochs in the fixture domain; type-aware for robustness). */
static bool gi2_val_eq(const json_t *a, const json_t *b) {
    bool an = gi2_is_null(a), bn = gi2_is_null(b);
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        default: return false;   /* containers: outside the epoch domain */
    }
}

/* int(token) for the /undo corpus: optional sign + digits, |v| <= 2^31.
 * Returns false on any ValueError-shaped token (underscore/overflow excluded
 * from the fixture domain). */
static bool gi2_parse_int(const char *tok, long long *out) {
    if (!tok || !*tok) return false;
    const char *p = tok;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return false;
    long long v = 0;
    for (; *p; p++) {
        if (!isdigit((unsigned char)*p)) return false;
        v = v * 10 + (*p - '0');
        if (v > 2147483647LL) return false;
    }
    *out = neg ? -v : v;
    return true;
}

/* first whitespace-split token (str.split()[0]); NULL when no token. */
static char *gi2_first_token(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return NULL;
    const char *e = s;
    while (*e && !isspace((unsigned char)*e)) e++;
    size_t n = (size_t)(e - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* t(key, **kw) seam with no kwargs. */
static json_t *gi2_t(gi2_ctx_t *c, const char *key) {
    return gi2_call(c, "t", gi2_args_str("key", key));
}

/* ══════════════ ported methods ══════════════ */

/* forward decls for the composed siblings */
static json_t *gi2_hm_confirm_destructive(gi2_ctx_t *c, json_t *args);
static json_t *gi2_hm_rewrite_turn_to_prompt(gi2_ctx_t *c, json_t *args);
static json_t *gi2_hm_resolve_command(gi2_ctx_t *c, json_t *args);
static json_t *gi2_hm_dispatch_canonical_command(gi2_ctx_t *c, json_t *args);
static json_t *gi2_hm_skill_slash_rewrite(gi2_ctx_t *c, json_t *args);
static json_t *gi2_hm_pending_reply_intercepts(gi2_ctx_t *c, json_t *args);
static json_t *gi2_hm_rescue_orphaned_fifo(gi2_ctx_t *c, json_t *args);
static json_t *gi2_hm_admit_to_fifo(gi2_ctx_t *c, json_t *args);

/* _canon(cmd): (_resolve_cmd(cmd) if cmd else None, def.name if def else cmd).
 * Returns the canonical NODE (owned copy of cmd or def.name); *def_out is the
 * resolve result (caller frees). resolve_command scripts: null or {"name": str}. */
static json_t *gi2_canon(gi2_ctx_t *c, const json_t *cmd, json_t **def_out,
                         json_t **err_out) {
    *def_out = NULL;
    *err_out = NULL;
    if (!gi2_truthy(cmd)) return cmd ? json_copy(cmd) : json_null();
    json_t *r = gi2_call(c, "resolve_command", gi2_arg1("cmd", cmd));
    if (gi2_is_raise(r)) { *err_out = r; return NULL; }
    *def_out = r;
    if (gi2_is_null(r)) return cmd ? json_copy(cmd) : json_null();
    if (r->type != JSON_OBJECT) {
        *err_out = gi2_attr_err(r, "name");
        return NULL;
    }
    json_t *name = json_obj_get(r, "name");
    if (!name) {
        *err_out = gi2_err("AttributeError",
                           "'types.SimpleNamespace' object has no attribute 'name'");
        return NULL;
    }
    return json_copy(name);
}

/* PoP: gi2_hm_resolve_command @ gateway/run_inbound.py:_hm_resolve_command */
static json_t *gi2_hm_resolve_command(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");

    json_t *command = gi2_ev_command(c);
    GI2_CHK(command);
    json_t *cmd_def = NULL, *cerr = NULL;
    json_t *canonical = gi2_canon(c, command, &cmd_def, &cerr);
    if (cerr) { json_free(command); json_free(cmd_def); return cerr; }

    /* alias quick-command expansion (built-ins keep precedence) */
    if (gi2_truthy(command) && gi2_is_null(cmd_def)) {
        json_t *q = gi2_call(c, "self._hm_quick_commands", json_object());
        if (gi2_is_raise(q)) { json_free(command); json_free(cmd_def); json_free(canonical); return q; }
        json_t *qcmd = NULL;
        if (q && q->type == JSON_OBJECT)
            qcmd = json_obj_get(q, gi2_str(command) ? gi2_str(command) : "");
        else if (!gi2_is_null(q)) {
            json_t *e = gi2_attr_err(q, "get");
            json_free(q); json_free(command); json_free(cmd_def); json_free(canonical);
            return e;
        }
        if (qcmd && gi2_str_eq(json_obj_get(qcmd, "type"), "alias")) {
            json_t *nc = gi2_call(c, "self._hm_expand_alias_quick_command",
                                  gi2_arg2("event", ev, "qcmd", qcmd));
            if (gi2_is_raise(nc)) {
                json_free(q); json_free(command); json_free(cmd_def); json_free(canonical);
                return nc;
            }
            if (!gi2_is_null(nc)) {
                json_free(command);
                command = nc;
                json_free(cmd_def);
                json_free(canonical);
                canonical = gi2_canon(c, command, &cmd_def, &cerr);
                if (cerr) { json_free(q); json_free(command); json_free(cmd_def); return cerr; }
            } else {
                json_free(nc);
            }
        }
        json_free(q);
    }

    /* known-command gate: is_gateway_known_command only when both truthy */
    bool known = false;
    if (gi2_truthy(command) && gi2_truthy(canonical)) {
        json_t *k = gi2_call(c, "is_gateway_known_command", gi2_arg1("canonical", canonical));
        if (gi2_is_raise(k)) { json_free(command); json_free(cmd_def); json_free(canonical); return k; }
        known = gi2_truthy(k);
        json_free(k);
    }
    if (!(gi2_truthy(command) && gi2_truthy(canonical) && known)) {
        json_t *ret = gi2_quad(json_bool(false), NULL, command, canonical);
        json_free(command); json_free(cmd_def); json_free(canonical);
        return ret;
    }

    json_t *denied = gi2_call(c, "self._check_slash_access",
                              gi2_arg2("source", src, "command", canonical));
    if (gi2_is_raise(denied)) { json_free(command); json_free(cmd_def); json_free(canonical); return denied; }
    if (!gi2_is_null(denied)) {
        json_t *ret = gi2_quad(json_bool(true), denied, command, canonical);
        json_free(command); json_free(cmd_def); json_free(canonical);
        return ret;
    }
    json_free(denied);

    json_t *ha = json_object();
    json_set(ha, "event", ev ? json_copy(ev) : json_null());
    json_set(ha, "source", src ? json_copy(src) : json_null());
    json_set_hdr(ha, "key", key);
    json_set(ha, "command", command ? json_copy(command) : json_null());
    json_set(ha, "canonical", canonical ? json_copy(canonical) : json_null());
    json_t *hooks = gi2_call(c, "self._hm_command_hooks", ha);
    if (gi2_is_raise(hooks)) { json_free(command); json_free(cmd_def); json_free(canonical); return hooks; }
    json_t *parts[3];
    if (!gi2_unpack(hooks, 3, parts)) {
        json_t *e = parts[0];
        json_free(hooks); json_free(command); json_free(cmd_def); json_free(canonical);
        return e;
    }
    json_t *h = parts[0], *result = parts[1], *new_command = parts[2];
    if (gi2_truthy(h)) {
        json_t *ret = gi2_quad(json_bool(true), result, command, canonical);
        json_free(hooks); json_free(command); json_free(cmd_def); json_free(canonical);
        return ret;
    }
    if (!gi2_is_null(new_command)) {
        json_free(command);
        command = json_copy(new_command);
        json_free(cmd_def);
        json_free(canonical);
        canonical = gi2_canon(c, command, &cmd_def, &cerr);
        if (cerr) { json_free(hooks); json_free(command); json_free(cmd_def); return cerr; }
    }
    json_t *itd = gi2_call(c, "is_interrupt_then_dispatch", gi2_arg1("canonical", canonical));
    if (gi2_is_raise(itd)) { json_free(hooks); json_free(command); json_free(cmd_def); json_free(canonical); return itd; }
    if (gi2_truthy(itd)) {
        json_t *dr = gi2_call(c, "self._hm_drop_held_input", gi2_arg1("event", ev));
        if (gi2_is_raise(dr)) {
            json_free(itd); json_free(hooks); json_free(command); json_free(cmd_def); json_free(canonical);
            return dr;
        }
        json_free(dr);
    }
    json_free(itd);
    json_t *ret = gi2_quad(json_bool(false), NULL, command, canonical);
    json_free(hooks); json_free(command); json_free(cmd_def); json_free(canonical);
    return ret;
}

/* PoP: gi2_hm_confirm_destructive @ gateway/run_inbound.py:_hm_confirm_destructive */
static json_t *gi2_hm_confirm_destructive(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    const char *command = json_get_str(args, "command", "");
    const char *detail = json_get_str(args, "detail", "");
    char title[256];
    snprintf(title, sizeof title, "/%s", command);
    json_t *a = json_object();
    json_set(a, "event", ev ? json_copy(ev) : json_null());
    json_set_hdr(a, "command", command);
    json_set_hdr(a, "title", title);
    json_set_hdr(a, "detail", detail);
    json_t *exe = json_object();
    json_set_hdr(exe, "__callable__", "execute");
    json_set(a, "execute", exe);
    json_t *r = gi2_call(c, "self._maybe_confirm_destructive_slash", a);
    GI2_CHK(r);
    json_t *ret = gi2_pair(json_bool(true), r);
    json_free(r);
    return ret;
}

/* PoP: gi2_hm_cmd_new @ gateway/run_inbound.py:_hm_cmd_new */
static json_t *gi2_hm_cmd_new(gi2_ctx_t *c, json_t *args) {
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    json_t *lobby = gi2_call(c, "self._is_telegram_topic_root_lobby",
                             gi2_arg1("source", src));
    GI2_CHK(lobby);
    if (gi2_truthy(lobby)) {
        json_free(lobby);
        json_t *m = gi2_call(c, "self._telegram_topic_root_new_message", json_object());
        GI2_CHK(m);
        json_t *ret = gi2_pair(json_bool(true), m);
        json_free(m);
        return ret;
    }
    json_free(lobby);
    json_t *pj = gi2_call(c, "person_input_joined_turn", gi2_args_str("key", key));
    GI2_CHK(pj);
    json_free(pj);
    json_t *detail = gi2_t(c, "gateway.confirm.detail_new");
    GI2_CHK(detail);
    json_t *ca = json_object();
    json_set(ca, "event", json_copy(json_obj_get(args, "event")));
    json_set_hdr(ca, "command", "new");
    json_set(ca, "detail", detail);
    json_set_hdr(ca, "handler", "self._handle_reset_command");
    json_t *ret = gi2_hm_confirm_destructive(c, ca);
    json_free(ca);
    return ret;
}

/* PoP: gi2_hm_cmd_start @ gateway/run_inbound.py:_hm_cmd_start */
static json_t *gi2_hm_cmd_start(gi2_ctx_t *c, json_t *args) {
    (void)c; (void)args;
    /* logger.info("Ignoring /start platform ping ...") is silent */
    return gi2_pair(json_bool(true), json_string(""));
}

/* PoP: gi2_hm_cmd_egress @ gateway/run_inbound.py:_hm_cmd_egress */
static json_t *gi2_hm_cmd_egress(gi2_ctx_t *c, json_t *args) {
    (void)args;
    json_t *r = gi2_call(c, "format_status_text", json_object());
    GI2_CHK(r);
    json_t *ret = gi2_pair(json_bool(true), r);
    json_free(r);
    return ret;
}

/* PoP: gi2_hm_rewrite_turn_to_prompt @ gateway/run_inbound.py:_hm_rewrite_turn_to_prompt */
static json_t *gi2_hm_rewrite_turn_to_prompt(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *name = json_get_str(args, "name", "");
    json_t *ack = gi2_call(c, "self._send_command_ack",
                           gi2_arg3("source", src, "ack", json_obj_get(args, "ack"),
                                    "name", json_obj_get(args, "name")));
    GI2_CHK(ack);
    json_free(ack);
    /* event.text = await asyncio.to_thread(build): build is a
     * {"fn": seam, "args": {...}} descriptor firing exactly one seam. */
    json_t *build = json_obj_get(args, "build");
    const char *fn = gi2_field_str(build, "fn", "");
    json_t *ba = json_obj_get(build, "args");
    json_t *r = gi2_call(c, fn, ba ? json_copy(ba) : json_object());
    if (gi2_is_raise(r)) {
        if (gi2_is_exception(r)) {
            json_free(r);
            json_t *ta = gi2_args_str("key", "gateway.prompt_command.start_failed");
            json_set(ta, "command", json_string(name));
            json_t *tr = gi2_call(c, "t", ta);
            GI2_CHK(tr);
            json_t *ret = gi2_pair(json_bool(true), tr);
            json_free(tr);
            return ret;
        }
        return r;  /* BaseException passthrough */
    }
    if (ev && ev->type == JSON_OBJECT) json_set(ev, "text", r);
    else json_free(r);
    return gi2_pair(json_bool(false), NULL);
}

/* Live _hm_rewrite_turn_to_prompt mutates the caller's event IN PLACE
 * (event.text = build()). ca carries an OWNED copy of the event (libjson has
 * no detach: json_set/json_obj_del both free the displaced value), so after
 * the rewrite sync the — possibly rewritten — text back to the original
 * event. text is the only field the rewrite touches; on the raise paths it
 * returns before setting text, so the copy still holds the original text and
 * the sync is a no-op there. */
static void gi2_sync_text(json_t *orig_ev, json_t *new_ev) {
    if (!orig_ev || orig_ev->type != JSON_OBJECT || !new_ev || new_ev->type != JSON_OBJECT) return;
    json_t *tx = json_obj_get(new_ev, "text");
    if (tx) json_set(orig_ev, "text", json_copy(tx));
}

/* PoP: gi2_hm_cmd_learn @ gateway/run_inbound.py:_hm_cmd_learn */
static json_t *gi2_hm_cmd_learn(gi2_ctx_t *c, json_t *args) {
    json_t *r = gi2_ev_command_args(c);
    GI2_CHK(r);
    if (!gi2_str(r)) {
        json_t *e = gi2_attr_err(r, "strip");
        json_free(r);
        return e;
    }
    char *req = gi2_strip(r->str_val);
    json_free(r);
    json_t *ack = gi2_t(c, req[0] ? "gateway.learn.ack_described"
                                  : "gateway.learn.ack_conversation");
    if (gi2_is_raise(ack)) { free(req); return ack; }
    json_t *build = json_object();
    json_set_hdr(build, "fn", "build_learn_prompt");
    json_set(build, "args", gi2_args_str("req", req));
    free(req);
    json_t *ca = json_object();
    json_set(ca, "event", json_copy(json_obj_get(args, "event")));
    json_set(ca, "source", json_copy(json_obj_get(args, "source")));
    json_set_hdr(ca, "name", "learn");
    json_set(ca, "ack", ack);
    json_set(ca, "build", build);
    json_t *ret = gi2_hm_rewrite_turn_to_prompt(c, ca);
    gi2_sync_text(json_obj_get(args, "event"), json_obj_get(ca, "event"));
    json_free(ca);
    return ret;
}

/* PoP: gi2_hm_cmd_plan @ gateway/run_inbound.py:_hm_cmd_plan */
static json_t *gi2_hm_cmd_plan(gi2_ctx_t *c, json_t *args) {
    json_t *r = gi2_ev_command_args(c);
    GI2_CHK(r);
    if (!gi2_str(r)) {
        json_t *e = gi2_attr_err(r, "strip");
        json_free(r);
        return e;
    }
    char *task = gi2_strip(r->str_val);
    json_free(r);
    json_t *ack;
    if (task[0]) {
        size_t cut = gi2_utf8_prefix(task, 80);
        bool ell = gi2_utf8_len(task) > 80;
        char *label = malloc(cut + 4);
        if (!label) { free(task); return gi2_err("MemoryError", ""); }
        memcpy(label, task, cut);
        if (ell) memcpy(label + cut, "\xe2\x80\xa6", 3);
        label[cut + (ell ? 3 : 0)] = '\0';
        json_t *ta = gi2_args_str("key", "gateway.plan.ack_task");
        json_set(ta, "task", json_string(label));
        free(label);
        ack = gi2_call(c, "t", ta);
    } else {
        ack = gi2_t(c, "gateway.plan.ack_conversation");
    }
    if (gi2_is_raise(ack)) { free(task); return ack; }
    json_t *build = json_object();
    json_set_hdr(build, "fn", "build_plan_prompt");
    json_set(build, "args", gi2_args_str("task", task));
    free(task);
    json_t *ca = json_object();
    json_set(ca, "event", json_copy(json_obj_get(args, "event")));
    json_set(ca, "source", json_copy(json_obj_get(args, "source")));
    json_set_hdr(ca, "name", "plan");
    json_set(ca, "ack", ack);
    json_set(ca, "build", build);
    json_t *ret = gi2_hm_rewrite_turn_to_prompt(c, ca);
    gi2_sync_text(json_obj_get(args, "event"), json_obj_get(ca, "event"));
    json_free(ca);
    return ret;
}

/* PoP: gi2_hm_cmd_init @ gateway/run_inbound.py:_hm_cmd_init */
static json_t *gi2_hm_cmd_init(gi2_ctx_t *c, json_t *args) {
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    /* try: extra / session_key / build all inside the except Exception */
    json_t *prompt = NULL;
    json_t *caught = NULL;
    json_t *r = gi2_ev_command_args(c);
    if (gi2_is_raise(r)) {
        if (gi2_is_exception(r)) caught = r; else return r;
    } else if (!gi2_str(r)) {
        caught = gi2_attr_err(r, "strip");
        json_free(r);
    } else {
        char *extra = gi2_strip(r->str_val);
        json_free(r);
        json_t *sk_node = NULL;
        if (key && key[0]) {
            sk_node = json_string(key);
        } else {
            json_t *kr = gi2_call(c, "self._session_key_for_source",
                                  gi2_arg1("source", src));
            if (gi2_is_raise(kr)) {
                free(extra);
                if (gi2_is_exception(kr)) caught = kr; else return kr;
            } else {
                sk_node = kr;   /* moved; may be null (session_key=None) */
            }
        }
        if (!caught) {
            json_t *ba = gi2_args_str("extra", extra);
            json_set(ba, "session_key", sk_node ? sk_node : json_null());
            sk_node = NULL;
            prompt = gi2_call(c, "build_init_prompt_for_cwd", ba);
            if (gi2_is_raise(prompt)) {
                if (gi2_is_exception(prompt)) { caught = prompt; prompt = NULL; }
                else { free(extra); json_free(sk_node); return prompt; }
            }
        }
        free(extra);
        json_free(sk_node);
    }
    if (caught) {
        json_free(caught);
        json_t *tr = gi2_t(c, "gateway.init.start_failed");
        GI2_CHK(tr);
        json_t *ret = gi2_pair(json_bool(true), tr);
        json_free(tr);
        return ret;
    }
    /* "UPDATE the existing AGENTS.md" in _init_prompt (str domain) */
    if (!gi2_str(prompt)) {
        char buf[160];
        snprintf(buf, sizeof buf,
                 "argument of type '%s' is not a container or iterable",
                 gi2_py_type(prompt));
        json_free(prompt);
        return gi2_err("TypeError", buf);
    }
    json_t *ack = gi2_t(c, strstr(prompt->str_val, "UPDATE the existing AGENTS.md")
                             ? "gateway.init.ack_update" : "gateway.init.ack_generate");
    if (gi2_is_raise(ack)) { json_free(prompt); return ack; }
    json_t *sa = gi2_arg3("source", src, "ack", ack, "name", json_string("init"));
    json_t *sr = gi2_call(c, "self._send_command_ack", sa);
    if (gi2_is_raise(sr)) { json_free(ack); json_free(prompt); return sr; }
    json_free(sr);
    json_free(ack);
    json_t *ev = json_obj_get(args, "event");
    if (ev && ev->type == JSON_OBJECT) json_set(ev, "text", prompt);
    else json_free(prompt);
    return gi2_pair(json_bool(false), NULL);
}

/* PoP: gi2_hm_cmd_blueprint @ gateway/run_inbound.py:_hm_cmd_blueprint */
static json_t *gi2_hm_cmd_blueprint(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    json_t *br = gi2_call(c, "self._handle_blueprint_command", gi2_arg1("event", ev));
    GI2_CHK(br);
    /* getattr(result, "text", "") or "" / getattr(result, "agent_seed", None):
     * defaults on non-carriers, field reads on object carriers. */
    const char *text = "";
    json_t *seed = NULL;
    if (br && br->type == JSON_OBJECT) {
        const char *t = gi2_str(json_obj_get(br, "text"));
        text = (t && t[0]) ? t : "";
        seed = json_obj_get(br, "agent_seed");
    }
    if (!gi2_truthy(seed)) {
        json_t *ret = gi2_pair(json_bool(true), text[0] ? json_string(text) : NULL);
        json_free(br);
        return ret;
    }
    if (text[0]) {
        json_t *sa = gi2_arg3("source", src, "ack", json_string(text),
                              "name", json_string("blueprint"));
        json_t *sr = gi2_call(c, "self._send_command_ack", sa);
        if (gi2_is_raise(sr)) { json_free(br); return sr; }
        json_free(sr);
    }
    /* event.text = seed (the live try/except Exception around a dataclass
     * attribute write cannot raise; transcribed as the plain write). */
    if (ev && ev->type == JSON_OBJECT) json_set(ev, "text", json_copy(seed));
    json_free(br);
    return gi2_pair(json_bool(false), NULL);
}

/* PoP: gi2_hm_cmd_undo @ gateway/run_inbound.py:_hm_cmd_undo */
static json_t *gi2_hm_cmd_undo(gi2_ctx_t *c, json_t *args) {
    json_t *r = gi2_ev_command_args(c);
    GI2_CHK(r);
    if (!gi2_str(r)) {
        json_t *e = gi2_attr_err(r, "strip");
        json_free(r);
        return e;
    }
    char *raw = gi2_strip(r->str_val);
    json_free(r);
    long long n = 1;
    if (raw[0]) {
        char *tok = gi2_first_token(raw);
        long long v;
        if (tok && gi2_parse_int(tok, &v)) {
            n = v > 1 ? v : 1;   /* max(1, int(...)) */
        }
        /* ValueError / IndexError suppressed */
        free(tok);
    }
    free(raw);
    json_t *detail;
    if (n == 1) {
        detail = gi2_t(c, "gateway.confirm.detail_undo_one");
    } else {
        json_t *ta = gi2_args_str("key", "gateway.confirm.detail_undo_many");
        json_set(ta, "count", json_number((double)n));
        detail = gi2_call(c, "t", ta);
    }
    GI2_CHK(detail);
    json_t *ca = json_object();
    json_set(ca, "event", json_copy(json_obj_get(args, "event")));
    json_set_hdr(ca, "command", "undo");
    json_set(ca, "detail", detail);
    json_set_hdr(ca, "handler", "self._handle_undo_command");
    json_t *ret = gi2_hm_confirm_destructive(c, ca);
    json_free(ca);
    return ret;
}

/* PoP: gi2_hm_cmd_queue @ gateway/run_inbound.py:_hm_cmd_queue */
static json_t *gi2_hm_cmd_queue(gi2_ctx_t *c, json_t *args) {
    json_t *usage = gi2_t(c, "gateway.queue.usage");
    GI2_CHK(usage);
    json_t *r = gi2_call(c, "self._hm_send_payload_as_turn",
                         gi2_arg2("event", json_obj_get(args, "event"), "usage", usage));
    json_free(usage);
    GI2_CHK(r);
    return r;   /* the claimed sibling's (handled, result) tuple verbatim */
}

/* PoP: gi2_hm_cmd_steer @ gateway/run_inbound.py:_hm_cmd_steer */
static json_t *gi2_hm_cmd_steer(gi2_ctx_t *c, json_t *args) {
    json_t *usage = gi2_t(c, "gateway.steer.usage_idle");
    GI2_CHK(usage);
    json_t *r = gi2_call(c, "self._hm_send_payload_as_turn",
                         gi2_arg2("event", json_obj_get(args, "event"), "usage", usage));
    json_free(usage);
    GI2_CHK(r);
    return r;
}

/* PoP: gi2_hm_dispatch_canonical_command @ gateway/run_inbound.py:_hm_dispatch_canonical_command */
static json_t *gi2_hm_dispatch_canonical_command(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *canonical = gi2_str(json_obj_get(args, "canonical"));
    /* plain_handler = plain.get(canonical) or idle.get(canonical) */
    json_t *ph = gi2_call(c, "self._gateway_plain_command_handlers", json_object());
    GI2_CHK(ph);
    json_t *ih = NULL;
    json_t *h = NULL;
    if (ph && ph->type == JSON_OBJECT) {
        if (canonical) h = json_obj_get(ph, canonical);
    } else if (!gi2_is_null(ph)) {
        json_t *e = gi2_attr_err(ph, "get");
        json_free(ph);
        return e;
    }
    if (!gi2_truthy(h)) {
        ih = gi2_call(c, "self._gateway_idle_command_handlers", json_object());
        if (gi2_is_raise(ih)) { json_free(ph); return ih; }
        if (ih && ih->type == JSON_OBJECT) {
            if (canonical) h = json_obj_get(ih, canonical);
        } else if (!gi2_is_null(ih)) {
            json_t *e = gi2_attr_err(ih, "get");
            json_free(ph); json_free(ih);
            return e;
        }
    }
    if (!gi2_is_null(h)) {   /* `is not None` — present-but-falsy still calls */
        const char *hname = gi2_str(h);
        if (!hname || !hname[0]) {
            char buf[128];
            snprintf(buf, sizeof buf, "'%s' object is not callable", gi2_py_type(h));
            json_free(ph); json_free(ih);
            return gi2_err("TypeError", buf);
        }
        /* async with self._async_profile_scope_for_source(source): the
         * factory call is the seam; enter/exit are silent no-ops. */
        json_t *scope = gi2_call(c, "self._async_profile_scope_for_source",
                                 gi2_arg1("source", src));
        if (gi2_is_raise(scope)) { json_free(ph); json_free(ih); return scope; }
        json_free(scope);
        json_t *r = gi2_call(c, hname, gi2_arg1("event", ev));
        if (gi2_is_raise(r)) { json_free(ph); json_free(ih); return r; }
        json_t *ret = gi2_pair(json_bool(true), r);
        json_free(r);
        json_free(ph);
        json_free(ih);
        return ret;
    }
    json_free(ph);
    json_free(ih);
    if (canonical) {
        for (size_t i = 0; i < GI2_N_CANONICAL; i++) {
            if (strcmp(canonical, GI2_CANONICAL[i]) != 0) continue;
            if (strcmp(canonical, "moa") == 0) {
                /* _hm_cmd_moa lives in the MoA mixin (stub base here) */
                return gi2_err("AttributeError",
                               "'GatewayInboundMixin' object has no attribute '_hm_cmd_moa'");
            }
            if (strcmp(canonical, "new") == 0) return gi2_hm_cmd_new(c, args);
            if (strcmp(canonical, "start") == 0) return gi2_hm_cmd_start(c, args);
            if (strcmp(canonical, "egress") == 0) return gi2_hm_cmd_egress(c, args);
            if (strcmp(canonical, "learn") == 0) return gi2_hm_cmd_learn(c, args);
            if (strcmp(canonical, "plan") == 0) return gi2_hm_cmd_plan(c, args);
            if (strcmp(canonical, "init") == 0) return gi2_hm_cmd_init(c, args);
            if (strcmp(canonical, "blueprint") == 0) return gi2_hm_cmd_blueprint(c, args);
            if (strcmp(canonical, "undo") == 0) return gi2_hm_cmd_undo(c, args);
            if (strcmp(canonical, "queue") == 0) return gi2_hm_cmd_queue(c, args);
            if (strcmp(canonical, "steer") == 0) return gi2_hm_cmd_steer(c, args);
        }
    }
    return gi2_pair(json_bool(false), NULL);
}

/* PoP: gi2_hm_skill_slash_rewrite @ gateway/run_inbound.py:_hm_skill_slash_rewrite */
static json_t *gi2_hm_skill_slash_rewrite(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    json_t *command = json_obj_get(args, "command");
    if (!gi2_truthy(command)) return NULL;
    json_t *ba4 = json_object();
    json_set(ba4, "event", ev ? json_copy(ev) : json_null());
    json_set(ba4, "source", src ? json_copy(src) : json_null());
    json_set_hdr(ba4, "key", key);
    json_set(ba4, "command", command ? json_copy(command) : json_null());
    json_t *b = gi2_call(c, "self._hm_bundle_slash_rewrite", ba4);
    GI2_CHK(b);
    if (gi2_truthy(b)) { json_free(b); return NULL; }
    json_free(b);

    /* Everything below sits inside `try: ... except Exception: logger.debug`
     * (silent) -> return None. BaseException propagates. GI2_TC calls a seam
     * and jumps to the catch label on an Exception-family raise. */
    json_t *skill_cmds = NULL, *ck = NULL, *ui_r = NULL, *sp = NULL, *pd = NULL;
    char *ui = NULL;
    json_t *extra = NULL, *instr = NULL;
    bool stacked_avail = false;
    json_t *ret = NULL;

#define GI2_TC(dst, name, a) do { \
        dst = gi2_call(c, name, a); \
        if (gi2_is_raise(dst)) { \
            if (gi2_is_exception(dst)) goto catch; \
            ret = dst; dst = NULL; goto done; \
        } \
    } while (0)

    GI2_TC(skill_cmds, "get_skill_commands", json_object());
    GI2_TC(ck, "resolve_skill_command_key", gi2_arg1("command", command));
    if (gi2_is_null(ck)) {
        json_t *ur = gi2_call(c, "self._hm_unknown_slash_reply",
                              gi2_arg2("command", command, "source", src));
        if (gi2_is_raise(ur)) {
            if (gi2_is_exception(ur)) goto catch;
            ret = ur; goto done;
        }
        ret = ur;
        goto done;
    }
    const char *ck_s = gi2_str(ck);
    if (!ck_s) goto catch;   /* non-str key: dict indexing TypeError domain */
    const char *plat = gi2_field_str(src, "platform", NULL);
    ui_r = gi2_ev_command_args(c);
    if (gi2_is_raise(ui_r)) {
        if (gi2_is_exception(ui_r)) goto catch;
        ret = ui_r; ui_r = NULL; goto done;
    }
    if (!gi2_str(ui_r)) goto catch;   /* .strip AttributeError -> except */
    ui = gi2_strip(ui_r->str_val);

    /* inner try: split_stacked_skill_commands; on ANY Exception fallback */
    sp = gi2_call(c, "split_stacked_skill_commands",
                  gi2_args_str("user_instruction", ui));
    if (gi2_is_raise(sp)) {
        if (!gi2_is_exception(sp)) { ret = sp; sp = NULL; goto done; }
        json_free(sp); sp = NULL;
        stacked_avail = false;
        extra = json_array();
        instr = json_string(ui);
    } else if (sp && sp->type == JSON_ARRAY && json_len(sp) == 2) {
        stacked_avail = true;
        extra = json_copy(json_get(sp, 0));
        instr = json_copy(json_get(sp, 1));
    } else {
        /* unpack ValueError -> inner except fallback */
        json_free(sp); sp = NULL;
        stacked_avail = false;
        extra = json_array();
        instr = json_string(ui);
    }

    /* skill_cmds[cmd_key].get("name", "") — KeyError/TypeError -> catch */
    if (!skill_cmds || skill_cmds->type != JSON_OBJECT) goto catch;
    json_t *entry = json_obj_get(skill_cmds, ck_s);
    if (!entry) goto catch;
    if (entry->type != JSON_OBJECT) goto catch;
    const char *skill_name = gi2_field_str(entry, "name", "");
    bool extra_nonempty = extra && extra->type == JSON_ARRAY && json_len(extra) > 0;
    if (plat && plat[0] && (skill_name[0] || extra_nonempty)) {
        GI2_TC(pd, "get_disabled_skill_names", gi2_args_str("platform", plat));
        if (!pd || pd->type != JSON_ARRAY) goto catch;  /* `in` TypeError */
        if (skill_name[0] && gi2_arr_has_str(pd, skill_name)) {
            json_t *ta = gi2_args_str("key", "gateway.skills.disabled_for_platform");
            json_set_hdr(ta, "name", skill_name);
            json_set_hdr(ta, "platform", plat);
            json_t *tr = gi2_call(c, "t", ta);
            if (gi2_is_raise(tr)) {
                if (gi2_is_exception(tr)) goto catch;
                ret = tr; goto done;
            }
            ret = tr;
            goto done;
        }
        /* _disabled_extra comprehension (atomic; entries cannot raise) */
        json_t *dis = json_array();
        if (extra && extra->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(extra); i++) {
                const char *k2 = gi2_str(json_get(extra, i));
                json_t *e2 = k2 ? json_obj_get(skill_cmds, k2) : NULL;
                const char *nm = gi2_field_str(e2, "name", "");
                if (gi2_arr_has_str(pd, nm)) json_append(dis, json_string(nm));
            }
        }
        if (json_len(dis) > 0) {
            size_t cap = 64;
            for (size_t i = 0; i < json_len(dis); i++)
                cap += strlen(gi2_str(json_get(dis, i))) + 2;
            char *names = malloc(cap);
            names[0] = '\0';
            for (size_t i = 0; i < json_len(dis); i++) {
                if (i) strcat(names, ", ");
                strcat(names, gi2_str(json_get(dis, i)));
            }
            json_t *ta = gi2_args_str("key", "gateway.skills.stacked_disabled");
            json_set(ta, "names", json_string(names));
            json_set_hdr(ta, "platform", plat);
            free(names);
            json_free(dis);
            json_t *tr = gi2_call(c, "t", ta);
            if (gi2_is_raise(tr)) {
                if (gi2_is_exception(tr)) goto catch;
                ret = tr; goto done;
            }
            ret = tr;
            goto done;
        }
        json_free(dis);
    }
    if (extra_nonempty && stacked_avail) {
        json_t *keys = json_array();
        json_append(keys, json_copy(ck));
        if (extra && extra->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(extra); i++)
                json_append(keys, json_copy(json_get(extra, i)));
        json_t *sa = gi2_arg2("keys", keys, "instruction", instr);
        json_set_hdr(sa, "task_id", key);
        json_free(keys);
        json_t *sr;
        GI2_TC(sr, "build_stacked_skill_invocation_message", sa);
        if (!gi2_truthy(sr)) {
            json_t *tr = gi2_call(c, "t",
                                  gi2_arg2("key", json_string("gateway.skills.stacked_load_failed"),
                                           "command", command));
            if (gi2_is_raise(tr)) {
                if (gi2_is_exception(tr)) goto catch;
                ret = tr; goto done;
            }
            ret = tr;
            goto done;
        }
        json_t *parts[3];
        if (!gi2_unpack(sr, 3, parts)) {
            json_t *e = parts[0];
            if (gi2_is_exception(e)) goto catch;
            ret = e; goto done;
        }
        if (ev && ev->type == JSON_OBJECT) json_set(ev, "text", json_copy(parts[0]));
    } else {
        json_t *ma = gi2_arg2("cmd_key", ck, "user_instruction", json_string(ui));
        json_set_hdr(ma, "task_id", key);
        json_t *msg;
        GI2_TC(msg, "build_skill_invocation_message", ma);
        if (gi2_truthy(msg)) {
            if (ev && ev->type == JSON_OBJECT) json_set(ev, "text", msg);
            else json_free(msg);
        } else {
            json_free(msg);
        }
    }
    ret = NULL;
    goto done;

catch:
    /* except Exception as e: logger.debug(...) — silent */
    ret = NULL;
done:
    json_free(skill_cmds);
    json_free(ck);
    json_free(ui_r);
    json_free(sp);
    json_free(pd);
    free(ui);
    json_free(extra);
    json_free(instr);
    return ret;
#undef GI2_TC
}

/* PoP: gi2_hm_pending_reply_intercepts @ gateway/run_inbound.py:_hm_pending_reply_intercepts */
static json_t *gi2_hm_pending_reply_intercepts(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    json_t *agc = ev ? json_obj_get(ev, "allow_gateway_control") : NULL;
    if (!agc) return gi2_err("AttributeError", "allow_gateway_control");
    if (!gi2_truthy(agc)) return NULL;
    if (gi2_truthy(ev ? json_obj_get(ev, "_fifo_only") : NULL)) return NULL;
    if (gi2_truthy(json_obj_get(c->state, "executing_admission"))) return NULL;
    json_t *reply = gi2_call(c, "self._hm_update_prompt_reply",
                             gi2_arg2("event", ev, "key", json_string(key)));
    GI2_CHK(reply);
    if (gi2_is_null(reply)) {
        json_free(reply);
        reply = gi2_call(c, "self._hm_clarify_reply",
                         gi2_arg3("event", ev, "source", src, "key", json_string(key)));
        GI2_CHK(reply);
    }
    if (gi2_is_null(reply)) {
        json_free(reply);
        reply = gi2_call(c, "self._hm_slash_confirm_reply",
                         gi2_arg2("event", ev, "key", json_string(key)));
        GI2_CHK(reply);
    }
    return reply;
}

/* PoP: gi2_hm_dispatch_idle_commands @ gateway/run_inbound.py:_hm_dispatch_idle_commands */
static json_t *gi2_hm_dispatch_idle_commands(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    json_t *se = gi2_call(c, "self._route_stop_epoch", gi2_args_str("key", key));
    GI2_CHK(se);

    json_t *ra = gi2_arg3("event", ev, "source", src, "key", json_string(key));
    json_t *rr = gi2_hm_resolve_command(c, ra);
    json_free(ra);
    GI2_CHK(rr);
    json_t *rp[4];
    if (!gi2_unpack(rr, 4, rp)) { json_t *e = rp[0]; json_free(rr); return e; }
    json_t *h = json_copy(rp[0]), *result = json_copy(rp[1]);
    json_t *command = json_copy(rp[2]), *canonical = json_copy(rp[3]);
    json_free(rr);

    if (!gi2_truthy(h)) {
        json_t *ca = json_object();
        json_set(ca, "event", json_copy(ev));
        json_set(ca, "source", json_copy(src));
        json_set_hdr(ca, "key", key);
        json_set(ca, "canonical", json_copy(canonical));
        json_t *cr = gi2_hm_dispatch_canonical_command(c, ca);
        gi2_sync_text(ev, json_obj_get(ca, "event"));
        json_free(ca);
        if (gi2_is_raise(cr)) { json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se); return cr; }
        json_t *cp[2];
        if (!gi2_unpack(cr, 2, cp)) {
            json_t *e = cp[0];
            json_free(cr); json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se);
            return e;
        }
        json_free(h); json_free(result);
        h = json_copy(cp[0]); result = json_copy(cp[1]);
        json_free(cr);
    }
    if (!gi2_truthy(h)) {
        json_t *qa = gi2_arg3("event", ev, "source", src, "command", command);
        json_t *qr = gi2_call(c, "self._hm_dispatch_quick_and_plugin_commands", qa);
        if (gi2_is_raise(qr)) { json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se); return qr; }
        json_t *qp[3];
        if (!gi2_unpack(qr, 3, qp)) {
            json_t *e = qp[0];
            json_free(qr); json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se);
            return e;
        }
        json_free(h); json_free(result); json_free(command);
        h = json_copy(qp[0]); result = json_copy(qp[1]); command = json_copy(qp[2]);
        json_free(qr);
    }
    if (!gi2_truthy(h)) {
        /* _run_in_executor_with_context runs the ported sibling synchronously
         * (no trace of its own), exactly like asyncio.to_thread. */
        json_t *sa = json_object();
        json_set(sa, "event", json_copy(ev));
        json_set(sa, "source", json_copy(src));
        json_set_hdr(sa, "key", key);
        json_set(sa, "command", json_copy(command));
        json_t *sr = gi2_hm_skill_slash_rewrite(c, sa);
        gi2_sync_text(ev, json_obj_get(sa, "event"));
        json_free(sa);
        if (gi2_is_raise(sr)) { json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se); return sr; }
        json_free(result);
        result = sr;
        json_free(h);
        h = json_bool(!gi2_is_null(sr));
    }
    if (!gi2_truthy(h) && gi2_truthy(command)) {
        json_t *aa = json_object();
        json_set(aa, "event", json_copy(ev));
        json_set(aa, "source", json_copy(src));
        json_set_hdr(aa, "key", key);
        json_set(aa, "stop_epoch", json_copy(se));
        json_t *ar = gi2_call(c, "self._admit_command_turn", aa);
        if (gi2_is_raise(ar)) { json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se); return ar; }
        json_t *ap[2];
        if (!gi2_unpack(ar, 2, ap)) {
            json_t *e = ap[0];
            json_free(ar); json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se);
            return e;
        }
        json_free(h); json_free(result);
        h = json_copy(ap[0]); result = json_copy(ap[1]);
        json_free(ar);
    }
    json_t *ret = gi2_pair(h, result);
    json_free(h); json_free(result); json_free(command); json_free(canonical); json_free(se);
    return ret;
}

/* PoP: gi2_hm_rescue_orphaned_fifo @ gateway/run_inbound.py:_hm_rescue_orphaned_fifo */
static json_t *gi2_hm_rescue_orphaned_fifo(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    bool internal = json_get_bool(args, "is_internal", false);
    const char *key = json_get_str(args, "key", "");
    json_t *fallback = gi2_triple(ev, src, json_bool(internal));
    /* try: ... except Exception: logger.debug (silent) -> original triple */
    json_t *ad = gi2_call(c, "self._delivery_adapter_for", gi2_arg1("source", src));
    if (gi2_is_raise(ad)) {
        if (gi2_is_exception(ad)) { json_free(ad); return fallback; }
        json_free(fallback);
        return ad;
    }
    bool skip = gi2_is_null(ad);
    if (!skip) skip = gi2_truthy(ev ? json_obj_get(ev, "internal") : NULL);
    json_t *cmd = NULL;
    if (!skip) {
        cmd = gi2_ev_command(c);
        if (gi2_is_raise(cmd)) {
            if (gi2_is_exception(cmd)) { json_free(cmd); json_free(ad); return fallback; }
            json_free(fallback); json_free(ad);
            return cmd;
        }
        skip = gi2_truthy(cmd);
    }
    if (skip) {
        json_free(cmd); json_free(ad);
        return fallback;
    }
    json_free(cmd);
    json_t *rescued = gi2_call(c, "self._rescue_orphaned_overflow",
                               gi2_arg2("key", json_string(key), "adapter", ad));
    if (gi2_is_raise(rescued)) {
        if (gi2_is_exception(rescued)) { json_free(rescued); json_free(ad); return fallback; }
        json_free(fallback); json_free(ad);
        return rescued;
    }
    if (gi2_is_null(rescued)) {
        json_free(rescued); json_free(ad);
        return fallback;
    }
    json_t *enq = gi2_call(c, "self._enqueue_fifo",
                           gi2_arg3("key", json_string(key), "event", ev, "adapter", ad));
    json_free(ad);
    if (gi2_is_raise(enq)) {
        if (gi2_is_exception(enq)) { json_free(enq); json_free(rescued); return fallback; }
        json_free(fallback); json_free(rescued);
        return enq;
    }
    json_free(enq);
    json_t *rsrc = rescued->type == JSON_OBJECT ? json_obj_get(rescued, "source") : NULL;
    json_t *src_out = !gi2_is_null(rsrc) ? rsrc : src;
    bool r_internal = gi2_truthy(rescued->type == JSON_OBJECT
                                     ? json_obj_get(rescued, "internal") : NULL);
    json_t *ret = gi2_triple(rescued, src_out, json_bool(r_internal));
    json_free(rescued);
    json_free(fallback);
    return ret;
}

/* PoP: gi2_hm_authority_admission @ gateway/run_inbound.py:_hm_authority_admission */
static json_t *gi2_hm_authority_admission(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    bool internal = json_get_bool(args, "is_internal", false);
    json_t *auth = gi2_call(c, "active_authority", json_object());
    GI2_CHK(auth);
    if (!gi2_is_null(auth)) {
        /* live order: authority is not None AND not get_command() AND not
         * is_internal — get_command fires even for internal events. */
        json_t *cmd = gi2_ev_command(c);
        GI2_CHK(cmd);
        bool has_cmd = gi2_truthy(cmd);
        json_free(cmd);
        if (!has_cmd && !internal
                && !gi2_truthy(json_obj_get(c->state, "executing_admission"))) {
            json_t *run = gi2_call(c, "self._is_session_running", gi2_args_str("key", key));
            GI2_CHK(run);
            bool busy = gi2_truthy(run)
                && !gi2_truthy(ev ? json_obj_get(ev, "_fifo_only") : NULL);
            json_free(run);
            if (busy) {
                json_t *ad = gi2_call(c, "self._delivery_adapter_for", gi2_arg1("source", src));
                GI2_CHK(ad);
                json_t *st = gi2_call(c, "stamp_busy_arrival",
                                      gi2_arg2("adapter", ad, "event", ev));
                json_free(ad);
                GI2_CHK(st);
                json_free(st);
            }
            /* try: return await _hm_admit_to_fifo(...)
             * finally: settle_busy_arrival(event) */
            json_t *fa = json_object();
            json_set(fa, "authority", json_copy(auth));
            json_set(fa, "event", json_copy(ev));
            json_set(fa, "source", json_copy(src));
            json_set_hdr(fa, "key", key);
            json_set(fa, "busy", json_bool(busy));
            json_t *fr = gi2_hm_admit_to_fifo(c, fa);
            json_free(fa);
            json_t *settle = gi2_call(c, "settle_busy_arrival", gi2_arg1("event", ev));
            if (gi2_is_raise(settle)) {
                json_free(fr); json_free(auth);
                return settle;   /* finally raise overrides the body */
            }
            json_free(settle);
            json_free(auth);
            return fr;
        }
    }
    if (gi2_is_null(auth) && !internal) {
        json_t *cmd = gi2_ev_command(c);
        GI2_CHK(cmd);
        bool has_cmd = gi2_truthy(cmd);
        json_free(cmd);
        if (!has_cmd && !gi2_is_null(json_obj_get(c->state, "session_authority"))) {
            json_free(auth);
            return gi2_refusal("profile_mismatch", NULL, NULL);
        }
    }
    json_free(auth);
    return gi2_pair(json_bool(false), NULL);
}

/* PoP: gi2_hm_admit_to_fifo @ gateway/run_inbound.py:_hm_admit_to_fifo */
static json_t *gi2_hm_admit_to_fifo(gi2_ctx_t *c, json_t *args) {
    json_t *auth = json_obj_get(args, "authority");
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    bool busy = json_get_bool(args, "busy", false);
    if (busy) {
        json_t *ap = gi2_call(c, "self._route_plaintext_approval_while_busy",
                              gi2_arg2("event", ev, "key", json_string(key)));
        GI2_CHK(ap);
        bool landed = gi2_truthy(ap);
        json_free(ap);
        if (!landed) {
            json_t *bp = gi2_call(c, "self._hm_busy_preempt",
                                  gi2_arg3("event", ev, "source", src, "key", json_string(key)));
            GI2_CHK(bp);
            landed = gi2_truthy(bp);
            json_free(bp);
        }
        if (landed) return gi2_pair(json_bool(true), NULL);
    }
    json_t *pc = gi2_call(c, "self._hm_post_admission_consume",
                          gi2_arg3("event", ev, "source", src, "key", json_string(key)));
    GI2_CHK(pc);
    json_t *cp[2];
    if (!gi2_unpack(pc, 2, cp)) { json_t *e = cp[0]; json_free(pc); return e; }
    if (gi2_truthy(cp[0])) {
        json_t *ret = gi2_pair(json_bool(true), cp[1]);
        json_free(pc);
        return ret;
    }
    json_free(pc);
    if (gi2_truthy(ev ? json_obj_get(ev, "_fifo_only") : NULL)) {
        json_t *ep = ev ? json_obj_get(ev, "_stop_epoch") : NULL;
        if (!ep) return gi2_err("AttributeError", "_stop_epoch");
        json_t *epoch = gi2_call(c, "self._route_stop_epoch", gi2_args_str("key", key));
        GI2_CHK(epoch);
        bool same = gi2_val_eq(ep, epoch);
        json_free(epoch);
        if (!same) return gi2_pair(json_bool(true), NULL);
    }
    if (busy && gi2_truthy(ev ? json_obj_get(ev, "_gateway_pending_stt_clips") : NULL)) {
        json_t *hv = gi2_call(c, "hand_prepared_voice_to_turn", gi2_arg1("event", ev));
        GI2_CHK(hv);
        json_free(hv);
    }
    json_t *nb = gi2_call(c, "note_busy_admitting", gi2_arg1("event", ev));
    GI2_CHK(nb);
    json_free(nb);
    json_t *notice = NULL;
    if (busy) {
        notice = gi2_call(c, "pending_setup_busy_notice",
                          gi2_arg2("key", json_string(key), "source", src));
        GI2_CHK(notice);
    }
    if (!gi2_is_null(notice)) {
        json_t *ad = gi2_call(c, "self._delivery_adapter_for", gi2_arg1("source", src));
        if (gi2_is_raise(ad)) { json_free(notice); return ad; }
        json_t *ack = gi2_call(c, "self._send_busy_ack_reply",
                               gi2_arg3("event", ev, "adapter", ad, "notice", notice));
        json_free(ad);
        if (gi2_is_raise(ack)) { json_free(notice); return ack; }
        json_free(ack);
    }
    json_free(notice);
    json_t *am = gi2_call(c, "admit_message", gi2_arg2("authority", auth, "event", ev));
    GI2_CHK(am);
    json_t *ret = gi2_pair(json_bool(true), am);
    json_free(am);
    return ret;
}

/* PoP: gi2_handle_message @ gateway/run_inbound.py:_handle_message */
static json_t *gi2_handle_message(gi2_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    const char *mt = json_get_str(ev, "message_type", "");
    static const char *const types[] = {
        "text", "location", "photo", "video", "audio", "voice",
        "document", "sticker", "command"
    };
    bool ok = false;
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++)
        if (strcmp(mt, types[i]) == 0) { ok = true; break; }
    if (!ok) {
        char esc[256]; size_t n = 0;
        for (const char *p = mt; *p && n + 2 < sizeof(esc); p++) {
            unsigned char ch = (unsigned char)*p;
            if (ch == '\\' || ch == '\'') { esc[n++] = '\\'; esc[n++] = (char)ch; }
            else esc[n++] = (char)ch;
        }
        esc[n] = '\0';
        char msg[320];
        snprintf(msg, sizeof(msg), "'%s' is not a valid MessageType", esc);
        return gi2_err("ValueError", msg);
    }
    json_t *ad = gi2_call(c, "self._hm_admit_event", gi2_arg1("event", ev));
    GI2_CHK(ad);
    if (gi2_is_null(ad)) { json_free(ad); return NULL; }
    json_t *ap[3];
    if (!gi2_unpack(ad, 3, ap)) { json_t *e = ap[0]; json_free(ad); return e; }
    json_t *ev2 = json_copy(ap[0]);
    {
        const char *mt2 = json_get_str(ev2, "message_type", "");
        bool ok2 = false;
        for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++)
            if (strcmp(mt2, types[i]) == 0) { ok2 = true; break; }
        if (!ok2) {
            char esc[256]; size_t n = 0;
            for (const char *p = mt2; *p && n + 2 < sizeof(esc); p++) {
                unsigned char ch = (unsigned char)*p;
                if (ch == '\\' || ch == '\'') { esc[n++] = '\\'; esc[n++] = (char)ch; }
                else esc[n++] = (char)ch;
            }
            esc[n] = '\0';
            char msg[320];
            snprintf(msg, sizeof(msg), "'%s' is not a valid MessageType", esc);
            json_t *e2 = gi2_err("ValueError", msg);
            json_free(ev2);
            return e2;
        }
    }
    json_t *src = json_copy(ap[1]);
    bool internal = gi2_truthy(ap[2]);
    json_free(ad);

    if (!internal) {
        json_t *rc = gi2_call(c, "record_gateway_slash_command", gi2_arg1("event", ev2));
        if (gi2_is_raise(rc)) { json_free(ev2); json_free(src); return rc; }
        json_free(rc);
    }
    json_t *pn = gi2_call(c, "self._hm_estop_gate",
                          gi2_arg3("event", ev2, "source", src,
                                   "is_internal", json_bool(internal)));
    if (gi2_is_raise(pn)) { json_free(ev2); json_free(src); return pn; }
    if (!gi2_is_null(pn)) { json_free(ev2); json_free(src); return pn; }
    json_free(pn);

    json_t *kr = gi2_call(c, "self._session_key_for_source", gi2_arg1("source", src));
    if (gi2_is_raise(kr)) { json_free(ev2); json_free(src); return kr; }
    char *key = gi2_strdup(gi2_str(kr) ? gi2_str(kr) : "");
    json_free(kr);

    if (!internal) {
        json_t *ia = gi2_arg3("event", ev2, "source", src, "key", json_string(key));
        json_t *reply = gi2_hm_pending_reply_intercepts(c, ia);
        json_free(ia);
        if (gi2_is_raise(reply)) { json_free(ev2); json_free(src); free(key); return reply; }
        if (!gi2_is_null(reply)) { json_free(ev2); json_free(src); free(key); return reply; }
        json_free(reply);
    }

    json_t *oa = json_object();
    json_set(oa, "event", json_copy(ev2));
    json_set(oa, "source", json_copy(src));
    json_set_hdr(oa, "key", key);
    json_set(oa, "is_internal", json_bool(internal));
    json_t *or_ = gi2_hm_authority_admission(c, oa);
    json_free(oa);
    if (gi2_is_raise(or_)) { json_free(ev2); json_free(src); free(key); return or_; }
    json_t *op[2];
    if (!gi2_unpack(or_, 2, op)) {
        json_t *e = op[0];
        json_free(or_); json_free(ev2); json_free(src); free(key);
        return e;
    }
    if (gi2_truthy(op[0])) {
        json_t *ret = json_copy(op[1]);
        json_free(or_); json_free(ev2); json_free(src); free(key);
        return ret;
    }
    json_free(or_);

    json_t *evict = gi2_call(c, "self._hm_evict_idle_stale_agent", gi2_args_str("key", key));
    if (gi2_is_raise(evict)) { json_free(ev2); json_free(src); free(key); return evict; }
    json_free(evict);
    json_t *r1 = gi2_call(c, "self._is_session_running", gi2_args_str("key", key));
    if (gi2_is_raise(r1)) { json_free(ev2); json_free(src); free(key); return r1; }
    if (gi2_truthy(r1)) {
        json_t *er = gi2_call(c, "self._hm_evict_reaped_agent", gi2_args_str("key", key));
        if (gi2_is_raise(er)) { json_free(r1); json_free(ev2); json_free(src); free(key); return er; }
        json_free(er);
    }
    json_free(r1);
    json_t *r2 = gi2_call(c, "self._is_session_running", gi2_args_str("key", key));
    if (gi2_is_raise(r2)) { json_free(ev2); json_free(src); free(key); return r2; }
    if (gi2_truthy(r2)) {
        json_free(r2);
        json_t *rm = gi2_call(c, "self._hm_handle_running_session_message",
                              gi2_arg3("event", ev2, "source", src, "key", json_string(key)));
        json_free(ev2); json_free(src); free(key);
        return rm;
    }
    json_free(r2);

    json_t *ida = gi2_arg3("event", ev2, "source", src, "key", json_string(key));
    json_t *ir = gi2_hm_dispatch_idle_commands(c, ida);
    json_free(ida);
    if (gi2_is_raise(ir)) { json_free(ev2); json_free(src); free(key); return ir; }
    json_t *ip[2];
    if (!gi2_unpack(ir, 2, ip)) {
        json_t *e = ip[0];
        json_free(ir); json_free(ev2); json_free(src); free(key);
        return e;
    }
    if (gi2_truthy(ip[0])) {
        json_t *ret = json_copy(ip[1]);
        json_free(ir); json_free(ev2); json_free(src); free(key);
        return ret;
    }
    json_free(ir);

    if (!internal) {
        json_t *lobby = gi2_call(c, "self._is_telegram_topic_root_lobby", gi2_arg1("source", src));
        if (gi2_is_raise(lobby)) { json_free(ev2); json_free(src); free(key); return lobby; }
        if (gi2_truthy(lobby)) {
            json_free(lobby);
            json_t *rem = gi2_call(c, "self._should_send_telegram_lobby_reminder",
                                   gi2_arg1("source", src));
            if (gi2_is_raise(rem)) { json_free(ev2); json_free(src); free(key); return rem; }
            if (gi2_truthy(rem)) {
                json_free(rem);
                json_t *lm = gi2_call(c, "self._telegram_topic_root_lobby_message", json_object());
                json_free(ev2); json_free(src); free(key);
                return lm;
            }
            json_free(rem);
            json_free(ev2); json_free(src); free(key);
            return NULL;
        }
        json_free(lobby);
        if (gi2_truthy(json_obj_get(c->state, "_external_drain_active"))) {
            json_t *dm = gi2_t(c, "gateway.busy.draining_maintenance");
            json_free(ev2); json_free(src); free(key);
            return dm;
        }
    }

    json_t *cl = gi2_call(c, "self._claim_active_session_slot",
                          gi2_arg2("key", json_string(key), "source", src));
    if (gi2_is_raise(cl)) { json_free(ev2); json_free(src); free(key); return cl; }
    json_t *clp[2];
    if (!gi2_unpack(cl, 2, clp)) {
        json_t *e = clp[0];
        json_free(cl); json_free(ev2); json_free(src); free(key);
        return e;
    }
    json_t *lease = json_copy(clp[0]);
    if (!gi2_is_null(clp[1])) {
        json_t *ret = json_copy(clp[1]);
        json_free(cl); json_free(lease); json_free(ev2); json_free(src); free(key);
        return ret;
    }
    json_free(cl);

    json_t *ra = json_object();
    json_set(ra, "event", json_copy(ev2));
    json_set(ra, "source", json_copy(src));
    json_set(ra, "is_internal", json_bool(internal));
    json_set_hdr(ra, "key", key);
    json_t *rr = gi2_hm_rescue_orphaned_fifo(c, ra);
    json_free(ra);
    if (gi2_is_raise(rr)) { json_free(lease); json_free(ev2); json_free(src); free(key); return rr; }
    json_t *rp2[3];
    if (!gi2_unpack(rr, 3, rp2)) {
        json_t *e = rp2[0];
        json_free(rr); json_free(lease); json_free(ev2); json_free(src); free(key);
        return e;
    }
    json_free(ev2); json_free(src);
    ev2 = json_copy(rp2[0]);
    src = json_copy(rp2[1]);
    internal = gi2_truthy(rp2[2]);
    json_free(rr);

    /* claim the session slot: get-or-create state + sentinel/event/clock */
    json_t *node = gi2_session_state(c, key);
    json_t *turn = json_obj_get(node, "turn");
    if (!turn || turn->type != JSON_OBJECT) {
        json_free(lease); json_free(ev2); json_free(src); free(key);
        return gi2_err("AttributeError", "turn");
    }
    if (!gi2_is_null(lease)) json_set(turn, "lease", json_copy(lease));
    json_free(lease);
    json_set(turn, "agent", json_string(GI2_SENTINEL));
    json_set(turn, "event", json_copy(ev2));
    json_t *ts = gi2_call(c, "time.time", json_object());
    if (gi2_is_raise(ts)) { json_free(ev2); json_free(src); free(key); return ts; }
    json_set(turn, "started_ts", ts);
    json_t *src_clock = gi2_call(c, "start_reply_clock",
                                 gi2_arg2("source", src, "internal", json_bool(internal)));
    if (gi2_is_raise(src_clock)) { json_free(ev2); json_free(src); free(key); return src_clock; }
    json_free(src_clock);
    json_t *pa = gi2_call(c, "self._persist_active_agents", json_object());
    if (gi2_is_raise(pa)) { json_free(ev2); json_free(src); free(key); return pa; }
    json_free(pa);
    json_t *gen = gi2_call(c, "self._begin_session_run_generation", gi2_args_str("key", key));
    if (gi2_is_raise(gen)) { json_free(ev2); json_free(src); free(key); return gen; }

    /* try: arm / consume / agent / post-turn hooks ... finally: restore */
    json_t *body_ret = NULL;   /* owned success return */
    json_t *pending = NULL;    /* owned propagating raise */
    json_t *r;

    r = gi2_call(c, "arm_admitted_moa", gi2_arg2("key", json_string(key), "event", ev2));
    if (gi2_is_raise(r)) { pending = r; goto finally; }
    json_free(r);

    if (!internal) {
        r = gi2_call(c, "self._hm_post_admission_consume",
                     gi2_arg3("event", ev2, "source", src, "key", json_string(key)));
        if (gi2_is_raise(r)) { pending = r; goto finally; }
        json_t *pcp[2];
        if (!gi2_unpack(r, 2, pcp)) {
            pending = pcp[0];
            json_free(r);
            goto finally;
        }
        if (gi2_truthy(pcp[0])) {
            body_ret = json_copy(pcp[1]);
            json_free(r);
            goto finally;
        }
        json_free(r);
    }

    json_t *hma = json_object();
    json_set(hma, "event", json_copy(ev2));
    json_set(hma, "source", json_copy(src));
    json_set_hdr(hma, "key", key);
    json_set(hma, "run_generation", json_copy(gen));
    r = gi2_call(c, "self._handle_message_with_agent", hma);
    if (gi2_is_raise(r)) {
        if (gi2_is_kind(r, "TurnLeaseTimeoutError")) {
            json_free(r);
            /* logger.error(... exc.session_id ...) is silent */
            json_t *tr = gi2_t(c, "gateway.busy.another_turn_running");
            if (gi2_is_raise(tr)) { pending = tr; goto finally; }
            body_ret = tr;
            goto finally;
        }
        pending = r;
        goto finally;
    }
    {
        json_t *agent_result = r;
        json_t *pha = json_object();
        json_set(pha, "agent_result", json_copy(agent_result));
        json_set(pha, "source", json_copy(src));
        json_set(pha, "is_internal", json_bool(internal));
        json_set(pha, "event", json_copy(ev2));
        json_t *ph = gi2_call(c, "self._run_post_turn_hooks", pha);
        if (gi2_is_raise(ph)) {
            if (gi2_is_exception(ph)) {
                json_free(ph);   /* except Exception: logger.debug — silent */
            } else {
                pending = ph;
                json_free(agent_result);
                goto finally;
            }
        } else {
            json_free(ph);
        }
        body_ret = agent_result;
    }

finally:
    /* every finally seam raise overrides the body return/raise */
    r = gi2_call(c, "self._restore_pending_one_turn_model_override",
                 gi2_arg2("key", json_string(key), "run_generation", gen));
    if (gi2_is_raise(r)) {
        json_free(body_ret); json_free(pending); json_free(gen);
        json_free(ev2); json_free(src); free(key);
        return r;
    }
    json_free(r);
    if (!gi2_truthy(ev2 && ev2->type == JSON_OBJECT
                        ? json_obj_get(ev2, "_turn_marker_handoff") : NULL)) {
        r = gi2_call(c, "self._clear_durable_active_turn", gi2_arg1("event", ev2));
        if (gi2_is_raise(r)) {
            json_free(body_ret); json_free(pending); json_free(gen);
            json_free(ev2); json_free(src); free(key);
            return r;
        }
        json_free(r);
    }
    r = gi2_call(c, "self._release_running_agent_state",
                 gi2_arg2("key", json_string(key), "run_generation", gen));
    if (gi2_is_raise(r)) {
        json_free(body_ret); json_free(pending); json_free(gen);
        json_free(ev2); json_free(src); free(key);
        return r;
    }
    json_free(r);
    r = gi2_call(c, "self._release_turn_lease",
                 gi2_arg2("key", json_string(key), "run_generation", gen));
    if (gi2_is_raise(r)) {
        json_free(body_ret); json_free(pending); json_free(gen);
        json_free(ev2); json_free(src); free(key);
        return r;
    }
    json_free(r);
    json_free(gen);
    json_free(ev2);
    json_free(src);
    free(key);
    if (pending) { json_free(body_ret); return pending; }
    return body_ret ? body_ret : json_null();
}