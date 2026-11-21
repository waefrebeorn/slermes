/*
 * discord_adapter_flow8.c -- FLOW port of a coherent remainder subset of
 * plugins/platforms/discord/adapter.py (hermes-agent): tracker dunder + cache
 * snapshot / creds accessors, voice-task cancellation + voice state probes,
 * slash auth gates, attachment/visibility helpers and websocket-abort.
 *
 * All 17 defs are GENUINELY UNCLAIMED by flow1/flow2/flow5/flow6/flow7
 * (parent-verified PoP scan, dotted-name-aware). Each carries a PoP annotation
 * at its definition below. No flow1/2/5/6/7 def is re-ported here.
 *
 * Decision sequences only: every live I/O / injected callable / claimed
 * sibling enters through ONE universal seam da8_call(ctx, name, args). The
 * harness wires it to a recording stub fed in.io; the oracle installs
 * recording stubs emitting the identical {"name","args","ret"} event from the
 * SAME scripts. Seam-name set is exactly the SEAM TABLE below and is
 * cross-checked by the oracle (set equality across C/oracle/fixtures).
 *
 * Self-contained: libjson only; the harness #includes this .c directly.
 * All symbols are static and da8_-prefixed.
 *
 * Seam name table (name -> live call):
 *   task.cancel                     bot/voice timeout task .cancel()
 *   vc.is_connected                 voice client .is_connected()
 *   receiver.discard_pending        voice receiver .discard_pending()
 *   _command_sync_state_path        self._command_sync_state_path()
 *   atomic_json_write               atomic_json_write(path, state, ...)
 *   _update_command_sync_entry      self._update_command_sync_entry(...)
 *   _discord_thread_require_mention  self._discord_thread_require_mention()
 *   current_gates                   self.current_gates() -> (users, roles)
 *   _component_check_auth           _component_check_auth(interaction, ...)
 *   abort                           transport.abort()
 *   _evaluate_slash_authorization    self._evaluate_slash_authorization(interaction)
 *   _reject_slash                    (await) self._reject_slash(...)
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow8.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - Exception = {"__raise__": cls, "message": str(exc)}; full builtin hierarchy
 *   is modeled so BaseException (SystemExit/KeyboardInterrupt/CancelledError)
 *   passes through every except site. Missing/exhausted script = fail-closed
 *   RuntimeError on BOTH sides.
 * - Attribute doubles (websocket/socket/response/conn/protocol/writer/transport,
 *   task/vc/receiver/message.channel/interaction) are JSON objects materialized
 *   as SimpleNamespace; ANY object double is ALWAYS truthy. getattr-default
 *   reads by KEY PRESENCE; a truthy slot at a callable key fires that seam.
 * - self state attrs (_ids, _max_tracked, _creds, _voice_timeout_tasks,
 *   _voice_mixers, _voice_clients, _voice_receivers, children, _threads,
 *   allowed_user_ids, allowed_role_ids, live_auth, current_gates_callable)
 *   come from state with the defaults the oracle uses.
 * - str(None)=="None" for pystr_full; int() parses ASCII digits with sign/ws.
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

static char *da8_strdup(const char *s); /* forward decl (da8_exc_str uses it) */

