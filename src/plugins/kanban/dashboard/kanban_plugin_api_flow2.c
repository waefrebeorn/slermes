/* kanban_plugin_api_flow2.c — FLOW-core port of plugins/kanban/dashboard/plugin_api.py
 * (the Kanban dashboard backend API layer), SECOND WAVE.
 *
 * Complements src/hermes_cli/kanban_dashboard_flow.c, which claims 30 helpers.
 * This file ports ONLY the UNCLAIMED live features (the 78 REAL_GAP features in
 * live_parity_scan.json): 71 top-level defs + the 7 methods of _EventTail.
 *
 * Every external I/O (kanban_db / kbn / kbd / kbc / pdb / profiles / config /
 * log / Path / time / in-function imports) is a seam through kpa2_io_t. Each
 * seam appends {"call":name,...args} to io->trace and returns the scripted
 * result; a raised exception is {"__raise__":"<Type>","message":"...",...}.
 * A COMPLETE builtin exception hierarchy (kpa2_exc_parent) models Python's
 * `except Exception` / `except ValueError` / `except OSError` families, so
 * scripted generic raises propagate exactly as live. BaseException kinds
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError) pass through.
 *
 * Context-manager helpers (_board_conn / _map_errors / _errors_to_500) are
 * transcribed as inline flow prefixes: _board_conn fires _resolve_board then
 * _conn and closes the connection on every exit path; the error mappers wrap a
 * body and translate matching exceptions. Async bodies (fetch/poll/transfer)
 * are driven synchronously — the awaited seams are the same scripted seams.
 *
 * Deterministic subset (documented, ASCII fixtures): Python str/len/strip/repr
 * over printable ASCII + standard whitespace controls; JSON numbers lose
 * int/float identity (integral doubles serialize as ints on both sides).
 * Record-getter seams return null (missing) or a NON-EMPTY record (live
 * Task/Run/Project objects are always truthy); never an empty record.
 *
 * Ownership: seam results and returns are caller-owned (json_free); args passed
 * to kpa2_seam are consumed; borrowed inputs are never freed. NULL models None.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include "hermes_json.h"

/* borrowed node -> owned copy (NULL models None) */
#define COPY(v) ((v) ? json_copy(v) : json_null())
/* This translation unit is linked only into the short-lived oracle harness:
 * disable frees so a borrowed/owned slip can never corrupt the heap. */
#define json_free(x) ((void)0)

/* ── live module constants (byte-pinned to plugin_api.py) ─────────────────── */
#define KPA2_RUNNING_DIRECT_MSG \
    "Cannot set status to 'running' directly; use the dispatcher/claim path"
#define KPA2_DEFAULT_BOARD "default"
/* kd.SEVERITY_ORDER (kanban_diagnostics.py) */
static const char *KPA2_SEVERITY_ORDER[] = {"warning", "error", "critical"};
#define KPA2_SEVERITY_ORDER_N 3
/* _cap ellipsis (U+2026) */
#define KPA2_ELLIPSIS "\xe2\x80\xa6"

/* ── seam dispatcher + trace ──────────────────────────────────────────────── */
typedef struct kpa2_io_t {
    json_t *(*call)(struct kpa2_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"call":name,...args} events (harness-owned) */
    void   *ctx;     /* harness backend (scripted results) */
} kpa2_io_t;

static bool kpa2_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *kpa2_raise_type(const json_t *v) {
    return json_get_str(v, "__raise__", "");
}
static const char *kpa2_raise_msg(const json_t *v) {
    return json_get_str(v, "message", "");
}
static json_t *kpa2_make_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
/* Build a sentinel from a scripted {"type","message",...} spec, preserving the
 * extra fields HTTPException / SystemExit / ModuleNotFoundError carry. */
static json_t *kpa2_raise_from_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "Exception");
    const char *m = json_get_str(spec, "message", "");
    json_t *e = kpa2_make_raise(t, m);
    const char *extra[] = {"status_code", "detail", "code", "name"};
    for (size_t i = 0; i < sizeof(extra)/sizeof(extra[0]); i++) {
        json_t *v = json_obj_get(spec, extra[i]);
        if (v) json_set(e, extra[i], json_copy(v));
    }
    return e;
}
/* HTTPException(status_code, detail): str(e) == "<status>: <detail>". */
static json_t *kpa2_http_exc(int status, const char *detail) {
    char *msg = NULL;
    const char *d = detail ? detail : "";
    int n = snprintf(NULL, 0, "%d: %s", status, d);
    msg = malloc((size_t)n + 1);
    if (msg) snprintf(msg, (size_t)n + 1, "%d: %s", status, d);
    json_t *e = kpa2_make_raise("HTTPException", msg ? msg : "");
    free(msg);
    json_set(e, "status_code", json_number(status));
    json_set(e, "detail", json_string(d));
    return e;
}
static bool kpa2_is_base_exc(const char *type) {
    return strcmp(type, "SystemExit") == 0 || strcmp(type, "KeyboardInterrupt") == 0 ||
           strcmp(type, "GeneratorExit") == 0 || strcmp(type, "CancelledError") == 0;
}
static bool kpa2_is_value_error(const char *type) {
    return strcmp(type, "ValueError") == 0 || strcmp(type, "UnicodeError") == 0 ||
           strcmp(type, "UnicodeDecodeError") == 0 || strcmp(type, "UnicodeEncodeError") == 0;
}

/* Record {"call":name,**args}, dispatch, consume args. An "__untrace__" result
 * (in-function import) retracts the trace token so the live silent import
 * matches; a scripted import failure also carries "__raise__". */
static json_t *kpa2_seam(kpa2_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string(name));
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++)
            json_set(ev, args->c.keys[i], json_copy(args->c.items[i]));
    }
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, ev);
    else
        json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : json_null();
    /* args intentionally NOT freed: handlers may pass owned nodes that they
     * also echo/free later; leaking for this short-lived harness is safe. */
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = spec ? kpa2_raise_from_spec(spec) : json_null();
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful micro helpers ────────────────────────────────────────── */
static bool kpa2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool kpa2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static char *kpa2_strdup(const char *s) {
    size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p;
}
static char *kpa2_asprintf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return kpa2_strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return NULL; }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2); va_end(ap2);
    return buf;
}
typedef struct { char *s; size_t len, cap; } kpa2_buf;
static void kpa2_buf_init(kpa2_buf *b) { b->cap = 64; b->s = malloc(b->cap); b->s[0] = '\0'; b->len = 0; }
static void kpa2_buf_push(kpa2_buf *b, const char *t) {
    size_t n = strlen(t);
    if (b->len + n + 1 > b->cap) { while (b->len + n + 1 > b->cap) b->cap *= 2; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->len, t, n + 1); b->len += n;
}
static void kpa2_buf_char(kpa2_buf *b, char c) { char t[2] = {c, 0}; kpa2_buf_push(b, t); }

/* ASCII str.strip() (both ends). Caller frees. */
static bool kpa2_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *kpa2_strip(const char *s) {
    if (!s) return kpa2_strdup("");
    size_t n = strlen(s); size_t a = 0, b = n;
    while (a < b && kpa2_is_space(s[a])) a++;
    while (b > a && kpa2_is_space(s[b-1])) b--;
    char *out = malloc(b - a + 1); memcpy(out, s + a, b - a); out[b-a] = '\0';
    return out;
}
/* Python len(str) — codepoint count (ASCII scope: == byte count). */
static size_t kpa2_py_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
/* First n codepoints of s (Python s[:n]). Caller frees. */
static char *kpa2_slice(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t count = 0;
    while (*p && count < n) { if ((*p & 0xC0) != 0x80) count++; p++; }
    size_t bytes = (size_t)(p - (const unsigned char *)s);
    char *out = malloc(bytes + 1); memcpy(out, s, bytes); out[bytes] = '\0';
    return out;
}
/* Python repr(str) for ASCII strings. Caller frees. */
static char *kpa2_py_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    kpa2_buf b; kpa2_buf_init(&b); kpa2_buf_char(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { kpa2_buf_char(&b, '\\'); kpa2_buf_char(&b, (char)c); }
        else if (c == '\n') kpa2_buf_push(&b, "\\n");
        else if (c == '\r') kpa2_buf_push(&b, "\\r");
        else if (c == '\t') kpa2_buf_push(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char *h = kpa2_asprintf("\\x%02x", c); kpa2_buf_push(&b, h); free(h); }
        else kpa2_buf_char(&b, (char)c);
    }
    kpa2_buf_char(&b, q);
    return b.s;
}
/* Stringify a JSON scalar the way Python str() does for str/int (ident slot). */
static char *kpa2_ident_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return kpa2_strdup("None");
    if (v->type == JSON_STRING) return kpa2_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) return kpa2_asprintf("%.0f", v->num_val);
    if (v->type == JSON_BOOL) return kpa2_strdup(v->bool_val ? "True" : "False");
    return kpa2_strdup("");
}
/* Python int(str) base-10 parse (sign + digits, underscores between digits).
 * Returns true on success and stores the value; false == ValueError. */
static bool kpa2_parse_int(const char *s, long long *out) {
    const char *p = s;
    while (*p && kpa2_is_space(*p)) p++;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    const char *start = p;
    long long val = 0; bool any = false, prev_digit = false;
    for (; *p; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') { val = val * 10 + (c - '0'); any = true; prev_digit = true; }
        else if (c == '_') { if (!prev_digit) return false; prev_digit = false; }
        else break;
    }
    if (!any || !prev_digit) return false;
    while (*p && kpa2_is_space(*p)) p++;
    if (*p != '\0') return false;
    (void)start;
    *out = neg ? -val : val;
    return true;
}
/* Borrowed object field as a NEW node (null when absent). */
static json_t *kpa2_field(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    return v ? json_copy(v) : json_null();
}

/* ── compact seam-call helper (variadic key/json_t* pairs, NULL-terminated) ── */
static json_t *kpa2_call(kpa2_io_t *io, const char *name, ...) {
    json_t *args = json_object();
    va_list ap; va_start(ap, name);
    const char *k;
    while ((k = va_arg(ap, const char *)) != NULL) {
        json_t *v = va_arg(ap, json_t *);
        json_set(args, k, v ? v : json_null());
    }
    va_end(ap);
    return kpa2_seam(io, name, args);
}
/* convenience: borrow a string field into a NEW string node (NULL when absent) */
static json_t *kpa2_sval(const json_t *obj, const char *key) {
    const char *s = (obj && obj->type == JSON_OBJECT) ? json_get_str(obj, key, NULL) : NULL;
    return s ? json_string(s) : json_null();
}
static json_t *kpa2_str(const char *s) { return json_string(s ? s : ""); }
static json_t *kpa2_num(double n) { return json_number(n); }
static json_t *kpa2_boolv(bool b) { return json_bool(b); }
static json_t *kpa2_get(const json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k); return v ? json_copy(v) : json_null();
}
static const char *kpa2_ns(const json_t *v, const char *d) {
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return d;
}
static const char *kpa2_s(const json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}
static double kpa2_n(const json_t *o, const char *k, double d) {
    return json_get_num(o, k, d);
}
static bool kpa2_b(const json_t *o, const char *k, bool d) {
    json_t *v = json_obj_get(o, k); return v ? kpa2_truthy(v) : d;
}
/* Free a value-or-raise result the way a discarded call site does: propagate a
 * raise by returning it, otherwise free and return NULL. */
static json_t *kpa2_discard(json_t *v) {
    if (!v) return NULL;
    if (kpa2_raised(v)) return v;
    json_free(v); return NULL;
}
/* str(node) for Python str() semantics on scalars (int/str/bool/None). */
static char *kpa2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return kpa2_strdup("None");
    if (v->type == JSON_STRING) return kpa2_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return kpa2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d >= -9007199254740992.0 && d <= 9007199254740992.0)
            return kpa2_asprintf("%.0f", d);
        return kpa2_asprintf("%.17g", d);
    }
    return kpa2_strdup("");
}

