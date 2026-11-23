/*
 * discord_adapter_flow7.c -- FLOW port of a coherent remainder subset of
 * plugins/platforms/discord/adapter.py (hermes-agent): standalone response /
 * encoding helpers, forum-probe cache, allowed-mentions builder, Discord
 * config int + voice/playback timeout readers, approval-budget / batch-delay /
 * channel-prompt / audio-timeout decision helpers, attachment classification,
 * guild-role / reply-target / thread-stamp utilities.
 *
 * All 18 defs are GENUINELY UNCLAIMED by flow1/flow2/flow5/flow6 (parent-verified
 * PoP scan over every C source). Every one carries a PoP annotation at its
 * definition below. No flow1/2/5/6 def is re-ported here.
 *
 * Decision sequences only: every live I/O / injected callable / claimed
 * sibling / module helper enters through ONE universal seam
 * da7_call(ctx, name, args). The harness wires it to a recording stub fed
 * in.io; the oracle installs recording stubs emitting the identical
 * {"name","args","ret"} event from the SAME scripts. No inlined helper uses a
 * different oracle seam name: the seam-name set is exactly the SEAM TABLE
 * below and is cross-checked by the oracle (set equality across C/oracle/
 * fixtures).
 *
 * Self-contained: libjson only; the harness #includes this .c directly.
 * All symbols are static and da7_-prefixed.
 *
 * Seam name table (name -> live call):
 *   resp.get_encoding                get_encoding() on an aiohttp-like response
 *   resp.close                       close() on a response
 *   resp.release                     release() on a response
 *   _extra_or_secret                 _extra_or_secret(configured, key, name, None)
 *   discord.AllowedMentions          discord.AllowedMentions(everyone=..., ...)
 *   read_raw_config                  hermes_cli.config.read_raw_config()
 *   logger.debug                     logger.debug(fmt, key, exc)
 *   _load_discord_int_config         self._load_discord_int_config(key, default, minimum=...)
 *   _approval_mention_content        self._approval_mention_content()
 *   super._exec_approval_cmd_budget  super()._exec_approval_cmd_budget(desc, smart_denied)
 *   super._text_batch_delay_for      super()._text_batch_delay_for(pending)
 *   resolve_channel_prompt           gateway.platforms.base.resolve_channel_prompt(extra, cid, pid)
 *   _playback_timeout_limit          self._playback_timeout_limit()
 *   _probe_audio_duration_seconds    self._probe_audio_duration_seconds(audio_path)
 *   _is_discord_voice_message_attachment self._is_discord_voice_message_attachment(att)
 *   guild.get_member                 guild.get_member(uid_int)
 *   _Snowflake                       _Snowflake(int(message_id))
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow7.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)} (OSError adds
 *   errno, message "[Errno N] msg"). Full builtin hierarchy is modeled so
 *   except (ValueError, TypeError) / except Exception match live exactly;
 *   SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/BaseException
 *   pass through every except-Exception site.
 * - Attribute doubles (resp / att / guild / member / pending / reference /
 *   thread) are JSON objects materialized as SimpleNamespace; an OBJECT double
 *   is ALWAYS truthy. getattr-with-default reads by KEY PRESENCE (present-null
 *   counts as present, value None). Callables (close/release/get_encoding) are
 *   represented by a truthy value at their key and fire the matching seam.
 * - str(None) == "None"; str() positions carry strings / integral numbers /
 *   bools / None only. int() parses ASCII digits with optional sign and
 *   surrounding whitespace. MessageType is represented by its member NAME.
 * - self attributes VOICE_TIMEOUT(300) / PLAYBACK_TIMEOUT(120) /
 *   PLAYBACK_TIMEOUT_PADDING(30) come from state with those defaults; optional
 *   _voice_timeout_seconds / _playback_timeout_seconds /
 *   _text_batch_split_delay_seconds are read from state when present.
 * - The forum-probe cache lives in state["_channel_type_probe_cache"]; the
 *   two cache defs are the only ported funcs that mutate state.
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

static char *da7_strdup(const char *s); /* forward decl (da7_exc_str uses it) */

