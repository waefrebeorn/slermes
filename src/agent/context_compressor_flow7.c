/* context_compressor_flow7.c — seventh-wave FLOW port of
 * agent/context_compressor.py (LIVE authority, quarry READ-ONLY).
 *
 * Ports 14 UNCLAIMED module functions / methods (remainder beyond flow6):
 *   _begin_compress_attempt, _load_consecutive_overload_aborts,
 *   _load_anti_thrash_recovery_deadline, _persist_consecutive_overload_aborts,
 *   _increment_consecutive_overload_aborts, _record_summary_success,
 *   _record_summary_input_coverage, _log_compression_start,
 *   _reset_session_compaction_state, _native_anthropic_budget,
 *   _record_structural_no_op, _has_merged_inflight_replay,
 *   _derive_trigger, _fallback_summary_for_window.
 *
 * Every symbol is static cc7_*; libjson only. Claimed siblings / foreign
 * helpers / self-methods / logger / clock enter through the cc7_io_t universal
 * seam. Ordered trace: one event per seam call, args (null keys stripped) and
 * recorded ret. Fail-closed: an absent/exhausted script yields a RuntimeError
 * raise spec recorded in the trace and propagated past every site.
 *
 * FULL Python 3.14 builtin exception hierarchy (CC7_EXC_PARENT + cc7_exc_is_base)
 * models `except Exception` vs BaseException passthrough: a scripted base-only
 * kind (KeyboardInterrupt/SystemExit/GeneratorExit/CancelledError/BaseException)
 * propagates as a BaseException exactly as live.
 *
 * State model: mutable inputs named in MUTABLE_KEYS (self, msg, result,
 * telemetry) are seeded into `st` from `in` by the harness; ported methods
 * mutate `st` in place, and the harness emits `st`. The oracle mirrors with
 * deep-copied inputs and reads back their post-call projection.
 *
 * PoP: cc7_<name> @ agent/context_compressor.py:<fn>.
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow7 \
 *           oracle_context_compressor_flow7.py "" "lib/libjson/json.o"
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

#define CC7_SUMMARY_END_MARKER \
    "--- END OF CONTEXT SUMMARY — respond to the message below, not the summary above ---"
#define CC7_INFLIGHT_REPLAY_HEADER \
    "[STILL IN PROGRESS — this is the active request, restated after the " \
    "compaction boundary because it was not finished yet. Continue it; do not " \
    "start over.]"

/* ── universal seam ── */
typedef struct cc7_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cc7_io_t;

/* ── accessors / truthiness ── */
static int cc7_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cc7_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cc7_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cc7_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cc7_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cc7_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cc7_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cc7_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static const json_t *cc7_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cc7_num(const json_t *v) { return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0; }
static long long cc7_int(const json_t *v) { return (long long)cc7_num(v); }

/* ── formatting helpers ── */
static char *cc7_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* ASCII whitespace set matching Python str.strip() for fixture-domain text. */
static int cc7_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x0b || c == 0x0c;
}