/* ── FULL builtin exception hierarchy (byte-pinned to CPython 3.14) ──────────
 * kpa2_exc_parent(type) -> direct base name (NULL for the two roots), used by
 * kpa2_exc_is() to answer `except SomeType` for scripted raises. SystemExit /
 * KeyboardInterrupt / GeneratorExit / CancelledError are BaseException-only. */
static const char *kpa2_exc_parent(const char *t) {
    if (!t) return NULL;
    /* BaseException roots: no parent */
    if (!strcmp(t,"BaseException")) return NULL;
    if (!strcmp(t,"Exception")) return "BaseException";
    if (!strcmp(t,"SystemExit")||!strcmp(t,"KeyboardInterrupt")||!strcmp(t,"GeneratorExit")||
        !strcmp(t,"CancelledError")||!strcmp(t,"asyncio.CancelledError")) return "BaseException";
    /* ArithmeticError family */
    if (!strcmp(t,"ArithmeticError")) return "Exception";
    if (!strcmp(t,"FloatingPointError")||!strcmp(t,"OverflowError")||!strcmp(t,"ZeroDivisionError")) return "ArithmeticError";
    /* AssertionError / AttributeError / BufferError / EOFError */
    if (!strcmp(t,"AssertionError")||!strcmp(t,"AttributeError")||!strcmp(t,"BufferError")||!strcmp(t,"EOFError")) return "Exception";
    /* ImportError family */
    if (!strcmp(t,"ImportError")) return "Exception";
    if (!strcmp(t,"ModuleNotFoundError")) return "ImportError";
    /* LookupError family */
    if (!strcmp(t,"LookupError")) return "Exception";
    if (!strcmp(t,"IndexError")||!strcmp(t,"KeyError")) return "LookupError";
    /* MemoryError / NameError / ReferenceError / StopIteration / StopAsyncIteration / TypeError / SystemError */
    if (!strcmp(t,"MemoryError")||!strcmp(t,"TypeError")||!strcmp(t,"SystemError")||
        !strcmp(t,"StopIteration")||!strcmp(t,"StopAsyncIteration")) return "Exception";
    if (!strcmp(t,"NameError")) return "Exception";
    if (!strcmp(t,"UnboundLocalError")) return "NameError";
    if (!strcmp(t,"ReferenceError")) return "Exception";
    /* OSError family */
    if (!strcmp(t,"OSError")||!strcmp(t,"IOError")||!strcmp(t,"EnvironmentError")) return "Exception";
    if (!strcmp(t,"BlockingIOError")||!strcmp(t,"ChildProcessError")||!strcmp(t,"FileExistsError")||
        !strcmp(t,"FileNotFoundError")||!strcmp(t,"InterruptedError")||!strcmp(t,"IsADirectoryError")||
        !strcmp(t,"NotADirectoryError")||!strcmp(t,"PermissionError")||!strcmp(t,"ProcessLookupError")||
        !strcmp(t,"TimeoutError")) return "OSError";
    if (!strcmp(t,"ConnectionError")) return "OSError";
    if (!strcmp(t,"BrokenPipeError")||!strcmp(t,"ConnectionAbortedError")||!strcmp(t,"ConnectionRefusedError")||
        !strcmp(t,"ConnectionResetError")) return "ConnectionError";
    /* RuntimeError family */
    if (!strcmp(t,"RuntimeError")) return "Exception";
    if (!strcmp(t,"NotImplementedError")||!strcmp(t,"RecursionError")) return "RuntimeError";
    /* SyntaxError family */
    if (!strcmp(t,"SyntaxError")) return "Exception";
    if (!strcmp(t,"IndentationError")) return "SyntaxError";
    if (!strcmp(t,"TabError")) return "IndentationError";
    /* UnicodeError family (ValueError subclass) */
    if (!strcmp(t,"UnicodeError")) return "ValueError";
    if (!strcmp(t,"UnicodeDecodeError")||!strcmp(t,"UnicodeEncodeError")||!strcmp(t,"UnicodeTranslateError")) return "UnicodeError";
    /* ValueError + app exceptions */
    if (!strcmp(t,"ValueError")) return "Exception";
    if (!strcmp(t,"HTTPException")||!strcmp(t,"_StatusRejected")||!strcmp(t,"_SetupGoBack")) return "Exception";
    /* Warning family */
    if (!strcmp(t,"Warning")) return "Exception";
    if (!strcmp(t,"UserWarning")||!strcmp(t,"DeprecationWarning")||!strcmp(t,"PendingDeprecationWarning")||
        !strcmp(t,"SyntaxWarning")||!strcmp(t,"RuntimeWarning")||!strcmp(t,"FutureWarning")||
        !strcmp(t,"ImportWarning")||!strcmp(t,"UnicodeWarning")||!strcmp(t,"BytesWarning")||
        !strcmp(t,"ResourceWarning")) return "Warning";
    /* Unknown/custom -> Exception (fail closed toward the common catch) */
    return "Exception";
}
/* True if type t is wanted or a subclass of wanted. */
static bool kpa2_exc_is(const char *t, const char *wanted) {
    if (!t || !wanted) return false;
    if (!strcmp(t, wanted)) return true;
    const char *cur = t;
    for (int i = 0; i < 24 && cur; i++) {
        cur = kpa2_exc_parent(cur);
        if (cur && !strcmp(cur, wanted)) return true;
    }
    return false;
}
static bool kpa2_raised_is(const json_t *v, const char *wanted) {
    return kpa2_raised(v) && kpa2_exc_is(kpa2_raise_type(v), wanted);
}



/* ── flow handlers (each fires the LIVE decision sequence via kpa2_call) ──── */
/* helper: single-element array */
static json_t *kpa2_arr1(json_t *v) { json_t *a=json_array(); json_append(a, v?v:json_null()); return a; }

/* _board_conn: resolve board, open conn, close on exit; yield [board, conn]. */
/* PoP: kpa2_board_conn @ plugins/kanban/dashboard/plugin_api.py:_board_conn */
static json_t *kpa2_board_conn(kpa2_io_t *io, json_t *in) {
    json_t *b = kpa2_call(io, "_resolve_board", "board", kpa2_get(in,"board"), NULL);
    if (kpa2_raised(b)) return b;
    json_t *conn = kpa2_call(io, "_conn", "board", COPY(b), NULL);
    if (kpa2_raised(conn)) { json_free(b); return conn; }
    json_t *c = kpa2_discard(kpa2_call(io, "conn.close", "conn", COPY(conn), NULL));
    json_t *ret = json_array(); json_append(ret, b ? b : json_null()); json_append(ret, conn);
    if (c) { json_free(ret); return c; }
    return ret;
}
/* _with_board_pinned: resolve+pin then call fn seam. */
/* PoP: kpa2_with_board_pinned @ plugins/kanban/dashboard/plugin_api.py:_with_board_pinned */
static json_t *kpa2_with_board_pinned(kpa2_io_t *io, json_t *in) {
    json_t *b = kpa2_call(io, "_resolve_board", "board", kpa2_get(in,"board"), NULL);
    if (kpa2_raised(b)) return b;
    char *board = kpa2_truthy(b) ? kpa2_pystr(b) : kpa2_strdup("default");
    json_t *enter = kpa2_call(io, "kanban_db.scoped_current_board", "board", kpa2_str(board), NULL);
    if (kpa2_raised(enter)) { json_free(b); free(board); return enter; }
    json_t *r = kpa2_call(io, kpa2_s(in,"fn","fn"), "board", kpa2_str(board), NULL);
    json_t *exit = kpa2_discard(kpa2_call(io, "scoped_current_board.exit", NULL));
    json_free(b); free(board);
    if (kpa2_raised(r)) return r;
    if (exit) return exit;
    return r;
}
/* _run_aux: pin board, lazy import hermes_cli.<module>, call <fn>(task_id,author). */
/* PoP: kpa2_run_aux @ plugins/kanban/dashboard/plugin_api.py:_run_aux */
static json_t *kpa2_run_aux(kpa2_io_t *io, json_t *in) {
    json_t *b = kpa2_call(io, "_resolve_board", "board", kpa2_get(in,"board"), NULL);
    if (kpa2_raised(b)) return b;
    char *board = kpa2_truthy(b) ? kpa2_pystr(b) : kpa2_strdup("default");
    json_t *enter = kpa2_call(io, "kanban_db.scoped_current_board", "board", kpa2_str(board), NULL);
    if (kpa2_raised(enter)) { json_free(b); free(board); return enter; }
    char mod[256]; snprintf(mod,sizeof mod,"hermes_cli.%s", kpa2_s(in,"module",""));
    json_t *m = kpa2_call(io, "importlib.import_module", "module", kpa2_str(mod), NULL);
    json_t *r = NULL;
    if (kpa2_raised(m)) r = m;
    else {
        json_t *author = json_obj_get(in,"author");
        r = kpa2_call(io, kpa2_s(in,"fn","fn"), "task_id", kpa2_get(in,"task_id"),
                      "author", (author && kpa2_truthy(author)) ? json_copy(author) : json_null(), NULL);
    }
    json_t *exit = kpa2_discard(kpa2_call(io, "scoped_current_board.exit", NULL));
    json_free(b); free(board);
    if (kpa2_raised(r)) return r;
    if (exit) return exit;
    return r;
}
/* _map_errors: translate matching exception into HTTPException(status,str(exc)). */
/* PoP: kpa2_map_errors @ plugins/kanban/dashboard/plugin_api.py:_map_errors */
static json_t *kpa2_map_errors(kpa2_io_t *io, json_t *in) {
    json_t *body = kpa2_call(io, "body", NULL);
    if (!kpa2_raised(body)) return body;
    const char *t = kpa2_raise_type(body);
    char *msg = kpa2_strdup(kpa2_raise_msg(body));
    json_t *types = json_obj_get(in, "types");
    bool match = false;
    if (types && types->type == JSON_ARRAY) {
        for (size_t i = 0; i < types->c.count; i++) {
            const char *w = json_get_str(json_get(types,i),"",NULL);
            if (w && kpa2_exc_is(t, w)) { match = true; break; }
        }
    }
    json_free(body);
    if (!match) { free(msg); return kpa2_make_raise(t, ""); }
    json_t *e = kpa2_http_exc((int)kpa2_n(in,"status",400), msg ? msg : "");
    free(msg); return e;
}
/* _errors_to_500: HTTPException passes; other Exception -> 500 "<prefix>: <exc>". */
/* PoP: kpa2_errors_to_500 @ plugins/kanban/dashboard/plugin_api.py:_errors_to_500 */
static json_t *kpa2_errors_to_500(kpa2_io_t *io, json_t *in) {
    json_t *body = kpa2_call(io, "body", NULL);
    if (!kpa2_raised(body)) return body;
    const char *t = kpa2_raise_type(body);
    if (kpa2_exc_is(t, "HTTPException") || !kpa2_exc_is(t, "Exception")) return body;
    char *msg = kpa2_asprintf("%s: %s", kpa2_s(in,"prefix",""), kpa2_raise_msg(body));
    json_t *e = kpa2_http_exc(500, msg); free(msg); json_free(body); return e;
}
/* _parents_blocking_ready: rows -> [{"id","title","status"}]. */
/* PoP: kpa2_parents_blocking_ready @ plugins/kanban/dashboard/plugin_api.py:_parents_blocking_ready */
static json_t *kpa2_parents_blocking_ready(kpa2_io_t *io, json_t *in) {
    json_t *rows = kpa2_call(io, "conn.execute", "sql",
        kpa2_str("SELECT t.id, t.title, t.status FROM tasks t JOIN task_links l ON l.parent_id = t.id WHERE l.child_id = ? AND t.status != 'done'"),
        "args", kpa2_arr1(kpa2_get(in,"task_id")), NULL);
    if (kpa2_raised(rows)) return rows;
    json_t *out = json_array();
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i=0;i<rows->c.count;i++) {
            json_t *r = json_get(rows,i); json_t *d = json_object();
            json_set(d,"id",kpa2_get(r,"id")); json_set(d,"title",kpa2_get(r,"title"));
            json_set(d,"status",kpa2_get(r,"status")); json_append(out,d);
        }
    }
    json_free(rows); return out;
}
/* _configured_home_channels: import+load config, build list sorted by platform. */
/* PoP: kpa2_configured_home_channels @ plugins/kanban/dashboard/plugin_api.py:_configured_home_channels */
static json_t *kpa2_configured_home_channels(kpa2_io_t *io, json_t *in) {
    (void)in;
    json_t *m = kpa2_call(io, "importlib.import_module", "module", kpa2_str("gateway.config"), NULL);
    if (kpa2_raised(m)) { json_free(m); return json_array(); }
    json_free(m);
    json_t *cfg = kpa2_call(io, "load_gateway_config", NULL);
    if (kpa2_raised(cfg)) { json_free(cfg); return json_array(); }
    json_t *out = json_array();
    json_t *plats = json_obj_get(cfg, "platforms");
    if (plats && plats->type == JSON_OBJECT) {
        for (size_t i=0;i<plats->c.count;i++) {
            json_t *pc = plats->c.items[i];
            json_t *hc = json_obj_get(pc, "home_channel");
            if (!hc || !kpa2_truthy(hc)) continue;
            json_t *d = json_object();
            json_set(d,"platform", kpa2_get(pc,"value"));
            json_set(d,"chat_id", kpa2_get(hc,"chat_id"));
            json_t *tid = json_obj_get(hc,"thread_id");
            json_set(d,"thread_id", (tid && kpa2_truthy(tid)) ? json_copy(tid) : kpa2_str(""));
            json_t *nm = json_obj_get(hc,"name");
            json_set(d,"name", (nm && kpa2_truthy(nm)) ? json_copy(nm) : kpa2_str("Home"));
            json_append(out,d);
        }
    }
    json_free(cfg);
    for (size_t i=1;i<out->c.count;i++)
        for (size_t j=i;j>0 && strcmp(kpa2_s(json_get(out,j-1),"platform",""),kpa2_s(json_get(out,j),"platform",""))>0;j--) {
            json_t *t=out->c.items[j-1]; out->c.items[j-1]=out->c.items[j]; out->c.items[j]=t;
        }
    return out;
}



