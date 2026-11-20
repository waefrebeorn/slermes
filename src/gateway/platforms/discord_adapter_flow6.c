/*
 * discord_adapter_flow6.c -- FLOW port of a coherent remainder subset of
 * plugins/platforms/discord/adapter.py (hermes-agent): localization / approval
 * notification formatting helpers, config/env gate readers, channel/user
 * allowlist classifiers, and the missed-message-backfill numeric readers.
 *
 * All 21 defs are GENUINELY UNCLAIMED by flow1/flow2/flow5 (parent-verified
 * PoP scan). Every one carries a PoP annotation at its definition below.
 * No flow1/2/5 def is re-ported here.
 *
 * Decision sequences only: every live I/O / injected callable / claimed
 * sibling / module helper enters through ONE universal seam
 * da6_call(ctx, name, args). The harness wires it to a recording stub fed
 * in.io; the oracle installs recording stubs emitting the identical
 * {"name","args","ret"} event from the SAME scripts. No inlined helper uses a
 * different oracle seam name: the seam-name set is exactly the SEAM TABLE
 * below and is cross-checked by the oracle (set equality across C/oracle/
 * fixtures).
 *
 * Self-contained: libjson only; the harness #includes this .c directly.
 * All symbols are static and da6_-prefixed.
 *
 * Seam name table (name -> live call):
 *   t                              t(key, **kwargs)
 *   _truncate_discord_component_text _truncate_discord_component_text(text, limit)
 *   _t_discord                     _t_discord(key, limit)
 *   _text                          _text(key)
 *   unauthorized_action_notice     unauthorized_action_notice(Platform.DISCORD)
 *   logger.warning                 logger.warning(msg)
 *   _scoped_gate_env               _scoped_gate_env(name, default)
 *   _get_allowed_channels          self._get_allowed_channels()
 *   PairingStore.is_approved       PairingStore().is_approved("discord", uid)
 *   _is_forum_parent               self._is_forum_parent(parent)
 *   _extra_or_env_flag             self._extra_or_env_flag(key, env, default, truthy)
 *   _missed_message_backfill_number self._missed_message_backfill_number(...)
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow6.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)} (OSError adds
 *   errno, message "[Errno N] msg"). Full builtin hierarchy is modeled so
 *   except (ValueError, TypeError) and except Exception match live exactly;
 *   SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/BaseException
 *   pass through every except-Exception site.
 * - Attribute doubles (channel/parent): JSON objects materialized as
 *   SimpleNamespace; an OBJECT double is ALWAYS truthy (even empty); scalars
 *   keep Python truthiness. getattr-with-default reads by KEY PRESENCE
 *   (present-null counts as present, value None).
 * - str(None) == "None"; str() positions carry strings / integral numbers /
 *   bools / None only (no container repr). int() parses ASCII digits with
 *   optional sign and surrounding whitespace. str.strip() folds the full
 *   Unicode White_Space set; str.lower() folds ASCII (fixture corpus).
 * - Set returns are represented as JSON arrays; set intersection compares
 *   canonical str() forms. config_extra and _allowed_user_ids are read from
 *   the echoed state; no ported function mutates state.
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

static char *da6_strdup(const char *s); /* forward decl (da6_exc_str uses it) */

