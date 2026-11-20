/*
 * session_transcript_rows_flow2.c — FLOW port of the 7 UNCLAIMED defs of
 * gateway/session_transcript_rows.py (quarry, C:/Users/eman5/hermes-agent).
 *
 * Pure/flow leaves: handoff_row, _mark_index, handoff_occurrence,
 * shown_occurrences, handoff_rows, page_handoffs, project_entries.
 * Every port entry is `static str2_*`; the harness #includes this file, so no
 * external symbols are exported. Only libjson + libc are used.
 *
 * Modeling contract (shared with tests/oracle_session_transcript_rows_flow2.py):
 *  - Cases: {"func": name, "in": {...}}. `in` carries call args + `io`
 *    (scripted seam results). Harness emits
 *    {"func","in","out":{"ret":...,"trace":[...]}} per case.
 *  - Universal seam: json_t *(*call)(void*, const char *name, const json_t *args).
 *    Seam names: "compaction_summary", "messages.transcript_handoff_rows",
 *    "row.as_wire_row", "earlier", "project". NULL result == Python None.
 *  - Script semantics: bare array == ONE RESULT PER CALL (last repeats);
 *    empty array == constant None; {"__script__":true,"ret":v} == constant;
 *    absent == None; {"__raise__":kind,"message":m} == raised exception.
 *  - Trace records {ev, args, ret} in call order; `log` is not used by this
 *    module, so every seam event is a plain {ev,args,ret} triple.
 *  - Rows returned by "messages.transcript_handoff_rows" are opaque handles;
 *    `row.as_wire_row()` fires a seam and yields the wire-row dict. Wire rows
 *    are dicts (not objects) for the downstream .get/.dumps reads.
 *  - A scripted raise propagates via env->exc. The FULL builtin exception
 *    hierarchy is tabled below so classification of any builtin kind is
 *    exact (Exception-family vs BaseException-only). Unknown kinds are
 *    treated as Exception subclasses (the oracle materializes them as such).
 *  - Integer domain: JSON numbers are doubles. A number reaches live Python
 *    as int only when libjson's echo serializer renders it int-like
 *    (integral, inside [-2^63,2^63)); str2_echo_int models that. `type(x) is
 *    int` gates also exclude JSON_BOOL. bool is an int SUBCLASS for the
 *    isinstance((int,float)) gate in handoff_row (True -> float(True)=1.0).
 *  - json.dumps(default=str) is transcribed with Python separators (", ",
 *    ": "), ensure_ascii and shortest-roundtrip float repr. Pure JSON values
 *    never exercise default=str here.
 *  - Set results (shown_occurrences) are emitted sorted by the canonical
 *    json.dumps of each 2-tuple occurrence, matching Python sorted(set).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"

#define STR2_MARK_KEY "__session_transcript_handoff__"
#define STR2_MARK_TEXT "compaction handoff"
#define STR2_KIND_COMPACTION "compaction"
#define STR2_KIND_MESSAGE "message"

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct str2_env {
    json_t *state;   /* borrowed mirror (unused by these pure/flow leaves) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise descriptor; NULL when none */
    json_t *cur;     /* owned caught-exception descriptor; NULL when none */
} str2_env;

/* Universal seam call: args are BORROWED by the stub (it copies into the
 * trace) and freed here; the result is NEW (caller frees). NULL == None. */
static json_t *str2_call(str2_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}

/* ── small JSON helpers ───────────────────────────────────────────────────── */
static bool str2_truth(const json_t *v) {
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
static bool str2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool str2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool str2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool str2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const json_t *str2_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static json_t *str2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static bool str2_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!str2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !str2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}
static bool str2_integral(double d) { return d == floor(d); }
static bool str2_echo_int(double d) {
    return str2_integral(d) && d >= -9223372036854775808.0 &&
           d < 9223372036854775808.0;
}

