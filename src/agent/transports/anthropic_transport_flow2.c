/* anthropic_transport_flow2.c — FLOW port of the anthropic Messages-API transport
 * surface agent/transports/anthropic.py (the remainder not carried by
 * anthropic_transport_flow.c, PLUS those two re-ported here for a complete,
 * independent module oracle). This upgrades the stub-era
 * transports_anthropic_ports.c / provider_anthropic.c claims to real behaviour.
 *
 * Ten ported functions, all static at2_* so nothing collides with the stub-era
 * symbols or the ant_* flow port:
 *   _unprefix_oauth_tool_name, api_mode, convert_messages, convert_tools,
 *   build_kwargs, normalize_response, response_finish_reason,
 *   validate_response, extract_cache_stats, map_finish_reason (inherited from
 *   agent/transports/base.py, credited here as the transport surface).
 *
 * Decision-sequence port: every collaborator call rides the injected universal
 * seam at2_call; the harness scripts results and records the ordered trace the
 * oracle diffs against LIVE quarry defs under the same scripts. Ported functions
 * call each other REAL (internal composition): normalize_response ->
 * _unprefix_oauth_tool_name / response_finish_reason -> map_finish_reason.
 * libjson-only; self-contained (the harness #includes this file).
 *
 * Seam name -> live call (args projection; None-valued keys dropped both sides):
 *   convert_messages_to_anthropic  convert_messages_to_anthropic(messages, base_url=, model=)
 *                                  {messages, base_url, model}
 *   convert_tools_to_anthropic     convert_tools_to_anthropic(tools)        {tools}
 *   build_anthropic_kwargs         build_anthropic_kwargs(model=, messages=, tools=, **defaults)
 *                                  {model, messages, tools, <9 default keys>}
 *   to_plain_data                  _to_plain_data(value)                     {value}
 *   sanitize_replay_block          _sanitize_replay_block(b)                 {b}
 *   guardrail_intervened           anthropic_response_guardrail_intervened(response) {response}
 *   map_finish_reason              self.map_finish_reason(raw_reason)        {raw_reason}
 *   registry.get_entry             registry.get_entry(name)                 {name}
 *
 * Representation contract (shared with tests/oracle_anthropic_transport_flow2.py):
 *   - None -> JSON null; tuple/set -> array; integral floats collapse to ints
 *     (Python == equates them); dict key order mirrors live insertion order.
 *   - uncaught exception -> {"__raise__": cls, "msg": str(exc)}; a scripted seam
 *     raise parks the marker VERBATIM. Full builtin exception hierarchy: the
 *     oracle resolves cls via getattr(builtins, ...) (BaseException catch so the
 *     SystemExit/KeyboardInterrupt/GeneratorExit trio propagates).
 *   - scripts: MISSING key -> RuntimeError("no script for <name>");
 *     {"$seq":[...]} per-call queue (exhaustion -> RuntimeError("script exhausted: <name>"));
 *     {"__raise__":cls,"msg":m} raises (trace ret is the WHOLE marker); scripted null -> None;
 *     any other value reused (fresh deep copy per call).
 *
 * Fixture domain (ASCII scope):
 *   - block.type / response.stop_reason / tool name are strings; tool input is a
 *     strict Python-json-serialisable value; text/thinking join operands are str.
 *   - response is an attribute dict projected identically both sides (content is
 *     an array of block objects; usage / stop_details optional).
 *   - _unprefix_oauth_tool_name reverse aliases are the fixed adapter constant
 *     {chat_history_lookup: session_search, context_notes: memory}.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

/* ── context + basic node helpers ────────────────────────────────────────── */

typedef struct at2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} at2_ctx;

static bool at2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool at2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool at2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool at2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool at2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool at2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool at2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool at2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* borrowed object field or NULL (absent). present-null returns the JSON_NULL node. */
static json_t *at2_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present value (incl null) passes through; absent -> default.
 * Returns BORROWED node. */
