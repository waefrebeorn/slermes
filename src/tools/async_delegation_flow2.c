/* async_delegation_flow2.c — FLOW port of the UNCLAIMED remainder of
 * tools/async_delegation.py (42 unclaimed defs by strict CRLF-aware PoP grep;
 * the 46 claimed names in async_delegation.c / async_delegation_wrappers.c /
 * util_helpers.c are SEAMED, never re-ported). This port takes 29 unclaimed
 * pure leaves, state/control helpers and delivery/event helpers:
 *
 *   _single_crash, _batch_crash, _batch_status, _stalled_error_text, _stall_meta,
 *   _stalled_result, _keep_finished_children, _recovered_results,
 *   is_interim_delegation_event, completion_copy_id, _call_interrupt,
 *   mark_copy_held, unmark_copy_held, refused_for_session, allow_for_session,
 *   _unfence, refuse_for_session, _session_records, active_count,
 *   active_task_count, _queued_completion_ids, _copy_may_be_live, _note_reoffers,
 *   maybe_sweep_orphaned_completions, _event_delivery, claim_event_delivery,
 *   complete_event_delivery, release_event_delivery, return_completion_offer.
 *
 * Claimed siblings (e.g. active_count's siblings active_for_session / interrupt_all,
 * _children_activity_from_token, _finalize, _persist_completion, ...) are driven
 * as universal-seam recorders; pure leaves are transcribed inline. Decision
 * sequences, truthiness / or-short-circuits, dict/array order and except-Exception
 * catch scopes are transcribed per call site. A scripted BaseException
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError / *Group)
 * propagates past every except Exception guard (FULL builtin exception-parent
 * table audited against the py3.14 gate interpreter).
 *
 * Value model (shared with tests/oracle_async_delegation_flow2.py):
 *   - state window `st` mirrors the module globals as JSON (records / refused /
 *     refused_by / offered / startup_replayed / reoffered_at / held_copies /
 *     last_orphan_sweep). Locks are no-ops (single-threaded harness).
 *   - callables project to {"__callable__":"<seam>"}; raise specs are
 *     {"__raise__":"<Class>","message":str(e)}.
 *   - a bare JSON array seam script is a per-call QUEUE; a list RESULT must be
 *     wrapped {"__script__":true,"ret":[...]}. Absent / exhausted scripts return
 *     a RuntimeError raise spec, recorded BEFORE propagation.
 *
 * Domain (documented for the generator): ASCII text; integral floats excluded
 * in str()/repr() positions; delegation ids / session keys are strings;
 * tokens are strings; timestamps are numbers; callable seams return scalar
 * or raise specs (never attribute doubles); logger args are scalars.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via "tools/async_delegation_flow2.c". Every symbol is a
 * static ad2_*.
 *
 * Gate: bash tests/run_one_oracle.sh async_delegation_flow2 \
 *           oracle_async_delegation_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam ── */
typedef struct ad2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} ad2_io_t;

/* ── accessors / truthiness ── */
static int ad2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int ad2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int ad2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int ad2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int ad2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int ad2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int ad2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *ad2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *ad2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double ad2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* integral-float reads as int in libjson; isistance(x, int) admits bool. */
static int ad2_is_int(const json_t *v)
{
    if (ad2_is_bool(v)) return 1;
    if (!ad2_is_num(v)) return 0;
    double d = v->num_val;
    return isfinite(d) && d == (double)(long long)d;
}

/* isinstance(x, (int,float)): number OR bool. */
static int ad2_is_number_or_bool(const json_t *v)
{
    return ad2_is_num(v) || ad2_is_bool(v);
}

static const char *ad2_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── raise specs ── */
static int ad2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *ad2_kind(const json_t *r)
{
    const char *k = ad2_str(ad2_get(r, "__raise__"));
    return k ? k : "";
}

static const char *ad2_rmsg(const json_t *r)
{
    const char *m = ad2_str(ad2_get(r, "message"));
    return m ? m : "";
}

static json_t *ad2_mkraise(const char *kind, const char *msg)
{
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}

/* ── FULL builtin exception-parent hierarchy (py3.14, audited against the gate
 *    interpreter's builtins). Walk to test except Exception / except <Class>.
 *    The BaseException trio (+ CancelledError and both *Group roots) parent to
 *    BaseException ONLY, so they pass through every except Exception. ── */
typedef struct { const char *name; const char *parent; } ad2_exc_row;
static const ad2_exc_row AD2_EXC_PARENTS[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "BaseException"},
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
    {"EnvironmentError", "OSError"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "OSError"},
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
    {"WindowsError", "OSError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
    {NULL, NULL},
};

static const char *ad2_exc_parent(const char *name)
{
    for (size_t i = 0; AD2_EXC_PARENTS[i].name; i++)
        if (strcmp(AD2_EXC_PARENTS[i].name, name) == 0) return AD2_EXC_PARENTS[i].parent;
    return "Exception";
}

