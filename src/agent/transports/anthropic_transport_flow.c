/* anthropic_transport_flow.c — FLOW + leaf port of the anthropic Messages-API
 * transport surface: the UNCLAIMED remainder of agent/transports/anthropic.py
 * (_unprefix_oauth_tool_name, response_finish_reason) plus the UNCLAIMED leaves
 * and replay/conversion flows of its conversion engine agent/anthropic_message_convert.py.
 * The CLAIMED siblings (api_mode/convert_messages/convert_tools/build_kwargs/
 * normalize_response/validate_response/extract_cache_stats/map_finish_reason on the
 * transport, and _sanitize_tool_id/_sanitize_replay_block/_to_plain_data/
 * _extract_preserved_thinking_blocks/_content_parts_to_anthropic_blocks/
 * _apply_assistant_cache_control_to_last_cacheable_block in the converter, plus
 * bedrock_adapter.anthropic_response_guardrail_intervened) are SEAMED here.
 *
 * Decision-sequence port: ALL collaborator calls ride the injected universal seam
 * ant_call; the harness scripts results and records the ordered trace the oracle
 * diffs against LIVE quarry defs under the same scripts. Ported functions call each
 * other REAL (internal composition). libjson-only; self-contained (the harness
 * #includes "agent/transports/anthropic_transport_flow.c"). Every function is static
 * ant_* so it never collides with the stub-era transports_anthropic_ports.c symbols.
 *
 * Seam name -> live call (args projection; None-valued keys dropped both sides):
 *   sanitize_tool_id                  _sanitize_tool_id(tool_id)                 {tool_id}
 *   sanitize_replay_block             _sanitize_replay_block(b)                  {b}
 *   extract_preserved_thinking_blocks _extract_preserved_thinking_blocks(message){message}
 *   content_parts_to_anthropic_blocks _content_parts_to_anthropic_blocks(parts)  {parts}
 *   apply_assistant_cache_control     _apply_assistant_cache_control_to_last_cacheable_block(blocks, cc)
 *                                     {blocks, cache_control}  (STATE-EFFECT seam, see below)
 *   map_finish_reason                 self.map_finish_reason(raw_reason)         {raw_reason}
 *   guardrail_intervened              anthropic_response_guardrail_intervened(response){response}
 *   registry.get_entry                registry.get_entry(name)                   {name}
 *
 * apply_assistant_cache_control is a claimed sibling whose live body MUTATES blocks
 * (setdefault cache_control on the last text/tool_use block, reversed). It is seamed
 * (recorded) AND its documented state effect is applied identically by the C port
 * (ant_apply_cc_effect after the call) and the oracle recorder — same algorithm both
 * sides, so the mutated ret matches. The seam args snapshot blocks BEFORE the effect.
 *
 * Representation contract (shared with tests/oracle_anthropic_transport_flow.py):
 *   - None -> JSON null; tuple/set -> array (sets sorted by str()); generator -> array.
 *   - integral floats collapse to ints (Python == equates them); dict key order mirrors
 *     live insertion order (json_set replaces in place).
 *   - uncaught exception -> {"__raise__": cls, "msg": str(exc)}; a scripted seam raise
 *     parks the marker VERBATIM. BaseException trio (SystemExit/KeyboardInterrupt/
 *     GeneratorExit) propagates through every site (no except Exception in this set;
 *     _parse_tool_args catches only JSONDecodeError/ValueError around json.loads).
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     {"$seq":[...]} per-call queue (exhaustion -> RuntimeError("script exhausted: <name>"));
 *     {"__raise__":cls,"msg":m} raises (trace ret is the WHOLE marker); scripted null -> None;
 *     any other value reused.
 *
 * Fixture domain (documented exclusions, ASCII scope):
 *   - _parse_tool_args: strict Python json.loads grammar. NaN/Infinity, lone \uD800-\uDFFF
 *     surrogates, integers beyond 2^53, and nesting deeper than 64 are OUT of domain
 *     (Python accepts NaN/lone-surrogate/big-int; libjson double cannot represent them).
 *     Surrogate PAIRS combine; invalid JSON -> {}.
 *   - str() operands (_tool_result_content text_summary) are str/int scalars; .strip()
 *     uses the full Unicode White_Space set (ant_nonws). Block "type" values are hashable
 *     scalars (str/null/int/bool/float) where tested against frozensets; _has_block_type
 *     `types` arg is a list (== membership, no hash). _block_ids key values are str/null.
 *   - response.stop_reason is always present (response_finish_reason); response is an
 *     attribute dict projected identically both sides.
 *   - sanitize_replay_block seam results are dict-or-null (clean.get reads); container
 *     seam results are deep-copied per call (json_copy) so in-place mutation never aliases
 *     the fixture.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct ant_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} ant_ctx;

static bool ant_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool ant_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ant_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ant_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ant_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ant_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ant_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool ant_truthy(const json_t *v) {
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

static const char *ant_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* borrowed object field or NULL (absent). present-null returns the JSON_NULL node. */
static json_t *ant_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present value (incl null) passes through; absent -> default.
 * Returns BORROWED node. */
