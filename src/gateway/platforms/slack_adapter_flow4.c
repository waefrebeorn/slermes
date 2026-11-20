/*
 * slack_adapter_flow4.c -- FLOW cores, wave 4, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's STILL-UNCLAIMED remainder after flow (28 leaves), flow2 (26 flows)
 * and flow3 (12 flows).  All ported names were UNCLAIMED by a CRLF-aware
 * strict PoP grep of the whole main-only repo at port time.
 *
 * Ported (25 functions, all UNCLAIMED at their port time; each carries a PoP
 * annotation at its definition below). Wave-50 core (9):
 *   _sdk_supports_agent_sessions, _interaction_fields, _section_text,
 *   _parse_users_info, _slack_file_kind, _cron_continuable_surface,
 *   _slack_allow_bots, _reactions_enabled, _is_retryable_upload_error.
 * Coherent block-building / event-field-extraction remainder (16, this wave):
 *   _button, _markdown_block_payload, _feedback_block, _append_feedback_block,
 *   _build_identity_prompt, _event_team_id, _context_channel_id,
 *   _first_file_share, _is_foreign_author, _agent_view_event_fields,
 *   _extract_assistant_thread_metadata, _event_declares_bot_sender,
 *   _resolve_thread_ts, _session_thread_ts, _normalize_changed_message,
 *   _assistant_thread_title_enabled.
 *
 * Decision sequences only: every injected/unported callable
 * (sdk_agent_sessions_probe, is_slack_voice_clip [claimed flow1],
 * _extra_or_secret, logger.warning, _is_retryable_error, t,
 * slack_api_human_users) enters through ONE universal seam
 * sa4_call(ctx, name, args).  The harness wires it to a
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

/* ====================================================================== */
/* Second coherent subset: block-building + event-field extraction.        */
/* Inline transcriptions of claimed pure leaves (NO new PoP, no seam name): */
/*   _str_or_empty, _first_truthy, _extra_flag, _team_in_install_set,       */
/*   _workspace_message_marker.                                            */

/* str(v) if truthy else "" -- matches _str_or_empty(value). */
static json_t *sa4_str_or_empty(const json_t *v) {
    if (!sa4_truthy(v)) return json_string("");
    char *s = sa4_pystr(v);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}
/* First truthy mapping[key] in keys order, else None -- _first_truthy. */
static json_t *sa4_first_truthy(const json_t *mapping, const char *const *keys, size_t n) {
    for (size_t i = 0; i < n; i++) {
        json_t *v = sa4_get(mapping, keys[i], NULL);
        if (sa4_truthy(v)) return v;
        json_free(v);
    }
    return json_null();
}
/* config.extra[key] boolean with str forms -- _extra_flag (default when unset). */
static bool sa4_extra_flag(sa4_ctx_t *ctx, const char *key, bool def) {
    json_t *extra = json_obj_get(ctx->state, "extra");
    json_t *raw = sa4_get(extra, key, NULL);
    if (!raw || raw->type == JSON_NULL) { json_free(raw); return def; }
    if (raw->type == JSON_BOOL) { bool b = raw->bool_val; json_free(raw); return b; }
    char *s = sa4_pystr(raw);
    char *lo = sa4_lower_strip(s);
    static const char *ON[4] = {"1", "true", "yes", "on"};
    bool on = sa4_in_set_str(ON, 4, lo);
    free(s); free(lo); json_free(raw);
    return on;
}
/* Team pass-through when it names an installed workspace -- _team_in_install_set. */
static json_t *sa4_team_in_install_set(sa4_ctx_t *ctx, const json_t *team) {
    if (!sa4_truthy(team)) return json_string("");
    char *s = sa4_pystr(team);
    json_t *cl = json_obj_get(ctx->state, "_team_clients");
    bool ok = sa4_plain_dict(cl) && json_obj_get(cl, s) != NULL;
    /* cl is BORROWED from ctx->state -- never free it here. */
    json_t *r = json_string(ok ? s : "");
    free(s);
    return r;
}
/* String-membership in a {"__set__":[...]} carrier. */
static bool sa4_set_has_str(const json_t *setnode, const char *s) {
    json_t *arr = sa4_is_set(setnode) ? json_obj_get(setnode, "__set__") : NULL;
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *ms = sa4_str(json_get(arr, i));
        if (ms && strcmp(ms, s) == 0) return true;
    }
    return false;
}

