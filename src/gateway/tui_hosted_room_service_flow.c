/*
 * tui_hosted_room_service_flow.c — FLOW port of the UNCLAIMED orchestration
 * surface of tui_gateway/hosted_room_service.py (LIVE authority, py3.14
 * quarry). Self-contained: libjson only. Every function is static thrs_*; the
 * harness (#include "gateway/tui_hosted_room_service_flow.c") wires the ONE
 * universal seam to scripted results and records the ordered trace.
 *
 * PoP GREP (CRLF-aware, tree-wide src/include/lib): hosted_room_service.py has
 * ZERO PoP-credited defs (parent-verified: 79 genuinely unclaimed functions in
 * this module family, none covered). The 20 defs ported here (annotated
 * immediately above each static function) are:
 *   task_member_id, _approvable_attempts, attempt_action, _member_of,
 *   _authority, _grant_revoke_is_terminal, _hosted_room_turn_timeout_seconds,
 *   _hook, disband_fence, route_publication_lock, _member_activity,
 *   _attempt_actions, _stop_pending_refusal, _policy_snapshot, profile_homes,
 *   _route_statuses, _events, _append_room_status, publish_terminal,
 *   peer_admission_receipt
 *
 * FLOW DOCTRINE: port the DECISION SEQUENCE / call order / state transitions /
 * error paths. Every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None); the decision core never
 * touches I/O directly. A raising live call is scripted
 * {"__raise__": "<ExcClass>", "msg": ...}; the core propagates it exactly
 * where the live try/except catches it. `except (TypeError, ValueError)` etc.
 * use the FULL builtin parent table so sibling exceptions (and the
 * BaseException trio) pass through; thrs_is_base_exc is the explicit marker.
 * A MISSING seam key fail-closes: the harness delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present
 * JSON null is a valid None.
 *
 * Seam name table (args are the projected kwargs, null-valued args are dropped
 * by the harness BEFORE recording, identically on both sides):
 *   os.getenv                    {key, default}
 *   discussion.turn_round        {turn_id}
 *   task_progress                {room_id, task_id, execution_generation}
 *   self.local_profiles          {}
 *   policy_checkpoint.snapshot   {room_id, latest_seq}
 *   hosted_rooms.read_events     {room_id, since_seq, limit}
 *   hosted_rooms.append_event    {room_id, event_id, kind, actor, payload,
 *                                 authority_gateway_id, authority_epoch}
 *   self._member_is_peer         {room_id, member_id}
 *   forget_task_progress         {room_id, task_id, execution_generation}
 *   self._prepare_tracked        {binding}
 *   self.runtime.wakeup          {}
 *   self._room                   {room_id}
 *   hosted_rooms.remote_run_receipt {record}
 *   disbanding                   {room_id}   (callable fence attribute)
 *
 * Module constants (seeded in the oracle, hardcoded here):
 *   _APPROVABLE_STATUSES = ("running", "indeterminate", "deferred")
 *   _RETRYABLE_STATUSES  = ("indeterminate", "deferred")
 *   _HOSTED_ROOM_TERMINAL_GRACE_SECONDS = 30.0
 *   host ceilling default HERMES_AGENT_TIMEOUT = 1800.0 (in-function literal)
 *   gateway.hosted_rooms.MAX_LOG_LIMIT = 500
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings. str(x) is exercised only for scalars (string / number /
 *   bool / null); Python repr of containers is NOT modelled (excluded).
 * - int(x) is exercised for number / bool / plain digit strings / null;
 *   container int() is excluded (its TypeError message is type-dependent).
 * - identity / exc / route / decision / binding / service / self are attribute
 *   doubles backed by JSON objects; a missing scalar attribute raises
 *   AttributeError with the live SimpleNamespace type name.
 * - _events fixtures always terminate (has_more false within a few pages);
 *   a non-advancing cursor raises the exact RuntimeError.
 * - callable attrs are scripted {"__callable__": "<marker>"}; C projects them
 *   to that object; non-callable fences project to {"__nullcontext__": true}.
 *
 * Ownership: seam results and returned json_t pointers are malloc'd, caller
 * frees; inputs are borrowed. thrs_call borrows args (caller frees). A
 * propagated raise dict is returned upward as-is; a caught raise dict is
 * freed at its catch site.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"

/* ── universal seam + ordered trace ── */

typedef struct thrs_trace { json_t *events; } thrs_trace_t;

typedef struct thrs_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} thrs_seams_t;

