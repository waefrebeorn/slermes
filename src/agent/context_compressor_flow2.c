/* context_compressor_flow2.c — second-wave FLOW + leaf port of
 * agent/context_compressor.py (LIVE authority).
 *
 * Ports 24 UNCLAIMED module functions: the checkpoint/sidecar/salvage flow
 * (drop_shadowed_checkpoints, _prune_stale_reasoning_replay,
 * salvage_grown_transcript, _archive_watermark_for, ...), the refusal /
 * status-code detectors, the summary-stub / skill-view failure predicates,
 * the synthetic-user-row / typed-user-text leaves, and the tool-payload
 * failure-suffix chain (_json_dict / _failure_suffix / _payload_failure_suffix
 * / _result_failure_suffix). Claimed siblings and all foreign I/O enter
 * through the cc2_io_t universal seam; ported siblings compose for REAL.
 *
 * Decision sequences only: Python truthiness, `or` short-circuits, `is None`
 * / `is False` identity vs truthy tests, kwarg/operand evaluation order, dict
 * insertion order, and exception catch scopes are transcribed per call site.
 * A scripted BaseException (SystemExit / KeyboardInterrupt / GeneratorExit /
 * CancelledError) propagates past every guard exactly as live (none of these
 * 24 defs swallows Exception; _json_dict catches only JSONDecodeError/TypeError
 * from the REAL json parser, never a scripted seam raise).
 *
 * Value model (shared with tests/oracle_context_compressor_flow2.py):
 *   - attribute doubles: {"__attr__": {...}}  <-> types.SimpleNamespace
 *   - callable doubles:  {"__callable__": "<seam>"} (callable() is True)
 *   - raise specs:       {"__raise__": "<ClassName>", "message": str(e)}
 *   - messages / payloads: plain JSON objects/arrays (isinstance(x, dict) is
 *     True ONLY for a plain object, never an __attr__ double)
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - str()/strip()/split() positions carry ASCII text; str(reason) in
 *     _failure_suffix carries str / integral number / bool / None only (no
 *     containers, no non-integral floats — Python repr is not transcribed).
 *   - _json_dict fixtures stay inside the json.loads/libjson agreement zone
 *     (valid compact objects, clearly-invalid JSON, non-object JSON, empty,
 *     non-str); NaN / duplicate-key / control-char / trailing-comma edges are
 *     excluded.
 *   - int() coercion in _archive_watermark_for accepts null/bool/integral
 *     float/number/signed-digit-string (ASCII space padding); the int strip
 *     set excludes \x1c-\x1f (narrower than str.strip).
 *   - _is_summary_refusal runs the byte-pinned _SUMMARY_REFUSAL_PREFIX_RE
 *     matcher over ASCII text (the U+2019 apostrophe branch never fires on
 *     ASCII input but is transcribed).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The
 * harness #includes this .c via the area-qualified
 * "agent/context_compressor_flow2.c". Every symbol is static cc2_* / cc2h_*.
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow2 \
 *           oracle_context_compressor_flow2.py "" "lib/libjson/json.o"
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

/* ── live module constants (byte-pinned to agent/context_compressor.py +
 *    session_rules/turns.py + agent/conversation_compression.py) ── */
#define CC2_PRUNE_MIN_CHARS 200
#define CC2_SALVAGE_SUMMARY_MAX_CHARS 8000
#define CC2_SALVAGE_KEEP_RECENT_TOOLS 2
#define CC2_COMPRESSED_SUMMARY_METADATA_KEY "_compressed_summary"
#define CC2_DB_PERSISTED_MARKER "_db_persisted"
#define CC2_PRUNED_TOOL_PLACEHOLDER "[Old tool output cleared to save context space]"
/* _SUMMARY_END_MARKER / _MERGED_PRIOR_CONTEXT_HEADER / notice header carry an
 * em dash (U+2014 = ef bb bf? no: e2 80 94); hex-escaped, split before any
 * following ASCII hex-digit char. */
#define CC2_SUMMARY_END_MARKER \
    "--- END OF CONTEXT SUMMARY \xe2\x80\x94 respond to the message below, not the summary above ---"
#define CC2_MERGED_PRIOR_CONTEXT_HEADER \
    "[PRIOR CONTEXT \xe2\x80\x94 for reference only; not a new message]"
#define CC2_PRUNED_SKILL_RELOAD_NOTICE_HEADER \
    "[Skills pruned during compression \xe2\x80\x94 reload before acting on these tasks]"
/* _STALE_REPLAY_PRUNE_KEYS = ("codex_reasoning_items",) */
static const char *CC2_STALE_REPLAY_PRUNE_KEYS[] = { "codex_reasoning_items", NULL };
/* _SYNTHETIC_USER_ROW_PREFIXES (tuple order preserved) */
static const char *CC2_SYNTHETIC_USER_ROW_PREFIXES[] = {
    "[System:", "[CONTEXT", "[PRIOR CONTEXT", "[IMPORTANT: Background",
    "[Your active task list", "[Planning state preserved", "[ASYNC DELEGATION",
    "[OUT-OF-BAND", "Cronjob Response:", NULL,
};

/* ── universal seam ── */
typedef struct cc2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cc2_io_t;

/* ── truthiness / accessors ── */
static int cc2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cc2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cc2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cc2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cc2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cc2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cc2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cc2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cc2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cc2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static json_t *cc2_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* isinstance(x, dict): a PLAIN object, never an __attr__ SimpleNamespace. */
static int cc2_is_plain_dict(const json_t *v)
{
    return cc2_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* getattr on an attribute double; any other carrier has no attributes. */
static const json_t *cc2_getattr(const json_t *v, const char *name)
{
    if (!cc2_is_obj(v)) return NULL;
    const json_t *inner = json_obj_get(v, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) return NULL;
    return json_obj_get(inner, name);
}

/* callable(x): only a {"__callable__": ...} double is callable here. */
static int cc2_is_callable(const json_t *v)
{
    return cc2_is_obj(v) && json_obj_get(v, "__callable__") != NULL;
}

static const char *cc2_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT:
            return json_obj_get(v, "__attr__") ? "types.SimpleNamespace" : "dict";
        default: return "object";
    }
}

/* ── raise specs ── */
static int cc2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cc2_kind(const json_t *r)
{
    const char *k = cc2_str(cc2_get(r, "__raise__"));
    return k ? k : "";
}

/* ── full builtin exception hierarchy (Python 3.14) ──
 * parent[kind] = immediate base name; IOError/EnvironmentError/WindowsError
 * are OSError aliases, py3.14 adds PythonFinalizationError(RuntimeError) and
 * _IncompleteInputError(SyntaxError).  ExceptionGroup inherits Exception (and
 * BaseExceptionGroup) so it stays inside `except Exception`; CancelledError is
 * asyncio.CancelledError, a BaseException since 3.8.  cc2_exc_is_base() walks
 * the chain to implement the "BaseException but not Exception" predicate the
 * same way live `except Exception` matches: an unknown/scripted custom class
 * is treated as an Exception descendant (not base-only), which preserves the
 * behaviour of the 24 defs in this port (none of them swallows a scripted
 * Exception; a scripted BaseException trio propagates past every guard).
 */
