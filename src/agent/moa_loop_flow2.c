/*
 * moa_loop_flow2.c — FLOW + leaves port of the UNCLAIMED remainder of
 * agent/moa_loop.py (quarry: C:/Users/eman5/hermes-agent, LIVE authority).
 *
 * Claim landscape (grep PoP by PYTHON name first):
 *   - moa_loop_flow.c (flow1) faithfully ports 18 fan-out/aggregation cores;
 *     those claimed siblings are SEAMED here (only _agent_cache_opts is
 *     reached by these flows).
 *   - 12 defs are unclaimed anywhere in src/ + include/: _redact_message_content,
 *     _resolve_preset_cached, _reference_liveness_hook (+ nested _touch_on_progress),
 *     _touch_fanout_progress, _slot_request_headers, collect (nested in
 *     _run_references_parallel), MoAChatCompletions._plan_aggregator_cache,
 *     MoAClient.last_aggregator_route / last_aggregator_attempt (properties),
 *     _moa_reference_relay (nested in build_moa_facade), bind_moa_runtime.
 *   - 8 more are claimed ONLY by printf-stub era fakes (moa_loop_wrappers.c /
 *     moa_loop_ports.c: no live behaviour, never gated — see the skill's
 *     stub-port-audit / stub-purge doctrine: a real port of the same
 *     (module,func) pair is the sanctioned revival that lets the stub die):
 *     _redact_reference_text, _redact_reference_outputs, _redact_trace_messages,
 *     _redact_trace_accounting, _moa_privacy_mode, _slot_label,
 *     _attach_reference_guidance, peel_reference_guidance (the old port's
 *     suffix-peel shapes do not exist in the live def).
 *   20 targets total. Ported functions compose REAL (internal composition);
 *   every claimed sibling / foreign-module import / callable double rides the
 *   injected universal seam moa2_io_t.call.
 *
 * ── Universal seam contract ────────────────────────────────────────────────
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 * args is OWNED by moa2_call (freed after the call); the result is a NEW
 * json_t owned by the caller: NULL or a JSON_NULL node == Python None; an
 * object carrying a string "__raise__" key ({"__raise__": cls, "msg": str})
 * == an exception of that class with str(exc) == msg, propagated or caught
 * exactly where the live code does (BaseException-only kinds pass through
 * every `except Exception` / suppress(Exception) site). The harness records
 * one {ev, args, ret} event per call (ret = RAW scripted value) in call order.
 *
 * Script shapes (fixture "io" object, per seam name):
 *   absent / null        -> None on every call
 *   {"seq": [v1, v2..]}  -> vi on the i-th call (0-based); None when exhausted
 *   any other value      -> reused on every call
 *
 * ── Seam name table (name -> live call) ────────────────────────────────────
 *   redact_sensitive_text            agent.redact.redact_sensitive_text
 *   coerce_privacy_filter            hermes_cli.moa_config.coerce_privacy_filter
 *   worker_config_snapshot           agent.safe_worker_policy.worker_config_snapshot
 *   get_config_path                  hermes_cli.config.get_config_path
 *   config_cache_version_or_none     hermes_cli.config_backend.config_cache_version_or_none
 *   load_config                      hermes_cli.config.load_config
 *   resolve_moa_preset               hermes_cli.moa_config.resolve_moa_preset
 *   _normalize_aux_provider          agent.auxiliary_client._normalize_aux_provider
 *   get_provider_profile             providers.get_provider_profile
 *   route_answer                     agent.provider_route_hooks.route_answer
 *   touch                            agent._touch_activity callable double
 *   progress_callback                collect()'s progress_callback callable double
 *   future.result                    concurrent.futures.Future.result (idx arg)
 *   _agent_cache_opts                flow1-claimed module sibling
 *   plan_cache_sections_for_destination  agent.agent_runtime_helpers
 *   build_moa_facade                 stub-claimed module sibling
 *   tool_progress_callback           agent.tool_progress_callback callable double
 *
 * ── Value model ────────────────────────────────────────────────────────────
 * None            -> JSON null (pointer NULL and JSON_NULL node both read None).
 * attribute object-> {"__attr__": {name: value}} (SimpleNamespace double); a
 *                    plain JSON object is a dict. isinstance(x, dict) is true
 *                    ONLY for plain objects (no __attr__ tag).
 * callable double -> {"__call__": "<seam name>"}; callable() is true only for
 *                    that marker. Calling it fires the named seam.
 * closure         -> _reference_liveness_hook returns
 *                    {"__closure__": "_touch_on_progress", "touch": v, "label": s}
 *                    (the captured cells; the oracle projects the live closure
 *                    to the same descriptor).
 * _RefAccounting  -> {"_type": "_RefAccounting", usage, cost_usd, cost_status,
 *                    cost_source, messages, output, model, provider, temperature}
 *                    (full field set in dataclass order; replace() semantics).
 * tuple           -> JSON array. reference output -> [label, text, acct].
 * preset cache key-> compact JSON of [cfg_stamp, preset_name] (json.dumps
 *                    separators=(",",":") on the PY side; ASCII domain).
 *
 * ── State window (borrowed mutable "state" object) ─────────────────────────
 *   _resolve_preset_cached : preset_cache (key -> preset; cleared+replaced on
 *                            a stamp-bearing miss). The threading.Lock is
 *                            unobservable single-threaded (no seam).
 *   collect                : results (list), completed (counter).
 *   _attach_reference_guidance : agg_messages (the mutated list; the harness
 *                            seeds it from in.agg_messages, input echo pristine).
 *   bind_moa_runtime       : agent (the mutated attribute double; seeded from
 *                            in.agent).
 *   _plan_aggregator_cache : the facade self window (_agent, ...); read-only.
 *   others                 : {} (pure / seam-only).
 *
 * ── Fixture domain (ASCII scope) ───────────────────────────────────────────
 * str.strip()/regexes model ASCII whitespace ([ \t\n\r\v\f\x1c-\x1f]) and ASCII
 * \w/\b; str() operands are scalars (containers would need Python repr —
 * excluded); unpack elements are lists (str/dict elements iterate chars/keys
 * live — excluded); preset-cache stamps are null|str|int; integral floats
 * normalize to ints on both sides; the logger is SILENT (flow1 precedent: no
 * logger seam events; none of the ported logger calls has seam-valued args).
 *
 * Self-contained: libjson only (gate links lib/libjson/json.o). The harness
 * #includes "agent/moa_loop_flow2.c" directly. All symbols static moa2_*.
 */
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>

/* ── JSON / Python-value helpers ─────────────────────────────────────────── */

static bool moa2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool moa2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool moa2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool moa2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool moa2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool moa2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

/* isinstance(v, dict): plain object only (an __attr__ double is NOT a dict). */
static bool moa2_is_dict(const json_t *v) {
    return moa2_is_obj(v) && !json_obj_get(v, "__attr__");
}
/* The attribute window of an attribute double, or NULL. */
static const json_t *moa2_attr(const json_t *v) {
    if (!moa2_is_obj(v)) return NULL;
    const json_t *a = json_obj_get(v, "__attr__");
    return moa2_is_obj(a) ? a : NULL;
}
static bool moa2_is_callable(const json_t *v) {
    return moa2_is_obj(v) && moa2_is_str(json_obj_get(v, "__call__"));
}
static bool moa2_is_acct(const json_t *v) {
    return moa2_is_obj(v) && moa2_is_str(json_obj_get(v, "_type"))
        && strcmp(json_obj_get(v, "_type")->str_val, "_RefAccounting") == 0;
}

static bool moa2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* Python truthiness (NULL pointer == None). */
static bool moa2_truth(const json_t *v) {
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

/* Python type name of a JSON value (ASCII scalar domain). */
static const char *moa2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return moa2_attr(v) ? "types.SimpleNamespace" : "dict";
    }
    return "object";
}

static void moa2_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() of a scalar (None/bool/str/number). malloc'd. */
static char *moa2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                moa2_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: {
            /* Outside the scalar str() domain: serialize (documented exclusion
             * for containers; kept fail-safe rather than crashing). */
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}