/* -- universal seam context -- */
typedef struct da6_ctx {
    json_t *state;      /* owned; input state copy, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da6_ctx_t;

/* -- basic helpers -- */
static bool da6_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da6_raise_kind(const json_t *v) {
    return da6_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da6_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool da6_truthy(const json_t *v) {
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
static bool da6_attr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_OBJECT) return true;
    return da6_truthy(v);
}
static const char *da6_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool da6_str_eq(const json_t *v, const char *s) {
    const char *t = da6_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *da6_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* -- full builtin exception hierarchy (parent map) -- */
typedef struct { const char *k; const char *p; } da6_exc_row_t;
static const da6_exc_row_t DA6_EXC_PARENTS[] = {
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
static const char *da6_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(DA6_EXC_PARENTS) / sizeof(DA6_EXC_PARENTS[0]); i++)
        if (strcmp(k, DA6_EXC_PARENTS[i].k) == 0) return DA6_EXC_PARENTS[i].p;
    return NULL;
}
/* True when kind extends target (identity included). Unknown kinds extend
 * Exception, matching a dynamically-created type(kind,(Exception,)). */
static bool da6_exc_extends(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int guard = 0; guard < 32 && cur; guard++) {
        if (strcmp(cur, target) == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false; /* known root, not target */
        const char *p = da6_exc_parent(cur);
        if (!p) {
            /* unknown leaf behaves as an Exception subclass */
            return strcmp(target, "Exception") == 0 || strcmp(target, "BaseException") == 0;
        }
        cur = p;
    }
    return false;
}
static bool da6_is_base_exc(const char *kind) {
    if (!kind) return false;
    if (strcmp(kind, "BaseException") == 0) return true;
    return da6_exc_extends(kind, "BaseException") && !da6_exc_extends(kind, "Exception");
}
static bool da6_is_value_or_type(const char *kind) {
    return da6_exc_extends(kind, "ValueError") || da6_exc_extends(kind, "TypeError");
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc/proj_exc. */
static json_t *da6_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && da6_exc_extends(type, "OSError")) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = da6_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return da6_err(type, msg);
}
/* str(exc) from a raise descriptor is already the projected message. */
static char *da6_exc_str(const json_t *exc) {
    return da6_strdup(json_get_str(exc, "message", ""));
}

/* -- the one universal seam (mirrors oracle Ctx.call exactly) -- */
static json_t *da6_call(da6_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da6_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da6_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = da6_make_exc(raise);
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
static char *da6_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static json_t *da6_snode(const char *s) { return json_string(s ? s : ""); }
static unsigned int da6_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0) return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
         | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}
