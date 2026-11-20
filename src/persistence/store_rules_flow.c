/*
 * store_rules_flow.c - FLOW cores + leaves of the conversation store's pure
 * compaction-rule helpers. Defining module:
 *   persistence/sqlite/conversation/_store/rules.py
 * ("Pure helpers and constants of the application modules the store functions
 * run"). All 11 rules.py defs are ported and credited there (each verified
 * UNCLAIMED by PoP grep, by name and by module). The uid/typed-ask leaf cluster
 * rules.py imports from session_rules/turns.py is ported too (6 more unclaimed
 * defs: mint_uid, uid_list, message_uid_or_none, _is_empty_content,
 * drop_typed_ask, record_absorbed_message), credited to session_rules/turns.py.
 * 17 ported functions total.
 *
 * Decision sequences only: every external collaborator enters through ONE
 * universal seam: sr_call(ctx, name, args). The harness wires it to a recording
 * stub fed the case's scripted results (in.io); the oracle installs recording
 * stubs emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. Seamed collaborators:
 *   uuid4                       (uuid.uuid4, mint_uid)
 *   datetime.fromisoformat      (_parse_iso)
 *   datetime.strptime           (_parse_iso, _parse_timestamp_match)
 *   dt.replace / dt.astimezone / dt.timestamp / dt.timetuple
 *                               (datetime instance methods, _localize/coerce_epoch)
 *   time.mktime / time.localtime (_localize Windows fold recovery)
 *   logger.warning              (hermes_cli.timefmt logger, coerce_epoch)
 * Ported leaves call each other DIRECTLY (no seam): stamp_message_uid ->
 * message_uid_or_none + mint_uid; _newest_checkpoint_carrier ->
 * _is_checkpoint_item; _parse_iso/_parse_timestamp_match -> _localize;
 * strip_leading_message_timestamps -> _parse_timestamp_match;
 * record_absorbed_message -> _is_empty_content + drop_typed_ask +
 * message_uid_or_none + uid_list.
 * Constants seeded, not seamed: MESSAGE_UID, DATA_USE, TYPED_ASK,
 * ABSORBED_MESSAGE_UIDS, EPOCH_MIN, EPOCH_MAX, _IMAGE_PART_TYPES,
 * sys.platform ("win32" - the fold-recovery branch is Windows-specific),
 * errno.EINVAL (22). The two regexes (_SURROGATE_RE, _TIMESTAMP_PREFIX_RE)
 * are ported as hand-written ASCII matchers, not seamed.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sr_-prefixed.
 *
 * Representation contract (shared with tests/oracle_store_rules_flow.py):
 * - NULL json_t* == an absent key / Python None; a JSON_NULL node is a real
 *   null VALUE. An exception is a {"__raise__": cls, "message": str(exc)}
 *   object (OSError adds "errno"). BaseException (SystemExit,
 *   KeyboardInterrupt, GeneratorExit) passes through every except clause.
 * - Plain JSON numbers are Python ints; JSON bools are bools. A Python float
 *   travels as {"__float__": true, "v": <number>}; a tuple as
 *   {"__tuple__": true, "v": [...]}; a datetime double as
 *   {"__dt__": true, "tzinfo": <token|null>, "microsecond": <int>}.
 * - tz is an opaque token (null or a string) passed through to dt.replace.
 * - A re.Match for the standalone _parse_timestamp_match drive is built from
 *   args.match_text by the ported matcher (oracle: the live regex); a failed
 *   match models match=None and raises AttributeError on .group.
 * - Mutating functions (lift_data_use, stamp_message_uid, drop_typed_ask,
 *   record_absorbed_message) mutate their dict args in place; the harness
 *   echoes the mutated params under "state" keyed by parameter name.
 * - Sets return as SORTED arrays; tuples fold to arrays and integral floats
 *   to ints in canon. Dict key order is irrelevant to canon.
 * - Fixture domain: text ASCII plus raw UTF-8 lone-surrogate triplets
 *   (ED A0 80 .. ED BF BF, transported verbatim through libjson and decoded
 *   surrogatepass by the oracle); timestamps int/float/None; uids non-empty
 *   ASCII strings; data_use values list/tuple/str/dict/int/bool/float/null;
 *   uuid4 returns {"hex": str}; dt.timestamp returns int/float; wall_time and
 *   localtime results are 9-tuples; no NaN/Infinity JSON literals.
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
typedef struct sr_ctx {
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sr_ctx_t;

/* -- constants (seeded, not seamed) -- */
#define SR_MESSAGE_UID            "message_uid"
#define SR_DATA_USE               "data_use"
#define SR_TYPED_ASK              "typed_ask"
#define SR_ABSORBED_MESSAGE_UIDS  "_absorbed_message_uids"
#define SR_EPOCH_MIN              0.0
#define SR_EPOCH_MAX              4200000000.0
#define SR_SYS_PLATFORM           "win32"
#define SR_ERRNO_EINVAL           22
static const char *SR_IMAGE_PART_TYPES[] = { "image_url", "input_image", "image" };
#define SR_IMAGE_PART_TYPES_N 3

