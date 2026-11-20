/*
 * slack_adapter_flow6.c -- FLOW cores, wave 6, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's STILL-UNCLAIMED remainder after flow (28), flow2 (26), flow3 (12),
 * flow4 (25) and flow5 (18). Every ported name was UNCLAIMED by a CRLF-aware
 * strict PoP grep of the whole main-only repo at port time (each carries its
 * PoP annotation at its definition below). No flow4/5 function is re-ported.
 *
 * Ported (18 functions, all UNCLAIMED at their port time):
 *   _retry_after_from_exc, _standalone_post_text, slack_deps_present,
 *   _stream_key, _evict_oldest_by_ts, _trim_bot_message_timestamps,
 *   _trim_mentioned_threads, _EA_HEADER, _EA_SMART_DENY_LINE,
 *   _slash_channel_gated, _is_slack_cdn_url, _format_chunks,
 *   _team_in_install_set, _build_thread_session_key,
 *   _warn_if_inchannel_without_flat_reply, _esc, _unseen, _append_line.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa6_call(ctx, name, args). The harness wires it to a
 * recording stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow6.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - {"__obj__": attrs} is an attribute carrier (SimpleNamespace double);
 *   {"__set__": [..]} is a set carrier; {"__tuple__":..} a tuple carrier.
 * - isinstance/except families walk the full builtin exception table below.
 * - BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) PASS THROUGH every except-Exception site.
 * - Fail-closed: a missing/exhausted seam script is a RuntimeError raise
 *   spec recorded in the trace before propagation.
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

/* -- universal seam context -- */
typedef struct sa6_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa6_ctx_t;

