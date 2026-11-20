/*
 * session_search_flow3.c — FLOW port of the UNCLAIMED FTS5-search result-shaping
 * remainder of tools/session_search_tool.py (current quarry revision).
 *
 * A CRLF-aware PoP grep of src/ shows every Python name ported here is free:
 *   _content_text, _redact_derived_value, _redact_tool_calls, _discovery_entry,
 *   _derived_snippet, _bookend, _discover_payload, _hydrate_hit
 * (the FTS5 core _discover/_title_match_result/_normalize_title_query/_shape_message
 * and the flow2 gap leaves are already claimed elsewhere and are SEAMED here).
 *
 * Ported targets (8), all static and ss3_-prefixed:
 *   _content_text          -> ss3_content_text
 *   _redact_derived_value  -> ss3_redact_derived_value
 *   _redact_tool_calls     -> ss3_redact_tool_calls
 *   _discovery_entry       -> ss3_discovery_entry
 *   _derived_snippet       -> ss3_derived_snippet
 *   _bookend               -> ss3_bookend
 *   _discover_payload      -> ss3_discover_payload
 *   _hydrate_hit           -> ss3_hydrate_hit
 *
 * Every live collaborator call goes through ONE universal seam
 * ss3_call(ctx, name, args).  The harness wires it to a recording stub fed
 * in.io; the oracle installs recorders emitting the identical event shape.
 * The harness #includes this file directly (self-contained, libjson only).
 *
 * Seam table (name -> live call):
 *   redact_derived_text        redact_derived_text(value)            {value}
 *   format_timestamp           _format_timestamp(ts)                {ts}
 *   get_session_meta           _get_session_meta(persistence, sid)  {session_id}
 *   shape_message              _shape_message(m, **kw)              {message,anchor_id?,max_content_len?,derived}
 *   is_compaction_summary      _is_compaction_summary(m.content)    {content}
 *   read_anchored_view         messages.read_anchored_view(...)     {session_id,message_id,window,bookend}
 *   logger.warning             logging.warning(fmt,*args,exc_info)  {message,args,exc_info}
 *   logger.debug               logging.debug(fmt,*args,exc_info)    {message,args,exc_info}
 *   search_index_rebuild_status search.search_index_rebuild_status() {}
 *
 * Representation contract (shared with tests/oracle_session_search_flow3.py):
 * - NULL / JSON_NULL == Python None; a raise is
 *   {"__raise__": <ClassName>, "message": str(exc)}.
 * - None-valued arg keys (including present null) are dropped before a seam
 *   event is recorded; every seam event is {"name","args","ret"} in order.
 * - Scripted seams: plain value reused, bare array = per-call queue (last does
 *   NOT repeat), {"__script__":true,"ret"/"raise"}, or a bare
 *   {"__raise__","message"} dict. Missing/exhausted -> fail-closed RuntimeError.
 * - FULL py3.14 builtin exception hierarchy decides `except Exception`:
 *   SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/
 *   BaseExceptionGroup walk to BaseException and pass through; every other
 *   builtin (and unknown synthetic kinds) reaches Exception and is swallowed.
 * - json.dumps uses Python separators ", " / ": " and ensure_ascii=False.
 * - Fixture domain: ASCII text; integral floats normalize to int; invalid-JSON
 *   and non-string fts_snippet/anchor shapes are excluded and documented.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ss3_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results; NULL == no channel */
    json_t *cursors;    /* owned; per-name queue cursors */
    json_t *trace;      /* owned; ordered seam events */
    const json_t *in;   /* borrowed whole case input */
} ss3_ctx_t;

