/*
 * slack_adapter_flow5.c -- FLOW cores, wave 5, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's STILL-UNCLAIMED remainder after flow (28), flow2 (26), flow3 (12)
 * and flow4 (25).  All ported names were UNCLAIMED by a CRLF-aware strict PoP
 * grep of the whole main-only repo at port time (each carries a PoP annotation
 * at its definition below).
 *
 * Ported (18 functions, all UNCLAIMED at their port time):
 *   _session_status_method, _session_title_method, _standalone_format_mrkdwn,
 *   _standalone_proxy_kwargs, _media_message_type, _native_task_card_key,
 *   native_task_card_destination_supported, _suppressed_ignored,
 *   _outbound_blocked, supports_draft_streaming, _reacting_target,
 *   _channel_prompt_with_identity, _slack_is_free_channel, _slack_reply_expected,
 *   _slack_message_matches_mention_patterns, _append_block_text,
 *   _thread_rehydration_key, _thread_session_source.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa5_call(ctx, name, args).  The harness wires it to a
 * recording stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts.  Ordered trace,
 * BaseException passthrough, fail-closed.  Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow5.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - {"__obj__": attrs} is an attribute carrier (SimpleNamespace double);
 *   {"__set__": [..]} is a set carrier; {"__exc__": {...}} is an exception
 *   instance; str() positions carry str/int/bool/None only (ASCII domain).
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
typedef struct sa5_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa5_ctx_t;

/* -- basic helpers -- */
static bool sa5_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa5_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa5_obj_attrs(const json_t *v) {
    return sa5_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa5_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa5_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa5_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa5_is_obj(v) &&
           !sa5_is_set(v) && !sa5_is_tuple_marker(v) && !sa5_is_raise(v);
}
static bool sa5_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa5_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa5_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa5_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa5_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa5_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sa5_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa5_call(sa5_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa5_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa5_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa5_make_exc(raise);
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
typedef struct sa5_exc_parent { const char *name, *parent1, *parent2; } sa5_exc_parent_t;
static const sa5_exc_parent_t SA5_EXC_TABLE[] = {
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
#define SA5_EXC_TABLE_LEN (sizeof(SA5_EXC_TABLE) / sizeof(SA5_EXC_TABLE[0]))

static const sa5_exc_parent_t *sa5_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA5_EXC_TABLE_LEN; i++)
        if (strcmp(SA5_EXC_TABLE[i].name, name) == 0) return &SA5_EXC_TABLE[i];
    return NULL;
}
typedef struct sa5_chain { char names[64][96]; size_t n; } sa5_chain_t;
static bool sa5_chain_has(const sa5_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa5_chain_add(sa5_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa5_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa5_chain_walk(sa5_chain_t *c, const char *name) {
    if (!name || !name[0] || sa5_chain_has(c, name)) return;
    sa5_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa5_exc_parent_t *be = sa5_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";  /* synthetic -> Exception */
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa5_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa5_chain_walk(c, p2);
}
static bool sa5_exc_family(const json_t *r, const char *family) {
    if (!sa5_is_raise(r)) return false;
    sa5_chain_t c; c.n = 0;
    sa5_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa5_chain_has(&c, family);
}
static bool sa5_raise_passes(const json_t *r) {
    return sa5_is_raise(r) && !sa5_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa5_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa5_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa5_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa5_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa5_lower(const char *s) {
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
static char *sa5_lower_strip(const char *s) {
    char *lo = sa5_lower(s);
    char *st = sa5_strip(lo);
    free(lo);
    return st;
}
static bool sa5_in_set_str(const char *const *items, size_t n, const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, items[i]) == 0) return true;
    return false;
}
/* Python str() of a scalar (ASCII domain: str/int/bool/None only). */
static char *sa5_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa5_strdup("None");
    if (v->type == JSON_STRING) return sa5_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa5_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa5_strdup(buf);
    }
    return sa5_strdup("");
}
/* dict.get(key, default): absent -> default; present-null -> JSON_NULL node. */
static json_t *sa5_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static const char *sa5_pytype_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) return (v->num_val == floor(v->num_val)) ? "int" : "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    if (sa5_is_obj(v)) return "types.SimpleNamespace";
    if (sa5_is_set(v)) return "set";
    return "dict";
}
static json_t *sa5_no_get(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sa5_pytype_name(v));
    return sa5_err("AttributeError", msg);
}
static json_t *sa5_no_startswith(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'startswith'", sa5_pytype_name(v));
    return sa5_err("AttributeError", msg);
}

