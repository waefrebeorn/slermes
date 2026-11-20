/*
 * slack_adapter_flow3.c -- FLOW cores, wave 3, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's STILL-UNCLAIMED pure leaves, boolean gates, key builders,
 * exception classifier and elapsed-status formatter after
 * slack_adapter_flow (28 leaves) and slack_adapter_flow2 (26 flows).
 *
 * Ported (all UNCLAIMED by the strict PoP grep at port time):
 *   _mrkdwn_insensitive, _workspace_thread_key, _agent_view_context_key,
 *   _thread_cache_key, _thread_root_message, _default_status_text,
 *   _is_block_payload_rejection, _strip_stream_cursor, _extra_flag,
 *   _slack_require_mention, _extra_or_env_flag, _extra_or_env_channel_set.
 *
 * Decision sequences only: every injected callable (t, time.monotonic,
 * _get_scoped_secret, _extra_or_secret, response.get) enters through ONE
 * universal seam sa3_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sa3_-prefixed.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow3.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is a {"__raise__": cls, "message": str(exc)} object.
 * - Attribute carriers travel as {"__obj__": {attrs}}; a plain JSON object IS
 *   a Python dict. Sets travel as {"__set__": [members]} (C keeps members
 *   sorted; the oracle canon sorts sets). Tuple INPUTS use {"__tuple__":...}.
 * - Exception doubles (args.exc.__exc__) may carry a callable attribute shaped
 *   {"__callable__": name}; calling it fires that seam with {"code": <arg>}.
 *   isinstance/except families walk the FULL builtin exception table below.
 * - state.extra is the plain dict behind self.config.extra.
 * - str()-computed positions carry str/int/bool/None only; time.monotonic
 *   scripts carry NON-INTEGRAL floats; exception kinds are non-KeyError so
 *   str(exc) == message. Document domain guards.
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
typedef struct sa3_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa3_ctx_t;

/* -- basic helpers -- */
static bool sa3_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool sa3_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}

static json_t *sa3_obj_attrs(const json_t *v) {
    return sa3_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}

static bool sa3_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}

static bool sa3_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}

static bool sa3_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa3_is_obj(v) &&
           !sa3_is_set(v) && !sa3_is_tuple_marker(v);
}

static bool sa3_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa3_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}

static const char *sa3_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static char *sa3_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static json_t *sa3_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}

static json_t *sa3_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sa3_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa3_call(sa3_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa3_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa3_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa3_make_exc(raise);
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

/* -- full builtin exception hierarchy (CPython 3.14, generated from the
 * quarry interpreter; aliases EnvironmentError/IOError/WindowsError map to
 * OSError, ExceptionGroup carries its second base, asyncio.CancelledError
 * is a BaseException subclass). parent2 == "" means single inheritance. -- */
typedef struct sa3_exc_parent { const char *name, *parent1, *parent2; } sa3_exc_parent_t;

static const sa3_exc_parent_t SA3_EXC_TABLE[] = {
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
#define SA3_EXC_TABLE_LEN (sizeof(SA3_EXC_TABLE) / sizeof(SA3_EXC_TABLE[0]))

static const sa3_exc_parent_t *sa3_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA3_EXC_TABLE_LEN; i++)
        if (strcmp(SA3_EXC_TABLE[i].name, name) == 0) return &SA3_EXC_TABLE[i];
    return NULL;
}

typedef struct sa3_chain { char names[64][96]; size_t n; } sa3_chain_t;

static bool sa3_chain_has(const sa3_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}

static void sa3_chain_add(sa3_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa3_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}

static void sa3_chain_walk(sa3_chain_t *c, const char *name) {
    if (!name || !name[0] || sa3_chain_has(c, name)) return;
    sa3_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa3_exc_parent_t *be = sa3_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";  /* synthetic -> Exception */
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa3_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa3_chain_walk(c, p2);
}

/* Does a raise descriptor's kind belong to the given exception family? */
static bool sa3_exc_family(const json_t *r, const char *family) {
    if (!sa3_is_raise(r)) return false;
    sa3_chain_t c; c.n = 0;
    sa3_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa3_chain_has(&c, family);
}