/* PoP: sa4_button @ plugins/platforms/slack/adapter.py:_button */
/* static: Block Kit button; style/emoji keys only present when set. */
static json_t *sa4_button(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *text = json_obj_get(args, "text");
    json_t *action_id = json_obj_get(args, "action_id");
    json_t *value = json_obj_get(args, "value");
    json_t *emoji = json_obj_get(args, "emoji");
    json_t *style = json_obj_get(args, "style");
    json_t *to = json_object();
    json_set(to, "type", json_string("plain_text"));
    json_set(to, "text", json_copy(text ? text : json_null()));
    if (sa4_truthy(emoji)) json_set(to, "emoji", json_bool(true));
    json_t *btn = json_object();
    json_set(btn, "type", json_string("button"));
    json_set(btn, "text", to);
    const char *st = sa4_str(style);
    if (st && st[0]) json_set(btn, "style", json_string(st));
    json_set(btn, "action_id", json_copy(action_id ? action_id : json_null()));
    json_set(btn, "value", json_copy(value ? value : json_null()));
    return btn;
}

/* PoP: sa4_markdown_block_payload @ plugins/platforms/slack/adapter.py:_markdown_block_payload */
/* instance: markdown block or None (empty / > _MARKDOWN_BLOCK_MAX=11500). */
static json_t *sa4_markdown_block_payload(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *content = json_obj_get(args, "content");
    if (!sa4_truthy(content)) return json_null();
    const char *s = sa4_str(content);
    if (!s) return json_null();
    char *st = sa4_strip(s);
    bool ok = st && st[0] && strlen(s) <= 11500;
    free(st);
    if (!ok) return json_null();
    json_t *b = json_object();
    json_set(b, "type", json_string("markdown"));
    json_set(b, "text", json_string(s));
    json_t *arr = json_array();
    json_append(arr, b);
    return arr;
}

/* PoP: sa4_feedback_block @ plugins/platforms/slack/adapter.py:_feedback_block */
/* instance: feedback_buttons context_actions block; 4 seamed t() translations. */
static json_t *sa4_feedback_block(sa4_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *KEYS[4] = {
        "platform.slack.feedback.good", "platform.slack.feedback.good_a11y",
        "platform.slack.feedback.bad", "platform.slack.feedback.bad_a11y"};
    char *tr[4];
    for (int i = 0; i < 4; i++) {
        json_t *a = json_object();
        json_set(a, "key", json_string(KEYS[i]));
        json_t *r = sa4_call(ctx, "t", a);
        if (sa4_is_raise(r)) {
            for (int j = 0; j < i; j++) free(tr[j]);
            return r;
        }
        const char *s = sa4_str(r);
        tr[i] = sa4_strdup(s ? s : "");
        json_free(r);
        if (strlen(tr[i]) > 75) tr[i][75] = '\0';
    }
    json_t *pos_text = json_object();
    json_set(pos_text, "type", json_string("plain_text"));
    json_set(pos_text, "text", json_string(tr[0]));
    json_t *pos = json_object();
    json_set(pos, "text", pos_text);
    json_set(pos, "accessibility_label", json_string(tr[1]));
    json_set(pos, "value", json_string("positive"));
    json_t *neg_text = json_object();
    json_set(neg_text, "type", json_string("plain_text"));
    json_set(neg_text, "text", json_string(tr[2]));
    json_t *neg = json_object();
    json_set(neg, "text", neg_text);
    json_set(neg, "accessibility_label", json_string(tr[3]));
    json_set(neg, "value", json_string("negative"));
    for (int i = 0; i < 4; i++) free(tr[i]);
    json_t *el = json_object();
    json_set(el, "type", json_string("feedback_buttons"));
    json_set(el, "action_id", json_string("hermes_feedback"));
    json_set(el, "positive_button", pos);
    json_set(el, "negative_button", neg);
    json_t *els = json_array();
    json_append(els, el);
    json_t *block = json_object();
    json_set(block, "type", json_string("context_actions"));
    json_set(block, "elements", els);
    return block;
}

