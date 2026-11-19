/*
 * telegram_adapter_flow.c — Telegram adapter FLOW cores of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the DM-topic /
 * forum-thread send-routing decision chain (metadata readers, anchor
 * resolution, thread kwargs, chunk/single-send routing), the polling/send
 * error-classifier chain (exception-graph walk, PTB type probes, anchor /
 * bad-request / timeout / rich-capability classification) and the Bot API
 * 10.1 rich-message eligibility chain (shape regexes, size cap, opt-in
 * gates), plus chat-type / config-extra / notification leaves.
 *
 * Decision sequences only: every live I/O / injected callable
 * (parse_telegram_message_id, inspect.iscoroutinefunction,
 * _rich_message_payload, telegram.error isinstance probes) enters through
 * ONE universal seam: ta_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle
 * installs recording stubs emitting the identical event shape
 * {"name", "args", "ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ta_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow.py):
 * - NULL json_t* == Python None / absent; a JSON_NULL node is a real null
 *   VALUE. Exception specs: {"__raise__": cls, "message": str(exc)}.
 * - Adapter state (in.state): "_reply_to_mode", "_notifications_mode",
 *   "_rich_messages_enabled" (default True), "_rich_send_disabled",
 *   "_allow_cjk_rich_messages", "config" (null or {"extra": {...}}),
 *   "_bot" (null or {"do_api_request": <marker>}), "ptb_available"
 *   (default true: telegram.error importable), "timedout_missing"
 *   (default false: telegram.error lacks TimedOut -> AttributeError).
 *   None of the ported defs mutate state; out.state echoes it.
 * - Exception graph (in.errors): id -> {"class": name, "message": str,
 *   "base": builtin-base-name (default Exception), "cause": id|null,
 *   "context": id|null, "error_code": any}. The oracle builds real
 *   exception objects (builtin class when the name is one, else a
 *   synthetic subclass of "base") and wires __cause__/__context__.
 *   _iter_exception_graph projects the yield order to an id list.
 * - telegram.error isinstance probes are the seam
 *   "telegram_error_isinstance" {"error": id, "class": name}, ONE event
 *   per class in tuple order, short-circuiting on the first true (the
 *   oracle's PTB class doubles use a metaclass __instancecheck__ that
 *   records the same event). Import failure is state-driven
 *   (ptb_available=false -> ImportError branch, no seam events);
 *   timedout_missing=true models the AttributeError import branch of
 *   _is_timed_out. Builtin isinstance checks (OSError / AttributeError /
 *   TypeError / NotImplementedError) walk the fixture class/base chain
 *   plus the builtin hierarchy — no seam.
 * - int() ports carry Python's exact ValueError/TypeError/OverflowError
 *   messages (probed on the quarry CPython 3.14.9); int(str) strips the
 *   full Unicode White_Space set, accepts a sign and underscores BETWEEN
 *   digits only; int(float) truncates toward zero.
 * - str() projection covers strings, integral/non-integral numbers,
 *   bools and None; containers in str() slots are OUTSIDE the fixture
 *   corpus (Python repr is not modeled). lower()/regex \s fold ASCII
 *   only in case-sensitive matching positions; strip() emptiness and
 *   int() trimming use the full Unicode White_Space set. CJK detection
 *   and len() are UTF-8 codepoint-aware.
 * - str.splitlines() splits \n \r \r\n \v \f \x1c \x1d \x1e (ASCII
 *   corpus; U+0085/U+2028/U+2029 boundaries are outside it). re
 *   MULTILINE ^ anchors after \n only. The details/math regexes are
 *   ported as greedy/lazy scanners with IGNORECASE+DOTALL semantics;
 *   \b treats codepoints >= 0x80 as word characters.
 * - logger calls are silent on both sides (none of the ported defs log).
 * - Fail-closed: a missing/exhausted seam script is a RuntimeError raise
 *   spec recorded in the trace before propagation; with no io channel
 *   every seam call fails closed. except-ImportError sites swallow ONLY
 *   ImportError (plus AttributeError for _is_timed_out); every other
 *   kind — BaseException trio included — passes through.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TA_RICH_MESSAGE_MAX_CHARS 32768
#define TA_GENERAL_TOPIC_THREAD_ID "1"

/* ── universal seam context ── */
typedef struct ta_ctx {
    json_t *state;        /* owned; echoed as out.state (never mutated) */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta_ctx_t;

/* ── basic helpers ── */
static bool ta_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *ta_raise_kind(const json_t *v) {
    return ta_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool ta_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *ta_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool ta_str_eq(const json_t *v, const char *s) {
    const char *t = ta_str(v);
    return t && s && strcmp(t, s) == 0;
}

static bool ta_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static json_t *ta_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static bool ta_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *ta_call(ta_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta_err(json_get_str(raise, "type", "RuntimeError"),
                             json_get_str(raise, "message", ""));
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

/* ── string helpers ── */
static char *ta_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Decode one UTF-8 codepoint; advances *p. Invalid bytes decode as U+FFFD
 * without advancing past the lead byte (fixtures are valid UTF-8). */
static unsigned long ta_next_cp(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned char c = s[0];
    if (c == 0) return 0;
    if (c < 0x80) { *p += 1; return c; }
    unsigned long cp; int n;
    if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; n = 2; }
    else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; n = 3; }
    else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; n = 4; }
    else { *p += 1; return 0xfffd; }
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xc0) != 0x80) { *p += 1; return 0xfffd; }
        cp = (cp << 6) | (s[i] & 0x3f);
    }
    *p += n;
    return cp;
}

/* Full Unicode White_Space set (str.strip / int() trimming). */
static bool ta_is_space_cp(unsigned long cp) {
    if (cp >= 0x09 && cp <= 0x0d) return true;
    if (cp >= 0x1c && cp <= 0x1f) return true;
    if (cp == 0x20 || cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}

static bool ta_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip(): full Unicode White_Space both ends. Caller frees. */
static char *ta_strip(const char *s) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        const char *q = p;
        unsigned long cp = ta_next_cp(&q);
        if (!ta_is_space_cp(cp)) break;
        p = q;
    }
    const char *end = p + strlen(p);
    while (end > p) {
        const char *q = end - 1;
        while (q > p && ((unsigned char)*q & 0xc0) == 0x80) q--;
        const char *r = q;
        unsigned long cp = ta_next_cp(&r);
        if (r != end || !ta_is_space_cp(cp)) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}

/* content.strip() truthiness: any non-whitespace codepoint present. */
static bool ta_has_nonspace(const char *s) {
    if (!s) return false;
    const char *p = s;
    while (*p) {
        unsigned long cp = ta_next_cp(&p);
        if (!ta_is_space_cp(cp)) return true;
    }
    return false;
}

/* str.lower(): ASCII fold (fixture corpus folds ASCII only). */
static char *ta_lower(const char *s) {
    char *o = ta_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* UTF-8 codepoint count (Python len counts characters). */
static size_t ta_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}

/* Python float repr: shortest round-trip decimal (fixtures stay in the
 * plain-decimal range; exponent forms are outside the corpus). */
static void ta_float_repr(double d, char *buf, size_t cap) {
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
    snprintf(buf, cap, "%.17g", d);
}

/* str() projection for scalar JSON values (containers are outside the
 * fixture corpus). Caller frees. */
static char *ta_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta_strdup("None");
    switch (v->type) {
        case JSON_STRING: return ta_strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return ta_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) snprintf(buf, sizeof buf, "%.0f", d);
            else ta_float_repr(d, buf, sizeof buf);
            return ta_strdup(buf);
        }
        default: {
            /* Outside the documented corpus; serialize so a stray
             * container reads as a visible mismatch, never as silence. */
            char *s = json_serialize(v);
            char *o = ta_strdup(s ? s : "");
            free(s);
            return o;
        }
    }
}

/* Python repr() of an ASCII str for int() ValueError messages. */
static char *ta_repr_str(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n * 4 + 3);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\'' || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) { k += (size_t)snprintf(o + k, 5, "\\x%02x", c); }
        else o[k++] = (char)c;
    }
    o[k++] = '\'';
    o[k] = '\0';
    return o;
}

/* Python int() on a scalar JSON value. Returns a fresh number node, or a
 * raise spec with the exact CPython 3.14 message. */