/* -- basic helpers -- */
static bool sa6_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa6_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa6_obj_attrs(const json_t *v) {
    return sa6_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa6_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa6_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa6_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa6_is_obj(v) &&
           !sa6_is_set(v) && !sa6_is_tuple_marker(v) && !sa6_is_raise(v);
}
static bool sa6_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa6_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa6_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa6_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa6_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa6_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sa6_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa6_call(sa6_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa6_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa6_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa6_make_exc(raise);
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

/* -- full builtin exception hierarchy (CPython 3.14, quarry; aliases
 * EnvironmentError/IOError/WindowsError -> OSError, ExceptionGroup carries
 * its second base, asyncio.CancelledError is a BaseException subclass). -- */
typedef struct sa6_exc_parent { const char *name, *parent1, *parent2; } sa6_exc_parent_t;
static const sa6_exc_parent_t SA6_EXC_TABLE[] = {
    {"ArithmeticError", "Exception", ""},
    {"AssertionError", "Exception", ""},
    {"AttributeError", "Exception", ""},
    {"BaseException", "object", ""},
    {"BaseExceptionGroup", "BaseException", ""},
    {"BlockingIOError", "OSError", ""},
    {"BrokenPipeError", "ConnectionError", ""},
    {"BufferError", "Exception", ""},
    {"BytesWarning", "Warning", ""},
    {"CancelledError", "BaseException", ""},
    {"ChildProcessError", "OSError", ""},
    {"ConnectionAbortedError", "ConnectionError", ""},
    {"ConnectionError", "OSError", ""},
    {"ConnectionRefusedError", "ConnectionError", ""},
    {"ConnectionResetError", "ConnectionError", ""},
    {"DeprecationWarning", "Warning", ""},
    {"EOFError", "Exception", ""},
    {"EncodingWarning", "Warning", ""},
    {"EnvironmentError", "OSError", ""},
    {"Exception", "BaseException", ""},
    {"ExceptionGroup", "BaseExceptionGroup", "Exception"},
    {"FileExistsError", "OSError", ""},
    {"FileNotFoundError", "OSError", ""},
    {"FloatingPointError", "ArithmeticError", ""},
    {"FutureWarning", "Warning", ""},
    {"GeneratorExit", "BaseException", ""},
    {"IOError", "OSError", ""},
    {"ImportError", "Exception", ""},
    {"ImportWarning", "Warning", ""},
    {"IndentationError", "SyntaxError", ""},
    {"IndexError", "LookupError", ""},
    {"InterruptedError", "OSError", ""},
    {"IsADirectoryError", "OSError", ""},
    {"KeyError", "LookupError", ""},
    {"KeyboardInterrupt", "BaseException", ""},
    {"LookupError", "Exception", ""},
    {"MemoryError", "Exception", ""},
    {"ModuleNotFoundError", "ImportError", ""},
    {"NameError", "Exception", ""},
    {"NotADirectoryError", "OSError", ""},
    {"NotImplementedError", "RuntimeError", ""},
    {"OSError", "Exception", ""},
    {"OverflowError", "ArithmeticError", ""},
    {"PendingDeprecationWarning", "Warning", ""},
    {"PermissionError", "OSError", ""},
    {"ProcessLookupError", "OSError", ""},
    {"PythonFinalizationError", "RuntimeError", ""},
    {"RecursionError", "RuntimeError", ""},
    {"ReferenceError", "Exception", ""},
    {"ResourceWarning", "Warning", ""},
    {"RuntimeError", "Exception", ""},
    {"RuntimeWarning", "Warning", ""},
    {"StopAsyncIteration", "Exception", ""},
    {"StopIteration", "Exception", ""},
    {"SyntaxError", "Exception", ""},
    {"SyntaxWarning", "Warning", ""},
    {"SystemError", "Exception", ""},
    {"SystemExit", "BaseException", ""},
    {"TabError", "IndentationError", ""},
    {"TimeoutError", "OSError", ""},
    {"TypeError", "Exception", ""},
    {"UnboundLocalError", "NameError", ""},
    {"UnicodeDecodeError", "UnicodeError", ""},
    {"UnicodeEncodeError", "UnicodeError", ""},
    {"UnicodeError", "ValueError", ""},
    {"UnicodeTranslateError", "UnicodeError", ""},
    {"UnicodeWarning", "Warning", ""},
    {"UserWarning", "Warning", ""},
    {"ValueError", "Exception", ""},
    {"Warning", "Exception", ""},
    {"WindowsError", "OSError", ""},
    {"ZeroDivisionError", "ArithmeticError", ""},
    {"_IncompleteInputError", "SyntaxError", ""},
};
#define SA6_EXC_TABLE_LEN (sizeof(SA6_EXC_TABLE) / sizeof(SA6_EXC_TABLE[0]))

static const sa6_exc_parent_t *sa6_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA6_EXC_TABLE_LEN; i++)
        if (strcmp(SA6_EXC_TABLE[i].name, name) == 0) return &SA6_EXC_TABLE[i];
    return NULL;
}
typedef struct sa6_chain { char names[64][96]; size_t n; } sa6_chain_t;
static bool sa6_chain_has(const sa6_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa6_chain_add(sa6_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa6_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa6_chain_walk(sa6_chain_t *c, const char *name) {
    if (!name || !name[0] || sa6_chain_has(c, name)) return;
    sa6_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa6_exc_parent_t *be = sa6_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";  /* synthetic -> Exception */
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa6_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa6_chain_walk(c, p2);
}
static bool sa6_exc_family(const json_t *r, const char *family) {
    if (!sa6_is_raise(r)) return false;
    sa6_chain_t c; c.n = 0;
    sa6_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa6_chain_has(&c, family);
}
static bool sa6_raise_passes(const json_t *r) {
    return sa6_is_raise(r) && !sa6_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa6_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa6_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa6_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa6_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa6_lower(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (out) {
        for (size_t i = 0; i < n; i++)
            out[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] - 'A' + 'a') : s[i];
        out[n] = '\0';
    }
    return out;
}
static char *sa6_lower_strip(const char *s) {
    char *lo = sa6_lower(s);
    char *st = sa6_strip(lo);
    free(lo);
    return st;
}
static bool sa6_in_set_str(const char *const *items, size_t n, const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, items[i]) == 0) return true;
    return false;
}
/* Python str() of a scalar (ASCII domain: str/int/bool/None only). */
static char *sa6_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa6_strdup("None");
    if (v->type == JSON_STRING) return sa6_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa6_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa6_strdup(buf);
    }
    return sa6_strdup("");
}
/* dict.get(key, default): absent -> default; present-null -> JSON_NULL node. */
static json_t *sa6_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static const char *sa6_pytype_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) return (v->num_val == floor(v->num_val)) ? "int" : "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    if (sa6_is_obj(v)) return "types.SimpleNamespace";
    if (sa6_is_set(v)) return "set";
    return "dict";
}
static json_t *sa6_no_get(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sa6_pytype_name(v));
    return sa6_err("AttributeError", msg);
}
static json_t *sa6_no_startswith(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'startswith'", sa6_pytype_name(v));
    return sa6_err("AttributeError", msg);
}