static size_t da6_cp_len(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xe0) == 0xc0) return 2;
    if ((p[0] & 0xf0) == 0xe0) return 3;
    return 4;
}
static bool da6_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f)) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
/* str.strip(): full Unicode White_Space, both ends. Caller frees. */
static char *da6_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    while (*p && da6_is_ws_cp(da6_decode_cp(p))) p += da6_cp_len(p);
    const unsigned char *end = p + strlen((const char *)p);
    while (end > p) {
        const unsigned char *q = end - 1;
        while (q > p && (*q & 0xc0) == 0x80) q--;
        if (!da6_is_ws_cp(da6_decode_cp(q))) break;
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
static char *da6_lstrip_chars(const char *s, const char *chars) {
    if (!s) s = "";
    const char *p = s;
    while (*p && strchr(chars, *p)) p++;
    return da6_strdup(p);
}
static char *da6_lower(const char *s) {
    char *o = da6_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da6_first_line(const char *s) {
    const char *nl = strchr(s ? s : "", '\n');
    size_t n = nl ? (size_t)(nl - s) : strlen(s ? s : "");
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}
static size_t da6_cp_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) { n++; p += da6_cp_len(p); }
    return n;
}
static char *da6_prefix(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *start = p;
    size_t k = 0;
    while (*p && k < n) { p += da6_cp_len(p); k++; }
    size_t bytes = (size_t)(p - start);
    char *o = malloc(bytes + 1);
    if (!o) return NULL;
    memcpy(o, start, bytes);
    o[bytes] = '\0';
    return o;
}
static char *da6_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da6_strdup("");
    switch (v->type) {
        case JSON_STRING: return da6_strdup(v->str_val);
        case JSON_BOOL: return da6_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == floor(v->num_val) && isfinite(v->num_val))
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else snprintf(buf, sizeof buf, "%g", v->num_val);
            return da6_strdup(buf);
        }
        default: return da6_strdup("");
    }
}
static char *da6_strval(const json_t *v, const char *dflt) {
    if (da6_is_null(v) || !da6_truthy(v)) return da6_strdup(dflt ? dflt : "");
    return da6_pystr(v);
}
static json_t *da6_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static bool da6_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static json_t *da6_arg_get(const json_t *obj, const char *key) {
    json_t *v = da6_get(obj, key);
    return (v && v->type != JSON_NULL) ? json_copy(v) : json_null();
}
static bool da6_int(const json_t *v, long long *out) {
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
/* str(None) == "None" (da6_pystr maps NULL to ""; Python str maps it to "None"). */
static char *da6_pystr_full(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da6_strdup("None");
    return da6_pystr(v);
}
/* str.isdigit(): non-empty, all ASCII digits (fixture corpus is ASCII). */
static bool da6_isdigit_str(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}
/* cast a JSON value to a number the way int()/float() does; *ok=false on failure. */
static json_t *da6_cast(const json_t *v, const char *how, bool *ok) {
    *ok = true;
    if (!how) how = "int";
    if (strcmp(how, "int") == 0) {
        long long n;
        if (!da6_int(v, &n)) { *ok = false; return NULL; }
        return json_number((double)n);
    }
    /* float */
    if (da6_is_null(v) || (v && v->type == JSON_BOOL)) { *ok = false; return NULL; }
    if (v && v->type == JSON_NUMBER) return json_copy(v);
    if (!v || v->type != JSON_STRING) { *ok = false; return NULL; }
    char *end = NULL;
    double d = strtod(v->str_val, &end);
    if (!end || *end != '\0') { *ok = false; return NULL; }
    return json_number(d);
}
static bool da6_truthy_set(const char *s) {
    return strcmp(s, "true") == 0 || strcmp(s, "1") == 0 ||
           strcmp(s, "yes") == 0 || strcmp(s, "on") == 0;
}

/* ============================ PORTED DEF 1 ============================ */
/* PoP: da6_csv @ plugins/platforms/discord/adapter.py:_csv */
static json_t *da6_csv(da6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *value = json_obj_get(args, "value");
    if (value && value->type == JSON_ARRAY) {
        size_t n = json_len(value);
        char *out = malloc(1);
        if (!out) return json_null();
        out[0] = '\0';
        for (size_t i = 0; i < n; i++) {
            char *s = da6_pystr_full(json_get(value, i));
            size_t sl = strlen(s), ol = strlen(out);
            char *nb = realloc(out, ol + sl + 2);
            if (!nb) { free(out); free(s); return json_null(); }
            out = nb;
            if (i) { out[ol] = ','; out[ol + 1] = '\0'; ol++; }
            memcpy(out + ol, s, sl + 1);
            free(s);
        }
        json_t *r = da6_snode(out); free(out); return r;
    }
    char *s = da6_pystr_full(value);
    json_t *r = da6_snode(s); free(s); return r;
}

/* ============================ PORTED DEF 2 ============================ */
/* PoP: da6_t_discord @ plugins/platforms/discord/adapter.py:_t_discord */
static json_t *da6_t_discord(da6_ctx_t *ctx, const json_t *args) {
    json_t *limit = json_obj_get(args, "limit");
    json_t *kwv = json_obj_get(args, "kwargs");
    json_t *a = json_object();
    json_set_hdr(a, "key", json_get_str(args, "key", ""));
    json_set(a, "kwargs", (kwv && kwv->type == JSON_OBJECT) ? json_copy(kwv) : json_object());
    json_t *tr = da6_call(ctx, "t", a);
    if (da6_is_raise(tr)) return tr;
    json_t *b = json_object();
    json_set(b, "text", da6_snode(da6_str(tr) ? da6_str(tr) : ""));
    json_set(b, "limit", limit ? json_copy(limit) : json_number(0.0));
    return da6_call(ctx, "_truncate_discord_component_text", b);
}

/* ============================ PORTED DEF 3 ============================ */
/* PoP: da6_text @ plugins/platforms/discord/adapter.py:_native_slash_commands.<locals>._text */
static json_t *da6_text(da6_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set_hdr(a, "key", json_get_str(args, "key", ""));
    json_set(a, "limit", json_number(100.0));
    return da6_call(ctx, "_t_discord", a);
}

/* ============================ PORTED DEF 4 ============================ */
/* PoP: da6_choice_label @ plugins/platforms/discord/adapter.py:_native_slash_commands.<locals>._choice_label */
static json_t *da6_choice_label(da6_ctx_t *ctx, const json_t *args) {
    const char *label = json_get_str(args, "label", "");
    if (strchr(label, '.')) {
        json_t *a = json_object();
        json_set_hdr(a, "key", label);
        return da6_call(ctx, "_text", a);
    }
    return da6_snode(label);
}

/* ============================ PORTED DEF 5 ============================ */
/* PoP: da6_default_voice_ack_phrases @ plugins/platforms/discord/adapter.py:_default_voice_ack_phrases */
static json_t *da6_default_voice_ack_phrases(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *arr = json_array();
    for (int i = 1; i <= 5; i++) {
        char key[80];
        snprintf(key, sizeof key, "platform.discord.voice.ack_%d", i);
        json_t *a = json_object();
        json_set_hdr(a, "key", key);
        json_set(a, "kwargs", json_object());
        json_t *r = da6_call(ctx, "t", a);
        if (da6_is_raise(r)) { json_free(arr); return r; }
        json_append(arr, r);
    }
    return arr;
}

/* ============================ PORTED DEF 6 ============================ */
/* PoP: da6_unauthorized @ plugins/platforms/discord/adapter.py:_unauthorized */
static json_t *da6_unauthorized(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "platform", "discord");
    return da6_call(ctx, "unauthorized_action_notice", a);
}