static json_t *ta_pyint(const json_t *v) {
    if (!v || v->type == JSON_NULL)
        return ta_err("TypeError",
                      "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isinf(d)) return ta_err("OverflowError", "cannot convert float infinity to integer");
        if (isnan(d)) return ta_err("ValueError", "cannot convert float NaN to integer");
        return json_number(trunc(d));
    }
    if (v->type != JSON_STRING)
        return ta_err("TypeError",
                      "int() argument must be a string, a bytes-like object or a real number, not 'list'");
    const char *raw = v->str_val ? v->str_val : "";
    char *clean = ta_strip(raw);
    if (!clean) return ta_err("RuntimeError", "out of memory");
    const char *p = clean;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    /* underscores must sit BETWEEN two digits */
    const char *digits = p;
    size_t nd = 0;
    bool ok = *p != '\0';
    for (const char *q = p; *q; q++) {
        if (*q == '_') {
            if (q == digits || q[1] < '0' || q[1] > '9' || q[-1] < '0' || q[-1] > '9') { ok = false; break; }
        } else if (*q >= '0' && *q <= '9') {
            nd++;
        } else { ok = false; break; }
    }
    if (!ok || nd == 0) {
        char *r = ta_repr_str(raw);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        free(clean);
        return ta_err("ValueError", msg);
    }
    /* accumulate (fixture ids fit long long; wrap is outside the corpus) */
    long long acc = 0;
    for (const char *q = digits; *q; q++)
        if (*q != '_') acc = acc * 10 + (*q - '0');
    free(clean);
    return json_number((double)(neg ? -acc : acc));
}
/* ── exception-graph registry ── */
static const json_t *ta_err_obj(const ta_ctx_t *ctx, const char *eid) {
    if (!eid || !ctx->errors || ctx->errors->type != JSON_OBJECT) return NULL;
    return json_obj_get(ctx->errors, eid);
}

static const char *ta_err_class(const ta_ctx_t *ctx, const char *eid) {
    const json_t *e = ta_err_obj(ctx, eid);
    return e ? json_get_str(e, "class", "Exception") : "Exception";
}

static const char *ta_err_text(const ta_ctx_t *ctx, const char *eid) {
    const json_t *e = ta_err_obj(ctx, eid);
    return e ? json_get_str(e, "message", "") : "";
}

/* Builtin exception hierarchy (the isinstance surface the ported defs use). */
static const char *ta_builtin_parent(const char *name) {
    static const char *oserr[] = {
        "TimeoutError", "PermissionError", "FileNotFoundError", "FileExistsError",
        "BlockingIOError", "ConnectionError", "ConnectionResetError", "BrokenPipeError",
        "InterruptedError", "NotADirectoryError", "IsADirectoryError",
        "ChildProcessError", "ProcessLookupError", "IOError",
    };
    for (size_t i = 0; i < sizeof(oserr) / sizeof(oserr[0]); i++)
        if (strcmp(name, oserr[i]) == 0) return "OSError";
    if (strcmp(name, "NotImplementedError") == 0) return "RuntimeError";
    static const char *exc[] = {
        "RuntimeError", "AttributeError", "TypeError", "ValueError", "ImportError",
        "KeyError", "IndexError", "LookupError", "ArithmeticError", "OverflowError",
        "StopIteration", "UnicodeError", "OSError", "Exception", "RecursionError",
        "MemoryError", "NameError", "UnboundLocalError", "SyntaxError", "IndentationError",
        "TabError", "SystemError", "ReferenceError", "BufferError", "AssertionError",
        "EOFError", "FloatingPointError", "ZeroDivisionError", "ModuleNotFoundError",
    };
    for (size_t i = 0; i < sizeof(exc) / sizeof(exc[0]); i++)
        if (strcmp(name, exc[i]) == 0)
            return (strcmp(name, "OSError") == 0 || strcmp(name, "Exception") == 0 ||
                    strcmp(name, "ModuleNotFoundError") == 0)
                       ? (strcmp(name, "ModuleNotFoundError") == 0 ? "ImportError" : "BaseException")
                       : "Exception";
    return NULL;
}

static bool ta_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 16; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta_builtin_parent(cur);
    }
    return false;
}

/* isinstance(error, <builtin>) over the fixture class/base chain. */
static bool ta_err_isinstance(const ta_ctx_t *ctx, const char *eid, const char *target) {
    const json_t *e = ta_err_obj(ctx, eid);
    if (!e) return false;
    const char *cls = json_get_str(e, "class", "Exception");
    if (ta_chain_has(cls, target)) return true;
    /* a builtin class name IS the real builtin (the oracle ignores "base"
     * for it); only synthetic classes inherit the fixture base */
    if (ta_chain_has(cls, "BaseException")) return false;
    const char *base = json_get_str(e, "base", "Exception");
    return ta_chain_has(base, target);
}

/* ── seam wrappers ── */
static json_t *ta_parse_message_id(ta_ctx_t *ctx, const json_t *value) {
    json_t *args = json_object();
    json_set(args, "value", value ? json_copy(value) : json_null());
    return ta_call(ctx, "parse_telegram_message_id", args);
}

static json_t *ta_ptb_isinstance(ta_ctx_t *ctx, const char *eid, const char *cls) {
    json_t *args = json_object();
    json_set_hdr(args, "error", eid ? eid : "");
    json_set_hdr(args, "class", cls);
    return ta_call(ctx, "telegram_error_isinstance", args);
}

static const json_t *ta_state(const ta_ctx_t *ctx, const char *key);

static bool ta_ptb_available(const ta_ctx_t *ctx) {
    const json_t *v = ta_state(ctx, "ptb_available");
    return v ? ta_truthy(v) : true;
}

static bool ta_timedout_missing(const ta_ctx_t *ctx) {
    const json_t *v = ta_state(ctx, "timedout_missing");
    return v ? ta_truthy(v) : false;
}

/* ── state / arg readers ── */
static const json_t *ta_state(const ta_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}

static const json_t *ta_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}

static json_t *ta_args1(const char *k, const json_t *v) {
    json_t *a = json_object();
    json_set(a, k, v ? json_copy(v) : json_null());
    return a;
}

/* ── regex ports (ASCII \s corpus; see header) ── */
static const char *ta_skip_ws(const char *p) {
    while (ta_is_ascii_ws((unsigned char)*p)) p++;
    return p;
}

static bool ta_is_word_cp(unsigned long cp) {
    if (cp >= 0x80) return true; /* \w is unicode-aware; non-ASCII letters count */
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
           (cp >= '0' && cp <= '9') || cp == '_';
}

static bool ta_ci_prefix(const char *s, const char *lit) {
    while (*lit) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*lit)) return false;
        s++; lit++;
    }
    return true;
}

static const char *ta_stristr(const char *hay, const char *needle) {
    size_t n = strlen(needle);
    if (n == 0) return hay;
    for (const char *p = hay; *p; p++)
        if (ta_ci_prefix(p, needle)) return p;
    return NULL;
}

/* TABLE_SEPARATOR_RE.match(line):
 * ^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$ */
static bool ta_table_sep_match(const char *s) {
    const char *p = ta_skip_ws(s);
    if (*p == '|') p++;
    p = ta_skip_ws(p);
    if (*p == ':') p++;
    if (*p != '-') return false;
    while (*p == '-') p++;
    if (*p == ':') p++;
    p = ta_skip_ws(p);
    int groups = 0;
    while (*p == '|') {
        const char *q = ta_skip_ws(p + 1);
        if (*q == ':') q++;
        if (*q != '-') break; /* lone trailing pipe: leave it for \|? */
        while (*q == '-') q++;
        if (*q == ':') q++;
        p = ta_skip_ws(q);
        groups++;
    }
    if (groups < 1) return false;
    if (*p == '|') p++;
    p = ta_skip_ws(p);
    return *p == '\0';
}

/* str.splitlines() on the ASCII boundary corpus. */
static json_t *ta_splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    const char *start = s;
    const char *p = s;
    while (*p) {
        char c = *p;
        bool brk = (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
                    c == '\x1c' || c == '\x1d' || c == '\x1e');
        if (brk) {
            size_t n = (size_t)(p - start);
            char *line = malloc(n + 1);
            if (line) { memcpy(line, start, n); line[n] = '\0'; }
            json_append(arr, json_string(line ? line : ""));
            free(line);
            if (c == '\r' && p[1] == '\n') p++;
            p++;
            start = p;
        } else {
            p++;
        }
    }
    if (*start) json_append(arr, json_string(start));
    return arr;
}