/* str(x or "") — empty string when x is falsy. malloc'd. */
static char *moa2_pystr_or_empty(const json_t *v) {
    if (!moa2_truth(v)) return strdup("");
    return moa2_pystr(v);
}

/* str.strip() ASCII whitespace set: space, \t-\r, \x1c-\x1f. malloc'd. */
static char *moa2_strip(const char *s) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) { p++; continue; }
        break;
    }
    const char *end = p + strlen(p);
    while (end > p) {
        unsigned char c = (unsigned char)end[-1];
        if (c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) { end--; continue; }
        break;
    }
    size_t n = (size_t)(end - p);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* ── dynamic string buffer ── */
typedef struct { char *p; size_t n, cap; } moa2_buf_t;
static void moa2_buf_init(moa2_buf_t *b) { b->p = NULL; b->n = 0; b->cap = 0; }
static void moa2_buf_add(moa2_buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}
static void moa2_buf_adds(moa2_buf_t *b, const char *s) { moa2_buf_add(b, s, strlen(s)); }

/* ── exception model ─────────────────────────────────────────────────────── */

/* Park a NEW raise spec {"__raise__": cls, "msg": msg} into *exc. */
static void moa2_raise(json_t **exc, const char *cls, const char *msg) {
    if (*exc) return;   /* first raise wins */
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    *exc = r;
}
static void moa2_raise_fmt(json_t **exc, const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    moa2_raise(exc, cls, buf);
}

/* AttributeError: '<T>' object has no attribute '<name>' */
static void moa2_attr_err(json_t **exc, const json_t *carrier, const char *name) {
    moa2_raise_fmt(exc, "AttributeError", "'%s' object has no attribute '%s'",
                   moa2_type_name(carrier), name);
}

/* FULL builtin exception hierarchy (py3.14): name -> parent. A scripted kind
 * walks this table; reaching Exception == caught by `except Exception`,
 * reaching BaseException without Exception == propagates. Unknown names are
 * synthetic Exception subclasses (the oracle's make_exc builds them so). */
typedef struct { const char *name; const char *parent; } moa2_exc_parent_t;
static const moa2_exc_parent_t MOA2_EXC_PARENTS[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},          /* asyncio.CancelledError (py3.8+) */
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"TypeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
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
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
};

static bool moa2_exc_is_exception(const char *cls) {
    if (!cls) return true;
    for (int depth = 0; depth < 16; depth++) {
        if (strcmp(cls, "Exception") == 0) return true;
        if (strcmp(cls, "BaseException") == 0) return false;
        const char *parent = NULL;
        bool found = false;
        for (size_t i = 0; i < sizeof(MOA2_EXC_PARENTS) / sizeof(MOA2_EXC_PARENTS[0]); i++) {
            if (strcmp(MOA2_EXC_PARENTS[i].name, cls) == 0) {
                parent = MOA2_EXC_PARENTS[i].parent;
                found = true;
                break;
            }
        }
        if (!found) return true;   /* synthetic type(cls, (Exception,), {}) */
        if (!parent) return false; /* BaseException root */
        cls = parent;
    }
    return true;
}

/* Swallow a caught exception (live `except Exception` / suppress(Exception)).
 * BaseException-only kinds propagate: returns false, leaves *exc set. */
static bool moa2_swallow(json_t **exc) {
    if (!*exc) return true;
    const char *cls = json_get_str(*exc, "__raise__", "");
    if (!moa2_exc_is_exception(cls)) return false;
    json_free(*exc); *exc = NULL;
    return true;
}

/* ── universal seam ──────────────────────────────────────────────────────── */

typedef struct moa2_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} moa2_io_t;

/* args builders (None-valued keys dropped on both sides). */
static void moa2_arg_j(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void moa2_arg_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void moa2_arg_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}

/* Universal seam call: frees args, returns the NEW scripted result (NULL ==
 * None) or transfers a raise-spec into *exc. */
static json_t *moa2_call(const moa2_io_t *io, const char *name, json_t *args,
                         json_t **exc) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__")) {
        if (!*exc) *exc = r;
        else json_free(r);
        return NULL;
    }
    return r;
}

/* Fire a callable double ({"__call__": name}); a non-callable non-None value
 * raises the live TypeError '<T>' object is not callable. */
static json_t *moa2_call_double(const json_t *fn, const moa2_io_t *io,
                                json_t *args, json_t **exc) {
    if (moa2_is_callable(fn)) {
        const char *name = json_obj_get(fn, "__call__")->str_val;
        return moa2_call(io, name, args, exc);
    }
    moa2_raise_fmt(exc, "TypeError", "'%s' object is not callable",
                   moa2_type_name(fn));
    json_free(args);
    return NULL;
}

/* getattr(v, name, None): attribute doubles read the window; every other
 * carrier (dict / scalar / None) returns the default. */
static const json_t *moa2_getattr_def(const json_t *v, const char *name) {
    const json_t *a = moa2_attr(v);
    if (!a) return NULL;
    return json_obj_get(a, name);
}

/* Direct attribute access v.name (raises when missing / wrong carrier).
 * Returns the borrowed value or NULL with *exc set. */
static const json_t *moa2_getattr_req(const json_t *v, const char *name,
                                      json_t **exc) {
    const json_t *a = moa2_attr(v);
    if (a) {
        const json_t *f = json_obj_get(a, name);
        if (!f) moa2_attr_err(exc, v, name);
        return f;
    }
    moa2_attr_err(exc, v, name);
    return NULL;
}

/* Unpack a seam result into exactly n items (list domain). Returns false and
 * sets *exc on the live ValueError/TypeError. items[] borrows the result. */
static bool moa2_unpack(const json_t *r, size_t n, const json_t **items,
                        json_t **exc) {
    if (!moa2_is_arr(r)) {
        moa2_raise_fmt(exc, "TypeError", "cannot unpack non-iterable %s object",
                       moa2_type_name(r));
        return false;
    }
    if (r->c.count > n) {
        moa2_raise_fmt(exc, "ValueError",
                       "too many values to unpack (expected %zu, got %zu)",
                       n, r->c.count);
        return false;
    }
    if (r->c.count < n) {
        moa2_raise_fmt(exc, "ValueError",
                       "not enough values to unpack (expected %zu, got %zu)",
                       n, r->c.count);
        return false;
    }
    for (size_t i = 0; i < n; i++) items[i] = r->c.items[i];
    return true;
}

/* dict.get on a value that must be a dict (raises AttributeError otherwise). */
static const json_t *moa2_dict_get(const json_t *d, const char *key,
                                   json_t **exc) {
    if (!moa2_is_dict(d)) {
        moa2_attr_err(exc, d, "get");
        return NULL;
    }
    return json_obj_get(d, key);
}

/* ── MoA PII regex ports (ASCII scope) ─────────────────────────────────────
 * _MOA_EMAIL_RE = \b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b
 * _MOA_PHONE_RE = (?<![\w.+-])(?:\+?1[ .-])?
 *                 (?:\(\d{3}\)[ .-]?|\d{3}[.-])\d{3}[.-]\d{4}(?![\w-])
 * Leftmost non-overlapping re.sub scan with the greedy/backtrack order of the
 * live patterns (fuzzed against live re over random ASCII corpora). */

