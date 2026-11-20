/*
 * slack_adapter_flow4.c -- FLOW cores, wave 4, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's STILL-UNCLAIMED remainder after flow (28 leaves), flow2 (26 flows)
 * and flow3 (12 flows).  All ported names were UNCLAIMED by a CRLF-aware
 * strict PoP grep of the whole main-only repo at port time.
 *
 * Ported (9 functions, all UNCLAIMED by the CRLF-aware strict PoP grep at
 * port time; each carries a PoP annotation at its definition below):
 *   _sdk_supports_agent_sessions, _interaction_fields, _section_text,
 *   _parse_users_info, _slack_file_kind, _cron_continuable_surface,
 *   _slack_allow_bots, _reactions_enabled, _is_retryable_upload_error.
 *
 * Decision sequences only: every injected/unported callable
 * (sdk_agent_sessions_probe, is_slack_voice_clip [claimed flow1],
 * _extra_or_secret, logger.warning, _is_retryable_error) enters through ONE
 * universal seam sa4_call(ctx, name, args).  The harness wires it to a
 * recording stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow4.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - {"__obj__": attrs} is an attribute carrier (SimpleNamespace double);
 *   a plain JSON object is a Python dict.  {"__exc__": {...}} is an exception
 *   instance: "type"/"message" are the ctor args, other keys are attributes.
 *   "exc_message_attr" (if present) is the value of the live `exc.message`
 *   attribute (a RuntimeError has no .message unless explicitly set).
 * - isinstance/except families walk the FULL builtin exception table below.
 * - state["extra"] is the plain dict behind self.config.extra;
 *   state["_AGENT_SESSIONS_SUPPORTED"] is the SDK-probe module global.
 * - str() positions carry str/int/bool/None only (no containers, no
 *   non-integral floats).  status_code fixtures are integral numbers.
 *   exc kinds are non-KeyError so str(exc) == message.  Exception instances
 *   on the body-join path carry response absent/null (else str(SimpleNamespace)
 *   would be untranscribable).  Document these domain guards.
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
typedef struct sa4_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa4_ctx_t;

/* -- basic helpers -- */
static bool sa4_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa4_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa4_obj_attrs(const json_t *v) {
    return sa4_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa4_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa4_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa4_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa4_is_obj(v) &&
           !sa4_is_set(v) && !sa4_is_tuple_marker(v) && !sa4_is_raise(v);
}
static bool sa4_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa4_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa4_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa4_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa4_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa4_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sa4_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa4_call(sa4_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa4_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa4_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa4_make_exc(raise);
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
typedef struct sa4_exc_parent { const char *name, *parent1, *parent2; } sa4_exc_parent_t;
static const sa4_exc_parent_t SA4_EXC_TABLE[] = {
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
#define SA4_EXC_TABLE_LEN (sizeof(SA4_EXC_TABLE) / sizeof(SA4_EXC_TABLE[0]))

static const sa4_exc_parent_t *sa4_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA4_EXC_TABLE_LEN; i++)
        if (strcmp(SA4_EXC_TABLE[i].name, name) == 0) return &SA4_EXC_TABLE[i];
    return NULL;
}
typedef struct sa4_chain { char names[64][96]; size_t n; } sa4_chain_t;
static bool sa4_chain_has(const sa4_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa4_chain_add(sa4_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa4_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa4_chain_walk(sa4_chain_t *c, const char *name) {
    if (!name || !name[0] || sa4_chain_has(c, name)) return;
    sa4_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa4_exc_parent_t *be = sa4_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";  /* synthetic -> Exception */
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa4_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa4_chain_walk(c, p2);
}
static bool sa4_exc_family(const json_t *r, const char *family) {
    if (!sa4_is_raise(r)) return false;
    sa4_chain_t c; c.n = 0;
    sa4_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa4_chain_has(&c, family);
}
static bool sa4_raise_passes(const json_t *r) {
    return sa4_is_raise(r) && !sa4_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa4_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa4_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa4_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa4_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa4_lower(const char *s) {
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
static char *sa4_lower_strip(const char *s) {
    char *lo = sa4_lower(s);
    char *st = sa4_strip(lo);
    free(lo);
    return st;
}
static bool sa4_in_set_str(const char *const *items, size_t n, const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, items[i]) == 0) return true;
    return false;
}
/* Python str() of a scalar (ASCII domain: str/int/bool/None only). */
static char *sa4_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa4_strdup("None");
    if (v->type == JSON_STRING) return sa4_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa4_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa4_strdup(buf);
    }
    return sa4_strdup("");
}
/* dict.get(key, default): absent -> default; present-null -> JSON_NULL node. */
static json_t *sa4_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static const char *sa4_pytype_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) return (v->num_val == floor(v->num_val)) ? "int" : "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    if (sa4_is_obj(v)) return "types.SimpleNamespace";
    if (sa4_is_set(v)) return "set";
    return "dict";
}
static json_t *sa4_no_get(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sa4_pytype_name(v));
    return sa4_err("AttributeError", msg);
}
static json_t *sa4_no_startswith(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'startswith'", sa4_pytype_name(v));
    return sa4_err("AttributeError", msg);
}