/* -- universal seam context -- */
typedef struct da7_ctx {
    json_t *state;      /* owned; input state copy, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da7_ctx_t;

/* -- basic helpers -- */
static bool da7_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da7_raise_kind(const json_t *v) {
    return da7_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da7_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool da7_truthy(const json_t *v) {
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
static bool da7_attr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_OBJECT) return true;
    return da7_truthy(v);
}
static const char *da7_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool da7_str_eq(const json_t *v, const char *s) {
    const char *t = da7_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *da7_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* -- full builtin exception hierarchy (parent map) -- */
typedef struct { const char *k; const char *p; } da7_exc_row_t;
static const da7_exc_row_t DA7_EXC_PARENTS[] = {
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
static const char *da7_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(DA7_EXC_PARENTS) / sizeof(DA7_EXC_PARENTS[0]); i++)
        if (strcmp(k, DA7_EXC_PARENTS[i].k) == 0) return DA7_EXC_PARENTS[i].p;
    return NULL;
}
/* True when kind extends target (identity included). Unknown kinds extend
 * Exception, matching a dynamically-created type(kind,(Exception,)). */
static bool da7_exc_extends(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int guard = 0; guard < 32 && cur; guard++) {
        if (strcmp(cur, target) == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false; /* known root, not target */
        const char *p = da7_exc_parent(cur);
        if (!p) {
            /* unknown leaf behaves as an Exception subclass */
            return strcmp(target, "Exception") == 0 || strcmp(target, "BaseException") == 0;
        }
        cur = p;
    }
    return false;
}
static bool da7_is_base_exc(const char *kind) {
    if (!kind) return false;
    if (strcmp(kind, "BaseException") == 0) return true;
    return da7_exc_extends(kind, "BaseException") && !da7_exc_extends(kind, "Exception");
}
static bool da7_is_value_or_type(const char *kind) {
    return da7_exc_extends(kind, "ValueError") || da7_exc_extends(kind, "TypeError");
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc/proj_exc. */
static json_t *da7_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && da7_exc_extends(type, "OSError")) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = da7_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return da7_err(type, msg);
}
/* str(exc) from a raise descriptor is already the projected message. */
static char *da7_exc_str(const json_t *exc) {
    return da7_strdup(json_get_str(exc, "message", ""));
}

/* -- the one universal seam (mirrors oracle Ctx.call exactly) -- */
static json_t *da7_call(da7_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da7_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da7_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = da7_make_exc(raise);
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
static char *da7_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static json_t *da7_snode(const char *s) { return json_string(s ? s : ""); }
static unsigned int da7_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0) return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
         | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}