/* ============================ PORTED DEF 7 ============================ */
/* PoP: da6_warn_missing_media @ plugins/platforms/discord/adapter.py:_standalone_warn_missing_media */
static json_t *da6_warn_missing_media(da6_ctx_t *ctx, const json_t *args) {
    const char *mp = json_get_str(args, "media_path", "");
    size_t n = strlen(mp) + 48;
    char *w = malloc(n + 1);
    if (!w) return json_null();
    snprintf(w, n + 1, "Media file not found, skipping: %s", mp);
    json_t *res = da6_snode(w);
    json_t *a = json_object();
    json_set_hdr(a, "fmt", w);
    json_set(a, "args", json_array());
    free(w);
    json_t *r = da6_call(ctx, "logger.warning", a);
    if (da6_is_raise(r)) { json_free(res); return r; }
    return res;
}

/* ============================ PORTED DEF 8 ============================ */
/* PoP: da6_env_bool @ plugins/platforms/discord/adapter.py:_env_bool */
static json_t *da6_env_bool(da6_ctx_t *ctx, const json_t *args) {
    json_t *dv = json_obj_get(args, "default");
    json_t *a = json_object();
    json_set_hdr(a, "name", json_get_str(args, "name", ""));
    /* _env_bool calls _scoped_gate_env(name) with NO default argument. */
    json_set(a, "default", json_null());
    json_t *r = da6_call(ctx, "_scoped_gate_env", a);
    if (da6_is_raise(r)) return r;
    if (!da6_str(r)) return da6_err("AttributeError", "'NoneType' object has no attribute 'lower'");
    char *low = da6_lower(da6_str(r));
    if (low[0] == '\0') { free(low); return dv ? json_copy(dv) : json_bool(false); }
    bool hit = da6_truthy_set(low);
    free(low);
    return json_bool(hit);
}

/* ============================ PORTED DEF 9 ============================ */
/* PoP: da6_ea_reason_label @ plugins/platforms/discord/adapter.py:DiscordAdapter._EA_REASON_LABEL */
static json_t *da6_ea_reason_label(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "gateway.exec_approval.reason_label");
    json_set(a, "kwargs", json_object());
    json_t *r = da6_call(ctx, "t", a);
    if (da6_is_raise(r)) return r;
    char buf[2048];
    snprintf(buf, sizeof buf, "**%s:** ", da6_str(r) ? da6_str(r) : "");
    return da6_snode(buf);
}

/* ============================ PORTED DEF 10 ============================ */
/* PoP: da6_ea_header @ plugins/platforms/discord/adapter.py:DiscordAdapter._EA_HEADER */
static json_t *da6_ea_header(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *keys[3] = {
        "gateway.exec_approval.header",
        "platform.discord.approval.question",
        "platform.discord.approval.requested_command_label",
    };
    char *v[3];
    for (int i = 0; i < 3; i++) {
        json_t *a = json_object();
        json_set_hdr(a, "key", keys[i]);
        json_set(a, "kwargs", json_object());
        json_t *r = da6_call(ctx, "t", a);
        if (da6_is_raise(r)) {
            for (int j = 0; j < i; j++) free(v[j]);
            return r;
        }
        v[i] = da6_strdup(da6_str(r) ? da6_str(r) : "");
    }
    size_t cap = strlen(v[0]) + strlen(v[1]) + strlen(v[2]) + 128;
    char *out = malloc(cap + 1);
    snprintf(out, cap + 1,
             "\xE2\x9A\xA0\xEF\xB8\x8F **%s**\n\n%s\n\n**%s**\n", v[0], v[1], v[2]);
    for (int i = 0; i < 3; i++) free(v[i]);
    json_t *res = da6_snode(out); free(out); return res;
}

