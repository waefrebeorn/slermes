/*
 * store_identity_flow.c - FLOW cores + leaves of the conversation store's
 * message-identity codec. Defining module:
 *   persistence/sqlite/conversation/_store/identity.py
 * ("The codec between the live message-identity keys and the identity columns
 * of a message row."). All 12 identity.py defs are ported and credited there
 * (each verified UNCLAIMED by PoP grep, by name and by module). The two
 * unclaimed pure leaves identity.py imports are ported too and credited to
 * their REAL defining modules:
 *   _json_or              @ persistence/sqlite/_state/common.py
 *   coalesce_tool_call_id @ session_rules/turns.py  (_tc_field transcribed
 *                           inline as its helper, no separate claim)
 * 14 ported functions total. The CLAIMED sibling uid_list
 * (session_rules/turns.py:uid_list, ported by store_rules_flow.c) is SEAMED,
 * not re-ported.
 *
 * Decision sequences only: every external collaborator enters through ONE
 * universal seam: si_call(ctx, name, args). The harness wires it to a recording
 * stub fed the case's scripted results (in.io); the oracle installs recording
 * stubs emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. Seamed collaborators:
 *   uid_list        (session_rules.turns.uid_list, claimed sibling; _uid_list
 *                    and _restore_identity_columns)
 *   logger.warning  (persistence.sqlite._state.common logger, _json_or's
 *                    deserialization-failure warning; single positional arg)
 * Ported leaves call each other DIRECTLY (no seam): _tool_call_uid_map ->
 * _live_or_column + _uid_map; _absorbed_uids_json -> _live_or_column +
 * _uid_list; _tool_call_uids_json -> _tool_call_uid_map; _tool_call_uid_or_none
 * -> _live_or_column; _fill_missing_tool_call_uids -> coalesce_tool_call_id +
 * _uid_map; _restore_row_identity -> _uid_map + _live_extends;
 * _restore_identity_columns -> _restore_row_identity + _uid_list;
 * _stable_tool_key -> _json_or + coalesce_tool_call_id; _uid_list/_uid_map ->
 * _json_or. json.loads / json.dumps are PURE stdlib and transcribed inline
 * (a strict Python-grammar parser and an ensure_ascii encoder), not seamed.
 * Constants seeded, not seamed: MESSAGE_UID, ABSORBED_MESSAGE_UIDS,
 * TOOL_CALL_UIDS (session_rules/turns.py), TOOL_CALL_UID (_store/rules.py).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and si_-prefixed.
 *
 * Representation contract (shared with tests/oracle_store_identity_flow.py):
 * - NULL json_t* == an absent key / Python None; a JSON_NULL node is a real
 *   null VALUE. An exception is a {"__raise__": cls, "message": str(exc)}
 *   object (OSError adds "errno"). BaseException (SystemExit,
 *   KeyboardInterrupt, GeneratorExit) passes through every except clause
 *   (identity.py has NO except-Exception site: the only catch is _json_or's
 *   except (JSONDecodeError, TypeError) around the TRANSCRIBED json.loads;
 *   every seam raise propagates uncaught).
 * - Plain JSON numbers are Python ints; JSON bools are bools. A Python float
 *   travels as {"__float__": true, "v": <number>}; a tuple as
 *   {"__tuple__": true, "v": [...]}. The strict parser tags a parsed number
 *   float iff its literal carries '.'/'e'/'E' (json.loads' parse_int vs
 *   parse_float), so the 'int' vs 'float' TypeError wording on iterating a
 *   parsed bare number in _stable_tool_key stays faithful.
 * - Mutating functions (_fill_missing_tool_call_uids, _restore_row_identity,
 *   _restore_identity_columns) mutate their msg arg in place; the harness
 *   echoes the mutated params under "state" keyed by parameter name (row is
 *   read-only). Non-mutating funcs echo {}.
 * - Tuples fold to arrays and integral floats to ints in canon. Dict key
 *   order is irrelevant to canon; the seam TRACE is ordered. json.dumps
 *   output strings ARE order-sensitive: _tool_call_uids_json sorts keys
 *   (codepoint order); _absorbed_uids_json keeps list order.
 * - Fixture domain (ASCII scope): strings ASCII (control chars ride \u00xx
 *   escapes); uids non-empty ASCII strings; uid_list seam results are lists
 *   of non-empty ASCII strings (its live return shape); uid-map values
 *   non-empty str or non-empty list of non-empty str; row/msg dicts or the
 *   scalar/list/tuple types the type-guard probes name; JSON texts parse
 *   within depth 32, int magnitude <= 2^53, finite floats; NO NaN/Infinity
 *   literals, NO lone-surrogate \u escapes, NO bytes/bytearray inputs, NO
 *   non-ASCII whitespace in strip positions, NO KeyError keys containing
 *   quotes/backslashes (the KeyError message is repr(key)). These exclusions
 *   are outside the live fixture corpus, not behavior differences on it.
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
typedef struct si_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} si_ctx_t;

/* -- constants (seeded, not seamed) -- */
#define SI_MESSAGE_UID            "message_uid"
#define SI_ABSORBED_MESSAGE_UIDS  "_absorbed_message_uids"
#define SI_TOOL_CALL_UIDS         "_tool_call_uids"
#define SI_TOOL_CALL_UID          "_tool_call_uid"