/* ====================================================================== */

/* -- attribute / collection helpers (added for wave 6) -- */
/* getattr(obj, key) with default None -- obj is an {"__obj__":{...}} carrier
 * or a missing/NULL value.  Returns a fresh node (JSON_NULL when absent). */
static json_t *sa6_getattr(const json_t *obj, const char *key) {
    json_t *attrs = sa6_obj_attrs(obj);
    if (!attrs) return json_null();
    json_t *v = json_obj_get(attrs, key);
    return json_copy(v ? v : json_null());
}
/* Required attribute access -- raises AttributeError when absent (matches a
 * bare `obj.attr`). type_name names the Python carrier type in the message. */
static json_t *sa6_getattr_req(const json_t *obj, const char *key, const char *type_name) {
    json_t *attrs = sa6_obj_attrs(obj);
    json_t *v = attrs ? json_obj_get(attrs, key) : NULL;
    if (!v) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 type_name ? type_name : "types.SimpleNamespace", key);
        return sa6_err("AttributeError", msg);
    }
    return json_copy(v);
}
/* Python `s in collection` for a string s: JSON array, {"__set__":[..]} carrier,
 * or a plain dict (membership tests keys). */
static bool sa6_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa6_is_set(coll)) coll = json_obj_get(coll, "__set__");
    if (!coll) return false;
    if (coll->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(coll); i++) {
            const char *m = sa6_str(json_get(coll, i));
            if (m && strcmp(m, s) == 0) return true;
        }
        return false;
    }
    if (coll->type == JSON_OBJECT) return json_obj_get(coll, s) != NULL;
    return false;
}

/* ====================================================================== */

/* -- wave-6 string helpers -- */
static char *sa6_rstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && sa6_is_ws((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *sa6_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa6_strdup(s);
    size_t cap = strlen(s) + 1, used = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    const char *p = s, *q;
    while ((q = strstr(p, from)) != NULL) {
        size_t pre = (size_t)(q - p);
        size_t need = used + pre + tl + 1;
        if (need > cap) { cap = need * 2; char *nb = (char *)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
        memcpy(out + used, p, pre); used += pre;
        memcpy(out + used, to, tl); used += tl;
        p = q + fl;
    }
    size_t tail = strlen(p);
    size_t need = used + tail + 1;
    if (need > cap) { char *nb = (char *)realloc(out, need); if (!nb) { free(out); return NULL; } out = nb; }
    memcpy(out + used, p, tail); used += tail;
    out[used] = '\0';
    return out;
}
static bool sa6_endswith(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t a = strlen(s), b = strlen(suffix);
    return b <= a && strcmp(s + a - b, suffix) == 0;
}
/* len() of a JSON array or object (dict) */
static size_t sa6_len(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_ARRAY) return json_len(v);
    if (v->type == JSON_OBJECT) return v->c.count;
    return 0;
}

/* strip only a trailing run of one char (Python str.rstrip(chars)) */
static char *sa6_rstrip_chars(const char *s, char ch) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == ch) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* PoP: sa6_retry_after_from_exc @ plugins/platforms/slack/adapter.py:_retry_after_from_exc */
/* module: parse_retry_after_seconds(getattr(getattr(e,"response",None),"headers",None)). */
static json_t *sa6_retry_after_from_exc(sa6_ctx_t *ctx, const json_t *args) {
    json_t *e = json_obj_get(args, "e");
    json_t *resp = sa6_getattr(e, "response");
    json_t *hdrs = sa6_getattr(resp, "headers");
    json_free(resp);
    json_t *a = json_object();
    json_set(a, "headers", hdrs ? hdrs : json_null());
    return sa6_call(ctx, "parse_retry_after_seconds", a);
}

/* PoP: sa6_standalone_post_text @ plugins/platforms/slack/adapter.py:_standalone_post_text */
/* module: chat.postMessage via SDK client, kwargs then response payload. */
static json_t *sa6_standalone_post_text(sa6_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(json_obj_get(args, "chat_id")));
    json_set(a, "text", json_copy(json_obj_get(args, "text")));
    json_set(a, "unfurl_kwargs", json_copy(json_obj_get(args, "unfurl_kwargs")));
    json_set(a, "thread_id", json_copy(json_obj_get(args, "thread_id")));
    json_t *kw = sa6_call(ctx, "standalone_post_kwargs", a);
    if (sa6_is_raise(kw)) return kw;
    json_t *resp = sa6_call(ctx, "chat_postMessage", json_copy(kw));
    if (sa6_is_raise(resp)) { json_free(kw); return resp; }
    json_t *b = json_object();
    json_set(b, "response", json_copy(resp));
    json_free(resp); json_free(kw);
    return sa6_call(ctx, "slack_response_payload", b);
}