/* ============================ PORTED DEF 11 ============================ */
/* PoP: da6_ea_smart_deny_line @ plugins/platforms/discord/adapter.py:DiscordAdapter._EA_SMART_DENY_LINE */
static json_t *da6_ea_smart_deny_line(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "gateway.exec_approval.smart_deny_line");
    json_set(a, "kwargs", json_object());
    json_t *r = da6_call(ctx, "t", a);
    if (da6_is_raise(r)) return r;
    const char *line = da6_str(r) ? da6_str(r) : "";
    const char *colon = strchr(line, ':');
    size_t cap = strlen(line) + 128;
    char *out = malloc(cap + 1);
    if (colon) {
        size_t ll = (size_t)(colon - line);
        char *label = malloc(ll + 1);
        memcpy(label, line, ll); label[ll] = '\0';
        snprintf(out, cap + 1, "\n\n**%s:**%s", label, colon + 1);
        free(label);
    } else {
        snprintf(out, cap + 1, "\n\n**%s**", line);
    }
    json_t *res = da6_snode(out); free(out); return res;
}

/* ============================ PORTED DEF 12 ============================ */
/* PoP: da6_discord_channel_ids_allowed @ plugins/platforms/discord/adapter.py:DiscordAdapter._discord_channel_ids_allowed */
static json_t *da6_discord_channel_ids_allowed(da6_ctx_t *ctx, const json_t *args) {
    json_t *ids = json_obj_get(args, "channel_ids");
    size_t nids = (ids && ids->type == JSON_ARRAY) ? json_len(ids) : 0;
    if (nids == 0) return json_bool(false);
    json_t *r = da6_call(ctx, "_get_allowed_channels", json_object());
    if (da6_is_raise(r)) return r;
    size_t na = (r && r->type == JSON_ARRAY) ? json_len(r) : 0;
    if (na == 0) { json_free(r); return json_bool(false); }
    for (size_t j = 0; j < na; j++) {
        const char *y = da6_str(json_get(r, j));
        if (y && strcmp(y, "*") == 0) { json_free(r); return json_bool(true); }
    }
    bool hit = false;
    for (size_t i = 0; i < nids && !hit; i++) {
        char *x = da6_pystr_full(json_get(ids, i));
        for (size_t j = 0; j < na; j++) {
            char *y = da6_pystr_full(json_get(r, j));
            if (strcmp(x, y) == 0) hit = true;
            free(y);
            if (hit) break;
        }
        free(x);
    }
    json_free(r);
    return json_bool(hit);
}

/* ============================ PORTED DEF 13 ============================ */
/* PoP: da6_is_pairing_approved_user @ plugins/platforms/discord/adapter.py:DiscordAdapter._is_pairing_approved_user */
static json_t *da6_is_pairing_approved_user(da6_ctx_t *ctx, const json_t *args) {
    json_t *uid = json_obj_get(args, "user_id");
    char *base = da6_truthy(uid) ? da6_pystr_full(uid) : da6_strdup("");
    char *s = da6_strip(base);
    free(base);
    if (s[0] == '\0') { free(s); return json_bool(false); }
    json_t *a = json_object();
    json_set_hdr(a, "platform", "discord");
    json_set_hdr(a, "user_id", s);
    free(s);
    json_t *r = da6_call(ctx, "PairingStore.is_approved", a);
    if (da6_is_raise(r)) {
        const char *k = da6_raise_kind(r);
        if (da6_is_base_exc(k)) return r;   /* BaseException passes through */
        json_free(r);
        return json_bool(false);           /* except Exception: return False */
    }
    bool yes = da6_attr_truthy(r);
    json_free(r);
    return json_bool(yes);
}