static char *cc7_strip(const char *s)
{
    if (!s) return NULL;
    const char *a = s, *b = s + strlen(s);
    while (a < b && cc7_ws((unsigned char)*a)) a++;
    while (b > a && cc7_ws((unsigned char)(b[-1]))) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* ── raise specs ── */
static json_t *cc7_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static int cc7_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* ── FULL builtin exception hierarchy (Python 3.14) ── */
typedef struct { const char *name; const char *parent; } cc7_exc_edge_t;
static const cc7_exc_edge_t CC7_EXC_PARENT[] = {
    { "BaseException", NULL },
    { "SystemExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "GeneratorExit", "BaseException" },
    { "CancelledError", "BaseException" },
    { "BaseExceptionGroup", "BaseException" },
    { "Exception", "BaseException" },
    { "ExceptionGroup", "Exception" },
    { "StopIteration", "Exception" },
    { "StopAsyncIteration", "Exception" },
    { "ArithmeticError", "Exception" },
    { "FloatingPointError", "ArithmeticError" },
    { "OverflowError", "ArithmeticError" },
    { "ZeroDivisionError", "ArithmeticError" },
    { "AssertionError", "Exception" },
    { "AttributeError", "Exception" },
    { "BufferError", "Exception" },
    { "EOFError", "Exception" },
    { "ImportError", "Exception" },
    { "ModuleNotFoundError", "ImportError" },
    { "LookupError", "Exception" },
    { "IndexError", "LookupError" },
    { "KeyError", "LookupError" },
    { "MemoryError", "Exception" },
    { "NameError", "Exception" },
    { "UnboundLocalError", "NameError" },
    { "OSError", "Exception" },
    { "EnvironmentError", "OSError" },
    { "IOError", "OSError" },
    { "WindowsError", "OSError" },
    { "BlockingIOError", "OSError" },
    { "ChildProcessError", "OSError" },
    { "ConnectionError", "OSError" },
    { "BrokenPipeError", "ConnectionError" },
    { "ConnectionAbortedError", "ConnectionError" },
    { "ConnectionRefusedError", "ConnectionError" },
    { "ConnectionResetError", "ConnectionError" },
    { "FileExistsError", "OSError" },
    { "FileNotFoundError", "OSError" },
    { "InterruptedError", "OSError" },
    { "IsADirectoryError", "OSError" },
    { "NotADirectoryError", "OSError" },
    { "PermissionError", "OSError" },
    { "ProcessLookupError", "OSError" },
    { "TimeoutError", "OSError" },
    { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" },
    { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" },
    { "PythonFinalizationError", "RuntimeError" },
    { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" },
    { "TabError", "IndentationError" },
    { "_IncompleteInputError", "SyntaxError" },
    { "SystemError", "Exception" },
    { "TypeError", "Exception" },
    { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" },
    { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" },
    { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" },
    { "DeprecationWarning", "Warning" },
    { "ImportWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" },
    { "ResourceWarning", "Warning" },
    { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" },
    { "UnicodeWarning", "Warning" },
    { "UserWarning", "Warning" },
    { "FutureWarning", "Warning" },
    { "BytesWarning", "Warning" },
    { "EncodingWarning", "Warning" },
    { NULL, NULL },
};

static const char *cc7_exc_parent(const char *kind)
{
    if (!kind || !*kind) return NULL;
    for (const cc7_exc_edge_t *e = CC7_EXC_PARENT; e->name; e++)
        if (strcmp(kind, e->name) == 0) return e->parent;
    return NULL;
}

static int cc7_exc_is_base(const char *kind)
{
    if (!kind || !*kind) return 0;
    const char *cur = kind;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, "Exception") == 0 || strcmp(cur, "ExceptionGroup") == 0) return 0;
        if (strcmp(cur, "BaseException") == 0) return 1;
        const char *p = cc7_exc_parent(cur);
        if (!p) return 0;
        cur = p;
    }
    return 0;
}

/* ── universal seam call (frees args; returns owned result) ── */
static json_t *cc7_call(cc7_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cc7_xfmt("no seam installed for %s", name);
        r = cc7_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* ═══════════════════════ ported functions ═══════════════════════ */

/* PoP: cc7_begin_compress_attempt @ agent/context_compressor.py:_begin_compress_attempt */
static json_t *cc7_begin_compress_attempt(cc7_io_t *io, const json_t *in, json_t *st)
{
    json_t *self = (json_t *)cc7_get(st, "self");
    /* reset per-call result state (failure flags intentionally NOT reset) */
    if (cc7_is_obj(self)) {
        json_set(self, "_last_summary_dropped_count", json_number(0));
        json_set(self, "_last_summary_fallback_used", json_bool(0));
        json_set(self, "_last_feasibility_skip", json_bool(0));
        json_set(self, "_last_summary_error", json_null());
        json_set(self, "_last_aux_model_failure_error", json_null());
        json_set(self, "_last_aux_model_failure_model", json_null());
        json_set(self, "_last_compress_aborted", json_bool(0));
        json_set(self, "_last_compress_refused_would_grow", json_bool(0));
        json_set(self, "_last_summary_overload_degraded", json_bool(0));
        json_set(self, "_last_compression_made_progress", json_bool(0));
    }
    const json_t *ct = cc7_get(in, "current_tokens");
    json_t *a = json_object();
    json_set(a, "current_tokens", ct ? json_copy(ct) : json_null());
    json_t *tel = cc7_call(io, "_begin_compression_telemetry", a);
    if (cc7_is_raise(tel)) return tel;
    if (!cc7_is_obj(tel)) {
        json_t *o = json_object();
        json_free(tel);
        tel = o;
    }
    json_set(tel, "chunk_count", json_number(0));
    if (cc7_truth(cc7_get(in, "force"))) {
        json_t *a2 = json_object();
        json_t *r = cc7_call(io, "_clear_compression_failure_cooldown", a2);
        if (cc7_is_raise(r)) { json_free(tel); return r; }
        json_free(r);
        if (cc7_is_obj(self)) json_set(self, "_structural_no_op_backoff_until", json_number(0));
    }
    return tel;
}

/* helper: _load_durable(name, op, label, type, default) -> seam, no state write here */
static json_t *cc7_load_durable(cc7_io_t *io, const char *name, const char *op,
                                const char *label, const char *type, const json_t *def)
{
    json_t *a = json_object();
    json_set(a, "name", json_string(name));
    json_set(a, "op", json_string(op));
    json_set(a, "label", json_string(label));
    json_set(a, "type", json_string(type));
    json_set(a, "default", def ? json_copy(def) : json_number(0));
    return cc7_call(io, "_load_durable", a);
}

/* PoP: cc7_load_consecutive_overload_aborts @ agent/context_compressor.py:_load_consecutive_overload_aborts */
static json_t *cc7_load_consecutive_overload_aborts(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *r = cc7_load_durable(io, "_consecutive_overload_aborts",
                                 "get_compression_overload_streak", "compression overload streak",
                                 "int", NULL);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: cc7_load_anti_thrash_recovery_deadline @ agent/context_compressor.py:_load_anti_thrash_recovery_deadline */
static json_t *cc7_load_anti_thrash_recovery_deadline(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *d = json_number(0);
    json_t *r = cc7_load_durable(io, "_anti_thrash_recovery_deadline",
                                 "get_compression_recovery_deadline", "compression recovery deadline",
                                 "float", d);
    json_free(d);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: cc7_persist_consecutive_overload_aborts @ agent/context_compressor.py:_persist_consecutive_overload_aborts */
static json_t *cc7_persist_consecutive_overload_aborts(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    const json_t *self = cc7_get(st, "self");
    const json_t *v = cc7_get(self, "_consecutive_overload_aborts");
    if (!v)
        return cc7_mkraise("AttributeError",
                           "'types.SimpleNamespace' object has no attribute '_consecutive_overload_aborts'");
    json_t *a = json_object();
    json_set(a, "op", json_string("set_compression_overload_streak"));
    json_set(a, "label", json_string("compression overload streak"));
    json_set(a, "value", json_copy(v));
    return cc7_call(io, "_durable_write", a);
}

/* PoP: cc7_increment_consecutive_overload_aborts @ agent/context_compressor.py:_increment_consecutive_overload_aborts */
static json_t *cc7_increment_consecutive_overload_aborts(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *self = (json_t *)cc7_get(st, "self");
    json_t *a = json_object();
    json_set(a, "op", json_string("increment_compression_overload_streak"));
    json_set(a, "label", json_string("compression overload streak increment"));
    json_set(a, "type", json_string("int"));
    json_set(a, "default", json_number(0));
    json_t *r = cc7_call(io, "_durable_read", a);
    if (cc7_is_raise(r)) return r;
    long long streak = 0;
    int found = 0;
    if (cc7_is_arr(r) && r->c.count >= 2) {
        found = cc7_truth(r->c.items[0]);
        streak = cc7_int(r->c.items[1]);
    }
    json_free(r);
    long long cur = cc7_int(cc7_get(self, "_consecutive_overload_aborts"));
    long long next = (found && streak) ? streak : cur + 1;
    if (cc7_is_obj(self)) json_set(self, "_consecutive_overload_aborts", json_number((double)next));
    return json_null();
}

/* PoP: cc7_record_summary_success @ agent/context_compressor.py:_record_summary_success */
static json_t *cc7_record_summary_success(cc7_io_t *io, const json_t *in, json_t *st)
{
    json_t *self = (json_t *)cc7_get(st, "self");
    const json_t *sum = cc7_get(in, "summary");
    if (cc7_is_obj(self)) json_set(self, "_previous_summary", sum ? json_copy(sum) : json_null());
    json_t *a1 = json_object();
    json_t *r = cc7_call(io, "_clear_compression_failure_cooldown", a1);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    if (cc7_is_obj(self)) {
        json_set(self, "_summary_model_fallen_back", json_bool(0));
        json_set(self, "_last_summary_error", json_null());
    }
    json_t *a2 = json_object();
    r = cc7_call(io, "_clear_terminal_summary_failures", a2);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    json_t *a3 = json_object();
    r = cc7_call(io, "_reset_consecutive_overload_aborts", a3);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: cc7_record_summary_input_coverage @ agent/context_compressor.py:_record_summary_input_coverage */
static json_t *cc7_record_summary_input_coverage(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *self = cc7_get(st, "self");
    json_t *tel = (json_t *)cc7_get(self, "_active_compression_telemetry");
    if (!cc7_is_obj(tel)) return json_null();
    const json_t *cov = cc7_get(in, "coverage");
    static const char *src[] = { "input_chars", "sampled_chars", "omitted_chars",
                                 "record_count", "sampled_record_count", "elided_record_count", NULL };
    static const char *dst[] = { "summary_input_chars", "summary_input_sampled_chars",
                                 "summary_input_omitted_chars", "summary_input_record_count",
                                 "summary_input_sampled_record_count", "summary_input_elided_record_count", NULL };
    for (int i = 0; src[i]; i++) {
        const json_t *v = cc7_get(cov, src[i]);
        json_set(tel, dst[i], v ? json_copy(v) : json_number(0));
    }
    return json_null();
}

/* helper: fire a logger seam with fmt + args array */
static json_t *cc7_log(cc7_io_t *io, const char *name, const char *fmt, json_t *args)
{
    json_t *a = json_object();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());
    return cc7_call(io, name, a);
}

/* PoP: cc7_log_compression_start @ agent/context_compressor.py:_log_compression_start */
static json_t *cc7_log_compression_start(cc7_io_t *io, const json_t *in, json_t *st)
{
    const json_t *self = cc7_get(st, "self");
    long long display = cc7_int(cc7_get(in, "display_tokens"));
    long long cs = cc7_int(cc7_get(in, "compress_start"));
    long long ce = cc7_int(cc7_get(in, "compress_end"));
    long long nturns = cc7_int(cc7_get(in, "n_turns"));
    long long tail = cc7_int(cc7_get(in, "tail_msgs"));

    json_t *args1 = json_array();
    json_append(args1, json_number((double)display));
    json_append(args1, cc7_get(self, "threshold_tokens") ? json_copy(cc7_get(self, "threshold_tokens")) : json_number(0));
    json_t *r = cc7_log(io, "_log_info", "Context compression triggered (%d tokens >= %d threshold)", args1);
    if (cc7_is_raise(r)) return r;
    json_free(r);

    json_t *args2 = json_array();
    json_append(args2, cc7_get(self, "context_length") ? json_copy(cc7_get(self, "context_length")) : json_number(0));
    json_append(args2, json_number(cc7_num(cc7_get(self, "threshold_percent")) * 100.0));
    json_append(args2, cc7_get(self, "threshold_tokens") ? json_copy(cc7_get(self, "threshold_tokens")) : json_number(0));
    r = cc7_log(io, "_log_info", "Model context limit: %d tokens (%.0f%% = %d)", args2);
    if (cc7_is_raise(r)) return r;
    json_free(r);

    json_t *args3 = json_array();
    json_append(args3, json_number((double)(cs + 1)));
    json_append(args3, json_number((double)ce));
    json_append(args3, json_number((double)nturns));
    json_append(args3, json_number((double)cs));
    json_append(args3, json_number((double)tail));
    r = cc7_log(io, "_log_info",
                "Summarizing turns %d-%d (%d turns), protecting %d head + %d tail messages", args3);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: cc7_reset_session_compaction_state @ agent/context_compressor.py:_reset_session_compaction_state */
static json_t *cc7_reset_session_compaction_state(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *self = (json_t *)cc7_get(st, "self");
    const json_t *pc = cc7_get(self, "prepared_compaction");
    if (!cc7_is_null(pc)) {
        json_t *a = json_object();
        json_set(a, "reason", json_string("session boundary"));
        json_t *r = cc7_call(io, "_prepared_compaction_discard", a);
        if (cc7_is_raise(r)) return r;
        json_free(r);
    }
    if (cc7_is_obj(self)) {
        json_set(self, "_previous_summary", json_null());
        json_set(self, "_summary_has_user_turn", json_null());
        json_set(self, "_last_summary_error", json_null());
        json_set(self, "_last_aux_model_failure_error", json_null());
        json_set(self, "_last_aux_model_failure_model", json_null());
        json_set(self, "_last_aux_resolved_model", json_null());
        json_set(self, "_consecutive_timeout_failures", json_number(0));
        json_set(self, "_consecutive_truncation_failures", json_number(0));
        json_set(self, "_consecutive_overload_aborts", json_number(0));
        json_set(self, "_last_summary_overload_degraded", json_bool(0));
        json_set(self, "_last_summary_dropped_count", json_number(0));
        json_set(self, "_last_summary_fallback_used", json_bool(0));
        json_set(self, "_last_feasibility_skip", json_bool(0));
        json_set(self, "_last_compression_savings_pct", json_number(100));
        json_set(self, "_ineffective_compression_count", json_number(0));
        json_set(self, "_anti_thrash_recovery_deadline", json_number(0));
        json_set(self, "_structural_no_op_backoff_until", json_number(0));
        json_set(self, "_structural_no_op_len", json_number(0));
        json_set(self, "_prellm_skip_count", json_number(0));
        json_set(self, "_fallback_compression_streak", json_number(0));
        json_set(self, "_fallback_probe_at", json_number(0));
        json_set(self, "_verify_compaction_cleared_threshold", json_bool(0));
        json_set(self, "_last_compression_made_progress", json_bool(0));
        json_set(self, "_summary_failure_cooldown_until", json_number(0));
        json_set(self, "_cooldown_persist_failed", json_bool(0));
        json_set(self, "_last_compress_aborted", json_bool(0));
        json_set(self, "_last_compress_refused_would_grow", json_bool(0));
        json_set(self, "_context_probed", json_bool(0));
        json_set(self, "_context_probe_persistable", json_bool(0));
    }
    json_t *a1 = json_object();
    json_t *r = cc7_call(io, "_reset_real_usage_pairing", a1);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    if (cc7_is_obj(self)) {
        json_set(self, "_last_compression_telemetry", json_null());
        json_set(self, "_active_compression_telemetry", json_null());
        json_set(self, "_compression_telemetry_seed", json_null());
    }
    json_t *a2 = json_object();
    r = cc7_call(io, "_reset_proactive_prune_rearm", a2);
    if (cc7_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: cc7_native_anthropic_budget @ agent/context_compressor.py:_native_anthropic_budget */
static json_t *cc7_native_anthropic_budget(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    const json_t *self = cc7_get(st, "self");
    json_t *a = json_object();
    json_set(a, "self", self ? json_copy(self) : json_null());
    return cc7_call(io, "_native_anthropic_tail_prices", a);
}

/* PoP: cc7_record_structural_no_op @ agent/context_compressor.py:_record_structural_no_op */
static json_t *cc7_record_structural_no_op(cc7_io_t *io, const json_t *in, json_t *st)
{
    json_t *self = (json_t *)cc7_get(st, "self");
    json_t *a0 = json_object();
    json_t *mono = cc7_call(io, "_time_monotonic", a0);
    if (cc7_is_raise(mono)) return mono;
    double backoff = cc7_num(mono) + cc7_num(cc7_get(self, "_STRUCTURAL_NO_OP_BACKOFF_SECONDS"));
    json_free(mono);
    if (cc7_is_obj(self)) {
        json_set(self, "_structural_no_op_backoff_until", json_number(backoff));
        const json_t *nm = cc7_get(in, "n_messages");
        long long n = cc7_truth(nm) ? cc7_int(nm) : 0;
        json_set(self, "_structural_no_op_len", json_number((double)n));
    }
    if (!cc7_truth(cc7_get(self, "quiet_mode"))) {
        json_t *args = json_array();
        json_append(args, cc7_get(in, "reason") ? json_copy(cc7_get(in, "reason")) : json_string(""));
        json_append(args, json_number(cc7_num(cc7_get(self, "_STRUCTURAL_NO_OP_BACKOFF_SECONDS"))));
        json_t *r = cc7_log(io, "_log_warning",
                            "Compression skipped (%s): retrying in %.0fs (structural no-op backoff)", args);
        if (cc7_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: cc7_has_merged_inflight_replay @ agent/context_compressor.py:_has_merged_inflight_replay */
static json_t *cc7_has_merged_inflight_replay(cc7_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *msg = cc7_get(in, "message");
    json_t *a1 = json_object();
    json_set(a1, "message", msg ? json_copy(msg) : json_null());
    json_t *isum = cc7_call(io, "_is_context_summary_message", a1);
    if (cc7_is_raise(isum)) return isum;
    if (!cc7_truth(isum)) { json_free(isum); return json_bool(0); }
    json_free(isum);

    json_t *a2 = json_object();
    json_set(a2, "content", cc7_get(msg, "content") ? json_copy(cc7_get(msg, "content")) : json_null());
    json_t *txt = cc7_call(io, "_content_text_for_contains", a2);
    if (cc7_is_raise(txt)) return txt;
    const char *text = cc7_str(txt) ? cc7_str(txt) : "";

    /* Python: text.rpartition(_SUMMARY_END_MARKER) -> (_, boundary, remainder) */
    const char *last = NULL;
    const char *p = text;
    size_t seplen = strlen(CC7_SUMMARY_END_MARKER);
    while ((p = strstr(p, CC7_SUMMARY_END_MARKER)) != NULL) { last = p; p += seplen; }
    const char *remainder = last ? (last + seplen) : text;
    /* rest = remainder.lstrip() */
    while (*remainder && cc7_ws((unsigned char)*remainder)) remainder++;
    int ok = 0;
    if (last) {
        size_t hlen = strlen(CC7_INFLIGHT_REPLAY_HEADER);
        if (strncmp(remainder, CC7_INFLIGHT_REPLAY_HEADER, hlen) == 0) {
            char *tail = cc7_strip(remainder + hlen);
            ok = tail && tail[0] ? 1 : 0;
            free(tail);
        }
    }
    json_free(txt);
    return json_bool(ok);
}

/* PoP: cc7_derive_trigger @ agent/context_compressor.py:_derive_trigger */
static json_t *cc7_derive_trigger(cc7_io_t *io, const json_t *in, json_t *st)
{
    const json_t *self = cc7_get(st, "self");
    const json_t *model = cc7_get(in, "model");
    const json_t *ctxlen = cc7_get(in, "context_length");
    const json_t *prov = cc7_get(in, "provider");

    json_t *a1 = json_object();
    json_set(a1, "model", model ? json_copy(model) : json_string(""));
    json_set(a1, "model_thresholds", cc7_get(self, "model_thresholds") ? json_copy(cc7_get(self, "model_thresholds")) : json_null());
    json_set(a1, "config_threshold_percent", cc7_get(self, "_config_threshold_percent") ? json_copy(cc7_get(self, "_config_threshold_percent")) : json_null());
    json_set(a1, "provider", prov ? json_copy(prov) : json_string(""));
    json_t *base = cc7_call(io, "resolve_model_threshold", a1);
    if (cc7_is_raise(base)) return base;

    json_t *a2 = json_object();
    json_set(a2, "context_length", ctxlen ? json_copy(ctxlen) : json_number(0));
    json_set(a2, "base_percent", json_copy(base));
    json_t *eff = cc7_call(io, "_effective_threshold_percent", a2);
    if (cc7_is_raise(eff)) { json_free(base); return eff; }

    json_t *a3 = json_object();
    json_set(a3, "context_length", ctxlen ? json_copy(ctxlen) : json_number(0));
    json_set(a3, "effective_percent", json_copy(eff));
    json_set(a3, "max_tokens", cc7_get(self, "max_tokens") ? json_copy(cc7_get(self, "max_tokens")) : json_null());
    json_t *thr = cc7_call(io, "_compute_threshold_tokens", a3);
    if (cc7_is_raise(thr)) { json_free(base); json_free(eff); return thr; }

    json_t *a4 = json_object();
    json_set(a4, "context_length", ctxlen ? json_copy(ctxlen) : json_number(0));
    json_t *cap = cc7_call(io, "_effective_threshold_cap", a4);
    if (cc7_is_raise(cap)) { json_free(base); json_free(eff); json_free(thr); return cap; }

    double threshold = cc7_num(thr);
    if (!cc7_is_null(cap) && cc7_num(cap) < threshold) threshold = cc7_num(cap);
    json_t *out = json_array();
    json_append(out, json_copy(base));
    json_append(out, json_copy(eff));
    json_append(out, json_number(threshold));
    json_free(base); json_free(eff); json_free(thr); json_free(cap);
    return out;
}

/* PoP: cc7_fallback_summary_for_window @ agent/context_compressor.py:_fallback_summary_for_window */
static json_t *cc7_fallback_summary_for_window(cc7_io_t *io, const json_t *in, json_t *st)
{
    json_t *self = (json_t *)cc7_get(st, "self");
    json_t *tel = (json_t *)cc7_get(st, "telemetry");
    int quiet = cc7_truth(cc7_get(self, "quiet_mode"));
    int skip = cc7_truth(cc7_get(in, "feasibility_skip"));

    if (!quiet && skip) {
        json_t *r = cc7_log(io, "_log_info", "Pre-LLM summary skip — inserting deterministic fallback context summary", json_array());
        if (cc7_is_raise(r)) return r;
        json_free(r);
    } else if (!quiet) {
        json_t *r = cc7_log(io, "_log_warning", "Summary generation failed — inserting deterministic fallback context summary", json_array());
        if (cc7_is_raise(r)) return r;
        json_free(r);
    }

    const json_t *nd = cc7_get(in, "n_dropped");
    if (cc7_is_obj(self)) {
        json_set(self, "_last_summary_dropped_count", nd ? json_copy(nd) : json_number(0));
        json_set(self, "_last_summary_fallback_used", json_bool(1));
    }
    if (cc7_is_obj(tel)) {
        json_set(tel, "fallback_used", json_bool(1));
        const char *fclass;
        if (cc7_truth(cc7_get(self, "_last_summary_overload_degraded"))) {
            fclass = "summary_overload_degraded";
        } else {
            const char *existing = cc7_str(cc7_get(tel, "failure_class"));
            if (existing && existing[0]) fclass = existing;
            else fclass = skip ? "feasibility_skip" : "summary_generation_failed";
        }
        json_set(tel, "failure_class", json_string(fclass));
    }

    json_t *a1 = json_object();
    json_set(a1, "turns", cc7_get(in, "turns_to_summarize") ? json_copy(cc7_get(in, "turns_to_summarize")) : json_null());
    const json_t *reason = skip ? NULL : cc7_get(self, "_last_summary_error");
    json_set(a1, "reason", reason ? json_copy(reason) : json_null());
    json_t *summary = cc7_call(io, "_build_static_fallback_summary", a1);
    if (cc7_is_raise(summary)) return summary;

    json_t *a2 = json_object();
    json_set(a2, "summary", json_copy(summary));
    json_t *stripped = cc7_call(io, "_strip_summary_prefix", a2);
    if (cc7_is_raise(stripped)) { json_free(summary); return stripped; }
    if (cc7_is_obj(self)) json_set(self, "_previous_summary", json_copy(stripped));
    json_free(stripped);
    return summary;
}