/* PoP: sa4_append_feedback_block @ plugins/platforms/slack/adapter.py:_append_feedback_block */
/* instance: append feedback block when enabled and len(blocks)<50. */
static json_t *sa4_append_feedback_block(sa4_ctx_t *ctx, const json_t *args) {
    json_t *blocks = json_obj_get(args, "blocks");
    if (!sa4_truthy(blocks) || !blocks || blocks->type != JSON_ARRAY) return json_copy(blocks);
    bool flag = sa4_extra_flag(ctx, "feedback_buttons", false);
    if (!flag || json_len(blocks) >= 50) return json_copy(blocks);
    json_t *fb = sa4_feedback_block(ctx, args);
    if (sa4_is_raise(fb)) return fb;
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(blocks); i++) json_append(out, json_copy(json_get(blocks, i)));
    json_append(out, fb);
    return out;
}

/* PoP: sa4_build_identity_prompt @ plugins/platforms/slack/adapter.py:_build_identity_prompt */
/* instance: ephemeral system-prompt line naming the bot handle. */
static json_t *sa4_build_identity_prompt(sa4_ctx_t *ctx, const json_t *args) {
    json_t *tdv = json_obj_get(args, "team_id");
    const char *td = sa4_str(tdv) ? sa4_str(tdv) : "";
    json_t *names = json_obj_get(ctx->state, "_team_bot_names");
    char name[512] = "";
    if (td[0]) {
        json_t *nm = sa4_plain_dict(names) ? json_obj_get(names, td) : NULL;
        const char *ns = sa4_str(nm);
        if (ns) snprintf(name, sizeof name, "%s", ns);
    }
    if (!name[0]) {
        json_t *dn = json_obj_get(ctx->state, "_bot_display_name");
        const char *ds = sa4_str(dn);
        if (ds) snprintf(name, sizeof name, "%s", ds);
    }
    char *st = sa4_strip(name);
    bool empty = !st || !st[0];
    if (empty) { free(st); return json_string(""); }
    char out[4096];
    snprintf(out, sizeof out,
        "You are connected to this Slack workspace as the bot \"@%s\". "
        "The adapter already applied mention and channel routing; treat every "
        "delivered turn as intentionally routed to you. Your routing mention "
        "\"@%s\" may have been stripped from the visible text \xe2\x80\x94 do not "
        "reject or ignore a message solely because \"@%s\" is absent. In "
        "messages, each line is prefixed with the sender's name, and visible "
        "mentions are shown as @DisplayName; a mention of any other participant "
        "is not a mention of you, even if their name is similar.", st, st, st);
    free(st);
    return json_string(out);
}