/* ============================ PORTED DEF 14 ============================ */
/* PoP: da6_missed_backfill_enabled @ plugins/platforms/discord/adapter.py:DiscordAdapter._missed_message_backfill_enabled */
static json_t *da6_missed_backfill_enabled(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *extra = da6_get(ctx->state, "config_extra");
    json_t *cfg = da6_get(extra, "missed_message_backfill");
    if (cfg && cfg->type == JSON_OBJECT && da6_has(cfg, "enabled")) {
        json_t *val = da6_get(cfg, "enabled");
        if (val && val->type == JSON_STRING) {
            char *low = da6_lower(da6_str(val));
            bool hit = da6_truthy_set(low);
            free(low);
            return json_bool(hit);
        }
        return json_bool(da6_attr_truthy(val));
    }
    json_t *a = json_object();
    json_set_hdr(a, "name", "DISCORD_MISSED_MESSAGE_BACKFILL");
    json_set_hdr(a, "default", "false");
    json_t *r = da6_call(ctx, "_scoped_gate_env", a);
    if (da6_is_raise(r)) return r;
    char *st = da6_strip(da6_pystr_full(r));
    char *low = da6_lower(st);
    free(st);
    bool hit = da6_truthy_set(low);
    free(low);
    return json_bool(hit);
}

/* ============================ PORTED DEF 15 ============================ */
/* PoP: da6_get_effective_topic @ plugins/platforms/discord/adapter.py:DiscordAdapter._get_effective_topic */
static json_t *da6_get_effective_topic(da6_ctx_t *ctx, const json_t *args) {
    json_t *channel = json_obj_get(args, "channel");
    bool is_thread = json_get_bool(args, "is_thread", false);
    json_t *topic = da6_get(channel, "topic");
    if (!da6_truthy(topic) && is_thread) {
        json_t *parent = da6_get(channel, "parent");
        if (da6_attr_truthy(parent)) {
            json_t *a = json_object();
            json_set(a, "parent", json_copy(parent));
            json_t *f = da6_call(ctx, "_is_forum_parent", a);
            if (da6_is_raise(f)) return f;
            bool yes = da6_truthy(f);
            json_free(f);
            if (yes) topic = da6_get(parent, "topic");
        }
    }
    return topic ? json_copy(topic) : json_null();
}

/* ============================ PORTED DEF 16 ============================ */
/* PoP: da6_approval_mention_content @ plugins/platforms/discord/adapter.py:DiscordAdapter._approval_mention_content */
static json_t *da6_approval_mention_content(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "approval_mentions");
    json_set_hdr(a, "env_key", "DISCORD_APPROVAL_MENTIONS");
    json_set_hdr(a, "default", "false");
    json_set(a, "truthy", json_bool(true));
    json_t *flag = da6_call(ctx, "_extra_or_env_flag", a);
    if (da6_is_raise(flag)) return flag;
    if (!da6_truthy(flag)) { json_free(flag); return json_null(); }
    json_free(flag);
    json_t *ids = da6_get(ctx->state, "_allowed_user_ids");
    size_t n = (ids && ids->type == JSON_ARRAY) ? json_len(ids) : 0;
    char **v = malloc(sizeof(char *) * (n ? n : 1));
    if (!v) return json_null();
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        char *s = da6_pystr_full(json_get(ids, i));
        if (da6_isdigit_str(s)) v[m++] = s; else free(s);
    }
    for (size_t i = 0; i < m; i++)
        for (size_t j = i + 1; j < m; j++)
            if (strcmp(v[i], v[j]) > 0) { char *t = v[i]; v[i] = v[j]; v[j] = t; }
    if (m == 0) { free(v); return json_null(); }
    size_t cap = 1;
    for (size_t i = 0; i < m; i++) cap += strlen(v[i]) + 4;
    char *out = malloc(cap + 1);
    out[0] = '\0';
    for (size_t i = 0; i < m; i++) {
        if (i) strcat(out, " ");
        strcat(out, "<@"); strcat(out, v[i]); strcat(out, ">");
        free(v[i]);
    }
    free(v);
    json_t *res = da6_snode(out); free(out); return res;
}

