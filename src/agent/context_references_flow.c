/* context_references_flow.c -- FLOW + leaf port of the UNCLAIMED remainder of
 * agent/context_references.py (LIVE authority).
 *
 * Ports 18 UNCLAIMED functions (grep PoP first: the parse/preprocess flow,
 * _resolve_path, _ensure_reference_path_allowed, _strip_*, _parse_file_*,
 * _is_binary_file, _build_folder_listing, _iter_visible_entries,
 * _expand_reference, _expand_git_reference, _fetch_url_content,
 * _default_url_fetcher, _binary_reference_block, _file_metadata,
 * _agent_visible_path, format_reference_value, register/get provider are
 * CLAIMED in context_references.c / _ports.c / _gaps3.c and enter here as
 * scripted seams):
 *   1  ContextCompletionItem.__post_init__
 *   2  ContextReferenceProvider.autocomplete   (abstract body -> None)
 *   3  ContextReferenceProvider.expand         (abstract body -> None)
 *   4  register_context_reference_provider.publish  (nested closure)
 *   5  _GIT_REFERENCE_ARGS["diff"]   lambda
 *   6  _GIT_REFERENCE_ARGS["staged"] lambda
 *   7  _GIT_REFERENCE_ARGS["git"]    lambda
 *   8  _expand_path_reference
 *   9  _read_file_reference
 *   10 _read_file_reference._next_line  (nested)
 *   11 _format_file_reference
 *   12 _run_quiet
 *   13 _run_quiet._drain  (nested)
 *   14 _is_under
 *   15 _composer_paste_roots
 *   16 _agent_staged_path
 *   17 _on_disk_reference_block
 *   18 _oversized_text_reference_block
 * Claimed siblings and all foreign I/O enter through the cr_io_t universal
 * seam; ported siblings compose for REAL (_read_file_reference ->
 * _oversized_text_reference_block -> _on_disk_reference_block;
 * _agent_staged_path -> _is_under; _expand_path_reference -> the file chain).
 *
 * Decision sequences only: Python truthiness, `or` short-circuits, `is None`
 * identity vs truthy tests, kwarg/operand evaluation order, dict insertion
 * order, and exception catch scopes are transcribed per call site. A scripted
 * BaseException (SystemExit / KeyboardInterrupt / GeneratorExit /
 * CancelledError) propagates past every `except Exception` / `except OSError`
 * / `except LiveConnectionError` / `except TimeoutExpired` guard exactly as
 * live (cr_exc_is walks the FULL builtin hierarchy table below).
 *
 * Value model (shared with tests/oracle_context_references_flow.py):
 *   - attribute doubles: {"__attr__": {...}}  <-> types.SimpleNamespace
 *   - callable doubles:  {"__callable__": "<seam>"} (callable() is True)
 *   - raise specs:       {"__raise__": "<ClassName>", "message": str(e)}
 *   - paths: plain POSIX strings <-> PurePosixPath (ns["Path"] is
 *     PurePosixPath in the oracle; filesystem methods are patched seams)
 *   - Expansion tuples -> JSON arrays; integral floats normalize to ints.
 *   - object identity (publish `is not held`) modeled by "token" field
 *     equality; registry stores the token-bearing doubles.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - all text ASCII; paths absolute POSIX, single leading slash, no '.' /
 *     '..' components (resolve() output shape).
 *   - ref doubles carry all 7 ContextReference fields (present-null models
 *     None for line_start / line_end).
 *   - readline / read1 scripts are str pieces ("" or null = EOF); read1
 *     pieces materialize as bytes on the live side (ASCII -> decode is the
 *     identity, so the C keeps str).
 *   - stat scripts: {"__attr__": {"st_size": <int>}} or an OSError raise;
 *     absent st_size is the AttributeError branch.
 *   - publish owners are str|None (structural !=); providers carry "token".
 *   - int() in the git lambda: ASCII sign/digits, underscores BETWEEN digits,
 *     int-strip whitespace set (White_Space minus \x1c-\x1f); no unicode
 *     digits / no non-integral float targets.
 *   - IS_WINDOWS is parameterized per case (in.is_windows, default true --
 *     the live constant is host-dependent; both branches are ported).
 *   - _run_quiet drains serialize stdout then stderr BEFORE wait (live runs
 *     two daemon threads; the oracle's Thread double starts synchronously in
 *     zip order, so both sides share this deterministic trace order). kill
 *     does not mutate the scripted returncode (it is the final value).
 *   - CompletedProcess projects to {args, returncode, stdout, stderr}.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The
 * harness #includes this .c via the area-qualified
 * "agent/context_references_flow.c". Every symbol is static cr_* / crh_*.
 *
 * Gate: bash tests/run_one_oracle.sh context_references_flow \
 *           oracle_context_references_flow.py "" "lib/libjson/json.o"
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

/* -- live module constants (byte-pinned to agent/context_references.py +
 *    agent/model_metadata.py) -- */
#define CR_CHARS_PER_TOKEN 4
#define CR_MAX_QUIET_OUTPUT_BYTES (4 * 1024 * 1024)
#define CR_OUTPUT_CAP_EXCEEDED_RETURNCODE 137
#define CR_COMPOSER_PASTES_DIRNAME "composer-pastes"
#define CR_READ1_SIZE 65536
/* emoji / em-dash constants hex-escaped so this file stays pure ASCII */
#define CR_EMOJI_FILE "\xf0\x9f\x93\x84"     /* U+1F4C4 */
#define CR_EMOJI_FOLDER "\xf0\x9f\x93\x81"   /* U+1F4C1 */
#define CR_EMOJI_CLIP "\xf0\x9f\x93\x8e"     /* U+1F4CE */
#define CR_EM_DASH "\xe2\x80\x94"            /* U+2014 */

/* _FENCE_LANGUAGES (dict order irrelevant: lookup only) */
typedef struct { const char *ext; const char *lang; } cr_fence_t;
static const cr_fence_t CR_FENCE_LANGUAGES[] = {
    { ".py", "python" }, { ".js", "javascript" }, { ".ts", "typescript" },
    { ".tsx", "tsx" }, { ".jsx", "jsx" }, { ".json", "json" },
    { ".md", "markdown" }, { ".sh", "bash" }, { ".yml", "yaml" },
    { ".yaml", "yaml" }, { ".toml", "toml" },
};

/* -- FULL builtin exception hierarchy (+ the custom kinds the live defs
 *    catch). cr_exc_is walks this chain; an unknown scripted kind defaults
 *    to Exception (the oracle builds type(kind, (Exception,))). -- */