/* -- basic type helpers -- */
static bool sr_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sr_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}
static bool sr_is_tuple_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__tuple__", false);
}
static bool sr_is_dt_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__dt__", false);
}
static bool sr_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sr_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sr_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool sr_is_int(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool sr_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sr_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sr_is_float_tag(v) &&
           !sr_is_tuple_tag(v) && !sr_is_dt_tag(v);
}
static bool sr_is_numlike(const json_t *v) {
    return sr_is_int(v) || sr_is_bool(v) || sr_is_float_tag(v);
}
static double sr_num(const json_t *v) {
    if (sr_is_float_tag(v)) return json_get_num(v, "v", 0);
    if (sr_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static const char *sr_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool sr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (sr_is_float_tag(v)) return json_get_num(v, "v", 0) != 0.0;
    if (sr_is_tuple_tag(v)) {
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
static const json_t *sr_seq_items(const json_t *v) {
    if (sr_is_tuple_tag(v)) return json_obj_get(v, "v");
    return v;
}
static bool sr_is_seq(const json_t *v) { return sr_is_list(v) || sr_is_tuple_tag(v); }
static size_t sr_seq_len(const json_t *v) {
    const json_t *it = sr_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? it->c.count : 0;
}
static const json_t *sr_seq_get(const json_t *v, size_t i) {
    const json_t *it = sr_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? json_get(it, i) : NULL;
}
/* Python type(x).__name__ on the representation (domain-restricted). */
static const char *sr_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (sr_is_float_tag(v)) return "float";
    if (sr_is_tuple_tag(v)) return "tuple";
    if (sr_is_dt_tag(v)) return "datetime.datetime";
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
static bool sr_eq(const json_t *a, const json_t *b) {
    bool an = sr_is_numlike(a), bn = sr_is_numlike(b);
    if (an || bn) return an && bn && sr_num(a) == sr_num(b);
    if (sr_is_null(a) || sr_is_null(b)) return sr_is_null(a) && sr_is_null(b);
    bool as = sr_is_str(a), bs = sr_is_str(b);
    if (as || bs) return as && bs && strcmp(a->str_val, b->str_val) == 0;
    bool aq = sr_is_seq(a), bq = sr_is_seq(b);
    if (aq || bq) {
        if (!aq || !bq) return false;
        if (sr_is_tuple_tag(a) != sr_is_tuple_tag(b)) return false;
        size_t n = sr_seq_len(a);
        if (n != sr_seq_len(b)) return false;
        for (size_t i = 0; i < n; i++)
            if (!sr_eq(sr_seq_get(a, i), sr_seq_get(b, i))) return false;
        return true;
    }
    if (sr_is_dict(a) && sr_is_dict(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !sr_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}
/* x == "<literal>" for a str x (non-str x is never equal to a str). */
static bool sr_eq_str(const json_t *v, const char *lit) {
    return sr_is_str(v) && v->str_val && strcmp(v->str_val, lit) == 0;
}

/* -- exceptions -- */
static json_t *sr_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *sr_err_fmt(const char *type, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return sr_err(type, buf);
}

/* FULL builtin exception hierarchy (py3.14). Unknown scripted names parent to
 * Exception, mirroring the oracle's type(kind, (Exception,), {}) fallback;
 * SystemExit/KeyboardInterrupt/GeneratorExit parent to BaseException ONLY so
 * they pass through every except-Exception clause. */
typedef struct { const char *name; const char *parent; } sr_exc_ent_t;
static const sr_exc_ent_t SR_EXC_TABLE[] = {
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
#define SR_EXC_TABLE_N (sizeof(SR_EXC_TABLE) / sizeof(SR_EXC_TABLE[0]))

static const char *sr_exc_parent(const char *cls) {
    for (size_t i = 0; i < SR_EXC_TABLE_N; i++)
        if (strcmp(SR_EXC_TABLE[i].name, cls) == 0) return SR_EXC_TABLE[i].parent;
    return NULL;
}
/* isinstance(exc, target) via the parent chain. */
static bool sr_exc_is_a(const char *cls, const char *target) {
    if (!cls || !target) return false;
    char cur[128];
    snprintf(cur, sizeof cur, "%s", cls);
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cur, target) == 0) return true;
        const char *parent = sr_exc_parent(cur);
        if (!parent) {
            if (strcmp(cur, "BaseException") == 0) return false;
            /* unknown scripted class: the oracle parents it to Exception */
            parent = (strcmp(cur, "Exception") == 0) ? "BaseException" : "Exception";
        }
        snprintf(cur, sizeof cur, "%s", parent);
    }
    return false;
}
static const char *sr_raise_cls(const json_t *e) {
    return json_get_str(e, "__raise__", "RuntimeError");
}
/* Materialize a scripted raise spec exactly as the oracle make_exc does:
 * OSError-family with an errno formats "[Errno N] msg" and carries errno.
 * CPython's OSError.__new__ auto-maps a bare OSError(errno, msg) to the
 * errno's subclass (probed on the quarry host, py3.14 Windows); subclasses
 * are never remapped. The table is host-specific like CPython's own. */
typedef struct { int en; const char *cls; } sr_errno_map_t;
static const sr_errno_map_t SR_ERRNO_MAP[] = {
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
#define SR_ERRNO_MAP_N (sizeof(SR_ERRNO_MAP) / sizeof(SR_ERRNO_MAP[0]))

static json_t *sr_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    const json_t *en = spec ? json_obj_get(spec, "errno") : NULL;
    if (en && en->type == JSON_NUMBER && sr_exc_is_a(type, "OSError")) {
        if (strcmp(type, "OSError") == 0) {
            for (size_t i = 0; i < SR_ERRNO_MAP_N; i++) {
                if (SR_ERRNO_MAP[i].en == (int)en->num_val) {
                    type = SR_ERRNO_MAP[i].cls;
                    break;
                }
            }
        }
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = sr_err(type, buf);
        json_set(e, "errno", json_number(en->num_val));
        return e;
    }
    return sr_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). A missing or
 * exhausted script is a fail-closed RuntimeError on both sides. */
static json_t *sr_call(sr_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sr_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sr_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sr_make_exc(raise);
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
static json_t *sr_proj(const json_t *v) {
    if (!v) return json_null();
    if (sr_is_float_tag(v)) return json_number(json_get_num(v, "v", 0));
    if (sr_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *it = json_obj_get(v, "v");
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++)
                json_append(a, sr_proj(json_get(it, i)));
        return a;
    }
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++)
            json_set(o, v->c.keys[i], sr_proj(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, sr_proj(json_get(v, i)));
        return a;
    }
    return json_copy(v);
}

/* -- string helpers (ASCII scope) -- */
static bool sr_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v' || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}
/* Python str.strip() -> malloc'd copy */
static char *sr_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sr_is_ws(s[a])) a++;
    while (b > a && sr_is_ws(s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}
static bool sr_str_empty_after_strip(const char *s) {
    char *t = sr_strip(s);
    bool e = !t || !t[0];
    free(t);
    return e;
}
static bool sr_isascii(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (*p >= 0x80) return false;
    return true;
}
static bool sr_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool sr_ieq_span(const char *s, size_t n, const char *lit) {
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
static bool sr_digits(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

/* Python float(str) on the ASCII domain: ws-strip (float() accepts only
 * " \t\n\r\f\v" - NOT \x1c-\x1f, unlike str.strip()/re \s), sign,
 * inf/infinity/nan (case-insensitive), decimal grammar with single
 * underscores BETWEEN digits, optional fraction and exponent. Returns false
 * where float() raises ValueError (the caller coerces to nan). */
static bool sr_float_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static bool sr_py_float(const char *s, double *out) {
    if (!s) return false;
    while (sr_float_ws(*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && sr_float_ws(e[-1])) e--;
    if (e == s) return false;
    bool neg = false;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (e == s) return false;
    size_t n = (size_t)(e - s);
    if (sr_ieq_span(s, n, "inf") || sr_ieq_span(s, n, "infinity")) {
        *out = neg ? -HUGE_VAL : HUGE_VAL;
        return true;
    }
    if (sr_ieq_span(s, n, "nan")) { *out = NAN; return true; }
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
static size_t sr_utf8_decode(const char *s, unsigned int *cp) {
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
static size_t sr_utf8_encode(unsigned int cp, char *buf) {
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
static char *sr_sanitize_surrogates_utf8(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 8);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; ) {
        unsigned int cp;
        size_t w = sr_utf8_decode(s + i, &cp);
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
typedef struct sr_match {
    bool ok;
    size_t end;
    char *iso;
    char *date;
    char *time;
} sr_match_t;

static bool sr_tz_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '+' || c == '-' ||
           c == '/' || c == ':';
}
static void sr_match_free(sr_match_t *m) {
    free(m->iso); free(m->date); free(m->time);
    m->iso = m->date = m->time = NULL;
}
static void sr_ts_match(const char *s, sr_match_t *m) {
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
        if (strlen(q) < 11 || !sr_digits(q, 4) || q[4] != '-' ||
            !sr_digits(q + 5, 2) || q[7] != '-' || !sr_digits(q + 8, 2) || q[10] != ' ')
            human = false;
        else { memcpy(date, q, 10); date[10] = '\0'; q += 11; }
    }
    /* time: \d{2}:\d{2}:\d{2} */
    if (human) {
        if (strlen(q) < 8 || !sr_digits(q, 2) || q[2] != ':' ||
            !sr_digits(q + 3, 2) || q[5] != ':' || !sr_digits(q + 6, 2))
            human = false;
        else { memcpy(time, q, 8); time[8] = '\0'; q += 8; }
    }
    /* optional tz: ' ' + [A-Za-z0-9_+\-/:]+ (greedy; ']' not in the class) */
    if (human && q[0] == ' ' && sr_tz_char(q[1])) {
        q++;
        while (sr_tz_char(*q)) q++;
    }
    if (human && q[0] == ']') {
        q++;
        while (*q && sr_is_ws(*q)) q++;
        m->ok = true;
        m->end = (size_t)(q - s);
        m->date = strdup(date);
        m->time = strdup(time);
        return;
    }
    /* iso branch: \d{4}-\d{2}-\d{2}T[^\]]+ - the group INCLUDES date+T */
    q = p;
    if (strlen(q) >= 11 && sr_digits(q, 4) && q[4] == '-' &&
        sr_digits(q + 5, 2) && q[7] == '-' && sr_digits(q + 8, 2) && q[10] == 'T') {
        const char *body = q + 11;
        const char *r = body;
        while (*r && *r != ']') r++;
        if (r > body && *r == ']') {
            size_t iso_len = (size_t)(r - q);
            r++;
            while (*r && sr_is_ws(*r)) r++;
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
static const json_t *sr_map_get(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!sr_is_dict(obj)) {
        *err = sr_err_fmt("AttributeError", "'%s' object has no attribute 'get'",
                          sr_type_name(obj));
        return NULL;
    }
    return json_obj_get(obj, key);
}
/* inert attribute read on a JSON double (uuid4().hex): dict keys are attrs. */
static const json_t *sr_attr_get(const json_t *v, const char *name, json_t **err) {
    *err = NULL;
    if (sr_is_dict(v)) {
        const json_t *r = json_obj_get(v, name);
        if (r) return r;
    }
    *err = sr_err_fmt("AttributeError", "'%s' object has no attribute '%s'",
                      sr_type_name(v), name);
    return NULL;
}
/* list(value): list/tuple copy, str -> code-point chars, dict -> keys;
 * anything else raises TypeError "'<type>' object is not iterable". */
static json_t *sr_list_of(const json_t *v) {
    if (sr_is_list(v)) return json_copy(v);
    if (sr_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *it = json_obj_get(v, "v");
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++)
                json_append(a, json_copy(json_get(it, i)));
        return a;
    }
    if (sr_is_str(v)) {
        json_t *a = json_array();
        const char *s = v->str_val;
        size_t n = strlen(s);
        for (size_t i = 0; i < n; ) {
            unsigned int cp;
            size_t w = sr_utf8_decode(s + i, &cp);
            char buf[5];
            size_t bw = sr_utf8_encode(cp, buf);
            buf[bw] = '\0';
            json_append(a, json_string(buf));
            i += w;
        }
        return a;
    }
    if (sr_is_dict(v)) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, json_string(v->c.keys[i]));
        return a;
    }
    return sr_err_fmt("TypeError", "'%s' object is not iterable", sr_type_name(v));
}
/* first n items of a sequence, preserving list-vs-tuple kind (Python [:n]). */
static json_t *sr_slice(const json_t *v, size_t n) {
    bool tup = sr_is_tuple_tag(v);
    json_t *items = json_array();
    size_t len = sr_seq_len(v);
    if (n > len) n = len;
    for (size_t i = 0; i < n; i++)
        json_append(items, json_copy(sr_seq_get(v, i)));
    if (!tup) return items;
    json_t *o = json_object();
    json_set(o, "__tuple__", json_bool(true));
    json_set(o, "v", items);
    return o;
}
/* ============== session_rules/turns.py - uid/typed-ask leaf cluster ============== */

/* uid = msg.get(MESSAGE_UID); return uid if isinstance(uid, str) and uid else None */
static json_t *sr_message_uid_or_none_impl(const json_t *msg) {
    json_t *err = NULL;
    const json_t *uid = sr_map_get(msg, SR_MESSAGE_UID, &err);
    if (err) return err;
    if (sr_is_str(uid) && uid->str_val[0]) return json_copy(uid);
    return json_null();
}
/* PoP: sr_message_uid_or_none @ session_rules/turns.py:message_uid_or_none */
static json_t *sr_message_uid_or_none(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr_message_uid_or_none_impl(json_obj_get(args, "msg"));
}

/* return uuid4().hex */
static json_t *sr_mint_uid_impl(sr_ctx_t *ctx) {
    json_t *u = sr_call(ctx, "uuid4", json_object());
    if (sr_is_raise(u)) return u;
    json_t *err = NULL;
    const json_t *hex = sr_attr_get(u, "hex", &err);
    if (err) { json_free(u); return err; }
    json_t *ret = json_copy(hex);
    json_free(u);
    return ret;
}
/* PoP: sr_mint_uid @ session_rules/turns.py:mint_uid */
static json_t *sr_mint_uid(sr_ctx_t *ctx, json_t *args) {
    (void)args;
    return sr_mint_uid_impl(ctx);
}

/* list(dict.fromkeys(u for u in (value if isinstance(value, list) else ())
 *                    if isinstance(u, str) and u)) */
static json_t *sr_uid_list_impl(const json_t *value) {
    json_t *out = json_array();
    if (!sr_is_list(value)) return out;
    for (size_t i = 0; i < value->c.count; i++) {
        const json_t *u = json_get(value, i);
        if (!sr_is_str(u) || !u->str_val[0]) continue;
        bool dup = false;
        for (size_t j = 0; j < out->c.count; j++)
            if (sr_eq(json_get(out, j), u)) { dup = true; break; }
        if (!dup) json_append(out, json_copy(u));
    }
    return out;
}
/* PoP: sr_uid_list @ session_rules/turns.py:uid_list */
static json_t *sr_uid_list(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr_uid_list_impl(json_obj_get(args, "value"));
}

/* content is None or content == [] or (isinstance(content, str) and not content.strip()) */
static bool sr_is_empty_content_impl(const json_t *content) {
    if (sr_is_null(content)) return true;
    json_t *empty = json_array();
    bool eq = sr_eq(content, empty);
    json_free(empty);
    if (eq) return true;
    return sr_is_str(content) && sr_str_empty_after_strip(content->str_val);
}
/* PoP: sr_is_empty_content @ session_rules/turns.py:_is_empty_content */
static json_t *sr_is_empty_content(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return json_bool(sr_is_empty_content_impl(json_obj_get(args, "content")));
}

/* metadata = message.get("display_metadata");
 * if isinstance(metadata, dict) and TYPED_ASK in metadata:
 *     message["display_metadata"] = {k: v for k, v in metadata.items() if k != TYPED_ASK} */
static json_t *sr_drop_typed_ask_impl(json_t *message) {
    json_t *err = NULL;
    const json_t *md = sr_map_get(message, "display_metadata", &err);
    if (err) return err;
    if (sr_is_dict(md) && json_obj_get(md, SR_TYPED_ASK)) {
        json_t *n = json_object();
        for (size_t i = 0; i < md->c.count; i++) {
            if (strcmp(md->c.keys[i], SR_TYPED_ASK) == 0) continue;
            json_set(n, md->c.keys[i], json_copy(md->c.items[i]));
        }
        json_set(message, "display_metadata", n);
    }
    return json_null();
}
/* PoP: sr_drop_typed_ask @ session_rules/turns.py:drop_typed_ask */
static json_t *sr_drop_typed_ask(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr_drop_typed_ask_impl(json_obj_get(args, "message"));
}

/* record_absorbed_message(survivor, dropped, *, dropped_leads=False) */
static json_t *sr_record_absorbed_message_impl(json_t *survivor, json_t *dropped,
                                               bool dropped_leads) {
    json_t *err = NULL;
    const json_t *dcontent = sr_map_get(dropped, "content", &err);
    if (err) return err;
    if (!sr_is_empty_content_impl(dcontent)) {
        json_t *r = sr_drop_typed_ask_impl(survivor);
        if (sr_is_raise(r)) return r;
        json_free(r);
    }
    json_t *suid = sr_message_uid_or_none_impl(survivor);
    if (sr_is_raise(suid)) return suid;
    json_t *duid = sr_message_uid_or_none_impl(dropped);
    if (sr_is_raise(duid)) { json_free(suid); return duid; }
    const json_t *sabs = sr_map_get(survivor, SR_ABSORBED_MESSAGE_UIDS, &err);
    if (err) { json_free(suid); json_free(duid); return err; }
    json_t *own = sr_uid_list_impl(sabs);
    const json_t *dabs = sr_map_get(dropped, SR_ABSORBED_MESSAGE_UIDS, &err);
    if (err) { json_free(suid); json_free(duid); json_free(own); return err; }
    json_t *theirs = sr_uid_list_impl(dabs);
    json_t *ordered = json_array();
    if (dropped_leads && sr_truthy(duid)) {
        json_set(survivor, SR_MESSAGE_UID, json_copy(duid));
        for (size_t i = 0; i < theirs->c.count; i++)
            json_append(ordered, json_copy(json_get(theirs, i)));
        if (sr_truthy(suid)) json_append(ordered, json_copy(suid));
        for (size_t i = 0; i < own->c.count; i++)
            json_append(ordered, json_copy(json_get(own, i)));
    } else {
        for (size_t i = 0; i < own->c.count; i++)
            json_append(ordered, json_copy(json_get(own, i)));
        if (sr_truthy(duid)) json_append(ordered, json_copy(duid));
        for (size_t i = 0; i < theirs->c.count; i++)
            json_append(ordered, json_copy(json_get(theirs, i)));
    }
    json_free(own);
    json_free(theirs);
    /* [uid for uid in dict.fromkeys(ordered) if uid != survivor.get(MESSAGE_UID)]
     * (dedupe-then-filter == filter-while-deduping: the predicate is value-only) */
    const json_t *live_uid = json_obj_get(survivor, SR_MESSAGE_UID);
    json_t *absorbed = json_array();
    for (size_t i = 0; i < ordered->c.count; i++) {
        const json_t *u = json_get(ordered, i);
        bool dup = false;
        for (size_t j = 0; j < absorbed->c.count; j++)
            if (sr_eq(json_get(absorbed, j), u)) { dup = true; break; }
        if (dup) continue;
        if (sr_eq(u, live_uid)) continue;
        json_append(absorbed, json_copy(u));
    }
    json_free(ordered);
    if (absorbed->c.count > 0)
        json_set(survivor, SR_ABSORBED_MESSAGE_UIDS, absorbed);
    else
        json_free(absorbed);
    json_free(suid);
    json_free(duid);
    return json_null();
}
/* PoP: sr_record_absorbed_message @ session_rules/turns.py:record_absorbed_message */
static json_t *sr_record_absorbed_message(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr_record_absorbed_message_impl(json_obj_get(args, "survivor"),
                                           json_obj_get(args, "dropped"),
                                           json_get_bool(args, "dropped_leads", false));
}

/* ============== persistence/sqlite/conversation/_store/rules.py ============== */

/* metadata = message.get("display_metadata");
 * if isinstance(metadata, dict) and metadata.get(DATA_USE):
 *     message[DATA_USE] = list(metadata[DATA_USE]) */
static json_t *sr_lift_data_use_impl(json_t *message) {
    json_t *err = NULL;
    const json_t *md = sr_map_get(message, "display_metadata", &err);
    if (err) return err;
    if (sr_is_dict(md)) {
        const json_t *du = json_obj_get(md, SR_DATA_USE);
        if (sr_truthy(du)) {
            json_t *lst = sr_list_of(du);
            if (sr_is_raise(lst)) return lst;
            json_set(message, SR_DATA_USE, lst);
        }
    }
    return json_null();
}
/* PoP: sr_lift_data_use @ persistence/sqlite/conversation/_store/rules.py:lift_data_use */
static json_t *sr_lift_data_use(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr_lift_data_use_impl(json_obj_get(args, "message"));
}

/* uid = message_uid_or_none(msg);
 * if uid is None: uid = msg[MESSAGE_UID] = mint_uid(); return uid */
static json_t *sr_stamp_message_uid_impl(sr_ctx_t *ctx, json_t *msg) {
    json_t *uid = sr_message_uid_or_none_impl(msg);
    if (sr_is_raise(uid)) return uid;
    if (sr_is_null(uid)) {
        json_free(uid);
        json_t *mint = sr_mint_uid_impl(ctx);
        if (sr_is_raise(mint)) return mint;
        json_set(msg, SR_MESSAGE_UID, json_copy(mint));
        return mint;
    }
    return uid;
}
/* PoP: sr_stamp_message_uid @ persistence/sqlite/conversation/_store/rules.py:stamp_message_uid */
static json_t *sr_stamp_message_uid(sr_ctx_t *ctx, json_t *args) {
    return sr_stamp_message_uid_impl(ctx, json_obj_get(args, "msg"));
}

/* isinstance(item, dict) and item.get("type") == "compaction" */
static bool sr_is_checkpoint_item_impl(const json_t *item) {
    return sr_is_dict(item) && sr_eq_str(json_obj_get(item, "type"), "compaction");
}
/* PoP: sr_is_checkpoint_item @ persistence/sqlite/conversation/_store/rules.py:_is_checkpoint_item */
static json_t *sr_is_checkpoint_item(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return json_bool(sr_is_checkpoint_item_impl(json_obj_get(args, "item")));
}

/* reverse scan: last assistant dict whose key-list carries a checkpoint item */
static json_t *sr_newest_checkpoint_carrier_impl(const json_t *messages, const char *key) {
    if (!sr_is_list(messages))
        return sr_err_fmt("TypeError", "object of type '%s' has no len()", sr_type_name(messages));
    for (size_t k = messages->c.count; k > 0; k--) {
        size_t i = k - 1;
        const json_t *msg = json_get(messages, i);
        if (!sr_is_dict(msg) || !sr_eq_str(json_obj_get(msg, "role"), "assistant"))
            continue;
        const json_t *items = json_obj_get(msg, key ? key : "");
        if (!sr_is_list(items)) continue;
        for (size_t j = 0; j < items->c.count; j++) {
            if (sr_is_checkpoint_item_impl(json_get(items, j)))
                return json_number((double)i);
        }
    }
    return json_number(-1);
}
/* PoP: sr_newest_checkpoint_carrier @ persistence/sqlite/conversation/_store/rules.py:_newest_checkpoint_carrier */
static json_t *sr_newest_checkpoint_carrier(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr_newest_checkpoint_carrier_impl(json_obj_get(args, "messages"),
                                             json_get_str(args, "key", "content"));
}

/* if text.isascii(): return text; return _SURROGATE_RE.sub('\ufffd', text) */
static json_t *sr_sanitize_surrogates_impl(const json_t *text) {
    if (!sr_is_str(text))
        return sr_err_fmt("AttributeError", "'%s' object has no attribute 'isascii'",
                          sr_type_name(text));
    if (sr_isascii(text->str_val)) return json_copy(text);
    char *out = sr_sanitize_surrogates_utf8(text->str_val);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}
/* PoP: sr_sanitize_surrogates @ persistence/sqlite/conversation/_store/rules.py:_sanitize_surrogates */
static json_t *sr_sanitize_surrogates(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return sr_sanitize_surrogates_impl(json_obj_get(args, "text"));
}

/* _localize(dt, tz): epoch for dt in tz or the local zone; None outside limits.
 * sys.platform is seeded "win32"; the OSError EINVAL fold recovery is the
 * Windows-specific branch. dt.timetuple() sits OUTSIDE the inner try. */
static json_t *sr_localize_impl(sr_ctx_t *ctx, const json_t *dt, const json_t *tz) {
    if (!sr_is_dt_tag(dt))
        return sr_err_fmt("AttributeError", "'%s' object has no attribute 'tzinfo'",
                          sr_type_name(dt));
    const json_t *tzinfo = json_obj_get(dt, "tzinfo");
    bool naive_local = sr_is_null(tzinfo) && sr_is_null(tz);
    const json_t *cur = dt;
    bool cur_owned = false;
    json_t *ret = NULL;
    if (sr_is_null(tzinfo)) {
        json_t *a = json_object();
        json_set(a, "dt", json_copy(dt));
        json_t *nd;
        if (!sr_is_null(tz)) {
            json_set(a, "tzinfo", json_copy(tz));
            nd = sr_call(ctx, "dt.replace", a);
        } else {
            nd = sr_call(ctx, "dt.astimezone", a);
        }
        if (sr_is_raise(nd)) { ret = nd; goto done; }
        cur = nd;
        cur_owned = true;
    }
    {
        json_t *a = json_object();
        json_set(a, "dt", json_copy(cur));
        json_t *ts = sr_call(ctx, "dt.timestamp", a);
        if (sr_is_raise(ts)) {
            const char *cls = sr_raise_cls(ts);
            if (sr_exc_is_a(cls, "OverflowError") || sr_exc_is_a(cls, "ValueError")) {
                json_free(ts);
                ret = json_null();
                goto done;
            }
            if (sr_exc_is_a(cls, "OSError")) {
                double en = json_get_num(ts, "errno", -1);
                bool recover = (strcmp(SR_SYS_PLATFORM, "win32") == 0) &&
                               naive_local && (en == (double)SR_ERRNO_EINVAL);
                json_free(ts);
                if (!recover) { ret = json_null(); goto done; }
                json_t *wa = json_object();
                json_set(wa, "dt", json_copy(cur));
                json_t *wall = sr_call(ctx, "dt.timetuple", wa);
                if (sr_is_raise(wall)) { ret = wall; goto done; }
                json_t *ma = json_object();
                json_set(ma, "wall_time", json_copy(wall));
                json_t *ep = sr_call(ctx, "time.mktime", ma);
                if (sr_is_raise(ep)) {
                    const char *c2 = sr_raise_cls(ep);
                    if (sr_exc_is_a(c2, "OSError") || sr_exc_is_a(c2, "OverflowError") ||
                        sr_exc_is_a(c2, "ValueError")) {
                        json_free(ep); json_free(wall);
                        ret = json_null();
                        goto done;
                    }
                    json_free(wall);
                    ret = ep;
                    goto done;
                }
                double epoch = sr_num(ep);
                if (epoch >= 0.0 && epoch < 86400.0) {
                    json_t *la = json_object();
                    json_set(la, "epoch", json_copy(ep));
                    json_t *lt = sr_call(ctx, "time.localtime", la);
                    if (sr_is_raise(lt)) {
                        const char *c3 = sr_raise_cls(lt);
                        if (sr_exc_is_a(c3, "OSError") || sr_exc_is_a(c3, "OverflowError") ||
                            sr_exc_is_a(c3, "ValueError")) {
                            json_free(lt); json_free(ep); json_free(wall);
                            ret = json_null();
                            goto done;
                        }
                        json_free(ep); json_free(wall);
                        ret = lt;
                        goto done;
                    }
                    json_t *ls = sr_slice(lt, 6);
                    json_t *ws = sr_slice(wall, 6);
                    bool same = sr_eq(ls, ws);
                    json_free(ls); json_free(ws); json_free(lt);
                    if (same) {
                        double micro = json_get_num(cur, "microsecond", 0);
                        ret = json_number(epoch + micro / 1000000.0);
                        json_free(ep); json_free(wall);
                        goto done;
                    }
                }
                json_free(ep); json_free(wall);
                ret = json_null();
                goto done;
            }
            ret = ts;  /* TypeError / RuntimeError / BaseException... propagate */
            goto done;
        }
        ret = json_number(sr_num(ts));
        json_free(ts);
    }
done:
    if (cur_owned) json_free((json_t *)cur);
    return ret;
}
/* PoP: sr_localize @ persistence/sqlite/conversation/_store/rules.py:_localize */
static json_t *sr_localize(sr_ctx_t *ctx, json_t *args) {
    return sr_localize_impl(ctx, json_obj_get(args, "dt"), json_obj_get(args, "tz"));
}

/* for parse in (datetime.fromisoformat, lambda t: datetime.strptime(t, FMT)):
 *     try: return _localize(parse(text), tz)
 *     except (TypeError, ValueError): continue
 * return None */
static json_t *sr_parse_iso_impl(sr_ctx_t *ctx, const json_t *text, const json_t *tz) {
    json_t *a = json_object();
    json_set(a, "text", text ? json_copy(text) : json_null());
    json_t *p = sr_call(ctx, "datetime.fromisoformat", a);
    if (sr_is_raise(p)) {
        const char *cls = sr_raise_cls(p);
        if (sr_exc_is_a(cls, "TypeError") || sr_exc_is_a(cls, "ValueError")) {
            json_free(p);
            goto parser2;
        }
        return p;
    }
    {
        json_t *loc = sr_localize_impl(ctx, p, tz);
        json_free(p);
        if (sr_is_raise(loc)) {
            const char *cls = sr_raise_cls(loc);
            if (sr_exc_is_a(cls, "TypeError") || sr_exc_is_a(cls, "ValueError")) {
                json_free(loc);
                goto parser2;
            }
            return loc;
        }
        return loc;
    }
parser2:
    {
        json_t *a2 = json_object();
        json_set(a2, "text", text ? json_copy(text) : json_null());
        json_set_hdr(a2, "fmt", "%Y-%m-%dT%H:%M:%S%z");
        json_t *p2 = sr_call(ctx, "datetime.strptime", a2);
        if (sr_is_raise(p2)) {
            const char *cls = sr_raise_cls(p2);
            if (sr_exc_is_a(cls, "TypeError") || sr_exc_is_a(cls, "ValueError")) {
                json_free(p2);
                return json_null();
            }
            return p2;
        }
        json_t *loc2 = sr_localize_impl(ctx, p2, tz);
        json_free(p2);
        if (sr_is_raise(loc2)) {
            const char *cls = sr_raise_cls(loc2);
            if (sr_exc_is_a(cls, "TypeError") || sr_exc_is_a(cls, "ValueError")) {
                json_free(loc2);
                return json_null();
            }
            return loc2;
        }
        return loc2;
    }
}
/* PoP: sr_parse_iso @ persistence/sqlite/conversation/_store/rules.py:_parse_iso */
static json_t *sr_parse_iso(sr_ctx_t *ctx, json_t *args) {
    return sr_parse_iso_impl(ctx, json_obj_get(args, "text"), json_obj_get(args, "tz"));
}

/* if match.group("iso"): return _parse_iso(match.group("iso"), tz)
 * try: dt = datetime.strptime(f"{date} {time}", "%Y-%m-%d %H:%M:%S")
 * except ValueError: return None
 * return _localize(dt, tz) */
static json_t *sr_parse_timestamp_match_impl(sr_ctx_t *ctx, const sr_match_t *m,
                                             const json_t *tz) {
    if (!m || !m->ok)
        return sr_err("AttributeError", "'NoneType' object has no attribute 'group'");
    if (m->iso && m->iso[0]) {
        json_t *iso_node = json_string(m->iso);
        json_t *r = sr_parse_iso_impl(ctx, iso_node, tz);
        json_free(iso_node);
        return r;
    }
    char buf[64];
    snprintf(buf, sizeof buf, "%s %s", m->date ? m->date : "", m->time ? m->time : "");
    json_t *a = json_object();
    json_set_hdr(a, "text", buf);
    json_set_hdr(a, "fmt", "%Y-%m-%d %H:%M:%S");
    json_t *dt = sr_call(ctx, "datetime.strptime", a);
    if (sr_is_raise(dt)) {
        if (sr_exc_is_a(sr_raise_cls(dt), "ValueError")) {
            json_free(dt);
            return json_null();
        }
        return dt;
    }
    json_t *loc = sr_localize_impl(ctx, dt, tz);
    json_free(dt);
    return loc;
}
/* PoP: sr_parse_timestamp_match @ persistence/sqlite/conversation/_store/rules.py:_parse_timestamp_match */
static json_t *sr_parse_timestamp_match(sr_ctx_t *ctx, json_t *args) {
    const json_t *mt = json_obj_get(args, "match_text");
    if (!sr_is_str(mt))
        return sr_err_fmt("TypeError", "expected string or bytes-like object, got '%s'",
                          sr_type_name(mt));
    sr_match_t m;
    sr_ts_match(mt->str_val, &m);
    json_t *r = sr_parse_timestamp_match_impl(ctx, &m, json_obj_get(args, "tz"));
    sr_match_free(&m);
    return r;
}

/* while (match := _TIMESTAMP_PREFIX_RE.match(text)) is not None:
 *     parsed = _parse_timestamp_match(match, tz=tz)
 *     if parsed is not None: embedded_epoch = parsed
 *     text = text[match.end():]
 * return text, embedded_epoch */
static json_t *sr_strip_leading_message_timestamps_impl(sr_ctx_t *ctx,
                                                        const json_t *content,
                                                        const json_t *tz) {
    if (!sr_is_str(content) || !content->str_val[0]) {
        json_t *t = json_array();
        json_append(t, content ? json_copy(content) : json_null());
        json_append(t, json_null());
        json_t *o = json_object();
        json_set(o, "__tuple__", json_bool(true));
        json_set(o, "v", t);
        return o;
    }
    const char *text = content->str_val;
    json_t *embedded = NULL;
    for (;;) {
        sr_match_t m;
        sr_ts_match(text, &m);
        if (!m.ok) break;
        json_t *parsed = sr_parse_timestamp_match_impl(ctx, &m, tz);
        size_t end = m.end;
        sr_match_free(&m);
        if (sr_is_raise(parsed)) {
            if (embedded) json_free(embedded);
            return parsed;
        }
        if (!sr_is_null(parsed)) {
            if (embedded) json_free(embedded);
            embedded = parsed;
        } else {
            json_free(parsed);
        }
        text += end;
    }
    json_t *t = json_array();
    json_append(t, json_string(text));
    json_append(t, embedded ? embedded : json_null());
    json_t *o = json_object();
    json_set(o, "__tuple__", json_bool(true));
    json_set(o, "v", t);
    return o;
}
/* PoP: sr_strip_leading_message_timestamps @ persistence/sqlite/conversation/_store/rules.py:strip_leading_message_timestamps */
static json_t *sr_strip_leading_message_timestamps(sr_ctx_t *ctx, json_t *args) {
    return sr_strip_leading_message_timestamps_impl(ctx, json_obj_get(args, "content"),
                                                    json_obj_get(args, "tz"));
}

/* coerce_epoch(value, *, session_id=None, field="timestamp") */
static json_t *sr_coerce_epoch_impl(sr_ctx_t *ctx, const json_t *value,
                                    const json_t *session_id, const char *field) {
    if (sr_is_null(value) || sr_eq_str(value, "")) return json_null();
    double ts;
    if (sr_is_dt_tag(value)) {
        json_t *a = json_object();
        json_set(a, "dt", json_copy(value));
        json_t *r = sr_call(ctx, "dt.timestamp", a);
        if (sr_is_raise(r)) {
            const char *cls = sr_raise_cls(r);
            if (sr_exc_is_a(cls, "TypeError") || sr_exc_is_a(cls, "ValueError")) {
                json_free(r);
                ts = NAN;
            } else {
                return r;
            }
        } else {
            ts = sr_num(r);
            json_free(r);
        }
    } else if (sr_is_str(value)) {
        if (!sr_py_float(value->str_val, &ts)) ts = NAN;
    } else if (sr_is_numlike(value)) {
        ts = sr_num(value);
    } else {
        ts = NAN;  /* float(list/dict/tuple) raises TypeError -> nan */
    }
    if (!(ts >= SR_EPOCH_MIN && ts <= SR_EPOCH_MAX)) {  /* also false for nan */
        char suffix[256];
        if (sr_truthy(session_id))
            snprintf(suffix, sizeof suffix, " on session %s",
                     sr_str(session_id) ? sr_str(session_id) : "");
        else
            suffix[0] = '\0';
        json_t *a = json_object();
        json_set_hdr(a, "fmt", "Ignoring corrupt %s %r%s");
        json_t *wargs = json_array();
        json_append(wargs, json_string(field ? field : "timestamp"));
        json_append(wargs, value ? json_copy(value) : json_null());
        json_append(wargs, json_string(suffix));
        json_set(a, "args", wargs);
        json_t *w = sr_call(ctx, "logger.warning", a);
        if (sr_is_raise(w)) return w;
        json_free(w);
        return json_null();
    }
    return json_number(ts);
}
/* PoP: sr_coerce_epoch @ persistence/sqlite/conversation/_store/rules.py:coerce_epoch */
static json_t *sr_coerce_epoch(sr_ctx_t *ctx, json_t *args) {
    return sr_coerce_epoch_impl(ctx, json_obj_get(args, "value"),
                                json_obj_get(args, "session_id"),
                                json_get_str(args, "field", "timestamp"));
}

/* isinstance(part, dict) and part.get("type") in _IMAGE_PART_TYPES */
static bool sr_is_image_part_impl(const json_t *part) {
    if (!sr_is_dict(part)) return false;
    const json_t *t = json_obj_get(part, "type");
    if (!sr_is_str(t)) return false;
    for (size_t i = 0; i < SR_IMAGE_PART_TYPES_N; i++)
        if (strcmp(t->str_val, SR_IMAGE_PART_TYPES[i]) == 0) return true;
    return false;
}
/* PoP: sr_is_image_part @ persistence/sqlite/conversation/_store/rules.py:_is_image_part */
static json_t *sr_is_image_part(sr_ctx_t *ctx, json_t *args) {
    (void)ctx;
    return json_bool(sr_is_image_part_impl(json_obj_get(args, "part")));
}
