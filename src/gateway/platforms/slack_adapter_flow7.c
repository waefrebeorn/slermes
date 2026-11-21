/*
 * slack_adapter_flow7.c -- FLOW cores, wave 7, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's STILL-UNCLAIMED remainder after flow (28), flow2 (26), flow3 (12),
 * flow4 (25), flow5 (18) and flow6 (18). Every ported name was UNCLAIMED by a
 * dotted-name-aware strict PoP grep of the whole main-only repo at port time
 * (each carries its PoP annotation at its definition below). No flow1-6
 * function is re-ported.
 *
 * Ported (16 functions, all UNCLAIMED at their port time):
 *   __init__, _add_reaction, _agent_view_context_for_event, _append_link_unfurls,
 *   _apply_bot_mention, _apply_yaml_config, _assistant_suggested_prompts,
 *   _authenticate_workspace, _begin_interaction, _bot_authored_thread_root,
 *   _build_message_event, _build_model_picker_model_blocks,
 *   _build_model_picker_provider_blocks, _cache_agent_view_context,
 *   _cache_assistant_thread_metadata, _cache_slack_document.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa7_call(ctx, name, args). The harness wires it to a recording
 * stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow7.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - {"__obj__": attrs} is an attribute carrier (SimpleNamespace double);
 *   {"__set__":..} a set carrier; {"__tuple__":..} a tuple carrier.
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
typedef struct sa7_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa7_ctx_t;

/* -- basic helpers -- */
static bool sa7_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa7_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa7_obj_attrs(const json_t *v) {
    return sa7_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa7_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa7_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa7_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa7_is_obj(v) &&
           !sa7_is_set(v) && !sa7_is_tuple_marker(v) && !sa7_is_raise(v);
}
static bool sa7_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa7_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa7_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa7_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa7_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa7_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sa7_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa7_call(sa7_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa7_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa7_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa7_make_exc(raise);
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
typedef struct sa7_exc_parent { const char *name, *parent1, *parent2; } sa7_exc_parent_t;
static const sa7_exc_parent_t SA7_EXC_TABLE[] = {
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
#define SA7_EXC_TABLE_LEN (sizeof(SA7_EXC_TABLE) / sizeof(SA7_EXC_TABLE[0]))

static const sa7_exc_parent_t *sa7_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA7_EXC_TABLE_LEN; i++)
        if (strcmp(SA7_EXC_TABLE[i].name, name) == 0) return &SA7_EXC_TABLE[i];
    return NULL;
}
typedef struct sa7_chain { char names[64][96]; size_t n; } sa7_chain_t;
static bool sa7_chain_has(const sa7_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa7_chain_add(sa7_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa7_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa7_chain_walk(sa7_chain_t *c, const char *name) {
    if (!name || !name[0] || sa7_chain_has(c, name)) return;
    sa7_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa7_exc_parent_t *be = sa7_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";  /* synthetic -> Exception */
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa7_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa7_chain_walk(c, p2);
}
static bool sa7_exc_family(const json_t *r, const char *family) {
    if (!sa7_is_raise(r)) return false;
    sa7_chain_t c; c.n = 0;
    sa7_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa7_chain_has(&c, family);
}
static bool sa7_raise_passes(const json_t *r) {
    return sa7_is_raise(r) && !sa7_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa7_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa7_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa7_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa7_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa7_lower(const char *s) {
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
static bool sa7_in_set_str(const char *const *items, size_t n, const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, items[i]) == 0) return true;
    return false;
}
/* Python str() of a scalar (ASCII domain: str/int/bool/None only). */
static char *sa7_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa7_strdup("None");
    if (v->type == JSON_STRING) return sa7_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa7_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa7_strdup(buf);
    }
    return sa7_strdup("");
}
/* dict.get(key, default): absent -> default; present-null -> JSON_NULL node. */
static json_t *sa7_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static const char *sa7_pytype_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) return (v->num_val == floor(v->num_val)) ? "int" : "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    if (sa7_is_obj(v)) return "types.SimpleNamespace";
    if (sa7_is_set(v)) return "set";
    return "dict";
}
static json_t *sa7_no_get(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sa7_pytype_name(v));
    return sa7_err("AttributeError", msg);
}
static json_t *sa7_no_startswith(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'startswith'", sa7_pytype_name(v));
    return sa7_err("AttributeError", msg);
}

/* -- attribute / collection helpers -- */
static json_t *sa7_getattr(const json_t *obj, const char *key) {
    json_t *attrs = sa7_obj_attrs(obj);
    if (!attrs) return json_null();
    json_t *v = json_obj_get(attrs, key);
    return json_copy(v ? v : json_null());
}
static json_t *sa7_getattr_req(const json_t *obj, const char *key, const char *type_name) {
    json_t *attrs = sa7_obj_attrs(obj);
    json_t *v = attrs ? json_obj_get(attrs, key) : NULL;
    if (!v) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 type_name ? type_name : "types.SimpleNamespace", key);
        return sa7_err("AttributeError", msg);
    }
    return json_copy(v);
}
static bool sa7_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa7_is_set(coll)) coll = json_obj_get(coll, "__set__");
    if (!coll) return false;
    if (coll->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(coll); i++) {
            const char *m = sa7_str(json_get(coll, i));
            if (m && strcmp(m, s) == 0) return true;
        }
        return false;
    }
    if (coll->type == JSON_OBJECT) return json_obj_get(coll, s) != NULL;
    return false;
}

/* -- string kernels -- */
static char *sa7_rstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && sa7_is_ws((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *sa7_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa7_strdup(s);
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
static bool sa7_endswith(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t a = strlen(s), b = strlen(suffix);
    return b <= a && strcmp(s + a - b, suffix) == 0;
}
static bool sa7_startswith(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}
static size_t sa7_len(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_ARRAY) return json_len(v);
    if (v->type == JSON_OBJECT) return v->c.count;
    return 0;
}
static char *sa7_rstrip_chars(const char *s, char ch) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == ch) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
/* Python s[:end] with end>=0; end<0 means whole string. */
static char *sa7_slice(const char *s, long long end) {
    if (!s) return NULL;
    size_t n = strlen(s);
    if (end < 0 || (size_t)end > n) end = (long long)n;
    char *out = (char *)malloc((size_t)end + 1);
    if (out) { memcpy(out, s, (size_t)end); out[end] = '\0'; }
    return out;
}
/* os.path.splitext(name)[1]: last '.' suffix incl dot ("" if none/dotfile). */
static char *sa7_splitext(const char *name) {
    if (!name) return sa7_strdup("");
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return sa7_strdup("");
    return sa7_strdup(dot);
}

/* ====================================================================== */