static bool moa2_re_word(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_';
}
static bool moa2_re_alpha(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static bool moa2_re_digit(unsigned char c) { return c >= '0' && c <= '9'; }
static bool moa2_re_local(unsigned char c) {
    return moa2_re_word(c) || c == '.' || c == '%' || c == '+' || c == '-';
}
static bool moa2_re_domain(unsigned char c) {
    /* [A-Za-z0-9.-] — NO underscore (unlike the local-part class). */
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '.' || c == '-';
}
static bool moa2_re_sep1(unsigned char c) { return c == ' ' || c == '.' || c == '-'; }
static bool moa2_re_sep2(unsigned char c) { return c == '.' || c == '-'; }

static bool moa2_email_at(const char *s, size_t len, size_t i, size_t *end) {
    /* \b at i */
    bool wprev = (i > 0 && moa2_re_word((unsigned char)s[i - 1]));
    bool wcur = (i < len && moa2_re_word((unsigned char)s[i]));
    if (wprev == wcur) return false;
    /* local part: maximal class run ('@' is outside the class, so shorter
     * backtracks can never reach an '@' the maximal run missed). */
    size_t j = i;
    while (j < len && moa2_re_local((unsigned char)s[j])) j++;
    if (j == i || j >= len || s[j] != '@') return false;
    /* domain: maximal class run, then backtrack to each '.' (last first). */
    size_t k = j + 1, dstart = k;
    while (k < len && moa2_re_domain((unsigned char)s[k])) k++;
    if (k == dstart) return false;
    for (size_t d = k; d > dstart; d--) {
        if (s[d - 1] != '.') continue;
        if (d - 1 <= dstart) continue;   /* domain needs >= 1 char before '.' */
        size_t t = d, tstart = t;
        while (t < len && moa2_re_alpha((unsigned char)s[t])) t++;
        if (t - tstart < 2) continue;
        /* trailing \b: the maximal letter run must end at a non-word char;
         * shorter letter backtracks still face a word char, so only maximal. */
        if (t < len && moa2_re_word((unsigned char)s[t])) continue;
        *end = t;
        return true;
    }
    return false;
}

static bool moa2_phone_at(const char *s, size_t len, size_t i, size_t *end) {
    /* lookbehind (?<![\w.+-]) */
    if (i > 0) {
        unsigned char p = (unsigned char)s[i - 1];
        if (moa2_re_word(p) || p == '.' || p == '+' || p == '-') return false;
    }
    /* optional country code (greedy: present first, then absent) */
    for (int country = 1; country >= 0; country--) {
        size_t j = i;
        if (country) {
            size_t t = j;
            if (t < len && s[t] == '+') t++;
            if (t >= len || s[t] != '1') continue;
            t++;
            if (t >= len || !moa2_re_sep1((unsigned char)s[t])) continue;
            j = t + 1;
        }
        /* area alternatives in engine order: (ddd) with sep, (ddd) without,
         * ddd[.-] */
        for (int alt = 0; alt < 3; alt++) {
            size_t k = j;
            if (alt < 2) {
                if (!(k + 5 <= len && s[k] == '('
                      && moa2_re_digit((unsigned char)s[k + 1])
                      && moa2_re_digit((unsigned char)s[k + 2])
                      && moa2_re_digit((unsigned char)s[k + 3])
                      && s[k + 4] == ')')) continue;
                k += 5;
                if (alt == 0) {
                    if (k < len && moa2_re_sep1((unsigned char)s[k])) k++;
                    else continue;
                }
            } else {
                if (!(k + 4 <= len
                      && moa2_re_digit((unsigned char)s[k])
                      && moa2_re_digit((unsigned char)s[k + 1])
                      && moa2_re_digit((unsigned char)s[k + 2])
                      && moa2_re_sep2((unsigned char)s[k + 3]))) continue;
                k += 4;
            }
            /* exchange-subscriber: ddd[.-]dddd */
            if (k + 8 > len) continue;
            if (!(moa2_re_digit((unsigned char)s[k])
                  && moa2_re_digit((unsigned char)s[k + 1])
                  && moa2_re_digit((unsigned char)s[k + 2])
                  && moa2_re_sep2((unsigned char)s[k + 3])
                  && moa2_re_digit((unsigned char)s[k + 4])
                  && moa2_re_digit((unsigned char)s[k + 5])
                  && moa2_re_digit((unsigned char)s[k + 6])
                  && moa2_re_digit((unsigned char)s[k + 7]))) continue;
            size_t e = k + 8;
            /* lookahead (?![\w-]) */
            if (e < len) {
                unsigned char n = (unsigned char)s[e];
                if (moa2_re_word(n) || n == '-') continue;
            }
            *end = e;
            return true;
        }
    }
    return false;
}

typedef bool (*moa2_re_at_fn)(const char *, size_t, size_t, size_t *);

static char *moa2_re_sub(const char *s, moa2_re_at_fn at, const char *repl) {
    size_t len = strlen(s);
    moa2_buf_t b; moa2_buf_init(&b);
    size_t i = 0;
    while (i < len) {
        size_t end = 0;
        if (at(s, len, i, &end)) {
            moa2_buf_adds(&b, repl);
            i = end;
        } else {
            moa2_buf_add(&b, s + i, 1);
            i++;
        }
    }
    char *out = b.p ? b.p : strdup("");
    return out;
}

/* ── 1. _redact_reference_text ──────────────────────────────────────────────
 *   if not isinstance(text, str) or not text: return text
 *   from agent.redact import redact_sensitive_text
 *   text = redact_sensitive_text(text, force=True, code_file=True)
 *   text = _MOA_EMAIL_RE.sub("[redacted email]", text)
 *   return _MOA_PHONE_RE.sub("[redacted phone]", text)
 */
static json_t *moa2_redact_text_core(const json_t *text, const moa2_io_t *io,
                                     json_t **exc) {
    if (!moa2_is_str(text) || !text->str_val || !text->str_val[0])
        return text ? json_copy(text) : json_null();
    json_t *a = json_object();
    moa2_arg_s(a, "text", text->str_val);
    moa2_arg_b(a, "force", true);
    moa2_arg_b(a, "code_file", true);
    json_t *r = moa2_call(io, "redact_sensitive_text", a, exc);
    if (*exc) return NULL;
    if (!moa2_is_str(r)) {
        moa2_raise_fmt(exc, "TypeError",
                       "expected string or bytes-like object, got '%s'",
                       moa2_type_name(r));
        json_free(r);
        return NULL;
    }
    char *e = moa2_re_sub(r->str_val ? r->str_val : "", moa2_email_at,
                          "[redacted email]");
    json_free(r);
    if (!e) return json_string("");
    char *p = moa2_re_sub(e, moa2_phone_at, "[redacted phone]");
    free(e);
    json_t *ret = json_string(p ? p : "");
    free(p);
    return ret;
}

/* PoP: moa2_redact_reference_text @ agent/moa_loop.py:_redact_reference_text */
static json_t *moa2_redact_reference_text(const json_t *in, const moa2_io_t *io,
                                          json_t *state, json_t **exc) {
    (void)state;
    return moa2_redact_text_core(json_obj_get(in, "text"), io, exc);
}

/* ── 2. _redact_message_content ─────────────────────────────────────────────
 *   if isinstance(content, str): return _redact_reference_text(content)
 *   if isinstance(content, list):
 *       return [{**p, "text": _redact_reference_text(p.get("text"))}
 *               if isinstance(p, dict) and isinstance(p.get("text"), str)
 *               else p for p in content]
 *   return content
 */
static json_t *moa2_redact_content_core(const json_t *content,
                                        const moa2_io_t *io, json_t **exc) {
    if (moa2_is_str(content))
        return moa2_redact_text_core(content, io, exc);
    if (moa2_is_arr(content)) {
        json_t *out = json_array();
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *p = content->c.items[i];
            const json_t *pt = moa2_is_dict(p) ? json_obj_get(p, "text") : NULL;
            if (moa2_is_dict(p) && moa2_is_str(pt)) {
                json_t *red = moa2_redact_text_core(pt, io, exc);
                if (*exc) { json_free(out); return NULL; }
                json_t *np = json_copy(p);
                json_set(np, "text", red ? red : json_null());
                json_append(out, np);
            } else {
                json_append(out, p ? json_copy(p) : json_null());
            }
        }
        return out;
    }
    return content ? json_copy(content) : json_null();
}

/* PoP: moa2_redact_message_content @ agent/moa_loop.py:_redact_message_content */
static json_t *moa2_redact_message_content(const json_t *in, const moa2_io_t *io,
                                           json_t *state, json_t **exc) {
    (void)state;
    return moa2_redact_content_core(json_obj_get(in, "content"), io, exc);
}

/* ── 3. _redact_reference_outputs ───────────────────────────────────────────
 *   return [(label, _redact_reference_text(text), acct)
 *           for label, text, acct in reference_outputs]
 * (elements are 3-lists in the fixture domain; str/dict elements excluded)
 */