/* ============================ PORTED DEF 17 ============================ */
/* PoP: da6_missed_backfill_number @ plugins/platforms/discord/adapter.py:DiscordAdapter._missed_message_backfill_number */
static json_t *da6_missed_backfill_number(da6_ctx_t *ctx, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    const char *env_key = json_get_str(args, "env_key", "");
    const char *cast = json_get_str(args, "cast", "int");
    json_t *def = json_obj_get(args, "default");
    json_t *lo = json_obj_get(args, "lo");
    json_t *hi = json_obj_get(args, "hi");
    json_t *extra = da6_get(ctx->state, "config_extra");
    json_t *cfg = da6_get(extra, "missed_message_backfill");
    json_t *raw;
    if (cfg && cfg->type == JSON_OBJECT && da6_has(cfg, key)) {
        raw = json_copy(da6_get(cfg, key));
    } else {
        json_t *a = json_object();
        json_set_hdr(a, "name", env_key);
        char *ds = da6_pystr_full(def);
        json_set_hdr(a, "default", ds);
        free(ds);
        raw = da6_call(ctx, "_scoped_gate_env", a);
        if (da6_is_raise(raw)) return raw;
    }
    if (da6_is_raise(raw)) return raw;
    bool ok;
    json_t *val = da6_cast(raw, cast, &ok);
    json_free(raw);
    if (!ok) {
        val = da6_cast(def, cast, &ok);
        if (!ok) return da6_err("ValueError", "invalid cast for default");
    }
    double d = val->num_val;
    json_free(val);
    double lov = lo ? lo->num_val : 0.0;
    double result;
    if (hi && hi->type != JSON_NULL) {
        double hiv = hi->num_val;
        result = d < hiv ? d : hiv;
        result = result > lov ? result : lov;
    } else {
        result = d > lov ? d : lov;
    }
    return json_number(result);
}

/* shared helper for the four fixed-argument backfill wrappers */
static json_t *da6_backfill_wrapper(da6_ctx_t *ctx, const char *key, const char *env_key,
                                    double def, const char *cast, double lo, double hi, bool has_hi) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "env_key", env_key);
    json_set(a, "default", json_number(def));
    json_set_hdr(a, "cast", cast);
    json_set(a, "lo", json_number(lo));
    json_set(a, "hi", has_hi ? json_number(hi) : json_null());
    return da6_call(ctx, "_missed_message_backfill_number", a);
}

/* PoP: da6_missed_backfill_limit @ plugins/platforms/discord/adapter.py:DiscordAdapter._missed_message_backfill_limit */
static json_t *da6_missed_backfill_limit(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da6_backfill_wrapper(ctx, "limit", "DISCORD_MISSED_MESSAGE_BACKFILL_LIMIT",
                                100.0, "int", 1.0, 500.0, true);
}
/* PoP: da6_missed_backfill_window @ plugins/platforms/discord/adapter.py:DiscordAdapter._missed_message_backfill_window_seconds */
static json_t *da6_missed_backfill_window(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da6_backfill_wrapper(ctx, "window_seconds", "DISCORD_MISSED_MESSAGE_BACKFILL_WINDOW_SECONDS",
                                21600.0, "float", 60.0, 0.0, false);
}
/* PoP: da6_missed_backfill_max_dispatches @ plugins/platforms/discord/adapter.py:DiscordAdapter._missed_message_backfill_max_dispatches */
static json_t *da6_missed_backfill_max_dispatches(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da6_backfill_wrapper(ctx, "max_dispatches", "DISCORD_MISSED_MESSAGE_BACKFILL_MAX_DISPATCHES",
                                10.0, "int", 1.0, 100.0, true);
}
/* PoP: da6_missed_backfill_max_attempts @ plugins/platforms/discord/adapter.py:DiscordAdapter._missed_message_backfill_max_attempts */
static json_t *da6_missed_backfill_max_attempts(da6_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da6_backfill_wrapper(ctx, "max_attempts", "DISCORD_MISSED_MESSAGE_BACKFILL_MAX_ATTEMPTS",
                                3.0, "int", 1.0, 100.0, true);
}