/* -- basic type helpers -- */
static bool si_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool si_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}
static bool si_is_tuple_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__tuple__", false);
}
static bool si_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool si_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool si_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool si_is_int(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool si_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool si_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !si_is_float_tag(v) && !si_is_tuple_tag(v);
}
static bool si_is_numlike(const json_t *v) {
    return si_is_int(v) || si_is_bool(v) || si_is_float_tag(v);
}
static double si_num(const json_t *v) {
    if (si_is_float_tag(v)) return json_get_num(v, "v", 0);
    if (si_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static const char *si_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool si_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (si_is_float_tag(v)) return json_get_num(v, "v", 0) != 0.0;
    if (si_is_tuple_tag(v)) {
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
static const json_t *si_seq_items(const json_t *v) {
    if (si_is_tuple_tag(v)) return json_obj_get(v, "v");
    return v;
}
static bool si_is_seq(const json_t *v) { return si_is_list(v) || si_is_tuple_tag(v); }
static size_t si_seq_len(const json_t *v) {
    const json_t *it = si_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? it->c.count : 0;
}
static const json_t *si_seq_get(const json_t *v, size_t i) {
    const json_t *it = si_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? json_get(it, i) : NULL;
}
/* Python type(x).__name__ on the representation (domain-restricted). */
static const char *si_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (si_is_float_tag(v)) return "float";
    if (si_is_tuple_tag(v)) return "tuple";
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
static bool si_eq(const json_t *a, const json_t *b) {
    bool an = si_is_numlike(a), bn = si_is_numlike(b);
    if (an || bn) return an && bn && si_num(a) == si_num(b);
    if (si_is_null(a) || si_is_null(b)) return si_is_null(a) && si_is_null(b);
    bool as = si_is_str(a), bs = si_is_str(b);
    if (as || bs) return as && bs && strcmp(a->str_val, b->str_val) == 0;
    bool aq = si_is_seq(a), bq = si_is_seq(b);
    if (aq || bq) {
        if (!aq || !bq) return false;
        if (si_is_tuple_tag(a) != si_is_tuple_tag(b)) return false;
        size_t n = si_seq_len(a);
        if (n != si_seq_len(b)) return false;
        for (size_t i = 0; i < n; i++)
            if (!si_eq(si_seq_get(a, i), si_seq_get(b, i))) return false;
        return true;
    }
    if (si_is_dict(a) && si_is_dict(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !si_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}
/* x == "<literal>" for a str x (non-str x is never equal to a str). */
static bool si_eq_str(const json_t *v, const char *lit) {
    return si_is_str(v) && v->str_val && strcmp(v->str_val, lit) == 0;
}

/* -- exceptions -- */
static json_t *si_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *si_err_fmt(const char *type, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return si_err(type, buf);
}

/* FULL builtin exception hierarchy (py3.14). Unknown scripted names parent to
 * Exception, mirroring the oracle's type(kind, (Exception,), {}) fallback;
 * SystemExit/KeyboardInterrupt/GeneratorExit parent to BaseException ONLY so
 * they pass through every except-Exception clause. */
typedef struct { const char *name; const char *parent; } si_exc_ent_t;
static const si_exc_ent_t SI_EXC_TABLE[] = {
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
#define SI_EXC_TABLE_N (sizeof(SI_EXC_TABLE) / sizeof(SI_EXC_TABLE[0]))

static const char *si_exc_parent(const char *cls) {
    for (size_t i = 0; i < SI_EXC_TABLE_N; i++)
        if (strcmp(SI_EXC_TABLE[i].name, cls) == 0) return SI_EXC_TABLE[i].parent;
    return NULL;
}
/* isinstance(exc, target) via the parent chain. */
static bool si_exc_is_a(const char *cls, const char *target) {
    if (!cls || !target) return false;
    char cur[128];
    snprintf(cur, sizeof cur, "%s", cls);
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cur, target) == 0) return true;
        const char *parent = si_exc_parent(cur);
        if (!parent) {
            if (strcmp(cur, "BaseException") == 0) return false;
            /* unknown scripted class: the oracle parents it to Exception */
            parent = (strcmp(cur, "Exception") == 0) ? "BaseException" : "Exception";
        }
        snprintf(cur, sizeof cur, "%s", parent);
    }
    return false;
}
static const char *si_raise_cls(const json_t *e) {
    return json_get_str(e, "__raise__", "RuntimeError");
}
/* Materialize a scripted raise spec exactly as the oracle make_exc does:
 * OSError-family with an errno formats "[Errno N] msg" and carries errno.
 * CPython's OSError.__new__ auto-maps a bare OSError(errno, msg) to the
 * errno's subclass (probed on the quarry host, py3.14 Windows); subclasses
 * are never remapped. The table is host-specific like CPython's own. */
typedef struct { int en; const char *cls; } si_errno_map_t;
static const si_errno_map_t SI_ERRNO_MAP[] = {
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
#define SI_ERRNO_MAP_N (sizeof(SI_ERRNO_MAP) / sizeof(SI_ERRNO_MAP[0]))

static json_t *si_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    const json_t *en = spec ? json_obj_get(spec, "errno") : NULL;
    if (en && en->type == JSON_NUMBER && si_exc_is_a(type, "OSError")) {
        if (strcmp(type, "OSError") == 0) {
            for (size_t i = 0; i < SI_ERRNO_MAP_N; i++) {
                if (SI_ERRNO_MAP[i].en == (int)en->num_val) {
                    type = SI_ERRNO_MAP[i].cls;
                    break;
                }
            }
        }
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = si_err(type, buf);
        json_set(e, "errno", json_number(en->num_val));
        return e;
    }
    return si_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). A missing or
 * exhausted script is a fail-closed RuntimeError on both sides. */
static json_t *si_call(si_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = si_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = si_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = si_make_exc(raise);
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
static json_t *si_proj(const json_t *v) {
    if (!v) return json_null();
    if (si_is_float_tag(v)) return json_number(json_get_num(v, "v", 0));
    if (si_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *it = json_obj_get(v, "v");
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++)
                json_append(a, si_proj(json_get(it, i)));
        return a;
    }
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++)
            json_set(o, v->c.keys[i], si_proj(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, si_proj(json_get(v, i)));
        return a;
    }
    return json_copy(v);
}

/* -- string helpers (ASCII scope) -- */
static bool si_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v' || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}
/* Python str.strip() on the ASCII domain -> malloc'd copy */
static char *si_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && si_is_ws(s[a])) a++;
    while (b > a && si_is_ws(s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

/* -- UTF-8 walker (valid UTF-8; lone-surrogate triplets out of domain) -- */
static size_t si_utf8_decode(const char *s, unsigned int *cp) {
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
static size_t si_utf8_encode(unsigned int cp, char *buf) {
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

/* -- Python json.dumps (ensure_ascii=True, default separators) on the uid
 * domain: str, list[str], dict[str, str|list[str]]. ESCAPE_DCT short escapes
 * for \b\f\n\r\t, \u00xx for other control chars and 0x7f, \" and \\, \uXXXX
 * (lowercase hex, surrogate pairs for astral) for non-ASCII. -- */
typedef struct { char *buf; size_t len, cap; } si_sb_t;
static void si_sb_init(si_sb_t *sb) { sb->buf = malloc(64); sb->len = 0; sb->cap = 64; if (sb->buf) sb->buf[0] = '\0'; }
static void si_sb_putc(si_sb_t *sb, char c) {
    if (sb->len + 2 > sb->cap) {
        sb->cap = sb->cap * 2 + 16;
        char *nb = realloc(sb->buf, sb->cap);
        if (!nb) return;
        sb->buf = nb;
    }
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}
static void si_sb_puts(si_sb_t *sb, const char *s) { while (*s) si_sb_putc(sb, *s++); }
static void si_sb_puthex4(si_sb_t *sb, unsigned int cp) {
    char tmp[7];
    snprintf(tmp, sizeof tmp, "\\u%04x", cp & 0xFFFF);
    si_sb_puts(sb, tmp);
}
static void si_dumps_str_into(si_sb_t *sb, const char *s) {
    si_sb_putc(sb, '"');
    for (size_t i = 0; s[i]; ) {
        unsigned int cp;
        size_t w = si_utf8_decode(s + i, &cp);
        i += w;
        switch (cp) {
            case '"':  si_sb_puts(sb, "\\\""); continue;
            case '\\': si_sb_puts(sb, "\\\\"); continue;
            case '\b': si_sb_puts(sb, "\\b"); continue;
            case '\f': si_sb_puts(sb, "\\f"); continue;
            case '\n': si_sb_puts(sb, "\\n"); continue;
            case '\r': si_sb_puts(sb, "\\r"); continue;
            case '\t': si_sb_puts(sb, "\\t"); continue;
            default: break;
        }
        if (cp < 0x20 || cp == 0x7f) { si_sb_puthex4(sb, cp); continue; }
        if (cp < 0x80) { si_sb_putc(sb, (char)cp); continue; }
        if (cp > 0xFFFF) {
            unsigned int v = cp - 0x10000;
            si_sb_puthex4(sb, 0xD800 + (v >> 10));
            si_sb_puthex4(sb, 0xDC00 + (v & 0x3FF));
        } else {
            si_sb_puthex4(sb, cp);
        }
    }
    si_sb_putc(sb, '"');
}
/* json.dumps(list_of_str) */
static char *si_dumps_uid_list(const json_t *arr) {
    si_sb_t sb; si_sb_init(&sb);
    si_sb_putc(&sb, '[');
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < arr->c.count; i++) {
            if (i) si_sb_puts(&sb, ", ");
            const json_t *u = json_get(arr, i);
            si_dumps_str_into(&sb, si_is_str(u) ? u->str_val : "");
        }
    }
    si_sb_putc(&sb, ']');
    return sb.buf;
}
static int si_key_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* json.dumps(uid_map, sort_keys=True): values are str or list[str]. */
static char *si_dumps_uid_map_sorted(const json_t *obj) {
    si_sb_t sb; si_sb_init(&sb);
    si_sb_putc(&sb, '{');
    if (si_is_dict(obj) && obj->c.count > 0) {
        const char **keys = malloc(obj->c.count * sizeof(char *));
        if (keys) {
            for (size_t i = 0; i < obj->c.count; i++) keys[i] = obj->c.keys[i];
            qsort(keys, obj->c.count, sizeof(char *), si_key_cmp);
            for (size_t i = 0; i < obj->c.count; i++) {
                if (i) si_sb_puts(&sb, ", ");
                si_dumps_str_into(&sb, keys[i]);
                si_sb_puts(&sb, ": ");
                const json_t *v = json_obj_get(obj, keys[i]);
                if (si_is_list(v)) {
                    char *lst = si_dumps_uid_list(v);
                    si_sb_puts(&sb, lst ? lst : "[]");
                    free(lst);
                } else {
                    si_dumps_str_into(&sb, si_is_str(v) ? v->str_val : "");
                }
            }
            free(keys);
        }
    }
    si_sb_putc(&sb, '}');
    return sb.buf;
}

/* -- strict Python json.loads grammar (transcribed, not seamed) --
 * Whitespace is ONLY space/\t/\n/\r; strings reject raw control chars and
 * unknown escapes; \uXXXX needs 4 hex digits (surrogate pairs combine; lone
 * surrogates are out of the ASCII fixture domain and rejected); numbers are
 * -?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)? with no leading zeros / trailing dot /
 * bare exponent; a number literal with '.'/'e'/'E' is a Python float (tagged),
 * else an int. No NaN/Infinity, depth cap 32, int magnitude <= 2^53, finite
 * floats (documented domain). Duplicate object keys: last value wins at the
 * first position (json_set == dict). Returns false where json.loads raises
 * JSONDecodeError; the caller (_json_or) logs + falls back. */
typedef struct { const char *p; int depth; } si_jp_t;
static void si_jp_ws(si_jp_t *j) {
    while (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r') j->p++;
}
static int si_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool si_jp_string(si_jp_t *j, char **out) {
    if (*j->p != '"') return false;
    j->p++;
    size_t cap = 32, len = 0;
    char *buf = malloc(cap);
    if (!buf) return false;
#define SI_JP_EMIT(byte) do { \
        if (len + 2 > cap) { cap = cap * 2 + 16; char *nb = realloc(buf, cap); \
            if (!nb) { free(buf); return false; } buf = nb; } \
        buf[len++] = (char)(byte); \
    } while (0)
    for (;;) {
        unsigned char c = (unsigned char)*j->p;
        if (c == '\0') { free(buf); return false; }
        j->p++;
        if (c == '"') { buf[len] = '\0'; *out = buf; return true; }
        if (c == '\\') {
            char e = *j->p;
            if (!e) { free(buf); return false; }
            j->p++;
            switch (e) {
                case '"': SI_JP_EMIT('"'); break;
                case '\\': SI_JP_EMIT('\\'); break;
                case '/': SI_JP_EMIT('/'); break;
                case 'b': SI_JP_EMIT('\b'); break;
                case 'f': SI_JP_EMIT('\f'); break;
                case 'n': SI_JP_EMIT('\n'); break;
                case 'r': SI_JP_EMIT('\r'); break;
                case 't': SI_JP_EMIT('\t'); break;
                case 'u': {
                    int h0 = si_hexval(j->p[0]), h1 = si_hexval(j->p[1]),
                        h2 = si_hexval(j->p[2]), h3 = si_hexval(j->p[3]);
                    if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) { free(buf); return false; }
                    unsigned int cp = (unsigned int)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
                    j->p += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF &&
                        j->p[0] == '\\' && j->p[1] == 'u') {
                        int g0 = si_hexval(j->p[2]), g1 = si_hexval(j->p[3]),
                            g2 = si_hexval(j->p[4]), g3 = si_hexval(j->p[5]);
                        if (g0 >= 0 && g1 >= 0 && g2 >= 0 && g3 >= 0) {
                            unsigned int lo = (unsigned int)((g0 << 12) | (g1 << 8) | (g2 << 4) | g3);
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                j->p += 6;
                            }
                        }
                    }
                    if (cp >= 0xD800 && cp <= 0xDFFF) { free(buf); return false; }
                    char enc[4];
                    size_t w = si_utf8_encode(cp, enc);
                    for (size_t k = 0; k < w; k++) SI_JP_EMIT(enc[k]);
                    break;
                }
                default: free(buf); return false;
            }
            continue;
        }
        if (c < 0x20) { free(buf); return false; }
        SI_JP_EMIT(c);
    }
#undef SI_JP_EMIT
}
static bool si_jp_number(si_jp_t *j, json_t **out) {
    const char *start = j->p;
    bool is_float = false;
    if (*j->p == '-') j->p++;
    if (*j->p == '0') {
        j->p++;
        if (*j->p >= '0' && *j->p <= '9') return false;  /* leading zero */
    } else if (*j->p >= '1' && *j->p <= '9') {
        while (*j->p >= '0' && *j->p <= '9') j->p++;
    } else {
        return false;
    }
    if (*j->p == '.' && j->p[1] >= '0' && j->p[1] <= '9') {
        is_float = true;
        j->p++;
        while (*j->p >= '0' && *j->p <= '9') j->p++;
    }
    if (*j->p == 'e' || *j->p == 'E') {
        const char *save = j->p;
        j->p++;
        if (*j->p == '+' || *j->p == '-') j->p++;
        if (!(*j->p >= '0' && *j->p <= '9')) { j->p = save; }
        else {
            is_float = true;
            while (*j->p >= '0' && *j->p <= '9') j->p++;
        }
    }
    size_t n = (size_t)(j->p - start);
    char *tmp = malloc(n + 1);
    if (!tmp) return false;
    memcpy(tmp, start, n);
    tmp[n] = '\0';
    double v = strtod(tmp, NULL);
    free(tmp);
    if (!isfinite(v)) return false;  /* overflow literals out of domain */
    if (!is_float) {
        if (v > 9007199254740992.0 || v < -9007199254740992.0) return false;
        *out = json_number(v);
    } else {
        json_t *o = json_object();
        json_set(o, "__float__", json_bool(true));
        json_set(o, "v", json_number(v));
        *out = o;
    }
    return true;
}
static bool si_jp_value(si_jp_t *j, json_t **out) {
    if (j->depth > 32) return false;
    si_jp_ws(j);
    char c = *j->p;
    if (c == '{') {
        j->p++;
        j->depth++;
        json_t *o = json_object();
        si_jp_ws(j);
        if (*j->p == '}') { j->p++; j->depth--; *out = o; return true; }
        for (;;) {
            char *key = NULL;
            if (!si_jp_string(j, &key)) { json_free(o); return false; }
            si_jp_ws(j);
            if (*j->p != ':') { free(key); json_free(o); return false; }
            j->p++;
            json_t *val = NULL;
            if (!si_jp_value(j, &val)) { free(key); json_free(o); return false; }
            json_set(o, key, val);
            free(key);
            si_jp_ws(j);
            if (*j->p == ',') { j->p++; si_jp_ws(j); continue; }
            if (*j->p == '}') { j->p++; j->depth--; *out = o; return true; }
            json_free(o);
            return false;
        }
    }
    if (c == '[') {
        j->p++;
        j->depth++;
        json_t *a = json_array();
        si_jp_ws(j);
        if (*j->p == ']') { j->p++; j->depth--; *out = a; return true; }
        for (;;) {
            json_t *val = NULL;
            if (!si_jp_value(j, &val)) { json_free(a); return false; }
            json_append(a, val);
            si_jp_ws(j);
            if (*j->p == ',') { j->p++; continue; }
            if (*j->p == ']') { j->p++; j->depth--; *out = a; return true; }
            json_free(a);
            return false;
        }
    }
    if (c == '"') {
        char *s = NULL;
        if (!si_jp_string(j, &s)) return false;
        *out = json_string(s);
        free(s);
        return true;
    }
    if (c == 't') {
        if (strncmp(j->p, "true", 4) != 0) return false;
        j->p += 4; *out = json_bool(true); return true;
    }
    if (c == 'f') {
        if (strncmp(j->p, "false", 5) != 0) return false;
        j->p += 5; *out = json_bool(false); return true;
    }
    if (c == 'n') {
        if (strncmp(j->p, "null", 4) != 0) return false;
        j->p += 4; *out = json_null(); return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) return si_jp_number(j, out);
    return false;
}
static bool si_json_loads(const char *s, json_t **out) {
    si_jp_t j = { s, 0 };
    *out = NULL;
    json_t *v = NULL;
    if (!si_jp_value(&j, &v)) return false;
    si_jp_ws(&j);
    if (*j.p != '\0') { json_free(v); return false; }
    *out = v;
    return true;
}

