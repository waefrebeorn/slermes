/*
 * store_rules_flow2.c - FLOW cores of the UNCLAIMED remainder of the
 * conversation-store turn-policy helpers. rules.py itself has all 11 defs
 * claimed by store_rules_flow.c; this module ports the pure turn-policy leaves
 * defined in session_rules/turns.py that remain unclaimed by any PoP (verified
 * CRLF-aware grep): _field, _part_text, _with_part_text,
 * _content_text_for_contains, is_content_blank, drop_stale_api_content,
 * _display_metadata_str, withheld_prompt_text, typed_prompt_content,
 * retryable_user_text -- 10 defs.
 *
 * Decision sequences only: external collaborators (if any) enter through the
 * universal seam sr2_call; leaves call each other DIRECTLY (no seam):
 * _content_text_for_contains -> _part_text; withheld_prompt_text ->
 * _display_metadata_str; typed_prompt_content -> withheld_prompt_text.
 * The seeded constants are WITHHELD_PROMPT and PRE_LLM_BLOCK_WITHHELD_PROMPT.
 * Full builtin exception hierarchy is carried for faithful error projection.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sr2_-prefixed.
 *
 * Representation contract matches store_rules_flow.c: NULL json_t* == Python
 * None; a raise is {"__raise__": cls, "message": str(exc)}; a Python float is
 * {"__float__":true,"v":n}, a tuple {"__tuple__":true,"v":[...]}. Sets fold to
 * sorted arrays; dict key order is irrelevant to canon. Mutating functions
 * (drop_stale_api_content) mutate their dict arg in place; the harness echoes
 * the mutated params under "state" keyed by parameter name.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct sr2_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sr2_ctx_t;

/* -- constants (seeded, not seamed) -- */
#define SR2_MESSAGE_UID            "message_uid"
#define SR2_DATA_USE               "data_use"
#define SR2_TYPED_ASK              "typed_ask"
#define SR2_ABSORBED_MESSAGE_UIDS  "_absorbed_message_uids"
#define SR2_EPOCH_MIN              0.0
#define SR2_EPOCH_MAX              4200000000.0
#define SR2_SYS_PLATFORM           "win32"
#define SR2_ERRNO_EINVAL           22
static const char *SR2_IMAGE_PART_TYPES[] = { "image_url", "input_image", "image" };
#define SR2_IMAGE_PART_TYPES_N 3