/* board-conn prefix helper: live `with _board_conn(board) as (board, conn)`. */
static json_t *kpa2_bc_open(kpa2_io_t *io, json_t *in, json_t **board, json_t **conn) {
    json_t *b = kpa2_call(io, "_resolve_board", "board", kpa2_get(in,"board"), NULL);
    if (kpa2_raised(b)) return b;
    json_t *c = kpa2_call(io, "_conn", "board", COPY(b), NULL);
    if (kpa2_raised(c)) { json_free(b); return c; }
    *board = b; *conn = c; return NULL;
}
static json_t *kpa2_bc_close(kpa2_io_t *io, json_t *conn) {
    return kpa2_discard(kpa2_call(io, "conn.close", "conn", COPY(conn), NULL));
}
/* statement-position helper call: propagate raise, discard value. */
static json_t *kpa2_callvoid(kpa2_io_t *io, const char *name, ...) {
    json_t *args = json_object(); va_list ap; va_start(ap,name);
    const char *k; while ((k=va_arg(ap,const char*))!=NULL) { json_t *v=va_arg(ap,json_t*); json_set(args,k,v?v:json_null()); }
    va_end(ap); return kpa2_discard(kpa2_seam(io,name,args));
}

/* get_task: run_state validation, require, task_dict, links, diagnostics, rollups. */
/* PoP: kpa2_get_task @ plugins/kanban/dashboard/plugin_api.py:get_task */
static json_t *kpa2_get_task(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *rst=json_obj_get(in,"run_state_type"), *rsn=json_obj_get(in,"run_state_name");
    bool xor = kpa2_truthy(rst) != kpa2_truthy(rsn);
    if (xor) { e=kpa2_http_exc(400,"run_state_type and run_state_name must be passed together or omitted"); goto done; }
    if (kpa2_truthy(rst)) {
        const char *s=kpa2_ns(rst,"");
        if (strcmp(s,"status") && strcmp(s,"outcome")) { e=kpa2_http_exc(400,"run_state_type must be 'status' or 'outcome'"); goto done; }
    }
    json_t *task=kpa2_call(io,"_require_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (kpa2_raised(task)) { e=task; goto done; }
    json_t *summary=kpa2_call(io,"kanban_db.latest_summary","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *rs=kpa2_call(io,"kanban_db.current_run_started_ats","conn",COPY(conn),"ids",kpa2_arr1(kpa2_get(in,"task_id")),NULL);
    json_t *td=kpa2_call(io,"_task_dict","task",task,
        "latest_summary",(kpa2_raised(summary)?json_null():COPY(summary)),
        "current_run_started_at",(kpa2_raised(rs)?json_null():kpa2_get(rs,kpa2_s(in,"task_id",""))),NULL);
    json_t *links=kpa2_call(io,"_links_for","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (kpa2_raised(links)) { e=links; json_free(summary); json_free(rs); json_free(td); goto done; }
    json_t *children=json_obj_get(links,"children");
    json_t *sums=kpa2_call(io,"kanban_db.latest_summaries","conn",COPY(conn),"ids",children?json_copy(children):json_array(),NULL);
    json_t *diags=kpa2_call(io,"_compute_task_diagnostics","conn",COPY(conn),"task_ids",kpa2_arr1(kpa2_get(in,"task_id")),NULL);
    json_t *dt=kpa2_raised(diags)?json_null():kpa2_get(diags,kpa2_s(in,"task_id",""));
    json_t *att=kpa2_call(io,"_attach_diagnostics","task_d",td,"diags",dt?dt:json_array(),NULL);
    json_t *comments=kpa2_call(io,"kanban_db.list_comments","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *events=kpa2_call(io,"kanban_db.list_events","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *atts=kpa2_call(io,"kanban_db.list_attachments","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *lts=kpa2_call(io,"_link_tasks","conn",COPY(conn),"links",COPY(links),NULL);
    json_t *runs=kpa2_call(io,"kanban_db.list_runs","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *ret=json_object();
    json_set(ret,"task",td?td:json_null());
    json_set(ret,"comments",kpa2_raised(comments)?json_null():COPY(comments));
    json_set(ret,"events",kpa2_raised(events)?json_null():COPY(events));
    json_set(ret,"attachments",kpa2_raised(atts)?json_null():COPY(atts));
    json_set(ret,"links",links?links:json_null());
    json_set(ret,"link_tasks",lts?lts:json_null());
    json_set(ret,"child_results",json_array());
    json_set(ret,"runs",kpa2_raised(runs)?json_null():COPY(runs));
    json_free(summary); json_free(rs); json_free(sums); json_free(diags); json_free(att);
    e=ret;
done:
    { json_t *c=kpa2_bc_close(io,conn); if(c && !e) e=c; else if(c) json_free(c); }
    json_free(b); return e;
}
/* list_task_attachments / remove_attachment / delete_task / add_comment / add_link / delete_link */
/* PoP: kpa2_list_task_attachments @ plugins/kanban/dashboard/plugin_api.py:list_task_attachments */
static json_t *kpa2_list_task_attachments(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    e=kpa2_callvoid(io,"_require_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *atts=NULL;
    if (!e) atts=kpa2_call(io,"kanban_db.list_attachments","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (e) { json_t*c=kpa2_bc_close(io,conn); if(c&&!kpa2_raised(e)) json_free(e),e=c; else if(c) json_free(c); json_free(b); return e; }
    json_t *ret=json_object(); json_set(ret,"attachments",kpa2_raised(atts)?json_null():COPY(atts));
    json_free(atts); json_t*c=kpa2_bc_close(io,conn); if(c&&!kpa2_raised(ret)) json_free(ret),ret=c; else if(c) json_free(c);
    json_free(b); return ret;
}
/* PoP: kpa2_remove_attachment @ plugins/kanban/dashboard/plugin_api.py:remove_attachment */
static json_t *kpa2_remove_attachment(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"kanban_db.delete_attachment","conn",COPY(conn),"attachment_id",kpa2_get(in,"attachment_id"),NULL);
    if (!kpa2_raised(r) && !kpa2_truthy(r)) r=kpa2_http_exc(404,"attachment not found");
    if (kpa2_raised(r)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return r; }
    json_t *ret=json_object(); json_set(ret,"ok",json_bool(true)); json_set(ret,"id",kpa2_get(in,"attachment_id"));
    json_free(r); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return ret;
}
/* PoP: kpa2_delete_task @ plugins/kanban/dashboard/plugin_api.py:delete_task */
static json_t *kpa2_delete_task(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"kanban_db.delete_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (!kpa2_raised(r) && !kpa2_truthy(r)) {
        char *m=kpa2_asprintf("task %s not found",kpa2_s(in,"task_id","")); r=kpa2_http_exc(404,m); free(m);
    }
    if (kpa2_raised(r)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return r; }
    json_t *ret=json_object(); json_set(ret,"deleted",json_bool(true)); json_set(ret,"task_id",kpa2_get(in,"task_id"));
    json_free(r); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return ret;
}
/* PoP: kpa2_add_link @ plugins/kanban/dashboard/plugin_api.py:add_link */
static json_t *kpa2_add_link(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"kanban_db.link_tasks","conn",COPY(conn),"parent_id",kpa2_get(in,"parent_id"),"child_id",kpa2_get(in,"child_id"),NULL);
    if (kpa2_raised(r)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return r; }
    json_t *ret=json_object(); json_set(ret,"ok",json_bool(true)); json_set(ret,"gated",COPY(r));
    json_free(r); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return ret;
}
/* PoP: kpa2_delete_link @ plugins/kanban/dashboard/plugin_api.py:delete_link */
static json_t *kpa2_delete_link(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"kanban_db.unlink_tasks","conn",COPY(conn),"parent_id",kpa2_get(in,"parent_id"),"child_id",kpa2_get(in,"child_id"),NULL);
    if (kpa2_raised(r)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return r; }
    json_t *ret=json_object(); json_set(ret,"ok",json_bool(kpa2_truthy(r)));
    json_free(r); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return ret;
}
/* PoP: kpa2_add_comment @ plugins/kanban/dashboard/plugin_api.py:add_comment */
static json_t *kpa2_add_comment(kpa2_io_t *io, json_t *in) {
    const char *body=kpa2_s(json_obj_get(in,"payload"),"body","");
    char *t=kpa2_strip(body); bool empty = !t || !*t; free(t);
    if (empty) return kpa2_http_exc(400,"body is required");
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    e=kpa2_callvoid(io,"_require_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (!e) e=kpa2_callvoid(io,"kanban_db.add_comment","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *ret=e?e:json_object(); if(!e) json_set(ret,"ok",json_bool(true));
    json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return ret;
}



/* Generic board-conn wrapper: fire resolve+conn+[core seam]+close. The core
 * seam's scripted result is the flow return (error-mapped by the caller). */
static json_t *kpa2_bc_core(kpa2_io_t *io, json_t *in, const char *core, json_t *coreargs) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *a=(coreargs && coreargs->type==JSON_OBJECT) ? json_copy(coreargs) : json_object();
    json_t *r=kpa2_seam(io,core,a);
    json_t *c=kpa2_bc_close(io,conn); if(c && !kpa2_raised(r)) json_free(r),r=c; else if(c) json_free(c);
    json_free(b); return r;
}
/* get_stats / get_assignees / get_run_endpoint / get_home_channels(no core) */
/* PoP: kpa2_get_stats @ plugins/kanban/dashboard/plugin_api.py:get_stats */
static json_t *kpa2_get_stats(kpa2_io_t *io, json_t *in) {
    return kpa2_bc_core(io,in,"kanban_db.board_stats",kpa2_get(in,"args"));
}
/* PoP: kpa2_get_assignees @ plugins/kanban/dashboard/plugin_api.py:get_assignees */
static json_t *kpa2_get_assignees(kpa2_io_t *io, json_t *in) {
    json_t *r=kpa2_bc_core(io,in,"kanban_db.known_assignees",kpa2_get(in,"args"));
    if (kpa2_raised(r)) return r;
    json_t *o=json_object(); json_set(o,"assignees",r); return o;
}
/* PoP: kpa2_get_run_endpoint @ plugins/kanban/dashboard/plugin_api.py:get_run_endpoint */
static json_t *kpa2_get_run_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *r=kpa2_bc_core(io,in,"_require_run",kpa2_get(in,"args"));
    if (kpa2_raised(r)) return r;
    json_t *o=json_object(); json_set(o,"run",r); return o;
}
/* PoP: kpa2_get_home_channels @ plugins/kanban/dashboard/plugin_api.py:get_home_channels */
static json_t *kpa2_get_home_channels(kpa2_io_t *io, json_t *in) {
    json_t *homes=kpa2_call(io,"_configured_home_channels",NULL);
    if (kpa2_raised(homes)) return homes;
    if (kpa2_truthy(json_obj_get(in,"task_id"))) {
        json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e){json_free(homes);return e;}
        e=kpa2_callvoid(io,"kbn.list_notify_subs","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
        json_t*c=kpa2_bc_close(io,conn); if(c&&!e) e=c; else if(c) json_free(c); json_free(b);
        if(e){json_free(homes);return e;}
    }
    return homes;
}
/* home_for_platform: configured homes -> find platform or 404. */
/* PoP: kpa2_home_for_platform @ plugins/kanban/dashboard/plugin_api.py:_home_for_platform */
static json_t *kpa2_home_for_platform(kpa2_io_t *io, json_t *in) {
    json_t *homes=kpa2_call(io,"_configured_home_channels",NULL);
    if (kpa2_raised(homes)) return homes;
    const char *want=kpa2_s(in,"platform","");
    if (homes && homes->type==JSON_ARRAY) {
        for (size_t i=0;i<homes->c.count;i++) {
            json_t *h=json_get(homes,i);
            if (!strcmp(kpa2_s(h,"platform",""),want)) { json_t*r=json_copy(h); json_free(homes); return r; }
        }
    }
    json_free(homes);
    return kpa2_http_exc(404,kpa2_s(in,"detail",""));
}
/* subscribe/unsubscribe_home: home lookup + board conn + notify sub. */
/* PoP: kpa2_subscribe_home @ plugins/kanban/dashboard/plugin_api.py:subscribe_home */
static json_t *kpa2_subscribe_home(kpa2_io_t *io, json_t *in) {
    json_t *home=kpa2_call(io,"_home_for_platform","platform",kpa2_get(in,"platform"),NULL);
    if (kpa2_raised(home)) return home;
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e){json_free(home);return e;}
    e=kpa2_callvoid(io,"_require_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if(!e) e=kpa2_callvoid(io,"kbn.add_notify_sub","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),
        "platform",kpa2_get(in,"platform"),"chat_id",kpa2_get(home,"chat_id"),NULL);
    json_t *ret=e?e:json_object(); if(!e){json_set(ret,"ok",json_bool(true));json_set(ret,"task_id",kpa2_get(in,"task_id"));json_set(ret,"home_channel",home);}
    json_t*c=kpa2_bc_close(io,conn); if(c&&!kpa2_raised(ret)) json_free(ret),ret=c; else if(c) json_free(c);
    json_free(b); return ret;
}
/* PoP: kpa2_unsubscribe_home @ plugins/kanban/dashboard/plugin_api.py:unsubscribe_home */
static json_t *kpa2_unsubscribe_home(kpa2_io_t *io, json_t *in) {
    json_t *home=kpa2_call(io,"_home_for_platform","platform",kpa2_get(in,"platform"),NULL);
    if (kpa2_raised(home)) return home;
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e){json_free(home);return e;}
    e=kpa2_callvoid(io,"kbn.remove_notify_sub","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),
        "platform",kpa2_get(in,"platform"),"chat_id",kpa2_get(home,"chat_id"),NULL);
    json_t *ret=e?e:json_object(); if(!e){json_set(ret,"ok",json_bool(true));json_set(ret,"task_id",kpa2_get(in,"task_id"));json_set(ret,"home_channel",home);}
    json_t*c=kpa2_bc_close(io,conn); if(c&&!kpa2_raised(ret)) json_free(ret),ret=c; else if(c) json_free(c);
    json_free(b); return ret;
}
/* get_task_log: board conn + require, then read worker log + path stat. */
/* PoP: kpa2_get_task_log @ plugins/kanban/dashboard/plugin_api.py:get_task_log */
static json_t *kpa2_get_task_log(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    e=kpa2_callvoid(io,"_require_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *content=NULL;
    if (!e) content=kpa2_call(io,"kanban_db.read_worker_log","task_id",kpa2_get(in,"task_id"),"tail_bytes",kpa2_get(in,"tail"),NULL);
    if (e) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return e; }
    json_t *sz=kpa2_call(io,"log_path.stat","task_id",kpa2_get(in,"task_id"),NULL);
    json_t *ret=json_object();
    json_set(ret,"task_id",kpa2_get(in,"task_id"));
    json_set(ret,"path",kpa2_get(in,"log_path"));
    json_set(ret,"exists",json_bool(!kpa2_is_none(content)));
    json_set(ret,"size_bytes",kpa2_raised(sz)?json_null():COPY(sz));
    json_set(ret,"content",kpa2_is_none(content)?kpa2_str(""):COPY(content));
    json_set(ret,"truncated",json_bool(false));
    json_free(content); json_free(sz);
    json_t*c=kpa2_bc_close(io,conn); if(c&&!kpa2_raised(ret)) json_free(ret),ret=c; else if(c) json_free(c);
    json_free(b); return ret;
}
/* dispatch: board conn + dispatch_once; asdict or str fallback. */
/* PoP: kpa2_dispatch @ plugins/kanban/dashboard/plugin_api.py:dispatch */
static json_t *kpa2_dispatch(kpa2_io_t *io, json_t *in) {
    json_t *r=kpa2_bc_core(io,in,"kbd.dispatch_once",kpa2_get(in,"args"));
    if (kpa2_raised(r)) return r;
    return r; /* scripted result models asdict(result) */
}
/* terminate / reclaim / reassign endpoints: board conn + core + conflict mapping. */
/* PoP: kpa2_terminate_run_endpoint @ plugins/kanban/dashboard/plugin_api.py:terminate_run_endpoint */
static json_t *kpa2_terminate_run_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"_require_run","conn",COPY(conn),"run_id",kpa2_get(in,"run_id"),NULL);
    if (kpa2_raised(r)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return r; }
    const char *ended=kpa2_s(r,"ended_at","");
    if (*ended) { char*m=kpa2_asprintf("run %s already ended",kpa2_s(in,"run_id","")); r=kpa2_http_exc(409,m); free(m); }
    if (!kpa2_raised(r)) {
        json_t *ok=kpa2_call(io,"kanban_db.reclaim_task","conn",COPY(conn),"task_id",kpa2_get(r,"task_id"),NULL);
        if (!kpa2_truthy(ok)) { char*m=kpa2_asprintf("cannot terminate run %s: task %s is no longer in a reclaimable state",kpa2_s(in,"run_id",""),kpa2_s(r,"task_id","")); json_free(ok); ok=kpa2_http_exc(409,m); free(m); }
        if (kpa2_raised(ok)) { json_free(r); r=ok; }
        else { json_t*o=json_object(); json_set(o,"ok",json_bool(true)); json_set(o,"run_id",kpa2_get(in,"run_id")); json_set(o,"task_id",kpa2_get(r,"task_id")); json_free(r); json_free(ok); r=o; }
    }
    json_t*c=kpa2_bc_close(io,conn); if(c&&!kpa2_raised(r)) json_free(r),r=c; else if(c) json_free(c);
    json_free(b); return r;
}
/* PoP: kpa2_reclaim_task_endpoint @ plugins/kanban/dashboard/plugin_api.py:reclaim_task_endpoint */
static json_t *kpa2_reclaim_task_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"kanban_db.reclaim_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (!kpa2_raised(r) && !kpa2_truthy(r)) {
        char *m=kpa2_asprintf("cannot reclaim %s: not in a claimable state (not running, or unknown id)",kpa2_s(in,"task_id",""));
        json_free(r); r=kpa2_http_exc(409,m); free(m);
    }
    if (kpa2_raised(r)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return r; }
    json_t *o=json_object(); json_set(o,"ok",json_bool(true)); json_set(o,"task_id",kpa2_get(in,"task_id"));
    json_free(r); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return o;
}
/* PoP: kpa2_reassign_task_endpoint @ plugins/kanban/dashboard/plugin_api.py:reassign_task_endpoint */
static json_t *kpa2_reassign_task_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"kanban_db.reassign_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (!kpa2_raised(r) && !kpa2_truthy(r)) {
        char *m=kpa2_asprintf("cannot reassign %s: unknown id, or still running (pass reclaim_first=true to release the claim first)",kpa2_s(in,"task_id",""));
        json_free(r); r=kpa2_http_exc(409,m); free(m);
    }
    if (kpa2_raised(r)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return r; }
    json_t *o=json_object(); json_set(o,"ok",json_bool(true)); json_set(o,"task_id",kpa2_get(in,"task_id"));
    json_set(o,"assignee",json_obj_get(in,"profile")?json_copy(json_obj_get(in,"profile")):json_null());
    json_free(r); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return o;
}



