/* context_compressor_flow4.c — fourth-wave FLOW port of
 * agent/context_compressor.py (LIVE authority, quarry READ-ONLY).
 *
 * Ports 11 UNCLAIMED module functions — the remainder beyond flow3:
 *   _effective_input_window, _model_threshold_key_rank, _is_text_only_content,
 *   _is_clarify_non_response, _restated_request_text, _authored_request_text,
 *   _pending_tool_round, _image_payload, _sum_skill_view,
 *   _collect_paths_from_jsonish, _sum_clarify.
 *
 * Each function's OWN logic (int/float coercion, str.strip/startswith/partition,
 * `or` short-circuit, `is` vs truthiness, dict/list walking, comma format,
 * last-occurrence rsplit, dedupe-append) is transcribed per call site. Claimed
 * siblings / foreign helpers / regex primitives enter through the cc4_io_t
 * universal seam; builtin re/json shapes are recorded, not reimplemented.
 *
 * FULL Python 3.14 builtin exception hierarchy (CC4_EXC_PARENT + cc4_exc_is_base):
 * a scripted {"__raise__":kind,...} seam result propagates past every site;
 * base-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/
 * BaseExceptionGroup) are classified identically to live `except Exception`.
 * Unknown scripted custom classes are treated as Exception descendants.
 *
 * Value model (shared with tests/oracle_context_compressor_flow4.py):
 *   - raise specs: {"__raise__":"<Class>","message":str}
 *   - messages / payloads / args: plain JSON objects/arrays
 *   - integral floats normalize to ints on both sides.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via "agent/context_compressor_flow4.c". Every symbol is
 * static cc4_* / cc4h_*.
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow4 \
 *           oracle_context_compressor_flow4.py "" "lib/libjson/json.o"
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

#define CC4_PRUNE_MIN_CHARS 200
#define CC4_MAX_CHARS (CC4_PRUNE_MIN_CHARS - 1)
#define CC4_STEER "steer"
#define CC4_SKILL_PREFIX "[SKILL_PRUNED:"
#define CC4_CANCELLED_TEXT "The user cancelled. Use your best judgement to proceed."
#define CC4_HDR ("[STILL IN PROGRESS — this is the active request, restated after the " \
                  "compaction boundary because it was not finished yet. Continue it; do not " \
                  "start over.]")

/* ── universal seam ── */
typedef struct cc4_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cc4_io_t;

/* ── accessors / truthiness ── */
static int cc4_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cc4_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cc4_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cc4_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cc4_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cc4_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cc4_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cc4_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static const json_t *cc4_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cc4_num(const json_t *v) { return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0; }
static long long cc4_int(const json_t *v) { return (long long)cc4_num(v); }

/* ── formatting helpers ── */
static char *cc4_xfmt(const char *fmt, ...)
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

static json_t *cc4_s(char *buf) { json_t *j = json_string(buf ? buf : ""); free(buf); return j; }

/* Python str() of a JSON scalar (containers are outside the fixture domain). */
static char *cc4_py_str(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cc4_xfmt("None");
    if (v->type == JSON_BOOL) return cc4_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18) return cc4_xfmt("%lld", (long long)d);
        return cc4_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cc4_xfmt("%s", v->str_val ? v->str_val : "");
    return cc4_xfmt("");
}