/* -- universal seam context -- */
typedef struct da8_ctx {
    json_t *state;      /* owned; input state copy, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da8_ctx_t;

/* -- basic helpers -- */
static bool da8_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da8_raise_kind(const json_t *v) {
    return da8_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da8_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool da8_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
/* OBJECT attribute doubles are always truthy (SimpleNamespace semantics). */
static bool da8_attr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_OBJECT) return true;
    return da8_truthy(v);
}
static const char *da8_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool da8_str_eq(const json_t *v, const char *s) {
    const char *t = da8_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *da8_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* -- full builtin exception hierarchy (parent map) -- */
typedef struct { const char *k; const char *p; } da8_exc_row_t;
static const da8_exc_row_t DA8_EXC_PARENTS[] = {
    {"BaseException", NULL},
    {"Exception", "BaseException"},
    {"GeneratorExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BufferError", "Exception"},
    {"EOFError", "Exception"}, {"ExceptionGroup", "Exception"},
    {"ImportError", "Exception"}, {"LookupError", "Exception"},
    {"MemoryError", "Exception"}, {"NameError", "Exception"},
    {"OSError", "Exception"}, {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"}, {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"}, {"SyntaxError", "Exception"},
    {"SystemError", "Exception"}, {"TypeError", "Exception"},
    {"ValueError", "Exception"}, {"Warning", "Exception"},
    {"EnvironmentError", "OSError"}, {"IOError", "OSError"},
    {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"BrokenPipeError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
    {"BytesWarning", "Warning"}, {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"}, {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"}, {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"}, {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"CancelledError", "BaseException"},
    {"asyncio.CancelledError", "BaseException"},
};
static const char *da8_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(DA8_EXC_PARENTS) / sizeof(DA8_EXC_PARENTS[0]); i++)
        if (strcmp(k, DA8_EXC_PARENTS[i].k) == 0) return DA8_EXC_PARENTS[i].p;
    return NULL;
}
/* True when kind extends target (identity included). Unknown kinds extend
 * Exception, matching a dynamically-created type(kind,(Exception,)). */
static bool da8_exc_extends(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int guard = 0; guard < 32 && cur; guard++) {
        if (strcmp(cur, target) == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false; /* known root, not target */
        const char *p = da8_exc_parent(cur);
        if (!p) {
            /* unknown leaf behaves as an Exception subclass */
            return strcmp(target, "Exception") == 0 || strcmp(target, "BaseException") == 0;
        }
        cur = p;
    }
    return false;
}
static bool da8_is_base_exc(const char *kind) {
    if (!kind) return false;
    if (strcmp(kind, "BaseException") == 0) return true;
    return da8_exc_extends(kind, "BaseException") && !da8_exc_extends(kind, "Exception");
}
static bool da8_is_value_or_type(const char *kind) {
    return da8_exc_extends(kind, "ValueError") || da8_exc_extends(kind, "TypeError");
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc/proj_exc. */
static json_t *da8_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && da8_exc_extends(type, "OSError")) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = da8_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return da8_err(type, msg);
}
/* str(exc) from a raise descriptor is already the projected message. */
static char *da8_exc_str(const json_t *exc) {
    return da8_strdup(json_get_str(exc, "message", ""));
}

/* -- the one universal seam (mirrors oracle Ctx.call exactly) -- */
static json_t *da8_call(da8_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da8_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da8_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = da8_make_exc(raise);
            else {
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

/* -- string helpers -- */
static char *da8_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static json_t *da8_snode(const char *s) { return json_string(s ? s : ""); }
static unsigned int da8_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0) return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
         | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}
static size_t da8_cp_len(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xe0) == 0xc0) return 2;
    if ((p[0] & 0xf0) == 0xe0) return 3;
    return 4;
}
static bool da8_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f)) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
/* str.strip(): full Unicode White_Space, both ends. Caller frees. */
static char *da8_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    while (*p && da8_is_ws_cp(da8_decode_cp(p))) p += da8_cp_len(p);
    const unsigned char *end = p + strlen((const char *)p);
    while (end > p) {
        const unsigned char *q = end - 1;
        while (q > p && (*q & 0xc0) == 0x80) q--;
        if (!da8_is_ws_cp(da8_decode_cp(q))) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}