static json_t *ant_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None. */
static bool ant_json_eq(const json_t *a, const json_t *b) {
    if (ant_is_none(a) && ant_is_none(b)) return true;
    if (ant_is_none(a) || ant_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return ant_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ant_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !ant_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* Python type name for a JSON node (fixture domain). */
static const char *ant_type_name(const json_t *v) {
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

/* Python repr(float): shortest decimal that round-trips, positional for decimal
 * exponent in [-4, 16), scientific otherwise (e.g. 0.1, 1e+16, 1e-05). Caller frees. */
static char *ant_float_repr(double d) {
    char sci[64];
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(sci, sizeof sci, "%.*e", prec - 1, d);
        if (strtod(sci, NULL) == d) break;
    }
    /* parse sci: [-]D[.DDD]e[+-]XX */
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
    /* strip trailing zeros (keep >=1 digit) */
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

/* str(v) for a scalar (null -> "None"). Caller frees. Domain: str/int/bool/float/null. */
static char *ant_pystr(const json_t *v) {
    if (ant_is_none(v)) return strdup("None");
    if (ant_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (ant_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else return ant_float_repr(d);
        return strdup(buf);
    }
    if (ant_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* ── Unicode White_Space scan (str.strip / str.isspace) ──────────────────── */

/* Decode one UTF-8 codepoint at s; advance *len. Returns codepoint (or 0xFFFD). */
static unsigned int ant_utf8_decode(const char *s, size_t *len) {
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
static bool ant_codepoint_ws(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return true;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return true;
    return false;
}
/* true if s contains at least one non-White_Space codepoint (== bool(s.strip())). */
static bool ant_nonws(const char *s) {
    if (!s) return false;
    size_t i = 0, n = strlen(s);
    while (i < n) {
        size_t l; unsigned int cp = ant_utf8_decode(s + i, &l);
        if (!ant_codepoint_ws(cp)) return true;
        i += l;
    }
    return false;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool ant_exc_pending(const ant_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void ant_raise(ant_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static void ant_raise_fmt(ant_ctx *ctx, const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ant_raise(ctx, cls, buf);
}
static const char *ant_exc_class(const ant_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* AttributeError '<T>' object has no attribute '<name>' on a non-dict carrier. */
static void ant_raise_attr_get(ant_ctx *ctx, const json_t *v) {
    ant_raise_fmt(ctx, "AttributeError", "'%s' object has no attribute 'get'", ant_type_name(v));
}
/* TypeError '<T>' object is not iterable. */
static void ant_raise_not_iterable(ant_ctx *ctx, const json_t *v) {
    ant_raise_fmt(ctx, "TypeError", "'%s' object is not iterable", ant_type_name(v));
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW (caller frees). */
static json_t *ant_call(ant_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *chosen = NULL;
    json_t *fc_marker = NULL;   /* owned fail-closed / exhaustion marker */
    if (!script) {              /* MISSING key -> fail-closed */
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
static void ant_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void ant_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!ant_is_none(val)) json_set(args, key, json_copy(val));
}

/* ── pure block leaves ───────────────────────────────────────────────────── */

/* PoP: ant_block_type @ agent/anthropic_message_convert.py:_block_type */
static json_t *ant_block_type(const json_t *b) {
    return ant_is_obj(b) ? json_obj_get(b, "type") : NULL;
}

/* PoP: ant_cache_control_of @ agent/anthropic_message_convert.py:_cache_control_of */
static json_t *ant_cache_control_of(const json_t *b) {
    if (!ant_is_obj(b)) return NULL;
    json_t *cc = json_obj_get(b, "cache_control");
    return ant_is_obj(cc) ? cc : NULL;
}

/* PoP: ant_is_blank_text_block @ agent/anthropic_message_convert.py:_is_blank_text_block */
static bool ant_is_blank_text_block(const json_t *b) {
    json_t *bt = ant_block_type(b);
    if (!(bt && bt->type == JSON_STRING && ant_str_eq(bt->str_val, "text"))) return false;
    json_t *text = json_obj_get(b, "text");
    if (!ant_is_str(text)) return true;
    return !ant_nonws(text->str_val);
}

/* PoP: ant_text_block @ agent/anthropic_message_convert.py:_text_block */
static json_t *ant_text_block(const json_t *text) {
    json_t *o = json_object();
    json_set(o, "type", json_string("text"));
    json_set(o, "text", text ? json_copy(text) : json_null());
    return o;
}

/* PoP: ant_text_block_with_citations @ agent/anthropic_message_convert.py:_text_block_with_citations */
static json_t *ant_text_block_with_citations(const json_t *text, const json_t *cits) {
    json_t *block = ant_text_block(text);
    if (ant_is_arr(cits) && cits->c.count > 0)
        json_set(block, "citations", json_copy(cits));
    return block;
}

/* PoP: ant_image_source_data @ agent/anthropic_message_convert.py:_image_source_data */
static json_t *ant_image_source_data(const json_t *block) {
    if (!ant_is_obj(block)) return NULL;
    json_t *source = json_obj_get(block, "source");
    if (!ant_is_obj(source)) return NULL;
    json_t *data = json_obj_get(source, "data");
    return ant_is_str(data) ? data : NULL;
}

/* membership of a block-type value in a LIST of types (== semantics, no hash). */
static bool ant_type_in_list(const json_t *bt, const json_t *types) {
    if (!ant_is_arr(types)) return false;
    for (size_t i = 0; i < types->c.count; i++)
        if (ant_json_eq(bt, types->c.items[i])) return true;
    return false;
}

/* PoP: ant_has_block_type @ agent/anthropic_message_convert.py:_has_block_type */
static bool ant_has_block_type(ant_ctx *ctx, const json_t *blocks, const json_t *types) {
    if (!ant_is_arr(blocks)) { ant_raise_not_iterable(ctx, blocks); return false; }
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *bt = ant_block_type(blocks->c.items[i]);
        if (ant_type_in_list(bt, types)) return true;
    }
    return false;
}

/* PoP: ant_strip_thinking @ agent/anthropic_message_convert.py:_strip_thinking */
static json_t *ant_strip_thinking(ant_ctx *ctx, const json_t *blocks) {
    if (!ant_is_arr(blocks)) { ant_raise_not_iterable(ctx, blocks); return NULL; }
    static const char *TH[] = {"thinking", "redacted_thinking"};
    json_t *out = json_array();
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *b = blocks->c.items[i];
        json_t *bt = ant_block_type(b);
        bool thinking = false;
        if (bt && bt->type == JSON_STRING)
            for (int k = 0; k < 2; k++) if (ant_str_eq(bt->str_val, TH[k])) thinking = true;
        if (!thinking) json_append(out, json_copy(b));
    }
    return out;
}

/* ── strict Python json.loads (grammar pre-validation + value build) ─────── */

typedef struct { const char *s; size_t i, n; int depth; bool bad; } ant_jparse;

static void ant_jp_ws(ant_jparse *p) {
    while (p->i < p->n) {
        char c = p->s[p->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->i++;
        else break;
    }
}
static json_t *ant_jp_value(ant_jparse *p);

static int ant_hex4(ant_jparse *p, unsigned int *out) {
    if (p->i + 4 > p->n) return 0;
    unsigned int v = 0;
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i + k];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    p->i += 4; *out = v; return 1;
}
/* append a codepoint as UTF-8 to a growing buffer */
static void ant_utf8_append(char **buf, size_t *len, size_t *cap, unsigned int cp) {
    char tmp[4]; int n;
    if (cp < 0x80) { tmp[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { tmp[0] = (char)(0xC0 | (cp >> 6)); tmp[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000) { tmp[0] = (char)(0xE0 | (cp >> 12)); tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else { tmp[0] = (char)(0xF0 | (cp >> 18)); tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    if (*len + (size_t)n + 1 > *cap) {
        *cap = (*cap ? *cap * 2 : 64);
        while (*len + (size_t)n + 1 > *cap) *cap *= 2;
        *buf = (char *)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, tmp, (size_t)n); *len += (size_t)n; (*buf)[*len] = '\0';
}
static json_t *ant_jp_string(ant_jparse *p) {
    /* assumes current char is '"' */
    p->i++; /* opening quote */
    char *buf = NULL; size_t len = 0, cap = 0;
    ant_utf8_append(&buf, &len, &cap, 0); len = 0; buf[0] = '\0'; /* alloc */
    while (p->i < p->n) {
        unsigned char c = (unsigned char)p->s[p->i];
        if (c == '"') { p->i++; json_t *r = json_string(buf ? buf : ""); free(buf); return r; }
        if (c == '\\') {
            p->i++;
            if (p->i >= p->n) break;
            char e = p->s[p->i++];
            switch (e) {
                case '"': ant_utf8_append(&buf, &len, &cap, '"'); break;
                case '\\': ant_utf8_append(&buf, &len, &cap, '\\'); break;
                case '/': ant_utf8_append(&buf, &len, &cap, '/'); break;
                case 'b': ant_utf8_append(&buf, &len, &cap, '\b'); break;
                case 'f': ant_utf8_append(&buf, &len, &cap, '\f'); break;
                case 'n': ant_utf8_append(&buf, &len, &cap, '\n'); break;
                case 'r': ant_utf8_append(&buf, &len, &cap, '\r'); break;
                case 't': ant_utf8_append(&buf, &len, &cap, '\t'); break;
                case 'u': {
                    unsigned int cp;
                    if (!ant_hex4(p, &cp)) { p->bad = true; free(buf); return NULL; }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        /* high surrogate: require \uXXXX low surrogate (pair only; lone out of domain) */
                        if (p->i + 2 > p->n || p->s[p->i] != '\\' || p->s[p->i + 1] != 'u') { p->bad = true; free(buf); return NULL; }
                        p->i += 2;
                        unsigned int lo;
                        if (!ant_hex4(p, &lo) || lo < 0xDC00 || lo > 0xDFFF) { p->bad = true; free(buf); return NULL; }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        p->bad = true; free(buf); return NULL; /* lone low surrogate out of domain */
                    }
                    ant_utf8_append(&buf, &len, &cap, cp);
                    break;
                }
                default: p->bad = true; free(buf); return NULL; /* invalid \escape */
            }
        } else if (c < 0x20) {
            p->bad = true; free(buf); return NULL; /* raw control char (strict) */
        } else {
            /* raw byte (UTF-8 lead/continuation passed through verbatim) */
            if (len + 2 > cap) { cap = cap ? cap * 2 : 64; buf = (char *)realloc(buf, cap); }
            buf[len++] = (char)c; buf[len] = '\0'; p->i++;
        }
    }
    p->bad = true; free(buf); return NULL; /* unterminated */
}
static json_t *ant_jp_number(ant_jparse *p) {
    size_t start = p->i;
    if (p->i < p->n && p->s[p->i] == '-') p->i++;
    /* int part: 0 | [1-9][0-9]* */
    if (p->i >= p->n) { p->bad = true; return NULL; }
    if (p->s[p->i] == '0') { p->i++; }
    else if (p->s[p->i] >= '1' && p->s[p->i] <= '9') {
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    } else { p->bad = true; return NULL; }
    /* frac */
    if (p->i < p->n && p->s[p->i] == '.') {
        p->i++;
        if (p->i >= p->n || p->s[p->i] < '0' || p->s[p->i] > '9') { p->bad = true; return NULL; }
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    }
    /* exp */
    if (p->i < p->n && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
        p->i++;
        if (p->i < p->n && (p->s[p->i] == '+' || p->s[p->i] == '-')) p->i++;
        if (p->i >= p->n || p->s[p->i] < '0' || p->s[p->i] > '9') { p->bad = true; return NULL; }
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    }
    size_t len = p->i - start;
    char *tmp = (char *)malloc(len + 1);
    memcpy(tmp, p->s + start, len); tmp[len] = '\0';
    double d = strtod(tmp, NULL);
    free(tmp);
    return json_number(d);
}
static json_t *ant_jp_value(ant_jparse *p) {
    ant_jp_ws(p);
    if (p->i >= p->n) { p->bad = true; return NULL; }
    if (p->depth > 64) { p->bad = true; return NULL; }
    char c = p->s[p->i];
    if (c == '"') return ant_jp_string(p);
    if (c == '{') {
        p->i++; p->depth++;
        json_t *o = json_object();
        ant_jp_ws(p);
        if (p->i < p->n && p->s[p->i] == '}') { p->i++; p->depth--; return o; }
        for (;;) {
            ant_jp_ws(p);
            if (p->i >= p->n || p->s[p->i] != '"') { p->bad = true; json_free(o); return NULL; }
            json_t *key = ant_jp_string(p);
            if (!key) { json_free(o); return NULL; }
            ant_jp_ws(p);
            if (p->i >= p->n || p->s[p->i] != ':') { p->bad = true; json_free(key); json_free(o); return NULL; }
            p->i++;
            json_t *val = ant_jp_value(p);
            if (!val) { json_free(key); json_free(o); return NULL; }
            json_set(o, key->str_val, val); /* takes ownership of val; last wins, first position */
            json_free(key);
            ant_jp_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->n && p->s[p->i] == '}') { p->i++; p->depth--; return o; }
            p->bad = true; json_free(o); return NULL;
        }
    }
    if (c == '[') {
        p->i++; p->depth++;
        json_t *a = json_array();
        ant_jp_ws(p);
        if (p->i < p->n && p->s[p->i] == ']') { p->i++; p->depth--; return a; }
        for (;;) {
            json_t *val = ant_jp_value(p);
            if (!val) { json_free(a); return NULL; }
            json_append(a, val);
            ant_jp_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->n && p->s[p->i] == ']') { p->i++; p->depth--; return a; }
            p->bad = true; json_free(a); return NULL;
        }
    }
    if (c == '-' || (c >= '0' && c <= '9')) return ant_jp_number(p);
    if (p->n - p->i >= 4 && strncmp(p->s + p->i, "true", 4) == 0) { p->i += 4; return json_bool(true); }
    if (p->n - p->i >= 5 && strncmp(p->s + p->i, "false", 5) == 0) { p->i += 5; return json_bool(false); }
    if (p->n - p->i >= 4 && strncmp(p->s + p->i, "null", 4) == 0) { p->i += 4; return json_null(); }
    p->bad = true; return NULL; /* NaN/Infinity/+1/.1 etc. out of domain */
}
/* Strict json.loads: returns NEW json_t or NULL on any grammar error. */
static json_t *ant_json_loads(const char *s) {
    if (!s) return NULL;
    ant_jparse p = {s, 0, strlen(s), 0, false};
    json_t *v = ant_jp_value(&p);
    if (!v || p.bad) { if (v) json_free(v); return NULL; }
    ant_jp_ws(&p);
    if (p.i != p.n) { json_free(v); return NULL; } /* Extra data */
    return v;
}

/* PoP: ant_parse_tool_args @ agent/anthropic_message_convert.py:_parse_tool_args */
static json_t *ant_parse_tool_args(const json_t *raw) {
    if (!ant_is_str(raw)) return raw ? json_copy(raw) : json_null();
    json_t *v = ant_json_loads(raw->str_val);
    if (!v) return json_object(); /* bad JSON -> {} */
    return v;
}

/* ── Python-style json.dumps (ensure_ascii=True, ", "/": " separators) ───── */

typedef struct { char *buf; size_t len, cap; } ant_strbuf;
static void ant_sb_putc(ant_strbuf *b, char c) {
    if (b->len + 2 > b->cap) { b->cap = b->cap ? b->cap * 2 : 128; b->buf = (char *)realloc(b->buf, b->cap); }
    b->buf[b->len++] = c; b->buf[b->len] = '\0';
}
static void ant_sb_puts(ant_strbuf *b, const char *s) { while (*s) ant_sb_putc(b, *s++); }
static void ant_sb_quote(ant_strbuf *b, const char *s) {
    ant_sb_putc(b, '"');
    size_t i = 0, n = strlen(s);
    while (i < n) {
        size_t l; unsigned int cp = ant_utf8_decode(s + i, &l);
        if (cp == '"') ant_sb_puts(b, "\\\"");
        else if (cp == '\\') ant_sb_puts(b, "\\\\");
        else if (cp == '\b') ant_sb_puts(b, "\\b");
        else if (cp == '\f') ant_sb_puts(b, "\\f");
        else if (cp == '\n') ant_sb_puts(b, "\\n");
        else if (cp == '\r') ant_sb_puts(b, "\\r");
        else if (cp == '\t') ant_sb_puts(b, "\\t");
        else if (cp < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", cp); ant_sb_puts(b, e); }
        else if (cp < 0x80) ant_sb_putc(b, (char)cp);
        else if (cp <= 0xFFFF) { char e[8]; snprintf(e, sizeof e, "\\u%04x", cp); ant_sb_puts(b, e); }
        else {
            /* surrogate pair for astral (ensure_ascii) */
            unsigned int v = cp - 0x10000;
            unsigned int hi = 0xD800 + (v >> 10), lo = 0xDC00 + (v & 0x3FF);
            char e[16]; snprintf(e, sizeof e, "\\u%04x\\u%04x", hi, lo); ant_sb_puts(b, e);
        }
        i += l;
    }
    ant_sb_putc(b, '"');
}
static void ant_sb_node(ant_strbuf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { ant_sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: ant_sb_puts(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64]; double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) { snprintf(tmp, sizeof tmp, "%.0f", d); ant_sb_puts(b, tmp); }
            else { char *fr = ant_float_repr(d); ant_sb_puts(b, fr); free(fr); }
            break;
        }
        case JSON_STRING: ant_sb_quote(b, v->str_val ? v->str_val : ""); break;
        case JSON_ARRAY:
            ant_sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ant_sb_puts(b, ", ");
                ant_sb_node(b, v->c.items[i]);
            }
            ant_sb_putc(b, ']'); break;
        case JSON_OBJECT:
            ant_sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ant_sb_puts(b, ", ");
                ant_sb_quote(b, v->c.keys[i]);
                ant_sb_puts(b, ": ");
                ant_sb_node(b, v->c.items[i]);
            }
            ant_sb_putc(b, '}'); break;
        default: ant_sb_puts(b, "null"); break;
    }
}
/* json.dumps(v) with Python defaults. Caller frees. */
static char *ant_json_dumps(const json_t *v) {
    ant_strbuf b = {NULL, 0, 0};
    ant_sb_node(&b, v);
    return b.buf ? b.buf : strdup("");
}

/* ── set projection (sorted by str(), dedup by equality) ─────────────────── */

static int ant_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* Build a sorted, deduped array from a list of value nodes (set projection). */
static json_t *ant_set_sorted(json_t **vals, size_t n) {
    /* dedup by ant_json_eq */
    json_t **uniq = (json_t **)malloc((n ? n : 1) * sizeof(json_t *));
    size_t un = 0;
    for (size_t i = 0; i < n; i++) {
        bool dup = false;
        for (size_t j = 0; j < un; j++) if (ant_json_eq(vals[i], uniq[j])) { dup = true; break; }
        if (!dup) uniq[un++] = vals[i];
    }
    /* sort by str projection */
    char **keys = (char **)malloc((un ? un : 1) * sizeof(char *));
    for (size_t i = 0; i < un; i++) keys[i] = ant_pystr(uniq[i]);
    /* insertion sort indices by keys (stable enough; unique str keys mostly) */
    size_t *idx = (size_t *)malloc((un ? un : 1) * sizeof(size_t));
    for (size_t i = 0; i < un; i++) idx[i] = i;
    for (size_t i = 1; i < un; i++) {
        size_t cur = idx[i]; size_t j = i;
        while (j > 0 && strcmp(keys[idx[j - 1]], keys[cur]) > 0) { idx[j] = idx[j - 1]; j--; }
        idx[j] = cur;
    }
    json_t *arr = json_array();
    for (size_t i = 0; i < un; i++) {
        /* json_append drops a NULL item; a None set element must survive as null. */
        json_t *cv = uniq[idx[i]] ? json_copy(uniq[idx[i]]) : json_null();
        json_append(arr, cv);
    }
    for (size_t i = 0; i < un; i++) free(keys[i]);
    free(keys); free(idx); free(uniq);
    return arr;
}

/* PoP: ant_block_ids @ agent/anthropic_message_convert.py:_block_ids */
static json_t *ant_block_ids(ant_ctx *ctx, const json_t *blocks, const json_t *btype, const char *key) {
    if (!ant_is_arr(blocks)) { ant_raise_not_iterable(ctx, blocks); return NULL; }
    json_t **vals = (json_t **)malloc((blocks->c.count ? blocks->c.count : 1) * sizeof(json_t *));
    size_t n = 0;
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *b = blocks->c.items[i];
        json_t *bt = ant_block_type(b);
        if (!ant_json_eq(bt, btype)) continue;
        if (!ant_is_obj(b)) { ant_raise_attr_get(ctx, b); free(vals); return NULL; }
        vals[n++] = json_obj_get(b, key); /* borrowed; may be NULL (None) */
    }
    json_t *arr = ant_set_sorted(vals, n);
    free(vals);
    return arr;
}

/* PoP: ant_assistant_block_lists @ agent/anthropic_message_convert.py:_assistant_block_lists */
static json_t *ant_assistant_block_lists(ant_ctx *ctx, const json_t *result) {
    if (!ant_is_arr(result)) { ant_raise_not_iterable(ctx, result); return NULL; }
    json_t *out = json_array();
    for (size_t i = 0; i < result->c.count; i++) {
        json_t *m = result->c.items[i];
        if (!ant_is_obj(m)) { ant_raise_attr_get(ctx, m); json_free(out); return NULL; }
        json_t *role = json_obj_get(m, "role");
        if (!(role && role->type == JSON_STRING && ant_str_eq(role->str_val, "assistant"))) continue;
        json_t *content = json_obj_get(m, "content");
        if (!ant_is_arr(content)) continue;
        json_t *pair = json_array();
        json_append(pair, json_number((double)i));
        json_append(pair, json_copy(m));
        json_append(out, pair);
    }
    return out;
}

/* PoP: ant_current_tool_loop_start @ agent/anthropic_message_convert.py:_current_tool_loop_start */
static json_t *ant_current_tool_loop_start(ant_ctx *ctx, const json_t *result) {
    if (!ant_is_arr(result)) {
        ant_raise_fmt(ctx, "TypeError", "object of type '%s' has no len()", ant_type_name(result));
        return NULL;
    }
    for (size_t k = result->c.count; k > 0; k--) {
        size_t i = k - 1;
        json_t *m = result->c.items[i];
        if (!ant_is_obj(m)) { ant_raise_attr_get(ctx, m); return NULL; }
        json_t *role = json_obj_get(m, "role");
        if (!(role && role->type == JSON_STRING && ant_str_eq(role->str_val, "user"))) continue;
        json_t *content = json_obj_get(m, "content");
        bool has_tool_result = false;
        if (ant_is_arr(content)) {
            for (size_t j = 0; j < content->c.count; j++) {
                json_t *bt = ant_block_type(content->c.items[j]);
                if (bt && bt->type == JSON_STRING && ant_str_eq(bt->str_val, "tool_result")) { has_tool_result = true; break; }
            }
        }
        if (!has_tool_result) return json_number((double)(i + 1));
    }
    return json_number(0);
}

/* helper: as_blocks lambda (str -> [text_block]; list -> copy; dict -> keys). */
static json_t *ant_concat_as_blocks(ant_ctx *ctx, const json_t *c) {
    if (ant_is_str(c)) {
        json_t *a = json_array();
        json_append(a, ant_text_block(c));
        return a;
    }
    if (ant_is_arr(c)) return json_copy(c);
    if (ant_is_obj(c)) {
        json_t *a = json_array();
        for (size_t i = 0; i < c->c.count; i++) json_append(a, json_string(c->c.keys[i]));
        return a;
    }
    ant_raise_not_iterable(ctx, c);
    return NULL;
}
/* PoP: ant_concat_content @ agent/anthropic_message_convert.py:_concat_content */
static json_t *ant_concat_content(ant_ctx *ctx, const json_t *prev, const json_t *curr) {
    json_t *a = ant_concat_as_blocks(ctx, prev);
    if (!a) return NULL;
    json_t *b = ant_concat_as_blocks(ctx, curr);
    if (!b) { json_free(a); return NULL; }
    for (size_t i = 0; i < b->c.count; i++) json_append(a, json_copy(b->c.items[i]));
    json_free(b);
    return a;
}

/* PoP: ant_keep_valid_thinking @ agent/anthropic_message_convert.py:_keep_valid_thinking */
static json_t *ant_keep_valid_thinking(ant_ctx *ctx, const json_t *content, bool signature_dead) {
    if (!ant_is_arr(content)) { ant_raise_not_iterable(ctx, content); return NULL; }
    json_t *out = json_array();
    for (size_t i = 0; i < content->c.count; i++) {
        json_t *b = content->c.items[i];
        json_t *bt = ant_block_type(b);
        bool thinking = bt && bt->type == JSON_STRING &&
            (ant_str_eq(bt->str_val, "thinking") || ant_str_eq(bt->str_val, "redacted_thinking"));
        if (!thinking) { json_append(out, json_copy(b)); continue; }
        bool is_redacted = ant_str_eq(bt->str_val, "redacted_thinking");
        json_t *signed_v = json_obj_get(b, is_redacted ? "data" : "signature");
        if (ant_truthy(signed_v) && !signature_dead) {
            json_append(out, json_copy(b));
        } else if ((signature_dead || !is_redacted) && ant_truthy(json_obj_get(b, "thinking"))) {
            json_append(out, ant_text_block(json_obj_get(b, "thinking")));
        }
    }
    return out;
}

/* ── claimed-sibling state effect: apply_assistant_cache_control ─────────── */

/* _apply_assistant_cache_control_to_last_cacheable_block(blocks, cc): if cc is a dict,
 * setdefault cache_control=dict(cc) on the LAST text/tool_use block (reversed), break. */
static void ant_apply_cc_effect(json_t *blocks, const json_t *cc) {
    if (!ant_is_obj(cc) || !ant_is_arr(blocks)) return;
    for (size_t k = blocks->c.count; k > 0; k--) {
        json_t *block = blocks->c.items[k - 1];
        json_t *bt = ant_block_type(block);
        if (bt && bt->type == JSON_STRING &&
            (ant_str_eq(bt->str_val, "text") || ant_str_eq(bt->str_val, "tool_use"))) {
            if (ant_is_obj(block) && !json_has(block, "cache_control"))
                json_set(block, "cache_control", json_copy(cc));
            break;
        }
    }
}
/* fire the seam (recorded) then apply the identical state effect. */
static void ant_apply_cc_seam(ant_ctx *ctx, json_t *blocks, const json_t *cc) {
    json_t *a = json_object();
    ant_arg_node(a, "blocks", blocks);
    ant_arg_node(a, "cache_control", cc);
    json_t *r = ant_call(ctx, "apply_assistant_cache_control", a);
    if (r) json_free(r);
    if (!ant_exc_pending(ctx)) ant_apply_cc_effect(blocks, cc);
}

/* PoP: ant_tool_use_block @ agent/anthropic_message_convert.py:_tool_use_block */
static json_t *ant_tool_use_block(ant_ctx *ctx, const json_t *tool_id, const json_t *name, const json_t *tool_input) {
    json_t *a = json_object();
    ant_arg_node(a, "tool_id", tool_id);
    json_t *sid = ant_call(ctx, "sanitize_tool_id", a);
    if (ant_exc_pending(ctx)) { if (sid) json_free(sid); return NULL; }
    json_t *o = json_object();
    json_set(o, "type", json_string("tool_use"));
    json_set(o, "id", sid ? sid : json_null());
    json_set(o, "name", name ? json_copy(name) : json_null());
    json_set(o, "input", tool_input ? json_copy(tool_input) : json_null());
    return o;
}

/* PoP: ant_replay_ordered_blocks @ agent/anthropic_message_convert.py:_replay_ordered_blocks */
static json_t *ant_replay_ordered_blocks(ant_ctx *ctx, const json_t *m, const json_t *ordered_blocks) {
    if (!ant_is_arr(ordered_blocks)) { ant_raise_not_iterable(ctx, ordered_blocks); return NULL; }
    /* redacted_input_by_id = {sanitize_tool_id(tc.id): parse_tool_args((tc.function or {}).arguments)
     *                        for tc in m.tool_calls or [] if isinstance(tc, dict)} */
    json_t *redacted = json_object();
    json_t *empty_obj = json_object();
    json_t *tcs = ant_get_def(m, "tool_calls", NULL);
    if (!ant_truthy(tcs)) tcs = NULL; /* or [] */
    if (ant_is_arr(tcs)) {
        for (size_t i = 0; i < tcs->c.count; i++) {
            json_t *tc = tcs->c.items[i];
            if (!ant_is_obj(tc)) continue;
            json_t *id_v = ant_get_def(tc, "id", json_string(""));
            /* key: sanitize_tool_id(id) */
            json_t *ka = json_object();
            ant_arg_node(ka, "tool_id", id_v);
            json_t *sid = ant_call(ctx, "sanitize_tool_id", ka);
            if (ant_exc_pending(ctx)) { if (sid) json_free(sid); json_free(redacted); json_free(empty_obj); return NULL; }
            /* value: parse_tool_args((tc.get("function", {}) or {}).get("arguments", "{}")) */
            json_t *fn = ant_get_def(tc, "function", empty_obj);
            if (!ant_truthy(fn)) fn = empty_obj;
            if (!ant_is_obj(fn)) {
                ant_raise_attr_get(ctx, fn);
                if (sid) json_free(sid); json_free(redacted); json_free(empty_obj); return NULL;
            }
            json_t *args_v = ant_get_def(fn, "arguments", json_string("{}"));
            json_t *parsed = ant_parse_tool_args(args_v);
            const char *sid_key = ant_is_str(sid) ? sid->str_val : "";
            json_set(redacted, sid_key, parsed);
            if (sid) json_free(sid);
        }
    }
    json_free(empty_obj);
    /* loop ordered_blocks */
    json_t *replayed = json_array();
    json_t *relocated_cc = NULL; /* borrowed dict or NULL */
    bool dropped_blank_text = false;
    for (size_t i = 0; i < ordered_blocks->c.count; i++) {
        json_t *b = ordered_blocks->c.items[i];
        json_t *sa = json_object();
        ant_arg_node(sa, "b", b);
        json_t *clean = ant_call(ctx, "sanitize_replay_block", sa);
        if (ant_exc_pending(ctx)) { if (clean) json_free(clean); json_free(replayed); json_free(redacted); return NULL; }
        if (ant_is_none(clean)) {
            if (clean) json_free(clean);
            json_t *bt = ant_block_type(b);
            if (bt && bt->type == JSON_STRING && ant_str_eq(bt->str_val, "text")) dropped_blank_text = true;
            json_t *cc = ant_cache_control_of(b);
            if (cc) relocated_cc = cc;
            continue;
        }
        /* clean is a dict (domain); clean.get("type")=="tool_use" -> re-source input */
        json_t *ct = ant_get(clean, "type");
        if (ct && ct->type == JSON_STRING && ant_str_eq(ct->str_val, "tool_use")) {
            json_t *cid = ant_get_def(clean, "id", json_string(""));
            json_t *rv = ant_is_str(cid) ? json_obj_get(redacted, cid->str_val) : NULL;
            if (rv && rv->type != JSON_NULL) json_set(clean, "input", json_copy(rv));
        }
        json_append(replayed, clean); /* ownership transferred */
    }
    json_free(redacted);
    /* placeholder when nothing cacheable survived */
    static const char *CACHEABLE[] = {"text", "tool_use"};
    json_t *cacheable = json_array();
    for (int k = 0; k < 2; k++) json_append(cacheable, json_string(CACHEABLE[k]));
    bool has_cacheable = ant_has_block_type(ctx, replayed, cacheable);
    json_free(cacheable);
    if (ant_exc_pending(ctx)) { json_free(replayed); return NULL; }
    if (!has_cacheable && (dropped_blank_text || relocated_cc != NULL)) {
        json_t *ph = ant_text_block(json_string("(empty)"));
        json_append(replayed, ph);
    }
    if (replayed->c.count == 0) { json_free(replayed); return json_null(); }
    ant_apply_cc_seam(ctx, replayed, relocated_cc);
    if (ant_exc_pending(ctx)) { json_free(replayed); return NULL; }
    ant_apply_cc_seam(ctx, replayed, ant_get(m, "cache_control"));
    if (ant_exc_pending(ctx)) { json_free(replayed); return NULL; }
    json_t *msg_content = ant_get(m, "content");
    if (ant_is_arr(msg_content)) {
        json_t *inline_cc = NULL;
        for (size_t i = 0; i < msg_content->c.count; i++) {
            json_t *cc = ant_cache_control_of(msg_content->c.items[i]);
            if (cc) { inline_cc = cc; break; }
        }
        ant_apply_cc_seam(ctx, replayed, inline_cc);
        if (ant_exc_pending(ctx)) { json_free(replayed); return NULL; }
    }
    return replayed;
}

/* PoP: ant_assistant_replay_carrier @ agent/anthropic_message_convert.py:assistant_replay_carrier */
static json_t *ant_assistant_replay_carrier(ant_ctx *ctx, const json_t *m) {
    json_t *ob = ant_get(m, "anthropic_content_blocks");
    if (ant_is_arr(ob) && ob->c.count > 0) {
        json_t *replayed = ant_replay_ordered_blocks(ctx, m, ob);
        if (ant_exc_pending(ctx)) { if (replayed) json_free(replayed); return NULL; }
        if (ant_truthy(replayed)) {
            json_t *out = json_array();
            json_append(out, json_bool(true));
            json_append(out, replayed);
            return out;
        }
        if (replayed) json_free(replayed);
    }
    json_t *a = json_object();
    ant_arg_node(a, "message", m);
    json_t *blocks = ant_call(ctx, "extract_preserved_thinking_blocks", a);
    if (ant_exc_pending(ctx)) { if (blocks) json_free(blocks); return NULL; }
    json_t *out = json_array();
    json_append(out, json_bool(false));
    json_append(out, blocks ? blocks : json_null());
    return out;
}

/* PoP: ant_tool_result_content @ agent/anthropic_message_convert.py:_tool_result_content */
static json_t *ant_tool_result_content(ant_ctx *ctx, const json_t *m) {
    json_t *empty_str = json_string("");
    json_t *content = ant_get_def(m, "content", empty_str);
    json_t *multimodal = NULL; /* None */
    json_t *empty_arr = json_array();
    if (ant_is_obj(content) && ant_truthy(ant_get(content, "_multimodal"))) {
        json_t *parts = ant_get(content, "content");
        if (!ant_truthy(parts)) parts = empty_arr;
        json_t *a = json_object();
        ant_arg_node(a, "parts", parts);
        multimodal = ant_call(ctx, "content_parts_to_anthropic_blocks", a);
        if (ant_exc_pending(ctx)) { if (multimodal) json_free(multimodal); json_free(empty_str); json_free(empty_arr); return NULL; }
        if (!ant_truthy(multimodal) && ant_truthy(ant_get(content, "text_summary"))) {
            if (multimodal) json_free(multimodal);
            json_t *ts = ant_get(content, "text_summary");
            char *s = ant_pystr(ts);
            json_t *tb = ant_text_block(json_string(s));
            free(s);
            multimodal = json_array();
            json_append(multimodal, tb);
        }
    } else if (ant_is_arr(content)) {
        json_t *a = json_object();
        ant_arg_node(a, "parts", content);
        json_t *converted = ant_call(ctx, "content_parts_to_anthropic_blocks", a);
        if (ant_exc_pending(ctx)) { if (converted) json_free(converted); json_free(empty_str); json_free(empty_arr); return NULL; }
        json_t *img_types = json_array();
        json_append(img_types, json_string("image"));
        bool has_img = ant_has_block_type(ctx, converted, img_types);
        json_free(img_types);
        if (ant_exc_pending(ctx)) { if (converted) json_free(converted); json_free(empty_str); json_free(empty_arr); return NULL; }
        if (has_img) multimodal = converted;
        else { if (converted) json_free(converted); }
    }
    json_free(empty_arr);
    if (multimodal == NULL) {
        json_t *stashed = ant_get(m, "_anthropic_content_blocks");
        if (ant_is_arr(stashed) && stashed->c.count > 0) {
            json_t *text_content = (ant_is_str(content) && ant_nonws(content->str_val)) ? content : NULL;
            multimodal = json_array();
            if (text_content) {
                json_append(multimodal, ant_text_block(text_content));
                for (size_t i = 0; i < stashed->c.count; i++) json_append(multimodal, json_copy(stashed->c.items[i]));
            } else {
                for (size_t i = 0; i < stashed->c.count; i++) json_append(multimodal, json_copy(stashed->c.items[i]));
            }
        }
    }
    if (ant_truthy(multimodal)) { json_free(empty_str); return multimodal; }
    if (multimodal) json_free(multimodal);
    /* (content if str else json.dumps(content) if content else "") or "(no output)" */
    char *result = NULL;
    if (ant_is_str(content)) result = strdup(content->str_val ? content->str_val : "");
    else if (ant_truthy(content)) result = ant_json_dumps(content);
    else result = strdup("");
    if (!result[0]) { free(result); result = strdup("(no output)"); }
    json_t *out = json_string(result);
    free(result);
    json_free(empty_str); /* content may alias this default node; free only after last read */
    return out;
}

/* PoP: ant_convert_system_content @ agent/anthropic_message_convert.py:_convert_system_content */
static json_t *ant_convert_system_content(ant_ctx *ctx, const json_t *content) {
    if (!ant_is_arr(content)) return content ? json_copy(content) : json_null();
    /* any(p.get("cache_control") for p in content if isinstance(p, dict)) */
    bool any_cc = false;
    for (size_t i = 0; i < content->c.count; i++) {
        json_t *p = content->c.items[i];
        if (ant_is_obj(p) && ant_truthy(ant_get(p, "cache_control"))) { any_cc = true; break; }
    }
    if (!any_cc) {
        /* "\n".join(p["text"] for p in content if p.get("type") == "text") */
        json_t *items = json_array(); /* yielded text values */
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *p = content->c.items[i];
            if (!ant_is_obj(p)) { ant_raise_attr_get(ctx, p); json_free(items); return NULL; }
            json_t *pt = ant_get(p, "type");
            if (!(pt && pt->type == JSON_STRING && ant_str_eq(pt->str_val, "text"))) continue;
            json_t *text = json_obj_get(p, "text");
            if (!text) { ant_raise(ctx, "KeyError", "'text'"); json_free(items); return NULL; }
            json_append(items, json_copy(text));
        }
        /* join: every item must be str */
        for (size_t i = 0; i < items->c.count; i++) {
            if (!ant_is_str(items->c.items[i])) {
                ant_raise_fmt(ctx, "TypeError", "sequence item %zu: expected str instance, %s found",
                              i, ant_type_name(items->c.items[i]));
                json_free(items); return NULL;
            }
        }
        ant_strbuf b = {NULL, 0, 0};
        for (size_t i = 0; i < items->c.count; i++) {
            if (i) ant_sb_putc(&b, '\n');
            ant_sb_puts(&b, items->c.items[i]->str_val);
        }
        json_free(items);
        json_t *out = json_string(b.buf ? b.buf : "");
        free(b.buf);
        return out;
    }
    /* [{**p, "text": "(empty)"} if blank && isinstance(text,str) else p for p in content if dict] */
    json_t *out = json_array();
    for (size_t i = 0; i < content->c.count; i++) {
        json_t *p = content->c.items[i];
        if (!ant_is_obj(p)) continue;
        json_t *copy = json_copy(p);
        if (ant_is_blank_text_block(p) && ant_is_str(ant_get(p, "text")))
            json_set(copy, "text", json_string("(empty)"));
        json_append(out, copy);
    }
    return out;
}

/* ── transport-level flows (agent/transports/anthropic.py) ───────────────── */

/* _OAUTH_TOOL_NAME_REVERSE_ALIASES (fixed module constant in agent/anthropic_adapter.py). */
static const char *ant_reverse_alias(const char *bare) {
    if (ant_str_eq(bare, "chat_history_lookup")) return "session_search";
    if (ant_str_eq(bare, "context_notes")) return "memory";
    return NULL;
}

/* PoP: ant_unprefix_oauth_tool_name @ agent/transports/anthropic.py:_unprefix_oauth_tool_name */
static json_t *ant_unprefix_oauth_tool_name(ant_ctx *ctx, const json_t *name) {
    if (!ant_is_str(name)) {
        ant_raise_fmt(ctx, "TypeError", "'%s' object is not subscriptable", ant_type_name(name));
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
        ant_arg_str(a, "name", candidates[k]);
        json_t *entry = ant_call(ctx, "registry.get_entry", a);
        if (ant_exc_pending(ctx)) { if (entry) json_free(entry); free(mcp_bare); return NULL; }
        bool hit = ant_truthy(entry);
        if (entry) json_free(entry);
        if (hit) { free(mcp_bare); return json_string(candidates[k]); }
    }
    free(mcp_bare);
    const char *alias = ant_reverse_alias(bare);
    return json_string(alias ? alias : nm);
}

/* PoP: ant_response_finish_reason @ agent/transports/anthropic.py:response_finish_reason */
static json_t *ant_response_finish_reason(ant_ctx *ctx, const json_t *response) {
    json_t *a = json_object();
    ant_arg_node(a, "response", response);
    json_t *g = ant_call(ctx, "guardrail_intervened", a);
    if (ant_exc_pending(ctx)) { if (g) json_free(g); return NULL; }
    bool intervened = ant_truthy(g);
    if (g) json_free(g);
    if (intervened) return json_string("content_filter");
    /* self.map_finish_reason(response.stop_reason) — stop_reason present in domain */
    json_t *sr = ant_get(response, "stop_reason");
    json_t *ma = json_object();
    ant_arg_node(ma, "raw_reason", sr);
    json_t *r = ant_call(ctx, "map_finish_reason", ma);
    if (ant_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}