typedef struct { const char *name; const char *parent; } cc2_exc_edge_t;
static const cc2_exc_edge_t CC2_EXC_PARENT[] = {
    { "BaseException", NULL },
    { "SystemExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "GeneratorExit", "BaseException" },
    { "CancelledError", "BaseException" },          /* asyncio.CancelledError */
    { "BaseExceptionGroup", "BaseException" },
    { "Exception", "BaseException" },
    { "ExceptionGroup", "Exception" },              /* ExceptionGroup(Exception, BaseExceptionGroup) */
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
    { "EnvironmentError", "OSError" },              /* alias */
    { "IOError", "OSError" },                        /* alias */
    { "WindowsError", "OSError" },                   /* alias */
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
    { "PythonFinalizationError", "RuntimeError" },  /* py3.14 */
    { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" },
    { "TabError", "IndentationError" },
    { "_IncompleteInputError", "SyntaxError" },     /* py3.14 */
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

static const char *cc2_exc_parent(const char *kind)
{
    if (!kind || !*kind) return NULL;
    for (const cc2_exc_edge_t *e = CC2_EXC_PARENT; e->name; e++)
        if (strcmp(kind, e->name) == 0) return e->parent;
    return NULL;  /* unknown / scripted custom class */
}

/* True iff kind is a BaseException descendant that is NOT an Exception
 * descendant (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError
 * / BaseExceptionGroup and their ilk). */
static int cc2_exc_is_base(const char *kind)
{
    if (!kind || !*kind) return 0;
    const char *cur = kind;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, "Exception") == 0 || strcmp(cur, "ExceptionGroup") == 0) return 0;
        if (strcmp(cur, "BaseException") == 0) return 1;
        const char *p = cc2_exc_parent(cur);
        if (!p) return 0;   /* unknown custom class -> Exception-descendant */
        cur = p;
    }
    return 0;
}

static int cc2_is_base(const char *kind)
{
    return cc2_exc_is_base(kind);
}

static json_t *cc2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *cc2_xfmt(const char *fmt, ...)
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

static json_t *cc2_attr_raise(const json_t *v, const char *name)
{
    char *m = cc2_xfmt("'%s' object has no attribute '%s'", cc2_typename(v), name);
    json_t *r = cc2_mkraise("AttributeError", m ? m : "");
    free(m);
    return r;
}

/* ── universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. ── */
static json_t *cc2_call(cc2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cc2_xfmt("no seam installed for %s", name);
        r = cc2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cc2_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides (harness strips nulls
 * recursively; oracle norm_args drops None). */
static void cc2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void cc2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cc2_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void cc2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* ── string helpers (ASCII domain) ── */
/* str.strip() whitespace INCLUDES \x1c-\x1f. */
static int cc2_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
/* int() strips a NARROWER set: NOT \x1c-\x1f. */
static int cc2_int_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *cc2_strip_gen(const char *s, int (*isws)(unsigned char))
{
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isws((unsigned char)s[a])) a++;
    while (b > a && isws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static char *cc2_strip(const char *s) { return cc2_strip_gen(s, cc2_is_ws); }
static char *cc2_int_strip(const char *s) { return cc2_strip_gen(s, cc2_int_ws); }

static char *cc2_lstrip(const char *s)
{
    if (!s) s = "";
    size_t a = 0, n = strlen(s);
    while (a < n && cc2_is_ws((unsigned char)s[a])) a++;
    char *out = (char *)malloc(n - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, n - a);
    out[n - a] = '\0';
    return out;
}

static char *cc2_rstrip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s), b = n;
    while (b > 0 && cc2_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b + 1);
    if (!out) return NULL;
    memcpy(out, s, b);
    out[b] = '\0';
    return out;
}

static char *cc2_lower(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

static int cc2_startswith(const char *s, const char *p)
{
    if (!s || !p) return 0;
    return strncmp(s, p, strlen(p)) == 0;
}
static int cc2_endswith(const char *s, const char *p)
{
    if (!s || !p) return 0;
    size_t n = strlen(s), m = strlen(p);
    return n >= m && strcmp(s + n - m, p) == 0;
}
static int cc2_contains(const char *hay, const char *needle)
{
    return hay && needle && strstr(hay, needle) != NULL;
}

/* " ".join(s.split()): collapse ASCII whitespace runs to single spaces,
 * no leading/trailing space, no empty tokens. Caller frees. */
static char *cc2_split_join(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t k = 0;
    int in_tok = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (cc2_is_ws(c)) {
            if (in_tok) { out[k++] = ' '; in_tok = 0; }
        } else {
            out[k++] = (char)c;
            in_tok = 1;
        }
    }
    /* a trailing space was emitted when the last token ended on whitespace;
     * in_tok==0 means the last char written was a separator -> drop it. */
    if (k > 0 && !in_tok) k--;
    out[k] = '\0';
    return out;
}

/* Python str() projection (domain: str / integral number / bool / None).
 * Containers / non-integral floats are excluded from str() positions. */
static char *cc2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cc2_xfmt("None");
    if (v->type == JSON_BOOL) return cc2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) return cc2_xfmt("%.0f", d);
        return cc2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cc2_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);   /* excluded from fixtures */
    return s ? s : cc2_xfmt("");
}

/* int() string parser: optional sign, digits with underscores BETWEEN digits,
 * full consumption. Returns 1 on success. */
static int cc2_parse_int_str(const char *t, long long *out)
{
    if (!t) return 0;
    const char *p = t;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    const char *start = p;
    int digits = 0, prev_digit = 0;
    for (; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') { digits++; prev_digit = 1; }
        else if (c == '_') {
            if (!prev_digit) return 0;          /* underscore needs a digit before */
            if (!(p[1] >= '0' && p[1] <= '9')) return 0;  /* and after */
            prev_digit = 0;
        } else return 0;
    }
    if (!digits || p == start) return 0;
    /* build clean digit buffer */
    size_t n = (size_t)(p - start);
    char *clean = (char *)malloc(n + 2);
    if (!clean) return 0;
    size_t k = 0;
    if (neg) clean[k++] = '-';
    for (const char *q = start; q < p; q++) if (*q != '_') clean[k++] = *q;
    clean[k] = '\0';
    char *end = NULL;
    errno = 0;
    long long v = strtoll(clean, &end, 10);
    int ok = (end && *end == '\0' && end != clean && errno != ERANGE);
    free(clean);
    if (!ok) return 0;
    *out = v;
    return 1;
}

/* int(value) projection. *err set on failure (caller owns/propagates). */
static long long cc2_pyint(const json_t *v, json_t **err)
{
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = cc2_mkraise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)trunc(v->num_val);
    if (v->type == JSON_STRING) {
        char *t = cc2_int_strip(v->str_val ? v->str_val : "");
        long long d = 0;
        int ok = t && cc2_parse_int_str(t, &d);
        if (!ok) {
            char *m = cc2_xfmt("invalid literal for int() with base 10: '%s'",
                               v->str_val ? v->str_val : "");
            *err = cc2_mkraise("ValueError", m ? m : "");
            free(m);
        }
        free(t);
        return ok ? d : 0;
    }
    char *m = cc2_xfmt("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                       cc2_typename(v));
    *err = cc2_mkraise("TypeError", m ? m : "");
    free(m);
    return 0;
}

/* string concatenation guard: live `a + b` with a non-str raises TypeError.
 * Returns a malloc'd concat when both are strings, else sets *err. */
