/* context_compressor_flow6.c — sixth-wave FLOW port of
 * agent/context_compressor.py (LIVE authority, quarry READ-ONLY).
 *
 * Ports 18 UNCLAIMED module functions / methods (remainder beyond flow5):
 *   _is_real_user_turn, _real_user_indices_desc, _render_tool_call_for_summary,
 *   preview_threshold_tokens, _dedupe_tool_results, _prune_boundary,
 *   _demote_stale_tail_tools, _drop_blank_echoes, _assemble_head,
 *   _assemble_tail, _augment_summary_lean, _merge_summary_into_tail_row,
 *   _clear_terminal_summary_failures, maybe_seed_preflight_display_tokens,
 *   note_native_compaction_checkpoint, record_rejected_compaction,
 *   _apply_real_prompt_verdict, _abort_on_summary_failure.
 *
 * Every symbol is static cc6_*; libjson only. Claimed siblings / foreign
 * helpers / regex primitives enter through the cc6_io_t universal seam.
 *
 * FULL Python 3.14 builtin exception hierarchy (CC6_EXC_PARENT + cc6_exc_is_base):
 * a scripted {"__raise__":kind,...} seam result propagates past every site;
 * base-only kinds are classified identically to live `except Exception`.
 *
 * State model: mutable inputs named in MUTABLE_KEYS (self, msg, result,
 * telemetry) are seeded into `st` from `in` by the harness; ported methods
 * mutate `st` in place, and the harness emits `st`. The oracle mirrors with
 * deep-copied inputs and reads back their post-call projection.
 *
 * PoP: cc6_<name> @ agent/context_compressor.py:<fn>.
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow6 \
 *           oracle_context_compressor_flow6.py "" "lib/libjson/json.o"
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

#define CC6_PRUNE_MIN_CHARS 200
#define CC6_MAX_TAIL_FLOOR 8
#define CC6_DUP_MARKER "[Duplicate tool output — same content as a more recent call]"
#define CC6_END_MARKER "--- END OF CONTEXT SUMMARY — respond to the message below, not the summary above ---"
#define CC6_MERGED_DELIM "[END OF PRIOR CONTEXT — COMPACTION SUMMARY BELOW]"
#define CC6_MERGED_HEADER "[PRIOR CONTEXT — for reference only; not a new message]"
#define CC6_META_KEY "_compressed_summary"
#define CC6_META_USER_KEY "_compressed_summary_has_user_turn"
#define CC6_ANCHOR_HEADING "## Anchor Index (mechanically extracted, exact)"
#define CC6_USER_HEADING "## User Messages (verbatim, newest first)"
#define CC6_RECOVERY_HEADING "## Context Recovery"

/* terminal summary-failure flags (from _TERMINAL_SUMMARY_FAILURES) */
static const char *CC6_TERMINAL_FLAGS[] = {
    "_last_summary_auth_failure", "_last_summary_network_failure",
    "_last_summary_truncated_failure", "_last_summary_empty_content_failure",
    "_last_summary_overload_failure", NULL
};
static const char *CC6_TERMINAL_CLASSES[] = {
    "summary_auth_failure", "summary_network_failure",
    "summary_truncated_failure", "summary_empty_content_failure",
    "summary_overload_failure", NULL
};

/* ── universal seam ── */
typedef struct cc6_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cc6_io_t;

/* ── accessors / truthiness ── */
static int cc6_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cc6_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cc6_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cc6_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cc6_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cc6_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cc6_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cc6_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static const json_t *cc6_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cc6_num(const json_t *v) { return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0; }
static long long cc6_int(const json_t *v) { return (long long)cc6_num(v); }

static int cc6_idx_in(const json_t *arr, long long v)
{
    if (!cc6_is_arr(arr)) return 0;
    for (size_t i = 0; i < arr->c.count; i++)
        if (cc6_int(arr->c.items[i]) == v) return 1;
    return 0;
}

/* ── formatting helpers ── */
static char *cc6_xfmt(const char *fmt, ...)
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

static json_t *cc6_s(char *buf) { json_t *j = json_string(buf ? buf : ""); free(buf); return j; }

/* Python str() of a JSON scalar (containers outside fixture domain). */
static char *cc6_py_str(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cc6_xfmt("None");
    if (v->type == JSON_BOOL) return cc6_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18) return cc6_xfmt("%lld", (long long)d);
        return cc6_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cc6_xfmt("%s", v->str_val ? v->str_val : "");
    return cc6_xfmt("");
}

