/* context_compressor_tail_flow2.c — second-wave FLOW + leaf port of the
 * UNCLAIMED remainder of agent/context_compressor_tail.py (LIVE authority).
 *
 * A strict, CRLF-aware PoP grep over the src, include and tests trees credits
 * only TWO defs of this module elsewhere:
 *   tail_cut_progress_floor -> src/agent/context_budget_flow.c
 *   _lean_recovery_stub     -> src/agent/context_compressor_pure.c
 *     (cc_lean_recovery_stub)
 * Everything else is UNCLAIMED and ported here (12 PoP entries): the lean-tail
 * budget / demotion flow (lean_tail_budget, is_demoted_stub,
 * skill_view_names_by_call_id, demote_stale_tail_tools), the native-Anthropic
 * tail-price closure (native_anthropic_tail_prices + its nested prices), the
 * tail-cut pricer closure (tail_cut_pricer), and the Skill-Safety-Rule ribbon
 * (with_skill_safety_rule + its nested unruled, without_skill_safety_rule,
 * elide_keeping_skill_safety_rule, with_pruned_row_skill_safety_rule).
 *
 * Claimed siblings + all foreign I/O enter through the cct2_io_t universal
 * seam; ported siblings compose for REAL:
 *   demote_stale_tail_tools      -> skill_view_names_by_call_id (real)
 *   with_skill_safety_rule       -> unruled (real) -> without_skill_safety_rule
 *   elide_keeping_skill_safety_rule -> without_skill_safety_rule (real)
 *   native_anthropic_tail_prices -> prices (real, driven inline)
 * Seamed claimed siblings: _lean_recovery_stub, _is_summary_stub, _json_dict,
 * _result_failure_suffix, _skill_pruned_marker, _rewritten, _tc_get,
 * _content_text_for_contains, elide, _estimate_msg_budget_tokens, and the
 * anthropic policy/replay/sanitization + tool_result_storage collaborators.
 *
 * Decision sequences only: Python truthiness, `or`/`and` short-circuits,
 * `is None` identity vs truthy tests, operand/attribute evaluation order,
 * dict insertion order, negative list-index resolution, and exception catch
 * scopes are transcribed per call site. None of these 12 defs swallows
 * Exception, so a scripted BaseException (SystemExit / KeyboardInterrupt /
 * GeneratorExit / CancelledError) propagates past every guard exactly as live.
 *
 * Value model (shared with tests/oracle_context_compressor_tail_flow2.py):
 *   - attribute doubles: {"__attr__": {...}}  <-> types.SimpleNamespace
 *   - callable doubles:  {"__callable__": "<seam>"} (callable() is True)
 *   - raise specs:       {"__raise__": "<ClassName>", "message": str(e)}
 *   - messages / rows: plain JSON objects (isinstance(x, dict) is True ONLY
 *     for a plain object, never an __attr__ double)
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - text positions carry ASCII bytes; the ONLY non-ASCII carried is the
 *     U+2014 em dash (e2 80 94) inside the byte-pinned SKILL_SAFETY_RULE and
 *     _DEMOTED_STUB_RE literals, matched byte-wise. re \s uses the ASCII set
 *     [ \t\n\r\f\v\x1c-\x1f]; Unicode whitespace (NBSP etc.) is out of domain.
 *   - len() positions count UTF-8 CODE POINTS (Python len), not bytes.
 *   - str() positions (tool-call id / tool_call_id) carry str / integral
 *     number / bool / None only (no containers, no non-integral floats).
 *   - _lean_recovery_stub seam scripts are str / scalar (a list stub extends
 *     in place via +=, transcribed; tuple stubs are out of domain).
 *   - tool_calls iterables are list / str / dict (keys) / falsy; other truthy
 *     carriers raise TypeError. messages / head / tail / original / pruned
 *     are lists; non-list carriers raise the live len()/iteration TypeError.
 *   - tail_start / head_end / probe / limit are integral (bool coerces 1/0);
 *     negative tail_start / head_end resolve Python negative indices.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The
 * harness #includes this .c via the area-qualified
 * "agent/context_compressor_tail_flow2.c". Every symbol is static cct2_*.
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_tail_flow2 \
 *           oracle_context_compressor_tail_flow2.py "" "lib/libjson/json.o"
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

/* ── live module constants (byte-pinned to agent/context_compressor_tail.py
 *    + agent/context_compressor.py) ── */
#define CCT2_LEAN_TAIL_KEEP_TOOL_ROUNDS 6
#define CCT2_LEAN_TAIL_DEMOTE_MIN_CHARS 1500
#define CCT2_TAIL_MAX_THRESHOLD_FRACTION 0.50
#define CCT2_MIN_SUMMARY_TOKENS 2000
#define CCT2_LEAN_TAIL_FLOOR_TOKENS 10000
#define CCT2_LEAN_TAIL_CAP_TOKENS 25000
#define CCT2_SKILL_PRUNED_MARKER_PREFIX "[SKILL_PRUNED:"
#define CCT2_SKILL_SAFETY_HEADING "## Skill Safety Rule"
/* SKILL_SAFETY_RULE carries one U+2014 em dash (e2 80 94); hex-escaped. */
#define CCT2_SKILL_SAFETY_RULE \
    "## Skill Safety Rule\n" \
    "A skill placeholder containing `[SKILL_PRUNED]` lost its content in context compression and is inaccessible \xe2\x80\x94 " \
    "reload it with skill_view(name='...') before acting on anything that depends on it. After reloading, ignore any " \
    "remaining `[SKILL_PRUNED]` markers for that same skill; they are historical artifacts of earlier compactions."
#define CCT2_SKILL_SAFETY_SUFFIX "\n\n" CCT2_SKILL_SAFETY_RULE
/* _DEMOTED_STUB_RE literal tail (the em dash again). */
#define CCT2_DEMOTED_RE_MID " output demoted at compaction \xe2\x80\x94 "
#define CCT2_DEMOTED_RE_TAIL " chars preserved in session history."

/* ── universal seam ── */
typedef struct cct2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cct2_io_t;

/* ── truthiness / accessors ── */
static int cct2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cct2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cct2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cct2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cct2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cct2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cct2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cct2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cct2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cct2_num(const json_t *v)
{
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

/* numeric carrier for arithmetic / comparison positions (int / bool / float). */
static int cct2_is_numeric(const json_t *v)
{
    return cct2_is_num(v) || cct2_is_bool(v);
}

static json_t *cct2_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }
/* copy for ARRAY storage: a None element must be a JSON_NULL node, never a NULL
 * pointer (json_append(arr, NULL) is a no-op in libjson and would drop it). */