/* ====================================================================== */

/* -- attribute / collection helpers (added for wave 5) -- */
/* getattr(obj, key) with default None -- obj is an {"__obj__":{...}} carrier
 * or a missing/NULL value.  Returns a fresh node (JSON_NULL when absent). */
static json_t *sa5_getattr(const json_t *obj, const char *key) {
    json_t *attrs = sa5_obj_attrs(obj);
    if (!attrs) return json_null();
    json_t *v = json_obj_get(attrs, key);
    return json_copy(v ? v : json_null());
}
/* Required attribute access -- raises AttributeError when absent (matches a
 * bare `obj.attr`). type_name names the Python carrier type in the message. */
static json_t *sa5_getattr_req(const json_t *obj, const char *key, const char *type_name) {
    json_t *attrs = sa5_obj_attrs(obj);
    json_t *v = attrs ? json_obj_get(attrs, key) : NULL;
    if (!v) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 type_name ? type_name : "types.SimpleNamespace", key);
        return sa5_err("AttributeError", msg);
    }
    return json_copy(v);
}
/* Python `s in collection` for a string s: JSON array, {"__set__":[..]} carrier,
 * or a plain dict (membership tests keys). */
static bool sa5_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa5_is_set(coll)) coll = json_obj_get(coll, "__set__");
    if (!coll) return false;
    if (coll->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(coll); i++) {
            const char *m = sa5_str(json_get(coll, i));
            if (m && strcmp(m, s) == 0) return true;
        }
        return false;
    }
    if (coll->type == JSON_OBJECT) return json_obj_get(coll, s) != NULL;
    return false;
}

/* ====================================================================== */
/* PoP: sa5_session_status_method @ plugins/platforms/slack/adapter.py:_session_status_method */
/* module: SDK agent-session status setter when available, else legacy. */
static json_t *sa5_session_status_method(sa5_ctx_t *ctx, const json_t *args) {
    json_t *client = json_obj_get(args, "client");
    json_t *r = sa5_call(ctx, "sdk_supports_agent_sessions", json_object());
    if (sa5_is_raise(r)) return r;
    bool supported = sa5_truthy(r);
    json_free(r);
    if (supported) {
        json_t *m = sa5_getattr(client, "agents_sessions_setStatus");
        if (m && m->type != JSON_NULL) return m;
        json_free(m);
    }
    return sa5_getattr_req(client, "assistant_threads_setStatus", "types.SimpleNamespace");
}

/* PoP: sa5_session_title_method @ plugins/platforms/slack/adapter.py:_session_title_method */
/* module: SDK agent-session rename setter when available, else legacy. */
static json_t *sa5_session_title_method(sa5_ctx_t *ctx, const json_t *args) {
    json_t *client = json_obj_get(args, "client");
    json_t *r = sa5_call(ctx, "sdk_supports_agent_sessions", json_object());
    if (sa5_is_raise(r)) return r;
    bool supported = sa5_truthy(r);
    json_free(r);
    if (supported) {
        json_t *m = sa5_getattr(client, "agents_sessions_rename");
        if (m && m->type != JSON_NULL) return m;
        json_free(m);
    }
    return sa5_getattr_req(client, "assistant_threads_setTitle", "types.SimpleNamespace");
}

/* PoP: sa5_standalone_format_mrkdwn @ plugins/platforms/slack/adapter.py:_standalone_format_mrkdwn */
/* module: format_message via a fresh adapter; on Exception log+return raw text. */
static json_t *sa5_standalone_format_mrkdwn(sa5_ctx_t *ctx, const json_t *args) {
    json_t *text = json_obj_get(args, "text");
    if (!sa5_truthy(text)) return json_copy(text ? text : json_null());
    json_t *a = json_object();
    json_set(a, "text", json_copy(text));
    json_t *r = sa5_call(ctx, "format_message", a);
    if (sa5_is_raise(r)) {
        if (sa5_raise_passes(r)) return r;  /* BaseException passthrough */
        json_t *la = json_object();
        json_set(la, "msg", json_string("Failed to apply Slack mrkdwn formatting in _standalone_send"));
        json_set(la, "args", json_array());
        json_t *lr = sa5_call(ctx, "logger.debug", la);
        if (sa5_is_raise(lr)) { json_free(r); return lr; }
        json_free(lr); json_free(r);
        return json_copy(text);
    }
    return r;
}

