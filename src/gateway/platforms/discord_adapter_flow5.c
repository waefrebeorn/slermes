/*
 * discord_adapter_flow5.c -- FLOW port of a coherent subset of
 * plugins/platforms/discord/adapter.py (hermes-agent): the Discord MESSAGE
 * CONSTRUCTION / CLASSIFICATION helpers -- content formatters
 * (format_message / format_tool_preview), reply-reference construction
 * (_message_reference_from_ids / _reply_reference_for_send), channel/thread
 * identity (_message_channel_ids / _thread_parent_channel / _thread_created /
 * _get_parent_channel_id / _interaction_guild_id / _forum_thread_parts /
 * _derive_auto_thread_name / _derive_forum_thread_name), error/message
 * classifiers (_is_down_notice_content / _is_reply_reference_rejected /
 * _is_length_overflow_error) and embed construction (_first_embed /
 * _config_embed).
 *
 * All 17 defs are GENUINELY UNCLAIMED by flow1/flow2 (verified by PoP scan).
 * Every one carries a PoP annotation at its definition below.
 *
 * Decision sequences only: every live I/O / injected callable / claimed
 * sibling / module helper / ctor / logger call enters through ONE universal
 * seam da5_call(ctx, name, args). The harness wires it to a recording stub
 * fed in.io; the oracle installs recording stubs emitting the identical
 * {"name","args","ret"} event from the SAME scripts. There are NO inlined
 * helpers under a different oracle seam name: the seam-name set is exactly
 * the SEAM TABLE below and is cross-checked by the oracle.
 *
 * Self-contained: libjson only; the harness #includes this .c directly.
 * All symbols are static and da5_-prefixed.
 *
 * Seam name table (name -> live call):
 *   _format_discord_markdown_link    _format_discord_markdown_link(text, url)
 *   convert_table_to_bullets         convert_table_to_bullets(content)
 *   t                                t(key)
 *   _truncate_discord_component_text _truncate_discord_component_text(text, limit)
 *   discord.MessageReference         discord.MessageReference(**kwargs)
 *   discord.Embed                    discord.Embed(**kwargs)
 *   discord.Color.blue               discord.Color.blue()
 *   re.search                        re.search(pattern, text)
 *   re.sub                           re.sub(pattern, repl, text)
 *   logger.debug                     logger.debug(fmt, *args)
 *   _message_reference_from_ids      self._message_reference_from_ids(mid, channel)
 *                                    (ported sibling, scripted when not target)
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow5.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)} (OSError adds
 *   errno, message "[Errno N] msg"). Full builtin hierarchy is modeled so
 *   except (ValueError, TypeError) and except Exception match live exactly;
 *   SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/BaseException
 *   pass through every except-Exception site.
 * - Attribute doubles (message/channel/author/guild/parent/thread/preview/
 *   interaction): JSON objects materialized as SimpleNamespace; an OBJECT
 *   double is ALWAYS truthy (even empty); scalars keep Python truthiness.
 *   hasattr/getattr-with-default read by KEY PRESENCE (present-null counts as
 *   present). getattr(x, k, None) -> null for absent OR present-null.
 * - str() positions carry strings / integral numbers / bools / None only
 *   (no container repr, no non-integral float repr). int() parses ASCII
 *   digits with optional sign; invalid tokens are printable ASCII.
 * - Slicing/length uses Unicode codepoints, not bytes. str.strip() folds the
 *   full Unicode White_Space set; str.lower() folds ASCII (fixture corpus).
 * - state: only "_reply_to_mode" is read (by _reply_reference_for_send);
 *   absent -> None. No ported function mutates state; out.state is echoed
 *   unchanged. Fixture io keys are a subset of the seam table.
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

static char *da5_strdup(const char *s); /* forward decl (da5_exc_str uses it) */