static json_t *at2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}

/* Python type name for a JSON node (fixture domain). */
static const char *at2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Python repr(float): shortest decimal that round-trips. Caller frees. */
static char *at2_float_repr(double d) {
    char sci[64];
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(sci, sizeof sci, "%.*e", prec - 1, d);
        if (strtod(sci, NULL) == d) break;
    }
    const char *p = sci;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    char dig[32]; int ndig = 0;
    while (*p && *p != 'e' && *p != 'E') {
        if (*p >= '0' && *p <= '9') dig[ndig++] = *p;
        p++;
    }
    dig[ndig] = '\0';
    int E = 0;
    if (*p == 'e' || *p == 'E') E = atoi(p + 1);
    while (ndig > 1 && dig[ndig - 1] == '0') dig[--ndig] = '\0';
    char out[80];
    if (E >= -4 && E < 16) {
        int point = E + 1;
        if (point <= 0) {
            int z = -point;
            char zeros[32]; int zi = 0; for (int i = 0; i < z && zi < 30; i++) zeros[zi++] = '0'; zeros[zi] = '\0';
            snprintf(out, sizeof out, "%s0.%s%s", neg ? "-" : "", zeros, dig);
        } else if (point >= ndig) {
            char zeros[32]; int zi = 0; for (int i = 0; i < point - ndig && zi < 30; i++) zeros[zi++] = '0'; zeros[zi] = '\0';
            snprintf(out, sizeof out, "%s%s%s.0", neg ? "-" : "", dig, zeros);
        } else {
            snprintf(out, sizeof out, "%s%.*s.%s", neg ? "-" : "", point, dig, dig + point);
        }
    } else {
        char mant[40];
        if (ndig > 1) snprintf(mant, sizeof mant, "%c.%s", dig[0], dig + 1);
        else snprintf(mant, sizeof mant, "%c", dig[0]);
        int ae = E < 0 ? -E : E;
        snprintf(out, sizeof out, "%s%se%c%02d", neg ? "-" : "", mant, E < 0 ? '-' : '+', ae);
    }
    return strdup(out);
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool at2_exc_pending(const at2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void at2_raise(at2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static void at2_raise_fmt(at2_ctx *ctx, const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    at2_raise(ctx, cls, buf);
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW (caller frees). */
static json_t *at2_call(at2_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *chosen = NULL;
    json_t *fc_marker = NULL;
    if (!script) {
        fc_marker = json_object();
        json_set(fc_marker, "__raise__", json_string("RuntimeError"));
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        json_set(fc_marker, "msg", json_string(msg));
        chosen = fc_marker;
    } else if (script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                fc_marker = json_object();
                json_set(fc_marker, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(fc_marker, "msg", json_string(msg));
                chosen = fc_marker;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        } else {
            chosen = script;
        }
    } else {
        chosen = script;
    }
    json_t *ret = NULL;
    bool raises = false;
    if (fc_marker) {
        if (ctx->exc) json_free(ctx->exc);
        ctx->exc = json_copy(fc_marker);
        raises = true;
    } else if (chosen && chosen->type == JSON_OBJECT) {
        json_t *r = json_obj_get(chosen, "__raise__");
        if (r && r->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(chosen);
            raises = true;
        } else {
            ret = json_copy(chosen);
        }
    } else if (chosen && chosen->type != JSON_NULL) {
        ret = json_copy(chosen);
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (raises) json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    if (fc_marker) json_free(fc_marker);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed val copied. */
static void at2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void at2_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!at2_is_none(val)) json_set(args, key, json_copy(val));
}

/* ── Python-style json.dumps (ensure_ascii=True, ", " / ": " separators) ─── */

/* Decode one UTF-8 codepoint at s; advance *len. */
static unsigned int at2_utf8_decode(const char *s, size_t *len) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *len = 1; return c; }
    unsigned int cp; size_t n;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
    else { *len = 1; return 0xFFFD; }
    for (size_t i = 1; i < n; i++) {
        unsigned char cc = (unsigned char)s[i];
        if ((cc & 0xC0) != 0x80) { *len = i; return 0xFFFD; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    *len = n;
    return cp;
}

typedef struct { char *buf; size_t len, cap; } at2_strbuf;
static void at2_sb_putc(at2_strbuf *b, char c) {
    if (b->len + 2 > b->cap) { b->cap = b->cap ? b->cap * 2 : 128; b->buf = (char *)realloc(b->buf, b->cap); }
    b->buf[b->len++] = c; b->buf[b->len] = '\0';
}
static void at2_sb_puts(at2_strbuf *b, const char *s) { while (*s) at2_sb_putc(b, *s++); }
static void at2_sb_quote(at2_strbuf *b, const char *s) {
    at2_sb_putc(b, '"');
    size_t i = 0, n = strlen(s);
    while (i < n) {
        size_t l; unsigned int cp = at2_utf8_decode(s + i, &l);
        if (cp == '"') at2_sb_puts(b, "\\\"");
        else if (cp == '\\') at2_sb_puts(b, "\\\\");
        else if (cp == '\b') at2_sb_puts(b, "\\b");
        else if (cp == '\f') at2_sb_puts(b, "\\f");
        else if (cp == '\n') at2_sb_puts(b, "\\n");
        else if (cp == '\r') at2_sb_puts(b, "\\r");
        else if (cp == '\t') at2_sb_puts(b, "\\t");
        else if (cp < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", cp); at2_sb_puts(b, e); }
        else if (cp < 0x80) at2_sb_putc(b, (char)cp);
        else if (cp <= 0xFFFF) { char e[8]; snprintf(e, sizeof e, "\\u%04x", cp); at2_sb_puts(b, e); }
        else {
            unsigned int v = cp - 0x10000;
            unsigned int hi = 0xD800 + (v >> 10), lo = 0xDC00 + (v & 0x3FF);
            char e[16]; snprintf(e, sizeof e, "\\u%04x\\u%04x", hi, lo); at2_sb_puts(b, e);
        }
        i += l;
    }
    at2_sb_putc(b, '"');
}
static void at2_sb_node(at2_strbuf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { at2_sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: at2_sb_puts(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64]; double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) { snprintf(tmp, sizeof tmp, "%.0f", d); at2_sb_puts(b, tmp); }
            else { char *fr = at2_float_repr(d); at2_sb_puts(b, fr); free(fr); }
            break;
        }
        case JSON_STRING: at2_sb_quote(b, v->str_val ? v->str_val : ""); break;
        case JSON_ARRAY:
            at2_sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) at2_sb_puts(b, ", ");
                at2_sb_node(b, v->c.items[i]);
            }
            at2_sb_putc(b, ']'); break;
        case JSON_OBJECT:
            at2_sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) at2_sb_puts(b, ", ");
                at2_sb_quote(b, v->c.keys[i]);
                at2_sb_puts(b, ": ");
                at2_sb_node(b, v->c.items[i]);
            }
            at2_sb_putc(b, '}'); break;
        default: at2_sb_puts(b, "null"); break;
    }
}
/* json.dumps(v) with Python defaults. Caller frees. */
static char *at2_json_dumps(const json_t *v) {
    at2_strbuf b = {NULL, 0, 0};
    at2_sb_node(&b, v);
    return b.buf ? b.buf : strdup("");
}