static json_t *moa2_redact_outputs_core(const json_t *outputs,
                                        const moa2_io_t *io, json_t **exc) {
    if (!moa2_is_arr(outputs)) {
        moa2_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                       moa2_type_name(outputs));
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < outputs->c.count; i++) {
        const json_t *o = outputs->c.items[i];
        const json_t *items[3];
        if (!moa2_unpack(o, 3, items, exc)) { json_free(out); return NULL; }
        json_t *red = moa2_redact_text_core(items[1], io, exc);
        if (*exc) { json_free(out); return NULL; }
        json_t *t = json_array();
        json_append(t, items[0] ? json_copy(items[0]) : json_null());
        json_append(t, red ? red : json_null());
        json_append(t, items[2] ? json_copy(items[2]) : json_null());
        json_append(out, t);
    }
    return out;
}

/* PoP: moa2_redact_reference_outputs @ agent/moa_loop.py:_redact_reference_outputs */
static json_t *moa2_redact_reference_outputs(const json_t *in, const moa2_io_t *io,
                                             json_t *state, json_t **exc) {
    (void)state;
    return moa2_redact_outputs_core(json_obj_get(in, "reference_outputs"), io, exc);
}

/* ── 4. _redact_trace_messages ──────────────────────────────────────────────
 *   if not isinstance(messages, list): return messages
 *   return [{**m, "content": _redact_message_content(m.get("content"))}
 *           if isinstance(m, dict) else m for m in messages]
 */
static json_t *moa2_redact_trace_msgs_core(const json_t *messages,
                                           const moa2_io_t *io, json_t **exc) {
    if (!moa2_is_arr(messages))
        return messages ? json_copy(messages) : json_null();
    json_t *out = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        const json_t *m = messages->c.items[i];
        if (!moa2_is_dict(m)) {
            json_append(out, m ? json_copy(m) : json_null());
            continue;
        }
        json_t *red = moa2_redact_content_core(json_obj_get(m, "content"), io, exc);
        if (*exc) { json_free(out); return NULL; }
        json_t *nm = json_copy(m);
        json_set(nm, "content", red ? red : json_null());
        json_append(out, nm);
    }
    return out;
}

/* PoP: moa2_redact_trace_messages @ agent/moa_loop.py:_redact_trace_messages */
static json_t *moa2_redact_trace_messages(const json_t *in, const moa2_io_t *io,
                                          json_t *state, json_t **exc) {
    (void)state;
    return moa2_redact_trace_msgs_core(json_obj_get(in, "messages"), io, exc);
}

/* ── 5. _redact_trace_accounting ────────────────────────────────────────────
 *   if not isinstance(acct, _RefAccounting): return acct
 *   return replace(acct, messages=_redact_trace_messages(acct.messages),
 *                  output=_redact_reference_text(acct.output))
 * (kwargs evaluate left-to-right: messages first, then output; the fixture
 * acct carries the full dataclass field set so replace() == copy+set)
 */
static json_t *moa2_redact_acct_core(const json_t *acct, const moa2_io_t *io,
                                     json_t **exc) {
    if (!moa2_is_acct(acct))
        return acct ? json_copy(acct) : json_null();
    json_t *m2 = moa2_redact_trace_msgs_core(json_obj_get(acct, "messages"), io, exc);
    if (*exc) return NULL;
    json_t *o2 = moa2_redact_text_core(json_obj_get(acct, "output"), io, exc);
    if (*exc) { json_free(m2); return NULL; }
    json_t *ret = json_copy(acct);
    json_set(ret, "messages", m2 ? m2 : json_null());
    json_set(ret, "output", o2 ? o2 : json_null());
    return ret;
}

/* PoP: moa2_redact_trace_accounting @ agent/moa_loop.py:_redact_trace_accounting */
static json_t *moa2_redact_trace_accounting(const json_t *in, const moa2_io_t *io,
                                            json_t *state, json_t **exc) {
    (void)state;
    return moa2_redact_acct_core(json_obj_get(in, "acct"), io, exc);
}

/* ── 6. _moa_privacy_mode ───────────────────────────────────────────────────
 *   from hermes_cli.moa_config import coerce_privacy_filter
 *   raw = moa_raw if isinstance(moa_raw, dict) else {}
 *   return coerce_privacy_filter(raw.get("privacy_filter"))
 */
/* PoP: moa2_moa_privacy_mode @ agent/moa_loop.py:_moa_privacy_mode */
static json_t *moa2_moa_privacy_mode(const json_t *in, const moa2_io_t *io,
                                     json_t *state, json_t **exc) {
    (void)state;
    const json_t *moa_raw = json_obj_get(in, "moa_raw");
    const json_t *raw = moa2_is_dict(moa_raw) ? moa_raw : NULL;
    const json_t *pf = raw ? json_obj_get(raw, "privacy_filter") : NULL;
    json_t *a = json_object();
    moa2_arg_j(a, "value", pf);
    json_t *r = moa2_call(io, "coerce_privacy_filter", a, exc);
    if (*exc) return NULL;
    return r ? r : json_null();
}

/* ── 7. _slot_label ─────────────────────────────────────────────────────────
 *   label = f"{(slot.get('provider') or '').strip()}:{(slot.get('model') or '').strip()}"
 *   effort = str(slot.get("reasoning_effort") or "").strip()
 *   return f"{label}[reasoning={effort}]" if effort else label
 */
static json_t *moa2_slot_label_core(const json_t *slot, json_t **exc) {
    const json_t *pv = moa2_dict_get(slot, "provider", exc);
    if (*exc) return NULL;
    const json_t *mv = moa2_dict_get(slot, "model", exc);
    if (*exc) return NULL;
    /* (x or '').strip(): falsy -> ""; truthy non-str -> AttributeError */
    char *ps = NULL, *ms = NULL;
    if (!moa2_truth(pv)) ps = strdup("");
    else if (!moa2_is_str(pv)) { moa2_attr_err(exc, pv, "strip"); return NULL; }
    else ps = moa2_strip(pv->str_val);
    if (!moa2_truth(mv)) ms = strdup("");
    else if (!moa2_is_str(mv)) { moa2_attr_err(exc, mv, "strip"); free(ps); return NULL; }
    else ms = moa2_strip(mv->str_val);

    const json_t *ev = moa2_dict_get(slot, "reasoning_effort", exc);
    if (*exc) { free(ps); free(ms); return NULL; }
    char *es = moa2_pystr_or_empty(ev);
    char *effort = moa2_strip(es);
    free(es);

    moa2_buf_t b; moa2_buf_init(&b);
    moa2_buf_adds(&b, ps ? ps : "");
    moa2_buf_adds(&b, ":");
    moa2_buf_adds(&b, ms ? ms : "");
    if (effort && effort[0]) {
        moa2_buf_adds(&b, "[reasoning=");
        moa2_buf_adds(&b, effort);
        moa2_buf_adds(&b, "]");
    }
    json_t *ret = json_string(b.p ? b.p : "");
    free(b.p); free(ps); free(ms); free(effort);
    return ret;
}

/* PoP: moa2_slot_label @ agent/moa_loop.py:_slot_label */
static json_t *moa2_slot_label(const json_t *in, const moa2_io_t *io,
                               json_t *state, json_t **exc) {
    (void)io; (void)state;
    return moa2_slot_label_core(json_obj_get(in, "slot"), exc);
}

/* ── 8. peel_reference_guidance ─────────────────────────────────────────────
 *   if not guidance or not messages: return messages
 *   guidance_text = str(guidance)
 *   last = messages[-1]
 *   if not isinstance(last, dict) or last.get("role") != "user": return messages
 *   content = last.get("content")
 *   if content == guidance_text: return list(messages[:-1])
 *   if isinstance(content, list) and len(content) == 1:
 *       part = content[0]
 *       if isinstance(part, dict) and part.get("type", "text") == "text"
 *          and (part.get("text") or "") == guidance_text:
 *           return list(messages[:-1])
 *   return messages
 * (inputs never mutated; the live def has NO suffix-peel shapes)
 */
