/*
 * session_search_gaps_flow2.c — FLOW port of the four "gap" leaves of
 * tools/session_search_tool.py (quarry commit 39e480c051, where these defs
 * still lived in that module before the common/discover split).  The stub-
 * era PoP claims in src/tools/session_search_tool_gaps.c are shims (constant
 * returns), not ports; this file is the real differential implementation.
 *
 * Ported targets (4):
 *   _session_end_reason          -> ssg2_session_end_reason
 *   _session_left_live_context   -> ssg2_session_left_live_context
 *   _is_fresh_reset_session      -> ssg2_is_fresh_reset_session
 *   _session_search_impl         -> ssg2_session_search_impl
 *
 * Decision sequences only: every live collaborator call enters through ONE
 * universal seam ssg2_call(ctx, name, args); the harness wires it to a
 * recording stub fed the case's in.io, and the oracle installs recorders
 * emitting the identical {"name","args","ret"} event.  The harness #includes
 * this file directly (self-contained, libjson only); all symbols are static
 * and ssg2_-prefixed.
 *
 * Seam table (name -> live call / args):
 *   db.get_session          db.get_session(session_id)     {session_id}
 *   session_db.close        located.close()                {db}
 *   _resolve_profile_db     _resolve_profile_db(profile)   {profile}
 *   tool_error              tool_error(msg, success=False) {message,success}
 *   _scroll                 _scroll(...)                   {db,session_id,
 *                              around_message_id,window,current_session_id}
 *   _read_session           _read_session(db,sid,link_profile)
 *                                                          {db,session_id,
 *                              link_profile}
 *   _locate_session_db      _locate_session_db(sid)        {session_id}
 *   _list_recent_sessions   _list_recent_sessions(...)     {db,limit,
 *                              current_session_id,link_profile}
 *   _discover               _discover(...)                 {db,query,
 *                              role_filter,limit,sort,current_session_id,
 *                              link_profile}
 *
 * Representation contract (shared with tests/oracle_session_search_gaps_flow2.py):
 * - NULL json_t* / JSON_NULL == Python None.  A raise is
 *   {"__raise__": <ClassName>, "message": str(exc)}.
 * - Args whose value is None (including a JSON_NULL node) are dropped on
 *   both sides before a seam event is recorded.
 * - Scripted seams: plain value reused, bare array per-call queue (last value
 *   does NOT repeat), {"__script__":true,"ret"/"raise"}; a bare
 *   {"__raise__","message"} dict also raises.  Missing/exhausted script is
 *   fail-closed RuntimeError; absent io is "no seam channel installed".
 * - The FULL py3.14 builtin exception hierarchy decides `except Exception`:
 *   SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/
 *   BaseExceptionGroup walk to BaseException and pass through; every other
 *   builtin (and unknown synthetic kinds) reaches Exception and is swallowed.
 * - db / profile_db / located are opaque STRING markers; the oracle wraps a
 *   marker in a DBDouble whose only method is get_session/close.
 * - state.owned_dbs (array, when PRESENT) models the non-None _owned_dbs list
 *   that _session_search_impl appends to on cross-profile resolution; only
 *   present keys are echoed.
 * - json.loads only accepts str; a null/int/dict seam result raises the exact
 *   CPython TypeError.  Invalid JSON text is out of the fixture domain.
 * - json.dumps(found, ensure_ascii=False) uses Python separators ", " / ": ".
 * - int(limit) domain: ASCII numeric strings (sign, whitespace, underscores),
 *   integral numbers, bool, null/other -> TypeError->3.  KeyError is excluded
 *   from scripted raises (str(KeyError(k)) == repr(k)); Unicode*Error/Group
 *   raises are restricted to plain-message fixtures.
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

typedef struct ssg2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results; NULL == no channel */
    json_t *cursors;    /* owned; per-name queue cursors */
    json_t *trace;      /* owned; ordered seam events */
    const json_t *in;   /* borrowed whole case input */
} ssg2_ctx_t;