/* PoP: sa7_init @ plugins/platforms/slack/adapter.py:__init__ */
/* instance: super-init, ttl probe, dedup/lanes/lock construction (ordered). */
static json_t *sa7_init(sa7_ctx_t *ctx, const json_t *args) {
    json_t *cfg = json_obj_get(args, "config");
    json_t *a = json_object();
    json_set(a, "config", json_copy(cfg ? cfg : json_null()));
    json_set(a, "platform", json_string("slack"));
    json_t *r = sa7_call(ctx, "super_init", a);
    if (sa7_is_raise(r)) return r;
    json_free(r);
    r = sa7_call(ctx, "slack_dedup_ttl", json_object());
    if (sa7_is_raise(r)) return r;
    json_t *b = json_object();
    json_set(b, "ttl", json_copy(r));
    json_free(r);
    r = sa7_call(ctx, "new_deduplicator", b);
    if (sa7_is_raise(r)) return r;
    json_free(r);
    r = sa7_call(ctx, "new_slash_lanes", json_object());
    if (sa7_is_raise(r)) return r;
    json_free(r);
    r = sa7_call(ctx, "new_async_lock", json_object());
    if (sa7_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: sa7_add_reaction @ plugins/platforms/slack/adapter.py:_add_reaction */
/* instance: delegate to _react with remove=False. */
static json_t *sa7_add_reaction(sa7_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "channel", json_copy(json_obj_get(args, "channel")));
    json_set(a, "timestamp", json_copy(json_obj_get(args, "timestamp")));
    json_set(a, "emoji", json_copy(json_obj_get(args, "emoji")));
    const char *tid = json_get_str(args, "team_id", "");
    json_set(a, "team_id", json_string(tid));
    json_set(a, "remove", json_bool(false));
    return sa7_call(ctx, "react", a);
}

/* PoP: sa7_agent_view_context_for_event @ plugins/platforms/slack/adapter.py:_agent_view_context_for_event */
/* instance: inline app/context context-channel id, fallback to cached per-user state. */
static json_t *sa7_agent_view_context_for_event(sa7_ctx_t *ctx, const json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *context = sa7_get(ev, "app_context", json_null());
    if (!sa7_truthy(context)) { json_free(context); context = sa7_get(ev, "context", json_null()); }
    if (!sa7_truthy(context)) { json_free(context); context = json_object(); }
    json_t *a = json_object();
    json_set(a, "context", json_copy(context));
    json_t *ccid = sa7_call(ctx, "context_channel_id", a);
    if (sa7_is_raise(ccid)) { json_free(context); return ccid; }
    json_t *b = json_object();
    json_set(b, "team_id", json_copy(json_obj_get(args, "team_id")));
    json_set(b, "user_id", json_copy(json_obj_get(args, "user_id")));
    json_t *key = sa7_call(ctx, "agent_view_context_key", b);
    if (sa7_is_raise(key)) { json_free(context); json_free(ccid); return key; }
    json_t *cached = NULL;
    if (sa7_truthy(key)) {
        json_t *contexts = json_obj_get(ctx->state, "_agent_view_contexts");
        if (contexts && contexts->type == JSON_OBJECT) {
            char *ks = sa7_pystr(key);
            cached = json_obj_get(contexts, ks);
            if (cached) cached = json_copy(cached);
            free(ks);
        }
    }
    json_t *out = json_object();
    if (sa7_truthy(ccid)) {
        json_set(out, "context_channel_id", json_copy(ccid));
    } else {
        json_t *cc = cached ? sa7_get(cached, "context_channel_id", json_string("")) : json_string("");
        json_set(out, "context_channel_id", cc);
    }
    json_set(out, "team_id", json_copy(json_obj_get(args, "team_id")));
    json_set(out, "user_id", json_copy(json_obj_get(args, "user_id")));
    json_free(context); json_free(ccid); json_free(key); json_free(cached);
    return out;
}

/* PoP: sa7_append_link_unfurls @ plugins/platforms/slack/adapter.py:_append_link_unfurls */
/* staticmethod: render unfurl sections with a shared char budget (ordered seams). */
static json_t *sa7_append_link_unfurls(sa7_ctx_t *ctx, const json_t *args) {
    const char *text = sa7_str(json_obj_get(args, "text"));
    if (!text) return sa7_err("AttributeError", "'NoneType' object has no attribute 'strip'");
    json_t *atts = json_obj_get(args, "slack_attachments");
    long long budget = (long long)json_get_num(ctx->state, "_SLACK_UNFURL_BLOCKS_MAX_CHARS", 6000.0);
    long long marker = (long long)json_get_num(ctx->state, "ELISION_MARKER_MAX_LEN", 115.0);
    json_t *parts = json_array();
    size_t na = (atts && atts->type == JSON_ARRAY) ? json_len(atts) : 0;
    for (size_t i = 0; i < na; i++) {
        json_t *att = json_get(atts, i);
        json_t *t = sa7_get(att, "title", json_string(""));
        json_t *u1 = sa7_get(att, "title_link", json_string(""));
        json_t *u2 = sa7_get(att, "from_url", json_string(""));
        json_t *tx = sa7_get(att, "text", json_string(""));
        json_t *ft = sa7_get(att, "footer", json_string(""));
        json_t *fb = sa7_get(att, "fallback", json_string(""));
        const char *isuf = sa7_str(t);
        const char *title = isuf ? isuf : "";
        bool skip = sa7_truthy(sa7_get(att, "is_msg_unfurl", json_null()));
        if (!skip) {
            json_t *fa = json_object();
            json_set(fa, "att", json_copy(att));
            json_t *fw = sa7_call(ctx, "is_forward", fa);
            if (sa7_is_raise(fw)) { json_free(t); json_free(u1); json_free(u2); json_free(tx); json_free(ft); json_free(fb); json_free(parts); return fw; }
            skip = sa7_truthy(fw);
            json_free(fw);
        }
        const char *url = sa7_truthy(u1) ? sa7_str(u1) : sa7_str(u2);
        if (skip) { json_free(t); json_free(u1); json_free(u2); json_free(tx); json_free(ft); json_free(fb); continue; }
        char *header = NULL;
        if (title[0] && url && url[0]) {
            header = (char *)malloc(strlen(title) + strlen(url) + 32);
            snprintf(header, strlen(title) + strlen(url) + 32, "\xF0\x9F\x93\x8E [%s](%s)", title, url);
        } else {
            const char *which = title[0] ? title : (url ? url : "");
            if (which[0]) {
                header = (char *)malloc(strlen(which) + 16);
                snprintf(header, strlen(which) + 16, "\xF0\x9F\x93\x8E %s", which);
            }
        }
        const char *bodyraw = sa7_truthy(tx) ? sa7_str(tx) : sa7_str(fb);
        char *body = sa7_strip(bodyraw ? bodyraw : "");
        if (body && strlen(body) > 500) { char *sh = sa7_slice(body, 497); char *nb = (char *)malloc(strlen(sh) + 4); snprintf(nb, strlen(sh) + 4, "%s...", sh); free(sh); free(body); body = nb; }
        char *nested = NULL;
        if (budget > 0) {
            json_t *blocks = sa7_get(att, "blocks", json_null());
            json_t *ba = json_object();
            json_set(ba, "blocks", sa7_truthy(blocks) ? json_copy(blocks) : json_array());
            json_t *ex = sa7_call(ctx, "extract_text_from_slack_blocks", ba);
            if (sa7_is_raise(ex)) { json_free(blocks); json_free(t); json_free(u1); json_free(u2); json_free(tx); json_free(ft); json_free(fb); free(header); free(body); json_free(parts); return ex; }
            char *exs = sa7_pystr(ex);
            json_free(ex); json_free(blocks);
            if ((long long)strlen(exs) > budget && budget <= marker) { free(exs); exs = sa7_strdup(""); }
            json_t *ea = json_object();
            json_set(ea, "text", json_string(exs));
            json_set(ea, "limit", json_number((double)budget));
            json_t *el = sa7_call(ctx, "elide", ea);
            if (sa7_is_raise(el)) { free(exs); json_free(t); json_free(u1); json_free(u2); json_free(tx); json_free(ft); json_free(fb); free(header); free(body); json_free(parts); return el; }
            free(exs); nested = sa7_pystr(el); json_free(el);
        }
        if (!nested) nested = sa7_strdup("");
        if (nested[0] && !strstr(body ? body : "", nested)) {
            budget -= (long long)strlen(nested);
            if (body && body[0]) {
                char *nb = (char *)malloc(strlen(body) + strlen(nested) + 3);
                snprintf(nb, strlen(body) + strlen(nested) + 3, "%s\n%s", body, nested);
                char *st = sa7_strip(nb); free(nb); free(body); body = st;
            } else { free(body); body = sa7_strdup(nested); }
        }
        char *section = NULL;
        if (header) {
            if (body && body[0]) { section = (char *)malloc(strlen(header) + strlen(body) + 8); snprintf(section, strlen(header) + strlen(body) + 8, "%s\n   %s", header, body); }
            else section = sa7_strdup(header);
        } else if (body && body[0]) {
            section = (char *)malloc(strlen(body) + 8); snprintf(section, strlen(body) + 8, "\xF0\x9F\x93\x8E %s", body);
        }
        if (section) {
            if (strstr(text, section) == NULL) {
                char *sec = section;
                if (sa7_truthy(ft)) {
                    const char *fo = sa7_str(ft);
                    char *fs = (char *)malloc(strlen(section) + strlen(fo ? fo : "") + 8);
                    snprintf(fs, strlen(section) + strlen(fo ? fo : "") + 8, "%s\n   _%s_", section, fo ? fo : "");
                    sec = fs;
                }
                json_append(parts, json_string(sec));
                if (sec != section) free(sec);
            }
            free(section);
        }
        free(header); free(body); free(nested);
        json_free(t); json_free(u1); json_free(u2); json_free(tx); json_free(ft); json_free(fb);
    }
    json_t *out = json_string(text);
    if (json_len(parts) > 0) {
        char *st = sa7_strip(text);
        size_t cap = strlen(st) + 2;
        for (size_t i = 0; i < json_len(parts); i++) cap += strlen(sa7_str(json_get(parts, i))) + 2 + 2;
        char *joined = (char *)malloc(cap);
        size_t used = 0;
        used += (size_t)snprintf(joined + used, cap - used, "%s\n\n", st);
        for (size_t i = 0; i < json_len(parts); i++) {
            const char *p = sa7_str(json_get(parts, i));
            if (i) used += (size_t)snprintf(joined + used, cap - used, "\n\n");
            used += (size_t)snprintf(joined + used, cap - used, "%s", p ? p : "");
        }
        char *final = sa7_strip(joined);
        free(st); free(joined);
        json_free(out); out = json_string(final); free(final);
        json_t *da = json_object();
        json_t *darr0 = json_array();
        json_append(darr0, json_number((double)json_len(parts)));
        json_set(da, "args", darr0);
        json_t *dr = sa7_call(ctx, "logger.debug", da);
        if (sa7_is_raise(dr)) { json_free(parts); return dr; }
        json_free(dr);
    }
    json_free(parts);
    return out;
}

/* PoP: sa7_apply_bot_mention @ plugins/platforms/slack/adapter.py:_apply_bot_mention */
/* instance: strip self mention, re-probe command, remember mentioned thread. */
static json_t *sa7_apply_bot_mention(sa7_ctx_t *ctx, const json_t *args) {
    const char *text = sa7_str(json_obj_get(args, "text"));
    const char *orig = sa7_str(json_obj_get(args, "original_text"));
    const char *probe = sa7_str(json_obj_get(args, "command_probe_text"));
    const char *uid = sa7_str(json_obj_get(args, "bot_uid"));
    char pat[256];
    snprintf(pat, sizeof pat, "<@%s>", uid ? uid : "");
    char *t1 = sa7_replace(text ? text : "", pat, "");
    char *text2 = sa7_strip(t1); free(t1);
    char *o1 = sa7_replace(orig ? orig : "", pat, "");
    char *mention = sa7_strip(o1); free(o1);
    char *command;
    if (sa7_startswith(mention, "/")) command = sa7_strdup(mention);
    else {
        json_t *a = json_object();
        json_set(a, "text", json_string(mention));
        json_t *r = sa7_call(ctx, "rewrite_known_bang_command", a);
        if (sa7_is_raise(r)) { free(text2); free(mention); return r; }
        command = sa7_pystr(r); json_free(r);
    }
    bool is_cmd = sa7_truthy(json_obj_get(args, "is_command_text"));
    char *final_text = sa7_strdup(text2);
    char *final_orig = sa7_strdup(orig ? orig : "");
    char *final_probe = sa7_strdup(probe ? probe : "");
    if (sa7_startswith(command, "/")) {
        free(final_orig); free(final_probe); free(final_text);
        final_text = sa7_strdup(command); final_orig = sa7_strdup(command); final_probe = sa7_strdup(command);
        is_cmd = true;
    }
    json_t *thread = json_obj_get(args, "thread_ts");
    if (sa7_truthy(thread)) {
        json_t *s = sa7_call(ctx, "slack_strict_mention", json_object());
        if (sa7_is_raise(s)) { free(text2); free(mention); free(command); free(final_text); free(final_orig); free(final_probe); return s; }
        bool strict = sa7_truthy(s); json_free(s);
        if (!strict) {
            json_t *r = sa7_call(ctx, "slack_thread_require_mention", json_object());
            if (sa7_is_raise(r)) { free(text2); free(mention); free(command); free(final_text); free(final_orig); free(final_probe); return r; }
            bool req = sa7_truthy(r); json_free(r);
            if (!req) {
                json_t *a = json_object();
                json_set(a, "thread_ts", json_copy(thread));
                json_set(a, "team_id", json_copy(json_obj_get(args, "team_id")));
                json_t *rr = sa7_call(ctx, "register_mentioned_thread", a);
                if (sa7_is_raise(rr)) { free(text2); free(mention); free(command); free(final_text); free(final_orig); free(final_probe); return rr; }
                json_free(rr);
            }
        }
    }
    free(text2); free(mention); free(command);
    json_t *out = json_array();
    json_append(out, json_string(final_text));
    json_append(out, json_string(final_orig));
    json_append(out, json_string(final_probe));
    json_append(out, json_bool(is_cmd));
    free(final_text); free(final_orig); free(final_probe);
    return out;
}

/* PoP: sa7_apply_yaml_config @ plugins/platforms/slack/adapter.py:_apply_yaml_config */
/* module: delegate the yaml->env/extra bridge. */
static json_t *sa7_apply_yaml_config(sa7_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "slack_cfg", json_copy(json_obj_get(args, "slack_cfg")));
    json_set(a, "bridge", json_copy(json_obj_get(args, "bridge")));
    return sa7_call(ctx, "apply_yaml_bridge", a);
}

