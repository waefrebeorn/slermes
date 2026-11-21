/*
 * slack_adapter_flow8.c -- FLOW cores, wave 8, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's STILL-UNCLAIMED remainder after flow1-7. Every ported name was
 * UNCLAIMED by a dotted-name-aware strict PoP grep of the whole main-only repo
 * at port time (each carries its PoP annotation at its definition below). No
 * flow1-7 function is re-ported.
 *
 * Ported (16 functions, all UNCLAIMED at their port time):
 *   _cache_slack_file, _call_with_block_fallback, _cancel_socket_tasks,
 *   _channel_gate_allows, _clear_thread_status_quietly, _client_for,
 *   _close_workspace_clients, _collect_inbound_media,
 *   _collect_thread_root_images, _commit_stream,
 *   _conversations_replies_with_backoff, _describe_slack_download_failure,
 *   _dm_target, _dm_top_level_threads_as_sessions, _download_slack_file,
 *   _download_slack_file_bytes.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa8_call(ctx, name, args). The harness wires it to a recording
 * stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow8.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc), ...extra}.
 * - {"__obj__": attrs} is an attribute carrier (SimpleNamespace double).
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
typedef struct sa8_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa8_ctx_t;

/* -- basic helpers -- */
static bool sa8_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa8_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa8_obj_attrs(const json_t *v) {
    return sa8_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa8_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa8_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa8_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa8_is_obj(v) &&
           !sa8_is_set(v) && !sa8_is_tuple_marker(v) && !sa8_is_raise(v);
}
static bool sa8_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa8_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa8_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa8_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa8_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa8_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sa8_err(type, msg);
    /* Preserve extra fields (e.g. HTTPStatusError.status_code). */
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys ? spec->c.keys[i] : NULL;
            if (!k || strcmp(k, "type") == 0 || strcmp(k, "message") == 0) continue;
            json_set(e, k, json_copy(spec->c.items[i]));
        }
    }
    return e;
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa8_call(sa8_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa8_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa8_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa8_make_exc(raise);
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
typedef struct sa8_exc_parent { const char *name, *parent1, *parent2; } sa8_exc_parent_t;
static const sa8_exc_parent_t SA8_EXC_TABLE[] = {
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
#define SA8_EXC_TABLE_LEN (sizeof(SA8_EXC_TABLE) / sizeof(SA8_EXC_TABLE[0]))

static const sa8_exc_parent_t *sa8_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA8_EXC_TABLE_LEN; i++)
        if (strcmp(SA8_EXC_TABLE[i].name, name) == 0) return &SA8_EXC_TABLE[i];
    return NULL;
}
typedef struct sa8_chain { char names[64][96]; size_t n; } sa8_chain_t;
static bool sa8_chain_has(const sa8_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa8_chain_add(sa8_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa8_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa8_chain_walk(sa8_chain_t *c, const char *name) {
    if (!name || !name[0] || sa8_chain_has(c, name)) return;
    sa8_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa8_exc_parent_t *be = sa8_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";  /* synthetic -> Exception */
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa8_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa8_chain_walk(c, p2);
}
static bool sa8_exc_family(const json_t *r, const char *family) {
    if (!sa8_is_raise(r)) return false;
    sa8_chain_t c; c.n = 0;
    sa8_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa8_chain_has(&c, family);
}
static bool sa8_raise_passes(const json_t *r) {
    return sa8_is_raise(r) && !sa8_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa8_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa8_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa8_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa8_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa8_lower(const char *s) {
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
static bool sa8_in_set_str(const char *const *items, size_t n, const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, items[i]) == 0) return true;
    return false;
}
/* Python str() of a scalar (ASCII domain: str/int/bool/None only). */
static char *sa8_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa8_strdup("None");
    if (v->type == JSON_STRING) return sa8_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa8_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa8_strdup(buf);
    }
    return sa8_strdup("");
}
/* dict.get(key, default): absent -> default; present-null -> JSON_NULL node. */
static json_t *sa8_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static const char *sa8_pytype_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) return (v->num_val == floor(v->num_val)) ? "int" : "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    if (sa8_is_obj(v)) return "types.SimpleNamespace";
    if (sa8_is_set(v)) return "set";
    return "dict";
}
static json_t *sa8_no_get(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sa8_pytype_name(v));
    return sa8_err("AttributeError", msg);
}
static json_t *sa8_no_startswith(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'startswith'", sa8_pytype_name(v));
    return sa8_err("AttributeError", msg);
}

/* -- attribute / collection helpers -- */
static json_t *sa8_getattr(const json_t *obj, const char *key) {
    json_t *attrs = sa8_obj_attrs(obj);
    if (!attrs) return json_null();
    json_t *v = json_obj_get(attrs, key);
    return json_copy(v ? v : json_null());
}
static json_t *sa8_getattr_req(const json_t *obj, const char *key, const char *type_name) {
    json_t *attrs = sa8_obj_attrs(obj);
    json_t *v = attrs ? json_obj_get(attrs, key) : NULL;
    if (!v) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 type_name ? type_name : "types.SimpleNamespace", key);
        return sa8_err("AttributeError", msg);
    }
    return json_copy(v);
}
static bool sa8_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa8_is_set(coll)) coll = json_obj_get(coll, "__set__");
    if (!coll) return false;
    if (coll->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(coll); i++) {
            const char *m = sa8_str(json_get(coll, i));
            if (m && strcmp(m, s) == 0) return true;
        }
        return false;
    }
    if (coll->type == JSON_OBJECT) return json_obj_get(coll, s) != NULL;
    return false;
}

/* -- string kernels -- */
static char *sa8_rstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && sa8_is_ws((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *sa8_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa8_strdup(s);
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
static bool sa8_endswith(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t a = strlen(s), b = strlen(suffix);
    return b <= a && strcmp(s + a - b, suffix) == 0;
}
static bool sa8_startswith(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}
static size_t sa8_len(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_ARRAY) return json_len(v);
    if (v->type == JSON_OBJECT) return v->c.count;
    return 0;
}
static char *sa8_rstrip_chars(const char *s, char ch) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == ch) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
/* Python s[:end] with end>=0; end<0 means whole string. */
static char *sa8_slice(const char *s, long long end) {
    if (!s) return NULL;
    size_t n = strlen(s);
    if (end < 0 || (size_t)end > n) end = (long long)n;
    char *out = (char *)malloc((size_t)end + 1);
    if (out) { memcpy(out, s, (size_t)end); out[end] = '\0'; }
    return out;
}
/* os.path.splitext(name)[1]: last '.' suffix incl dot ("" if none/dotfile). */
static char *sa8_splitext(const char *name) {
    if (!name) return sa8_strdup("");
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return sa8_strdup("");
    return sa8_strdup(dot);
}