/* -- basic type helpers -- */
static bool sr2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sr2_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}
static bool sr2_is_tuple_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__tuple__", false);
}
static bool sr2_is_dt_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__dt__", false);
}
static bool sr2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sr2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sr2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool sr2_is_int(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool sr2_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sr2_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sr2_is_float_tag(v) &&
           !sr2_is_tuple_tag(v) && !sr2_is_dt_tag(v);
}
static bool sr2_is_numlike(const json_t *v) {
    return sr2_is_int(v) || sr2_is_bool(v) || sr2_is_float_tag(v);
}
static double sr2_num(const json_t *v) {
    if (sr2_is_float_tag(v)) return json_get_num(v, "v", 0);
    if (sr2_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static const char *sr2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool sr2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (sr2_is_float_tag(v)) return json_get_num(v, "v", 0) != 0.0;
    if (sr2_is_tuple_tag(v)) {
        const json_t *it = json_obj_get(v, "v");
        return it && it->type == JSON_ARRAY && it->c.count > 0;
    }
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const json_t *sr2_seq_items(const json_t *v) {
    if (sr2_is_tuple_tag(v)) return json_obj_get(v, "v");
    return v;
}
static bool sr2_is_seq(const json_t *v) { return sr2_is_list(v) || sr2_is_tuple_tag(v); }
static size_t sr2_seq_len(const json_t *v) {
    const json_t *it = sr2_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? it->c.count : 0;
}
static const json_t *sr2_seq_get(const json_t *v, size_t i) {
    const json_t *it = sr2_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? json_get(it, i) : NULL;
}
/* Python type(x).__name__ on the representation (domain-restricted). */
static const char *sr2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (sr2_is_float_tag(v)) return "float";
    if (sr2_is_tuple_tag(v)) return "tuple";
    if (sr2_is_dt_tag(v)) return "datetime.datetime";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Python == on the representation (numeric cross-type, deep list/dict;
 * list != tuple). */
static bool sr2_eq(const json_t *a, const json_t *b) {
    bool an = sr2_is_numlike(a), bn = sr2_is_numlike(b);
    if (an || bn) return an && bn && sr2_num(a) == sr2_num(b);
    if (sr2_is_null(a) || sr2_is_null(b)) return sr2_is_null(a) && sr2_is_null(b);
    bool as = sr2_is_str(a), bs = sr2_is_str(b);
    if (as || bs) return as && bs && strcmp(a->str_val, b->str_val) == 0;
    bool aq = sr2_is_seq(a), bq = sr2_is_seq(b);
    if (aq || bq) {
        if (!aq || !bq) return false;
        if (sr2_is_tuple_tag(a) != sr2_is_tuple_tag(b)) return false;
        size_t n = sr2_seq_len(a);
        if (n != sr2_seq_len(b)) return false;
        for (size_t i = 0; i < n; i++)
            if (!sr2_eq(sr2_seq_get(a, i), sr2_seq_get(b, i))) return false;
        return true;
    }
    if (sr2_is_dict(a) && sr2_is_dict(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !sr2_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}
/* x == "<literal>" for a str x (non-str x is never equal to a str). */
static bool sr2_eq_str(const json_t *v, const char *lit) {
    return sr2_is_str(v) && v->str_val && strcmp(v->str_val, lit) == 0;
}

/* -- exceptions -- */
static json_t *sr2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *sr2_err_fmt(const char *type, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return sr2_err(type, buf);
}

/* FULL builtin exception hierarchy (py3.14). Unknown scripted names parent to
 * Exception, mirroring the oracle's type(kind, (Exception,), {}) fallback;
 * SystemExit/KeyboardInterrupt/GeneratorExit parent to BaseException ONLY so
 * they pass through every except-Exception clause. */
typedef struct { const char *name; const char *parent; } sr2_exc_ent_t;
static const sr2_exc_ent_t SR2_EXC_TABLE[] = {
    { "BaseException", NULL },
    { "Exception", "BaseException" },
    { "TypeError", "Exception" },
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
    { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" },
    { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" },
    { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" },
    { "TabError", "IndentationError" },
    { "SystemError", "Exception" },
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
    { "EncodingWarning", "Warning" },
    { "ResourceWarning", "Warning" },
    { "ExceptionGroup", "Exception" },
    { "BaseExceptionGroup", "BaseException" },
    { "GeneratorExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "SystemExit", "BaseException" },
};
#define SR2_EXC_TABLE_N (sizeof(SR2_EXC_TABLE) / sizeof(SR2_EXC_TABLE[0]))

static const char *sr2_exc_parent(const char *cls) {
    for (size_t i = 0; i < SR2_EXC_TABLE_N; i++)
        if (strcmp(SR2_EXC_TABLE[i].name, cls) == 0) return SR2_EXC_TABLE[i].parent;
    return NULL;
}
/* isinstance(exc, target) via the parent chain. */
static bool sr2_exc_is_a(const char *cls, const char *target) {
    if (!cls || !target) return false;
    char cur[128];
    snprintf(cur, sizeof cur, "%s", cls);
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cur, target) == 0) return true;
        const char *parent = sr2_exc_parent(cur);
        if (!parent) {
            if (strcmp(cur, "BaseException") == 0) return false;
            /* unknown scripted class: the oracle parents it to Exception */
            parent = (strcmp(cur, "Exception") == 0) ? "BaseException" : "Exception";
        }
        snprintf(cur, sizeof cur, "%s", parent);
    }
    return false;
}
static const char *sr2_raise_cls(const json_t *e) {
    return json_get_str(e, "__raise__", "RuntimeError");
}
/* Materialize a scripted raise spec exactly as the oracle make_exc does:
 * OSError-family with an errno formats "[Errno N] msg" and carries errno.
 * CPython's OSError.__new__ auto-maps a bare OSError(errno, msg) to the
 * errno's subclass (probed on the quarry host, py3.14 Windows); subclasses
 * are never remapped. The table is host-specific like CPython's own. */
typedef struct { int en; const char *cls; } sr2_errno_map_t;
static const sr2_errno_map_t SR2_ERRNO_MAP[] = {
    { 1, "PermissionError" }, { 2, "FileNotFoundError" },
    { 3, "ProcessLookupError" }, { 4, "InterruptedError" },
    { 10, "ChildProcessError" }, { 11, "BlockingIOError" },
    { 13, "PermissionError" }, { 17, "FileExistsError" },
    { 20, "NotADirectoryError" }, { 21, "IsADirectoryError" },
    { 32, "BrokenPipeError" },
#ifdef _WIN32
    { 138, "TimeoutError" },
    { 10053, "ConnectionAbortedError" }, { 10054, "ConnectionResetError" },
    { 10058, "BrokenPipeError" }, { 10060, "TimeoutError" },
    { 10061, "ConnectionRefusedError" },
#else
    { 103, "ConnectionAbortedError" }, { 104, "ConnectionResetError" },
    { 110, "TimeoutError" }, { 111, "ConnectionRefusedError" },
#endif
};
#define SR2_ERRNO_MAP_N (sizeof(SR2_ERRNO_MAP) / sizeof(SR2_ERRNO_MAP[0]))

static json_t *sr2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    const json_t *en = spec ? json_obj_get(spec, "errno") : NULL;
    if (en && en->type == JSON_NUMBER && sr2_exc_is_a(type, "OSError")) {
        if (strcmp(type, "OSError") == 0) {
            for (size_t i = 0; i < SR2_ERRNO_MAP_N; i++) {
                if (SR2_ERRNO_MAP[i].en == (int)en->num_val) {
                    type = SR2_ERRNO_MAP[i].cls;
                    break;
                }
            }
        }
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = sr2_err(type, buf);
        json_set(e, "errno", json_number(en->num_val));
        return e;
    }
    return sr2_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). A missing or
 * exhausted script is a fail-closed RuntimeError on both sides. */
static json_t *sr2_call(sr2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sr2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sr2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sr2_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* -- output projection (tags -> plain JSON; mirrors the oracle canon) -- */
static json_t *sr2_proj(const json_t *v) {
    if (!v) return json_null();
    if (sr2_is_float_tag(v)) return json_number(json_get_num(v, "v", 0));
    if (sr2_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *it = json_obj_get(v, "v");
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++)
                json_append(a, sr2_proj(json_get(it, i)));
        return a;
    }
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++)
            json_set(o, v->c.keys[i], sr2_proj(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, sr2_proj(json_get(v, i)));
        return a;
    }
    return json_copy(v);
}

/* -- string helpers (ASCII scope) -- */
static bool sr2_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v' || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}
/* Python str.strip() -> malloc'd copy */
static char *sr2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sr2_is_ws(s[a])) a++;
    while (b > a && sr2_is_ws(s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}
static bool sr2_str_empty_after_strip(const char *s) {
    char *t = sr2_strip(s);
    bool e = !t || !t[0];
    free(t);
    return e;
}
static bool sr2_isascii(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (*p >= 0x80) return false;
    return true;
}
static bool sr2_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool sr2_ieq_span(const char *s, size_t n, const char *lit) {
    size_t ln = strlen(lit);
    if (n != ln) return false;
    for (size_t i = 0; i < n; i++) {
        char a = s[i], b = lit[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return false;
    }
    return true;
}
static bool sr2_digits(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

/* Python float(str) on the ASCII domain: ws-strip (float() accepts only
 * " \t\n\r\f\v" - NOT \x1c-\x1f, unlike str.strip()/re \s), sign,
 * inf/infinity/nan (case-insensitive), decimal grammar with single
 * underscores BETWEEN digits, optional fraction and exponent. Returns false
 * where float() raises ValueError (the caller coerces to nan). */
static bool sr2_float_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static bool sr2_py_float(const char *s, double *out) {
    if (!s) return false;
    while (sr2_float_ws(*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && sr2_float_ws(e[-1])) e--;
    if (e == s) return false;
    bool neg = false;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (e == s) return false;
    size_t n = (size_t)(e - s);
    if (sr2_ieq_span(s, n, "inf") || sr2_ieq_span(s, n, "infinity")) {
        *out = neg ? -HUGE_VAL : HUGE_VAL;
        return true;
    }
    if (sr2_ieq_span(s, n, "nan")) { *out = NAN; return true; }
    const char *p = s;
    bool prev_digit = false, any_digit = false;
    while (p < e && ((*p >= '0' && *p <= '9') || *p == '_')) {
        if (*p == '_') {
            if (!prev_digit || p + 1 >= e || p[1] < '0' || p[1] > '9') return false;
            prev_digit = false; p++; continue;
        }
        prev_digit = true; any_digit = true; p++;
    }
    if (p < e && *p == '.') {
        p++;
        prev_digit = false;
        while (p < e && ((*p >= '0' && *p <= '9') || *p == '_')) {
            if (*p == '_') {
                if (!prev_digit || p + 1 >= e || p[1] < '0' || p[1] > '9') return false;
                prev_digit = false; p++; continue;
            }
            prev_digit = true; any_digit = true; p++;
        }
    }
    if (!any_digit) return false;
    if (p < e && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < e && (*p == '+' || *p == '-')) p++;
        bool exp_digit = false;
        prev_digit = false;
        while (p < e && ((*p >= '0' && *p <= '9') || *p == '_')) {
            if (*p == '_') {
                if (!prev_digit || p + 1 >= e || p[1] < '0' || p[1] > '9') return false;
                prev_digit = false; p++; continue;
            }
            prev_digit = true; exp_digit = true; p++;
        }
        if (!exp_digit) return false;
    }
    if (p != e) return false;
    char *buf = malloc(n + 1);
    if (!buf) return false;
    size_t k = 0;
    for (const char *q = s; q < e; q++)
        if (*q != '_') buf[k++] = *q;
    buf[k] = '\0';
    *out = strtod(buf, NULL);
    if (neg) *out = -*out;
    free(buf);
    return true;
}

/* -- UTF-8 walker (valid UTF-8 + raw lone-surrogate triplets domain) -- */
static size_t sr2_utf8_decode(const char *s, unsigned int *cp) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0 && ((unsigned char)s[1] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(c & 0x1F) << 6) | (unsigned char)(s[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0 && ((unsigned char)s[1] & 0xC0) == 0x80 &&
        ((unsigned char)s[2] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(c & 0x0F) << 12) |
              ((unsigned int)(s[1] & 0x3F) << 6) | (unsigned char)(s[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0 && ((unsigned char)s[1] & 0xC0) == 0x80 &&
        ((unsigned char)s[2] & 0xC0) == 0x80 && ((unsigned char)s[3] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(c & 0x07) << 18) |
              ((unsigned int)(s[1] & 0x3F) << 12) |
              ((unsigned int)(s[2] & 0x3F) << 6) | (unsigned char)(s[3] & 0x3F);
        return 4;
    }
    *cp = c;
    return 1;
}
static size_t sr2_utf8_encode(unsigned int cp, char *buf) {
    if (cp < 0x80) { buf[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* _SURROGATE_RE.sub('\ufffd', text): replace lone surrogate code points. */
static char *sr2_sanitize_surrogates_utf8(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 8);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; ) {
        unsigned int cp;
        size_t w = sr2_utf8_decode(s + i, &cp);
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            memcpy(out + k, "\xEF\xBF\xBD", 3);
            k += 3;
        } else {
            memcpy(out + k, s + i, w);
            k += w;
        }
        i += w;
    }
    out[k] = '\0';
    return out;
}

/* -- _TIMESTAMP_PREFIX_RE hand-written matcher (ASCII scope) --
 * ^\[(?:(?P<dow>[A-Z][a-z]{2}) (?P<date>\d{4}-\d{2}-\d{2})
 *    (?P<time>\d{2}:\d{2}:\d{2})(?: (?P<tz>[A-Za-z0-9_+\-/:]+))?
 *   |(?P<iso>\d{4}-\d{2}-\d{2}T[^\]]+))\]\s*
 * Human branch first, iso branch on backtrack; \s is the ASCII whitespace set
 * (incl. \x1c-\x1f); \d is 0-9. Groups date/time/iso are malloc'd or NULL. */
typedef struct sr2_match {
    bool ok;
    size_t end;
    char *iso;
    char *date;
    char *time;
} sr2_match_t;

static bool sr2_tz_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '+' || c == '-' ||
           c == '/' || c == ':';
}
static void sr2_match_free(sr2_match_t *m) {
    free(m->iso); free(m->date); free(m->time);
    m->iso = m->date = m->time = NULL;
}
static void sr2_ts_match(const char *s, sr2_match_t *m) {
    memset(m, 0, sizeof *m);
    if (!s || s[0] != '[') return;
    const char *p = s + 1;
    const char *q = p;
    bool human = true;
    char date[11], time[9];
    date[0] = time[0] = '\0';
    /* dow: [A-Z][a-z]{2} + ' ' */
    if (strlen(q) < 4 || !(q[0] >= 'A' && q[0] <= 'Z') ||
        !(q[1] >= 'a' && q[1] <= 'z') || !(q[2] >= 'a' && q[2] <= 'z') || q[3] != ' ')
        human = false;
    else q += 4;
    /* date: \d{4}-\d{2}-\d{2} + ' ' */
    if (human) {
        if (strlen(q) < 11 || !sr2_digits(q, 4) || q[4] != '-' ||
            !sr2_digits(q + 5, 2) || q[7] != '-' || !sr2_digits(q + 8, 2) || q[10] != ' ')
            human = false;
        else { memcpy(date, q, 10); date[10] = '\0'; q += 11; }
    }
    /* time: \d{2}:\d{2}:\d{2} */
    if (human) {
        if (strlen(q) < 8 || !sr2_digits(q, 2) || q[2] != ':' ||
            !sr2_digits(q + 3, 2) || q[5] != ':' || !sr2_digits(q + 6, 2))
            human = false;
        else { memcpy(time, q, 8); time[8] = '\0'; q += 8; }
    }
    /* optional tz: ' ' + [A-Za-z0-9_+\-/:]+ (greedy; ']' not in the class) */
    if (human && q[0] == ' ' && sr2_tz_char(q[1])) {
        q++;
        while (sr2_tz_char(*q)) q++;
    }
    if (human && q[0] == ']') {
        q++;
        while (*q && sr2_is_ws(*q)) q++;
        m->ok = true;
        m->end = (size_t)(q - s);
        m->date = strdup(date);
        m->time = strdup(time);
        return;
    }
    /* iso branch: \d{4}-\d{2}-\d{2}T[^\]]+ - the group INCLUDES date+T */
    q = p;
    if (strlen(q) >= 11 && sr2_digits(q, 4) && q[4] == '-' &&
        sr2_digits(q + 5, 2) && q[7] == '-' && sr2_digits(q + 8, 2) && q[10] == 'T') {
        const char *body = q + 11;
        const char *r = body;
        while (*r && *r != ']') r++;
        if (r > body && *r == ']') {
            size_t iso_len = (size_t)(r - q);
            r++;
            while (*r && sr2_is_ws(*r)) r++;
            m->ok = true;
            m->end = (size_t)(r - s);
            m->iso = malloc(iso_len + 1);
            if (m->iso) { memcpy(m->iso, q, iso_len); m->iso[iso_len] = '\0'; }
        }
    }
}

/* -- attribute / mapping access with real AttributeError messages -- */
/* obj.get(key) on a non-dict raises; on a dict returns BORROWED value or NULL
 * (absent == None). */
static const json_t *sr2_map_get(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!sr2_is_dict(obj)) {
        *err = sr2_err_fmt("AttributeError", "'%s' object has no attribute 'get'",
                          sr2_type_name(obj));
        return NULL;
    }
    return json_obj_get(obj, key);
}
/* inert attribute read on a JSON double (uuid4().hex): dict keys are attrs. */
static const json_t *sr2_attr_get(const json_t *v, const char *name, json_t **err) {
    *err = NULL;
    if (sr2_is_dict(v)) {
        const json_t *r = json_obj_get(v, name);
        if (r) return r;
    }
    *err = sr2_err_fmt("AttributeError", "'%s' object has no attribute '%s'",
                      sr2_type_name(v), name);
    return NULL;
}
/* list(value): list/tuple copy, str -> code-point chars, dict -> keys;
 * anything else raises TypeError "'<type>' object is not iterable". */
static json_t *sr2_list_of(const json_t *v) {
    if (sr2_is_list(v)) return json_copy(v);
    if (sr2_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *it = json_obj_get(v, "v");
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++)
                json_append(a, json_copy(json_get(it, i)));
        return a;
    }
    if (sr2_is_str(v)) {
        json_t *a = json_array();
        const char *s = v->str_val;
        size_t n = strlen(s);
        for (size_t i = 0; i < n; ) {
            unsigned int cp;
            size_t w = sr2_utf8_decode(s + i, &cp);
            char buf[5];
            size_t bw = sr2_utf8_encode(cp, buf);
            buf[bw] = '\0';
            json_append(a, json_string(buf));
            i += w;
        }
        return a;
    }
    if (sr2_is_dict(v)) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, json_string(v->c.keys[i]));
        return a;
    }
    return sr2_err_fmt("TypeError", "'%s' object is not iterable", sr2_type_name(v));
}
/* first n items of a sequence, preserving list-vs-tuple kind (Python [:n]). */
static json_t *sr2_slice(const json_t *v, size_t n) {
    bool tup = sr2_is_tuple_tag(v);
    json_t *items = json_array();
    size_t len = sr2_seq_len(v);
    if (n > len) n = len;
    for (size_t i = 0; i < n; i++)
        json_append(items, json_copy(sr2_seq_get(v, i)));
    if (!tup) return items;
    json_t *o = json_object();
    json_set(o, "__tuple__", json_bool(true));
    json_set(o, "v", items);
    return o;
}

/* ============== session_rules/turns.py - unclaimed pure turn-policy leaves ============== */

/* Python str.strip() is already available as sr2_strip; join an array of strings. */
static char *sr2_join_strings(const json_t *arr, const char *sep) {
    size_t sep_len = sep ? strlen(sep) : 0;
    size_t n = arr && arr->type == JSON_ARRAY ? arr->c.count : 0;
    size_t total = 1;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val) total += strlen(v->str_val);
        if (i + 1 < n) total += sep_len;
    }
    char *out = malloc(total);
    if (!out) return NULL;
    size_t k = 0;
    out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (i && sep_len) { memcpy(out + k, sep, sep_len); k += sep_len; }
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val) {
            size_t l = strlen(v->str_val);
            memcpy(out + k, v->str_val, l);
            k += l;
        }
    }
    out[k] = '\0';
    return out;
}