static void thrs_trace_add(thrs_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void thrs_trace_free(thrs_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *thrs_call(const thrs_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *thrs_call0(const thrs_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = thrs_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool thrs_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool thrs_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool thrs_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool thrs_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool thrs_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool thrs_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
/* A bare JSON-number node's scalar value (json_get_num takes an object key). */
static double thrs_node_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}
/* Read a SCALAR json string node directly (json_get_str is an object accessor). */
static const char *thrs_jstr(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static char *thrs_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* Python str() over the documented scalar domain. */
static json_t *thrs_str(const json_t *v) {
    if (!v) return json_string("None");
    switch (v->type) {
        case JSON_STRING: return json_string(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return json_string(v->bool_val ? "True" : "False");
        case JSON_NULL:   return json_string("None");
        case JSON_NUMBER: {
            char *s;
            if (v->num_val == (double)(long long)v->num_val)
                s = thrs_xsf("%.0f", v->num_val);
            else
                s = thrs_xsf("%g", v->num_val);
            json_t *r = json_string(s);
            free(s);
            return r;
        }
        default: {
            char *s = json_serialize(v);
            json_t *r = json_string(s ? s : "");
            free(s);
            return r;
        }
    }
}

/* Python int() over the documented scalar domain. On failure sets *exc to a
 * real raise descriptor (ValueError / TypeError) and returns 0. */
static long long thrs_int(const json_t *v, json_t **exc) {
    if (thrs_is_num(v)) return (long long)v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (thrs_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
                      *p == '\v' || *p == '\f')) p++;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        const char *digits = p;
        while (*p >= '0' && *p <= '9') p++;
        if (p > digits) {
            const char *tail = p;
            while (*tail && (*tail == ' ' || *tail == '\t' || *tail == '\n' ||
                             *tail == '\r' || *tail == '\v' || *tail == '\f')) tail++;
            if (*tail == '\0') {
                long long n = 0;
                for (const char *q = digits; q < p; q++) n = n * 10 + (*q - '0');
                return neg ? -n : n;
            }
        }
        char *msg = thrs_xsf("invalid literal for int() with base 10: '%s'", s);
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("ValueError"));
        json_set(e, "msg", json_string(msg));
        free(msg);
        *exc = e;
        return 0;
    }
    const char *tn = (!v || v->type == JSON_NULL) ? "NoneType"
                   : (v->type == JSON_ARRAY) ? "list"
                   : (v->type == JSON_OBJECT) ? "dict" : "object";
    char *msg = thrs_xsf("int() argument must be a string, a bytes-like object or a real number, not '%s'", tn);
    json_t *e = json_object();
    json_set(e, "__raise__", json_string("TypeError"));
    json_set(e, "msg", json_string(msg));
    free(msg);
    *exc = e;
    return 0;
}

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *thrs_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool thrs_is_raise(const json_t *r) { return thrs_raise_class(r) != NULL; }

static json_t *thrs_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *thrs_raise_spec_fmt(const char *cls, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *buf = malloc((size_t)(n < 0 ? 1 : n + 1));
    if (buf) {
        if (n >= 0) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
        else buf[0] = '\0';
    }
    va_end(ap2);
    json_t *r = thrs_raise_spec(cls, buf ? buf : "");
    free(buf);
    return r;
}

static bool thrs_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
/* Full CPython 3.14 builtin exception parent table. The BaseException trio
 * stays on the BaseException branch ONLY so `except Exception` cannot catch
 * it. Unknown custom kinds base on Exception (mirrors the oracle). */
static const char *thrs_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning", NULL
    };
    if (thrs_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (thrs_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (thrs_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (thrs_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (thrs_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (thrs_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = {
        "NotImplementedError", "RecursionError", "PythonFinalizationError", NULL
    };
    if (thrs_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (thrs_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (thrs_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (thrs_in_names(k, NAME_CHILDREN)) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    return "Exception";
}
static bool thrs_caught(const json_t *r, const char *cls) {
    const char *k = thrs_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = thrs_exc_parent(k);
    }
    return false;
}
/* BaseException trio passes through any `except Exception` / sibling catch. */
static bool thrs_is_base_exc(const json_t *r) {
    const char *k = thrs_raise_class(r);
    return k && (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
                 !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException"));
}

/* Attribute-double helpers: a present null is a valid None; an ABSENT scalar
 * attribute raises AttributeError with the live SimpleNamespace type name. */
static json_t *thrs_attr(const json_t *obj, const char *name, json_t **exc) {
    const json_t *v = json_obj_get(obj, name);
    if (!v) {
        *exc = thrs_raise_spec_fmt("AttributeError",
            "'types.SimpleNamespace' object has no attribute '%s'", name);
        return NULL;
    }
    return json_copy(v);
}

/* ── ports: module-level functions ── */

/* PoP: thrs_task_member_id @ tui_gateway/hosted_room_service.py:task_member_id */
static json_t *thrs_task_member_id(const json_t *task, json_t **exc) {
    if (!thrs_is_obj(task)) {
        *exc = thrs_raise_spec("TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    const json_t *payload = json_obj_get(task, "payload");
    if (!payload) { *exc = thrs_raise_spec("KeyError", "'payload'"); return NULL; }
    const json_t *v = json_obj_get(payload, "target_member_id");
    if (thrs_jtruth(v)) return thrs_str(v);
    const json_t *p2 = json_obj_get(payload, "target_profile");
    if (!p2) { *exc = thrs_raise_spec("KeyError", "'target_profile'"); return NULL; }
    return thrs_str(p2);
}

/* PoP: thrs_member_of @ tui_gateway/hosted_room_service.py:_member_of */
static json_t *thrs_member_of(const json_t *task) {
    const json_t *payload = json_obj_get(task, "payload");
    if (!thrs_is_obj(payload)) payload = NULL;
    const json_t *v = payload ? json_obj_get(payload, "target_member_id") : NULL;
    if (thrs_jtruth(v)) return thrs_str(v);
    v = payload ? json_obj_get(payload, "target_profile") : NULL;
    if (thrs_jtruth(v)) return thrs_str(v);
    return json_string("");
}

/* PoP: thrs_authority @ tui_gateway/hosted_room_service.py:_authority */
static json_t *thrs_authority(const json_t *room, json_t **exc) {
    const json_t *gid = json_obj_get(room, "authority_gateway_id");
    if (!gid) { *exc = thrs_raise_spec("KeyError", "'authority_gateway_id'"); return NULL; }
    const json_t *ep = json_obj_get(room, "authority_epoch");
    if (!ep) { *exc = thrs_raise_spec("KeyError", "'authority_epoch'"); return NULL; }
    json_t *e = NULL;
    long long epoch = thrs_int(ep, &e);
    if (e) { *exc = e; return NULL; }
    json_t *out = json_array();
    json_append(out, thrs_str(gid));
    json_append(out, json_number((double)epoch));
    return out;
}

/* PoP: thrs_attempt_action @ tui_gateway/hosted_room_service.py:attempt_action */
static json_t *thrs_attempt_action(const char *kind, const json_t *task, json_t **exc) {
    json_t *member = thrs_task_member_id(task, exc);
    if (*exc) return NULL;
    const json_t *ident = json_obj_get(task, "identity");
    if (!ident) { json_free(member); *exc = thrs_raise_spec("KeyError", "'identity'"); return NULL; }
    json_t *tid = thrs_attr(ident, "task_id", exc);
    if (*exc) { json_free(member); return NULL; }
    const json_t *gen = json_obj_get(task, "execution_generation");
    if (!gen) {
        json_free(member); json_free(tid);
        *exc = thrs_raise_spec("KeyError", "'execution_generation'");
        return NULL;
    }
    json_t *out = json_object();
    json_set(out, "kind", json_string(kind));
    json_set(out, "member_id", member);
    json_set(out, "task_id", tid);
    json_set(out, "execution_generation", json_copy(gen));
    return out;
}

/* PoP: thrs_grant_revoke_is_terminal @ tui_gateway/hosted_room_service.py:_grant_revoke_is_terminal */
static json_t *thrs_grant_revoke_is_terminal(const json_t *exco) {
    const json_t *sc = json_obj_get(exco, "status_code");
    if (!sc) return thrs_raise_spec("AttributeError",
        "'types.SimpleNamespace' object has no attribute 'status_code'");
    bool c1 = thrs_is_num(sc) && (sc->num_val == 401.0 || sc->num_val == 403.0);
    if (!c1) return json_bool(false);
    const json_t *ec = json_obj_get(exco, "error_code");
    if (!ec) return thrs_raise_spec("AttributeError",
        "'types.SimpleNamespace' object has no attribute 'error_code'");
    bool c2 = thrs_is_str(ec) && (!strcmp(ec->str_val, "invalid_room_grant") ||
                                  !strcmp(ec->str_val, "room_reauthorization_required"));
    return json_bool(c2);
}

/* PoP: thrs_hosted_room_turn_timeout_seconds @ tui_gateway/hosted_room_service.py:_hosted_room_turn_timeout_seconds */
static json_t *thrs_hosted_room_turn_timeout_seconds(const thrs_seams_t *s) {
    json_t *a = json_object();
    json_set(a, "key", json_string("HERMES_AGENT_TIMEOUT"));
    json_set(a, "default", json_string("1800"));
    json_t *v = thrs_call(s, "os.getenv", a);
    json_free(a);
    double agent_timeout = 0.0;
    if (thrs_is_raise(v)) {
        if (thrs_caught(v, "TypeError") || thrs_caught(v, "ValueError")) {
            agent_timeout = 0.0;   /* except (TypeError, ValueError): 0.0 */
            json_free(v);
        } else {
            return v;              /* propagates (incl. BaseException trio) */
        }
    } else if (thrs_is_str(v)) {
        char *end = NULL;
        const char *txt = v->str_val ? v->str_val : "";
        while (*txt == ' ' || *txt == '\t' || *txt == '\n') txt++;
        double d = strtod(txt, &end);
        if (end && *end == '\0') agent_timeout = d;
        else agent_timeout = 0.0;  /* float() ValueError -> 0.0 */
        json_free(v);
    } else if (thrs_is_num(v)) {
        agent_timeout = v->num_val;      /* float(number) */
        json_free(v);
    } else if (v && v->type == JSON_BOOL) {
        agent_timeout = v->bool_val ? 1.0 : 0.0;
        json_free(v);
    } else if (thrs_is_null(v)) {
        agent_timeout = 0.0;             /* float(None) TypeError -> 0.0 */
        json_free(v);
    } else {
        json_free(v);
    }
    if (!(agent_timeout > 0.0)) agent_timeout = 1800.0;
    return json_number(agent_timeout + 30.0);
}

/* PoP: thrs_hook @ tui_gateway/hosted_room_service.py:_hook */
static json_t *thrs_hook(const json_t *obj, const char *name) {
    const json_t *v = json_obj_get(obj, name);
    if (thrs_is_obj(v) && v->c.count == 1) {
        const json_t *m = json_obj_get(v, "__callable__");
        if (m && m->type == JSON_STRING) {
            json_t *out = json_object();
            json_set(out, "__callable__", json_copy(m));
            return out;
        }
    }
    return json_null();
}

/* PoP: thrs_disband_fence @ tui_gateway/hosted_room_service.py:disband_fence */
static json_t *thrs_disband_fence(const thrs_seams_t *s, const json_t *service, const json_t *room_id) {
    const json_t *f = json_obj_get(service, "disbanding");
    if (thrs_is_obj(f) && f->c.count == 1) {
        const json_t *m = json_obj_get(f, "__callable__");
        if (m && m->type == JSON_STRING) {
            json_t *a = json_object();
            json_set(a, "room_id", json_copy(room_id));
            return thrs_call(s, "disbanding", a);
        }
    }
    json_t *out = json_object();
    json_set(out, "__nullcontext__", json_bool(true));
    return out;
}

/* PoP: thrs_route_publication_lock @ tui_gateway/hosted_room_service.py:route_publication_lock */
static json_t *thrs_route_publication_lock(const json_t *service) {
    const json_t *v = json_obj_get(service, "peer_route_lock");
    if (thrs_is_null(v)) {
        json_t *out = json_object();
        json_set(out, "__nullcontext__", json_bool(true));
        return out;
    }
    return json_copy(v);
}

/* PoP: thrs_member_activity @ tui_gateway/hosted_room_service.py:_member_activity */
static json_t *thrs_member_activity(const thrs_seams_t *s, const json_t *task, json_t **exc) {
    const json_t *ident = json_obj_get(task, "identity");
    if (!ident) { *exc = thrs_raise_spec("KeyError", "'identity'"); return NULL; }
    json_t *member = thrs_task_member_id(task, exc);
    if (*exc) return NULL;
    const json_t *st = json_obj_get(task, "status");
    if (!st) { json_free(member); *exc = thrs_raise_spec("KeyError", "'status'"); return NULL; }
    json_t *status = thrs_str(st);
    json_t *thread = thrs_attr(ident, "thread_id", exc);
    if (*exc) { json_free(member); json_free(status); return NULL; }
    json_t *turn = thrs_attr(ident, "turn_id", exc);
    if (*exc) { json_free(member); json_free(status); json_free(thread); return NULL; }

    json_t *row = json_object();
    json_set(row, "member_id", member);
    json_set(row, "status", status);
    json_set(row, "thread_id", thread);

    json_t *a = json_object();
    json_set(a, "turn_id", json_copy(turn));
    json_t *round = thrs_call(s, "discussion.turn_round", a);
    json_free(a);
    json_free(turn);
    if (thrs_is_raise(round)) { *exc = round; return NULL; }
    if (!thrs_is_null(round)) json_set(row, "round_index", json_copy(round));
    json_free(round);

    const json_t *started = json_obj_get(task, "started_at");
    if (started && !thrs_is_null(started)) json_set(row, "started_at", json_copy(started));

    json_t *room = thrs_attr(ident, "room_id", exc);
    if (*exc) return NULL;
    json_t *tid = thrs_attr(ident, "task_id", exc);
    if (*exc) { json_free(room); return NULL; }
    const json_t *gn = json_obj_get(task, "execution_generation");
    if (!gn) { json_free(room); json_free(tid); *exc = thrs_raise_spec("KeyError", "'execution_generation'"); return NULL; }
    json_t *ie = NULL;
    long long gen = thrs_int(gn, &ie);
    if (ie) { json_free(room); json_free(tid); *exc = ie; return NULL; }

    json_t *a2 = json_object();
    json_set(a2, "room_id", room);
    json_set(a2, "task_id", tid);
    json_set(a2, "execution_generation", json_number((double)gen));
    json_t *progress = thrs_call(s, "task_progress", a2);
    if (thrs_is_raise(progress)) { *exc = progress; return NULL; }
    if (thrs_is_obj(progress)) {
        for (size_t i = 0; i < progress->c.count; i++)
            json_set(row, progress->c.keys[i], json_copy(progress->c.items[i]));
    }
    json_free(progress);
    return row;
}

/* ── triple (attempt) compare / dedupe / sort ── */

static bool thrs_triple_eq(const json_t *a, const json_t *b) {
    const json_t *a0 = json_get(a, 0), *b0 = json_get(b, 0);
    const json_t *a1 = json_get(a, 1), *b1 = json_get(b, 1);
    const json_t *a2 = json_get(a, 2), *b2 = json_get(b, 2);
    return !strcmp(thrs_jstr(a0, ""), thrs_jstr(b0, "")) &&
           !strcmp(thrs_jstr(a1, ""), thrs_jstr(b1, "")) &&
           thrs_node_num(a2, 0) == thrs_node_num(b2, 0);
}
static int thrs_triple_cmp(const json_t *a, const json_t *b) {
    int c = strcmp(thrs_jstr(json_get(a, 0), ""), thrs_jstr(json_get(b, 0), ""));
    if (c) return c;
    c = strcmp(thrs_jstr(json_get(a, 1), ""), thrs_jstr(json_get(b, 1), ""));
    if (c) return c;
    double x = thrs_node_num(json_get(a, 2), 0), y = thrs_node_num(json_get(b, 2), 0);
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}
static json_t *thrs_sorted_unique_triples(json_t *raw) {
    size_t n = json_len(raw);
    json_t **v = n ? (json_t **)calloc(n, sizeof(json_t *)) : NULL;
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_copy(json_get(raw, i));
        bool dup = false;
        for (size_t j = 0; j < m; j++) if (thrs_triple_eq(t, v[j])) { dup = true; break; }
        if (dup) json_free(t); else v[m++] = t;
    }
    for (size_t i = 1; i < m; i++) {
        json_t *k = v[i]; size_t j = i;
        while (j > 0 && thrs_triple_cmp(k, v[j - 1]) < 0) { v[j] = v[j - 1]; j--; }
        v[j] = k;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < m; i++) json_append(out, v[i]);
    free(v);
    return out;
}

/* PoP: thrs_approvable_attempts @ tui_gateway/hosted_room_service.py:_approvable_attempts */
static json_t *thrs_approvable_attempts(const json_t *tasks, json_t **exc) {
    json_t *raw = json_array();
    if (thrs_is_arr(tasks)) {
        for (size_t i = 0; i < tasks->c.count; i++) {
            const json_t *t = tasks->c.items[i];
            const json_t *st = json_obj_get(t, "status");
            if (!st) { *exc = thrs_raise_spec("KeyError", "'status'"); return NULL; }
            bool ok = thrs_is_str(st) && (!strcmp(st->str_val, "running") ||
                       !strcmp(st->str_val, "indeterminate") || !strcmp(st->str_val, "deferred"));
            if (!ok) continue;
            /* live tuple order: task_member_id(task) FIRST, then identity.task_id */
            json_t *m = thrs_task_member_id(t, exc);
            if (*exc) return NULL;
            const json_t *ident = json_obj_get(t, "identity");
            if (!ident) { json_free(m); *exc = thrs_raise_spec("KeyError", "'identity'"); return NULL; }
            json_t *tid = thrs_attr(ident, "task_id", exc);
            if (*exc) { json_free(m); return NULL; }
            const json_t *gn = json_obj_get(t, "execution_generation");
            if (!gn) { json_free(m); json_free(tid); *exc = thrs_raise_spec("KeyError", "'execution_generation'"); return NULL; }
            json_t *triple = json_array();
            json_append(triple, m);
            json_append(triple, tid);
            json_append(triple, json_copy(gn));
            json_append(raw, triple);
        }
    }
    json_t *out = thrs_sorted_unique_triples(raw);
    json_free(raw);
    return out;
}

/* PoP: thrs_attempt_actions @ tui_gateway/hosted_room_service.py:_attempt_actions */
static json_t *thrs_attempt_actions(const json_t *tasks_shown, json_t **exc) {
    json_t *out = json_array();
    if (thrs_is_arr(tasks_shown)) {
        for (size_t i = 0; i < tasks_shown->c.count; i++) {
            const json_t *t = tasks_shown->c.items[i];
            const json_t *st = json_obj_get(t, "status");
            if (!st) { *exc = thrs_raise_spec("KeyError", "'status'"); return NULL; }
            bool ok = thrs_is_str(st) && (!strcmp(st->str_val, "indeterminate") ||
                                          !strcmp(st->str_val, "deferred"));
            if (!ok) continue;
            json_t *a = thrs_attempt_action("retry", t, exc);
            if (*exc) return NULL;
            json_append(out, a);
        }
    }
    return out;
}

/* ── ports: HostedRoomService methods (self is a borrowed JSON object) ── */

/* PoP: thrs_stop_pending_refusal @ tui_gateway/hosted_room_service.py:_stop_pending_refusal */
static json_t *thrs_stop_pending_refusal(void) {
    return json_null();   /* live body: return None */
}

/* PoP: thrs_policy_snapshot @ tui_gateway/hosted_room_service.py:_policy_snapshot */
static json_t *thrs_policy_snapshot(const thrs_seams_t *s, const json_t *room, json_t **exc) {
    const json_t *rid = json_obj_get(room, "room_id");
    if (!rid) { *exc = thrs_raise_spec("KeyError", "'room_id'"); return NULL; }
    const json_t *ep = json_obj_get(room, "latest_seq");
    if (!ep) { *exc = thrs_raise_spec("KeyError", "'latest_seq'"); return NULL; }
    json_t *ie = NULL;
    long long seq = thrs_int(ep, &ie);
    if (ie) { *exc = ie; return NULL; }
    json_t *a = json_object();
    json_set(a, "room_id", thrs_str(rid));
    json_set(a, "latest_seq", json_number((double)seq));
    json_t *res = thrs_call(s, "policy_checkpoint.snapshot", a);
    if (thrs_is_raise(res)) { *exc = res; return NULL; }
    return res;
}

/* path join: PurePosixPath(root) / "profiles" / name — conditional separator. */
static char *thrs_path_join3(const char *root, const char *mid, const char *name) {
    size_t nr = strlen(root);           /* PurePosixPath strips trailing '/' */
    while (nr > 1 && root[nr - 1] == '/') nr--;
    size_t nm = strlen(mid), nn = strlen(name);
    bool sep1 = nr > 0 && root[nr - 1] != '/';
    bool sep2 = nm > 0 && mid[nm - 1] != '/';
    char *buf = malloc(nr + nm + nn + 3);
    if (!buf) return strdup("");
    buf[0] = '\0';
    memcpy(buf, root, nr);
    buf[nr] = '\0';
    if (sep1) strcat(buf, "/");
    strcat(buf, mid);
    if (sep2) strcat(buf, "/");
    strcat(buf, name);
    return buf;
}

/* PoP: thrs_profile_homes @ tui_gateway/hosted_room_service.py:profile_homes */
static json_t *thrs_profile_homes(const thrs_seams_t *s, const json_t *self, json_t **exc) {
    json_t *names = thrs_call0(s, "self.local_profiles");
    if (thrs_is_raise(names)) { *exc = names; return NULL; }
    const char *raw_root = json_get_str(self, "root", "");
    size_t nr0 = strlen(raw_root);
    while (nr0 > 1 && raw_root[nr0 - 1] == '/') nr0--;
    char *norm_root = malloc(nr0 + 1);
    if (!norm_root) norm_root = strdup(".");
    else { memcpy(norm_root, raw_root, nr0); norm_root[nr0] = '\0'; }
    const char *default_root = norm_root[0] == '\0' ? "." : norm_root;
    json_t *out = json_object();
    if (thrs_is_arr(names)) {
        for (size_t i = 0; i < names->c.count; i++) {
            const json_t *nv = names->c.items[i];
            const char *name = thrs_is_str(nv) ? nv->str_val : "";
            if (!strcmp(name, "default")) {
                json_set(out, name, json_string(default_root));
            } else {
                char *p = thrs_path_join3(norm_root, "profiles", name);
                json_set(out, name, json_string(p));
                free(p);
            }
        }
    }
    json_free(names);
    free(norm_root);
    return out;
}

/* PoP: thrs_route_statuses @ tui_gateway/hosted_room_service.py:_route_statuses */
static json_t *thrs_route_statuses(const json_t *self, const json_t *room_id) {
    const json_t *states = json_obj_get(self, "_peer_route_status");
    size_t n = thrs_is_arr(states) ? states->c.count : 0;
    json_t **v = n ? (json_t **)calloc(n, sizeof(json_t *)) : NULL;
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *e = states->c.items[i];
        const json_t *rid = json_obj_get(e, "room_id");
        const json_t *mid = json_obj_get(e, "member_id");
        if (!thrs_is_null(room_id)) {
            const char *want = thrs_is_str(room_id) ? room_id->str_val : "";
            const char *have = thrs_jstr(rid, "");
            if (strcmp(want, have)) continue;
        }
        json_t *row = json_object();
        json_set(row, "room_id", json_copy(rid));
        json_set(row, "member_id", json_copy(mid));
        json_set(row, "status", json_copy(json_obj_get(e, "status")));
        v[m++] = row;
    }
    /* sorted(self._peer_route_status.items()) sorts by (room_id, member_id). */
    for (size_t i = 1; i < m; i++) {
        json_t *k = v[i]; size_t j = i;
        while (j > 0) {
            const char *ka = thrs_jstr(json_obj_get(k, "room_id"), "");
            const char *kb = thrs_jstr(json_obj_get(v[j - 1], "room_id"), "");
            int c = strcmp(ka, kb);
            if (!c) c = strcmp(thrs_jstr(json_obj_get(k, "member_id"), ""),
                               thrs_jstr(json_obj_get(v[j - 1], "member_id"), ""));
            if (c >= 0) break;
            v[j] = v[j - 1]; j--;
        }
        v[j] = k;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < m; i++) json_append(out, v[i]);
    free(v);
    return out;
}

/* PoP: thrs_events @ tui_gateway/hosted_room_service.py:_events */
static json_t *thrs_events(const thrs_seams_t *s, const json_t *room_id, json_t **exc) {
    json_t *events = json_array();
    long long cursor = 0;
    for (;;) {
        json_t *a = json_object();
        json_set(a, "room_id", json_copy(room_id));
        json_set(a, "since_seq", json_number((double)cursor));
        json_set(a, "limit", json_number(500.0));  /* MAX_LOG_LIMIT */
        json_t *page = thrs_call(s, "hosted_rooms.read_events", a);
        if (thrs_is_raise(page)) { *exc = page; return NULL; }
        const json_t *rows = json_obj_get(page, "events");
        if (thrs_is_arr(rows)) {
            for (size_t i = 0; i < rows->c.count; i++)
                if (thrs_is_obj(rows->c.items[i])) json_append(events, json_copy(rows->c.items[i]));
        }
        const json_t *cv = json_obj_get(page, "cursor");
        long long next = cursor;
        if (!thrs_is_null(cv)) {
            json_t *ie = NULL;
            next = thrs_int(cv, &ie);
            if (ie) { json_free(page); *exc = ie; return NULL; }
        }
        const json_t *more = json_obj_get(page, "has_more");
        if (!thrs_jtruth(more)) { json_free(page); return events; }
        if (next <= cursor) {
            json_free(page);
            *exc = thrs_raise_spec("RuntimeError", "hosted room replay cursor did not advance");
            return NULL;
        }
        cursor = next;
        json_free(page);
    }
}

/* PoP: thrs_append_room_status @ tui_gateway/hosted_room_service.py:_append_room_status */
static json_t *thrs_append_room_status(const thrs_seams_t *s, const json_t *room, const json_t *decision, json_t **exc) {
    const json_t *deid = json_obj_get(decision, "discussion_event_id");
    if (thrs_is_null(deid)) return NULL;
    json_t *auth = thrs_authority(room, exc);
    if (*exc) return NULL;
    json_t *gid = json_get(auth, 0);
    json_t *epoch = json_get(auth, 1);
    const json_t *cont = json_obj_get(decision, "continues");
    const char *suffix = thrs_jtruth(cont) ? "" : "";
    char *sfx = NULL;
    if (thrs_jtruth(cont)) {
        json_t *cs = thrs_str(cont);
        sfx = thrs_xsf(":c%s", cs->str_val);
        json_free(cs);
        suffix = sfx;
    }
    const json_t *reason = json_obj_get(decision, "reason");
    const json_t *status = json_obj_get(decision, "status");
    const json_t *thread = json_obj_get(decision, "thread_id");
    char *eid = thrs_xsf("dactivity:%s:%s%s", deid->str_val ? deid->str_val : "",
                          thrs_jstr(reason, ""), suffix);
    free(sfx);

    json_t *a = json_object();
    json_set(a, "room_id", thrs_str(json_obj_get(room, "room_id")));
    json_set(a, "event_id", json_string(eid));
    free(eid);
    json_set(a, "kind", json_string("room.activity"));
    json_t *actor = json_object();
    json_set(actor, "kind", json_string("gateway"));
    json_set(actor, "id", json_copy(gid));
    json_set(a, "actor", actor);
    json_t *payload = json_object();
    json_set(payload, "status", json_copy(status));
    json_set(payload, "reason_code", json_copy(reason));
    json_set(payload, "thread_id", json_copy(thread));
    json_set(payload, "discussion_event_id", json_copy(deid));
    json_set(a, "payload", payload);
    json_set(a, "authority_gateway_id", json_copy(gid));
    json_set(a, "authority_epoch", json_copy(epoch));
    json_free(auth);
    json_t *res = thrs_call(s, "hosted_rooms.append_event", a);
    if (thrs_is_raise(res)) { *exc = res; return NULL; }
    json_free(res);
    return NULL;
}

/* PoP: thrs_publish_terminal @ tui_gateway/hosted_room_service.py:publish_terminal */
static json_t *thrs_publish_terminal(const thrs_seams_t *s, const json_t *binding, const json_t *task, json_t **exc) {
    const json_t *st = json_obj_get(task, "status");
    if (!st) { *exc = thrs_raise_spec("KeyError", "'status'"); return NULL; }
    bool deferred = thrs_is_str(st) && !strcmp(st->str_val, "deferred");
    bool forget = !deferred;
    const char *room_id = json_get_str(binding, "room_id", "");
    if (!forget) {
        json_t *member = thrs_task_member_id(task, exc);
        if (*exc) return NULL;
        json_t *a = json_object();
        json_set(a, "room_id", json_string(room_id));
        json_set(a, "member_id", member);
        json_t *ispeer = thrs_call(s, "self._member_is_peer", a);
        if (thrs_is_raise(ispeer)) { *exc = ispeer; return NULL; }
        forget = !thrs_jtruth(ispeer);
        json_free(ispeer);
    }
    if (forget) {
        const json_t *ident = json_obj_get(task, "identity");
        if (!ident) { *exc = thrs_raise_spec("KeyError", "'identity'"); return NULL; }
        json_t *tid = thrs_attr(ident, "task_id", exc);
        if (*exc) return NULL;
        const json_t *gn = json_obj_get(task, "execution_generation");
        if (!gn) { json_free(tid); *exc = thrs_raise_spec("KeyError", "'execution_generation'"); return NULL; }
        json_t *ie = NULL;
        long long gen = thrs_int(gn, &ie);
        if (ie) { json_free(tid); *exc = ie; return NULL; }
        json_t *a = json_object();
        json_set(a, "room_id", json_string(room_id));
        json_set(a, "task_id", tid);
        json_set(a, "execution_generation", json_number((double)gen));
        json_t *r = thrs_call(s, "forget_task_progress", a);
        if (thrs_is_raise(r)) { *exc = r; return NULL; }
        json_free(r);
    }
    json_t *a2 = json_object();
    json_set(a2, "binding", json_copy(binding));
    json_t *r2 = thrs_call(s, "self._prepare_tracked", a2);
    if (thrs_is_raise(r2)) { *exc = r2; return NULL; }
    json_free(r2);
    json_t *r3 = thrs_call0(s, "self.runtime.wakeup");
    if (thrs_is_raise(r3)) { *exc = r3; return NULL; }
    json_free(r3);
    return NULL;
}

/* PoP: thrs_peer_admission_receipt @ tui_gateway/hosted_room_service.py:peer_admission_receipt */
static json_t *thrs_peer_admission_receipt(const thrs_seams_t *s, const json_t *self,
                                           const json_t *room_id, const json_t *task, json_t **exc) {
    json_t *member = thrs_member_of(task);
    const char *member_s = member->str_val ? member->str_val : "";
    const json_t *routes = json_obj_get(self, "peer_routes");
    const json_t *route = NULL;
    if (thrs_is_arr(routes)) {
        for (size_t i = 0; i < routes->c.count; i++) {
            const json_t *r = routes->c.items[i];
            if (!strcmp(thrs_jstr(json_obj_get(r, "room_id"), ""), thrs_jstr(room_id, "")) &&
                !strcmp(thrs_jstr(json_obj_get(r, "member_id"), ""), member_s)) { route = r; break; }
        }
    }
    json_free(member);
    if (!route) return json_null();
    json_t *a = json_object();
    json_set(a, "room_id", json_copy(room_id));
    json_t *room = thrs_call(s, "self._room", a);
    if (thrs_is_raise(room)) { *exc = room; return NULL; }
    const json_t *ident = json_obj_get(task, "identity");
    if (!ident) { json_free(room); *exc = thrs_raise_spec("KeyError", "'identity'"); return NULL; }
    json_t *tid = thrs_attr(ident, "task_id", exc);
    if (*exc) { json_free(room); return NULL; }
    const json_t *gn = json_obj_get(task, "execution_generation");
    if (!gn) { json_free(room); json_free(tid); *exc = thrs_raise_spec("KeyError", "'execution_generation'"); return NULL; }
    json_t *ie = NULL;
    long long gen = thrs_int(gn, &ie);
    if (ie) { json_free(room); json_free(tid); *exc = ie; return NULL; }
    json_t *ie2 = NULL;
    const json_t *ep = json_obj_get(room, "authority_epoch");
    long long epoch = ep ? thrs_int(ep, &ie2) : 0;
    if (ie2) { json_free(room); json_free(tid); *exc = ie2; return NULL; }

    json_t *record = json_object();
    json_set(record, "room_id", json_copy(room_id));
    json_set(record, "home_install_id", json_copy(json_obj_get(route, "home_install_id")));
    json_set(record, "authority_gateway_id", thrs_str(json_obj_get(room, "authority_gateway_id")));
    json_set(record, "authority_epoch", json_number((double)epoch));
    json_set(record, "member_id", json_copy(json_obj_get(route, "member_id")));
    json_set(record, "target_install_id", json_copy(json_obj_get(route, "target_install_id")));
    json_set(record, "target_profile", json_copy(json_obj_get(route, "target_profile")));
    json_set(record, "task_id", tid);
    json_set(record, "execution_generation", json_number((double)gen));
    json_free(room);
    json_t *a2 = json_object();
    json_set(a2, "record", record);
    json_t *res = thrs_call(s, "hosted_rooms.remote_run_receipt", a2);
    if (thrs_is_raise(res)) { *exc = res; return NULL; }
    return res;
}