/* BaseException-only kinds pass through an except-Exception site. */
static bool sa3_raise_passes(const json_t *r) {
    return sa3_is_raise(r) && !sa3_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa3_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}

static char *sa3_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa3_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa3_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

static char *sa3_rstrip(const char *s) {
    if (!s) return NULL;
    size_t b = strlen(s);
    while (b > 0 && sa3_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b + 1);
    if (out) { memcpy(out, s, b); out[b] = '\0'; }
    return out;
}

static char *sa3_lower(const char *s) {
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

static char *sa3_strip_lower(const char *s) {
    char *st = sa3_strip(s);
    char *lo = sa3_lower(st);
    free(st);
    return lo;
}

static bool sa3_contains(const char *hay, const char *needle) {
    if (!hay || !needle) return false;
    return strstr(hay, needle) != NULL;
}

static bool sa3_in_set_str(const char *const *items, size_t n, const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, items[i]) == 0) return true;
    return false;
}

/* Python str() of a scalar value (ASCII scope: str/int/bool/None only). */
static char *sa3_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa3_strdup("None");
    if (v->type == JSON_STRING) return sa3_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa3_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa3_strdup(buf);
    }
    return sa3_strdup("");  /* containers outside the str() domain */
}

/* -- set carrier helpers (C keeps members sorted) -- */
static json_t *sa3_set_new(void) {
    json_t *o = json_object();
    json_set(o, "__set__", json_array());
    return o;
}

static void sa3_set_add_str(json_t *set, const char *s) {
    if (!set || !s || !s[0]) return;
    json_t *arr = json_obj_get(set, "__set__");
    if (!arr) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *e = sa3_str(json_get(arr, i));
        if (e && strcmp(e, s) == 0) return;
    }
    json_append(arr, json_string(s));
    /* insertion sort by string value */
    size_t n = json_len(arr);
    for (size_t i = 1; i < n; i++) {
        size_t j = i;
        while (j > 0) {
            const char *a = sa3_str(json_get(arr, j - 1));
            const char *b = sa3_str(json_get(arr, j));
            if (a && b && strcmp(a, b) <= 0) break;
            /* swap via copies (json_array_set frees the displaced element) */
            json_t *ta = json_copy(json_get(arr, j - 1));
            json_t *tb = json_copy(json_get(arr, j));
            json_array_set(arr, j - 1, tb);
            json_array_set(arr, j, ta);
            j--;
        }
    }
}

/* -- PoP: sa3_mrkdwn_insensitive @ plugins/platforms/slack/adapter.py:_mrkdwn_insensitive -- */
/* " ".join(re.sub(r"[*_~]","",text).split()) */
static json_t *sa3_mrkdwn_insensitive(sa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    if (!text) text = "";
    size_t n = strlen(text);
    char *filtered = (char *)malloc(n + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = text[i];
        if (c == '*' || c == '_' || c == '~') continue;
        filtered[k++] = c;
    }
    filtered[k] = '\0';
    /* split on ASCII whitespace, join with one space */
    json_t *words = json_array();
    size_t i = 0;
    while (i < k) {
        while (i < k && sa3_is_ws((unsigned char)filtered[i])) i++;
        size_t a = i;
        while (i < k && !sa3_is_ws((unsigned char)filtered[i])) i++;
        if (i > a) {
            char *w = (char *)malloc(i - a + 1);
            memcpy(w, filtered + a, i - a); w[i - a] = '\0';
            json_append(words, json_string(w));
            free(w);
        }
    }
    size_t total = 0;
    for (size_t j = 0; j < json_len(words); j++)
        total += strlen(sa3_str(json_get(words, j)) ? sa3_str(json_get(words, j)) : "") + (j ? 1 : 0);
    char *out = (char *)malloc(total + 1);
    size_t p = 0;
    for (size_t j = 0; j < json_len(words); j++) {
        if (j) out[p++] = ' ';
        const char *w = sa3_str(json_get(words, j));
        size_t wl = strlen(w); memcpy(out + p, w, wl); p += wl;
    }
    out[p] = '\0';
    json_t *ret = json_string(out);
    free(out); free(filtered); json_free(words);
    return ret;
}