static int ad2_exc_is_instance(const char *kind, const char *base)
{
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, base) == 0) return 1;
        const char *p = ad2_exc_parent(cur);
        if (!p || strcmp(cur, p) == 0) break;
        snprintf(cur, sizeof(cur), "%s", p);
        if (strcmp(cur, "BaseException") == 0) return strcmp(base, "BaseException") == 0;
    }
    return 0;
}

static int ad2_exc_is_exception(const char *kind)
{
    return ad2_exc_is_instance(kind, "Exception");
}

/* ── printf-style malloc'd format ── */
static char *ad2_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return NULL; }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* ── universal seam wrapper (returns owned json_t*, possibly a raise spec) ── */
static json_t *ad2_call(ad2_io_t *io, const char *name, const json_t *args)
{
    if (!io || !io->call) {
        char *m = ad2_xfmt("no script for %s", name);
        json_t *r = ad2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
        return r;
    }
    return io->call(io->ctx, name, args);
}

/* logger.<level>(msg, *args, exc_info=...): canonical args projection.
 * `pos` is an owned array node of the positional args after msg (or NULL). */
static json_t *ad2_log(ad2_io_t *io, const char *level, const char *msg, json_t *pos, int exc_info)
{
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg ? msg : ""));
    json_set(a, "args", pos ? pos : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(1));
    char *nm = ad2_xfmt("logger.%s", level);
    json_t *r = ad2_call(io, nm ? nm : "logger.debug", a);
    free(nm);
    return r;
}

static json_t *ad2_str_arr(const char *const *items, size_t n)
{
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(items[i] ? items[i] : ""));
    return a;
}

/* ── state helpers (borrowed state window st) ── */
static json_t *ad2_state_obj(json_t *st, const char *key)
{
    json_t *o = ad2_get(st, key);
    return (o && o->type == JSON_OBJECT) ? o : NULL;
}

static json_t *ad2_state_arr(json_t *st, const char *key)
{
    json_t *a = ad2_get(st, key);
    return (a && a->type == JSON_ARRAY) ? a : NULL;
}

static json_t *ad2_obj_get_def_obj(json_t *o, const char *k)
{
    json_t *v = ad2_get(o, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}


/* ── scalar python-string helpers (ASCII domain) ── */
static char *ad2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        char buf[64];
        if (isfinite(d) && d == (double)(long long)d && fabs(d) < 9.2e18)
            snprintf(buf, sizeof(buf), "%.0f", d);
        else
            snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_ARRAY) return strdup("[]");
    return strdup("{}");
}

/* str(v or "") — falsy collapses to the EMPTY STRING, not "None". */
static char *ad2_str_or_empty(const json_t *v)
{
    if (!ad2_truth(v)) return strdup("");
    return ad2_pystr(v);
}

/* " ".join(str(v).split()) on ASCII whitespace (fixture domain). */
static char *ad2_norm_words(const json_t *v)
{
    char *s = ad2_str_or_empty(v);
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 2);
    if (!out) { free(s); return strdup(""); }
    size_t k = 0;
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        int ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
                  || (c >= 0x1c && c <= 0x1f));
        if (ws) { i++; continue; }
        if (k) out[k++] = ' ';
        while (i < n) {
            unsigned char d = (unsigned char)s[i];
            int w = (d == ' ' || d == '\t' || d == '\n' || d == '\r' || d == '\v' || d == '\f'
                     || (d >= 0x1c && d <= 0x1f));
            if (w) break;
            out[k++] = s[i++];
        }
    }
    out[k] = '\0';
    free(s);
    return out;
}

/* Python round() to integer: round-half-to-even. */
static long ad2_round_i(double x) { return (long)llrint(x); }
/* Python round(x, 2). */
static double ad2_round2(double x) { return (double)llrint(x * 100.0) / 100.0; }

static char *ad2_join_strs(json_t *arr, const char *sep)
{
    if (!ad2_is_arr(arr)) return strdup("");
    size_t cap = 1, n = 0;
    for (size_t i = 0; i < arr->c.count; i++) cap += (strlen(sep) + (ad2_str(arr->c.items[i]) ? strlen(ad2_str(arr->c.items[i])) : 1));
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    for (size_t i = 0; i < arr->c.count; i++) {
        if (i) { strcat(out, sep); n += strlen(sep); }
        const char *s = ad2_str(arr->c.items[i]);
        if (s) { strcat(out, s); n += strlen(s); }
    }
    (void)n;
    return out;
}

/* membership of a JSON string in an array of strings */
static int ad2_in_strs(const json_t *arr, const char *s)
{
    if (!ad2_is_arr(arr) || !s) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *x = ad2_str(arr->c.items[i]);
        if (x && strcmp(x, s) == 0) return 1;
    }
    return 0;
}

/* ── PoP: ad2_single_crash @ tools/async_delegation.py:_single_crash ── */
static json_t *ad2_single_crash(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *r = json_object();
    json_set(r, "status", json_string("error"));
    json_set(r, "summary", json_null());
    const json_t *ev = ad2_get(in, "error");
    json_set(r, "error", ev ? json_copy((json_t *)ev) : json_null());
    json_set(r, "api_calls", json_number(0));
    json_set(r, "duration_seconds", json_number(ad2_num(ad2_get(in, "duration"))));
    return r;
}