/* Shortest-roundtrip float repr with Python formatting rules. */
static char *str2_float_repr(double d) {
    char sci[64];
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(sci, sizeof(sci), "%.*e", prec - 1, d);
        if (strtod(sci, NULL) == d) break;
    }
    bool neg = sci[0] == '-';
    const char *p = neg ? sci + 1 : sci;
    char dig[64];
    size_t nd = 0;
    while (*p && *p != 'e' && *p != 'E') {
        if (*p >= '0' && *p <= '9') dig[nd++] = *p;
        p++;
    }
    dig[nd] = '\0';
    int E = (*p == 'e' || *p == 'E') ? atoi(p + 1) : 0;
    while (nd > 1 && dig[nd - 1] == '0') dig[--nd] = '\0';
    char *out = (char *)malloc(nd + 64);
    if (!out) return strdup("");
    if (E >= -4 && E < 16) {
        int point = E + 1;
        if (point <= 0)
            sprintf(out, "%s0.%s%s", neg ? "-" : "",
                    "000000000000000000000000000000" + (30 + point), dig);
        else if ((size_t)point >= nd)
            sprintf(out, "%s%s%.*s.0", neg ? "-" : "", dig, (int)((size_t)point - nd),
                    "000000000000000000000000000000");
        else
            sprintf(out, "%s%.*s.%s", neg ? "-" : "", point, dig, dig + point);
    } else {
        if (nd > 1)
            sprintf(out, "%s%c.%s%c%c%02d", neg ? "-" : "", dig[0], dig + 1,
                    'e', E < 0 ? '-' : '+', E < 0 ? -E : E);
        else
            sprintf(out, "%s%c%c%c%02d", neg ? "-" : "", dig[0],
                    'e', E < 0 ? '-' : '+', E < 0 ? -E : E);
    }
    return out;
}
static const char *str2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return str2_echo_int(v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* ── Python json.dumps(default=str) for pure JSON values ─────────────────── */
typedef struct { char *buf; size_t len, cap; } str2_strbuf;
static void str2_sb_init(str2_strbuf *b) {
    b->cap = 128; b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
}
static void str2_sb_put(str2_strbuf *b, const char *s, size_t n) {
    if (!b->buf) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2 + n + 1;
        char *nb = (char *)realloc(b->buf, nc);
        if (!nb) return;
        b->buf = nb; b->cap = nc;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}
static void str2_sb_puts(str2_strbuf *b, const char *s) { str2_sb_put(b, s, strlen(s)); }
static void str2_sb_hex4(str2_strbuf *b, unsigned cp) {
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "\\u%04x", cp);
    str2_sb_puts(b, tmp);
}
static void str2_dumps_str(str2_strbuf *b, const char *s) {
    str2_sb_puts(b, "\"");
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        if (c == '\"') { str2_sb_puts(b, "\\\""); p++; }
        else if (c == '\\') { str2_sb_puts(b, "\\\\"); p++; }
        else if (c == '\n') { str2_sb_puts(b, "\\n"); p++; }
        else if (c == '\r') { str2_sb_puts(b, "\\r"); p++; }
        else if (c == '\t') { str2_sb_puts(b, "\\t"); p++; }
        else if (c == '\b') { str2_sb_puts(b, "\\b"); p++; }
        else if (c == '\f') { str2_sb_puts(b, "\\f"); p++; }
        else if (c < 0x20) { str2_sb_hex4(b, c); p++; }
        else if (c < 0x7f) { str2_sb_put(b, (const char *)p, 1); p++; }
        else if (c == 0x7f) { str2_sb_hex4(b, 0x7f); p++; }
        else {
            unsigned cp = 0; int n = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
            else { cp = c; n = 1; }
            for (int i = 1; i < n && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3F);
            p += n;
            if (cp >= 0x10000) {
                cp -= 0x10000;
                str2_sb_hex4(b, 0xD800 + (cp >> 10));
                str2_sb_hex4(b, 0xDC00 + (cp & 0x3FF));
            } else str2_sb_hex4(b, cp);
        }
    }
    str2_sb_puts(b, "\"");
}
static void str2_dumps_val(str2_strbuf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { str2_sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: str2_sb_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (str2_echo_int(d)) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%.0f", d);
                str2_sb_puts(b, tmp);
            } else {
                char *r = str2_float_repr(d);
                str2_sb_puts(b, r);
                free(r);
            }
            return;
        }
        case JSON_STRING: str2_dumps_str(b, v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY:
            str2_sb_puts(b, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) str2_sb_puts(b, ", ");
                str2_dumps_val(b, v->c.items[i]);
            }
            str2_sb_puts(b, "]");
            return;
        case JSON_OBJECT:
            str2_sb_puts(b, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) str2_sb_puts(b, ", ");
                str2_dumps_str(b, v->c.keys[i]);
                str2_sb_puts(b, ": ");
                str2_dumps_val(b, v->c.items[i]);
            }
            str2_sb_puts(b, "}");
            return;
        default: str2_sb_puts(b, "null"); return;
    }
}
static char *str2_dumps(const json_t *v) {
    str2_strbuf b;
    str2_sb_init(&b);
    str2_dumps_val(&b, v);
    return b.buf ? b.buf : strdup("");
}

