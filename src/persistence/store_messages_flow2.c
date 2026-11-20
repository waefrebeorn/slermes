/*
 * store_messages_flow2.c - FLOW cores of the UNCLAIMED pure-leaf remainder of
 * persistence/sqlite/conversation/_store/messages.py.
 *
 * Claim audit (CRLF-aware PoP grep, 2026-11-20): 13 of this module's defs are
 * ALREADY claimed by hermes_state_*.c / web_server_*.c / session_search_tool.c
 * (get_messages, get_messages_around, clear_messages, replace_messages,
 * rewind_to_message, _insert_message_rows, get_active_message_ids,
 * get_resume_conversations, get_ancestor_display_prefix, latest_message_row_id,
 * message_count, set_message_reaction, _decode_content). Those are NOT touched
 * here. The ten defs below carry fresh sm2_* PoP claims and were verified
 * UNCLAIMED:
 *   _reasoning_json_text, _encode_content, _encode_display_metadata,
 *   _decode_display_metadata, _reaction_list, _fold_summary_carrier,
 *   _active_clause, _exact_replayed_user_clone_key, _is_model_only_row,
 *   _is_explicit_fork_child_row.
 * The remaining unclaimed defs are DB/seam-heavy (conn.execute, cursor doubles,
 * write_txn) and are out of scope for this pure-leaf FLOW port.
 *
 * Decision sequences only: all ten leaves are value transforms whose only
 * collaborators are seeded constants and two tiny pure helpers (_json_or,
 * _sanitize_surrogates) plus json/logger. The universal seam sm2_call is
 * carried for contract parity but no leaf reaches it, so the trace is empty on
 * both sides. Full builtin exception hierarchy is carried for faithful error
 * projection.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sm2_-prefixed (no link collision with the 13
 * sibling definitions or the other flow ports).
 *
 * Representation contract: NULL json_t* == Python None; a raise is
 * {"__raise__": cls, "message": str(exc)}; a Python float is
 * {"__float__":true,"v":n}, a tuple {"__tuple__":true,"v":[...]}, and a
 * reserved-prefix ("\x00json:") string is {"__prefixed__":<rest>} (libjson
 * cannot carry a NUL byte in a C string). Mutating leaves (_fold_summary_carrier)
 * rewrite their dict arg in place; the harness echoes it under "state".
 *
 * Constant provenance (quarry, verified):
 *   _CONTENT_JSON_PREFIX        core.py:167            "\x00json:"
 *   REACTIONS_METADATA_KEY      core.py:170            "reactions"
 *   _DISPLAY_ACTIVE_CLAUSE      messages_sql.py:52     " AND (active = 1 OR compacted = 1)"
 *   _INVALID                    messages_sql.py:92     object() sentinel
 *   MODEL_ONLY_DISPLAY_METADATA_KEY turns.py:221       "model_only"
 *   _json_or / _sanitize_surrogates: common.py:390 / rules.py:82
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
typedef struct sm2_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sm2_ctx_t;

/* -- seeded constants -- */
#define SM2_PREFIX_REST      "json:"          /* prefixed string is '\0' + this */
#define SM2_REACTIONS_KEY    "reactions"
#define SM2_DISPLAY_ACTIVE   " AND (active = 1 OR compacted = 1)"
#define SM2_MODEL_ONLY_KEY   "model_only"
#define SM2_PREFIX_KEY       "__prefixed__"
#define SM2_INVALID_KEY      "__invalid__"

/* -- basic type helpers -- */
static bool sm2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sm2_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}
static bool sm2_is_tuple_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__tuple__", false);
}
static bool sm2_is_prefix_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, SM2_PREFIX_KEY) != NULL;
}
static bool sm2_is_invalid(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, SM2_INVALID_KEY, false);
}
static bool sm2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sm2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sm2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool sm2_is_num(const json_t *v) {
    return v && v->type == JSON_NUMBER && !sm2_is_bool(v);
}
static bool sm2_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sm2_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sm2_is_float_tag(v) &&
           !sm2_is_tuple_tag(v) && !sm2_is_prefix_tag(v) && !sm2_is_invalid(v);
}
static const char *sm2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool sm2_eq_str(const json_t *v, const char *lit) {
    return sm2_is_str(v) && v->str_val && strcmp(v->str_val, lit) == 0;
}
static bool sm2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL || sm2_is_invalid(v)) return false;
    if (sm2_is_float_tag(v)) return json_get_num(v, "v", 0) != 0.0;
    if (sm2_is_tuple_tag(v)) {
        const json_t *it = json_obj_get(v, "v");
        return it && it->type == JSON_ARRAY && it->c.count > 0;
    }
    if (sm2_is_prefix_tag(v)) {
        const json_t *r = json_obj_get(v, SM2_PREFIX_KEY);
        return sm2_is_str(r) && r->str_val && r->str_val[0] != '\0';
    }
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *sm2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL || sm2_is_invalid(v)) return "NoneType";
    if (sm2_is_float_tag(v)) return "float";
    if (sm2_is_tuple_tag(v)) return "tuple";
    if (sm2_is_prefix_tag(v)) return "str";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* Python == on values (numeric cross-type, deep list/dict, list != tuple). */