/* -- universal seam context -- */
typedef struct da5_ctx {
    json_t *state;      /* owned; input state copy, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da5_ctx_t;

/* -- basic helpers -- */
static bool da5_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da5_raise_kind(const json_t *v) {
    return da5_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da5_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}
static bool da5_truthy(const json_t *v) {
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
static bool da5_attr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_OBJECT) return true;
    return da5_truthy(v);
}
static const char *da5_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool da5_str_eq(const json_t *v, const char *s) {
    const char *t = da5_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *da5_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* -- full builtin exception hierarchy (parent map) -- */
typedef struct { const char *k; const char *p; } da5_exc_row_t;
static const da5_exc_row_t DA5_EXC_PARENTS[] = {
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
static const char *da5_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(DA5_EXC_PARENTS) / sizeof(DA5_EXC_PARENTS[0]); i++)
        if (strcmp(k, DA5_EXC_PARENTS[i].k) == 0) return DA5_EXC_PARENTS[i].p;
    return NULL;
}
/* True when kind extends target (identity included). Unknown kinds extend
 * Exception, matching a dynamically-created type(kind,(Exception,)). */
static bool da5_exc_extends(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int guard = 0; guard < 32 && cur; guard++) {
        if (strcmp(cur, target) == 0) return true;
        cur = da5_exc_parent(cur);
        if (!cur) {
            if (strcmp(kind, target) == 0) return true;
            if (strcmp(target, "Exception") == 0 || strcmp(target, "BaseException") == 0)
                return true;  /* unknown leaf behaves as Exception subclass */
            return false;
        }
    }
    return false;
}
static bool da5_is_base_exc(const char *kind) {
    if (!kind) return false;
    if (strcmp(kind, "BaseException") == 0) return true;
    return da5_exc_extends(kind, "BaseException") && !da5_exc_extends(kind, "Exception");
}
static bool da5_is_value_or_type(const char *kind) {
    return da5_exc_extends(kind, "ValueError") || da5_exc_extends(kind, "TypeError");
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc/proj_exc. */
static json_t *da5_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && da5_exc_extends(type, "OSError")) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = da5_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return da5_err(type, msg);
}
/* str(exc) from a raise descriptor is already the projected message. */
static char *da5_exc_str(const json_t *exc) {
    return da5_strdup(json_get_str(exc, "message", ""));
}

/* -- the one universal seam (mirrors oracle Ctx.call exactly) -- */
static json_t *da5_call(da5_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da5_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da5_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = da5_make_exc(raise);
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
static char *da5_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static json_t *da5_snode(const char *s) { return json_string(s ? s : ""); }
static unsigned int da5_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0) return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
         | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}
static size_t da5_cp_len(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xe0) == 0xc0) return 2;
    if ((p[0] & 0xf0) == 0xe0) return 3;
    return 4;
}
static bool da5_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f)) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
/* str.strip(): full Unicode White_Space, both ends. Caller frees. */
static char *da5_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    while (*p && da5_is_ws_cp(da5_decode_cp(p))) p += da5_cp_len(p);
    const unsigned char *end = p + strlen((const char *)p);
    while (end > p) {
        const unsigned char *q = end - 1;
        while (q > p && (*q & 0xc0) == 0x80) q--;
        if (!da5_is_ws_cp(da5_decode_cp(q))) break;
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
static char *da5_lstrip_chars(const char *s, const char *chars) {
    if (!s) s = "";
    const char *p = s;
    while (*p && strchr(chars, *p)) p++;
    return da5_strdup(p);
}
static char *da5_lower(const char *s) {
    char *o = da5_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da5_first_line(const char *s) {
    const char *nl = strchr(s ? s : "", '\n');
    size_t n = nl ? (size_t)(nl - s) : strlen(s ? s : "");
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}
static size_t da5_cp_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) { n++; p += da5_cp_len(p); }
    return n;
}
static char *da5_prefix(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *start = p;
    size_t k = 0;
    while (*p && k < n) { p += da5_cp_len(p); k++; }
    size_t bytes = (size_t)(p - start);
    char *o = malloc(bytes + 1);
    if (!o) return NULL;
    memcpy(o, start, bytes);
    o[bytes] = '\0';
    return o;
}
static char *da5_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da5_strdup("");
    switch (v->type) {
        case JSON_STRING: return da5_strdup(v->str_val);
        case JSON_BOOL: return da5_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == floor(v->num_val) && isfinite(v->num_val))
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else snprintf(buf, sizeof buf, "%g", v->num_val);
            return da5_strdup(buf);
        }
        default: return da5_strdup("");
    }
}
static char *da5_strval(const json_t *v, const char *dflt) {
    if (da5_is_null(v) || !da5_truthy(v)) return da5_strdup(dflt ? dflt : "");
    return da5_pystr(v);
}
static json_t *da5_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static bool da5_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static json_t *da5_arg_get(const json_t *obj, const char *key) {
    json_t *v = da5_get(obj, key);
    return (v && v->type != JSON_NULL) ? json_copy(v) : json_null();
}
static bool da5_int(const json_t *v, long long *out) {
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

/* ============================ PORTED DEF 1 ============================ */
/* PoP: da5_message_channel_ids @ plugins/platforms/discord/adapter.py:_message_channel_ids */
static json_t *da5_message_channel_ids(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *message = json_obj_get(args, "message");
    json_t *channel = da5_get(message, "channel");
    char *cid = da5_strval(da5_get(channel, "id"), "");
    char *pid = da5_strval(da5_get(channel, "parent_id"), "");
    json_t *arr = json_array();
    json_append(arr, da5_snode(cid));
    if (pid[0] != '\0') {
        json_append(arr, da5_snode(cid));   /* thread_id = channel_id */
        json_append(arr, da5_snode(pid));   /* parent_id = pid */
    } else {
        json_append(arr, json_null());
        json_append(arr, json_null());
    }
    free(cid); free(pid);
    return arr;
}

/* ============================ PORTED DEF 2 ============================ */
/* PoP: da5_thread_parent_channel @ plugins/platforms/discord/adapter.py:_thread_parent_channel */
static json_t *da5_thread_parent_channel(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *channel = json_obj_get(args, "channel");
    json_t *parent = da5_get(channel, "parent");
    if (da5_attr_truthy(parent)) return json_copy(parent);
    return json_copy(channel ? channel : json_null());
}

/* ============================ PORTED DEF 3 ============================ */
/* PoP: da5_thread_created @ plugins/platforms/discord/adapter.py:_thread_created */
static json_t *da5_thread_created(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *thread = json_obj_get(args, "thread");
    const char *name = json_get_str(args, "name", "");
    char *tids = da5_pystr(da5_get(thread, "id"));
    json_t *nm = da5_get(thread, "name");
    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "thread_id", da5_snode(tids));
    if (da5_truthy(nm)) json_set(out, "thread_name", json_copy(nm));
    else json_set_hdr(out, "thread_name", name);
    json_set(out, "thread", json_copy(thread ? thread : json_null()));
    free(tids);
    return out;
}