/* -- PoP: sa3_workspace_thread_key @ plugins/platforms/slack/adapter.py:_workspace_thread_key -- */
static json_t *sa3_workspace_thread_key(sa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ch = json_obj_get(args, "channel_id");
    const json_t *ts = json_obj_get(args, "thread_ts");
    if (!sa3_truthy(ch) || !sa3_truthy(ts)) return json_null();
    const json_t *tm = json_obj_get(args, "team_id");
    char *team_s = sa3_truthy(tm) ? sa3_pystr(tm) : sa3_strdup("");
    char *ch_s = sa3_pystr(ch), *ts_s = sa3_pystr(ts);
    json_t *arr = json_array();
    json_append(arr, json_string(team_s));
    json_append(arr, json_string(ch_s));
    json_append(arr, json_string(ts_s));
    free(team_s); free(ch_s); free(ts_s);
    return arr;
}

/* -- PoP: sa3_agent_view_context_key @ plugins/platforms/slack/adapter.py:_agent_view_context_key -- */
static json_t *sa3_agent_view_context_key(sa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *tm = json_obj_get(args, "team_id");
    const json_t *usr = json_obj_get(args, "user_id");
    if (!sa3_truthy(tm) || !sa3_truthy(usr)) return json_null();
    char *a = sa3_pystr(tm), *b = sa3_pystr(usr);
    json_t *arr = json_array();
    json_append(arr, json_string(a));
    json_append(arr, json_string(b));
    free(a); free(b);
    return arr;
}

/* -- PoP: sa3_thread_cache_key @ plugins/platforms/slack/adapter.py:_thread_cache_key -- */
static json_t *sa3_thread_cache_key(sa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *a = sa3_pystr(json_obj_get(args, "channel_id"));
    char *b = sa3_pystr(json_obj_get(args, "thread_ts"));
    char *c = sa3_pystr(json_obj_get(args, "team_id"));
    size_t n = strlen(a) + strlen(b) + strlen(c) + 3;
    char *out = (char *)malloc(n);
    snprintf(out, n, "%s:%s:%s", a, b, c);
    json_t *ret = json_string(out);
    free(out); free(a); free(b); free(c);
    return ret;
}

/* Python == for the scalar ts keys used here. */
static bool sa3_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return false;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
        return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL)
        return a->bool_val == b->bool_val;
    if (a->type == JSON_NULL && b->type == JSON_NULL) return true;
    return false;
}

/* -- PoP: sa3_thread_root_message @ plugins/platforms/slack/adapter.py:_thread_root_message -- */
static json_t *sa3_thread_root_message(sa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *msgs = json_obj_get(args, "messages");
    const json_t *want = json_obj_get(args, "thread_ts");
    if (!msgs || msgs->type != JSON_ARRAY) return json_null();
    json_t *empty = json_string("");
    for (size_t i = 0; i < json_len(msgs); i++) {
        const json_t *m = json_get(msgs, i);
        if (!m || m->type != JSON_OBJECT) {
            const char *tn = "object";
            if (m) {
                if (m->type == JSON_NUMBER) tn = "int";
                else if (m->type == JSON_STRING) tn = "str";
                else if (m->type == JSON_BOOL) tn = "bool";
                else if (m->type == JSON_NULL) tn = "NoneType";
                else if (m->type == JSON_ARRAY) tn = "list";
            }
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tn);
            json_free(empty);
            return sa3_err("AttributeError", msg);
        }
        const json_t *v = json_obj_get(m, "ts");
        const json_t *mts = v ? v : empty;
        if (sa3_eq(mts, want)) { json_free(empty); return json_copy(m); }
    }
    json_free(empty);
    return json_null();
}

/* -- PoP: sa3_default_status_text @ plugins/platforms/slack/adapter.py:_default_status_text -- */
static json_t *sa3_default_status_text(sa3_ctx_t *ctx, const json_t *args) {
    long long elapsed = 0;
    const json_t *started = json_obj_get(args, "started");
    if (started && started->type != JSON_NULL) {
        json_t *mono = sa3_call(ctx, "time.monotonic", json_object());
        if (sa3_is_raise(mono)) return mono;
        double st = (started->type == JSON_NUMBER) ? started->num_val : 0.0;
        double mo = (mono->type == JSON_NUMBER) ? mono->num_val : 0.0;
        elapsed = (long long)(mo - st);  /* int() truncates toward zero */
        json_free(mono);
    }
    json_t *ta = json_object();
    if (elapsed < 30) {
        json_set(ta, "key", json_string("platform.slack.status.thinking"));
    } else {
        long long mins = elapsed / 60, secs = elapsed % 60;  /* elapsed >= 30 > 0 */
        char buf[64];
        if (mins) snprintf(buf, sizeof buf, "%lldm%02llds", mins, secs);
        else snprintf(buf, sizeof buf, "%llds", secs);
        json_set(ta, "key", json_string("platform.slack.status.still_working"));
        json_set(ta, "elapsed", json_string(buf));
    }
    return sa3_call(ctx, "t", ta);
}