/* get_config: load config, build 4-field dict. */
/* PoP: kpa2_get_config @ plugins/kanban/dashboard/plugin_api.py:get_config */
static json_t *kpa2_get_config(kpa2_io_t *io, json_t *in) {
    (void)in;
    json_t *cfg=kpa2_call(io,"_load_config_or_empty",NULL);
    if (kpa2_raised(cfg)) return cfg;
    json_t *dash=json_obj_get(cfg,"dashboard");
    json_t *k=json_obj_get(dash,"kanban");
    json_t *o=json_object();
    json_t *dt=json_obj_get(k,"default_tenant");
    json_set(o,"default_tenant",(dt&&kpa2_truthy(dt))?json_copy(dt):kpa2_str(""));
    json_set(o,"lane_by_profile",json_bool(kpa2_b(k,"lane_by_profile",true)));
    json_set(o,"include_archived_by_default",json_bool(kpa2_b(k,"include_archived_by_default",false)));
    json_set(o,"render_markdown",json_bool(kpa2_b(k,"render_markdown",true)));
    json_free(cfg); return o;
}
/* _run_estimate: import guard, cap prompt, affinity, LLM call, tolerant parse. */
/* PoP: kpa2_run_estimate @ plugins/kanban/dashboard/plugin_api.py:_run_estimate */
static json_t *kpa2_run_estimate(kpa2_io_t *io, json_t *in) {
    const char *title=kpa2_s(in,"title",""); char *t=kpa2_strip(title);
    bool empty=!t||!*t; free(t);
    if (empty) { json_t*o=json_object(); json_set(o,"ok",json_bool(false)); json_set(o,"reason",kpa2_str("a title is required to estimate")); return o; }
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("agent.auxiliary_client"),NULL);
    if (kpa2_raised(m)) { json_free(m); json_t*o=json_object(); json_set(o,"ok",json_bool(false)); json_set(o,"reason",kpa2_str("auxiliary client unavailable")); return o; }
    json_free(m);
    json_t *aff=kpa2_call(io,"get_affinity_scope",NULL);
    if (kpa2_raised(aff)) aff=json_null();
    json_t *resp=kpa2_call(io,"call_llm","title",kpa2_get(in,"title"),"body",kpa2_get(in,"body"),NULL);
    if (kpa2_raised(resp)) {
        char *r=kpa2_asprintf("LLM error: %s",kpa2_raise_type(resp));
        json_t*o=json_object(); json_set(o,"ok",json_bool(false)); json_set(o,"reason",kpa2_str(r)); free(r); json_free(resp);
        if (kpa2_truthy(aff)) { json_t*x=kpa2_callvoid(io,"reset_affinity_scope",NULL); if(x) json_free(x); }
        json_free(aff); return o;
    }
    json_t*o=json_object();
    json_t *parsed=json_obj_get(resp,"parsed");
    if (!parsed||!kpa2_truthy(parsed)) {
        json_set(o,"ok",json_bool(false)); json_set(o,"reason",kpa2_str("could not parse an estimate from the model"));
    } else {
        json_set(o,"ok",json_bool(true));
        json_set(o,"est_tokens",kpa2_get(parsed,"est_tokens"));
        json_t *cx=json_obj_get(parsed,"complexity");
        const char *cs=kpa2_s(parsed,"complexity",""); char *cu=kpa2_strip(cs);
        bool valid=cu&&(!strcmp(cu,"S")||!strcmp(cu,"M")||!strcmp(cu,"L")); free(cu);
        json_set(o,"complexity",valid?json_copy(cx):json_null());
        const char *rr=kpa2_s(parsed,"rationale",""); char *rt=kpa2_strip(rr);
        json_set(o,"rationale",(rt&&*rt)?kpa2_str(rt):json_null()); free(rt);
        json_set(o,"model",kpa2_get(resp,"model"));
    }
    if (kpa2_truthy(aff)) { json_t*x=kpa2_callvoid(io,"reset_affinity_scope",NULL); if(x) json_free(x); }
    json_free(aff); json_free(resp); return o;
}
/* estimate_text / estimate_task / decompose / specify: thin _run_* wrappers. */
/* PoP: kpa2_estimate_text_endpoint @ plugins/kanban/dashboard/plugin_api.py:estimate_text_endpoint */
static json_t *kpa2_estimate_text_endpoint(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"_run_estimate","title",kpa2_get(json_obj_get(in,"payload"),"title"),
        "body",kpa2_get(json_obj_get(in,"payload"),"body"),"task_id",json_null(),NULL);
}
/* PoP: kpa2_estimate_task_endpoint @ plugins/kanban/dashboard/plugin_api.py:estimate_task_endpoint */
static json_t *kpa2_estimate_task_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *t=kpa2_bc_core(io,in,"_require_task",kpa2_get(in,"args"));
    if (kpa2_raised(t)) return t;
    return kpa2_call(io,"_run_estimate","title",kpa2_get(t,"title"),"body",kpa2_get(t,"body"),"task_id",kpa2_get(in,"task_id"),NULL);
}
/* PoP: kpa2_decompose_task_endpoint @ plugins/kanban/dashboard/plugin_api.py:decompose_task_endpoint */
static json_t *kpa2_decompose_task_endpoint(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"_run_aux","board",kpa2_get(in,"board"),"module",kpa2_str("kanban_decompose"),
        "fn",kpa2_str("decompose_task"),"task_id",kpa2_get(in,"task_id"),
        "author",kpa2_get(json_obj_get(in,"payload"),"author"),NULL);
}
/* PoP: kpa2_specify_task_endpoint @ plugins/kanban/dashboard/plugin_api.py:specify_task_endpoint */
static json_t *kpa2_specify_task_endpoint(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"_run_aux","board",kpa2_get(in,"board"),"module",kpa2_str("kanban_specify"),
        "fn",kpa2_str("specify_task"),"task_id",kpa2_get(in,"task_id"),
        "author",kpa2_get(json_obj_get(in,"payload"),"author"),NULL);
}
/* model_options: import+build payload, map providers, except->[]. */
/* PoP: kpa2_model_options @ plugins/kanban/dashboard/plugin_api.py:model_options */
static json_t *kpa2_model_options(kpa2_io_t *io, json_t *in) {
    (void)in;
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.inventory"),NULL);
    if (kpa2_raised(m)) { json_free(m); json_t*o=json_object(); json_set(o,"providers",json_array()); return o; }
    json_free(m);
    json_t *p=kpa2_call(io,"build_models_payload",NULL);
    if (kpa2_raised(p)) { json_free(p); json_t*o=json_object(); json_set(o,"providers",json_array()); return o; }
    json_t *out=json_array();
    json_t *rows=json_obj_get(p,"providers");
    if (rows&&rows->type==JSON_ARRAY) for(size_t i=0;i<rows->c.count;i++) {
        json_t *r=json_get(rows,i); json_t *ms=json_obj_get(r,"models");
        if (!ms||!kpa2_truthy(ms)) continue;
        json_t *d=json_object();
        json_t *slug=json_obj_get(r,"slug"); json_set(d,"slug",(slug&&kpa2_truthy(slug))?json_copy(slug):kpa2_str(""));
        json_t *label=json_obj_get(r,"label");
        json_set(d,"label",(label&&kpa2_truthy(label))?json_copy(label):kpa2_get(r,"slug"));
        json_set(d,"models",json_copy(ms)); json_append(out,d);
    }
    json_free(p); json_t*o=json_object(); json_set(o,"providers",out); return o;
}
/* _resolve_project / _projects_by_id / _board_counts / _default_workspace_kind */
/* PoP: kpa2_resolve_project @ plugins/kanban/dashboard/plugin_api.py:_resolve_project */
static json_t *kpa2_resolve_project(kpa2_io_t *io, json_t *in) {
    const char *ref=kpa2_s(in,"ref",""); char *s=kpa2_strip(ref); bool empty=!s||!*s; free(s);
    if (empty) { json_t*a=json_array(); json_append(a,json_null());json_append(a,json_null());json_append(a,json_null()); return a; }
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.projects_db"),NULL);
    if (kpa2_raised(m)) return m; json_free(m);
    json_t *p=kpa2_call(io,"pdb.get_project","ref",kpa2_str(ref),NULL);
    if (kpa2_raised(p)) return p;
    if (!kpa2_truthy(p)) return kpa2_http_exc(400,kpa2_s(in,"detail",""));
    json_t *a=json_array(); json_append(a,kpa2_get(p,"id")); json_append(a,kpa2_get(p,"name"));
    json_t *pp=json_obj_get(p,"primary_path"); json_append(a,(pp&&kpa2_truthy(pp))?json_copy(pp):json_null());
    json_free(p); return a;
}
/* PoP: kpa2_projects_by_id @ plugins/kanban/dashboard/plugin_api.py:_projects_by_id */
static json_t *kpa2_projects_by_id(kpa2_io_t *io, json_t *in) {
    (void)in;
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.projects_db"),NULL);
    if (kpa2_raised(m)) return json_object(); json_free(m);
    json_t *p=kpa2_call(io,"pdb.list_projects",NULL);
    if (kpa2_raised(p)) { json_free(p); return json_object(); }
    json_t *o=json_object();
    if (p&&p->type==JSON_ARRAY) for(size_t i=0;i<p->c.count;i++){json_t*r=json_get(p,i);json_set(o,kpa2_s(r,"id",""),json_copy(r));}
    json_free(p); return o;
}
/* PoP: kpa2_board_counts @ plugins/kanban/dashboard/plugin_api.py:_board_counts */
static json_t *kpa2_board_counts(kpa2_io_t *io, json_t *in) {
    json_t *ex=kpa2_call(io,"kanban_db.kanban_db_path","board",kpa2_get(in,"slug"),NULL);
    if (kpa2_raised(ex)) { json_free(ex); return json_object(); }
    if (!kpa2_truthy(ex)) { json_free(ex); return json_object(); }
    json_free(ex);
    json_t *rows=kpa2_call(io,"conn.execute","sql",kpa2_str("SELECT status, COUNT(*) AS n FROM tasks GROUP BY status"),NULL);
    if (kpa2_raised(rows)) { json_free(rows); return json_object(); }
    json_t *o=json_object();
    if (rows&&rows->type==JSON_ARRAY) for(size_t i=0;i<rows->c.count;i++){json_t*r=json_get(rows,i);json_set(o,kpa2_s(r,"status",""),json_number(kpa2_n(r,"n",0)));}
    json_free(rows); return o;
}
/* PoP: kpa2_default_workspace_kind @ plugins/kanban/dashboard/plugin_api.py:_default_workspace_kind */
static json_t *kpa2_default_workspace_kind(kpa2_io_t *io, json_t *in) {
    char *wd=kpa2_strip(kpa2_s(json_obj_get(in,"board"),"default_workdir",""));
    if (!wd||!*wd) { free(wd); return kpa2_str("scratch"); }
    json_t *r=kpa2_call(io,"kbw._git_toplevel","path",kpa2_str(wd),NULL);
    free(wd);
    if (kpa2_raised(r)) { json_free(r); return kpa2_str("dir"); }
    json_t *o=kpa2_truthy(r)?kpa2_str("worktree"):kpa2_str("dir"); json_free(r); return o;
}
/* PoP: kpa2_annotate_board_meta @ plugins/kanban/dashboard/plugin_api.py:_annotate_board_meta */
static json_t *kpa2_annotate_board_meta(kpa2_io_t *io, json_t *in) {
    json_t *meta=json_obj_get(in,"meta");
    json_t *w=kpa2_call(io,"_default_workspace_kind","board",COPY(meta),NULL);
    json_t *p=kpa2_call(io,"_resolve_project","ref",kpa2_get(meta,"project_id"),NULL);
    json_t *o=COPY(meta);
    if (!kpa2_raised(w)) json_set(o,"default_workspace_kind",w); else json_free(w);
    if (!kpa2_raised(p)&&p&&p->type==JSON_ARRAY&&p->c.count>1) json_set(o,"project_name",json_copy(json_get(p,1)));
    json_free(p); return o;
}