static char *cc2_concat_str(const json_t *a, const char *b, json_t **err)
{
    *err = NULL;
    if (!cc2_is_str(a)) {
        char *m = cc2_xfmt("unsupported operand type(s) for +: '%s' and 'str'",
                           cc2_typename(a));
        *err = cc2_mkraise("TypeError", m ? m : "");
        free(m);
        return NULL;
    }
    const char *as = a->str_val ? a->str_val : "";
    char *out = cc2_xfmt("%s%s", as, b ? b : "");
    return out;
}

/* ── _SUMMARY_REFUSAL_PREFIX_RE matcher (byte-pinned, IGNORECASE, ASCII) ──
 * Pattern (two anchored alternatives):
 *   ^\s*(?:(?:sorry|i(?:['’]m| am)\s+sorry|i\s+apologi[sz]e|as\s+an\s+ai)
 *        \s*[,;:]?\s*(?:but\s+)?)?(?:i|we)\s+
 *     (?:can(?:\s*not|['’]t)|could\s*not|couldn['’]t|won['’]t|will\s+not|
 *        must\s+decline|refuse\s+to|am\s+unable\s+to|am\s+not\s+able\s+to)\b
 *   |^\s*(?:i['’]?m|i\s+am)\s+(?:unable|not\s+able)\b
 * Transcribed as an ordered backtracking scanner; \s here is the re ASCII set
 * [ \t\n\r\f\v] (the matcher runs on the whitespace-normalized opener). The
 * U+2019 apostrophe (e2 80 99) branch is carried but never fires on ASCII. */
#include <errno.h>
#define CC2_FAIL SIZE_MAX