/* PoP: sa4_event_team_id @ plugins/platforms/slack/adapter.py:_event_team_id */
/* static: candidate order authorizations[0] > envelope > event/body team. */
static json_t *sa4_event_team_id(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *event = json_obj_get(args, "event");
    json_t *body = json_obj_get(args, "body");
    json_t *install_set = json_obj_get(args, "install_set");
    char *cands[8]; size_t nc = 0;
    if (sa4_plain_dict(body)) {
        json_t *auths = sa4_get(body, "authorizations", NULL);
        if (sa4_truthy(auths) && auths->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(auths) && nc < 8; i++) {
                json_t *a = json_get(auths, i);
                json_t *tid = sa4_plain_dict(a) ? sa4_get(a, "team_id", NULL) : NULL;
                if (sa4_truthy(tid)) {
                    cands[nc++] = sa4_pystr(tid);
                    json_free(tid);
                    break;
                }
                json_free(tid);
            }
        }
        json_free(auths);
        json_t *env = sa4_get(body, "team_id", NULL);
        if (env && env->type == JSON_STRING && sa4_truthy(env)) cands[nc++] = sa4_strdup(env->str_val);
        else if (sa4_plain_dict(env)) {
            json_t *id = sa4_get(env, "id", NULL);
            if (sa4_truthy(id)) cands[nc++] = sa4_pystr(id);
            json_free(id);
        }
        json_free(env);
    }
    for (int p = 0; p < 2; p++) {
        json_t *payload = p == 0 ? event : body;
        if (!sa4_plain_dict(payload)) continue;
        json_t *team = sa4_get(payload, "team_id", NULL);
        if (!sa4_truthy(team)) { json_free(team); team = sa4_get(payload, "team", NULL); }
        if (!sa4_truthy(team)) { json_free(team); continue; }
        if (team->type == JSON_STRING && sa4_truthy(team)) cands[nc++] = sa4_strdup(team->str_val);
        else if (sa4_plain_dict(team)) {
            json_t *id = sa4_get(team, "id", NULL);
            if (sa4_truthy(id)) cands[nc++] = sa4_pystr(id);
            json_free(id);
        }
        json_free(team);
    }
    json_t *ret = NULL;
    bool has_set = install_set && install_set->type != JSON_NULL && sa4_plain_dict(install_set);
    bool set_nonempty = has_set && json_object_size(install_set) > 0;
    if (has_set && set_nonempty) {
        for (size_t i = 0; i < nc; i++) {
            if (json_obj_get(install_set, cands[i])) { ret = json_string(cands[i]); break; }
        }
        if (!ret && json_object_size(install_set) == 1)
            ret = json_string(json_object_get_key_at(install_set, 0));
        if (!ret) ret = json_string("");
    } else {
        ret = json_string(nc ? cands[0] : "");
    }
    for (size_t i = 0; i < nc; i++) free(cands[i]);
    return ret;
}

/* PoP: sa4_context_channel_id @ plugins/platforms/slack/adapter.py:_context_channel_id */
/* static: actively viewed channel from either context shape. */
static json_t *sa4_context_channel_id(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    /* Two call shapes: the harness passes {"context": <dict>}; the two flow
     * callers (agent_view / assistant_thread) pass the context dict itself.
     * Accept both and normalize to the context dict. */
    const json_t *context = args;
    json_t *wrapped = (args && args->type == JSON_OBJECT)
                        ? json_obj_get(args, "context") : NULL;
    if (wrapped && sa4_plain_dict(wrapped)) context = wrapped;
    if (!sa4_plain_dict(context)) return json_string("");
    json_t *cid = sa4_get(context, "channel_id", NULL);
    if (sa4_truthy(cid)) { json_t *r = sa4_str_or_empty(cid); json_free(cid); return r; }
    json_free(cid);
    json_t *ents = sa4_get(context, "entities", NULL);
    if (sa4_truthy(ents) && ents->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ents); i++) {
            json_t *e = json_get(ents, i);
            if (!sa4_plain_dict(e)) continue;
            json_t *v = sa4_get(e, "value", NULL);
            if (sa4_plain_dict(v)) {
                json_t *ic = sa4_get(v, "channel_id", NULL);
                if (sa4_truthy(ic)) { json_t *r = sa4_str_or_empty(ic); json_free(ic); json_free(v); json_free(ents); return r; }
                json_free(ic);
            } else if (sa4_str(v)) {
                json_t *tv = sa4_get(e, "type", NULL);
                const char *ts = sa4_str(tv);
                size_t tl = ts ? strlen(ts) : 0, sl = strlen("channel_id");
                bool ends = tl >= sl && strcmp(ts + tl - sl, "channel_id") == 0;
                json_free(tv);
                if (ends) { json_t *r = json_string(v->str_val); json_free(v); json_free(ents); return r; }
            }
            json_free(v);
        }
    }
    json_free(ents);
    return json_string("");
}