/* ====================================================================== */
/* PoP: sa4_sdk_supports_agent_sessions @ plugins/platforms/slack/adapter.py:_sdk_supports_agent_sessions */
/* global _AGENT_SESSIONS_SUPPORTED; try: probe = callable(getattr(AsyncWebClient,...)) except Exception: False */
static json_t *sa4_sdk_supports_agent_sessions(sa4_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cached = json_obj_get(ctx->state, "_AGENT_SESSIONS_SUPPORTED");
    if (cached && cached->type != JSON_NULL) return json_copy(cached);
    json_t *r = sa4_call(ctx, "sdk_agent_sessions_probe", json_object());
    bool val = false;
    if (sa4_is_raise(r)) {
        if (sa4_raise_passes(r)) return r;  /* BaseException-only propagates */
        val = false;                        /* except Exception -> False */
        json_free(r);
    } else {
        val = sa4_truthy(r);                /* callable() result: bool domain */
        json_free(r);
    }
    json_t *b = json_bool(val);
    json_set(ctx->state, "_AGENT_SESSIONS_SUPPORTED", json_copy(b));
    return b;
}

/* PoP: sa4_interaction_fields @ plugins/platforms/slack/adapter.py:_interaction_fields */
/* static: 7-tuple of chained dict.get defaults */
static json_t *sa4_interaction_fields(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *body = json_obj_get(args, "body");
    json_t *action = json_obj_get(args, "action");
    if (!sa4_plain_dict(body)) return sa4_no_get(body);
    if (!sa4_plain_dict(action)) return sa4_no_get(action);
    json_t *msg = sa4_get(body, "message", json_object());
    if (!sa4_plain_dict(msg)) { json_t *e = sa4_no_get(msg); json_free(msg); return e; }
    json_t *chan = sa4_get(body, "channel", json_object());
    json_t *user = sa4_get(body, "user", json_object());
    json_t *out = json_array();
    json_append(out, sa4_get(action, "action_id", json_string("")));
    json_append(out, sa4_get(action, "value", json_string("")));
    json_append(out, json_copy(msg));
    json_append(out, sa4_get(msg, "ts", json_string("")));
    if (!sa4_plain_dict(chan)) {
        json_t *e = sa4_no_get(chan);
        json_free(chan); json_free(user); json_free(msg); json_free(out);
        return e;
    }
    json_append(out, sa4_get(chan, "id", json_string("")));
    if (!sa4_plain_dict(user)) {
        json_t *e = sa4_no_get(user);
        json_free(user); json_free(msg); json_free(out);
        return e;
    }
    json_append(out, sa4_get(user, "name", json_string("unknown")));
    json_append(out, sa4_get(user, "id", json_string("")));
    json_free(chan); json_free(user); json_free(msg);
    return out;
}