/* ── basic helpers ── */
static bool ssg2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool ssg2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool ssg2_truthy(const json_t *v) {
    if (ssg2_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *ssg2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool ssg2_str_eq(const json_t *v, const char *s) {
    const char *t = ssg2_str(v);
    return t && s && strcmp(t, s) == 0;
}

static char *ssg2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static char *ssg2_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return ssg2_strdup("");
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : ssg2_strdup("");
}

static json_t *ssg2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static const char *ssg2_exc_type(const json_t *v) {
    return json_get_str(v, "__raise__", "RuntimeError");
}

static const char *ssg2_exc_msg(const json_t *v) {
    return json_get_str(v, "message", "");
}

/* Python type name over the fixture domain (for TypeError messages). */
static const char *ssg2_typename(const json_t *v) {
    if (ssg2_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str(x) over the domain str/int/bool/null; containers out of domain. */
static char *ssg2_pystr(const json_t *v) {
    if (ssg2_is_null(v)) return ssg2_strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return ssg2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                return ssg2_fmt("%lld", (long long)v->num_val);
            return ssg2_fmt("%.17g", v->num_val);
        case JSON_STRING: return ssg2_strdup(v->str_val ? v->str_val : "");
        default: return ssg2_strdup("");
    }
}

/* str.strip() over the ASCII fixture domain (includes \x1c-\x1f). */
static char *ssg2_strip(const char *s) {
    if (!s) return ssg2_strdup("");
    while (*s && (unsigned char)*s <= 0x20) s++;
    size_t n = strlen(s);
    while (n > 0 && (unsigned char)s[n - 1] <= 0x20) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

static bool ssg2_nonempty_str(const json_t *v) {
    const char *s = ssg2_str(v);
    return s && s[0] != '\0';
}

/* ── FULL builtin exception hierarchy (py3.14) ──
 * ExceptionGroup's first base is BaseExceptionGroup but it IS an Exception
 * subclass; store its Exception-lineage parent.  CancelledError is asyncio's
 * BaseException subclass.  Aliases (IOError/EnvironmentError/WindowsError)
 * route to Exception.  Unknown synthetic kinds route to Exception. */
typedef struct { const char *name; const char *parent; } ssg2_exc_row_t;
static const ssg2_exc_row_t SSG2_EXC_PARENTS[] = {
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

static const char *ssg2_exc_parent(const char *name) {
    for (size_t i = 0; i < sizeof(SSG2_EXC_PARENTS) / sizeof(SSG2_EXC_PARENTS[0]); i++)
        if (strcmp(SSG2_EXC_PARENTS[i].name, name) == 0)
            return SSG2_EXC_PARENTS[i].parent;
    return "Exception";
}

/* True when `except Exception` catches this kind. */
static bool ssg2_exc_is_exception(const char *kind) {
    const char *cur = kind;
    for (int hops = 0; hops < 16; hops++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0 || strcmp(cur, "object") == 0) return false;
        cur = ssg2_exc_parent(cur);
    }
    return false;
}

/* ── universal seam dispatch ── */
static json_t *ssg2_make_exc(const json_t *spec) {
    return ssg2_err(json_get_str(spec, "type", "RuntimeError"),
                    json_get_str(spec, "message", ""));
}

static json_t *ssg2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ssg2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one event and answer the script.  args is OWNED (moved into trace). */
static json_t *ssg2_call(ssg2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *res;
    if (!c->io) {
        char *msg = ssg2_fmt("no seam channel installed for %s", name);
        res = ssg2_err("RuntimeError", msg);
        free(msg);
    } else {
        const json_t *entry = json_obj_get(c->io, name);
        if (!entry) {
            char *msg = ssg2_fmt("no script for %s", name);
            res = ssg2_err("RuntimeError", msg);
            free(msg);
        } else if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(c->cursors, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) {
                char *msg = ssg2_fmt("script exhausted for %s", name);
                res = ssg2_err("RuntimeError", msg);
                free(msg);
            } else {
                json_set(c->cursors, name, json_number(cur + 1));
                res = ssg2_entry_result(json_get(entry, idx));
            }
        } else {
            res = ssg2_entry_result(entry);
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
static void ssg2_jset(json_t *obj, const char *key, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(obj, key, json_copy(v));
}

/* ── json.loads subset: str -> parsed tree, or a TypeError raise ── */
static json_t *ssg2_json_loads(const json_t *v) {
    if (!v || v->type != JSON_STRING) {
        char *msg = ssg2_fmt("the JSON object must be str, bytes or bytearray, not %s",
                             ssg2_typename(v));
        json_t *e = ssg2_err("TypeError", msg);
        free(msg);
        return e;
    }
    json_t *parsed = json_parse(v->str_val, NULL);
    if (!parsed) {
        /* Out of the fixture domain: seams always return well-formed JSON. */
        return ssg2_err("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)");
    }
    return parsed;
}

/* ── Python json.dumps(ensure_ascii=False) ── */
typedef struct { char *p; size_t cap, len; } ssg2_sb_t;
static void ssg2_sb_put(ssg2_sb_t *s, const char *t) {
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
static void ssg2_sb_putc(ssg2_sb_t *s, char c) { char t[2] = {c, '\0'}; ssg2_sb_put(s, t); }
static void ssg2_dumps_into(ssg2_sb_t *s, const json_t *v) {
    if (ssg2_is_null(v)) { ssg2_sb_put(s, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: ssg2_sb_put(s, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char b[64]; double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(b, sizeof b, "%.0f", d);
            else snprintf(b, sizeof b, "%.17g", d);
            ssg2_sb_put(s, b); return;
        }
        case JSON_STRING:
            ssg2_sb_putc(s, '"');
            for (const char *p = v->str_val ? v->str_val : ""; *p; p++) {
                unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '"':  ssg2_sb_put(s, "\\\""); break;
                    case '\\': ssg2_sb_put(s, "\\\\"); break;
                    case '\n': ssg2_sb_put(s, "\\n"); break;
                    case '\r': ssg2_sb_put(s, "\\r"); break;
                    case '\t': ssg2_sb_put(s, "\\t"); break;
                    case '\b': ssg2_sb_put(s, "\\b"); break;
                    case '\f': ssg2_sb_put(s, "\\f"); break;
                    default:
                        if (c < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); ssg2_sb_put(s, e); }
                        else ssg2_sb_putc(s, (char)c); /* ensure_ascii=False */
                }
            }
            ssg2_sb_putc(s, '"'); return;
        case JSON_ARRAY:
            ssg2_sb_putc(s, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ssg2_sb_put(s, ", ");
                ssg2_dumps_into(s, v->c.items[i]);
            }
            ssg2_sb_putc(s, ']'); return;
        case JSON_OBJECT: {
            ssg2_sb_putc(s, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ssg2_sb_put(s, ", ");
                json_t *k = json_string(v->c.keys[i]);
                ssg2_dumps_into(s, k);
                json_free(k);
                ssg2_sb_put(s, ": ");
                ssg2_dumps_into(s, v->c.items[i]);
            }
            ssg2_sb_putc(s, '}'); return;
        }
        default: ssg2_sb_put(s, "null"); return;
    }
}
static char *ssg2_py_dumps(const json_t *v) {
    ssg2_sb_t s = {0};
    ssg2_dumps_into(&s, v);
    return s.p ? s.p : ssg2_strdup("");
}

/* int(limit) coercion subset: returns a long long.  valid=false -> TypeError. */
static long long ssg2_coerce_int(const json_t *v, bool *valid) {
    *valid = true;
    if (ssg2_is_null(v)) { *valid = false; return 3; }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d) || fabs(d) > 9.0e18) { *valid = false; return 3; }
        return (long long)d; /* truncate toward zero, like int(float) */
    }
    if (v->type == JSON_STRING) {
        char *t = ssg2_strip(v->str_val);
        const char *p = t;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        long long acc = 0; bool any = false, prev_us = false;
        for (; *p; p++) {
            if (*p == '_') {
                if (!any || prev_us) { *valid = false; break; }
                prev_us = true; continue;
            }
            if (!isdigit((unsigned char)*p)) { *valid = false; break; }
            acc = acc * 10 + (*p - '0'); any = true; prev_us = false;
        }
        if (!any || prev_us) *valid = false;
        free(t);
        return *valid ? (neg ? -acc : acc) : 3;
    }
    *valid = false;
    return 3;
}

/* ── PoP: ssg2_session_end_reason @ tools/session_search_tool.py:_session_end_reason ── */
static json_t *ssg2_session_end_reason(ssg2_ctx_t *c, const json_t *args) {
    const json_t *sid = json_obj_get(args, "session_id");
    if (!ssg2_truthy(sid)) return json_null();
    json_t *a = json_object();
    ssg2_jset(a, "session_id", sid);
    json_t *r = ssg2_call(c, "db.get_session", a);
    if (ssg2_is_raise(r)) {
        if (ssg2_exc_is_exception(ssg2_exc_type(r))) { json_free(r); return json_null(); }
        return r; /* BaseException propagates unchanged */
    }
    if (!ssg2_truthy(r)) { json_free(r); return json_null(); }
    if (r->type != JSON_OBJECT) { json_free(r); return json_null(); }
    json_t *er = json_obj_get(r, "end_reason");
    json_t *out = ssg2_truthy(er) ? json_copy(er) : json_null();
    json_free(r);
    return out;
}

/* ── PoP: ssg2_session_left_live_context @ tools/session_search_tool.py:_session_left_live_context ── */
static json_t *ssg2_session_left_live_context(ssg2_ctx_t *c, const json_t *args) {
    json_t *r = ssg2_session_end_reason(c, args);
    if (ssg2_is_raise(r)) return r;
    bool ended = !ssg2_is_null(r);
    json_free(r);
    return json_bool(ended);
}

/* ── PoP: ssg2_is_fresh_reset_session @ tools/session_search_tool.py:_is_fresh_reset_session ── */
static const char *SSG2_FRESH_RESET_END_REASONS[] = {
    "session_reset", "session_switch", "idle", "daily", "suspended",
    "resume_pending_expired", "new_session",
};
static json_t *ssg2_is_fresh_reset_session(ssg2_ctx_t *c, const json_t *args) {
    (void)c;
    const json_t *er = json_obj_get(args, "end_reason");
    bool found = false;
    if (er && er->type == JSON_STRING) {
        for (size_t i = 0; i < sizeof(SSG2_FRESH_RESET_END_REASONS) /
                             sizeof(SSG2_FRESH_RESET_END_REASONS[0]); i++) {
            if (strcmp(er->str_val, SSG2_FRESH_RESET_END_REASONS[i]) == 0) {
                found = true; break;
            }
        }
    }
    return json_bool(found);
}

/* ── PoP: ssg2_session_search_impl @ tools/session_search_tool.py:_session_search_impl ── */
static json_t *ssg2_session_search_impl(ssg2_ctx_t *c, const json_t *args) {
    const json_t *query = json_obj_get(args, "query");
    const json_t *role_filter = json_obj_get(args, "role_filter");
    const json_t *limit = json_obj_get(args, "limit");
    const json_t *db0 = json_obj_get(args, "db");
    const json_t *cur0 = json_obj_get(args, "current_session_id");
    const json_t *sid0 = json_obj_get(args, "session_id");
    const json_t *around = json_obj_get(args, "around_message_id");
    const json_t *window_arg = json_obj_get(args, "window");
    json_t *window = window_arg ? json_copy(window_arg) : json_number(5.0);
    const json_t *sortv = json_obj_get(args, "sort");
    const json_t *profile = json_obj_get(args, "profile");

    json_t *db = json_copy(db0);                 /* owned; may be null */
    json_t *session_id = json_copy(sid0);        /* owned; may be null */
    json_t *prof = json_copy(profile);           /* owned; may be null */
    json_t *cur = json_copy(cur0);               /* owned; may be null */
#define SSG2_FREE_LOCALS() do { \
        json_free(db); json_free(session_id); json_free(prof); \
        json_free(cur); json_free(window); \
    } while (0)

    /* Normalise a raw `@session:<profile>/<id>` link. */
    const char *sidstr = ssg2_str(session_id);
    if (sidstr && strchr(sidstr, '/')) {
        const char *slash = strchr(sidstr, '/');
        size_t np = (size_t)(slash - sidstr);
        char *emb_prof = malloc(np + 1);
        char *emb_id = ssg2_strdup(slash + 1);
        if (emb_prof) { memcpy(emb_prof, sidstr, np); emb_prof[np] = '\0'; }
        if (emb_id && emb_id[0]) {
            json_free(session_id);
            session_id = json_string(emb_id);
            bool adopt = emb_prof && emb_prof[0];
            if (adopt && prof) {
                char *pt = ssg2_pystr(prof);
                char *ps = ssg2_strip(pt);
                adopt = (ps[0] == '\0');
                free(ps); free(pt);
            }
            if (adopt) {
                json_free(prof);
                prof = json_string(emb_prof);
            }
        }
        free(emb_prof);
        free(emb_id);
    }

    /* Cross-profile read: swap in the named profile's DB. */
    bool do_profile = false;
    if (prof && prof->type != JSON_NULL) {
        char *pt = ssg2_pystr(prof);
        char *ps = ssg2_strip(pt);
        do_profile = (ps[0] != '\0');
        free(ps); free(pt);
    }
    if (do_profile) {
        json_t *a = json_object();
        ssg2_jset(a, "profile", prof);
        json_t *profile_db = ssg2_call(c, "_resolve_profile_db", a);
        if (ssg2_is_raise(profile_db)) {
            if (ssg2_exc_is_exception(ssg2_exc_type(profile_db))) {
                char *pt = ssg2_pystr(prof);
                char *msg = ssg2_fmt("profile '%s': %s", pt, ssg2_exc_msg(profile_db));
                json_free(profile_db);
                json_t *ea = json_object();
                json_set_hdr(ea, "message", msg);
                json_set(ea, "success", json_bool(false));
                free(msg); free(pt);
                return ssg2_call(c, "tool_error", ea);
            }
            SSG2_FREE_LOCALS();
            return profile_db; /* BaseException propagates */
        }
        if (!ssg2_is_null(profile_db)) {
            json_free(db);
            db = profile_db; /* adopt owned */
            json_t *owned = json_obj_get(c->state, "owned_dbs");
            if (owned && owned->type == JSON_ARRAY) json_append(owned, json_copy(db));
            json_free(cur);
            cur = json_null();
        } else {
            json_free(profile_db);
        }
    }

    /* Scroll shape takes precedence. */
    if (ssg2_nonempty_str(session_id) && !ssg2_is_null(around)) {
        json_t *a = json_object();
        ssg2_jset(a, "db", db);
        ssg2_jset(a, "session_id", session_id);
        ssg2_jset(a, "around_message_id", around);
        ssg2_jset(a, "window", window);
        ssg2_jset(a, "current_session_id", cur);
        SSG2_FREE_LOCALS();
        return ssg2_call(c, "_scroll", a);
    }

    /* Read shape. */
    if (ssg2_nonempty_str(session_id)) {
        char *sid = ssg2_strip(session_id->str_val);
        json_t *a = json_object();
        ssg2_jset(a, "db", db);
        json_set_hdr(a, "session_id", sid);
        ssg2_jset(a, "link_profile", prof);
        json_t *result = ssg2_call(c, "_read_session", a);
        if (ssg2_is_raise(result)) {
            free(sid); SSG2_FREE_LOCALS();
            return result;
        }
        json_t *parsed = ssg2_json_loads(result);
        if (ssg2_is_raise(parsed)) {
            free(sid); json_free(result); json_free(db);
            json_free(session_id); json_free(prof); json_free(cur);
            return parsed;
        }
        bool success = parsed->type == JSON_OBJECT &&
                       ssg2_truthy(json_obj_get(parsed, "success"));
        json_free(parsed);
        if (success) {
            free(sid); SSG2_FREE_LOCALS();
            return result;
        }

        json_t *la = json_object();
        json_set_hdr(la, "session_id", sid);
        json_t *located_res = ssg2_call(c, "_locate_session_db", la);
        free(sid);
        if (ssg2_is_raise(located_res)) {
            json_free(result); json_free(db); json_free(session_id);
            json_free(prof); json_free(cur);
            return located_res;
        }
        /* located, owner = _locate_session_db(sid) — a 2-tuple unpack. */
        if (located_res->type != JSON_ARRAY) {
            char *msg = ssg2_fmt("cannot unpack non-iterable %s object",
                                 ssg2_typename(located_res));
            json_t *e = ssg2_err("TypeError", msg);
            free(msg);
            json_free(located_res); json_free(result); json_free(db);
            json_free(session_id); json_free(prof); json_free(cur);
            return e;
        }
        size_t ln = json_len(located_res);
        if (ln != 2) {
            char *msg = ln < 2
                ? ssg2_fmt("not enough values to unpack (expected 2, got %d)", (int)ln)
                : ssg2_strdup("too many values to unpack (expected 2)");
            json_t *e = ssg2_err("ValueError", msg);
            free(msg);
            json_free(located_res); json_free(result); json_free(db);
            json_free(session_id); json_free(prof); json_free(cur);
            return e;
        }
        json_t *located = json_get(located_res, 0);
        json_t *owner = json_get(located_res, 1);
        if (!ssg2_is_null(located)) {
            char *sid2 = ssg2_strip(session_id->str_val);
            json_t *ra = json_object();
            ssg2_jset(ra, "db", located);
            json_set_hdr(ra, "session_id", sid2);
            ssg2_jset(ra, "link_profile", owner);
            json_t *found_res = ssg2_call(c, "_read_session", ra);
            free(sid2);

            /* finally: located.close() */
            json_t *ca = json_object();
            ssg2_jset(ca, "db", located);
            json_t *close_res = ssg2_call(c, "session_db.close", ca);
            if (ssg2_is_raise(close_res)) {
                /* finally raise replaces the in-flight body raise. */
                if (found_res) json_free(found_res);
                json_free(located_res); json_free(result); json_free(db);
                json_free(session_id); json_free(prof); json_free(cur);
                return close_res; /* owned raise node, preserved */
            }
            json_free(close_res);

            if (ssg2_is_raise(found_res)) {
                json_free(located_res); json_free(result); json_free(db);
                json_free(session_id); json_free(prof); json_free(cur);
                return found_res;
            }
            json_t *found = ssg2_json_loads(found_res);
            json_free(found_res);
            if (ssg2_is_raise(found)) {
                json_free(located_res); json_free(result); json_free(db);
                json_free(session_id); json_free(prof); json_free(cur);
                return found;
            }
            if (found->type == JSON_OBJECT &&
                ssg2_truthy(json_obj_get(found, "success"))) {
                json_set(found, "profile", owner ? json_copy(owner) : json_null());
                char *s = ssg2_py_dumps(found);
                json_t *ret = json_string(s);
                free(s);
                json_free(found);
                json_free(located_res); json_free(result); json_free(db);
                json_free(session_id); json_free(prof); json_free(cur);
                return ret;
            }
            json_free(found);
        }
        json_free(located_res);
        SSG2_FREE_LOCALS();
        return result;
    }

    /* Limit clamp [1, 10].  isinstance(limit,int) admits bool and integral
     * numbers; a non-integral float coerces identically via int(). */
    long long lim = 3;
    bool ok = true;
    if (limit && limit->type == JSON_STRING) {
        bool valid = true;
        lim = ssg2_coerce_int(limit, &valid);
        if (!valid) lim = 3;
    } else {
        lim = ssg2_coerce_int(limit, &ok);
        if (!ok) lim = 3;
    }
    if (lim < 1) lim = 1;
    if (lim > 10) lim = 10;

    /* Browse shape. */
    {
        char *qblank = ssg2_strip(ssg2_str(query));
        if (!ssg2_truthy(query) || (query && query->type != JSON_STRING) ||
            qblank[0] == '\0') {
            free(qblank);
            json_t *a = json_object();
            ssg2_jset(a, "db", db);
            json_set(a, "limit", json_number((double)lim));
            ssg2_jset(a, "current_session_id", cur);
            ssg2_jset(a, "link_profile", prof);
            SSG2_FREE_LOCALS();
            return ssg2_call(c, "_list_recent_sessions", a);
        }
        free(qblank);
    }

    /* Parse role_filter. */
    json_t *role_list = json_null();
    if (role_filter && role_filter->type == JSON_STRING) {
        char *rf = ssg2_strip(role_filter->str_val);
        if (rf[0]) {
            role_list = json_array();
            char *save = NULL;
            for (char *tok = strtok_r(rf, ",", &save); tok;
                 tok = strtok_r(NULL, ",", &save)) {
                char *t = ssg2_strip(tok);
                if (t[0]) json_append(role_list, json_string(t));
                free(t);
            }
        }
        free(rf);
    }

    /* Normalise sort. */
    json_t *sort_norm = json_null();
    const char *sv = ssg2_str(sortv);
    if (sv) {
        char *st = ssg2_strip(sv);
        for (char *p = st; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(st, "newest") == 0 || strcmp(st, "oldest") == 0)
            sort_norm = json_string(st);
        free(st);
    }

    json_t *a = json_object();
    ssg2_jset(a, "db", db);
    {
        char *qs = ssg2_strip(query->str_val); /* query.strip() */
        json_set_hdr(a, "query", qs);
        free(qs);
    }
    ssg2_jset(a, "role_filter", role_list);
    json_set(a, "limit", json_number((double)lim));
    ssg2_jset(a, "sort", sort_norm);
    ssg2_jset(a, "current_session_id", cur);
    ssg2_jset(a, "link_profile", prof);
    SSG2_FREE_LOCALS();
    return ssg2_call(c, "_discover", a);
}