/* str.join over an array of strings; raises TypeError on a non-str element. Caller frees. */
static char *at2_join(at2_ctx *ctx, const json_t *arr, const char *sep) {
    at2_strbuf b = {NULL, 0, 0};
    size_t slen = strlen(sep);
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *item = arr->c.items[i];
        if (!at2_is_str(item)) {
            at2_raise_fmt(ctx, "TypeError", "sequence item %zu: expected str instance, %s found",
                          i, at2_type_name(item));
            free(b.buf);
            return NULL;
        }
        if (i) at2_sb_puts(&b, sep);
        at2_sb_puts(&b, item->str_val ? item->str_val : "");
        (void)slen;
    }
    return b.buf ? b.buf : strdup("");
}

/* ── transport-level flows ───────────────────────────────────────────────── */

/* fixed module constant _OAUTH_TOOL_NAME_REVERSE_ALIASES (agent/anthropic_adapter.py). */
static const char *at2_reverse_alias(const char *bare) {
    if (at2_str_eq(bare, "chat_history_lookup")) return "session_search";
    if (at2_str_eq(bare, "context_notes")) return "memory";
    return NULL;
}

/* PoP: at2_unprefix_oauth_tool_name @ agent/transports/anthropic.py:_unprefix_oauth_tool_name */
static json_t *at2_unprefix_oauth_tool_name(at2_ctx *ctx, const json_t *name) {
    if (!at2_is_str(name)) {
        at2_raise_fmt(ctx, "TypeError", "'%s' object is not subscriptable", at2_type_name(name));
        return NULL;
    }
    const char *nm = name->str_val ? name->str_val : "";
    size_t nlen = strlen(nm);
    const char *bare = nlen > 5 ? nm + 5 : ""; /* name[len("mcp__"):] */
    char *mcp_bare = (char *)malloc(strlen(bare) + 5);
    sprintf(mcp_bare, "mcp_%s", bare);
    const char *candidates[3] = {nm, mcp_bare, bare};
    for (int k = 0; k < 3; k++) {
        json_t *a = json_object();
        at2_arg_str(a, "name", candidates[k]);
        json_t *entry = at2_call(ctx, "registry.get_entry", a);
        if (at2_exc_pending(ctx)) { if (entry) json_free(entry); free(mcp_bare); return NULL; }
        bool hit = at2_truthy(entry);
        if (entry) json_free(entry);
        if (hit) { free(mcp_bare); return json_string(candidates[k]); }
    }
    free(mcp_bare);
    const char *alias = at2_reverse_alias(bare);
    return json_string(alias ? alias : nm);
}