/* -- mapping / subscript access with real py3.14 messages -- */
/* obj.get(key) on a non-dict raises; on a dict returns BORROWED value or NULL
 * (absent == None). */
static const json_t *si_map_get(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!si_is_dict(obj)) {
        *err = si_err_fmt("AttributeError", "'%s' object has no attribute 'get'",
                          si_type_name(obj));
        return NULL;
    }
    return json_obj_get(obj, key);
}
/* obj[key] on a dict: BORROWED value, KeyError 'key' when absent. Non-dict
 * receivers raise the py3.14 subscript TypeErrors (probed on 3.14.9). */
static const json_t *si_subscript(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (si_is_dict(obj)) {
        const json_t *v = json_obj_get(obj, key);
        if (!v) *err = si_err_fmt("KeyError", "'%s'", key);
        return v;
    }
    if (si_is_str(obj)) {
        *err = si_err("TypeError", "string indices must be integers, not 'str'");
        return NULL;
    }
    if (si_is_list(obj)) {
        *err = si_err("TypeError", "list indices must be integers or slices, not str");
        return NULL;
    }
    if (si_is_tuple_tag(obj)) {
        *err = si_err("TypeError", "tuple indices must be integers or slices, not str");
        return NULL;
    }
    *err = si_err_fmt("TypeError", "'%s' object is not subscriptable", si_type_name(obj));
    return NULL;
}
/* obj[key] = val (val BORROWED; copied on success). Non-dict receivers raise
 * the py3.14 item-assignment TypeErrors (probed on 3.14.9). */