/* (?m)^\s*[-*]\s+\[[ xX]\]\s+  (search; ^ anchors after \n only) */
static bool ta_task_list_search(const char *s) {
    if (!s) return false;
    for (const char *base = s; *base; base++) {
        if (base != s && base[-1] != '\n') continue;
        const char *p = ta_skip_ws(base);
        if (*p != '-' && *p != '*') continue;
        p++;
        const char *q = ta_skip_ws(p);
        if (q == p) continue; /* \s+ */
        p = q;
        if (*p != '[') continue;
        p++;
        if (*p != ' ' && *p != 'x' && *p != 'X') continue;
        p++;
        if (*p != ']') continue;
        p++;
        q = ta_skip_ws(p);
        if (q == p) continue; /* \s+ */
        return true;
    }
    return false;
}

/* (?m)^<details\b|^</details>|^<summary\b|^</summary>  (case-SENSITIVE) */
static bool ta_details_tags_search(const char *s) {
    if (!s) return false;
    for (const char *base = s; *base; base++) {
        if (base != s && base[-1] != '\n') continue;
        const char *p = base;
        if (strncmp(p, "<details", 8) == 0) {
            const char *after = p + 8;
            const char *q = after;
            unsigned long cp = ta_next_cp(&q);
            if (*after == '\0' || !ta_is_word_cp(cp)) return true;
        }
        if (strncmp(p, "</details>", 10) == 0) return true;
        if (strncmp(p, "<summary", 8) == 0) {
            const char *after = p + 8;
            const char *q = after;
            unsigned long cp = ta_next_cp(&q);
            if (*after == '\0' || !ta_is_word_cp(cp)) return true;
        }
        if (strncmp(p, "</summary>", 10) == 0) return true;
    }
    return false;
}

/* _RICH_CJK_RE ranges over UTF-8 codepoints. */
static bool ta_cjk_search(const char *s) {
    if (!s) return false;
    const char *p = s;
    while (*p) {
        unsigned long cp = ta_next_cp(&p);
        if ((cp >= 0x3040 && cp <= 0x30ff) || (cp >= 0x3400 && cp <= 0x4dbf) ||
            (cp >= 0x4e00 && cp <= 0x9fff) || (cp >= 0xac00 && cp <= 0xd7af) ||
            (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0x20000 && cp <= 0x323af))
            return true;
    }
    return false;
}

/* _RICH_DETAILS_RE.findall: <details\b[^>]*>.*?</details>  (IGNORECASE|DOTALL,
 * non-overlapping, lazy close). Appends block strings to arr. */
static void ta_details_blocks(const char *s, json_t *arr) {
    if (!s) return;
    size_t n = strlen(s), i = 0;
    while (i < n) {
        if (s[i] == '<' && ta_ci_prefix(s + i, "<details")) {
            const char *after = s + i + 8;
            const char *q = after;
            unsigned long cp = ta_next_cp(&q);
            bool boundary = (*after == '\0') || !ta_is_word_cp(cp);
            if (boundary) {
                const char *p = after;
                while (*p && *p != '>') {
                    const char *z = p;
                    ta_next_cp(&z);
                    p = z;
                }
                if (*p == '>') {
                    const char *close = ta_stristr(p + 1, "</details>");
                    if (close) {
                        size_t end = (size_t)(close + 10 - s);
                        char *block = malloc(end - i + 1);
                        if (block) {
                            memcpy(block, s + i, end - i);
                            block[end - i] = '\0';
                        }
                        json_append(arr, json_string(block ? block : ""));
                        free(block);
                        i = end;
                        continue;
                    }
                }
            }
        }
        const char *z = s + i;
        ta_next_cp(&z);
        i = (size_t)(z - s);
    }
}

/* _RICH_MATH_IN_DETAILS_RE.search: $$..$$ | \[..\] | \(..\) | \<command>
 * (IGNORECASE|DOTALL; commands match as prefixes, no word boundary). */
static bool ta_math_search(const char *s) {
    if (!s) return false;
    static const char *cmds[] = {
        "sum", "frac", "alpha", "beta", "gamma", "delta", "theta", "lambda",
        "mu", "pi", "sigma", "int", "prod", "sqrt", "lim", "infty",
    };
    static const char *envs[] = { "equation", "align", "matrix", "cases" };
    const char *p = s;
    while (*p) {
        if (p[0] == '$' && p[1] == '$') {
            if (strstr(p + 2, "$$")) return true;
        } else if (p[0] == '\\' && p[1] == '[') {
            if (strstr(p + 2, "\\]")) return true;
        } else if (p[0] == '\\' && p[1] == '(') {
            if (strstr(p + 2, "\\)")) return true;
        } else if (p[0] == '\\') {
            for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++)
                if (ta_ci_prefix(p + 1, cmds[i])) return true;
            if (ta_ci_prefix(p + 1, "begin") && p[6] == '{') {
                for (size_t i = 0; i < sizeof(envs) / sizeof(envs[0]); i++) {
                    size_t el = strlen(envs[i]);
                    if (ta_ci_prefix(p + 7, envs[i]) && p[7 + el] == '}') return true;
                }
            }
        }
        const char *z = p;
        ta_next_cp(&z);
        p = z;
    }
    return false;
}
/* ── metadata / DM-topic routing chain ── */

static const char *ta_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* (metadata or {}).get(key) — a truthy non-dict metadata raises
 * AttributeError exactly as live. *err owns the raise spec on failure. */
static const json_t *ta_meta_get(const json_t *metadata, const char *key, json_t **err) {
    *err = NULL;
    if (!ta_truthy(metadata)) return NULL;
    if (metadata->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ta_type_name(metadata));
        *err = ta_err("AttributeError", msg);
        return NULL;
    }
    return json_obj_get(metadata, key);
}