/* ── basic helpers ── */
static bool ss3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool ss3_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool ss3_truthy(const json_t *v) {
    if (ss3_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *ss3_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool ss3_str_eq(const json_t *v, const char *s) {
    const char *t = ss3_str(v);
    return t && s && strcmp(t, s) == 0;
}

static char *ss3_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static char *ss3_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return ss3_strdup("");
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : ss3_strdup("");
}

static json_t *ss3_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static const char *ss3_exc_type(const json_t *v) {
    return json_get_str(v, "__raise__", "RuntimeError");
}

static const char *ss3_exc_msg(const json_t *v) {
    return json_get_str(v, "message", "");
}

/* Python type name over the fixture domain (for TypeError messages). */
static const char *ss3_typename(const json_t *v) {
    if (ss3_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str(x) over the domain str/int/bool/null; containers handled by dumps. */
static char *ss3_pystr(const json_t *v) {
    if (ss3_is_null(v)) return ss3_strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return ss3_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                return ss3_fmt("%lld", (long long)v->num_val);
            return ss3_fmt("%.17g", v->num_val);
        case JSON_STRING: return ss3_strdup(v->str_val ? v->str_val : "");
        default: return ss3_strdup("");
    }
}

/* str.strip() over the ASCII fixture domain (includes \x1c-\x1f). */
static char *ss3_strip(const char *s) {
    if (!s) return ss3_strdup("");
    while (*s && (unsigned char)*s <= 0x20) s++;
    size_t n = strlen(s);
    while (n > 0 && (unsigned char)s[n - 1] <= 0x20) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* strip an explicit char set from both ends (Python s.strip(chars)). */
static char *ss3_strip_chars(const char *s, const char *chars) {
    if (!s) return ss3_strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && strchr(chars, s[a])) a++;
    while (b > a && strchr(chars, s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}

static char *ss3_lower_dup(const char *s) {
    char *o = ss3_strdup(s);
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* case-insensitive ASCII substring search (Python str.lower().find). */
static long ss3_find_ci(const char *hay, const char *needle) {
    if (!hay || !needle || !needle[0]) return -1;
    char *h = ss3_lower_dup(hay);
    char *n = ss3_lower_dup(needle);
    char *p = strstr(h, n);
    long idx = p ? (long)(p - h) : -1;
    free(h); free(n);
    return idx;
}

/* ── FULL builtin exception hierarchy (py3.14) ── */
typedef struct { const char *name; const char *parent; } ss3_exc_row_t;
static const ss3_exc_row_t SS3_EXC_PARENTS[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"CancelledError", "BaseException"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "Exception"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "Exception"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
};

static const char *ss3_exc_parent(const char *name) {
    for (size_t i = 0; i < sizeof(SS3_EXC_PARENTS) / sizeof(SS3_EXC_PARENTS[0]); i++)
        if (strcmp(SS3_EXC_PARENTS[i].name, name) == 0)
            return SS3_EXC_PARENTS[i].parent;
    return "Exception";
}

/* True when `except Exception` catches this kind. */
static bool ss3_exc_is_exception(const char *kind) {
    const char *cur = kind;
    for (int hops = 0; hops < 16; hops++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return false;
        cur = ss3_exc_parent(cur);
    }
    return false;
}

/* ── universal seam dispatch ── */
static json_t *ss3_make_exc(const json_t *spec) {
    return ss3_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}

static json_t *ss3_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ss3_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one event and answer the script.  args is OWNED (moved into trace). */
static json_t *ss3_call(ss3_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *res;
    if (!c->io) {
        char *msg = ss3_fmt("no seam channel installed for %s", name);
        res = ss3_err("RuntimeError", msg);
        free(msg);
    } else {
        const json_t *entry = json_obj_get(c->io, name);
        if (!entry) {
            char *msg = ss3_fmt("no script for %s", name);
            res = ss3_err("RuntimeError", msg);
            free(msg);
        } else if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(c->cursors, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) {
                char *msg = ss3_fmt("script exhausted for %s", name);
                res = ss3_err("RuntimeError", msg);
                free(msg);
            } else {
                json_set(c->cursors, name, json_number(cur + 1));
                res = ss3_entry_result(json_get(entry, idx));
            }
        } else {
            res = ss3_entry_result(entry);
        }
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* Copy a borrowed value into args; NULL / JSON_NULL drops the key. */
static void ss3_jset(json_t *obj, const char *key, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(obj, key, json_copy(v));
}

/* ── Python json.dumps(ensure_ascii=False, separators=", " / ": ") ── */
typedef struct { char *p; size_t cap, len; } ss3_sb_t;
static void ss3_sb_put(ss3_sb_t *s, const char *t) {
    size_t n = strlen(t);
    if (s->len + n + 1 > s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 256;
        while (nc < s->len + n + 1) nc *= 2;
        char *np = realloc(s->p, nc);
        if (!np) return;
        s->p = np; s->cap = nc;
    }
    memcpy(s->p + s->len, t, n); s->len += n; s->p[s->len] = '\0';
}
static void ss3_sb_putc(ss3_sb_t *s, char c) { char t[2] = {c, '\0'}; ss3_sb_put(s, t); }
static void ss3_dumps_into(ss3_sb_t *s, const json_t *v) {
    if (ss3_is_null(v)) { ss3_sb_put(s, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: ss3_sb_put(s, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char b[64]; double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(b, sizeof b, "%.0f", d);
            else snprintf(b, sizeof b, "%.17g", d);
            ss3_sb_put(s, b); return;
        }
        case JSON_STRING:
            ss3_sb_putc(s, '"');
            for (const char *p = v->str_val ? v->str_val : ""; *p; p++) {
                unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '"':  ss3_sb_put(s, "\\\""); break;
                    case '\\': ss3_sb_put(s, "\\\\"); break;
                    case '\n': ss3_sb_put(s, "\\n"); break;
                    case '\r': ss3_sb_put(s, "\\r"); break;
                    case '\t': ss3_sb_put(s, "\\t"); break;
                    case '\b': ss3_sb_put(s, "\\b"); break;
                    case '\f': ss3_sb_put(s, "\\f"); break;
                    default:
                        if (c < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); ss3_sb_put(s, e); }
                        else ss3_sb_putc(s, (char)c);
                }
            }
            ss3_sb_putc(s, '"'); return;
        case JSON_ARRAY:
            ss3_sb_putc(s, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ss3_sb_put(s, ", ");
                ss3_dumps_into(s, v->c.items[i]);
            }
            ss3_sb_putc(s, ']'); return;
        case JSON_OBJECT:
            ss3_sb_putc(s, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ss3_sb_put(s, ", ");
                json_t *k = json_string(v->c.keys[i]);
                ss3_dumps_into(s, k);
                json_free(k);
                ss3_sb_put(s, ": ");
                ss3_dumps_into(s, v->c.items[i]);
            }
            ss3_sb_putc(s, '}'); return;
        default: ss3_sb_put(s, "null"); return;
    }
}
static char *ss3_py_dumps(const json_t *v) {
    ss3_sb_t s = {0};
    ss3_dumps_into(&s, v);
    return s.p ? s.p : ss3_strdup("");
}

/* thousands-separated integer, Python f"{n:,}" */
static char *ss3_thousands(long long n) {
    bool neg = n < 0;
    unsigned long long u = neg ? (unsigned long long)(-(n + 1)) + 1ULL : (unsigned long long)n;
    char dig[64]; int dn = 0;
    do { dig[dn++] = (char)('0' + (u % 10)); u /= 10; } while (u);
    ss3_sb_t s = {0};
    if (neg) ss3_sb_putc(&s, '-');
    for (int i = dn - 1; i >= 0; i--) {
        ss3_sb_putc(&s, dig[i]);
        if (i > 0 && i % 3 == 0) ss3_sb_putc(&s, ',');
    }
    return s.p ? s.p : ss3_strdup("0");
}

/* ── FTS snippet regex helpers ── */

/* re.findall(r">>>(.*?)<<<", s, DOTALL): dedup raw captures (dict.fromkeys),
 * keep those whose .strip() is non-empty.  Returned array owns strings. */
static json_t *ss3_snippet_terms(const char *s) {
    json_t *out = json_array();
    if (!s) return out;
    const char *p = s;
    while ((p = strstr(p, ">>>")) != NULL) {
        const char *start = p + 3;
        const char *end = strstr(start, "<<<");
        if (!end) break;
        size_t n = (size_t)(end - start);
        char *term = malloc(n + 1);
        if (term) {
            memcpy(term, start, n); term[n] = '\0';
            char *t = ss3_strip(term);
            bool nonempty = t[0] != '\0';
            bool dup = false;
            for (size_t i = 0; i < json_len(out); i++) {
                const char *prev = ss3_str(json_get(out, i));
                if (prev && strcmp(prev, term) == 0) { dup = true; break; }
            }
            if (nonempty && !dup) json_append(out, json_string(term));
            free(t);
            free(term);
        }
        p = end + 3;
    }
    return out;
}

/* re.sub("|".join(re.escape(t) for t in sorted(terms,key=len,reverse=True)),
 *        lambda m: ">>>"+m.group(0)+"<<<", window, IGNORECASE). */
static char *ss3_highlight(const char *text, const json_t *terms) {
    size_t nterms = json_len(terms);
    if (!nterms) return ss3_strdup(text);
    /* stable sort terms by descending length (Python sorted is stable) */
    const json_t **ord = malloc(nterms * sizeof(*ord));
    if (!ord) return ss3_strdup(text);
    for (size_t i = 0; i < nterms; i++) ord[i] = json_get(terms, i);
    for (size_t i = 1; i < nterms; i++) {
        const json_t *k = ord[i]; size_t j = i;
        size_t kl = strlen(ss3_str(k) ? ss3_str(k) : "");
        while (j > 0 && strlen(ss3_str(ord[j-1]) ? ss3_str(ord[j-1]) : "") < kl) {
            ord[j] = ord[j-1]; j--;
        }
        ord[j] = k;
    }
    ss3_sb_t s = {0};
    size_t n = strlen(text), i = 0;
    while (i < n) {
        bool matched = false;
        for (size_t t = 0; t < nterms; t++) {
            const char *term = ss3_str(ord[t]);
            if (!term || !term[0]) continue;
            size_t tl = strlen(term);
            if (i + tl <= n) {
                char *frag = malloc(tl + 1);
                if (frag) {
                    memcpy(frag, text + i, tl); frag[tl] = '\0';
                    char *lf = ss3_lower_dup(frag), *lt = ss3_lower_dup(term);
                    bool eq = strcmp(lf, lt) == 0;
                    free(lf); free(lt); free(frag);
                    if (eq) {
                        ss3_sb_put(&s, ">>>");
                        char m[512];
                        if (tl < sizeof(m)) {
                            memcpy(m, text + i, tl); m[tl] = '\0';
                            ss3_sb_put(&s, m);
                        }
                        ss3_sb_put(&s, "<<<");
                        i += tl;
                        matched = true;
                        break;
                    }
                }
            }
        }
        if (!matched) { ss3_sb_putc(&s, text[i]); i++; }
    }
    free(ord);
    return s.p ? s.p : ss3_strdup(text);
}

/* re.match(r"data:[^\s,]*;base64,", s) at position 0. */
static bool ss3_data_url_match(const char *s) {
    if (!s || strncmp(s, "data:", 5) != 0) return false;
    const char *beg = s + 5;
    /* re.match(r"data:[^\s,]*;base64,") — [^\s,]* backtracks so that the
     * literal ";base64," can start at any point after a clean prefix. */
    for (const char *q = beg; *q; q++) {
        if (strncmp(q, ";base64,", 8) == 0) {
            for (const char *r = beg; r < q; r++)
                if (isspace((unsigned char)*r) || *r == ',') return false;
            return true;
        }
        if (isspace((unsigned char)*q) || *q == ',') return false;
    }
    return false;
}

/* ── _content_text (pure) ──
 * list: " ".join(p["text"] for dict parts whose "text" is str)
 * else: "" for null, str(content) otherwise (str domain). */
static char *ss3_content_text(const json_t *content) {
    if (content && content->type == JSON_ARRAY) {
        ss3_sb_t s = {0};
        bool first = true;
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *part = content->c.items[i];
            if (part->type != JSON_OBJECT) continue;
            const json_t *t = json_obj_get(part, "text");
            if (!t || t->type != JSON_STRING || !t->str_val) continue;
            if (!first) ss3_sb_put(&s, " ");
            ss3_sb_put(&s, t->str_val);
            first = false;
        }
        return s.p ? s.p : ss3_strdup("");
    }
    if (ss3_is_null(content)) return ss3_strdup("");
    return ss3_pystr(content);
}

/* ── PoP: ss3_redact_derived_value @ tools/session_search_tool.py:_redact_derived_value ── */
/* Recursive strict redaction.  Returns an owned node; a raise spec propagates. */
static json_t *ss3_redact_value(ss3_ctx_t *c, const json_t *v) {
    if (v && v->type == JSON_STRING) {
        if (ss3_data_url_match(v->str_val))
            return json_string("[inline data omitted]");
        json_t *a = json_object();
        ss3_jset(a, "value", v);
        json_t *r = ss3_call(c, "redact_derived_text", a);
        if (ss3_is_raise(r)) return r;
        return r; /* seam result already owned */
    }
    if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT)) {
        json_t *out = (v->type == JSON_ARRAY) ? json_array() : json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *child = ss3_redact_value(c, v->c.items[i]);
            if (ss3_is_raise(child)) { json_free(out); return child; }
            if (v->type == JSON_ARRAY) json_append(out, child);
            else json_set(out, v->c.keys[i], child);
        }
        return out;
    }
    return json_copy(v);
}

/* ── PoP: ss3_redact_tool_calls @ tools/session_search_tool.py:_redact_tool_calls ── */
static json_t *ss3_redact_tool_calls(ss3_ctx_t *c, const json_t *v) {
    if (!v || (v->type != JSON_ARRAY && v->type != JSON_OBJECT))
        return ss3_redact_value(c, v);
    /* live: isinstance(tool_calls, (list, tuple)); dict is NOT a list -> redact as value */
    if (v->type != JSON_ARRAY) return ss3_redact_value(c, v);
    json_t *out = json_array();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *call = v->c.items[i];
        const json_t *fn = (call->type == JSON_OBJECT) ? json_obj_get(call, "function") : NULL;
        if (fn && fn->type == JSON_OBJECT) {
            const json_t *arg = json_obj_get(fn, "arguments");
            if (arg && arg->type != JSON_NULL) {
                json_t *rv = ss3_redact_value(c, arg);
                if (ss3_is_raise(rv)) { json_free(out); return rv; }
                json_t *nc = json_object();
                bool placed = false;
                for (size_t k = 0; k < call->c.count; k++) {
                    if (strcmp(call->c.keys[k], "function") == 0) {
                        json_t *nf = json_copy(fn);
                        json_set(nf, "arguments", rv); /* transfers rv; keeps key order */
                        rv = NULL;
                        json_set(nc, "function", nf);
                        placed = true;
                    } else {
                        json_set(nc, call->c.keys[k], json_copy(call->c.items[k]));
                    }
                }
                if (!placed) {
                    json_t *nf = json_copy(fn);
                    json_set(nf, "arguments", rv); rv = NULL;
                    json_set(nc, "function", nf);
                }
                json_free(rv);
                json_append(out, nc);
                continue;
            }
        }
        json_append(out, json_copy(call));
    }
    return out;
}