/* PoP: sa4_first_file_share @ plugins/platforms/slack/adapter.py:_first_file_share */
/* static: first share for channel_id, else first share anywhere, else {}. */
static json_t *sa4_first_file_share(sa4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *fo = json_obj_get(args, "file_obj");
    const char *channel = sa4_str(json_obj_get(args, "channel_id"));
    json_t *shares = sa4_get(fo, "shares", NULL);
    json_t *share = NULL;
    if (sa4_plain_dict(shares)) {
        for (size_t i = 0; i < json_object_size(shares); i++) {
            json_t *bucket = json_object_get_at(shares, i);
            if (!sa4_plain_dict(bucket)) continue;
            if (channel) {
                json_t *cs = json_obj_get(bucket, channel);
                if (sa4_truthy(cs) && cs->type == JSON_ARRAY && json_len(cs)) {
                    json_t *first = json_get(cs, 0);
                    json_t *r = sa4_truthy(first) ? json_copy(first) : json_object();
                    json_free(shares); json_free(share);
                    return r;
                }
            }
            if (!share) {
                for (size_t j = 0; j < json_object_size(bucket); j++) {
                    json_t *lst = json_object_get_at(bucket, j);
                    if (sa4_truthy(lst) && lst->type == JSON_ARRAY && json_len(lst)) {
                        json_free(share);
                        share = json_copy(json_get(lst, 0));
                        break;
                    }
                }
            }
        }
    }
    json_free(shares);
    if (share && sa4_truthy(share)) return share;
    json_free(share);
    return json_object();
}

/* PoP: sa4_is_foreign_author @ plugins/platforms/slack/adapter.py:_is_foreign_author */
/* instance: T... author outside every authenticated workspace. */
static json_t *sa4_is_foreign_author(sa4_ctx_t *ctx, const json_t *args) {
    json_t *a = sa4_get(args, "author_team", NULL);
    json_t *r = sa4_get(args, "resolved_team", NULL);
    bool foreign = sa4_truthy(a) && sa4_truthy(r) && strcmp(sa4_str(a) ? sa4_str(a) : "", sa4_str(r) ? sa4_str(r) : "") != 0;
    if (foreign) {
        json_t *pass = sa4_team_in_install_set(ctx, a);
        foreign = !sa4_truthy(pass);
        json_free(pass);
    }
    json_free(a); json_free(r);
    return json_bool(foreign);
}

/* PoP: sa4_agent_view_event_fields @ plugins/platforms/slack/adapter.py:_agent_view_event_fields */
/* instance: {context_channel_id, user_id, team_id} from an Agent-view event. */
static json_t *sa4_agent_view_event_fields(sa4_ctx_t *ctx, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *body = json_obj_get(args, "body");
    json_t *context = sa4_get(event, "context", NULL);
    if (!sa4_truthy(context)) { json_free(context); context = sa4_get(event, "app_context", NULL); }
    if (!sa4_truthy(context)) { json_free(context); context = json_object(); }
    json_t *uid = sa4_get(event, "user", NULL);
    if (!sa4_truthy(uid)) { json_free(uid); uid = sa4_get(event, "user_id", NULL); }
    if (!sa4_truthy(uid)) { json_free(uid); uid = json_string(""); }
    json_t *te_args = json_object();
    json_set(te_args, "event", json_copy(event ? event : json_null()));
    json_set(te_args, "body", json_copy(body ? body : json_null()));
    json_t *cl = json_obj_get(ctx->state, "_team_clients");
    json_set(te_args, "install_set", json_copy(cl ? cl : json_null()));
    json_t *tid = sa4_event_team_id(ctx, te_args);
    json_t *ccid = sa4_context_channel_id(ctx, context);
    json_t *out = json_object();
    json_set(out, "context_channel_id", ccid);
    json_set(out, "user_id", sa4_str_or_empty(uid));
    json_set(out, "team_id", sa4_str_or_empty(tid));
    /* context/uid/tid/te_args owned; cl is BORROWED from ctx->state (not freed). */
    json_free(context); json_free(uid); json_free(tid); json_free(te_args);
    return out;
}