static json_t *moa2_peel_core(const json_t *messages, const json_t *guidance,
                              json_t **exc) {
    if (!moa2_truth(guidance) || !moa2_truth(messages))
        return messages ? json_copy(messages) : json_null();
    char *gtext = moa2_pystr(guidance);
    const json_t *last = NULL;
    if (moa2_is_arr(messages)) {
        last = messages->c.items[messages->c.count - 1];
    } else if (moa2_is_str(messages)) {
        /* str[-1] is the last character (never a dict -> pass through) */
        free(gtext);
        return json_copy(messages);
    } else {
        moa2_raise_fmt(exc, "TypeError", "'%s' object is not subscriptable",
                       moa2_type_name(messages));
        free(gtext);
        return NULL;
    }
    json_t *ret = NULL;
    bool peeled = false;
    if (moa2_is_dict(last)
        && moa2_str_eq(json_get_str(last, "role", NULL), "user")) {
        const json_t *content = json_obj_get(last, "content");
        if (moa2_is_str(content) && moa2_str_eq(content->str_val, gtext)) {
            peeled = true;
        } else if (moa2_is_arr(content) && content->c.count == 1) {
            const json_t *part = content->c.items[0];
            if (moa2_is_dict(part)) {
                const json_t *ty = json_obj_get(part, "type");
                bool type_ok = !ty ? true
                    : (moa2_is_str(ty) && moa2_str_eq(ty->str_val, "text"));
                const json_t *tx = json_obj_get(part, "text");
                /* (part.get("text") or "") == guidance_text: only a str can
                 * equal the str guidance_text (truthy non-str stays itself) */
                bool text_ok = false;
                if (!moa2_truth(tx)) text_ok = (gtext[0] == '\0');
                else if (moa2_is_str(tx)) text_ok = moa2_str_eq(tx->str_val, gtext);
                if (type_ok && text_ok) peeled = true;
            }
        }
    }
    if (peeled) {
        ret = json_array();
        for (size_t i = 0; i + 1 < messages->c.count; i++)
            json_append(ret, json_copy(messages->c.items[i]));
    } else {
        ret = json_copy(messages);
    }
    free(gtext);
    return ret;
}

/* PoP: moa2_peel_reference_guidance @ agent/moa_loop.py:peel_reference_guidance */
static json_t *moa2_peel_reference_guidance(const json_t *in, const moa2_io_t *io,
                                            json_t *state, json_t **exc) {
    (void)io; (void)state;
    return moa2_peel_core(json_obj_get(in, "messages"),
                          json_obj_get(in, "guidance"), exc);
}

/* ── 9. _attach_reference_guidance ──────────────────────────────────────────
 *   agg_messages.append({"role": "user", "content": guidance})
 * (mutates the state-carried list; returns None)
 */
static bool moa2_attach_core(json_t *msgs, const json_t *guidance,
                             json_t **exc) {
    if (!moa2_is_arr(msgs)) {
        moa2_attr_err(exc, msgs, "append");
        return false;
    }
    json_t *m = json_object();
    json_set(m, "role", json_string("user"));
    json_set(m, "content", guidance ? json_copy(guidance) : json_null());
    json_append(msgs, m);
    return true;
}

/* PoP: moa2_attach_reference_guidance @ agent/moa_loop.py:_attach_reference_guidance */
static json_t *moa2_attach_reference_guidance(const json_t *in, const moa2_io_t *io,
                                              json_t *state, json_t **exc) {
    (void)io;
    if (!moa2_attach_core(json_obj_get(state, "agg_messages"),
                          json_obj_get(in, "guidance"), exc))
        return NULL;
    return json_null();
}

/* ── 10. _reference_liveness_hook (+ nested _touch_on_progress) ─────────────
 *   touch = getattr(agent, "_touch_activity", None)
 *   def _touch_on_progress():
 *       if callable(touch): touch(f"MoA reference {label}: stream progress")
 *   return _touch_on_progress
 * The closure projects to {"__closure__", "touch", "label"} (captured cells).
 */
/* PoP: moa2_reference_liveness_hook @ agent/moa_loop.py:_reference_liveness_hook */
static json_t *moa2_reference_liveness_hook(const json_t *in, const moa2_io_t *io,
                                            json_t *state, json_t **exc) {
    (void)io; (void)state; (void)exc;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *touch = moa2_getattr_def(agent, "_touch_activity");
    const char *label = json_get_str(in, "label", "");
    json_t *ret = json_object();
    json_set(ret, "__closure__", json_string("_touch_on_progress"));
    json_set(ret, "touch", touch ? json_copy(touch) : json_null());
    json_set(ret, "label", json_string(label));
    return ret;
}

/* PoP: moa2_touch_on_progress @ agent/moa_loop.py:_touch_on_progress */
static json_t *moa2_touch_on_progress(const json_t *in, const moa2_io_t *io,
                                      json_t *state, json_t **exc) {
    (void)state;
    const json_t *touch = json_obj_get(in, "touch");
    if (!moa2_is_callable(touch)) return json_null();
    const char *label = json_get_str(in, "label", "");
    moa2_buf_t b; moa2_buf_init(&b);
    moa2_buf_adds(&b, "MoA reference ");
    moa2_buf_adds(&b, label);
    moa2_buf_adds(&b, ": stream progress");
    json_t *a = json_object();
    moa2_arg_s(a, "message", b.p ? b.p : "");
    free(b.p);
    json_t *r = moa2_call_double(touch, io, a, exc);
    json_free(r);
    if (*exc) return NULL;
    return json_null();
}

/* ── 11. _touch_fanout_progress ─────────────────────────────────────────────
 *   touch = getattr(agent, "_touch_activity", None)
 *   if callable(touch):
 *       with contextlib.suppress(Exception): touch(description)
 */
static void moa2_touch_fanout_core(const json_t *agent, const json_t *description,
                                   const moa2_io_t *io, json_t **exc) {
    const json_t *touch = moa2_getattr_def(agent, "_touch_activity");
    if (!moa2_is_callable(touch)) return;
    json_t *a = json_object();
    moa2_arg_j(a, "message", description);
    json_t *r = moa2_call_double(touch, io, a, exc);
    json_free(r);
    if (*exc) {
        if (!moa2_swallow(exc)) return;   /* BaseException propagates */
    }
}

/* PoP: moa2_touch_fanout_progress @ agent/moa_loop.py:_touch_fanout_progress */
static json_t *moa2_touch_fanout_progress(const json_t *in, const moa2_io_t *io,
                                          json_t *state, json_t **exc) {
    (void)state;
    moa2_touch_fanout_core(json_obj_get(in, "agent"),
                           json_obj_get(in, "description"), io, exc);
    if (*exc) return NULL;
    return json_null();
}

/* ── 12. _slot_request_headers ──────────────────────────────────────────────
 *   provider = _normalize_aux_provider(str(runtime.get("provider") or ""))
 *   profile = get_provider_profile(provider) if provider else None
 *   if profile is None: return None
 *   return route_answer("request_headers", provider=provider,
 *                       base_url=profile.base_url or "",
 *                       model=str(runtime.get("model") or ""),
 *                       user_initiated=True, scope=agent)
 */