/* PoP: sa4_section_text @ plugins/platforms/slack/adapter.py:_section_text */
/* static: first section block text, truncated to limit (None -> no slice) */
static json_t *sa4_section_text(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *message = json_obj_get(args, "message");
    json_t *blocks = sa4_get(message, "blocks", json_array());
    if (!blocks || blocks->type != JSON_ARRAY) {
        json_t *e = sa4_no_get(blocks); json_free(blocks); return e;
    }
    /* default limit 3000; present null -> None (no slice) */
    json_t *lim = json_obj_get(args, "limit");
    bool do_slice = true;
    long long limit = 3000;
    if (lim && lim->type == JSON_NULL) {
        do_slice = false;
    } else if (lim && lim->type == JSON_NUMBER) {
        limit = (long long)lim->num_val;
    } else if (lim) {
        do_slice = false;
    }
    char *text = sa4_strdup("");
    for (size_t i = 0; i < json_len(blocks); i++) {
        json_t *block = json_get(blocks, i);
        if (!sa4_plain_dict(block)) {
            json_t *e = sa4_no_get(block);
            json_free(blocks); free(text); return e;
        }
        json_t *bt = sa4_get(block, "type", json_string(""));
        const char *bts = sa4_str(bt);
        bool is_section = bts && strcmp(bts, "section") == 0;
        json_free(bt);
        if (is_section) {
            json_t *tv = sa4_get(block, "text", NULL);
            json_t *d = sa4_truthy(tv) ? tv : (json_t *)NULL;
            if (d && !sa4_plain_dict(d)) {
                json_t *e = sa4_no_get(d);
                json_free(tv); json_free(blocks); free(text); return e;
            }
            json_t *inner = d ? sa4_get(d, "text", json_string("")) : json_string("");
            free(text);
            text = sa4_strdup(sa4_str(inner) ? sa4_str(inner) : "");
            json_free(inner);
            json_free(tv);
            break;
        }
    }
    json_t *ret;
    if (!do_slice || limit < 0 || text == NULL) {
        ret = json_string(text ? text : "");
    } else {
        size_t n = strlen(text);
        size_t take = (size_t)limit < n ? (size_t)limit : n;
        char *buf = (char *)malloc(take + 1);
        if (buf) { memcpy(buf, text, take); buf[take] = '\0'; }
        ret = json_string(buf ? buf : "");
        free(buf);
    }
    free(text);
    json_free(blocks);
    return ret;
}

/* PoP: sa4_parse_users_info @ plugins/platforms/slack/adapter.py:_parse_users_info */
/* static: (display name, is_bot) from a users.info payload */
static json_t *sa4_parse_users_info(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *payload = json_obj_get(args, "payload");
    json_t *user_id = json_obj_get(args, "user_id");
    json_t *user = sa4_get(payload, "user", json_object());
    if (!sa4_plain_dict(user)) {
        json_t *e = sa4_no_get(user); json_free(user); return e;
    }
    json_t *profile = sa4_get(user, "profile", json_object());
    bool profile_is_dict = sa4_plain_dict(profile);
    bool is_bot = sa4_truthy(sa4_get(user, "is_bot", NULL)) ||
                  sa4_truthy(sa4_get(user, "is_workflow_bot", NULL)) ||
                  (profile_is_dict && sa4_truthy(sa4_get(profile, "bot_id", NULL)));
    /* name = first truthy of display_name/real_name(profile)/real_name/name(user)/user_id */
    json_t *name = NULL;
    const char *keys_p[] = {"display_name", "real_name"};
    for (size_t i = 0; i < 2 && !sa4_truthy(name); i++) {
        json_free(name);
        name = profile_is_dict ? sa4_get(profile, keys_p[i], NULL) : json_null();
    }
    const char *keys_u[] = {"real_name", "name"};
    for (size_t i = 0; i < 2 && !sa4_truthy(name); i++) {
        json_free(name);
        name = sa4_get(user, keys_u[i], NULL);
    }
    if (!sa4_truthy(name)) { json_free(name); name = json_copy(user_id ? user_id : json_null()); }
    if (!sa4_plain_dict(profile)) {
        /* profile.get("display_name") is evaluated unconditionally by the
         * name or-chain, so a non-dict profile always raises AttributeError. */
        json_t *e = sa4_no_get(profile);
        json_free(profile); json_free(user); json_free(name); return e;
    }
    json_free(profile); json_free(user);
    json_t *out = json_array();
    json_append(out, name);
    json_append(out, json_bool(is_bot));
    return out;
}