/* str.replace(old,new): replace every non-overlapping occurrence. */
static char *sr2_str_replace(const char *s, const char *old, const char *newv) {
    if (!s || !old) return NULL;
    size_t olen = strlen(old);
    if (olen == 0) return strdup(s);
    if (!newv) newv = "";
    size_t nlen = strlen(newv);
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, old)) != NULL) { count++; p += olen; }
    size_t slen = strlen(s);
    size_t outlen = slen - count * olen + count * nlen;
    char *out = malloc(outlen + 1);
    if (!out) return NULL;
    size_t k = 0;
    const char *cur = s;
    while ((p = strstr(cur, old)) != NULL) {
        size_t pre = (size_t)(p - cur);
        memcpy(out + k, cur, pre); k += pre;
        memcpy(out + k, newv, nlen); k += nlen;
        cur = p + olen;
    }
    strcpy(out + k, cur);
    return out;
}

/* _field(value,key) = value.get(key) if Mapping else getattr(value,key,None).
 * In the JSON domain only dicts are Mappings; every other type has no such
 * attribute, so the non-dict branch is None. */
static json_t *sr2_field_impl(const json_t *value, const char *key) {
    if (sr2_is_dict(value)) {
        const json_t *v = json_obj_get(value, key);
        return v ? json_copy(v) : json_null();
    }
    return json_null();
}
/* PoP: sr2_field @ session_rules/turns.py:_field */
static json_t *sr2_field(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_field_impl(json_obj_get(args, "value"), json_get_str(args, "key", ""));
}