/* ============================ PORTED DEF 4 ============================ */
/* PoP: da5_get_parent_channel_id @ plugins/platforms/discord/adapter.py:_get_parent_channel_id */
static json_t *da5_get_parent_channel_id(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *channel = json_obj_get(args, "channel");
    json_t *parent = da5_get(channel, "parent");
    if (!da5_is_null(parent)) {
        json_t *pid = da5_get(parent, "id");
        if (!da5_is_null(pid)) {
            char *s = da5_pystr(pid);
            json_t *node = da5_snode(s);
            free(s);
            return node;
        }
    }
    json_t *parent_id = da5_get(channel, "parent_id");
    if (!da5_is_null(parent_id)) {
        char *s = da5_pystr(parent_id);
        json_t *node = da5_snode(s);
        free(s);
        return node;
    }
    return json_null();
}

/* ============================ PORTED DEF 5 ============================ */
/* PoP: da5_interaction_guild_id @ plugins/platforms/discord/adapter.py:_interaction_guild_id */
static json_t *da5_interaction_guild_id(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *interaction = json_obj_get(args, "interaction");
    json_t *gid = da5_get(interaction, "guild_id");
    if (da5_is_null(gid)) {
        json_t *channel = da5_get(interaction, "channel");
        json_t *guild = da5_get(channel, "guild");
        gid = da5_get(guild, "id");
    }
    if (da5_truthy(gid)) {
        char *s = da5_pystr(gid);
        json_t *node = da5_snode(s);
        free(s);
        return node;
    }
    return json_null();
}

