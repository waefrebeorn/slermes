/* context_compressor_flow3.c — third-wave FLOW port of
 * agent/context_compressor.py (LIVE authority, quarry READ-ONLY).
 *
 * Ports 15 UNCLAIMED module functions — the tool-result summarization flow:
 *   _sum_terminal, _sum_write_file, _sum_search_files, _sum_browser,
 *   _sum_delegate_task, _sum_execute_code, _sum_web_extract,
 *   _sum_skills_list, _sum_skill_manage, _sum_cronjob_manage,
 *   _sum_process_manage, _summarize_refused_tool_result, _bullets,
 *   _tool_result_parts, _summary_part_text.
 *
 * Each function's OWN logic (truthiness, `or` short-circuit, `is None` vs
 * `is False`, kwarg/operand evaluation order, str slicing / strip / comma
 * format, dict.fromkeys dedupe, int-vs-bool isinstance) is transcribed per
 * call site.  Claimed siblings and foreign helpers enter through the cc3_io_t
 * universal seam; builtin re.search/json.dumps shapes are transcribed in C.
 *
 * FULL Python 3.14 builtin exception hierarchy (CC3_EXC_PARENT + cc3_exc_is_base):
 * a scripted {"__raise__":kind,...} seam result propagates past every site;
 * base-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/
 * BaseExceptionGroup) are classified identically to live `except Exception`
 * matching.  Unknown scripted custom classes are treated as Exception
 * descendants, matching the live drive.
 *
 * Value model (shared with tests/oracle_context_compressor_flow3.py):
 *   - raise specs: {"__raise__":"<Class>","message":str}
 *   - messages / payloads / args: plain JSON objects/arrays
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator): str()/strip()/slice
 * positions carry ASCII text; re.search is the byte-pinned `"<key>"\s*:\s*<int>`
 * shape; _PRUNE_MIN_CHARS = 200; _IMAGE_PART_TYPES = {image,image_url,input_image}.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o).  The harness
 * #includes this .c via "agent/context_compressor_flow3.c".  Every symbol is
 * static cc3_* / cc3h_*.
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow3 \
 *           oracle_context_compressor_flow3.py "" "lib/libjson/json.o"
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

#define CC3_PRUNE_MIN_CHARS 200

/* ── universal seam ── */
typedef struct cc3_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cc3_io_t;

/* ── accessors / truthiness ── */
static int cc3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cc3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cc3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cc3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cc3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cc3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

/* Python truthiness of a JSON value. */
static int cc3_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cc3_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cc3_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cc3_num(const json_t *v) { return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0; }
static long long cc3_int(const json_t *v) { return (long long)cc3_num(v); }

/* ── formatting helpers ── */
static char *cc3_xfmt(const char *fmt, ...)
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

static json_t *cc3_s(char *buf) { json_t *j = json_string(buf ? buf : ""); free(buf); return j; }

/* Python str() of a JSON scalar (containers are outside the fixture domain). */
static char *cc3_py_str(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cc3_xfmt("None");
    if (v->type == JSON_BOOL) return cc3_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18) return cc3_xfmt("%lld", (long long)d);
        return cc3_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cc3_xfmt("%s", v->str_val ? v->str_val : "");
    return cc3_xfmt("");   /* containers are not in the ASCII scalar domain */
}

/* args.get(key, default) then str(): missing -> default, present -> str(value). */
static char *cc3_get_str_default(const json_t *o, const char *k, const char *def)
{
    const json_t *v = cc3_get(o, k);
    if (!v) return cc3_xfmt("%s", def);
    return cc3_py_str(v);
}