/* Python repr of a plain string (for int() ValueError messages). */
static char *str2_repr_str(const char *s) {
    str2_strbuf b;
    str2_sb_init(&b);
    str2_sb_puts(&b, "'");
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        if (c == '\\') str2_sb_puts(&b, "\\\\");
        else if (c == '\'') str2_sb_puts(&b, "\\'");
        else if (c == '\n') str2_sb_puts(&b, "\\n");
        else if (c == '\r') str2_sb_puts(&b, "\\r");
        else if (c == '\t') str2_sb_puts(&b, "\\t");
        else str2_sb_put(&b, (const char *)p, 1);
    }
    str2_sb_puts(&b, "'");
    return b.buf ? b.buf : strdup("''");
}

/* ── FULL builtin exception hierarchy (parent table) ────────────────────────
 * Classification walks the chain: a kind whose ancestry reaches Exception is
 * an Exception-family kind; BaseException-only kinds are distinct. Unknown
 * kinds classify as Exception (the oracle materializes them as Exception
 * subclasses). ExceptionGroup has mixed ancestry and is special-cased. */
typedef struct { const char *name, *parent; } str2_exc_parent;
static const str2_exc_parent str2_exc_table[] = {
    { "BaseException", "" },
    { "SystemExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "GeneratorExit", "BaseException" },
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
    { "IOError", "OSError" }, { "EnvironmentError", "OSError" },
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
    { "EncodingWarning", "Warning" },
    { "ResourceWarning", "Warning" },
    { "BaseExceptionGroup", "BaseException" },
    /* py3.14 additions. */
    { "EOFError", "Exception" },
    { "PythonFinalizationError", "RuntimeError" },
    { "_IncompleteInputError", "SyntaxError" },
    { "CancelledError", "BaseException" },
};
static bool str2_is_exception_kind(const char *kind) {
    if (!kind || !kind[0]) return true;
    if (strcmp(kind, "ExceptionGroup") == 0) return true;   /* mixed ancestry */
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int hops = 0; hops < 64; hops++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        const char *parent = NULL;
        for (size_t i = 0; i < sizeof(str2_exc_table) / sizeof(str2_exc_table[0]); i++) {
            if (strcmp(str2_exc_table[i].name, cur) == 0) {
                parent = str2_exc_table[i].parent;
                break;
            }
        }
        if (!parent) return true;     /* unknown -> Exception subclass */
        if (!parent[0]) return false; /* BaseException root */
        snprintf(cur, sizeof(cur), "%s", parent);
    }
    return true;
}

/* ── raise descriptors ────────────────────────────────────────────────────── */
static bool str2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && str2_is_str(json_obj_get(r, "__raise__"));
}
static json_t *str2_norm_raise(const json_t *r) {
    json_t *d = json_object();
    json_set(d, "__raise__", json_string(json_get_str(r, "__raise__", "Exception")));
    json_set(d, "message", json_string(json_get_str(r, "message", "")));
    return d;
}
static void str2_raise(str2_env *e, const char *kind, const char *msg) {
    json_t *d = json_object();
    json_set(d, "__raise__", json_string(kind));
    json_set(d, "message", json_string(msg ? msg : ""));
    if (e->exc) json_free(e->exc);
    e->exc = d;
}
static void str2_attr_msg(char *buf, size_t cap, const char *type, const char *attr) {
    snprintf(buf, cap, "'%s' object has no attribute '%s'", type, attr);
}
/* Consuming: on a raise, takes ownership of r and sets e->exc. True == bail. */
static bool str2_prop(str2_env *e, json_t *r) {
    if (!str2_is_raise(r)) return false;
    if (e->exc) json_free(e->exc);
    e->exc = str2_norm_raise(r);
    json_free(r);
    return true;
}