/* -- PoP: sa3_is_block_payload_rejection @ plugins/platforms/slack/adapter.py:_is_block_payload_rejection -- */
static json_t *sa3_is_block_payload_rejection(sa3_ctx_t *ctx, const json_t *args) {
    const json_t *we = json_obj_get(args, "exc");
    const json_t *spec = we ? json_obj_get(we, "__exc__") : NULL;
    const char *message = json_get_str(spec, "message", "");
    static const char *const RECOVERABLE[] = {"invalid_blocks", "msg_too_long", "too_many_blocks"};
    const json_t *response = spec ? json_obj_get(spec, "response") : NULL;
    bool callable_get = false;
    json_t *attrs = sa3_obj_attrs(response);
    if (attrs) {
        json_t *g = json_obj_get(attrs, "get");
        if (g && g->type == JSON_OBJECT && json_obj_get(g, "__callable__")) callable_get = true;
    }
    if (callable_get) {
        json_t *a = json_object();
        json_set(a, "code", json_string("error"));
        json_t *got = sa3_call(ctx, "response.get", a);
        if (sa3_is_raise(got)) {
            if (sa3_raise_passes(got)) return got;  /* BaseException passes through */
            /* Exception-family raise swallowed by except Exception */
        } else {
            const char *code = sa3_str(got);
            if (sa3_in_set_str(RECOVERABLE, 3, code)) return json_bool(true);
        }
        json_free(got);
    }
    for (size_t i = 0; i < 3; i++)
        if (sa3_contains(message, RECOVERABLE[i])) return json_bool(true);
    return json_bool(false);
}

/* -- PoP: sa3_strip_stream_cursor @ plugins/platforms/slack/adapter.py:_strip_stream_cursor -- */
static json_t *sa3_strip_stream_cursor(sa3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    if (!text) text = "";
    char *stripped = sa3_rstrip(text);
    static const char *const GLYPHS[] = {"\xe2\x96\x89", "\xe2\x96\x8d", "\xe2\x96\x8c", "\xe2\x80\xa6"};
    static const size_t GLEN[] = {3, 3, 3, 3};
    for (size_t i = 0; i < 4; i++) {
        size_t n = strlen(stripped);
        if (n >= GLEN[i] && strcmp(stripped + n - GLEN[i], GLYPHS[i]) == 0) {
            char *cut = (char *)malloc(n - GLEN[i] + 1);
            memcpy(cut, stripped, n - GLEN[i]);
            cut[n - GLEN[i]] = '\0';
            char *out = sa3_rstrip(cut);
            free(cut); free(stripped);
            json_t *ret = json_string(out);
            free(out);
            return ret;
        }
    }
    free(stripped);
    return json_string(text);  /* no glyph -> ORIGINAL text */
}

static json_t *sa3_state_extra(sa3_ctx_t *ctx) {
    const json_t *e = json_obj_get(ctx->state, "extra");
    return (e && e->type == JSON_OBJECT) ? (json_t *)e : NULL;
}

/* -- PoP: sa3_extra_flag @ plugins/platforms/slack/adapter.py:_extra_flag -- */
static json_t *sa3_extra_flag(sa3_ctx_t *ctx, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    bool def = json_get_bool(args, "default", false);
    json_t *extra = sa3_state_extra(ctx);
    const json_t *raw = extra ? json_obj_get(extra, key) : NULL;
    if (!raw || raw->type == JSON_NULL) return json_bool(def);
    char *s = sa3_pystr(raw);
    char *lo = sa3_strip_lower(s);
    static const char *const TRUESET[] = {"1", "true", "yes", "on"};
    bool ok = sa3_in_set_str(TRUESET, 4, lo);
    free(lo); free(s);
    return json_bool(ok);
}