/* PoP: sa5_standalone_proxy_kwargs @ plugins/platforms/slack/adapter.py:_standalone_proxy_kwargs */
/* module: (session_kwargs, request_kwargs) from the resolved proxy URL. */
static json_t *sa5_standalone_proxy_kwargs(sa5_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *u = sa5_call(ctx, "resolve_proxy_url", json_object());
    if (sa5_is_raise(u)) return u;
    json_t *a = json_object();
    json_set(a, "url", json_copy(u));
    json_free(u);
    return sa5_call(ctx, "proxy_kwargs_for_aiohttp", a);
}

/* PoP: sa5_media_message_type @ plugins/platforms/slack/adapter.py:_media_message_type */
/* static: PHOTO/VIDEO/VOICE by first matching prefix, else DOCUMENT/TEXT. */
static json_t *sa5_media_message_type(sa5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *types = json_obj_get(args, "media_types");
    if (!types || types->type != JSON_ARRAY || json_len(types) == 0) return json_string("TEXT");
    static const char *PREFIX[3] = {"image/", "video/", "audio/"};
    static const char *KIND[3] = {"PHOTO", "VIDEO", "VOICE"};
    for (int i = 0; i < 3; i++) {
        for (size_t j = 0; j < json_len(types); j++) {
            const char *s = sa5_str(json_get(types, j));
            if (s && strncmp(s, PREFIX[i], strlen(PREFIX[i])) == 0)
                return json_string(KIND[i]);
        }
    }
    return json_string("DOCUMENT");
}

/* PoP: sa5_native_task_card_key @ plugins/platforms/slack/adapter.py:_native_task_card_key */
/* instance: (team, chat, thread_ts) key via resolved thread/team seams, or None. */
static json_t *sa5_native_task_card_key(sa5_ctx_t *ctx, const json_t *args) {
    json_t *chat = json_obj_get(args, "chat_id");
    json_t *reply = json_obj_get(args, "reply_to");
    json_t *meta = json_obj_get(args, "metadata");
    json_t *ta = json_object();
    json_set(ta, "reply_to", json_copy(reply ? reply : json_null()));
    json_set(ta, "metadata", json_copy(meta ? meta : json_null()));
    json_t *tts = sa5_call(ctx, "resolve_thread_ts", ta);
    if (sa5_is_raise(tts)) return tts;
    if (!sa5_truthy(tts)) { json_free(tts); return json_null(); }
    json_t *ma = json_object();
    json_set(ma, "metadata", json_copy(meta ? meta : json_null()));
    json_t *team = sa5_call(ctx, "metadata_team_id", ma);
    if (sa5_is_raise(team)) { json_free(tts); return team; }
    char *ts = sa5_pystr(tts);
    json_free(tts);
    json_t *wa = json_object();
    json_set(wa, "team_id", json_copy(team));
    json_set(wa, "chat_id", json_copy(chat ? chat : json_null()));
    json_set(wa, "thread_ts", json_string(ts ? ts : ""));
    free(ts); json_free(team);
    return sa5_call(ctx, "workspace_thread_key", wa);
}

/* PoP: sa5_native_task_card_destination_supported @ plugins/platforms/slack/adapter.py:native_task_card_destination_supported */
/* instance: placement eligibility == native task card key is not None. */
static json_t *sa5_native_task_card_destination_supported(sa5_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(json_obj_get(args, "chat_id")));
    json_set(a, "reply_to", json_copy(json_obj_get(args, "reply_to")));
    json_set(a, "metadata", json_copy(json_obj_get(args, "metadata")));
    json_t *r = sa5_call(ctx, "native_task_card_key", a);
    if (sa5_is_raise(r)) return r;
    bool ok = (r && r->type != JSON_NULL);
    json_free(r);
    return json_bool(ok);
}