/* PoP: sa4_slack_file_kind @ plugins/platforms/slack/adapter.py:_slack_file_kind */
/* static: image/audio/voice clip/video/document from mimetype (+ claimed sibling seam) */
static json_t *sa4_slack_file_kind(sa4_ctx_t *ctx, const json_t *args) {
    json_t *f = json_obj_get(args, "f");
    json_t *mt = json_obj_get(args, "mimetype");
    const char *mime = sa4_str(mt);
    if (!mime) {
        /* mt is borrowed from args; do not free it (the harness frees args). */
        return sa4_no_startswith(mt);
    }
    static const char *pairs[2][2] = {{"image", "image/"}, {"audio", "audio/"}};
    for (int i = 0; i < 2; i++) {
        if (strncmp(mime, pairs[i][1], strlen(pairs[i][1])) == 0)
            return json_string(pairs[i][0]);
    }
    if (strncmp(mime, "video/", 6) == 0) {
        json_t *a = json_object();
        json_set(a, "f", f ? json_copy(f) : json_null());
        json_t *r = sa4_call(ctx, "is_slack_voice_clip", a);
        if (sa4_is_raise(r)) return r;
        bool clip = sa4_truthy(r);
        json_free(r);
        return json_string(clip ? "voice clip" : "video");
    }
    return json_string("document");
}

/* PoP: sa4_cron_continuable_surface @ plugins/platforms/slack/adapter.py:_cron_continuable_surface */
/* instance: str(raw).strip().lower() == "in_channel" gate */
static json_t *sa4_cron_continuable_surface(sa4_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *extra = json_obj_get(ctx->state, "extra");
    json_t *raw = sa4_get(extra, "cron_continuable_surface", NULL);
    char *s = sa4_pystr(raw);
    char *lo = sa4_lower(s);
    char *st = sa4_strip(lo);
    bool in_channel = st && strcmp(st, "in_channel") == 0;
    free(s); free(lo); free(st); json_free(raw);
    return json_string(in_channel ? "in_channel" : "thread");
}

/* PoP: sa4_slack_allow_bots @ plugins/platforms/slack/adapter.py:_slack_allow_bots */
/* instance: scoped policy -> normalized value; unknown warns + "none" */
static json_t *sa4_slack_allow_bots(sa4_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set(a, "extra", json_copy(json_obj_get(ctx->state, "extra")));
    json_set(a, "key", json_string("allow_bots"));
    json_set(a, "env_var", json_string("SLACK_ALLOW_BOTS"));
    json_set(a, "default", json_string("none"));
    json_t *raw = sa4_call(ctx, "_extra_or_secret", a);
    if (sa4_is_raise(raw)) return raw;
    char *s = sa4_pystr(raw);
    char *v = sa4_lower_strip(s);
    static const char *VALID[3] = {"none", "mentions", "all"};
    bool ok = sa4_in_set_str(VALID, 3, v);
    if (!ok) {
        json_t *la = json_object();
        json_set(la, "msg", json_string("[Slack] Unknown allow_bots=%r; treating as 'none'"));
        json_t *arr = json_array();
        json_append(arr, json_copy(raw));
        json_set(la, "args", arr);
        json_t *lr = sa4_call(ctx, "logger.warning", la);
        if (sa4_is_raise(lr)) { free(s); free(v); json_free(raw); return lr; }
        json_free(lr);
        free(s); free(v); json_free(raw);
        return json_string("none");
    }
    json_t *ret = json_string(v ? v : "");
    free(s); free(v); json_free(raw);
    return ret;
}