/* str.lstrip(chars): strip leading single-byte chars present in `chars`. */
static char *da8_lstrip_chars(const char *s, const char *chars) {
    if (!s) s = "";
    const char *p = s;
    while (*p && strchr(chars, *p)) p++;
    return da8_strdup(p);
}
static char *da8_lower(const char *s) {
    char *o = da8_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da8_first_line(const char *s) {
    const char *nl = strchr(s ? s : "", '\n');
    size_t n = nl ? (size_t)(nl - s) : strlen(s ? s : "");
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}
static size_t da8_cp_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) { n++; p += da8_cp_len(p); }
    return n;
}
static char *da8_prefix(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *start = p;
    size_t k = 0;
    while (*p && k < n) { p += da8_cp_len(p); k++; }
    size_t bytes = (size_t)(p - start);
    char *o = malloc(bytes + 1);
    if (!o) return NULL;
    memcpy(o, start, bytes);
    o[bytes] = '\0';
    return o;
}
static char *da8_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da8_strdup("");
    switch (v->type) {
        case JSON_STRING: return da8_strdup(v->str_val);
        case JSON_BOOL: return da8_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == floor(v->num_val) && isfinite(v->num_val))
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else snprintf(buf, sizeof buf, "%g", v->num_val);
            return da8_strdup(buf);
        }
        default: return da8_strdup("");
    }
}
static char *da8_strval(const json_t *v, const char *dflt) {
    if (da8_is_null(v) || !da8_truthy(v)) return da8_strdup(dflt ? dflt : "");
    return da8_pystr(v);
}
static json_t *da8_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static bool da8_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static json_t *da8_arg_get(const json_t *obj, const char *key) {
    json_t *v = da8_get(obj, key);
    return (v && v->type != JSON_NULL) ? json_copy(v) : json_null();
}
static bool da8_int(const json_t *v, long long *out) {
    if (!v) return false;
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return false;
        *out = (long long)v->num_val;
        return true;
    }
    if (v->type != JSON_STRING) return false;
    const char *s = v->str_val;
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    bool neg = false;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    long long acc = 0;
    int nd = 0;
    for (; *s >= '0' && *s <= '9'; s++) {
        acc = acc * 10 + (*s - '0');
        nd++;
    }
    if (!nd) return false;
    /* Python int() strips trailing whitespace too (e.g. int(' 42 ') == 42). */
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    if (*s != '\0') return false;
    *out = neg ? -acc : acc;
    return true;
}


/* -- extra helpers for flow6 -- */
/* str(None) == "None" (da8_pystr maps NULL to ""; Python str maps it to "None"). */
static char *da8_pystr_full(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da8_strdup("None");
    return da8_pystr(v);
}
/* str.isdigit(): non-empty, all ASCII digits (fixture corpus is ASCII). */
static bool da8_isdigit_str(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}
/* cast a JSON value to a number the way int()/float() does; *ok=false on failure. */
static json_t *da8_cast(const json_t *v, const char *how, bool *ok) {
    *ok = true;
    if (!how) how = "int";
    if (strcmp(how, "int") == 0) {
        long long n;
        if (!da8_int(v, &n)) { *ok = false; return NULL; }
        return json_number((double)n);
    }
    /* float */
    if (da8_is_null(v) || (v && v->type == JSON_BOOL)) { *ok = false; return NULL; }
    if (v && v->type == JSON_NUMBER) return json_copy(v);
    if (!v || v->type != JSON_STRING) { *ok = false; return NULL; }
    char *end = NULL;
    double d = strtod(v->str_val, &end);
    if (!end || *end != '\0') { *ok = false; return NULL; }
    return json_number(d);
}
static bool da8_truthy_set(const char *s) {
    return strcmp(s, "true") == 0 || strcmp(s, "1") == 0 ||
           strcmp(s, "yes") == 0 || strcmp(s, "on") == 0;
}