/* PoP: sa5_suppressed_ignored @ plugins/platforms/slack/adapter.py:_suppressed_ignored */
/* instance: True (after logging) when the chat is a configured ignored channel. */
static json_t *sa5_suppressed_ignored(sa5_ctx_t *ctx, const json_t *args) {
    json_t *chat = json_obj_get(args, "chat_id");
    json_t *what = json_obj_get(args, "what");
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(chat ? chat : json_null()));
    json_t *ig = sa5_call(ctx, "is_ignored_channel", a);
    if (sa5_is_raise(ig)) return ig;
    bool ignored = sa5_truthy(ig);
    json_free(ig);
    if (ignored) {
        json_t *la = json_object();
        json_set(la, "msg", json_string("[Slack] Suppressed %s configured ignored channel %s"));
        json_t *arr = json_array();
        json_append(arr, json_copy(what ? what : json_null()));
        json_append(arr, json_copy(chat ? chat : json_null()));
        json_set(la, "args", arr);
        json_t *lr = sa5_call(ctx, "logger.log", la);
        if (sa5_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: sa5_outbound_blocked @ plugins/platforms/slack/adapter.py:_outbound_blocked */
/* instance: failed SendResult when ignored / disconnected, else None. */
static json_t *sa5_outbound_blocked(sa5_ctx_t *ctx, const json_t *args) {
    json_t *chat = json_obj_get(args, "chat_id");
    json_t *what = json_obj_get(args, "what");
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(chat ? chat : json_null()));
    json_set(a, "what", json_copy(what ? what : json_null()));
    json_t *sup = sa5_call(ctx, "suppressed_ignored", a);
    if (sa5_is_raise(sup)) return sup;
    if (sa5_truthy(sup)) {
        json_free(sup);
        json_t *r = json_object();
        json_set(r, "success", json_bool(false));
        json_set(r, "error", json_string("ignored_channel"));
        return r;
    }
    json_free(sup);
    json_t *app = json_obj_get(ctx->state, "app");
    if (!sa5_truthy(app)) {
        json_t *r = json_object();
        json_set(r, "success", json_bool(false));
        json_set(r, "error", json_string("Not connected"));
        return r;
    }
    return json_null();
}

/* PoP: sa5_supports_draft_streaming @ plugins/platforms/slack/adapter.py:supports_draft_streaming */
/* instance: native stream usable unless disabled, unfurl config needs edit transport, or no app. */
static json_t *sa5_supports_draft_streaming(sa5_ctx_t *ctx, const json_t *args) {
    (void)args;
    if (sa5_truthy(json_obj_get(ctx->state, "native_stream_unsupported"))) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "extra", json_copy(json_obj_get(ctx->state, "extra")));
    json_t *u = sa5_call(ctx, "slack_unfurl_kwargs", a);
    if (sa5_is_raise(u)) return u;
    bool need_edit = sa5_truthy(u);
    json_free(u);
    if (need_edit) return json_bool(false);
    json_t *app = json_obj_get(ctx->state, "app");
    return json_bool(app && app->type != JSON_NULL);
}

/* PoP: sa5_reacting_target @ plugins/platforms/slack/adapter.py:_reacting_target */
/* instance: (ts, team_id, marker) when reactions on and the event is tracked. */
static json_t *sa5_reacting_target(sa5_ctx_t *ctx, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *re = sa5_call(ctx, "reactions_enabled", json_object());
    if (sa5_is_raise(re)) return re;
    if (!sa5_truthy(re)) { json_free(re); return json_null(); }
    json_free(re);
    json_t *ts = sa5_getattr(event, "message_id");
    if (!sa5_truthy(ts)) { json_free(ts); return json_null(); }
    json_t *source = sa5_obj_attrs(event) ? json_obj_get(sa5_obj_attrs(event), "source") : NULL;
    json_t *scope = sa5_getattr(source, "scope_id");
    char *team = sa5_truthy(scope) ? sa5_pystr(scope) : sa5_strdup("");
    json_free(scope);
    json_t *ma = json_object();
    json_set(ma, "team_id", json_string(team ? team : ""));
    json_set(ma, "ts", json_copy(ts));
    json_t *marker = sa5_call(ctx, "workspace_message_marker", ma);
    if (sa5_is_raise(marker)) { free(team); json_free(ts); return marker; }
    json_t *ids = json_obj_get(ctx->state, "_reacting_message_ids");
    char *ms = sa5_pystr(marker);
    bool tracked = sa5_truthy(marker) && sa5_collection_has_str(ids, ms);
    if (tracked) {
        json_t *out = json_array();
        json_append(out, json_copy(ts));
        json_append(out, json_string(team ? team : ""));
        json_append(out, json_copy(marker));
        free(ms); free(team); json_free(ts); json_free(marker);
        return out;
    }
    free(ms); free(team); json_free(ts); json_free(marker);
    return json_null();
}