/* ============================ PORTED DEF 6 ============================ */
/* PoP: da5_derive_auto_thread_name @ plugins/platforms/discord/adapter.py:_derive_auto_thread_name */
static json_t *da5_derive_auto_thread_name(da5_ctx_t *ctx, const json_t *args) {
    char *s0 = da5_strip(json_get_str(args, "content", ""));
    json_t *a = json_object();
    json_set_hdr(a, "pattern", "<@[!&]?\\d+>");
    json_set_hdr(a, "repl", "");
    json_set(a, "text", da5_snode(s0));
    json_t *r1 = da5_call(ctx, "re.sub", a);
    if (da5_is_raise(r1)) return r1;
    const char *s1 = da5_str(r1) ? da5_str(r1) : "";

    a = json_object();
    json_set_hdr(a, "pattern", "<#\\d+>");
    json_set_hdr(a, "repl", "");
    json_set(a, "text", da5_snode(s1));
    json_t *r2 = da5_call(ctx, "re.sub", a);
    if (da5_is_raise(r2)) return r2;
    const char *s2 = da5_str(r2) ? da5_str(r2) : "";

    a = json_object();
    json_set_hdr(a, "pattern", "\\s+");
    json_set_hdr(a, "repl", " ");
    json_set(a, "text", da5_snode(s2));
    json_t *r3 = da5_call(ctx, "re.sub", a);
    if (da5_is_raise(r3)) return r3;
    char *s3 = da5_strip(da5_str(r3) ? da5_str(r3) : "");

    char *out;
    size_t n = da5_cp_count(s3);
    if (n == 0) {
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "platform.discord.thread.default_name");
        json_t *tr = da5_call(ctx, "t", ta);
        if (da5_is_raise(tr)) return tr;
        out = da5_strdup(da5_str(tr) ? da5_str(tr) : "");
    } else if (n > 80) {
        char *head = da5_prefix(s3, 77);
        size_t hl = strlen(head);
        out = malloc(hl + 4);
        if (out) { memcpy(out, head, hl); memcpy(out + hl, "...", 4); }
        free(head);
    } else {
        out = da5_prefix(s3, n);
    }
    free(s0); free(s3);
    json_t *node = da5_snode(out);
    free(out);
    return node;
}

/* ============================ PORTED DEF 7 ============================ */
/* PoP: da5_derive_forum_thread_name @ plugins/platforms/discord/adapter.py:_derive_forum_thread_name */
static json_t *da5_derive_forum_thread_name(da5_ctx_t *ctx, const json_t *args) {
    char *st = da5_strip(json_get_str(args, "message", ""));
    char *fl = da5_first_line(st);
    char *fls = da5_strip(fl);
    char *hashless = da5_lstrip_chars(fls, "#");
    char *first = da5_strip(hashless);
    char *out;
    if (first[0] == '\0') {
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "platform.discord.forum.default_title");
        json_t *tr = da5_call(ctx, "t", ta);
        if (da5_is_raise(tr)) return tr;
        out = da5_strdup(da5_str(tr) ? da5_str(tr) : "");
    } else {
        out = da5_prefix(first, 100);
    }
    free(st); free(fl); free(fls); free(hashless); free(first);
    json_t *node = da5_snode(out);
    free(out);
    return node;
}

/* ============================ PORTED DEF 8 ============================ */
/* PoP: da5_forum_thread_parts @ plugins/platforms/discord/adapter.py:_forum_thread_parts */
static json_t *da5_forum_thread_parts(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *thread = json_obj_get(args, "thread");
    json_t *chan = da5_has(thread, "send") ? thread : da5_get(thread, "thread");
    json_t *cv = da5_get(chan, "id");
    if (da5_is_null(cv)) cv = da5_get(thread, "id");
    char *tid = da5_pystr(cv);
    json_t *starter = da5_get(thread, "message");
    char *mid;
    if (da5_truthy(starter)) {
        json_t *sid = da5_get(starter, "id");
        mid = da5_is_null(sid) ? da5_strdup(tid) : da5_pystr(sid);
    } else {
        mid = da5_strdup(tid);
    }
    json_t *arr = json_array();
    json_append(arr, json_copy(chan ? chan : json_null()));
    json_append(arr, da5_snode(tid));
    json_append(arr, json_copy(starter ? starter : json_null()));
    json_append(arr, da5_snode(mid));
    free(tid); free(mid);
    return arr;
}

/* Python repr() of a str: single quotes unless the value has a single quote and
 * no double quote, then double quotes. Escape backslash, the chosen quote,
 * \n \r \t and other control chars as \xNN. */