static void si_item_assign(json_t *obj, const char *key, const json_t *val, json_t **err) {
    *err = NULL;
    if (si_is_dict(obj)) {
        json_set(obj, key, val ? json_copy(val) : json_null());
        return;
    }
    if (si_is_list(obj)) {
        *err = si_err("TypeError", "list indices must be integers or slices, not str");
        return;
    }
    *err = si_err_fmt("TypeError", "'%s' object does not support item assignment",
                      si_type_name(obj));
}

/* ============== persistence/sqlite/_state/common.py leaf ============== */

/* try: return json.loads(raw)
 * except (json.JSONDecodeError, TypeError):
 *     logger.warning(warning); return fallback
 * json.loads is transcribed (si_json_loads); a non-str raw is the TypeError
 * branch (bytes/bytearray out of the fixture domain). logger.warning is the
 * seam; a raise from it propagates (it sits in the except handler). */
static json_t *si_json_or_impl(si_ctx_t *ctx, const json_t *raw, const json_t *fallback,
                               const char *warning) {
    if (si_is_str(raw)) {
        json_t *parsed = NULL;
        if (si_json_loads(raw->str_val, &parsed)) return parsed;
    }
    json_t *a = json_object();
    json_set_hdr(a, "message", warning);
    json_t *w = si_call(ctx, "logger.warning", a);
    if (si_is_raise(w)) return w;
    json_free(w);
    return fallback ? json_copy(fallback) : json_null();
}
/* PoP: si_json_or @ persistence/sqlite/_state/common.py:_json_or */
static json_t *si_json_or(si_ctx_t *ctx, json_t *args) {
    return si_json_or_impl(ctx, json_obj_get(args, "raw"), json_obj_get(args, "fallback"),
                           json_get_str(args, "warning", ""));
}