static bool sm2_eq(const json_t *a, const json_t *b) {
    if (sm2_is_null(a) || sm2_is_null(b)) return sm2_is_null(a) && sm2_is_null(b);
    bool an = sm2_is_num(a) || sm2_is_bool(a) || sm2_is_float_tag(a);
    bool bn = sm2_is_num(b) || sm2_is_bool(b) || sm2_is_float_tag(b);
    if (an || bn) {
        if (!an || !bn) return false;
        double da = sm2_is_float_tag(a) ? json_get_num(a, "v", 0)
                  : sm2_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double db = sm2_is_float_tag(b) ? json_get_num(b, "v", 0)
                  : sm2_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return da == db;
    }
    bool as = sm2_is_str(a) || sm2_is_prefix_tag(a);
    bool bs = sm2_is_str(b) || sm2_is_prefix_tag(b);
    if (as || bs) {
        if (!as || !bs) return false;
        if (sm2_is_prefix_tag(a) != sm2_is_prefix_tag(b)) return false;
        const json_t *sa = sm2_is_prefix_tag(a) ? json_obj_get(a, SM2_PREFIX_KEY) : a;
        const json_t *sb = sm2_is_prefix_tag(b) ? json_obj_get(b, SM2_PREFIX_KEY) : b;
        const char *xa = sm2_str(sa), *xb = sm2_str(sb);
        if (sm2_is_prefix_tag(a)) {
            /* compare the '\0json:' + rest logical value via rest equality */
            return xa && xb && strcmp(xa, xb) == 0;
        }
        return xa && xb && strcmp(xa, xb) == 0;
    }
    bool aq = sm2_is_list(a) || sm2_is_tuple_tag(a);
    bool bq = sm2_is_list(b) || sm2_is_tuple_tag(b);
    if (aq || bq) {
        if (!aq || !bq) return false;
        if (sm2_is_tuple_tag(a) != sm2_is_tuple_tag(b)) return false;
        const json_t *ia = sm2_is_tuple_tag(a) ? json_obj_get(a, "v") : a;
        const json_t *ib = sm2_is_tuple_tag(b) ? json_obj_get(b, "v") : b;
        if (!ia || !ib || ia->c.count != ib->c.count) return false;
        for (size_t i = 0; i < ia->c.count; i++)
            if (!sm2_eq(json_get(ia, i), json_get(ib, i))) return false;
        return true;
    }
    if (sm2_is_dict(a) && sm2_is_dict(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !sm2_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* -- exceptions -- */
static json_t *sm2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *sm2_err_fmt(const char *type, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return sm2_err(type, buf);
}

/* FULL builtin exception hierarchy (py3.14). Unknown scripted names parent to
 * Exception; SystemExit/KeyboardInterrupt/GeneratorExit parent to BaseException
 * ONLY so they pass through every except-Exception clause. */
typedef struct { const char *name; const char *parent; } sm2_exc_ent_t;
static const sm2_exc_ent_t SM2_EXC_TABLE[] = {
    { "BaseException", NULL }, { "Exception", "BaseException" },
    { "TypeError", "Exception" }, { "StopIteration", "Exception" },
    { "StopAsyncIteration", "Exception" }, { "ArithmeticError", "Exception" },
    { "FloatingPointError", "ArithmeticError" }, { "OverflowError", "ArithmeticError" },
    { "ZeroDivisionError", "ArithmeticError" }, { "AssertionError", "Exception" },
    { "AttributeError", "Exception" }, { "BufferError", "Exception" },
    { "EOFError", "Exception" }, { "ImportError", "Exception" },
    { "ModuleNotFoundError", "ImportError" }, { "LookupError", "Exception" },
    { "IndexError", "LookupError" }, { "KeyError", "LookupError" },
    { "MemoryError", "Exception" }, { "NameError", "Exception" },
    { "UnboundLocalError", "NameError" }, { "OSError", "Exception" },
    { "BlockingIOError", "OSError" }, { "ChildProcessError", "OSError" },
    { "ConnectionError", "OSError" }, { "BrokenPipeError", "ConnectionError" },
    { "ConnectionAbortedError", "ConnectionError" }, { "ConnectionRefusedError", "ConnectionError" },
    { "ConnectionResetError", "ConnectionError" }, { "FileExistsError", "OSError" },
    { "FileNotFoundError", "OSError" }, { "InterruptedError", "OSError" },
    { "IsADirectoryError", "OSError" }, { "NotADirectoryError", "OSError" },
    { "PermissionError", "OSError" }, { "ProcessLookupError", "OSError" },
    { "TimeoutError", "OSError" }, { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" }, { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" }, { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" }, { "TabError", "IndentationError" },
    { "SystemError", "Exception" }, { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" }, { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" }, { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" }, { "DeprecationWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" }, { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" }, { "UserWarning", "Warning" },
    { "FutureWarning", "Warning" }, { "ImportWarning", "Warning" },
    { "UnicodeWarning", "Warning" }, { "BytesWarning", "Warning" },
    { "EncodingWarning", "Warning" }, { "ResourceWarning", "Warning" },
    { "ExceptionGroup", "Exception" }, { "BaseExceptionGroup", "BaseException" },
    { "GeneratorExit", "BaseException" }, { "KeyboardInterrupt", "BaseException" },
    { "SystemExit", "BaseException" },
};
#define SM2_EXC_TABLE_N (sizeof(SM2_EXC_TABLE) / sizeof(SM2_EXC_TABLE[0]))
static const char *sm2_exc_parent(const char *cls) {
    for (size_t i = 0; i < SM2_EXC_TABLE_N; i++)
        if (strcmp(SM2_EXC_TABLE[i].name, cls) == 0) return SM2_EXC_TABLE[i].parent;
    return NULL;
}
static bool sm2_exc_is_a(const char *cls, const char *target) {
    if (!cls || !target) return false;
    char cur[128];
    snprintf(cur, sizeof cur, "%s", cls);
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cur, target) == 0) return true;
        const char *parent = sm2_exc_parent(cur);
        if (!parent) {
            if (strcmp(cur, "BaseException") == 0) return false;
            parent = (strcmp(cur, "Exception") == 0) ? "BaseException" : "Exception";
        }
        snprintf(cur, sizeof cur, "%s", parent);
    }
    return false;
}
typedef struct { int en; const char *cls; } sm2_errno_map_t;
static const sm2_errno_map_t SM2_ERRNO_MAP[] = {
    { 1, "PermissionError" }, { 2, "FileNotFoundError" },
    { 3, "ProcessLookupError" }, { 4, "InterruptedError" },
    { 10, "ChildProcessError" }, { 11, "BlockingIOError" },
    { 13, "PermissionError" }, { 17, "FileExistsError" },
    { 20, "NotADirectoryError" }, { 21, "IsADirectoryError" }, { 32, "BrokenPipeError" },
#ifdef _WIN32
    { 138, "TimeoutError" }, { 10053, "ConnectionAbortedError" },
    { 10054, "ConnectionResetError" }, { 10058, "BrokenPipeError" },
    { 10060, "TimeoutError" }, { 10061, "ConnectionRefusedError" },
#else
    { 103, "ConnectionAbortedError" }, { 104, "ConnectionResetError" },
    { 110, "TimeoutError" }, { 111, "ConnectionRefusedError" },
#endif
};
#define SM2_ERRNO_MAP_N (sizeof(SM2_ERRNO_MAP) / sizeof(SM2_ERRNO_MAP[0]))
static json_t *sm2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    const json_t *en = spec ? json_obj_get(spec, "errno") : NULL;
    if (en && en->type == JSON_NUMBER && sm2_exc_is_a(type, "OSError")) {
        if (strcmp(type, "OSError") == 0)
            for (size_t i = 0; i < SM2_ERRNO_MAP_N; i++)
                if (SM2_ERRNO_MAP[i].en == (int)en->num_val) { type = SM2_ERRNO_MAP[i].cls; break; }
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = sm2_err(type, buf);
        json_set(e, "errno", json_number(en->num_val));
        return e;
    }
    return sm2_err(type, msg);
}