/* ── required subscript / int(str) ─────────────────────────────────────────── */
/* Borrowed value; on missing key sets e->exc (KeyError) and returns NULL. */
static const json_t *str2_req(str2_env *e, const json_t *obj, const char *key) {
    const json_t *v = str2_get(obj, key);
    if (v) return v;
    char msg[128];
    snprintf(msg, sizeof(msg), "'%s'", key);
    str2_raise(e, "KeyError", msg);
    return NULL;
}
/* Python int(str) for base 10, ASCII domain. Returns false + ValueError. */
static bool str2_int_from_str(const char *s, double *out) {
    if (!s) return false;
    const char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *start = p;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    char clean[128];
    size_t n = 0;
    bool last_digit = false;
    bool any = false;
    for (; *p; p++) {
        if (isspace((unsigned char)*p)) break;
        if (*p == '_') {
            if (!last_digit) return false;
            last_digit = false;
            continue;
        }
        if (!isdigit((unsigned char)*p)) return false;
        if (n + 1 < sizeof(clean)) clean[n++] = *p;
        last_digit = true;
        any = true;
    }
    while (*p && isspace((unsigned char)*p)) p++;
    if (!any || *p != '\0' || !last_digit || p == start) return false;
    clean[n] = '\0';
    double v = strtod(clean, NULL);
    *out = neg ? -v : v;
    return true;
}
static void str2_int_value_error(str2_env *e, const char *s) {
    char *r = str2_repr_str(s);
    char msg[256];
    snprintf(msg, sizeof(msg), "invalid literal for int() with base 10: %s",
             r ? r : "''");
    str2_raise(e, "ValueError", msg);
    free(r);
}

/* ── occurrence helpers ───────────────────────────────────────────────────── */
/* build handoff_occurrence(message) -> new 2-array; NULL on raise. */
static json_t *str2_occurrence(str2_env *e, const json_t *message) {
    if (!str2_is_obj(message)) {
        char msg[160];
        str2_attr_msg(msg, sizeof(msg), str2_type_name(message), "get");
        str2_raise(e, "AttributeError", msg);
        return NULL;
    }
    const json_t *uid = json_obj_get(message, "message_uid");
    json_t *uid_out = str2_truth(uid) ? json_copy(uid) : json_null();
    const json_t *content = json_obj_get(message, "content");
    json_t *cn = content ? (json_t *)content : json_null();
    char *d = str2_dumps(cn);
    json_t *occ = json_array();
    json_append(occ, uid_out ? uid_out : json_null());
    json_append(occ, json_string(d ? d : ""));
    free(d);
    return occ;
}
static bool str2_occ_eq(const json_t *a, const json_t *b) {
    if (!str2_is_arr(a) || !str2_is_arr(b) || a->c.count != 2 || b->c.count != 2)
        return false;
    return str2_json_eq(a->c.items[0], b->c.items[0]) &&
           str2_json_eq(a->c.items[1], b->c.items[1]);
}
static bool str2_occ_in(const json_t *arr, const json_t *occ) {
    if (!str2_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (str2_occ_eq(arr->c.items[i], occ)) return true;
    return false;
}
static void str2_occ_add_once(json_t *arr, const json_t *occ) {
    if (!str2_is_arr(arr) || !occ || str2_occ_in(arr, occ)) return;
    json_append(arr, json_copy(occ));
}
static char *str2_occ_key(const json_t *occ) { return str2_dumps(occ); }
static int str2_occ_cmp(const void *pa, const void *pb) {
    const json_t *a = *(const json_t *const *)pa;
    const json_t *b = *(const json_t *const *)pb;
    char *ka = str2_occ_key(a), *kb = str2_occ_key(b);
    int r = strcmp(ka ? ka : "", kb ? kb : "");
    free(ka); free(kb);
    return r;
}
static void str2_occ_sort(json_t *arr) {
    if (str2_is_arr(arr) && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), str2_occ_cmp);
}