static int cc2_re_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static int cc2_re_word(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static size_t cc2_skip_ws(const char *s, size_t n, size_t p)
{
    while (p < n && cc2_re_ws((unsigned char)s[p])) p++;
    return p;
}
/* case-insensitive ASCII literal at p; returns end or FAIL. */
static size_t cc2_lit(const char *s, size_t n, size_t p, const char *lit)
{
    size_t L = strlen(lit);
    if (p + L > n) return CC2_FAIL;
    for (size_t i = 0; i < L; i++) {
        unsigned char a = (unsigned char)s[p + i], b = (unsigned char)lit[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return CC2_FAIL;
    }
    return p + L;
}
/* \s+ : require >=1 ws then skip all; returns end or FAIL. */
static size_t cc2_wsplus(const char *s, size_t n, size_t p)
{
    if (p >= n || !cc2_re_ws((unsigned char)s[p])) return CC2_FAIL;
    return cc2_skip_ws(s, n, p);
}
/* a quote char: ASCII ' (len 1) or U+2019 (len 3); returns byte length or 0. */
static size_t cc2_quote_len(const char *s, size_t n, size_t p)
{
    if (p < n && s[p] == '\'') return 1;
    if (p + 2 < n && (unsigned char)s[p] == 0xe2
        && (unsigned char)s[p + 1] == 0x80 && (unsigned char)s[p + 2] == 0x99) return 3;
    return 0;
}
/* \b after a modal ending in a letter: next char non-word or end. */
static int cc2_boundary(const char *s, size_t n, size_t p)
{
    return p == n || !cc2_re_word((unsigned char)s[p]);
}

/* one modal branch matched at p -> end (before \b); FAIL if no match. */
static size_t cc2_modal_one(const char *s, size_t n, size_t p)
{
    size_t q, r, e;
    /* can(?:\s*not|['’]t) */
    q = cc2_lit(s, n, p, "can");
    if (q != CC2_FAIL) {
        r = cc2_skip_ws(s, n, q);
        e = cc2_lit(s, n, r, "not");
        if (e != CC2_FAIL) return e;
        size_t ql = cc2_quote_len(s, n, q);
        if (ql) { e = cc2_lit(s, n, q + ql, "t"); if (e != CC2_FAIL) return e; }
    }
    /* could\s*not */
    q = cc2_lit(s, n, p, "could");
    if (q != CC2_FAIL) { r = cc2_skip_ws(s, n, q); e = cc2_lit(s, n, r, "not"); if (e != CC2_FAIL) return e; }
    /* couldn['’]t */
    q = cc2_lit(s, n, p, "couldn");
    if (q != CC2_FAIL) { size_t ql = cc2_quote_len(s, n, q); if (ql) { e = cc2_lit(s, n, q + ql, "t"); if (e != CC2_FAIL) return e; } }
    /* won['’]t */
    q = cc2_lit(s, n, p, "won");
    if (q != CC2_FAIL) { size_t ql = cc2_quote_len(s, n, q); if (ql) { e = cc2_lit(s, n, q + ql, "t"); if (e != CC2_FAIL) return e; } }
    /* will\s+not */
    q = cc2_lit(s, n, p, "will");
    if (q != CC2_FAIL) { r = cc2_wsplus(s, n, q); if (r != CC2_FAIL) { e = cc2_lit(s, n, r, "not"); if (e != CC2_FAIL) return e; } }
    /* must\s+decline */
    q = cc2_lit(s, n, p, "must");
    if (q != CC2_FAIL) { r = cc2_wsplus(s, n, q); if (r != CC2_FAIL) { e = cc2_lit(s, n, r, "decline"); if (e != CC2_FAIL) return e; } }
    /* refuse\s+to */
    q = cc2_lit(s, n, p, "refuse");
    if (q != CC2_FAIL) { r = cc2_wsplus(s, n, q); if (r != CC2_FAIL) { e = cc2_lit(s, n, r, "to"); if (e != CC2_FAIL) return e; } }
    /* am\s+unable\s+to */
    q = cc2_lit(s, n, p, "am");
    if (q != CC2_FAIL) {
        r = cc2_wsplus(s, n, q);
        if (r != CC2_FAIL) {
            size_t u = cc2_lit(s, n, r, "unable");
            if (u != CC2_FAIL) { size_t w = cc2_wsplus(s, n, u); if (w != CC2_FAIL) { e = cc2_lit(s, n, w, "to"); if (e != CC2_FAIL) return e; } }
            /* am\s+not\s+able\s+to */
            size_t nn = cc2_lit(s, n, r, "not");
            if (nn != CC2_FAIL) {
                size_t w2 = cc2_wsplus(s, n, nn);
                if (w2 != CC2_FAIL) {
                    size_t ab = cc2_lit(s, n, w2, "able");
                    if (ab != CC2_FAIL) { size_t w3 = cc2_wsplus(s, n, ab); if (w3 != CC2_FAIL) { e = cc2_lit(s, n, w3, "to"); if (e != CC2_FAIL) return e; } }
                }
            }
        }
    }
    return CC2_FAIL;
}

/* (?:i|we)\s+MODAL\b at p */
static int cc2_iwe_modal(const char *s, size_t n, size_t p)
{
    static const char *prons[] = { "i", "we" };
    for (int k = 0; k < 2; k++) {
        size_t q = cc2_lit(s, n, p, prons[k]);
        if (q == CC2_FAIL) continue;
        size_t r = cc2_wsplus(s, n, q);
        if (r == CC2_FAIL) continue;
        size_t e = cc2_modal_one(s, n, r);
        if (e != CC2_FAIL && cc2_boundary(s, n, e)) return 1;
    }
    return 0;
}

/* optional preamble end candidates at p (after P1 \s* [,;:]? \s* (but\s+)?). */
static size_t cc2_preamble_ends(const char *s, size_t n, size_t p, size_t *ends, size_t cap)
{
    size_t cnt = 0;
    size_t p1[8]; size_t p1n = 0;
    size_t q, r, e;
    /* sorry */
    e = cc2_lit(s, n, p, "sorry"); if (e != CC2_FAIL && p1n < 8) p1[p1n++] = e;
    /* i(?:['’]m| am)\s+sorry */
    q = cc2_lit(s, n, p, "i");
    if (q != CC2_FAIL) {
        size_t ql = cc2_quote_len(s, n, q);
        size_t inner = CC2_FAIL;
        if (ql) inner = cc2_lit(s, n, q + ql, "m");
        if (inner == CC2_FAIL) inner = cc2_lit(s, n, q, " am");
        if (inner != CC2_FAIL) {
            r = cc2_wsplus(s, n, inner);
            if (r != CC2_FAIL) { e = cc2_lit(s, n, r, "sorry"); if (e != CC2_FAIL && p1n < 8) p1[p1n++] = e; }
        }
        /* i\s+apologi[sz]e */
        r = cc2_wsplus(s, n, q);
        if (r != CC2_FAIL) {
            size_t a = cc2_lit(s, n, r, "apologi");
            if (a != CC2_FAIL && a < n && (s[a] == 's' || s[a] == 'z')) {
                e = cc2_lit(s, n, a + 1, "e"); if (e != CC2_FAIL && p1n < 8) p1[p1n++] = e;
            }
        }
    }
    /* as\s+an\s+ai */
    q = cc2_lit(s, n, p, "as");
    if (q != CC2_FAIL) {
        r = cc2_wsplus(s, n, q);
        if (r != CC2_FAIL) {
            size_t an = cc2_lit(s, n, r, "an");
            if (an != CC2_FAIL) { size_t w = cc2_wsplus(s, n, an); if (w != CC2_FAIL) { e = cc2_lit(s, n, w, "ai"); if (e != CC2_FAIL && p1n < 8) p1[p1n++] = e; } }
        }
    }
    for (size_t i = 0; i < p1n; i++) {
        size_t e1 = cc2_skip_ws(s, n, p1[i]);
        size_t e2s[2]; size_t e2n = 0;
        e2s[e2n++] = e1;
        if (e1 < n && (s[e1] == ',' || s[e1] == ';' || s[e1] == ':'))
            e2s[e2n++] = cc2_skip_ws(s, n, e1 + 1);
        for (size_t j = 0; j < e2n; j++) {
            size_t e2 = e2s[j];
            if (cnt < cap) ends[cnt++] = e2;                 /* no but */
            size_t b = cc2_lit(s, n, e2, "but");
            if (b != CC2_FAIL && b < n && cc2_re_ws((unsigned char)s[b])) {
                size_t e3 = cc2_skip_ws(s, n, b);
                if (cnt < cap) ends[cnt++] = e3;             /* but\s+ */
            }
        }
    }
    return cnt;
}

static int cc2_alt1(const char *s, size_t n)
{
    size_t p = cc2_skip_ws(s, n, 0);
    size_t ends[32];
    size_t cnt = cc2_preamble_ends(s, n, p, ends, 32);
    for (size_t i = 0; i < cnt; i++)
        if (cc2_iwe_modal(s, n, ends[i])) return 1;
    return cc2_iwe_modal(s, n, p);   /* no preamble */
}

static int cc2_alt2(const char *s, size_t n)
{
    size_t p = cc2_skip_ws(s, n, 0);
    size_t ends[2]; size_t en = 0;
    size_t q = cc2_lit(s, n, p, "i");
    if (q != CC2_FAIL) {
        /* i['’]?m */
        size_t ql = cc2_quote_len(s, n, q);
        size_t m = cc2_lit(s, n, q + ql, "m");
        if (m != CC2_FAIL && en < 2) ends[en++] = m;
        /* i\s+am */
        size_t r = cc2_wsplus(s, n, q);
        if (r != CC2_FAIL) { size_t am = cc2_lit(s, n, r, "am"); if (am != CC2_FAIL && en < 2) ends[en++] = am; }
    }
    for (size_t i = 0; i < en; i++) {
        size_t r = cc2_wsplus(s, n, ends[i]);
        if (r == CC2_FAIL) continue;
        size_t u = cc2_lit(s, n, r, "unable");
        if (u != CC2_FAIL && cc2_boundary(s, n, u)) return 1;
        size_t nn = cc2_lit(s, n, r, "not");
        if (nn != CC2_FAIL) {
            size_t w = cc2_wsplus(s, n, nn);
            if (w != CC2_FAIL) { size_t ab = cc2_lit(s, n, w, "able"); if (ab != CC2_FAIL && cc2_boundary(s, n, ab)) return 1; }
        }
    }
    return 0;
}

static int cc2_refusal_match(const char *s)
{
    if (!s) return 0;
    size_t n = strlen(s);
    return cc2_alt1(s, n) || cc2_alt2(s, n);
}

/* re.search(r"(?m)^##\s", content): a line start (0 or after \n) with "##"
 * then a re-whitespace char. */
static int cc2_has_heading(const char *s)
{
    if (!s) return 0;
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        if (i != 0 && s[i - 1] != '\n') continue;
        if (i + 2 < n && s[i] == '#' && s[i + 1] == '#' && cc2_re_ws((unsigned char)s[i + 2]))
            return 1;
    }
    return 0;
}

/* ── refusal / status cores ── */

/* _is_summary_refusal(content): content is a str (caller guards). */
static int cc2h_is_summary_refusal(const char *content)
{
    char *normalized = cc2_split_join(content);
    if (!normalized) return 0;
    int matched = cc2_refusal_match(normalized);
    if (!matched) { free(normalized); return 0; }
    if (cc2_has_heading(content)) { free(normalized); return 0; }
    /* normalized[:400].casefold() contains "summar" or "checkpoint" */
    size_t n = strlen(normalized);
    size_t lim = n < 400 ? n : 400;
    char *head = (char *)malloc(lim + 1);
    int found = 0;
    if (head) {
        memcpy(head, normalized, lim);
        head[lim] = '\0';
        char *low = cc2_lower(head);
        if (low) {
            found = cc2_contains(low, "summar") || cc2_contains(low, "checkpoint");
            free(low);
        }
        free(head);
    }
    free(normalized);
    return found;
}

/* _exc_status_code(exc): getattr(exc,"status_code",None) or
 * getattr(getattr(exc,"response",None),"status_code",None). */
static json_t *cc2h_exc_status_code(const json_t *exc)
{
    const json_t *v1 = cc2_getattr(exc, "status_code");
    if (cc2_truth(v1)) return json_copy(v1);
    const json_t *resp = cc2_getattr(exc, "response");
    const json_t *v2 = cc2_getattr(resp, "status_code");
    return cc2_copy_or_null(v2);
}

/* _response_refusal_text(response): two seams then dict/str projection. */
static json_t *cc2h_response_refusal_text(cc2_io_t *io, const json_t *response, json_t **err)
{
    *err = NULL;
    json_t *a1 = cc2_args();
    cc2_arg_j(a1, "response", response);
    json_t *msg = cc2_call(io, "_coerce_llm_message", a1);
    if (cc2_is_raise(msg)) { *err = msg; return NULL; }
    json_t *a2 = cc2_args();
    cc2_arg_j(a2, "msg", msg);
    cc2_arg_s(a2, "name", "refusal");
    json_t *refusal = cc2_call(io, "_message_field", a2);
    json_free(msg);
    if (cc2_is_raise(refusal)) { *err = refusal; return NULL; }
    /* if isinstance(refusal, dict): message or reason or text */
    if (cc2_is_plain_dict(refusal)) {
        const json_t *v = cc2_get(refusal, "message");
        if (!cc2_truth(v)) v = cc2_get(refusal, "reason");
        if (!cc2_truth(v)) v = cc2_get(refusal, "text");
        json_t *nr = cc2_copy_or_null(v);
        json_free(refusal);
        refusal = nr;
    }
    json_t *ret;
    if (cc2_is_str(refusal)) {
        char *t = cc2_strip(refusal->str_val ? refusal->str_val : "");
        ret = json_string(t ? t : "");
        free(t);
    } else {
        ret = json_string("");
    }
    json_free(refusal);
    return ret;
}

/* ── small comparison helpers ── */
static int cc2_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static int cc2_contains_n(const char *hay, size_t haylen, const char *needle)
{
    size_t nl = strlen(needle);
    if (nl == 0) return 1;
    if (haylen < nl) return 0;
    for (size_t i = 0; i + nl <= haylen; i++)
        if (memcmp(hay + i, needle, nl) == 0) return 1;
    return 0;
}

/* ── checkpoint / sidecar cores ── */
static int cc2h_is_checkpoint_item(const json_t *item)
{
    return cc2_is_plain_dict(item) && cc2_str_eq(cc2_str(cc2_get(item, "type")), "compaction");
}

static long long cc2h_newest_checkpoint_carrier(const json_t *messages, const char *key)
{
    if (!cc2_is_arr(messages)) return -1;
    size_t n = json_len(messages);
    for (size_t k = n; k > 0; k--) {
        size_t i = k - 1;
        const json_t *msg = json_get(messages, i);
        if (!cc2_is_plain_dict(msg)) continue;
        if (!cc2_str_eq(cc2_str(cc2_get(msg, "role")), "assistant")) continue;
        const json_t *items = cc2_get(msg, key);
        if (cc2_is_arr(items)) {
            for (size_t j = 0; j < json_len(items); j++)
                if (cc2h_is_checkpoint_item(json_get(items, j))) return (long long)i;
        }
    }
    return -1;
}

/* kept is OWNED (transferred into msg or freed). */
static void cc2h_set_sidecar(json_t *msg, const char *key, json_t *kept)
{
    if (cc2_truth(kept)) {
        json_set(msg, key, kept);
    } else {
        json_free(kept);
        json_obj_del(msg, key);
    }
}

static json_t *cc2h_drop_shadowed_checkpoints(json_t *messages, const char *key, const json_t *before)
{
    long long newest = cc2h_newest_checkpoint_carrier(messages, key);
    long long stop = newest;
    if (!cc2_is_null(before)) {
        long long b = (long long)cc2_num(before);
        if (b < stop) stop = b;
    }
    if (stop < 0) stop = 0;
    json_t *rewritten = json_array();
    for (long long i = 0; i < stop; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!cc2_is_plain_dict(msg)) continue;
        if (!cc2_str_eq(cc2_str(cc2_get(msg, "role")), "assistant")) continue;
        const json_t *items = cc2_get(msg, key);
        if (!cc2_is_arr(items)) continue;
        int any = 0;
        for (size_t j = 0; j < json_len(items); j++)
            if (cc2h_is_checkpoint_item(json_get(items, j))) { any = 1; break; }
        if (!any) continue;
        json_t *kept = json_array();
        for (size_t j = 0; j < json_len(items); j++) {
            const json_t *item = json_get(items, j);
            if (!cc2h_is_checkpoint_item(item)) json_append(kept, json_copy(item));
        }
        cc2h_set_sidecar(msg, key, kept);
        json_append(rewritten, json_number((double)i));
    }
    return rewritten;
}