/* -- PoP: sa3_slack_require_mention @ plugins/platforms/slack/adapter.py:_slack_require_mention -- */
static json_t *sa3_slack_require_mention(sa3_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *extra = sa3_state_extra(ctx);
    const json_t *cfg = extra ? json_obj_get(extra, "require_mention") : NULL;
    if (!cfg || cfg->type == JSON_NULL) {
        json_t *a = json_object();
        json_set(a, "name", json_string("SLACK_REQUIRE_MENTION"));
        json_set(a, "default", json_string("true"));
        cfg = sa3_call(ctx, "_get_scoped_secret", a);
        if (sa3_is_raise(cfg)) return (json_t *)cfg;
    }
    if (cfg->type == JSON_STRING) {
        char *lo = sa3_lower(cfg->str_val);
        static const char *const FALSESET[] = {"false", "0", "no", "off"};
        bool off = sa3_in_set_str(FALSESET, 4, lo);
        free(lo);
        return json_bool(!off);
    }
    return json_bool(sa3_truthy(cfg));
}

/* -- PoP: sa3_extra_or_env_flag @ plugins/platforms/slack/adapter.py:_extra_or_env_flag -- */
static json_t *sa3_extra_or_env_flag(sa3_ctx_t *ctx, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    const char *env_var = json_get_str(args, "env_var", "");
    bool strip = json_get_bool(args, "strip", false);
    json_t *extra = sa3_state_extra(ctx);
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_object());
    json_set(a, "key", json_string(key));
    json_set(a, "env_var", json_string(env_var));
    json_set(a, "default", json_string("false"));
    json_set(a, "blank_is_unset", json_bool(false));
    json_t *configured = sa3_call(ctx, "_extra_or_secret", a);
    if (sa3_is_raise(configured)) return configured;
    if (configured->type == JSON_STRING) {
        char *s = strip ? sa3_strip(configured->str_val) : sa3_strdup(configured->str_val);
        char *lo = sa3_lower(s);
        static const char *const TRUESET[] = {"true", "1", "yes", "on"};
        bool ok = sa3_in_set_str(TRUESET, 4, lo);
        free(lo); free(s);
        json_free(configured);
        return json_bool(ok);
    }
    bool ok = sa3_truthy(configured);
    json_free(configured);
    return json_bool(ok);
}

/* -- PoP: sa3_extra_or_env_channel_set @ plugins/platforms/slack/adapter.py:_extra_or_env_channel_set -- */
static json_t *sa3_extra_or_env_channel_set(sa3_ctx_t *ctx, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    const char *env_var = json_get_str(args, "env_var", "");
    bool coerce = json_get_bool(args, "coerce_scalar", false);
    json_t *extra = sa3_state_extra(ctx);
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_object());
    json_set(a, "key", json_string(key));
    json_set(a, "env_var", json_string(env_var));
    json_set(a, "default", json_string(""));
    json_set(a, "blank_is_unset", json_bool(false));
    json_t *raw = sa3_call(ctx, "_extra_or_secret", a);
    if (sa3_is_raise(raw)) return raw;
    json_t *set = sa3_set_new();
    if (raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw); i++) {
            char *s = sa3_pystr(json_get(raw, i));
            char *t = sa3_strip(s);
            sa3_set_add_str(set, t);
            free(t); free(s);
        }
    } else {
        char *s = NULL;
        if (coerce) s = (raw->type == JSON_NULL) ? sa3_strdup("") : sa3_pystr(raw);
        else if (raw->type == JSON_STRING) s = sa3_strdup(raw->str_val);
        if (s) {
            char *trimmed = sa3_strip(s);
            if (trimmed && trimmed[0]) {
                char *work = sa3_strdup(trimmed);
                char *tok = strtok(work, ",");
                while (tok) {
                    char *t = sa3_strip(tok);
                    sa3_set_add_str(set, t);
                    free(t);
                    tok = strtok(NULL, ",");
                }
                free(work);
            }
            free(trimmed); free(s);
        }
    }
    json_free(raw);
    return set;
}