/* PoP: sa4_extract_assistant_thread_metadata @ plugins/platforms/slack/adapter.py:_extract_assistant_thread_metadata */
/* instance: Slack Assistant thread identity data from an event payload. */
static json_t *sa4_extract_assistant_thread_metadata(sa4_ctx_t *ctx, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *body = json_obj_get(args, "body");
    json_t *at = sa4_get(event, "assistant_thread", NULL);
    if (!sa4_plain_dict(at)) { json_free(at); at = json_object(); }
    json_t *context = sa4_get(at, "context", NULL);
    if (!sa4_truthy(context)) {
        json_free(context);
        const char *fk[2] = {"app_context", "context"};
        context = sa4_first_truthy(event, fk, 2);
    }
    if (!sa4_truthy(context)) { json_free(context); context = json_object(); }
    /* channel_id = assistant_thread.channel_id or event.channel or context.channel_id */
    json_t *cid = sa4_get(at, "channel_id", NULL);
    if (!sa4_truthy(cid)) { json_free(cid); cid = sa4_get(event, "channel", NULL); }
    if (!sa4_truthy(cid)) { json_free(cid); cid = sa4_get(context, "channel_id", NULL); }
    /* thread_ts = assistant_thread.thread_ts or first_truthy(event,(thread_ts,message_ts)) */
    json_t *tts = sa4_get(at, "thread_ts", NULL);
    if (!sa4_truthy(tts)) {
        json_free(tts);
        const char *fk[2] = {"thread_ts", "message_ts"};
        tts = sa4_first_truthy(event, fk, 2);
    }
    /* user_id = assistant_thread.user_id or event.user or context.user_id */
    json_t *uid = sa4_get(at, "user_id", NULL);
    if (!sa4_truthy(uid)) { json_free(uid); uid = sa4_get(event, "user", NULL); }
    if (!sa4_truthy(uid)) { json_free(uid); uid = sa4_get(context, "user_id", NULL); }
    /* team_id = event_team_id(...) or team_in_install_set(assistant_thread.team_id) */
    json_t *te_args = json_object();
    json_set(te_args, "event", json_copy(event ? event : json_null()));
    json_set(te_args, "body", json_copy(body ? body : json_null()));
    json_t *cl = json_obj_get(ctx->state, "_team_clients");
    json_set(te_args, "install_set", json_copy(cl ? cl : json_null()));
    json_t *team = sa4_event_team_id(ctx, te_args);
    if (!sa4_truthy(team)) {
        json_free(team);
        json_t *atd = sa4_get(at, "team_id", NULL);
        json_t *d = sa4_str_or_empty(atd);
        team = sa4_team_in_install_set(ctx, d);
        json_free(d); json_free(atd);
    }
    json_t *ccid = sa4_context_channel_id(ctx, context);
    json_t *out = json_object();
    json_set(out, "channel_id", sa4_str_or_empty(cid));
    json_set(out, "thread_ts", sa4_str_or_empty(tts));
    json_set(out, "user_id", sa4_str_or_empty(uid));
    json_set(out, "team_id", sa4_str_or_empty(team));
    json_set(out, "context_channel_id", ccid);
    json_free(at); json_free(context); json_free(cid); json_free(tts); json_free(uid);
    /* team/te_args owned; cl is BORROWED from ctx->state (not freed). */
    json_free(team); json_free(te_args);
    return out;
}

/* PoP: sa4_event_declares_bot_sender @ plugins/platforms/slack/adapter.py:_event_declares_bot_sender */
/* instance: True when the Slack event itself identifies a bot sender. */
static json_t *sa4_event_declares_bot_sender(sa4_ctx_t *ctx, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *bid = sa4_get(event, "bot_id", NULL);
    json_t *bp = sa4_get(event, "bot_profile", NULL);
    json_t *sub = sa4_get(event, "subtype", NULL);
    bool bot = sa4_truthy(bid) || sa4_truthy(bp) || (sa4_str(sub) && strcmp(sa4_str(sub), "bot_message") == 0);
    json_free(bid); json_free(bp); json_free(sub);
    if (bot) return json_bool(true);
    json_t *prof = sa4_get(event, "user_profile", NULL);
    if (sa4_plain_dict(prof)) {
        json_t *ib = sa4_get(prof, "is_bot", NULL);
        if (sa4_truthy(ib)) bot = true;
        json_free(ib);
    }
    json_free(prof);
    if (bot) return json_bool(true);
    json_t *app = sa4_get(event, "app_id", NULL);
    json_t *cmid = sa4_get(event, "client_msg_id", NULL);
    if (sa4_truthy(app) && !sa4_truthy(cmid)) {
        json_t *a = json_object();
        json_t *r = sa4_call(ctx, "slack_api_human_users", a);
        if (sa4_is_raise(r)) { json_free(app); json_free(cmid); return r; }
        json_t *user = sa4_get(event, "user", NULL);
        const char *u = sa4_str(user);
        bool in = false;
        if (r->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(r); i++) {
                const char *m = sa4_str(json_get(r, i));
                if (u && m && strcmp(u, m) == 0) { in = true; break; }
            }
        }
        json_free(user); json_free(app); json_free(cmid); json_free(r);
        return json_bool(!in);
    }
    json_free(app); json_free(cmid);
    return json_bool(false);
}

