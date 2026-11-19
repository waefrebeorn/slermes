/* anthropic_convert_flow2.c — second-wave FLOW port of the UNCLAIMED remainder
 * of agent/anthropic_message_convert.py (the OpenAI -> Anthropic request
 * conversion pipeline). anthropic_transport_flow.c ported 19 leaves; the
 * claim audit (PoP grep + stub-body inspection per references/stub-port-audit.md)
 * found the 15 functions below claimed ONLY by printf-echo stubs / fabricated
 * bodies in src/agent/anthropic_adapter_ports.c (+ one echo-fake in
 * src/agent/anthropic_adapter.c and two incomplete ports missing the core
 * cache-relocation / placeholder behaviour), i.e. UNIMPLEMENTED:
 *   _extract_preserved_thinking_blocks  (antr_ body scans "content" keys the
 *                                        live fn never reads — fabricated)
 *   convert_tools_to_anthropic          (printf echo)
 *   _convert_content_to_anthropic       (printf echo)
 *   _content_parts_to_anthropic_blocks  (printf echo)
 *   _convert_assistant_message          (printf echo)
 *   _convert_tool_message_to_result     (printf echo)
 *   _convert_user_message               (printf echo)
 *   _strip_orphaned_tool_blocks         (printf echo)
 *   _merge_consecutive_roles            (printf echo)
 *   _manage_thinking_signatures         (printf echo)
 *   _evict_old_screenshots              (printf echo)
 *   _ensure_leading_user_turn           (prints 0/1, never mutates — echo fake)
 *   _fix_blank_text_blocks_in_list      (drops blanks only: no cc relocation,
 *                                        no placeholder fallback, no logging)
 *   _scrub_blank_text_blocks            (drops blanks only: no placeholder,
 *                                        no cc relocation, no tool_result inner)
 *   convert_messages_to_anthropic       (printf echo)
 *
 * Decision-sequence port: ALL collaborator calls ride the injected universal
 * seam acv2_call; the harness scripts results and records the ordered trace the
 * oracle diffs against LIVE quarry defs under the same scripts. Ported functions
 * call each other REAL (internal composition). libjson-only; self-contained (the
 * harness #includes "agent/anthropic_convert_flow2.c"). Every function is static
 * acv2_* so it never collides with the stub-era antr_ / amc_ / ant_ symbols.
 *
 * Seam name -> live call (args projection; None-valued keys dropped both sides):
 *   block_type                        _block_type(b)                       {b}
 *   has_block_type                    _has_block_type(blocks, types)       {blocks, types}
 *   text_block                        _text_block(text)                    {text}
 *   strip_thinking                    _strip_thinking(blocks)              {blocks}
 *   block_ids                         _block_ids(blocks, btype, key)       {blocks, btype, key}
 *   assistant_block_lists             _assistant_block_lists(result)       {result}
 *   parse_tool_args                   _parse_tool_args(raw)                {raw}
 *   tool_use_block                    _tool_use_block(id, name, input)     {tool_id, name, tool_input}
 *   concat_content                    _concat_content(prev, curr)          {prev, curr}
 *   keep_valid_thinking               _keep_valid_thinking(content, dead)  {content, signature_dead}
 *   current_tool_loop_start           _current_tool_loop_start(result)     {result}
 *   image_source_data                 _image_source_data(block)            {block}
 *   convert_system_content            _convert_system_content(content)     {content}
 *   assistant_replay_carrier          assistant_replay_carrier(m)          {m}
 *   tool_result_content               _tool_result_content(m)              {m}
 *   carry_cache_control               _carry_cache_control(out, b, copy=)  {out, b, copy}
 *   split_blank_text_blocks           _split_blank_text_blocks(blocks)     {blocks}
 *   sanitize_tool_id                  _sanitize_tool_id(tool_id)           {tool_id}
 *   normalize_tool_input_schema       _normalize_tool_input_schema(schema) {schema}
 *   convert_content_part_to_anthropic _convert_content_part_to_anthropic(part) {part}
 *   apply_assistant_cache_control     _apply_assistant_cache_control_to_last_cacheable_block(blocks, cc)
 *                                     {blocks, cache_control}  (STATE-EFFECT seam)
 *   reasoning_minted_outside_claude   agent.bedrock_adapter.reasoning_minted_outside_claude(message)
 *                                     {message}  (in-function import)
 *   anthropic_thinking_route          agent.anthropic_thinking_policy.anthropic_thinking_route(base_url, model)
 *                                     {base_url, model}
 *   route_replays_prior_thinking      agent.anthropic_thinking_policy.route_replays_prior_thinking(route, model)
 *                                     {route, model}
 *   images_all_within_many_image_limit agent.image_eviction_policy.images_all_within_many_image_limit(datas)
 *                                     {datas}
 *   outbound_image_retire_count       agent.image_eviction_policy.outbound_image_retire_count(...)
 *                                     {counts, reserved_count, carrier_bytes_newest_first,
 *                                      reserved_bytes, many_image_safe}
 *   logger.warning                    logger.warning(fmt, *args)           {fmt, args}
 *
 * Special seam contracts:
 *   apply_assistant_cache_control / carry_cache_control are claimed siblings
 *   whose live bodies MUTATE their first argument. They are seamed (recorded)
 *   AND their documented state effect is applied identically by the C port
 *   (acv2_apply_cc_effect / acv2_carry_cc_effect after the call) and the oracle
 *   recorder — same algorithm both sides. Seam args snapshot the carrier BEFORE
 *   the effect. carry_cache_control's live return is the mutated `out` dict;
 *   convert_tools_to_anthropic appends that return, so the scripted ret must
 *   equal the post-effect out (the generator records it from the live sibling).
 *   assistant_block_lists is an INDEX-REF seam: the live generator yields
 *   (i, result[i]) with m the SAME object the caller mutates. The scripted ret
 *   records [i, m] snapshots for the trace, but both sides rebind m to
 *   result[i] so in-place mutations land on the state window.
 *
 * Representation contract (shared with tests/oracle_anthropic_convert_flow2.py):
 *   - None -> JSON null; tuple/set -> array (sets sorted by str()); the
 *     _THINKING_TYPES / _CACHEABLE_TYPES constants are seeded as TUPLES in the
 *     oracle (== membership, matching the C array model), so has_block_type
 *     `types` args carry ["thinking","redacted_thinking"] / ["text","tool_use"]
 *     in that order; the {"image"} set literal in _evict_old_screenshots
 *     projects to ["image"].
 *   - integral floats collapse to ints (Python == equates them); dict key order
 *     mirrors live insertion order (json_set replaces in place).
 *   - uncaught exception -> {"__raise__": cls, "msg": str(exc)}; a scripted seam
 *     raise parks the marker VERBATIM. None of the 15 ported defs has a
 *     try/except, so EVERY raise (incl. the BaseException trio SystemExit /
 *     KeyboardInterrupt / GeneratorExit) propagates to the harness envelope.
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     {"$seq":[...]} per-call queue (exhaustion -> RuntimeError("script
 *     exhausted: <name>")); {"__raise__":cls,"msg":m} raises (trace ret is the
 *     WHOLE marker); scripted null -> None; any other value reused.
 *   - stateful defs (_convert_tool_message_to_result, _strip_orphaned_tool_blocks,
 *     _merge_consecutive_roles, _manage_thinking_signatures, _evict_old_screenshots,
 *     _ensure_leading_user_turn, _scrub_blank_text_blocks) mutate the caller's
 *     `result` list in place; the harness echoes it as state {"result": ...}
 *     EVEN ON A RAISE (partial mutation history).
 *
 * Fixture domain (documented exclusions, ASCII scope):
 *   - str() operands (assistant scalar content, reasoning_details block type)
 *     are str/int/bool/float/null scalars; dict/list assistant scalar content
 *     is OUT of domain (str(dict) repr). .strip() uses the full Unicode
 *     White_Space set (acv2_nonws); .lower() is ASCII A-Z (ASCII scope).
 *   - tool ids / names / block "type" values are hashable scalars (str/null/
 *     int/bool/float); seen_names membership is ==-based.
 *   - tools is a list or falsy; a truthy non-list raises TypeError not iterable
 *     (truthy str/dict tools would iterate chars/keys in Python — out of domain).
 *   - image data payloads are ASCII str; len(data) counts codepoints
 *     (acv2_pylen), equal to bytes in the ASCII domain.
 *   - outbound_image_retire_count returns a non-negative int; carrier slice
 *     clamps like Python.
 *   - seam results keep their live shapes: split_blank_text_blocks ->
 *     [kept, relocated-or-null, dropped-indexes]; assistant_replay_carrier ->
 *     [ordered-bool, blocks-list]; assistant_block_lists -> [[i, m], ...] with
 *     i a valid result index; sanitize_tool_id -> str; block_ids -> sorted array.
 *   - reasoning_minted_outside_claude / thinking-route / eviction-policy seams
 *     are environment policy decisions scripted per case (the generator's
 *     sibling world consumes the same env scripts).
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The harness
 * #includes this .c via the area-qualified "agent/anthropic_convert_flow2.c".
 * Every symbol is static acv2_*.
 *
 * Gate: bash tests/run_one_oracle.sh anthropic_convert_flow2 \
 *           oracle_anthropic_convert_flow2.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

/* ── live module constants (byte-pinned to agent/anthropic_message_convert.py) ── */
static const char *ACV2_THINKING_TYPES[] = {"thinking", "redacted_thinking"};
static const char *ACV2_CACHEABLE_TYPES[] = {"text", "tool_use"};
#define ACV2_EMPTY_TEXT_PLACEHOLDER "(empty)"

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct acv2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} acv2_ctx;