/* PoP: sa5_channel_prompt_with_identity @ plugins/platforms/slack/adapter.py:_channel_prompt_with_identity */
/* instance: channel prompt with the bot identity prepended (ephemeral). */
static json_t *sa5_channel_prompt_with_identity(sa5_ctx_t *ctx, const json_t *args) {
    json_t *chat = json_obj_get(args, "channel_id");
    json_t *team = json_obj_get(args, "team_id");
    json_t *a = json_object();
    json_set(a, "extra", json_copy(json_obj_get(ctx->state, "extra")));
    json_set(a, "channel_id", json_copy(chat ? chat : json_null()));
    json_t *cp = sa5_call(ctx, "resolve_channel_prompt", a);
    if (sa5_is_raise(cp)) return cp;
    json_t *ia = json_object();
    json_set(ia, "team_id", json_copy(team ? team : json_null()));
    json_t *ip = sa5_call(ctx, "build_identity_prompt", ia);
    if (sa5_is_raise(ip)) { json_free(cp); return ip; }
    json_t *result;
    if (sa5_truthy(ip)) {
        if (sa5_truthy(cp)) {
            char *is = sa5_pystr(ip), *cs = sa5_pystr(cp);
            size_t n = strlen(is) + strlen(cs) + 3;
            char *buf = (char *)malloc(n);
            if (buf) snprintf(buf, n, "%s\n\n%s", is, cs);
            char *st = sa5_strip(buf ? buf : "");
            result = json_string(st ? st : "");
            free(st); free(buf); free(is); free(cs);
        } else {
            result = json_copy(ip);
        }
    } else {
        result = json_copy(cp);
    }
    json_free(ip); json_free(cp);
    return result;
}

/* PoP: sa5_slack_is_free_channel @ plugins/platforms/slack/adapter.py:_slack_is_free_channel */
/* instance: channel admits messages without an @mention. */
static json_t *sa5_slack_is_free_channel(sa5_ctx_t *ctx, const json_t *args) {
    const char *chat = sa5_str(json_obj_get(args, "channel_id"));
    json_t *req = sa5_call(ctx, "slack_require_mention_channels", json_object());
    if (sa5_is_raise(req)) return req;
    if (sa5_collection_has_str(req, chat)) { json_free(req); return json_bool(false); }
    json_free(req);
    json_t *freech = sa5_call(ctx, "slack_free_response_channels", json_object());
    if (sa5_is_raise(freech)) return freech;
    if (sa5_collection_has_str(freech, chat)) { json_free(freech); return json_bool(true); }
    json_free(freech);
    json_t *rm = sa5_call(ctx, "slack_require_mention", json_object());
    if (sa5_is_raise(rm)) return rm;
    bool pass = !sa5_truthy(rm);
    json_free(rm);
    return json_bool(pass);
}