/* PoP: sa4_resolve_thread_ts @ plugins/platforms/slack/adapter.py:_resolve_thread_ts */
/* instance: thread_ts for an API call (metadata over reply_to, flat when disabled). */
static json_t *sa4_resolve_thread_ts(sa4_ctx_t *ctx, const json_t *args) {
    json_t *reply = json_obj_get(args, "reply_to");
    json_t *meta = json_obj_get(args, "metadata");
    json_t *extra = json_obj_get(ctx->state, "extra");
    json_t *rit = sa4_get(extra, "reply_in_thread", json_bool(true));
    bool in_thread = sa4_truthy(rit);
    json_free(rit);
    if (!in_thread) {
        json_t *md = sa4_truthy(meta) ? meta : (json_t *)NULL;
        json_t *existing = md ? sa4_get(md, "thread_id", NULL) : NULL;
        if (!sa4_truthy(existing)) { json_free(existing); existing = md ? sa4_get(md, "thread_ts", NULL) : NULL; }
        if (sa4_truthy(existing) && sa4_truthy(reply)) {
            const char *e = sa4_str(existing), *rp = sa4_str(reply);
            if (e && rp && strcmp(e, rp) == 0) { json_free(existing); existing = json_null(); }
        }
        json_t *r = sa4_truthy(existing) ? existing : json_null();
        if (r != existing) json_free(existing);
        if (!sa4_truthy(r)) { json_free(r); r = json_null(); }
        return r;
    }
    if (sa4_plain_dict(meta)) {
        json_t *tid = sa4_get(meta, "thread_id", NULL);
        if (sa4_truthy(tid)) return tid;
        json_free(tid);
        json_t *tts = sa4_get(meta, "thread_ts", NULL);
        if (sa4_truthy(tts)) return tts;
        json_free(tts);
    }
    return json_copy(reply ? reply : json_null());
}

/* PoP: sa4_session_thread_ts @ plugins/platforms/slack/adapter.py:_session_thread_ts */
/* instance: thread_ts for session keying (DM / channel / reaction-handoff). */
static json_t *sa4_session_thread_ts(sa4_ctx_t *ctx, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *ts = json_obj_get(args, "ts");
    json_t *isdm = json_obj_get(args, "is_dm");
    json_t *am = json_obj_get(args, "assistant_meta");
    if (sa4_truthy(isdm)) {
        json_t *tts = sa4_get(event, "thread_ts", NULL);
        if (!sa4_truthy(tts)) { json_free(tts); tts = sa4_get(am, "thread_ts", NULL); }
        if (!sa4_truthy(tts)) {
            json_free(tts);
            if (sa4_extra_flag(ctx, "dm_top_level_threads_as_sessions", true)) return json_copy(ts);
            return json_null();
        }
        return tts;
    }
    json_t *nothread = sa4_get(event, "_hermes_no_thread_response", NULL);
    if (sa4_truthy(nothread)) {
        json_t *tts = sa4_get(event, "thread_ts", NULL);
        json_free(nothread);
        return sa4_truthy(tts) ? tts : json_null();
    }
    json_free(nothread);
    json_t *raw = sa4_get(event, "thread_ts", NULL);
    const char *rs = sa4_str(raw), *tss = sa4_str(ts);
    if (sa4_truthy(raw) && !(rs && tss && strcmp(rs, tss) == 0)) { json_free(raw); return json_copy(raw); }
    json_free(raw);
    json_t *extra = json_obj_get(ctx->state, "extra");
    json_t *rit = sa4_get(extra, "reply_in_thread", json_bool(true));
    bool in_thread = sa4_truthy(rit);
    json_free(rit);
    return in_thread ? json_copy(ts) : json_null();
}