static long long cc2h_last_index_with_role(const json_t *messages, const char *role)
{
    if (!cc2_is_arr(messages)) return -1;
    size_t n = json_len(messages);
    for (size_t k = n; k > 0; k--) {
        size_t i = k - 1;
        const json_t *m = json_get(messages, i);
        if (cc2_is_plain_dict(m) && cc2_str_eq(cc2_str(cc2_get(m, "role")), role))
            return (long long)i;
    }
    return -1;
}

static long long cc2h_prune_stale_reasoning_replay(json_t *messages)
{
    long long last_user = cc2h_last_index_with_role(messages, "user");
    if (last_user < 0) return 0;
    json_t *before = json_number((double)last_user);
    /* pruned = set(); update with each key's rewritten indices; len(pruned). */
    long long *seen = NULL; size_t seen_n = 0, seen_cap = 0;
    for (int k = 0; CC2_STALE_REPLAY_PRUNE_KEYS[k]; k++) {
        json_t *dropped = cc2h_drop_shadowed_checkpoints(messages, CC2_STALE_REPLAY_PRUNE_KEYS[k], before);
        for (size_t j = 0; j < json_len(dropped); j++) {
            long long idx = (long long)cc2_num(json_get(dropped, j));
            int dup = 0;
            for (size_t s = 0; s < seen_n; s++) if (seen[s] == idx) { dup = 1; break; }
            if (!dup) {
                if (seen_n == seen_cap) { seen_cap = seen_cap ? seen_cap * 2 : 8; seen = (long long *)realloc(seen, seen_cap * sizeof(long long)); }
                seen[seen_n++] = idx;
            }
        }
        json_free(dropped);
    }
    json_free(before);
    free(seen);
    return (long long)seen_n;
}

static int cc2h_looks_like_compaction_summary(const json_t *msg, const char *content)
{
    const char *role = cc2_str(cc2_get(msg, "role"));
    char *rs = cc2_rstrip(content);
    int endsw = cc2_endswith(rs, CC2_SUMMARY_END_MARKER);
    free(rs);
    if (!endsw) return 0;
    if (cc2_startswith(content, CC2_MERGED_PRIOR_CONTEXT_HEADER)) return 0;
    if (cc2_str_eq(role, "tool")) return 0;
    const json_t *meta = cc2_get(msg, CC2_COMPRESSED_SUMMARY_METADATA_KEY);
    if ((cc2_str_eq(role, "user") || cc2_str_eq(role, "assistant")) && !cc2_truth(meta)) return 0;
    size_t n = strlen(content);
    size_t lim = n < 280 ? n : 280;
    int head_hit = cc2_contains_n(content, lim, "CONTEXT COMPACTION")
                || cc2_contains_n(content, lim, "Conversation Summary");
    return cc2_truth(meta) || head_hit;
}