/* _part_text(item): the str itself, a dict's "text", else None. */
static json_t *sr2_part_text_impl(const json_t *item) {
    if (sr2_is_str(item)) return json_copy(item);
    if (sr2_is_dict(item)) {
        const json_t *v = json_obj_get(item, "text");
        return v ? json_copy(v) : json_null();
    }
    return json_null();
}
/* PoP: sr2_part_text @ session_rules/turns.py:_part_text */
static json_t *sr2_part_text(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_part_text_impl(json_obj_get(args, "item"));
}

/* _with_part_text(item,text): {**item,"text":text} for dicts, else text. */
static json_t *sr2_with_part_text_impl(const json_t *item, const json_t *text) {
    if (sr2_is_dict(item)) {
        json_t *o = json_object();
        for (size_t i = 0; i < item->c.count; i++)
            json_set(o, item->c.keys[i], json_copy(item->c.items[i]));
        json_set(o, "text", json_copy((json_t *)text));
        return o;
    }
    return json_copy((json_t *)text);
}
/* PoP: sr2_with_part_text @ session_rules/turns.py:_with_part_text */
static json_t *sr2_with_part_text(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_with_part_text_impl(json_obj_get(args, "item"), json_obj_get(args, "text"));
}

/* _content_text_for_contains(content):
 *   list -> "\n".join(t for _part_text if str and truthy)
 *   None -> "" ; str -> itself ; other -> str(content) (not in fixture domain). */