static size_t da7_cp_len(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xe0) == 0xc0) return 2;
    if ((p[0] & 0xf0) == 0xe0) return 3;
    return 4;
}
static bool da7_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f)) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
/* str.strip(): full Unicode White_Space, both ends. Caller frees. */
static char *da7_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    while (*p && da7_is_ws_cp(da7_decode_cp(p))) p += da7_cp_len(p);
    const unsigned char *end = p + strlen((const char *)p);
    while (end > p) {
        const unsigned char *q = end - 1;
        while (q > p && (*q & 0xc0) == 0x80) q--;
        if (!da7_is_ws_cp(da7_decode_cp(q))) break;
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
static char *da7_lstrip_chars(const char *s, const char *chars) {
    if (!s) s = "";
    const char *p = s;
    while (*p && strchr(chars, *p)) p++;
    return da7_strdup(p);
}
static char *da7_lower(const char *s) {
    char *o = da7_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da7_first_line(const char *s) {
    const char *nl = strchr(s ? s : "", '\n');
    size_t n = nl ? (size_t)(nl - s) : strlen(s ? s : "");
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}
static size_t da7_cp_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) { n++; p += da7_cp_len(p); }
    return n;
}
static char *da7_prefix(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *start = p;
    size_t k = 0;
    while (*p && k < n) { p += da7_cp_len(p); k++; }
    size_t bytes = (size_t)(p - start);
    char *o = malloc(bytes + 1);
    if (!o) return NULL;
    memcpy(o, start, bytes);
    o[bytes] = '\0';
    return o;
}
static char *da7_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da7_strdup("");
    switch (v->type) {
        case JSON_STRING: return da7_strdup(v->str_val);
        case JSON_BOOL: return da7_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == floor(v->num_val) && isfinite(v->num_val))
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else snprintf(buf, sizeof buf, "%g", v->num_val);
            return da7_strdup(buf);
        }
        default: return da7_strdup("");
    }
}
static char *da7_strval(const json_t *v, const char *dflt) {
    if (da7_is_null(v) || !da7_truthy(v)) return da7_strdup(dflt ? dflt : "");
    return da7_pystr(v);
}
static json_t *da7_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static bool da7_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static json_t *da7_arg_get(const json_t *obj, const char *key) {
    json_t *v = da7_get(obj, key);
    return (v && v->type != JSON_NULL) ? json_copy(v) : json_null();
}
static bool da7_int(const json_t *v, long long *out) {
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
/* str(None) == "None" (da7_pystr maps NULL to ""; Python str maps it to "None"). */
static char *da7_pystr_full(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da7_strdup("None");
    return da7_pystr(v);
}
/* str.isdigit(): non-empty, all ASCII digits (fixture corpus is ASCII). */
static bool da7_isdigit_str(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}
/* cast a JSON value to a number the way int()/float() does; *ok=false on failure. */
static json_t *da7_cast(const json_t *v, const char *how, bool *ok) {
    *ok = true;
    if (!how) how = "int";
    if (strcmp(how, "int") == 0) {
        long long n;
        if (!da7_int(v, &n)) { *ok = false; return NULL; }
        return json_number((double)n);
    }
    /* float */
    if (da7_is_null(v) || (v && v->type == JSON_BOOL)) { *ok = false; return NULL; }
    if (v && v->type == JSON_NUMBER) return json_copy(v);
    if (!v || v->type != JSON_STRING) { *ok = false; return NULL; }
    char *end = NULL;
    double d = strtod(v->str_val, &end);
    if (!end || *end != '\0') { *ok = false; return NULL; }
    return json_number(d);
}
static bool da7_truthy_set(const char *s) {
    return strcmp(s, "true") == 0 || strcmp(s, "1") == 0 ||
           strcmp(s, "yes") == 0 || strcmp(s, "on") == 0;
}

/* ===================== flow7 extra helpers ===================== */
/* getattr(obj, key, default): KEY PRESENCE (present-null counts as present). */
static json_t *da7_attr(const json_t *obj, const char *key, const json_t *dflt) {
    json_t *v = da7_get(obj, key);
    return v ? json_copy(v) : (dflt ? json_copy(dflt) : json_null());
}
/* direct attribute access that raises AttributeError when the key is absent */
static json_t *da7_need(const json_t *obj, const char *key) {
    json_t *v = da7_get(obj, key);
    if (!v) {
        char msg[256];
        snprintf(msg, sizeof msg, "'SimpleNamespace' object has no attribute '%s'", key);
        return da7_err("AttributeError", msg);
    }
    return json_copy(v);
}
static bool da7_startswith(const char *s, const char *pre) {
    if (!s) return false;
    size_t n = strlen(pre);
    return strncmp(s, pre, n) == 0;
}
/* canonical-set membership over a JSON array (compares str() forms) */
static bool da7_in_set(const json_t *arr, const json_t *v) {
    if (!arr || arr->type != JSON_ARRAY || da7_is_null(v)) return false;
    char *s = da7_pystr_full(v);
    bool hit = false;
    for (size_t i = 0; i < json_len(arr) && !hit; i++) {
        char *t = da7_pystr_full(json_get(arr, i));
        if (strcmp(s, t) == 0) hit = true;
        free(t);
    }
    free(s);
    return hit;
}
/* Python repr() of a str (ASCII domain): single quotes unless the value has
 * a single quote but no double quote; escape backslash, quote and controls. */
static char *da7_py_repr(const char *s) {
    if (!s) s = "";
    int has_s = strchr(s, '\'') != NULL;
    int has_d = strchr(s, '"') != NULL;
    char q = (has_s && !has_d) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return da7_strdup("''");
    static const char HX[] = "0123456789abcdef";
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (unsigned char)q) { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            out[k++] = '\\'; out[k++] = 'x'; out[k++] = HX[c >> 4]; out[k++] = HX[c & 15];
        } else out[k++] = (char)c;
    }
    out[k++] = q; out[k] = '\0';
    return out;
}
/* Python str() of the exception int() would raise for a bad raw value. */
static char *da7_int_err(const json_t *raw) {
    char buf[512];
    if (raw && raw->type == JSON_STRING) {
        char *rep = da7_py_repr(raw->str_val ? raw->str_val : "");
        snprintf(buf, sizeof buf, "invalid literal for int() with base 10: %s", rep);
        free(rep);
    }
    else if (da7_is_null(raw))
        snprintf(buf, sizeof buf,
                 "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    else
        snprintf(buf, sizeof buf,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 (!raw) ? "NoneType" : "SimpleNamespace");
    return da7_strdup(buf);
}
/* truthiness of a callable slot: present and truthy */
static bool da7_callable_slot(const json_t *obj, const char *key) {
    json_t *v = da7_get(obj, key);
    return v && da7_truthy(v);
}

/* ============================ PORTED DEF 1 ============================ */
/* PoP: da7_standalone_response_encoding @ plugins/platforms/discord/adapter.py:_standalone_response_encoding */
static json_t *da7_standalone_response_encoding(da7_ctx_t *ctx, const json_t *args) {
    json_t *resp = json_obj_get(args, "resp");
    if (da7_callable_slot(resp, "get_encoding")) {
        json_t *r = da7_call(ctx, "resp.get_encoding", json_object());
        if (da7_is_raise(r)) {
            if (da7_is_base_exc(da7_raise_kind(r))) return r;  /* BaseException passes */
            json_free(r);
            return da7_snode("utf-8");                          /* except Exception */
        }
        if (da7_truthy(r)) return r;                            /* get_encoding() or "utf-8" */
        json_free(r);
    }
    return da7_snode("utf-8");
}

/* ============================ PORTED DEF 2 ============================ */
/* PoP: da7_standalone_close_response @ plugins/platforms/discord/adapter.py:_standalone_close_response */
static json_t *da7_standalone_close_response(da7_ctx_t *ctx, const json_t *args) {
    json_t *resp = json_obj_get(args, "resp");
    if (da7_callable_slot(resp, "close")) {
        json_t *r = da7_call(ctx, "resp.close", json_object());
        if (da7_is_raise(r)) return r;
        json_free(r);
        return json_null();
    }
    if (da7_callable_slot(resp, "release")) {
        json_t *r = da7_call(ctx, "resp.release", json_object());
        if (da7_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* ============================ PORTED DEF 3 ============================ */
/* PoP: da7_remember_channel_is_forum @ plugins/platforms/discord/adapter.py:_remember_channel_is_forum */
static json_t *da7_remember_channel_is_forum(da7_ctx_t *ctx, const json_t *args) {
    char *key = da7_pystr_full(json_obj_get(args, "chat_id"));
    json_t *val = json_bool(da7_truthy(json_obj_get(args, "is_forum")));
    json_t *cache = da7_get(ctx->state, "_channel_type_probe_cache");
    if (!cache || cache->type != JSON_OBJECT) {
        cache = json_object();
        json_set(ctx->state, "_channel_type_probe_cache", cache);
    }
    json_set(cache, key, val);
    free(key);
    return json_null();
}

/* ============================ PORTED DEF 4 ============================ */
/* PoP: da7_probe_is_forum_cached @ plugins/platforms/discord/adapter.py:_probe_is_forum_cached */
static json_t *da7_probe_is_forum_cached(da7_ctx_t *ctx, const json_t *args) {
    char *key = da7_pystr_full(json_obj_get(args, "chat_id"));
    json_t *cache = da7_get(ctx->state, "_channel_type_probe_cache");
    json_t *v = da7_get(cache, key);
    free(key);
    return v ? json_copy(v) : json_null();
}

/* ============================ PORTED DEF 5 ============================ */
/* PoP: da7_build_allowed_mentions @ plugins/platforms/discord/adapter.py:_build_allowed_mentions */
static json_t *da7_build_allowed_mentions(da7_ctx_t *ctx, const json_t *args) {
    json_t *dav = da7_get(ctx->state, "DISCORD_AVAILABLE");
    if (dav && !da7_truthy(dav)) return json_null();
    json_t *extra = json_obj_get(args, "extra");
    json_t *cfg = da7_get(extra, "allow_mentions");
    if (!cfg || cfg->type != JSON_OBJECT) cfg = NULL;

    static const char *keys[4] = {"everyone", "roles", "users", "replied_user"};
    static const char *names[4] = {
        "DISCORD_ALLOW_MENTION_EVERYONE", "DISCORD_ALLOW_MENTION_ROLES",
        "DISCORD_ALLOW_MENTION_USERS", "DISCORD_ALLOW_MENTION_REPLIED_USER"};
    static const bool defaults_[4] = {false, false, true, true};
    bool vals[4];
    for (int i = 0; i < 4; i++) {
        json_t *a = json_object();
        json_set(a, "configured", cfg ? json_copy(cfg) : json_object());
        json_set_hdr(a, "key", keys[i]);
        json_set_hdr(a, "name", names[i]);
        json_set(a, "default", json_null());
        json_t *raw = da7_call(ctx, "_extra_or_secret", a);
        if (da7_is_raise(raw)) return raw;
        bool v;
        if (raw->type == JSON_BOOL) v = raw->bool_val;
        else if (da7_is_null(raw)) v = defaults_[i];
        else {
            char *st = da7_strip(da7_pystr_full(raw));
            char *lo = da7_lower(st);
            v = da7_truthy_set(lo);
            free(lo); free(st);
        }
        vals[i] = v;
        json_free(raw);
    }
    json_t *a = json_object();
    for (int i = 0; i < 4; i++) json_set(a, keys[i], json_bool(vals[i]));
    return da7_call(ctx, "discord.AllowedMentions", a);
}

/* ============================ PORTED DEF 6 ============================ */
/* PoP: da7_load_discord_int_config @ plugins/platforms/discord/adapter.py:DiscordAdapter._load_discord_int_config */
static json_t *da7_load_discord_int_config(da7_ctx_t *ctx, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    json_t *defv = json_obj_get(args, "default");
    json_t *minv = json_obj_get(args, "minimum");
    json_t *rc = da7_call(ctx, "read_raw_config", json_object());
    bool failed = da7_is_raise(rc);
    json_t *exc = NULL;
    json_t *rawv = NULL;
    if (!failed) {
        json_t *cfg = da7_truthy(rc) ? rc : NULL;
        json_t *sec = cfg ? da7_get(cfg, "discord") : NULL;
        json_t *scope = (sec && da7_truthy(sec)) ? sec : NULL;
        rawv = scope ? da7_get(scope, key) : NULL;
        if (!rawv) rawv = defv;
        long long n;
        if (da7_int(rawv, &n)) {
            double mn = (minv && minv->type == JSON_NUMBER) ? minv->num_val : 0.0;
            double res = ((double)n < mn) ? mn : (double)n;
            json_free(rc);
            return json_number(res);
        }
        failed = true;
    } else {
        exc = rc;
    }
    /* except Exception as e: logger.debug(fmt, key, e); return default */
    char *msg = exc ? da7_exc_str(exc) : da7_int_err(rawv);
    json_t *a = json_object();
    json_set_hdr(a, "fmt", "Could not load discord.%s config: %s");
    json_t *arr = json_array();
    json_append(arr, json_string(key));
    json_append(arr, json_string(msg));
    free(msg);
    json_set(a, "args", arr);
    json_t *lg = da7_call(ctx, "logger.debug", a);
    /* exc aliases rc (the seam's raise descriptor): free rc exactly once. */
    if (da7_is_raise(lg)) { json_free(rc); return lg; }
    json_free(lg);
    json_free(rc);
    return defv ? json_copy(defv) : json_number(0.0);
}

/* shared wrapper for the two fixed-arg timeout loaders */
static json_t *da7_timeout_loader(da7_ctx_t *ctx, const char *key, double def, double minv) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set(a, "default", json_number(def));
    json_set(a, "minimum", json_number(minv));
    return da7_call(ctx, "_load_discord_int_config", a);
}
/* PoP: da7_load_voice_timeout @ plugins/platforms/discord/adapter.py:DiscordAdapter._load_voice_timeout */
static json_t *da7_load_voice_timeout(da7_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da7_timeout_loader(ctx, "voice_channel_inactivity_timeout_seconds", 300.0, 0.0);
}
/* PoP: da7_load_playback_timeout @ plugins/platforms/discord/adapter.py:DiscordAdapter._load_playback_timeout */
static json_t *da7_load_playback_timeout(da7_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da7_timeout_loader(ctx, "voice_playback_timeout_seconds", 120.0, 1.0);
}

/* shared helper for the two timeout-limit getters */
static json_t *da7_timeout_limit(da7_ctx_t *ctx, const char *attr, const char *constant, double cdef) {
    json_t *v = da7_get(ctx->state, attr);
    if (!v) {
        v = da7_get(ctx->state, constant);
        if (!v) v = json_number(cdef);
    }
    long long n;
    if (!da7_int(v, &n)) {
        char *msg = da7_int_err(v);
        json_t *r = da7_err("ValueError", msg);
        free(msg);
        return r;
    }
    return json_number((double)n);
}
/* PoP: da7_voice_timeout_limit @ plugins/platforms/discord/adapter.py:DiscordAdapter._voice_timeout_limit */
static json_t *da7_voice_timeout_limit(da7_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da7_timeout_limit(ctx, "_voice_timeout_seconds", "VOICE_TIMEOUT", 300.0);
}
/* PoP: da7_playback_timeout_limit @ plugins/platforms/discord/adapter.py:DiscordAdapter._playback_timeout_limit */
static json_t *da7_playback_timeout_limit(da7_ctx_t *ctx, const json_t *args) {
    (void)args;
    return da7_timeout_limit(ctx, "_playback_timeout_seconds", "PLAYBACK_TIMEOUT", 120.0);
}

/* ============================ PORTED DEF 11 ============================ */
/* PoP: da7_exec_approval_cmd_budget @ plugins/platforms/discord/adapter.py:DiscordAdapter._exec_approval_cmd_budget */
static json_t *da7_exec_approval_cmd_budget(da7_ctx_t *ctx, const json_t *args) {
    json_t *m = da7_call(ctx, "_approval_mention_content", json_object());
    if (da7_is_raise(m)) return m;
    size_t L = 0;
    if (da7_str(m)) L = da7_cp_count(da7_str(m));
    else if (!da7_is_null(m)) {
        char *s = da7_pystr_full(m); L = da7_cp_count(s); free(s);
    }
    json_free(m);
    json_t *a = json_object();
    json_set_hdr(a, "description", json_get_str(args, "description", ""));
    json_set(a, "smart_denied", json_bool(json_get_bool(args, "smart_denied", false)));
    json_t *b = da7_call(ctx, "super._exec_approval_cmd_budget", a);
    if (da7_is_raise(b)) return b;
    double v = (b->type == JSON_NUMBER) ? b->num_val : 0.0;
    json_free(b);
    double res = v - (double)(L + 1);
    return json_number(res < 0.0 ? 0.0 : res);
}

/* ============================ PORTED DEF 12 ============================ */
/* PoP: da7_text_batch_delay_for @ plugins/platforms/discord/adapter.py:DiscordAdapter._text_batch_delay_for */
static json_t *da7_text_batch_delay_for(da7_ctx_t *ctx, const json_t *args) {
    json_t *pending = json_obj_get(args, "pending");
    if (da7_callable_slot(pending, "_bot_tag_debounce")) {
        json_t *v = da7_get(ctx->state, "_text_batch_split_delay_seconds");
        return v ? json_copy(v) : json_number(0.0);
    }
    json_t *a = json_object();
    json_set(a, "pending", pending ? json_copy(pending) : json_null());
    return da7_call(ctx, "super._text_batch_delay_for", a);
}

/* ============================ PORTED DEF 13 ============================ */
/* PoP: da7_resolve_channel_prompt @ plugins/platforms/discord/adapter.py:DiscordAdapter._resolve_channel_prompt */
static json_t *da7_resolve_channel_prompt(da7_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_t *extra = da7_get(ctx->state, "config_extra");
    json_set(a, "extra", extra ? json_copy(extra) : json_object());
    json_set_hdr(a, "channel_id", json_get_str(args, "channel_id", ""));
    json_t *pid = json_obj_get(args, "parent_id");
    json_set(a, "parent_id", pid ? json_copy(pid) : json_null());
    return da7_call(ctx, "resolve_channel_prompt", a);
}

/* ============================ PORTED DEF 14 ============================ */
/* PoP: da7_playback_timeout_for_audio @ plugins/platforms/discord/adapter.py:DiscordAdapter._playback_timeout_for_audio */
static json_t *da7_playback_timeout_for_audio(da7_ctx_t *ctx, const json_t *args) {
    json_t *p = da7_call(ctx, "_playback_timeout_limit", json_object());
    if (da7_is_raise(p)) return p;
    double floor = 0.0;
    { bool ok; json_t *f = da7_cast(p, "float", &ok); if (ok && f) floor = f->num_val; if (f) json_free(f); }
    json_free(p);
    json_t *a = json_object();
    json_set_hdr(a, "audio_path", json_get_str(args, "audio_path", ""));
    json_t *d = da7_call(ctx, "_probe_audio_duration_seconds", a);
    if (da7_is_raise(d)) return d;
    bool hasd = false;
    double dur = 0.0;
    { bool ok; json_t *x = da7_cast(d, "float", &ok); if (ok && x) { dur = x->num_val; hasd = true; } if (x) json_free(x); }
    json_free(d);
    if (!hasd || dur <= 0.0) return json_number(floor);
    json_t *pad = da7_get(ctx->state, "PLAYBACK_TIMEOUT_PADDING");
    double padn = (pad && pad->type == JSON_NUMBER) ? pad->num_val : 30.0;
    double cand = dur + padn;
    return json_number(cand > floor ? cand : floor);
}

/* ============================ PORTED DEF 15 ============================ */
/* PoP: da7_attachment_message_type @ plugins/platforms/discord/adapter.py:DiscordAdapter._attachment_message_type */
static json_t *da7_attachment_message_type(da7_ctx_t *ctx, const json_t *args) {
    json_t *att = json_obj_get(args, "att");
    json_t *ct = da7_need(att, "content_type");
    if (da7_is_raise(ct)) return ct;
    const char *s = (ct->type == JSON_STRING) ? ct->str_val : "";
    json_free(ct);
    if (da7_startswith(s, "image/")) return da7_snode("PHOTO");
    if (da7_startswith(s, "video/")) return da7_snode("VIDEO");
    if (da7_startswith(s, "audio/")) {
        json_t *a = json_object();
        json_set(a, "att", att ? json_copy(att) : json_null());
        json_t *r = da7_call(ctx, "_is_discord_voice_message_attachment", a);
        if (da7_is_raise(r)) return r;
        const char *name = da7_truthy(r) ? "VOICE" : "AUDIO";
        json_free(r);
        return da7_snode(name);
    }
    return da7_snode("DOCUMENT");
}

/* ============================ PORTED DEF 16 ============================ */
/* PoP: da7_guild_member_has_role @ plugins/platforms/discord/adapter.py:DiscordAdapter._guild_member_has_role */
static json_t *da7_guild_member_has_role(da7_ctx_t *ctx, const json_t *args) {
    json_t *guild = json_obj_get(args, "guild");
    (void)guild;
    json_t *uid = json_obj_get(args, "user_id");
    json_t *roles = json_obj_get(args, "allowed_roles");
    long long n;
    if (!da7_int(uid, &n)) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "user_id", json_number((double)n));
    json_t *m = da7_call(ctx, "guild.get_member", a);
    if (da7_is_raise(m)) return m;
    if (!m || m->type != JSON_OBJECT || da7_is_null(m)) { json_free(m); return json_bool(false); }
    json_t *mr = da7_get(m, "roles");
    if (mr && mr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(mr); i++) {
            json_t *r = json_get(mr, i);
            json_t *rid = (r && r->type == JSON_OBJECT) ? da7_get(r, "id") : NULL;
            if (rid && da7_in_set(roles, rid)) { json_free(m); return json_bool(true); }
        }
    }
    json_free(m);
    return json_bool(false);
}

/* ============================ PORTED DEF 17 ============================ */
/* PoP: da7_reply_target @ plugins/platforms/discord/adapter.py:DiscordAdapter._reply_target */
static json_t *da7_reply_target(da7_ctx_t *ctx, const json_t *args) {
    json_t *ref = json_obj_get(args, "reference");
    json_t *resolved = da7_get(ref, "resolved");
    json_t *rid = da7_get(resolved, "id");
    if (rid && !da7_is_null(rid)) return resolved ? json_copy(resolved) : json_null();
    json_t *mid = da7_get(ref, "message_id");
    if (mid && !da7_is_null(mid)) {
        long long n;
        if (da7_int(mid, &n)) {
            json_t *a = json_object();
            json_set(a, "id", json_number((double)n));
            return da7_call(ctx, "_Snowflake", a);
        }
    }
    return json_null();
}

/* ============================ PORTED DEF 18 ============================ */
/* PoP: da7_stamp_auto_thread_name @ plugins/platforms/discord/adapter.py:DiscordAdapter._stamp_auto_thread_name */
static json_t *da7_stamp_auto_thread_name(da7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *thread = json_obj_get(args, "thread");
    json_t *name = json_obj_get(args, "thread_name");
    if (!thread || thread->type != JSON_OBJECT) return thread ? json_copy(thread) : json_null();
    json_t *t = json_copy(thread);
    json_set(t, "_hermes_auto_thread_initial_name", name ? json_copy(name) : json_null());
    return t;
}