static void cc2h_salvage_reduce_todo_snapshot(json_t *out)
{
    if (!cc2_is_arr(out)) return;
    size_t n = json_len(out);
    for (size_t k = n; k > 0; k--) {
        size_t i = k - 1;
        json_t *msg = json_get(out, i);
        if (!cc2_is_plain_dict(msg)) continue;
        if (!(cc2_truth(cc2_get(msg, "_todo_snapshot_synthetic"))
              && cc2_str_eq(cc2_str(cc2_get(msg, "role")), "user"))) continue;
        const json_t *content = cc2_get(msg, "content");
        const char *cs = cc2_str(content);
        const char *hit = cs ? strstr(cs, CC2_PRUNED_SKILL_RELOAD_NOTICE_HEADER) : NULL;
        if (hit) {
            json_set(msg, "content", json_string(hit));
        } else {
            json_array_remove(out, i);
        }
        return;
    }
}

/* ── failure-suffix chain cores ── */
static json_t *cc2h_json_dict(const json_t *text)
{
    if (!cc2_truth(text)) return json_object();
    if (!cc2_is_str(text)) return json_object();   /* json.loads TypeError caught */
    char *err = NULL;
    json_t *parsed = json_parse(text->str_val ? text->str_val : "", &err);
    free(err);
    if (!parsed) return json_object();
    if (cc2_is_plain_dict(parsed)) return parsed;
    json_free(parsed);
    return json_object();
}

static json_t *cc2h_failure_suffix(const json_t *reason, const char *label)
{
    char *preview;
    if (!cc2_truth(reason)) {
        preview = cc2_xfmt("");
    } else {
        char *s = cc2_pystr(reason);
        char *sj = cc2_split_join(s ? s : "");
        free(s);
        size_t n = strlen(sj ? sj : "");
        size_t lim = n < 80 ? n : 80;
        preview = (char *)malloc(lim + 1);
        if (preview) { memcpy(preview, sj ? sj : "", lim); preview[lim] = '\0'; }
        free(sj);
    }
    json_t *ret;
    char *m;
    if (preview && preview[0]) m = cc2_xfmt(" %s: %s", label, preview);
    else m = cc2_xfmt(" %s", label);
    ret = json_string(m ? m : "");
    free(m);
    free(preview);
    return ret;
}

static json_t *cc2h_payload_failure_suffix(const json_t *payload)
{
    const json_t *error = cc2_get(payload, "error");
    const json_t *success = cc2_get(payload, "success");
    int success_false = cc2_is_bool(success) && !success->bool_val;
    if (!cc2_truth(error) && !success_false) return json_string("");
    return cc2h_failure_suffix(error, "FAILED");
}

static json_t *cc2h_result_failure_suffix(const json_t *content)
{
    json_t *d = cc2h_json_dict(content);
    json_t *r = cc2h_payload_failure_suffix(d);
    json_free(d);
    return r;
}

/* numeric coercion for a comparison operand; sets *err (TypeError) when the
 * value is not a number/bool. op is the live operator text ("<=", ">=", "<"). */
static int cc2_as_number(const json_t *v, double *out, json_t **err, const char *op)
{
    *err = NULL;
    if (cc2_is_num(v)) { *out = v->num_val; return 1; }
    if (cc2_is_bool(v)) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    char *m = cc2_xfmt("'%s' not supported between instances of '%s' and 'int'",
                       op, cc2_typename(v));
    *err = cc2_mkraise("TypeError", m ? m : "");
    free(m);
    return 0;
}

/* ── salvage_grown_transcript core ── */
static json_t *cc2h_salvage_grown_transcript(cc2_io_t *io, const json_t *original,
                                             const json_t *candidate, const json_t *budget_node,
                                             json_t **err)
{
    *err = NULL;
    if (!cc2_truth(candidate) || !cc2_truth(original)) return json_null();

    json_t *budget = NULL; int budget_owned = 0;
    if (cc2_is_null(budget_node)) {
        json_t *a = cc2_args();
        cc2_arg_j(a, "messages", original);
        budget = cc2_call(io, "estimate_messages_tokens_rough", a);
        if (cc2_is_raise(budget)) { *err = budget; return NULL; }
        budget_owned = 1;
    } else {
        budget = (json_t *)budget_node;
    }
    double bnum;
    if (!cc2_as_number(budget, &bnum, err, "<=")) {
        if (budget_owned) json_free(budget);
        return NULL;
    }
    if (bnum <= 0) { if (budget_owned) json_free(budget); return json_null(); }

    /* out = [dict(msg) if isinstance(msg, dict) else msg for msg in candidate]
     * (shallow live; all mutations below are top-level key writes, so a deep
     * copy is observationally identical — candidate is not emitted). */
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(candidate); i++)
        json_append(out, json_copy(json_get(candidate, i)));

    /* tool_indices; keep_tools = set(tool_indices[-2:]) */
    long long ti[256]; size_t ti_n = 0;
    for (size_t i = 0; i < json_len(out); i++) {
        const json_t *msg = json_get(out, i);
        if (cc2_is_plain_dict(msg) && cc2_str_eq(cc2_str(cc2_get(msg, "role")), "tool"))
            if (ti_n < 256) ti[ti_n++] = (long long)i;
    }
    long long keep[CC2_SALVAGE_KEEP_RECENT_TOOLS]; size_t keep_n = 0;
    for (size_t i = (ti_n > CC2_SALVAGE_KEEP_RECENT_TOOLS ? ti_n - CC2_SALVAGE_KEEP_RECENT_TOOLS : 0); i < ti_n; i++)
        keep[keep_n++] = ti[i];

    for (size_t index = 0; index < json_len(out); index++) {
        json_t *msg = json_get(out, index);
        if (!cc2_is_plain_dict(msg)) continue;
        const char *role = cc2_str(cc2_get(msg, "role"));
        int kept = 0;
        for (size_t k = 0; k < keep_n; k++) if (keep[k] == (long long)index) kept = 1;
        if (cc2_str_eq(role, "tool") && !kept) {
            const json_t *content = cc2_get(msg, "content");
            const char *cs = cc2_str(content);
            if (cs && strlen(cs) > CC2_PRUNE_MIN_CHARS)
                json_set(msg, "content", json_string(CC2_PRUNED_TOOL_PLACEHOLDER));
        }
        const json_t *content = cc2_get(msg, "content");
        const char *cs = cc2_str(content);
        if (cs && strlen(cs) > CC2_SALVAGE_SUMMARY_MAX_CHARS
            && cc2h_looks_like_compaction_summary(msg, cs)) {
            json_t *a = cc2_args();
            cc2_arg_s(a, "content", cs);
            cc2_arg_n(a, "limit", CC2_SALVAGE_SUMMARY_MAX_CHARS);
            json_t *elided = cc2_call(io, "elide_keeping_skill_safety_rule", a);
            if (cc2_is_raise(elided)) { *err = elided; json_free(out); if (budget_owned) json_free(budget); return NULL; }
            json_t *cerr = NULL;
            char *cat = cc2_concat_str(elided, "\n\n" CC2_SUMMARY_END_MARKER, &cerr);
            json_free(elided);
            if (cerr) { *err = cerr; json_free(out); if (budget_owned) json_free(budget); return NULL; }
            json_set(msg, "content", json_string(cat ? cat : ""));
            free(cat);
        }
    }
    cc2h_prune_stale_reasoning_replay(out);

    json_t *a = cc2_args();
    cc2_arg_j(a, "messages", out);
    json_t *est = cc2_call(io, "estimate_messages_tokens_rough", a);
    if (cc2_is_raise(est)) { *err = est; json_free(out); if (budget_owned) json_free(budget); return NULL; }
    double en;
    if (!cc2_as_number(est, &en, err, ">=")) { json_free(est); json_free(out); if (budget_owned) json_free(budget); return NULL; }
    json_free(est);
    if (en >= bnum) cc2h_salvage_reduce_todo_snapshot(out);

    int has_user = 0;
    for (size_t i = 0; i < json_len(out); i++) {
        const json_t *m = json_get(out, i);
        if (cc2_is_plain_dict(m) && cc2_str_eq(cc2_str(cc2_get(m, "role")), "user")) { has_user = 1; break; }
    }
    json_t *ret = NULL;
    if (has_user) {
        json_t *a2 = cc2_args();
        cc2_arg_j(a2, "messages", out);
        json_t *est2 = cc2_call(io, "estimate_messages_tokens_rough", a2);
        if (cc2_is_raise(est2)) { *err = est2; json_free(out); if (budget_owned) json_free(budget); return NULL; }
        double e2;
        if (!cc2_as_number(est2, &e2, err, "<")) { json_free(est2); json_free(out); if (budget_owned) json_free(budget); return NULL; }
        json_free(est2);
        if (e2 < bnum) ret = out;
        else { json_free(out); ret = json_null(); }
    } else {
        json_free(out);
        ret = json_null();
    }
    if (budget_owned) json_free(budget);
    return ret;
}