/* ============== session_rules/turns.py leaf ============== */

/* _tc_field(tc, key): tc.get(key) if isinstance(tc, dict) else
 * getattr(tc, key, None) — JSON scalars/lists/tuples carry no such attrs. */
static const json_t *si_tc_field(const json_t *tc, const char *key) {
    if (si_is_dict(tc)) return json_obj_get(tc, key);
    return NULL;
}
/* for raw in (_tc_field(tc, "call_id"), _tc_field(tc, "id")):
 *     value = raw.strip() if isinstance(raw, str) else ""
 *     if value: return value.split("|", 1)[0].strip() or value
 * return "" */
static json_t *si_coalesce_tool_call_id_impl(const json_t *tc) {
    static const char *keys[] = { "call_id", "id" };
    for (size_t k = 0; k < 2; k++) {
        const json_t *raw = si_tc_field(tc, keys[k]);
        char *value = si_strip(si_is_str(raw) ? raw->str_val : "");
        if (!value) return json_string("");
        if (value[0]) {
            const char *pipe = strchr(value, '|');
            char *head = NULL;
            if (pipe) {
                size_t hn = (size_t)(pipe - value);
                head = malloc(hn + 1);
                if (head) { memcpy(head, value, hn); head[hn] = '\0'; }
            } else {
                head = strdup(value);
            }
            char *hs = si_strip(head ? head : "");
            free(head);
            const char *result = (hs && hs[0]) ? hs : value;
            json_t *ret = json_string(result);
            free(hs);
            free(value);
            return ret;
        }
        free(value);
    }
    return json_string("");
}
/* PoP: si_coalesce_tool_call_id @ session_rules/turns.py:coalesce_tool_call_id */
static json_t *si_coalesce_tool_call_id(si_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return si_coalesce_tool_call_id_impl(json_obj_get(args, "tc"));
}

/* ============== persistence/sqlite/conversation/_store/identity.py ============== */

/* return msg.get(live_key) if live_key in msg else msg.get(column)
 * For a str/list/tuple msg the `in` test never raises and BOTH branches then
 * raise the same .get AttributeError; for a scalar msg `in` raises the
 * py3.14 not-a-container TypeError first. */
static json_t *si_live_or_column_impl(const json_t *msg, const char *live_key,
                                      const char *column) {
    if (si_is_dict(msg)) {
        const json_t *v = json_obj_get(msg, live_key);
        if (v) return json_copy(v);
        v = json_obj_get(msg, column);
        return v ? json_copy(v) : json_null();
    }
    if (si_is_str(msg) || si_is_list(msg) || si_is_tuple_tag(msg))
        return si_err_fmt("AttributeError", "'%s' object has no attribute 'get'",
                          si_type_name(msg));
    return si_err_fmt("TypeError", "argument of type '%s' is not a container or iterable",
                      si_type_name(msg));
}
/* PoP: si_live_or_column @ persistence/sqlite/conversation/_store/identity.py:_live_or_column */
static json_t *si_live_or_column(si_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return si_live_or_column_impl(json_obj_get(args, "msg"),
                                  json_get_str(args, "live_key", ""),
                                  json_get_str(args, "column", ""));
}

/* if isinstance(value, str):
 *     value = _json_or(value, [], "Failed to deserialize a message uid list, falling back to []")
 * return uid_list(value)   <- CLAIMED sibling, seamed */