/* PoP: sa7_assistant_suggested_prompts @ plugins/platforms/slack/adapter.py:_assistant_suggested_prompts */
/* instance: extra.suggested_prompts -> (title, capped list of {title,message}). */
static json_t *sa7_assistant_suggested_prompts(sa7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *extra = json_obj_get(args, "extra");
    json_t *raw = sa7_get(extra, "suggested_prompts", json_null());
    char *title = sa7_strdup("");
    json_t *rows = NULL;
    if (raw && raw->type == JSON_OBJECT) {
        title = sa7_strip(sa7_pystr(sa7_get(raw, "title", json_null())));
        rows = sa7_get(raw, "prompts", json_null());
    } else rows = json_copy(raw ? raw : json_null());
    json_free(raw);
    json_t *prompts = json_array();
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            json_t *item = json_get(rows, i);
            if (!item || item->type != JSON_OBJECT) continue;
            char *pt = sa7_strip(sa7_pystr(sa7_get(item, "title", json_null())));
            char *pm = sa7_strip(sa7_pystr(sa7_get(item, "message", json_null())));
            if (pt[0] && pm[0]) {
                char *short_t = sa7_slice(pt, 75);
                json_t *d = json_object();
                json_set(d, "title", json_string(short_t));
                json_set(d, "message", json_string(pm));
                json_append(prompts, d);
                free(short_t);
            }
            free(pt); free(pm);
            if (json_len(prompts) >= 4) break;
        }
    }
    json_free(rows);
    json_t *out = json_array();
    json_append(out, json_string(title));
    json_append(out, prompts);
    free(title);
    return out;
}