static int cc4_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() over the ASCII whitespace set. */
static char *cc4_strip(const char *s)
{
    if (!s) return cc4_xfmt("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cc4_ws((unsigned char)s[a])) a++;
    while (b > a && cc4_ws((unsigned char)s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (!o) return cc4_xfmt("");
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

/* Python value.rstrip(chars) over a byte set. */
static char *cc4_rstrip(const char *s, const char *chars)
{
    if (!s) return cc4_xfmt("");
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    char *o = (char *)malloc(n + 1);
    if (!o) return cc4_xfmt("");
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* f"{n:,}" — comma-group an integer. */
static char *cc4_comma(long long n)
{
    unsigned long long u = n < 0 ? (unsigned long long)(-(n + 1)) + 1ULL : (unsigned long long)n;
    char digits[32];
    int nd = snprintf(digits, sizeof digits, "%llu", u);
    if (nd <= 0) return cc4_xfmt("0");
    size_t groups = ((size_t)nd + 2) / 3;
    size_t cap = (size_t)nd + groups + 4;
    char *o = (char *)malloc(cap);
    if (!o) return cc4_xfmt("0");
    size_t k = 0;
    if (n < 0) o[k++] = '-';
    for (int i = 0; i < nd; i++) {
        if (i > 0 && (nd - i) % 3 == 0) o[k++] = ',';
        o[k++] = digits[i];
    }
    o[k] = '\0';
    return o;
}

/* ascii lower + strip, fresh buffer (for scope.strip().lower()) */
static char *cc4_lower_strip(const char *s)
{
    char *t = cc4_strip(s);
    for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
    return t;
}

/* ── raise specs ── */
static json_t *cc4_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static int cc4_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* ── FULL builtin exception hierarchy (Python 3.14) ──
 * parent[kind] = immediate base name. IOError/EnvironmentError/WindowsError
 * are OSError aliases; py3.14 adds PythonFinalizationError(RuntimeError) and
 * _IncompleteInputError(SyntaxError). ExceptionGroup inherits Exception (and
 * BaseExceptionGroup) so it stays inside `except Exception`; CancelledError is
 * asyncio.CancelledError, a BaseException since 3.8. cc4_exc_is_base() walks
 * the chain the same way live `except Exception` matches: an unknown/scripted
 * custom class is treated as an Exception descendant (not base-only). */
typedef struct { const char *name; const char *parent; } cc4_exc_edge_t;
static const cc4_exc_edge_t CC4_EXC_PARENT[] = {
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

static const char *cc4_exc_parent(const char *kind)
{
    if (!kind || !*kind) return NULL;
    for (const cc4_exc_edge_t *e = CC4_EXC_PARENT; e->name; e++)
        if (strcmp(kind, e->name) == 0) return e->parent;
    return NULL;
}

static int cc4_exc_is_base(const char *kind)
{
    if (!kind || !*kind) return 0;
    const char *cur = kind;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, "Exception") == 0 || strcmp(cur, "ExceptionGroup") == 0) return 0;
        if (strcmp(cur, "BaseException") == 0) return 1;
        const char *p = cc4_exc_parent(cur);
        if (!p) return 0;
        cur = p;
    }
    return 0;
}

/* ── universal seam call ── */
static json_t *cc4_call(cc4_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cc4_xfmt("no seam installed for %s", name);
        r = cc4_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* single-arg seam: args {key: value} */
static json_t *cc4_fire_j(cc4_io_t *io, const char *name, const char *key, const json_t *v)
{
    json_t *a = json_object();
    if (v && v->type != JSON_NULL) json_set(a, key, json_copy(v));
    return cc4_call(io, name, a);
}
static json_t *cc4_fire_s(cc4_io_t *io, const char *name, const char *key, const char *s)
{
    json_t *a = json_object();
    json_set(a, key, json_string(s ? s : ""));
    return cc4_call(io, name, a);
}

/* ═══════════════════════ ported functions ═══════════════════════ */

/* PoP: cc4_effective_input_window @ agent/context_compressor.py:_effective_input_window */
static json_t *cc4_effective_input_window(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    long long cl = cc4_int(cc4_get(in, "context_length"));
    const json_t *mt = cc4_get(in, "max_tokens");   /* Python `max_tokens or 0` */
    long long reservation = cc4_is_null(mt) ? 0 : cc4_int(mt);
    long long ew = cl - reservation;
    return json_number((double)(ew > 0 ? ew : cl));
}

/* PoP: cc4_model_threshold_key_rank @ agent/context_compressor.py:_model_threshold_key_rank */
static json_t *cc4_model_threshold_key_rank(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *key = cc4_str(cc4_get(in, "key"));
    const char *model = cc4_str(cc4_get(in, "model"));
    const char *provider = cc4_str(cc4_get(in, "provider"));
    key = key ? key : ""; model = model ? model : ""; provider = provider ? provider : "";
    json_t *out = json_array();
    const char *colon = strchr(key, ':');
    if (!colon) {
        if (strstr(model, key) != NULL) {
            json_append(out, json_number((double)strlen(key)));
            json_append(out, json_number(0));
        } else { json_free(out); out = json_null(); }
        return out;
    }
    size_t slen = (size_t)(colon - key);
    char *scope = (char *)malloc(slen + 1);
    if (!scope) { json_free(out); return json_null(); }
    memcpy(scope, key, slen); scope[slen] = '\0';
    char *sc = cc4_lower_strip(scope);
    free(scope);
    const char *substr = colon + 1;
    if (strcmp(sc, provider) == 0 && strstr(model, substr) != NULL) {
        json_append(out, json_number((double)strlen(substr)));
        json_append(out, json_number(1));
    } else { json_free(out); out = json_null(); }
    free(sc);
    return out;
}

/* PoP: cc4_is_text_only_content @ agent/context_compressor.py:_is_text_only_content */
static json_t *cc4_is_text_only_content(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *content = cc4_get(in, "content");
    if (cc4_is_str(content)) return json_bool(1);
    if (!cc4_is_arr(content)) return json_bool(0);
    for (size_t i = 0; i < content->c.count; i++) {
        const json_t *part = content->c.items[i];
        if (cc4_is_str(part)) continue;
        if (!cc4_is_obj(part)) return json_bool(0);
        const char *pt = cc4_str(cc4_get(part, "type"));
        if (!pt || (strcmp(pt, "text") != 0 && strcmp(pt, "input_text") != 0)) return json_bool(0);
        if (!cc4_is_str(cc4_get(part, "text"))) return json_bool(0);
    }
    return json_bool(1);
}

/* PoP: cc4_is_clarify_non_response @ agent/context_compressor.py:_is_clarify_non_response */
static json_t *cc4_is_clarify_non_response(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *item = cc4_get(in, "item");
    if (!cc4_is_str(item)) return json_bool(0);
    char *text = cc4_strip(cc4_str(item));
    int hit = 0;
    if (strcmp(text, CC4_CANCELLED_TEXT) == 0) hit = 1;
    static const char *PREFIXES[] = {
        "The user did not provide a response", "[user did not respond",
        "[clarify prompt could not be delivered", "[oneshot mode:", NULL
    };
    for (int i = 0; !hit && PREFIXES[i]; i++)
        if (strncmp(text, PREFIXES[i], strlen(PREFIXES[i])) == 0) hit = 1;
    if (hit) { free(text); return json_bool(1); }
    json_t *r = cc4_fire_s(io, "_clarify_headless_fullmatch", "text", text);
    free(text);
    if (cc4_is_raise(r)) return r;
    int v = cc4_truth(r) ? 1 : 0;
    json_free(r);
    return json_bool(v);
}

/* PoP: cc4_restated_request_text @ agent/context_compressor.py:_restated_request_text */
static json_t *cc4_restated_request_text(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *r = cc4_fire_j(io, "_content_text_for_contains", "content", cc4_get(in, "content"));
    if (cc4_is_raise(r)) return r;
    char *text = cc4_strip(cc4_str(r));
    json_free(r);
    const char *p = strstr(text, CC4_HDR);
    if (p) {
        const char *last = p;
        const char *q = strstr(p + 1, CC4_HDR);
        while (q) { last = q; q = strstr(q + 1, CC4_HDR); }
        char *tail = cc4_strip(last + strlen(CC4_HDR));
        free(text);
        return cc4_s(tail);
    }
    return cc4_s(text);
}

/* PoP: cc4_authored_request_text @ agent/context_compressor.py:_authored_request_text */
static json_t *cc4_authored_request_text(cc4_io_t *io, const json_t *in, json_t *st)
{
    json_t *r = cc4_restated_request_text(io, in, st);
    if (cc4_is_raise(r)) return r;
    const char *txt = cc4_str(r) ? cc4_str(r) : "";
    json_t *s = cc4_fire_s(io, "_gateway_reply_pointer_sub", "text", txt);
    json_free(r);
    if (cc4_is_raise(s)) return s;
    char *out = cc4_strip(cc4_str(s));
    json_free(s);
    return cc4_s(out);
}

/* PoP: cc4_pending_tool_round @ agent/context_compressor.py:_pending_tool_round */
static json_t *cc4_pending_tool_round(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *msgs = cc4_get(in, "messages");
    long long end = cc4_is_arr(msgs) ? (long long)msgs->c.count : 0;
    while (end > 0) {
        const json_t *m = msgs->c.items[end - 1];
        const char *dk = cc4_is_obj(m) ? cc4_str(cc4_get(m, "display_kind")) : NULL;
        if (dk && strcmp(dk, CC4_STEER) == 0) end--;
        else break;
    }
    long long start = end;
    while (start > 0) {
        const json_t *m = msgs->c.items[start - 1];
        const char *role = cc4_is_obj(m) ? cc4_str(cc4_get(m, "role")) : NULL;
        if (role && strcmp(role, "tool") == 0) start--;
        else break;
    }
    json_t *out = json_array();
    json_append(out, json_number((double)start));
    json_append(out, json_number((double)end));
    return out;
}

/* PoP: cc4_image_payload @ agent/context_compressor.py:_image_payload */
static json_t *cc4_image_payload(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *content = cc4_get(cc4_get(in, "msg"), "content");
    json_t *parts = cc4_fire_j(io, "_tool_result_parts", "content", content);
    if (cc4_is_raise(parts)) return parts;
    json_t *out = json_array();
    if (!cc4_is_arr(parts)) { json_free(parts); return out; }
    for (size_t i = 0; i < parts->c.count; i++) {
        const json_t *p = parts->c.items[i];
        json_t *ip = cc4_fire_j(io, "_is_image_part", "part", p);
        if (cc4_is_raise(ip)) { json_free(out); json_free(parts); return ip; }
        if (cc4_truth(ip)) {
            const json_t *iu = cc4_get(p, "image_url");
            const json_t *src = cc4_get(p, "source");
            const json_t *d1 = cc4_is_obj(iu) ? cc4_get(iu, "url") : iu;
            const json_t *data = cc4_truth(d1) ? d1
                                                : (cc4_is_obj(src) ? cc4_get(src, "data") : NULL);
            const char *s = cc4_str(data);
            json_append(out, json_string(s ? s : ""));
        }
        json_free(ip);
    }
    json_free(parts);
    return out;
}

/* PoP: cc4_sum_skill_view @ agent/context_compressor.py:_sum_skill_view */
static json_t *cc4_sum_skill_view(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc4_get(in, "args");
    const json_t *content = cc4_get(in, "content");
    const json_t *skillv = cc4_get(args, "name");
    char *skill = skillv ? cc4_py_str(skillv) : cc4_xfmt("?");

    json_t *fj = cc4_fire_j(io, "_result_failure_suffix", "content", content);
    if (cc4_is_raise(fj)) { free(skill); return fj; }
    const char *fs = cc4_str(fj) ? cc4_str(fj) : "";
    char *failed = cc4_xfmt("%s", fs);
    json_free(fj);
    if (!fs[0]) {
        json_t *vj = cc4_fire_j(io, "_skill_view_failed", "content", content);
        if (cc4_is_raise(vj)) { free(skill); free(failed); return vj; }
        if (cc4_truth(vj)) { free(failed); failed = cc4_xfmt(" FAILED"); }
        json_free(vj);
    }
    json_t *sj = cc4_fire_j(io, "_is_summary_stub", "content", content);
    if (cc4_is_raise(sj)) { free(skill); free(failed); return sj; }
    int stub = cc4_truth(sj);
    json_free(sj);
    int keep;
    if (stub) {
        const char *cs = cc4_str(content);
        keep = (cs && strstr(cs, CC4_SKILL_PREFIX)) ? 1 : 0;
    } else keep = failed[0] ? 0 : 1;

    char *marker = cc4_xfmt("");
    if (strcmp(skill, "?") != 0 && keep) {
        json_t *mj = cc4_fire_s(io, "_skill_pruned_marker", "skill", skill);
        if (cc4_is_raise(mj)) { free(skill); free(failed); free(marker); return mj; }
        const char *ms = cc4_str(mj) ? cc4_str(mj) : "";
        free(marker);
        marker = cc4_xfmt(" %s", ms);
        json_free(mj);
    }
    char *comma = cc4_comma(cc4_int(cc4_get(in, "content_len")));
    json_t *ret = cc4_s(cc4_xfmt("[skill_view] name=%s%s (%s chars)%s",
                                 skill, failed, comma, marker));
    free(skill); free(failed); free(marker); free(comma);
    return ret;
}

/* ── _collect_paths_from_jsonish helpers ── */
static int cc4_arr_has_str(const json_t *arr, const char *s)
{
    if (!cc4_is_arr(arr)) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *x = cc4_str(arr->c.items[i]);
        if (x && strcmp(x, s) == 0) return 1;
    }
    return 0;
}

static void cc4_dedupe_append(json_t *files, const char *value, int limit)
{
    char *v = cc4_strip(value);
    if (v[0] && !cc4_arr_has_str(files, v) && (int)json_len(files) < limit)
        json_append(files, json_string(v));
    free(v);
}

/* returns NULL on success, or a raise spec to propagate */
static json_t *cc4_collect_rec(cc4_io_t *io, const json_t *obj, json_t *files)
{
    if (cc4_is_obj(obj)) {
        for (size_t i = 0; i < obj->c.count; i++) {
            const char *k = obj->c.keys[i];
            const json_t *v = obj->c.items[i];
            if ((strcmp(k, "path") == 0 || strcmp(k, "workdir") == 0 ||
                 strcmp(k, "file_path") == 0 || strcmp(k, "output_path") == 0) && cc4_is_str(v))
                cc4_dedupe_append(files, cc4_str(v), 12);
            json_t *r = cc4_collect_rec(io, v, files);
            if (r) return r;
        }
    } else if (cc4_is_arr(obj)) {
        for (size_t i = 0; i < obj->c.count; i++) {
            json_t *r = cc4_collect_rec(io, obj->c.items[i], files);
            if (r) return r;
        }
    } else if (cc4_is_str(obj)) {
        json_t *m = cc4_fire_j(io, "_path_mention_findall", "text", obj);
        if (cc4_is_raise(m)) return m;
        if (cc4_is_arr(m)) {
            for (size_t i = 0; i < m->c.count; i++) {
                const char *ms = cc4_str(m->c.items[i]) ? cc4_str(m->c.items[i]) : "";
                char *r = cc4_rstrip(ms, ".,:;");
                cc4_dedupe_append(files, r, 12);
                free(r);
            }
        }
        json_free(m);
    }
    return NULL;
}

/* PoP: cc4_collect_paths_from_jsonish @ agent/context_compressor.py:_collect_paths_from_jsonish */
static json_t *cc4_collect_paths_from_jsonish(cc4_io_t *io, const json_t *in, json_t *st)
{
    const json_t *seed = cc4_get(in, "relevant_files");
    json_t *files = cc4_is_arr(seed) ? json_copy(seed) : json_array();
    json_t *r = cc4_collect_rec(io, cc4_get(in, "obj"), files);
    json_set(st, "relevant_files", files);
    if (r) return r;
    return json_null();
}

/* PoP: cc4_sum_clarify @ agent/context_compressor.py:_sum_clarify */
static json_t *cc4_sum_clarify(cc4_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *content = cc4_get(in, "content");
    json_t *payload = cc4_fire_j(io, "_json_dict", "text", content);
    if (cc4_is_raise(payload)) return payload;

    const json_t *resp = cc4_get(payload, "responses");
    json_t *tmp = NULL;
    if (!cc4_is_arr(resp) && cc4_get(payload, "user_response")) {
        tmp = json_array();
        json_append(tmp, json_copy(payload));
        resp = tmp;
    }
    json_t *answers = json_array();
    if (cc4_is_arr(resp)) {
        for (size_t i = 0; i < resp->c.count; i++) {
            const json_t *entry = resp->c.items[i];
            if (!cc4_is_obj(entry)) continue;
            const json_t *statv = cc4_get(entry, "status");
            int has_status = statv != NULL;
            const char *status = cc4_str(statv);
            const json_t *uv = cc4_get(entry, "user_response");
            int answered = (status && strcmp(status, "answered") == 0) ||
                           (!has_status && uv != NULL);
            if (!answered) continue;
            json_t *values;
            if (cc4_is_arr(uv)) values = json_copy(uv);
            else { values = json_array(); json_append(values, json_copy(uv)); }
            if (!has_status) {
                json_t *f = cc4_fire_j(io, "_filter_legacy_clarify_answers", "values", values);
                json_free(values);
                if (cc4_is_raise(f)) { json_free(answers); json_free(tmp); json_free(payload); return f; }
                values = f;
            }
            if (cc4_is_arr(values)) {
                for (size_t j = 0; j < values->c.count; j++) {
                    const char *a = cc4_str(values->c.items[j]);
                    if (a && a[0]) json_append(answers, json_string(a));
                }
            } else if (cc4_is_str(values)) {
                /* Python answers.extend("legacy") iterates its characters. */
                const char *s = cc4_str(values);
                for (const char *p = s; *p; p++) {
                    char one[2] = { *p, '\0' };
                    json_append(answers, json_string(one));
                }
            }
            json_free(values);
        }
    }
    json_free(tmp);
    json_free(payload);

    if (json_len(answers) == 0) {
        json_free(answers);
        return json_string("[clarify] asked user a question");
    }
    const json_t *val = (json_len(answers) == 1) ? json_get(answers, 0) : answers;
    json_t *a = json_object();
    json_set(a, "value", json_copy(val));
    json_set(a, "ensure_ascii", json_bool(0));
    json_t *dj = cc4_call(io, "_json_dumps", a);
    json_free(answers);
    if (cc4_is_raise(dj)) return dj;
    const char *ser = cc4_str(dj) ? cc4_str(dj) : "";
    char *text = cc4_xfmt("[clarify] user responded: %s", ser);
    json_free(dj);

    json_t *b = json_object();
    json_set(b, "text", json_string(text));
    json_set(b, "max_chars", json_number((double)CC4_MAX_CHARS));
    free(text);
    return cc4_call(io, "_elide", b);
}