static json_t *sr2_content_text_for_contains_impl(const json_t *content) {
    if (sr2_is_list(content)) {
        json_t *parts = json_array();
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *t = sr2_part_text_impl(json_get(content, i));
            if (sr2_is_str(t) && t->str_val && t->str_val[0]) json_append(parts, t);
            else json_free(t);
        }
        char *joined = sr2_join_strings(parts, "\n");
        json_free(parts);
        json_t *r = joined ? json_string(joined) : json_string("");
        free(joined);
        return r;
    }
    if (sr2_is_null(content)) return json_string("");
    if (sr2_is_str(content)) return json_copy(content);
    return json_string("<object>");
}
/* PoP: sr2_content_text_for_contains @ session_rules/turns.py:_content_text_for_contains */
static json_t *sr2_content_text_for_contains(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_content_text_for_contains_impl(json_obj_get(args, "content"));
}

/* is_content_blank(content): None -> True; str -> not strip();
 * list -> not "".join(p["text"] for dict parts of type "text").strip(); else False. */
static json_t *sr2_is_content_blank_impl(const json_t *content) {
    if (sr2_is_null(content)) return json_bool(true);
    if (sr2_is_str(content)) return json_bool(sr2_str_empty_after_strip(content->str_val));
    if (sr2_is_list(content)) {
        json_t *parts = json_array();
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *p = json_get(content, i);
            if (!sr2_is_dict(p) || !sr2_eq_str(json_obj_get(p, "type"), "text")) continue;
            const json_t *t = json_obj_get(p, "text");
            json_append(parts, t && t->type == JSON_STRING ? json_copy(t) : json_string(""));
        }
        char *joined = sr2_join_strings(parts, "");
        json_free(parts);
        bool blank = sr2_str_empty_after_strip(joined ? joined : "");
        free(joined);
        return json_bool(blank);
    }
    return json_bool(false);
}
/* PoP: sr2_is_content_blank @ session_rules/turns.py:is_content_blank */
static json_t *sr2_is_content_blank(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_is_content_blank_impl(json_obj_get(args, "content"));
}