/* PoP: sa4_reactions_enabled @ plugins/platforms/slack/adapter.py:_reactions_enabled */
/* instance: scoped policy NOT in {false,0,no} */
static json_t *sa4_reactions_enabled(sa4_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set(a, "extra", json_copy(json_obj_get(ctx->state, "extra")));
    json_set(a, "key", json_string("reactions"));
    json_set(a, "env_var", json_string("SLACK_REACTIONS"));
    json_set(a, "default", json_string("true"));
    json_t *raw = sa4_call(ctx, "_extra_or_secret", a);
    if (sa4_is_raise(raw)) return raw;
    char *s = sa4_pystr(raw);
    char *lo = sa4_lower(s);
    static const char *OFF[3] = {"false", "0", "no"};
    bool enabled = !sa4_in_set_str(OFF, 3, lo);
    free(s); free(lo); json_free(raw);
    return json_bool(enabled);
}

/* PoP: sa4_is_retryable_upload_error @ plugins/platforms/slack/adapter.py:_is_retryable_upload_error */
/* instance: status_code fast path, transient-marker body scan, else seamed classifier */
static json_t *sa4_is_retryable_upload_error(sa4_ctx_t *ctx, const json_t *args) {
    json_t *excw = json_obj_get(args, "exc");
    json_t *spec = (excw && excw->type == JSON_OBJECT) ? json_obj_get(excw, "__exc__") : NULL;
    if (!spec) return sa4_err("TypeError", "expected exception double");
    json_t *resp = json_obj_get(spec, "response");          /* __obj__ carrier or null/absent */
    json_t *ra = sa4_obj_attrs(resp);
    json_t *sc = ra ? json_obj_get(ra, "status_code") : NULL;
    if (sc && sc->type != JSON_NULL && sc->type == JSON_NUMBER) {
        bool retry = (sc->num_val == 429.0) || (sc->num_val >= 500.0);
        return json_bool(retry);
    }
    /* body = " ".join(str(part) for part in (exc, exc.message, exc.response) if part).lower()
     * domain: response is absent/null on the body path, so only the two string
     * parts can contribute; str(exc) == message (non-KeyError kinds). */
    const char *msg = json_get_str(spec, "message", "");
    json_t *mattr = json_obj_get(spec, "exc_message_attr");
    const char *m2 = (mattr && mattr->type == JSON_STRING) ? mattr->str_val : "";
    size_t cap = strlen(msg ? msg : "") + strlen(m2) + 4;
    char *body = (char *)malloc(cap);
    body[0] = '\0';
    if (msg && msg[0]) strcat(body, msg);
    if (m2[0]) { if (body[0]) strcat(body, " "); strcat(body, m2); }
    static const char *MARKERS[6] = {
        "rate_limited", "ratelimited", "429", "connection reset",
        "service unavailable", "temporarily unavailable"
    };
    bool hit = false;
    char *lo = sa4_lower(body);
    for (int i = 0; i < 6 && !hit; i++) hit = strstr(lo, MARKERS[i]) != NULL;
    free(lo);
    if (hit) { free(body); return json_bool(true); }
    lo = sa4_lower(body);
    json_t *aa = json_object();
    json_set(aa, "body", json_string(lo));
    json_t *r = sa4_call(ctx, "_is_retryable_error", aa);
    if (sa4_is_raise(r)) { free(lo); free(body); return r; }
    bool retry = sa4_truthy(r);
    json_free(r); free(lo); free(body);
    return json_bool(retry);
}