/* -- local helpers for flow8 ports -- */
static json_t *sa8_o(void) { return json_object(); }
static void sa8_ss(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void sa8_sc(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, json_copy(v ? v : json_null()));
}
static void sa8_sb(json_t *o, const char *k, bool b) { json_set(o, k, json_bool(b)); }
static void sa8_sn(json_t *o, const char *k, double n) { json_set(o, k, json_number(n)); }
static const char *sa8_s(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static long long sa8_to_int(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}
static long long sa8_gi(const json_t *o, const char *k, long long def) {
    return sa8_to_int(o ? json_obj_get(o, k) : NULL, def);
}
static bool sa8_json_eq(const json_t *a, const json_t *b) {
    if (a == b) return true;
    char *sa = json_serialize(a ? a : json_null());
    char *sb = json_serialize(b ? b : json_null());
    bool eq = sa && sb && strcmp(sa, sb) == 0;
    free(sa); free(sb);
    return eq;
}
static bool sa8_is_raise_pass(const json_t *r) { return sa8_raise_passes(r); }

/* ====================================================================== */

/* PoP: sa8_cancel_socket_tasks @ plugins/platforms/slack/adapter.py:_cancel_socket_tasks */
static json_t *sa8_cancel_socket_tasks(sa8_ctx_t *ctx, const json_t *args) {
    const json_t *tasks = json_obj_get(args, "tasks");
    json_t *live = json_array();
    size_t nt = (tasks && tasks->type == JSON_ARRAY) ? json_len(tasks) : 0;
    for (size_t i = 0; i < nt; i++) {
        json_t *t = json_get(tasks, i);
        if (!t || t->type == JSON_NULL) continue;
        const char *tag = sa8_s(json_obj_get(t, "tag"), "");
        if (!json_get_bool(t, "has_cancel", false)) continue;
        if (json_get_bool(t, "has_done", false)) {
            json_t *a = sa8_o(); sa8_ss(a, "tag", tag);
            json_t *d = sa8_call(ctx, "task.done", a);
            if (sa8_is_raise(d)) { json_free(live); return d; }
            bool skip = sa8_truthy(d); json_free(d);
            if (skip) continue;
        }
        json_append(live, json_copy(t));
    }
    size_t nl = json_len(live);
    for (size_t i = 0; i < nl; i++) {
        json_t *a = sa8_o(); sa8_ss(a, "tag", sa8_s(json_obj_get(json_get(live, i), "tag"), ""));
        json_t *r = sa8_call(ctx, "task.cancel", a);
        if (sa8_is_raise(r)) { json_free(live); return r; }
        json_free(r);
    }
    if (nl == 0) { json_free(live); return json_null(); }
    json_t *a = sa8_o();
    json_t *tags = json_array();
    for (size_t i = 0; i < nl; i++)
        json_append(tags, json_string(sa8_s(json_obj_get(json_get(live, i), "tag"), "")));
    json_set(a, "tags", tags);
    sa8_sn(a, "timeout", 3.0);
    json_t *w = sa8_call(ctx, "asyncio.wait", a);
    if (sa8_is_raise(w)) { json_free(live); return w; }
    const json_t *done = json_obj_get(w, "done");
    const json_t *still = json_obj_get(w, "still");
    size_t nd = (done && done->type == JSON_ARRAY) ? json_len(done) : 0;
    for (size_t i = 0; i < nd; i++) {
        const char *tag = sa8_s(json_get(done, i), "");
        json_t *c = sa8_o(); sa8_ss(c, "tag", tag);
        json_t *r = sa8_call(ctx, "task.cancelled", c);
        if (sa8_is_raise(r)) { json_free(w); json_free(live); return r; }
        bool cc = sa8_truthy(r); json_free(r);
        if (cc) continue;
        json_t *e = sa8_o(); sa8_ss(e, "tag", tag);
        r = sa8_call(ctx, "task.exception", e);
        if (sa8_is_raise(r)) { json_free(w); json_free(live); return r; }
        bool has_exc = !(r && r->type == JSON_NULL); json_free(r);
        if (has_exc) {
            json_t *d = sa8_o(); sa8_ss(d, "tag", tag);
            r = sa8_call(ctx, "logger.debug", d);
            if (sa8_is_raise(r)) { json_free(w); json_free(live); return r; }
            json_free(r);
        }
    }
    size_t ns = (still && still->type == JSON_ARRAY) ? json_len(still) : 0;
    if (ns > 0) {
        json_t *p = sa8_o(); sa8_sn(p, "count", (double)ns); sa8_sn(p, "timeout", 3.0);
        json_t *r = sa8_call(ctx, "logger.warning", p);
        if (sa8_is_raise(r)) { json_free(w); json_free(live); return r; }
        json_free(r);
    }
    json_free(w); json_free(live);
    return json_null();
}

/* PoP: sa8_close_workspace_clients @ plugins/platforms/slack/adapter.py:_close_workspace_clients */
static json_t *sa8_close_workspace_clients(sa8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *primary = sa8_call(ctx, "app_client", sa8_o());
    if (sa8_is_raise(primary)) return primary;
    json_t *clients = json_array();
    if (primary && primary->type != JSON_NULL) json_append(clients, json_copy(primary));
    json_free(primary);
    const json_t *tc = json_obj_get(ctx->state, "_team_clients");
    if (tc && tc->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(tc); i++) json_append(clients, json_copy(json_get(tc, i)));
    json_t *seen = json_array();
    for (size_t i = 0; i < json_len(clients); i++) {
        json_t *c = json_get(clients, i);
        const json_t *ident = json_obj_get(c, "ident");
        bool dup = false;
        for (size_t j = 0; j < json_len(seen); j++)
            if (sa8_json_eq(json_get(seen, j), ident)) { dup = true; break; }
        if (dup) continue;
        json_append(seen, json_copy(ident ? ident : json_null()));
        const json_t *methods = json_obj_get(c, "methods");
        const char *order[2] = { "close", "aclose" };
        for (int m = 0; m < 2; m++) {
            bool present = false;
            if (methods && methods->type == JSON_ARRAY)
                for (size_t k = 0; k < json_len(methods); k++) {
                    const char *mv = sa8_s(json_get(methods, k), "");
                    if (strcmp(mv, order[m]) == 0) { present = true; break; }
                }
            if (!present) continue;
            json_t *a = sa8_o(); sa8_sc(a, "ident", ident);
            json_t *r = sa8_call(ctx, order[m], a);
            if (sa8_is_raise(r)) { json_free(seen); json_free(clients); return r; }
            if (r && r->type == JSON_OBJECT && sa8_truthy(json_obj_get(r, "__await__"))) {
                json_t *a2 = sa8_o(); sa8_sc(a2, "ident", ident);
                json_t *r2 = sa8_call(ctx, "await_close", a2);
                if (sa8_is_raise(r2)) { json_free(r); json_free(seen); json_free(clients); return r2; }
                json_free(r2);
            }
            json_free(r);
            break;
        }
    }
    json_free(seen); json_free(clients);
    return json_null();
}

/* PoP: sa8_describe_slack_download_failure @ plugins/platforms/slack/adapter.py:_describe_slack_download_failure */
static json_t *sa8_describe_slack_download_failure(sa8_ctx_t *ctx, const json_t *args) {
    const json_t *fobj = json_obj_get(args, "file_obj");
    const json_t *exc = json_obj_get(args, "exc");
    json_t *a = sa8_o(); sa8_sc(a, "file_obj", fobj);
    json_t *label = sa8_call(ctx, "attachment_label", a);
    if (sa8_is_raise(label)) return label;
    a = sa8_o(); sa8_sc(a, "exc", exc);
    json_t *resp = sa8_call(ctx, "getattr_response", a);
    if (sa8_is_raise(resp)) { json_free(label); return resp; }
    a = sa8_o(); sa8_sc(a, "response", resp); sa8_sc(a, "file_obj", fobj);
    json_t *detail = sa8_call(ctx, "describe_api_error", a);
    if (sa8_is_raise(detail)) { json_free(label); json_free(resp); return detail; }
    if (sa8_truthy(detail)) { json_free(label); json_free(resp); return detail; }
    a = sa8_o(); sa8_sc(a, "exc", exc);
    json_t *ish = sa8_call(ctx, "is_http_status_error", a);
    if (sa8_is_raise(ish)) { json_free(label); json_free(resp); json_free(detail); return ish; }
    if (sa8_truthy(ish)) {
        a = sa8_o(); sa8_sc(a, "exc", exc);
        json_t *status = sa8_call(ctx, "status_code", a);
        if (sa8_is_raise(status)) { json_free(label); json_free(resp); json_free(detail); json_free(ish); return status; }
        a = sa8_o(); sa8_sc(a, "status", status);
        json_t *tmpl = sa8_call(ctx, "http_template", a);
        if (sa8_is_raise(tmpl)) { json_free(label); json_free(resp); json_free(detail); json_free(ish); json_free(status); return tmpl; }
        if (sa8_truthy(tmpl)) {
            a = sa8_o(); sa8_sc(a, "template", tmpl); sa8_sc(a, "file_label", label);
            json_t *out = sa8_call(ctx, "format_template", a);
            json_free(label); json_free(resp); json_free(detail); json_free(ish);
            json_free(status); json_free(tmpl);
            return out;
        }
        json_free(tmpl); json_free(status);
    }
    a = sa8_o(); sa8_sc(a, "exc", exc);
    json_t *msg = sa8_call(ctx, "str_exc", a);
    if (sa8_is_raise(msg)) { json_free(label); json_free(resp); json_free(detail); json_free(ish); return msg; }
    const char *ms = sa8_s(msg, "");
    if (strstr(ms, "Slack returned HTML instead of media") || strstr(ms, "non-image data")) {
        char *lbl = sa8_pystr(label);
        size_t n = strlen(lbl) + 300;
        char *buf = (char *)malloc(n);
        snprintf(buf, n, "Slack attachment access failed for %s: Slack returned an HTML/login or non-media response. This usually means a scope, auth, or file-permission problem.", lbl);
        json_t *out = json_string(buf);
        free(buf); free(lbl);
        json_free(label); json_free(resp); json_free(detail); json_free(ish); json_free(msg);
        return out;
    }
    json_free(label); json_free(resp); json_free(detail); json_free(ish); json_free(msg);
    return json_null();
}

/* PoP: sa8_client_for @ plugins/platforms/slack/adapter.py:_client_for */
static json_t *sa8_client_for(sa8_ctx_t *ctx, const json_t *args) {
    json_t *a = sa8_o(); sa8_sc(a, "metadata", json_obj_get(args, "metadata"));
    json_t *tid = sa8_call(ctx, "metadata_team_id", a);
    if (sa8_is_raise(tid)) return tid;
    json_t *b = sa8_o(); sa8_sc(b, "chat_id", json_obj_get(args, "chat_id")); sa8_sc(b, "team_id", tid);
    json_free(tid);
    return sa8_call(ctx, "get_client", b);
}

/* PoP: sa8_dm_target @ plugins/platforms/slack/adapter.py:_dm_target */
static json_t *sa8_dm_target(sa8_ctx_t *ctx, const json_t *args) {
    json_t *a = sa8_o(); sa8_sc(a, "metadata", json_obj_get(args, "metadata"));
    json_t *tid = sa8_call(ctx, "metadata_team_id", a);
    if (sa8_is_raise(tid)) return tid;
    json_t *b = sa8_o(); sa8_sc(b, "chat_id", json_obj_get(args, "chat_id")); sa8_sc(b, "team_id", tid);
    json_free(tid);
    return sa8_call(ctx, "ensure_dm_conversation", b);
}

/* PoP: sa8_clear_thread_status_quietly @ plugins/platforms/slack/adapter.py:_clear_thread_status_quietly */
static json_t *sa8_clear_thread_status_quietly(sa8_ctx_t *ctx, const json_t *args) {
    json_t *a = sa8_o(); sa8_sc(a, "chat_id", json_obj_get(args, "chat_id"));
    sa8_sc(a, "metadata", json_obj_get(args, "metadata"));
    json_t *r = sa8_call(ctx, "stop_typing", a);
    if (!sa8_is_raise(r)) { json_free(r); return json_null(); }
    if (sa8_is_raise_pass(r)) return r;
    json_t *b = sa8_o(); sa8_sc(b, "message", json_obj_get(r, "message"));
    json_free(r);
    json_t *d = sa8_call(ctx, "logger.debug", b);
    if (sa8_is_raise(d)) return d;
    json_free(d);
    return json_null();
}

/* PoP: sa8_call_with_block_fallback @ plugins/platforms/slack/adapter.py:_call_with_block_fallback */
static json_t *sa8_call_with_block_fallback(sa8_ctx_t *ctx, const json_t *args) {
    const json_t *kwargs = json_obj_get(args, "kwargs");
    const char *verb = json_get_str(args, "verb", "");
    const char *method = json_get_str(args, "method", "");
    json_t *r = sa8_call(ctx, "client_fn", sa8_o());
    if (sa8_is_raise(r)) return r;
    json_free(r);
    json_t *a = sa8_o(); sa8_ss(a, "method", method); sa8_sc(a, "kwargs", kwargs);
    json_t *r1 = sa8_call(ctx, "method_call", a);
    if (!sa8_is_raise(r1)) return r1;
    if (sa8_is_raise_pass(r1)) return r1;
    const json_t *blocks = json_obj_get(kwargs, "blocks");
    if (sa8_truthy(blocks)) {
        json_t *b = sa8_o(); sa8_sc(b, "exc", r1);
        json_t *br = sa8_call(ctx, "is_block_rejection", b);
        if (sa8_is_raise(br)) { json_free(r1); return br; }
        bool rej = sa8_truthy(br); json_free(br);
        if (rej) {
            json_t *retry = json_copy(kwargs ? kwargs : json_object());
            if (strcmp(verb, "edit") == 0) {
                json_set(retry, "blocks", json_array());
            } else {
                json_obj_del(retry, "blocks");
            }
            json_t *li = sa8_o(); sa8_ss(li, "verb", verb);
            json_t *lr = sa8_call(ctx, "logger.info", li);
            if (sa8_is_raise(lr)) { json_free(retry); json_free(r1); return lr; }
            json_free(lr);
            r = sa8_call(ctx, "client_fn", sa8_o());
            if (sa8_is_raise(r)) { json_free(retry); json_free(r1); return r; }
            json_free(r);
            json_t *c = sa8_o(); sa8_ss(c, "method", method); sa8_sc(c, "kwargs", retry);
            json_free(retry); json_free(r1);
            return sa8_call(ctx, "method_call", c);
        }
    }
    return r1;
}

/* PoP: sa8_commit_stream @ plugins/platforms/slack/adapter.py:_commit_stream */
static json_t *sa8_commit_stream(sa8_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const json_t *stream = json_obj_get(args, "stream");
    const char *text = json_get_str(args, "text", "");
    const char *delta = json_get_str(args, "delta", "");
    bool replace = json_get_bool(args, "replace", false);
    const char *chat_id = (key && key->type == JSON_ARRAY && json_len(key) > 1)
                            ? sa8_s(json_get(key, 1), "") : "";
    const json_t *ts = stream ? json_obj_get(stream, "ts") : NULL;
    json_t *a = sa8_o(); sa8_sc(a, "key", key);
    json_t *r = sa8_call(ctx, "pop_stream", a);
    if (sa8_is_raise(r)) return r;
    json_free(r);
    a = sa8_o(); sa8_sc(a, "ts", ts); sa8_ss(a, "delta", delta);
    json_t *sealed = sa8_call(ctx, "seal_stream", a);
    if (sa8_is_raise(sealed)) return sealed;
    bool bsealed = sa8_truthy(sealed);
    char *dstrip = sa8_strip(delta);
    bool dnonempty = dstrip && dstrip[0] != '\0';
    free(dstrip);
    if (!bsealed && !dnonempty) {
        json_free(sealed);
        a = sa8_o(); sa8_sc(a, "ts", ts); sa8_ss(a, "delta", "");
        sealed = sa8_call(ctx, "seal_stream", a);
        if (sa8_is_raise(sealed)) return sealed;
        bsealed = sa8_truthy(sealed);
    }
    bool committed = false;
    bool need_edit = true;
    if (bsealed && !replace) {
        a = sa8_o(); sa8_ss(a, "text", text);
        json_t *mb = sa8_call(ctx, "maybe_blocks", a);
        if (sa8_is_raise(mb)) { json_free(sealed); return mb; }
        if (!sa8_truthy(mb)) { committed = true; need_edit = false; }
        json_free(mb);
    }
    if (need_edit) {
        long long base = stream ? sa8_gi(stream, "base", 0) : 0;
        const char *shown = text;
        char *shbuf = NULL;
        if (!replace) {
            size_t len = strlen(text);
            size_t off = base < 0 ? 0 : (size_t)base;
            if (off > len) off = len;
            shbuf = sa8_strdup(text + off);
            shown = shbuf;
        }
        a = sa8_o(); sa8_ss(a, "chat_id", chat_id); sa8_sc(a, "ts", ts); sa8_ss(a, "shown", shown);
        json_t *ed = sa8_call(ctx, "edit_message", a);
        free(shbuf);
        if (sa8_is_raise(ed)) { json_free(sealed); return ed; }
        bool es = json_get_bool(ed, "success", false);
        json_free(ed);
        committed = es || (bsealed && !replace);
        if (!bsealed) {
            a = sa8_o(); sa8_ss(a, "chat_id", chat_id); sa8_sc(a, "ts", ts);
            json_t *w = sa8_call(ctx, "logger.warning", a);
            if (sa8_is_raise(w)) { json_free(sealed); return w; }
            json_free(w);
        }
    }
    json_free(sealed);
    if (!committed) return json_null();
    a = sa8_o(); sa8_ss(a, "chat_id", chat_id);
    r = sa8_call(ctx, "stop_typing", a);
    if (sa8_is_raise(r)) return r;
    json_free(r);
    a = sa8_o(); sa8_sb(a, "success", true); sa8_sc(a, "message_id", ts);
    return sa8_call(ctx, "send_result", a);
}

/* PoP: sa8_dm_top_level_threads_as_sessions @ plugins/platforms/slack/adapter.py:_dm_top_level_threads_as_sessions */
static json_t *sa8_dm_top_level_threads_as_sessions(sa8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = sa8_o();
    sa8_ss(a, "key", "dm_top_level_threads_as_sessions");
    sa8_sb(a, "default", true);
    return sa8_call(ctx, "extra_flag", a);
}

/* PoP: sa8_channel_gate_allows @ plugins/platforms/slack/adapter.py:_channel_gate_allows */
static json_t *sa8_channel_gate_allows(sa8_ctx_t *ctx, const json_t *args) {
    const char *channel_id = json_get_str(args, "channel_id", "");
    const char *routing_text = json_get_str(args, "routing_text", "");
    const char *bot_uid = json_get_str(args, "bot_uid", "");
    bool mentioned = json_get_bool(args, "is_mentioned", false);
    bool thread_reply = json_get_bool(args, "is_thread_reply", false);
    const char *user_id = json_get_str(args, "user_id", "");
    const char *team_id = json_get_str(args, "team_id", "");
    bool is_dm = json_get_bool(args, "is_dm", false);
    bool force = json_get_bool(args, "force_process", false);
    json_t *allowed = sa8_call(ctx, "allowed_channels", sa8_o());
    if (sa8_is_raise(allowed)) return allowed;
    if (sa8_truthy(allowed) && !sa8_collection_has_str(allowed, channel_id)) {
        json_free(allowed); return json_bool(false);
    }
    json_free(allowed);
    const char *bot_user = json_get_str(ctx->state, "_bot_user_id", "");
    json_t *uids = json_array();
    if (bot_uid && bot_uid[0]) json_append(uids, json_string(bot_uid));
    if (bot_user && bot_user[0]) json_append(uids, json_string(bot_user));
    json_t *ign = sa8_call(ctx, "ignore_other_user_mentions", sa8_o());
    if (sa8_is_raise(ign)) { json_free(uids); return ign; }
    bool ign_b = sa8_truthy(ign); json_free(ign);
    bool block_other = false;
    if (ign_b && !mentioned) {
        json_t *a = sa8_o(); sa8_ss(a, "text", routing_text); sa8_sc(a, "uids", uids);
        json_t *ms = sa8_call(ctx, "mentions_self", a);
        if (sa8_is_raise(ms)) { json_free(uids); return ms; }
        bool ms_b = sa8_truthy(ms); json_free(ms);
        if (!ms_b) {
            json_t *b = sa8_o(); sa8_ss(b, "text", routing_text); sa8_sc(b, "uids", uids);
            json_t *ao = sa8_call(ctx, "addressed_to_other_user", b);
            if (sa8_is_raise(ao)) { json_free(uids); return ao; }
            block_other = sa8_truthy(ao); json_free(ao);
        }
    }
    json_free(uids);
    if (block_other) return json_bool(false);
    json_t *tr = sa8_call(ctx, "thread_require_mention", sa8_o());
    if (sa8_is_raise(tr)) return tr;
    bool thread_gated = sa8_truthy(tr) && thread_reply && !mentioned; json_free(tr);
    if (force) return json_bool(true);
    json_t *a = sa8_o(); sa8_ss(a, "channel_id", channel_id);
    json_t *free_r = sa8_call(ctx, "is_free_channel", a);
    if (sa8_is_raise(free_r)) return free_r;
    bool free_b = sa8_truthy(free_r); json_free(free_r);
    if (!free_b) {
        json_t *sm = sa8_call(ctx, "strict_mention", sa8_o());
        if (sa8_is_raise(sm)) return sm;
        bool sm_b = sa8_truthy(sm); json_free(sm);
        if (sm_b && !mentioned) return json_bool(false);
    }
    if (thread_gated) {
        json_t *d = sa8_o(); sa8_ss(d, "channel_id", channel_id);
        json_t *r = sa8_call(ctx, "logger.debug", d);
        if (sa8_is_raise(r)) return r;
        json_free(r);
        return json_bool(false);
    }
    if (free_b) return json_bool(true);
    if (!mentioned) {
        json_t *w = sa8_o();
        sa8_ss(w, "channel_id", channel_id); sa8_ss(w, "user_id", user_id);
        sa8_ss(w, "team_id", team_id); sa8_sb(w, "is_thread_reply", thread_reply);
        sa8_ss(w, "chat_type", is_dm ? "dm" : "group");
        return sa8_call(ctx, "should_wake", w);
    }
    return json_bool(true);
}

/* PoP: sa8_cache_slack_file @ plugins/platforms/slack/adapter.py:_cache_slack_file */
static json_t *sa8_cache_slack_file(sa8_ctx_t *ctx, const json_t *args) {
    const char *kind = json_get_str(args, "kind", "");
    const json_t *f = json_obj_get(args, "f");
    const char *url = json_get_str(args, "url", "");
    const char *mimetype = json_get_str(args, "mimetype", "");
    const char *team_id = json_get_str(args, "team_id", "");
    json_t *out = json_array();
    if (strcmp(kind, "image") == 0) {
        const char *slash = strrchr(mimetype, '/');
        const char *base = slash ? slash + 1 : mimetype;
        const char *semi = strchr(base, ';');
        char stem[128];
        size_t n = semi ? (size_t)(semi - base) : strlen(base);
        if (n >= sizeof(stem)) n = sizeof(stem) - 1;
        memcpy(stem, base, n); stem[n] = '\0';
        char ext[140]; snprintf(ext, sizeof ext, ".%s", stem);
        if (!sa8_in_set_str((const char *const[]){".jpg", ".jpeg", ".png", ".gif", ".webp"}, 5, ext))
            snprintf(ext, sizeof ext, ".jpg");
        json_t *a = sa8_o(); sa8_ss(a, "url", url); sa8_ss(a, "ext", ext); sa8_ss(a, "team_id", team_id);
        json_t *cached = sa8_call(ctx, "download_file", a);
        if (sa8_is_raise(cached)) { json_free(out); return cached; }
        json_append(out, cached);
        json_append(out, json_string(mimetype));
        json_append(out, json_string(""));
        return out;
    }
    if (strcmp(kind, "audio") == 0 || strcmp(kind, "voice clip") == 0) {
        json_t *a = sa8_o(); sa8_sc(a, "f", f); sa8_ss(a, "mimetype", mimetype);
        json_t *extv = sa8_call(ctx, "resolve_audio_ext", a);
        if (sa8_is_raise(extv)) { json_free(out); return extv; }
        const char *ext = sa8_s(extv, "");
        json_t *b = sa8_o(); sa8_ss(b, "url", url); sa8_ss(b, "ext", ext);
        sa8_sb(b, "audio", true); sa8_ss(b, "team_id", team_id);
        json_t *cached = sa8_call(ctx, "download_file", b);
        if (sa8_is_raise(cached)) { json_free(extv); json_free(out); return cached; }
        if (strcmp(kind, "audio") == 0) {
            json_free(extv);
            json_append(out, cached);
            json_append(out, json_string(mimetype));
            json_append(out, json_string(""));
            return out;
        }
        json_t *d = sa8_o(); sa8_ss(d, "mimetype", mimetype); sa8_sc(d, "cached", cached);
        json_t *r = sa8_call(ctx, "logger.debug", d);
        if (sa8_is_raise(r)) { json_free(cached); json_free(extv); json_free(out); return r; }
        json_free(r);
        const json_t *amap = json_obj_get(ctx->state, "_SLACK_EXT_TO_AUDIO_MIME");
        const char *amime = "audio/mp4";
        if (amap && amap->type == JSON_OBJECT) {
            const json_t *mv = json_obj_get(amap, ext);
            if (mv) amime = sa8_s(mv, "audio/mp4");
        }
        json_free(extv);
        json_append(out, cached);
        json_append(out, json_string(amime));
        json_append(out, json_string(""));
        return out;
    }
    if (strcmp(kind, "video") == 0) {
        const char *name = f ? json_get_str(f, "name", "") : "";
        char *ext = sa8_lower(sa8_splitext(name));
        const json_t *vt = json_obj_get(ctx->state, "SUPPORTED_VIDEO_TYPES");
        if (!(vt && vt->type == JSON_OBJECT && json_obj_get(vt, ext))) {
            const char *semi = strchr(mimetype, ';');
            char mt[128]; size_t n = semi ? (size_t)(semi - mimetype) : strlen(mimetype);
            if (n >= sizeof(mt)) n = sizeof(mt) - 1;
            memcpy(mt, mimetype, n); mt[n] = '\0';
            char *lmt = sa8_lower(mt);
            const char *found = ".mp4";
            if (vt && vt->type == JSON_OBJECT) {
                for (size_t i = 0; i < vt->c.count; i++) {
                    const char *k = vt->c.keys ? vt->c.keys[i] : NULL;
                    if (k && strcmp(sa8_s(vt->c.items[i], ""), lmt) == 0) { found = k; break; }
                }
            }
            free(lmt);
            free(ext); ext = sa8_strdup(found);
        }
        json_t *a = sa8_o(); sa8_ss(a, "url", url); sa8_ss(a, "team_id", team_id);
        json_t *raw = sa8_call(ctx, "download_bytes", a);
        if (sa8_is_raise(raw)) { free(ext); json_free(out); return raw; }
        json_t *b = sa8_o(); sa8_sc(b, "raw_bytes", raw); sa8_ss(b, "ext", ext);
        json_t *cached = sa8_call(ctx, "cache_video", b);
        if (sa8_is_raise(cached)) { json_free(raw); free(ext); json_free(out); return cached; }
        json_t *d = sa8_o(); sa8_sc(d, "cached", cached);
        json_t *r = sa8_call(ctx, "logger.debug", d);
        if (sa8_is_raise(r)) { json_free(cached); json_free(raw); free(ext); json_free(out); return r; }
        json_free(r); json_free(raw);
        const char *vmime = mimetype[0] ? mimetype : "video/mp4";
        if (vt && vt->type == JSON_OBJECT) {
            const json_t *mv = json_obj_get(vt, ext);
            if (mv) vmime = sa8_s(mv, vmime);
        }
        free(ext);
        json_append(out, cached);
        json_append(out, json_string(vmime));
        json_append(out, json_string(""));
        return out;
    }
    json_t *a = sa8_o(); sa8_sc(a, "f", f); sa8_ss(a, "url", url);
    sa8_ss(a, "mimetype", mimetype); sa8_ss(a, "team_id", team_id);
    json_free(out);
    return sa8_call(ctx, "cache_document", a);
}

/* PoP: sa8_collect_inbound_media @ plugins/platforms/slack/adapter.py:_collect_inbound_media */
static json_t *sa8_collect_inbound_media(sa8_ctx_t *ctx, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    const char *text = json_get_str(args, "text", "");
    char *text_own = sa8_strdup(text);
    json_t *urls = json_array(), *types = json_array(), *inlined = json_array();
    const json_t *ru = json_obj_get(args, "thread_root_media_urls");
    const json_t *rt = json_obj_get(args, "thread_root_media_types");
    if (ru && ru->type == JSON_ARRAY) for (size_t i = 0; i < json_len(ru); i++) {
        json_append(urls, json_copy(json_get(ru, i)));
        json_append(inlined, json_bool(false));
    }
    if (rt && rt->type == JSON_ARRAY) for (size_t i = 0; i < json_len(rt); i++)
        json_append(types, json_copy(json_get(rt, i)));
    json_t *notices = json_array();
    json_t *a = sa8_o(); sa8_sc(a, "files", json_obj_get(event, "files"));
    sa8_sc(a, "attachments", json_obj_get(event, "attachments"));
    json_t *files = sa8_call(ctx, "files_with_forwarded", a);
    if (sa8_is_raise(files)) { free(text_own); return files; }
    size_t nf = (files && files->type == JSON_ARRAY) ? json_len(files) : 0;
    for (size_t i = 0; i < nf; i++) {
        json_t *f = json_get(files, i);
        if (!f || f->type != JSON_OBJECT) continue;
        if (strcmp(json_get_str(f, "file_access", ""), "check_file_info") == 0) {
            json_t *b = sa8_o(); sa8_sc(b, "f", f);
            json_t *sf = sa8_call(ctx, "resolve_stub", b);
            if (sa8_is_raise(sf)) { json_free(files); json_free(notices); free(text_own); return sf; }
            if (sf && sf->type == JSON_NULL) { json_free(sf); continue; }
            f = sf;
        }
        const char *mimetype = json_get_str(f, "mimetype", "unknown");
        const json_t *u1 = json_obj_get(f, "url_private_download");
        const char *url = sa8_truthy(u1) ? sa8_s(u1, "") : json_get_str(f, "url_private", "");
        if (!url[0]) continue;
        json_t *k = sa8_o(); sa8_sc(k, "f", f); sa8_ss(k, "mimetype", mimetype);
        json_t *kind = sa8_call(ctx, "file_kind", k);
        if (sa8_is_raise(kind)) { json_free(files); json_free(notices); free(text_own); return kind; }
        json_t *c = sa8_o(); sa8_sc(c, "kind", kind); sa8_sc(c, "f", f);
        sa8_ss(c, "url", url); sa8_ss(c, "mimetype", mimetype);
        json_t *cached = sa8_call(ctx, "cache_slack_file", c);
        json_free(kind);
        if (sa8_is_raise(cached)) {
            if (sa8_is_raise_pass(cached)) { json_free(files); json_free(notices); free(text_own); return cached; }
            json_t *d = sa8_o(); sa8_sc(d, "exc", cached); sa8_sc(d, "file_obj", f);
            json_t *diag = sa8_call(ctx, "describe_failure", d);
            if (sa8_is_raise(diag)) { json_free(cached); json_free(files); json_free(notices); free(text_own); return diag; }
            json_t *n = sa8_o(); sa8_sc(n, "notice", diag);
            json_t *nr = sa8_call(ctx, "note_failure", n);
            if (sa8_is_raise(nr)) { json_free(diag); json_free(cached); json_free(files); json_free(notices); free(text_own); return nr; }
            json_free(nr);
            json_append(notices, diag);
            json_free(cached);
            continue;
        }
        if (!cached || cached->type == JSON_NULL) { json_free(cached); continue; }
        json_append(urls, json_copy(json_get(cached, 0)));
        json_append(types, json_copy(json_get(cached, 1)));
        json_t *ir = sa8_o(); sa8_sc(ir, "text", json_obj_get(args, "text"));
        sa8_sc(ir, "injection", json_get(cached, 2));
        json_free(cached);
        json_t *irv = sa8_call(ctx, "inline_document_text", ir);
        if (sa8_is_raise(irv)) {
            if (sa8_is_raise_pass(irv)) { json_free(files); json_free(notices); free(text_own); return irv; }
            json_t *d = sa8_o(); sa8_sc(d, "exc", irv); sa8_sc(d, "file_obj", f);
            json_t *diag = sa8_call(ctx, "describe_failure", d);
            if (sa8_is_raise(diag)) { json_free(irv); json_free(files); json_free(notices); free(text_own); return diag; }
            json_t *n = sa8_o(); sa8_sc(n, "notice", diag);
            json_t *nr = sa8_call(ctx, "note_failure", n);
            if (sa8_is_raise(nr)) { json_free(diag); json_free(irv); json_free(files); json_free(notices); free(text_own); return nr; }
            json_free(nr);
            json_append(notices, diag); json_free(irv);
            continue;
        }
        const char *nt = json_get_str(irv, "text", text_own);
        free(text_own); text_own = sa8_strdup(nt);
        const json_t *iv = json_obj_get(irv, "inlined");
        bool inv = false;
        if (iv && iv->type == JSON_ARRAY && json_len(iv) > 0) inv = sa8_truthy(json_get(iv, 0));
        json_append(inlined, json_bool(inv));
        json_free(irv);
    }
    json_free(files);
    if (json_len(notices) > 0) {
        size_t cap = 64, used = 0;
        char *block = (char *)malloc(cap);
        const char *hdr = "[Slack attachment notice]\n";
        memcpy(block, hdr, strlen(hdr) + 1); used = strlen(hdr);
        for (size_t i = 0; i < json_len(notices); i++) {
            const char *val = sa8_pystr(json_get(notices, i));
            size_t need = used + strlen(val) + 4;
            if (need > cap) { cap = need * 2; block = (char *)realloc(block, cap); }
            block[used++] = '-'; block[used++] = ' ';
            memcpy(block + used, val, strlen(val)); used += strlen(val);
            block[used++] = '\n'; block[used] = '\0';
            free((void *)val);
        }
        size_t n = strlen(block);
        if (n > 0 && block[n - 1] == '\n') block[n - 1] = '\0';
        size_t total = strlen(block) + (text_own && text_own[0] ? strlen(text_own) + 2 : 0) + 2;
        char *nb = (char *)malloc(total);
        if (text_own && text_own[0]) snprintf(nb, total, "%s\n\n%s", block, text_own);
        else snprintf(nb, total, "%s", block);
        free(text_own); free(block); text_own = nb;
    }
    json_free(notices);
    json_t *out = json_array();
    json_append(out, urls); json_append(out, types);
    json_append(out, inlined); json_append(out, json_string(text_own ? text_own : ""));
    free(text_own);
    return out;
}

/* PoP: sa8_conversations_replies_with_backoff @ plugins/platforms/slack/adapter.py:_conversations_replies_with_backoff */
static json_t *sa8_conversations_replies_with_backoff(sa8_ctx_t *ctx, const json_t *args) {
    const char *channel_id = json_get_str(args, "channel_id", "");
    const char *thread_ts = json_get_str(args, "thread_ts", "");
    long long limit = sa8_gi(args, "limit", 200);
    const char *team_id = json_get_str(args, "team_id", "");
    json_t *a = sa8_o(); sa8_ss(a, "channel_id", channel_id); sa8_ss(a, "team_id", team_id);
    json_t *g = sa8_call(ctx, "get_client", a);
    if (sa8_is_raise(g)) return g;
    json_free(g);
    for (int attempt = 0; attempt < 3; attempt++) {
        json_t *b = sa8_o(); sa8_ss(b, "channel", channel_id); sa8_ss(b, "ts", thread_ts);
        sa8_sn(b, "limit", (double)limit); sa8_sb(b, "inclusive", true);
        json_t *r = sa8_call(ctx, "conversations_replies", b);
        if (!sa8_is_raise(r)) return r;
        if (sa8_is_raise_pass(r)) return r;
        const char *msg = json_get_str(r, "message", "");
        char *low = sa8_lower(msg);
        bool rate = strstr(low, "ratelimited") || strstr(low, "429") || strstr(low, "rate_limited");
        free(low);
        if (rate && attempt < 2) {
            json_t *s = sa8_o(); sa8_sn(s, "seconds", 1.0 * (1 << attempt));
            json_t *sr = sa8_call(ctx, "sleep", s);
            if (sa8_is_raise(sr)) { json_free(r); return sr; }
            json_free(sr); json_free(r);
            continue;
        }
        return r;
    }
    return json_null();
}

/* PoP: sa8_collect_thread_root_images @ plugins/platforms/slack/adapter.py:_collect_thread_root_images */
static json_t *sa8_collect_thread_root_images(sa8_ctx_t *ctx, const json_t *args) {
    const char *channel_id = json_get_str(args, "channel_id", "");
    const char *thread_ts = json_get_str(args, "thread_ts", "");
    const char *team_id = json_get_str(args, "team_id", "");
    json_t *urls = json_array(), *types = json_array();
    json_t *a = sa8_o(); sa8_ss(a, "channel_id", channel_id);
    sa8_ss(a, "thread_ts", thread_ts); sa8_ss(a, "team_id", team_id);
    json_t *key = sa8_call(ctx, "thread_cache_key", a);
    if (sa8_is_raise(key)) {
        if (sa8_is_raise_pass(key)) { json_free(urls); json_free(types); return key; }
        json_free(key);
        json_t *d = sa8_o(); sa8_ss(d, "stage", "thread_root_recovery");
        json_t *dr = sa8_call(ctx, "logger.debug", d);
        if (sa8_is_raise(dr)) { json_free(urls); json_free(types); return dr; }
        json_free(dr);
        json_t *out = json_array(); json_append(out, urls); json_append(out, types); return out;
    }
    const json_t *cache = json_obj_get(ctx->state, "_thread_context_cache");
    json_t *cached = NULL;
    if (cache && cache->type == JSON_OBJECT) {
        char *ks = sa8_pystr(key);
        const json_t *cv = json_obj_get(cache, ks);
        if (cv) cached = (json_t *)cv;
        free(ks);
    }
    json_free(key);
    json_t *root = NULL;
    if (cached) {
        json_t *b = sa8_o();
        const json_t *msgs = json_obj_get(cached, "messages");
        sa8_sc(b, "messages", msgs ? msgs : json_array());
        sa8_ss(b, "thread_ts", thread_ts);
        json_t *r = sa8_call(ctx, "thread_root_message", b);
        if (sa8_is_raise(r)) {
            if (sa8_is_raise_pass(r)) { json_free(urls); json_free(types); return r; }
            json_free(r);
            json_t *d = sa8_o(); sa8_ss(d, "stage", "thread_root_recovery");
            json_t *dr = sa8_call(ctx, "logger.debug", d);
            if (sa8_is_raise(dr)) { json_free(urls); json_free(types); return dr; }
            json_free(dr);
            json_t *out = json_array(); json_append(out, urls); json_append(out, types); return out;
        }
        if (r && r->type != JSON_NULL) root = r; else json_free(r);
    }
    json_t *files = NULL;
    if (root) {
        json_t *c = sa8_o();
        const json_t *fv = json_obj_get(root, "files");
        const json_t *av = json_obj_get(root, "attachments");
        sa8_sc(c, "files", fv); sa8_sc(c, "attachments", av);
        json_t *r = sa8_call(ctx, "files_with_forwarded", c);
        if (sa8_is_raise(r)) {
            if (sa8_is_raise_pass(r)) { json_free(root); json_free(urls); json_free(types); return r; }
            json_free(r);
            json_t *d = sa8_o(); sa8_ss(d, "stage", "thread_root_recovery");
            json_t *dr = sa8_call(ctx, "logger.debug", d);
            if (sa8_is_raise(dr)) { json_free(root); json_free(urls); json_free(types); return dr; }
            json_free(dr);
            json_t *out = json_array(); json_append(out, urls); json_append(out, types); return out;
        }
        files = r;
    }
    long long maximg = sa8_gi(ctx->state, "_THREAD_ROOT_IMAGE_MAX", 4);
    size_t nf = (files && files->type == JSON_ARRAY) ? json_len(files) : 0;
    for (size_t i = 0; i < nf; i++) {
        if ((long long)json_len(urls) >= maximg) break;
        json_t *f = json_get(files, i);
        if (!f || f->type != JSON_OBJECT) continue;
        if (strcmp(json_get_str(f, "file_access", ""), "check_file_info") == 0) {
            json_t *b = sa8_o(); sa8_sc(b, "f", f);
            json_t *sf = sa8_call(ctx, "resolve_stub", b);
            if (sa8_is_raise(sf)) {
                if (sa8_is_raise_pass(sf)) { json_free(files); json_free(root); json_free(urls); json_free(types); return sf; }
                json_free(sf);
                json_t *w = sa8_o(); const char *idv = json_get_str(f, "id", "");
                if (!idv[0]) idv = json_get_str(f, "name", "");
                sa8_ss(w, "id", idv[0] ? idv : "unknown");
                json_t *wr = sa8_call(ctx, "logger.warning", w);
                if (sa8_is_raise(wr)) { json_free(files); json_free(root); json_free(urls); json_free(types); return wr; }
                json_free(wr);
                continue;
            }
            if (sf && sf->type == JSON_NULL) { json_free(sf); continue; }
            f = sf;
        }
        const json_t *mv = json_obj_get(f, "mimetype");
        char *mime = sa8_pystr(mv ? mv : json_null());
        const json_t *u1 = json_obj_get(f, "url_private_download");
        const char *url = sa8_truthy(u1) ? sa8_s(u1, "") : json_get_str(f, "url_private", "");
        if (!sa8_startswith(mime, "image/") || !url || !url[0]) { free(mime); continue; }
        json_t *d = sa8_o(); sa8_ss(d, "kind", "image"); sa8_sc(d, "f", f);
        sa8_ss(d, "url", url); sa8_ss(d, "mimetype", mime); sa8_ss(d, "team_id", team_id);
        free(mime);
        json_t *cf = sa8_call(ctx, "cache_slack_file", d);
        if (sa8_is_raise(cf)) {
            if (sa8_is_raise_pass(cf)) { json_free(files); json_free(root); json_free(urls); json_free(types); return cf; }
            json_free(cf);
            json_t *w = sa8_o(); const char *idv = json_get_str(f, "id", "");
            if (!idv[0]) idv = json_get_str(f, "name", "");
            sa8_ss(w, "id", idv[0] ? idv : "unknown");
            json_t *wr = sa8_call(ctx, "logger.warning", w);
            if (sa8_is_raise(wr)) { json_free(files); json_free(root); json_free(urls); json_free(types); return wr; }
            json_free(wr);
            continue;
        }
        json_append(urls, json_copy(json_get(cf, 0)));
        json_append(types, json_copy(json_get(cf, 1)));
        json_free(cf);
    }
    json_free(files); json_free(root);
    json_t *out = json_array(); json_append(out, urls); json_append(out, types);
    return out;
}

/* -- download_bytes: one attempt (returns content or NULL, sets *raised) -- */
static json_t *sa8_dl_once(sa8_ctx_t *ctx, const char *url, const json_t *token,
                           const char *html_label, json_t **raised) {
    const char *download_url = url;
    int rc = 0;
    while (1) {
        json_t *a = sa8_o(); sa8_ss(a, "url", download_url); sa8_sc(a, "token", token);
        char *auth = sa8_pystr(token);
        size_t an = strlen(auth) + 16;
        char *authbuf = (char *)malloc(an);
        snprintf(authbuf, an, "Bearer %s", auth);
        sa8_ss(a, "auth", authbuf);
        free(authbuf); free(auth);
        json_t *resp = sa8_call(ctx, "client_get", a);
        if (sa8_is_raise(resp)) { *raised = resp; return NULL; }
        long long status = sa8_gi(resp, "status_code", 200);
        if (!(status == 301 || status == 302 || status == 303 || status == 307 || status == 308))
            goto got_response;
        json_t *b = sa8_o(); sa8_sc(b, "response", resp);
        json_t *target = sa8_call(ctx, "redirect_target", b);
        if (sa8_is_raise(target)) { json_free(resp); *raised = target; return NULL; }
        if (!sa8_truthy(target)) { json_free(target); goto got_response; }
        const char *tstr = sa8_s(target, "");
        json_t *c = sa8_o(); sa8_ss(c, "url", tstr);
        json_t *s2 = sa8_call(ctx, "is_safe_url", c);
        if (sa8_is_raise(s2)) { json_free(target); json_free(resp); *raised = s2; return NULL; }
        if (!sa8_truthy(s2)) {
            json_free(s2); json_free(target); json_free(resp);
            char msg[512]; snprintf(msg, sizeof msg, "Blocked unsafe Slack file redirect (SSRF protection): %s", tstr);
            *raised = sa8_err("ValueError", msg); return NULL;
        }
        json_free(s2);
        json_t *c2 = sa8_o(); sa8_ss(c2, "url", tstr);
        json_t *c2r = sa8_call(ctx, "is_slack_cdn_url", c2);
        if (sa8_is_raise(c2r)) { json_free(target); json_free(resp); *raised = c2r; return NULL; }
        if (!sa8_truthy(c2r)) {
            json_free(c2r); json_free(target); json_free(resp);
            char msg[512]; snprintf(msg, sizeof msg, "Blocked non-Slack-CDN file redirect (token-exfiltration protection): %s", tstr);
            *raised = sa8_err("ValueError", msg); return NULL;
        }
        json_free(c2r);
        if (rc == 3) {
            json_free(target); json_free(resp);
            *raised = sa8_err("ValueError", "Too many Slack file redirects (maximum 3)"); return NULL;
        }
        download_url = sa8_strdup(tstr);
        rc++;
        json_free(target); json_free(resp);
        continue;
        got_response: {
            json_t *fs = sa8_o(); sa8_sn(fs, "status", (double)status);
            json_t *rs = sa8_call(ctx, "raise_for_status", fs);
            if (sa8_is_raise(rs)) { json_free(resp); *raised = rs; return NULL; }
            json_free(rs);
            const json_t *headers = json_obj_get(resp, "headers");
            const char *ct = headers ? json_get_str(headers, "content-type", "") : "";
            if (strstr(ct, "text/html")) {
                char msg[1024];
                snprintf(msg, sizeof msg,
                         "Slack returned HTML instead of %s (content-type: %s); check bot token scopes and file permissions, or a cross-origin redirect dropped the token (Enterprise Grid files-origin)",
                         html_label, ct);
                json_free(resp); *raised = sa8_err("ValueError", msg); return NULL;
            }
            json_t *content = json_obj_get(resp, "content");
            json_t *ret = content ? json_copy(content) : json_null();
            json_free(resp);
            return ret;
        }
    }
}

/* PoP: sa8_download_slack_file_bytes @ plugins/platforms/slack/adapter.py:_download_slack_file_bytes */
static json_t *sa8_download_slack_file_bytes(sa8_ctx_t *ctx, const json_t *args) {
    const char *url = json_get_str(args, "url", "");
    const char *team_id = json_get_str(args, "team_id", "");
    const char *html_label = json_get_str(args, "html_label", "file bytes");
    json_t *a = sa8_o(); sa8_ss(a, "url", url);
    json_t *safe = sa8_call(ctx, "is_safe_url", a);
    if (sa8_is_raise(safe)) return safe;
    if (!sa8_truthy(safe)) {
        json_free(safe);
        char msg[512]; snprintf(msg, sizeof msg, "Blocked unsafe Slack file URL (SSRF protection): %s", url);
        return sa8_err("ValueError", msg);
    }
    json_free(safe);
    json_t *b = sa8_o(); sa8_ss(b, "url", url);
    json_t *cdn = sa8_call(ctx, "is_slack_cdn_url", b);
    if (sa8_is_raise(cdn)) return cdn;
    if (!sa8_truthy(cdn)) {
        json_free(cdn);
        char msg[512]; snprintf(msg, sizeof msg, "Blocked non-Slack-CDN file URL (token-exfiltration protection): %s", url);
        return sa8_err("ValueError", msg);
    }
    json_free(cdn);
    json_t *c = sa8_o(); sa8_ss(c, "url", url); sa8_ss(c, "team_id", team_id);
    json_t *token = sa8_call(ctx, "resolve_download_token", c);
    if (sa8_is_raise(token)) return token;
    for (int attempt = 0; attempt < 3; attempt++) {
        json_t *raised = NULL;
        json_t *content = sa8_dl_once(ctx, url, token, html_label, &raised);
        if (!raised) { json_free(token); return content; }
        const char *kind = json_get_str(raised, "__raise__", "");
        long long sc = sa8_gi(raised, "status_code", 600);
        if (strcmp(kind, "HTTPStatusError") == 0 && sc < 429) { json_free(token); return raised; }
        if (strcmp(kind, "TimeoutException") != 0 && strcmp(kind, "HTTPStatusError") != 0) {
            json_free(token); return raised;
        }
        if (attempt < 2) {
            json_t *s = sa8_o(); sa8_sn(s, "seconds", 1.5 * (attempt + 1));
            json_t *sr = sa8_call(ctx, "sleep", s);
            if (sa8_is_raise(sr)) { json_free(raised); json_free(token); return sr; }
            json_free(sr); json_free(raised);
            continue;
        }
        json_free(token); return raised;
    }
    json_free(token);
    return json_null();
}

/* PoP: sa8_download_slack_file @ plugins/platforms/slack/adapter.py:_download_slack_file */
static json_t *sa8_download_slack_file(sa8_ctx_t *ctx, const json_t *args) {
    const char *url = json_get_str(args, "url", "");
    const char *ext = json_get_str(args, "ext", "");
    bool audio = json_get_bool(args, "audio", false);
    const char *team_id = json_get_str(args, "team_id", "");
    json_t *a = sa8_o(); sa8_ss(a, "url", url); sa8_ss(a, "team_id", team_id);
    sa8_ss(a, "html_label", "media");
    json_t *data = sa8_call(ctx, "download_bytes", a);
    if (sa8_is_raise(data)) return data;
    json_t *b = sa8_o(); sa8_sc(b, "data", data); sa8_ss(b, "ext", ext);
    sa8_sb(b, "audio", audio);
    json_free(data);
    return sa8_call(ctx, "cache_media", b);
}