typedef struct { const char *name; const char *parent; } cr_exc_parent_t;
static const cr_exc_parent_t CR_EXC_PARENTS[] = {
    { "BaseException", "" },
    { "SystemExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "GeneratorExit", "BaseException" },
    { "CancelledError", "BaseException" },
    { "BaseExceptionGroup", "BaseException" },
    { "Exception", "BaseException" },
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
    { "WindowsError", "OSError" },
    { "EnvironmentError", "OSError" },
    { "IOError", "OSError" },
    { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" },
    { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" },
    { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" },
    { "TabError", "IndentationError" },
    { "SystemError", "Exception" },
    { "TypeError", "Exception" },
    { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" },
    { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" },
    { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" },
    { "DeprecationWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" },
    { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" },
    { "UserWarning", "Warning" },
    { "FutureWarning", "Warning" },
    { "ImportWarning", "Warning" },
    { "UnicodeWarning", "Warning" },
    { "BytesWarning", "Warning" },
    { "ResourceWarning", "Warning" },
    { "EncodingWarning", "Warning" },
    { "ExceptionGroup", "Exception" },
    /* custom kinds the live defs catch (identity classes in the oracle) */
    { "SubprocessError", "Exception" },
    { "TimeoutExpired", "SubprocessError" },
    { "LiveConnectionError", "RuntimeError" },
};

static int cr_exc_is(const char *kind, const char *ancestor)
{
    if (!kind) kind = "";
    if (strcmp(ancestor, "BaseException") == 0) return 1;
    const char *cur = kind;
    for (int guard = 0; guard < 64 && cur && cur[0]; guard++) {
        if (strcmp(cur, ancestor) == 0) return 1;
        const char *parent = NULL;
        for (size_t i = 0; i < sizeof(CR_EXC_PARENTS) / sizeof(CR_EXC_PARENTS[0]); i++) {
            if (strcmp(CR_EXC_PARENTS[i].name, cur) == 0) { parent = CR_EXC_PARENTS[i].parent; break; }
        }
        if (!parent) {
            /* unknown scripted kind: oracle builds type(kind, (Exception,)) */
            cur = "Exception";
            if (strcmp(cur, ancestor) == 0) return 1;
            parent = "BaseException";
        }
        cur = parent;
    }
    return 0;
}

/* -- universal seam -- */
typedef struct cr_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cr_io_t;

/* -- truthiness / accessors -- */
static int cr_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cr_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cr_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cr_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cr_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cr_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cr_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cr_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cr_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cr_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static const char *cr_typename(const json_t *v)
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

/* -- raise specs -- */
static int cr_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cr_kind(const json_t *r)
{
    const char *k = cr_str(cr_get(r, "__raise__"));
    return k ? k : "";
}

static json_t *cr_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *cr_xfmt(const char *fmt, ...)
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

static json_t *cr_attr_raise(const json_t *v, const char *name)
{
    char *m = cr_xfmt("'%s' object has no attribute '%s'", cr_typename(v), name);
    json_t *r = cr_mkraise("AttributeError", m ? m : "");
    free(m);
    return r;
}

/* -- universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. -- */
static json_t *cr_call(cr_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cr_xfmt("no seam installed for %s", name);
        r = cr_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cr_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides (harness strips nulls
 * recursively; oracle norm_args drops None). */
static void cr_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void cr_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cr_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* -- attribute doubles -- */
static const json_t *cr_getattr(const json_t *v, const char *name)
{
    if (!cr_is_obj(v)) return NULL;
    const json_t *inner = json_obj_get(v, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) return NULL;
    return json_obj_get(inner, name);
}

/* setattr on an attribute double (returns 0 when the carrier is not one) */
static int cr_setattr(json_t *v, const char *name, json_t *val)
{
    if (!cr_is_obj(v)) { json_free(val); return 0; }
    json_t *inner = json_obj_get(v, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) { json_free(val); return 0; }
    json_set(inner, name, val);
    return 1;
}

/* -- string helpers (ASCII domain) -- */
static char *cr_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int cr_ends_with(const char *s, const char *suf)
{
    if (!s || !suf) return 0;
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && strcmp(s + n - m, suf) == 0;
}

/* str(v) for the f-string positions that can carry a scripted null/number:
 * None -> "None", integral number -> int repr, str -> itself. */
static char *cr_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cr_strdup("None");
    if (v->type == JSON_STRING) return cr_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return cr_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18)
            return cr_xfmt("%lld", (long long)d);
        return cr_xfmt("%g", d);
    }
    return cr_strdup("");
}

/* deep structural equality (publish owner !=; JSON scalars/containers) */
static int cr_json_eq(const json_t *a, const json_t *b)
{
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!cr_json_eq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !cr_json_eq(a->c.items[i], bv)) return 0;
            }
            return 1;
        default: return 1;
    }
}

/* -- POSIX path helpers (PurePosixPath model; clean absolute paths) -- */
/* split into components; drops empty runs (PurePosixPath collapses '//'),
 * keeps '.' / '..' literally (outside the fixture domain). Caller frees. */
static char **cr_path_parts(const char *path, size_t *n_out)
{
    *n_out = 0;
    if (!path) return NULL;
    size_t cap = 8, n = 0;
    char **parts = (char **)malloc(cap * sizeof(char *));
    if (!parts) return NULL;
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        if (n == cap) {
            cap *= 2;
            char **np = (char **)realloc(parts, cap * sizeof(char *));
            if (!np) break;
            parts = np;
        }
        size_t len = (size_t)(p - start);
        char *seg = (char *)malloc(len + 1);
        if (seg) { memcpy(seg, start, len); seg[len] = '\0'; parts[n++] = seg; }
    }
    *n_out = n;
    return parts;
}

static void cr_parts_free(char **parts, size_t n)
{
    if (!parts) return;
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
}

/* PurePosixPath(base) / name -- absolute name replaces; root "/" joins clean */
static char *cr_path_join(const char *base, const char *name)
{
    if (!base) base = "";
    if (!name) name = "";
    if (name[0] == '/') return cr_strdup(name);
    size_t bn = strlen(base);
    while (bn > 1 && base[bn - 1] == '/') bn--;
    if (bn == 1 && base[0] == '/') return cr_xfmt("/%s", name);
    if (bn == 0) return cr_strdup(name);
    return cr_xfmt("%.*s/%s", (int)bn, base, name);
}

/* path.relative_to(root) succeeds iff root's components prefix path's */
static int cr_is_under_str(const char *path, const char *root)
{
    size_t np = 0, nr = 0;
    char **pp = cr_path_parts(path, &np);
    char **rp = cr_path_parts(root, &nr);
    int ok = nr <= np;
    if (ok) {
        for (size_t i = 0; i < nr; i++) {
            if (strcmp(pp[i], rp[i]) != 0) { ok = 0; break; }
        }
    }
    cr_parts_free(pp, np);
    cr_parts_free(rp, nr);
    return ok;
}

/* PurePosixPath.suffix: last dot in the final component (not a leading dot) */
static char *cr_path_suffix(const char *path)
{
    size_t n = 0;
    char **parts = cr_path_parts(path, &n);
    const char *last = n ? parts[n - 1] : "";
    const char *dot = strrchr(last, '.');
    char *suf = cr_strdup((dot && dot != last) ? dot : "");
    cr_parts_free(parts, n);
    return suf;
}

/* ASCII lower (str.lower on the ASCII suffix domain) */
static void cr_lower_ascii(char *s)
{
    for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Python list-of-str repr for the TimeoutExpired message: ['git', 'diff'] */
static char *cr_cmd_repr(const json_t *cmd)
{
    size_t cap = 64, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[n++] = '[';
    if (cr_is_arr(cmd)) {
        for (size_t i = 0; i < cmd->c.count; i++) {
            const char *s = cr_str(json_get(cmd, i));
            if (!s) s = "";
            if (i) { buf[n++] = ','; buf[n++] = ' '; }
            buf[n++] = '\'';
            for (const char *p = s; *p; p++) {
                if (n + 4 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
                if (*p == '\\' || *p == '\'') buf[n++] = '\\';
                buf[n++] = *p;
            }
            buf[n++] = '\'';
        }
    }
    if (n + 2 >= cap) { cap += 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
    buf[n++] = ']';
    buf[n] = '\0';
    return buf;
}

/* Python str repr for the int() ValueError message (ASCII domain): prefer
 * single quotes; switch to double when the text has ' but no "; escape the
 * quote char, backslash, and control chars (\t \n \r named, others \xNN). */
static char *cr_repr_str(const char *s)
{
    if (!s) s = "";
    char quote = '\'';
    int has_sq = 0, has_dq = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\'') has_sq = 1;
        if (*p == '"') has_dq = 1;
    }
    if (has_sq && !has_dq) quote = '"';
    size_t cap = strlen(s) * 4 + 3, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[n++] = quote;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)quote || c == '\\') { buf[n++] = '\\'; buf[n++] = (char)c; }
        else if (c == '\t') { buf[n++] = '\\'; buf[n++] = 't'; }
        else if (c == '\n') { buf[n++] = '\\'; buf[n++] = 'n'; }
        else if (c == '\r') { buf[n++] = '\\'; buf[n++] = 'r'; }
        else if (c < 0x20 || c == 0x7f) { n += (size_t)sprintf(buf + n, "\\x%02x", c); }
        else buf[n++] = (char)c;
    }
    buf[n++] = quote;
    buf[n] = '\0';
    return buf;
}