static json_t *si_uid_list_impl(si_ctx_t *ctx, const json_t *value) {
    json_t *owned = NULL;
    if (si_is_str(value)) {
        json_t *fb = json_array();
        owned = si_json_or_impl(ctx, value, fb,
                           "Failed to deserialize a message uid list, falling back to []");
        json_free(fb);
        if (si_is_raise(owned)) return owned;
        value = owned;
    }
    json_t *a = json_object();
    json_set(a, "value", value ? json_copy(value) : json_null());
    json_t *ret = si_call(ctx, "uid_list", a);
    if (owned) json_free(owned);
    return ret;
}
/* PoP: si_uid_list @ persistence/sqlite/conversation/_store/identity.py:_uid_list */
static json_t *si_uid_list(si_ctx_t *ctx, json_t *args) {
    return si_uid_list_impl(ctx, json_obj_get(args, "value"));
}

/* if isinstance(value, str):
 *     value = _json_or(value, {}, "Failed to deserialize a tool-call uid map, falling back to {}")
 * if not isinstance(value, dict): return {}
 * return {k: v for k, v in value.items() if isinstance(k, str) and k and (
 *     (isinstance(v, str) and v) or (isinstance(v, list) and v and
 *      all(isinstance(u, str) and u for u in v)))} */
static json_t *si_uid_map_impl(si_ctx_t *ctx, const json_t *value) {
    json_t *owned = NULL;
    if (si_is_str(value)) {
        json_t *fb = json_object();
        owned = si_json_or_impl(ctx, value, fb,
                           "Failed to deserialize a tool-call uid map, falling back to {}");
        json_free(fb);
        if (si_is_raise(owned)) return owned;
        value = owned;
    }
    json_t *out = json_object();
    if (si_is_dict(value)) {
        for (size_t i = 0; i < value->c.count; i++) {
            const char *k = value->c.keys[i];
            const json_t *v = value->c.items[i];
            if (!k[0]) continue;  /* JSON keys are str; drop the empty key */
            bool keep = false;
            if (si_is_str(v) && v->str_val[0]) {
                keep = true;
            } else if (si_is_list(v) && v->c.count > 0) {
                keep = true;
                for (size_t j = 0; j < v->c.count; j++) {
                    const json_t *u = json_get(v, j);
                    if (!si_is_str(u) || !u->str_val[0]) { keep = false; break; }
                }
            }
            if (keep) json_set(out, k, json_copy(v));
        }
    }
    if (owned) json_free(owned);
    return out;
}
/* PoP: si_uid_map @ persistence/sqlite/conversation/_store/identity.py:_uid_map */
static json_t *si_uid_map(si_ctx_t *ctx, json_t *args) {
    return si_uid_map_impl(ctx, json_obj_get(args, "value"));
}

/* return _uid_map(_live_or_column(msg, TOOL_CALL_UIDS, "tool_call_uids")) */
static json_t *si_tool_call_uid_map_impl(si_ctx_t *ctx, const json_t *msg) {
    json_t *v = si_live_or_column_impl(msg, SI_TOOL_CALL_UIDS, "tool_call_uids");
    if (si_is_raise(v)) return v;
    json_t *r = si_uid_map_impl(ctx, v);
    json_free(v);
    return r;
}
/* PoP: si_tool_call_uid_map @ persistence/sqlite/conversation/_store/identity.py:_tool_call_uid_map */
static json_t *si_tool_call_uid_map(si_ctx_t *ctx, json_t *args) {
    return si_tool_call_uid_map_impl(ctx, json_obj_get(args, "msg"));
}

/* uids = _uid_list(_live_or_column(msg, ABSORBED_MESSAGE_UIDS, "absorbed_message_uids"))
 * return json.dumps(uids) if uids else None */
static json_t *si_absorbed_uids_json_impl(si_ctx_t *ctx, const json_t *msg) {
    json_t *v = si_live_or_column_impl(msg, SI_ABSORBED_MESSAGE_UIDS, "absorbed_message_uids");
    if (si_is_raise(v)) return v;
    json_t *uids = si_uid_list_impl(ctx, v);
    json_free(v);
    if (si_is_raise(uids)) return uids;
    json_t *ret = json_null();
    if (si_truthy(uids)) {
        char *s = si_dumps_uid_list(uids);
        ret = json_string(s ? s : "[]");
        free(s);
    }
    json_free(uids);
    return ret;
}
/* PoP: si_absorbed_uids_json @ persistence/sqlite/conversation/_store/identity.py:_absorbed_uids_json */
static json_t *si_absorbed_uids_json(si_ctx_t *ctx, json_t *args) {
    return si_absorbed_uids_json_impl(ctx, json_obj_get(args, "msg"));
}

/* uids = _tool_call_uid_map(msg)
 * return json.dumps(uids, sort_keys=True) if uids else None */
static json_t *si_tool_call_uids_json_impl(si_ctx_t *ctx, const json_t *msg) {
    json_t *uids = si_tool_call_uid_map_impl(ctx, msg);
    if (si_is_raise(uids)) return uids;
    json_t *ret = json_null();
    if (si_truthy(uids)) {
        char *s = si_dumps_uid_map_sorted(uids);
        ret = json_string(s ? s : "{}");
        free(s);
    }
    json_free(uids);
    return ret;
}
/* PoP: si_tool_call_uids_json @ persistence/sqlite/conversation/_store/identity.py:_tool_call_uids_json */
static json_t *si_tool_call_uids_json(si_ctx_t *ctx, json_t *args) {
    return si_tool_call_uids_json_impl(ctx, json_obj_get(args, "msg"));
}

/* uid = _live_or_column(msg, TOOL_CALL_UID, "tool_call_uid")
 * return uid if isinstance(uid, str) and uid else None */
static json_t *si_tool_call_uid_or_none_impl(const json_t *msg) {
    json_t *uid = si_live_or_column_impl(msg, SI_TOOL_CALL_UID, "tool_call_uid");
    if (si_is_raise(uid)) return uid;
    if (si_is_str(uid) && uid->str_val[0]) return uid;
    json_free(uid);
    return json_null();
}
/* PoP: si_tool_call_uid_or_none @ persistence/sqlite/conversation/_store/identity.py:_tool_call_uid_or_none */
static json_t *si_tool_call_uid_or_none(si_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return si_tool_call_uid_or_none_impl(json_obj_get(args, "msg"));
}

/* stored = stored_uid if isinstance(stored_uid, list) else [stored_uid]
 * return isinstance(live_uid, list) and live_uid[:len(stored)] == stored */