/* PoP: at2_api_mode @ agent/transports/anthropic.py:api_mode */
static json_t *at2_api_mode(at2_ctx *ctx) {
    (void)ctx;
    return json_string("anthropic_messages");
}

/* PoP: at2_convert_messages @ agent/transports/anthropic.py:convert_messages */
static json_t *at2_convert_messages(at2_ctx *ctx, const json_t *messages, const json_t *kwargs) {
    json_t *a = json_object();
    at2_arg_node(a, "messages", messages);
    at2_arg_node(a, "base_url", at2_get(kwargs, "base_url"));
    at2_arg_node(a, "model", at2_get(kwargs, "model"));
    return at2_call(ctx, "convert_messages_to_anthropic", a);
}

/* PoP: at2_convert_tools @ agent/transports/anthropic.py:convert_tools */
static json_t *at2_convert_tools(at2_ctx *ctx, const json_t *tools) {
    json_t *a = json_object();
    at2_arg_node(a, "tools", tools);
    return at2_call(ctx, "convert_tools_to_anthropic", a);
}

/* PoP: at2_build_kwargs @ agent/transports/anthropic.py:build_kwargs */
static json_t *at2_build_kwargs(at2_ctx *ctx, const json_t *model, const json_t *messages,
                                const json_t *tools, const json_t *params) {
    /* _BUILD_KWARG_DEFAULTS, in insertion order. */
    json_t *defs = json_object();
    json_set(defs, "max_tokens", json_number(16384));
    json_set(defs, "reasoning_config", json_null());
    json_set(defs, "tool_choice", json_null());
    json_set(defs, "is_oauth", json_bool(false));
    json_set(defs, "preserve_dots", json_bool(false));
    json_set(defs, "context_length", json_null());
    json_set(defs, "base_url", json_null());
    json_set(defs, "fast_mode", json_bool(false));
    json_set(defs, "drop_context_1m_beta", json_bool(false));
    json_t *a = json_object();
    at2_arg_node(a, "model", model);
    at2_arg_node(a, "messages", messages);
    at2_arg_node(a, "tools", tools);
    for (size_t i = 0; i < defs->c.count; i++) {
        const char *k = defs->c.keys[i];
        json_t *v = at2_get(params, k);
        if (!v) v = defs->c.items[i];
        at2_arg_node(a, k, v);
    }
    json_free(defs);
    return at2_call(ctx, "build_anthropic_kwargs", a);
}