/* PoP: sa5_slack_reply_expected @ plugins/platforms/slack/adapter.py:_slack_reply_expected */
/* instance: reply_expected tri-state for an admitted message. */
static json_t *sa5_slack_reply_expected(sa5_ctx_t *ctx, const json_t *args) {
    json_t *text = json_obj_get(args, "routing_text");
    json_t *uidv = json_obj_get(args, "bot_uid");
    json_t *chat = json_obj_get(args, "channel_id");
    bool addressed = sa5_truthy(json_obj_get(args, "addressed"));
    bool opens = sa5_truthy(json_obj_get(args, "opens_own_session"));
    bool followup = sa5_truthy(json_obj_get(args, "channel_thread_followup"));

    /* self_uids = {u for u in (bot_uid, self._bot_user_id) if u} */
    json_t *self_uid = json_obj_get(ctx->state, "_bot_user_id");
    const char *u0 = sa5_str(uidv), *u1 = sa5_str(self_uid);
    /* project(set) sorts members; ≤2 uids, so append in sorted order. */
    const char *a = (u0 && u0[0]) ? u0 : NULL;
    const char *b = (u1 && u1[0] && !(a && strcmp(a, u1) == 0)) ? u1 : NULL;
    json_t *setarr = json_array();
    if (a && b) {
        if (strcmp(a, b) <= 0) { json_append(setarr, json_string(a)); json_append(setarr, json_string(b)); }
        else { json_append(setarr, json_string(b)); json_append(setarr, json_string(a)); }
    } else if (a) json_append(setarr, json_string(a));
    else if (b) json_append(setarr, json_string(b));
    json_t *uidset = json_object();
    json_set(uidset, "__set__", setarr);

    if (addressed) { json_free(uidset); return json_bool(true); }
    json_t *a1 = json_object();
    json_set(a1, "text", json_copy(text ? text : json_null()));
    json_set(a1, "self_uids", uidset);
    json_t *ms = sa5_call(ctx, "slack_message_mentions_self", a1);
    if (sa5_is_raise(ms)) return ms;
    if (sa5_truthy(ms)) { json_free(ms); return json_bool(true); }
    json_free(ms);

    /* uidset was adopted by a1's trace event; rebuild for the next seam */
    json_t *uidset2 = json_object();
    json_set(uidset2, "__set__", json_copy(setarr));
    json_t *a2 = json_object();
    json_set(a2, "text", json_copy(text ? text : json_null()));
    json_set(a2, "self_uids", uidset2);
    json_t *oth = sa5_call(ctx, "slack_message_addressed_to_other_user", a2);
    if (sa5_is_raise(oth)) return oth;
    if (sa5_truthy(oth)) { json_free(oth); return json_bool(false); }
    json_free(oth);

    if (followup) {
        json_t *sil = sa5_call(ctx, "slack_thread_followup_silence", json_object());
        if (sa5_is_raise(sil)) return sil;
        bool s = sa5_truthy(sil);
        json_free(sil);
        if (s) return json_bool(false);
    }
    if (opens) {
        json_t *ca = json_object();
        json_set(ca, "channel_id", json_copy(chat ? chat : json_null()));
        json_t *free = sa5_call(ctx, "slack_is_free_channel", ca);
        if (sa5_is_raise(free)) return free;
        bool f = sa5_truthy(free);
        json_free(free);
        return f ? json_bool(false) : json_null(); /* False if free, else None */
    }
    return json_null();
}

/* PoP: sa5_slack_message_matches_mention_patterns @ plugins/platforms/slack/adapter.py:_slack_message_matches_mention_patterns */
/* instance: True when text matches a configured (substring-double) wake word. */
static json_t *sa5_slack_message_matches_mention_patterns(sa5_ctx_t *ctx, const json_t *args) {
    json_t *text = json_obj_get(args, "text");
    if (!sa5_truthy(text)) return json_bool(false);
    json_t *pats = sa5_call(ctx, "slack_mention_patterns", json_object());
    if (sa5_is_raise(pats)) return pats;
    const char *s = sa5_str(text);
    bool hit = false;
    if (pats && pats->type == JSON_ARRAY && s) {
        for (size_t i = 0; i < json_len(pats) && !hit; i++) {
            const char *p = sa5_str(json_get(pats, i));
            if (p && p[0] && strstr(s, p) != NULL) hit = true;
        }
    }
    json_free(pats);
    return json_bool(hit);
}