static bool si_live_extends_impl(const json_t *live_uid, const json_t *stored_uid) {
    if (!si_is_list(live_uid)) return false;
    const json_t *stored = stored_uid;
    json_t *wrap = NULL;
    if (!si_is_list(stored_uid)) {
        wrap = json_array();
        json_append(wrap, stored_uid ? json_copy(stored_uid) : json_null());
        stored = wrap;
    }
    size_t n = stored->c.count;
    if (n > live_uid->c.count) n = live_uid->c.count;
    json_t *slice = json_array();
    for (size_t i = 0; i < n; i++)
        json_append(slice, json_copy(json_get(live_uid, i)));
    bool eq = si_eq(slice, stored);
    json_free(slice);
    if (wrap) json_free(wrap);
    return eq;
}
/* PoP: si_live_extends @ persistence/sqlite/conversation/_store/identity.py:_live_extends */
static json_t *si_live_extends(si_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return json_bool(si_live_extends_impl(json_obj_get(args, "live_uid"),
                                          json_obj_get(args, "stored_uid")));
}

/* if not row["tool_call_uids"] or not isinstance(calls := msg.get("tool_calls"), list):
 *     return
 * live = msg.get(TOOL_CALL_UIDS); live = live if isinstance(live, dict) else {}
 * named = {coalesce_tool_call_id(tc) for tc in calls}
 * if missing := {k: v for k, v in _uid_map(row["tool_call_uids"]).items()
 *                if k in named and k not in live}:
 *     msg[TOOL_CALL_UIDS] = {**live, **missing} */
static json_t *si_fill_missing_tool_call_uids_impl(si_ctx_t *ctx, const json_t *row,
                                                   json_t *msg) {
    json_t *err = NULL;
    const json_t *rc = si_subscript(row, "tool_call_uids", &err);
    if (err) return err;
    if (!si_truthy(rc)) return json_null();
    const json_t *calls = si_map_get(msg, "tool_calls", &err);
    if (err) return err;
    if (!si_is_list(calls)) return json_null();
    const json_t *live = si_map_get(msg, SI_TOOL_CALL_UIDS, &err);
    if (err) return err;
    const json_t *live_dict = si_is_dict(live) ? live : NULL;
    /* named set (membership only; coalesce is pure, no seam) */
    json_t *named = json_array();
    for (size_t i = 0; i < calls->c.count; i++) {
        json_t *id = si_coalesce_tool_call_id_impl(json_get(calls, i));
        json_append(named, id);
    }
    json_t *um = si_uid_map_impl(ctx, rc);
    if (si_is_raise(um)) { json_free(named); return um; }
    json_t *missing = json_object();
    for (size_t i = 0; i < um->c.count; i++) {
        const char *k = um->c.keys[i];
        bool in_named = false;
        for (size_t j = 0; j < named->c.count; j++)
            if (si_eq_str(json_get(named, j), k)) { in_named = true; break; }
        if (!in_named) continue;
        if (live_dict && json_obj_get(live_dict, k)) continue;
        json_set(missing, k, json_copy(um->c.items[i]));
    }
    json_free(um);
    json_free(named);
    if (missing->c.count > 0) {
        json_t *merged = json_object();
        if (live_dict)
            for (size_t i = 0; i < live_dict->c.count; i++)
                json_set(merged, live_dict->c.keys[i], json_copy(live_dict->c.items[i]));
        for (size_t i = 0; i < missing->c.count; i++)
            json_set(merged, missing->c.keys[i], json_copy(missing->c.items[i]));
        json_free(missing);
        si_item_assign(msg, SI_TOOL_CALL_UIDS, merged, &err);
        json_free(merged);
        if (err) return err;
    } else {
        json_free(missing);
    }
    return json_null();
}
/* PoP: si_fill_missing_tool_call_uids @ persistence/sqlite/conversation/_store/identity.py:_fill_missing_tool_call_uids */
static json_t *si_fill_missing_tool_call_uids(si_ctx_t *ctx, json_t *args) {
    return si_fill_missing_tool_call_uids_impl(ctx, json_obj_get(args, "row"),
                                               json_obj_get(args, "msg"));
}

/* if row[MESSAGE_UID]: msg[MESSAGE_UID] = row[MESSAGE_UID]
 * if row["tool_call_uids"] and (stored := _uid_map(row["tool_call_uids"])):
 *     live = msg.get(TOOL_CALL_UIDS); live = live if isinstance(live, dict) else {}
 *     msg[TOOL_CALL_UIDS] = {**live, **{call_id: uid for call_id, uid in stored.items()
 *                                       if not _live_extends(live.get(call_id), uid)}}
 * if row["tool_call_uid"]: msg[TOOL_CALL_UID] = row["tool_call_uid"] */
static json_t *si_restore_row_identity_impl(si_ctx_t *ctx, const json_t *row, json_t *msg) {
    json_t *err = NULL;
    const json_t *mu = si_subscript(row, SI_MESSAGE_UID, &err);
    if (err) return err;
    if (si_truthy(mu)) {
        si_item_assign(msg, SI_MESSAGE_UID, mu, &err);
        if (err) return err;
    }
    const json_t *tcu = si_subscript(row, "tool_call_uids", &err);
    if (err) return err;
    if (si_truthy(tcu)) {
        json_t *stored = si_uid_map_impl(ctx, tcu);
        if (si_is_raise(stored)) return stored;
        if (si_truthy(stored)) {
            const json_t *live = si_map_get(msg, SI_TOOL_CALL_UIDS, &err);
            if (err) { json_free(stored); return err; }
            const json_t *live_dict = si_is_dict(live) ? live : NULL;
            json_t *filtered = json_object();
            for (size_t i = 0; i < stored->c.count; i++) {
                const char *k = stored->c.keys[i];
                const json_t *uid = stored->c.items[i];
                const json_t *lv = live_dict ? json_obj_get(live_dict, k) : NULL;
                if (!si_live_extends_impl(lv, uid))
                    json_set(filtered, k, json_copy(uid));
            }
            json_t *merged = json_object();
            if (live_dict)
                for (size_t i = 0; i < live_dict->c.count; i++)
                    json_set(merged, live_dict->c.keys[i], json_copy(live_dict->c.items[i]));
            for (size_t i = 0; i < filtered->c.count; i++)
                json_set(merged, filtered->c.keys[i], json_copy(filtered->c.items[i]));
            json_free(filtered);
            si_item_assign(msg, SI_TOOL_CALL_UIDS, merged, &err);
            json_free(merged);
            if (err) { json_free(stored); return err; }
        }
        json_free(stored);
    }
    const json_t *tcuid = si_subscript(row, "tool_call_uid", &err);
    if (err) return err;
    if (si_truthy(tcuid)) {
        si_item_assign(msg, SI_TOOL_CALL_UID, tcuid, &err);
        if (err) return err;
    }
    return json_null();
}
/* PoP: si_restore_row_identity @ persistence/sqlite/conversation/_store/identity.py:_restore_row_identity */
static json_t *si_restore_row_identity(si_ctx_t *ctx, json_t *args) {
    return si_restore_row_identity_impl(ctx, json_obj_get(args, "row"),
                                        json_obj_get(args, "msg"));
}