/* ===================== flow7 extra helpers ===================== */
/* getattr(obj, key, default): KEY PRESENCE (present-null counts as present). */
static json_t *da8_attr(const json_t *obj, const char *key, const json_t *dflt) {
    json_t *v = da8_get(obj, key);
    return v ? json_copy(v) : (dflt ? json_copy(dflt) : json_null());
}
/* direct attribute access that raises AttributeError when the key is absent */
static json_t *da8_need(const json_t *obj, const char *key) {
    json_t *v = da8_get(obj, key);
    if (!v) {
        char msg[256];
        snprintf(msg, sizeof msg, "'SimpleNamespace' object has no attribute '%s'", key);
        return da8_err("AttributeError", msg);
    }
    return json_copy(v);
}
static bool da8_startswith(const char *s, const char *pre) {
    if (!s) return false;
    size_t n = strlen(pre);
    return strncmp(s, pre, n) == 0;
}
/* canonical-set membership over a JSON array (compares str() forms) */
static bool da8_in_set(const json_t *arr, const json_t *v) {
    if (!arr || arr->type != JSON_ARRAY || da8_is_null(v)) return false;
    char *s = da8_pystr_full(v);
    bool hit = false;
    for (size_t i = 0; i < json_len(arr) && !hit; i++) {
        char *t = da8_pystr_full(json_get(arr, i));
        if (strcmp(s, t) == 0) hit = true;
        free(t);
    }
    free(s);
    return hit;
}
/* Python str() of the exception int() would raise for a bad raw value. */
static char *da8_int_err(const json_t *raw) {
    char buf[512];
    if (raw && raw->type == JSON_STRING)
        snprintf(buf, sizeof buf, "invalid literal for int() with base 10: '%s'", raw->str_val);
    else if (da8_is_null(raw))
        snprintf(buf, sizeof buf,
                 "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    else
        snprintf(buf, sizeof buf,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 (!raw) ? "NoneType" : "SimpleNamespace");
    return da8_strdup(buf);
}
/* truthiness of a callable slot: present and truthy */
static bool da8_callable_slot(const json_t *obj, const char *key) {
    json_t *v = da8_get(obj, key);
    return v && da8_truthy(v);
}

/* ============================ PORTED DEF 1: __contains__ ============================ */
/* PoP: da8_tracker_contains @ plugins/platforms/discord/adapter.py:_DiscordNonConversationalMessageTracker.__contains__ */
static json_t *da8_tracker_contains(da8_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *ids = da8_get(ctx->state, "_ids");
    const json_t *m = json_obj_get(args, "message_id");
    char *s = da8_truthy(m) ? da8_pystr_full(m) : da8_strdup("");
    bool hit = json_obj_get(ids, s) != NULL;
    free(s);
    return json_bool(hit);
}

/* ============================ PORTED DEF 2: _snapshot ============================ */
/* PoP: da8_tracker_snapshot @ plugins/platforms/discord/adapter.py:_DiscordNonConversationalMessageTracker._snapshot */
static json_t *da8_tracker_snapshot(da8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *ids_obj = da8_get(ctx->state, "_ids");
    size_t n = (ids_obj && ids_obj->type == JSON_OBJECT) ? ids_obj->c.count : 0;
    long long mx = 0;
    if (!da8_int(da8_get(ctx->state, "_max_tracked"), &mx)) mx = 0;
    long long start_ll = 0;
    bool trimmed = ((long long)n > mx);
    if (trimmed) {
        if (mx > 0) start_ll = (long long)n - mx;
        else if (mx == 0) start_ll = 0;
        else start_ll = -mx;               /* negative max -> -max slice */
        if (start_ll < 0) start_ll = 0;
        if (start_ll > (long long)n) start_ll = (long long)n;
    }
    size_t start = (size_t)start_ll;
    json_t *out = json_array();
    for (size_t i = start; i < n; i++)
        json_append(out, json_string(ids_obj->c.keys[i] ? ids_obj->c.keys[i] : ""));
    if (trimmed) {
        json_t *newids = json_object();
        for (size_t i = start; i < n; i++)
            json_set(newids, ids_obj->c.keys[i] ? ids_obj->c.keys[i] : "", json_null());
        json_set(ctx->state, "_ids", newids);
    }
    return out;
}

/* -- VoiceReceiver creds accessors (share one helper) -- */
static json_t *da8_creds_item(da8_ctx_t *ctx, size_t idx) {
    json_t *creds = da8_get(ctx->state, "_creds");
    size_t n = (creds && creds->type == JSON_ARRAY) ? json_len(creds) : 0;
    if (idx >= n) return da8_err("IndexError", "list index out of range");
    return json_copy(json_get(creds, idx));
}
/* PoP: da8_secret_key @ plugins/platforms/discord/adapter.py:VoiceReceiver._secret_key */
static json_t *da8_secret_key(da8_ctx_t *ctx, const json_t *args) { (void)args; return da8_creds_item(ctx, 0); }
/* PoP: da8_dave_session @ plugins/platforms/discord/adapter.py:VoiceReceiver._dave_session */
static json_t *da8_dave_session(da8_ctx_t *ctx, const json_t *args) { (void)args; return da8_creds_item(ctx, 1); }
/* PoP: da8_dave_protocol_version @ plugins/platforms/discord/adapter.py:VoiceReceiver._dave_protocol_version */
static json_t *da8_dave_protocol_version(da8_ctx_t *ctx, const json_t *args) { (void)args; return da8_creds_item(ctx, 2); }
/* PoP: da8_dave_downgraded @ plugins/platforms/discord/adapter.py:VoiceReceiver._dave_downgraded */
static json_t *da8_dave_downgraded(da8_ctx_t *ctx, const json_t *args) { (void)args; return da8_creds_item(ctx, 3); }

/* ============================ PORTED DEF 7: _cancel_voice_timeout ============================ */
/* PoP: da8_cancel_voice_timeout @ plugins/platforms/discord/adapter.py:DiscordAdapter._cancel_voice_timeout */
static json_t *da8_cancel_voice_timeout(da8_ctx_t *ctx, const json_t *args) {
    char *gid = da8_pystr_full(json_obj_get(args, "guild_id"));
    json_t *tasks = da8_get(ctx->state, "_voice_timeout_tasks");
    json_t *raw = da8_get(tasks, gid);
    json_t *task = raw ? json_copy(raw) : NULL;
    json_obj_del(tasks, gid);                 /* pop(guild_id, None) */
    if (da8_truthy(task)) {
        /* live code: `if task: task.cancel()` -- unconditional call, no callable check */
        json_t *r = da8_call(ctx, "task.cancel", json_object());
        if (da8_is_raise(r)) { free(gid); json_free(task); return r; }
        json_free(r);
    }
    free(gid);
    if (task) json_free(task);
    return json_null();
}

/* ============================ PORTED DEF 8: voice_mixer_active ============================ */
/* PoP: da8_voice_mixer_active @ plugins/platforms/discord/adapter.py:DiscordAdapter.voice_mixer_active */
static json_t *da8_voice_mixer_active(da8_ctx_t *ctx, const json_t *args) {
    json_t *mixers = da8_get(ctx->state, "_voice_mixers");
    if (!da8_truthy(mixers)) return json_bool(false);
    char *gid = da8_pystr_full(json_obj_get(args, "guild_id"));
    json_t *v = da8_get(mixers, gid);
    free(gid);
    return json_bool(v != NULL && !da8_is_null(v));
}

/* ============================ PORTED DEF 9: is_in_voice_channel ============================ */
/* PoP: da8_is_in_voice_channel @ plugins/platforms/discord/adapter.py:DiscordAdapter.is_in_voice_channel */
static json_t *da8_is_in_voice_channel(da8_ctx_t *ctx, const json_t *args) {
    char *gid = da8_pystr_full(json_obj_get(args, "guild_id"));
    json_t *vc = da8_get(da8_get(ctx->state, "_voice_clients"), gid);
    free(gid);
    if (!vc || da8_is_null(vc)) return json_bool(false);
    json_t *r = da8_call(ctx, "vc.is_connected", json_object());
    if (da8_is_raise(r)) return r;
    bool res = da8_truthy(r);
    json_free(r);
    return json_bool(res);
}

/* ============================ PORTED DEF 10: discard_pending_voice_input ============================ */
/* PoP: da8_discard_pending_voice_input @ plugins/platforms/discord/adapter.py:DiscordAdapter.discard_pending_voice_input */
static json_t *da8_discard_pending_voice_input(da8_ctx_t *ctx, const json_t *args) {
    char *gid = da8_pystr_full(json_obj_get(args, "guild_id"));
    json_t *rcv = da8_get(da8_get(ctx->state, "_voice_receivers"), gid);
    free(gid);
    if (!rcv || da8_is_null(rcv)) return json_null();
    json_t *r = da8_call(ctx, "receiver.discard_pending", json_object());
    if (da8_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* ============================ PORTED DEF 11: _disable_all ============================ */
/* PoP: da8_disable_all @ plugins/platforms/discord/adapter.py:_HermesView._disable_all */
static json_t *da8_disable_all(da8_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *children = da8_get(ctx->state, "children");
    if (children && children->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(children); i++) {
            json_t *c = json_get(children, i);
            if (c && c->type == JSON_OBJECT) json_set(c, "disabled", json_bool(true));
        }
    }
    return json_null();
}

/* ============================ PORTED DEF 12: _write_command_sync_state ============================ */
/* PoP: da8_write_command_sync_state @ plugins/platforms/discord/adapter.py:DiscordAdapter._write_command_sync_state */
static json_t *da8_write_command_sync_state(da8_ctx_t *ctx, const json_t *args) {
    json_t *path = da8_call(ctx, "_command_sync_state_path", json_object());
    if (da8_is_raise(path)) return path;
    json_t *a = json_object();
    json_set(a, "path", path);
    json_t *st = json_obj_get(args, "state");
    json_set(a, "state", st ? json_copy(st) : json_object());
    json_set(a, "indent", json_null());
    json_t *sep = json_array();
    json_append(sep, json_string(","));
    json_append(sep, json_string(":"));
    json_set(a, "separators", sep);
    json_t *r = da8_call(ctx, "atomic_json_write", a);
    if (da8_is_raise(r)) return r;   /* no return in live body: only exceptions escape */
    json_free(r);
    return json_null();
}

/* ============================ PORTED DEF 13: _record_command_sync_attempt ============================ */
/* PoP: da8_record_command_sync_attempt @ plugins/platforms/discord/adapter.py:DiscordAdapter._record_command_sync_attempt */
static json_t *da8_record_command_sync_attempt(da8_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_t *app = json_obj_get(args, "app_id");
    json_set(a, "app_id", app ? json_copy(app) : json_null());
    json_set_hdr(a, "fingerprint", json_get_str(args, "fingerprint", ""));
    json_set(a, "keep_existing", json_bool(true));
    json_t *drop = json_array();
    json_append(drop, json_string("last_success_at"));
    json_append(drop, json_string("summary"));
    json_set(a, "drop", drop);
    json_t *r = da8_call(ctx, "_update_command_sync_entry", a);
    if (da8_is_raise(r)) return r;   /* live body has no return: only exceptions escape */
    json_free(r);
    return json_null();
}

/* ============================ PORTED DEF 14: _in_bot_thread ============================ */
/* PoP: da8_in_bot_thread @ plugins/platforms/discord/adapter.py:DiscordAdapter._in_bot_thread */
static json_t *da8_in_bot_thread(da8_ctx_t *ctx, const json_t *args) {
    json_t *channel = da8_get(json_obj_get(args, "message"), "channel");
    if (!da8_truthy(da8_get(channel, "__thread__"))) return json_bool(false);
    char *s = da8_pystr_full(da8_get(channel, "id"));
    json_t *threads = da8_get(ctx->state, "_threads");
    bool inthr = false;
    if (threads && threads->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(threads) && !inthr; i++) {
            char *t = da8_pystr_full(json_get(threads, i));
            if (strcmp(s, t) == 0) inthr = true;
            free(t);
        }
    }
    free(s);
    if (!inthr) return json_bool(false);
    json_t *r = da8_call(ctx, "_discord_thread_require_mention", json_object());
    if (da8_is_raise(r)) return r;
    bool need = da8_truthy(r);
    json_free(r);
    return json_bool(!need);
}

/* ============================ PORTED DEF 15: _check_auth ============================ */
/* PoP: da8_check_auth @ plugins/platforms/discord/adapter.py:_HermesView._check_auth */
static json_t *da8_check_auth(da8_ctx_t *ctx, const json_t *args) {
    json_t *interaction = json_obj_get(args, "interaction");
    json_t *users, *roles;
    if (da8_truthy(da8_get(ctx->state, "current_gates_callable"))) {
        json_t *r = da8_call(ctx, "current_gates", json_object());
        if (da8_is_raise(r)) return r;
        users = (r && r->type == JSON_ARRAY && json_len(r) > 0) ? json_copy(json_get(r, 0)) : json_null();
        roles = (r && r->type == JSON_ARRAY && json_len(r) > 1) ? json_copy(json_get(r, 1)) : json_null();
        json_free(r);
    } else {
        json_t *u = da8_get(ctx->state, "allowed_user_ids");
        json_t *ro = da8_get(ctx->state, "allowed_role_ids");
        users = u ? json_copy(u) : json_array();
        roles = ro ? json_copy(ro) : json_array();
    }
    json_t *a = json_object();
    json_set(a, "interaction", interaction ? json_copy(interaction) : json_null());
    json_set(a, "users", users);
    json_set(a, "roles", roles);
    json_t *la = da8_get(ctx->state, "live_auth");
    json_set(a, "live_auth", la ? json_copy(la) : json_null());
    return da8_call(ctx, "_component_check_auth", a);
}

/* ============================ PORTED DEF 16: _abort_discord_websocket_transport ============================ */
/* PoP: da8_abort_discord_websocket_transport @ plugins/platforms/discord/adapter.py:_abort_discord_websocket_transport */
static json_t *da8_abort_discord_websocket_transport(da8_ctx_t *ctx, const json_t *args) {
    json_t *websocket = json_obj_get(args, "websocket");
    json_t *socket = da8_attr(websocket, "socket", NULL);
    json_t *response = da8_attr(socket, "_response", NULL);
    json_t *connection = da8_attr(socket, "_conn", NULL);
    if (da8_is_null(connection)) connection = da8_attr(response, "connection", NULL);
    json_t *protocol = da8_attr(connection, "protocol", NULL);
    json_t *writer = da8_attr(socket, "_writer", NULL);
    json_t *transport = da8_attr(writer, "transport", NULL);
    if (da8_is_null(transport)) transport = da8_attr(protocol, "transport", NULL);
    if (!da8_callable_slot(transport, "abort")) return json_bool(false);
    json_t *r = da8_call(ctx, "abort", json_object());   /* no try/except: propagates */
    if (da8_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* ============================ PORTED DEF 17: _check_slash_authorization ============================ */
/* PoP: da8_check_slash_authorization @ plugins/platforms/discord/adapter.py:DiscordAdapter._check_slash_authorization */
static json_t *da8_check_slash_authorization(da8_ctx_t *ctx, const json_t *args) {
    json_t *interaction = json_obj_get(args, "interaction");
    json_t *ev = da8_call(ctx, "_evaluate_slash_authorization", json_object());
    if (da8_is_raise(ev)) return ev;
    bool allowed = (ev && ev->type == JSON_ARRAY && json_len(ev) > 0) ? da8_truthy(json_get(ev, 0)) : false;
    json_t *reason = (ev && ev->type == JSON_ARRAY && json_len(ev) > 1) ? json_copy(json_get(ev, 1)) : json_null();
    json_free(ev);
    if (allowed) { json_free(reason); return json_bool(true); }
    json_t *a = json_object();
    json_set(a, "interaction", interaction ? json_copy(interaction) : json_null());
    json_set_hdr(a, "command_text", json_get_str(args, "command_text", ""));
    if (da8_truthy(reason)) json_set(a, "reason", reason);
    else { json_free(reason); json_set_hdr(a, "reason", "unauthorized"); }
    return da8_call(ctx, "_reject_slash", a);
}