/* drop_stale_api_content(msg): msg.pop("api_content", None). */
static json_t *sr2_drop_stale_api_content_impl(json_t *msg) {
    if (!sr2_is_dict(msg))
        return sr2_err_fmt("AttributeError", "'%s' object has no attribute 'pop'",
                           sr2_type_name(msg));
    json_obj_del(msg, "api_content");
    return json_null();
}
/* PoP: sr2_drop_stale_api_content @ session_rules/turns.py:drop_stale_api_content */
static json_t *sr2_drop_stale_api_content(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_drop_stale_api_content_impl(json_obj_get(args, "msg"));
}

/* seeded constants */
#define SR2_WITHHELD_PROMPT "withheld_prompt"
#define SR2_PRE_LLM_BLOCK_WITHHELD_PROMPT "[Message withheld: a pre_llm_call policy hook blocked it.]"

/* _display_metadata_str(message,key): message.display_metadata[key] if str else None */
static json_t *sr2_display_metadata_str_impl(const json_t *message, const char *key) {
    if (!sr2_is_dict(message)) return json_null();
    const json_t *md = json_obj_get(message, "display_metadata");
    if (!sr2_is_dict(md)) return json_null();
    const json_t *v = json_obj_get(md, key);
    if (sr2_is_str(v)) return json_copy(v);
    return json_null();
}
/* PoP: sr2_display_metadata_str @ session_rules/turns.py:_display_metadata_str */
static json_t *sr2_display_metadata_str(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_display_metadata_str_impl(json_obj_get(args, "message"),
                                         json_get_str(args, "key", ""));
}