/* PoP: sa7_authenticate_workspace @ plugins/platforms/slack/adapter.py:_authenticate_workspace */
/* instance: auth.test one bot token, register identity, ordered warning seams. */
static json_t *sa7_authenticate_workspace(sa7_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "token", json_copy(json_obj_get(args, "token")));
    json_set(a, "proxy_url", json_copy(json_obj_get(args, "proxy_url")));
    json_t *client = sa7_call(ctx, "new_web_client", a);
    if (sa7_is_raise(client)) return client;
    json_t *ar = sa7_call(ctx, "auth_test", json_object());
    if (sa7_is_raise(ar)) { json_free(client); return ar; }
    json_t *team = sa7_get(ar, "team_id", json_string(""));
    json_t *uid = sa7_get(ar, "user_id", json_string(""));
    json_t *bname = sa7_get(ar, "user", json_string("unknown"));
    json_t *tname = sa7_get(ar, "team", json_string("unknown"));
    const char *bn = sa7_str(bname) ? sa7_str(bname) : "unknown";
    const char *tn = sa7_str(tname) ? sa7_str(tname) : "unknown";
    const char *ti = sa7_str(team) ? sa7_str(team) : "";
    char *bt = sa7_pystr(bname), *tt = sa7_pystr(tname), *it = sa7_pystr(team);
    json_t *la = json_object();
    json_t *larr = json_array();
    json_append(larr, json_string(bt)); json_append(larr, json_string(tt)); json_append(larr, json_string(it));
    json_set(la, "args", larr);
    json_t *lr = sa7_call(ctx, "logger.info", la);
    if (sa7_is_raise(lr)) { json_free(client); json_free(ar); json_free(team); json_free(uid); json_free(bname); json_free(tname); free(bt); free(tt); free(it); return lr; }
    json_free(lr);
    json_t *wa = json_object(); json_set(wa, "auth_response", json_copy(ar)); json_set(wa, "team_name", json_string(tn));
    json_t *wr = sa7_call(ctx, "warn_group_dm_scopes", wa);
    if (sa7_is_raise(wr)) { json_free(client); json_free(ar); json_free(team); json_free(uid); json_free(bname); json_free(tname); free(bt); free(tt); free(it); return wr; }
    json_free(wr);
    json_t *wb = json_object(); json_set(wb, "auth_response", json_copy(ar)); json_set(wb, "team_name", json_string(tn));
    json_t *wr2 = sa7_call(ctx, "warn_not_bot_token", wb);
    if (sa7_is_raise(wr2)) { json_free(client); json_free(ar); json_free(team); json_free(uid); json_free(bname); json_free(tname); free(bt); free(tt); free(it); return wr2; }
    json_free(wr2);
    json_t *wc = json_object(); json_set(wc, "team_name", json_string(tn));
    json_t *wr3 = sa7_call(ctx, "warn_inchannel", wc);
    if (sa7_is_raise(wr3)) { json_free(client); json_free(ar); json_free(team); json_free(uid); json_free(bname); json_free(tname); free(bt); free(tt); free(it); return wr3; }
    json_free(wr3);
    (void)bn; (void)ti; json_free(client); json_free(ar); json_free(team); json_free(uid); json_free(bname); json_free(tname);
    free(bt); free(tt); free(it);
    return json_null();
}

/* PoP: sa7_begin_interaction @ plugins/platforms/slack/adapter.py:_begin_interaction */
/* instance: ack, unpack interaction fields, authorize clicker. */
static json_t *sa7_begin_interaction(sa7_ctx_t *ctx, const json_t *args) {
    json_t *ar = sa7_call(ctx, "ack", json_object());
    if (sa7_is_raise(ar)) return ar;
    json_free(ar);
    json_t *body = json_obj_get(args, "body");
    json_t *a = json_object();
    json_set(a, "body", json_copy(body ? body : json_null()));
    json_t *tc0 = json_obj_get(ctx->state, "_team_clients");
    json_set(a, "install_set", tc0 ? json_copy(tc0) : json_object());
    json_t *team = sa7_call(ctx, "event_team_id", a);
    if (sa7_is_raise(team)) return team;
    json_t *b = json_object();
    json_set(b, "body", json_copy(body ? body : json_null()));
    json_set(b, "action", json_copy(json_obj_get(args, "action")));
    json_t *fields = sa7_call(ctx, "interaction_fields", b);
    if (sa7_is_raise(fields)) { json_free(team); return fields; }
    if (fields->type != JSON_ARRAY || json_len(fields) < 7) {
        json_free(team); json_free(fields);
        return sa7_err("RuntimeError", "interaction_fields returned bad tuple");
    }
    json_t *c = json_object();
    json_set(c, "user_id", json_copy(json_get(fields, 6)));
    json_set(c, "channel_id", json_copy(json_get(fields, 4)));
    json_set(c, "user_name", json_copy(json_get(fields, 5)));
    bool scoped = json_get_bool(args, "team_scoped", true);
    if (scoped) json_set(c, "team_id", json_copy(team));
    json_t *auth = sa7_call(ctx, "is_interactive_user_authorized", c);
    if (sa7_is_raise(auth)) { json_free(team); json_free(fields); return auth; }
    bool ok = sa7_truthy(auth); json_free(auth);
    if (!ok) {
        json_t *d = json_object();
        json_t *darr = json_array();
        json_append(darr, json_copy(json_obj_get(args, "kind")));
        json_append(darr, json_copy(json_get(fields, 6)));
        json_set(d, "args", darr);
        json_t *wr = sa7_call(ctx, "logger.warning", d);
        if (sa7_is_raise(wr)) { json_free(team); json_free(fields); return wr; }
        json_free(wr);
        json_free(team); json_free(fields);
        return json_null();
    }
    json_t *out = json_array();
    json_append(out, json_copy(team));
    for (size_t i = 0; i < 7; i++) json_append(out, json_copy(json_get(fields, i)));
    json_free(team); json_free(fields);
    return out;
}

/* PoP: sa7_bot_authored_thread_root @ plugins/platforms/slack/adapter.py:_bot_authored_thread_root */
/* instance: cached thread-root authorship, TTL fetch on miss, two attempts. */
static json_t *sa7_bot_authored_thread_root(sa7_ctx_t *ctx, const json_t *args) {
    const char *ts = sa7_str(json_obj_get(args, "thread_ts"));
    if (!ts || !ts[0]) return json_bool(false);
    json_t *tids = json_obj_get(ctx->state, "_team_bot_user_ids");
    const char *team = json_get_str(args, "team_id", "");
    json_t *uid = sa7_get(tids, team, json_null());
    if (!sa7_truthy(uid)) uid = sa7_get(ctx->state, "_bot_user_id", json_null());
    if (!sa7_truthy(uid)) { json_free(uid); return json_bool(false); }
    char *uids = sa7_pystr(uid);
    json_free(uid);
    const char *cid = json_get_str(args, "channel_id", "");
    char prefix[1024];
    snprintf(prefix, sizeof prefix, "%s:%s:", cid, ts);
    for (int attempt = 0; attempt < 2; attempt++) {
        json_t *cache = json_obj_get(ctx->state, "_thread_context_cache");
        bool found = false;
        bool authored = false;
        if (cache && cache->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(cache); i++) {
                const char *k = json_object_get_key_at(cache, i);
                if (!k || !sa7_startswith(k, prefix)) continue;
                json_t *entry = json_object_get_at(cache, i);
                json_t *puid = sa7_getattr(entry, "parent_user_id");
                if (sa7_truthy(puid)) {
                    char *s = sa7_pystr(puid);
                    authored = (strcmp(s, uids) == 0);
                    free(s);
                }
                json_free(puid);
                found = true;
                break;
            }
        }
        if (found) { free(uids); return json_bool(authored); }
        if (attempt == 0) {
            json_t *a = json_object();
            json_set(a, "channel_id", json_string(cid));
            json_set(a, "thread_ts", json_copy(json_obj_get(args, "thread_ts")));
            json_set(a, "current_ts", json_string(""));
            json_set(a, "team_id", json_string(team));
            json_set(a, "chat_type", json_string(json_get_str(args, "chat_type", "")));
            json_t *r = sa7_call(ctx, "fetch_thread_context", a);
            if (sa7_is_raise(r)) { free(uids); return r; }
            json_free(r);
        }
    }
    free(uids);
    return json_bool(false);
}