/* The one universal seam (carried for contract parity; no leaf calls it). */
static json_t *sm2_call(sm2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sm2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sm2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = sm2_make_exc(raise);
            else { json_t *r = json_obj_get(used, "ret"); ret = r ? json_copy(r) : json_null(); }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* -- output projection -- */
static json_t *sm2_proj(const json_t *v) {
    if (!v) return json_null();
    if (sm2_is_float_tag(v)) return json_number(json_get_num(v, "v", 0));
    if (sm2_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *it = json_obj_get(v, "v");
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++) json_append(a, sm2_proj(json_get(it, i)));
        return a;
    }
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++) json_set(o, v->c.keys[i], sm2_proj(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, sm2_proj(json_get(v, i)));
        return a;
    }
    return json_copy(v);
}

/* -- UTF-8 walker -- */
static size_t sm2_utf8_decode(const char *s, unsigned int *cp) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0 && ((unsigned char)s[1] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(c & 0x1F) << 6) | (unsigned char)(s[1] & 0x3F); return 2;
    }
    if ((c & 0xF0) == 0xE0 && ((unsigned char)s[1] & 0xC0) == 0x80 &&
        ((unsigned char)s[2] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(c & 0x0F) << 12) | ((unsigned int)(s[1] & 0x3F) << 6) |
              (unsigned char)(s[2] & 0x3F); return 3;
    }
    if ((c & 0xF8) == 0xF0 && ((unsigned char)s[1] & 0xC0) == 0x80 &&
        ((unsigned char)s[2] & 0xC0) == 0x80 && ((unsigned char)s[3] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(c & 0x07) << 18) | ((unsigned int)(s[1] & 0x3F) << 12) |
              ((unsigned int)(s[2] & 0x3F) << 6) | (unsigned char)(s[3] & 0x3F); return 4;
    }
    *cp = c; return 1;
}
static size_t sm2_utf8_encode(unsigned int cp, char *buf) {
    if (cp < 0x80) { buf[0] = (char)cp; return 1; }
    if (cp < 0x800) { buf[0] = (char)(0xC0 | (cp >> 6)); buf[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) { buf[0] = (char)(0xE0 | (cp >> 12)); buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    buf[0] = (char)(0xF0 | (cp >> 18)); buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[3] = (char)(0x80 | (cp & 0x3F)); return 4;
}
/* _sanitize_surrogates: replace lone surrogate code points with U+FFFD. */
static char *sm2_sanitize_surrogates(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 8);
    if (!out) return strdup(s);
    size_t k = 0;
    for (size_t i = 0; i < n; ) {
        unsigned int cp;
        size_t w = sm2_utf8_decode(s + i, &cp);
        if (cp >= 0xD800 && cp <= 0xDFFF) { memcpy(out + k, "\xEF\xBF\xBD", 3); k += 3; }
        else { memcpy(out + k, s + i, w); k += w; }
        i += w;
    }
    out[k] = '\0';
    return out;
}

/* -- growable string builder -- */
typedef struct { char *p; size_t len, cap; } sm2_buf_t;
static void sm2_buf_reserve(sm2_buf_t *b, size_t need) {
    if (b->p && b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 128;
    while (nc < b->len + need + 1) nc *= 2;
    char *np = realloc(b->p, nc);
    if (!np) return;
    b->p = np; b->cap = nc;
    if (b->len == 0) b->p[0] = '\0';
}
static void sm2_buf_putc(sm2_buf_t *b, char c) {
    sm2_buf_reserve(b, 1);
    if (!b->p) return;
    b->p[b->len++] = c; b->p[b->len] = '\0';
}
static void sm2_buf_puts(sm2_buf_t *b, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    sm2_buf_reserve(b, n);
    if (!b->p) return;
    memcpy(b->p + b->len, s, n);
    b->len += n; b->p[b->len] = '\0';
}

/* escape a C string into a JSON string token WITHOUT surrounding quotes */
static void sm2_esc_body(sm2_buf_t *b, const char *s, bool ascii) {
    while (*s) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"': sm2_buf_puts(b, "\\\""); s++; break;
            case '\\': sm2_buf_puts(b, "\\\\"); s++; break;
            case '\b': sm2_buf_puts(b, "\\b"); s++; break;
            case '\f': sm2_buf_puts(b, "\\f"); s++; break;
            case '\n': sm2_buf_puts(b, "\\n"); s++; break;
            case '\r': sm2_buf_puts(b, "\\r"); s++; break;
            case '\t': sm2_buf_puts(b, "\\t"); s++; break;
            default:
                if (c < 0x20) {
                    char t[8]; snprintf(t, sizeof t, "\\u%04x", c); sm2_buf_puts(b, t); s++;
                } else if (c < 0x80) {
                    sm2_buf_putc(b, (char)c); s++;
                } else {
                    unsigned int cp; size_t w = sm2_utf8_decode(s, &cp);
                    if (ascii) {
                        if (cp <= 0xFFFF) { char t[8]; snprintf(t, sizeof t, "\\u%04x", cp); sm2_buf_puts(b, t); }
                        else { unsigned int v = cp - 0x10000;
                               char t[16]; snprintf(t, sizeof t, "\\u%04x\\u%04x",
                                    0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF)); sm2_buf_puts(b, t); }
                    } else { char t[5]; size_t k = sm2_utf8_encode(cp, t); t[k] = '\0'; sm2_buf_puts(b, t); }
                    s += w;
                }
        }
    }
}
static void sm2_esc(sm2_buf_t *b, const char *s, bool ascii) {
    sm2_buf_putc(b, '"');
    sm2_esc_body(b, s, ascii);
    sm2_buf_putc(b, '"');
}