/* PoP: sa6_slack_deps_present @ plugins/platforms/slack/adapter.py:slack_deps_present */
/* module: passive import-availability probe (seed_constants -> SLACK_AVAILABLE=False). */
static json_t *sa6_slack_deps_present(sa6_ctx_t *ctx, const json_t *args) {
    (void)ctx; (void)args;
    return json_bool(false);
}

/* PoP: sa6_stream_key @ plugins/platforms/slack/adapter.py:_stream_key */
/* instance: (team, chat, thread_ts) via four ordered seams, "" fallbacks. */
static json_t *sa6_stream_key(sa6_ctx_t *ctx, const json_t *args) {
    json_t *chat = json_obj_get(args, "chat_id");
    json_t *meta = json_obj_get(args, "metadata");
    json_t *md = (meta && sa6_truthy(meta)) ? json_copy(meta) : json_object();
    json_t *a = json_object(); json_set(a, "metadata", json_copy(md));
    json_t *team = sa6_call(ctx, "metadata_team_id", a);
    if (sa6_is_raise(team)) { json_free(md); return team; }
    if (!sa6_truthy(team)) {
        json_free(team);
        json_t *b = json_object(); json_set(b, "chat_id", json_copy(chat ? chat : json_null()));
        team = sa6_call(ctx, "scope_id_for_chat", b);
        if (sa6_is_raise(team)) { json_free(md); return team; }
        if (!sa6_truthy(team)) { json_free(team); team = json_string(""); }
    }
    json_t *c = json_object();
    json_set(c, "reply_to", json_null());
    json_set(c, "metadata", json_copy(md));
    json_t *tts = sa6_call(ctx, "resolve_thread_ts", c);
    if (sa6_is_raise(tts)) { json_free(md); json_free(team); return tts; }
    char *ts = sa6_truthy(tts) ? sa6_pystr(tts) : sa6_strdup("");
    json_t *d = json_object();
    json_set(d, "team_id", json_copy(team));
    json_set(d, "chat_id", json_copy(chat ? chat : json_null()));
    json_set(d, "thread_ts", json_string(ts ? ts : ""));
    free(ts); json_free(md); json_free(team); json_free(tts);
    return sa6_call(ctx, "workspace_thread_key", d);
}

