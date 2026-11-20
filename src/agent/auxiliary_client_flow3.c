/* auxiliary_client_flow3.c — FLOW port of the remaining UNCLAIMED guard/accumulator
 * leaves of agent/auxiliary_client.py (beyond auxiliary_client_flow2).
 *
 * Ported targets (all strict-PoP unclaimed at authoring time):
 *   _CodexStreamGuard: effective_deadline, cancel_requested, adopt_stream,
 *   release_stream, close_attempt_stream, record_progress, timeout_message,
 *   _close_client_on_timeout, check_cancelled, _watchdog_fire, _arm_timer,
 *   start, on_event
 *   _ChatStreamAccumulator: _check_deadlines, _feed_reasoning_details,
 *   _feed_tool_calls
 *   leaves: __getattr__ (_AuxProbeClientStub), remember_temperature_rejection,
 *   _get_provider_chain
 *
 * All collaborator I/O (time/threading, hooks, imports, loggers, closures) rides
 * the injected universal seam ac3_call; the harness scripts results and records
 * the ordered trace the oracle diffs against LIVE Python under the same scripts.
 * Ported methods compose on the injected `self` carrier (shared state).
 *
 * Representation contract (shared with tests/oracle_auxiliary_client_flow3.py):
 *   - `self` is an attribute dict; event fields carry {"set": bool}; callable
 *     fields carry {"__callable__": marker[, "begin": bool]}; lock fields are
 *     inert markers; integral floats collapse to ints on both sides.
 *   - uncaught exception -> {"__raise__": cls}; a logger arg exception projects
 *     to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError); {"__raise__":cls,"msg":m} raises; any other value
 *     reused. None-valued seam arg keys are dropped on BOTH sides.
 *   - FULL builtin exception hierarchy: SystemExit/KeyboardInterrupt/GeneratorExit
 *     and the custom AuxiliaryExplicitCancellation derive from BaseException ONLY
 *     (never match `except Exception`); every other builtin derives (transitively)
 *     from Exception.
 *
 * libjson-only. acf_/ac2_ prefixes are taken by flow1/flow2; this port uses ac3_.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct ac3_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} ac3_ctx;

static bool ac3_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool ac3_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ac3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ac3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ac3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ac3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ac3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool ac3_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *ac3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double ac3_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ac3_get_bool_field(const json_t *obj, const char *key) {
    json_t *v = ac3_is_obj(obj) ? json_obj_get(obj, key) : NULL;
    return ac3_is_bool(v) ? v->bool_val : ac3_truthy(v);
}

/* borrowed object field or NULL */
static json_t *ac3_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* callable marker present+non-null on a self field? */
static bool ac3_callable(const json_t *v) {
    return ac3_is_obj(v) && ac3_truthy(json_obj_get(v, "__callable__"));
}