/* ── PoP: ss3_content_text @ tools/session_search_tool.py:_content_text ── */
static json_t *ss3_content_text_fn(ss3_ctx_t *c, const json_t *args) {
    (void)c;
    const json_t *content = json_obj_get(args, "content");
    char *s = ss3_content_text(content);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* ── PoP: ss3_discovery_entry @ tools/session_search_tool.py:_discovery_entry ── */
static const char *SS3_DISCOVERY_KEYS[] = {
    "session_id", "when", "source", "model", "title", "matched_role",
    "match_message_id", "snippet", "bookend_start", "messages", "bookend_end",
    "messages_before", "messages_after", "detail",
};
static json_t *ss3_discovery_entry(ss3_ctx_t *c, const json_t *args) {
    (void)c;
    json_t *entry = json_object();
    for (size_t i = 0; i < sizeof(SS3_DISCOVERY_KEYS)/sizeof(SS3_DISCOVERY_KEYS[0]); i++) {
        const json_t *v = json_obj_get(args, SS3_DISCOVERY_KEYS[i]);
        json_set(entry, SS3_DISCOVERY_KEYS[i], v ? json_copy(v) : json_null());
    }
    const json_t *root = json_obj_get(args, "lineage_root");
    const char *rs = ss3_str(root);
    if (rs && rs[0]) {
        const char *sid = ss3_str(json_obj_get(args, "session_id"));
        if (!sid || strcmp(rs, sid) != 0)
            json_set(entry, "parent_session_id", json_copy(root));
    }
    return entry;
}

/* ── PoP: ss3_derived_snippet @ tools/session_search_tool.py:_derived_snippet ── */
static json_t *ss3_derived_snippet(ss3_ctx_t *c, const json_t *args) {
    const json_t *fts = json_obj_get(args, "fts_snippet");
    const json_t *anchor = json_obj_get(args, "anchor");
    if (ss3_is_null(anchor)) return json_string("");

    char *raw = (fts && fts->type == JSON_STRING) ? ss3_strdup(fts->str_val)
                                                 : ((fts && !ss3_is_null(fts)) ? ss3_pystr(fts) : ss3_strdup(""));
    json_t *terms = ss3_snippet_terms(raw);

    /* sources[0] = _content_text(_redact_derived_value(anchor.content)) */
    const json_t *acontent = json_obj_get(anchor, "content");
    json_t *red = ss3_redact_value(c, acontent);
    if (ss3_is_raise(red)) { json_free(terms); free(raw); return red; }
    char *src0 = ss3_content_text(red);
    json_free(red);

    /* tool_calls source */
    char *src1 = NULL;
    const json_t *calls = json_obj_get(anchor, "tool_calls");
    if (calls && !ss3_is_null(calls)) {
        json_t *rc = ss3_redact_tool_calls(c, calls);
        if (ss3_is_raise(rc)) { json_free(terms); free(raw); free(src0); return rc; }
        if (rc->type == JSON_STRING) src1 = ss3_strdup(rc->str_val);
        else src1 = ss3_py_dumps(rc);
        json_free(rc);
    }

    /* probes = terms, or [str(fts_snippet or "").strip(". ")[:20]] */
    json_t *probes = terms;
    char *fallback_probe = NULL;
    if (json_len(terms) == 0) {
        char *tb = ss3_strip_chars(raw, ". ");
        size_t lim = strlen(tb) < 20 ? strlen(tb) : 20;
        fallback_probe = malloc(lim + 1);
        if (fallback_probe) { memcpy(fallback_probe, tb, lim); fallback_probe[lim] = '\0'; }
        free(tb);
        probes = json_array();
        if (fallback_probe) json_append(probes, json_string(fallback_probe));
    }

    /* locate: for each source, min find over probes; first source with pos>=0 wins */
    const char *cands[2] = { src0, src1 };
    size_t ncand = src1 ? 2 : 1;
    const char *text = src0 ? src0 : "";
    long pos = -1;
    for (size_t ci = 0; ci < ncand; ci++) {
        char *lowered = ss3_lower_dup(cands[ci] ? cands[ci] : "");
        long best = -1;
        for (size_t t = 0; t < json_len(probes); t++) {
            const char *pv = ss3_str(json_get(probes, t));
            if (!pv || !pv[0]) continue;
            long idx = ss3_find_ci(lowered, pv);
            if (idx >= 0 && (best < 0 || idx < best)) best = idx;
        }
        free(lowered);
        if (best >= 0) { pos = best; text = cands[ci]; break; }
    }
    size_t tlen = strlen(text);
    size_t start = (pos >= 0 && (size_t)pos > 100) ? (size_t)pos - 100 : 0;
    size_t abspos = pos >= 0 ? (size_t)pos : 0;
    size_t end = abspos + 200; if (end > tlen) end = tlen;
    if (start > end) start = end;

    char *window = malloc(end - start + 1);
    if (window) { memcpy(window, text + start, end - start); window[end - start] = '\0'; }
    char *hl = (json_len(terms) > 0) ? ss3_highlight(window ? window : "", terms)
                                     : ss3_strdup(window ? window : "");

    ss3_sb_t s = {0};
    if (start) ss3_sb_put(&s, "...");
    ss3_sb_put(&s, hl);
    if (end < tlen) ss3_sb_put(&s, "...");
    json_t *ret = json_string(s.p ? s.p : "");

    free(s.p); free(window); free(hl);
    free(src0); free(src1); free(fallback_probe); free(raw);
    if (probes != terms) json_free(probes);
    json_free(terms);
    return ret;
}

/* ── PoP: ss3_bookend @ tools/session_search_tool.py:_bookend ── */
static json_t *ss3_bookend(ss3_ctx_t *c, const json_t *args) {
    const json_t *rows = json_obj_get(args, "rows");
    json_t *out = json_array();
    if (!rows || rows->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < rows->c.count; i++) {
        json_t *m = rows->c.items[i];
        json_t *a = json_object();
        ss3_jset(a, "content", json_obj_get(m, "content"));
        json_t *isc = ss3_call(c, "is_compaction_summary", a);
        if (ss3_is_raise(isc)) { json_free(out); return isc; }
        bool skip = ss3_truthy(isc);
        json_free(isc);
        if (skip) continue;
        json_t *b = json_object();
        ss3_jset(b, "message", m);
        json_set(b, "derived", json_bool(true));
        json_set(b, "max_content_len", json_number(1200.0));
        json_t *sm = ss3_call(c, "shape_message", b);
        if (ss3_is_raise(sm)) { json_free(out); return sm; }
        json_append(out, sm);
    }
    return out;
}

/* ── PoP: ss3_discover_payload @ tools/session_search_tool.py:_discover_payload ── */
static json_t *ss3_discover_payload(ss3_ctx_t *c, const json_t *args) {
    json_t *status = ss3_call(c, "search_index_rebuild_status", json_object());
    if (ss3_is_raise(status)) {
        if (ss3_exc_is_exception(ss3_exc_type(status))) {
            /* _quiet: logging.debug(msg, exc_info=True), then return None */
            json_t *d = json_object();
            json_set_hdr(d, "message", "fts_rebuild_status failed");
            json_set(d, "args", json_array());
            json_set(d, "exc_info", json_bool(true));
            json_t *lr = ss3_call(c, "logger.debug", d);
            if (ss3_is_raise(lr)) { json_free(status); return lr; }
            json_free(lr);
            json_free(status);
            status = NULL;
        } else {
            return status; /* BaseException propagates through _quiet */
        }
    }

    json_t *p = json_object();
    json_set(p, "success", json_bool(true));
    json_set(p, "mode", json_string("discover"));
    ss3_jset(p, "query", json_obj_get(args, "query"));
    ss3_jset(p, "detail", json_obj_get(args, "detail"));
    ss3_jset(p, "results", json_obj_get(args, "results"));
    const json_t *results = json_obj_get(args, "results");
    json_set(p, "count", json_number((double)(results && results->type == JSON_ARRAY ? results->c.count : 0)));
    const json_t *extra = json_obj_get(args, "extra");
    if (extra && extra->type == JSON_OBJECT)
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(p, extra->c.keys[i], json_copy(extra->c.items[i]));

    if (status && status->type == JSON_OBJECT) {
        const json_t *pct = json_obj_get(status, "percent");
        char *ps = ss3_pystr(pct);
        char *ix = ss3_thousands((long long)json_get_num(status, "indexed", 0));
        char *tt = ss3_thousands((long long)json_get_num(status, "total", 0));
        char *note = ss3_fmt(
            "The search index is rebuilding in the background (%s%% done, %s of %s messages). "
            "Results from older messages may be incomplete until it finishes.", ps, ix, tt);
        json_t *inner = json_object();
        json_set(inner, "percent", pct ? json_copy(pct) : json_null());
        json_set(inner, "note", json_string(note));
        json_set(p, "index_rebuild", inner);
        free(note); free(ix); free(tt); free(ps);
    }
    json_free(status);

    char *s = ss3_py_dumps(p);
    json_t *ret = json_string(s);
    free(s);
    json_free(p);
    return ret;
}

/* meta && meta.field */
static const json_t *ss3_mfield(const json_t *meta, const char *k) {
    if (!meta || meta->type != JSON_OBJECT) return NULL;
    return json_obj_get(meta, k);
}

/* ── PoP: ss3_hydrate_hit @ tools/session_search_tool.py:_hydrate_hit ── */
static json_t *ss3_hydrate_hit(ss3_ctx_t *c, const json_t *args) {
    const json_t *hit = json_obj_get(args, "hit");
    const json_t *root = json_obj_get(args, "lineage_root");
    const json_t *detail = json_obj_get(args, "result_detail");
    const json_t *hs = json_obj_get(hit, "session_id");
    const json_t *sid = (hs && hs->type != JSON_NULL) ? hs : root;
    const json_t *mid = json_obj_get(hit, "message_id");

    /* try: view = read_anchored_view(...) */
    json_t *a = json_object();
    ss3_jset(a, "session_id", sid);
    ss3_jset(a, "message_id", mid);
    json_set(a, "window", json_number(5.0));
    json_set(a, "bookend", json_number(3.0));
    json_t *vres = ss3_call(c, "read_anchored_view", a);
    if (ss3_is_raise(vres)) {
        if (ss3_exc_is_exception(ss3_exc_type(vres))) {
            char *msg_s = ss3_strdup(ss3_exc_msg(vres));
            json_t *w = json_object();
            json_set_hdr(w, "message", "get_anchored_view failed for %s/%s: %s");
            json_t *arr = json_array();
            json_append(arr, sid ? json_copy(sid) : json_null());
            json_append(arr, mid ? json_copy(mid) : json_null());
            json_append(arr, json_string(msg_s));
            json_set(w, "args", arr);
            json_set(w, "exc_info", json_bool(true));
            json_free(vres);
            json_t *wr = ss3_call(c, "logger.warning", w);
            json_free(wr);
            free(msg_s);
            return json_null();
        }
        return vres; /* BaseException propagates */
    }
    json_t *view = vres; /* owned object */

    /* session_meta = _get_session_meta(hit_sid) or _get_session_meta(root) */
    json_t *ma = json_object();
    ss3_jset(ma, "session_id", sid);
    json_t *meta = ss3_call(c, "get_session_meta", ma);
    if (ss3_is_raise(meta)) { json_free(view); return meta; }
    if (ss3_is_null(meta)) {
        json_free(meta);
        json_t *mb = json_object();
        ss3_jset(mb, "session_id", root);
        meta = ss3_call(c, "get_session_meta", mb);
        if (ss3_is_raise(meta)) { json_free(view); return meta; }
    }
    bool full = detail && ss3_str_eq(detail, "full");

    /* anchor = next(m for m in view.window if m.id == msg_id) */
    const json_t *anchor = NULL;
    const json_t *win = json_obj_get(view, "window");
    char *mid_s = ss3_pystr(mid);
    if (win && win->type == JSON_ARRAY) {
        for (size_t i = 0; i < win->c.count; i++) {
            json_t *m = win->c.items[i];
            char *id_s = ss3_pystr(json_obj_get(m, "id"));
            if (strcmp(id_s, mid_s) == 0) { anchor = m; free(id_s); break; }
            free(id_s);
        }
    }

    /* Build the discovery entry in canonical key order via ss3_discovery_entry. */
    json_t *ea = json_object();
    ss3_jset(ea, "lineage_root", root);
    ss3_jset(ea, "session_id", sid);

    /* when = _format_timestamp((meta and meta.started_at) or hit.session_started) */
    const json_t *when = ss3_mfield(meta, "started_at");
    if (!ss3_truthy(when)) when = json_obj_get(hit, "session_started");
    json_t *ta = json_object();
    ss3_jset(ta, "ts", when);
    json_t *when_v = ss3_call(c, "format_timestamp", ta);
    if (ss3_is_raise(when_v)) { json_free(view); json_free(meta); free(mid_s); return when_v; }
    json_set(ea, "when", when_v);

    /* source/model */
    const json_t *src = ss3_mfield(meta, "source");
    if (!ss3_truthy(src)) src = json_obj_get(hit, "source");
    ss3_jset(ea, "source", src);
    const json_t *model = ss3_mfield(meta, "model");
    if (!ss3_truthy(model)) model = json_obj_get(hit, "model");
    if (!ss3_truthy(model)) json_set(ea, "model", json_string("unknown"));
    else ss3_jset(ea, "model", model);

    /* title = redact_derived_text(meta.title if meta else None) or None */
    json_t *ra = json_object();
    const json_t *mtitle = ss3_mfield(meta, "title");
    ss3_jset(ra, "value", mtitle);
    json_t *title = ss3_call(c, "redact_derived_text", ra);
    if (ss3_is_raise(title)) { json_free(view); json_free(meta); free(mid_s); return title; }
    if (ss3_truthy(title)) json_set(ea, "title", title);
    else { json_free(title); json_set(ea, "title", json_null()); }

    ss3_jset(ea, "matched_role", json_obj_get(hit, "role"));
    ss3_jset(ea, "match_message_id", mid);

    /* snippet = _derived_snippet(hit.snippet, anchor) */
    json_t *sa = json_object();
    ss3_jset(sa, "fts_snippet", json_obj_get(hit, "snippet"));
    ss3_jset(sa, "anchor", anchor);
    json_t *snip = ss3_derived_snippet(c, sa);
    if (ss3_is_raise(snip)) { json_free(view); json_free(meta); free(mid_s); return snip; }
    json_set(ea, "snippet", snip);

    /* bookend_start / bookend_end */
    if (full) {
        json_t *ba = json_object();
        ss3_jset(ba, "rows", json_obj_get(view, "bookend_start"));
        json_t *bs = ss3_bookend(c, ba);
        if (ss3_is_raise(bs)) { json_free(view); json_free(meta); free(mid_s); return bs; }
        json_set(ea, "bookend_start", bs);
    } else json_set(ea, "bookend_start", json_array());

    /* messages: [shape_message(m, anchor_id=mid, max_content_len=4000, derived=True)
     *            for m in view.window if full or m.id == msg_id] */
    json_t *msgs = json_array();
    if (win && win->type == JSON_ARRAY) {
        for (size_t i = 0; i < win->c.count; i++) {
            json_t *m = win->c.items[i];
            char *id_s = ss3_pystr(json_obj_get(m, "id"));
            bool keep = full || strcmp(id_s, mid_s) == 0;
            free(id_s);
            if (!keep) continue;
            json_t *sh = json_object();
            ss3_jset(sh, "message", m);
            json_set(sh, "derived", json_bool(true));
            json_set(sh, "max_content_len", json_number(4000.0));
            ss3_jset(sh, "anchor_id", mid);
            json_t *one = ss3_call(c, "shape_message", sh);
            if (ss3_is_raise(one)) {
                json_free(msgs); json_free(view); json_free(meta); free(mid_s); return one;
            }
            json_append(msgs, one);
        }
    }
    json_set(ea, "messages", msgs);

    if (full) {
        json_t *ba = json_object();
        ss3_jset(ba, "rows", json_obj_get(view, "bookend_end"));
        json_t *be = ss3_bookend(c, ba);
        if (ss3_is_raise(be)) { json_free(ea); json_free(view); json_free(meta); free(mid_s); return be; }
        json_set(ea, "bookend_end", be);
    } else json_set(ea, "bookend_end", json_array());

    ss3_jset(ea, "messages_before", json_obj_get(view, "before"));
    ss3_jset(ea, "messages_after", json_obj_get(view, "after"));
    ss3_jset(ea, "detail", detail);

    /* ss3_discovery_entry adds parent_session_id when the root differs. */
    json_t *entry = ss3_discovery_entry(c, ea);
    json_free(ea); json_free(view); json_free(meta); free(mid_s);
    return entry;
}

/* ── dispatch wrappers (the universal-seam harness hands each case its args
 * object; these unwrap the single positional argument the live def takes). ── */
static json_t *ss3_redact_derived_value_dispatch(ss3_ctx_t *c, const json_t *args) {
    return ss3_redact_value(c, json_obj_get(args, "value"));
}
static json_t *ss3_redact_tool_calls_dispatch(ss3_ctx_t *c, const json_t *args) {
    return ss3_redact_tool_calls(c, json_obj_get(args, "tool_calls"));
}