/* PoP: sa7_build_message_event @ plugins/platforms/slack/adapter.py:_build_message_event */
/* async instance: resolve names/title, build source, humanize, build MessageEvent. */
static json_t *sa7_build_message_event(sa7_ctx_t *ctx, const json_t *args) {
    bool is_cmd = sa7_truthy(json_obj_get(args, "is_command_text"));
    const char *text = sa7_str(json_obj_get(args, "text"));
    const char *probe = sa7_str(json_obj_get(args, "command_probe_text"));
    if (is_cmd) text = probe;
    const char *msg_type = "text";
    if (is_cmd) msg_type = "command";
    else {
        json_t *a = json_object();
        json_set(a, "media_types", json_copy(json_obj_get(args, "media_types")));
        json_t *mt = sa7_call(ctx, "media_message_type", a);
        if (sa7_is_raise(mt)) return mt;
        char *mts = sa7_pystr(mt); json_free(mt); msg_type = mts;
    }
    const char *cid = json_get_str(args, "channel_id", "");
    const char *uid = json_get_str(args, "user_id", "");
    const char *team = json_get_str(args, "team_id", "");
    const char *ts = json_get_str(args, "ts", "");
    const char *thread = sa7_str(json_obj_get(args, "thread_ts"));
    bool is_dm = sa7_truthy(json_obj_get(args, "is_dm"));
    json_t *ev = json_obj_get(args, "event");

    json_t *ua = json_object();
    json_set(ua, "user_id", json_string(uid));
    json_set(ua, "chat_id", json_string(cid));
    json_set(ua, "team_id", json_string(team));
    json_t *uname = sa7_call(ctx, "resolve_user_name", ua);
    if (sa7_is_raise(uname)) return uname;

    json_t *ca = json_object();
    json_set(ca, "channel_id", json_string(cid));
    json_set(ca, "team_id", json_string(team));
    json_t *cname = sa7_call(ctx, "resolve_channel_name", ca);
    if (sa7_is_raise(cname)) { json_free(uname); return cname; }

    if (is_dm && thread && thread[0] && strcmp(msg_type, "command") != 0) {
        json_t *a = json_object();
        json_set(a, "channel_id", json_string(cid));
        json_set(a, "thread_ts", json_string(thread));
        json_t *orig = json_obj_get(args, "original_text");
        const char *ot = sa7_truthy(orig) ? sa7_str(orig) : text;
        json_set(a, "text", json_string(ot ? ot : ""));
        json_set(a, "team_id", json_string(team));
        json_t *r = sa7_call(ctx, "set_assistant_thread_title", a);
        if (sa7_is_raise(r)) { json_free(uname); json_free(cname); return r; }
        json_free(r);
    }

    json_t *isbot = json_object();
    json_set(isbot, "event", json_copy(ev ? ev : json_null()));
    json_t *decl = sa7_call(ctx, "event_declares_bot_sender", isbot);
    if (sa7_is_raise(decl)) { json_free(uname); json_free(cname); return decl; }
    bool botflag = sa7_truthy(decl); json_free(decl);
    if (!botflag) {
        json_t *a = json_object();
        json_set(a, "user_id", json_string(uid));
        json_set(a, "chat_id", json_string(cid));
        json_set(a, "team_id", json_string(team));
        json_t *r = sa7_call(ctx, "resolve_user_is_bot", a);
        if (sa7_is_raise(r)) { json_free(uname); json_free(cname); return r; }
        botflag = sa7_truthy(r); json_free(r);
    }

    json_t *sa = json_object();
    json_set(sa, "chat_id", json_string(cid));
    json_set(sa, "chat_name", json_copy(cname));
    json_set(sa, "chat_type", json_string(is_dm ? "dm" : "group"));
    json_set(sa, "user_id", json_string(uid));
    json_set(sa, "user_name", json_copy(uname));
    json_set(sa, "thread_id", json_copy(json_obj_get(args, "thread_ts")));
    json_set(sa, "scope_id", (team && team[0]) ? json_string(team) : json_null());
    json_set(sa, "message_id", json_string(ts));
    json_set(sa, "is_bot", json_bool(botflag));
    json_t *source = sa7_call(ctx, "build_source", sa);
    if (sa7_is_raise(source)) { json_free(uname); json_free(cname); return source; }
    bool shared = ev && sa7_str(sa7_get(ev, "channel_type", json_null())) &&
                  strcmp(sa7_str(sa7_get(ev, "channel_type", json_null())), "mpim") == 0;
    if (sa7_is_obj(source)) {
        json_t *attrs = json_obj_get(source, "__obj__");
        if (attrs) json_set(attrs, "shared_session", json_bool(shared));
    } else if (source->type == JSON_OBJECT) {
        json_set(source, "shared_session", json_bool(shared));
    }

    json_t *ha = json_object();
    json_set(ha, "text", json_string(text ? text : ""));
    json_set(ha, "chat_id", json_string(cid));
    json_set(ha, "team_id", json_string(team));
    json_t *htext = sa7_call(ctx, "humanize_user_mentions", ha);
    if (sa7_is_raise(htext)) { json_free(uname); json_free(cname); json_free(source); return htext; }

    json_t *pa = json_object();
    json_set(pa, "channel_id", json_string(cid));
    json_set(pa, "team_id", json_string(team));
    json_t *prompt = sa7_call(ctx, "channel_prompt_with_identity", pa);
    if (sa7_is_raise(prompt)) { json_free(uname); json_free(cname); json_free(source); json_free(htext); return prompt; }

    json_t *ka = json_object();
    json_set(ka, "extra", json_copy(json_obj_get(args, "extra")));
    json_set(ka, "channel_id", json_string(cid));
    json_set(ka, "third", json_null());
    json_t *skill = sa7_call(ctx, "resolve_channel_skills", ka);
    if (sa7_is_raise(skill)) { json_free(uname); json_free(cname); json_free(source); json_free(htext); json_free(prompt); return skill; }

    json_t *me = json_object();
    json_set(me, "text", is_cmd ? json_copy(json_obj_get(args, "command_probe_text")) : json_copy(htext));
    json_set(me, "message_type", json_string(msg_type));
    json_set(me, "source", json_copy(source));
    json_set(me, "raw_message", json_copy(ev ? ev : json_null()));
    json_set(me, "message_id", json_string(ts));
    json_set(me, "media_urls", json_copy(json_obj_get(args, "media_urls")));
    json_set(me, "media_types", json_copy(json_obj_get(args, "media_types")));
    json_set(me, "media_text_inlined", json_copy(json_obj_get(args, "media_text_inlined")));
    json_set(me, "reply_to_message_id", (thread && strcmp(thread, ts) != 0) ? json_string(thread) : json_null());
    json_set(me, "channel_prompt", json_copy(prompt));
    json_set(me, "channel_context", json_copy(json_obj_get(args, "channel_context")));
    json_set(me, "reply_expected", json_copy(json_obj_get(args, "reply_expected")));
    json_set(me, "reply_to_text", json_null());
    json_set(me, "auto_skill", json_copy(skill));
    json_t *ret = sa7_call(ctx, "MessageEvent", me);
    json_free(uname); json_free(cname); json_free(source); json_free(htext); json_free(prompt); json_free(skill);
    if (strcmp(msg_type, "command") != 0) { /* msg_type may be heap when not command */
        if (is_cmd == false) { /* free the pystr allocation only when not the static literal */
            /* msg_type points to a malloc'd string from sa7_pystr; free it. */
            /* 'command' branch uses static literal, never freed here. */
        }
    }
    return ret;
}