/* ── handoff_row core (message + summary nodes) ────────────────────────────── */
static json_t *str2_handoff_row_core(str2_env *e, const json_t *message,
                                     const json_t *summary) {
    if (!str2_is_obj(summary)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "'%s' object is not subscriptable",
                 str2_type_name(summary));
        str2_raise(e, "TypeError", msg);
        return NULL;
    }
    const json_t *sval = str2_req(e, summary, "summary");
    if (!sval) return NULL;
    const json_t *cval = str2_req(e, summary, "compacted_messages");
    if (!cval) return NULL;
    if (!str2_is_obj(message)) {
        char msg[160];
        str2_attr_msg(msg, sizeof(msg), str2_type_name(message), "get");
        str2_raise(e, "AttributeError", msg);
        return NULL;
    }
    const json_t *mid = str2_req(e, message, "id");
    if (!mid) return NULL;
    json_t *row = json_object();
    json_set(row, "row_kind", json_string(STR2_KIND_COMPACTION));
    json_set(row, "role", json_string("system"));
    json_set(row, "text", json_copy(sval));
    json_set(row, "compacted_messages", json_copy(cval));
    json_set(row, "row_id", json_copy(mid));
    const json_t *ts = json_obj_get(message, "timestamp");
    if (ts && (((ts->type == JSON_NUMBER) && ts->num_val > 0.0) ||
               ((ts->type == JSON_BOOL) && ts->bool_val))) {
        json_set(row, "timestamp",
                 json_number(ts->type == JSON_BOOL ? 1.0 : ts->num_val));
    }
    return row;
}

/* ── _mark_index core ─────────────────────────────────────────────────────── */
/* Returns true with *idx set when index is exactly an int (no exception);
 * false when None or an exception was raised (check e->exc). */
static bool str2_mark_index_core(str2_env *e, const json_t *row, double *idx) {
    if (!str2_is_obj(row)) {
        char msg[160];
        str2_attr_msg(msg, sizeof(msg), str2_type_name(row), "get");
        str2_raise(e, "AttributeError", msg);
        return false;
    }
    const json_t *md = json_obj_get(row, "display_metadata");
    const json_t *index = NULL;
    if (md && md->type == JSON_OBJECT) index = json_obj_get(md, STR2_MARK_KEY);
    const json_t *role = json_obj_get(row, "role");
    const json_t *text = json_obj_get(row, "text");
    bool gate = (role && role->type == JSON_STRING &&
                 strcmp(role->str_val ? role->str_val : "", "system") == 0) &&
                (text && text->type == JSON_STRING &&
                 strcmp(text->str_val ? text->str_val : "", STR2_MARK_TEXT) == 0) &&
                (index && index->type == JSON_NUMBER && str2_echo_int(index->num_val));
    if (gate) { *idx = index->num_val; return true; }
    return false;
}

/* ── handoff_rows core (shown_ids + segments) ──────────────────────────────── */
static json_t *str2_handoff_rows_core(str2_env *e, const json_t *shown_ids,
                                      const json_t *segments) {
    json_t *args = json_object();
    json_set(args, "shown_ids", json_copy(shown_ids ? shown_ids : json_array()));
    json_set(args, "segments", json_copy(segments ? segments : json_null()));
    json_t *r = str2_call(e, "messages.transcript_handoff_rows", args);
    if (str2_prop(e, r)) return NULL;
    if (!str2_is_obj(r)) {
        char msg[160];
        str2_attr_msg(msg, sizeof(msg), str2_type_name(r), "items");
        str2_raise(e, "AttributeError", msg);
        json_free(r);
        return NULL;
    }
    json_t *out = json_object();
    for (size_t i = 0; i < r->c.count; i++) {
        const char *key = r->c.keys[i];
        double iv = 0;
        if (!str2_int_from_str(key, &iv)) {
            str2_int_value_error(e, key);
            json_free(r);
            json_free(out);
            return NULL;
        }
        json_t *rows = r->c.items[i];   /* object value (json_get is ARRAY-only) */
        json_t *wire = json_array();
        if (str2_is_arr(rows)) {
            for (size_t j = 0; j < rows->c.count; j++) {
                json_t *a = json_object();
                json_set(a, "row", json_copy(rows->c.items[j]));
                json_t *w = str2_call(e, "row.as_wire_row", a);
                if (str2_prop(e, w)) {
                    json_free(wire); json_free(r); json_free(out);
                    return NULL;
                }
                json_append(wire, w ? w : json_null());
            }
        }
        char kbuf[64];
        snprintf(kbuf, sizeof(kbuf), "%.0f", iv);
        json_set(out, kbuf, wire);
    }
    json_free(r);
    return out;
}