/* _restore_row_identity(row, msg)
 * if row["absorbed_message_uids"] and (absorbed := _uid_list(row["absorbed_message_uids"])):
 *     msg[ABSORBED_MESSAGE_UIDS] = absorbed */
static json_t *si_restore_identity_columns_impl(si_ctx_t *ctx, const json_t *row,
                                                json_t *msg) {
    json_t *r = si_restore_row_identity_impl(ctx, row, msg);
    if (si_is_raise(r)) return r;
    json_free(r);
    json_t *err = NULL;
    const json_t *am = si_subscript(row, "absorbed_message_uids", &err);
    if (err) return err;
    if (si_truthy(am)) {
        json_t *absorbed = si_uid_list_impl(ctx, am);
        if (si_is_raise(absorbed)) return absorbed;
        if (si_truthy(absorbed)) {
            si_item_assign(msg, SI_ABSORBED_MESSAGE_UIDS, absorbed, &err);
            if (err) { json_free(absorbed); return err; }
        }
        json_free(absorbed);
    }
    return json_null();
}
/* PoP: si_restore_identity_columns @ persistence/sqlite/conversation/_store/identity.py:_restore_identity_columns */
static json_t *si_restore_identity_columns(si_ctx_t *ctx, json_t *args) {
    return si_restore_identity_columns_impl(ctx, json_obj_get(args, "row"),
                                            json_obj_get(args, "msg"));
}

/* if row["role"] != "assistant": return None
 * calls = _json_or(row["tool_calls"] or "[]", [], "Failed to deserialize tool_calls, falling back to []")
 * call_ids = tuple(coalesce_tool_call_id(tc) for tc in calls or ())
 * if call_ids and all(call_ids):
 *     return ("assistant", None, row["timestamp"], row["tool_call_id"], call_ids, row["tool_name"])
 * return None */
static json_t *si_stable_tool_key_impl(si_ctx_t *ctx, const json_t *row) {
    json_t *err = NULL;
    const json_t *role = si_subscript(row, "role", &err);
    if (err) return err;
    if (!si_eq_str(role, "assistant")) return json_null();
    const json_t *tc = si_subscript(row, "tool_calls", &err);
    if (err) return err;
    json_t *fallback_raw = json_string("[]");
    const json_t *raw = si_truthy(tc) ? tc : fallback_raw;
    json_t *fb = json_array();
    json_t *calls = si_json_or_impl(ctx, raw, fb,
                               "Failed to deserialize tool_calls, falling back to []");
    json_free(fb);
    json_free(fallback_raw);
    if (si_is_raise(calls)) return calls;
    json_t *call_ids = json_array();
    if (si_truthy(calls)) {
        if (si_is_list(calls)) {
            for (size_t i = 0; i < calls->c.count; i++)
                json_append(call_ids, si_coalesce_tool_call_id_impl(json_get(calls, i)));
        } else if (si_is_str(calls)) {
            const char *s = calls->str_val;
            for (size_t i = 0; s[i]; ) {
                unsigned int cp;
                size_t w = si_utf8_decode(s + i, &cp);
                char ch[5];
                size_t bw = si_utf8_encode(cp, ch);
                ch[bw] = '\0';
                json_t *chn = json_string(ch);
                json_append(call_ids, si_coalesce_tool_call_id_impl(chn));
                json_free(chn);
                i += w;
            }
        } else if (si_is_dict(calls)) {
            for (size_t i = 0; i < calls->c.count; i++) {
                json_t *kn = json_string(calls->c.keys[i]);
                json_append(call_ids, si_coalesce_tool_call_id_impl(kn));
                json_free(kn);
            }
        } else {
            json_free(call_ids);
            json_free(calls);
            return si_err_fmt("TypeError", "'%s' object is not iterable",
                              si_type_name(calls));
        }
    }
    json_free(calls);
    bool all_nonempty = call_ids->c.count > 0;
    for (size_t i = 0; i < call_ids->c.count; i++) {
        const json_t *id = json_get(call_ids, i);
        if (!si_is_str(id) || !id->str_val[0]) { all_nonempty = false; break; }
    }
    if (!all_nonempty) {
        json_free(call_ids);
        return json_null();
    }
    const json_t *ts = si_subscript(row, "timestamp", &err);
    if (err) { json_free(call_ids); return err; }
    const json_t *tcid = si_subscript(row, "tool_call_id", &err);
    if (err) { json_free(call_ids); return err; }
    const json_t *tn = si_subscript(row, "tool_name", &err);
    if (err) { json_free(call_ids); return err; }
    json_t *inner = json_object();
    json_set(inner, "__tuple__", json_bool(true));
    json_set(inner, "v", call_ids);
    json_t *items = json_array();
    json_append(items, json_string("assistant"));
    json_append(items, json_null());
    json_append(items, ts ? json_copy(ts) : json_null());
    json_append(items, tcid ? json_copy(tcid) : json_null());
    json_append(items, inner);
    json_append(items, tn ? json_copy(tn) : json_null());
    json_t *outer = json_object();
    json_set(outer, "__tuple__", json_bool(true));
    json_set(outer, "v", items);
    return outer;
}
/* PoP: si_stable_tool_key @ persistence/sqlite/conversation/_store/identity.py:_stable_tool_key */
static json_t *si_stable_tool_key(si_ctx_t *ctx, json_t *args) {
    return si_stable_tool_key_impl(ctx, json_obj_get(args, "row"));
}