/* withheld_prompt_text(message) = _display_metadata_str(message, WITHHELD_PROMPT) */
static json_t *sr2_withheld_prompt_text_impl(const json_t *message) {
    return sr2_display_metadata_str_impl(message, SR2_WITHHELD_PROMPT);
}
/* PoP: sr2_withheld_prompt_text @ session_rules/turns.py:withheld_prompt_text */
static json_t *sr2_withheld_prompt_text(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_withheld_prompt_text_impl(json_obj_get(args, "message"));
}

/* typed_prompt_content(content,message): swap the withheld placeholder for the typed text. */
static json_t *sr2_typed_prompt_content_impl(const json_t *content, const json_t *message) {
    json_t *typed = sr2_withheld_prompt_text_impl(message);
    if (sr2_is_null(typed)) return json_copy((json_t *)content);
    const char *ts = sr2_str(typed);
    if (sr2_is_str(content)) {
        char *r = sr2_str_replace(content->str_val, SR2_PRE_LLM_BLOCK_WITHHELD_PROMPT,
                                  ts ? ts : "");
        json_free(typed);
        json_t *out = r ? json_string(r) : json_string("");
        free(r);
        return out;
    }
    if (sr2_is_list(content)) {
        json_t *out = json_array();
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *part = json_get(content, i);
            if (sr2_is_dict(part)) {
                const json_t *pt = json_obj_get(part, "text");
                if (sr2_is_str(pt)) {
                    char *r = sr2_str_replace(pt->str_val, SR2_PRE_LLM_BLOCK_WITHHELD_PROMPT,
                                              ts ? ts : "");
                    json_t *n = json_object();
                    for (size_t j = 0; j < part->c.count; j++)
                        json_set(n, part->c.keys[j], json_copy(part->c.items[j]));
                    json_set_hdr(n, "text", r ? r : "");
                    free(r);
                    json_append(out, n);
                    continue;
                }
            }
            json_append(out, json_copy((json_t *)part));
        }
        json_free(typed);
        return out;
    }
    json_free(typed);
    return json_copy((json_t *)content);
}
/* PoP: sr2_typed_prompt_content @ session_rules/turns.py:typed_prompt_content */
static json_t *sr2_typed_prompt_content(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_typed_prompt_content_impl(json_obj_get(args, "content"),
                                         json_obj_get(args, "message"));
}