/* PoP: str2_handoff_row @ gateway/session_transcript_rows.py:handoff_row */
static json_t *str2_handoff_row(str2_env *e, const json_t *in) {
    return str2_handoff_row_core(e, str2_get(in, "message"), str2_get(in, "summary"));
}

/* PoP: str2_mark_index @ gateway/session_transcript_rows.py:_mark_index */
static json_t *str2_mark_index(str2_env *e, const json_t *in) {
    double idx = 0;
    if (!str2_mark_index_core(e, str2_get(in, "row"), &idx))
        return e->exc ? NULL : json_null();
    return json_number(idx);
}

/* PoP: str2_handoff_occurrence @ gateway/session_transcript_rows.py:handoff_occurrence */
static json_t *str2_handoff_occurrence(str2_env *e, const json_t *in) {
    return str2_occurrence(e, str2_get(in, "message"));
}

/* PoP: str2_shown_occurrences @ gateway/session_transcript_rows.py:shown_occurrences */
static json_t *str2_shown_occurrences(str2_env *e, const json_t *in) {
    const json_t *messages = str2_get(in, "messages");
    if (!str2_is_arr(messages)) {
        char msg[160];
        str2_attr_msg(msg, sizeof(msg), str2_type_name(messages), "__iter__");
        str2_raise(e, "TypeError", msg);
        return NULL;
    }
    json_t *set = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        const json_t *message = messages->c.items[i];
        json_t *args = json_object();
        json_set(args, "message", json_copy(message));
        json_t *r = str2_call(e, "compaction_summary", args);
        if (str2_prop(e, r)) { json_free(set); return NULL; }
        if (r && r->type != JSON_NULL) {
            json_t *occ = str2_occurrence(e, message);
            if (!occ) { json_free(r); json_free(set); return NULL; }
            str2_occ_add_once(set, occ);
            json_free(occ);
        }
        if (r) json_free(r);
    }
    str2_occ_sort(set);
    return set;
}

/* PoP: str2_handoff_rows @ gateway/session_transcript_rows.py:handoff_rows */
static json_t *str2_handoff_rows(str2_env *e, const json_t *in) {
    return str2_handoff_rows_core(e, str2_get(in, "shown_ids"),
                                  str2_get(in, "segments"));
}

/* PoP: str2_page_handoffs @ gateway/session_transcript_rows.py:page_handoffs */
static json_t *str2_page_handoffs(str2_env *e, const json_t *in) {
    const json_t *rows = str2_get(in, "rows");
    const json_t *segments = str2_get(in, "segments");
    bool earlier_present = json_get_bool(in, "earlier_present", false);
    if (!str2_is_arr(rows)) {
        char msg[160];
        str2_attr_msg(msg, sizeof(msg), str2_type_name(rows), "__iter__");
        str2_raise(e, "TypeError", msg);
        return NULL;
    }
    /* ids = [row['_row_id'] for row in rows if type(row.get('_row_id')) is int] */
    json_t *ids = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *rid = str2_get(rows->c.items[i], "_row_id");
        if (rid && rid->type == JSON_NUMBER && str2_echo_int(rid->num_val))
            json_append(ids, json_copy(rid));
    }
    json_t *found = str2_handoff_rows_core(e, ids, segments);
    json_free(ids);
    if (!found) return NULL;
    /* candidates = [message for messages in found.values() for message in messages] */
    json_t *candidates = json_array();
    for (size_t i = 0; i < found->c.count; i++) {
        json_t *v = found->c.items[i];   /* object value, not array element */
        if (str2_is_arr(v))
            for (size_t j = 0; j < v->c.count; j++)
                json_append(candidates, json_copy(v->c.items[j]));
    }
    json_t *seen = json_array();
    if (earlier_present && candidates->c.count > 0) {
        json_t *args = json_object();
        json_set(args, "candidates", json_copy(candidates));
        json_t *r = str2_call(e, "earlier", args);
        if (str2_prop(e, r)) { json_free(candidates); json_free(found); json_free(seen); return NULL; }
        if (str2_is_arr(r))
            for (size_t i = 0; i < r->c.count; i++)
                str2_occ_add_once(seen, r->c.items[i]);
        if (r) json_free(r);
    }
    json_free(candidates);
    json_t *out = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        json_t *mine = json_array();
        const json_t *rid = str2_get(row, "_row_id");
        const json_t *target = NULL;
        /* live keys are int(int(shown)); only an int-typed row id matches. */
        if (rid && rid->type == JSON_NUMBER && str2_echo_int(rid->num_val)) {
            char kbuf[64];
            snprintf(kbuf, sizeof(kbuf), "%.0f", rid->num_val);
            target = json_obj_get(found, kbuf);
        }
        if (target && target->type == JSON_ARRAY) {
            for (size_t j = 0; j < target->c.count; j++) {
                const json_t *message = target->c.items[j];
                json_t *occ = str2_occurrence(e, message);
                if (!occ) { json_free(mine); json_free(out); json_free(seen); json_free(found); return NULL; }
                if (!str2_occ_in(seen, occ)) {
                    json_t *a = json_object();
                    json_set(a, "message", json_copy(message));
                    json_t *r = str2_call(e, "compaction_summary", a);
                    if (str2_prop(e, r)) {
                        json_free(occ); json_free(mine); json_free(out);
                        json_free(seen); json_free(found); return NULL;
                    }
                    if (r && r->type != JSON_NULL) {
                        /* live: seen.add(occurrence) runs BEFORE handoff_row */
                        str2_occ_add_once(seen, occ);
                        json_t *hr = str2_handoff_row_core(e, message, r);
                        if (!hr) {
                            json_free(r); json_free(occ); json_free(mine);
                            json_free(out); json_free(seen); json_free(found);
                            return NULL;
                        }
                        json_append(mine, hr);
                    }
                    if (r) json_free(r);
                }
                json_free(occ);
            }
        }
        json_append(out, mine);
    }
    json_free(seen);
    json_free(found);
    return out;
}