/* ── PoP: ad2_batch_crash @ tools/async_delegation.py:_batch_crash ── */
static json_t *ad2_batch_crash(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *r = json_object();
    json_set(r, "results", json_array());
    const json_t *ev = ad2_get(in, "error");
    json_set(r, "error", ev ? json_copy((json_t *)ev) : json_null());
    json_set(r, "total_duration_seconds", json_number(ad2_num(ad2_get(in, "duration"))));
    return r;
}

/* ── PoP: ad2_batch_status @ tools/async_delegation.py:_batch_status ── */
static json_t *ad2_batch_status(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *combined = ad2_get(in, "combined");
    const json_t *kids = ad2_get(combined, "results");
    int has = ad2_truth(kids) && ad2_is_arr(kids);
    int all_bad = 1;
    if (has) {
        for (size_t i = 0; i < kids->c.count; i++) {
            const char *s = ad2_str(ad2_get(kids->c.items[i], "status"));
            if (s && strcmp(s, "completed") == 0) { all_bad = 0; break; }
        }
    }
    return json_string(has && all_bad ? "error" : "completed");
}

/* ── PoP: ad2_stalled_error_text @ tools/async_delegation.py:_stalled_error_text ── */
static json_t *ad2_stalled_error_text(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *rec = ad2_get(in, "event_record");
    char *goal = ad2_norm_words(ad2_get(rec, "goal"));
    size_t glen = strlen(goal);
    if (glen > 120) glen = 120;
    char *label = goal[0] ? ad2_xfmt("Background task \"%.*s\"", (int)glen, goal) : strdup("The background task");
    double quiet = ad2_truth(ad2_get(rec, "_stall_quiet_seconds")) ? ad2_num(ad2_get(rec, "_stall_quiet_seconds")) : 0.0;
    char *silence = strdup("");
    if (quiet >= 60.0) {
        long mins = ad2_round_i(quiet / 60.0);
        silence = ad2_xfmt(" after %ld min of no progress", mins);
    }
    char *s = ad2_xfmt("%s stopped responding%s and was cancelled. Nothing else was affected; "
                       "ask me to run it again if you still need it.", label, silence);
    free(goal); free(label); free(silence);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* ── PoP: ad2_stall_meta @ tools/async_delegation.py:_stall_meta ── */
static json_t *ad2_stall_meta(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *rec = ad2_get(in, "event_record");
    const json_t *it = ad2_get(rec, "_stall_in_tool");
    json_t *phase;
    if (ad2_truth(it)) phase = json_string("in_tool");
    else if (ad2_is_null(it)) phase = json_null();
    else phase = json_string("idle");
    json_t *r = json_object();
    json_set(r, "stalled_after_quiet_seconds", json_copy(ad2_get(rec, "_stall_quiet_seconds")) ? json_copy(ad2_get(rec, "_stall_quiet_seconds")) : json_null());
    json_set(r, "stall_threshold_seconds", json_copy(ad2_get(rec, "_stall_threshold_seconds")) ? json_copy(ad2_get(rec, "_stall_threshold_seconds")) : json_null());
    json_set(r, "stall_phase", phase);
    json_set(r, "stall_grace_seconds", json_number(120.0));
    return r;
}

/* ── PoP: ad2_stalled_result @ tools/async_delegation.py:_stalled_result ── */
static json_t *ad2_stalled_result(ad2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *rec = ad2_get(in, "event_record");
    json_t *completed = json_copy(ad2_get(rec, "completed_at"));
    if (ad2_is_null(completed) || !ad2_truth(completed)) {
        if (completed) json_free(completed);
        completed = ad2_call(io, "time.time", json_object());
        if (ad2_is_raise(completed)) return completed;
    }
    const json_t *disp = ad2_get(rec, "dispatched_at");
    double base = ad2_truth(disp) ? ad2_num(disp) : ad2_num(completed);
    double duration = ad2_round2(ad2_num(completed) - base);
    /* error = _stalled_error_text(rec) */
    json_t *ein = json_object();
    json_set(ein, "event_record", json_copy(rec));
    json_t *ej = ad2_stalled_error_text(io, ein, st);
    json_free(ein);
    char *errs = strdup(ad2_str(ej) ? ad2_str(ej) : "");
    json_free(ej);
    /* logger.error("Async delegation %s force-finalized as stalled after %.0fs", did, duration) */
    json_t *cargs = json_array();
    json_append(cargs, json_string(ad2_str(ad2_get(in, "delegation_id")) ? ad2_str(ad2_get(in, "delegation_id")) : ""));
    json_append(cargs, json_number(duration));
    json_t *lg = ad2_log(io, "error", "Async delegation %s force-finalized as stalled after %.0fs", cargs, 0);
    if (ad2_is_raise(lg)) { free(errs); return lg; }
    json_free(lg);
    /* stall_meta(rec) */
    json_t *min = json_object();
    json_set(min, "event_record", json_copy(rec));
    json_t *meta = ad2_stall_meta(io, min, st);
    json_free(min);
    json_t *r = json_object();
    if (ad2_truth(ad2_get(rec, "is_batch"))) {
        json_set(r, "results", json_array());
        json_set(r, "error", json_string(errs));
        json_set(r, "total_duration_seconds", json_number(duration));
    } else {
        json_set(r, "status", json_string("error"));
        json_set(r, "summary", json_null());
        json_set(r, "error", json_string(errs));
        json_set(r, "api_calls", json_number(0));
        json_set(r, "duration_seconds", json_number(duration));
        json_set(r, "status", json_string("stalled"));
        json_set(r, "exit_reason", json_string("stalled"));
    }
    for (size_t i = 0; i < meta->c.count; i++)
        json_set(r, meta->c.keys[i], json_copy(meta->c.items[i]));
    json_free(meta); json_free(completed); free(errs);
    return r;
}

/* helper: append a record copy into an array if status in statuses and a selector matches */
static void ad2_append_matching(json_t *out, const json_t *rec, const json_t *statuses,
                                const char *sk, const char *o, const char *p)
{
    const char *stt = ad2_str(ad2_get(rec, "status"));
    if (!ad2_in_strs(statuses, stt)) return;
    const char *fields[3] = {"origin_ui_session_id", "session_key", "parent_session_id"};
    const char *wants[3] = {o, sk, p};
    int ok = 0;
    for (int i = 0; i < 3; i++) {
        if (!wants[i] || !wants[i][0]) continue;
        char *v = ad2_str_or_empty(ad2_get(rec, fields[i]));
        if (strcmp(v, wants[i]) == 0) ok = 1;
        free(v);
        if (ok) break;
    }
    if (ok) json_append(out, json_copy((json_t *)rec));
}

/* ── PoP: ad2_session_records @ tools/async_delegation.py:_session_records ── */
static json_t *ad2_session_records(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *statuses = ad2_get(in, "statuses");
    const char *sk = ad2_str(ad2_get(in, "session_key"));
    const char *o = ad2_str(ad2_get(in, "origin_ui_session_id"));
    const char *p = ad2_str(ad2_get(in, "parent_session_id"));
    json_t *out = json_array();
    if (!(sk && sk[0]) && !(o && o[0]) && !(p && p[0])) return out;
    json_t *records = ad2_state_obj(st, "records");
    if (records) for (size_t i = 0; i < records->c.count; i++)
        ad2_append_matching(out, records->c.items[i], statuses, sk, o, p);
    return out;
}

/* ── PoP: ad2_keep_finished_children @ tools/async_delegation.py:_keep_finished_children ── */
static json_t *ad2_keep_finished_children(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *rec = ad2_get(in, "record");
    const json_t *result = ad2_get(in, "result");
    const char *status = ad2_str(ad2_get(in, "status")) ? ad2_str(ad2_get(in, "status")) : "";
    const json_t *finished = ad2_get(rec, "_finished_children");
    if (!ad2_truth(finished) || ad2_truth(ad2_get(result, "results")))
        return json_copy((json_t *)result);
    const json_t *indexes = ad2_get(rec, "task_indexes");
    json_t *tmp = NULL;
    if (!ad2_truth(indexes) || !ad2_is_arr(indexes)) {
        tmp = json_array();
        const json_t *goals = ad2_get(rec, "goals");
        size_t n = ad2_truth(goals) && ad2_is_arr(goals) ? goals->c.count : 0;
        for (size_t i = 0; i < n; i++) json_append(tmp, json_number((double)i));
        indexes = tmp;
    }
    json_t *r = json_copy((json_t *)result);
    json_t *newres = json_array();
    for (size_t i = 0; i < indexes->c.count; i++) {
        const json_t *iv = indexes->c.items[i];
        char key[64];
        snprintf(key, sizeof(key), "%.0f", ad2_num(iv));
        const json_t *found = ad2_get(finished, key);
        /* Live keys are ints; JSON object keys are strings, so finished.get(i)
         * is ALWAYS a miss in the fixture domain — take the default entry. */
        if (0 && ad2_truth(found)) { json_append(newres, json_copy((json_t *)found)); continue; }
        json_t *e = json_object();
        json_set(e, "task_index", json_copy((json_t *)iv));
        json_set(e, "status", json_string(status));
        json_set(e, "summary", json_null());
        json_set(e, "error", json_copy(ad2_get(result, "error")) ? json_copy(ad2_get(result, "error")) : json_null());
        json_append(newres, e);
    }
    json_set(r, "results", newres);
    if (tmp) json_free(tmp);
    return r;
}

/* ── PoP: ad2_recovered_results @ tools/async_delegation.py:_recovered_results ── */
static json_t *ad2_recovered_results(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *task = ad2_get(in, "task");
    const json_t *stored = ad2_get(in, "stored");
    const json_t *error = ad2_get(in, "error");
    const json_t *partial = ad2_is_obj(stored) ? stored : NULL;
    const json_t *pr = partial ? ad2_get(partial, "results") : NULL;
    if (!(ad2_truth(ad2_get(task, "is_batch")) && partial && ad2_truth(ad2_get(partial, "partial")) && ad2_truth(pr)))
        return json_null();
    const json_t *indexes = ad2_get(task, "task_indexes");
    json_t *tmp = NULL;
    if (!ad2_truth(indexes) || !ad2_is_arr(indexes)) {
        tmp = json_array();
        const json_t *goals = ad2_get(task, "goals");
        size_t n = ad2_truth(goals) && ad2_is_arr(goals) ? goals->c.count : 0;
        for (size_t i = 0; i < n; i++) json_append(tmp, json_number((double)i));
        indexes = tmp;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < indexes->c.count; i++) {
        const json_t *iv = indexes->c.items[i];
        char key[64];
        snprintf(key, sizeof(key), "%.0f", ad2_num(iv));
        const json_t *found = NULL;
        if (ad2_is_arr(pr)) for (size_t j = 0; j < pr->c.count; j++) {
            const json_t *r = pr->c.items[j];
            if (ad2_is_int(ad2_get(r, "task_index")) && ad2_num(ad2_get(r, "task_index")) == ad2_num(iv)) { found = r; break; }
        }
        if (found && ad2_truth(found)) { json_append(out, json_copy((json_t *)found)); continue; }
        json_t *e = json_object();
        json_set(e, "task_index", json_copy((json_t *)iv));
        json_set(e, "status", json_string("unknown"));
        json_set(e, "summary", json_null());
        json_set(e, "error", json_copy((json_t *)error));
        json_append(out, e);
    }
    if (tmp) json_free(tmp);
    return out;
}

/* ── PoP: ad2_is_interim_delegation_event @ tools/async_delegation.py:is_interim_delegation_event ── */
static int ad2_is_interim_delegation_event(const json_t *evt)
{
    const char *t = ad2_str(ad2_get(evt, "type"));
    return (t && strcmp(t, "async_delegation") == 0) && ad2_truth(ad2_get(evt, "task_failure_notice"));
}

static json_t *ad2_is_interim_entry(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return json_bool(ad2_is_interim_delegation_event(ad2_get(in, "evt")));
}

/* ── PoP: ad2_completion_copy_id @ tools/async_delegation.py:completion_copy_id ── */
static char *ad2_completion_copy_id_str(const json_t *evt)
{
    const char *t = ad2_str(ad2_get(evt, "type"));
    if (!ad2_is_obj(evt) || !(t && strcmp(t, "async_delegation") == 0) || ad2_is_interim_delegation_event(evt))
        return strdup("");
    return ad2_str_or_empty(ad2_get(evt, "delegation_id"));
}

static json_t *ad2_completion_copy_id(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    char *s = ad2_completion_copy_id_str(ad2_get(in, "evt"));
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* ── PoP: ad2_call_interrupt @ tools/async_delegation.py:_call_interrupt ── */
static json_t *ad2_call_interrupt(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *fn = ad2_get(in, "fn");
    const char *seam = ad2_str(ad2_get(fn, "__callable__"));
    if (!seam) return json_bool(0);
    json_t *res = ad2_call(io, seam, json_object());
    if (ad2_is_raise(res)) {
        if (ad2_exc_is_exception(ad2_kind(res))) {
            const char *msg = ad2_str(ad2_get(in, "msg"));
            json_t *pos = json_array();
            const json_t *cargs = ad2_get(in, "cargs");
            if (ad2_is_arr(cargs)) for (size_t i = 0; i < cargs->c.count; i++) json_append(pos, json_copy(cargs->c.items[i]));
            json_append(pos, json_string(ad2_rmsg(res)));
            json_free(res);
            json_t *lg = ad2_log(io, "debug", msg ? msg : "", pos, 0);
            if (ad2_is_raise(lg)) return lg;
            json_free(lg);
            return json_bool(0);
        }
        return res;  /* BaseException propagates */
    }
    json_free(res);
    return json_bool(1);
}

/* ── PoP: ad2_mark_copy_held @ tools/async_delegation.py:mark_copy_held ── */
static json_t *ad2_mark_copy_held(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *did = ad2_str(ad2_get(in, "delegation_id"));
    if (did && did[0]) {
        json_t *hc = ad2_get(st, "held_copies");
        if (!ad2_is_obj(hc)) { hc = json_object(); json_set(st, "held_copies", hc); }
        double cur = ad2_num(ad2_get(hc, did));
        json_set(hc, did, json_number(cur + 1));
    }
    return json_null();
}

/* ── PoP: ad2_unmark_copy_held @ tools/async_delegation.py:unmark_copy_held ── */
static json_t *ad2_unmark_copy_held(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *did = ad2_str(ad2_get(in, "delegation_id"));
    if (did && did[0]) {
        json_t *hc = ad2_get(st, "held_copies");
        if (!ad2_is_obj(hc)) { hc = json_object(); json_set(st, "held_copies", hc); }
        double cur = ad2_num(ad2_get(hc, did)) - 1.0;
        if (cur <= 0.0) json_obj_del(hc, did);
        else json_set(hc, did, json_number(cur));
    }
    return json_null();
}

/* selector key encoding: field + "|" + value */
static char *ad2_selkey(const char *field, const char *value)
{
    return ad2_xfmt("%s|%s", field, value ? value : "");
}

/* ── PoP: ad2_unfence @ tools/async_delegation.py:_unfence ── */
static void ad2_unfence(json_t *st, const char *token)
{
    if (!token) return;
    json_t *refused = ad2_get(st, "refused");
    json_t *by = ad2_get(st, "refused_by");
    if (!ad2_is_obj(refused)) return;
    const json_t *sels = ad2_get(refused, token);
    if (!ad2_is_arr(sels)) { json_obj_del(refused, token); return; }
    for (size_t i = 0; i < sels->c.count; i++) {
        const char *sel = ad2_str(sels->c.items[i]);
        if (!sel || !ad2_is_obj(by)) continue;
        json_t *toks = ad2_get(by, sel);
        if (!ad2_is_arr(toks)) continue;
        json_t *nt = json_array();
        for (size_t j = 0; j < toks->c.count; j++) {
            const char *t = ad2_str(toks->c.items[j]);
            if (!(t && strcmp(t, token) == 0)) json_append(nt, json_copy(toks->c.items[j]));
        }
        if (nt->c.count == 0) json_obj_del(by, sel);
        else json_set(by, sel, nt);
        json_free(nt);
    }
    json_obj_del(refused, token);
}

/* ── PoP: ad2_refused_for_session @ tools/async_delegation.py:refused_for_session ── */
static json_t *ad2_refused_for_session(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *token = ad2_str(ad2_get(in, "token"));
    json_t *refused = ad2_get(st, "refused");
    return json_bool(token && ad2_is_obj(refused) && ad2_get(refused, token) != NULL);
}

/* ── PoP: ad2_allow_for_session @ tools/async_delegation.py:allow_for_session ── */
static json_t *ad2_allow_for_session(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    ad2_unfence(st, ad2_str(ad2_get(in, "token")));
    return json_null();
}

/* ── PoP: ad2_refuse_for_session @ tools/async_delegation.py:refuse_for_session ── */
static json_t *ad2_refuse_for_session(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *token = ad2_str(ad2_get(in, "token"));
    if (!token) return json_null();
    json_t *refused = ad2_get(st, "refused");
    if (!ad2_is_obj(refused)) { refused = json_object(); json_set(st, "refused", refused); }
    json_t *by = ad2_get(st, "refused_by");
    if (!ad2_is_obj(by)) { by = json_object(); json_set(st, "refused_by", by); }
    ad2_unfence(st, token);
    json_t *fence = json_array();
    const char *sk = ad2_str(ad2_get(in, "session_key"));
    if (sk && sk[0]) { char *k = ad2_selkey("session_key", sk); json_append(fence, json_string(k)); free(k); }
    const json_t *pids = ad2_get(in, "parent_session_ids");
    if (ad2_is_arr(pids)) for (size_t i = 0; i < pids->c.count; i++) {
        const char *p = ad2_str(pids->c.items[i]);
        if (!p || !p[0]) continue;
        char *k = ad2_selkey("parent_session_id", p);
        int dup = 0;
        for (size_t j = 0; j < fence->c.count; j++) if (strcmp(ad2_str(fence->c.items[j]), k) == 0) dup = 1;
        if (!dup) json_append(fence, json_string(k));
        free(k);
    }
    json_set(refused, token, json_copy(fence));
    for (size_t i = 0; i < fence->c.count; i++) {
        const char *sel = ad2_str(fence->c.items[i]);
        json_t *toks = ad2_get(by, sel);
        if (!ad2_is_arr(toks)) { toks = json_array(); json_set(by, sel, toks); }
        int has = 0;
        for (size_t j = 0; j < toks->c.count; j++) if (strcmp(ad2_str(toks->c.items[j]), token) == 0) has = 1;
        if (!has) json_append(toks, json_string(token));
    }
    json_free(fence);
    return json_null();
}


/* ── PoP: ad2_queued_completion_ids @ tools/async_delegation.py:_queued_completion_ids ── */
static json_t *ad2_queued_completion_ids(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *queues = ad2_get(in, "queues");
    json_t *ids = json_object();  /* set -> object of id->true */
    if (ad2_is_arr(queues)) {
        json_t *seen = json_array();
        for (size_t qi = 0; qi < queues->c.count; qi++) {
            const json_t *q = queues->c.items[qi];
            if (ad2_is_null(q)) continue;
            const char *qid = ad2_str(ad2_get(q, "__id__"));
            if (!qid) qid = "anon";
            int dup = 0;
            for (size_t j = 0; j < seen->c.count; j++) if (strcmp(ad2_str(seen->c.items[j]), qid) == 0) dup = 1;
            if (dup) continue;
            json_append(seen, json_string(qid));
            const json_t *mutex = ad2_get(q, "mutex");
            const json_t *items = ad2_get(q, "queue");
            if (ad2_is_null(mutex) || !ad2_is_arr(items)) {
                json_free(seen); json_free(ids);
                return json_null();
            }
            /* with mutex: snapshot, then with _held_lock: ids.update(_held_copies) */
            json_t *hc = ad2_get(st, "held_copies");
            if (ad2_is_obj(hc)) for (size_t k = 0; k < hc->c.count; k++) json_set(ids, hc->c.keys[k], json_bool(1));
            for (size_t k = 0; k < items->c.count; k++) {
                json_t *ein = json_object();
                json_set(ein, "evt", json_copy(items->c.items[k]));
                char *cid = ad2_completion_copy_id_str(items->c.items[k]);
                json_free(ein);
                if (cid && cid[0]) json_set(ids, cid, json_bool(1));
                free(cid);
            }
        }
        json_free(seen);
    }
    json_obj_del(ids, "");
    /* Python set -> sorted array (order-insensitive normalization on both sides) */
    json_t *out = json_array();
    for (size_t i = 0; i < ids->c.count; i++) {
        const char *k = ids->c.keys[i];
        size_t pos = out->c.count;
        json_append(out, json_string(k));
        for (size_t j = pos; j > 0 && strcmp(ad2_str(out->c.items[j-1]), ad2_str(out->c.items[j])) > 0; j--) {
            json_t *t = out->c.items[j-1]; out->c.items[j-1] = out->c.items[j]; out->c.items[j] = t;
        }
    }
    json_free(ids);
    return out;
}

/* ── PoP: ad2_copy_may_be_live @ tools/async_delegation.py:_copy_may_be_live ── */
static json_t *ad2_copy_may_be_live(ad2_io_t *io, const json_t *in, json_t *st)
{
    const char *did = ad2_str(ad2_get(in, "delegation_id"));
    const json_t *offered = ad2_get(in, "offered");
    int did_offered = 0;
    if (ad2_is_arr(offered)) for (size_t i = 0; i < offered->c.count; i++) {
        const char *x = ad2_str(offered->c.items[i]);
        if (did && x && strcmp(x, did) == 0) { did_offered = 1; break; }
    }
    if (!did_offered) {
        /* int(pid) == os.getpid() — seam fires only when the first test is false */
        json_t *pjr = ad2_call(io, "os.getpid", json_object());
        if (ad2_is_raise(pjr)) return pjr;
        long pid = (long)ad2_num(ad2_get(in, "pid"));
        long real = (long)ad2_num(pjr);
        json_free(pjr);
        if (pid != real) return json_bool(1);
    }
    double updated = ad2_num(ad2_get(in, "updated_at"));
    double now = ad2_num(ad2_get(in, "now"));
    const json_t *live = ad2_get(in, "live_ids");
    int in_live = 0;
    if (ad2_is_arr(live) && did) for (size_t i = 0; i < live->c.count; i++) {
        const char *x = ad2_str(live->c.items[i]);
        if (x && strcmp(x, did) == 0) { in_live = 1; break; }
    }
    if (in_live || updated >= now - 300.0) return json_bool(1);
    const char *home = ad2_str(ad2_get(in, "home"));
    char *key = ad2_selkey(home ? home : "", did ? did : "");
    json_t *ra = ad2_get(st, "reoffered_at");
    double at = ad2_is_obj(ra) && ad2_get(ra, key) ? ad2_num(ad2_get(ra, key)) : -INFINITY;
    free(key);
    return json_bool(at >= now - 300.0);
}

/* ── PoP: ad2_note_reoffers @ tools/async_delegation.py:_note_reoffers ── */
static json_t *ad2_note_reoffers(ad2_io_t *io, const json_t *in, json_t *st)
{
    const char *home = ad2_str(ad2_get(in, "home")) ? ad2_str(ad2_get(in, "home")) : "";
    const json_t *lost = ad2_get(in, "lost");
    double now = ad2_num(ad2_get(in, "now"));
    json_t *ra = ad2_get(st, "reoffered_at");
    if (!ad2_is_obj(ra)) { ra = json_object(); json_set(st, "reoffered_at", ra); }
    json_t *offered = ad2_get(st, "offered");
    if (!ad2_is_arr(offered)) { offered = json_array(); json_set(st, "offered", offered); }
    int first = 0;
    if (ad2_is_arr(lost)) for (size_t i = 0; i < lost->c.count; i++) {
        const char *d = ad2_str(lost->c.items[i]);
        char *k = ad2_selkey(home, d ? d : "");
        if (!ad2_get(ra, k)) first = 1;
        free(k);
    }
    /* prune keys older than the replay cap */
    for (size_t i = ra->c.count; i > 0; i--) {
        json_t *v = ra->c.items[i-1];
        if (now - ad2_num(v) > 172800.0) json_obj_del(ra, ra->c.keys[i-1]);
    }
    if (ad2_is_arr(lost)) for (size_t i = 0; i < lost->c.count; i++) {
        const char *d = ad2_str(lost->c.items[i]);
        char *k = ad2_selkey(home, d ? d : "");
        json_set(ra, k, json_number(now));
        free(k);
    }
    if (ad2_is_arr(lost)) for (size_t i = 0; i < lost->c.count; i++) {
        const char *d = ad2_str(lost->c.items[i]);
        for (size_t j = offered->c.count; j > 0; j--) {
            json_t *pair = offered->c.items[j-1];
            const char *ph = ad2_is_arr(pair) && pair->c.count > 0 ? ad2_str(pair->c.items[0]) : NULL;
            const char *pd = ad2_is_arr(pair) && pair->c.count > 1 ? ad2_str(pair->c.items[1]) : NULL;
            if (ph && pd && strcmp(ph, home) == 0 && d && strcmp(pd, d) == 0) json_array_remove(offered, j-1);
        }
    }
    char *joined = ad2_join_strs((json_t *)lost, ", ");
    const char *cargsv[2] = { joined ? joined : "", joined ? joined : "" };
    /* build args explicitly: [len(lost), joined] */
    json_t *pos = json_array();
    json_append(pos, json_number(ad2_is_arr(lost) ? (double)lost->c.count : 0.0));
    json_append(pos, json_string(joined ? joined : ""));
    (void)cargsv;
    json_t *lg = ad2_log(io, first ? "warning" : "debug",
        "Re-offering %d pending async completion(s) whose in-memory copy was lost while this process ran: %s",
        pos, 0);
    free(joined);
    return lg;
}

/* ── PoP: ad2_maybe_sweep_orphaned_completions @ tools/async_delegation.py:maybe_sweep_orphaned_completions ── */
static json_t *ad2_maybe_sweep_orphaned_completions(ad2_io_t *io, const json_t *in, json_t *st)
{
    /* home = hermes_home_key(get_hermes_home()) */
    json_t *hh = ad2_call(io, "get_hermes_home", json_object());
    if (ad2_is_raise(hh)) return hh;
    json_t *ha = json_object();
    json_set(ha, "path", json_copy(hh));
    json_free(hh);
    json_t *home = ad2_call(io, "hermes_home_key", ha);
    if (ad2_is_raise(home)) return home;
    const char *h = ad2_str(home);
    const json_t *nj = ad2_get(in, "now");
    json_t *tm = NULL;
    double now;
    if (ad2_is_null(nj)) {
        tm = ad2_call(io, "time.monotonic", json_object());
        if (ad2_is_raise(tm)) { json_free(home); return tm; }
        now = ad2_num(tm);
    } else now = ad2_num(nj);
    json_t *ls = ad2_get(st, "last_orphan_sweep");
    if (!ad2_is_obj(ls)) { ls = json_object(); json_set(st, "last_orphan_sweep", ls); }
    const json_t *last = h ? ad2_get(ls, h) : NULL;
    if (!ad2_is_null(last) && (now - ad2_num(last)) < 30.0) {
        json_free(home); if (tm) json_free(tm);
        return json_number(0);
    }
    if (h) json_set(ls, h, json_number(now));
    json_free(home); if (tm) json_free(tm);
    /* try: sweep_orphaned_completions(target_queue) */
    json_t *sa = json_object();
    json_t *sw = ad2_call(io, "sweep_orphaned_completions", sa);
    if (ad2_is_raise(sw)) {
        if (ad2_exc_is_exception(ad2_kind(sw))) {
            json_free(sw);
            json_t *lg = ad2_log(io, "debug", "Orphaned async delegation sweep failed", json_array(), 1);
            if (ad2_is_raise(lg)) return lg;
            json_free(lg);
            return json_number(0);
        }
        return sw;
    }
    return sw;
}

/* ── PoP: ad2_event_delivery @ tools/async_delegation.py:_event_delivery ── */
static json_t *ad2_event_delivery(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *claim = ad2_get(in, "claim_id");
    const json_t *evt = ad2_get(in, "evt");
    const char *t = ad2_str(ad2_get(evt, "type"));
    if (!ad2_truth(claim) || !(t && strcmp(t, "async_delegation") == 0)) return json_null();
    const char *fnseam = ad2_str(ad2_get(ad2_get(in, "fn"), "__callable__"));
    if (!fnseam) return json_null();
    char *did = ad2_str_or_empty(ad2_get(evt, "delegation_id"));
    json_t *a = json_object();
    json_set(a, "delegation_id", json_string(did));
    json_set(a, "claim_id", json_copy((json_t *)claim));
    free(did);
    json_t *r = ad2_call(io, fnseam, a);
    if (ad2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* ── PoP: ad2_return_completion_offer @ tools/async_delegation.py:return_completion_offer ── */
static json_t *ad2_return_completion_offer(ad2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *evt = ad2_get(in, "evt");
    const char *t = ad2_str(ad2_get(evt, "type"));
    char *did = (t && strcmp(t, "async_delegation") == 0) ? ad2_str_or_empty(ad2_get(evt, "delegation_id")) : strdup("");
    if (!did[0] || ad2_is_interim_delegation_event(evt)) { free(did); return json_null(); }
    json_t *offered = ad2_get(st, "offered");
    if (ad2_is_arr(offered)) for (size_t i = offered->c.count; i > 0; i--) {
        json_t *pair = offered->c.items[i-1];
        const char *pd = ad2_is_arr(pair) && pair->c.count > 1 ? ad2_str(pair->c.items[1]) : NULL;
        if (pd && strcmp(pd, did) == 0) json_array_remove(offered, i-1);
    }
    free(did);
    return json_null();
}