/* int() coercion for the git lambda: ASCII sign/digits, underscores between
 * digits, int-strip ws set (White_Space minus \x1c-\x1f). Saturates at 10
 * (the min() clamp) but validates the WHOLE grammar. Returns 1 on success. */
static int cr_int_strip_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int cr_parse_int_clamped(const char *s, long long *out)
{
    if (!s) return 0;
    const char *p = s;
    while (cr_int_strip_ws((unsigned char)*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!isdigit((unsigned char)*p)) return 0;
    long long v = 0;
    int prev_digit = 0;
    for (; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (isdigit(c)) {
            if (v < 10) {
                v = v * 10 + (c - '0');
                if (v > 10) v = 10;
            }
            prev_digit = 1;
        } else if (c == '_' && prev_digit && isdigit((unsigned char)p[1])) {
            prev_digit = 0;
        } else {
            break;
        }
    }
    while (cr_int_strip_ws((unsigned char)*p)) p++;
    if (*p) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* -- composed ported siblings (value-or-raise; NULL ret == None) -- */

/* PoP: cr_is_under @ agent/context_references.py:_is_under */
static json_t *crh_is_under(const json_t *path, const json_t *root)
{
    const char *p = cr_str(path);
    const char *r = cr_str(root);
    if (!p || !r) {
        /* relative_to on a non-path carrier: outside the documented domain;
         * fail closed with the live AttributeError shape */
        return cr_attr_raise(!p ? path : root, "relative_to");
    }
    return json_bool(cr_is_under_str(p, r) ? true : false);
}

/* PoP: cr_on_disk_reference_block @ agent/context_references.py:_on_disk_reference_block */
static json_t *crh_on_disk_reference_block(cr_io_t *io, const json_t *ref, const char *path,
                                           const char *descriptor, const char *reason, const char *guidance)
{
    char *size = NULL;
    json_t *a = cr_args();
    cr_arg_s(a, "path", path);
    json_t *st = cr_call(io, "stat", a);
    if (cr_is_raise(st)) {
        if (cr_exc_is(cr_kind(st), "OSError")) {
            json_free(st);
            size = cr_strdup("unknown size");
        } else {
            return st;
        }
    } else {
        const json_t *sz = cr_getattr(st, "st_size");
        if (!sz) {
            json_t *r = cr_attr_raise(st, "st_size");
            json_free(st);
            return r;
        }
        json_t *fa = cr_args();
        cr_arg_j(fa, "size", sz);
        json_t *fb = cr_call(io, "format_bytes", fa);
        json_free(st);
        if (cr_is_raise(fb)) {
            if (cr_exc_is(cr_kind(fb), "OSError")) {
                json_free(fb);
                size = cr_strdup("unknown size");
            } else {
                return fb;
            }
        } else {
            size = cr_pystr(fb);
            json_free(fb);
        }
    }
    json_t *va = cr_args();
    cr_arg_s(va, "path", path);
    json_t *av = cr_call(io, "_agent_visible_path", va);
    if (cr_is_raise(av)) { free(size); return av; }
    char *avs = cr_pystr(av);
    json_free(av);
    const char *raw = cr_str(cr_getattr(ref, "raw"));
    char *out = cr_xfmt(CR_EMOJI_CLIP " %s (%s, %s) " CR_EM_DASH " %s "
                        "It is available on disk at `%s`. %s",
                        raw ? raw : "", descriptor ? descriptor : "", size ? size : "",
                        reason ? reason : "", avs ? avs : "", guidance ? guidance : "");
    free(size);
    free(avs);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* PoP: cr_oversized_text_reference_block @ agent/context_references.py:_oversized_text_reference_block */
static json_t *crh_oversized_text_reference_block(cr_io_t *io, const json_t *ref, const char *path,
                                                  long long text_tokens)
{
    char *desc = cr_xfmt("text file, approximately %lld tokens", text_tokens);
    json_t *r = crh_on_disk_reference_block(
        io, ref, path, desc ? desc : "",
        "too large to inline safely.",
        "Use read_file with a narrow line range, search_files, or terminal/code tools "
        "to inspect only the relevant parts; do not load the entire file into context.");
    free(desc);
    return r;
}

/* PoP: cr_next_line @ agent/context_references.py:_next_line */
static json_t *crh_next_line(cr_io_t *io, int collect, const json_t *remaining, long long line_cap, int has_cap)
{
    json_t *pieces = json_array();
    int seen = 0;
    long long collected = 0;
    json_t *ret = NULL;
    for (;;) {
        json_t *a = cr_args();
        if (has_cap) cr_arg_n(a, "cap", (double)line_cap);
        json_t *piece = cr_call(io, "readline", a);
        if (cr_is_raise(piece)) { json_free(pieces); return piece; }
        if (!cr_truth(piece)) { json_free(piece); break; }
        seen = 1;
        if (collect) {
            const char *ps = cr_str(piece);
            if (!ps) {
                char *m = cr_xfmt("object of type '%s' has no len()", cr_typename(piece));
                json_t *r = cr_mkraise("TypeError", m ? m : "");
                free(m);
                json_free(piece);
                json_free(pieces);
                return r;
            }
            json_append(pieces, json_copy(piece));
            collected += (long long)strlen(ps);
            if (!cr_is_null(remaining) && collected > (long long)cr_num(remaining)) {
                json_free(piece);
                break;
            }
        }
        const char *ps = cr_str(piece);
        int ends_nl = ps ? cr_ends_with(ps, "\n") : 0;
        if (!ps) {
            json_t *r = cr_attr_raise(piece, "endswith");
            json_free(piece);
            json_free(pieces);
            return r;
        }
        json_free(piece);
        if (ends_nl) break;
    }
    if (!seen) {
        ret = json_null();
    } else if (collect) {
        size_t total = 0;
        for (size_t i = 0; i < json_len(pieces); i++) total += strlen(cr_str(json_get(pieces, i)));
        char *buf = (char *)malloc(total + 1);
        if (buf) {
            size_t n = 0;
            for (size_t i = 0; i < json_len(pieces); i++) {
                const char *s = cr_str(json_get(pieces, i));
                size_t l = strlen(s);
                memcpy(buf + n, s, l);
                n += l;
            }
            buf[n] = '\0';
            ret = json_string(buf);
            free(buf);
        } else {
            ret = json_string("");
        }
    } else {
        ret = json_string("");
    }
    json_free(pieces);
    return ret;
}

/* PoP: cr_read_file_reference @ agent/context_references.py:_read_file_reference */
static json_t *crh_read_file_reference(cr_io_t *io, const json_t *ref, const char *path,
                                       const json_t *max_inline_tokens)
{
    json_t *a = cr_args();
    cr_arg_s(a, "path", path);
    json_t *bin = cr_call(io, "_is_binary_file", a);
    if (cr_is_raise(bin)) return bin;
    if (cr_truth(bin)) {
        json_free(bin);
        json_t *ba = cr_args();
        cr_arg_j(ba, "ref", ref);
        cr_arg_s(ba, "path", path);
        json_t *blk = cr_call(io, "_binary_reference_block", ba);
        if (cr_is_raise(blk)) return blk;
        json_t *early = json_array();
        json_append(early, json_null());
        json_append(early, blk);
        json_t *ret = json_array();
        json_append(ret, early);
        json_append(ret, json_string(""));
        return ret;
    }
    json_free(bin);

    const json_t *ls = cr_getattr(ref, "line_start");
    if (!ls) return cr_attr_raise(ref, "line_start");
    if (!cr_is_null(ls)) {
        long long line_start = (long long)cr_num(ls);
        int has_budget = !cr_is_null(max_inline_tokens);
        long long char_budget = has_budget ? (long long)cr_num(max_inline_tokens) * CR_CHARS_PER_TOKEN : 0;
        int has_cap = has_budget;
        long long line_cap = has_cap ? char_budget + 1 : 0;

        json_t *oa = cr_args();
        cr_arg_s(oa, "path", path);
        cr_arg_s(oa, "encoding", "utf-8");
        json_t *fh = cr_call(io, "open", oa);
        if (cr_is_raise(fh)) return fh;
        json_free(fh);

        json_t *ret = NULL;   /* value-or-raise outcome inside the with */
        json_t *parts = json_array();
        long long total_chars = 0;
        long long skip = line_start - 1;
        if (skip < 0) skip = 0;
        for (long long i = 0; i < skip && !ret; i++) {
            json_t *line = crh_next_line(io, 0, NULL, line_cap, has_cap);
            if (cr_is_raise(line)) { ret = line; break; }
            int is_none = cr_is_null(line);
            json_free(line);
            if (is_none) break;
        }
        if (!ret) {
            const json_t *le = cr_getattr(ref, "line_end");
            if (!le) { ret = cr_attr_raise(ref, "line_end"); }
            else {
                long long line_end = cr_truth(le) ? (long long)cr_num(le) : line_start;
                long long count = line_end - line_start + 1;
                for (long long i = 0; i < count && !ret; i++) {
                    json_t *rem = NULL;
                    if (has_budget) rem = json_number((double)(char_budget - total_chars));
                    json_t *line = crh_next_line(io, 1, rem, line_cap, has_cap);
                    json_free(rem);
                    if (cr_is_raise(line)) { ret = line; break; }
                    if (cr_is_null(line)) { json_free(line); break; }
                    const char *s = cr_str(line);
                    total_chars += (long long)(s ? strlen(s) : 0);
                    if (has_budget && total_chars > char_budget) {
                        json_free(line);
                        json_t *blk = crh_oversized_text_reference_block(io, ref, path, total_chars / CR_CHARS_PER_TOKEN);
                        if (cr_is_raise(blk)) { ret = blk; break; }
                        json_t *early = json_array();
                        json_append(early, json_null());
                        json_append(early, blk);
                        ret = json_array();
                        json_append(ret, early);
                        json_append(ret, json_string(""));
                        break;
                    }
                    json_append(parts, line);
                }
            }
        }
        if (!ret) {
            size_t total = 0;
            for (size_t i = 0; i < json_len(parts); i++) total += strlen(cr_str(json_get(parts, i)));
            char *buf = (char *)malloc(total + 1);
            size_t n = 0;
            if (buf) {
                for (size_t i = 0; i < json_len(parts); i++) {
                    const char *s = cr_str(json_get(parts, i));
                    size_t l = strlen(s);
                    memcpy(buf + n, s, l);
                    n += l;
                }
                buf[n] = '\0';
            }
            ret = json_array();
            json_append(ret, json_null());
            json_append(ret, json_string(buf ? buf : ""));
            free(buf);
        }
        json_free(parts);
        /* with-exit: fires on every path; an exit raise replaces the outcome */
        json_t *ex = cr_call(io, "open.__exit__", cr_args());
        if (cr_is_raise(ex)) { json_free(ret); return ex; }
        json_free(ex);
        return ret;
    }

    json_t *sa = cr_args();
    cr_arg_s(sa, "path", path);
    json_t *st = cr_call(io, "stat", sa);
    if (cr_is_raise(st)) return st;
    const json_t *sz = cr_getattr(st, "st_size");
    if (!sz) {
        json_t *r = cr_attr_raise(st, "st_size");
        json_free(st);
        return r;
    }
    long long size = (long long)cr_num(sz);
    json_free(st);
    if (!cr_is_null(max_inline_tokens) && size > (long long)cr_num(max_inline_tokens) * CR_CHARS_PER_TOKEN) {
        json_t *blk = crh_oversized_text_reference_block(io, ref, path, size / CR_CHARS_PER_TOKEN);
        if (cr_is_raise(blk)) return blk;
        json_t *early = json_array();
        json_append(early, json_null());
        json_append(early, blk);
        json_t *ret = json_array();
        json_append(ret, early);
        json_append(ret, json_string(""));
        return ret;
    }
    json_t *ra = cr_args();
    cr_arg_s(ra, "path", path);
    cr_arg_s(ra, "encoding", "utf-8-sig");
    json_t *text = cr_call(io, "read_text", ra);
    if (cr_is_raise(text)) return text;
    json_t *ret = json_array();
    json_append(ret, json_null());
    json_append(ret, text);
    return ret;
}

/* PoP: cr_format_file_reference @ agent/context_references.py:_format_file_reference */
static json_t *crh_format_file_reference_text(cr_io_t *io, const json_t *ref, const char *path,
                                              const json_t *text, const json_t *max_inline_tokens)
{
    char *suf = cr_path_suffix(path);
    cr_lower_ascii(suf);
    const char *lang = "";
    for (size_t i = 0; i < sizeof(CR_FENCE_LANGUAGES) / sizeof(CR_FENCE_LANGUAGES[0]); i++) {
        if (strcmp(CR_FENCE_LANGUAGES[i].ext, suf) == 0) { lang = CR_FENCE_LANGUAGES[i].lang; break; }
    }
    free(suf);
    json_t *a = cr_args();
    cr_arg_j(a, "text", text);
    json_t *tok = cr_call(io, "estimate_tokens_rough", a);
    if (cr_is_raise(tok)) return tok;
    long long text_tokens = (long long)cr_num(tok);
    json_free(tok);
    if (!cr_is_null(max_inline_tokens) && text_tokens > (long long)cr_num(max_inline_tokens)) {
        json_t *blk = crh_oversized_text_reference_block(io, ref, path, text_tokens);
        if (cr_is_raise(blk)) return blk;
        json_t *ret = json_array();
        json_append(ret, json_null());
        json_append(ret, blk);
        return ret;
    }
    const char *raw = cr_str(cr_getattr(ref, "raw"));
    const char *txt = cr_str(text);
    char *block = cr_xfmt(CR_EMOJI_FILE " %s (%lld tokens)\n```%s\n%s\n```",
                          raw ? raw : "", text_tokens, lang, txt ? txt : "");
    json_t *ret = json_array();
    json_append(ret, json_null());
    json_append(ret, json_string(block ? block : ""));
    free(block);
    return ret;
}

/* PoP: cr_drain @ agent/context_references.py:_drain */
static json_t *crh_drain(cr_io_t *io, const char *stream, json_t *sink, int *truncated)
{
    long long total = 0;
    char *ev = cr_xfmt("read1_%s", stream);
    for (;;) {
        json_t *a = cr_args();
        cr_arg_n(a, "size", (double)CR_READ1_SIZE);
        json_t *chunk = cr_call(io, ev ? ev : "read1", a);
        if (cr_is_raise(chunk)) { free(ev); return chunk; }
        if (!cr_truth(chunk)) { json_free(chunk); break; }
        const char *s = cr_str(chunk);
        long long len = (long long)(s ? strlen(s) : 0);
        total += len;
        if (total <= CR_MAX_QUIET_OUTPUT_BYTES) {
            json_append(sink, chunk);
        } else {
            json_free(chunk);
            *truncated = 1;
            json_t *k = cr_call(io, "kill", cr_args());
            if (cr_is_raise(k)) { free(ev); return k; }
            json_free(k);
            break;
        }
    }
    free(ev);
    return json_null();
}

/* -- entries -- */

/* PoP: cr_post_init @ agent/context_references.py:__post_init__ */
static json_t *cr_post_init(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in;
    json_t *self = cr_get(st, "self");
    const json_t *display = cr_getattr(self, "display");
    if (!display) return cr_attr_raise(self, "display");
    /* self.display = self.display or self.text  (text read ONLY when falsy) */
    json_t *val;
    if (cr_truth(display)) {
        val = json_copy(display);
    } else {
        const json_t *text = cr_getattr(self, "text");
        if (!text) return cr_attr_raise(self, "text");
        val = json_copy(text);
    }
    cr_setattr(self, "display", val);
    return json_null();
}

/* PoP: cr_autocomplete @ agent/context_references.py:autocomplete */
static json_t *cr_autocomplete(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in; (void)st;
    return json_null();   /* abstract body: docstring only */
}

/* PoP: cr_expand @ agent/context_references.py:expand */
static json_t *cr_expand(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in; (void)st;
    return json_null();   /* abstract body: docstring only */
}

/* PoP: cr_publish @ agent/context_references.py:publish */
static json_t *cr_publish(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const char *prefix = json_get_str(in, "prefix", "");
    json_t *providers = cr_get(st, "providers");
    json_t *owners = cr_get(st, "owners");
    if (!cr_is_obj(providers)) { providers = json_object(); json_set(st, "providers", providers); }
    if (!cr_is_obj(owners)) { owners = json_object(); json_set(st, "owners", owners); }
    const json_t *held = cr_get(in, "held");
    const json_t *provider = cr_get(in, "provider");
    const json_t *owner = cr_get(in, "owner");
    /* providers.get(prefix) is not held  (identity via token) */
    const json_t *cur = json_obj_get(providers, prefix);
    int same = 0;
    if (cur && held) {
        const json_t *ct = cr_getattr(cur, "token");
        const json_t *ht = cr_getattr(held, "token");
        same = ct && ht && cr_json_eq(ct, ht);
    }
    const json_t *cur_owner = json_obj_get(owners, prefix);
    int owner_ne = !cr_json_eq(cur_owner, owner);
    if (!same || owner_ne) return json_bool(false);
    json_set(providers, prefix, provider ? json_copy(provider) : json_null());
    return json_bool(true);
}

/* PoP: cr_git_args_diff @ agent/context_references.py:_GIT_REFERENCE_ARGS */
static json_t *cr_git_args_diff(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in; (void)st;
    json_t *r = json_array();
    json_append(r, json_string("diff"));
    return r;
}

/* PoP: cr_git_args_staged @ agent/context_references.py:_GIT_REFERENCE_ARGS */
static json_t *cr_git_args_staged(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)in; (void)st;
    json_t *r = json_array();
    json_append(r, json_string("diff"));
    json_append(r, json_string("--staged"));
    return r;
}

/* PoP: cr_git_args_git @ agent/context_references.py:_GIT_REFERENCE_ARGS */
static json_t *cr_git_args_git(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *ref = cr_get(in, "ref");
    const json_t *target = cr_getattr(ref, "target");
    if (!target) return cr_attr_raise(ref, "target");
    /* int(ref.target or '1') */
    long long v = 1;
    if (cr_truth(target)) {
        if (cr_is_num(target)) {
            double d = cr_num(target);
            v = (long long)d;   /* int() truncates toward zero */
        } else {
            const char *s = cr_str(target);
            if (!s) {
                char *m = cr_xfmt("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                                  cr_typename(target));
                return cr_mkraise("TypeError", m ? m : "");
            }
            if (!cr_parse_int_clamped(s, &v)) {
                char *rs = cr_repr_str(s);
                char *m = cr_xfmt("invalid literal for int() with base 10: %s", rs ? rs : "''");
                free(rs);
                return cr_mkraise("ValueError", m ? m : "");
            }
        }
    }
    if (v < 1) v = 1;
    if (v > 10) v = 10;
    char *flag = cr_xfmt("-%lld", v);
    json_t *r = json_array();
    json_append(r, json_string("log"));
    json_append(r, json_string(flag ? flag : "-1"));
    json_append(r, json_string("-p"));
    free(flag);
    return r;
}

/* PoP: cr_expand_path_reference @ agent/context_references.py:_expand_path_reference */
static json_t *cr_expand_path_reference(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *ref = cr_get(in, "ref");
    const char *cwd = json_get_str(in, "cwd", "");
    const json_t *allowed_root = cr_get(in, "allowed_root");
    const json_t *mit = cr_get(in, "max_inline_tokens");
    const char *kind = cr_str(cr_getattr(ref, "kind"));
    const char *raw = cr_str(cr_getattr(ref, "raw"));
    const char *target = cr_str(cr_getattr(ref, "target"));
    if (!kind) return cr_attr_raise(ref, "kind");
    if (!raw) return cr_attr_raise(ref, "raw");
    int is_folder = strcmp(kind, "folder") == 0;

    json_t *a = cr_args();
    cr_arg_s(a, "cwd", cwd);
    cr_arg_s(a, "target", target);
    cr_arg_j(a, "allowed_root", allowed_root);
    json_t *pathj = cr_call(io, "_resolve_path", a);
    if (cr_is_raise(pathj)) return pathj;
    const char *path = cr_str(pathj);
    if (!path) { json_free(pathj); return cr_mkraise("RuntimeError", "path result outside domain"); }

    json_t *ea = cr_args();
    cr_arg_s(ea, "path", path);
    json_t *ensure = cr_call(io, "_ensure_reference_path_allowed", ea);
    if (cr_is_raise(ensure)) { json_free(pathj); return ensure; }
    json_free(ensure);

    json_t *xa = cr_args();
    cr_arg_s(xa, "path", path);
    json_t *ex = cr_call(io, "exists", xa);
    if (cr_is_raise(ex)) { json_free(pathj); return ex; }
    int exists = cr_truth(ex);
    json_free(ex);
    if (!exists) {
        char *m = cr_xfmt("%s: %s not found", raw, kind);
        json_t *ret = json_array();
        json_append(ret, json_string(m ? m : ""));
        json_append(ret, json_null());
        free(m);
        json_free(pathj);
        return ret;
    }

    json_t *ta = cr_args();
    cr_arg_s(ta, "path", path);
    json_t *ty = cr_call(io, is_folder ? "is_dir" : "is_file", ta);
    if (cr_is_raise(ty)) { json_free(pathj); return ty; }
    int is_type = cr_truth(ty);
    json_free(ty);
    if (!is_type) {
        char *m = cr_xfmt("%s: path is not a %s", raw, kind);
        json_t *ret = json_array();
        json_append(ret, json_string(m ? m : ""));
        json_append(ret, json_null());
        free(m);
        json_free(pathj);
        return ret;
    }

    if (is_folder) {
        json_t *la = cr_args();
        cr_arg_s(la, "path", path);
        cr_arg_s(la, "cwd", cwd);
        cr_arg_j(la, "display_base", allowed_root);
        json_t *listing = cr_call(io, "_build_folder_listing", la);
        if (cr_is_raise(listing)) { json_free(pathj); return listing; }
        json_t *tk = cr_args();
        cr_arg_j(tk, "text", listing);
        json_t *tok = cr_call(io, "estimate_tokens_rough", tk);
        if (cr_is_raise(tok)) { json_free(listing); json_free(pathj); return tok; }
        const char *ls = cr_str(listing);
        char *block = cr_xfmt(CR_EMOJI_FOLDER " %s (%lld tokens)\n%s",
                              raw, (long long)cr_num(tok), ls ? ls : "");
        json_free(tok);
        json_free(listing);
        json_t *ret = json_array();
        json_append(ret, json_null());
        json_append(ret, json_string(block ? block : ""));
        free(block);
        json_free(pathj);
        return ret;
    }

    /* file branch: with offline_file_access(path, what=...): read */
    json_t *ca = cr_args();
    cr_arg_s(ca, "path", path);
    cr_arg_s(ca, "what", "preview context reference");
    json_t *cm = cr_call(io, "offline_file_access", ca);
    if (cr_is_raise(cm)) {
        /* CM factory raise: no __exit__ (never entered); LCE still caught */
        if (cr_exc_is(cr_kind(cm), "LiveConnectionError")) {
            json_free(cm);
            json_t *blk = crh_on_disk_reference_block(
                io, ref, path, "live SQLite database file",
                "not previewed: raw access would cancel SQLite's POSIX locks.",
                "Do not open this file directly while its database connection is live.");
            json_free(pathj);
            if (cr_is_raise(blk)) return blk;
            json_t *ret = json_array();
            json_append(ret, json_null());
            json_append(ret, blk);
            return ret;
        }
        json_free(pathj);
        return cm;
    }
    json_free(cm);

    json_t *body = crh_read_file_reference(io, ref, path, mit);
    /* __exit__ fires on every path; an exit raise replaces the outcome */
    json_t *exit_r = cr_call(io, "offline_file_access.__exit__", cr_args());
    if (cr_is_raise(exit_r)) { json_free(body); body = exit_r; }
    else json_free(exit_r);

    if (cr_is_raise(body)) {
        if (cr_exc_is(cr_kind(body), "LiveConnectionError")) {
            json_free(body);
            json_t *blk = crh_on_disk_reference_block(
                io, ref, path, "live SQLite database file",
                "not previewed: raw access would cancel SQLite's POSIX locks.",
                "Do not open this file directly while its database connection is live.");
            json_free(pathj);
            if (cr_is_raise(blk)) return blk;
            json_t *ret = json_array();
            json_append(ret, json_null());
            json_append(ret, blk);
            return ret;
        }
        json_free(pathj);
        return body;
    }

    /* return early or _format_file_reference(ref, path, text, mit) */
    json_t *early = json_get(body, 0);
    json_t *text = json_get(body, 1);
    json_t *ret;
    if (!cr_is_null(early)) {
        ret = json_copy(early);
    } else {
        ret = crh_format_file_reference_text(io, ref, path, text, mit);
    }
    json_free(body);
    json_free(pathj);
    return ret;
}

/* PoP: cr_read_file_reference_entry @ agent/context_references.py:_read_file_reference */
static json_t *cr_read_file_reference(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *ref = cr_get(in, "ref");
    const char *path = json_get_str(in, "path", "");
    const json_t *mit = cr_get(in, "max_inline_tokens");
    return crh_read_file_reference(io, ref, path, mit);
}

/* PoP: cr_next_line_entry @ agent/context_references.py:_next_line */
static json_t *cr_next_line(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    int collect = cr_truth(cr_get(in, "collect"));
    const json_t *remaining = cr_get(in, "remaining");
    const json_t *lc = cr_get(in, "line_cap");
    int has_cap = !cr_is_null(lc);
    return crh_next_line(io, collect, remaining, has_cap ? (long long)cr_num(lc) : 0, has_cap);
}

/* PoP: cr_format_file_reference_entry @ agent/context_references.py:_format_file_reference */
static json_t *cr_format_file_reference(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *ref = cr_get(in, "ref");
    const char *path = json_get_str(in, "path", "");
    const json_t *text = cr_get(in, "text");
    const json_t *mit = cr_get(in, "max_inline_tokens");
    return crh_format_file_reference_text(io, ref, path, text, mit);
}

/* PoP: cr_run_quiet @ agent/context_references.py:_run_quiet */
static json_t *cr_run_quiet(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *cmd = cr_get(in, "cmd");
    const char *cwd = json_get_str(in, "cwd", "");
    const json_t *timeout = cr_get(in, "timeout");
    const json_t *env = cr_get(in, "env");
    int is_windows = cr_truth(cr_get(in, "is_windows"));

    json_t *creationflags = NULL;
    if (is_windows) {
        json_t *hf = cr_call(io, "windows_hide_flags", cr_args());
        if (cr_is_raise(hf)) return hf;
        creationflags = hf;
    }
    json_t *pa = cr_args();
    cr_arg_j(pa, "cmd", cmd);
    cr_arg_s(pa, "cwd", cwd);
    cr_arg_n(pa, "stdin", -3);   /* subprocess.DEVNULL */
    cr_arg_n(pa, "stdout", -1);  /* subprocess.PIPE */
    cr_arg_n(pa, "stderr", -1);
    cr_arg_j(pa, "creationflags", creationflags);
    cr_arg_j(pa, "env", env);
    json_free(creationflags);
    json_t *proc = cr_call(io, "popen", pa);
    if (cr_is_raise(proc)) return proc;

    int truncated = 0;
    json_t *sink_out = json_array();
    json_t *sink_err = json_array();
    json_t *d = crh_drain(io, "stdout", sink_out, &truncated);
    if (cr_is_raise(d)) { json_free(proc); json_free(sink_out); json_free(sink_err); return d; }
    json_free(d);
    d = crh_drain(io, "stderr", sink_err, &truncated);
    if (cr_is_raise(d)) { json_free(proc); json_free(sink_out); json_free(sink_err); return d; }
    json_free(d);

    json_t *wa = cr_args();
    cr_arg_j(wa, "timeout", timeout);
    json_t *w = cr_call(io, "wait", wa);
    if (cr_is_raise(w)) {
        if (cr_exc_is(cr_kind(w), "TimeoutExpired")) {
            json_free(w);
            json_t *k = cr_call(io, "kill", cr_args());
            if (cr_is_raise(k)) { json_free(proc); json_free(sink_out); json_free(sink_err); return k; }
            json_free(k);
            json_t *w2 = cr_call(io, "wait", cr_args());
            if (cr_is_raise(w2)) { json_free(proc); json_free(sink_out); json_free(sink_err); return w2; }
            json_free(w2);
            char *crepr = cr_cmd_repr(cmd);
            char *m = cr_xfmt("Command '%s' timed out after %lld seconds",
                              crepr ? crepr : "", (long long)cr_num(timeout));
            free(crepr);
            json_t *r = cr_mkraise("TimeoutExpired", m ? m : "");
            free(m);
            json_free(proc);
            json_free(sink_out);
            json_free(sink_err);
            return r;
        }
        json_free(proc);
        json_free(sink_out);
        json_free(sink_err);
        return w;
    }
    json_free(w);

    /* join + decode (ASCII identity) */
    size_t no = 0, ne = 0;
    for (size_t i = 0; i < json_len(sink_out); i++) no += strlen(cr_str(json_get(sink_out, i)));
    for (size_t i = 0; i < json_len(sink_err); i++) ne += strlen(cr_str(json_get(sink_err, i)));
    char *out = (char *)malloc(no + 1);
    char *err = (char *)malloc(ne + 1);
    size_t n = 0;
    if (out) { for (size_t i = 0; i < json_len(sink_out); i++) { const char *s = cr_str(json_get(sink_out, i)); size_t l = strlen(s); memcpy(out + n, s, l); n += l; } out[n] = '\0'; }
    n = 0;
    if (err) { for (size_t i = 0; i < json_len(sink_err); i++) { const char *s = cr_str(json_get(sink_err, i)); size_t l = strlen(s); memcpy(err + n, s, l); n += l; } err[n] = '\0'; }
    json_free(sink_out);
    json_free(sink_err);

    const json_t *rc = cr_getattr(proc, "returncode");
    if (!rc) {
        json_t *r = cr_attr_raise(proc, "returncode");
        json_free(proc);
        free(out);
        free(err);
        return r;
    }
    long long returncode = (long long)cr_num(rc);
    json_free(proc);
    if (truncated && returncode == 0) returncode = CR_OUTPUT_CAP_EXCEEDED_RETURNCODE;

    json_t *ret = json_object();
    json_set(ret, "args", cmd ? json_copy(cmd) : json_null());
    json_set(ret, "returncode", json_number((double)returncode));
    json_set(ret, "stdout", json_string(out ? out : ""));
    json_set(ret, "stderr", json_string(err ? err : ""));
    free(out);
    free(err);
    return ret;
}

/* PoP: cr_drain_entry @ agent/context_references.py:_drain */
static json_t *cr_drain(cr_io_t *io, const json_t *in, json_t *st)
{
    const char *stream = json_get_str(in, "stream", "stdout");
    json_t *sink = json_array();
    int truncated = 0;
    json_t *r = crh_drain(io, stream, sink, &truncated);
    if (cr_is_raise(r)) { json_free(sink); return r; }
    json_free(r);
    json_set(st, "sink", sink);
    json_set(st, "truncated", json_bool(truncated ? true : false));
    return json_null();
}

/* PoP: cr_is_under_entry @ agent/context_references.py:_is_under */
static json_t *cr_is_under(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return crh_is_under(cr_get(in, "path"), cr_get(in, "root"));
}

/* PoP: cr_composer_paste_roots @ agent/context_references.py:_composer_paste_roots */
static json_t *cr_composer_paste_roots(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *dirs = cr_call(io, "_hermes_dirs", cr_args());
    if (cr_is_raise(dirs)) return dirs;
    if (!cr_is_arr(dirs)) {
        char *m = cr_xfmt("'%s' object is not iterable", cr_typename(dirs));
        json_t *r = cr_mkraise("TypeError", m ? m : "");
        free(m);
        json_free(dirs);
        return r;
    }
    json_t *ret = json_array();
    if (cr_is_arr(dirs)) {
        for (size_t i = 0; i < dirs->c.count; i++) {
            const char *d = cr_str(json_get(dirs, i));
            char *j = cr_path_join(d ? d : "", CR_COMPOSER_PASTES_DIRNAME);
            json_append(ret, json_string(j ? j : ""));
            free(j);
        }
    }
    json_free(dirs);
    return ret;
}

/* PoP: cr_agent_staged_path @ agent/context_references.py:_agent_staged_path */
static json_t *cr_agent_staged_path(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *path = json_get_str(in, "path", "");
    /* try: mounts = get_cache_directory_mounts(); any(...) except Exception: False */
    json_t *mounts = cr_call(io, "get_cache_directory_mounts", cr_args());
    if (cr_is_raise(mounts)) {
        if (cr_exc_is(cr_kind(mounts), "Exception")) { json_free(mounts); return json_bool(false); }
        return mounts;
    }
    json_t *ret = json_bool(false);
    if (cr_is_arr(mounts)) {
        for (size_t i = 0; i < mounts->c.count; i++) {
            const json_t *entry = json_get(mounts, i);
            const json_t *hp = cr_get(entry, "host_path");
            if (!hp) {
                /* entry["host_path"] KeyError (or TypeError on non-dict) --
                 * both are Exception -> caught -> False */
                json_free(mounts);
                return json_bool(false);
            }
            const char *hps = cr_str(hp);
            if (!hps) { json_free(mounts); return json_bool(false); }
            json_t *ea = cr_args();
            cr_arg_s(ea, "path", hps);
            json_t *exp = cr_call(io, "expanduser", ea);
            if (cr_is_raise(exp)) {
                if (cr_exc_is(cr_kind(exp), "Exception")) { json_free(exp); json_free(mounts); return json_bool(false); }
                json_free(mounts);
                return exp;
            }
            const char *exps = cr_str(exp);
            json_t *ra = cr_args();
            cr_arg_s(ra, "path", exps ? exps : "");
            json_free(exp);
            json_t *res = cr_call(io, "resolve", ra);
            if (cr_is_raise(res)) {
                if (cr_exc_is(cr_kind(res), "Exception")) { json_free(res); json_free(mounts); return json_bool(false); }
                json_free(mounts);
                return res;
            }
            const char *ress = cr_str(res);
            int under = ress ? cr_is_under_str(path, ress) : 0;
            json_free(res);
            if (under) { ret = json_bool(true); break; }
        }
    }
    json_free(mounts);
    return ret;
}

/* PoP: cr_on_disk_reference_block_entry @ agent/context_references.py:_on_disk_reference_block */
static json_t *cr_on_disk_reference_block(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *ref = cr_get(in, "ref");
    const char *path = json_get_str(in, "path", "");
    const char *descriptor = json_get_str(in, "descriptor", "");
    const char *reason = json_get_str(in, "reason", "");
    const char *guidance = json_get_str(in, "guidance", "");
    return crh_on_disk_reference_block(io, ref, path, descriptor, reason, guidance);
}

/* PoP: cr_oversized_text_reference_block_entry @ agent/context_references.py:_oversized_text_reference_block */
static json_t *cr_oversized_text_reference_block(cr_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *ref = cr_get(in, "ref");
    const char *path = json_get_str(in, "path", "");
    long long tt = (long long)cr_num(cr_get(in, "text_tokens"));
    return crh_oversized_text_reference_block(io, ref, path, tt);
}