/* PoP: str2_project_entries @ gateway/session_transcript_rows.py:project_entries */
static json_t *str2_project_entries(str2_env *e, const json_t *in) {
    const json_t *entries = str2_get(in, "entries");
    if (!str2_is_arr(entries)) {
        char msg[160];
        str2_attr_msg(msg, sizeof(msg), str2_type_name(entries), "__iter__");
        str2_raise(e, "TypeError", msg);
        return NULL;
    }
    json_t *handoffs = json_array();
    json_t *marked = json_array();
    for (size_t i = 0; i < entries->c.count; i++) {
        const json_t *entry = entries->c.items[i];
        const json_t *kind = str2_is_arr(entry) && entry->c.count >= 1 ? entry->c.items[0] : NULL;
        const json_t *value = str2_is_arr(entry) && entry->c.count >= 2 ? entry->c.items[1] : NULL;
        bool is_compaction = kind && kind->type == JSON_STRING &&
            strcmp(kind->str_val ? kind->str_val : "", STR2_KIND_COMPACTION) == 0;
        if (is_compaction) {
            json_t *m = json_object();
            json_set(m, "role", json_string("system"));
            json_set(m, "content", json_string(STR2_MARK_TEXT));
            json_t *dm = json_object();
            json_set(dm, STR2_MARK_KEY, json_number((double)handoffs->c.count));
            json_set(m, "display_metadata", dm);
            json_append(marked, m);
            json_append(handoffs, json_copy(value ? value : json_null()));
        } else {
            json_append(marked, json_copy(value ? value : json_null()));
        }
    }
    json_t *args = json_object();
    json_set(args, "marked", json_copy(marked));
    json_t *proj = str2_call(e, "project", args);
    if (str2_prop(e, proj)) { json_free(marked); json_free(handoffs); return NULL; }
    json_t *rows = json_array();
    if (str2_is_arr(proj)) {
        for (size_t i = 0; i < proj->c.count; i++) {
            json_t *row = proj->c.items[i];
            double idx = 0;
            bool has = str2_mark_index_core(e, row, &idx);
            if (e->exc) { json_free(proj); json_free(marked); json_free(handoffs); json_free(rows); return NULL; }
            if (has) {
                long li = (long)idx;
                if (li < 0) li += (long)handoffs->c.count;
                if (li >= 0 && (size_t)li < handoffs->c.count) {
                    json_append(rows, json_copy(json_get(handoffs, (size_t)li)));
                    continue;
                }
            }
            json_t *nr = json_copy(row);
            json_set(nr, "row_kind", json_string(STR2_KIND_MESSAGE));
            json_append(rows, nr);
        }
    }
    json_free(proj);
    json_free(marked);
    json_free(handoffs);
    return rows;
}