/* PoP: moa2_slot_request_headers @ agent/moa_loop.py:_slot_request_headers */
static json_t *moa2_slot_request_headers(const json_t *in, const moa2_io_t *io,
                                         json_t *state, json_t **exc) {
    (void)state;
    const json_t *runtime = json_obj_get(in, "runtime");
    const json_t *agent = json_obj_get(in, "agent");

    const json_t *pv = moa2_dict_get(runtime, "provider", exc);
    if (*exc) return NULL;
    char *ps = moa2_pystr_or_empty(pv);
    json_t *a = json_object();
    moa2_arg_s(a, "provider", ps ? ps : "");
    free(ps);
    json_t *provider = moa2_call(io, "_normalize_aux_provider", a, exc);
    if (*exc) return NULL;

    json_t *profile = NULL;
    if (moa2_truth(provider)) {
        json_t *pa = json_object();
        moa2_arg_j(pa, "provider", provider);
        profile = moa2_call(io, "get_provider_profile", pa, exc);
        if (*exc) { json_free(provider); return NULL; }
    }
    if (moa2_is_null(profile)) {
        json_free(provider); json_free(profile);
        return json_null();
    }
    /* profile.base_url — direct attribute access */
    const json_t *bu = moa2_getattr_req(profile, "base_url", exc);
    if (*exc) { json_free(provider); json_free(profile); return NULL; }
    const json_t *base_url = moa2_truth(bu) ? bu : NULL;

    const json_t *mv = moa2_dict_get(runtime, "model", exc);
    if (*exc) { json_free(provider); json_free(profile); return NULL; }
    char *ms = moa2_pystr_or_empty(mv);

    json_t *ra = json_object();
    moa2_arg_s(ra, "name", "request_headers");
    moa2_arg_j(ra, "provider", provider);
    if (base_url) moa2_arg_j(ra, "base_url", base_url);
    else moa2_arg_s(ra, "base_url", "");
    moa2_arg_s(ra, "model", ms ? ms : "");
    moa2_arg_b(ra, "user_initiated", true);
    moa2_arg_j(ra, "scope", agent);
    free(ms);
    json_t *ret = moa2_call(io, "route_answer", ra, exc);
    json_free(provider); json_free(profile);
    if (*exc) return NULL;
    return ret ? ret : json_null();
}

/* ── 13. _moa_reference_relay (nested in build_moa_facade) ──────────────────
 *   cb = getattr(agent, "tool_progress_callback", None)
 *   spec = _RELAY_EVENTS.get(event)
 *   if cb is None or spec is None: return
 *   primary, secondary, extra_map = spec
 *   with contextlib.suppress(Exception):
 *       cb(event, str(kwargs.get(primary) or ""),
 *          str(kwargs.get(secondary) or "") if secondary else None, None,
 *          **{out: kwargs.get(src) for out, src in extra_map.items()})
 * (the constant 4th positional None carries no seam-arg key)
 */
typedef struct {
    const char *event;
    const char *primary;
    const char *secondary;   /* NULL == no secondary kwarg */
    const char *extra_out[4];
    const char *extra_src[4];
    size_t n_extra;
} moa2_relay_spec_t;

static const moa2_relay_spec_t MOA2_RELAY_EVENTS[] = {
    {"moa.reference", "label", "text",
     {"moa_index", "moa_count"}, {"index", "count"}, 2},
    {"moa.progress", "label", NULL,
     {"moa_refs_done", "moa_refs_total"}, {"refs_done", "refs_total"}, 2},
    {"moa.phase", "aggregator", NULL,
     {"moa_phase", "moa_refs_done", "moa_refs_total"}, {"phase", "refs_done", "refs_total"}, 3},
    {"moa.aggregating", "aggregator", NULL,
     {"moa_ref_count"}, {"ref_count"}, 1},
};

/* PoP: moa2_moa_reference_relay @ agent/moa_loop.py:_moa_reference_relay */
static json_t *moa2_moa_reference_relay(const json_t *in, const moa2_io_t *io,
                                        json_t *state, json_t **exc) {
    (void)state;
    const json_t *agent = json_obj_get(in, "agent");
    const char *event = json_get_str(in, "event", "");
    const json_t *kwargs = json_obj_get(in, "kwargs");

    const json_t *cb = moa2_getattr_def(agent, "tool_progress_callback");
    const moa2_relay_spec_t *spec = NULL;
    for (size_t i = 0; i < sizeof(MOA2_RELAY_EVENTS) / sizeof(MOA2_RELAY_EVENTS[0]); i++) {
        if (moa2_str_eq(MOA2_RELAY_EVENTS[i].event, event)) {
            spec = &MOA2_RELAY_EVENTS[i];
            break;
        }
    }
    if (moa2_is_null(cb) || !spec) return json_null();

    /* Everything below is inside the live suppress(Exception) region. */
    const json_t *pk = moa2_dict_get(kwargs, spec->primary, exc);
    if (*exc) { if (!moa2_swallow(exc)) return NULL; return json_null(); }
    char *label = moa2_pystr_or_empty(pk);
    char *text = NULL;
    if (spec->secondary) {
        const json_t *sk = moa2_dict_get(kwargs, spec->secondary, exc);
        if (*exc) { free(label); if (!moa2_swallow(exc)) return NULL; return json_null(); }
        text = moa2_pystr_or_empty(sk);
    }
    json_t *a = json_object();
    moa2_arg_s(a, "event", event);
    moa2_arg_s(a, "label", label ? label : "");
    moa2_arg_s(a, "text", text);   /* NULL (no secondary) -> key dropped */
    free(label); free(text);
    for (size_t i = 0; i < spec->n_extra; i++) {
        const json_t *v = moa2_dict_get(kwargs, spec->extra_src[i], exc);
        if (*exc) {
            json_free(a);
            if (!moa2_swallow(exc)) return NULL;
            return json_null();
        }
        moa2_arg_j(a, spec->extra_out[i], v);
    }
    json_t *r = moa2_call_double(cb, io, a, exc);
    json_free(r);
    if (*exc) { if (!moa2_swallow(exc)) return NULL; }
    return json_null();
}

/* ── 14/15. MoAClient.last_aggregator_route / last_aggregator_attempt ───────
 *   @property
 *   def last_aggregator_route(self):
 *       return getattr(self.chat.completions, "last_aggregator_route", None)
 * (self.chat / .completions are DIRECT attribute accesses; the final getattr
 * has a default, so dict/scalar/None completions read None)
 */
static json_t *moa2_client_prop_core(const json_t *slf, const char *name,
                                     json_t **exc) {
    const json_t *chat = moa2_getattr_req(slf, "chat", exc);
    if (*exc) return NULL;
    const json_t *comp = moa2_getattr_req(chat, "completions", exc);
    if (*exc) return NULL;
    const json_t *v = moa2_getattr_def(comp, name);
    return v ? json_copy(v) : json_null();
}

/* PoP: moa2_last_aggregator_route @ agent/moa_loop.py:last_aggregator_route */
static json_t *moa2_last_aggregator_route(const json_t *in, const moa2_io_t *io,
                                          json_t *state, json_t **exc) {
    (void)io; (void)state;
    return moa2_client_prop_core(json_obj_get(in, "self"),
                                 "last_aggregator_route", exc);
}

/* PoP: moa2_last_aggregator_attempt @ agent/moa_loop.py:last_aggregator_attempt */
static json_t *moa2_last_aggregator_attempt(const json_t *in, const moa2_io_t *io,
                                            json_t *state, json_t **exc) {
    (void)io; (void)state;
    return moa2_client_prop_core(json_obj_get(in, "self"),
                                 "last_aggregator_attempt", exc);
}

/* ── 16. _resolve_preset_cached ─────────────────────────────────────────────
 *   cfg_stamp = None if worker_config_snapshot() is not None
 *               else config_cache_version_or_none(get_config_path())
 *   moa_raw = load_config().get("moa") or {}
 *   key = (cfg_stamp, preset_name)
 *   with _preset_cache_lock:
 *       preset = _preset_cache.get(key) if cfg_stamp is not None else None
 *   if preset is None:
 *       preset = resolve_moa_preset(moa_raw, preset_name)
 *       if cfg_stamp is not None:
 *           with _preset_cache_lock:
 *               _preset_cache.clear(); _preset_cache[key] = preset
 *   return preset, moa_raw
 * The tuple key projects to compact JSON "[stamp,name]" (json.dumps
 * separators=(",",":")); the lock is unobservable single-threaded.
 */

/* Python json.dumps string escaping for the ASCII domain (\b \f \n \r \t,
 * \" \\, \u00xx for the remaining C0 set). malloc'd, quoted. */