/* PoP: at2_map_finish_reason @ agent/transports/anthropic.py:map_finish_reason */
static json_t *at2_map_finish_reason(at2_ctx *ctx, const json_t *raw) {
    /* base.py: raw_reason if _STOP_REASON_MAP is None else map.get(raw_reason, "stop").
     * AnthropicTransport._STOP_REASON_MAP is the fixed dict (never None). */
    if (at2_is_arr(raw) || at2_is_obj(raw)) {
        at2_raise_fmt(ctx, "TypeError",
                      "cannot use '%s' as a dict key (unhashable type: '%s')",
                      at2_type_name(raw), at2_type_name(raw));
        return NULL;
    }
    const char *r = at2_is_str(raw) ? raw->str_val : "";
    static const char *KEYS[] = {"end_turn", "tool_use", "max_tokens", "stop_sequence",
                                 "refusal", "model_context_window_exceeded"};
    static const char *VALS[] = {"stop", "tool_calls", "length", "stop",
                                 "content_filter", "length"};
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++)
        if (at2_str_eq(r, KEYS[i])) return json_string(VALS[i]);
    return json_string("stop");
}

/* PoP: at2_response_finish_reason @ agent/transports/anthropic.py:response_finish_reason */
static json_t *at2_response_finish_reason(at2_ctx *ctx, const json_t *response) {
    json_t *a = json_object();
    at2_arg_node(a, "response", response);
    json_t *g = at2_call(ctx, "guardrail_intervened", a);
    if (at2_exc_pending(ctx)) { if (g) json_free(g); return NULL; }
    bool intervened = at2_truthy(g);
    if (g) json_free(g);
    if (intervened) return json_string("content_filter");
    json_t *sr = at2_get(response, "stop_reason");
    json_t *ma = json_object();
    at2_arg_node(ma, "raw_reason", sr);
    json_t *r = at2_call(ctx, "map_finish_reason", ma);
    if (at2_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: at2_validate_response @ agent/transports/anthropic.py:validate_response */
static json_t *at2_validate_response(at2_ctx *ctx, const json_t *response) {
    (void)ctx;
    json_t *content = at2_get(response, "content");
    if (!at2_is_arr(content)) return json_bool(false);
    if (content->c.count > 0) return json_bool(true);
    json_t *sr = at2_get(response, "stop_reason");
    if (at2_is_str(sr) && (at2_str_eq(sr->str_val, "end_turn") || at2_str_eq(sr->str_val, "refusal")))
        return json_bool(true);
    return json_bool(false);
}

/* PoP: at2_extract_cache_stats @ agent/transports/anthropic.py:extract_cache_stats */
static json_t *at2_extract_cache_stats(at2_ctx *ctx, const json_t *response) {
    (void)ctx;
    json_t *usage = at2_get(response, "usage");
    if (at2_is_none(usage)) return json_null();
    json_t *cached = at2_get(usage, "cache_read_input_tokens");
    bool ct = at2_truthy(cached);
    json_t *written = at2_get(usage, "cache_creation_input_tokens");
    bool wt = at2_truthy(written);
    if (!ct && !wt) return json_null();
    json_t *o = json_object();
    json_set(o, "cached_tokens", ct ? json_copy(cached) : json_number(0));
    json_set(o, "creation_tokens", wt ? json_copy(written) : json_number(0));
    return o;
}

/* PoP: at2_normalize_response @ agent/transports/anthropic.py:normalize_response */
static json_t *at2_normalize_response(at2_ctx *ctx, const json_t *response, const json_t *kwargs) {
    bool strip = at2_truthy(at2_get_def(kwargs, "strip_tool_prefix", json_bool(false)));
    json_t *text_parts = json_array();
    json_t *reasoning_parts = json_array();
    json_t *reasoning_details = json_array();
    json_t *tool_calls = json_array();
    json_t *ordered_blocks = json_array();
    json_t *provider_data = NULL;

    json_t *content = at2_get(response, "content");
    if (at2_is_arr(content)) {
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *block = content->c.items[i];
            /* block_dict = _to_plain_data(block) */
            json_t *pa = json_object();
            at2_arg_node(pa, "value", block);
            json_t *plain = at2_call(ctx, "to_plain_data", pa);
            if (at2_exc_pending(ctx)) {
                if (plain) json_free(plain);
                goto norm_fail;
            }
            /* clean_block = _sanitize_replay_block(block_dict) if isinstance(block_dict, dict) else None */
            json_t *clean = NULL;
            if (at2_is_obj(plain)) {
                json_t *sa = json_object();
                at2_arg_node(sa, "b", plain);
                clean = at2_call(ctx, "sanitize_replay_block", sa);
                if (at2_exc_pending(ctx)) {
                    if (clean) json_free(clean);
                    if (plain) json_free(plain);
                    goto norm_fail;
                }
            }
            if (!at2_is_none(clean)) json_append(ordered_blocks, json_copy(clean));

            json_t *bt = at2_get(block, "type"); /* block.type */
            bool is_text = at2_is_str(bt) && at2_str_eq(bt->str_val, "text");
            bool is_thinking = at2_is_str(bt) &&
                (at2_str_eq(bt->str_val, "thinking") || at2_str_eq(bt->str_val, "redacted_thinking"));
            bool is_tool_use = at2_is_str(bt) && at2_str_eq(bt->str_val, "tool_use");

            if (is_text) {
                json_append(text_parts, json_copy(at2_get(block, "text")));
            } else if (is_thinking) {
                if (at2_str_eq(bt->str_val, "thinking"))
                    json_append(reasoning_parts, json_copy(at2_get(block, "thinking")));
                json_t *detail = !at2_is_none(clean) ? clean : plain;
                if (at2_is_obj(detail)) json_append(reasoning_details, json_copy(detail));
            } else if (is_tool_use) {
                json_t *name = at2_get(block, "name");
                if (strip && at2_is_str(name) && strncmp(name->str_val, "mcp__", 5) == 0) {
                    json_t *un = at2_unprefix_oauth_tool_name(ctx, name);
                    if (at2_exc_pending(ctx)) {
                        if (un) json_free(un);
                        if (clean) json_free(clean);
                        if (plain) json_free(plain);
                        goto norm_fail;
                    }
                    name = un;  /* owned result from the seam; borrowed name is left untouched */
                } else {
                    if (strip && !at2_is_str(name)) {
                        at2_raise_fmt(ctx, "AttributeError", "'%s' object has no attribute 'startswith'",
                                      at2_type_name(name));
                        if (clean) json_free(clean);
                        if (plain) json_free(plain);
                        goto norm_fail;
                    }
                    name = name ? json_copy(name) : json_null();
                }
                char *dumped = at2_json_dumps(at2_get(block, "input"));
                json_t *tc = json_object();
                json_t *idv = at2_get(block, "id");
                json_set(tc, "id", idv ? json_copy(idv) : json_null());
                json_set(tc, "name", name ? name : json_null());
                json_set(tc, "arguments", json_string(dumped));
                json_set(tc, "provider_data", json_null());
                free(dumped);
                json_append(tool_calls, tc);
                name = NULL;
            }
            if (clean) json_free(clean);
            if (plain) json_free(plain);
        }
    }

    /* provider_data = {"reasoning_details": reasoning_details} if reasoning_details else {} */
    provider_data = json_object();
    if (reasoning_details->c.count > 0)
        json_set(provider_data, "reasoning_details", json_copy(reasoning_details));

    /* stop_details = _to_plain_data(getattr(response, "stop_details", None)) */
    json_t *pa = json_object();
    at2_arg_node(pa, "value", at2_get(response, "stop_details"));
    json_t *stop_details = at2_call(ctx, "to_plain_data", pa);
    if (at2_exc_pending(ctx)) {
        if (stop_details) json_free(stop_details);
        goto norm_fail;
    }
    if (!at2_is_none(stop_details)) {
        json_set(provider_data, "stop_details", json_copy(stop_details));
    }
    if (stop_details) json_free(stop_details);

    /* signed = any(b.get("type") in _THINKING_TYPES and (b.get("signature") or b.get("data"))
     *           for b in ordered_blocks) */
    bool signed_any = false;
    for (size_t i = 0; i < ordered_blocks->c.count; i++) {
        json_t *b = ordered_blocks->c.items[i];
        json_t *bt = at2_get(b, "type");
        bool thinking = at2_is_str(bt) &&
            (at2_str_eq(bt->str_val, "thinking") || at2_str_eq(bt->str_val, "redacted_thinking"));
        if (!thinking) continue;
        if (at2_truthy(at2_get(b, "signature")) || at2_truthy(at2_get(b, "data"))) {
            signed_any = true;
            break;
        }
    }
    if (signed_any) {
        for (size_t i = 0; i < ordered_blocks->c.count; i++) {
            json_t *bt = at2_get(ordered_blocks->c.items[i], "type");
            if (at2_is_str(bt) && at2_str_eq(bt->str_val, "tool_use")) {
                json_set(provider_data, "anthropic_content_blocks", json_copy(ordered_blocks));
                break;
            }
        }
    }

    /* return NormalizedResponse(content=join, tool_calls=tool_calls or None,
     *   finish_reason=self.response_finish_reason(response), reasoning=join,
     *   usage=None, provider_data=provider_data or None) */
    json_t *out = json_object();
    if (text_parts->c.count > 0) {
        char *s = at2_join(ctx, text_parts, "\n");
        if (at2_exc_pending(ctx)) { free(s); json_free(out); goto norm_fail; }
        json_set(out, "content", json_string(s ? s : ""));
        free(s);
    } else {
        json_set(out, "content", json_null());
    }
    if (tool_calls->c.count > 0) json_set(out, "tool_calls", tool_calls);
    else { json_set(out, "tool_calls", json_null()); json_free(tool_calls); }
    tool_calls = NULL;

    json_t *fr = at2_response_finish_reason(ctx, response);
    if (at2_exc_pending(ctx)) { if (fr) json_free(fr); json_free(out); goto norm_fail; }
    json_set(out, "finish_reason", fr ? fr : json_null());

    if (reasoning_parts->c.count > 0) {
        char *s = at2_join(ctx, reasoning_parts, "\n\n");
        if (at2_exc_pending(ctx)) { free(s); json_free(out); goto norm_fail; }
        json_set(out, "reasoning", json_string(s ? s : ""));
        free(s);
    } else {
        json_set(out, "reasoning", json_null());
    }
    json_set(out, "usage", json_null());
    json_set(out, "provider_data", provider_data->c.count > 0 ? provider_data : json_null());
    if (provider_data->c.count > 0) provider_data = NULL; else json_free(provider_data);

    json_free(reasoning_details);
    json_free(ordered_blocks);
    json_free(text_parts);
    json_free(reasoning_parts);
    if (tool_calls) json_free(tool_calls);
    return out;

norm_fail:
    if (provider_data) json_free(provider_data);
    json_free(reasoning_details);
    json_free(ordered_blocks);
    json_free(text_parts);
    json_free(reasoning_parts);
    if (tool_calls) json_free(tool_calls);
    return NULL;
}
