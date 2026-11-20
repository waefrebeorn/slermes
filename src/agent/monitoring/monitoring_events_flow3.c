/* monitoring_events_flow3.c — FLOW port of the UNCLAIMED remainder of
 * agent/monitoring/events.py (typed gateway monitoring events).
 *
 * A CRLF-aware PoP grep of events.py function names over src/ found only the
 * stub port src/agent/monitoring/monitoring_events.c, which claims `to_dict`
 * (x3, hardcoded tag + passed-in fields_json) and a stale `_now_ns`. The
 * faithful FLOW remainder — dataclass construction (field defaults in wire
 * order), the time.time_ns default_factory, and {"event": EVENT, **asdict}
 * serialization — carries NO prior FLOW claim and is ported here.
 *
 * Representation contract (shared with tests/oracle_monitoring_events_flow3.py):
 *   - None -> JSON null; JSON objects keep INSERTION order (libjson json_set).
 *   - Each dataclass flow reads only the field keys present in `in`; absent
 *     REQUIRED fields raise TypeError; absent optional fields take their
 *     declared default; absent `ts_ns` fires the scripted `time.time_ns` seam
 *     (field(default_factory=time.time_ns)).
 *   - to_dict emits {"event": <EVENT>, **asdict(self)} — event tag FIRST,
 *     then every field in declaration (= wire) order.
 *   - the time.time_ns seam is scripted via in.io: {"$seq":[...]} per-call
 *     queue, {"__raise__":cls,"msg":m} raises (cls canonicalized through the
 *     FULL builtin exception hierarchy), absent -> null. Each call emits an
 *     ordered trace event {"ev","args","ret"}.
 *   - an uncaught exception -> {"__raise__":cls}; alias names
 *     IOError/EnvironmentError/WindowsError canonicalize to OSError (py3.14).
 *
 * FULL builtin exception hierarchy (verified against the target py3.14):
 * BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) and
 * BaseExceptionGroup stay OUTSIDE Exception; EOFError->Exception,
 * PythonFinalizationError->RuntimeError, _IncompleteInputError->SyntaxError,
 * ExceptionGroup routes through Exception for the `except Exception` contract.
 *
 * libjson-only. Self-contained: the harness #includes this file.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── context ────────────────────────────────────────────────────────────── */
typedef struct me3_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} me3_ctx;

static int me3_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static json_t *me3_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── FULL builtin exception hierarchy ───────────────────────────────────── */
/* Alias canonicalization: on py3.14 IOError/EnvironmentError/WindowsError ARE
 * OSError (type(exc).__name__ == "OSError"). */
static const char *me3_exc_canon(const char *c) {
    if (me3_eq(c, "IOError") || me3_eq(c, "EnvironmentError") ||
        me3_eq(c, "WindowsError")) return "OSError";
    return c;
}