/* retryable_user_text(content) -> str, raising ValueError with exact messages. */
static json_t *sr2_retryable_user_text_impl(const json_t *content) {
    bool is_str = sr2_is_str(content), is_list = sr2_is_list(content);
    if (!is_str && !is_list)
        return sr2_err("ValueError", "retry does not support non-text content");
    json_t *chunks = json_array();
    size_t n = is_str ? 1 : content->c.count;
    for (size_t i = 0; i < n; i++) {
        const json_t *part = is_str ? content : json_get(content, i);
        if (sr2_is_str(part)) { json_append(chunks, json_copy((json_t *)part)); continue; }
        if (!sr2_is_dict(part)) {
            json_free(chunks);
            return sr2_err("ValueError", "retry does not support non-text content");
        }
        const json_t *ptype = json_obj_get(part, "type");
        bool ok_type = sr2_eq_str(ptype, "text") || sr2_eq_str(ptype, "input_text") ||
                       sr2_eq_str(ptype, "output_text");
        if (!ok_type) {
            json_free(chunks);
            return sr2_err("ValueError", "retry does not support media or unknown content parts");
        }
        for (size_t j = 0; j < part->c.count; j++) {
            const char *k = part->c.keys[j];
            if (strcmp(k, "type") != 0 && strcmp(k, "text") != 0) {
                json_free(chunks);
                return sr2_err("ValueError", "retry cannot losslessly flatten annotated text parts");
            }
        }
        const json_t *pt = json_obj_get(part, "text");
        if (!sr2_is_str(pt)) {
            json_free(chunks);
            return sr2_err("ValueError", "retry text parts must contain text");
        }
        json_append(chunks, json_copy((json_t *)pt));
    }
    char *text = sr2_join_strings(chunks, "");
    json_free(chunks);
    bool blank = sr2_str_empty_after_strip(text ? text : "");
    json_t *out;
    if (blank) out = sr2_err("ValueError", "retry found no text to send");
    else out = text ? json_string(text) : json_string("");
    free(text);
    return out;
}
/* PoP: sr2_retryable_user_text @ session_rules/turns.py:retryable_user_text */
static json_t *sr2_retryable_user_text(sr2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr2_retryable_user_text_impl(json_obj_get(args, "content"));
}