static json_t *cct2_copy_or_jsonnull(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* isinstance(x, dict): a PLAIN object, never an __attr__ SimpleNamespace. */
static int cct2_is_plain_dict(const json_t *v)
{
    return cct2_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

static int cct2_is_attr_double(const json_t *v)
{
    return cct2_is_obj(v) && json_obj_get(v, "__attr__") != NULL;
}

/* getattr(double, name, default): only an __attr__ double carries attributes;
 * a PRESENT null is returned (not the default); any other carrier -> default. */
static const json_t *cct2_getattr_d(const json_t *v, const char *name, const json_t *dflt)
{
    if (cct2_is_attr_double(v)) {
        const json_t *inner = json_obj_get(v, "__attr__");
        if (inner && inner->type == JSON_OBJECT) {
            const json_t *val = json_obj_get(inner, name);
            if (val) return val;
        }
    }
    return dflt;
}

/* callable(x): only a {"__callable__": ...} double is callable here. */
static int cct2_is_callable(const json_t *v)
{
    return cct2_is_obj(v) && json_obj_get(v, "__callable__") != NULL;
}

static const char *cct2_callable_name(const json_t *v)
{
    return cct2_str(cct2_get(v, "__callable__"));
}

static const char *cct2_typename(const json_t *v)
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
static int cct2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *cct2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *cct2_xfmt(const char *fmt, ...)
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

static json_t *cct2_attr_raise(const json_t *v, const char *name)
{
    char *m = cct2_xfmt("'%s' object has no attribute '%s'", cct2_typename(v), name);
    json_t *r = cct2_mkraise("AttributeError", m ? m : "");
    free(m);
    return r;
}

static json_t *cct2_type_raise(const char *msg)
{
    return cct2_mkraise("TypeError", msg);
}

/* ── universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. ── */
static json_t *cct2_call(cct2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cct2_xfmt("no seam installed for %s", name);
        r = cct2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cct2_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides (harness strips nulls
 * recursively; oracle norm_args drops None). */
static void cct2_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void cct2_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cct2_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* ── string helpers (ASCII domain) ── */
static int cct2_contains(const char *hay, const char *needle)
{
    return hay && needle && strstr(hay, needle) != NULL;
}

/* Python len(str): count UTF-8 code points (skip continuation bytes). */
static size_t cct2_utf8_len(const char *s)
{
    if (!s) return 0;
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* str.replace(old, new): replace ALL non-overlapping occurrences (literal).
 * Caller frees. old non-empty. */
static char *cct2_str_replace(const char *s, const char *old, const char *new)
{
    if (!s) s = "";
    if (!old || !old[0]) return cct2_xfmt("%s", s);
    if (!new) new = "";
    size_t olen = strlen(old), nlen = strlen(new);
    size_t cap = strlen(s) + 1, n = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    const char *p = s;
    while (*p) {
        if (strncmp(p, old, olen) == 0) {
            while (n + nlen + 1 > cap) { cap *= 2; char *nb = (char *)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
            memcpy(out + n, new, nlen); n += nlen; p += olen;
        } else {
            if (n + 2 > cap) { cap *= 2; char *nb = (char *)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
    return out;
}

/* Python `needle in text` for the ribbon checks: str -> substring; list ->
 * element equality; dict -> key equality; other -> py3.14 TypeError. Returns
 * 1/0 on success, -1 with *err set on a non-container carrier. */
static int cct2_in_py(const json_t *text, const char *needle, json_t **err)
{
    *err = NULL;
    if (cct2_is_str(text)) return cct2_contains(text->str_val ? text->str_val : "", needle) ? 1 : 0;
    if (cct2_is_arr(text)) {
        for (size_t i = 0; i < json_len(text); i++) {
            const json_t *e = json_get(text, i);
            if (cct2_is_str(e) && strcmp(e->str_val ? e->str_val : "", needle) == 0) return 1;
        }
        return 0;
    }
    if (cct2_is_obj(text)) {
        for (size_t i = 0; i < text->c.count; i++)
            if (text->c.keys[i] && strcmp(text->c.keys[i], needle) == 0) return 1;
        return 0;
    }
    char *m = cct2_xfmt("argument of type '%s' is not a container or iterable", cct2_typename(text));
    *err = cct2_type_raise(m ? m : "");
    free(m);
    return -1;
}

/* str(value) projection (domain: str / integral number / bool / None). */
static char *cct2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cct2_xfmt("None");
    if (v->type == JSON_BOOL) return cct2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) return cct2_xfmt("%.0f", d);
        return cct2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cct2_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);   /* containers excluded from str() fixtures */
    return s ? s : cct2_xfmt("");
}

/* str(value or ""): falsy -> "", else str(value). Caller frees. */
static char *cct2_pystr_or_empty(const json_t *v)
{
    if (!cct2_truth(v)) return cct2_xfmt("");
    return cct2_pystr(v);
}

/* `a + b` where b is a str (the ribbon suffix). a str -> concat; a list ->
 * "can only concatenate list ..."; other -> unsupported operand. Caller frees
 * the result; *err set on raise. */
static char *cct2_concat_suffix(const json_t *a, const char *suffix, json_t **err)
{
    *err = NULL;
    if (cct2_is_str(a)) return cct2_xfmt("%s%s", a->str_val ? a->str_val : "", suffix);
    if (cct2_is_arr(a)) {
        char *m = cct2_xfmt("can only concatenate list (not \"str\") to list");
        *err = cct2_type_raise(m ? m : ""); free(m); return NULL;
    }
    char *m = cct2_xfmt("unsupported operand type(s) for +: '%s' and 'str'", cct2_typename(a));
    *err = cct2_type_raise(m ? m : ""); free(m); return NULL;
}

/* `" " + marker` (str + marker). marker str -> concat; else can-only-concat. */
static char *cct2_str_plus(const char *a, const json_t *b, json_t **err)
{
    *err = NULL;
    if (cct2_is_str(b)) return cct2_xfmt("%s%s", a, b->str_val ? b->str_val : "");
    char *m = cct2_xfmt("can only concatenate str (not \"%s\") to str", cct2_typename(b));
    *err = cct2_type_raise(m ? m : ""); free(m); return NULL;
}

/* `stub += rhs` (rhs str). stub str -> concat (returns new string, *owned_out
 * set, caller frees old); stub list -> in-place extend with rhs code points
 * (returns NULL, list mutated); scalar -> += TypeError. Returns 0 ok / -1 err. */
static int cct2_iadd_str(json_t **stub, const char *rhs, json_t **err)
{
    *err = NULL;
    json_t *s = *stub;
    if (cct2_is_str(s)) {
        char *n = cct2_xfmt("%s%s", s->str_val ? s->str_val : "", rhs);
        json_free(s);
        *stub = json_string(n ? n : "");
        free(n);
        return 0;
    }
    if (cct2_is_arr(s)) {
        for (const unsigned char *p = (const unsigned char *)rhs; *p; p++) {
            char ch[2] = { (char)*p, '\0' };
            json_append(s, json_string(ch));
        }
        return 0;
    }
    char *m = cct2_xfmt("unsupported operand type(s) for +=: '%s' and 'str'", cct2_typename(s));
    *err = cct2_type_raise(m ? m : ""); free(m); return -1;
}

/* Python list index resolution (negative wraps); IndexError when out of range.
 * Returns a BORROWED element or NULL with *err set. */
static json_t *cct2_list_get(const json_t *arr, long long i, json_t **err)
{
    *err = NULL;
    long long n = (long long)json_len(arr);
    long long idx = i < 0 ? i + n : i;
    if (idx < 0 || idx >= n) {
        *err = cct2_mkraise("IndexError", "list index out of range");
        return NULL;
    }
    return json_get(arr, (size_t)idx);
}

/* int() coercion for index/limit arithmetic positions: int/bool/float only;
 * other carriers raise the live TypeError. *err set on failure. */
static long long cct2_index_int(const json_t *v, json_t **err)
{
    *err = NULL;
    if (cct2_is_numeric(v)) return (long long)cct2_num(v);
    char *m = cct2_xfmt("unsupported operand type(s) for -: '%s' and 'int'", cct2_typename(v));
    *err = cct2_type_raise(m ? m : ""); free(m); return 0;
}

/* ── _DEMOTED_STUB_RE matcher (byte-pinned, re.match anchored at 0, ASCII) ──
 * Pattern: \[[^\s\]]* output demoted at compaction — [\d,]+ chars preserved
 *          in session history\.
 * [^\s\]]* excludes whitespace (ASCII + \x1c-\x1f) and ']'; the following
 * literal starts with ' ' so the greedy class has a single split point. */
static int cct2_re_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
static int cct2_demoted_stub_match(const char *s)
{
    if (!s || s[0] != '[') return 0;
    size_t n = strlen(s), p = 1;
    while (p < n && !cct2_re_ws((unsigned char)s[p]) && s[p] != ']') p++;
    size_t midlen = strlen(CCT2_DEMOTED_RE_MID);
    if (p + midlen > n || strncmp(s + p, CCT2_DEMOTED_RE_MID, midlen) != 0) return 0;
    p += midlen;
    size_t dstart = p;
    while (p < n && ((s[p] >= '0' && s[p] <= '9') || s[p] == ',')) p++;
    if (p == dstart) return 0;   /* [\d,]+ needs at least one */
    size_t taillen = strlen(CCT2_DEMOTED_RE_TAIL);
    if (p + taillen > n || strncmp(s + p, CCT2_DEMOTED_RE_TAIL, taillen) != 0) return 0;
    return 1;   /* prefix match; no trailing anchor */
}

/* ── forward decls (ported siblings compose REAL) ── */
static json_t *cct2h_skill_view_names(cct2_io_t *io, const json_t *messages);
static json_t *cct2h_without_rule(const json_t *content);
static json_t *cct2h_unruled(cct2_io_t *io, const json_t *rows);
static json_t *cct2h_prices(cct2_io_t *io, const json_t *messages, const json_t *rejected);

/* ===========================================================================
 * lean_tail_budget
 * =========================================================================== */
/* PoP: cct2_lean_tail_budget @ agent/context_compressor_tail.py:lean_tail_budget */
static json_t *cct2_lean_tail_budget(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *cl = cct2_get(in, "context_length");
    const json_t *tt = cct2_get(in, "threshold_tokens");

    /* window_term = min(CAP, int(context_length * 0.025)) — evaluated first. */
    if (!cct2_is_numeric(cl)) {
        char *m;
        if (cct2_is_str(cl) || cct2_is_arr(cl))
            m = cct2_xfmt("can't multiply sequence by non-int of type 'float'");
        else
            m = cct2_xfmt("unsupported operand type(s) for *: '%s' and 'float'", cct2_typename(cl));
        json_t *e = cct2_type_raise(m ? m : ""); free(m); return e;
    }
    double wt = (double)(long long)(cct2_num(cl) * 0.025);
    if (wt > CCT2_LEAN_TAIL_CAP_TOKENS) wt = CCT2_LEAN_TAIL_CAP_TOKENS;
    double floor = CCT2_LEAN_TAIL_FLOOR_TOKENS;

    /* if threshold_tokens > 0 */
    if (!cct2_is_numeric(tt)) {
        char *m = cct2_xfmt("'>' not supported between instances of '%s' and 'int'", cct2_typename(tt));
        json_t *e = cct2_type_raise(m ? m : ""); free(m); return e;
    }
    double ttv = cct2_num(tt);
    if (ttv > 0) {
        double share = (double)(long long)(ttv * CCT2_TAIL_MAX_THRESHOLD_FRACTION);
        if (share < floor) floor = share;
        if (ttv > CCT2_MIN_SUMMARY_TOKENS && share < wt) wt = share;
    }
    double ret = floor > wt ? floor : wt;
    return json_number(ret);
}

/* ===========================================================================
 * is_demoted_stub
 * =========================================================================== */
/* PoP: cct2_is_demoted_stub @ agent/context_compressor_tail.py:is_demoted_stub */
static json_t *cct2_is_demoted_stub(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *content = cct2_get(in, "content");
    int first = 0;
    if (cct2_is_str(content)) {
        const char *cs = content->str_val ? content->str_val : "";
        first = (cct2_utf8_len(cs) < 1000 && cct2_demoted_stub_match(cs)) ? 1 : 0;
    } else if (cct2_is_arr(content) || cct2_is_obj(content)) {
        /* len() works on containers; a short one reaches re.match -> TypeError */
        if (json_len(content) < 1000) {
            char *m = cct2_xfmt("expected string or bytes-like object, got '%s'", cct2_typename(content));
            return cct2_type_raise(m ? m : "");
        }
        first = 0;   /* len >= 1000 short-circuits the and before re.match */
    } else {
        char *m = cct2_xfmt("object of type '%s' has no len()", cct2_typename(content));
        return cct2_type_raise(m ? m : "");
    }
    if (first) return json_bool(true);
    /* or has_archive_pointer(content) — raw seam result (live `or` returns it) */
    json_t *a = cct2_args();
    cct2_arg_j(a, "content", content);
    return cct2_call(io, "has_archive_pointer", a);
}

/* ===========================================================================
 * skill_view_names_by_call_id
 * =========================================================================== */
static json_t *cct2h_tc_get(cct2_io_t *io, const json_t *obj, const char *key,
                            const json_t *dflt)
{
    json_t *a = cct2_args();
    cct2_arg_j(a, "obj", obj);
    cct2_arg_s(a, "key", key);
    if (dflt) cct2_arg_j(a, "default", dflt);
    else cct2_arg_s(a, "default", "");   /* live _tc_get default="" */
    return cct2_call(io, "_tc_get", a);
}

static json_t *cct2h_skill_view_names(cct2_io_t *io, const json_t *messages)
{
    /* live `for msg in messages`: list / str(code points) / dict(keys) iterate;
     * other carriers raise TypeError. For a str/dict carrier every yielded
     * element is a str, so msg.get("role") raises AttributeError on the FIRST
     * element; an empty carrier yields no rows (empty names). */
    size_t mn;
    int mode;
    if (cct2_is_arr(messages)) { mode = 0; mn = json_len(messages); }
    else if (cct2_is_str(messages)) { mode = 1; mn = cct2_utf8_len(messages->str_val ? messages->str_val : ""); }
    else if (cct2_is_obj(messages)) { mode = 2; mn = messages->c.count; }
    else {
        char *m = cct2_xfmt("'%s' object is not iterable", cct2_typename(messages));
        return cct2_type_raise(m ? m : "");
    }
    if (mode != 0) {
        if (mn == 0) return json_object();
        json_t *s = json_string("x");
        json_t *e = cct2_attr_raise(s, "get");
        json_free(s);
        return e;
    }
    json_t *names = json_object();
    json_t *empty_obj = json_object();
    for (size_t mi = 0; mi < mn; mi++) {
        const json_t *msg = json_get(messages, mi);
        if (!cct2_is_plain_dict(msg)) {
            json_free(empty_obj); json_free(names);
            return cct2_attr_raise(msg, "get");
        }
        const json_t *role = cct2_get(msg, "role");
        if (!(cct2_is_str(role) && strcmp(role->str_val ? role->str_val : "", "assistant") == 0))
            continue;
        const json_t *tcs = cct2_get(msg, "tool_calls");
        if (!cct2_truth(tcs)) continue;   /* or [] */
        /* iterate list / str(code points) / dict(keys); other truthy -> raise */
        size_t tn = 0;
        int mode = 0;   /* 0 list, 1 str, 2 dict */
        if (cct2_is_arr(tcs)) { mode = 0; tn = json_len(tcs); }
        else if (cct2_is_str(tcs)) { mode = 1; tn = cct2_utf8_len(tcs->str_val ? tcs->str_val : ""); }
        else if (cct2_is_obj(tcs)) { mode = 2; tn = tcs->c.count; }
        else {
            char *m = cct2_xfmt("'%s' object is not iterable", cct2_typename(tcs));
            json_free(empty_obj); json_free(names);
            return cct2_type_raise(m ? m : "");
        }
        for (size_t ti = 0; ti < tn; ti++) {
            json_t *tc_owned = NULL;
            const json_t *tc;
            if (mode == 0) tc = json_get(tcs, ti);
            else if (mode == 1) {
                /* ASCII code point ti */
                const char *s = tcs->str_val ? tcs->str_val : "";
                char ch[2] = { s[ti], '\0' };
                tc_owned = json_string(ch); tc = tc_owned;
            } else {
                tc_owned = json_string(tcs->c.keys[ti] ? tcs->c.keys[ti] : ""); tc = tc_owned;
            }
            /* fn = _tc_get(tc, "function", {}) */
            json_t *fn = cct2h_tc_get(io, tc, "function", empty_obj);
            if (cct2_is_raise(fn)) { json_free(tc_owned); json_free(empty_obj); json_free(names); return fn; }
            /* _tc_get(fn, "name") */
            json_t *nm = cct2h_tc_get(io, fn, "name", NULL);
            if (cct2_is_raise(nm)) { json_free(fn); json_free(tc_owned); json_free(empty_obj); json_free(names); return nm; }
            json_t *skill = NULL;   /* owned */
            int skill_borrowed = 0;
            json_t *parsed = NULL;
            if (cct2_is_str(nm) && strcmp(nm->str_val ? nm->str_val : "", "skill_view") == 0) {
                json_t *av = cct2h_tc_get(io, fn, "arguments", NULL);
                if (cct2_is_raise(av)) { json_free(nm); json_free(fn); json_free(tc_owned); json_free(empty_obj); json_free(names); return av; }
                json_t *ja = cct2_args();
                cct2_arg_j(ja, "text", av);
                json_free(av);
                parsed = cct2_call(io, "_json_dict", ja);
                if (cct2_is_raise(parsed)) { json_free(nm); json_free(fn); json_free(tc_owned); json_free(empty_obj); json_free(names); return parsed; }
                if (!cct2_is_obj(parsed)) {
                    json_t *e = cct2_attr_raise(parsed, "get");
                    json_free(parsed); json_free(nm); json_free(fn); json_free(tc_owned); json_free(empty_obj); json_free(names);
                    return e;
                }
                skill = cct2_get(parsed, "name");   /* borrowed from parsed */
                skill_borrowed = 1;
            }
            json_free(nm);
            json_free(fn);
            if (cct2_is_str(skill) && skill->str_val && skill->str_val[0]) {
                json_t *idv = cct2h_tc_get(io, tc, "id", NULL);
                if (cct2_is_raise(idv)) {
                    if (!skill_borrowed) json_free(skill);
                    json_free(parsed); json_free(tc_owned); json_free(empty_obj); json_free(names);
                    return idv;
                }
                char *key = cct2_pystr_or_empty(idv);
                json_free(idv);
                json_set(names, key ? key : "", json_copy(skill));
                free(key);
            }
            if (!skill_borrowed) json_free(skill);
            json_free(parsed);
            json_free(tc_owned);
        }
    }
    json_free(empty_obj);
    return names;
}

/* PoP: cct2_skill_view_names_by_call_id @ agent/context_compressor_tail.py:skill_view_names_by_call_id */
static json_t *cct2_skill_view_names_by_call_id(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return cct2h_skill_view_names(io, cct2_get(in, "messages"));
}

/* ===========================================================================
 * demote_stale_tail_tools
 * =========================================================================== */
/* PoP: cct2_demote_stale_tail_tools @ agent/context_compressor_tail.py:demote_stale_tail_tools */
static json_t *cct2_demote_stale_tail_tools(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *messages = cct2_get(in, "messages");
    const json_t *ts_node = cct2_get(in, "tail_start");
    const json_t *session_id = cct2_get(in, "session_id");

    if (!cct2_is_arr(messages)) {
        char *m = cct2_xfmt("object of type '%s' has no len()", cct2_typename(messages));
        return cct2_type_raise(m ? m : "");
    }
    json_t *err = NULL;
    long long ts = cct2_index_int(ts_node, &err);
    if (err) return err;
    long long n = (long long)json_len(messages);

    /* protection scan: range(n-1, ts-1, -1) filtered to role == "tool" */
    long long rounds = 0, prev_idx = 0;
    int prev_present = 0;
    json_t *protected = json_array();
    for (long long i = n - 1; i >= ts; i--) {
        json_t *row = cct2_list_get(messages, i, &err);
        if (err) { json_free(protected); return err; }
        if (!cct2_is_plain_dict(row)) { json_free(protected); return cct2_attr_raise(row, "get"); }
        const json_t *role = cct2_get(row, "role");
        if (!(cct2_is_str(role) && strcmp(role->str_val ? role->str_val : "", "tool") == 0)) continue;
        rounds += (!prev_present || prev_idx - i > 1) ? 1 : 0;
        prev_present = 1; prev_idx = i;
        if (rounds > CCT2_LEAN_TAIL_KEEP_TOOL_ROUNDS) break;
        json_append(protected, json_number((double)i));
    }

    json_t *result = json_copy(messages);
    long long demoted = 0;
    json_t *skills = NULL;
    int skills_computed = 0;
    for (long long i = ts; i < n; i++) {
        json_t *msg = cct2_list_get(messages, i, &err);
        if (err) { json_free(protected); json_free(result); json_free(skills); return err; }
        if (!cct2_is_plain_dict(msg)) { json_free(protected); json_free(result); json_free(skills); return cct2_attr_raise(msg, "get"); }
        const json_t *content = cct2_get(msg, "content");
        const json_t *role = cct2_get(msg, "role");
        int is_tool = cct2_is_str(role) && strcmp(role->str_val ? role->str_val : "", "tool") == 0;
        int is_protected = 0;
        for (size_t k = 0; k < json_len(protected); k++)
            if (json_get(protected, k)->num_val == (double)i) { is_protected = 1; break; }
        if (!is_tool || is_protected || !cct2_is_str(content)) continue;
        const char *cs = content->str_val ? content->str_val : "";
        if (cct2_utf8_len(cs) < CCT2_LEAN_TAIL_DEMOTE_MIN_CHARS || cct2_contains(cs, CCT2_SKILL_PRUNED_MARKER_PREFIX)) continue;
        /* _is_summary_stub(content) seam */
        json_t *sa = cct2_args();
        cct2_arg_j(sa, "content", content);
        json_t *ss = cct2_call(io, "_is_summary_stub", sa);
        if (cct2_is_raise(ss)) { json_free(protected); json_free(result); json_free(skills); return ss; }
        int is_stub = cct2_truth(ss);
        json_free(ss);
        if (is_stub) continue;

        /* stub = _lean_recovery_stub(msg.get("tool_name") or "", len(content), session_id) */
        const json_t *tn = cct2_get(msg, "tool_name");
        json_t *la = cct2_args();
        if (cct2_truth(tn)) cct2_arg_j(la, "tool_name", tn);
        else cct2_arg_s(la, "tool_name", "");
        cct2_arg_n(la, "content_len", (double)cct2_utf8_len(cs));
        cct2_arg_j(la, "session_id", session_id);
        json_t *stub = cct2_call(io, "_lean_recovery_stub", la);
        if (cct2_is_raise(stub)) { json_free(protected); json_free(result); json_free(skills); return stub; }

        /* skills = skill_view_names_by_call_id(messages) if skills is None */
        if (!skills_computed) {
            skills = cct2h_skill_view_names(io, messages);
            if (cct2_is_raise(skills)) { json_free(stub); json_free(protected); json_free(result); return skills; }
            skills_computed = 1;
        }
        /* skill := skills.get(str(msg.get("tool_call_id") or "")) */
        const json_t *tcid = cct2_get(msg, "tool_call_id");
        char *skey = cct2_pystr_or_empty(tcid);
        const json_t *skill = cct2_get(skills, skey ? skey : "");
        free(skey);
        if (cct2_truth(skill)) {
            json_t *fa = cct2_args();
            cct2_arg_j(fa, "content", content);
            json_t *suf = cct2_call(io, "_result_failure_suffix", fa);
            if (cct2_is_raise(suf)) { json_free(stub); json_free(protected); json_free(result); json_free(skills); return suf; }
            int failed = cct2_truth(suf);
            json_free(suf);
            if (!failed) {
                json_t *ma = cct2_args();
                cct2_arg_j(ma, "skill_name", skill);
                json_t *marker = cct2_call(io, "_skill_pruned_marker", ma);
                if (cct2_is_raise(marker)) { json_free(stub); json_free(protected); json_free(result); json_free(skills); return marker; }
                /* rhs = " " + marker */
                json_t *cerr = NULL;
                char *rhs = cct2_str_plus(" ", marker, &cerr);
                json_free(marker);
                if (cerr) { json_free(stub); json_free(protected); json_free(result); json_free(skills); return cerr; }
                /* stub += rhs */
                if (cct2_iadd_str(&stub, rhs ? rhs : "", &cerr) < 0) {
                    free(rhs); json_free(stub); json_free(protected); json_free(result); json_free(skills); return cerr;
                }
                free(rhs);
            }
        }
        /* result[i] = _rewritten(msg, stub) */
        json_t *ra = cct2_args();
        cct2_arg_j(ra, "msg", msg);
        cct2_arg_j(ra, "content", stub);
        json_t *rw = cct2_call(io, "_rewritten", ra);
        json_free(stub);
        if (cct2_is_raise(rw)) { json_free(protected); json_free(result); json_free(skills); return rw; }
        long long ridx = i < 0 ? i + n : i;
        json_array_set(result, (size_t)ridx, rw);
        demoted++;
    }
    json_free(protected);

    if (demoted > 0) {
        /* archive_compaction_drops(messages, result, session_id) */
        json_t *aa = cct2_args();
        cct2_arg_j(aa, "messages", messages);
        cct2_arg_j(aa, "dropped", result);
        cct2_arg_j(aa, "session_id", session_id);
        json_t *arch = cct2_call(io, "archive_compaction_drops", aa);
        if (cct2_is_raise(arch)) { json_free(result); json_free(skills); return arch; }
        /* result, _, kept = arch */
        if (!cct2_is_arr(arch)) {
            char *m = cct2_xfmt("cannot unpack non-iterable %s object", cct2_typename(arch));
            json_free(arch); json_free(result); json_free(skills);
            return cct2_type_raise(m ? m : "");
        }
        size_t an = json_len(arch);
        if (an < 3) {
            char *m = cct2_xfmt("not enough values to unpack (expected 3, got %zu)", an);
            json_free(arch); json_free(result); json_free(skills);
            return cct2_mkraise("ValueError", m ? m : "");
        }
        if (an > 3) {
            char *m = cct2_xfmt("too many values to unpack (expected 3, got %zu)", an);
            json_free(arch); json_free(result); json_free(skills);
            return cct2_mkraise("ValueError", m ? m : "");
        }
        const json_t *kept = json_get(arch, 2);
        if (!cct2_is_numeric(kept)) {
            char *m = cct2_xfmt("unsupported operand type(s) for -=: 'int' and '%s'", cct2_typename(kept));
            json_free(arch); json_free(result); json_free(skills);
            return cct2_type_raise(m ? m : "");
        }
        double new_demoted = (double)demoted - cct2_num(kept);
        json_t *new_result = json_copy(json_get(arch, 0));
        json_free(arch);
        json_free(result);
        result = new_result;
        json_free(skills);
        json_t *ret = json_array();
        json_append(ret, result);
        json_append(ret, json_number(new_demoted));
        return ret;
    }
    json_free(skills);
    json_t *ret = json_array();
    json_append(ret, result);
    json_append(ret, json_number((double)demoted));
    return ret;
}

/* ===========================================================================
 * native_anthropic_tail_prices + nested prices closure
 * =========================================================================== */
static json_t *cct2h_prices(cct2_io_t *io, const json_t *messages, const json_t *rejected)
{
    if (!cct2_is_arr(messages)) {
        char *m = cct2_xfmt("'%s' object is not iterable", cct2_typename(messages));
        return cct2_type_raise(m ? m : "");
    }
    /* request_copies = [dict(m) if isinstance(m, dict) else m for m in messages]
     * (value-identical copy for every row in the JSON model). */
    json_t *copies = json_array();
    for (size_t i = 0; i < json_len(messages); i++)
        json_append(copies, json_copy(json_get(messages, i)));
    if (cct2_truth(rejected)) {
        for (size_t i = 0; i < json_len(copies); i++) {
            json_t *sa = cct2_args();
            cct2_arg_j(sa, "message", json_get(copies, i));
            cct2_arg_j(sa, "rejected", rejected);
            json_t *sr = cct2_call(io, "strip_rejected_thinking", sa);
            if (cct2_is_raise(sr)) { json_free(copies); return sr; }
            json_free(sr);
        }
    }
    json_t *ca = cct2_args();
    cct2_arg_j(ca, "messages", copies);
    json_t *rc = cct2_call(io, "native_anthropic_row_costs", ca);
    json_free(copies);
    if (cct2_is_raise(rc)) return rc;
    if (!cct2_is_arr(rc)) {
        char *m = cct2_xfmt("'%s' object is not iterable", cct2_typename(rc));
        json_free(rc);
        return cct2_type_raise(m ? m : "");
    }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(rc); i++) {
        const json_t *pair = json_get(rc, i);
        if (!cct2_is_arr(pair)) {
            char *m = cct2_xfmt("cannot unpack non-iterable %s object", cct2_typename(pair));
            json_free(rc); json_free(out);
            return cct2_type_raise(m ? m : "");
        }
        size_t pn = json_len(pair);
        if (pn < 2) {
            char *m = cct2_xfmt("not enough values to unpack (expected 2, got %zu)", pn);
            json_free(rc); json_free(out);
            return cct2_mkraise("ValueError", m ? m : "");
        }
        if (pn > 2) {
            char *m = cct2_xfmt("too many values to unpack (expected 2, got %zu)", pn);
            json_free(rc); json_free(out);
            return cct2_mkraise("ValueError", m ? m : "");
        }
        const json_t *shadow = json_get(pair, 0);
        const json_t *thinking = json_get(pair, 1);
        if (cct2_is_plain_dict(shadow)) {
            json_t *ea = cct2_args();
            cct2_arg_j(ea, "msg", shadow);
            json_t *est = cct2_call(io, "_estimate_msg_budget_tokens", ea);
            if (cct2_is_raise(est)) { json_free(rc); json_free(out); return est; }
            if (!cct2_is_numeric(est) || !cct2_is_numeric(thinking)) {
                char *m = cct2_xfmt("unsupported operand type(s) for +: '%s' and '%s'",
                                    cct2_typename(est), cct2_typename(thinking));
                json_free(est); json_free(rc); json_free(out);
                return cct2_type_raise(m ? m : "");
            }
            json_append(out, json_number(cct2_num(est) + cct2_num(thinking)));
            json_free(est);
        } else {
            json_append(out, json_number(0));
        }
    }
    json_free(rc);
    return out;
}

/* PoP: cct2_prices @ agent/context_compressor_tail.py:prices */
static json_t *cct2_prices(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return cct2h_prices(io, cct2_get(in, "messages"), cct2_get(in, "rejected"));
}

/* PoP: cct2_native_anthropic_tail_prices @ agent/context_compressor_tail.py:native_anthropic_tail_prices */
static json_t *cct2_native_anthropic_tail_prices(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *compressor = cct2_get(in, "compressor");
    json_t *empty = json_string("");
    /* (getattr(compressor, "api_mode", "") or "") != "anthropic_messages" */
    const json_t *am = cct2_getattr_d(compressor, "api_mode", empty);
    const json_t *am_or = cct2_truth(am) ? am : empty;
    int is_route = cct2_is_str(am_or) && strcmp(am_or->str_val ? am_or->str_val : "", "anthropic_messages") == 0;
    if (!is_route) { json_free(empty); return json_null(); }
    /* native_anthropic_preserves_prior_thinking(base_url, model) */
    const json_t *base_url = cct2_getattr_d(compressor, "base_url", empty);
    const json_t *model = cct2_getattr_d(compressor, "model", empty);
    json_t *pa = cct2_args();
    cct2_arg_j(pa, "base_url", base_url);
    cct2_arg_j(pa, "model", model);
    json_t *pres = cct2_call(io, "native_anthropic_preserves_prior_thinking", pa);
    if (cct2_is_raise(pres)) { json_free(empty); return pres; }
    int keeps = cct2_truth(pres);
    json_free(pres);
    if (!keeps) { json_free(empty); return json_null(); }
    /* rejected = session_rejected_thinking(compressor, _session_db, _session_id, persistence=_persistence) */
    const json_t *sdb = cct2_getattr_d(compressor, "_session_db", NULL);
    const json_t *sid = cct2_getattr_d(compressor, "_session_id", empty);
    const json_t *pers = cct2_getattr_d(compressor, "_persistence", NULL);
    json_t *sa = cct2_args();
    cct2_arg_j(sa, "holder", compressor);
    cct2_arg_j(sa, "session_db", sdb);
    cct2_arg_j(sa, "session_id", sid);
    cct2_arg_j(sa, "persistence", pers);
    json_t *rejected = cct2_call(io, "session_rejected_thinking", sa);
    json_free(empty);
    if (cct2_is_raise(rejected)) return rejected;
    json_t *ret = cct2h_prices(io, cct2_get(in, "messages"), rejected);
    json_free(rejected);
    return ret;
}

/* ===========================================================================
 * tail_cut_pricer (returns price or a lambda; driven at in.probe)
 * =========================================================================== */
static json_t *cct2_call_price(cct2_io_t *io, const json_t *price, const json_t *probe)
{
    if (!cct2_is_callable(price)) {
        char *m = cct2_xfmt("'%s' object is not callable", cct2_typename(price));
        return cct2_type_raise(m ? m : "");
    }
    const char *name = cct2_callable_name(price);
    json_t *a = cct2_args();
    cct2_arg_j(a, "arg0", probe);
    return cct2_call(io, name ? name : "", a);
}

/* PoP: cct2_tail_cut_pricer @ agent/context_compressor_tail.py:tail_cut_pricer */
static json_t *cct2_tail_cut_pricer(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *compressor = cct2_get(in, "compressor");
    const json_t *messages = cct2_get(in, "messages");
    const json_t *he_node = cct2_get(in, "head_end");
    const json_t *probe = cct2_get(in, "probe");

    /* price = compressor._tail_row_pricer(messages) — direct attribute read */
    if (!cct2_is_attr_double(compressor))
        return cct2_attr_raise(compressor, "_tail_row_pricer");
    const json_t *inner = json_obj_get(compressor, "__attr__");
    const json_t *meth = (inner && inner->type == JSON_OBJECT) ? json_obj_get(inner, "_tail_row_pricer") : NULL;
    if (!meth) {
        json_t *ns = json_object();
        json_set(ns, "__attr__", json_object());
        json_t *e = cct2_attr_raise(ns, "_tail_row_pricer");
        json_free(ns);
        return e;
    }
    if (!cct2_is_callable(meth)) {
        char *m = cct2_xfmt("'%s' object is not callable", cct2_typename(meth));
        return cct2_type_raise(m ? m : "");
    }
    json_t *ta = cct2_args();
    cct2_arg_j(ta, "messages", messages);
    json_t *price = cct2_call(io, cct2_callable_name(meth), ta);
    if (cct2_is_raise(price)) return price;

    /* newest = next((i for i in range(len-1, head_end-1, -1) if seam(row)), -1) */
    if (!cct2_is_arr(messages)) {
        char *m = cct2_xfmt("object of type '%s' has no len()", cct2_typename(messages));
        json_free(price);
        return cct2_type_raise(m ? m : "");
    }
    json_t *err = NULL;
    long long he = cct2_index_int(he_node, &err);
    if (err) { json_free(price); return err; }
    long long n = (long long)json_len(messages);
    long long newest = -1;
    for (long long i = n - 1; i >= he; i--) {
        json_t *row = cct2_list_get(messages, i, &err);
        if (err) { json_free(price); return err; }
        json_t *sa = cct2_args();
        cct2_arg_j(sa, "msg", row);
        json_t *sr = cct2_call(io, "compressor._is_context_summary_message", sa);
        if (cct2_is_raise(sr)) { json_free(price); return sr; }
        int hit = cct2_truth(sr);
        json_free(sr);
        if (hit) { newest = i; break; }
    }

    json_t *ret;
    if (newest < 0) {
        ret = cct2_call_price(io, price, probe);
    } else {
        /* lambda i: 0 if i <= newest else price(i) */
        if (!cct2_is_numeric(probe)) {
            char *m = cct2_xfmt("'<=' not supported between instances of '%s' and 'int'", cct2_typename(probe));
            json_free(price);
            return cct2_type_raise(m ? m : "");
        }
        if (cct2_num(probe) <= (double)newest) ret = json_number(0);
        else ret = cct2_call_price(io, price, probe);
    }
    json_free(price);
    return ret;
}

/* ===========================================================================
 * Skill-Safety-Rule ribbon
 * =========================================================================== */
static json_t *cct2h_without_rule(const json_t *content)
{
    if (!cct2_is_str(content)) return cct2_attr_raise(content, "replace");
    char *r = cct2_str_replace(content->str_val ? content->str_val : "", CCT2_SKILL_SAFETY_SUFFIX, "");
    json_t *out = json_string(r ? r : "");
    free(r);
    return out;
}

/* PoP: cct2_without_skill_safety_rule @ agent/context_compressor_tail.py:without_skill_safety_rule */
static json_t *cct2_without_skill_safety_rule(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return cct2h_without_rule(cct2_get(in, "content"));
}

static json_t *cct2h_unruled(cct2_io_t *io, const json_t *rows)
{
    (void)io;   /* without_skill_safety_rule is REAL (no seam) */
    if (!cct2_is_arr(rows)) {
        char *m = cct2_xfmt("'%s' object is not iterable", cct2_typename(rows));
        return cct2_type_raise(m ? m : "");
    }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *m = json_get(rows, i);
        if (cct2_is_plain_dict(m)) {
            const json_t *role = cct2_get(m, "role");
            int not_system = !(cct2_is_str(role) && strcmp(role->str_val ? role->str_val : "", "system") == 0);
            const json_t *content = cct2_get(m, "content");
            if (not_system && cct2_is_str(content) && cct2_contains(content->str_val ? content->str_val : "", CCT2_SKILL_SAFETY_RULE)) {
                json_t *nc = cct2h_without_rule(content);
                json_t *nm = json_copy(m);
                json_set(nm, "content", nc);
                json_append(out, nm);
                continue;
            }
        }
        json_append(out, json_copy(m));
    }
    return out;
}

/* PoP: cct2_unruled @ agent/context_compressor_tail.py:unruled */
static json_t *cct2_unruled(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return cct2h_unruled(io, cct2_get(in, "rows"));
}

/* PoP: cct2_with_skill_safety_rule @ agent/context_compressor_tail.py:with_skill_safety_rule */
static json_t *cct2_with_skill_safety_rule(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *summary = cct2_get(in, "summary");
    /* head, tail = unruled(head), unruled(tail) */
    json_t *head = cct2h_unruled(io, cct2_get(in, "head"));
    if (cct2_is_raise(head)) return head;
    json_t *tail = cct2h_unruled(io, cct2_get(in, "tail"));
    if (cct2_is_raise(tail)) { json_free(head); return tail; }

    /* texts = (summary, *(_content_text_for_contains(m.get("content")) for m in (*head,*tail) if isinstance(m, dict))) */
    json_t *texts = json_array();
    json_append(texts, cct2_copy_or_jsonnull(summary));
    json_t *both[2] = { head, tail };
    for (int b = 0; b < 2; b++) {
        for (size_t i = 0; i < json_len(both[b]); i++) {
            const json_t *m = json_get(both[b], i);
            if (!cct2_is_plain_dict(m)) continue;
            json_t *ca = cct2_args();
            cct2_arg_j(ca, "content", cct2_get(m, "content"));
            json_t *t = cct2_call(io, "_content_text_for_contains", ca);
            if (cct2_is_raise(t)) { json_free(texts); json_free(head); json_free(tail); return t; }
            json_append(texts, t ? t : json_null());
        }
    }

    /* any(RULE in text) — short-circuits; non-container text raises */
    int has_rule = 0;
    json_t *err = NULL;
    for (size_t i = 0; i < json_len(texts); i++) {
        int hit = cct2_in_py(json_get(texts, i), CCT2_SKILL_SAFETY_RULE, &err);
        if (err) { json_free(texts); json_free(head); json_free(tail); return err; }
        if (hit) { has_rule = 1; break; }
    }
    int has_prune = 0;
    if (!has_rule) {
        for (size_t i = 0; i < json_len(texts); i++) {
            int hit = cct2_in_py(json_get(texts, i), CCT2_SKILL_PRUNED_MARKER_PREFIX, &err);
            if (err) { json_free(texts); json_free(head); json_free(tail); return err; }
            if (hit) { has_prune = 1; break; }
        }
    }
    json_free(texts);

    json_t *ret = json_array();
    if (has_rule || !has_prune) {
        json_append(ret, cct2_copy_or_jsonnull(summary));
    } else {
        /* f"{summary}{SUFFIX}" — summary is a str on this path */
        char *ns = cct2_xfmt("%s%s", cct2_is_str(summary) ? (summary->str_val ? summary->str_val : "") : "", CCT2_SKILL_SAFETY_SUFFIX);
        json_append(ret, json_string(ns ? ns : ""));
        free(ns);
    }
    json_append(ret, head);
    json_append(ret, tail);
    return ret;
}

/* PoP: cct2_elide_keeping_skill_safety_rule @ agent/context_compressor_tail.py:elide_keeping_skill_safety_rule */
static json_t *cct2_elide_keeping_skill_safety_rule(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *content = cct2_get(in, "content");
    const json_t *limit = cct2_get(in, "limit");
    json_t *err = NULL;
    int has_rule = cct2_in_py(content, CCT2_SKILL_SAFETY_RULE, &err);
    if (err) return err;
    if (!has_rule) {
        json_t *a = cct2_args();
        cct2_arg_j(a, "text", content);
        cct2_arg_j(a, "limit", limit);
        return cct2_call(io, "elide", a);
    }
    /* elide(without_skill_safety_rule(content), limit - len(SUFFIX)) + SUFFIX */
    json_t *stripped = cct2h_without_rule(content);
    if (cct2_is_raise(stripped)) return stripped;
    if (!cct2_is_numeric(limit)) {
        char *m = cct2_xfmt("unsupported operand type(s) for -: '%s' and 'int'", cct2_typename(limit));
        json_free(stripped);
        return cct2_type_raise(m ? m : "");
    }
    double new_limit = cct2_num(limit) - (double)cct2_utf8_len(CCT2_SKILL_SAFETY_SUFFIX);
    json_t *a = cct2_args();
    cct2_arg_j(a, "text", stripped);
    cct2_arg_n(a, "limit", new_limit);
    json_free(stripped);
    json_t *e = cct2_call(io, "elide", a);
    if (cct2_is_raise(e)) return e;
    char *cat = cct2_concat_suffix(e, CCT2_SKILL_SAFETY_SUFFIX, &err);
    json_free(e);
    if (err) return err;
    json_t *out = json_string(cat ? cat : "");
    free(cat);
    return out;
}

/* PoP: cct2_with_pruned_row_skill_safety_rule @ agent/context_compressor_tail.py:with_pruned_row_skill_safety_rule */
static json_t *cct2_with_pruned_row_skill_safety_rule(cct2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *original = cct2_get(in, "original");
    const json_t *pruned = cct2_get(in, "pruned");
    if (!cct2_is_arr(pruned)) {
        char *m = cct2_xfmt("'%s' object is not iterable", cct2_typename(pruned));
        return cct2_type_raise(m ? m : "");
    }
    /* any(RULE in _content_text_for_contains(m.get("content")) for m in pruned if isinstance(m, dict)) */
    int has_rule = 0;
    for (size_t i = 0; i < json_len(pruned); i++) {
        const json_t *m = json_get(pruned, i);
        if (!cct2_is_plain_dict(m)) continue;
        json_t *ca = cct2_args();
        cct2_arg_j(ca, "content", cct2_get(m, "content"));
        json_t *t = cct2_call(io, "_content_text_for_contains", ca);
        if (cct2_is_raise(t)) return t;
        json_t *err = NULL;
        int hit = cct2_in_py(t, CCT2_SKILL_SAFETY_RULE, &err);
        json_free(t);
        if (err) return err;
        if (hit) { has_rule = 1; break; }
    }
    if (has_rule) return json_copy(pruned);
    /* zip(original, pruned) */
    if (!cct2_is_arr(original)) {
        char *m = cct2_xfmt("'%s' object is not iterable", cct2_typename(original));
        return cct2_type_raise(m ? m : "");
    }
    size_t n = json_len(original) < json_len(pruned) ? json_len(original) : json_len(pruned);
    for (size_t i = 0; i < n; i++) {
        const json_t *before = json_get(original, i);
        const json_t *after = json_get(pruned, i);
        const json_t *content = cct2_is_plain_dict(after) ? cct2_get(after, "content") : NULL;
        if (cct2_is_str(content) && cct2_contains(content->str_val ? content->str_val : "", CCT2_SKILL_PRUNED_MARKER_PREFIX)) {
            if (!cct2_is_plain_dict(before)) return cct2_attr_raise(before, "get");
            const json_t *bc = cct2_get(before, "content");
            int equal = cct2_is_str(bc) && strcmp(content->str_val ? content->str_val : "", bc->str_val ? bc->str_val : "") == 0;
            if (!equal) {
                json_t *out = json_array();
                for (size_t j = 0; j < i; j++) json_append(out, json_copy(json_get(pruned, j)));
                char *nc = cct2_xfmt("%s%s", content->str_val ? content->str_val : "", CCT2_SKILL_SAFETY_SUFFIX);
                json_t *nm = json_copy(after);
                json_set(nm, "content", json_string(nc ? nc : ""));
                free(nc);
                json_append(out, nm);
                for (size_t j = i + 1; j < json_len(pruned); j++) json_append(out, json_copy(json_get(pruned, j)));
                return out;
            }
        }
    }
    return json_copy(pruned);
}