/* PoP: ta_metadata_thread_id @ plugins/platforms/telegram/adapter.py:_metadata_thread_id */
static json_t *ta_metadata_thread_id(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *err = NULL;
    const json_t *v1 = ta_meta_get(metadata, "thread_id", &err);
    if (err) return err;
    const json_t *v2 = ta_meta_get(metadata, "message_thread_id", &err);
    if (err) return err;
    const json_t *tid = ta_truthy(v1) ? v1 : v2;
    if (ta_is_null(tid)) return json_null();
    char *s = ta_str_of(tid);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: ta_metadata_direct_messages_topic_id @ plugins/platforms/telegram/adapter.py:_metadata_direct_messages_topic_id */
static json_t *ta_metadata_direct_messages_topic_id(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *err = NULL;
    const json_t *v1 = ta_meta_get(metadata, "direct_messages_topic_id", &err);
    if (err) return err;
    const json_t *v2 = ta_meta_get(metadata, "telegram_direct_messages_topic_id", &err);
    if (err) return err;
    const json_t *tid = ta_truthy(v1) ? v1 : v2;
    if (ta_is_null(tid)) return json_null();
    char *s = ta_str_of(tid);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: ta_metadata_reply_to_message_id @ plugins/platforms/telegram/adapter.py:_metadata_reply_to_message_id */
static json_t *ta_metadata_reply_to_message_id(ta_ctx_t *ctx, const json_t *args) {
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *err = NULL;
    const json_t *raw = ta_meta_get(metadata, "telegram_reply_to_message_id", &err);
    if (err) return err;
    return ta_parse_message_id(ctx, raw);
}

/* PoP: ta_dm_topic_fallback @ plugins/platforms/telegram/adapter.py:_dm_topic_fallback */
static json_t *ta_dm_topic_fallback(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *err = NULL;
    const json_t *flag = ta_meta_get(metadata, "telegram_dm_topic_reply_fallback", &err);
    if (err) return err;
    return json_bool(ta_truthy(metadata) && ta_truthy(flag));
}

/* PoP: ta_is_private_dm_topic_send @ plugins/platforms/telegram/adapter.py:_is_private_dm_topic_send */
static json_t *ta_is_private_dm_topic_send(ta_ctx_t *ctx, const json_t *args) {
    const json_t *thread_id = ta_arg(args, "thread_id");
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *dt = ta_metadata_direct_messages_topic_id(ctx, ta_args1("metadata", metadata));
    if (ta_is_raise(dt)) return dt;
    if (!ta_is_null(dt)) {
        json_free(dt);
        json_t *fb = ta_dm_topic_fallback(ctx, ta_args1("metadata", metadata));
        if (ta_is_raise(fb)) return fb;
        bool fallback = ta_truthy(fb);
        json_free(fb);
        if (!fallback) return json_bool(false);
        json_t *rt = ta_metadata_reply_to_message_id(ctx, ta_args1("metadata", metadata));
        if (ta_is_raise(rt)) return rt;
        bool has_anchor = !ta_is_null(rt);
        json_free(rt);
        return json_bool(has_anchor);
    }
    json_free(dt);
    json_t *err = NULL;
    const json_t *created = ta_meta_get(metadata, "telegram_dm_topic_created_for_send", &err);
    if (err) return err;
    if (ta_truthy(created)) return json_bool(false);
    if (!ta_truthy(thread_id)) return json_bool(false);
    return ta_dm_topic_fallback(ctx, ta_args1("metadata", metadata));
}

/* PoP: ta_reply_to_message_id_for_send @ plugins/platforms/telegram/adapter.py:_reply_to_message_id_for_send */
static json_t *ta_reply_to_message_id_for_send(ta_ctx_t *ctx, const json_t *args) {
    const json_t *metadata = ta_arg(args, "metadata");
    const json_t *mode = ta_arg(args, "reply_to_mode");
    json_t *anchor = ta_parse_message_id(ctx, ta_arg(args, "reply_to"));
    if (ta_is_raise(anchor)) return anchor;
    if (!ta_is_null(anchor)) return anchor;
    json_free(anchor);
    json_t *fb = ta_dm_topic_fallback(ctx, ta_args1("metadata", metadata));
    if (ta_is_raise(fb)) return fb;
    bool fallback = ta_truthy(fb);
    json_free(fb);
    if (fallback && !ta_str_eq(mode, "off"))
        return ta_metadata_reply_to_message_id(ctx, ta_args1("metadata", metadata));
    return json_null();
}

/* PoP: ta_message_thread_id_for_send @ plugins/platforms/telegram/adapter.py:_message_thread_id_for_send */
static json_t *ta_message_thread_id_for_send(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *thread_id = ta_arg(args, "thread_id");
    if (!ta_truthy(thread_id)) return json_null();
    char *s = ta_str_of(thread_id);
    bool general = s && strcmp(s, TA_GENERAL_TOPIC_THREAD_ID) == 0;
    free(s);
    if (general) return json_null();
    return ta_pyint(thread_id);
}

/* PoP: ta_message_thread_id_for_typing @ plugins/platforms/telegram/adapter.py:_message_thread_id_for_typing */
static json_t *ta_message_thread_id_for_typing(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *thread_id = ta_arg(args, "thread_id");
    if (!ta_truthy(thread_id)) return json_null();
    return ta_pyint(thread_id);
}

/* PoP: ta_direct_topic_kwargs @ plugins/platforms/telegram/adapter.py:_direct_topic_kwargs */
static json_t *ta_direct_topic_kwargs(ta_ctx_t *ctx, const json_t *args) {
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *dt = ta_metadata_direct_messages_topic_id(ctx, ta_args1("metadata", metadata));
    if (ta_is_raise(dt)) return dt;
    if (ta_is_null(dt)) {
        json_free(dt);
        return json_null();
    }
    json_t *n = ta_pyint(dt);
    json_free(dt);
    if (ta_is_raise(n)) return n;
    json_t *o = json_object();
    json_set(o, "message_thread_id", json_null());
    json_set(o, "direct_messages_topic_id", n);
    return o;
}

/* PoP: ta_thread_kwargs_for_send @ plugins/platforms/telegram/adapter.py:_thread_kwargs_for_send */
static json_t *ta_thread_kwargs_for_send(ta_ctx_t *ctx, const json_t *args) {
    const json_t *thread_id = ta_arg(args, "thread_id");
    const json_t *metadata = ta_arg(args, "metadata");
    const json_t *rtm = ta_arg(args, "reply_to_message_id");
    const json_t *mode = ta_arg(args, "reply_to_mode");
    json_t *owned_rtm = NULL;
    json_t *fb = ta_dm_topic_fallback(ctx, ta_args1("metadata", metadata));
    if (ta_is_raise(fb)) return fb;
    bool fallback = ta_truthy(fb);
    json_free(fb);
    if (fallback && !ta_str_eq(mode, "off")) {
        if (ta_is_null(rtm)) {
            owned_rtm = ta_metadata_reply_to_message_id(ctx, ta_args1("metadata", metadata));
            if (ta_is_raise(owned_rtm)) return owned_rtm;
            rtm = owned_rtm;
        }
        if (ta_is_null(rtm)) {
            json_t *tm = ta_message_thread_id_for_send(ctx, ta_args1("thread_id", thread_id));
            if (ta_is_raise(tm)) {
                json_free(owned_rtm);
                return tm;
            }
            if (!ta_is_null(tm)) {
                json_t *o = json_object();
                json_set(o, "message_thread_id", tm);
                json_free(owned_rtm);
                return o;
            }
            json_free(tm);
            json_t *dk = ta_direct_topic_kwargs(ctx, ta_args1("metadata", metadata));
            if (ta_is_raise(dk)) {
                json_free(owned_rtm);
                return dk;
            }
            json_free(owned_rtm);
            if (ta_is_null(dk)) {
                json_free(dk);
                return json_object();
            }
            return dk;
        }
    } else if (!fallback) {
        json_t *dk = ta_direct_topic_kwargs(ctx, ta_args1("metadata", metadata));
        if (ta_is_raise(dk)) return dk;
        if (!ta_is_null(dk)) return dk;
        json_free(dk);
    }
    json_t *tm = ta_message_thread_id_for_send(ctx, ta_args1("thread_id", thread_id));
    if (ta_is_raise(tm)) {
        json_free(owned_rtm);
        return tm;
    }
    json_t *o = json_object();
    json_set(o, "message_thread_id", tm);
    json_free(owned_rtm);
    return o;
}

/* PoP: ta_thread_kwargs_for_draft @ plugins/platforms/telegram/adapter.py:_thread_kwargs_for_draft */
static json_t *ta_thread_kwargs_for_draft(ta_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = ta_arg(args, "chat_id");
    const json_t *metadata = ta_arg(args, "metadata");
    /* live kwarg evaluation order: thread id, then reply anchor, then mode */
    json_t *tid = ta_metadata_thread_id(ctx, ta_args1("metadata", metadata));
    if (ta_is_raise(tid)) return tid;
    json_t *ra = json_object();
    json_set(ra, "reply_to", json_null());
    json_set(ra, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(ra, "reply_to_mode", json_null());
    json_t *rtm = ta_reply_to_message_id_for_send(ctx, ra);
    json_free(ra);
    if (ta_is_raise(rtm)) {
        json_free(tid);
        return rtm;
    }
    const json_t *mode = ta_state(ctx, "_reply_to_mode");
    json_t *ka = json_object();
    json_set(ka, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(ka, "thread_id", tid);
    json_set(ka, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(ka, "reply_to_message_id", rtm);
    json_set(ka, "reply_to_mode", mode ? json_copy(mode) : json_null());
    json_t *kw = ta_thread_kwargs_for_send(ctx, ka);
    json_free(ka);
    if (ta_is_raise(kw)) return kw;
    json_t *o = json_object();
    if (kw->type == JSON_OBJECT) {
        for (size_t i = 0; i < kw->c.count; i++) {
            json_t *v = kw->c.items[i];
            if (!ta_is_null(v)) json_set(o, kw->c.keys[i], json_copy(v));
        }
    }
    json_free(kw);
    return o;
}

/* PoP: ta_should_thread_reply @ plugins/platforms/telegram/adapter.py:_should_thread_reply */
static json_t *ta_should_thread_reply(ta_ctx_t *ctx, const json_t *args) {
    const json_t *reply_to = ta_arg(args, "reply_to");
    if (!ta_truthy(reply_to)) return json_bool(false);
    const json_t *mode = ta_state(ctx, "_reply_to_mode");
    if (!mode)
        return ta_err("AttributeError",
                      "'TelegramAdapter' object has no attribute '_reply_to_mode'");
    if (ta_str_eq(mode, "off")) return json_bool(false);
    if (ta_str_eq(mode, "all")) return json_bool(true);
    const json_t *idx = ta_arg(args, "chunk_index");
    double iv = 0;
    if (idx && idx->type == JSON_NUMBER) iv = idx->num_val;
    else if (idx && idx->type == JSON_BOOL) iv = idx->bool_val ? 1 : 0;
    return json_bool(iv == 0);
}

/* PoP: ta_chunk_reply_routing @ plugins/platforms/telegram/adapter.py:_chunk_reply_routing */
static json_t *ta_chunk_reply_routing(ta_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = ta_arg(args, "chat_id");
    const json_t *metadata = ta_arg(args, "metadata");
    const json_t *thread_id = ta_arg(args, "thread_id");
    const json_t *index = ta_arg(args, "index");
    json_t *mrt = ta_metadata_reply_to_message_id(ctx, ta_args1("metadata", metadata));
    if (ta_is_raise(mrt)) return mrt;
    json_t *pa = json_object();
    json_set(pa, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(pa, "thread_id", thread_id ? json_copy(thread_id) : json_null());
    json_set(pa, "metadata", metadata ? json_copy(metadata) : json_null());
    json_t *priv = ta_is_private_dm_topic_send(ctx, pa);
    json_free(pa);
    if (ta_is_raise(priv)) {
        json_free(mrt);
        return priv;
    }
    bool private_dm = ta_truthy(priv);
    json_free(priv);
    const json_t *mode = NULL;
    bool off = false;
    if (private_dm) {
        mode = ta_state(ctx, "_reply_to_mode");
        if (!mode) {
            json_free(mrt);
            return ta_err("AttributeError",
                          "'TelegramAdapter' object has no attribute '_reply_to_mode'");
        }
        if (ta_str_eq(mode, "off")) {
            json_t *fb = ta_dm_topic_fallback(ctx, ta_args1("metadata", metadata));
            if (ta_is_raise(fb)) {
                json_free(mrt);
                return fb;
            }
            off = ta_truthy(fb);
            json_free(fb);
        }
    }
    json_t *parsed = ta_parse_message_id(ctx, ta_arg(args, "reply_to"));
    if (ta_is_raise(parsed)) {
        json_free(mrt);
        return parsed;
    }
    const json_t *src_node = NULL;
    if (!ta_is_null(parsed)) src_node = parsed;
    else if (private_dm && !ta_is_null(mrt)) src_node = mrt;
    char *source = src_node ? ta_str_of(src_node) : NULL;
    bool should_thread = false;
    json_t *st_err = NULL;
    if (private_dm) {
        should_thread = source != NULL && !ta_str_eq(mode, "off");
    } else {
        json_t *sa = json_object();
        json_set(sa, "reply_to", source ? json_string(source) : json_null());
        json_set(sa, "chunk_index", index ? json_copy(index) : json_null());
        json_t *st = ta_should_thread_reply(ctx, sa);
        json_free(sa);
        if (ta_is_raise(st)) st_err = st;
        else {
            should_thread = ta_truthy(st);
            json_free(st);
        }
    }
    if (st_err) {
        free(source);
        json_free(parsed);
        json_free(mrt);
        return st_err;
    }
    json_t *reply_to_id = json_null();
    if (should_thread && source && source[0]) {
        json_t *sn = json_string(source);
        reply_to_id = ta_pyint(sn);
        json_free(sn);
        if (ta_is_raise(reply_to_id)) {
            free(source);
            json_free(parsed);
            json_free(mrt);
            return reply_to_id;
        }
    }
    free(source);
    json_free(parsed);
    json_free(mrt);
    json_t *o = json_array();
    json_append(o, json_bool(private_dm));
    json_append(o, json_bool(off));
    json_append(o, reply_to_id);
    return o;
}

/* PoP: ta_compute_single_send_routing @ plugins/platforms/telegram/adapter.py:_compute_single_send_routing */
static json_t *ta_compute_single_send_routing(ta_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = ta_arg(args, "chat_id");
    const json_t *metadata = ta_arg(args, "metadata");
    const json_t *thread_id = ta_arg(args, "thread_id");
    json_t *ca = json_object();
    json_set(ca, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(ca, "reply_to", ta_arg(args, "reply_to") ? json_copy(ta_arg(args, "reply_to")) : json_null());
    json_set(ca, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(ca, "thread_id", thread_id ? json_copy(thread_id) : json_null());
    json_set(ca, "index", json_number(0));
    json_t *routing = ta_chunk_reply_routing(ctx, ca);
    json_free(ca);
    if (ta_is_raise(routing)) return routing;
    bool private_dm = ta_truthy(json_get(routing, 0));
    bool off = ta_truthy(json_get(routing, 1));
    json_t *rtid = json_copy(json_get(routing, 2));
    json_free(routing);
    const json_t *mode = ta_state(ctx, "_reply_to_mode");
    if (!mode) {
        json_free(rtid);
        return ta_err("AttributeError",
                      "'TelegramAdapter' object has no attribute '_reply_to_mode'");
    }
    json_t *ka = json_object();
    json_set(ka, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(ka, "thread_id", thread_id ? json_copy(thread_id) : json_null());
    json_set(ka, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(ka, "reply_to_message_id", rtid);
    json_set(ka, "reply_to_mode", json_copy(mode));
    json_t *kw = ta_thread_kwargs_for_send(ctx, ka);
    json_free(ka);
    if (ta_is_raise(kw)) return kw;
    if (private_dm && ta_is_null(rtid) && !off &&
        !ta_truthy(json_obj_get(kw, "direct_messages_topic_id"))) {
        json_free(rtid);
        json_free(kw);
        return json_null();
    }
    json_t *o = json_array();
    json_append(o, rtid);
    json_append(o, kw);
    return o;
}
/* ── error-classifier chain ── */

/* DFS yield order of _iter_exception_graph as an id list (cycle-safe;
 * stack.extend((cause, context)) -> context pops first). Registry ids are
 * fixture-bounded (<= 256 nodes). */
static json_t *ta_graph_ids(ta_ctx_t *ctx, const char *eid) {
    if (!eid) return ta_err("KeyError", "'error'");
    if (!ta_err_obj(ctx, eid)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s'", eid);
        return ta_err("KeyError", msg);
    }
    json_t *out = json_array();
    const char *stack[256];
    const char *seen[256];
    size_t sp = 0, ns = 0;
    stack[sp++] = eid;
    while (sp) {
        const char *cur = stack[--sp];
        bool dup = false;
        for (size_t i = 0; i < ns; i++)
            if (strcmp(seen[i], cur) == 0) { dup = true; break; }
        if (dup) continue;
        if (ns < 256) seen[ns++] = cur;
        json_append(out, json_string(cur));
        const json_t *e = ta_err_obj(ctx, cur);
        const char *cause = e ? json_get_str(e, "cause", NULL) : NULL;
        const char *context = e ? json_get_str(e, "context", NULL) : NULL;
        if (cause && sp < 256) stack[sp++] = cause;
        if (context && sp < 256) stack[sp++] = context;
    }
    return out;
}

static const char *ta_err_arg(const json_t *args) {
    return ta_str(ta_arg(args, "error"));
}

/* PoP: ta_iter_exception_graph @ plugins/platforms/telegram/adapter.py:_iter_exception_graph */
static json_t *ta_iter_exception_graph(ta_ctx_t *ctx, const json_t *args) {
    return ta_graph_ids(ctx, ta_err_arg(args));
}

/* PoP: ta_exception_graph_matches @ plugins/platforms/telegram/adapter.py:_exception_graph_matches */
static json_t *ta_exception_graph_matches(ta_ctx_t *ctx, const json_t *args) {
    json_t *ids = ta_graph_ids(ctx, ta_err_arg(args));
    if (ta_is_raise(ids)) return ids;
    const char *nm = ta_str(ta_arg(args, "name_marker"));
    const json_t *tms = ta_arg(args, "text_markers");
    bool hit = false;
    for (size_t i = 0; i < json_len(ids) && !hit; i++) {
        const char *id = ta_str(json_get(ids, i));
        char *cls = ta_lower(ta_err_class(ctx, id));
        if (nm && cls && strstr(cls, nm)) hit = true;
        free(cls);
        if (!hit) {
            char *text = ta_lower(ta_err_text(ctx, id));
            if (tms && tms->type == JSON_ARRAY && text) {
                for (size_t j = 0; j < json_len(tms); j++) {
                    const char *m = ta_str(json_get(tms, j));
                    if (m && strstr(text, m)) { hit = true; break; }
                }
            }
            free(text);
        }
    }
    json_free(ids);
    return json_bool(hit);
}

/* PoP: ta_looks_like_connect_timeout @ plugins/platforms/telegram/adapter.py:_looks_like_connect_timeout */
static json_t *ta_looks_like_connect_timeout(ta_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *err = ta_arg(args, "error");
    json_set(a, "error", err ? json_copy(err) : json_null());
    json_set_hdr(a, "name_marker", "connecttimeout");
    json_t *tms = json_array();
    json_append(tms, json_string("connect timeout"));
    json_append(tms, json_string("connect timed out"));
    json_set(a, "text_markers", tms);
    json_t *r = ta_exception_graph_matches(ctx, a);
    json_free(a);
    return r;
}

/* PoP: ta_looks_like_pool_timeout @ plugins/platforms/telegram/adapter.py:_looks_like_pool_timeout */
static json_t *ta_looks_like_pool_timeout(ta_ctx_t *ctx, const json_t *args) {
    json_t *ids = ta_graph_ids(ctx, ta_err_arg(args));
    if (ta_is_raise(ids)) return ids;
    bool hit = false;
    for (size_t i = 0; i < json_len(ids) && !hit; i++) {
        const char *id = ta_str(json_get(ids, i));
        char *name = ta_lower(ta_err_class(ctx, id));
        char *text = ta_lower(ta_err_text(ctx, id));
        if ((name && strstr(name, "pooltimeout")) ||
            (text && strstr(text, "pool timeout")) ||
            (text && strstr(text, "connection pool") && strstr(text, "occupied")))
            hit = true;
        free(name);
        free(text);
    }
    json_free(ids);
    return json_bool(hit);
}

/* PoP: ta_is_thread_not_found_error @ plugins/platforms/telegram/adapter.py:_is_thread_not_found_error */
static json_t *ta_is_thread_not_found_error(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    char *text = ta_lower(ta_err_text(ctx, eid));
    bool hit = text && strstr(text, "thread not found");
    free(text);
    return json_bool(hit);
}

/* PoP: ta_is_bad_request_error @ plugins/platforms/telegram/adapter.py:_is_bad_request_error */
static json_t *ta_is_bad_request_error(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    char *name = ta_lower(ta_err_class(ctx, eid));
    size_t n = name ? strlen(name) : 0;
    bool hit = name && (strcmp(name, "badrequest") == 0 ||
                        (n >= 10 && strcmp(name + n - 10, "badrequest") == 0));
    free(name);
    if (hit) return json_bool(true);
    if (!ta_ptb_available(ctx)) return json_bool(false); /* except ImportError */
    json_t *r = ta_ptb_isinstance(ctx, eid, "BadRequest");
    if (ta_is_raise(r)) {
        if (strcmp(ta_raise_kind(r), "ImportError") == 0) {
            json_free(r);
            return json_bool(false);
        }
        return r; /* every other kind (BaseException included) propagates */
    }
    bool is = ta_truthy(r);
    json_free(r);
    return json_bool(is);
}

/* PoP: ta_is_reply_anchor_missing_error @ plugins/platforms/telegram/adapter.py:_is_reply_anchor_missing_error */
static json_t *ta_is_reply_anchor_missing_error(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    char *text = ta_lower(ta_err_text(ctx, eid));
    bool hit = text && (strstr(text, "message to be replied not found") ||
                        strstr(text, "reply message not found"));
    free(text);
    return json_bool(hit);
}

/* PoP: ta_should_retry_without_dm_topic_reply_anchor @ plugins/platforms/telegram/adapter.py:_should_retry_without_dm_topic_reply_anchor */
static json_t *ta_should_retry_without_dm_topic_reply_anchor(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    const json_t *metadata = ta_arg(args, "metadata");
    const json_t *rtm = ta_arg(args, "reply_to_message_id");
    json_t *fb = ta_dm_topic_fallback(ctx, ta_args1("metadata", metadata));
    if (ta_is_raise(fb)) return fb;
    bool fallback = ta_truthy(fb);
    json_free(fb);
    if (!fallback) return json_bool(false);
    json_t *br = ta_is_bad_request_error(ctx, ta_args1("error", ta_arg(args, "error")));
    if (ta_is_raise(br)) return br;
    bool bad = ta_truthy(br);
    json_free(br);
    if (!bad || ta_is_null(rtm)) return json_bool(false);
    return ta_is_reply_anchor_missing_error(ctx, ta_args1("error", ta_arg(args, "error")));
}

/* PoP: ta_looks_like_auth_error @ plugins/platforms/telegram/adapter.py:_looks_like_auth_error */
static json_t *ta_looks_like_auth_error(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    char *name = ta_lower(ta_err_class(ctx, eid));
    bool hit = name && (strcmp(name, "invalidtoken") == 0 || strcmp(name, "forbidden") == 0);
    free(name);
    if (hit) return json_bool(true);
    if (!ta_ptb_available(ctx)) return json_bool(false);
    static const char *classes[] = { "InvalidToken", "Forbidden" };
    for (size_t i = 0; i < 2; i++) {
        json_t *r = ta_ptb_isinstance(ctx, eid, classes[i]);
        if (ta_is_raise(r)) {
            if (strcmp(ta_raise_kind(r), "ImportError") == 0) {
                json_free(r);
                return json_bool(false);
            }
            return r;
        }
        bool is = ta_truthy(r);
        json_free(r);
        if (is) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: ta_looks_like_network_error @ plugins/platforms/telegram/adapter.py:_looks_like_network_error */
static json_t *ta_looks_like_network_error(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    char *name = ta_lower(ta_err_class(ctx, eid));
    bool deny = name && (strcmp(name, "badrequest") == 0 || strcmp(name, "invalidtoken") == 0 ||
                         strcmp(name, "forbidden") == 0 || strcmp(name, "retryafter") == 0);
    bool allow = name && (strcmp(name, "networkerror") == 0 || strcmp(name, "timedout") == 0 ||
                          strcmp(name, "connectionerror") == 0);
    free(name);
    if (deny) return json_bool(false);
    if (allow) return json_bool(true);
    if (ta_ptb_available(ctx)) {
        static const char *deny_cls[] = { "BadRequest", "InvalidToken", "Forbidden", "RetryAfter" };
        static const char *allow_cls[] = { "NetworkError", "TimedOut" };
        for (size_t i = 0; i < 4; i++) {
            json_t *r = ta_ptb_isinstance(ctx, eid, deny_cls[i]);
            if (ta_is_raise(r)) {
                if (strcmp(ta_raise_kind(r), "ImportError") == 0) {
                    json_free(r);
                    goto oserror; /* except ImportError: pass */
                }
                return r;
            }
            bool is = ta_truthy(r);
            json_free(r);
            if (is) return json_bool(false);
        }
        for (size_t i = 0; i < 2; i++) {
            json_t *r = ta_ptb_isinstance(ctx, eid, allow_cls[i]);
            if (ta_is_raise(r)) {
                if (strcmp(ta_raise_kind(r), "ImportError") == 0) {
                    json_free(r);
                    goto oserror;
                }
                return r;
            }
            bool is = ta_truthy(r);
            json_free(r);
            if (is) return json_bool(true);
        }
    }
oserror:
    return json_bool(ta_err_isinstance(ctx, eid, "OSError"));
}

/* PoP: ta_is_timed_out @ plugins/platforms/telegram/adapter.py:_is_timed_out */
static json_t *ta_is_timed_out(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    /* the try wraps ONLY the import; the isinstance below is OUTSIDE it,
     * so a seam raise of ANY kind propagates (unlike the sibling probes). */
    bool have = ta_ptb_available(ctx) && !ta_timedout_missing(ctx);
    if (have) {
        json_t *r = ta_ptb_isinstance(ctx, eid, "TimedOut");
        if (ta_is_raise(r)) return r;
        bool is = ta_truthy(r);
        json_free(r);
        if (is) return json_bool(true);
    }
    char *text = ta_lower(ta_err_text(ctx, eid));
    bool hit = text && strstr(text, "timed out");
    free(text);
    return json_bool(hit);
}

/* PoP: ta_is_rich_capability_error @ plugins/platforms/telegram/adapter.py:_is_rich_capability_error */
static json_t *ta_is_rich_capability_error(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    char *name = ta_lower(ta_err_class(ctx, eid));
    bool hit = name && (strcmp(name, "endpointnotfound") == 0 || strcmp(name, "invalidtoken") == 0);
    free(name);
    if (hit) return json_bool(true);
    if (ta_err_isinstance(ctx, eid, "AttributeError") ||
        ta_err_isinstance(ctx, eid, "TypeError") ||
        ta_err_isinstance(ctx, eid, "NotImplementedError"))
        return json_bool(true);
    const json_t *e = ta_err_obj(ctx, eid);
    const json_t *ec = e ? json_obj_get(e, "error_code") : NULL;
    if (ec && ec->type == JSON_NUMBER && ec->num_val == 404.0) return json_bool(true);
    char *s = ta_lower(ta_err_text(ctx, eid));
    bool m = s && (strstr(s, "method") || strstr(s, "endpoint"));
    bool nf = s && (strstr(s, "not found") || strstr(s, "does not exist"));
    hit = (m && nf) || (s && strstr(s, "no such method"));
    free(s);
    return json_bool(hit);
}

/* PoP: ta_is_rich_fallback_error @ plugins/platforms/telegram/adapter.py:_is_rich_fallback_error */
static json_t *ta_is_rich_fallback_error(ta_ctx_t *ctx, const json_t *args) {
    const char *eid = ta_err_arg(args);
    if (!eid) return ta_err("KeyError", "'error'");
    const json_t *err = ta_arg(args, "error");
    json_t *br = ta_is_bad_request_error(ctx, ta_args1("error", err));
    if (ta_is_raise(br)) return br;
    bool bad = ta_truthy(br);
    json_free(br);
    if (bad) return json_bool(true);
    json_t *cap = ta_is_rich_capability_error(ctx, ta_args1("error", err));
    if (ta_is_raise(cap)) return cap;
    bool capability = ta_truthy(cap);
    json_free(cap);
    if (capability) return json_bool(true);
    char *s = ta_lower(ta_err_text(ctx, ta_err_arg(args)));
    bool hit = s && (strstr(s, "unsupported") || strstr(s, "not implemented"));
    free(s);
    return json_bool(hit);
}

/* ── rich-message eligibility chain ── */

/* PoP: ta_rich_delivery_enabled @ plugins/platforms/telegram/adapter.py:_rich_delivery_enabled */
static json_t *ta_rich_delivery_enabled(ta_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *v = ta_state(ctx, "_rich_messages_enabled");
    return json_bool(v ? ta_truthy(v) : true);
}

/* PoP: ta_bot_supports_rich @ plugins/platforms/telegram/adapter.py:_bot_supports_rich */
static json_t *ta_bot_supports_rich(ta_ctx_t *ctx, const json_t *args) {
    (void)args;
    /* live reads self._bot DIRECTLY (no getattr default): absent -> raise */
    if (!json_has(ctx->state, "_bot"))
        return ta_err("AttributeError", "'TelegramAdapter' object has no attribute '_bot'");
    const json_t *bot = ta_state(ctx, "_bot");
    const json_t *fn = (bot && bot->type == JSON_OBJECT) ? json_obj_get(bot, "do_api_request") : NULL;
    json_t *a = json_object();
    json_set(a, "value", fn ? json_copy(fn) : json_null());
    return ta_call(ctx, "iscoroutinefunction", a);
}

/* PoP: ta_needs_rich_rendering @ plugins/platforms/telegram/adapter.py:_needs_rich_rendering */
static json_t *ta_needs_rich_rendering(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *content = ta_str(ta_arg(args, "content"));
    if (!content || !content[0]) return json_bool(false);
    json_t *lines = ta_splitlines(content);
    bool hit = false;
    for (size_t i = 0; i < json_len(lines) && !hit; i++)
        if (ta_table_sep_match(ta_str(json_get(lines, i)))) hit = true;
    json_free(lines);
    if (hit) return json_bool(true);
    if (ta_task_list_search(content)) return json_bool(true);
    if (ta_details_tags_search(content)) return json_bool(true);
    return json_bool(strstr(content, "$$") != NULL);
}

/* PoP: ta_has_telegram_desktop_cjk_rich_garble_shape @ plugins/platforms/telegram/adapter.py:_has_telegram_desktop_cjk_rich_garble_shape */
static json_t *ta_has_telegram_desktop_cjk_rich_garble_shape(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *content = ta_arg(args, "content");
    if (!ta_truthy(content)) return json_bool(false);
    return json_bool(ta_cjk_search(ta_str(content)));
}

/* PoP: ta_has_telegram_desktop_details_math_crash_shape @ plugins/platforms/telegram/adapter.py:_has_telegram_desktop_details_math_crash_shape */
static json_t *ta_has_telegram_desktop_details_math_crash_shape(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *content = ta_arg(args, "content");
    if (!ta_truthy(content)) return json_bool(false);
    json_t *blocks = json_array();
    ta_details_blocks(ta_str(content), blocks);
    bool hit = false;
    for (size_t i = 0; i < json_len(blocks) && !hit; i++)
        if (ta_math_search(ta_str(json_get(blocks, i)))) hit = true;
    json_free(blocks);
    return json_bool(hit);
}

/* PoP: ta_content_fits_rich_limits @ plugins/platforms/telegram/adapter.py:_content_fits_rich_limits */
static json_t *ta_content_fits_rich_limits(ta_ctx_t *ctx, const json_t *args) {
    const json_t *content = ta_arg(args, "content");
    json_t *a = json_object();
    json_set(a, "content", content ? json_copy(content) : json_null());
    json_set(a, "skip_entity_detection", json_bool(false));
    json_t *payload = ta_call(ctx, "rich_message_payload", a);
    if (ta_is_raise(payload)) return payload;
    if (!payload || payload->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable", ta_type_name(payload));
        json_free(payload);
        return ta_err("TypeError", msg);
    }
    const json_t *md = json_obj_get(payload, "markdown");
    if (!md) {
        json_free(payload);
        return ta_err("KeyError", "'markdown'");
    }
    if (md->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "object of type '%s' has no len()", ta_type_name(md));
        json_free(payload);
        return ta_err("TypeError", msg);
    }
    bool fits = ta_utf8_len(md->str_val) <= TA_RICH_MESSAGE_MAX_CHARS;
    json_free(payload);
    return json_bool(fits);
}

/* PoP: ta_rich_content_ok @ plugins/platforms/telegram/adapter.py:_rich_content_ok */
static json_t *ta_rich_content_ok(ta_ctx_t *ctx, const json_t *args) {
    const json_t *content = ta_arg(args, "content");
    if (!ta_truthy(content)) return json_bool(false);
    if (!ta_has_nonspace(ta_str(content))) return json_bool(false);
    json_t *dm = ta_has_telegram_desktop_details_math_crash_shape(ctx, ta_args1("content", content));
    if (ta_is_raise(dm)) return dm;
    bool crash = ta_truthy(dm);
    json_free(dm);
    if (crash) return json_bool(false);
    const json_t *allow = ta_state(ctx, "_allow_cjk_rich_messages");
    if (!ta_truthy(allow)) {
        json_t *cj = ta_has_telegram_desktop_cjk_rich_garble_shape(ctx, ta_args1("content", content));
        if (ta_is_raise(cj)) return cj;
        bool garble = ta_truthy(cj);
        json_free(cj);
        if (garble) return json_bool(false);
    }
    const json_t *csz = ta_arg(args, "check_size");
    bool check_size = csz ? ta_truthy(csz) : true;
    if (check_size) {
        json_t *fits = ta_content_fits_rich_limits(ctx, ta_args1("content", content));
        if (ta_is_raise(fits)) return fits;
        bool ok = ta_truthy(fits);
        json_free(fits);
        if (!ok) return json_bool(false);
    }
    json_t *bs = ta_bot_supports_rich(ctx, json_object());
    if (ta_is_raise(bs)) return bs;
    bool supported = ta_truthy(bs);
    json_free(bs);
    return json_bool(supported);
}

/* PoP: ta_rich_eligible @ plugins/platforms/telegram/adapter.py:_rich_eligible */
static json_t *ta_rich_eligible(ta_ctx_t *ctx, const json_t *args) {
    const json_t *content = ta_arg(args, "content");
    json_t *en = ta_rich_delivery_enabled(ctx, json_object());
    bool enabled = ta_truthy(en);
    json_free(en);
    if (!enabled) return json_bool(false);
    if (ta_truthy(ta_state(ctx, "_rich_send_disabled"))) return json_bool(false);
    if (!ta_truthy(content)) return json_bool(false);
    if (!ta_has_nonspace(ta_str(content))) return json_bool(false);
    json_t *nr = ta_needs_rich_rendering(ctx, ta_args1("content", content));
    if (ta_is_raise(nr)) return nr;
    bool needs = ta_truthy(nr);
    json_free(nr);
    if (!needs) return json_bool(false);
    json_t *ok = ta_rich_content_ok(ctx, ta_args1("content", content));
    if (ta_is_raise(ok)) return ok;
    bool r = ta_truthy(ok);
    json_free(ok);
    return json_bool(r);
}

/* PoP: ta_should_attempt_rich @ plugins/platforms/telegram/adapter.py:_should_attempt_rich */
static json_t *ta_should_attempt_rich(ta_ctx_t *ctx, const json_t *args) {
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *err = NULL;
    const json_t *ee = ta_meta_get(metadata, "expect_edits", &err);
    if (err) return err;
    if (ta_truthy(ee)) return json_bool(false);
    return ta_rich_eligible(ctx, ta_args1("content", ta_arg(args, "content")));
}

/* PoP: ta_prefers_fresh_final_streaming @ plugins/platforms/telegram/adapter.py:prefers_fresh_final_streaming */
static json_t *ta_prefers_fresh_final_streaming(ta_ctx_t *ctx, const json_t *args) {
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *err = NULL;
    const json_t *fb = ta_meta_get(metadata, "telegram_dm_topic_reply_fallback", &err);
    if (err) return err;
    if (!ta_truthy(fb)) {
        json_t *dt = ta_metadata_direct_messages_topic_id(ctx, ta_args1("metadata", metadata));
        if (ta_is_raise(dt)) return dt;
        bool has = !ta_is_null(dt);
        json_free(dt);
        if (!has) return json_bool(false);
    }
    return ta_rich_eligible(ctx, ta_args1("content", ta_arg(args, "content")));
}

/* PoP: ta_rich_transport_available @ plugins/platforms/telegram/adapter.py:_rich_transport_available */
static json_t *ta_rich_transport_available(ta_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *en = ta_state(ctx, "_rich_messages_enabled");
    if (!(en ? ta_truthy(en) : true)) return json_bool(false);
    if (ta_truthy(ta_state(ctx, "_rich_send_disabled"))) return json_bool(false);
    json_t *bs = ta_bot_supports_rich(ctx, json_object());
    if (ta_is_raise(bs)) return bs;
    bool r = ta_truthy(bs);
    json_free(bs);
    return json_bool(r);
}

/* PoP: ta_streaming_overflow_limit @ plugins/platforms/telegram/adapter.py:streaming_overflow_limit */
static json_t *ta_streaming_overflow_limit(ta_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *av = ta_rich_transport_available(ctx, json_object());
    if (ta_is_raise(av)) return av;
    bool available = ta_truthy(av);
    json_free(av);
    return available ? json_number(TA_RICH_MESSAGE_MAX_CHARS) : json_null();
}

/* ── chat-type / config / notification leaves ── */

/* PoP: ta_normalize_chat_type @ plugins/platforms/telegram/adapter.py:_normalize_chat_type */
static json_t *ta_normalize_chat_type(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ct = ta_arg(args, "chat_type");
    char *s = ta_truthy(ct) ? ta_str_of(ct) : ta_strdup("dm");
    char *st = ta_strip(s);
    free(s);
    char *lo = ta_lower(st);
    free(st);
    const char *norm = (lo && lo[0]) ? lo : "dm";
    json_t *r;
    if (strcmp(norm, "private") == 0) r = json_string("dm");
    else if (strcmp(norm, "supergroup") == 0)
        r = json_string(ta_truthy(ta_arg(args, "is_forum")) ? "forum" : "group");
    else r = json_string(norm);
    free(lo);
    return r;
}

/* PoP: ta_chat_type_str @ plugins/platforms/telegram/adapter.py:_chat_type_str */
static json_t *ta_chat_type_str(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *chat = ta_arg(args, "chat");
    if (!ta_truthy(chat)) return json_string("");
    const json_t *ty = (chat->type == JSON_OBJECT) ? json_obj_get(chat, "type") : NULL;
    char *s = ty ? ta_str_of(ty) : ta_strdup("");
    char *last = strrchr(s, '.');
    char *seg = ta_lower(last ? last + 1 : s);
    free(s);
    json_t *r = json_string(seg);
    free(seg);
    return r;
}

/* PoP: ta_gateway_chat_type @ plugins/platforms/telegram/adapter.py:_gateway_chat_type */
static json_t *ta_gateway_chat_type(ta_ctx_t *ctx, const json_t *args) {
    json_t *cs = ta_chat_type_str(ctx, ta_args1("chat", ta_arg(args, "chat")));
    if (ta_is_raise(cs)) return cs;
    json_t *na = json_object();
    json_set(na, "chat_type", cs);
    const json_t *forum = ta_arg(args, "is_forum");
    json_set(na, "is_forum", forum ? json_copy(forum) : json_null());
    json_t *ct = ta_normalize_chat_type(ctx, na);
    json_free(na);
    if (ta_is_raise(ct)) return ct;
    const char *s = ta_str(ct);
    bool keep = s && (strcmp(s, "group") == 0 || strcmp(s, "forum") == 0 || strcmp(s, "channel") == 0);
    if (keep) return ct;
    json_free(ct);
    return json_string("dm");
}

/* PoP: ta_coerce_bool_extra @ plugins/platforms/telegram/adapter.py:_coerce_bool_extra */
static json_t *ta_coerce_bool_extra(ta_ctx_t *ctx, const json_t *args) {
    const json_t *config = ta_state(ctx, "config");
    const json_t *extra = (config && config->type == JSON_OBJECT) ? json_obj_get(config, "extra") : NULL;
    const json_t *dflt = ta_arg(args, "default");
    const json_t *value = NULL;
    if (ta_truthy(extra)) {
        if (extra->type != JSON_OBJECT) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ta_type_name(extra));
            return ta_err("AttributeError", msg);
        }
        value = json_obj_get(extra, ta_str(ta_arg(args, "key")));
    }
    if (ta_is_null(value)) return dflt ? json_copy(dflt) : json_bool(false);
    if (value->type == JSON_STRING) {
        char *st = ta_strip(value->str_val);
        char *lo = ta_lower(st);
        free(st);
        json_t *r = NULL;
        if (lo && (strcmp(lo, "true") == 0 || strcmp(lo, "1") == 0 ||
                   strcmp(lo, "yes") == 0 || strcmp(lo, "on") == 0))
            r = json_bool(true);
        else if (lo && (strcmp(lo, "false") == 0 || strcmp(lo, "0") == 0 ||
                        strcmp(lo, "no") == 0 || strcmp(lo, "off") == 0))
            r = json_bool(false);
        else r = dflt ? json_copy(dflt) : json_bool(false);
        free(lo);
        return r;
    }
    return json_bool(ta_truthy(value));
}

/* PoP: ta_notification_kwargs @ plugins/platforms/telegram/adapter.py:_notification_kwargs */
static json_t *ta_notification_kwargs(ta_ctx_t *ctx, const json_t *args) {
    const json_t *mode = ta_state(ctx, "_notifications_mode");
    bool important = !mode || ta_str_eq(mode, "important");
    if (!important) return json_object();
    const json_t *metadata = ta_arg(args, "metadata");
    json_t *err = NULL;
    const json_t *notify = ta_meta_get(metadata, "notify", &err);
    if (err) return err;
    if (ta_truthy(notify)) return json_object();
    const json_t *appr = ta_meta_get(metadata, "is_approval_prompt", &err);
    if (err) return err;
    if (ta_truthy(appr)) return json_object();
    json_t *o = json_object();
    json_set(o, "disable_notification", json_bool(true));
    return o;
}

/* PoP: ta_send_retry_is_final @ plugins/platforms/telegram/adapter.py:_send_retry_is_final */
static json_t *ta_send_retry_is_final(ta_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *result = ta_arg(args, "result");
    if (ta_is_null(result))
        return ta_err("AttributeError", "'NoneType' object has no attribute 'raw_response'");
    if (result->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'raw_response'", ta_type_name(result));
        return ta_err("AttributeError", msg);
    }
    const json_t *raw = json_obj_get(result, "raw_response");
    if (!raw)
        return ta_err("AttributeError",
                      "'types.SimpleNamespace' object has no attribute 'raw_response'");
    bool is_dict = raw->type == JSON_OBJECT;
    bool uncertain = is_dict && ta_truthy(json_obj_get(raw, "delivery_uncertain"));
    return json_bool(is_dict && uncertain);
}