/* PoP: sa6_evict_oldest_by_ts @ plugins/platforms/slack/adapter.py:_evict_oldest_by_ts */
/* instance: over cap -> discard (len - cap//2) oldest via seam. */
static json_t *sa6_evict_oldest_by_ts(sa6_ctx_t *ctx, const json_t *args) {
    json_t *entries = json_obj_get(args, "entries");
    json_t *capv = json_obj_get(args, "cap");
    double capd = (capv && capv->type == JSON_NUMBER) ? capv->num_val : 0.0;
    size_t n = sa6_len(entries);
    if ((double)n <= capd) return json_null();
    long long cnt = (long long)n - (long long)floor(capd / 2.0);
    json_t *a = json_object();
    json_set(a, "entries", json_copy(entries ? entries : json_null()));
    json_set(a, "n", json_number((double)cnt));
    json_t *r = sa6_call(ctx, "discard_oldest_by_thread_ts", a);
    if (sa6_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: sa6_trim_bot_message_timestamps @ plugins/platforms/slack/adapter.py:_trim_bot_message_timestamps */
/* instance: evict _bot_message_ts down to half _BOT_TS_MAX. */
static json_t *sa6_trim_bot_message_timestamps(sa6_ctx_t *ctx, const json_t *args) {
    (void)args;
    double cap = json_get_num(ctx->state, "_BOT_TS_MAX", 5000.0);
    json_t *e = json_obj_get(ctx->state, "_bot_message_ts");
    json_t *a = json_object();
    json_set(a, "entries", e ? json_copy(e) : json_array());
    json_set(a, "cap", json_number(cap));
    json_t *r = sa6_call(ctx, "evict_oldest_by_ts", a);
    if (sa6_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: sa6_trim_mentioned_threads @ plugins/platforms/slack/adapter.py:_trim_mentioned_threads */
/* instance: over _MENTIONED_THREADS_MAX -> discard (max//2) oldest via seam. */
static json_t *sa6_trim_mentioned_threads(sa6_ctx_t *ctx, const json_t *args) {
    (void)args;
    double cap = json_get_num(ctx->state, "_MENTIONED_THREADS_MAX", 5000.0);
    json_t *e = json_obj_get(ctx->state, "_mentioned_threads");
    size_t n = sa6_len(e);
    if ((double)n <= cap) return json_null();
    long long half = (long long)floor(cap / 2.0);
    json_t *a = json_object();
    json_set(a, "entries", e ? json_copy(e) : json_array());
    json_set(a, "n", json_number((double)half));
    json_t *r = sa6_call(ctx, "discard_oldest_by_thread_ts", a);
    if (sa6_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: sa6_ea_header @ plugins/platforms/slack/adapter.py:_EA_HEADER */
/* instance: localized warning header via t() seam. */
static json_t *sa6_ea_header(sa6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object(); json_set(a, "key", json_string("gateway.exec_approval.header"));
    json_t *r = sa6_call(ctx, "t", a);
    if (sa6_is_raise(r)) return r;
    char *s = sa6_pystr(r); json_free(r);
    char out[2048];
    snprintf(out, sizeof out, ":warning: *%s*\n", s ? s : "");
    free(s);
    return json_string(out);
}

/* PoP: sa6_ea_smart_deny_line @ plugins/platforms/slack/adapter.py:_EA_SMART_DENY_LINE */
/* instance: localized smart-deny line; bold the label before the first colon. */
static json_t *sa6_ea_smart_deny_line(sa6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object(); json_set(a, "key", json_string("gateway.exec_approval.smart_deny_line"));
    json_t *r = sa6_call(ctx, "t", a);
    if (sa6_is_raise(r)) return r;
    char *line = sa6_pystr(r); json_free(r);
    const char *colon = line ? strchr(line, ':') : NULL;
    char out[2048];
    if (colon) {
        size_t lab = (size_t)(colon - line);
        snprintf(out, sizeof out, "\n*%.*s:*%s", (int)lab, line, colon + 1);
    } else {
        snprintf(out, sizeof out, "\n%s", line ? line : "");
    }
    free(line);
    return json_string(out);
}

/* PoP: sa6_slash_channel_gated @ plugins/platforms/slack/adapter.py:_slash_channel_gated */
/* instance: ignored channels blocked; outside allowed only non-DM gated. */
static json_t *sa6_slash_channel_gated(sa6_ctx_t *ctx, const json_t *args) {
    json_t *cid = json_obj_get(args, "channel_id");
    json_t *a = json_object(); json_set(a, "chat_id", json_copy(cid ? cid : json_null()));
    json_t *ig = sa6_call(ctx, "is_ignored_channel", a);
    if (sa6_is_raise(ig)) return ig;
    if (sa6_truthy(ig)) { json_free(ig); return json_bool(true); }
    json_free(ig);
    json_t *ac = sa6_call(ctx, "slack_allowed_channels", json_object());
    if (sa6_is_raise(ac)) return ac;
    bool allowed = sa6_truthy(ac);
    char *cs = sa6_pystr(cid);
    bool startsD = cs && cs[0] == 'D';
    bool notin = !sa6_collection_has_str(ac, cs);
    bool res = allowed && !startsD && notin;
    free(cs); json_free(ac);
    return json_bool(res);
}

/* PoP: sa6_is_slack_cdn_url @ plugins/platforms/slack/adapter.py:_is_slack_cdn_url */
/* classmethod: https URL on a Slack CDN host; urlparse seam, ValueError->False. */
static json_t *sa6_is_slack_cdn_url(sa6_ctx_t *ctx, const json_t *args) {
    json_t *url = json_obj_get(args, "url");
    json_t *a = json_object(); json_set(a, "url", json_copy(url ? url : json_null()));
    json_t *p = sa6_call(ctx, "urlparse", a);
    if (sa6_is_raise(p)) {
        if (sa6_exc_family(p, "Exception")) { json_free(p); return json_bool(false); }
        return p;
    }
    json_t *hn = sa6_getattr(p, "hostname");
    char *raw = sa6_truthy(hn) ? sa6_pystr(hn) : sa6_strdup("");
    char *lo = sa6_lower(raw);
    char *host = sa6_rstrip_chars(lo, '.');
    json_free(hn); free(raw); free(lo);
    json_t *sc = sa6_getattr(p, "scheme");
    const char *scheme = sa6_str(sc);
    bool https = scheme && strcmp(scheme, "https") == 0;
    bool nonempty = host && host[0] != '\0';
    static const char *EXACT[] = {"slack.com", "slack-files.com"};
    static const char *SUFFIX[] = {".slack.com", ".slack-files.com"};
    bool in = false;
    for (size_t i = 0; i < 2; i++) if (host && strcmp(host, EXACT[i]) == 0) in = true;
    for (size_t i = 0; i < 2; i++) if (sa6_endswith(host, SUFFIX[i])) in = true;
    bool res = nonempty && https && in;
    free(host); json_free(sc); json_free(p);
    return json_bool(res);
}

/* PoP: sa6_format_chunks @ plugins/platforms/slack/adapter.py:_format_chunks */
/* instance: mrkdwn format then truncate; empty truncate falls back to [formatted]. */
static json_t *sa6_format_chunks(sa6_ctx_t *ctx, const json_t *args) {
    json_t *content = json_obj_get(args, "content");
    json_t *a = json_object(); json_set(a, "text", json_copy(content ? content : json_null()));
    json_t *fmt = sa6_call(ctx, "format_message", a);
    if (sa6_is_raise(fmt)) return fmt;
    double mx = json_get_num(ctx->state, "MAX_MESSAGE_LENGTH", 39000.0);
    json_t *b = json_object();
    json_set(b, "text", json_copy(fmt));
    json_set(b, "max", json_number(mx));
    json_t *tr = sa6_call(ctx, "truncate_message", b);
    if (sa6_is_raise(tr)) { json_free(fmt); return tr; }
    if (sa6_truthy(tr)) { json_free(fmt); return tr; }
    json_free(tr);
    json_t *out = json_array(); json_append(out, json_copy(fmt)); json_free(fmt);
    return out;
}

/* PoP: sa6_team_in_install_set @ plugins/platforms/slack/adapter.py:_team_in_install_set */
/* instance: pass a team id only when present in _team_clients. */
static json_t *sa6_team_in_install_set(sa6_ctx_t *ctx, const json_t *args) {
    json_t *tid = json_obj_get(args, "team_id");
    if (!sa6_truthy(tid)) return json_string("");
    json_t *inst = json_obj_get(ctx->state, "_team_clients");
    if (!sa6_truthy(inst)) inst = NULL;
    char *s = sa6_pystr(tid);
    bool has = sa6_collection_has_str(inst, s);
    free(s);
    return has ? json_copy(tid) : json_string("");
}

/* PoP: sa6_build_thread_session_key @ plugins/platforms/slack/adapter.py:_build_thread_session_key */
/* instance: source->session_key seam chain; no store or Exception -> None. */
static json_t *sa6_build_thread_session_key(sa6_ctx_t *ctx, const json_t *args) {
    if (!sa6_truthy(json_obj_get(ctx->state, "_session_store"))) return json_null();
    json_t *a = json_object();
    json_set(a, "channel_id", json_copy(json_obj_get(args, "channel_id")));
    json_set(a, "thread_ts", json_copy(json_obj_get(args, "thread_ts")));
    json_set(a, "user_id", json_copy(json_obj_get(args, "user_id")));
    json_set(a, "team_id", json_string(json_get_str(args, "team_id", "")));
    json_set(a, "chat_type", json_string(json_get_str(args, "chat_type", "group")));
    json_t *src = sa6_call(ctx, "thread_session_source", a);
    if (sa6_is_raise(src)) {
        if (sa6_exc_family(src, "Exception")) { json_free(src); return json_null(); }
        return src;
    }
    json_t *b = json_object(); json_set(b, "source", json_copy(src)); json_free(src);
    json_t *key = sa6_call(ctx, "source_session_key", b);
    if (sa6_is_raise(key)) {
        if (sa6_exc_family(key, "Exception")) { json_free(key); return json_null(); }
        return key;
    }
    return key;
}

/* PoP: sa6_warn_if_inchannel_without_flat_reply @ plugins/platforms/slack/adapter.py:_warn_if_inchannel_without_flat_reply */
/* instance: warn (logger.warning seam) on in_channel cron without flat reply. */
static json_t *sa6_warn_if_inchannel_without_flat_reply(sa6_ctx_t *ctx, const json_t *args) {
    json_t *tn = json_obj_get(args, "team_name");
    json_t *surf = sa6_call(ctx, "cron_continuable_surface", json_object());
    if (sa6_is_raise(surf)) {
        if (sa6_exc_family(surf, "Exception")) { json_free(surf); return json_null(); }
        return surf;
    }
    const char *sv = sa6_str(surf);
    bool in_ch = sv && strcmp(sv, "in_channel") == 0;
    json_free(surf);
    if (!in_ch) return json_null();
    json_t *extra = json_obj_get(ctx->state, "extra");
    json_t *rit = sa6_get(extra, "reply_in_thread", json_bool(true));
    bool warn = sa6_truthy(rit);
    json_free(rit);
    if (!warn) return json_null();
    json_t *b = json_object();
    json_t *arr = json_array(); json_append(arr, json_copy(tn ? tn : json_null()));
    json_set(b, "args", arr);
    json_t *lr = sa6_call(ctx, "logger.warning", b);
    if (sa6_is_raise(lr)) {
        if (sa6_exc_family(lr, "Exception")) { json_free(lr); return json_null(); }
        return lr;
    }
    json_free(lr);
    return json_null();
}

/* PoP: sa6_esc @ plugins/platforms/slack/adapter.py:_esc */
/* nested: HTML-escape &, <, > in that order. */
static json_t *sa6_esc(sa6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = sa6_str(json_obj_get(args, "text"));
    if (!text) return sa6_err("AttributeError", "'NoneType' object has no attribute 'replace'");
    char *a = sa6_replace(text, "&", "&amp;");
    char *b = sa6_replace(a, "<", "&lt;");
    char *c = sa6_replace(b, ">", "&gt;");
    free(a); free(b);
    json_t *out = json_string(c ? c : ""); free(c);
    return out;
}

/* PoP: sa6_unseen @ plugins/platforms/slack/adapter.py:_unseen */
/* nested: piece not in base and not in any extras entry. */
static json_t *sa6_unseen(sa6_ctx_t *ctx, const json_t *args) {
    const char *piece = sa6_str(json_obj_get(args, "piece"));
    const char *base = sa6_str(json_obj_get(args, "base"));
    json_t *extras = json_obj_get(ctx->state, "extras");
    bool res = true;
    if (base && piece && strstr(base, piece) != NULL) res = false;
    if (res && extras && extras->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(extras); i++) {
            const char *e = sa6_str(json_get(extras, i));
            if (e && piece && strstr(e, piece) != NULL) { res = false; break; }
        }
    }
    return json_bool(res);
}

/* PoP: sa6_append_line @ plugins/platforms/slack/adapter.py:_append_line */
/* nested: append a rstripped, prefixed+bulleted line into closure `parts`. */
static json_t *sa6_append_line(sa6_ctx_t *ctx, const json_t *args) {
    json_t *text = json_obj_get(args, "text");
    const char *ts = sa6_str(text);
    if (!ts) return json_null();
    char *st = sa6_strip(ts);
    if (!st || st[0] == '\0') { free(st); return json_null(); }
    free(st);
    double qd = json_get_num(args, "quote_depth", 0.0);
    const char *bullet = json_get_str(args, "bullet", "");
    char *prefix;
    if (qd != 0.0) {
        size_t n = (size_t)qd;
        prefix = (char *)malloc(n + 2);
        for (size_t i = 0; i < n; i++) prefix[i] = '>';
        prefix[n] = ' '; prefix[n + 1] = '\0';
    } else prefix = sa6_strdup("");
    size_t len = strlen(prefix) + strlen(bullet) + strlen(ts) + 1;
    char *line = (char *)malloc(len);
    snprintf(line, len, "%s%s%s", prefix, bullet, ts);
    char *rs = sa6_rstrip(line);
    json_t *parts = json_obj_get(ctx->state, "parts");
    if (!parts || parts->type != JSON_ARRAY) { parts = json_array(); json_set(ctx->state, "parts", parts); }
    json_append(parts, json_string(rs ? rs : ""));
    free(rs); free(line); free(prefix);
    return json_null();
}