/* str(scalar) — caller frees */
static char *ac3_pystr(const json_t *v) {
    if (ac3_is_none(v)) return strdup("None");
    if (ac3_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (ac3_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (ac3_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* ── exception plumbing (FULL builtin hierarchy) ─────────────────────────── */

static bool ac3_exc_pending(const ac3_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void ac3_exc_clear(ac3_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void ac3_raise(ac3_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *ac3_exc_class(const ac3_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}

/* Direct parent of every builtin/custom exception this port can script. Classes
 * not listed have no parent and therefore never match an `except` gate. */
static const char *ac3_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        /* BaseException-only family: MUST NOT match `except Exception`. */
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"AuxiliaryExplicitCancellation", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        /* Exception itself */
        {"Exception", "BaseException"},
        {"ExceptionGroup", "Exception"},
        /* OSError family */
        {"OSError", "Exception"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        /* Core Exception subclasses */
        {"ValueError", "Exception"}, {"RuntimeError", "Exception"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"LookupError", "Exception"}, {"ArithmeticError", "Exception"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"MemoryError", "Exception"}, {"NameError", "Exception"},
        {"ReferenceError", "Exception"}, {"SyntaxError", "Exception"},
        {"SystemError", "Exception"}, {"Warning", "Exception"},
        /* Second-level specializations */
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"ModuleNotFoundError", "ImportError"}, {"UnboundLocalError", "NameError"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeError", "ValueError"},
        {"JSONDecodeError", "ValueError"},
        {"FloatingPointError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (ac3_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool ac3_exc_matches(const ac3_ctx *ctx, const char *const *classes, size_t n) {
    if (!ac3_exc_pending(ctx)) return false;
    const char *cls = ac3_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (ac3_str_eq(cls, classes[i])) return true;
        const char *p = ac3_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool ac3_exc_is_exception(const ac3_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return ac3_exc_matches(ctx, E, 1);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *ac3_script_result(ac3_ctx *ctx, const json_t *script) {
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *ac3_call(ac3_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = ac3_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* arg builders: None-valued keys dropped on BOTH sides ("" kept). */
static void ac3_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void ac3_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void ac3_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}

/* logger seam: {"msg","args"[,"exc_info"]} -> logger.<level>. */
static json_t *ac3_log(ac3_ctx *ctx, const char *level, const char *msg,
                       json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = ac3_call(ctx, name, a);
    json_free(r);
    return NULL;
}
/* project pending exception as a logger arg node */
static json_t *ac3_exc_arg(const ac3_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(ac3_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}

/* helper: seam with one positional arg under `key`. */
static json_t *ac3_call1(ac3_ctx *ctx, const char *name, const char *key, const json_t *val) {
    json_t *a = json_object();
    ac3_arg(a, key, val);
    return ac3_call(ctx, name, a);
}

/* ── _CodexStreamGuard methods ───────────────────────────────────────────── */

/* PoP: ac3_effective_deadline @ agent/auxiliary_client.py:effective_deadline */
static json_t *ac3_effective_deadline(ac3_ctx *ctx, json_t *self) {
    (void)ctx;
    double h = ac3_num(ac3_get(self, "hard_deadline"));
    double p = ac3_num(ac3_get(self, "_progress_deadline"));
    return json_number(p < h ? p : h);
}

/* PoP: ac3_cancel_requested @ agent/auxiliary_client.py:cancel_requested */
static json_t *ac3_cancel_requested(ac3_ctx *ctx, json_t *self) {
    json_t *check = ac3_get(self, "_protected_cancel_check");
    if (!ac3_callable(check)) return json_bool(false);
    return ac3_call1(ctx, "captured_aux_cancel_requested", "check",
                     ac3_get(check, "__callable__"));
}

/* PoP: ac3_adopt_stream @ agent/auxiliary_client.py:adopt_stream */
static json_t *ac3_adopt_stream(ac3_ctx *ctx, json_t *self, const json_t *stream) {
    (void)ctx;
    json_set(self, "_attempt_stream", stream ? json_copy(stream) : json_null());
    return json_null();
}

/* PoP: ac3_release_stream @ agent/auxiliary_client.py:release_stream */
static json_t *ac3_release_stream(ac3_ctx *ctx, json_t *self, const json_t *stream) {
    json_t *a = json_object();
    ac3_arg(a, "stream", stream);
    json_t *r = ac3_call(ctx, "close_quietly", a);
    json_free(r);
    if (ac3_exc_pending(ctx)) return json_null();
    json_set(self, "_attempt_stream", json_null());
    return json_null();
}

/* PoP: ac3_close_attempt_stream @ agent/auxiliary_client.py:close_attempt_stream */
static json_t *ac3_close_attempt_stream(ac3_ctx *ctx, json_t *self, const char *failure_note) {
    json_t *stream = ac3_get(self, "_attempt_stream");
    if (ac3_is_none(stream)) return json_null();
    json_t *tid = ac3_call(ctx, "threading.get_ident", json_object());
    if (ac3_exc_pending(ctx)) { json_free(tid); return json_null(); }
    double owner = ac3_num(ac3_get(self, "_owner_tid"));
    if (ac3_num(tid) != owner) {
        json_free(tid);
        json_t *sock = ac3_call1(ctx, "attempt_stream_socket", "stream", stream);
        if (ac3_exc_pending(ctx)) { json_free(sock); return json_null(); }
        if (!ac3_is_none(sock)) {
            json_t *a = json_object();
            ac3_arg(a, "socket", sock);
            json_t *r = ac3_call(ctx, "shutdown_socket", a);
            json_free(r); json_free(sock);
            return json_null();
        }
        json_free(sock);
    } else {
        json_free(tid);
    }
    json_t *a = json_object();
    ac3_arg(a, "stream", stream);
    ac3_arg_str(a, "note", failure_note);
    json_t *r = ac3_call(ctx, "close_quietly", a);
    json_free(r);
    return json_null();
}

/* PoP: ac3_record_progress @ agent/auxiliary_client.py:record_progress */
static json_t *ac3_record_progress(ac3_ctx *ctx, json_t *self) {
    json_t *now = ac3_call(ctx, "time.monotonic", json_object());
    if (ac3_exc_pending(ctx)) { json_free(now); return json_null(); }
    double v = ac3_num(now) + ac3_num(ac3_get(self, "no_progress_timeout"));
    json_free(now);
    json_set(self, "_progress_deadline", json_number(v));
    return json_null();
}

/* .1f helper into a caller buffer */
static void ac3_f1(char *buf, size_t n, double v) { snprintf(buf, n, "%.1f", v); }

/* PoP: ac3_timeout_message @ agent/auxiliary_client.py:timeout_message */
static json_t *ac3_timeout_message(ac3_ctx *ctx, json_t *self) {
    double start = ac3_num(ac3_get(self, "_start"));
    double hard = ac3_num(ac3_get(self, "hard_deadline"));
    double npt = ac3_num(ac3_get(self, "no_progress_timeout"));
    json_t *t1 = ac3_call(ctx, "time.monotonic", json_object());
    if (ac3_exc_pending(ctx)) { json_free(t1); return json_null(); }
    double elapsed = ac3_num(t1) - start;
    json_free(t1);
    json_t *t2 = ac3_call(ctx, "time.monotonic", json_object());
    if (ac3_exc_pending(ctx)) { json_free(t2); return json_null(); }
    double now2 = ac3_num(t2);
    json_free(t2);
    char buf[512];
    char e1[64];
    if (now2 >= hard) {
        ac3_f1(e1, sizeof e1, hard - start);
        snprintf(buf, sizeof buf,
                 "Codex auxiliary Responses stream exceeded %ss hard ceiling", e1);
        return json_string(buf);
    }
    ac3_f1(e1, sizeof e1, elapsed);
    char e2[64]; ac3_f1(e2, sizeof e2, npt);
    json_t *ev = ac3_get(self, "saw_content");
    bool saw = ac3_get_bool_field(ev, "set");
    if (!saw) {
        snprintf(buf, sizeof buf,
                 "Codex auxiliary Responses stream produced no output "
                 "within %ss (no-progress timeout, %ss elapsed)", e2, e1);
        return json_string(buf);
    }
    snprintf(buf, sizeof buf,
             "Codex auxiliary Responses stream stalled: no new output "
             "for %ss (%ss elapsed)", e2, e1);
    return json_string(buf);
}

/* forward decls for composed guard methods */
static json_t *ac3_check_cancelled(ac3_ctx *ctx, json_t *self);
static json_t *ac3_close_attempt_stream(ac3_ctx *ctx, json_t *self, const char *failure_note);
static json_t *ac3_close_client_on_timeout(ac3_ctx *ctx, json_t *self);
static json_t *ac3_arm_timer(ac3_ctx *ctx, json_t *self, double delay);
static json_t *ac3_timeout_message(ac3_ctx *ctx, json_t *self);
static json_t *ac3_effective_deadline(ac3_ctx *ctx, json_t *self);
static json_t *ac3_record_progress(ac3_ctx *ctx, json_t *self);

/* PoP: ac3_close_client_on_timeout @ agent/auxiliary_client.py:_close_client_on_timeout */
static json_t *ac3_close_client_on_timeout(ac3_ctx *ctx, json_t *self) {
    json_t *check = ac3_get(self, "_protected_cancel_check");
    bool timeout_won;
    bool has_begin = ac3_callable(check) &&
                     ac3_get_bool_field(check, "begin");
    if (has_begin) {
        json_t *r = ac3_call(ctx, "begin_timeout_cleanup", json_object());
        if (ac3_exc_pending(ctx)) { json_free(r); return json_null(); }
        timeout_won = ac3_truthy(r);
        json_free(r);
    } else {
        json_t *r = ac3_cancel_requested(ctx, self);
        if (ac3_exc_pending(ctx)) { json_free(r); return json_null(); }
        timeout_won = !ac3_truthy(r);
        json_free(r);
    }
    json_set(ac3_get(self, "timed_out"), "set", json_bool(true));
    if (!timeout_won) {
        json_t *r = ac3_close_attempt_stream(ctx, self,
            "cancelled attempt stream close during timeout failed");
        json_free(r);
        return json_null();
    }
    json_set(ac3_get(self, "timeout_release_pending"), "set", json_bool(true));
    json_t *tid = ac3_call(ctx, "threading.get_ident", json_object());
    if (ac3_exc_pending(ctx)) { json_free(tid); return json_null(); }
    bool owner = (ac3_num(tid) == ac3_num(ac3_get(self, "_owner_tid")));
    json_free(tid);
    json_t *client = ac3_get(self, "_client");
    if (owner) {
        json_t *a = json_object();
        ac3_arg(a, "stream", client);
        ac3_arg_str(a, "note", "client close during timeout failed");
        json_t *r = ac3_call(ctx, "close_quietly", a);
        json_free(r);
        if (ac3_exc_pending(ctx)) return json_null();
    } else {
        json_t *a = json_object();
        ac3_arg(a, "client", client);
        json_t *count = ac3_call(ctx, "force_close_tcp_sockets", a);
        bool count_ok = true;
        if (ac3_exc_pending(ctx)) {
            if (ac3_exc_is_exception(ctx)) {
                ac3_exc_clear(ctx);
                ac3_log(ctx, "debug",
                        "Codex auxiliary: client abort during timeout failed",
                        NULL, true);
                count_ok = false; /* logger.info is inside the same try, skipped */
            } else {
                json_free(count);
                return json_null();
            }
        }
        if (count_ok) {
            json_t *la = json_object();
            json_set(la, "msg", json_string(
                "Codex auxiliary client aborted (timeout, tcp_force_closed=%d, "
                "deferred_close=stranger_thread)"));
            json_t *laa = json_array();
            json_append(laa, count ? count : json_null());
            json_set(la, "args", laa); /* ownership of count transferred */
            json_t *lr = ac3_call(ctx, "logger.info", la);
            json_free(lr);
        } else {
            json_free(count);
        }
        json_t *r2 = ac3_close_attempt_stream(ctx, self,
            "attempt stream close during stranger-thread timeout failed");
        json_free(r2);
        if (ac3_exc_pending(ctx)) return json_null();
    }
    /* cache eviction, guarded */
    {
        json_t *a = json_object();
        ac3_arg(a, "client", client);
        json_t *r = ac3_call(ctx, "evict_cached_client_instance", a);
        if (ac3_exc_pending(ctx)) {
            if (ac3_exc_is_exception(ctx)) {
                ac3_exc_clear(ctx);
                ac3_log(ctx, "debug",
                        "Codex auxiliary: cache eviction on timeout failed",
                        NULL, true);
            }
            json_free(r);
            return json_null();
        }
        json_free(r);
    }
    return json_null();
}

/* PoP: ac3_check_cancelled @ agent/auxiliary_client.py:check_cancelled */
static json_t *ac3_check_cancelled(ac3_ctx *ctx, json_t *self) {
    json_t *total = ac3_get(self, "total_timeout");
    if (!ac3_is_none(total)) {
        json_t *now = ac3_call(ctx, "time.monotonic", json_object());
        if (ac3_exc_pending(ctx)) { json_free(now); return json_null(); }
        double nowv = ac3_num(now);
        json_free(now);
        json_t *dl = ac3_effective_deadline(ctx, self);
        double dlv = ac3_num(dl);
        json_free(dl);
        if (nowv >= dlv) {
            json_t *ev = ac3_get(self, "timed_out");
            if (!ac3_get_bool_field(ev, "set")) {
                json_t *r = ac3_close_client_on_timeout(ctx, self);
                json_free(r);
                if (ac3_exc_pending(ctx)) return json_null();
            }
            json_t *m = ac3_timeout_message(ctx, self);
            if (ac3_exc_pending(ctx)) { json_free(m); return json_null(); }
            char *s = ac3_pystr(m);
            ac3_raise(ctx, "TimeoutError", s ? s : "");
            free(s); json_free(m);
            return json_null();
        }
    }
    /* best-effort interrupt probes inside try/except */
    json_t *r = ac3_call(ctx, "aux_interrupt_cancel_requested", json_object());
    if (ac3_exc_pending(ctx)) {
        if (ac3_exc_matches(ctx, (const char *[]){"InterruptedError"}, 1)) {
            return json_null(); /* `except InterruptedError: raise` -> propagate (pending kept) */
        }
        if (ac3_exc_is_exception(ctx)) ac3_exc_clear(ctx); /* except Exception: pass */
        json_free(r);
        return json_null();
    }
    if (ac3_truthy(r)) {
        json_free(r);
        ac3_raise(ctx, "AuxiliaryExplicitCancellation", ""); /* BaseException: not caught */
        return json_null();
    }
    json_free(r);
    json_t *i = ac3_call(ctx, "is_interrupted", json_object());
    if (ac3_exc_pending(ctx)) {
        if (ac3_exc_matches(ctx, (const char *[]){"InterruptedError"}, 1)) return json_null();
        if (ac3_exc_is_exception(ctx)) ac3_exc_clear(ctx);
        json_free(i); return json_null();
    }
    bool interrupted = ac3_truthy(i);
    json_free(i);
    if (interrupted) {
        json_t *p = ac3_call(ctx, "aux_interrupt_protected", json_object());
        if (ac3_exc_pending(ctx)) {
            if (ac3_exc_matches(ctx, (const char *[]){"InterruptedError"}, 1)) return json_null();
            if (ac3_exc_is_exception(ctx)) ac3_exc_clear(ctx);
            json_free(p); return json_null();
        }
        bool prot = ac3_truthy(p);
        json_free(p);
        if (!prot) {
            ac3_raise(ctx, "InterruptedError",
                      "Codex auxiliary Responses stream interrupted");
            return json_null();
        }
    }
    return json_null();
}

/* PoP: ac3_watchdog_fire @ agent/auxiliary_client.py:_watchdog_fire */
static json_t *ac3_watchdog_fire(ac3_ctx *ctx, json_t *self) {
    json_t *dl = ac3_effective_deadline(ctx, self);
    double remaining = ac3_num(dl) - 0.0;
    json_free(dl);
    json_t *t = ac3_call(ctx, "time.monotonic", json_object());
    if (ac3_exc_pending(ctx)) { json_free(t); return json_null(); }
    remaining -= ac3_num(t);
    json_free(t);
    if (remaining > 0) {
        bool to = ac3_get_bool_field(ac3_get(self, "timed_out"), "set");
        bool sf = false;
        if (!to) sf = ac3_get_bool_field(ac3_get(self, "stream_finished"), "set");
        if (!(to || sf)) {
            json_t *r = ac3_arm_timer(ctx, self, remaining);
            json_free(r);
        }
        return json_null();
    }
    json_t *r = ac3_close_client_on_timeout(ctx, self);
    json_free(r);
    return json_null();
}

/* PoP: ac3_arm_timer @ agent/auxiliary_client.py:_arm_timer */
static json_t *ac3_arm_timer(ac3_ctx *ctx, json_t *self, double delay) {
    json_t *a = json_object();
    json_set(a, "delay", json_number(delay));
    json_t *t = ac3_call(ctx, "threading.Timer", a);
    if (ac3_exc_pending(ctx)) { json_free(t); return json_null(); }
    json_t *r = ac3_call(ctx, "timer.start", json_object());
    if (ac3_exc_pending(ctx)) { json_free(r); json_free(t); return json_null(); }
    json_free(r);
    json_set(self, "_timer", t ? t : json_null()); /* json_set takes ownership */
    return json_null();
}

/* PoP: ac3_start @ agent/auxiliary_client.py:start */
static json_t *ac3_start(ac3_ctx *ctx, json_t *self) {
    json_t *total = ac3_get(self, "total_timeout");
    if (ac3_truthy(total)) {
        json_t *dl = ac3_effective_deadline(ctx, self);
        double d = ac3_num(dl); json_free(dl);
        json_t *t = ac3_call(ctx, "time.monotonic", json_object());
        if (ac3_exc_pending(ctx)) { json_free(t); return json_null(); }
        d -= ac3_num(t); json_free(t);
        if (d < 0.0) d = 0.0;
        json_t *r = ac3_arm_timer(ctx, self, d);
        json_free(r);
        if (ac3_exc_pending(ctx)) return json_null();
    }
    json_t *r = ac3_check_cancelled(ctx, self);
    json_free(r);
    return json_null();
}

/* PoP: ac3_on_event @ agent/auxiliary_client.py:on_event */
static json_t *ac3_on_event(ac3_ctx *ctx, json_t *self, const json_t *event) {
    json_t *has = ac3_call1(ctx, "codex_event_has_content", "event", event);
    if (ac3_exc_pending(ctx)) { json_free(has); return json_null(); }
    bool content = ac3_truthy(has);
    json_free(has);
    if (content) {
        json_t *r = ac3_record_progress(ctx, self);
        json_free(r);
        if (ac3_exc_pending(ctx)) return json_null();
        json_set(ac3_get(self, "saw_content"), "set", json_bool(true));
        json_t *p = ac3_call(ctx, "notify_aux_provider_response", json_object());
        json_free(p);
        if (ac3_exc_pending(ctx)) return json_null();
    } else {
        json_t *p = ac3_call(ctx, "notify_aux_timing_response", json_object());
        json_free(p);
        if (ac3_exc_pending(ctx)) return json_null();
    }
    json_t *r = ac3_check_cancelled(ctx, self);
    json_free(r);
    return json_null();
}

/* ── _ChatStreamAccumulator methods ──────────────────────────────────────── */

/* PoP: ac3_check_deadlines @ agent/auxiliary_client.py:_check_deadlines */
static json_t *ac3_check_deadlines(ac3_ctx *ctx, json_t *self) {
    json_t *r = ac3_call(ctx, "aux_interrupt_cancel_requested", json_object());
    if (ac3_exc_pending(ctx)) { json_free(r); return json_null(); }
    if (ac3_truthy(r)) {
        json_free(r);
        ac3_raise(ctx, "AuxiliaryExplicitCancellation", "");
        return json_null();
    }
    json_free(r);
    json_t *t1 = ac3_call(ctx, "time.monotonic", json_object());
    if (ac3_exc_pending(ctx)) { json_free(t1); return json_null(); }
    double now = ac3_num(t1); json_free(t1);
    json_t *ceiling = ac3_get(self, "_total_ceiling");
    if (!ac3_is_none(ceiling)) {
        double started = ac3_num(ac3_get(self, "_started"));
        json_t *pr = ac3_get(self, "_prompt_reading");
        double secs = ac3_num(ac3_get(pr, "seconds"));
        double c = ac3_num(ceiling);
        if ((now - started - secs) >= c) {
            char buf[256]; char e[32];
            snprintf(e, sizeof e, "%.0f", c);
            snprintf(buf, sizeof buf,
                     "Auxiliary streamed call timed out after %ss total ceiling "
                     "(stream still open but over budget)", e);
            ac3_raise(ctx, "TimeoutError", buf);
            return json_null();
        }
    }
    json_t *hd = ac3_get(self, "_host_deadline");
    double hdval;
    if (ac3_callable(hd)) {
        json_t *h = ac3_call(ctx, "host_deadline", json_object());
        if (ac3_exc_pending(ctx)) { json_free(h); return json_null(); }
        hdval = ac3_num(h); json_free(h);
    } else {
        hdval = ac3_is_none(hd) ? NAN : ac3_num(hd);
    }
    if (!isnan(hdval) && now >= hdval) {
        json_t *t2 = ac3_call(ctx, "time.monotonic", json_object());
        if (ac3_exc_pending(ctx)) { json_free(t2); return json_null(); }
        double elapsed = ac3_num(t2) - ac3_num(ac3_get(self, "_started"));
        json_free(t2);
        char buf[400]; char e[32];
        snprintf(e, sizeof e, "%.0f", elapsed);
        snprintf(buf, sizeof buf,
                 "Auxiliary streamed call timed out at the host compression "
                 "deadline after %ss (the caller already stopped waiting; "
                 "streaming on would only pin its session lease)", e);
        ac3_raise(ctx, "TimeoutError", buf);
        return json_null();
    }
    return json_null();
}

/* PoP: ac3_feed_reasoning_details @ agent/auxiliary_client.py:_feed_reasoning_details */
static json_t *ac3_feed_reasoning_details(ac3_ctx *ctx, json_t *self, const json_t *delta) {
    (void)ctx;
    json_t *rd = ac3_get(delta, "reasoning_details");
    if (ac3_is_none(rd)) {
        json_t *me = ac3_get(delta, "model_extra");
        if (ac3_is_obj(me)) rd = ac3_get(me, "reasoning_details");
    }
    if (!ac3_is_arr(rd)) return json_bool(false);
    bool progress = false;
    static const char *FIELDS[] = {"summary", "thinking", "content", "text"};
    json_t *acc = ac3_get(self, "reasoning_details");
    if (!ac3_is_arr(acc)) { acc = json_array(); json_set(self, "reasoning_details", acc); }
    for (size_t i = 0; i < rd->c.count; i++) {
        json_t *d = rd->c.items[i];
        json_append(acc, json_copy(d));
        if (ac3_is_obj(d)) {
            for (size_t f = 0; f < sizeof(FIELDS)/sizeof(FIELDS[0]); f++) {
                json_t *v = json_obj_get(d, FIELDS[f]);
                if (ac3_is_str(v) && ac3_truthy(v)) { progress = true; break; }
            }
        }
    }
    return json_bool(progress);
}

/* PoP: ac3_feed_tool_calls @ agent/auxiliary_client.py:_feed_tool_calls */
static json_t *ac3_feed_tool_calls(ac3_ctx *ctx, json_t *self, const json_t *delta) {
    (void)ctx;
    bool progress = false;
    json_t *tcs = ac3_get(delta, "tool_calls");
    if (!ac3_is_arr(tcs)) return json_bool(false);
    json_t *acc = ac3_get(self, "tool_calls_acc");
    if (!ac3_is_obj(acc)) { acc = json_object(); json_set(self, "tool_calls_acc", acc); }
    for (size_t i = 0; i < tcs->c.count; i++) {
        json_t *tc = tcs->c.items[i];
        if (!ac3_is_obj(tc)) continue;
        double idx = ac3_num(ac3_get(tc, "index"));
        char key[32]; snprintf(key, sizeof key, "%.0f", idx);
        json_t *slot = json_obj_get(acc, key);
        if (!slot) {
            slot = json_object();
            json_set(slot, "id", json_string(""));
            json_set(slot, "name", json_string(""));
            json_set(slot, "arguments", json_array());
            json_set(acc, key, slot);
        }
        json_t *id = ac3_get(tc, "id");
        if (ac3_truthy(id)) { json_set(slot, "id", json_copy(id)); progress = true; }
        json_t *fn = ac3_get(tc, "function");
        if (ac3_is_obj(fn)) {
            json_t *nm = ac3_get(fn, "name");
            if (ac3_truthy(nm)) { json_set(slot, "name", json_copy(nm)); progress = true; }
            json_t *ar = ac3_get(fn, "arguments");
            if (ac3_truthy(ar)) {
                json_append(json_obj_get(slot, "arguments"), json_copy(ar));
                progress = true;
            }
        }
    }
    return json_bool(progress);
}

/* ── leaves ──────────────────────────────────────────────────────────────── */

/* PoP: ac3_getattr @ agent/auxiliary_client.py:__getattr__ */
static json_t *ac3_getattr(ac3_ctx *ctx, json_t *self, const char *name) {
    (void)self;
    char buf[512];
    snprintf(buf, sizeof buf,
             "_AuxProbeClientStub used as a real client (attribute '%s'); "
             "aux_probe_mode is for availability checks only", name ? name : "");
    ac3_raise(ctx, "RuntimeError", buf);
    return json_null();
}

/* PoP: ac3_remember_temperature_rejection @ agent/auxiliary_client.py:remember_temperature_rejection */
static json_t *ac3_remember_temperature_rejection(ac3_ctx *ctx, json_t *self,
        const char *provider, const char *base_url, const json_t *model) {
    (void)self;
    json_t *a1 = json_object();
    ac3_arg_str(a1, "provider", provider);
    ac3_arg_str(a1, "base_url", base_url);
    json_t *rk = ac3_call(ctx, "route_key", a1);
    if (ac3_exc_pending(ctx)) { json_free(rk); return json_null(); }
    json_t *a2 = json_object();
    ac3_arg(a2, "model", model);
    json_t *bm = ac3_call(ctx, "bare_model", a2);
    if (ac3_exc_pending(ctx)) { json_free(rk); json_free(bm); return json_null(); }
    /* add (rk, bm) to the routes set carried on `self` */
    json_t *routes = ac3_get(self, "routes");
    if (!ac3_is_arr(routes)) { routes = json_array(); json_set(self, "routes", routes); }
    json_t *pair = json_array();
    json_append(pair, rk ? rk : json_null());   /* ownership transferred */
    json_append(pair, bm ? bm : json_null());   /* ownership transferred */
    json_append(routes, pair);                  /* ownership transferred */
    return json_null();
}

/* PoP: ac3_get_provider_chain @ agent/auxiliary_client.py:_get_provider_chain */
static json_t *ac3_get_provider_chain(ac3_ctx *ctx) {
    (void)ctx;
    json_t *r = json_array();
    static const char *LAB[] = {"openrouter", "nous", "local/custom", "api-key"};
    static const char *FN[] = {"_try_openrouter", "_try_nous",
                               "_try_custom_endpoint", "_resolve_api_key_provider"};
    for (int i = 0; i < 4; i++) {
        json_t *p = json_array();
        json_append(p, json_string(LAB[i]));
        json_append(p, json_string(FN[i]));
        json_append(r, p);
    }
    return r;
}