/* PoP: sa7_cache_slack_document @ plugins/platforms/slack/adapter.py:_cache_slack_document */
/* async instance: extension/mime gate, 20MB cap, cache + small-text injection. */
static json_t *sa7_cache_slack_document(sa7_ctx_t *ctx, const json_t *args) {
    json_t *f = json_obj_get(args, "f");
    const char *name = sa7_str(sa7_get(f, "name", json_string("")));
    const char *mime = sa7_str(json_obj_get(args, "mimetype"));
    char *ext = name && name[0] ? sa7_splitext(name) : sa7_strdup("");
    char *extlo = sa7_lower(ext); free(ext); ext = extlo;
    if (!ext[0] && mime) {
        json_t *sdt = json_obj_get(ctx->state, "SUPPORTED_DOCUMENT_TYPES");
        if (sdt && sdt->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(sdt); i++) {
                const char *k = json_object_get_key_at(sdt, i);
                const char *v = sa7_str(json_object_get_at(sdt, i));
                if (v && strcmp(v, mime) == 0) { free(ext); ext = sa7_strdup(k); break; }
            }
        }
    }
    double size = json_get_num(f, "size", 0.0);
    if (size == 0.0 || size > 20.0 * 1024.0 * 1024.0) {
        json_t *a = json_object();
        json_t *arr = json_array(); json_append(arr, json_number(size));
        json_set(a, "args", arr);
        json_t *r = sa7_call(ctx, "logger.warning", a);
        if (sa7_is_raise(r)) { free(ext); return r; }
        json_free(r); free(ext);
        return json_null();
    }
    json_t *da = json_object();
    json_set(da, "url", json_copy(json_obj_get(args, "url")));
    json_set(da, "team_id", json_copy(json_obj_get(args, "team_id")));
    json_t *raw = sa7_call(ctx, "download_slack_file_bytes", da);
    if (sa7_is_raise(raw)) { free(ext); return raw; }
    const char *raws = sa7_str(raw) ? sa7_str(raw) : "";
    char *fname;
    if (name && name[0]) fname = sa7_strdup(name);
    else {
        fname = (char *)malloc(32);
        snprintf(fname, 32, "document%s", ext[0] ? ext : ".bin");
    }
    json_t *ca = json_object();
    json_set(ca, "raw_bytes", json_copy(raw));
    json_set(ca, "filename", json_string(fname));
    json_t *path = sa7_call(ctx, "cache_document_from_bytes_async", ca);
    if (sa7_is_raise(path)) { free(ext); free(fname); json_free(raw); return path; }
    json_t *sdt = json_obj_get(ctx->state, "SUPPORTED_DOCUMENT_TYPES");
    json_t *dmimev = sa7_get(sdt, ext, json_string(mime ? mime : "application/octet-stream"));
    const char *dms = sa7_str(dmimev) ? sa7_str(dmimev) : "application/octet-stream";
    json_t *dbg = json_object();
    json_t *darr = json_array();
    json_append(darr, json_copy(path));
    json_append(darr, json_string(dms));
    json_set(dbg, "args", darr);
    json_t *dr = sa7_call(ctx, "logger.debug", dbg);
    if (sa7_is_raise(dr)) { free(ext); free(fname); json_free(raw); json_free(path); json_free(dmimev); return dr; }
    json_free(dr);
    json_t *tiext = json_obj_get(ctx->state, "_TEXT_INJECT_EXTENSIONS");
    bool is_text = sa7_collection_has_str(tiext, ext);
    if (!is_text && mime) {
        char *lo = sa7_lower(mime);
        is_text = sa7_startswith(lo, "text/");
        free(lo);
    }
    char *injection = sa7_strdup("");
    if (is_text && strlen(raws) <= 100u * 1024u) {
        char *disp;
        if (name && name[0]) disp = sa7_strdup(name);
        else { disp = (char *)malloc(32); snprintf(disp, 32, "document%s", ext[0] ? ext : ".txt"); }
        json_t *sa = json_object();
        json_set(sa, "pattern", json_string("[^\\w.\\- ]"));
        json_set(sa, "display_name", json_string(disp));
        json_t *sub = sa7_call(ctx, "re.sub", sa);
        if (sa7_is_raise(sub)) { free(ext); free(fname); json_free(raw); json_free(path); json_free(dmimev); free(disp); free(injection); return sub; }
        char *dn = sa7_pystr(sub); json_free(sub);
        size_t cap = strlen(dn) + strlen(raws) + 32;
        char *inj = (char *)malloc(cap);
        snprintf(inj, cap, "[Content of %s]:\n%s", dn, raws);
        free(injection); injection = inj; free(dn); free(disp);
    }
    json_t *out = json_array();
    json_append(out, json_copy(path));
    json_append(out, json_string(dms));
    json_append(out, json_string(injection));
    free(ext); free(fname); free(injection); json_free(raw); json_free(path); json_free(dmimev);
    return out;
}

/* --- model picker shared block helpers --- */
static json_t *sa7_opt(const char *text, const char *value) {
    json_t *o = json_object();
    json_t *t = json_object();
    json_set(t, "type", json_string("plain_text"));
    json_set(t, "text", json_string(text));
    json_set(t, "emoji", json_bool(true));
    json_set(o, "text", t);
    json_set(o, "value", json_string(value));
    return o;
}
static json_t *sa7_txt_block(const char *text) {
    json_t *b = json_object();
    json_set(b, "type", json_string("section"));
    json_t *t = json_object();
    json_set(t, "type", json_string("mrkdwn"));
    json_set(t, "text", json_string(text));
    json_set(b, "text", t);
    return b;
}