/* PoP: sa5_append_block_text @ plugins/platforms/slack/adapter.py:_append_block_text */
/* static: merge additional block text + serialized block payload into text. */
static json_t *sa5_append_block_text(sa5_ctx_t *ctx, const json_t *args) {
    json_t *text = json_obj_get(args, "text");
    json_t *blocks = json_obj_get(args, "blocks");
    json_t *uid = json_obj_get(args, "bot_uid");
    char *cur = sa5_pystr(text);

    json_t *a1 = json_object();
    json_set(a1, "blocks", json_copy(blocks ? blocks : json_null()));
    json_set(a1, "text", json_copy(text ? text : json_null()));
    json_set(a1, "bot_uid", json_copy(uid ? uid : json_null()));
    json_t *bt = sa5_call(ctx, "extract_additional_text_from_slack_blocks", a1);
    if (sa5_is_raise(bt)) { free(cur); return bt; }
    char *bs = sa5_truthy(bt) ? sa5_pystr(bt) : sa5_strdup("");
    char *st = sa5_strip(bs);
    json_free(bt);
    if (st && st[0]) {
        json_t *la = json_object();
        json_set(la, "msg", json_string("Slack: extracted additional text from blocks (likely quoted/forwarded content; chars=%d)"));
        json_t *arr = json_array();
        json_append(arr, json_number((double)strlen(st)));
        json_set(la, "args", arr);
        json_t *lr = sa5_call(ctx, "logger.debug", la);
        if (sa5_is_raise(lr)) { free(cur); free(bs); free(st); return lr; }
        json_free(lr);
        size_t n = strlen(cur ? cur : "") + strlen(st) + 2;
        char *buf = (char *)malloc(n);
        if (buf) snprintf(buf, n, "%s\n%s", sa5_strip(cur ? cur : ""), st);
        char *j = sa5_strip(buf ? buf : "");
        free(cur); cur = j; free(buf);
    }
    free(bs); free(st);

    json_t *a2 = json_object();
    json_set(a2, "blocks", json_copy(blocks ? blocks : json_null()));
    json_t *bp = sa5_call(ctx, "serialize_slack_blocks_for_agent", a2);
    if (sa5_is_raise(bp)) { free(cur); return bp; }
    if (sa5_truthy(bp)) {
        char *ps = sa5_pystr(bp);
        size_t n = strlen(cur ? cur : "") + strlen(ps) + 3;
        char *buf = (char *)malloc(n);
        if (buf) snprintf(buf, n, "%s\n\n%s", sa5_strip(cur ? cur : ""), ps);
        char *j = sa5_strip(buf ? buf : "");
        free(cur); cur = j; free(buf); free(ps);
    }
    json_free(bp);
    json_t *ret = json_string(cur ? cur : "");
    free(cur);
    return ret;
}

/* PoP: sa5_thread_rehydration_key @ plugins/platforms/slack/adapter.py:_thread_rehydration_key */
/* instance: per-process key for the once-per-thread rehydration check. */
static json_t *sa5_thread_rehydration_key(sa5_ctx_t *ctx, const json_t *args) {
    const char *ch = sa5_str(json_obj_get(args, "channel_id")) ? sa5_str(json_obj_get(args, "channel_id")) : "";
    const char *tts = sa5_str(json_obj_get(args, "thread_ts")) ? sa5_str(json_obj_get(args, "thread_ts")) : "";
    const char *uid = sa5_str(json_obj_get(args, "user_id")) ? sa5_str(json_obj_get(args, "user_id")) : "";
    const char *team = sa5_str(json_obj_get(args, "team_id")) ? sa5_str(json_obj_get(args, "team_id")) : "";
    char base[1024];
    snprintf(base, sizeof base, "%s:%s:%s", team, ch, tts);
    json_t *store = json_obj_get(ctx->state, "_session_store");
    json_t *cfg = sa5_getattr(store, "config");
    bool per_user = false;
    if (sa5_is_obj(cfg)) {
        json_t *pu = json_obj_get(sa5_obj_attrs(cfg), "thread_sessions_per_user");
        per_user = sa5_truthy(pu);
    }
    json_free(cfg);
    char out[1200];
    if (per_user) snprintf(out, sizeof out, "%s:%s", base, uid);
    else snprintf(out, sizeof out, "%s", base);
    return json_string(out);
}

/* PoP: sa5_thread_session_source @ plugins/platforms/slack/adapter.py:_thread_session_source */
/* instance: build_source provenance for a thread. */
static json_t *sa5_thread_session_source(sa5_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(json_obj_get(args, "channel_id")));
    json_set(a, "chat_type", json_copy(json_obj_get(args, "chat_type")));
    json_set(a, "user_id", json_copy(json_obj_get(args, "user_id")));
    json_set(a, "thread_id", json_copy(json_obj_get(args, "thread_ts")));
    json_t *team = json_obj_get(args, "team_id");
    json_set(a, "scope_id", sa5_truthy(team) ? json_copy(team) : json_null());
    return sa5_call(ctx, "build_source", a);
}