static int cc3_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() over the ASCII whitespace set. */
static char *cc3_strip(const char *s)
{
    if (!s) return cc3_xfmt("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cc3_ws((unsigned char)s[a])) a++;
    while (b > a && cc3_ws((unsigned char)s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (!o) return cc3_xfmt("");
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

/* Python str.lstrip() over the ASCII whitespace set. */
static char *cc3_lstrip(const char *s)
{
    if (!s) return cc3_xfmt("");
    size_t n = strlen(s), a = 0;
    while (a < n && cc3_ws((unsigned char)s[a])) a++;
    return cc3_xfmt("%s", s + a);
}

/* first len bytes (Python [:n]); returns a fresh NUL-terminated buffer */
static char *cc3_prefix(const char *s, size_t n)
{
    size_t l = s ? strlen(s) : 0;
    if (n > l) n = l;
    char *o = (char *)malloc(n + 1);
    if (!o) return cc3_xfmt("");
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}

/* f"{n:,}" — comma-group an integer. */
static char *cc3_comma(long long n)
{
    unsigned long long u = n < 0 ? (unsigned long long)(-(n + 1)) + 1ULL : (unsigned long long)n;
    char digits[32];
    int nd = snprintf(digits, sizeof digits, "%llu", u);
    if (nd <= 0) return cc3_xfmt("0");
    size_t groups = ((size_t)nd + 2) / 3;
    size_t cap = (size_t)nd + groups + 4;
    char *o = (char *)malloc(cap);
    if (!o) return cc3_xfmt("0");
    size_t k = 0;
    if (n < 0) o[k++] = '-';
    for (int i = 0; i < nd; i++) {
        if (i > 0 && (nd - i) % 3 == 0) o[k++] = ',';
        o[k++] = digits[i];
    }
    o[k] = '\0';
    return o;
}

/* Transcribe re.search(r'"<key>"\s*:\s*(-?\d+)', text) -> group(1) or "".
 * allow_neg=0 transcribes `(\d+)` (no sign). */
static char *cc3_extract_num(const char *text, const char *key, int allow_neg)
{
    if (!text) return cc3_xfmt("");
    char pat[128];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(text, pat);
    if (!p) return cc3_xfmt("");
    p += strlen(pat);
    while (*p && cc3_ws((unsigned char)*p)) p++;
    if (*p != ':') return cc3_xfmt("");
    p++;
    while (*p && cc3_ws((unsigned char)*p)) p++;
    char out[64];
    size_t k = 0;
    if (allow_neg && *p == '-') { out[k++] = '-'; p++; }
    if (!isdigit((unsigned char)*p)) return cc3_xfmt("");
    while (isdigit((unsigned char)*p) && k < 62) out[k++] = *p++;
    out[k] = '\0';
    return cc3_xfmt("%s", out);
}

/* ── raise specs ── */
static json_t *cc3_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static int cc3_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cc3_kind(const json_t *r)
{
    const char *k = cc3_str(cc3_get(r, "__raise__"));
    return k ? k : "";
}

/* ── FULL builtin exception hierarchy (Python 3.14) ──
 * parent[kind] = immediate base name.  IOError/EnvironmentError/WindowsError
 * are OSError aliases; py3.14 adds PythonFinalizationError(RuntimeError) and
 * _IncompleteInputError(SyntaxError).  ExceptionGroup inherits Exception (and
 * BaseExceptionGroup) so it stays inside `except Exception`; CancelledError is
 * asyncio.CancelledError, a BaseException since 3.8.  cc3_exc_is_base() walks
 * the chain the same way live `except Exception` matches: an unknown/scripted
 * custom class is treated as an Exception descendant (not base-only). */
typedef struct { const char *name; const char *parent; } cc3_exc_edge_t;
static const cc3_exc_edge_t CC3_EXC_PARENT[] = {
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

static const char *cc3_exc_parent(const char *kind)
{
    if (!kind || !*kind) return NULL;
    for (const cc3_exc_edge_t *e = CC3_EXC_PARENT; e->name; e++)
        if (strcmp(kind, e->name) == 0) return e->parent;
    return NULL;  /* unknown / scripted custom class */
}

static int cc3_exc_is_base(const char *kind)
{
    if (!kind || !*kind) return 0;
    const char *cur = kind;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, "Exception") == 0 || strcmp(cur, "ExceptionGroup") == 0) return 0;
        if (strcmp(cur, "BaseException") == 0) return 1;
        const char *p = cc3_exc_parent(cur);
        if (!p) return 0;   /* unknown custom class -> Exception-descendant */
        cur = p;
    }
    return 0;
}

/* ── universal seam call ── */
static json_t *cc3_call(cc3_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cc3_xfmt("no seam installed for %s", name);
        r = cc3_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cc3_args(void) { return json_object(); }
static void cc3_arg_s(json_t *a, const char *k, const char *v) { if (v) json_set(a, k, json_string(v)); }
static void cc3_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cc3_arg_i(json_t *a, const char *k, long long v) { json_set(a, k, json_number((double)v)); }

/* seam: _str_arg(args, key, default="") */
static json_t *cc3_seam_str_arg(cc3_io_t *io, const json_t *args, const char *key, const char *def)
{
    json_t *a = cc3_args();
    cc3_arg_j(a, "args", args);
    cc3_arg_s(a, "key", key);
    cc3_arg_s(a, "default", def);
    return cc3_call(io, "_str_arg", a);
}

/* seam: _result_failure_suffix(content) */
static json_t *cc3_seam_result_failure(cc3_io_t *io, const json_t *content)
{
    json_t *a = cc3_args();
    cc3_arg_j(a, "content", content);
    return cc3_call(io, "_result_failure_suffix", a);
}

/* seam: _failure_suffix(reason, label="FAILED") */
static json_t *cc3_seam_failure(cc3_io_t *io, const json_t *reason, const char *label)
{
    json_t *a = cc3_args();
    cc3_arg_j(a, "reason", reason);
    cc3_arg_s(a, "label", label);
    return cc3_call(io, "_failure_suffix", a);
}

/* seam: _json_dict(text) */
static json_t *cc3_seam_json_dict(cc3_io_t *io, const json_t *text)
{
    json_t *a = cc3_args();
    cc3_arg_j(a, "text", text);
    return cc3_call(io, "_json_dict", a);
}

/* ─────────────────────────── ported functions ─────────────────────────── */

/* PoP: cc3_sum_terminal @ agent/context_compressor.py:_sum_terminal */
static json_t *cc3_sum_terminal(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    long long lc = cc3_int(cc3_get(in, "line_count"));

    json_t *cj = cc3_seam_str_arg(io, args, "command", "");
    if (cc3_is_raise(cj)) return cj;
    const char *cmd = cc3_str(cj) ? cc3_str(cj) : "";
    char *cmd2;
    if (strlen(cmd) <= 80) cmd2 = cc3_xfmt("%s", cmd);
    else { char buf[80]; memcpy(buf, cmd, 77); buf[77] = '\0'; cmd2 = cc3_xfmt("%s...", buf); }
    json_free(cj);

    char *code = cc3_extract_num(cc3_str(content), "exit_code", 1);
    if (!code[0]) { free(code); code = cc3_xfmt("?"); }   /* `else "?"` in live */
    json_t *ret = cc3_s(cc3_xfmt("[terminal] ran `%s` -> exit %s, %lld lines output", cmd2, code, lc));
    free(cmd2); free(code);
    return ret;
}

/* PoP: cc3_sum_write_file @ agent/context_compressor.py:_sum_write_file */
static json_t *cc3_sum_write_file(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    char *path = cc3_get_str_default(args, "path", "?");

    json_t *fj = cc3_seam_result_failure(io, content);
    if (cc3_is_raise(fj)) { free(path); return fj; }
    const char *fail = cc3_str(fj) ? cc3_str(fj) : "";
    if (fail[0]) {
        json_t *ret = cc3_s(cc3_xfmt("[write_file] %s%s", path, fail));
        free(path); json_free(fj);
        return ret;
    }
    json_free(fj);

    char *lines;
    const json_t *cv = cc3_get(args, "content");
    if (cc3_truth(cv)) {
        json_t *cj = cc3_seam_str_arg(io, args, "content", "");
        if (cc3_is_raise(cj)) { free(path); return cj; }
        const char *cs = cc3_str(cj) ? cc3_str(cj) : "";
        long long cnt = 1;
        for (const char *p = cs; *p; p++) if (*p == '\n') cnt++;
        lines = cc3_xfmt("%lld", cnt);
        json_free(cj);
    } else {
        lines = cc3_xfmt("?");
    }
    json_t *ret = cc3_s(cc3_xfmt("[write_file] wrote to %s (%s lines)", path, lines));
    free(path); free(lines);
    return ret;
}

/* PoP: cc3_sum_search_files @ agent/context_compressor.py:_sum_search_files */
static json_t *cc3_sum_search_files(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    char *target = cc3_get_str_default(args, "target", "content");
    char *pattern = cc3_get_str_default(args, "pattern", "?");
    char *path = cc3_get_str_default(args, "path", ".");
    char *count = cc3_extract_num(cc3_str(content), "total_count", 0);
    if (!count[0]) { free(count); count = cc3_xfmt("?"); }   /* `else "?"` in live */
    json_t *ret = cc3_s(cc3_xfmt("[search_files] %s search for '%s' in %s -> %s matches",
                                 target, pattern, path, count));
    free(target); free(pattern); free(path); free(count);
    return ret;
}

/* PoP: cc3_sum_browser @ agent/context_compressor.py:_sum_browser */
static json_t *cc3_sum_browser(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    const char *name = cc3_str(cc3_get(in, "name")) ? cc3_str(cc3_get(in, "name")) : "?";
    long long cl = cc3_int(cc3_get(in, "content_len"));
    char *url = cc3_get_str_default(args, "url", "");
    char *ref = cc3_get_str_default(args, "ref", "");
    char *detail;
    if (url[0]) detail = cc3_xfmt(" %s", url);
    else if (ref[0]) detail = cc3_xfmt(" ref=%s", ref);
    else detail = cc3_xfmt("");
    char *comma = cc3_comma(cl);

    json_t *fj = cc3_seam_result_failure(io, content);
    if (cc3_is_raise(fj)) { free(url); free(ref); free(detail); free(comma); return fj; }
    const char *fail = cc3_str(fj) ? cc3_str(fj) : "";
    json_t *ret = cc3_s(cc3_xfmt("[%s]%s (%s chars)%s", name, detail, comma, fail));
    free(url); free(ref); free(detail); free(comma); json_free(fj);
    return ret;
}

/* PoP: cc3_sum_delegate_task @ agent/context_compressor.py:_sum_delegate_task */
static json_t *cc3_sum_delegate_task(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    long long cl = cc3_int(cc3_get(in, "content_len"));

    json_t *gj = cc3_seam_str_arg(io, args, "goal", "");
    if (cc3_is_raise(gj)) return gj;
    const char *goal = cc3_str(gj) ? cc3_str(gj) : "";
    char *goal2;
    if (strlen(goal) <= 60) goal2 = cc3_xfmt("%s", goal);
    else { char buf[80]; memcpy(buf, goal, 57); buf[57] = '\0'; goal2 = cc3_xfmt("%s...", buf); }
    json_free(gj);
    char *comma = cc3_comma(cl);

    json_t *fj = cc3_seam_result_failure(io, content);
    if (cc3_is_raise(fj)) { free(goal2); free(comma); return fj; }
    const char *fail = cc3_str(fj) ? cc3_str(fj) : "";
    json_t *ret = cc3_s(cc3_xfmt("[delegate_task] '%s' (%s chars result)%s", goal2, comma, fail));
    free(goal2); free(comma); json_free(fj);
    return ret;
}

/* PoP: cc3_sum_execute_code @ agent/context_compressor.py:_sum_execute_code */
static json_t *cc3_sum_execute_code(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    long long lc = cc3_int(cc3_get(in, "line_count"));

    json_t *cj = cc3_seam_str_arg(io, args, "code", "");
    if (cc3_is_raise(cj)) return cj;
    const char *code = cc3_str(cj) ? cc3_str(cj) : "";
    char *head = cc3_prefix(code, 60);
    size_t hn = strlen(head);
    for (size_t i = 0; i < hn; i++) if (head[i] == '\n') head[i] = ' ';
    char *preview;
    if (strlen(code) > 60) preview = cc3_xfmt("%s...", head);
    else preview = cc3_xfmt("%s", head);
    free(head); json_free(cj);
    json_t *ret = cc3_s(cc3_xfmt("[execute_code] `%s` (%lld lines output)", preview, lc));
    free(preview);
    return ret;
}

/* PoP: cc3_sum_web_extract @ agent/context_compressor.py:_sum_web_extract */
static json_t *cc3_sum_web_extract(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    long long cl = cc3_int(cc3_get(in, "content_len"));
    const json_t *urls = cc3_get(args, "urls");
    char *first;
    if (cc3_is_arr(urls) && urls->c.count > 0) {
        const json_t *u0 = json_get(urls, 0);
        if (cc3_is_obj(u0)) {
            /* first.get("url") or first.get("href") or "?" */
            const json_t *u = cc3_get(u0, "url");
            if (!cc3_truth(u)) u = cc3_get(u0, "href");
            first = (cc3_is_str(u) && cc3_str(u)[0]) ? cc3_xfmt("%s", cc3_str(u)) : cc3_xfmt("?");
        } else if (cc3_is_str(u0)) {
            first = cc3_xfmt("%s", u0->str_val ? u0->str_val : "");
        } else {
            first = cc3_xfmt("?");
        }
        if (cc3_is_arr(urls) && urls->c.count > 1) {
            char *tmp = cc3_xfmt("%s (+%llu more)", first, (unsigned long long)(urls->c.count - 1));
            free(first); first = tmp;
        }
    } else {
        first = cc3_xfmt("?");
    }
    char *comma = cc3_comma(cl);

    json_t *fj = cc3_seam_result_failure(io, content);
    if (cc3_is_raise(fj)) { free(first); free(comma); return fj; }
    const char *fail = cc3_str(fj) ? cc3_str(fj) : "";
    json_t *ret = cc3_s(cc3_xfmt("[web_extract] %s (%s chars)%s", first, comma, fail));
    free(first); free(comma); json_free(fj);
    return ret;
}

/* PoP: cc3_sum_skills_list @ agent/context_compressor.py:_sum_skills_list */
static json_t *cc3_sum_skills_list(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    long long cl = cc3_int(cc3_get(in, "content_len"));

    json_t *aj = cc3_seam_str_arg(io, args, "category", "");
    if (cc3_is_raise(aj)) return aj;
    const char *cat = cc3_str(aj) ? cc3_str(aj) : "";
    char *scope = cat[0] ? cc3_xfmt(" category=%s", cat) : cc3_xfmt("");
    json_free(aj);

    json_t *pj = cc3_seam_json_dict(io, content);
    if (cc3_is_raise(pj)) { free(scope); return pj; }
    const json_t *count = cc3_get(pj, "count");
    char *listed;
    int is_int = count && (count->type == JSON_NUMBER || count->type == JSON_BOOL);
    if (is_int) {
        char *cs = cc3_py_str(count);
        listed = cc3_xfmt(" %s skills", cs);
        free(cs);
    } else {
        listed = cc3_xfmt("");
    }
    json_free(pj);

    json_t *fj = cc3_seam_result_failure(io, content);
    if (cc3_is_raise(fj)) { free(scope); free(listed); return fj; }
    const char *fail = cc3_str(fj) ? cc3_str(fj) : "";
    char *comma = cc3_comma(cl);
    json_t *ret = cc3_s(cc3_xfmt("[skills_list]%s%s%s (%s chars)", scope, listed, fail, comma));
    free(scope); free(listed); free(comma); json_free(fj);
    return ret;
}

/* PoP: cc3_sum_skill_manage @ agent/context_compressor.py:_sum_skill_manage */
static json_t *cc3_sum_skill_manage(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    long long cl = cc3_int(cc3_get(in, "content_len"));
    const json_t *ops = cc3_get(args, "operations");
    char *summary;

    if (cc3_is_arr(ops) && ops->c.count > 0) {
        char *rendered[3] = { NULL, NULL, NULL };
        int rn = 0;
        for (size_t i = 0; i < ops->c.count; i++) {
            const json_t *op = json_get(ops, i);
            if (!cc3_is_obj(op)) continue;
            json_t *act = cc3_seam_str_arg(io, op, "action", "?");
            if (cc3_is_raise(act)) { for(int j=0;j<rn;j++)free(rendered[j]); return act; }
            json_t *nm = cc3_seam_str_arg(io, op, "name", "?");
            if (cc3_is_raise(nm)) { json_free(act); for(int j=0;j<rn;j++)free(rendered[j]); return nm; }
            const char *a = cc3_str(act) ? cc3_str(act) : "?";
            const char *n = cc3_str(nm) ? cc3_str(nm) : "?";
            if (rn < 3) rendered[rn++] = cc3_xfmt("%s %s", a, n);
            json_free(act); json_free(nm);
        }
        char *joined = cc3_xfmt("");
        for (int j = 0; j < rn; j++) {
            char *tmp = j ? cc3_xfmt("%s; %s", joined, rendered[j]) : cc3_xfmt("%s", rendered[j]);
            free(joined); joined = tmp;
        }
        if (ops->c.count > 3) {
            char *tmp = cc3_xfmt("%s (+%llu more)", joined, (unsigned long long)(ops->c.count - 3));
            free(joined); joined = tmp;
        }
        summary = cc3_xfmt("[skill_manage] %s", joined);
        free(joined);
        for (int j = 0; j < rn; j++) free(rendered[j]);
    } else {
        json_t *act = cc3_seam_str_arg(io, args, "action", "?");
        if (cc3_is_raise(act)) return act;
        json_t *nm = cc3_seam_str_arg(io, args, "name", "?");
        if (cc3_is_raise(nm)) { json_free(act); return nm; }
        const char *a = cc3_str(act) ? cc3_str(act) : "?";
        const char *n = cc3_str(nm) ? cc3_str(nm) : "?";
        summary = cc3_xfmt("[skill_manage] %s %s", a, n);
        json_free(act); json_free(nm);
    }

    json_t *fj = cc3_seam_result_failure(io, content);
    if (cc3_is_raise(fj)) { free(summary); return fj; }
    const char *fail = cc3_str(fj) ? cc3_str(fj) : "";
    char *comma = cc3_comma(cl);
    json_t *ret = cc3_s(cc3_xfmt("%s%s (%s chars)", summary, fail, comma));
    free(summary); free(comma); json_free(fj);
    return ret;
}

/* PoP: cc3_sum_cronjob_manage @ agent/context_compressor.py:_sum_cronjob_manage */
static json_t *cc3_sum_cronjob_manage(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    char *action = cc3_get_str_default(args, "action", "?");
    char *stub = cc3_xfmt("[cronjob] %s", action);
    free(action);

    json_t *pj = cc3_seam_json_dict(io, content);
    if (cc3_is_raise(pj)) { free(stub); return pj; }

    json_t *a2 = cc3_args(); cc3_arg_j(a2, "payload", pj);
    json_t *sj = cc3_call(io, "_payload_failure_suffix", a2);
    if (cc3_is_raise(sj)) { json_free(pj); free(stub); return sj; }
    const char *suffix0 = cc3_str(sj) ? cc3_str(sj) : "";
    char *suffix = cc3_xfmt("%s", suffix0);
    json_free(sj);

    if (!suffix[0]) {
        const json_t *job = cc3_get(pj, "job");
        if (!cc3_is_obj(job)) job = NULL;
        const json_t *error = job ? cc3_get(job, "execution_error") : NULL;
        const json_t *skipped = job ? cc3_get(job, "execution_skipped") : NULL;
        int err_present = error && !cc3_is_null(error);          /* error is not None */
        int skipped_none = !skipped || cc3_is_null(skipped);    /* skipped is None */
        const json_t *succ = job ? cc3_get(job, "execution_success") : NULL;
        int succ_false = succ && succ->type == JSON_BOOL && !succ->bool_val;  /* is False */
        if (err_present || (skipped_none && succ_false)) {
            json_t *rj = cc3_seam_failure(io, error, "FAILED");
            if (cc3_is_raise(rj)) { json_free(pj); free(stub); free(suffix); return rj; }
            free(suffix); suffix = cc3_xfmt("%s", cc3_str(rj) ? cc3_str(rj) : "");
            json_free(rj);
        } else if (!skipped_none) {
            json_t *rj = cc3_seam_failure(io, skipped, "SKIPPED");
            if (cc3_is_raise(rj)) { json_free(pj); free(stub); free(suffix); return rj; }
            free(suffix); suffix = cc3_xfmt("%s", cc3_str(rj) ? cc3_str(rj) : "");
            json_free(rj);
        }
    }
    json_free(pj);
    json_t *ret = cc3_s(cc3_xfmt("%s%s", stub, suffix));
    free(stub); free(suffix);
    return ret;
}

/* PoP: cc3_sum_process_manage @ agent/context_compressor.py:_sum_process_manage */
static json_t *cc3_sum_process_manage(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    char *action = cc3_get_str_default(args, "action", "?");
    char *sid = cc3_get_str_default(args, "session_id", "?");
    char *stub = cc3_xfmt("[process] %s session=%s", action, sid);
    free(action); free(sid);

    json_t *pj = cc3_seam_json_dict(io, content);
    if (cc3_is_raise(pj)) { free(stub); return pj; }

    json_t *a2 = cc3_args(); cc3_arg_j(a2, "payload", pj);
    json_t *sj = cc3_call(io, "_payload_failure_suffix", a2);
    if (cc3_is_raise(sj)) { json_free(pj); free(stub); return sj; }
    const char *suffix0 = cc3_str(sj) ? cc3_str(sj) : "";
    char *suffix = cc3_xfmt("%s", suffix0);
    json_free(sj);

    if (!suffix[0]) {
        const json_t *ec = cc3_get(pj, "exit_code");
        int is_int = ec && ec->type == JSON_NUMBER && ec->num_val == (double)(long long)ec->num_val;
        long long ecv = is_int ? (long long)ec->num_val : 0;
        const json_t *cr = cc3_get(pj, "completion_reason");
        int killed = cr && cc3_is_str(cr) && strcmp(cr->str_val ? cr->str_val : "", "killed") == 0;
        if (is_int && ecv != 0 && !killed) {
            char *reason = cc3_xfmt("exit code %lld", ecv);
            json_t *rnode = json_string(reason);
            json_t *rj = cc3_seam_failure(io, rnode, "FAILED");
            json_free(rnode);
            free(reason);
            if (cc3_is_raise(rj)) { json_free(pj); free(stub); free(suffix); return rj; }
            free(suffix); suffix = cc3_xfmt("%s", cc3_str(rj) ? cc3_str(rj) : "");
            json_free(rj);
        }
    }
    json_free(pj);
    json_t *ret = cc3_s(cc3_xfmt("%s%s", stub, suffix));
    free(stub); free(suffix);
    return ret;
}

/* PoP: cc3_summarize_refused_tool_result @ agent/context_compressor.py:_summarize_refused_tool_result */
static json_t *cc3_summarize_refused_tool_result(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *args = cc3_get(in, "args");
    const json_t *content = cc3_get(in, "content");
    const char *tool = cc3_str(cc3_get(in, "tool_name")) ? cc3_str(cc3_get(in, "tool_name")) : "?";

    json_t *pj = cc3_seam_json_dict(io, content);
    if (cc3_is_raise(pj)) return pj;
    const json_t *status = cc3_get(pj, "status");
    const json_t *errj = cc3_get(pj, "error");
    char *error = (errj && cc3_is_str(errj)) ? cc3_xfmt("%s", errj->str_val ? errj->str_val : "") : cc3_xfmt("");
    if (!error[0] && cc3_str(content)) {
        char *ls = cc3_strip(cc3_str(content));
        if (strncmp(ls, "BLOCKED", 7) == 0) {
            free(error); error = cc3_strip(cc3_str(content));
        }
        free(ls);
    }

    char *outcome = NULL;
    char *err_ls = cc3_lstrip(error);
    int st_pending = status && cc3_is_str(status) && strcmp(status->str_val ? status->str_val : "", "pending_approval") == 0;
    int st_blocked = status && cc3_is_str(status) && strcmp(status->str_val ? status->str_val : "", "blocked") == 0;
    if (st_pending) {
        outcome = cc3_xfmt("awaiting the user's approval, not run");
    } else if (error[0] && (st_blocked || strncmp(err_ls, "BLOCKED", 7) == 0)) {
        outcome = cc3_xfmt("BLOCKED, not run");
        if (strstr(error, "NOT consented") != NULL) {
            char *tmp = cc3_xfmt("%s; the user did NOT consent, do not retry or reach the same outcome another way", outcome);
            free(outcome); outcome = tmp;
        }
    } else {
        free(err_ls); json_free(pj); free(error);
        return json_null();   /* None */
    }
    free(err_ls);
    json_free(pj);

    /* target = _str_arg(args,"command") or _str_arg(args,"path")  (Python `or`) */
    json_t *tj = cc3_seam_str_arg(io, args, "command", "");
    if (cc3_is_raise(tj)) { free(error); free(outcome); return tj; }
    const char *tgt = cc3_str(tj) ? cc3_str(tj) : "";
    json_free(tj);
    if (!tgt[0]) {
        tj = cc3_seam_str_arg(io, args, "path", "");
        if (cc3_is_raise(tj)) { free(error); free(outcome); return tj; }
        tgt = cc3_str(tj) ? cc3_str(tj) : "";
        json_free(tj);
    }
    char *target;
    if (!tgt[0]) target = cc3_xfmt("");
    else if (strlen(tgt) <= 60) target = cc3_xfmt(" `%s`", tgt);
    else { char *h = cc3_prefix(tgt, 57); target = cc3_xfmt(" `%s...`", h); free(h); }
    char *full = cc3_xfmt("[%s]%s %s", tool, target, outcome);
    char *cut = cc3_prefix(full, CC3_PRUNE_MIN_CHARS - 1);
    free(full); free(target); free(error); free(outcome);
    return cc3_s(cut);
}

/* PoP: cc3_bullets @ agent/context_compressor.py:_bullets */
static json_t *cc3_bullets(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *items = cc3_get(in, "items");
    long long limit = cc3_get(in, "limit") ? cc3_int(cc3_get(in, "limit")) : 8;
    /* unique = [item for item in dict.fromkeys(item.strip() for item in items) if item][:limit] */
    char **uniq = NULL;
    size_t un = 0, ucap = 0;
    if (cc3_is_arr(items)) {
        for (size_t i = 0; i < items->c.count; i++) {
            if ((long long)un >= limit) break;   /* Python [:limit]; limit==0 -> empty */
            const json_t *it = json_get(items, i);
            const char *raw = cc3_str(it) ? cc3_str(it) : "";
            char *s = cc3_strip(raw);
            if (!s[0]) { free(s); continue; }
            int dup = 0;
            for (size_t j = 0; j < un; j++) if (strcmp(uniq[j], s) == 0) { dup = 1; break; }
            if (dup) { free(s); continue; }
            if (un == ucap) { ucap = ucap ? ucap * 2 : 8; uniq = (char **)realloc(uniq, ucap * sizeof(char *)); }
            uniq[un++] = s;
        }
    }
    if (un == 0) { free(uniq); return json_string("None."); }
    char *out = cc3_xfmt("");
    for (size_t i = 0; i < un; i++) {
        char *tmp = i ? cc3_xfmt("%s\n- %s", out, uniq[i]) : cc3_xfmt("- %s", uniq[i]);
        free(out); out = tmp;
    }
    for (size_t i = 0; i < un; i++) free(uniq[i]);
    free(uniq);
    return cc3_s(out);
}

/* PoP: cc3_tool_result_parts @ agent/context_compressor.py:_tool_result_parts */
static json_t *cc3_tool_result_parts(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *content = cc3_get(in, "content");
    if (cc3_is_obj(content) && cc3_truth(cc3_get(content, "_multimodal"))) {
        const json_t *inner = cc3_get(content, "content");
        return inner ? json_copy(inner) : json_null();
    }
    return content ? json_copy(content) : json_null();
}

/* image part types from session_rules.turns: {"image","image_url","input_image"} */
static int cc3_is_image_part_type(const char *t)
{
    if (!t) return 0;
    return strcmp(t, "image") == 0 || strcmp(t, "image_url") == 0 || strcmp(t, "input_image") == 0;
}

/* PoP: cc3_summary_part_text @ agent/context_compressor.py:_summary_part_text */
static json_t *cc3_summary_part_text(cc3_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *part = cc3_get(in, "part");
    if (cc3_is_str(part)) return json_copy(part);
    const json_t *pt = cc3_get(part, "type");
    const char *ptype = cc3_str(pt);
    if (ptype && strcmp(ptype, "text") == 0) {
        const json_t *txt = cc3_get(part, "text");
        return txt ? json_copy(txt) : json_string("");
    }
    if (ptype && cc3_is_image_part_type(ptype)) {
        json_t *a = cc3_args();
        cc3_arg_j(a, "part", part);
        json_t *lbl = cc3_call(io, "_image_part_label", a);
        if (cc3_is_raise(lbl)) return lbl;
        if (cc3_is_str(lbl)) return lbl;
        json_free(lbl);
        return json_string("[image]");
    }
    if (ptype && ptype[0]) return cc3_s(cc3_xfmt("[%s]", ptype));
    return json_string("[attachment]");
}