static bool acv2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool acv2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool acv2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool acv2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool acv2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool acv2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static bool acv2_truthy(const json_t *v) {
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
static json_t *acv2_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present value (incl null) passes through; absent -> default.
 * Returns BORROWED node (def may be a caller-owned temporary). */
static json_t *acv2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None. */
static bool acv2_json_eq(const json_t *a, const json_t *b) {
    if (acv2_is_none(a) && acv2_is_none(b)) return true;
    if (acv2_is_none(a) || acv2_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return acv2_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!acv2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !acv2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* Python type name for a JSON node (fixture domain). */
static const char *acv2_type_name(const json_t *v) {
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
 * exponent in [-4, 16), scientific otherwise. Caller frees. */
static char *acv2_float_repr(double d) {
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

/* str(v) for a scalar (null -> "None"). Caller frees. Domain: str/int/bool/float/null. */
static char *acv2_pystr(const json_t *v) {
    if (acv2_is_none(v)) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (acv2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else return acv2_float_repr(d);
        return strdup(buf);
    }
    if (acv2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* ── Unicode White_Space scan (str.strip / str.isspace) ──────────────────── */

static unsigned int acv2_utf8_decode(const char *s, size_t *len) {
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
static bool acv2_codepoint_ws(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return true;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return true;
    return false;
}
/* true if s contains at least one non-White_Space codepoint (== bool(s.strip())). */
static bool acv2_nonws(const char *s) {
    if (!s) return false;
    size_t i = 0, n = strlen(s);
    while (i < n) {
        size_t l; unsigned int cp = acv2_utf8_decode(s + i, &l);
        if (!acv2_codepoint_ws(cp)) return true;
        i += l;
    }
    return false;
}
/* Python len(str): codepoint count (ASCII domain: == strlen). */
static size_t acv2_pylen(const char *s) {
    if (!s) return 0;
    size_t i = 0, n = strlen(s), count = 0;
    while (i < n) {
        size_t l; acv2_utf8_decode(s + i, &l);
        i += l; count++;
    }
    return count;
}
/* str.strip().lower() for the reasoning_details type gate (ASCII lower). Caller frees. */
static char *acv2_strip_lower(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b) {
        size_t l; unsigned int cp = acv2_utf8_decode(s + a, &l);
        if (!acv2_codepoint_ws(cp)) break;
        a += l;
    }
    while (b > a) {
        /* decode the codepoint ENDING at b: scan from a is O(n^2) worst case but
         * fixture strings are tiny; find the last lead byte before b. */
        size_t k = b - 1;
        while (k > a && ((unsigned char)s[k] & 0xC0) == 0x80) k--;
        size_t l; unsigned int cp = acv2_utf8_decode(s + k, &l);
        if (!acv2_codepoint_ws(cp) || k + l != b) break;
        b = k;
    }
    char *out = (char *)malloc(b - a + 1);
    for (size_t i = a; i < b; i++) {
        char c = s[i];
        out[i - a] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[b - a] = '\0';
    return out;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool acv2_exc_pending(const acv2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void acv2_raise(acv2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static void acv2_raise_fmt(acv2_ctx *ctx, const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    acv2_raise(ctx, cls, buf);
}
/* AttributeError '<T>' object has no attribute '<name>' on a non-dict carrier. */
static void acv2_raise_attr_get(acv2_ctx *ctx, const json_t *v) {
    acv2_raise_fmt(ctx, "AttributeError", "'%s' object has no attribute 'get'", acv2_type_name(v));
}
static void acv2_raise_attr_method(acv2_ctx *ctx, const json_t *v, const char *method) {
    acv2_raise_fmt(ctx, "AttributeError", "'%s' object has no attribute '%s'", acv2_type_name(v), method);
}
/* TypeError '<T>' object is not iterable. */
static void acv2_raise_not_iterable(acv2_ctx *ctx, const json_t *v) {
    acv2_raise_fmt(ctx, "TypeError", "'%s' object is not iterable", acv2_type_name(v));
}
static void acv2_raise_not_subscriptable(acv2_ctx *ctx, const json_t *v) {
    acv2_raise_fmt(ctx, "TypeError", "'%s' object is not subscriptable", acv2_type_name(v));
}
static void acv2_raise_key(acv2_ctx *ctx, const char *key) {
    acv2_raise_fmt(ctx, "KeyError", "'%s'", key);
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW (caller frees). */
static json_t *acv2_call(acv2_ctx *ctx, const char *name, json_t *args) {
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
static void acv2_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!acv2_is_none(val)) json_set(args, key, json_copy(val));
}
static void acv2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}

/* ── set projection helpers (arrays; membership/diff/union by ==) ────────── */

static bool acv2_set_contains(const json_t *set, const json_t *v) {
    if (!acv2_is_arr(set)) return false;
    for (size_t i = 0; i < set->c.count; i++)
        if (acv2_json_eq(set->c.items[i], v)) return true;
    return false;
}
/* left - right (preserves left order). NEW array. */
static json_t *acv2_set_diff(const json_t *left, const json_t *right) {
    json_t *out = json_array();
    if (acv2_is_arr(left))
        for (size_t i = 0; i < left->c.count; i++)
            if (!acv2_set_contains(right, left->c.items[i]))
                json_append(out, json_copy(left->c.items[i]));
    return out;
}
/* dest |= src (dedup by ==). Mutates dest. */
static void acv2_set_union_into(json_t *dest, const json_t *src) {
    if (!acv2_is_arr(src)) return;
    for (size_t i = 0; i < src->c.count; i++)
        if (!acv2_set_contains(dest, src->c.items[i]))
            json_append(dest, json_copy(src->c.items[i]));
}

/* list.insert(0, val) — ownership of val transferred. */
static void acv2_array_insert0(json_t *arr, json_t *val) {
    if (!acv2_is_arr(arr) || !val) { if (val) json_free(val); return; }
    size_t n = arr->c.count;
    json_t **ni = (json_t **)realloc(arr->c.items, (n + 1) * sizeof(json_t *));
    if (!ni) { json_free(val); return; }
    arr->c.items = ni;
    memmove(arr->c.items + 1, arr->c.items, n * sizeof(json_t *));
    arr->c.items[0] = val;
    arr->c.count = n + 1;
}

/* membership of a block-type value in the _THINKING_TYPES tuple (== semantics). */
static bool acv2_in_thinking(const json_t *bt) {
    for (int k = 0; k < 2; k++)
        if (acv2_is_str(bt) && acv2_str_eq(bt->str_val, ACV2_THINKING_TYPES[k])) return true;
    return false;
}
static bool acv2_in_cacheable(const json_t *bt) {
    for (int k = 0; k < 2; k++)
        if (acv2_is_str(bt) && acv2_str_eq(bt->str_val, ACV2_CACHEABLE_TYPES[k])) return true;
    return false;
}
/* types arg array matching the oracle's tuple-seeded constant order. */
static json_t *acv2_types_array(const char *const *types) {
    json_t *a = json_array();
    json_append(a, json_string(types[0]));
    json_append(a, json_string(types[1]));
    return a;
}

/* ── claimed-sibling state effects ───────────────────────────────────────── */

/* _apply_assistant_cache_control_to_last_cacheable_block(blocks, cc): if cc is a
 * dict, setdefault cache_control=dict(cc) on the LAST text/tool_use block
 * (reversed), break. */
static void acv2_apply_cc_effect(json_t *blocks, const json_t *cc) {
    if (!acv2_is_obj(cc) || !acv2_is_arr(blocks)) return;
    for (size_t k = blocks->c.count; k > 0; k--) {
        json_t *block = blocks->c.items[k - 1];
        json_t *bt = acv2_is_obj(block) ? json_obj_get(block, "type") : NULL;
        if (acv2_in_cacheable(bt)) {
            if (!json_has(block, "cache_control"))
                json_set(block, "cache_control", json_copy(cc));
            break;
        }
    }
}
/* fire the seam (recorded) then apply the identical state effect. */
static void acv2_apply_cc_seam(acv2_ctx *ctx, json_t *blocks, const json_t *cc) {
    json_t *a = json_object();
    acv2_arg_node(a, "blocks", blocks);
    acv2_arg_node(a, "cache_control", cc);
    json_t *r = acv2_call(ctx, "apply_assistant_cache_control", a);
    if (r) json_free(r);
    if (!acv2_exc_pending(ctx)) acv2_apply_cc_effect(blocks, cc);
}

/* _carry_cache_control(out, b, copy): cc = b.get("cache_control") if dict else
 * None; if cc is a dict: out["cache_control"] = dict(cc) if copy else cc.
 * Returns out (the scripted ret records the post-effect out). */
static void acv2_carry_cc_effect(json_t *out, const json_t *b, bool copy) {
    if (!acv2_is_obj(out) || !acv2_is_obj(b)) return;
    json_t *cc = json_obj_get(b, "cache_control");
    if (!acv2_is_obj(cc)) return;
    json_set(out, "cache_control", json_copy(cc));
    (void)copy; /* both live call sites pass copy=True; dict(cc) shallow copy ==
                 * json_copy for the flat marker dicts in the fixture domain */
}
static void acv2_carry_cc_seam(acv2_ctx *ctx, json_t *out, const json_t *b, bool copy) {
    json_t *a = json_object();
    acv2_arg_node(a, "out", out);
    acv2_arg_node(a, "b", b);
    json_set(a, "copy", json_bool(copy));
    json_t *r = acv2_call(ctx, "carry_cache_control", a);
    if (r) json_free(r);
    if (!acv2_exc_pending(ctx)) acv2_carry_cc_effect(out, b, copy);
}

/* logger.warning(fmt, *args) seam: {fmt, args:[...]}. Returns None. */
static void acv2_log_warning(acv2_ctx *ctx, const char *fmt, json_t *args) {
    json_t *a = json_object();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());
    json_t *r = acv2_call(ctx, "logger.warning", a);
    if (r) json_free(r);
}

/* assistant_block_lists INDEX-REF seam: fire + return the scripted pairs array.
 * Callers rebind m to result[idx] (live generator identity). */
static json_t *acv2_abl_seam(acv2_ctx *ctx, const json_t *result) {
    json_t *a = json_object();
    acv2_arg_node(a, "result", result);
    return acv2_call(ctx, "assistant_block_lists", a);
}
/* rebind pair -> (idx, result[idx]); raises IndexError on an out-of-range script. */
static json_t *acv2_abl_rebind(acv2_ctx *ctx, const json_t *result, const json_t *pair, size_t *idx_out) {
    if (!acv2_is_arr(pair) || pair->c.count < 1 || !acv2_is_num(pair->c.items[0])) {
        acv2_raise(ctx, "TypeError", "invalid assistant_block_lists script pair");
        return NULL;
    }
    double di = pair->c.items[0]->num_val;
    if (di < 0 || di >= (double)result->c.count || di != floor(di)) {
        acv2_raise(ctx, "IndexError", "list index out of range");
        return NULL;
    }
    *idx_out = (size_t)di;
    return result->c.items[(size_t)di];
}

/* tuple-unpack guard for scripted seam results (split / carrier). */
static bool acv2_unpack_ok(acv2_ctx *ctx, const json_t *r, size_t n) {
    if (!acv2_is_arr(r)) {
        acv2_raise_not_iterable(ctx, r);
        return false;
    }
    if (r->c.count < n) {
        acv2_raise_fmt(ctx, "ValueError", "not enough values to unpack (expected %zu)", n);
        return false;
    }
    if (r->c.count > n) {
        acv2_raise_fmt(ctx, "ValueError", "too many values to unpack (expected %zu)", n);
        return false;
    }
    return true;
}

/* ── ported functions ────────────────────────────────────────────────────── */

/* PoP: acv2_fix_blank_text_blocks_in_list @ agent/anthropic_message_convert.py:_fix_blank_text_blocks_in_list */
static json_t *acv2_fix_blank_text_blocks_in_list(acv2_ctx *ctx, const json_t *blocks,
                                                  const char *placeholder_text, double msg_index,
                                                  const json_t *role, const char *location) {
    json_t *a = json_object();
    acv2_arg_node(a, "blocks", blocks);
    json_t *r = acv2_call(ctx, "split_blank_text_blocks", a);
    if (acv2_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    if (!acv2_unpack_ok(ctx, r, 3)) { if (r) json_free(r); return NULL; }
    json_t *kept = json_copy(r->c.items[0]);
    json_t *relocated = acv2_is_obj(r->c.items[1]) ? json_copy(r->c.items[1]) : NULL;
    json_t *dropped = r->c.items[2];
    if (!acv2_is_arr(dropped)) {
        acv2_raise_not_iterable(ctx, dropped);
        json_free(kept); if (relocated) json_free(relocated); json_free(r);
        return NULL;
    }
    for (size_t i = 0; i < dropped->c.count; i++) {
        json_t *aa = json_array();
        json_append(aa, json_number(msg_index));
        json_append(aa, role ? json_copy(role) : json_null());
        json_append(aa, json_string(location));
        json_append(aa, json_copy(dropped->c.items[i]));
        acv2_log_warning(ctx,
            "Pre-call sanitizer: dropped blank text content block "
            "(message_index=%d role=%s location=%s block_index=%d block_type=text)",
            aa);
        if (acv2_exc_pending(ctx)) {
            json_free(kept); if (relocated) json_free(relocated); json_free(r);
            return NULL;
        }
    }
    if (!acv2_truthy(kept)) {
        json_free(kept);
        json_t *ta = json_object();
        acv2_arg_str(ta, "text", placeholder_text);
        json_t *tb = acv2_call(ctx, "text_block", ta);
        if (acv2_exc_pending(ctx)) {
            if (tb) json_free(tb);
            if (relocated) json_free(relocated);
            json_free(r);
            return NULL;
        }
        kept = json_array();
        json_append(kept, tb ? tb : json_null());
    }
    acv2_apply_cc_seam(ctx, kept, relocated);
    if (relocated) json_free(relocated);
    json_free(r);
    if (acv2_exc_pending(ctx)) { json_free(kept); return NULL; }
    return kept;
}

/* PoP: acv2_extract_preserved_thinking_blocks @ agent/anthropic_message_convert.py:_extract_preserved_thinking_blocks */
static json_t *acv2_extract_preserved_thinking_blocks(acv2_ctx *ctx, const json_t *message) {
    if (!acv2_is_obj(message)) { acv2_raise_attr_get(ctx, message); return NULL; }
    json_t *raw = json_obj_get(message, "reasoning_details");
    if (!acv2_is_arr(raw)) return json_array();
    json_t *a = json_object();
    acv2_arg_node(a, "message", message);
    json_t *minted = acv2_call(ctx, "reasoning_minted_outside_claude", a);
    if (acv2_exc_pending(ctx)) { if (minted) json_free(minted); return NULL; }
    bool foreign = acv2_truthy(minted);
    if (minted) json_free(minted);
    if (foreign) return json_array();
    json_t *empty_str = json_string("");
    json_t *out = json_array();
    for (size_t i = 0; i < raw->c.count; i++) {
        json_t *d = raw->c.items[i];
        if (!acv2_is_obj(d)) continue;
        json_t *tv = acv2_get_def(d, "type", empty_str);
        json_t *orv = acv2_truthy(tv) ? tv : empty_str;
        char *s = acv2_pystr(orv);
        char *sl = acv2_strip_lower(s);
        free(s);
        bool hit = false;
        for (int k = 0; k < 2; k++)
            if (acv2_str_eq(sl, ACV2_THINKING_TYPES[k])) hit = true;
        free(sl);
        if (hit) json_append(out, json_copy(d));
    }
    json_free(empty_str);
    return out;
}

/* PoP: acv2_convert_content_to_anthropic @ agent/anthropic_message_convert.py:_convert_content_to_anthropic */
static json_t *acv2_convert_content_to_anthropic(acv2_ctx *ctx, const json_t *content) {
    if (!acv2_is_arr(content)) return content ? json_copy(content) : json_null();
    json_t *out = json_array();
    for (size_t i = 0; i < content->c.count; i++) {
        json_t *a = json_object();
        acv2_arg_node(a, "part", content->c.items[i]);
        json_t *b = acv2_call(ctx, "convert_content_part_to_anthropic", a);
        if (acv2_exc_pending(ctx)) { if (b) json_free(b); json_free(out); return NULL; }
        if (b && b->type != JSON_NULL) json_append(out, b);
        else if (b) json_free(b);
    }
    return out;
}

/* PoP: acv2_content_parts_to_anthropic_blocks @ agent/anthropic_message_convert.py:_content_parts_to_anthropic_blocks */
static json_t *acv2_content_parts_to_anthropic_blocks(acv2_ctx *ctx, const json_t *parts) {
    json_t *out = json_array();
    if (!acv2_is_arr(parts)) return out;
    for (size_t i = 0; i < parts->c.count; i++) {
        json_t *a = json_object();
        acv2_arg_node(a, "part", parts->c.items[i]);
        json_t *block = acv2_call(ctx, "convert_content_part_to_anthropic", a);
        if (acv2_exc_pending(ctx)) { if (block) json_free(block); json_free(out); return NULL; }
        if (!acv2_truthy(block)) { if (block) json_free(block); continue; }
        if (!acv2_is_obj(block)) {
            acv2_raise_attr_get(ctx, block);
            json_free(block); json_free(out);
            return NULL;
        }
        json_t *btype = json_obj_get(block, "type");
        json_t *text_val = json_obj_get(block, "text");
        json_t *src = json_obj_get(block, "source");
        if (acv2_is_str(btype) && acv2_str_eq(btype->str_val, "text") &&
            acv2_is_str(text_val) && acv2_truthy(text_val)) {
            json_t *ta = json_object();
            acv2_arg_node(ta, "text", text_val);
            json_t *tb = acv2_call(ctx, "text_block", ta);
            if (acv2_exc_pending(ctx)) { if (tb) json_free(tb); json_free(block); json_free(out); return NULL; }
            json_append(out, tb ? tb : json_null());
        } else if (acv2_is_str(btype) && acv2_str_eq(btype->str_val, "image") &&
                   acv2_is_obj(src) && acv2_truthy(src)) {
            json_t *ib = json_object();
            json_set(ib, "type", json_string("image"));
            json_set(ib, "source", json_copy(src));
            json_append(out, ib);
        }
        json_free(block);
    }
    return out;
}

/* PoP: acv2_convert_user_message @ agent/anthropic_message_convert.py:_convert_user_message */
static json_t *acv2_convert_user_message(acv2_ctx *ctx, const json_t *content) {
    json_t *c = NULL;
    if (acv2_is_arr(content)) {
        json_t *converted = acv2_convert_content_to_anthropic(ctx, content);
        if (acv2_exc_pending(ctx)) { if (converted) json_free(converted); return NULL; }
        json_t *role = json_string("user");
        c = acv2_fix_blank_text_blocks_in_list(ctx, converted, "(empty message)", -1,
                                               role, "_convert_user_message");
        json_free(role);
        json_free(converted);
        if (acv2_exc_pending(ctx)) { if (c) json_free(c); return NULL; }
    } else if (!acv2_truthy(content) ||
               (acv2_is_str(content) && !acv2_nonws(content->str_val))) {
        c = json_string("(empty message)");
    } else {
        c = content ? json_copy(content) : json_null();
    }
    json_t *o = json_object();
    json_set(o, "role", json_string("user"));
    json_set(o, "content", c);
    return o;
}

/* PoP: acv2_convert_tools_to_anthropic @ agent/anthropic_message_convert.py:convert_tools_to_anthropic */
static json_t *acv2_convert_tools_to_anthropic(acv2_ctx *ctx, const json_t *tools) {
    json_t *result = json_array();
    json_t *seen = json_array();
    json_t *empty_obj = json_object();
    json_t *empty_str = json_string("");
    json_t *iter = acv2_truthy(tools) ? (json_t *)tools : NULL;
    if (iter && !acv2_is_arr(iter)) {
        acv2_raise_not_iterable(ctx, iter);
        json_free(result); json_free(seen); json_free(empty_obj); json_free(empty_str);
        return NULL;
    }
    for (size_t i = 0; iter && i < iter->c.count; i++) {
        json_t *t = iter->c.items[i];
        if (!acv2_is_obj(t)) {
            acv2_raise_attr_get(ctx, t);
            json_free(result); json_free(seen); json_free(empty_obj); json_free(empty_str);
            return NULL;
        }
        json_t *fn = acv2_get_def(t, "function", empty_obj);
        if (!acv2_is_obj(fn)) {
            acv2_raise_attr_get(ctx, fn);
            json_free(result); json_free(seen); json_free(empty_obj); json_free(empty_str);
            return NULL;
        }
        json_t *name = acv2_get_def(fn, "name", empty_str);
        if (acv2_truthy(name) && acv2_set_contains(seen, name)) {
            json_t *aa = json_array();
            json_append(aa, json_copy(name));
            acv2_log_warning(ctx,
                "convert_tools_to_anthropic: duplicate tool name '%s' "
                "\xe2\x80\x94 dropping second occurrence",
                aa);
            if (acv2_exc_pending(ctx)) {
                json_free(result); json_free(seen); json_free(empty_obj); json_free(empty_str);
                return NULL;
            }
            continue;
        }
        if (acv2_truthy(name)) json_append(seen, json_copy(name));
        json_t *tool = json_object();
        json_set(tool, "name", json_copy(name));
        json_set(tool, "description", json_copy(acv2_get_def(fn, "description", empty_str)));
        json_t *params = acv2_get(fn, "parameters");
        json_t *schema_arg = acv2_truthy(params) ? params : empty_obj;
        json_t *na = json_object();
        acv2_arg_node(na, "schema", schema_arg);
        json_t *ns = acv2_call(ctx, "normalize_tool_input_schema", na);
        if (acv2_exc_pending(ctx)) {
            if (ns) json_free(ns);
            json_free(tool); json_free(result); json_free(seen); json_free(empty_obj); json_free(empty_str);
            return NULL;
        }
        json_set(tool, "input_schema", ns ? ns : json_null());
        acv2_carry_cc_seam(ctx, tool, t, true);
        if (acv2_exc_pending(ctx)) {
            json_free(tool); json_free(result); json_free(seen); json_free(empty_obj); json_free(empty_str);
            return NULL;
        }
        json_append(result, tool);
    }
    json_free(seen); json_free(empty_obj); json_free(empty_str);
    return result;
}

/* PoP: acv2_convert_tool_message_to_result @ agent/anthropic_message_convert.py:_convert_tool_message_to_result */
static void acv2_convert_tool_message_to_result(acv2_ctx *ctx, json_t *result, const json_t *m) {
    if (!acv2_is_obj(m)) { acv2_raise_attr_get(ctx, m); return; }
    json_t *empty_str = json_string("");
    json_t *tr = json_object();
    json_set(tr, "type", json_string("tool_result"));
    json_t *tid = acv2_get_def(m, "tool_call_id", empty_str);
    json_t *sa = json_object();
    acv2_arg_node(sa, "tool_id", tid);
    json_t *sid = acv2_call(ctx, "sanitize_tool_id", sa);
    if (acv2_exc_pending(ctx)) { if (sid) json_free(sid); json_free(tr); json_free(empty_str); return; }
    json_set(tr, "tool_use_id", sid ? sid : json_null());
    json_t *ca = json_object();
    acv2_arg_node(ca, "m", m);
    json_t *content = acv2_call(ctx, "tool_result_content", ca);
    if (acv2_exc_pending(ctx)) { if (content) json_free(content); json_free(tr); json_free(empty_str); return; }
    json_set(tr, "content", content ? content : json_null());
    acv2_carry_cc_seam(ctx, tr, m, true);
    json_free(empty_str);
    if (acv2_exc_pending(ctx)) { json_free(tr); return; }
    /* last = result[-1] if result else {} */
    json_t *last = (acv2_is_arr(result) && result->c.count > 0)
        ? result->c.items[result->c.count - 1] : NULL;
    json_t *last_content = NULL;
    if (last) {
        if (!acv2_is_obj(last)) { acv2_raise_attr_get(ctx, last); json_free(tr); return; }
        json_t *role = json_obj_get(last, "role");
        if (acv2_is_str(role) && acv2_str_eq(role->str_val, "user"))
            last_content = json_obj_get(last, "content");
    }
    if (acv2_is_arr(last_content) && last_content->c.count > 0) {
        json_t *first = last_content->c.items[0];
        if (!acv2_is_obj(first)) { acv2_raise_attr_get(ctx, first); json_free(tr); return; }
        json_t *ft = json_obj_get(first, "type");
        if (acv2_is_str(ft) && acv2_str_eq(ft->str_val, "tool_result")) {
            json_append(last_content, tr);
            return;
        }
    }
    json_t *um = json_object();
    json_set(um, "role", json_string("user"));
    json_t *ua = json_array();
    json_append(ua, tr);
    json_set(um, "content", ua);
    json_append(result, um);
}

/* PoP: acv2_convert_assistant_message @ agent/anthropic_message_convert.py:_convert_assistant_message */
static json_t *acv2_convert_assistant_message(acv2_ctx *ctx, const json_t *m) {
    if (!acv2_is_obj(m)) { acv2_raise_attr_get(ctx, m); return NULL; }
    json_t *empty_str = json_string("");
    json_t *empty_obj = json_object();
    json_t *empty_arr = json_array();
    json_t *content = acv2_get_def(m, "content", empty_str);
    json_t *ca = json_object();
    acv2_arg_node(ca, "m", m);
    json_t *carrier = acv2_call(ctx, "assistant_replay_carrier", ca);
    if (acv2_exc_pending(ctx)) {
        if (carrier) json_free(carrier);
        json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
        return NULL;
    }
    if (!acv2_unpack_ok(ctx, carrier, 2)) {
        if (carrier) json_free(carrier);
        json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
        return NULL;
    }
    bool ordered = acv2_truthy(carrier->c.items[0]);
    json_t *blocks = json_copy(carrier->c.items[1]);
    json_free(carrier);
    if (ordered) {
        json_t *o = json_object();
        json_set(o, "role", json_string("assistant"));
        json_set(o, "content", blocks);
        json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
        return o;
    }
    json_t *relocated = NULL; /* owned dict or NULL */
    if (acv2_is_arr(content)) {
        json_t *converted = acv2_convert_content_to_anthropic(ctx, content);
        if (acv2_exc_pending(ctx)) {
            if (converted) json_free(converted);
            json_free(blocks); json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        json_t *sa = json_object();
        acv2_arg_node(sa, "blocks", converted);
        json_t *sr = acv2_call(ctx, "split_blank_text_blocks", sa);
        json_free(converted);
        if (acv2_exc_pending(ctx)) {
            if (sr) json_free(sr);
            json_free(blocks); json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        if (!acv2_unpack_ok(ctx, sr, 3)) {
            if (sr) json_free(sr);
            json_free(blocks); json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        json_t *kept = sr->c.items[0];
        relocated = acv2_is_obj(sr->c.items[1]) ? json_copy(sr->c.items[1]) : NULL;
        /* blocks.extend(kept): LOAD_ATTR extend resolves before the iteration */
        if (!acv2_is_arr(blocks)) {
            acv2_raise_attr_method(ctx, blocks, "extend");
            json_free(sr); json_free(blocks); if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        if (!acv2_is_arr(kept)) {
            acv2_raise_not_iterable(ctx, kept);
            json_free(sr); json_free(blocks); if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        for (size_t i = 0; i < kept->c.count; i++)
            json_append(blocks, json_copy(kept->c.items[i]));
        json_free(sr);
    } else if (acv2_truthy(content)) {
        char *s = acv2_pystr(content);
        if (acv2_nonws(s)) {
            /* blocks.append(_text_block(str(content))): LOAD_ATTR append first */
            if (!acv2_is_arr(blocks)) {
                acv2_raise_attr_method(ctx, blocks, "append");
                free(s); json_free(blocks); if (relocated) json_free(relocated);
                json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
                return NULL;
            }
            json_t *ta = json_object();
            acv2_arg_str(ta, "text", s);
            json_t *tb = acv2_call(ctx, "text_block", ta);
            if (acv2_exc_pending(ctx)) {
                if (tb) json_free(tb);
                free(s); json_free(blocks); if (relocated) json_free(relocated);
                json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
                return NULL;
            }
            json_append(blocks, tb ? tb : json_null());
        }
        free(s);
    }
    /* for tc in m.get("tool_calls", []): */
    json_t *tcs = acv2_get_def(m, "tool_calls", empty_arr);
    if (!acv2_is_arr(tcs)) {
        acv2_raise_not_iterable(ctx, tcs);
        json_free(blocks); if (relocated) json_free(relocated);
        json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
        return NULL;
    }
    for (size_t i = 0; i < tcs->c.count; i++) {
        json_t *tc = tcs->c.items[i];
        if (!acv2_truthy(tc) || !acv2_is_obj(tc)) continue;
        json_t *fn = acv2_get_def(tc, "function", empty_obj);
        if (!acv2_is_obj(fn)) {
            acv2_raise_attr_get(ctx, fn);
            json_free(blocks); if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        /* blocks.append(...): LOAD_ATTR append before the argument seams */
        if (!acv2_is_arr(blocks)) {
            acv2_raise_attr_method(ctx, blocks, "append");
            json_free(blocks); if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        json_t *id_v = acv2_get_def(tc, "id", empty_str);
        json_t *name_v = acv2_get_def(fn, "name", empty_str);
        json_t *args_default = json_string("{}");
        json_t *args_v = acv2_get_def(fn, "arguments", args_default);
        json_t *pa = json_object();
        acv2_arg_node(pa, "raw", args_v);
        json_t *parsed = acv2_call(ctx, "parse_tool_args", pa);
        json_free(args_default);
        if (acv2_exc_pending(ctx)) {
            if (parsed) json_free(parsed);
            json_free(blocks); if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        json_t *tua = json_object();
        acv2_arg_node(tua, "tool_id", id_v);
        acv2_arg_node(tua, "name", name_v);
        acv2_arg_node(tua, "tool_input", parsed);
        json_t *tub = acv2_call(ctx, "tool_use_block", tua);
        if (parsed) json_free(parsed);
        if (acv2_exc_pending(ctx)) {
            if (tub) json_free(tub);
            json_free(blocks); if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        json_append(blocks, tub ? tub : json_null());
    }
    /* reasoning_content injection */
    json_t *rc = acv2_get(m, "reasoning_content");
    if (acv2_is_str(rc)) {
        json_t *ha = json_object();
        acv2_arg_node(ha, "blocks", blocks);
        json_set(ha, "types", acv2_types_array(ACV2_THINKING_TYPES));
        json_t *hb = acv2_call(ctx, "has_block_type", ha);
        if (acv2_exc_pending(ctx)) {
            if (hb) json_free(hb);
            json_free(blocks); if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        bool has = acv2_truthy(hb);
        if (hb) json_free(hb);
        if (!has) {
            if (!acv2_is_arr(blocks)) {
                acv2_raise_attr_method(ctx, blocks, "insert");
                json_free(blocks); if (relocated) json_free(relocated);
                json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
                return NULL;
            }
            json_t *th = json_object();
            json_set(th, "type", json_string("thinking"));
            json_set(th, "thinking", json_copy(rc));
            acv2_array_insert0(blocks, th);
        }
    }
    /* effective = blocks or [_text_block(_EMPTY_TEXT_PLACEHOLDER)] */
    json_t *effective;
    if (acv2_truthy(blocks)) {
        effective = blocks;
    } else {
        json_free(blocks);
        blocks = NULL;
        json_t *ta = json_object();
        acv2_arg_str(ta, "text", ACV2_EMPTY_TEXT_PLACEHOLDER);
        json_t *tb = acv2_call(ctx, "text_block", ta);
        if (acv2_exc_pending(ctx)) {
            if (tb) json_free(tb);
            if (relocated) json_free(relocated);
            json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
            return NULL;
        }
        effective = json_array();
        json_append(effective, tb ? tb : json_null());
    }
    acv2_apply_cc_seam(ctx, effective, relocated);
    if (relocated) json_free(relocated);
    if (acv2_exc_pending(ctx)) {
        json_free(effective);
        json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
        return NULL;
    }
    acv2_apply_cc_seam(ctx, effective, acv2_get(m, "cache_control"));
    if (acv2_exc_pending(ctx)) {
        json_free(effective);
        json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
        return NULL;
    }
    json_t *o = json_object();
    json_set(o, "role", json_string("assistant"));
    json_set(o, "content", effective);
    json_free(empty_str); json_free(empty_obj); json_free(empty_arr);
    return o;
}

/* PoP: acv2_strip_orphaned_tool_blocks @ agent/anthropic_message_convert.py:_strip_orphaned_tool_blocks */
static void acv2_strip_orphaned_tool_blocks(acv2_ctx *ctx, json_t *result) {
    /* Pass 1: tool_use without an adjacent result. */
    json_t *pairs = acv2_abl_seam(ctx, result);
    if (acv2_exc_pending(ctx)) { if (pairs) json_free(pairs); return; }
    if (!acv2_is_arr(pairs)) { acv2_raise_not_iterable(ctx, pairs); if (pairs) json_free(pairs); return; }
    for (size_t pi = 0; pi < pairs->c.count; pi++) {
        size_t idx = 0;
        json_t *m = acv2_abl_rebind(ctx, result, pairs->c.items[pi], &idx);
        if (!m) { json_free(pairs); return; }
        json_t *mc = json_obj_get(m, "content");
        if (!mc) { acv2_raise_key(ctx, "content"); json_free(pairs); return; }
        json_t *ba = json_object();
        acv2_arg_node(ba, "blocks", mc);
        acv2_arg_str(ba, "btype", "tool_use");
        acv2_arg_str(ba, "key", "id");
        json_t *ids = acv2_call(ctx, "block_ids", ba);
        if (acv2_exc_pending(ctx)) { if (ids) json_free(ids); json_free(pairs); return; }
        if (!acv2_truthy(ids)) { if (ids) json_free(ids); continue; }
        json_t *adj = json_array();
        if (idx + 1 < result->c.count) {
            json_t *nxt = result->c.items[idx + 1];
            if (!acv2_is_obj(nxt)) {
                acv2_raise_attr_get(ctx, nxt);
                json_free(ids); json_free(adj); json_free(pairs);
                return;
            }
            json_t *nrole = json_obj_get(nxt, "role");
            json_t *ncontent = json_obj_get(nxt, "content");
            if (acv2_is_str(nrole) && acv2_str_eq(nrole->str_val, "user") && acv2_is_arr(ncontent)) {
                json_t *aa = json_object();
                acv2_arg_node(aa, "blocks", ncontent);
                acv2_arg_str(aa, "btype", "tool_result");
                acv2_arg_str(aa, "key", "tool_use_id");
                json_t *ar = acv2_call(ctx, "block_ids", aa);
                if (acv2_exc_pending(ctx)) {
                    if (ar) json_free(ar);
                    json_free(ids); json_free(adj); json_free(pairs);
                    return;
                }
                json_free(adj);
                adj = ar ? ar : json_array();
            }
        }
        json_t *orphaned = acv2_set_diff(ids, adj);
        json_free(ids); json_free(adj);
        if (!acv2_truthy(orphaned)) { json_free(orphaned); continue; }
        json_t *kept = json_array();
        for (size_t bi = 0; bi < mc->c.count; bi++) {
            json_t *b = mc->c.items[bi];
            json_t *bta = json_object();
            acv2_arg_node(bta, "b", b);
            json_t *bt = acv2_call(ctx, "block_type", bta);
            if (acv2_exc_pending(ctx)) {
                if (bt) json_free(bt);
                json_free(kept); json_free(orphaned); json_free(pairs);
                return;
            }
            bool is_tu = acv2_is_str(bt) && acv2_str_eq(bt->str_val, "tool_use");
            if (bt) json_free(bt);
            bool drop = false;
            if (is_tu) {
                json_t *bid = json_obj_get(b, "id");
                drop = acv2_set_contains(orphaned, bid);
            }
            if (!drop) json_append(kept, json_copy(b));
        }
        if (kept->c.count != mc->c.count) {
            json_t *ha = json_object();
            acv2_arg_node(ha, "blocks", mc);
            json_set(ha, "types", acv2_types_array(ACV2_THINKING_TYPES));
            json_t *hb = acv2_call(ctx, "has_block_type", ha);
            if (acv2_exc_pending(ctx)) {
                if (hb) json_free(hb);
                json_free(kept); json_free(orphaned); json_free(pairs);
                return;
            }
            bool has = acv2_truthy(hb);
            if (hb) json_free(hb);
            if (has) json_set(m, "_thinking_signature_invalidated", json_bool(true));
        }
        if (kept->c.count == 0) {
            json_t *ta = json_object();
            acv2_arg_str(ta, "text", "(tool call removed)");
            json_t *tb = acv2_call(ctx, "text_block", ta);
            if (acv2_exc_pending(ctx)) {
                if (tb) json_free(tb);
                json_free(kept); json_free(orphaned); json_free(pairs);
                return;
            }
            json_append(kept, tb ? tb : json_null());
        }
        json_set(m, "content", kept);
        json_free(orphaned);
    }
    json_free(pairs);
    /* Pass 2: tool_result whose tool_use no longer exists anywhere. */
    json_t *surviving = json_array();
    pairs = acv2_abl_seam(ctx, result);
    if (acv2_exc_pending(ctx)) { if (pairs) json_free(pairs); json_free(surviving); return; }
    if (!acv2_is_arr(pairs)) { acv2_raise_not_iterable(ctx, pairs); if (pairs) json_free(pairs); json_free(surviving); return; }
    for (size_t pi = 0; pi < pairs->c.count; pi++) {
        size_t idx = 0;
        json_t *m = acv2_abl_rebind(ctx, result, pairs->c.items[pi], &idx);
        if (!m) { json_free(pairs); json_free(surviving); return; }
        json_t *mc = json_obj_get(m, "content");
        if (!mc) { acv2_raise_key(ctx, "content"); json_free(pairs); json_free(surviving); return; }
        json_t *ba = json_object();
        acv2_arg_node(ba, "blocks", mc);
        acv2_arg_str(ba, "btype", "tool_use");
        acv2_arg_str(ba, "key", "id");
        json_t *ids = acv2_call(ctx, "block_ids", ba);
        if (acv2_exc_pending(ctx)) { if (ids) json_free(ids); json_free(pairs); json_free(surviving); return; }
        acv2_set_union_into(surviving, ids);
        if (ids) json_free(ids);
    }
    json_free(pairs);
    for (size_t mi = 0; mi < result->c.count; mi++) {
        json_t *m = result->c.items[mi];
        if (!acv2_is_obj(m)) { acv2_raise_attr_get(ctx, m); json_free(surviving); return; }
        json_t *role = json_obj_get(m, "role");
        if (!(acv2_is_str(role) && acv2_str_eq(role->str_val, "user"))) continue;
        json_t *mc = json_obj_get(m, "content");
        if (!acv2_is_arr(mc)) continue;
        json_t *nc = json_array();
        for (size_t bi = 0; bi < mc->c.count; bi++) {
            json_t *b = mc->c.items[bi];
            json_t *bta = json_object();
            acv2_arg_node(bta, "b", b);
            json_t *bt = acv2_call(ctx, "block_type", bta);
            if (acv2_exc_pending(ctx)) {
                if (bt) json_free(bt);
                json_free(nc); json_free(surviving);
                return;
            }
            bool is_tr = acv2_is_str(bt) && acv2_str_eq(bt->str_val, "tool_result");
            if (bt) json_free(bt);
            bool keep = true;
            if (is_tr) {
                json_t *tid = json_obj_get(b, "tool_use_id");
                keep = acv2_set_contains(surviving, tid);
            }
            if (keep) json_append(nc, json_copy(b));
        }
        if (nc->c.count != mc->c.count) {
            if (nc->c.count == 0) {
                json_t *ta = json_object();
                acv2_arg_str(ta, "text", "(tool result removed)");
                json_t *tb = acv2_call(ctx, "text_block", ta);
                if (acv2_exc_pending(ctx)) {
                    if (tb) json_free(tb);
                    json_free(nc); json_free(surviving);
                    return;
                }
                json_append(nc, tb ? tb : json_null());
            }
            json_set(m, "content", nc);
        } else {
            json_free(nc);
        }
    }
    json_free(surviving);
}

/* PoP: acv2_merge_consecutive_roles @ agent/anthropic_message_convert.py:_merge_consecutive_roles */
static json_t *acv2_merge_consecutive_roles(acv2_ctx *ctx, json_t *result) {
    if (!acv2_is_arr(result)) { acv2_raise_not_iterable(ctx, result); return NULL; }
    size_t n = result->c.count;
    json_t **fx = (json_t **)malloc((n ? n : 1) * sizeof(json_t *));
    size_t fn = 0;
    for (size_t mi = 0; mi < n; mi++) {
        json_t *m = result->c.items[mi];
        bool same = false;
        if (fn > 0) {
            json_t *prev = fx[fn - 1];
            if (!acv2_is_obj(prev)) {
                acv2_raise_not_subscriptable(ctx, prev);
                free(fx); return NULL;
            }
            json_t *pr = json_obj_get(prev, "role");
            if (!pr) { acv2_raise_key(ctx, "role"); free(fx); return NULL; }
            if (!acv2_is_obj(m)) {
                acv2_raise_not_subscriptable(ctx, m);
                free(fx); return NULL;
            }
            json_t *mr = json_obj_get(m, "role");
            if (!mr) { acv2_raise_key(ctx, "role"); free(fx); return NULL; }
            same = acv2_json_eq(pr, mr);
        }
        if (!same) { fx[fn++] = m; continue; }
        json_t *mr = json_obj_get(m, "role");
        if (!(acv2_is_str(mr) && acv2_str_eq(mr->str_val, "user"))) {
            json_t *flag = json_obj_get(m, "_thinking_signature_invalidated");
            if (acv2_truthy(flag))
                json_set(fx[fn - 1], "_thinking_signature_invalidated", json_bool(true));
            json_t *mc = json_obj_get(m, "content");
            if (!mc) { acv2_raise_key(ctx, "content"); free(fx); return NULL; }
            if (acv2_is_arr(mc)) {
                json_t *sa = json_object();
                acv2_arg_node(sa, "blocks", mc);
                json_t *st = acv2_call(ctx, "strip_thinking", sa);
                if (acv2_exc_pending(ctx)) { if (st) json_free(st); free(fx); return NULL; }
                json_set(m, "content", st ? st : json_null());
            }
        }
        json_t *prev = fx[fn - 1];
        json_t *pc = json_obj_get(prev, "content");
        if (!pc) { acv2_raise_key(ctx, "content"); free(fx); return NULL; }
        json_t *mc2 = json_obj_get(m, "content");
        if (!mc2) { acv2_raise_key(ctx, "content"); free(fx); return NULL; }
        json_t *cca = json_object();
        acv2_arg_node(cca, "prev", pc);
        acv2_arg_node(cca, "curr", mc2);
        json_t *cc = acv2_call(ctx, "concat_content", cca);
        if (acv2_exc_pending(ctx)) { if (cc) json_free(cc); free(fx); return NULL; }
        json_set(prev, "content", cc ? cc : json_null());
    }
    json_t *out = json_array();
    for (size_t i = 0; i < fn; i++) json_append(out, json_copy(fx[i]));
    free(fx);
    return out;
}

/* PoP: acv2_manage_thinking_signatures @ agent/anthropic_message_convert.py:_manage_thinking_signatures */
static void acv2_manage_thinking_signatures(acv2_ctx *ctx, json_t *result,
                                            const json_t *base_url, const json_t *model) {
    json_t *ra = json_object();
    acv2_arg_node(ra, "base_url", base_url);
    acv2_arg_node(ra, "model", model);
    json_t *route = acv2_call(ctx, "anthropic_thinking_route", ra);
    if (acv2_exc_pending(ctx)) { if (route) json_free(route); return; }
    json_t *lsa = json_object();
    acv2_arg_node(lsa, "result", result);
    json_t *ls = acv2_call(ctx, "current_tool_loop_start", lsa);
    if (acv2_exc_pending(ctx)) { if (ls) json_free(ls); json_free(route); return; }
    double loop_start = acv2_is_num(ls) ? ls->num_val : 0.0;
    if (ls) json_free(ls);
    if (!acv2_is_arr(result)) {
        acv2_raise_fmt(ctx, "TypeError", "object of type '%s' has no len()", acv2_type_name(result));
        json_free(route);
        return;
    }
    double last_assistant = (double)result->c.count;
    for (size_t k = result->c.count; k > 0; k--) {
        json_t *m = result->c.items[k - 1];
        if (!acv2_is_obj(m)) { acv2_raise_attr_get(ctx, m); json_free(route); return; }
        json_t *role = json_obj_get(m, "role");
        if (acv2_is_str(role) && acv2_str_eq(role->str_val, "assistant")) {
            last_assistant = (double)(k - 1);
            break;
        }
    }
    double loop = loop_start < last_assistant ? loop_start : last_assistant;
    json_t *pa = json_object();
    acv2_arg_node(pa, "route", route);
    acv2_arg_node(pa, "model", model);
    json_t *pp = acv2_call(ctx, "route_replays_prior_thinking", pa);
    if (acv2_exc_pending(ctx)) { if (pp) json_free(pp); json_free(route); return; }
    bool preserve = acv2_truthy(pp);
    if (pp) json_free(pp);
    json_t *pairs = acv2_abl_seam(ctx, result);
    if (acv2_exc_pending(ctx)) { if (pairs) json_free(pairs); json_free(route); return; }
    if (!acv2_is_arr(pairs)) {
        acv2_raise_not_iterable(ctx, pairs);
        if (pairs) json_free(pairs);
        json_free(route);
        return;
    }
    for (size_t pi = 0; pi < pairs->c.count; pi++) {
        size_t idx = 0;
        json_t *m = acv2_abl_rebind(ctx, result, pairs->c.items[pi], &idx);
        if (!m) { json_free(pairs); json_free(route); return; }
        json_t *mc = json_obj_get(m, "content");
        if (!mc) { acv2_raise_key(ctx, "content"); json_free(pairs); json_free(route); return; }
        bool is_kimi = acv2_is_str(route) && acv2_str_eq(route->str_val, "kimi");
        bool is_deepseek = acv2_is_str(route) && acv2_str_eq(route->str_val, "deepseek");
        bool is_third = acv2_is_str(route) && acv2_str_eq(route->str_val, "third_party");
        if (is_kimi) {
            /* pass — shared cleanup below still strips cache markers + the flag */
        } else if (is_deepseek) {
            json_t *nc = json_array();
            for (size_t bi = 0; bi < mc->c.count; bi++) {
                json_t *b = mc->c.items[bi];
                json_t *bta = json_object();
                acv2_arg_node(bta, "b", b);
                json_t *bt = acv2_call(ctx, "block_type", bta);
                if (acv2_exc_pending(ctx)) {
                    if (bt) json_free(bt);
                    json_free(nc); json_free(pairs); json_free(route);
                    return;
                }
                bool thinking = acv2_in_thinking(bt);
                if (bt) json_free(bt);
                bool keep = true;
                if (thinking) {
                    json_t *sig = json_obj_get(b, "signature");
                    bool signed_v = acv2_truthy(sig);
                    if (!signed_v) signed_v = acv2_truthy(json_obj_get(b, "data"));
                    keep = !signed_v;
                }
                if (keep) json_append(nc, json_copy(b));
            }
            if (nc->c.count == 0) {
                json_t *ta = json_object();
                acv2_arg_str(ta, "text", "(empty)");
                json_t *tb = acv2_call(ctx, "text_block", ta);
                if (acv2_exc_pending(ctx)) {
                    if (tb) json_free(tb);
                    json_free(nc); json_free(pairs); json_free(route);
                    return;
                }
                json_append(nc, tb ? tb : json_null());
            }
            json_set(m, "content", nc);
        } else if (is_third || ((double)idx < loop && !preserve)) {
            json_t *sa = json_object();
            acv2_arg_node(sa, "blocks", mc);
            json_t *st = acv2_call(ctx, "strip_thinking", sa);
            if (acv2_exc_pending(ctx)) { if (st) json_free(st); json_free(pairs); json_free(route); return; }
            if (!acv2_truthy(st)) {
                if (st) json_free(st);
                json_t *ta = json_object();
                acv2_arg_str(ta, "text", "(thinking elided)");
                json_t *tb = acv2_call(ctx, "text_block", ta);
                if (acv2_exc_pending(ctx)) {
                    if (tb) json_free(tb);
                    json_free(pairs); json_free(route);
                    return;
                }
                st = json_array();
                json_append(st, tb ? tb : json_null());
            }
            json_set(m, "content", st);
        } else {
            bool dead = acv2_truthy(json_obj_get(m, "_thinking_signature_invalidated"));
            json_t *ka = json_object();
            acv2_arg_node(ka, "content", mc);
            json_set(ka, "signature_dead", json_bool(dead));
            json_t *kv = acv2_call(ctx, "keep_valid_thinking", ka);
            if (acv2_exc_pending(ctx)) { if (kv) json_free(kv); json_free(pairs); json_free(route); return; }
            if (!acv2_truthy(kv)) {
                if (kv) json_free(kv);
                json_t *ta = json_object();
                acv2_arg_str(ta, "text", "(empty)");
                json_t *tb = acv2_call(ctx, "text_block", ta);
                if (acv2_exc_pending(ctx)) {
                    if (tb) json_free(tb);
                    json_free(pairs); json_free(route);
                    return;
                }
                kv = json_array();
                json_append(kv, tb ? tb : json_null());
            }
            json_set(m, "content", kv);
        }
        /* cache_control on thinking blocks interferes with signature validation. */
        json_t *newmc = json_obj_get(m, "content");
        if (acv2_is_arr(newmc)) {
            for (size_t bi = 0; bi < newmc->c.count; bi++) {
                json_t *b = newmc->c.items[bi];
                json_t *bta = json_object();
                acv2_arg_node(bta, "b", b);
                json_t *bt = acv2_call(ctx, "block_type", bta);
                if (acv2_exc_pending(ctx)) {
                    if (bt) json_free(bt);
                    json_free(pairs); json_free(route);
                    return;
                }
                bool th = acv2_in_thinking(bt);
                if (bt) json_free(bt);
                if (th) json_obj_del(b, "cache_control");
            }
        }
        json_obj_del(m, "_thinking_signature_invalidated");
    }
    json_free(pairs);
    json_free(route);
}

/* PoP: acv2_evict_old_screenshots @ agent/anthropic_message_convert.py:_evict_old_screenshots */
typedef struct acv2_img_ref { json_t *block; json_t *data; } acv2_img_ref;
typedef struct acv2_carrier { json_t *block; acv2_img_ref *images; size_t n; } acv2_carrier;

static void acv2_evict_cleanup(acv2_img_ref *reserved, size_t rn, acv2_carrier *carriers, size_t cn) {
    for (size_t i = 0; i < rn; i++) if (reserved[i].data) json_free(reserved[i].data);
    free(reserved);
    for (size_t i = 0; i < cn; i++) {
        for (size_t j = 0; j < carriers[i].n; j++)
            if (carriers[i].images[j].data) json_free(carriers[i].images[j].data);
        free(carriers[i].images);
    }
    free(carriers);
}

static void acv2_evict_old_screenshots(acv2_ctx *ctx, json_t *result) {
    if (!acv2_is_arr(result)) { acv2_raise_not_iterable(ctx, result); return; }
    size_t cap = 16;
    acv2_img_ref *reserved = (acv2_img_ref *)malloc(cap * sizeof(acv2_img_ref));
    size_t rn = 0;
    acv2_carrier *carriers = (acv2_carrier *)malloc(cap * sizeof(acv2_carrier));
    size_t cn = 0;
    /* reserved: forward walk, image blocks at the message top level */
    for (size_t mi = 0; mi < result->c.count; mi++) {
        json_t *msg = result->c.items[mi];
        if (!acv2_is_obj(msg)) {
            acv2_raise_attr_get(ctx, msg);
            acv2_evict_cleanup(reserved, rn, carriers, cn);
            return;
        }
        json_t *mc = json_obj_get(msg, "content");
        if (!acv2_is_arr(mc)) continue;
        for (size_t bi = 0; bi < mc->c.count; bi++) {
            json_t *b = mc->c.items[bi];
            json_t *bta = json_object();
            acv2_arg_node(bta, "b", b);
            json_t *bt = acv2_call(ctx, "block_type", bta);
            if (acv2_exc_pending(ctx)) {
                if (bt) json_free(bt);
                acv2_evict_cleanup(reserved, rn, carriers, cn);
                return;
            }
            bool is_img = acv2_is_str(bt) && acv2_str_eq(bt->str_val, "image");
            if (bt) json_free(bt);
            if (!is_img) continue;
            json_t *da = json_object();
            acv2_arg_node(da, "block", b);
            json_t *data = acv2_call(ctx, "image_source_data", da);
            if (acv2_exc_pending(ctx)) {
                if (data) json_free(data);
                acv2_evict_cleanup(reserved, rn, carriers, cn);
                return;
            }
            if (rn == cap) { cap *= 2; reserved = (acv2_img_ref *)realloc(reserved, cap * sizeof(acv2_img_ref)); carriers = (acv2_carrier *)realloc(carriers, cap * sizeof(acv2_carrier)); }
            reserved[rn].block = b;
            reserved[rn].data = data;
            rn++;
        }
    }
    /* carriers: newest -> oldest tool_result blocks carrying inner images */
    for (size_t k = result->c.count; k > 0; k--) {
        json_t *msg = result->c.items[k - 1];
        json_t *mc = json_obj_get(msg, "content");
        if (!acv2_is_arr(mc)) continue;
        for (size_t j = mc->c.count; j > 0; j--) {
            json_t *b = mc->c.items[j - 1];
            json_t *bta = json_object();
            acv2_arg_node(bta, "b", b);
            json_t *bt = acv2_call(ctx, "block_type", bta);
            if (acv2_exc_pending(ctx)) {
                if (bt) json_free(bt);
                acv2_evict_cleanup(reserved, rn, carriers, cn);
                return;
            }
            bool is_tr = acv2_is_str(bt) && acv2_str_eq(bt->str_val, "tool_result");
            if (bt) json_free(bt);
            if (!is_tr) continue;
            json_t *bc = json_obj_get(b, "content");
            if (!acv2_is_arr(bc)) continue;
            json_t *ha = json_object();
            acv2_arg_node(ha, "blocks", bc);
            json_t *img_types = json_array();
            json_append(img_types, json_string("image"));
            json_set(ha, "types", img_types);
            json_t *hb = acv2_call(ctx, "has_block_type", ha);
            if (acv2_exc_pending(ctx)) {
                if (hb) json_free(hb);
                acv2_evict_cleanup(reserved, rn, carriers, cn);
                return;
            }
            bool has = acv2_truthy(hb);
            if (hb) json_free(hb);
            if (!has) continue;
            if (cn == cap) { cap *= 2; reserved = (acv2_img_ref *)realloc(reserved, cap * sizeof(acv2_img_ref)); carriers = (acv2_carrier *)realloc(carriers, cap * sizeof(acv2_carrier)); }
            carriers[cn].block = b;
            carriers[cn].images = (acv2_img_ref *)malloc((bc->c.count ? bc->c.count : 1) * sizeof(acv2_img_ref));
            carriers[cn].n = 0;
            for (size_t ii = 0; ii < bc->c.count; ii++) {
                json_t *ib = bc->c.items[ii];
                json_t *ita = json_object();
                acv2_arg_node(ita, "b", ib);
                json_t *ibt = acv2_call(ctx, "block_type", ita);
                if (acv2_exc_pending(ctx)) {
                    if (ibt) json_free(ibt);
                    acv2_evict_cleanup(reserved, rn, carriers, cn);
                    return;
                }
                bool is_img = acv2_is_str(ibt) && acv2_str_eq(ibt->str_val, "image");
                if (ibt) json_free(ibt);
                if (!is_img) continue;
                json_t *da = json_object();
                acv2_arg_node(da, "block", ib);
                json_t *data = acv2_call(ctx, "image_source_data", da);
                if (acv2_exc_pending(ctx)) {
                    if (data) json_free(data);
                    acv2_evict_cleanup(reserved, rn, carriers, cn);
                    return;
                }
                carriers[cn].images[carriers[cn].n].block = ib;
                carriers[cn].images[carriers[cn].n].data = data;
                carriers[cn].n++;
            }
            cn++;
        }
    }
    /* many_image_safe = images_all_within_many_image_limit([*reserved data, *carrier data]) */
    json_t *datas = json_array();
    for (size_t i = 0; i < rn; i++)
        json_append(datas, reserved[i].data ? json_copy(reserved[i].data) : json_null());
    for (size_t i = 0; i < cn; i++)
        for (size_t j = 0; j < carriers[i].n; j++)
            json_append(datas, carriers[i].images[j].data ? json_copy(carriers[i].images[j].data) : json_null());
    json_t *ma = json_object();
    acv2_arg_node(ma, "datas", datas);
    json_free(datas);
    json_t *mis = acv2_call(ctx, "images_all_within_many_image_limit", ma);
    if (acv2_exc_pending(ctx)) {
        if (mis) json_free(mis);
        acv2_evict_cleanup(reserved, rn, carriers, cn);
        return;
    }
    /* retire = outbound_image_retire_count(counts, len(reserved), ...) */
    json_t *counts = json_array();
    json_t *cbnf = json_array();
    for (size_t i = 0; i < cn; i++) {
        json_append(counts, json_number((double)carriers[i].n));
        double bytes = 0;
        for (size_t j = 0; j < carriers[i].n; j++) {
            json_t *d = carriers[i].images[j].data;
            if (acv2_truthy(d) && acv2_is_str(d)) bytes += (double)acv2_pylen(d->str_val);
        }
        json_append(cbnf, json_number(bytes));
    }
    double reserved_bytes = 0;
    for (size_t i = 0; i < rn; i++) {
        json_t *d = reserved[i].data;
        if (acv2_truthy(d) && acv2_is_str(d)) reserved_bytes += (double)acv2_pylen(d->str_val);
    }
    json_t *oa = json_object();
    acv2_arg_node(oa, "counts", counts);
    json_set(oa, "reserved_count", json_number((double)rn));
    acv2_arg_node(oa, "carrier_bytes_newest_first", cbnf);
    json_set(oa, "reserved_bytes", json_number(reserved_bytes));
    acv2_arg_node(oa, "many_image_safe", mis);
    json_free(counts);
    json_free(cbnf);
    json_t *retire_v = acv2_call(ctx, "outbound_image_retire_count", oa);
    if (mis) json_free(mis);
    if (acv2_exc_pending(ctx)) {
        if (retire_v) json_free(retire_v);
        acv2_evict_cleanup(reserved, rn, carriers, cn);
        return;
    }
    double retire = acv2_is_num(retire_v) ? retire_v->num_val : 0.0;
    if (retire_v) json_free(retire_v);
    double s = (double)cn - retire;
    size_t start = s <= 0 ? 0 : (s >= (double)cn ? cn : (size_t)s);
    for (size_t k = start; k < cn; k++) {
        json_t *ta = json_object();
        acv2_arg_str(ta, "text", "[screenshot removed to save context]");
        json_t *ph = acv2_call(ctx, "text_block", ta);
        if (acv2_exc_pending(ctx)) {
            if (ph) json_free(ph);
            acv2_evict_cleanup(reserved, rn, carriers, cn);
            return;
        }
        json_t *bc = json_obj_get(carriers[k].block, "content");
        json_t *nc = json_array();
        for (size_t ii = 0; ii < bc->c.count; ii++) {
            json_t *ib = bc->c.items[ii];
            json_t *ita = json_object();
            acv2_arg_node(ita, "b", ib);
            json_t *ibt = acv2_call(ctx, "block_type", ita);
            if (acv2_exc_pending(ctx)) {
                if (ibt) json_free(ibt);
                if (ph) json_free(ph);
                json_free(nc);
                acv2_evict_cleanup(reserved, rn, carriers, cn);
                return;
            }
            bool is_img = acv2_is_str(ibt) && acv2_str_eq(ibt->str_val, "image");
            if (ibt) json_free(ibt);
            if (is_img) json_append(nc, ph ? json_copy(ph) : json_null());
            else json_append(nc, json_copy(ib));
        }
        json_set(carriers[k].block, "content", nc);
        if (ph) json_free(ph);
    }
    acv2_evict_cleanup(reserved, rn, carriers, cn);
}

/* PoP: acv2_ensure_leading_user_turn @ agent/anthropic_message_convert.py:_ensure_leading_user_turn */
static void acv2_ensure_leading_user_turn(acv2_ctx *ctx, json_t *result) {
    if (!acv2_truthy(result)) return;
    if (!acv2_is_arr(result)) { acv2_raise_not_subscriptable(ctx, result); return; }
    if (result->c.count == 0) return;
    json_t *first = result->c.items[0];
    if (!acv2_is_obj(first)) { acv2_raise_attr_get(ctx, first); return; }
    json_t *role = json_obj_get(first, "role");
    if (acv2_is_str(role) && acv2_str_eq(role->str_val, "user")) return;
    json_t *ta = json_object();
    acv2_arg_str(ta, "text", ACV2_EMPTY_TEXT_PLACEHOLDER);
    json_t *ph = acv2_call(ctx, "text_block", ta);
    if (acv2_exc_pending(ctx)) { if (ph) json_free(ph); return; }
    json_t *um = json_object();
    json_set(um, "role", json_string("user"));
    json_t *ua = json_array();
    json_append(ua, ph ? ph : json_null());
    json_set(um, "content", ua);
    acv2_array_insert0(result, um);
}

/* PoP: acv2_scrub_blank_text_blocks @ agent/anthropic_message_convert.py:_scrub_blank_text_blocks */
static void acv2_scrub_blank_text_blocks(acv2_ctx *ctx, json_t *result) {
    if (!acv2_is_arr(result)) { acv2_raise_not_iterable(ctx, result); return; }
    for (size_t mi = 0; mi < result->c.count; mi++) {
        json_t *msg = result->c.items[mi];
        if (!acv2_is_obj(msg)) continue;
        json_t *role = json_obj_get(msg, "role");
        json_t *content = json_obj_get(msg, "content");
        if (!acv2_is_arr(content) || content->c.count == 0) continue;
        const char *ph = (acv2_is_str(role) && acv2_str_eq(role->str_val, "assistant"))
            ? ACV2_EMPTY_TEXT_PLACEHOLDER : "(empty message)";
        json_t *fixed = acv2_fix_blank_text_blocks_in_list(ctx, content, ph, (double)mi, role, "content");
        if (acv2_exc_pending(ctx)) { if (fixed) json_free(fixed); return; }
        json_set(msg, "content", fixed);
        for (size_t bi = 0; bi < fixed->c.count; bi++) {
            json_t *blk = fixed->c.items[bi];
            json_t *bta = json_object();
            acv2_arg_node(bta, "b", blk);
            json_t *bt = acv2_call(ctx, "block_type", bta);
            if (acv2_exc_pending(ctx)) { if (bt) json_free(bt); return; }
            bool is_tr = acv2_is_str(bt) && acv2_str_eq(bt->str_val, "tool_result");
            if (bt) json_free(bt);
            json_t *inner = is_tr ? json_obj_get(blk, "content") : NULL;
            if (acv2_is_arr(inner) && inner->c.count > 0) {
                json_t *ifixed = acv2_fix_blank_text_blocks_in_list(ctx, inner, "(no output)",
                                                                    (double)mi, role, "tool_result");
                if (acv2_exc_pending(ctx)) { if (ifixed) json_free(ifixed); return; }
                json_set(blk, "content", ifixed);
            }
        }
    }
}

/* PoP: acv2_convert_messages_to_anthropic @ agent/anthropic_message_convert.py:convert_messages_to_anthropic */
static json_t *acv2_convert_messages_to_anthropic(acv2_ctx *ctx, const json_t *messages,
                                                  const json_t *base_url, const json_t *model) {
    if (!acv2_is_arr(messages)) { acv2_raise_not_iterable(ctx, messages); return NULL; }
    json_t *empty_str = json_string("");
    json_t *default_role = json_string("user");
    json_t *system = json_null();
    json_t *result = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *m = messages->c.items[i];
        if (!acv2_is_obj(m)) {
            acv2_raise_attr_get(ctx, m);
            json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
            return NULL;
        }
        json_t *role = acv2_get_def(m, "role", default_role);
        if (acv2_is_str(role) && acv2_str_eq(role->str_val, "system")) {
            json_t *cv = acv2_get_def(m, "content", empty_str);
            json_t *sa = json_object();
            acv2_arg_node(sa, "content", cv);
            json_t *sr = acv2_call(ctx, "convert_system_content", sa);
            if (acv2_exc_pending(ctx)) {
                if (sr) json_free(sr);
                json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
                return NULL;
            }
            json_free(system);
            system = sr ? sr : json_null();
        } else if (acv2_is_str(role) && acv2_str_eq(role->str_val, "assistant")) {
            json_t *am = acv2_convert_assistant_message(ctx, m);
            if (acv2_exc_pending(ctx)) {
                if (am) json_free(am);
                json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
                return NULL;
            }
            json_append(result, am ? am : json_null());
        } else if (acv2_is_str(role) && acv2_str_eq(role->str_val, "tool")) {
            acv2_convert_tool_message_to_result(ctx, result, m);
            if (acv2_exc_pending(ctx)) {
                json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
                return NULL;
            }
        } else {
            json_t *cv = acv2_get_def(m, "content", empty_str);
            json_t *um = acv2_convert_user_message(ctx, cv);
            if (acv2_exc_pending(ctx)) {
                if (um) json_free(um);
                json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
                return NULL;
            }
            json_append(result, um ? um : json_null());
        }
    }
    acv2_strip_orphaned_tool_blocks(ctx, result);
    if (acv2_exc_pending(ctx)) {
        json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
        return NULL;
    }
    json_t *merged = acv2_merge_consecutive_roles(ctx, result);
    if (acv2_exc_pending(ctx)) {
        if (merged) json_free(merged);
        json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
        return NULL;
    }
    json_free(result);
    result = merged;
    acv2_ensure_leading_user_turn(ctx, result);
    if (acv2_exc_pending(ctx)) {
        json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
        return NULL;
    }
    acv2_manage_thinking_signatures(ctx, result, base_url, model);
    if (acv2_exc_pending(ctx)) {
        json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
        return NULL;
    }
    acv2_evict_old_screenshots(ctx, result);
    if (acv2_exc_pending(ctx)) {
        json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
        return NULL;
    }
    acv2_scrub_blank_text_blocks(ctx, result);
    if (acv2_exc_pending(ctx)) {
        json_free(system); json_free(result); json_free(empty_str); json_free(default_role);
        return NULL;
    }
    json_t *out = json_array();
    json_append(out, system);
    json_append(out, result);
    json_free(empty_str); json_free(default_role);
    return out;
}