/* bulk/task mutation helpers */
/* PoP: kpa2_bulk_update @ plugins/kanban/dashboard/plugin_api.py:bulk_update */
static json_t *kpa2_bulk_update(kpa2_io_t *io, json_t *in) {
    json_t *ids=json_obj_get(json_obj_get(in,"payload"),"ids");
    if (!ids||!kpa2_truthy(ids)) return kpa2_http_exc(400,"ids is required");
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *results=kpa2_call(io,"_bulk_apply_one","conn",COPY(conn),"ids",json_copy(ids),NULL);
    if (kpa2_raised(results)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return results; }
    json_t*o=json_object(); json_set(o,"results",results);
    json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return o;
}
/* PoP: kpa2_patch_status @ plugins/kanban/dashboard/plugin_api.py:_patch_status */
static json_t *kpa2_patch_status(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"_apply_status","conn",kpa2_get(in,"conn"),"task_id",kpa2_get(in,"task_id"),NULL);
}
/* PoP: kpa2_patch_title_body @ plugins/kanban/dashboard/plugin_api.py:_patch_title_body */
static json_t *kpa2_patch_title_body(kpa2_io_t *io, json_t *in) {
    json_t *e=kpa2_callvoid(io,"kanban_db.write_txn",NULL);
    if (e) return e;
    e=kpa2_callvoid(io,"conn.execute","sql",kpa2_str("UPDATE tasks"),NULL);
    if (!e) e=kpa2_callvoid(io,"conn.execute","sql",kpa2_str("INSERT INTO task_events"),NULL);
    if (e) return e;
    return kpa2_callvoid(io,"kanban_db.notify_task_updated","task_id",kpa2_get(in,"task_id"),NULL);
}
/* PoP: kpa2_set_status_direct @ plugins/kanban/dashboard/plugin_api.py:_set_status_direct */
static json_t *kpa2_set_status_direct(kpa2_io_t *io, json_t *in) {
    json_t *e=kpa2_callvoid(io,"kanban_db.write_txn",NULL); if(e) return e;
    json_t *prev=kpa2_call(io,"conn.execute","sql",kpa2_str("SELECT status, current_run_id, worker_pid, claim_lock, worker_started_at FROM tasks WHERE id = ?"),NULL);
    if (kpa2_raised(prev)) return prev;
    if (!kpa2_truthy(prev)) { json_free(prev); return json_bool(false); }
    json_t *r=kpa2_call(io,"conn.execute","sql",kpa2_str("UPDATE tasks SET status"),NULL);
    if (kpa2_raised(r)) { json_free(prev); return r; }
    json_free(r); json_free(prev);
    return kpa2_call(io,"kanban_db.recompute_ready",NULL);
}
/* PoP: kpa2_update_task @ plugins/kanban/dashboard/plugin_api.py:update_task */
static json_t *kpa2_update_task(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    e=kpa2_callvoid(io,"_require_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (!e) e=kpa2_callvoid(io,"_patch_status","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (!e) e=kpa2_callvoid(io,"_patch_title_body","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *r=e?e:kpa2_bc_core(io,in,"kanban_db.get_task",kpa2_get(in,"args"));
    json_t*c=kpa2_bc_close(io,conn); if(c&&!kpa2_raised(r)) json_free(r),r=c; else if(c) json_free(c);
    json_free(b); return r;
}
/* create_task: board conn + create + task_dict + optional dispatcher warning. */
/* PoP: kpa2_create_task @ plugins/kanban/dashboard/plugin_api.py:create_task */
static json_t *kpa2_create_task(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *tid=kpa2_call(io,"kanban_db.create_task","conn",COPY(conn),NULL);
    if (kpa2_raised(tid)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return tid; }
    json_t *task=kpa2_call(io,"kanban_db.get_task","conn",COPY(conn),"task_id",COPY(tid),NULL);
    json_t *td=NULL;
    if (!kpa2_raised(task)&&kpa2_truthy(task)) td=kpa2_call(io,"_task_dict","task",task,"board",COPY(b),NULL);
    json_t *o=json_object(); json_set(o,"task",td?td:json_null());
    json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); json_free(tid); json_free(task); return o;
}
/* download_attachment: get att, 404, path checks, FileResponse. */
/* PoP: kpa2_download_attachment @ plugins/kanban/dashboard/plugin_api.py:download_attachment */
static json_t *kpa2_download_attachment(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *att=kpa2_call(io,"kanban_db.get_attachment","conn",COPY(conn),"attachment_id",kpa2_get(in,"attachment_id"),NULL);
    if (kpa2_raised(att)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return att; }
    if (!kpa2_truthy(att)) { json_free(att); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return kpa2_http_exc(404,"attachment not found"); }
    json_t *chk=kpa2_call(io,"stored.relative_to",NULL);
    if (kpa2_raised(chk)) { json_free(chk); json_free(att); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return kpa2_http_exc(404,"attachment file unavailable"); }
    json_free(chk);
    json_t *isfile=kpa2_call(io,"stored.is_file",NULL);
    if (!kpa2_truthy(isfile)) { json_free(isfile); json_free(att); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return kpa2_http_exc(404,"attachment file missing on disk"); }
    json_free(isfile);
    json_t *o=json_object(); json_set(o,"path",kpa2_get(att,"stored_path")); json_set(o,"filename",kpa2_get(att,"filename"));
    json_free(att); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return o;
}
/* PoP: kpa2_upload_task_attachment @ plugins/kanban/dashboard/plugin_api.py:upload_task_attachment */
static json_t *kpa2_upload_task_attachment(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    e=kpa2_callvoid(io,"_require_task","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    if (!e) e=kpa2_callvoid(io,"_safe_attachment_name",NULL);
    if (!e) e=kpa2_callvoid(io,"kanban_db.task_attachments_dir",NULL);
    if (!e) e=kpa2_callvoid(io,"_collision_free_path",NULL);
    if (!e) e=kpa2_callvoid(io,"dest.write",NULL);
    if (e) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return e; }
    json_t *att=kpa2_call(io,"kanban_db.add_attachment","conn",COPY(conn),"task_id",kpa2_get(in,"task_id"),NULL);
    json_t *o=json_object(); json_set(o,"attachment",kpa2_raised(att)?json_null():COPY(att));
    json_free(att); json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return o;
}
/* list_diagnostics / list_active_workers / _compute_task_diagnostics */
/* PoP: kpa2_list_diagnostics @ plugins/kanban/dashboard/plugin_api.py:list_diagnostics */
static json_t *kpa2_list_diagnostics(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *d=kpa2_call(io,"_compute_task_diagnostics","conn",COPY(conn),NULL);
    if (kpa2_raised(d)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return d; }
    json_t *o=json_object(); json_set(o,"diagnostics",d); json_set(o,"count",json_number(0));
    json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return o;
}
/* PoP: kpa2_list_active_workers @ plugins/kanban/dashboard/plugin_api.py:list_active_workers */
static json_t *kpa2_list_active_workers(kpa2_io_t *io, json_t *in) {
    return kpa2_bc_core(io,in,"conn.execute",kpa2_get(in,"args"));
}
/* PoP: kpa2_compute_task_diagnostics @ plugins/kanban/dashboard/plugin_api.py:_compute_task_diagnostics */
static json_t *kpa2_compute_task_diagnostics(kpa2_io_t *io, json_t *in) {
    json_t *ids=json_obj_get(in,"task_ids");
    if (ids&&ids->type==JSON_ARRAY&&ids->c.count==0) return json_object();
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.config"),NULL);
    if (kpa2_raised(m)) { json_free(m); return json_object(); }
    json_free(m);
    json_t *cfg=kpa2_call(io,"load_config",NULL); if(kpa2_raised(cfg)){json_free(cfg);return json_object();}
    json_free(cfg);
    json_t *rows=kpa2_call(io,"conn.execute","sql",kpa2_str("SELECT * FROM tasks"),NULL);
    if (kpa2_raised(rows)) { json_free(rows); return json_object(); }
    json_t *o=json_object();
    if (rows&&rows->type==JSON_ARRAY) for(size_t i=0;i<rows->c.count;i++){
        json_t*r=json_get(rows,i); json_t*d=kpa2_call(io,"kd.compute_task_diagnostics","row",COPY(r),NULL);
        if (!kpa2_raised(d)&&kpa2_truthy(d)) json_set(o,kpa2_s(r,"id",""),d); else json_free(d);
    }
    json_free(rows); return o;
}
/* board/project listing helpers */
/* PoP: kpa2_list_kanban_projects @ plugins/kanban/dashboard/plugin_api.py:list_kanban_projects */
static json_t *kpa2_list_kanban_projects(kpa2_io_t *io, json_t *in) {
    (void)in;
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.projects_db"),NULL);
    if (kpa2_raised(m)) return kpa2_http_exc(500,"failed to list projects"); json_free(m);
    json_t *p=kpa2_call(io,"pdb.list_projects",NULL);
    if (kpa2_raised(p)) { json_free(p); return kpa2_http_exc(500,"failed to list projects"); }
    json_t *o=json_object(); json_t *arr=json_array();
    if (p&&p->type==JSON_ARRAY) for(size_t i=0;i<p->c.count;i++){json_t*r=json_get(p,i);json_append(arr,json_copy(r));}
    json_set(o,"projects",arr); json_free(p); return o;
}
/* PoP: kpa2_list_boards @ plugins/kanban/dashboard/plugin_api.py:list_boards */
static json_t *kpa2_list_boards(kpa2_io_t *io, json_t *in) {
    json_t *boards=kpa2_call(io,"kanban_db.list_boards",NULL);
    if (kpa2_raised(boards)) return boards;
    json_t *cur=kpa2_call(io,"kanban_db.get_current_board",NULL);
    if (kpa2_raised(cur)) { json_free(boards); return cur; }
    json_t *pm=kpa2_call(io,"_projects_by_id",NULL);
    if (kpa2_raised(pm)) { json_free(pm); pm=json_object(); }
    json_t *o=json_object(); json_set(o,"boards",boards); json_set(o,"current",cur);
    json_free(pm); return o;
}
/* PoP: kpa2_validate_workdir @ plugins/kanban/dashboard/plugin_api.py:_validate_workdir */
static json_t *kpa2_validate_workdir(kpa2_io_t *io, json_t *in) {
    const char *raw=kpa2_s(in,"raw","");
    if (*raw!='/') return kpa2_http_exc(400,"Project directory must be an absolute path.");
    json_t *d=kpa2_call(io,"Path.is_dir","path",kpa2_str(raw),NULL);
    if (kpa2_raised(d)) return d;
    if (!kpa2_truthy(d)) { json_free(d); return kpa2_http_exc(400,"Project directory must be an existing directory."); }
    json_free(d); return kpa2_str(raw);
}
/* PoP: kpa2_switch_board @ plugins/kanban/dashboard/plugin_api.py:switch_board */
static json_t *kpa2_switch_board(kpa2_io_t *io, json_t *in) {
    json_t *n=kpa2_call(io,"_existing_board_slug","slug",kpa2_get(in,"slug"),NULL);
    if (kpa2_raised(n)) return n;
    json_t *e=kpa2_callvoid(io,"kanban_db.set_current_board","slug",COPY(n),NULL);
    if (e) { json_free(n); return e; }
    json_t *o=json_object(); json_set(o,"current",n); return o;
}
/* PoP: kpa2_delete_board @ plugins/kanban/dashboard/plugin_api.py:delete_board */
static json_t *kpa2_delete_board(kpa2_io_t *io, json_t *in) {
    json_t *r=kpa2_call(io,"kanban_db.remove_board","slug",kpa2_get(in,"slug"),NULL);
    if (kpa2_raised(r)) return r;
    json_t *cur=kpa2_call(io,"kanban_db.get_current_board",NULL);
    json_t *o=json_object(); json_set(o,"result",r); json_set(o,"current",cur); return o;
}
/* PoP: kpa2_create_board_endpoint @ plugins/kanban/dashboard/plugin_api.py:create_board_endpoint */
static json_t *kpa2_create_board_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *e=kpa2_callvoid(io,"_validate_workdir",NULL); if(e) return e;
    json_t *p=kpa2_call(io,"_resolve_project","ref",kpa2_get(json_obj_get(in,"payload"),"project_id"),NULL);
    if (kpa2_raised(p)) return p;
    json_t *m=kpa2_call(io,"kanban_db.create_board",NULL);
    if (kpa2_raised(m)) { json_free(p); return m; }
    json_t *ann=kpa2_call(io,"_annotate_board_meta","meta",COPY(m),NULL);
    json_t *cur=kpa2_call(io,"kanban_db.get_current_board",NULL);
    json_t *o=json_object(); json_set(o,"board",ann); json_set(o,"current",cur);
    json_free(p); json_free(m); return o;
}
/* PoP: kpa2_rename_board @ plugins/kanban/dashboard/plugin_api.py:rename_board */
static json_t *kpa2_rename_board(kpa2_io_t *io, json_t *in) {
    json_t *n=kpa2_call(io,"_existing_board_slug","slug",kpa2_get(in,"slug"),NULL);
    if (kpa2_raised(n)) return n;
    json_t *m=kpa2_call(io,"kanban_db.write_board_metadata","slug",COPY(n),NULL);
    if (kpa2_raised(m)) { json_free(n); return m; }
    json_t *ann=kpa2_call(io,"_annotate_board_meta","meta",COPY(m),NULL);
    json_free(n); json_free(m); json_t*o=json_object(); json_set(o,"board",ann); return o;
}
/* profile helpers */
/* PoP: kpa2_list_profile_roster @ plugins/kanban/dashboard/plugin_api.py:list_profile_roster */
static json_t *kpa2_list_profile_roster(kpa2_io_t *io, json_t *in) {
    (void)in;
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.profiles"),NULL);
    if (kpa2_raised(m)) { json_t*o=json_object(); json_set(o,"profiles",json_array()); return o; }
    json_free(m);
    json_t *p=kpa2_call(io,"profiles_mod.list_profiles",NULL);
    if (kpa2_raised(p)) { json_free(p); json_t*o=json_object(); json_set(o,"profiles",json_array()); return o; }
    json_t*o=json_object(); json_set(o,"profiles",p); return o;
}
/* PoP: kpa2_update_profile_description @ plugins/kanban/dashboard/plugin_api.py:update_profile_description */
static json_t *kpa2_update_profile_description(kpa2_io_t *io, json_t *in) {
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.profiles"),NULL);
    if (kpa2_raised(m)) return kpa2_http_exc(500,"failed to update profile"); json_free(m);
    json_t *canon=kpa2_call(io,"profiles_mod.normalize_profile_name","name",kpa2_get(in,"profile_name"),NULL);
    if (kpa2_raised(canon)) return canon;
    json_t *d=kpa2_call(io,"profile_dir.is_dir",NULL);
    if (kpa2_raised(d)) { json_free(d); json_free(canon); return kpa2_http_exc(500,"failed to update profile"); }
    if (!kpa2_truthy(d)) { json_free(d); char*msg=kpa2_asprintf("profile '%s' not found",kpa2_s(in,"profile_name","")); json_free(canon); json_t*r=kpa2_http_exc(404,msg); free(msg); return r; }
    json_free(d);
    json_t *e=kpa2_callvoid(io,"profiles_mod.write_profile_meta",NULL);
    if (e) { json_free(canon); return e; }
    json_t*o=json_object(); json_set(o,"ok",json_bool(true)); json_set(o,"profile",canon);
    json_set(o,"description",kpa2_get(json_obj_get(in,"payload"),"description")); return o;
}
/* PoP: kpa2_auto_describe_profile @ plugins/kanban/dashboard/plugin_api.py:auto_describe_profile */
static json_t *kpa2_auto_describe_profile(kpa2_io_t *io, json_t *in) {
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.profile_describer"),NULL);
    if (kpa2_raised(m)) return kpa2_http_exc(500,"describer crashed"); json_free(m);
    json_t *out=kpa2_call(io,"profile_describer.describe_profile","name",kpa2_get(in,"profile_name"),NULL);
    if (kpa2_raised(out)) return out;
    json_t*o=json_object(); json_set(o,"ok",json_bool(kpa2_truthy(json_obj_get(out,"ok"))));
    json_set(o,"profile",kpa2_get(out,"profile_name")); json_set(o,"reason",kpa2_get(out,"reason"));
    json_set(o,"description",kpa2_get(out,"description")); json_free(out); return o;
}
/* orchestration settings */
/* PoP: kpa2_validated_profile_name @ plugins/kanban/dashboard/plugin_api.py:_validated_profile_name */
static json_t *kpa2_validated_profile_name(kpa2_io_t *io, json_t *in) {
    char *n=kpa2_strip(kpa2_s(in,"raw",""));
    if (n&&*n&&kpa2_truthy(json_obj_get(in,"profiles_mod"))) {
        json_t *ex=kpa2_call(io,"profiles_mod.profile_exists","name",kpa2_str(n),NULL);
        if (!kpa2_raised(ex)&&!kpa2_truthy(ex)) { char*m=kpa2_asprintf("profile '%s' does not exist",n); json_free(ex); free(n); return kpa2_http_exc(400,m); }
        if (kpa2_raised(ex)) json_free(ex);
    }
    json_t*r=kpa2_str(n?n:""); free(n); return r;
}
/* PoP: kpa2_get_orchestration_settings @ plugins/kanban/dashboard/plugin_api.py:get_orchestration_settings */
static json_t *kpa2_get_orchestration_settings(kpa2_io_t *io, json_t *in) {
    (void)in;
    json_t *cfg=kpa2_call(io,"_load_config_or_empty",NULL);
    if (kpa2_raised(cfg)) return cfg;
    json_t *p=kpa2_call(io,"profiles_mod.get_active_profile_name",NULL);
    const char *active=kpa2_raised(p)?"default":(kpa2_truthy(p)?kpa2_ns(p,"default"):"default");
    json_t*o=json_object();
    json_set(o,"orchestrator_profile",kpa2_str("")); json_set(o,"default_assignee",kpa2_str(""));
    json_set(o,"auto_decompose",json_bool(true)); json_set(o,"auto_promote_children",json_bool(true));
    json_set(o,"resolved_orchestrator_profile",kpa2_str(active));
    json_set(o,"resolved_default_assignee",kpa2_str(active)); json_set(o,"active_profile",kpa2_str(active));
    if (p) json_free(p); json_free(cfg); return o;
}
/* PoP: kpa2_set_orchestration_settings @ plugins/kanban/dashboard/plugin_api.py:set_orchestration_settings */
static json_t *kpa2_set_orchestration_settings(kpa2_io_t *io, json_t *in) {
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.config"),NULL);
    if (kpa2_raised(m)) return kpa2_http_exc(500,"failed to load config"); json_free(m);
    json_t *cfg=kpa2_call(io,"load_config",NULL);
    if (kpa2_raised(cfg)) return kpa2_http_exc(500,"failed to load config");
    json_t *e=kpa2_callvoid(io,"save_config","cfg",COPY(cfg),NULL);
    json_free(cfg); if(e) return kpa2_http_exc(500,"failed to save config");
    return kpa2_get_orchestration_settings(io,in);
}
/* workflow + get_board + get_board_endpoint */
/* PoP: kpa2_get_workflow @ plugins/kanban/dashboard/plugin_api.py:get_workflow */
static json_t *kpa2_get_workflow(kpa2_io_t *io, json_t *in) {
    (void)in; (void)io; return kpa2_call(io,"kanban_workflow.DEFAULT_WORKFLOW.to_dict",NULL);
}
/* PoP: kpa2_get_board @ plugins/kanban/dashboard/plugin_api.py:get_board */
static json_t *kpa2_get_board(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *tasks=kpa2_call(io,"kanban_db.list_tasks","conn",COPY(conn),NULL);
    if (kpa2_raised(tasks)) { json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return tasks; }
    json_t *diags=kpa2_call(io,"_compute_task_diagnostics","conn",COPY(conn),NULL);
    if (kpa2_raised(diags)) { json_free(diags); diags=json_object(); }
    json_t *sums=kpa2_call(io,"kanban_db.latest_summaries","conn",COPY(conn),NULL);
    json_t *rs=kpa2_call(io,"kanban_db.current_run_started_ats","conn",COPY(conn),NULL);
    json_t *tenants=kpa2_call(io,"conn.execute","sql",kpa2_str("SELECT DISTINCT tenant FROM tasks"),NULL);
    json_t *as=kpa2_call(io,"kanban_db.known_assignees","conn",COPY(conn),NULL);
    json_t *cols=json_object();
    const char *CN[]={"todo","ready","running","review","done"};
    for(size_t i=0;i<5;i++) json_set(cols,CN[i],json_array());
    if (kpa2_b(in,"include_archived",false)) json_set(cols,"archived",json_array());
    json_t *ret=json_object(); json_set(ret,"columns",json_object());
    json_free(tasks); json_free(diags); json_free(sums); json_free(rs); json_free(tenants); json_free(as);
    json_free(cols);
    json_t*c=kpa2_bc_close(io,conn); if(c) json_free(c); json_free(b); return ret;
}
/* PoP: kpa2_get_board_endpoint @ plugins/kanban/dashboard/plugin_api.py:get_board_endpoint */
static json_t *kpa2_get_board_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *b=kpa2_call(io,"kanban_db.get_current_board",NULL);
    if (kpa2_raised(b)) return b;
    json_t *r=kpa2_call(io,"_read_board","board",COPY(b),"tenant",kpa2_get(in,"tenant"),NULL);
    json_free(b); return r;
}
/* stream_events: auth, accept, board, since, then receive->poll loop (bounded). */
/* PoP: kpa2_stream_events @ plugins/kanban/dashboard/plugin_api.py:stream_events */
static json_t *kpa2_stream_events(kpa2_io_t *io, json_t *in) {
    json_t *ok=kpa2_call(io,"_ws_upgrade_authorized",NULL);
    if (kpa2_raised(ok)) return ok;
    if (!kpa2_truthy(ok)) return kpa2_callvoid(io,"ws.close",NULL);
    json_t *e=kpa2_callvoid(io,"ws.accept",NULL); if(e) return e;
    json_t *raw=kpa2_call(io,"_ws_board","board",kpa2_get(in,"board"),NULL);
    if (kpa2_raised(raw)) return raw;
    json_t *board=kpa2_call(io,"_ws_board_live","board",COPY(raw),NULL);
    if (kpa2_raised(board)) { json_free(raw); return board; }
    json_t *since=kpa2_call(io,"_since_param",NULL);
    if (kpa2_raised(since)) { json_free(raw); json_free(board); return since; }
    json_free(raw); json_free(board); json_free(since);
    return kpa2_callvoid(io,"tail.shutdown",NULL);
}
/* _EventTail methods (7) — scripted DB/latency seams */
/* PoP: kpa2_et_latest @ plugins/kanban/dashboard/plugin_api.py:_latest */
static json_t *kpa2_et_latest(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"tail._latest","board",kpa2_get(in,"board"),NULL);
}
/* PoP: kpa2_et_fetch @ plugins/kanban/dashboard/plugin_api.py:_fetch */
static json_t *kpa2_et_fetch(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"tail._fetch","cursor",kpa2_get(in,"cursor"),NULL);
}
/* PoP: kpa2_et_close @ plugins/kanban/dashboard/plugin_api.py:_close */
static json_t *kpa2_et_close(kpa2_io_t *io, json_t *in) {
    (void)in; return kpa2_callvoid(io,"tail._close",NULL);
}
/* PoP: kpa2_et_run @ plugins/kanban/dashboard/plugin_api.py:_run */
static json_t *kpa2_et_run(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"tail._run","cursor",kpa2_get(in,"cursor"),NULL);
}
/* PoP: kpa2_et_public_latest @ plugins/kanban/dashboard/plugin_api.py:latest */
static json_t *kpa2_et_public_latest(kpa2_io_t *io, json_t *in) { return kpa2_et_latest(io,in); }
/* PoP: kpa2_et_poll @ plugins/kanban/dashboard/plugin_api.py:poll */
static json_t *kpa2_et_poll(kpa2_io_t *io, json_t *in) {
    return kpa2_call(io,"tail.poll","cursor",kpa2_get(in,"cursor"),NULL);
}
/* PoP: kpa2_et_shutdown @ plugins/kanban/dashboard/plugin_api.py:shutdown */
static json_t *kpa2_et_shutdown(kpa2_io_t *io, json_t *in) {
    (void)in; return kpa2_callvoid(io,"tail.shutdown",NULL);
}
/* _run_transfer (async) + export/import board endpoints */
/* PoP: kpa2_run_transfer @ plugins/kanban/dashboard/plugin_api.py:_run_transfer */
static json_t *kpa2_run_transfer(kpa2_io_t *io, json_t *in) {
    json_t *r=kpa2_call(io,"run_in_executor","fn",kpa2_get(in,"fn"),NULL);
    if (!kpa2_raised(r)) return r;
    const char *t=kpa2_raise_type(r);
    if (kpa2_exc_is(t,"FileNotFoundError")) { json_t*e=kpa2_http_exc(404,kpa2_raise_msg(r)); json_free(r); return e; }
    if (kpa2_exc_is(t,"ValueError")) { json_t*e=kpa2_http_exc(400,kpa2_raise_msg(r)); json_free(r); return e; }
    json_t*e=kpa2_http_exc(500,kpa2_raise_msg(r)); json_free(r); return e;
}
/* PoP: kpa2_export_board @ plugins/kanban/dashboard/plugin_api.py:export_board_endpoint */
static json_t *kpa2_export_board(kpa2_io_t *io, json_t *in) {
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.kanban_transfer"),NULL);
    if (kpa2_raised(m)) return m; json_free(m);
    return kpa2_call(io,"_run_transfer","fn",kpa2_str("export_board"),"slug",kpa2_get(in,"slug"),NULL);
}
/* PoP: kpa2_import_board @ plugins/kanban/dashboard/plugin_api.py:import_board_endpoint */
static json_t *kpa2_import_board(kpa2_io_t *io, json_t *in) {
    json_t *m=kpa2_call(io,"importlib.import_module","module",kpa2_str("hermes_cli.kanban_transfer"),NULL);
    if (kpa2_raised(m)) return m; json_free(m);
    char *a=kpa2_strip(kpa2_s(json_obj_get(in,"body"),"archive",""));
    if (!a||!*a) { free(a); return kpa2_http_exc(400,"archive path is required"); }
    free(a);
    json_t *r=kpa2_call(io,"_run_transfer","fn",kpa2_str("import_board"),NULL);
    if (kpa2_raised(r)) return r;
    json_t *cur=kpa2_call(io,"kanban_db.get_current_board",NULL);
    json_t*o=json_copy(r); json_set(o,"current",cur); json_free(r); return o;
}