/* -- Python json.dumps serializer (ASCII/JSON domain) -- */
typedef struct { bool ascii; bool sort_keys; const char *isep; const char *ksep; } sm2_dumpcfg_t;
static int sm2_keycmp(const void *pa, const void *pb, void *ctx) {
    (void)ctx;
    const json_t *o = (const json_t *)ctx;
    size_t a = *(const size_t *)pa, b = *(const size_t *)pb;
    return strcmp(o->c.keys[a], o->c.keys[b]);
}
static void sm2_ser(sm2_buf_t *b, const json_t *v, const sm2_dumpcfg_t *cfg) {
    if (!v || v->type == JSON_NULL || sm2_is_invalid(v)) { sm2_buf_puts(b, "null"); return; }
    if (sm2_is_float_tag(v)) {
        double d = json_get_num(v, "v", 0);
        char t[64];
        if (d == (double)(long long)d) snprintf(t, sizeof t, "%.1f", d);
        else snprintf(t, sizeof t, "%.15g", d);
        sm2_buf_puts(b, t); return;
    }
    if (sm2_is_prefix_tag(v)) {
        const json_t *r = json_obj_get(v, SM2_PREFIX_KEY);
        sm2_buf_putc(b, '"'); sm2_buf_puts(b, "\\u0000" SM2_PREFIX_REST);
        sm2_esc_body(b, sm2_str(r) ? sm2_str(r) : "", cfg->ascii);
        sm2_buf_putc(b, '"'); return;
    }
    if (sm2_is_tuple_tag(v)) {
        const json_t *it = json_obj_get(v, "v");
        sm2_buf_putc(b, '[');
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++) {
                if (i) sm2_buf_puts(b, cfg->isep);
                sm2_ser(b, json_get(it, i), cfg);
            }
        sm2_buf_putc(b, ']'); return;
    }
    switch (v->type) {
        case JSON_BOOL: sm2_buf_puts(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char t[64];
            if (v->num_val == (double)(long long)v->num_val) snprintf(t, sizeof t, "%.0f", v->num_val);
            else snprintf(t, sizeof t, "%.17g", v->num_val);
            sm2_buf_puts(b, t); break;
        }
        case JSON_STRING: sm2_esc(b, v->str_val, cfg->ascii); break;
        case JSON_ARRAY:
            sm2_buf_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sm2_buf_puts(b, cfg->isep);
                sm2_ser(b, json_get(v, i), cfg);
            }
            sm2_buf_putc(b, ']'); break;
        case JSON_OBJECT: {
            size_t n = v->c.count;
            size_t *idx = n ? malloc(n * sizeof *idx) : NULL;
            for (size_t i = 0; i < n; i++) idx[i] = i;
            if (cfg->sort_keys && n > 1)
                /* insertion sort keeps it portable; pointer ctx carries the object */
                for (size_t i = 1; i < n; i++) {
                    size_t cur = idx[i]; size_t j = i;
                    while (j > 0 && strcmp(v->c.keys[idx[j - 1]], v->c.keys[cur]) > 0) {
                        idx[j] = idx[j - 1]; j--;
                    }
                    idx[j] = cur;
                }
            sm2_buf_putc(b, '{');
            for (size_t i = 0; i < n; i++) {
                if (i) sm2_buf_puts(b, cfg->isep);
                size_t k = idx ? idx[i] : i;
                sm2_esc(b, v->c.keys[k], cfg->ascii);
                sm2_buf_puts(b, cfg->ksep);
                sm2_ser(b, v->c.items[k], cfg);
            }
            sm2_buf_putc(b, '}');
            free(idx); break;
        }
    }
}
/* returns a malloc'd Python-json.dumps string (value, not JSON-escaped) */
static char *sm2_dumps_ex(const json_t *v, bool ascii, bool sort_keys,
                          const char *isep, const char *ksep) {
    sm2_buf_t b = {0};
    sm2_dumpcfg_t cfg = { ascii, sort_keys, isep, ksep };
    sm2_ser(&b, v, &cfg);
    if (!b.p) return strdup("");
    return b.p;
}
static char *sm2_dumps_default(const json_t *v) {
    return sm2_dumps_ex(v, true, false, ", ", ": ");
}
/* Python json.loads; NULL on failure (mirrors JSONDecodeError/TypeError). */
static json_t *sm2_loads(const char *s) {
    if (!s) return NULL;
    return json_parse(s, NULL);
}
static json_t *sm2_invalid(void) {
    json_t *o = json_object();
    json_set(o, SM2_INVALID_KEY, json_bool(true));
    return o;
}
static char *sm2_key_of(const json_t *v) {
    if (sm2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    if (sm2_is_bool(v)) return strdup(v->bool_val ? "true" : "false");
    if (sm2_is_num(v)) { char t[64]; snprintf(t, sizeof t, "%.0f", v->num_val); return strdup(t); }
    return strdup("null");
}

/* ============== messages.py - unclaimed pure leaves ============== */

/* `json.dumps(value)` for the TEXT column; strings are stored as-is. */
static json_t *sm2_reasoning_json_text_impl(const json_t *v) {
    if (!sm2_truthy(v)) return json_null();
    if (sm2_is_str(v)) return json_copy((json_t *)v);
    char *s = sm2_dumps_default(v);
    json_t *r = s ? json_string(s) : json_null();
    free(s);
    return r;
}
/* PoP: sm2_reasoning_json_text @ persistence/sqlite/conversation/_store/messages.py:_reasoning_json_text */
static json_t *sm2_reasoning_json_text(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_reasoning_json_text_impl(json_obj_get(args, "value"));
}

/* _encode_content: plain str (surrogates scrubbed, reserved prefix escaped),
 * scalars unchanged, otherwise prefix + json.dumps. The NUL-bearing result is
 * carried as the {"__prefixed__": rest} transport tag. */
static json_t *sm2_encode_content_impl(const json_t *c) {
    if (sm2_is_str(c)) {
        char *sc = sm2_sanitize_surrogates(c->str_val);
        bool pref = sc && sc[0] == '\0' && strcmp(sc + 1, SM2_PREFIX_REST) == 0;
        if (!pref) {
            json_t *r = json_string(sc ? sc : "");
            free(sc);
            return r;
        }
        free(sc);
        /* a literal prefixed string falls through and is double-encoded */
    } else if (sm2_is_null(c) || sm2_is_bool(c) || sm2_is_num(c) || sm2_is_float_tag(c)) {
        return json_copy((json_t *)c);
    }
    char *d = sm2_dumps_default(c);
    if (!d) {
        const char *s = sm2_str(c);
        char *z = sm2_sanitize_surrogates(s ? s : "");
        json_t *r = json_string(z);
        free(z);
        return r;
    }
    json_t *tag = json_object();
    json_set_hdr(tag, SM2_PREFIX_KEY, d);
    free(d);
    return tag;
}
/* PoP: sm2_encode_content @ persistence/sqlite/conversation/_store/messages.py:_encode_content */
static json_t *sm2_encode_content(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_encode_content_impl(json_obj_get(args, "content"));
}

/* _json_or(raw, fallback, warning): json.loads(raw) or fallback. */
static json_t *sm2_json_or_impl(const json_t *raw, json_t *fallback) {
    if (!sm2_is_str(raw)) return fallback;
    json_t *p = sm2_loads(raw->str_val);
    return p ? p : fallback;
}
/* _encode_display_metadata: falsy->None, str->loads (object only), dict->dumps. */
static json_t *sm2_encode_display_metadata_impl(const json_t *dm) {
    if (!sm2_truthy(dm)) return json_null();
    json_t *obj = NULL;
    bool owned = false;
    if (sm2_is_str(dm)) {
        obj = sm2_json_or_impl(dm, sm2_invalid());
        owned = true;
        if (sm2_is_invalid(obj)) { json_free(obj); return json_null(); }
        if (!sm2_is_dict(obj)) { json_free(obj); return json_null(); }
    } else if (sm2_is_dict(dm)) {
        obj = (json_t *)dm;
    } else {
        return json_null();
    }
    char *s = sm2_dumps_default(obj);
    if (owned) json_free(obj);
    json_t *r = s ? json_string(s) : json_null();
    free(s);
    return r;
}
/* PoP: sm2_encode_display_metadata @ persistence/sqlite/conversation/_store/messages.py:_encode_display_metadata */
static json_t *sm2_encode_display_metadata(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_encode_display_metadata_impl(json_obj_get(args, "display_metadata"));
}

/* _decode_display_metadata: up to two string layers, then dict-or-None. */
static json_t *sm2_decode_display_metadata_impl(const json_t *raw) {
    if (sm2_is_null(raw)) return json_null();
    const json_t *meta = raw;
    json_t *owned = NULL;
    for (int i = 0; i < 2; i++) {
        if (sm2_is_str(meta)) {
            json_t *p = sm2_loads(meta->str_val);
            if (!p) { if (owned) json_free(owned); return json_null(); }
            if (owned) json_free(owned);
            owned = p;
            meta = p;
        }
    }
    json_t *r = sm2_is_dict(meta) ? json_copy((json_t *)meta) : json_null();
    if (owned) json_free(owned);
    return r;
}
/* PoP: sm2_decode_display_metadata @ persistence/sqlite/conversation/_store/messages.py:_decode_display_metadata */
static json_t *sm2_decode_display_metadata(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_decode_display_metadata_impl(json_obj_get(args, "raw"));
}

/* _reaction_list: the dict entries of meta["reactions"] (a list), else []. */
static json_t *sm2_reaction_list_impl(const json_t *meta) {
    const json_t *base = sm2_truthy(meta) ? meta : NULL;
    if (base && !sm2_is_dict(base))
        return sm2_err_fmt("AttributeError", "'%s' object has no attribute 'get'",
                           sm2_type_name(base));
    const json_t *reactions = base ? json_obj_get(base, SM2_REACTIONS_KEY) : NULL;
    json_t *out = json_array();
    if (sm2_is_list(reactions))
        for (size_t i = 0; i < reactions->c.count; i++) {
            const json_t *r = json_get(reactions, i);
            if (sm2_is_dict(r)) json_append(out, json_copy((json_t *)r));
        }
    return out;
}
/* PoP: sm2_reaction_list @ persistence/sqlite/conversation/_store/messages.py:_reaction_list */
static json_t *sm2_reaction_list(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_reaction_list_impl(json_obj_get(args, "meta"));
}

/* _fold_summary_carrier(row, key, first_by_uid): mutates first_by_uid. */
static json_t *sm2_fold_summary_carrier_impl(const json_t *row, const json_t *key,
                                             json_t *first) {
    if (!sm2_is_dict(row) || !json_obj_get(row, "message_uid"))
        return sm2_err("KeyError", "'message_uid'");
    const json_t *uid = json_obj_get(row, "message_uid");
    if (!sm2_truthy(uid)) return json_copy((json_t *)key);
    const json_t *cs = json_obj_get(row, "_compressed_summary");
    if (!cs) return sm2_err("KeyError", "'_compressed_summary'");
    if (sm2_truthy(cs)) {
        char *ks = sm2_key_of(uid);
        const json_t *v = (first && sm2_is_dict(first)) ? json_obj_get(first, ks) : NULL;
        free(ks);
        return v ? json_copy((json_t *)v) : json_copy((json_t *)key);
    }
    if (first && sm2_is_dict(first)) {
        char *ks = sm2_key_of(uid);
        if (!json_obj_get(first, ks)) json_set(first, ks, json_copy((json_t *)key));
        free(ks);
    }
    return json_copy((json_t *)key);
}
/* PoP: sm2_fold_summary_carrier @ persistence/sqlite/conversation/_store/messages.py:_fold_summary_carrier */
static json_t *sm2_fold_summary_carrier(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_fold_summary_carrier_impl(json_obj_get(args, "row"),
                                         json_obj_get(args, "key"),
                                         json_obj_get(args, "first_by_uid"));
}

/* _active_clause. */
static json_t *sm2_active_clause_impl(const json_t *ina, const json_t *inc) {
    if (sm2_truthy(ina)) return json_string("");
    if (sm2_truthy(inc)) return json_string(SM2_DISPLAY_ACTIVE);
    return json_string(" AND active = 1");
}
/* PoP: sm2_active_clause @ persistence/sqlite/conversation/_store/messages.py:_active_clause */
static json_t *sm2_active_clause(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_active_clause_impl(json_obj_get(args, "include_inactive"),
                                  json_obj_get(args, "include_compacted"));
}

/* _exact_replayed_user_clone_key: (timestamp, json.dumps(content, ensure_ascii=
 * False, sort_keys=True, separators=(",",":"))) or None. */
static json_t *sm2_exact_replayed_user_clone_key_impl(const json_t *ts, const json_t *c) {
    if (sm2_is_null(ts)) return json_null();
    if (sm2_is_null(c) || (sm2_is_str(c) && c->str_val && c->str_val[0] == '\0') ||
        (sm2_is_list(c) && c->c.count == 0)) return json_null();
    char *s = sm2_dumps_ex(c, false, true, ",", ":");
    if (!s) return json_null();
    json_t *tup = json_object();
    json_set(tup, "__tuple__", json_bool(true));
    json_t *v = json_array();
    json_append(v, json_copy((json_t *)ts));
    json_append(v, json_string(s));
    json_set(tup, "v", v);
    free(s);
    return tup;
}
/* PoP: sm2_exact_replayed_user_clone_key @ persistence/sqlite/conversation/_store/messages.py:_exact_replayed_user_clone_key */
static json_t *sm2_exact_replayed_user_clone_key(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_exact_replayed_user_clone_key_impl(json_obj_get(args, "timestamp"),
                                                   json_obj_get(args, "content"));
}

/* _is_model_only_row: bool((_decode_display_metadata(row["display_metadata"])
 * or {}).get("model_only")). */
static json_t *sm2_is_model_only_row_impl(const json_t *row) {
    if (!sm2_is_dict(row) || !json_obj_get(row, "display_metadata"))
        return sm2_err("KeyError", "'display_metadata'");
    json_t *md = sm2_decode_display_metadata_impl(json_obj_get(row, "display_metadata"));
    bool flag = false;
    if (sm2_is_dict(md)) flag = sm2_truthy(json_obj_get(md, SM2_MODEL_ONLY_KEY));
    json_free(md);
    return json_bool(flag);
}
/* PoP: sm2_is_model_only_row @ persistence/sqlite/conversation/_store/messages.py:_is_model_only_row */
static json_t *sm2_is_model_only_row(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_is_model_only_row_impl(json_obj_get(args, "row"));
}

/* _is_explicit_fork_child_row. */
static json_t *sm2_is_explicit_fork_child_row_impl(const json_t *session, const json_t *inc) {
    if (!sm2_is_dict(session))
        return sm2_err_fmt("AttributeError", "'%s' object has no attribute 'get'",
                           sm2_type_name(session));
    if (sm2_eq_str(json_obj_get(session, "source"), "tool")) return json_bool(true);
    const json_t *cfg = json_obj_get(session, "model_config");
    json_t *owned = NULL;
    if (sm2_is_str(cfg)) {
        owned = sm2_loads(cfg->str_val);
        if (!owned) return json_bool(false);
        cfg = owned;
    }
    if (!sm2_is_dict(cfg)) { if (owned) json_free(owned); return json_bool(false); }
    const json_t *b = json_obj_get(cfg, "_branched_from");
    const json_t *d = json_obj_get(cfg, "_delegate_from");
    const json_t *r = sm2_truthy(inc) ? json_obj_get(cfg, "_reset_from") : NULL;
    const json_t *pid = json_obj_get(session, "parent_session_id");
    bool result;
    if (sm2_truthy(pid))
        result = sm2_eq(pid, b) || sm2_eq(pid, d) || (r && sm2_eq(pid, r));
    else
        result = !sm2_is_null(b) || !sm2_is_null(d) || (r && !sm2_is_null(r));
    if (owned) json_free(owned);
    return json_bool(result);
}
/* PoP: sm2_is_explicit_fork_child_row @ persistence/sqlite/conversation/_store/messages.py:_is_explicit_fork_child_row */
static json_t *sm2_is_explicit_fork_child_row(sm2_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sm2_is_explicit_fork_child_row_impl(json_obj_get(args, "session"),
                                               json_obj_get(args, "include_reset"));
}