static char *moa2_py_json_str(const char *s) {
    if (!s) s = "";
    moa2_buf_t b; moa2_buf_init(&b);
    moa2_buf_add(&b, "\"", 1);
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  moa2_buf_adds(&b, "\\\""); break;
            case '\\': moa2_buf_adds(&b, "\\\\"); break;
            case '\b': moa2_buf_adds(&b, "\\b"); break;
            case '\f': moa2_buf_adds(&b, "\\f"); break;
            case '\n': moa2_buf_adds(&b, "\\n"); break;
            case '\r': moa2_buf_adds(&b, "\\r"); break;
            case '\t': moa2_buf_adds(&b, "\\t"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    moa2_buf_adds(&b, esc);
                } else {
                    moa2_buf_add(&b, (const char *)&c, 1);
                }
        }
    }
    moa2_buf_add(&b, "\"", 1);
    return b.p ? b.p : strdup("\"\"");
}

static char *moa2_cache_key(const json_t *stamp, const char *name) {
    moa2_buf_t b; moa2_buf_init(&b);
    moa2_buf_adds(&b, "[");
    if (moa2_is_null(stamp)) {
        moa2_buf_adds(&b, "null");
    } else if (moa2_is_str(stamp)) {
        char *q = moa2_py_json_str(stamp->str_val);
        moa2_buf_adds(&b, q ? q : "\"\"");
        free(q);
    } else if (moa2_is_bool(stamp)) {
        moa2_buf_adds(&b, stamp->bool_val ? "true" : "false");
    } else if (moa2_is_num(stamp)) {
        char nb[64];
        double d = stamp->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18)
            snprintf(nb, sizeof(nb), "%lld", (long long)d);
        else
            moa2_float_repr(d, nb, sizeof(nb));
        moa2_buf_adds(&b, nb);
    } else {
        moa2_buf_adds(&b, "null");   /* outside the stamp domain */
    }
    moa2_buf_adds(&b, ",");
    char *q = moa2_py_json_str(name ? name : "");
    moa2_buf_adds(&b, q ? q : "\"\"");
    free(q);
    moa2_buf_adds(&b, "]");
    return b.p ? b.p : strdup("[null,\"\"]");
}

/* PoP: moa2_resolve_preset_cached @ agent/moa_loop.py:_resolve_preset_cached */
static json_t *moa2_resolve_preset_cached(const json_t *in, const moa2_io_t *io,
                                          json_t *state, json_t **exc) {
    const char *preset_name = json_get_str(in, "preset_name", "");

    json_t *snap = moa2_call(io, "worker_config_snapshot", json_object(), exc);
    if (*exc) return NULL;
    json_t *stamp = NULL;   /* None */
    if (moa2_is_null(snap)) {
        json_t *path = moa2_call(io, "get_config_path", json_object(), exc);
        if (*exc) { json_free(snap); return NULL; }
        json_t *ca = json_object();
        moa2_arg_j(ca, "path", path);
        json_free(path);
        stamp = moa2_call(io, "config_cache_version_or_none", ca, exc);
        if (*exc) { json_free(snap); return NULL; }
    }
    json_free(snap);

    json_t *cfg = moa2_call(io, "load_config", json_object(), exc);
    if (*exc) { json_free(stamp); return NULL; }
    const json_t *moa = moa2_dict_get(cfg, "moa", exc);
    if (*exc) { json_free(cfg); json_free(stamp); return NULL; }
    json_t *moa_raw = moa2_truth(moa) ? json_copy(moa) : json_object();
    json_free(cfg);

    char *key = moa2_cache_key(stamp, preset_name);
    json_t *cache = json_obj_get(state, "preset_cache");
    if (!moa2_is_obj(cache)) {
        cache = json_object();
        json_set(state, "preset_cache", cache);
    }
    json_t *preset = NULL;
    if (!moa2_is_null(stamp)) {
        const json_t *hit = json_obj_get(cache, key);
        if (hit) preset = json_copy(hit);
    }
    if (moa2_is_null(preset)) {
        json_free(preset); preset = NULL;
        json_t *ra = json_object();
        moa2_arg_j(ra, "moa_raw", moa_raw);
        moa2_arg_s(ra, "preset_name", preset_name);
        preset = moa2_call(io, "resolve_moa_preset", ra, exc);
        if (*exc) {
            free(key); json_free(moa_raw); json_free(stamp);
            return NULL;
        }
        if (!moa2_is_null(stamp)) {
            while (cache->c.count > 0)
                json_obj_del(cache, cache->c.keys[0]);
            json_set(cache, key, preset ? json_copy(preset) : json_null());
        }
    }
    json_t *ret = json_array();
    json_append(ret, preset ? preset : json_null());
    json_append(ret, moa_raw);
    free(key); json_free(stamp);
    return ret;
}

/* ── 17. collect (nested in _run_references_parallel) ───────────────────────
 *   def collect(done):
 *       nonlocal completed
 *       for future in done:
 *           idx = futures[future]
 *           results[idx] = future.result()
 *           completed += 1
 *           _touch_fanout_progress(agent, f"MoA: {completed} of {total} references complete")
 *           if progress_callback is not None:
 *               try: progress_callback(completed, total, _slot_label(reference_models[idx]))
 *               except Exception as exc: logger.debug(...)   (silent)
 * `done` iterates in fixture order (the live set's hash order is modeled as
 * the scripted sequence); futures[future] projects to the future's idx field.
 * _touch_fanout_progress and _slot_label compose REAL (ported siblings).
 */
/* PoP: moa2_collect @ agent/moa_loop.py:collect */
static json_t *moa2_collect(const json_t *in, const moa2_io_t *io,
                            json_t *state, json_t **exc) {
    const json_t *done = json_obj_get(in, "done");
    const json_t *ref_models = json_obj_get(in, "reference_models");
    const json_t *agent = json_obj_get(in, "agent");
    double total = json_get_num(in, "total", 0);
    const json_t *cb = json_obj_get(in, "progress_callback");

    json_t *results = json_obj_get(state, "results");
    if (!moa2_is_arr(results)) {
        results = json_array();
        json_set(state, "results", results);
    }
    double completed = json_get_num(state, "completed", 0);

    if (!moa2_is_arr(done)) {
        moa2_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                       moa2_type_name(done));
        return NULL;
    }
    for (size_t i = 0; i < done->c.count; i++) {
        const json_t *d = done->c.items[i];
        double idx = json_get_num(d, "idx", 0);
        size_t ix = (size_t)idx;

        json_t *fa = json_object();
        json_set(fa, "idx", json_number(idx));
        json_t *res = moa2_call(io, "future.result", fa, exc);
        if (*exc) return NULL;
        if (ix >= results->c.count) {
            json_free(res);
            moa2_raise(exc, "IndexError", "list assignment index out of range");
            return NULL;
        }
        json_free(results->c.items[ix]);
        results->c.items[ix] = res ? res : json_null();
        completed += 1;
        json_set(state, "completed", json_number(completed));

        /* _touch_fanout_progress(agent, f"MoA: {completed} of {total} ...") */
        char msg[128];
        snprintf(msg, sizeof(msg), "MoA: %lld of %lld references complete",
                 (long long)completed, (long long)total);
        json_t *desc = json_string(msg);
        moa2_touch_fanout_core(agent, desc, io, exc);
        json_free(desc);
        if (*exc) return NULL;   /* BaseException from the touch seam */

        if (!moa2_is_null(cb)) {
            /* try region: _slot_label evaluates as the callback argument */
            json_t *e2 = NULL;
            const json_t *slot = NULL;
            if (!moa2_is_arr(ref_models)) {
                moa2_raise_fmt(&e2, "TypeError", "'%s' object is not subscriptable",
                               moa2_type_name(ref_models));
            } else if (ix >= ref_models->c.count) {
                moa2_raise(&e2, "IndexError", "list index out of range");
            } else {
                slot = ref_models->c.items[ix];
            }
            json_t *lbl = NULL;
            if (!e2) {
                lbl = moa2_slot_label_core(slot, &e2);
            }
            if (!e2) {
                json_t *pa = json_object();
                json_set(pa, "done", json_number(completed));
                json_set(pa, "total", json_number(total));
                moa2_arg_j(pa, "label", lbl);
                json_t *r = moa2_call_double(cb, io, pa, &e2);
                json_free(r);
            }
            json_free(lbl);
            if (e2) {
                if (!moa2_swallow(&e2)) { *exc = e2; return NULL; }
                /* logger.debug("MoA progress_callback failed: %s", exc) —
                 * silent (no seam-valued args). */
            }
        }
    }
    return json_null();
}