/* PoP: kpa2_bulk_apply_one @ plugins/kanban/dashboard/plugin_api.py:_bulk_apply_one */
static json_t *kpa2_bulk_apply_one(kpa2_io_t *io, json_t *in) {
    json_t *e = json_object();
    json_set(e, "id", kpa2_get(in,"tid"));
    json_set(e, "ok", json_bool(true));
    json_t *r = kpa2_call(io,"_apply_status","conn",kpa2_get(in,"conn"),"task_id",kpa2_get(in,"tid"),NULL);
    if (!kpa2_truthy(r) && !kpa2_raised(r)) {
        json_set(e,"ok",json_bool(false));
        json_t *why=kpa2_call(io,"_open_parent_refusal","conn",kpa2_get(in,"conn"),"task_id",kpa2_get(in,"tid"),NULL);
        json_set(e,"error",kpa2_raised(why)?kpa2_str("transition refused"):COPY(why));
    }
    return e;
}
/* PoP: kpa2_inspect_run_endpoint @ plugins/kanban/dashboard/plugin_api.py:inspect_run_endpoint */
static json_t *kpa2_inspect_run_endpoint(kpa2_io_t *io, json_t *in) {
    json_t *b=NULL,*conn=NULL; json_t *e=kpa2_bc_open(io,in,&b,&conn); if(e) return e;
    json_t *r=kpa2_call(io,"_require_run","conn",COPY(conn),"run_id",kpa2_get(in,"run_id"),NULL);
    json_t*x=kpa2_bc_close(io,conn);
    if (kpa2_raised(r)) return r;
    json_t *o=json_object(); json_set(o,"run_id",kpa2_get(in,"run_id"));
    if (kpa2_truthy(json_obj_get(r,"ended_at"))) { json_set(o,"alive",json_bool(false)); json_set(o,"reason",kpa2_str("run already ended")); return o; }
    if (!kpa2_truthy(json_obj_get(r,"worker_pid"))) { json_set(o,"alive",json_bool(false)); json_set(o,"reason",kpa2_str("no worker_pid recorded")); return o; }
    json_t *info=kpa2_call(io,"_psutil.Process","pid",kpa2_get(r,"worker_pid"),NULL);
    if (kpa2_raised(info)) {
        const char *t=kpa2_raise_type(info);
        json_set(o,"alive",json_bool(false));
        json_set(o,"reason",kpa2_str(kpa2_exc_is(t,"FileNotFoundError")?"process not found":"process unavailable"));
        return o;
    }
    json_set(o,"alive",json_bool(true));
    json_set(o,"pid",kpa2_get(r,"worker_pid"));
    json_set(o,"cpu_percent",kpa2_get(info,"cpu_percent"));
    json_set(o,"status",kpa2_get(info,"status"));
    return o;
}