/* ── _archive_watermark_for core ── */
static json_t *cc2h_archive_watermark_for(cc2_io_t *io, const json_t *session_db,
                                          const char *session_id, const json_t *held,
                                          const json_t *start_watermark, const json_t *persistence,
                                          json_t **err)
{
    *err = NULL;
    json_t *sw = cc2_copy_or_null(start_watermark);   /* owned working watermark */
    if (!cc2_is_null(persistence)) {
        if (cc2_is_null(sw)) {
            const json_t *msgs = cc2_getattr(persistence, "messages");
            (void)msgs;   /* active_watermark enters through the seam below */
            json_t *a = cc2_args();
            cc2_arg_s(a, "session_id", session_id);
            json_t *r = cc2_call(io, "persistence.messages.active_watermark", a);
            if (cc2_is_raise(r)) { *err = r; json_free(sw); return NULL; }
            const json_t *la = cc2_getattr(r, "last_active");
            if (!cc2_is_obj(r) || json_obj_get(r, "__attr__") == NULL || la == NULL) {
                *err = cc2_attr_raise(r, "last_active");
                json_free(r); json_free(sw); return NULL;
            }
            json_t *val = cc2_truth(la) ? json_copy(la) : json_number(0);
            json_free(r);
            json_t *ierr = NULL;
            long long iv = cc2_pyint(val, &ierr);
            json_free(val);
            if (ierr) { *err = ierr; json_free(sw); return NULL; }
            json_free(sw);
            sw = json_number((double)iv);
        }
    } else {
        const json_t *wof = cc2_getattr(session_db, "get_active_message_watermark");
        const json_t *gmr = cc2_getattr(session_db, "get_message_role");
        if (!cc2_is_callable(wof) || !cc2_is_callable(gmr)) { json_free(sw); return json_null(); }
        if (cc2_is_null(sw)) {
            json_t *a = cc2_args();
            cc2_arg_s(a, "session_id", session_id);
            json_t *w = cc2_call(io, "session_db.get_active_message_watermark", a);
            if (cc2_is_raise(w)) { *err = w; json_free(sw); return NULL; }
            json_free(sw);
            sw = w;   /* raw (may be None) */
        }
    }
    json_t *a = cc2_args();
    cc2_arg_j(a, "session_db", session_db);
    cc2_arg_s(a, "session_id", session_id);
    cc2_arg_j(a, "watermark", sw);
    cc2_arg_j(a, "messages", held);
    cc2_arg_b(a, "stale_raises", 1);
    cc2_arg_j(a, "persistence", persistence);
    json_free(sw);
    json_t *r = cc2_call(io, "held_archive_watermark", a);
    if (cc2_is_raise(r)) { *err = r; return NULL; }
    return r;
}

/* ===========================================================================
 * ENTRIES — json_t *cc2_<fn>(cc2_io_t *io, const json_t *in, json_t *st)
 * ret carries the value OR a raise spec; st is the mutated state window.
 * =========================================================================== */

/* PoP: cc2_is_checkpoint_item @ agent/context_compressor.py:_is_checkpoint_item */
static json_t *cc2_is_checkpoint_item(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return json_bool(cc2h_is_checkpoint_item(cc2_get(in, "item")));
}

/* PoP: cc2_newest_checkpoint_carrier @ agent/context_compressor.py:_newest_checkpoint_carrier */
static json_t *cc2_newest_checkpoint_carrier(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *key = json_get_str(in, "key", "codex_reasoning_items");
    return json_number((double)cc2h_newest_checkpoint_carrier(cc2_get(in, "messages"), key));
}

/* PoP: cc2_set_sidecar @ agent/context_compressor.py:_set_sidecar */
static json_t *cc2_set_sidecar(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    json_t *msg = cc2_get(st, "msg");
    const char *key = json_get_str(in, "key", "");
    json_t *kept = json_copy(cc2_get(in, "kept"));
    if (!kept) kept = json_array();
    cc2h_set_sidecar(msg, key, kept);
    return json_null();
}

/* PoP: cc2_drop_shadowed_checkpoints @ agent/context_compressor.py:drop_shadowed_checkpoints */
static json_t *cc2_drop_shadowed_checkpoints(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *key = json_get_str(in, "key", "codex_reasoning_items");
    return cc2h_drop_shadowed_checkpoints(cc2_get(st, "messages"), key, cc2_get(in, "before"));
}

/* PoP: cc2_last_index_with_role @ agent/context_compressor.py:_last_index_with_role */
static json_t *cc2_last_index_with_role(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *role = json_get_str(in, "role", "");
    return json_number((double)cc2h_last_index_with_role(cc2_get(in, "messages"), role));
}

/* PoP: cc2_last_assistant_index @ agent/context_compressor.py:_last_assistant_index */
static json_t *cc2_last_assistant_index(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return json_number((double)cc2h_last_index_with_role(cc2_get(in, "messages"), "assistant"));
}

/* PoP: cc2_prune_stale_reasoning_replay @ agent/context_compressor.py:_prune_stale_reasoning_replay */
static json_t *cc2_prune_stale_reasoning_replay(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    return json_number((double)cc2h_prune_stale_reasoning_replay(cc2_get(st, "messages")));
}

/* PoP: cc2_stamp_db_persisted_markers @ agent/context_compressor.py:stamp_db_persisted_markers */
static json_t *cc2_stamp_db_persisted_markers(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *messages = cc2_get(st, "messages");
    if (cc2_is_arr(messages))
        for (size_t i = 0; i < json_len(messages); i++) {
            json_t *msg = json_get(messages, i);
            if (cc2_is_plain_dict(msg)) json_set(msg, CC2_DB_PERSISTED_MARKER, json_bool(true));
        }
    return json_null();
}