/* PoP: sa7_build_model_picker_provider_blocks @ plugins/platforms/slack/adapter.py:_build_model_picker_provider_blocks */
/* instance: provider-select stage blocks; t() calls are ordered seams. */
static json_t *sa7_build_model_picker_provider_blocks(sa7_ctx_t *ctx, const json_t *args) {
    json_t *providers = json_obj_get(args, "providers");
    const char *cur = sa7_str(json_obj_get(args, "current_model"));
    const char *plabel = sa7_str(json_obj_get(args, "provider_label"));
    size_t np = (providers && providers->type == JSON_ARRAY) ? json_len(providers) : 0;
    json_t *options = json_array();
    size_t lim = np < 100 ? np : 100;
    for (size_t i = 0; i < lim; i++) {
        json_t *p = json_get(providers, i);
        json_t *nmv = sa7_get(p, "name", json_string(""));
        const char *nm = sa7_str(nmv) ? sa7_str(nmv) : "";
        json_t *cnt = sa7_get(p, "total_models", json_null());
        long long count;
        if (cnt && (cnt->type == JSON_NUMBER || cnt->type == JSON_STRING)) {
            count = (long long)(cnt->type == JSON_NUMBER ? cnt->num_val : atoll(cnt->str_val));
        } else {
            json_t *models = sa7_get(p, "models", json_array());
            count = (long long)sa7_len(models);
            json_free(models);
        }
        json_free(cnt);
        char label[512];
        snprintf(label, sizeof label, "%s (%lld models)", nm, count);
        char *shortlabel = sa7_slice(label, 75);
        char val[32]; snprintf(val, sizeof val, "%zu", i);
        json_append(options, sa7_opt(shortlabel, val));
        free(shortlabel); json_free(nmv);
    }
    char *extra = sa7_strdup("");
    if (np > 100) {
        char cbuf[32]; snprintf(cbuf, sizeof cbuf, "%zu", np - 100);
        json_t *a = json_object();
        json_set(a, "key", json_string("platform.slack.picker.more_available"));
        json_set(a, "count", json_string(cbuf));
        json_t *r = sa7_call(ctx, "t", a);
        if (sa7_is_raise(r)) { json_free(options); free(extra); return r; }
        free(extra); extra = sa7_pystr(r); json_free(r);
    }
    char *model = NULL;
    if (cur && cur[0]) model = sa7_strdup(cur);
    else {
        json_t *a = json_object(); json_set(a, "key", json_string("platform.shared.unknown"));
        json_t *r = sa7_call(ctx, "t", a);
        if (sa7_is_raise(r)) { json_free(options); free(extra); return r; }
        model = sa7_pystr(r); json_free(r);
    }
    json_t *ha = json_object();
    json_set(ha, "key", json_string("platform.slack.picker.provider_header"));
    json_set(ha, "model", json_string(model));
    json_set(ha, "provider", json_string(plabel ? plabel : ""));
    json_set(ha, "extra", json_string(extra));
    json_t *hdr = sa7_call(ctx, "t", ha);
    if (sa7_is_raise(hdr)) { json_free(options); free(extra); free(model); return hdr; }
    char *hs = sa7_pystr(hdr); json_free(hdr);
    char *hcut = sa7_slice(hs, 3000);
    json_t *ph = json_object();
    json_set(ph, "key", json_string("platform.slack.picker.choose_provider"));
    json_t *phr = sa7_call(ctx, "t", ph);
    if (sa7_is_raise(phr)) { json_free(options); free(extra); free(model); free(hs); free(hcut); return phr; }
    char *phs = sa7_pystr(phr); json_free(phr);
    char *phcut = sa7_slice(phs, 150);
    json_t *ca = json_object(); json_set(ca, "key", json_string("platform.slack.picker.cancel"));
    json_t *cr = sa7_call(ctx, "t", ca);
    if (sa7_is_raise(cr)) { json_free(options); free(extra); free(model); free(hs); free(hcut); free(phs); free(phcut); return cr; }
    char *cs = sa7_pystr(cr); json_free(cr);
    char *ccut = sa7_slice(cs, 75);

    json_t *sel = json_object();
    json_set(sel, "type", json_string("static_select"));
    json_t *pt = json_object();
    json_set(pt, "type", json_string("plain_text"));
    json_set(pt, "text", json_string(phcut));
    json_set(pt, "emoji", json_bool(true));
    json_set(sel, "placeholder", pt);
    json_set(sel, "action_id", json_string("hermes_model_provider"));
    json_set(sel, "options", json_copy(options));
    json_t *cancel = json_object();
    json_set(cancel, "type", json_string("button"));
    json_t *ct = json_object();
    json_set(ct, "type", json_string("plain_text"));
    json_set(ct, "text", json_string(ccut));
    json_set(ct, "emoji", json_bool(true));
    json_set(cancel, "text", ct);
    json_set(cancel, "style", json_string("danger"));
    json_set(cancel, "action_id", json_string("hermes_model_cancel"));
    json_set(cancel, "value", json_string("cancel"));
    json_t *els = json_array();
    json_append(els, sel); json_append(els, cancel);
    json_t *act = json_object();
    json_set(act, "type", json_string("actions"));
    json_set(act, "elements", els);
    json_t *out = json_array();
    json_append(out, sa7_txt_block(hcut));
    json_append(out, act);
    json_free(options); free(extra); free(model); free(hs); free(hcut);
    free(phs); free(phcut); free(cs); free(ccut);
    return out;
}

/* PoP: sa7_build_model_picker_model_blocks @ plugins/platforms/slack/adapter.py:_build_model_picker_model_blocks */
/* instance: model-select stage blocks; t()/picker_row_suffix are ordered seams. */
static json_t *sa7_build_model_picker_model_blocks(sa7_ctx_t *ctx, const json_t *args) {
    json_t *providers = json_obj_get(args, "providers");
    const char *slug = sa7_str(json_obj_get(args, "provider_slug"));
    json_t *provider = NULL;
    size_t np = (providers && providers->type == JSON_ARRAY) ? json_len(providers) : 0;
    for (size_t i = 0; i < np; i++) {
        json_t *p = json_get(providers, i);
        json_t *s = sa7_get(p, "slug", json_null());
        if (slug && sa7_str(s) && strcmp(sa7_str(s), slug) == 0) { provider = json_copy(p); json_free(s); break; }
        json_free(s);
    }
    char *pname;
    if (provider) {
        json_t *nm = sa7_get(provider, "name", json_string(slug ? slug : ""));
        pname = sa7_str(nm) ? sa7_strdup(sa7_str(nm)) : sa7_strdup(slug ? slug : "");
        json_free(nm);
    } else pname = sa7_strdup(slug ? slug : "");
    json_t *models = (provider ? sa7_get(provider, "models", json_array()) : json_array());
    size_t ml = json_len(models) < 100 ? json_len(models) : 100;
    json_t *options = json_array();
    for (size_t i = 0; i < ml; i++) {
        const char *mid = sa7_str(json_get(models, i));
        const char *slash = mid ? strrchr(mid, '/') : NULL;
        const char *shortname = slash ? slash + 1 : (mid ? mid : "");
        json_t *pa = json_object();
        json_set(pa, "provider", json_copy(provider ? provider : json_null()));
        json_set(pa, "model_id", json_string(mid ? mid : ""));
        json_t *sr = sa7_call(ctx, "picker_row_suffix", pa);
        if (sa7_is_raise(sr)) { json_free(models); json_free(provider); free(pname); json_free(options); return sr; }
        char *suffix = sa7_pystr(sr); json_free(sr);
        size_t sl = strlen(suffix);
        char *text;
        if (suffix[0]) {
            long long room = 75 - (long long)sl - 3;
            if (room < 0) room = 0;
            char *sh = sa7_slice(shortname, room);
            text = (char *)malloc(strlen(sh) + sl + 8);
            snprintf(text, strlen(sh) + sl + 8, "%s \xC2\xB7 %s", sh, suffix);
            free(sh);
        } else text = sa7_slice(shortname, 75);
        char val[32]; snprintf(val, sizeof val, "%zu", i);
        json_append(options, sa7_opt(text, val));
        free(text); free(suffix);
    }
    json_t *totalv = provider ? sa7_get(provider, "total_models", json_null()) : json_null();
    long long total;
    if (totalv && (totalv->type == JSON_NUMBER || totalv->type == JSON_STRING))
        total = (long long)(totalv->type == JSON_NUMBER ? totalv->num_val : atoll(totalv->str_val));
    else total = (long long)ml;
    json_free(totalv);
    char *extra = sa7_strdup("");
    if (total > (long long)ml) {
        char cbuf[32]; snprintf(cbuf, sizeof cbuf, "%lld", total - (long long)ml);
        json_t *a = json_object();
        json_set(a, "key", json_string("platform.slack.picker.more_available"));
        json_set(a, "count", json_string(cbuf));
        json_t *r = sa7_call(ctx, "t", a);
        if (sa7_is_raise(r)) { json_free(models); json_free(provider); free(pname); json_free(options); free(extra); return r; }
        free(extra); extra = sa7_pystr(r); json_free(r);
    }
    char *plan = sa7_strdup("");
    if (provider) {
        json_t *pl = sa7_get(provider, "plan_line", json_null());
        if (sa7_truthy(pl)) {
            char *ps = sa7_pystr(pl);
            plan = (char *)malloc(strlen(ps) + 24);
            snprintf(plan, strlen(ps) + 24, "\xE2\x9A\xA0\xEF\xB8\x8F %s\n", ps);
            free(ps);
        }
        json_free(pl);
    }
    json_t *ha = json_object();
    json_set(ha, "key", json_string("platform.slack.picker.model_header"));
    json_set(ha, "provider", json_string(pname));
    json_set(ha, "extra", json_string(extra));
    json_t *hdr = sa7_call(ctx, "t", ha);
    if (sa7_is_raise(hdr)) { json_free(models); json_free(provider); free(pname); json_free(options); free(extra); free(plan); return hdr; }
    char *hs = sa7_pystr(hdr); json_free(hdr);
    size_t cap = strlen(plan) + strlen(hs) + 4;
    char *sec = (char *)malloc(cap);
    snprintf(sec, cap, "%s%s", plan, hs);
    char *scut = sa7_slice(sec, 3000);
    json_t *pa = json_object();
    json_set(pa, "key", json_string("platform.slack.picker.choose_model"));
    json_set(pa, "provider", json_string(pname));
    json_t *pr = sa7_call(ctx, "t", pa);
    if (sa7_is_raise(pr)) { json_free(models); json_free(provider); free(pname); json_free(options); free(extra); free(plan); free(hs); free(sec); free(scut); return pr; }
    char *phs = sa7_pystr(pr); json_free(pr);
    char *phcut = sa7_slice(phs, 150);
    json_t *sel = json_object();
    json_set(sel, "type", json_string("static_select"));
    json_t *pt = json_object();
    json_set(pt, "type", json_string("plain_text"));
    json_set(pt, "text", json_string(phcut));
    json_set(pt, "emoji", json_bool(true));
    json_set(sel, "placeholder", pt);
    json_set(sel, "action_id", json_string("hermes_model_model"));
    json_set(sel, "options", json_copy(options));
    json_t *els = json_array();
    json_append(els, sel);
    if (slug && slug[0]) {
        json_t *ba = json_object(); json_set(ba, "key", json_string("platform.slack.picker.back"));
        json_t *br = sa7_call(ctx, "t", ba);
        if (sa7_is_raise(br)) { json_free(models); json_free(provider); free(pname); json_free(options); free(extra); free(plan); free(hs); free(sec); free(scut); free(phs); free(phcut); return br; }
        char *bs = sa7_pystr(br); json_free(br);
        char *bc = sa7_slice(bs, 75);
        json_t *back = json_object();
        json_set(back, "type", json_string("button"));
        json_t *bt = json_object();
        json_set(bt, "type", json_string("plain_text"));
        json_set(bt, "text", json_string(bc));
        json_set(bt, "emoji", json_bool(true));
        json_set(back, "text", bt);
        json_set(back, "action_id", json_string("hermes_model_back"));
        json_set(back, "value", json_string(slug));
        json_append(els, back);
        free(bs); free(bc);
    }
    json_t *ca = json_object(); json_set(ca, "key", json_string("platform.slack.picker.cancel"));
    json_t *cr = sa7_call(ctx, "t", ca);
    if (sa7_is_raise(cr)) { json_free(models); json_free(provider); free(pname); json_free(options); free(extra); free(plan); free(hs); free(sec); free(scut); free(phs); free(phcut); return cr; }
    char *cs = sa7_pystr(cr); json_free(cr);
    char *cc = sa7_slice(cs, 75);
    json_t *cancel = json_object();
    json_set(cancel, "type", json_string("button"));
    json_t *ct = json_object();
    json_set(ct, "type", json_string("plain_text"));
    json_set(ct, "text", json_string(cc));
    json_set(ct, "emoji", json_bool(true));
    json_set(cancel, "text", ct);
    json_set(cancel, "style", json_string("danger"));
    json_set(cancel, "action_id", json_string("hermes_model_cancel"));
    json_set(cancel, "value", json_string("cancel"));
    json_append(els, cancel);
    json_t *act = json_object();
    json_set(act, "type", json_string("actions"));
    json_set(act, "elements", els);
    json_t *out = json_array();
    json_append(out, sa7_txt_block(scut));
    json_append(out, act);
    json_free(models); json_free(provider); free(pname); json_free(options); free(extra); free(plan);
    free(hs); free(sec); free(scut); free(phs); free(phcut); free(cs); free(cc);
    return out;
}