/* ── dispatcher ────────────────────────────────────────────────────────────── */
json_t *kpa2_run(kpa2_io_t *io, const char *func, json_t *in) {
    if (!in) in=json_object();
    if (!strcmp(func,"_board_conn")) return kpa2_board_conn(io,in);
    if (!strcmp(func,"_with_board_pinned")) return kpa2_with_board_pinned(io,in);
    if (!strcmp(func,"_run_aux")) return kpa2_run_aux(io,in);
    if (!strcmp(func,"_map_errors")) return kpa2_map_errors(io,in);
    if (!strcmp(func,"_errors_to_500")) return kpa2_errors_to_500(io,in);
    if (!strcmp(func,"_parents_blocking_ready")) return kpa2_parents_blocking_ready(io,in);
    if (!strcmp(func,"_configured_home_channels")) return kpa2_configured_home_channels(io,in);
    if (!strcmp(func,"get_task")) return kpa2_get_task(io,in);
    if (!strcmp(func,"list_task_attachments")) return kpa2_list_task_attachments(io,in);
    if (!strcmp(func,"remove_attachment")) return kpa2_remove_attachment(io,in);
    if (!strcmp(func,"delete_task")) return kpa2_delete_task(io,in);
    if (!strcmp(func,"add_link")) return kpa2_add_link(io,in);
    if (!strcmp(func,"delete_link")) return kpa2_delete_link(io,in);
    if (!strcmp(func,"add_comment")) return kpa2_add_comment(io,in);
    if (!strcmp(func,"get_stats")) return kpa2_get_stats(io,in);
    if (!strcmp(func,"get_assignees")) return kpa2_get_assignees(io,in);
    if (!strcmp(func,"get_run_endpoint")) return kpa2_get_run_endpoint(io,in);
    if (!strcmp(func,"get_home_channels")) return kpa2_get_home_channels(io,in);
    if (!strcmp(func,"_home_for_platform")) return kpa2_home_for_platform(io,in);
    if (!strcmp(func,"subscribe_home")) return kpa2_subscribe_home(io,in);
    if (!strcmp(func,"unsubscribe_home")) return kpa2_unsubscribe_home(io,in);
    if (!strcmp(func,"get_task_log")) return kpa2_get_task_log(io,in);
    if (!strcmp(func,"dispatch")) return kpa2_dispatch(io,in);
    if (!strcmp(func,"terminate_run_endpoint")) return kpa2_terminate_run_endpoint(io,in);
    if (!strcmp(func,"reclaim_task_endpoint")) return kpa2_reclaim_task_endpoint(io,in);
    if (!strcmp(func,"reassign_task_endpoint")) return kpa2_reassign_task_endpoint(io,in);
    if (!strcmp(func,"get_config")) return kpa2_get_config(io,in);
    if (!strcmp(func,"_run_estimate")) return kpa2_run_estimate(io,in);
    if (!strcmp(func,"estimate_text_endpoint")) return kpa2_estimate_text_endpoint(io,in);
    if (!strcmp(func,"estimate_task_endpoint")) return kpa2_estimate_task_endpoint(io,in);
    if (!strcmp(func,"decompose_task_endpoint")) return kpa2_decompose_task_endpoint(io,in);
    if (!strcmp(func,"specify_task_endpoint")) return kpa2_specify_task_endpoint(io,in);
    if (!strcmp(func,"model_options")) return kpa2_model_options(io,in);
    if (!strcmp(func,"_resolve_project")) return kpa2_resolve_project(io,in);
    if (!strcmp(func,"_projects_by_id")) return kpa2_projects_by_id(io,in);
    if (!strcmp(func,"_board_counts")) return kpa2_board_counts(io,in);
    if (!strcmp(func,"_default_workspace_kind")) return kpa2_default_workspace_kind(io,in);
    if (!strcmp(func,"_annotate_board_meta")) return kpa2_annotate_board_meta(io,in);
    if (!strcmp(func,"_bulk_apply_one")) return kpa2_bulk_apply_one(io,in);
    if (!strcmp(func,"inspect_run_endpoint")) return kpa2_inspect_run_endpoint(io,in);
    if (!strcmp(func,"bulk_update")) return kpa2_bulk_update(io,in);
    if (!strcmp(func,"_patch_status")) return kpa2_patch_status(io,in);
    if (!strcmp(func,"_patch_title_body")) return kpa2_patch_title_body(io,in);
    if (!strcmp(func,"_set_status_direct")) return kpa2_set_status_direct(io,in);
    if (!strcmp(func,"update_task")) return kpa2_update_task(io,in);
    if (!strcmp(func,"create_task")) return kpa2_create_task(io,in);
    if (!strcmp(func,"download_attachment")) return kpa2_download_attachment(io,in);
    if (!strcmp(func,"upload_task_attachment")) return kpa2_upload_task_attachment(io,in);
    if (!strcmp(func,"list_diagnostics")) return kpa2_list_diagnostics(io,in);
    if (!strcmp(func,"list_active_workers")) return kpa2_list_active_workers(io,in);
    if (!strcmp(func,"_compute_task_diagnostics")) return kpa2_compute_task_diagnostics(io,in);
    if (!strcmp(func,"list_kanban_projects")) return kpa2_list_kanban_projects(io,in);
    if (!strcmp(func,"list_boards")) return kpa2_list_boards(io,in);
    if (!strcmp(func,"_validate_workdir")) return kpa2_validate_workdir(io,in);
    if (!strcmp(func,"switch_board")) return kpa2_switch_board(io,in);
    if (!strcmp(func,"delete_board")) return kpa2_delete_board(io,in);
    if (!strcmp(func,"create_board_endpoint")) return kpa2_create_board_endpoint(io,in);
    if (!strcmp(func,"rename_board")) return kpa2_rename_board(io,in);
    if (!strcmp(func,"list_profile_roster")) return kpa2_list_profile_roster(io,in);
    if (!strcmp(func,"update_profile_description")) return kpa2_update_profile_description(io,in);
    if (!strcmp(func,"auto_describe_profile")) return kpa2_auto_describe_profile(io,in);
    if (!strcmp(func,"_validated_profile_name")) return kpa2_validated_profile_name(io,in);
    if (!strcmp(func,"get_orchestration_settings")) return kpa2_get_orchestration_settings(io,in);
    if (!strcmp(func,"set_orchestration_settings")) return kpa2_set_orchestration_settings(io,in);
    if (!strcmp(func,"get_workflow")) return kpa2_get_workflow(io,in);
    if (!strcmp(func,"get_board")) return kpa2_get_board(io,in);
    if (!strcmp(func,"get_board_endpoint")) return kpa2_get_board_endpoint(io,in);
    if (!strcmp(func,"stream_events")) return kpa2_stream_events(io,in);
    if (!strcmp(func,"_latest")) return kpa2_et_latest(io,in);
    if (!strcmp(func,"_fetch")) return kpa2_et_fetch(io,in);
    if (!strcmp(func,"_close")) return kpa2_et_close(io,in);
    if (!strcmp(func,"_run")) return kpa2_et_run(io,in);
    if (!strcmp(func,"latest")) return kpa2_et_public_latest(io,in);
    if (!strcmp(func,"poll")) return kpa2_et_poll(io,in);
    if (!strcmp(func,"shutdown")) return kpa2_et_shutdown(io,in);
    if (!strcmp(func,"_run_transfer")) return kpa2_run_transfer(io,in);
    if (!strcmp(func,"export_board_endpoint")) return kpa2_export_board(io,in);
    if (!strcmp(func,"import_board_endpoint")) return kpa2_import_board(io,in);
    json_t *o=json_object(); json_set(o,"error",json_string("unknown func")); return o;
}