/* PoP: sa4_normalize_changed_message @ plugins/platforms/slack/adapter.py:_normalize_changed_message */
/* instance: message_changed envelope -> plain message event (or None). */
static json_t *sa4_normalize_changed_message(sa4_ctx_t *ctx, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *updated = sa4_get(event, "message", NULL);
    if (!sa4_plain_dict(updated)) { json_free(updated); return json_null(); }
    json_t *utsv = sa4_get(updated, "ts", NULL);
    char *orig = sa4_strdup(sa4_str(utsv) ? sa4_str(utsv) : "");
    json_free(utsv);
    json_t *seen = json_obj_get(ctx->state, "_processed_message_ts");
    if (orig[0] && sa4_set_has_str(seen, orig)) { free(orig); json_free(updated); return json_null(); }
    char *edt = NULL;
    json_t *edited = sa4_get(updated, "edited", NULL);
    if (sa4_plain_dict(edited)) {
        json_t *ets = sa4_get(edited, "ts", NULL);
        edt = sa4_strdup(sa4_str(ets) ? sa4_str(ets) : "");
        json_free(ets);
    } else edt = sa4_strdup("");
    json_free(edited);
    json_t *oev = sa4_get(event, "ts", NULL);
    char *outer = sa4_strdup(sa4_str(oev) ? sa4_str(oev) : "");
    json_free(oev);
    json_t *evt = sa4_get(event, "event_ts", NULL);
    const char *evs = sa4_str(evt);
    char *changed = NULL;
    if (evs && evs[0]) changed = sa4_strdup(evs);
    else if (edt[0]) changed = sa4_strdup(edt);
    else if (!evs || !evs[0]) {
        if (strcmp(outer, orig) != 0) changed = sa4_strdup(outer);
        else if (orig[0]) { changed = (char *)malloc(strlen(orig) + 10); if (changed) snprintf(changed, strlen(orig) + 10, "%s:changed", orig); }
        else changed = sa4_strdup("");
    }
    json_free(evt); free(edt); free(outer); free(orig);
    json_t *out = json_copy(updated);
    static const char *KEYS[4] = {"channel", "channel_type", "team", "team_id"};
    for (int i = 0; i < 4; i++) {
        json_t *cv = json_obj_get(out, KEYS[i]);
        json_t *ev = sa4_get(event, KEYS[i], NULL);
        if (!sa4_truthy(cv) && sa4_truthy(ev)) json_set(out, KEYS[i], json_copy(ev));
        /* cv is BORROWED from out (and json_set already frees the old value) -- never free it. */
        json_free(ev);
    }
    if (changed && changed[0]) json_set(out, "_slack_changed_event_ts", json_string(changed));
    free(changed);
    json_free(updated);
    return out;
}

/* PoP: sa4_assistant_thread_title_enabled @ plugins/platforms/slack/adapter.py:_assistant_thread_title_enabled */
/* instance: bool gate on extra.assistant_thread_titles (default True). */
static json_t *sa4_assistant_thread_title_enabled(sa4_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *extra = json_obj_get(ctx->state, "extra");
    json_t *raw = sa4_get(extra, "assistant_thread_titles", json_bool(true));
    bool enabled;
    if (sa4_str(raw)) {
        char *lo = sa4_lower_strip(raw->str_val);
        static const char *OFF[4] = {"0", "false", "no", "off"};
        enabled = !sa4_in_set_str(OFF, 4, lo);
        free(lo);
    } else enabled = sa4_truthy(raw);
    json_free(raw);
    return json_bool(enabled);
}