/* ── 18. MoAChatCompletions._plan_aggregator_cache ──────────────────────────
 *   try:
 *       planning_messages = peel_reference_guidance(agg_messages, str(guidance))
 *                           if guidance else agg_messages
 *       _agent = getattr(self, "_agent", None)
 *       cache_disabled, cache_ttl, cache_ttl_auto = _agent_cache_opts(_agent)
 *       agg_messages, tools = plan_cache_sections_for_destination(
 *           planning_messages, tools, provider=..., base_url=..., api_mode=...,
 *           model=..., cache_disabled=..., cache_ttl=..., cache_ttl_auto=...,
 *           static_system_prefix=getattr(_agent, "_cached_system_prompt_static", None))
 *       if guidance: _attach_reference_guidance(agg_messages, str(guidance))
 *   except Exception as exc: logger.warning(...)   (silent)
 *   return agg_messages, tools
 * peel/_attach compose REAL; _agent_cache_opts is the flow1-claimed seam.
 * On a caught raise the working (agg_messages, tools) keep their pre-raise
 * values (partial planner results survive an _attach failure).
 */
/* PoP: moa2_plan_aggregator_cache @ agent/moa_loop.py:_plan_aggregator_cache */
static json_t *moa2_plan_aggregator_cache(const json_t *in, const moa2_io_t *io,
                                          json_t *state, json_t **exc) {
    const json_t *guidance = json_obj_get(in, "guidance");
    const json_t *rt = json_obj_get(in, "agg_runtime");
    const json_t *am0 = json_obj_get(in, "agg_messages");
    const json_t *tl0 = json_obj_get(in, "tools");
    json_t *cur_msgs = am0 ? json_copy(am0) : json_null();
    json_t *cur_tools = tl0 ? json_copy(tl0) : json_null();
    json_t *peeled = NULL;
    json_t *opts = NULL;
    json_t *plan = NULL;
    json_t *e2 = NULL;
    const json_t *provider_v = NULL, *base_url_v = NULL,
                 *api_mode_v = NULL, *model_v = NULL;

    const json_t *planning = cur_msgs;
    if (moa2_truth(guidance)) {
        peeled = moa2_peel_core(cur_msgs, guidance, &e2);
        if (e2) goto caught;
        planning = peeled;
    }
    const json_t *agent = json_obj_get(state, "_agent");
    {
        json_t *aa = json_object();
        moa2_arg_j(aa, "agent", agent);
        opts = moa2_call(io, "_agent_cache_opts", aa, &e2);
        if (e2) goto caught;
    }
    const json_t *oi[3];
    if (!moa2_unpack(opts, 3, oi, &e2)) goto caught;
    /* provider/base_url/api_mode/model: `agg_runtime.get(k) or ""` — NO str()
     * projection here (unlike _slot_request_headers); truthy values pass raw. */
    {
        const json_t *pv = moa2_dict_get(rt, "provider", &e2);
        if (e2) goto caught;
        provider_v = moa2_truth(pv) ? pv : NULL;
        const json_t *bv = moa2_dict_get(rt, "base_url", &e2);
        if (e2) goto caught;
        base_url_v = moa2_truth(bv) ? bv : NULL;
        const json_t *av = moa2_dict_get(rt, "api_mode", &e2);
        if (e2) goto caught;
        api_mode_v = moa2_truth(av) ? av : NULL;
        const json_t *mv = moa2_dict_get(rt, "model", &e2);
        if (e2) goto caught;
        model_v = moa2_truth(mv) ? mv : NULL;
    }
    {
        const json_t *prefix = moa2_getattr_def(agent, "_cached_system_prompt_static");
        json_t *pa = json_object();
        moa2_arg_j(pa, "messages", planning);
        moa2_arg_j(pa, "tools", cur_tools);
        if (provider_v) moa2_arg_j(pa, "provider", provider_v);
        else moa2_arg_s(pa, "provider", "");
        if (base_url_v) moa2_arg_j(pa, "base_url", base_url_v);
        else moa2_arg_s(pa, "base_url", "");
        if (api_mode_v) moa2_arg_j(pa, "api_mode", api_mode_v);
        else moa2_arg_s(pa, "api_mode", "");
        if (model_v) moa2_arg_j(pa, "model", model_v);
        else moa2_arg_s(pa, "model", "");
        moa2_arg_j(pa, "cache_disabled", oi[0]);
        moa2_arg_j(pa, "cache_ttl", oi[1]);
        moa2_arg_j(pa, "cache_ttl_auto", oi[2]);
        moa2_arg_j(pa, "static_system_prefix", prefix);
        plan = moa2_call(io, "plan_cache_sections_for_destination", pa, &e2);
        if (e2) goto caught;
    }
    {
        const json_t *pi[2];
        if (!moa2_unpack(plan, 2, pi, &e2)) goto caught;
        json_free(cur_msgs);
        cur_msgs = pi[0] ? json_copy(pi[0]) : json_null();
        json_free(cur_tools);
        cur_tools = pi[1] ? json_copy(pi[1]) : json_null();
    }
    if (moa2_truth(guidance)) {
        if (!moa2_attach_core(cur_msgs, guidance, &e2)) goto caught;
    }
    goto done;

caught:
    if (!moa2_swallow(&e2)) {
        *exc = e2;
        json_free(cur_msgs); json_free(cur_tools);
        json_free(peeled); json_free(opts); json_free(plan);
        return NULL;
    }
    /* logger.warning("MoA aggregator cache plan failed — ...: %s", exc) —
     * silent (no seam-valued args). */
done:
    json_free(peeled); json_free(opts); json_free(plan);
    json_t *ret = json_array();
    json_append(ret, cur_msgs);
    json_append(ret, cur_tools);
    return ret;
}

/* ── 19. bind_moa_runtime ───────────────────────────────────────────────────
 *   agent.model = str(preset_name or "default")
 *   agent.provider = agent.requested_provider = "moa"
 *   agent.api_mode = "chat_completions"
 *   agent.api_key = api_key or "moa-virtual-provider"
 *   agent.base_url = "moa://local"
 *   agent._client_kwargs = {}
 *   agent.client = build_moa_facade(agent, agent.model)
 * (mutates the state-carried agent double; build_moa_facade is seamed)
 */
/* PoP: moa2_bind_moa_runtime @ agent/moa_loop.py:bind_moa_runtime */
static json_t *moa2_bind_moa_runtime(const json_t *in, const moa2_io_t *io,
                                     json_t *state, json_t **exc) {
    json_t *agent = json_obj_get(state, "agent");
    json_t *a = (json_t *)moa2_attr(agent);
    if (!a) {
        /* py3.14 attribute SET on a carrier without __dict__ */
        moa2_raise_fmt(exc, "AttributeError",
                       "'%s' object has no attribute 'model' and no __dict__ "
                       "for setting new attributes", moa2_type_name(agent));
        return NULL;
    }
    const json_t *pn = json_obj_get(in, "preset_name");
    char *model = moa2_truth(pn) ? moa2_pystr(pn) : strdup("default");
    json_set(a, "model", json_string(model ? model : "default"));
    free(model);
    json_set(a, "provider", json_string("moa"));
    json_set(a, "requested_provider", json_string("moa"));
    json_set(a, "api_mode", json_string("chat_completions"));
    const json_t *ak = json_obj_get(in, "api_key");
    json_set(a, "api_key", moa2_truth(ak) ? json_copy(ak)
                                          : json_string("moa-virtual-provider"));
    json_set(a, "base_url", json_string("moa://local"));
    json_set(a, "_client_kwargs", json_object());

    json_t *fa = json_object();
    moa2_arg_j(fa, "agent", agent);
    moa2_arg_s(fa, "preset_name", json_get_str(a, "model", ""));
    json_t *client = moa2_call(io, "build_moa_facade", fa, exc);
    if (*exc) return NULL;
    json_set(a, "client", client ? client : json_null());
    return json_null();
}