static char *cc6_cat(const char *a, const char *b)
{
    if (!a) a = ""; if (!b) b = "";
    return cc6_xfmt("%s%s", a, b);
}

/* ── raise specs ── */
static json_t *cc6_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static int cc6_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* ── FULL builtin exception hierarchy (Python 3.14) ── */
typedef struct { const char *name; const char *parent; } cc6_exc_edge_t;
static const cc6_exc_edge_t CC6_EXC_PARENT[] = {
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

static const char *cc6_exc_parent(const char *kind)
{
    if (!kind || !*kind) return NULL;
    for (const cc6_exc_edge_t *e = CC6_EXC_PARENT; e->name; e++)
        if (strcmp(kind, e->name) == 0) return e->parent;
    return NULL;
}

static int cc6_exc_is_base(const char *kind)
{
    if (!kind || !*kind) return 0;
    const char *cur = kind;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, "Exception") == 0 || strcmp(cur, "ExceptionGroup") == 0) return 0;
        if (strcmp(cur, "BaseException") == 0) return 1;
        const char *p = cc6_exc_parent(cur);
        if (!p) return 0;
        cur = p;
    }
    return 0;
}

/* ── universal seam call (frees args; returns owned result) ── */
static json_t *cc6_call(cc6_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cc6_xfmt("no seam installed for %s", name);
        r = cc6_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* ═══════════════════════ ported functions ═══════════════════════ */

/* PoP: cc6_is_real_user_turn @ agent/context_compressor.py:_is_real_user_turn */
static json_t *cc6_is_real_user_turn(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = json_object();
    json_set(a, "message", cc6_is_null(cc6_get(in, "message")) ? json_null() : json_copy(cc6_get(in, "message")));
    return cc6_call(io, "_turns_is_real_user_turn", a);
}

/* PoP: cc6_real_user_indices_desc @ agent/context_compressor.py:_real_user_indices_desc */
static json_t *cc6_real_user_indices_desc(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *msgs = cc6_get(in, "messages");
    long long head = cc6_int(cc6_get(in, "head_end"));
    json_t *out = json_array();
    if (!cc6_is_arr(msgs)) return out;
    for (long long i = (long long)msgs->c.count - 1; i >= head; i--) {
        json_t *a = json_object();
        json_set(a, "message", json_copy(msgs->c.items[i]));
        json_t *r = cc6_call(io, "_turns_is_real_user_turn", a);
        if (cc6_is_raise(r)) { json_free(out); return r; }
        if (cc6_truth(r)) json_append(out, json_number((double)i));
        json_free(r);
    }
    return out;
}

/* PoP: cc6_preview_threshold_tokens @ agent/context_compressor.py:preview_threshold_tokens */
static json_t *cc6_preview_threshold_tokens(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = json_object();
    json_set(a, "model", json_string(cc6_str(cc6_get(in, "model")) ? cc6_str(cc6_get(in, "model")) : ""));
    json_set(a, "context_length", json_copy(cc6_get(in, "context_length") ? cc6_get(in, "context_length") : json_null()));
    json_set(a, "provider", json_string(cc6_str(cc6_get(in, "provider")) ? cc6_str(cc6_get(in, "provider")) : ""));
    json_t *r = cc6_call(io, "_derive_trigger", a);
    if (cc6_is_raise(r)) return r;
    if (cc6_is_arr(r) && r->c.count >= 3) { json_t *v = json_copy(r->c.items[2]); json_free(r); return v; }
    json_free(r);
    return cc6_mkraise("IndexError", "list index out of range");
}

/* PoP: cc6_render_tool_call_for_summary @ agent/context_compressor.py:_render_tool_call_for_summary */
static json_t *cc6_render_tool_call_for_summary(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *tc = cc6_get(in, "tc");
    if (!cc6_is_obj(tc))
        return json_string("  ?(...)");
    const json_t *fn = cc6_get(tc, "function");
    char *namebuf = cc6_xfmt("?");
    if (cc6_is_obj(fn)) {
        const json_t *nv = cc6_get(fn, "name");
        if (nv) { free(namebuf); namebuf = cc6_py_str(nv); }
    }
    const json_t *av = cc6_is_obj(fn) ? cc6_get(fn, "arguments") : NULL;
    json_t *a = json_object();
    json_set(a, "arguments", av ? json_copy(av) : json_string(""));
    json_t *r = cc6_call(io, "_render_tool_args_for_summary", a);
    if (cc6_is_raise(r)) { free(namebuf); return r; }
    char *rv = cc6_xfmt("  %s(%s)", namebuf, cc6_str(r) ? cc6_str(r) : "");
    free(namebuf);
    json_free(r);
    return cc6_s(rv);
}

/* PoP: cc6_drop_blank_echoes @ agent/context_compressor.py:_drop_blank_echoes */
static json_t *cc6_drop_blank_echoes(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *msgs = cc6_get(in, "messages");
    json_t *a1 = json_object();
    json_set(a1, "messages", cc6_is_null(msgs) ? json_null() : json_copy(msgs));
    json_set(a1, "start", json_number(0));
    json_t *idx = cc6_call(io, "_find_last_user_message_idx", a1);
    if (cc6_is_raise(idx)) return idx;
    json_t *a2 = json_object();
    json_set(a2, "messages", cc6_is_null(msgs) ? json_null() : json_copy(msgs));
    json_set(a2, "idx", json_copy(idx));
    json_free(idx);
    json_t *blank = cc6_call(io, "_blank_echo_indices_after", a2);
    if (cc6_is_raise(blank)) return blank;
    json_t *out = json_array();
    if (!cc6_truth(blank) || !cc6_is_arr(msgs)) {
        json_free(blank);
        if (msgs) for (size_t i = 0; i < msgs->c.count; i++) json_append(out, json_copy(msgs->c.items[i]));
        return out;
    }
    for (size_t i = 0; i < msgs->c.count; i++)
        if (!cc6_idx_in(blank, (long long)i)) json_append(out, json_copy(msgs->c.items[i]));
    json_free(blank);
    return out;
}

/* PoP: cc6_prune_boundary @ agent/context_compressor.py:_prune_boundary */
static json_t *cc6_prune_boundary(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *result = cc6_get(in, "result");
    long long n = cc6_is_arr(result) ? (long long)result->c.count : 0;
    long long cnt = cc6_int(cc6_get(in, "protect_tail_count"));
    const json_t *tok = cc6_get(in, "protect_tail_tokens");
    if (cc6_is_null(tok) || !cc6_is_num(tok) || cc6_num(tok) <= 0)
        return json_number((double)(n - cnt));
    long long mp = cnt;
    if (mp > n) mp = n;
    if (mp > CC6_MAX_TAIL_FLOOR) mp = CC6_MAX_TAIL_FLOOR;
    json_t *a = json_object();
    json_set(a, "result", cc6_is_null(result) ? json_null() : json_copy(result));
    json_set(a, "start", json_number(0));
    json_set(a, "tokens", json_copy(tok));
    json_set(a, "min_protect", json_number((double)mp));
    json_set(a, "cut_at_break", json_bool(1));
    json_t *r = cc6_call(io, "_walk_tail_budget", a);
    if (cc6_is_raise(r)) return r;
    long long boundary = (cc6_is_arr(r) && r->c.count >= 1) ? cc6_int(r->c.items[0]) : 0;
    json_free(r);
    long long cap = n - mp;
    return json_number((double)(boundary < cap ? boundary : cap));
}

/* PoP: cc6_demote_stale_tail_tools @ agent/context_compressor.py:_demote_stale_tail_tools */
static json_t *cc6_demote_stale_tail_tools(cc6_io_t *io, const json_t *in, json_t *st)
{
    const json_t *self = cc6_get(st, "self");
    const json_t *sidv = cc6_get(self, "_session_id");
    const char *sid = (cc6_is_str(sidv) && cc6_str(sidv)) ? cc6_str(sidv) : "";
    json_t *a = json_object();
    const json_t *msgs = cc6_get(in, "messages");
    json_set(a, "messages", cc6_is_null(msgs) ? json_null() : json_copy(msgs));
    json_set(a, "tail_start", json_copy(cc6_get(in, "tail_start") ? cc6_get(in, "tail_start") : json_null()));
    json_set(a, "session_id", json_string(sid));
    json_t *r = cc6_call(io, "demote_stale_tail_tools", a);
    if (cc6_is_raise(r)) return r;
    if (cc6_is_arr(r) && r->c.count >= 1) { json_t *v = json_copy(r->c.items[0]); json_free(r); return v; }
    return r;
}

/* PoP: cc6_clear_terminal_summary_failures @ agent/context_compressor.py:_clear_terminal_summary_failures */
static json_t *cc6_clear_terminal_summary_failures(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *self = (json_t *)cc6_get(st, "self");
    if (cc6_is_obj(self))
        for (int i = 0; CC6_TERMINAL_FLAGS[i]; i++)
            json_set(self, CC6_TERMINAL_FLAGS[i], json_bool(0));
    return json_null();
}

/* PoP: cc6_maybe_seed_preflight_display_tokens @ agent/context_compressor.py:maybe_seed_preflight_display_tokens */
static json_t *cc6_maybe_seed_preflight_display_tokens(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *self = (json_t *)cc6_get(st, "self");
    long long pt = cc6_int(cc6_get(in, "preflight_tokens"));
    if (cc6_is_obj(self) && cc6_int(cc6_get(self, "last_prompt_tokens")) == 0 && pt > 0)
        json_set(self, "last_prompt_tokens", json_number((double)pt));
    return json_null();
}

/* PoP: cc6_note_native_compaction_checkpoint @ agent/context_compressor.py:note_native_compaction_checkpoint */
static json_t *cc6_note_native_compaction_checkpoint(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *self = (json_t *)cc6_get(st, "self");
    if (cc6_is_obj(self)) {
        json_set(self, "awaiting_real_usage_after_compression", json_bool(1));
        json_set(self, "last_compression_rough_tokens", json_number(0));
    }
    return json_null();
}

/* PoP: cc6_record_rejected_compaction @ agent/context_compressor.py:record_rejected_compaction */
static json_t *cc6_record_rejected_compaction(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    const json_t *self = cc6_get(st, "self");
    long long cnt = cc6_int(cc6_get(self, "_ineffective_compression_count")) + 1;
    json_t *a = json_object();
    json_set(a, "count", json_number((double)cnt));
    json_t *r = cc6_call(io, "_record_ineffective_compression_verdict", a);
    if (cc6_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: cc6_apply_real_prompt_verdict @ agent/context_compressor.py:_apply_real_prompt_verdict */
static json_t *cc6_apply_real_prompt_verdict(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *self = (json_t *)cc6_get(st, "self");
    const json_t *lptv = cc6_get(self, "last_prompt_tokens");
    long long lpt = cc6_int(lptv);
    if (lpt > 0) {
        json_set(self, "last_real_prompt_tokens", json_copy(lptv));
        json_set(self, "_provider_omits_usage", json_bool(0));
        double thr = cc6_num(cc6_get(self, "threshold_tokens"));
        if (lpt < thr) {
            json_t *a = json_object(); json_set(a, "count", json_number(0));
            json_t *r = cc6_call(io, "_record_ineffective_compression_verdict", a);
            if (cc6_is_raise(r)) return r; json_free(r);
        }
        if (cc6_truth(cc6_get(self, "_verify_compaction_cleared_threshold"))) {
            long long cnt = 0;
            if (lpt >= thr) { cnt = cc6_int(cc6_get(self, "_ineffective_compression_count")) + 1; }
            json_t *a = json_object(); json_set(a, "count", json_number((double)cnt));
            json_t *r = cc6_call(io, "_record_ineffective_compression_verdict", a);
            if (cc6_is_raise(r)) return r; json_free(r);
        }
    }
    return json_null();
}

/* PoP: cc6_abort_on_summary_failure @ agent/context_compressor.py:_abort_on_summary_failure */
static json_t *cc6_abort_on_summary_failure(cc6_io_t *io, const json_t *in, json_t *st)
{
    json_t *self = (json_t *)cc6_get(st, "self");
    /* first truthy terminal flag -> (class, message) */
    const char *fclass = NULL, *fmsg = NULL;
    int terminal_i = -1;
    for (int i = 0; CC6_TERMINAL_FLAGS[i]; i++)
        if (cc6_truth(cc6_get(self, CC6_TERMINAL_FLAGS[i]))) { terminal_i = i; break; }
    int terminal = terminal_i >= 0;
    if (!terminal && !cc6_truth(cc6_get(self, "abort_on_summary_failure")))
        return json_bool(0);
    json_set(self, "_last_summary_dropped_count", json_number(0));
    json_set(self, "_last_summary_fallback_used", json_bool(0));
    json_set(self, "_last_compress_aborted", json_bool(1));
    if (terminal) {
        fclass = CC6_TERMINAL_CLASSES[terminal_i];
    } else {
        fclass = "summary_generation_aborted";
        fmsg = "Summary generation failed — aborting compression (compression.abort_on_summary_failure=true). %d message(s) preserved unchanged. Conversation is frozen until the next /compress or /new.";
    }
    json_t *tele = (json_t *)cc6_get(st, "telemetry");
    if (cc6_is_obj(tele)) json_set(tele, "failure_class", json_string(fclass));
    json_t *a = json_object();
    json_set(a, "self", cc6_is_obj(self) ? json_copy(self) : json_null());
    json_t *cur = cc6_call(io, "_caller_attempt_is_current", a);
    if (cc6_is_raise(cur)) return cur;
    if (cc6_truth(cur)) {
        const json_t *prev = cc6_get(in, "previous_summary_before_scan");
        json_set(self, "_previous_summary", prev ? json_copy(prev) : json_null());
    }
    json_free(cur);
    (void)fmsg; /* logger no-op */
    return json_bool(1);
}

/* PoP: cc6_assemble_head @ agent/context_compressor.py:_assemble_head */
static json_t *cc6_assemble_head(cc6_io_t *io, const json_t *in, json_t *st)
{
    const json_t *msgs = cc6_get(in, "messages");
    long long cs = cc6_int(cc6_get(in, "compress_start"));
    const char *note = cc6_str(cc6_get(cc6_get(st, "self"), "_COMPRESSION_NOTE"));
    if (!note) note = "";
    json_t *compressed = json_array();
    if (!cc6_is_arr(msgs)) return compressed;
    for (long long i = 0; i < cs && (size_t)i < msgs->c.count; i++) {
        json_t *a0 = json_object(); json_set(a0, "msg", json_copy(msgs->c.items[i]));
        json_t *msg = cc6_call(io, "_fresh_compaction_message_copy", a0);
        if (cc6_is_raise(msg)) { json_free(compressed); return msg; }
        if (i == 0 && cc6_is_obj(msg)) {
            const char *role = cc6_str(cc6_get(msg, "role"));
            if (role && strcmp(role, "system") == 0) {
                const json_t *existing = cc6_get(msg, "content");
                json_t *a1 = json_object();
                json_set(a1, "content", cc6_is_null(existing) ? json_null() : json_copy(existing));
                json_t *txt = cc6_call(io, "_content_text_for_contains", a1);
                if (cc6_is_raise(txt)) { json_free(msg); json_free(compressed); return txt; }
                const char *ts = cc6_str(txt);
                if (!ts || strstr(ts, note) == NULL) {
                    const char *sep = (cc6_is_str(existing) && cc6_str(existing) && cc6_str(existing)[0]) ? "\n\n" : "";
                    char *addn = cc6_cat(sep, note);
                    json_t *a2 = json_object();
                    json_set(a2, "content", cc6_is_null(existing) ? json_null() : json_copy(existing));
                    json_set(a2, "addition", json_string(addn));
                    json_set(a2, "prepend", json_bool(0));
                    free(addn);
                    json_t *added = cc6_call(io, "_append_text_to_content", a2);
                    if (cc6_is_raise(added)) { json_free(txt); json_free(msg); json_free(compressed); return added; }
                    json_set(msg, "content", json_copy(added));
                    json_free(added);
                }
                json_free(txt);
            }
        }
        json_t *a3 = json_object(); json_set(a3, "msg", json_copy(msg));
        json_t *stripped = cc6_call(io, "_strip_context_summary_handoff_message", a3);
        if (cc6_is_raise(stripped)) { json_free(msg); json_free(compressed); return stripped; }
        if (!cc6_is_null(stripped)) json_append(compressed, stripped);
        else json_free(stripped);
        json_free(msg);
    }
    return compressed;
}

/* PoP: cc6_assemble_tail @ agent/context_compressor.py:_assemble_tail */
static json_t *cc6_assemble_tail(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *msgs = cc6_get(in, "messages");
    long long ce = cc6_int(cc6_get(in, "compress_end"));
    long long ts = cc6_int(cc6_get(in, "tail_start"));
    const json_t *sidx = cc6_get(in, "summary_indices");
    json_t *tail = json_array();
    if (!cc6_is_arr(msgs)) return tail;
    long long start = ce > ts ? ce : ts;
    for (long long i = start; (size_t)i < msgs->c.count; i++) {
        if (cc6_idx_in(sidx, i) && i >= ts) continue;
        json_t *a0 = json_object(); json_set(a0, "msg", json_copy(msgs->c.items[i]));
        json_t *msg = cc6_call(io, "_fresh_compaction_message_copy", a0);
        if (cc6_is_raise(msg)) { json_free(tail); return msg; }
        json_t *a1 = json_object(); json_set(a1, "msg", json_copy(msg));
        json_t *stripped = cc6_call(io, "_strip_context_summary_handoff_message", a1);
        if (cc6_is_raise(stripped)) { json_free(msg); json_free(tail); return stripped; }
        if (!cc6_is_null(stripped)) json_append(tail, stripped);
        else json_free(stripped);
        json_free(msg);
    }
    return tail;
}

/* PoP: cc6_dedupe_tool_results @ agent/context_compressor.py:_dedupe_tool_results */
static json_t *cc6_dedupe_tool_results(cc6_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *result = (json_t *)cc6_get(st, "result");
    if (!cc6_is_arr(result)) return json_number(0);
    json_t *hashes = json_array();
    long long pruned = 0;
    for (long long i = (long long)result->c.count - 1; i >= 0; i--) {
        json_t *msg = result->c.items[i];
        if (!cc6_is_obj(msg)) continue;
        const char *role = cc6_str(cc6_get(msg, "role"));
        const json_t *cv = cc6_get(msg, "content");
        const char *content = (cc6_is_str(cv) && cc6_str(cv) && cc6_str(cv)[0]) ? cc6_str(cv) : NULL;
        if (!role || strcmp(role, "tool") != 0 || !content || strlen(content) < CC6_PRUNE_MIN_CHARS) continue;
        json_t *a = json_object(); json_set(a, "text", json_string(content));
        json_t *h = cc6_call(io, "md5hex", a);
        if (cc6_is_raise(h)) { json_free(hashes); return h; }
        const char *hs = cc6_str(h) ? cc6_str(h) : "";
        char short_h[13];
        size_t hl = strlen(hs); if (hl > 12) hl = 12;
        memcpy(short_h, hs, hl); short_h[hl] = '\0';
        json_free(h);
        int seen = 0;
        for (size_t k = 0; k < hashes->c.count; k++) {
            const char *e = cc6_str(hashes->c.items[k]);
            if (e && strcmp(e, short_h) == 0) { seen = 1; break; }
        }
        if (seen) {
            json_t *nm = json_copy(msg);
            json_set(nm, "content", json_string(CC6_DUP_MARKER));
            json_array_set(result, (size_t)i, nm);
            pruned++;
        }
        json_append(hashes, json_string(short_h));
    }
    json_free(hashes);
    return json_number((double)pruned);
}

/* PoP: cc6_augment_summary_lean @ agent/context_compressor.py:_augment_summary_lean */
static json_t *cc6_augment_summary_lean(cc6_io_t *io, const json_t *in, json_t *st)
{
    const json_t *self = cc6_get(st, "self");
    const char *tm = cc6_str(cc6_get(self, "tail_mode"));
    const char *summary = cc6_str(cc6_get(in, "summary"));
    if (!tm || strcmp(tm, "lean") != 0) return json_string(summary ? summary : "");
    char *cur = cc6_xfmt("%s", summary ? summary : "");
    const json_t *turns = cc6_get(in, "turns_to_summarize");
    /* anchor */
    if (!strstr(cur, CC6_ANCHOR_HEADING)) {
        json_t *a0 = json_object(); json_set(a0, "turns", cc6_is_null(turns) ? json_null() : json_copy(turns));
        json_t *built = cc6_call(io, "_build_anchor_index", a0);
        if (cc6_is_raise(built)) { free(cur); return built; }
        json_t *a1 = json_object(); json_set(a1, "text", json_copy(built)); json_free(built);
        json_t *red = cc6_call(io, "_redact_compaction_text", a1);
        if (cc6_is_raise(red)) { free(cur); return red; }
        char *nxt = cc6_cat(cur, cc6_str(red) ? cc6_str(red) : ""); free(cur); json_free(red); cur = nxt;
    }
    /* user section */
    if (!strstr(cur, CC6_USER_HEADING)) {
        json_t *a0 = json_object(); json_set(a0, "turns", cc6_is_null(turns) ? json_null() : json_copy(turns));
        json_t *built = cc6_call(io, "_build_verbatim_user_section", a0);
        if (cc6_is_raise(built)) { free(cur); return built; }
        json_t *a1 = json_object(); json_set(a1, "text", json_copy(built)); json_free(built);
        json_t *red = cc6_call(io, "_redact_compaction_text", a1);
        if (cc6_is_raise(red)) { free(cur); return red; }
        char *nxt = cc6_cat(cur, cc6_str(red) ? cc6_str(red) : ""); free(cur); json_free(red); cur = nxt;
    }
    /* recovery footer */
    if (!strstr(cur, CC6_RECOVERY_HEADING)) {
        const json_t *sidv = cc6_get(self, "_session_id");
        const char *sid = (cc6_is_str(sidv) && cc6_str(sidv)) ? cc6_str(sidv) : "";
        json_t *a0 = json_object();
        json_set(a0, "session_id", json_string(sid));
        json_set(a0, "n", json_number((double)(cc6_is_arr(turns) ? turns->c.count : 0)));
        json_t *built = cc6_call(io, "_build_recovery_footer", a0);
        if (cc6_is_raise(built)) { free(cur); return built; }
        char *nxt = cc6_cat(cur, cc6_str(built) ? cc6_str(built) : ""); free(cur); json_free(built); cur = nxt;
    }
    return cc6_s(cur);
}

/* PoP: cc6_merge_summary_into_tail_row @ agent/context_compressor.py:_merge_summary_into_tail_row */
static json_t *cc6_merge_summary_into_tail_row(cc6_io_t *io, const json_t *in, json_t *st)
{
    json_t *msg = (json_t *)cc6_get(st, "msg");
    const json_t *self = cc6_get(st, "self");
    const json_t *oldv = cc6_get(msg, "content");
    const char *old = cc6_str(oldv) ? cc6_str(oldv) : "";
    const char *summary = cc6_str(cc6_get(in, "summary")) ? cc6_str(cc6_get(in, "summary")) : "";
    const char *role = cc6_str(cc6_get(in, "summary_role")) ? cc6_str(cc6_get(in, "summary_role")) : "";
    int force = cc6_truth(cc6_get(in, "force_user_leading"));
    json_t *added;
    if (force && strcmp(role, "user") == 0) {
        char *addn = cc6_xfmt("%s\n\n%s\n\n", summary, CC6_END_MARKER);
        json_t *a = json_object();
        json_set(a, "content", json_string(old));
        json_set(a, "addition", json_string(addn));
        json_set(a, "prepend", json_bool(1));
        free(addn);
        added = cc6_call(io, "_append_text_to_content", a);
        if (cc6_is_raise(added)) return added;
        json_set(msg, "content", json_copy(added));
        json_free(added);
    } else {
        char *suffix = cc6_xfmt("\n\n%s\n\n%s\n\n%s", CC6_MERGED_DELIM, summary, CC6_END_MARKER);
        json_t *a1 = json_object();
        json_set(a1, "content", json_string(old));
        json_set(a1, "addition", json_string(suffix));
        json_set(a1, "prepend", json_bool(0));
        free(suffix);
        json_t *r1 = cc6_call(io, "_append_text_to_content", a1);
        if (cc6_is_raise(r1)) return r1;
        char *hdr = cc6_xfmt("%s\n", CC6_MERGED_HEADER);
        json_t *a2 = json_object();
        json_set(a2, "content", json_copy(r1));
        json_set(a2, "addition", json_string(hdr));
        json_set(a2, "prepend", json_bool(1));
        free(hdr); json_free(r1);
        added = cc6_call(io, "_append_text_to_content", a2);
        if (cc6_is_raise(added)) return added;
        json_set(msg, "content", json_copy(added));
        json_free(added);
    }
    json_set(msg, CC6_META_KEY, json_bool(1));
    json_set(msg, CC6_META_USER_KEY, json_bool(cc6_truth(cc6_get(self, "_summary_has_user_turn"))));
    json_t *a3 = json_object(); json_set(a3, "msg", json_copy(msg));
    json_t *d = cc6_call(io, "drop_stale_api_content", a3);
    if (cc6_is_raise(d)) return d;
    json_free(d);
    return json_null();
}