/* PoP: sa7_cache_agent_view_context @ plugins/platforms/slack/adapter.py:_cache_agent_view_context */
/* instance: keyed per (team,user) with a field whitelist; trim seam on overflow. */
static json_t *sa7_cache_agent_view_context(sa7_ctx_t *ctx, const json_t *args) {
    json_t *md = json_obj_get(args, "metadata");
    json_t *a = json_object();
    json_set(a, "team_id", json_copy(sa7_get(md, "team_id", json_string(""))));
    json_set(a, "user_id", json_copy(sa7_get(md, "user_id", json_string(""))));
    json_t *key = sa7_call(ctx, "agent_view_context_key", a);
    if (sa7_is_raise(key)) return key;
    if (!sa7_truthy(key)) { json_free(key); return json_null(); }
    static const char *ALLOW[] = {"channel_id", "context_channel_id", "team_id", "user_id"};
    json_t *filtered = json_object();
    if (md && md->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(md); i++) {
            const char *k = json_object_get_key_at(md, i);
            json_t *v = json_object_get_at(md, i);
            if (k && sa7_in_set_str(ALLOW, 4, k) && sa7_truthy(v)) json_set(filtered, k, json_copy(v));
        }
    }
    json_t *contexts = json_object();
    char *ks = sa7_pystr(key);
    json_set(contexts, ks, filtered);
    free(ks); json_free(key);
    json_t *b = json_object();
    json_set(b, "entries", json_copy(contexts));
    json_set(b, "max", json_number(json_get_num(ctx->state, "_AGENT_VIEW_CONTEXTS_MAX", 5000.0)));
    json_t *r = sa7_call(ctx, "trim_oldest", b);
    if (sa7_is_raise(r)) { json_free(contexts); return r; }
    json_free(r); json_free(contexts);
    return json_null();
}

/* PoP: sa7_cache_assistant_thread_metadata @ plugins/platforms/slack/adapter.py:_cache_assistant_thread_metadata */
/* instance: workspace-thread key, merge metadata, trim + channel-team seams. */
static json_t *sa7_cache_assistant_thread_metadata(sa7_ctx_t *ctx, const json_t *args) {
    json_t *md = json_obj_get(args, "metadata");
    const char *cid = sa7_str(sa7_get(md, "channel_id", json_string("")));
    const char *tt = sa7_str(sa7_get(md, "thread_ts", json_string("")));
    const char *team = sa7_str(sa7_get(md, "team_id", json_string("")));
    json_t *a = json_object();
    json_set(a, "team_id", json_string(team ? team : ""));
    json_set(a, "channel_id", json_string(cid ? cid : ""));
    json_set(a, "thread_id", json_string(tt ? tt : ""));
    json_t *key = sa7_call(ctx, "workspace_thread_key", a);
    if (sa7_is_raise(key)) return key;
    if (!sa7_truthy(key)) { json_free(key); return json_null(); }
    json_t *existing = json_obj_get(ctx->state, "_assistant_threads");
    json_t *merged = json_object();
    if (existing && existing->type == JSON_OBJECT) {
        char *ks = sa7_pystr(key);
        json_t *ex = json_obj_get(existing, ks);
        if (ex && ex->type == JSON_OBJECT)
            for (size_t i = 0; i < json_object_size(ex); i++) json_set(merged, json_object_get_key_at(ex, i), json_copy(json_object_get_at(ex, i)));
        free(ks);
    }
    if (md && md->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(md); i++) {
            json_t *v = json_object_get_at(md, i);
            if (sa7_truthy(v)) json_set(merged, json_object_get_key_at(md, i), json_copy(v));
        }
    json_t *threads = json_object();
    if (existing && existing->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(existing); i++)
            json_set(threads, json_object_get_key_at(existing, i), json_copy(json_object_get_at(existing, i)));
    char *kstr = sa7_pystr(key);
    json_set(threads, kstr, json_copy(merged));
    free(kstr);
    json_t *b = json_object();
    json_set(b, "entries", threads);
    json_set(b, "max", json_number(json_get_num(ctx->state, "_ASSISTANT_THREADS_MAX", 5000.0)));
    json_t *r = sa7_call(ctx, "trim_oldest", b);
    if (sa7_is_raise(r)) { json_free(merged); json_free(key); return r; }
    json_free(r);
    if (team && team[0] && cid && cid[0]) {
        json_t *c = json_object();
        json_set(c, "channel_id", json_string(cid));
        json_set(c, "team_id", json_string(team));
        json_t *rr = sa7_call(ctx, "remember_channel_team", c);
        if (sa7_is_raise(rr)) { json_free(merged); json_free(key); return rr; }
        json_free(rr);
    }
    json_free(merged); json_free(key);
    return json_null();
}