static void da5_py_repr_str(const char *in, char *out, size_t cap) {
    if (!in) in = "";
    bool has_sq = strchr(in, '\'') != NULL;
    bool has_dq = strchr(in, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = 0;
    if (cap < 3) { if (cap) out[0] = '\0'; return; }
    out[n++] = q;
    for (const char *p = in; *p && n + 5 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c == (unsigned char)q) { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else if (c == '\r') { out[n++] = '\\'; out[n++] = 'r'; }
        else if (c == '\t') { out[n++] = '\\'; out[n++] = 't'; }
        else if (c < 0x20 || c == 0x7f) n += (size_t)snprintf(out + n, cap - n, "\\x%02x", c);
        else out[n++] = (char)c;
    }
    out[n++] = q;
    out[n] = '\0';
}

/* int error descriptor matching CPython exactly on the fixture corpus. */
static json_t *da5_int_error(const json_t *v) {
    if (!v || v->type == JSON_NULL)
        return da5_err("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL)
        return da5_err("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'bool'");
    if (v->type == JSON_ARRAY)
        return da5_err("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'list'");
    if (v->type == JSON_OBJECT)
        return da5_err("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'dict'");
    if (v->type == JSON_STRING) {
        char msg[1024];
        char repr[600];
        da5_py_repr_str(v->str_val, repr, sizeof(repr));
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", repr);
        return da5_err("ValueError", msg);
    }
    return da5_err("ValueError", "invalid literal for int() with base 10");
}

/* ============================ PORTED DEF 9 ============================ */
/* PoP: da5_message_reference_from_ids @ plugins/platforms/discord/adapter.py:_message_reference_from_ids */
static json_t *da5_message_reference_from_ids(da5_ctx_t *ctx, const json_t *args) {
    long long mid;
    json_t *mv = json_obj_get(args, "message_id");
    if (!da5_int(mv, &mid)) return da5_int_error(mv);
    json_t *channel = json_obj_get(args, "channel");
    json_t *a = json_object();
    json_set(a, "message_id", json_number((double)mid));
    json_set(a, "channel_id", da5_arg_get(channel, "id"));
    json_set(a, "guild_id", da5_arg_get(da5_get(channel, "guild"), "id"));
    json_set(a, "fail_if_not_exists", json_bool(false));
    return da5_call(ctx, "discord.MessageReference", a);
}

/* ============================ PORTED DEF 10 ============================ */
/* PoP: da5_reply_reference_for_send @ plugins/platforms/discord/adapter.py:_reply_reference_for_send */
static json_t *da5_reply_reference_for_send(da5_ctx_t *ctx, const json_t *args) {
    json_t *reply_to = json_obj_get(args, "reply_to");
    json_t *mode = da5_get(ctx->state, "_reply_to_mode");
    if (!da5_truthy(reply_to) || da5_str_eq(mode, "off")) return json_null();
    json_t *channel = json_obj_get(args, "channel");
    json_t *a = json_object();
    json_set(a, "message_id", json_copy(reply_to));
    json_set(a, "channel", json_copy(channel ? channel : json_null()));
    json_t *r = da5_call(ctx, "_message_reference_from_ids", a);
    if (da5_is_raise(r)) {
        const char *kind = da5_raise_kind(r);
        if (da5_is_base_exc(kind)) return r;  /* BaseException passes through */
        if (da5_is_value_or_type(kind)) {
            char *estr = da5_exc_str(r);
            json_t *la = json_object();
            json_set_hdr(la, "fmt", "Could not build reply-to reference: %s");
            json_t *larr = json_array();
            json_append(larr, da5_snode(estr));
            json_set(la, "args", larr);
            free(estr);
            json_t *lr = da5_call(ctx, "logger.debug", la);
            if (da5_is_raise(lr)) return lr;  /* logger raise propagates */
            json_free(r);
            return json_null();
        }
        return r;  /* other Exception propagates */
    }
    return r;
}

/* ============================ PORTED DEF 11 ============================ */
/* PoP: da5_format_tool_preview @ plugins/platforms/discord/adapter.py:format_tool_preview */
static json_t *da5_format_tool_preview(da5_ctx_t *ctx, const json_t *args) {
    json_t *preview = json_obj_get(args, "preview");
    json_t *url = da5_get(preview, "url");
    if (!da5_truthy(url)) return json_copy(da5_get(preview, "text"));
    json_t *a = json_object();
    json_set(a, "text", json_copy(da5_get(preview, "text")));
    json_set(a, "url", json_copy(url));
    return da5_call(ctx, "_format_discord_markdown_link", a);
}

/* ============================ PORTED DEF 12 ============================ */
/* PoP: da5_format_message @ plugins/platforms/discord/adapter.py:format_message */
static json_t *da5_format_message(da5_ctx_t *ctx, const json_t *args) {
    json_t *content = json_obj_get(args, "content");
    if (!da5_truthy(content)) return json_copy(content ? content : json_null());
    json_t *a = json_object();
    json_set(a, "content", json_copy(content));
    return da5_call(ctx, "convert_table_to_bullets", a);
}

/* ============================ PORTED DEF 13 ============================ */
/* PoP: da5_is_down_notice_content @ plugins/platforms/discord/adapter.py:_is_down_notice_content */
static json_t *da5_is_down_notice_content(da5_ctx_t *ctx, const json_t *args) {
    char *text = da5_lower(json_get_str(args, "content", ""));
    const char *pattern =
        "\\b(?:hermes|the agent|agent|the gateway|gateway|bmo)\\s+"
        "(?:is|was|appears to be|is currently|was currently)\\s+"
        "(?:down|offline|unavailable|not running)\\b";
    json_t *a = json_object();
    json_set_hdr(a, "pattern", pattern);
    json_set(a, "text", da5_snode(text));
    json_t *r = da5_call(ctx, "re.search", a);
    free(text);
    if (da5_is_raise(r)) return r;
    bool found = !da5_is_null(r);
    json_free(r);
    return json_bool(found);
}

/* ============================ PORTED DEF 14 ============================ */
/* PoP: da5_is_reply_reference_rejected @ plugins/platforms/discord/adapter.py:_is_reply_reference_rejected */
static json_t *da5_is_reply_reference_rejected(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *s = json_get_str(args, "err", "");
    bool hit = (strstr(s, "error code: 50035") && strstr(s, "Cannot reply to a system message"))
            || strstr(s, "error code: 10008") != NULL;
    return json_bool(hit);
}

/* ============================ PORTED DEF 15 ============================ */
/* PoP: da5_is_length_overflow_error @ plugins/platforms/discord/adapter.py:_is_length_overflow_error */
static json_t *da5_is_length_overflow_error(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *text = da5_lower(json_get_str(args, "err", ""));
    bool hit = strstr(text, "error code: 50035") != NULL &&
               (strstr(text, "2000 or fewer") != NULL || strstr(text, "fewer in length") != NULL);
    free(text);
    return json_bool(hit);
}

/* ============================ PORTED DEF 16 ============================ */
/* PoP: da5_first_embed @ plugins/platforms/discord/adapter.py:_first_embed */
static json_t *da5_first_embed(da5_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *message = json_obj_get(args, "message");
    json_t *embeds = da5_get(message, "embeds");
    if (da5_truthy(embeds) && embeds->type == JSON_ARRAY && json_len(embeds) > 0)
        return json_copy(json_get(embeds, 0));
    return json_null();
}

/* ============================ PORTED DEF 17 ============================ */
/* PoP: da5_config_embed @ plugins/platforms/discord/adapter.py:_config_embed */
static json_t *da5_config_embed(da5_ctx_t *ctx, const json_t *args) {
    const char *description = json_get_str(args, "description", "");
    json_t *title_v = json_obj_get(args, "title");
    char *title;
    if (da5_is_null(title_v)) {
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "platform.discord.picker.title");
        json_t *tr = da5_call(ctx, "t", ta);
        if (da5_is_raise(tr)) return tr;
        title = da5_strdup(da5_str(tr) ? da5_str(tr) : "");
    } else {
        title = da5_strdup(da5_str(title_v) ? da5_str(title_v) : "");
    }
    json_t *ca = json_object();
    json_set(ca, "text", da5_snode(title));
    json_set(ca, "limit", json_number(256.0));
    json_t *ct = da5_call(ctx, "_truncate_discord_component_text", ca);
    if (da5_is_raise(ct)) return ct;
    const char *ctitle = da5_str(ct) ? da5_str(ct) : "";
    json_t *color_v = json_obj_get(args, "color");
    json_t *color;
    if (da5_is_null(color_v)) {
        color = da5_call(ctx, "discord.Color.blue", json_object());
        if (da5_is_raise(color)) return color;
    } else color = json_copy(color_v);
    json_t *ea = json_object();
    json_set_hdr(ea, "title", ctitle);
    json_set(ea, "description", da5_snode(description));
    json_set(ea, "color", json_copy(color));
    free(title);
    return da5_call(ctx, "discord.Embed", ea);
}