/* Single-parent walk table. NULL parent terminates the walk (BaseException). */
static const char *me3_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"},
        {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        {"ExceptionGroup", "Exception"}, /* multi-base: route via Exception */
        {"Exception", "BaseException"},
        {"ArithmeticError", "Exception"},
        {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"},
        {"AttributeError", "Exception"},
        {"BufferError", "Exception"},
        {"EOFError", "Exception"},
        {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"},
        {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"MemoryError", "Exception"},
        {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"},
        {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"},
        {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"},
        {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"PythonFinalizationError", "RuntimeError"},
        {"StopAsyncIteration", "Exception"},
        {"StopIteration", "Exception"},
        {"SyntaxError", "Exception"},
        {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"_IncompleteInputError", "SyntaxError"},
        {"SystemError", "Exception"},
        {"TypeError", "Exception"},
        {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
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
    };
    const char *cc = me3_exc_canon(c);
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (me3_eq(T[i].c, cc)) return T[i].p;
    return "Exception";
}

static int me3_pending(const me3_ctx *c) { return c && c->exc != NULL; }
static void me3_clear(me3_ctx *c) {
    if (c && c->exc) { json_free(c->exc); c->exc = NULL; }
}
static void me3_raise(me3_ctx *c, const char *cls, const char *msg) {
    if (!c) return;
    me3_clear(c);
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(me3_exc_canon(cls ? cls : "Exception")));
    json_set(o, "msg", json_string(msg ? msg : ""));
    c->exc = o;
}
static const char *me3_exc_class(const me3_ctx *c) {
    return json_get_str(c->exc, "__raise__", "");
}

/* ── seam: time.time_ns ─────────────────────────────────────────────────── */
/* args ownership transferred; result NEW (borrowed from script -> copy). */
static json_t *me3_call(me3_ctx *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *script = c->io ? json_obj_get(c->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(c->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                me3_raise(c, "RuntimeError", "script exhausted");
                chosen = c->exc;
            } else {
                json_set(c->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args); /* takes ownership */
    args = NULL;

    int raised = 0;
    if (chosen && chosen->type == JSON_OBJECT) {
        json_t *r = json_obj_get(chosen, "__raise__");
        if (r && r->type == JSON_STRING) {
            /* Copy msg BEFORE me3_raise may free `chosen` (== c->exc). */
            char mbuf[512];
            const char *m = json_get_str(chosen, "msg", "");
            snprintf(mbuf, sizeof mbuf, "%s", m ? m : "");
            const char *cl = json_string_value(r);
            me3_raise(c, cl, mbuf);
            raised = 1;
        }
    }
    json_t *ret = NULL;
    if (raised) {
        json_set(ev, "ret", json_copy(c->exc));
    } else {
        ret = chosen ? json_copy(chosen) : json_null();
        json_set(ev, "ret", json_copy(ret));
    }
    json_append(c->trace, ev);
    return ret;
}

/* ── dataclass field specs (declaration = wire order) ───────────────────── */
enum { ME_REQ, ME_NULL, ME_STR, ME_INT, ME_BOOL, ME_TS };
typedef struct { const char *name; int kind; const char *sdef; } me3_field;

static const me3_field ME3_HEALTH[] = {
    {"name", ME_REQ, NULL},
    {"gateway_state", ME_NULL, NULL},
    {"old_state", ME_NULL, NULL},
    {"new_state", ME_NULL, NULL},
    {"exit_reason", ME_NULL, NULL},
    {"restart_requested", ME_NULL, NULL},
    {"active_agents", ME_INT, NULL},
    {"gateway_busy", ME_BOOL, NULL},
    {"gateway_drainable", ME_BOOL, NULL},
    {"platform_count", ME_INT, NULL},
    {"fatal_platform_count", ME_INT, NULL},
    {"profile", ME_NULL, NULL},
    {"install_id", ME_NULL, NULL},
    {"version", ME_NULL, NULL},
    {"supervision_mode", ME_NULL, NULL},
    {"pid", ME_NULL, NULL},
    {"ts_ns", ME_TS, NULL},
};
static const me3_field ME3_DIAG[] = {
    {"name", ME_REQ, NULL},
    {"subsystem", ME_REQ, NULL},
    {"error_class", ME_STR, "unknown"},
    {"error_code", ME_NULL, NULL},
    {"platform", ME_NULL, NULL},
    {"old_state", ME_NULL, NULL},
    {"new_state", ME_NULL, NULL},
    {"profile", ME_NULL, NULL},
    {"version", ME_NULL, NULL},
    {"severity", ME_STR, "warning"},
    {"ts_ns", ME_TS, NULL},
    {"source_logger", ME_NULL, NULL},
};
static const me3_field ME3_CRON[] = {
    {"status", ME_REQ, NULL},
    {"job_key", ME_REQ, NULL},
    {"source", ME_STR, "unknown"},
    {"duration_ms", ME_NULL, NULL},
    {"delivery_outcome", ME_NULL, NULL},
    {"error_class", ME_NULL, NULL},
    {"ts_ns", ME_TS, NULL},
};

/* Build the asdict(fields) ordered object, applying defaults. */
static json_t *me3_fields(me3_ctx *c, json_t *in, const me3_field *fs, size_t n) {
    json_t *obj = json_object();
    for (size_t i = 0; i < n; i++) {
        const me3_field *f = &fs[i];
        json_t *v = me3_get(in, f->name); /* present (even null) or NULL */
        if (v) {
            json_set(obj, f->name, json_copy(v));
            continue;
        }
        switch (f->kind) {
            case ME_REQ:
                me3_raise(c, "TypeError", "missing required field");
                json_free(obj);
                return NULL;
            case ME_TS: {
                json_t *ts = me3_call(c, "time.time_ns", json_object());
                if (me3_pending(c)) { json_free(ts); json_free(obj); return NULL; }
                json_set(obj, f->name, ts ? ts : json_null());
                break;
            }
            case ME_STR:
                json_set(obj, f->name, json_string(f->sdef ? f->sdef : ""));
                break;
            case ME_INT:
                json_set(obj, f->name, json_number(0));
                break;
            case ME_BOOL:
                json_set(obj, f->name, json_bool(false));
                break;
            case ME_NULL:
            default:
                json_set(obj, f->name, json_null());
                break;
        }
    }
    return obj;
}

/* PoP: me3_to_dict @ agent/monitoring/events.py:to_dict */
static json_t *me3_to_dict(me3_ctx *c, const char *event, json_t *fields) {
    (void)c;
    json_t *out = json_object();
    json_set(out, "event", json_string(event ? event : ""));
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *k = json_object_get_key_at(fields, i);
            json_t *v = json_object_get_at(fields, i);
            if (k) json_set(out, k, v ? json_copy(v) : json_null());
        }
    }
    return out;
}

static json_t *me3_event(me3_ctx *c, json_t *in, const char *event,
                         const me3_field *fs, size_t n) {
    json_t *fields = me3_fields(c, in, fs, n);
    if (me3_pending(c)) { json_free(fields); return NULL; }
    json_t *out = me3_to_dict(c, event, fields);
    json_free(fields);
    return out;
}

/* PoP: me3_gateway_health @ agent/monitoring/events.py:GatewayHealthEvent */
static json_t *me3_gateway_health(me3_ctx *c, json_t *in) {
    return me3_event(c, in, "gateway_health", ME3_HEALTH,
                     sizeof(ME3_HEALTH) / sizeof(ME3_HEALTH[0]));
}

/* PoP: me3_gateway_diagnostic @ agent/monitoring/events.py:GatewayDiagnosticEvent */
static json_t *me3_gateway_diagnostic(me3_ctx *c, json_t *in) {
    return me3_event(c, in, "gateway_diagnostic", ME3_DIAG,
                     sizeof(ME3_DIAG) / sizeof(ME3_DIAG[0]));
}

/* PoP: me3_cron_execution @ agent/monitoring/events.py:CronExecutionEvent */
static json_t *me3_cron_execution(me3_ctx *c, json_t *in) {
    return me3_event(c, in, "cron_execution", ME3_CRON,
                     sizeof(ME3_CRON) / sizeof(ME3_CRON[0]));
}

/* PoP: me3_to_dict_direct @ agent/monitoring/events.py:to_dict */
static json_t *me3_to_dict_direct(me3_ctx *c, json_t *in) {
    return me3_to_dict(c, json_get_str(in, "event", ""), me3_get(in, "fields"));
}

/* ── dispatch ───────────────────────────────────────────────────────────── */
static json_t *me3_dispatch(me3_ctx *ctx, const char *func, json_t *in) {
    if (me3_eq(func, "to_dict"))             return me3_to_dict_direct(ctx, in);
    if (me3_eq(func, "GatewayHealthEvent"))  return me3_gateway_health(ctx, in);
    if (me3_eq(func, "GatewayDiagnosticEvent")) return me3_gateway_diagnostic(ctx, in);
    if (me3_eq(func, "CronExecutionEvent"))  return me3_cron_execution(ctx, in);
    return json_null();
}