/* PoP: cc2_looks_like_compaction_summary @ agent/context_compressor.py:_looks_like_compaction_summary */
static json_t *cc2_looks_like_compaction_summary(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *content = cc2_get(in, "content");
    if (!cc2_is_str(content)) return cc2_attr_raise(content, "rstrip");
    return json_bool(cc2h_looks_like_compaction_summary(cc2_get(in, "msg"), content->str_val));
}

/* PoP: cc2_salvage_reduce_todo_snapshot @ agent/context_compressor.py:_salvage_reduce_todo_snapshot */
static json_t *cc2_salvage_reduce_todo_snapshot(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    cc2h_salvage_reduce_todo_snapshot(cc2_get(st, "out"));
    return json_null();
}

/* PoP: cc2_salvage_grown_transcript @ agent/context_compressor.py:salvage_grown_transcript */
static json_t *cc2_salvage_grown_transcript(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *err = NULL;
    json_t *r = cc2h_salvage_grown_transcript(io, cc2_get(in, "original"), cc2_get(in, "candidate"),
                                              cc2_get(in, "budget"), &err);
    return err ? err : r;
}

/* PoP: cc2_archive_watermark_for @ agent/context_compressor.py:_archive_watermark_for */
static json_t *cc2_archive_watermark_for(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *err = NULL;
    const char *sid = json_get_str(in, "session_id", "");
    json_t *r = cc2h_archive_watermark_for(io, cc2_get(in, "session_db"), sid, cc2_get(in, "held"),
                                           cc2_get(in, "start_watermark"), cc2_get(in, "persistence"), &err);
    return err ? err : r;
}

/* PoP: cc2_exc_status_code @ agent/context_compressor.py:_exc_status_code */
static json_t *cc2_exc_status_code(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    json_t *r = cc2h_exc_status_code(cc2_get(in, "exc"));
    return r ? r : json_null();
}

/* PoP: cc2_is_summary_refusal @ agent/context_compressor.py:_is_summary_refusal */
static json_t *cc2_is_summary_refusal(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *content = cc2_get(in, "content");
    if (!cc2_is_str(content)) return cc2_attr_raise(content, "split");
    return json_bool(cc2h_is_summary_refusal(content->str_val));
}

/* PoP: cc2_response_refusal_text @ agent/context_compressor.py:_response_refusal_text */
static json_t *cc2_response_refusal_text(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *err = NULL;
    json_t *r = cc2h_response_refusal_text(io, cc2_get(in, "response"), &err);
    return err ? err : r;
}

/* PoP: cc2_is_refusal_response @ agent/context_compressor.py:_is_refusal_response */
static json_t *cc2_is_refusal_response(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *err = NULL;
    json_t *t = cc2h_response_refusal_text(io, cc2_get(in, "response"), &err);
    if (err) return err;
    if (cc2_truth(t)) { json_free(t); return json_bool(true); }
    json_free(t);
    const json_t *content = cc2_get(in, "content");
    if (!cc2_is_str(content)) return cc2_attr_raise(content, "split");
    return json_bool(cc2h_is_summary_refusal(content->str_val));
}

/* PoP: cc2_is_summary_stub @ agent/context_compressor.py:_is_summary_stub */
static json_t *cc2_is_summary_stub(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *a = cc2_args();
    cc2_arg_j(a, "content", cc2_get(in, "content"));
    json_t *s = cc2_call(io, "without_skill_safety_rule", a);
    if (cc2_is_raise(s)) return s;
    if (!cc2_is_str(s)) { json_t *e = cc2_attr_raise(s, "startswith"); json_free(s); return e; }
    const char *sv = s->str_val ? s->str_val : "";
    int chain = cc2_startswith(sv, "[") && cc2_contains(sv, " chars)") && strlen(sv) < 400;
    if (chain) { json_free(s); return json_bool(true); }
    json_t *a2 = cc2_args();
    cc2_arg_j(a2, "content", s);
    json_t *d = cc2_call(io, "is_demoted_stub", a2);
    json_free(s);
    return d;   /* raw seam result (bool / None / ...) — live `or` returns it */
}

/* PoP: cc2_skill_view_failed @ agent/context_compressor.py:_skill_view_failed */
static json_t *cc2_skill_view_failed(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *content = cc2_get(in, "content");
    if (!cc2_is_str(content)) return json_bool(false);
    const char *cs = content->str_val ? content->str_val : "";
    json_t *suffix = cc2h_result_failure_suffix(content);
    if (cc2_truth(suffix)) { json_free(suffix); return json_bool(true); }
    json_free(suffix);
    if (!cc2_startswith(cs, "[skill_view] ")) return json_bool(false);
    const char *paren = strstr(cs, " (");
    size_t plen = paren ? (size_t)(paren - cs) : strlen(cs);
    return json_bool(cc2_contains_n(cs, plen, " FAILED"));
}

/* PoP: cc2_synthetic_user_row @ agent/context_compressor.py:_synthetic_user_row */
static json_t *cc2_synthetic_user_row(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *content = cc2_get(in, "content");
    if (!cc2_is_str(content)) return json_bool(true);
    const char *cs = content->str_val ? content->str_val : "";
    char *stripped = cc2_strip(cs);
    if (!stripped || !stripped[0]) { free(stripped); return json_bool(true); }
    free(stripped);
    char *ls = cc2_lstrip(cs);
    int hit = 0;
    for (int i = 0; CC2_SYNTHETIC_USER_ROW_PREFIXES[i]; i++)
        if (cc2_startswith(ls, CC2_SYNTHETIC_USER_ROW_PREFIXES[i])) { hit = 1; break; }
    free(ls);
    return json_bool(hit);
}

/* PoP: cc2_typed_user_text @ agent/context_compressor.py:_typed_user_text */
static json_t *cc2_typed_user_text(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *content = cc2_get(in, "content");
    json_t *a = cc2_args();
    cc2_arg_j(a, "content", content);
    json_t *loaded = cc2_call(io, "skills_loaded_by_scaffold", a);
    if (cc2_is_raise(loaded)) return loaded;
    if (cc2_truth(loaded)) {
        json_free(loaded);
        json_t *a2 = cc2_args();
        cc2_arg_j(a2, "content", content);
        cc2_arg_s(a2, "separator", " ");
        json_t *d = cc2_call(io, "describe_skill_invocation", a2);
        if (cc2_is_raise(d)) return d;
        if (cc2_truth(d)) return d;
        json_free(d);
        return json_string("");
    }
    json_free(loaded);
    return cc2_copy_or_null(content);
}

/* PoP: cc2_json_dict @ agent/context_compressor.py:_json_dict */
static json_t *cc2_json_dict(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return cc2h_json_dict(cc2_get(in, "text"));
}

/* PoP: cc2_failure_suffix @ agent/context_compressor.py:_failure_suffix */
static json_t *cc2_failure_suffix(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *label = json_get_str(in, "label", "FAILED");
    return cc2h_failure_suffix(cc2_get(in, "reason"), label);
}

/* PoP: cc2_payload_failure_suffix @ agent/context_compressor.py:_payload_failure_suffix */
static json_t *cc2_payload_failure_suffix(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return cc2h_payload_failure_suffix(cc2_get(in, "payload"));
}

/* PoP: cc2_result_failure_suffix @ agent/context_compressor.py:_result_failure_suffix */
static json_t *cc2_result_failure_suffix(cc2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return cc2h_result_failure_suffix(cc2_get(in, "content"));
}
