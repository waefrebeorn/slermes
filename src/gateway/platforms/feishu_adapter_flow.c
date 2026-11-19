/*
 * feishu_adapter_flow.c — Feishu/Lark adapter message-normalization FLOW
 * cores and leaves of plugins/platforms/feishu/adapter.py (hermes-agent).
 *
 * Ports the DECISION SEQUENCES of the inbound message normalization tree:
 * normalize_feishu_message -> payload load / mentions map / post parse /
 * media-ref / merge-forward / share-chat / interactive branches, the post
 * payload resolver chain, the recursive post-element renderer trio, the
 * card/forward text walkers, the mention helpers and the markdown/text
 * leaves. Call order, short-circuit branches, early returns, accumulator
 * mutation order (image_keys / media_refs / mentions_map) and exception
 * propagation mirror the live Python.
 *
 * Universal seam: every SEAMED sibling call enters through ONE callback
 * fa_call(ctx, name, args). The harness wires ctx->io to a recording stub
 * fed the case's scripted results; the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL == Python None; {"__raise__":cls,"message":str(exc)} == a
 * raised exception). A missing/exhausted script is a fail-closed
 * RuntimeError on both sides. BaseException kinds (SystemExit /
 * KeyboardInterrupt / CancelledError / GeneratorExit) pass through every
 * catch site (this cluster's only catches are _coerce_int's
 * except (TypeError, ValueError) and _load_feishu_payload's
 * except JSONDecodeError, both narrower than Exception, so scripted
 * BaseException raises from seams always propagate).
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL (run inline, direct C call): normalize_feishu_message,
 *    parse_feishu_post_payload, _render_post_element, _join_nested_posts,
 *    _render_nested_post, _build_markdown_post_payload,
 *    _coerce_required_int, _strip_markdown_to_plain_text, _walk_nodes
 *    (internal iterator), _FeishuBotIdentity.matches (internal method).
 *  SEAMED (fa_call, scripted): every other ported function plus the foreign
 *    import strip_markdown (gateway/platforms/helpers.py). A REAL function
 *    calls its REAL children directly and its SEAMED children via fa_call;
 *    a SEAMED function driven as its own case target runs its real body,
 *    which likewise fa_calls its SEAMED children. The recursive renderer
 *    trio and parse are REAL because they mutate shared accumulators
 *    (image_keys / media_refs / mentions_map) by reference; seaming them
 *    would hide mutation history from the scripted replay.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and fa_-prefixed.
 *
 * Representation contract (shared with tests/oracle_feishu_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - Dataclasses project to dicts (dataclasses.asdict field set):
 *   FeishuMentionRef {name, open_id, is_all, is_self};
 *   FeishuPostMediaRef {file_key, file_name, resource_type};
 *   FeishuPostParseResult {text_content, image_keys, media_refs};
 *   FeishuNormalizedMessage {raw_type, text_content, preferred_message_type,
 *     image_keys, media_refs, mentions, relation_kind, metadata}.
 * - frozenset returns (_sender_identity) project to SORTED lists; tuples
 *   (_extract_mention_ids) to lists; the _walk_nodes generator to a list.
 * - Attribute doubles (sender / mention / bot) are JSON objects whose keys
 *   are the live attributes; a missing key models a missing attribute
 *   (getattr default). mention.id: JSON string = str id; JSON object =
 *   UserId double {open_id, user_id}; null/absent = None.
 * - mentions_map travels as {placeholder: FeishuMentionRef-dict}; null ==
 *   None. It is mutated in place by the renderer (@_all) and normalize.
 * - Accumulator state: parse_feishu_post_payload / _render_post_element /
 *   _join_nested_posts / _render_nested_post cases emit the final
 *   image_keys / media_refs / mentions_map as out.state; all other funcs
 *   emit state {} (their observable output is entirely in ret).
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral
 *   floats to int). json.dumps in _build_markdown_post_payload uses Python
 *   default separators (", ", ": ") and ensure_ascii=False with \b/\f short
 *   escapes — fa_json_dumps reproduces that exactly (libjson's compact
 *   serializer is NOT used for that return).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - str()-computed positions (_render_text_element text, _first_non_empty_text
 *   scalar values, _render_code_block_element language/code, tag.lower())
 *   carry str / int / bool / None only — no containers, no non-integral
 *   floats (Python str(5.0)=="5.0" vs C %.0f=="5"; container repr out of
 *   domain). tag.lower() corpus is ASCII.
 * - _coerce_int string values use ASCII decimal digits, optional sign,
 *   ASCII whitespace padding and between-digit underscores; Unicode Nd
 *   digits and bytes are out of domain. int() TypeError/ValueError are
 *   caught -> default, so their messages are not compared.
 * - _load_feishu_payload raw_content is json.dumps-generated valid JSON or
 *   an obviously-invalid string; NaN/Infinity, duplicate keys, raw control
 *   chars and surrogate escapes are out of domain (libjson json_parse
 *   matches json.loads on this corpus).
 * - _normalize_feishu_text / mention placeholders use ASCII digits (\d) and
 *   the Unicode White_Space set for \s / strip (fa_is_ws_cp).
 * - _build_markdown_post_rows content.splitlines() is modelled on ASCII
 *   \n / \r\n / \r plus the Unicode line boundaries \v \f \x1c \x1d \x1e
 *   \x85 \u2028 \u2029 (fa_splitlines).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct fa_ctx {
    json_t *state;      /* owned; emitted as out.state (accumulator funcs) */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} fa_ctx_t;

/* ── basic helpers ── */
static bool fa_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *fa_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool fa_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool fa_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *fa_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool fa_str_eq(const json_t *v, const char *s) {
    const char *t = fa_str(v);
    return t && s && strcmp(t, s) == 0;
}
/* Array element string reader (json_get_str is an OBJECT accessor). */
static const char *fa_arr_str(const json_t *arr, size_t i, const char *def) {
    const json_t *v = json_get(arr, i);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static json_t *fa_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *fa_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return fa_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *fa_call(fa_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = fa_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = fa_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = fa_make_exc(raise);
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
/* Convenience: build a fresh args object (caller hands it to fa_call). */
static json_t *fa_args(void) { return json_object(); }
static void fa_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void fa_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void fa_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void fa_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void fa_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── string helpers ── */
static char *fa_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* Decode UTF-8 to a malloc'd codepoint array; *n gets the count. Invalid
 * bytes decode as U+FFFD (fixture corpus is valid UTF-8). */
static uint32_t *fa_utf8_decode(const char *s, size_t *n) {
    if (!s) s = "";
    size_t len = strlen(s);
    uint32_t *cps = malloc((len + 1) * sizeof(uint32_t));
    size_t k = 0;
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; size_t adv;
        if (c < 0x80) { cp = c; adv = 1; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i+1] & 0x3Fu); adv = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i+1] & 0x3Fu) << 6)
               | ((unsigned char)s[i+2] & 0x3Fu); adv = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < len) {
            cp = ((c & 0x07u) << 18) | (((unsigned char)s[i+1] & 0x3Fu) << 12)
               | (((unsigned char)s[i+2] & 0x3Fu) << 6) | ((unsigned char)s[i+3] & 0x3Fu); adv = 4;
        } else { cp = 0xFFFD; adv = 1; }
        cps[k++] = cp; i += adv;
    }
    *n = k;
    return cps;
}
static void fa_utf8_putcp(char **q, uint32_t cp) {
    char *o = *q;
    if (cp < 0x80) { *o++ = (char)cp; }
    else if (cp < 0x800) {
        *o++ = (char)(0xC0 | (cp >> 6)); *o++ = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        *o++ = (char)(0xE0 | (cp >> 12)); *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *o++ = (char)(0xF0 | (cp >> 18)); *o++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *o++ = (char)(0x80 | (cp & 0x3F));
    }
    *q = o;
}
static char *fa_utf8_encode(const uint32_t *cps, size_t n) {
    char *o = malloc(n * 4 + 1);
    if (!o) return NULL;
    char *q = o;
    for (size_t i = 0; i < n; i++) fa_utf8_putcp(&q, cps[i]);
    *q = '\0';
    return o;
}
static size_t fa_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
/* Python str.strip() White_Space set (also re \s for the fixture corpus). */
static bool fa_is_ws_cp(uint32_t c) {
    if (c == 0x20 || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) return true;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return true;
    if (c >= 0x2000 && c <= 0x200A) return true;
    if (c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000) return true;
    return false;
}
static char *fa_strip(const char *s) {
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && fa_is_ws_cp(cps[a])) a++;
    while (b > a && fa_is_ws_cp(cps[b-1])) b--;
    char *o = fa_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
static char *fa_lstrip(const char *s) {
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(s, &n);
    size_t a = 0;
    while (a < n && fa_is_ws_cp(cps[a])) a++;
    char *o = fa_utf8_encode(cps + a, n - a);
    free(cps);
    return o;
}
static char *fa_rstrip(const char *s) {
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(s, &n);
    size_t b = n;
    while (b > 0 && fa_is_ws_cp(cps[b-1])) b--;
    char *o = fa_utf8_encode(cps, b);
    free(cps);
    return o;
}
/* ASCII lower (tag.lower() corpus is ASCII). */
static char *fa_lower_ascii(const char *s) {
    char *o = fa_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* Replace every occurrence of `from` with `to` (byte-level; non-empty from). */
static char *fa_replace(const char *s, const char *from, const char *to) {
    if (!s) s = "";
    size_t flen = strlen(from), tlen = strlen(to);
    if (flen == 0) return fa_strdup(s);
    size_t count = 0;
    for (const char *p = s; (p = strstr(p, from)) != NULL; p += flen) count++;
    size_t olen = strlen(s) + count * (tlen - flen);
    char *o = malloc(olen + 1);
    if (!o) return NULL;
    char *q = o;
    const char *p = s;
    while (*p) {
        if (strncmp(p, from, flen) == 0) { memcpy(q, to, tlen); q += tlen; p += flen; }
        else *q++ = *p++;
    }
    *q = '\0';
    return o;
}
static bool fa_startswith(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}
static bool fa_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}
/* Python str() of a JSON scalar in the fixture corpus (str/int/bool/None). */
static char *fa_py_str_scalar(const json_t *v) {
    if (!v || v->type == JSON_NULL) return fa_strdup("None");
    if (v->type == JSON_STRING) return fa_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return fa_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val); /* out-of-domain */
        return fa_strdup(buf);
    }
    return fa_strdup(""); /* containers out of str() domain */
}
/* str(x or "") for the text slots: falsy -> "", else str(scalar). */
static char *fa_str_or_empty(const json_t *v) {
    if (!fa_truthy(v)) return fa_strdup("");
    return fa_py_str_scalar(v);
}
/* ── growable string buffer ── */
typedef struct fa_buf { char *s; size_t len, cap; } fa_buf_t;
static void fa_buf_init(fa_buf_t *b) { b->cap = 64; b->len = 0; b->s = malloc(b->cap); if (b->s) b->s[0] = '\0'; }
static void fa_buf_append(fa_buf_t *b, const char *s, size_t n) {
    if (!b->s) return;
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        char *nb = realloc(b->s, b->cap);
        if (!nb) { free(b->s); b->s = NULL; return; }
        b->s = nb;
    }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}
static void fa_buf_str(fa_buf_t *b, const char *s) { if (s) fa_buf_append(b, s, strlen(s)); }
static void fa_buf_cp(fa_buf_t *b, uint32_t cp) { char tmp[4]; char *q = tmp; fa_utf8_putcp(&q, cp); fa_buf_append(b, tmp, (size_t)(q - tmp)); }
static char *fa_buf_take(fa_buf_t *b) { char *s = b->s ? b->s : fa_strdup(""); b->s = NULL; return s; }

/* ── markdown / text algorithm cores ── */
/* _MARKDOWN_SPECIAL_CHARS_RE: prepend a backslash before each special char. */
static char *fa_escape_markdown(const char *text) {
    if (!text) text = "";
    fa_buf_t b; fa_buf_init(&b);
    for (const char *p = text; *p; p++) {
        char c = *p;
        if (c == '\\' || c == '`' || c == '*' || c == '_' || c == '{' || c == '}'
            || c == '[' || c == ']' || c == '(' || c == ')' || c == '#' || c == '+'
            || c == '-' || c == '!' || c == '|' || c == '>' || c == '~')
            fa_buf_append(&b, "\\", 1);
        fa_buf_append(&b, &c, 1);
    }
    return fa_buf_take(&b);
}
/* _wrap_inline_code core. */
static char *fa_wrap_inline_code(const char *text) {
    if (!text) text = "";
    size_t max_run = 0, run = 0;
    for (const char *p = text; *p; p++) {
        if (*p == '`') { run++; if (run > max_run) max_run = run; }
        else run = 0;
    }
    size_t fence_n = max_run + 1;
    bool pad = (text[0] == '`') || fa_endswith(text, "`");
    fa_buf_t b; fa_buf_init(&b);
    for (size_t i = 0; i < fence_n; i++) fa_buf_append(&b, "`", 1);
    if (pad) fa_buf_append(&b, " ", 1);
    fa_buf_str(&b, text);
    if (pad) fa_buf_append(&b, " ", 1);
    for (size_t i = 0; i < fence_n; i++) fa_buf_append(&b, "`", 1);
    return fa_buf_take(&b);
}
/* str.splitlines() boundaries (no trailing empty line for a final boundary). */
static bool fa_is_linebound(uint32_t c) {
    return c == 0x0A || c == 0x0B || c == 0x0C || c == 0x0D
        || c == 0x1C || c == 0x1D || c == 0x1E || c == 0x85
        || c == 0x2028 || c == 0x2029;
}
static json_t *fa_splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(s, &n);
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (fa_is_linebound(cps[i])) {
            char *line = fa_utf8_encode(cps + start, i - start);
            json_append(arr, json_string(line ? line : ""));
            free(line);
            if (cps[i] == 0x0D && i + 1 < n && cps[i+1] == 0x0A) i++;
            start = i + 1;
        }
    }
    if (start < n) {
        char *line = fa_utf8_encode(cps + start, n - start);
        json_append(arr, json_string(line ? line : ""));
        free(line);
    }
    free(cps);
    return arr;
}
/* str.split("\n"): split ONLY on \n, keep every piece incl. trailing empties. */
static json_t *fa_split_nl(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    const char *start = s;
    for (const char *p = s; ; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t n = (size_t)(p - start);
            char *line = malloc(n + 1); memcpy(line, start, n); line[n] = '\0';
            json_append(arr, json_string(line));
            free(line);
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return arr;
}
/* Collapse each maximal \s run to one space, then strip (per-line step). */
static char *fa_ws_collapse_strip(const char *line) {
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(line, &n);
    fa_buf_t b; fa_buf_init(&b);
    bool in_ws = false;
    for (size_t i = 0; i < n; i++) {
        if (fa_is_ws_cp(cps[i])) { if (!in_ws) { fa_buf_append(&b, " ", 1); in_ws = true; } }
        else { fa_buf_cp(&b, cps[i]); in_ws = false; }
    }
    free(cps);
    char *collapsed = fa_buf_take(&b);
    char *stripped = fa_strip(collapsed);
    free(collapsed);
    return stripped;
}
/* _normalize_feishu_text core. mentions_map: json object {key: ref-dict} or NULL. */
static char *fa_normalize_feishu_text(const char *text, const json_t *mentions_map) {
    if (!text) text = "";
    /* step 1: @_user_\d+ substitution (ASCII digits, left-to-right). */
    fa_buf_t b; fa_buf_init(&b);
    const char *p = text;
    while (*p) {
        if (strncmp(p, "@_user_", 7) == 0 && isdigit((unsigned char)p[7])) {
            const char *d = p + 7;
            while (isdigit((unsigned char)*d)) d++;
            size_t klen = (size_t)(d - p);
            char *key = malloc(klen + 1); memcpy(key, p, klen); key[klen] = '\0';
            const json_t *ref = mentions_map ? json_obj_get(mentions_map, key) : NULL;
            if (ref && ref->type == JSON_OBJECT) {
                const char *nm = json_get_str(ref, "name", "");
                const char *oid = json_get_str(ref, "open_id", "");
                const char *disp = (nm && nm[0]) ? nm : (oid && oid[0]) ? oid : "user";
                fa_buf_append(&b, "@", 1);
                fa_buf_str(&b, disp);
            } else {
                fa_buf_append(&b, " ", 1);
            }
            free(key);
            p = d;
        } else {
            fa_buf_append(&b, p, 1);
            p++;
        }
    }
    char *cleaned = fa_buf_take(&b);
    /* step 2: @_all -> @all */
    char *t = fa_replace(cleaned, "@_all", "@all"); free(cleaned); cleaned = t;
    /* step 3: CRLF / CR -> LF */
    t = fa_replace(cleaned, "\r\n", "\n"); free(cleaned); cleaned = t;
    t = fa_replace(cleaned, "\r", "\n"); free(cleaned); cleaned = t;
    /* step 4: per-line \s+ -> " " then strip, join with \n (split("\n")) */
    json_t *lines = fa_split_nl(cleaned);
    free(cleaned);
    fa_buf_t b2; fa_buf_init(&b2);
    for (size_t i = 0; i < json_len(lines); i++) {
        char *ls = fa_ws_collapse_strip(fa_arr_str(lines, i, ""));
        if (i > 0) fa_buf_append(&b2, "\n", 1);
        fa_buf_str(&b2, ls ? ls : "");
        free(ls);
    }
    json_free(lines);
    cleaned = fa_buf_take(&b2);
    /* step 5: drop empty lines (split("\n")) */
    lines = fa_split_nl(cleaned);
    free(cleaned);
    fa_buf_t b3; fa_buf_init(&b3);
    bool first = true;
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *ln = fa_arr_str(lines, i, "");
        if (!ln || !ln[0]) continue;
        if (!first) fa_buf_append(&b3, "\n", 1);
        fa_buf_str(&b3, ln);
        first = false;
    }
    json_free(lines);
    cleaned = fa_buf_take(&b3);
    /* step 6: [ \t]{2,} -> " " (a single space/tab is preserved verbatim) */
    fa_buf_t b4; fa_buf_init(&b4);
    size_t run = 0;
    for (const char *q = cleaned; *q; q++) {
        if (*q == ' ' || *q == '\t') { run++; }
        else {
            if (run >= 2) fa_buf_append(&b4, " ", 1);
            else if (run == 1) fa_buf_append(&b4, q - 1, 1);
            run = 0;
            fa_buf_append(&b4, q, 1);
        }
    }
    if (run >= 2) fa_buf_append(&b4, " ", 1);
    else if (run == 1) fa_buf_append(&b4, cleaned + strlen(cleaned) - 1, 1);
    free(cleaned);
    char *collapsed = fa_buf_take(&b4);
    /* step 7: strip */
    char *out = fa_strip(collapsed);
    free(collapsed);
    return out;
}

/* ── dataclass projection builders ── */
static json_t *fa_mention_ref(const char *name, const char *open_id, bool is_all, bool is_self) {
    json_t *r = json_object();
    json_set_hdr(r, "name", name ? name : "");
    json_set_hdr(r, "open_id", open_id ? open_id : "");
    json_set(r, "is_all", json_bool(is_all));
    json_set(r, "is_self", json_bool(is_self));
    return r;
}
static json_t *fa_media_ref(const char *file_key, const char *file_name, const char *resource_type) {
    json_t *r = json_object();
    json_set_hdr(r, "file_key", file_key ? file_key : "");
    json_set_hdr(r, "file_name", file_name ? file_name : "");
    json_set_hdr(r, "resource_type", resource_type ? resource_type : "file");
    return r;
}
static json_t *fa_parse_result(const char *text_content, json_t *image_keys, json_t *media_refs) {
    json_t *r = json_object();
    json_set_hdr(r, "text_content", text_content ? text_content : "");
    json_set(r, "image_keys", image_keys ? image_keys : json_array());
    json_set(r, "media_refs", media_refs ? media_refs : json_array());
    return r;
}
static json_t *fa_normalized(const char *raw_type, const char *text_content,
                             const char *preferred, json_t *image_keys, json_t *media_refs,
                             json_t *mentions, const char *relation_kind, json_t *metadata) {
    json_t *r = json_object();
    json_set_hdr(r, "raw_type", raw_type ? raw_type : "");
    json_set_hdr(r, "text_content", text_content ? text_content : "");
    json_set_hdr(r, "preferred_message_type", preferred ? preferred : "text");
    json_set(r, "image_keys", image_keys ? image_keys : json_array());
    json_set(r, "media_refs", media_refs ? media_refs : json_array());
    json_set(r, "mentions", mentions ? mentions : json_array());
    json_set_hdr(r, "relation_kind", relation_kind ? relation_kind : "plain");
    json_set(r, "metadata", metadata ? metadata : json_object());
    return r;
}
/* mentions_map (json obj of ref-dicts) -> list of ref-dicts in insertion order. */
static json_t *fa_map_values(const json_t *map) {
    json_t *arr = json_array();
    if (map && map->type == JSON_OBJECT)
        for (size_t i = 0; i < map->c.count; i++) json_append(arr, json_copy(map->c.items[i]));
    return arr;
}
/* ── int() coercion (ASCII-digit corpus; TypeError/ValueError caught upstream) ── */
/* int() strips Unicode White_Space EXCEPT \x1c-\x1f (unlike str.strip()). */
static bool fa_is_int_ws_cp(uint32_t c) {
    return fa_is_ws_cp(c) && !(c >= 0x1C && c <= 0x1F);
}
static char *fa_strip_int(const char *s) {
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && fa_is_int_ws_cp(cps[a])) a++;
    while (b > a && fa_is_int_ws_cp(cps[b-1])) b--;
    char *o = fa_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
/* status: 0 = ok, 1 = TypeError, 2 = ValueError. */
static long long fa_int_value(const json_t *v, int *status) {
    *status = 0;
    if (!v || v->type == JSON_NULL) { *status = 1; return 0; }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val; /* trunc toward zero */
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) { *status = 1; return 0; }
    if (v->type == JSON_STRING) {
        char *s = fa_strip_int(v->str_val ? v->str_val : "");
        const char *p = s;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        /* validate: >=1 ASCII digit; every '_' between two digits */
        const char *q = p;
        size_t ndigit = 0;
        bool ok = true;
        while (*q) {
            if (isdigit((unsigned char)*q)) { ndigit++; q++; }
            else if (*q == '_') {
                if (q == p || !isdigit((unsigned char)q[-1]) || !isdigit((unsigned char)q[1])) { ok = false; break; }
                q++;
            } else { ok = false; break; }
        }
        if (!ok || ndigit == 0) { free(s); *status = 2; return 0; }
        char *clean = malloc(strlen(p) + 1);
        size_t k = 0;
        for (const char *r = p; *r; r++) if (*r != '_') clean[k++] = *r;
        clean[k] = '\0';
        long long val = strtoll(clean, NULL, 10);
        free(clean); free(s);
        return neg ? -val : val;
    }
    *status = 1;
    return 0;
}

/* ── leaf case functions ── */

/* PoP: fa__is_bot_sender @ plugins/platforms/feishu/adapter.py:_is_bot_sender */
static json_t *fa__is_bot_sender(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *sender = json_obj_get(args, "sender");
    const char *st = json_get_str(sender, "sender_type", "");
    return json_bool(strcmp(st, "bot") == 0 || strcmp(st, "app") == 0);
}

/* PoP: fa__sender_identity @ plugins/platforms/feishu/adapter.py:_sender_identity */
static int fa_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *fa__sender_identity(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *sender = json_obj_get(args, "sender");
    const json_t *sid = json_obj_get(sender, "sender_id");
    json_t *out = json_array();
    if (!sid || sid->type == JSON_NULL) return out;
    const char *keys[] = {"open_id", "user_id", "union_id"};
    char *vals[3]; size_t nv = 0;
    for (int i = 0; i < 3; i++) {
        const char *v = json_get_str(sid, keys[i], "");
        if (v && v[0]) {
            bool dup = false;
            for (size_t j = 0; j < nv; j++) if (strcmp(vals[j], v) == 0) { dup = true; break; }
            if (!dup) vals[nv++] = fa_strdup(v);
        }
    }
    if (nv > 1) qsort(vals, nv, sizeof(char *), fa_cmp_str);
    for (size_t i = 0; i < nv; i++) { json_append(out, json_string(vals[i])); free(vals[i]); }
    return out;
}

/* PoP: fa__escape_markdown_text @ plugins/platforms/feishu/adapter.py:_escape_markdown_text */
static json_t *fa__escape_markdown_text(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = fa_escape_markdown(json_get_str(args, "text", ""));
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: fa__to_boolean @ plugins/platforms/feishu/adapter.py:_to_boolean */
static json_t *fa__to_boolean(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = json_obj_get(args, "value");
    bool b = false;
    if (v) {
        if (v->type == JSON_BOOL) b = v->bool_val;
        else if (v->type == JSON_NUMBER) b = (v->num_val == 1.0);
        else if (v->type == JSON_STRING) b = fa_str_eq(v, "true");
    }
    return json_bool(b);
}

/* PoP: fa__is_style_enabled @ plugins/platforms/feishu/adapter.py:_is_style_enabled */
static json_t *fa__is_style_enabled(fa_ctx_t *ctx, const json_t *args) {
    const json_t *style = json_obj_get(args, "style");
    if (!fa_truthy(style)) return json_bool(false);
    const char *key = json_get_str(args, "key", "");
    const json_t *v = json_obj_get(style, key);
    json_t *a2 = fa_args();
    fa_set_copy(a2, "value", v);
    json_t *r = fa_call(ctx, "_to_boolean", a2);
    if (fa_is_raise(r)) return r;
    bool b = (r && r->type == JSON_BOOL) ? r->bool_val : false;
    json_free(r);
    return json_bool(b);
}

/* PoP: fa__wrap_inline_code @ plugins/platforms/feishu/adapter.py:_wrap_inline_code */
static json_t *fa__wrap_inline_code(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = fa_wrap_inline_code(json_get_str(args, "text", ""));
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: fa__sanitize_fence_language @ plugins/platforms/feishu/adapter.py:_sanitize_fence_language */
static json_t *fa__sanitize_fence_language(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = fa_strip(json_get_str(args, "language", ""));
    char *t = fa_replace(s, "\n", " "); free(s);
    char *u = fa_replace(t, "\r", " "); free(t);
    json_t *r = json_string(u ? u : "");
    free(u);
    return r;
}

/* PoP: fa__coerce_int @ plugins/platforms/feishu/adapter.py:_coerce_int */
static json_t *fa__coerce_int(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *value = json_obj_get(args, "value");
    const json_t *def = json_obj_get(args, "default");
    long long min_value = (long long)json_get_num(args, "min_value", 0);
    int status = 0;
    long long parsed = fa_int_value(value, &status);
    if (status != 0) return def ? json_copy(def) : json_null();
    if (parsed >= min_value) return json_number((double)parsed);
    return def ? json_copy(def) : json_null();
}

/* PoP: fa__to_post_payload @ plugins/platforms/feishu/adapter.py:_to_post_payload */
static json_t *fa__to_post_payload(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *cand = json_obj_get(args, "candidate");
    if (!cand || cand->type != JSON_OBJECT) return json_object();
    const json_t *content = json_obj_get(cand, "content");
    if (!content || content->type != JSON_ARRAY) return json_object();
    char *title = fa_str_or_empty(json_obj_get(cand, "title"));
    const json_t *files = json_obj_get(cand, "files");
    json_t *r = json_object();
    json_set_hdr(r, "title", title ? title : "");
    json_set(r, "content", json_copy(content));
    json_set(r, "files", (files && files->type == JSON_ARRAY) ? json_copy(files) : json_array());
    free(title);
    return r;
}

/* PoP: fa__load_feishu_payload @ plugins/platforms/feishu/adapter.py:_load_feishu_payload */
static json_t *fa__load_feishu_payload(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *raw = json_get_str(args, "raw_content", "");
    if (!raw || !raw[0]) return json_object();
    json_t *parsed = json_parse(raw, NULL);
    if (!parsed) {
        json_t *r = json_object();
        json_set_hdr(r, "text", raw);
        return r;
    }
    if (parsed->type == JSON_OBJECT) return parsed;
    json_t *r = json_object();
    json_set(r, "content", parsed);
    return r;
}

/* PoP: fa__normalize_feishu_text @ plugins/platforms/feishu/adapter.py:_normalize_feishu_text */
static json_t *fa__normalize_feishu_text(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *mm = json_obj_get(args, "mentions_map");
    char *s = fa_normalize_feishu_text(json_get_str(args, "text", ""), mm);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: fa__unique_lines @ plugins/platforms/feishu/adapter.py:_unique_lines */
static json_t *fa__unique_lines(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *lines = json_obj_get(args, "lines");
    json_t *out = json_array();
    json_t *seen = json_object();
    size_t n = lines ? json_len(lines) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *ln = fa_arr_str(lines, i, "");
        if (!ln || !ln[0]) continue;
        if (json_obj_get(seen, ln)) continue;
        json_set(seen, ln, json_bool(true));
        json_append(out, json_string(ln));
    }
    json_free(seen);
    return out;
}
/* ── codepoint-set helpers for mention stripping ── */
static bool fa_is_mention_boundary(uint32_t c) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return true;
    if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?') return true;
    if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') return true;
    if (c == '<' || c == '>' || c == '"' || c == '\'' || c == '`') return true;
    /* CJK punctuation 、，。；：！？ */
    if (c == 0x3001 || c == 0xFF0C || c == 0x3002 || c == 0xFF1B || c == 0xFF1A
        || c == 0xFF01 || c == 0xFF1F) return true;
    return false;
}
static bool fa_is_terminal_punct(uint32_t c) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return true;
    if (c == '.' || c == '!' || c == '?') return true;
    if (c == 0x3002 || c == 0xFF01 || c == 0xFF1F) return true; /* 。！？ */
    return false;
}

/* PoP: fa__extract_mention_ids @ plugins/platforms/feishu/adapter.py:_extract_mention_ids */
static json_t *fa__extract_mention_ids(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *mention = json_obj_get(args, "mention");
    const json_t *id = json_obj_get(mention, "id");
    json_t *out = json_array();
    if (id && id->type == JSON_STRING) {
        char *it_raw = fa_str_or_empty(json_obj_get(mention, "id_type"));
        char *it = fa_lower_ascii(it_raw); free(it_raw);
        if (strcmp(it, "open_id") == 0) { json_append(out, json_copy(id)); json_append(out, json_string("")); }
        else if (strcmp(it, "user_id") == 0) { json_append(out, json_string("")); json_append(out, json_copy(id)); }
        else { json_append(out, json_string("")); json_append(out, json_string("")); }
        free(it);
        return out;
    }
    if (!id || id->type == JSON_NULL) {
        json_append(out, json_string("")); json_append(out, json_string(""));
        return out;
    }
    /* nested UserId double */
    char *oid = fa_str_or_empty(json_obj_get(id, "open_id"));
    char *uid = fa_str_or_empty(json_obj_get(id, "user_id"));
    json_append(out, json_string(oid ? oid : ""));
    json_append(out, json_string(uid ? uid : ""));
    free(oid); free(uid);
    return out;
}

/* PoP: fa__build_mention_hint @ plugins/platforms/feishu/adapter.py:_build_mention_hint */
static json_t *fa__build_mention_hint(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *mentions = json_obj_get(args, "mentions");
    json_t *parts = json_array();
    json_t *seen = json_array(); /* signature objects {is_all, open_id, name} */
    size_t n = mentions ? json_len(mentions) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *ref = json_get(mentions, i);
        if (!ref || ref->type != JSON_OBJECT) continue;
        bool is_self = json_get_bool(ref, "is_self", false);
        if (is_self) continue;
        bool is_all = json_get_bool(ref, "is_all", false);
        const char *oid = json_get_str(ref, "open_id", "");
        const char *nm = json_get_str(ref, "name", "");
        bool dup = false;
        for (size_t j = 0; j < json_len(seen); j++) {
            const json_t *s = json_get(seen, j);
            if (json_get_bool(s, "is_all", false) == is_all
                && strcmp(json_get_str(s, "open_id", ""), oid) == 0
                && strcmp(json_get_str(s, "name", ""), nm) == 0) { dup = true; break; }
        }
        if (dup) continue;
        json_t *sig = json_object();
        json_set(sig, "is_all", json_bool(is_all));
        json_set_hdr(sig, "open_id", oid);
        json_set_hdr(sig, "name", nm);
        json_append(seen, sig);
        fa_buf_t pb; fa_buf_init(&pb);
        if (is_all) fa_buf_str(&pb, "@all");
        else if (oid && oid[0]) {
            fa_buf_str(&pb, (nm && nm[0]) ? nm : "unknown");
            fa_buf_str(&pb, " (open_id=");
            fa_buf_str(&pb, oid);
            fa_buf_str(&pb, ")");
        } else fa_buf_str(&pb, (nm && nm[0]) ? nm : "unknown");
        char *part = fa_buf_take(&pb);
        json_append(parts, json_string(part ? part : ""));
        free(part);
    }
    json_free(seen);
    json_t *r;
    if (json_len(parts) == 0) r = json_string("");
    else {
        fa_buf_t b; fa_buf_init(&b);
        fa_buf_str(&b, "[Mentioned: ");
        for (size_t i = 0; i < json_len(parts); i++) {
            if (i > 0) fa_buf_str(&b, ", ");
            fa_buf_str(&b, fa_arr_str(parts, i, ""));
        }
        fa_buf_str(&b, "]");
        char *s = fa_buf_take(&b);
        r = json_string(s ? s : "");
        free(s);
    }
    json_free(parts);
    return r;
}

/* PoP: fa__strip_edge_self_mentions @ plugins/platforms/feishu/adapter.py:_strip_edge_self_mentions */
static json_t *fa__strip_edge_self_mentions(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    const json_t *mentions = json_obj_get(args, "mentions");
    if (!text || !text[0]) return json_string(text ? text : "");
    /* self_names = ["@" + (name or open_id or "user") for ref if is_self] */
    uint32_t *names[64]; size_t name_n[64]; size_t nn = 0;
    size_t mn = mentions ? json_len(mentions) : 0;
    for (size_t i = 0; i < mn && nn < 64; i++) {
        const json_t *ref = json_get(mentions, i);
        if (!ref || !json_get_bool(ref, "is_self", false)) continue;
        const char *nm = json_get_str(ref, "name", "");
        const char *oid = json_get_str(ref, "open_id", "");
        const char *disp = (nm && nm[0]) ? nm : (oid && oid[0]) ? oid : "user";
        fa_buf_t nb; fa_buf_init(&nb);
        fa_buf_str(&nb, "@"); fa_buf_str(&nb, disp);
        char *nms = fa_buf_take(&nb);
        names[nn] = fa_utf8_decode(nms, &name_n[nn]);
        free(nms);
        nn++;
    }
    if (nn == 0) return json_string(text);
    size_t rem_n = 0;
    uint32_t *rem = fa_utf8_decode(text, &rem_n);
    /* remaining = text.lstrip() */
    size_t start = 0;
    while (start < rem_n && fa_is_ws_cp(rem[start])) start++;
    if (start > 0) { memmove(rem, rem + start, (rem_n - start) * sizeof(uint32_t)); rem_n -= start; }
    /* leading loop */
    for (;;) {
        bool matched = false;
        for (size_t k = 0; k < nn; k++) {
            if (rem_n < name_n[k] || memcmp(rem, names[k], name_n[k] * sizeof(uint32_t)) != 0) continue;
            size_t after_n = rem_n - name_n[k];
            uint32_t *after = rem + name_n[k];
            if (after_n > 0 && !fa_is_mention_boundary(after[0])) continue;
            /* remaining = after.lstrip() */
            size_t s2 = 0;
            while (s2 < after_n && fa_is_ws_cp(after[s2])) s2++;
            memmove(rem, after + s2, (after_n - s2) * sizeof(uint32_t));
            rem_n = after_n - s2;
            matched = true;
            break;
        }
        if (!matched) break;
    }
    /* trailing loop */
    for (;;) {
        size_t i = rem_n;
        while (i > 0 && fa_is_terminal_punct(rem[i-1])) i--;
        size_t body_n = i;
        bool matched = false;
        for (size_t k = 0; k < nn; k++) {
            if (body_n < name_n[k]) continue;
            if (memcmp(rem + body_n - name_n[k], names[k], name_n[k] * sizeof(uint32_t)) != 0) continue;
            /* remaining = body[:-len(nm)].rstrip() + tail */
            size_t nb = body_n - name_n[k];
            while (nb > 0 && fa_is_ws_cp(rem[nb-1])) nb--;
            size_t tail_n = rem_n - i;
            uint32_t *nrem = malloc((nb + tail_n + 1) * sizeof(uint32_t));
            if (nb) memcpy(nrem, rem, nb * sizeof(uint32_t));
            if (tail_n) memcpy(nrem + nb, rem + i, tail_n * sizeof(uint32_t));
            free(rem);
            rem = nrem; rem_n = nb + tail_n;
            matched = true;
            break;
        }
        if (!matched) break;
    }
    char *out = fa_utf8_encode(rem, rem_n);
    free(rem);
    for (size_t k = 0; k < nn; k++) free(names[k]);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* PoP: fa__build_markdown_post_rows @ plugins/platforms/feishu/adapter.py:_build_markdown_post_rows */
static json_t *fa_md_row(const char *text) {
    json_t *row = json_array();
    json_t *el = json_object();
    json_set_hdr(el, "tag", "md");
    json_set_hdr(el, "text", text ? text : "");
    json_append(row, el);
    return row;
}
static bool fa_fence_close_match(const char *stripped) { return strcmp(stripped, "```") == 0; }
static bool fa_fence_open_match(const char *stripped) {
    return fa_startswith(stripped, "```") && strchr(stripped + 3, '`') == NULL;
}
static json_t *fa__build_markdown_post_rows(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *content = json_get_str(args, "content", "");
    if (!content || !content[0]) {
        json_t *rows = json_array(); json_append(rows, fa_md_row("")); return rows;
    }
    if (!strstr(content, "```")) {
        json_t *rows = json_array(); json_append(rows, fa_md_row(content)); return rows;
    }
    json_t *rows = json_array();
    json_t *current = json_array();
    bool in_code = false;
    json_t *lines = fa_splitlines(content);
    /* _flush_current inline: segment = "\n".join(current); if strip non-empty append row */
    #define FA_FLUSH() do { \
        fa_buf_t fb; fa_buf_init(&fb); \
        for (size_t fi = 0; fi < json_len(current); fi++) { \
            if (fi > 0) fa_buf_str(&fb, "\n"); \
            fa_buf_str(&fb, fa_arr_str(current, fi, "")); \
        } \
        char *seg = fa_buf_take(&fb); \
        char *st = fa_strip(seg); \
        if (st && st[0]) json_append(rows, fa_md_row(seg)); \
        free(st); free(seg); \
        json_free(current); current = json_array(); \
    } while (0)
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *raw_line = fa_arr_str(lines, i, "");
        char *stripped = fa_strip(raw_line);
        bool is_fence = in_code ? fa_fence_close_match(stripped) : fa_fence_open_match(stripped);
        free(stripped);
        if (is_fence && !in_code) FA_FLUSH();
        json_append(current, json_string(raw_line));
        if (is_fence) {
            in_code = !in_code;
            if (!in_code) FA_FLUSH();
        }
    }
    FA_FLUSH();
    #undef FA_FLUSH
    json_free(lines);
    json_free(current);
    if (json_len(rows) == 0) {
        json_free(rows);
        rows = json_array(); json_append(rows, fa_md_row(content));
    }
    return rows;
}

/* PoP: fa_matches @ plugins/platforms/feishu/adapter.py:matches */
static json_t *fa_matches(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *bot = json_obj_get(args, "bot");
    const char *open_id = json_get_str(args, "open_id", "");
    const char *user_id = json_get_str(args, "user_id", "");
    const char *name = json_get_str(args, "name", "");
    const char *b_oid = json_get_str(bot, "open_id", "");
    const char *b_uid = json_get_str(bot, "user_id", "");
    const char *b_nm = json_get_str(bot, "name", "");
    if (open_id[0] && b_oid[0]) return json_bool(strcmp(open_id, b_oid) == 0);
    if (user_id[0] && b_uid[0]) return json_bool(strcmp(user_id, b_uid) == 0);
    return json_bool(b_nm[0] && strcmp(name, b_nm) == 0);
}

/* PoP: fa__walk_nodes @ plugins/platforms/feishu/adapter.py:_walk_nodes */
static void fa_walk_collect(const json_t *value, json_t *arr) {
    if (!value) return;
    if (value->type == JSON_OBJECT) {
        json_append(arr, json_copy(value));
        for (size_t i = 0; i < value->c.count; i++) fa_walk_collect(value->c.items[i], arr);
    } else if (value->type == JSON_ARRAY) {
        for (size_t i = 0; i < value->c.count; i++) fa_walk_collect(value->c.items[i], arr);
    }
}
static json_t *fa__walk_nodes(fa_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *arr = json_array();
    fa_walk_collect(json_obj_get(args, "value"), arr);
    return arr;
}
/* ── field stringification helpers (str(element.get(k, def)) semantics) ── */
/* str(element.get(key, def)): present -> str(value); absent -> str(def). */
static char *fa_get_str_py(const json_t *el, const char *key, const char *def) {
    const json_t *node = json_obj_get(el, key);
    if (node) return fa_py_str_scalar(node);
    return fa_strdup(def ? def : "");
}
/* str(element.get(key, def) or ""): falsy inner -> "". */
static char *fa_str_or_empty_field(const json_t *el, const char *key, const char *def) {
    const json_t *node = json_obj_get(el, key);
    if (node) return fa_truthy(node) ? fa_py_str_scalar(node) : fa_strdup("");
    return (def && def[0]) ? fa_strdup(def) : fa_strdup("");
}
static char *fa_strip_free(char *s) { char *t = fa_strip(s ? s : ""); free(s); return t; }

/* PoP: fa__render_text_element @ plugins/platforms/feishu/adapter.py:_render_text_element */
static json_t *fa__render_text_element(fa_ctx_t *ctx, const json_t *args) {
    const json_t *element = json_obj_get(args, "element");
    char *text = fa_str_or_empty_field(element, "text", "");
    const json_t *style = json_obj_get(element, "style");
    const json_t *style_dict = (style && style->type == JSON_OBJECT) ? style : NULL;
    /* code branch */
    json_t *a = fa_args();
    fa_set_copy(a, "style", style_dict);
    fa_set_s(a, "key", "code");
    json_t *r = fa_call(ctx, "_is_style_enabled", a);
    if (fa_is_raise(r)) { free(text); return r; }
    bool code_en = (r && r->type == JSON_BOOL) ? r->bool_val : false;
    json_free(r);
    if (code_en) {
        json_t *wa = fa_args(); fa_set_s(wa, "text", text); free(text);
        return fa_call(ctx, "_wrap_inline_code", wa);
    }
    if (!text[0]) { free(text); return json_string(""); }
    static const char *keys[] = {"bold", "italic", "underline", "strikethrough"};
    static const char *pre[] = {"**", "*", "<u>", "~~"};
    static const char *suf[] = {"**", "*", "</u>", "~~"};
    char *rendered = text;
    for (int i = 0; i < 4; i++) {
        json_t *sa = fa_args();
        fa_set_copy(sa, "style", style_dict);
        fa_set_s(sa, "key", keys[i]);
        json_t *sr = fa_call(ctx, "_is_style_enabled", sa);
        if (fa_is_raise(sr)) { free(rendered); return sr; }
        bool en = (sr && sr->type == JSON_BOOL) ? sr->bool_val : false;
        json_free(sr);
        if (en) {
            fa_buf_t b; fa_buf_init(&b);
            fa_buf_str(&b, pre[i]); fa_buf_str(&b, rendered); fa_buf_str(&b, suf[i]);
            free(rendered);
            rendered = fa_buf_take(&b);
        }
    }
    json_t *out = json_string(rendered ? rendered : "");
    free(rendered);
    return out;
}

/* PoP: fa__render_code_block_element @ plugins/platforms/feishu/adapter.py:_render_code_block_element */
static json_t *fa__render_code_block_element(fa_ctx_t *ctx, const json_t *args) {
    const json_t *element = json_obj_get(args, "element");
    char *lang_a = fa_str_or_empty_field(element, "language", "");
    char *lang_input;
    if (lang_a[0]) lang_input = lang_a;
    else { free(lang_a); lang_input = fa_str_or_empty_field(element, "lang", ""); }
    json_t *la = fa_args(); fa_set_s(la, "language", lang_input); free(lang_input);
    json_t *lr = fa_call(ctx, "_sanitize_fence_language", la);
    if (fa_is_raise(lr)) return lr;
    const char *language = fa_str(lr) ? fa_str(lr) : "";
    char *code_a = fa_str_or_empty_field(element, "text", "");
    char *code_raw;
    if (code_a[0]) code_raw = code_a;
    else { free(code_a); code_raw = fa_str_or_empty_field(element, "content", ""); }
    char *code = fa_replace(code_raw, "\r\n", "\n"); free(code_raw);
    const char *trailing = fa_endswith(code, "\n") ? "" : "\n";
    fa_buf_t b; fa_buf_init(&b);
    fa_buf_str(&b, "```"); fa_buf_str(&b, language); fa_buf_str(&b, "\n");
    fa_buf_str(&b, code); fa_buf_str(&b, trailing); fa_buf_str(&b, "```");
    free(code);
    json_free(lr);
    char *s = fa_buf_take(&b);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* ── recursive post-element renderer trio (REAL cores) ── */
static json_t *fa_render_post_element(fa_ctx_t *ctx, const json_t *element,
                                      json_t *ik, json_t *mr, json_t *mm);
static json_t *fa_join_nested_posts(fa_ctx_t *ctx, const json_t *values,
                                    json_t *ik, json_t *mr, json_t *mm);
static json_t *fa_render_nested_post(fa_ctx_t *ctx, const json_t *value,
                                     json_t *ik, json_t *mr, json_t *mm);

static json_t *fa_render_post_element(fa_ctx_t *ctx, const json_t *element,
                                      json_t *ik, json_t *mr, json_t *mm) {
    if (element && element->type == JSON_STRING) return json_copy(element);
    if (!element || element->type != JSON_OBJECT) return json_string("");
    char *tag_raw = fa_get_str_py(element, "tag", "");
    char *tag_s = fa_strip_free(tag_raw);
    char *tag = fa_lower_ascii(tag_s); free(tag_s);
    json_t *out = NULL;
    if (strcmp(tag, "br") == 0) out = json_string("\n");
    else if (strcmp(tag, "hr") == 0 || strcmp(tag, "divider") == 0) out = json_string("\n\n---\n\n");
    else if (strcmp(tag, "text") == 0) {
        json_t *a = fa_args(); fa_set_copy(a, "element", element);
        out = fa_call(ctx, "_render_text_element", a);
    } else if (strcmp(tag, "code_block") == 0 || strcmp(tag, "pre") == 0) {
        json_t *a = fa_args(); fa_set_copy(a, "element", element);
        out = fa_call(ctx, "_render_code_block_element", a);
    } else if (strcmp(tag, "a") == 0) {
        char *href = fa_strip_free(fa_get_str_py(element, "href", ""));
        char *label = fa_strip_free(fa_str_or_empty_field(element, "text", href));
        if (!label[0]) { free(href); free(label); free(tag); return json_string(""); }
        json_t *ea = fa_args(); fa_set_s(ea, "text", label);
        json_t *er = fa_call(ctx, "_escape_markdown_text", ea);
        if (fa_is_raise(er)) { free(href); free(label); free(tag); return er; }
        const char *esc = fa_str(er) ? fa_str(er) : "";
        fa_buf_t b; fa_buf_init(&b);
        if (href[0]) { fa_buf_str(&b, "["); fa_buf_str(&b, esc); fa_buf_str(&b, "]("); fa_buf_str(&b, href); fa_buf_str(&b, ")"); }
        else fa_buf_str(&b, esc);
        json_free(er); free(href); free(label);
        char *s = fa_buf_take(&b); out = json_string(s ? s : ""); free(s);
    } else if (strcmp(tag, "at") == 0) {
        char *placeholder = fa_strip_free(fa_get_str_py(element, "user_id", ""));
        if (strcmp(placeholder, "@_all") == 0) {
            if (mm && mm->type == JSON_OBJECT && !json_obj_get(mm, "@_all"))
                json_set(mm, "@_all", fa_mention_ref("", "", true, false));
            free(placeholder); free(tag);
            return json_string("@all");
        }
        const json_t *ref = mm ? json_obj_get(mm, placeholder) : NULL;
        char *display;
        if (ref && ref->type == JSON_OBJECT) {
            const char *nm = json_get_str(ref, "name", "");
            const char *oid = json_get_str(ref, "open_id", "");
            display = fa_strdup((nm && nm[0]) ? nm : (oid && oid[0]) ? oid : "user");
        } else {
            char *un = fa_strip_free(fa_get_str_py(element, "user_name", ""));
            display = (un && un[0]) ? un : fa_strdup("user");
            if (display != un) free(un);
        }
        free(placeholder);
        json_t *ea = fa_args(); fa_set_s(ea, "text", display); free(display);
        json_t *er = fa_call(ctx, "_escape_markdown_text", ea);
        if (fa_is_raise(er)) { free(tag); return er; }
        const char *esc = fa_str(er) ? fa_str(er) : "";
        fa_buf_t b; fa_buf_init(&b); fa_buf_str(&b, "@"); fa_buf_str(&b, esc);
        json_free(er);
        char *s = fa_buf_take(&b); out = json_string(s ? s : ""); free(s);
    } else if (strcmp(tag, "img") == 0 || strcmp(tag, "image") == 0) {
        char *image_key = fa_strip_free(fa_get_str_py(element, "image_key", ""));
        if (image_key[0]) {
            bool found = false;
            for (size_t i = 0; i < json_len(ik); i++)
                if (fa_str_eq(json_get(ik, i), image_key)) { found = true; break; }
            if (!found) json_append(ik, json_string(image_key));
        }
        char *alt_a = fa_strip_free(fa_get_str_py(element, "text", ""));
        char *alt;
        if (alt_a[0]) alt = alt_a;
        else { free(alt_a); alt = fa_strip_free(fa_get_str_py(element, "alt", "")); }
        fa_buf_t b; fa_buf_init(&b);
        if (alt[0]) { fa_buf_str(&b, "[Image: "); fa_buf_str(&b, alt); fa_buf_str(&b, "]"); }
        else fa_buf_str(&b, "[Image]");
        free(image_key); free(alt);
        char *s = fa_buf_take(&b); out = json_string(s ? s : ""); free(s);
    } else if (strcmp(tag, "media") == 0 || strcmp(tag, "file") == 0
               || strcmp(tag, "audio") == 0 || strcmp(tag, "video") == 0) {
        char *file_key = fa_strip_free(fa_get_str_py(element, "file_key", ""));
        const char *nkeys[] = {"file_name", "title", "text"};
        char *file_name = fa_strdup("");
        for (int i = 0; i < 3; i++) {
            char *nm = fa_strip_free(fa_get_str_py(element, nkeys[i], ""));
            if (nm[0]) { free(file_name); file_name = nm; break; }
            free(nm);
        }
        fa_buf_t b; fa_buf_init(&b);
        if (file_name[0]) { fa_buf_str(&b, "[Attachment: "); fa_buf_str(&b, file_name); fa_buf_str(&b, "]"); }
        else fa_buf_str(&b, "[Attachment]");
        char *placeholder = fa_buf_take(&b);
        if (!file_key[0]) { free(file_key); free(file_name); free(tag); return json_string(placeholder); }
        bool found = false;
        for (size_t i = 0; i < json_len(mr); i++) {
            const json_t *ref = json_get(mr, i);
            if (fa_str_eq(json_obj_get(ref, "file_key"), file_key)) { found = true; break; }
        }
        if (!found) {
            const char *rt = (strcmp(tag, "audio") == 0 || strcmp(tag, "video") == 0) ? tag : "file";
            json_append(mr, fa_media_ref(file_key, file_name, rt));
            free(file_key); free(file_name); free(tag);
            return json_string(placeholder);
        }
        free(file_key); free(file_name); free(placeholder); free(tag);
        return json_string("");
    } else if (strcmp(tag, "emotion") == 0 || strcmp(tag, "emoji") == 0) {
        char *lab_a = fa_strip_free(fa_get_str_py(element, "text", ""));
        char *label;
        if (lab_a[0]) label = lab_a;
        else { free(lab_a); label = fa_strip_free(fa_get_str_py(element, "emoji_type", "")); }
        if (!label[0]) { free(label); free(tag); return json_string("[Emoji]"); }
        json_t *ea = fa_args(); fa_set_s(ea, "text", label); free(label);
        json_t *er = fa_call(ctx, "_escape_markdown_text", ea);
        if (fa_is_raise(er)) { free(tag); return er; }
        const char *esc = fa_str(er) ? fa_str(er) : "";
        fa_buf_t b; fa_buf_init(&b); fa_buf_str(&b, ":"); fa_buf_str(&b, esc); fa_buf_str(&b, ":");
        json_free(er);
        char *s = fa_buf_take(&b); out = json_string(s ? s : ""); free(s);
    } else if (strcmp(tag, "code") == 0) {
        char *code_a = fa_str_or_empty_field(element, "text", "");
        char *code;
        if (code_a[0]) code = code_a;
        else { free(code_a); code = fa_str_or_empty_field(element, "content", ""); }
        if (!code[0]) { free(code); free(tag); return json_string(""); }
        json_t *wa = fa_args(); fa_set_s(wa, "text", code); free(code);
        out = fa_call(ctx, "_wrap_inline_code", wa);
    } else {
        /* nested = (element.get(k) for k in text,title,content,children,elements) */
        json_t *vals = json_array();
        const char *nkeys[] = {"text", "title", "content", "children", "elements"};
        for (int i = 0; i < 5; i++) {
            const json_t *v = json_obj_get(element, nkeys[i]);
            json_append(vals, v ? json_copy(v) : json_null());
        }
        out = fa_join_nested_posts(ctx, vals, ik, mr, mm);
        json_free(vals);
    }
    free(tag);
    return out;
}

static json_t *fa_join_nested_posts(fa_ctx_t *ctx, const json_t *values,
                                    json_t *ik, json_t *mr, json_t *mm) {
    fa_buf_t b; fa_buf_init(&b);
    bool first = true;
    size_t n = values ? json_len(values) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *part = fa_render_nested_post(ctx, json_get(values, i), ik, mr, mm);
        if (fa_is_raise(part)) { free(b.s); return part; }
        const char *ps = fa_str(part);
        if (ps && ps[0]) {
            if (!first) fa_buf_str(&b, " ");
            fa_buf_str(&b, ps);
            first = false;
        }
        json_free(part);
    }
    char *s = fa_buf_take(&b);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

static json_t *fa_render_nested_post(fa_ctx_t *ctx, const json_t *value,
                                     json_t *ik, json_t *mr, json_t *mm) {
    if (value && value->type == JSON_STRING) {
        json_t *a = fa_args(); fa_set_s(a, "text", value->str_val);
        return fa_call(ctx, "_escape_markdown_text", a);
    }
    if (value && value->type == JSON_ARRAY)
        return fa_join_nested_posts(ctx, value, ik, mr, mm);
    if (value && value->type == JSON_OBJECT) {
        json_t *direct = fa_render_post_element(ctx, value, ik, mr, mm);
        if (fa_is_raise(direct)) return direct;
        const char *ds = fa_str(direct);
        if (ds && ds[0]) return direct;
        json_free(direct);
        json_t *vals = json_array();
        for (size_t i = 0; i < value->c.count; i++) json_append(vals, json_copy(value->c.items[i]));
        json_t *r = fa_join_nested_posts(ctx, vals, ik, mr, mm);
        json_free(vals);
        return r;
    }
    return json_string("");
}

/* case entries for the trio (accumulators borrowed from args, mutated in place) */
/* PoP: fa__render_post_element @ plugins/platforms/feishu/adapter.py:_render_post_element */
static json_t *fa__render_post_element(fa_ctx_t *ctx, const json_t *args) {
    json_t *ik = json_obj_get(args, "image_keys");
    json_t *mr = json_obj_get(args, "media_refs");
    json_t *mm = json_obj_get(args, "mentions_map");
    if (!ik) ik = json_array();
    if (!mr) mr = json_array();
    return fa_render_post_element(ctx, json_obj_get(args, "element"), ik, mr, mm);
}
/* PoP: fa__join_nested_posts @ plugins/platforms/feishu/adapter.py:_join_nested_posts */
static json_t *fa__join_nested_posts(fa_ctx_t *ctx, const json_t *args) {
    json_t *ik = json_obj_get(args, "image_keys");
    json_t *mr = json_obj_get(args, "media_refs");
    json_t *mm = json_obj_get(args, "mentions_map");
    if (!ik) ik = json_array();
    if (!mr) mr = json_array();
    return fa_join_nested_posts(ctx, json_obj_get(args, "values"), ik, mr, mm);
}
/* PoP: fa__render_nested_post @ plugins/platforms/feishu/adapter.py:_render_nested_post */
static json_t *fa__render_nested_post(fa_ctx_t *ctx, const json_t *args) {
    json_t *ik = json_obj_get(args, "image_keys");
    json_t *mr = json_obj_get(args, "media_refs");
    json_t *mm = json_obj_get(args, "mentions_map");
    if (!ik) ik = json_array();
    if (!mr) mr = json_array();
    return fa_render_nested_post(ctx, json_obj_get(args, "value"), ik, mr, mm);
}
/* ── Python json.dumps(obj, ensure_ascii=False) default separators ── */
static void fa_dumps_str(fa_buf_t *b, const char *s) {
    fa_buf_str(b, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"') fa_buf_str(b, "\\\"");
        else if (c == '\\') fa_buf_str(b, "\\\\");
        else if (c == '\b') fa_buf_str(b, "\\b");
        else if (c == '\f') fa_buf_str(b, "\\f");
        else if (c == '\n') fa_buf_str(b, "\\n");
        else if (c == '\r') fa_buf_str(b, "\\r");
        else if (c == '\t') fa_buf_str(b, "\\t");
        else if (c < 0x20) { char tmp[8]; snprintf(tmp, sizeof tmp, "\\u%04x", c); fa_buf_str(b, tmp); }
        else fa_buf_append(b, (const char *)&c, 1);
    }
    fa_buf_str(b, "\"");
}
static void fa_dumps_node(fa_buf_t *b, const json_t *n) {
    if (!n || n->type == JSON_NULL) { fa_buf_str(b, "null"); return; }
    switch (n->type) {
        case JSON_BOOL: fa_buf_str(b, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64];
            if (n->num_val == (double)(long long)n->num_val) snprintf(tmp, sizeof tmp, "%.0f", n->num_val);
            else snprintf(tmp, sizeof tmp, "%.17g", n->num_val);
            fa_buf_str(b, tmp); break;
        }
        case JSON_STRING: fa_dumps_str(b, n->str_val ? n->str_val : ""); break;
        case JSON_ARRAY:
            fa_buf_str(b, "[");
            for (size_t i = 0; i < n->c.count; i++) {
                if (i > 0) fa_buf_str(b, ", ");
                fa_dumps_node(b, n->c.items[i]);
            }
            fa_buf_str(b, "]"); break;
        case JSON_OBJECT:
            fa_buf_str(b, "{");
            for (size_t i = 0; i < n->c.count; i++) {
                if (i > 0) fa_buf_str(b, ", ");
                fa_dumps_str(b, n->c.keys[i]);
                fa_buf_str(b, ": ");
                fa_dumps_node(b, n->c.items[i]);
            }
            fa_buf_str(b, "}"); break;
        default: fa_buf_str(b, "null"); break;
    }
}
static char *fa_json_dumps(const json_t *n) {
    fa_buf_t b; fa_buf_init(&b);
    fa_dumps_node(&b, n);
    return fa_buf_take(&b);
}

/* ── parse_feishu_post_payload (REAL core; mutates mm via the renderer) ── */
#define FA_FALLBACK_POST_TEXT "[Rich text message]"
static json_t *fa_parse_post_payload(fa_ctx_t *ctx, const json_t *payload, json_t *mm) {
    json_t *resolved = NULL, *ik = NULL, *mr = NULL, *parts = NULL, *result = NULL, *rr = NULL;
    json_t *a = fa_args(); fa_set_copy(a, "payload", payload);
    resolved = fa_call(ctx, "_resolve_post_payload", a);
    if (fa_is_raise(resolved)) { rr = resolved; resolved = NULL; goto done; }
    if (!fa_truthy(resolved)) {
        result = fa_parse_result(FA_FALLBACK_POST_TEXT, json_array(), json_array());
        goto done;
    }
    ik = json_array(); mr = json_array(); parts = json_array();
    /* title */
    {
        char *title_s = fa_strip_free(fa_get_str_py(resolved, "title", ""));
        json_t *ta = fa_args(); fa_set_s(ta, "text", title_s); fa_set_null(ta, "mentions_map"); free(title_s);
        json_t *nt = fa_call(ctx, "_normalize_feishu_text", ta);
        if (fa_is_raise(nt)) { rr = nt; goto done; }
        const char *title = fa_str(nt);
        if (title && title[0]) json_append(parts, json_string(title));
        json_free(nt);
    }
    /* content rows */
    {
        const json_t *content = json_obj_get(resolved, "content");
        if (!fa_truthy(content)) content = NULL;
        size_t nrows = content ? json_len(content) : 0;
        for (size_t i = 0; i < nrows; i++) {
            const json_t *row = json_get(content, i);
            if (!row || row->type != JSON_ARRAY) continue;
            fa_buf_t rb; fa_buf_init(&rb);
            for (size_t j = 0; j < json_len(row); j++) {
                json_t *pr = fa_render_post_element(ctx, json_get(row, j), ik, mr, mm);
                if (fa_is_raise(pr)) { free(rb.s); rr = pr; goto done; }
                const char *ps = fa_str(pr);
                if (ps) fa_buf_str(&rb, ps);
                json_free(pr);
            }
            char *joined = fa_buf_take(&rb);
            json_t *na = fa_args(); fa_set_s(na, "text", joined); fa_set_null(na, "mentions_map"); free(joined);
            json_t *nt = fa_call(ctx, "_normalize_feishu_text", na);
            if (fa_is_raise(nt)) { rr = nt; goto done; }
            const char *row_text = fa_str(nt);
            if (row_text && row_text[0]) json_append(parts, json_string(row_text));
            json_free(nt);
        }
    }
    /* files entries */
    {
        const json_t *files = json_obj_get(resolved, "files");
        if (!fa_truthy(files)) files = NULL;
        size_t nfiles = files ? json_len(files) : 0;
        for (size_t i = 0; i < nfiles; i++) {
            const json_t *entry = json_get(files, i);
            if (!entry || entry->type != JSON_OBJECT) continue;
            json_t *ba = fa_args(); fa_set_copy(ba, "value", json_obj_get(entry, "is_folder"));
            json_t *tb = fa_call(ctx, "_to_boolean", ba);
            if (fa_is_raise(tb)) { rr = tb; goto done; }
            bool is_folder = (tb && tb->type == JSON_BOOL) ? tb->bool_val : false;
            json_free(tb);
            if (is_folder) continue;
            char *fk = fa_strip_free(fa_get_str_py(entry, "file_key", ""));
            if (!fk[0]) { free(fk); continue; }
            free(fk);
            json_t *entry_copy = json_copy(entry);
            json_set_hdr(entry_copy, "tag", "file");
            json_t *pr = fa_render_post_element(ctx, entry_copy, ik, mr, mm);
            json_free(entry_copy);
            if (fa_is_raise(pr)) { rr = pr; goto done; }
            const char *ps = fa_str(pr);
            if (ps && ps[0]) json_append(parts, json_string(ps));
            json_free(pr);
        }
    }
    /* text_content = "\n".join(parts).strip() or FALLBACK */
    {
        fa_buf_t pb; fa_buf_init(&pb);
        for (size_t i = 0; i < json_len(parts); i++) {
            if (i > 0) fa_buf_str(&pb, "\n");
            fa_buf_str(&pb, fa_arr_str(parts, i, ""));
        }
        char *joined = fa_buf_take(&pb);
        char *tc = fa_strip_free(joined);
        result = fa_parse_result((tc && tc[0]) ? tc : FA_FALLBACK_POST_TEXT, ik, mr);
        free(tc);
        ik = NULL; mr = NULL;
    }
done:
    json_free(resolved); json_free(ik); json_free(mr); json_free(parts);
    return rr ? rr : result;
}

/* PoP: fa_parse_feishu_post_payload @ plugins/platforms/feishu/adapter.py:parse_feishu_post_payload */
static json_t *fa_parse_feishu_post_payload(fa_ctx_t *ctx, const json_t *args) {
    json_t *mm = json_obj_get(args, "mentions_map");
    return fa_parse_post_payload(ctx, json_obj_get(args, "payload"), mm);
}

/* PoP: fa__resolve_post_payload @ plugins/platforms/feishu/adapter.py:_resolve_post_payload */
static json_t *fa__resolve_post_payload(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *a = fa_args(); fa_set_copy(a, "candidate", payload);
    json_t *direct = fa_call(ctx, "_to_post_payload", a);
    if (fa_is_raise(direct)) return direct;
    if (fa_truthy(direct)) return direct;
    json_free(direct);
    if (!payload || payload->type != JSON_OBJECT) return json_object();
    json_t *a2 = fa_args(); fa_set_copy(a2, "payload", json_obj_get(payload, "post"));
    json_t *r1 = fa_call(ctx, "_resolve_locale_payload", a2);
    if (fa_is_raise(r1)) return r1;
    if (fa_truthy(r1)) return r1;
    json_free(r1);
    json_t *a3 = fa_args(); fa_set_copy(a3, "payload", payload);
    return fa_call(ctx, "_resolve_locale_payload", a3);
}

/* PoP: fa__resolve_locale_payload @ plugins/platforms/feishu/adapter.py:_resolve_locale_payload */
static json_t *fa__resolve_locale_payload(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *a = fa_args(); fa_set_copy(a, "candidate", payload);
    json_t *direct = fa_call(ctx, "_to_post_payload", a);
    if (fa_is_raise(direct)) return direct;
    if (fa_truthy(direct)) return direct;
    json_free(direct);
    if (!payload || payload->type != JSON_OBJECT) return json_object();
    /* preferred locales then all values (insertion order) */
    json_t *cands = json_array();
    const char *pref[] = {"zh_cn", "en_us"};
    for (int i = 0; i < 2; i++) {
        const json_t *v = json_obj_get(payload, pref[i]);
        json_append(cands, v ? json_copy(v) : json_null());
    }
    for (size_t i = 0; i < payload->c.count; i++) json_append(cands, json_copy(payload->c.items[i]));
    json_t *ret = NULL;
    for (size_t i = 0; i < json_len(cands); i++) {
        json_t *ca = fa_args(); fa_set_copy(ca, "candidate", json_get(cands, i));
        json_t *r = fa_call(ctx, "_to_post_payload", ca);
        if (fa_is_raise(r)) { ret = r; break; }
        if (fa_truthy(r)) { ret = r; break; }
        json_free(r);
    }
    json_free(cands);
    return ret ? ret : json_object();
}

/* PoP: fa__build_markdown_post_payload @ plugins/platforms/feishu/adapter.py:_build_markdown_post_payload */
static json_t *fa__build_markdown_post_payload(fa_ctx_t *ctx, const json_t *args) {
    json_t *a = fa_args(); fa_set_copy(a, "content", json_obj_get(args, "content"));
    json_t *rows = fa_call(ctx, "_build_markdown_post_rows", a);
    if (fa_is_raise(rows)) return rows;
    json_t *inner = json_object();
    json_set(inner, "content", rows);
    json_t *obj = json_object();
    json_set(obj, "zh_cn", inner);
    char *s = fa_json_dumps(obj);
    json_free(obj);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: fa__coerce_required_int @ plugins/platforms/feishu/adapter.py:_coerce_required_int */
static json_t *fa__coerce_required_int(fa_ctx_t *ctx, const json_t *args) {
    json_t *a = fa_args();
    fa_set_copy(a, "value", json_obj_get(args, "value"));
    fa_set_copy(a, "default", json_obj_get(args, "default"));
    const json_t *mv = json_obj_get(args, "min_value");
    json_set(a, "min_value", mv ? json_copy(mv) : json_number(0));
    json_t *r = fa_call(ctx, "_coerce_int", a);
    if (fa_is_raise(r)) return r;
    if (!r || r->type == JSON_NULL) {
        json_free(r);
        const json_t *def = json_obj_get(args, "default");
        return def ? json_copy(def) : json_null();
    }
    return r;
}
/* ── _strip_markdown_to_plain_text native regex steps ── */
/* \[([^\]]+)\]\(([^)]+)\) -> "g1 (g2.strip())" */
static char *fa_md_link_sub(const char *s) {
    fa_buf_t b; fa_buf_init(&b);
    size_t len = strlen(s), i = 0;
    while (i < len) {
        if (s[i] == '[') {
            size_t j = i + 1;
            while (j < len && s[j] != ']') j++;
            if (j > i + 1 && j < len && s[j] == ']' && j + 1 < len && s[j+1] == '(') {
                size_t k = j + 2;
                while (k < len && s[k] != ')') k++;
                if (k > j + 2 && k < len && s[k] == ')') {
                    fa_buf_append(&b, s + i + 1, j - i - 1);
                    fa_buf_str(&b, " (");
                    char *g2 = malloc(k - j - 2 + 1); memcpy(g2, s + j + 2, k - j - 2); g2[k-j-2] = '\0';
                    char *g2s = fa_strip(g2); free(g2);
                    fa_buf_str(&b, g2s ? g2s : ""); free(g2s);
                    fa_buf_str(&b, ")");
                    i = k + 1;
                    continue;
                }
            }
        }
        fa_buf_append(&b, s + i, 1);
        i++;
    }
    return fa_buf_take(&b);
}
/* ^>\s? (MULTILINE): drop '>' at line start plus one optional WS codepoint. */
static char *fa_blockquote_sub(const char *s) {
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(s, &n);
    fa_buf_t b; fa_buf_init(&b);
    bool line_start = true;
    size_t i = 0;
    while (i < n) {
        if (line_start && cps[i] == '>') {
            i++;
            if (i < n && fa_is_ws_cp(cps[i])) i++;
            line_start = false;
            continue;
        }
        if (cps[i] == '\n') { fa_buf_cp(&b, cps[i]); line_start = true; }
        else { fa_buf_cp(&b, cps[i]); line_start = false; }
        i++;
    }
    free(cps);
    return fa_buf_take(&b);
}
/* ^\s*---+\s*$ (MULTILINE) -> "---". \s includes \n, so the leading/trailing
 * \s* can consume newlines; the match starts at a line start (^) and ends at a
 * line end ($), with greedy \s* backtracked to the nearest line end. */
static char *fa_hr_sub(const char *s) {
    size_t n = 0;
    uint32_t *cps = fa_utf8_decode(s, &n);
    fa_buf_t b; fa_buf_init(&b);
    size_t i = 0;
    while (i < n) {
        bool line_start = (i == 0 || cps[i-1] == '\n');
        if (line_start) {
            size_t a = i;
            while (a < n && fa_is_ws_cp(cps[a])) a++;
            size_t d = a;
            while (d < n && cps[d] == '-') d++;
            if (d - a >= 3) {
                size_t c = d;
                while (c < n && fa_is_ws_cp(cps[c])) c++;
                while (c > d && !(c == n || cps[c] == '\n')) c--;
                if (c == n || cps[c] == '\n') {
                    fa_buf_str(&b, "---");
                    i = c;
                    continue;
                }
            }
        }
        fa_buf_cp(&b, cps[i]);
        i++;
    }
    free(cps);
    return fa_buf_take(&b);
}
/* ~~([^~\n]+)~~ -> inner */
static char *fa_strike_sub(const char *s) {
    fa_buf_t b; fa_buf_init(&b);
    size_t len = strlen(s), i = 0;
    while (i < len) {
        if (s[i] == '~' && i + 1 < len && s[i+1] == '~') {
            size_t j = i + 2;
            while (j < len && s[j] != '~' && s[j] != '\n') j++;
            if (j > i + 2 && j + 1 < len && s[j] == '~' && s[j+1] == '~') {
                fa_buf_append(&b, s + i + 2, j - i - 2);
                i = j + 2;
                continue;
            }
        }
        fa_buf_append(&b, s + i, 1);
        i++;
    }
    return fa_buf_take(&b);
}
/* <u>([\s\S]*?)</u> -> inner (non-greedy to first </u>) */
static char *fa_underline_sub(const char *s) {
    fa_buf_t b; fa_buf_init(&b);
    size_t len = strlen(s), i = 0;
    while (i < len) {
        if (i + 3 <= len && strncmp(s + i, "<u>", 3) == 0) {
            const char *close = strstr(s + i + 3, "</u>");
            if (close) {
                fa_buf_append(&b, s + i + 3, (size_t)(close - (s + i + 3)));
                i = (size_t)(close - s) + 4;
                continue;
            }
        }
        fa_buf_append(&b, s + i, 1);
        i++;
    }
    return fa_buf_take(&b);
}

/* PoP: fa__strip_markdown_to_plain_text @ plugins/platforms/feishu/adapter.py:_strip_markdown_to_plain_text */
static json_t *fa__strip_markdown_to_plain_text(fa_ctx_t *ctx, const json_t *args) {
    const char *text = json_get_str(args, "text", "");
    char *plain = fa_replace(text, "\r\n", "\n");
    char *t = fa_md_link_sub(plain); free(plain); plain = t;
    t = fa_blockquote_sub(plain); free(plain); plain = t;
    t = fa_hr_sub(plain); free(plain); plain = t;
    t = fa_strike_sub(plain); free(plain); plain = t;
    t = fa_underline_sub(plain); free(plain); plain = t;
    json_t *a = fa_args(); fa_set_s(a, "text", plain); free(plain);
    return fa_call(ctx, "strip_markdown", a);
}

/* PoP: fa__attachment_placeholder @ plugins/platforms/feishu/adapter.py:_attachment_placeholder */
static json_t *fa__attachment_placeholder(fa_ctx_t *ctx, const json_t *args) {
    json_t *a = fa_args(); fa_set_copy(a, "text", json_obj_get(args, "file_name")); fa_set_null(a, "mentions_map");
    json_t *r = fa_call(ctx, "_normalize_feishu_text", a);
    if (fa_is_raise(r)) return r;
    const char *nm = fa_str(r);
    json_t *out;
    if (nm && nm[0]) {
        fa_buf_t b; fa_buf_init(&b);
        fa_buf_str(&b, "[Attachment: "); fa_buf_str(&b, nm); fa_buf_str(&b, "]");
        char *s = fa_buf_take(&b); out = json_string(s ? s : ""); free(s);
    } else out = json_string("[Attachment]");
    json_free(r);
    return out;
}

/* PoP: fa__first_non_empty_text @ plugins/platforms/feishu/adapter.py:_first_non_empty_text */
static json_t *fa__first_non_empty_text(fa_ctx_t *ctx, const json_t *args) {
    const json_t *values = json_obj_get(args, "values");
    size_t n = values ? json_len(values) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(values, i);
        if (!v || v->type == JSON_NULL || v->type == JSON_OBJECT || v->type == JSON_ARRAY) continue;
        char *raw = fa_py_str_scalar(v);
        json_t *a = fa_args(); fa_set_s(a, "text", raw); fa_set_null(a, "mentions_map"); free(raw);
        json_t *r = fa_call(ctx, "_normalize_feishu_text", a);
        if (fa_is_raise(r)) return r;
        const char *nm = fa_str(r);
        if (nm && nm[0]) return r;
        json_free(r);
    }
    return json_string("");
}

/* PoP: fa__find_header_title @ plugins/platforms/feishu/adapter.py:_find_header_title */
static json_t *fa__find_header_title(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    if (!payload || payload->type != JSON_OBJECT) return json_string("");
    const json_t *header = json_obj_get(payload, "header");
    if (!header || header->type != JSON_OBJECT) return json_string("");
    const json_t *title = json_obj_get(header, "title");
    if (title && title->type == JSON_OBJECT) {
        json_t *vals = json_array();
        const char *keys[] = {"content", "text", "name"};
        for (int i = 0; i < 3; i++) {
            const json_t *v = json_obj_get(title, keys[i]);
            json_append(vals, v ? json_copy(v) : json_null());
        }
        json_t *a = fa_args(); json_set(a, "values", vals);
        return fa_call(ctx, "_first_non_empty_text", a);
    }
    char *raw = fa_str_or_empty(title);
    json_t *a = fa_args(); fa_set_s(a, "text", raw); fa_set_null(a, "mentions_map"); free(raw);
    return fa_call(ctx, "_normalize_feishu_text", a);
}

/* _walk_nodes REAL iterator used by _find_first_text / _collect_action_labels. */
/* PoP: fa__find_first_text @ plugins/platforms/feishu/adapter.py:_find_first_text */
static json_t *fa__find_first_text(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    const json_t *keys = json_obj_get(args, "keys");
    json_t *nodes = json_array();
    fa_walk_collect(payload, nodes);
    json_t *ret = NULL;
    for (size_t i = 0; i < json_len(nodes) && !ret; i++) {
        const json_t *node = json_get(nodes, i);
        if (!node || node->type != JSON_OBJECT) continue;
        for (size_t k = 0; k < json_len(keys); k++) {
            const char *key = fa_arr_str(keys, k, "");
            const json_t *value = json_obj_get(node, key);
            if (!value || value->type != JSON_STRING) continue;
            json_t *a = fa_args(); fa_set_s(a, "text", value->str_val); fa_set_null(a, "mentions_map");
            json_t *r = fa_call(ctx, "_normalize_feishu_text", a);
            if (fa_is_raise(r)) { ret = r; break; }
            const char *nm = fa_str(r);
            if (nm && nm[0]) { ret = r; break; }
            json_free(r);
        }
    }
    json_free(nodes);
    return ret ? ret : json_string("");
}

/* PoP: fa__first_text_field @ plugins/platforms/feishu/adapter.py:_first_text_field */
static json_t *fa__first_text_field(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    const json_t *keys = json_obj_get(args, "keys");
    const json_t *deep = json_obj_get(args, "deep");
    json_t *values = json_array();
    for (size_t i = 0; i < json_len(keys); i++) {
        const char *key = fa_arr_str(keys, i, "");
        const json_t *v = json_obj_get(payload, key);
        json_append(values, v ? json_copy(v) : json_null());
    }
    if (deep && json_len(deep) > 0) {
        json_t *fa = fa_args();
        fa_set_copy(fa, "payload", payload);
        json_set(fa, "keys", json_copy(deep));
        json_t *fft = fa_call(ctx, "_find_first_text", fa);
        if (fa_is_raise(fft)) { json_free(values); return fft; }
        json_append(values, fft);
    }
    json_t *a = fa_args(); json_set(a, "values", values);
    return fa_call(ctx, "_first_non_empty_text", a);
}

/* PoP: fa__build_media_ref_from_payload @ plugins/platforms/feishu/adapter.py:_build_media_ref_from_payload */
static json_t *fa__build_media_ref_from_payload(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    const char *resource_type = json_get_str(args, "resource_type", "");
    char *file_key = fa_strip_free(fa_str_or_empty_field(payload, "file_key", ""));
    json_t *fa = fa_args();
    fa_set_copy(fa, "payload", payload);
    json_t *keys = json_array();
    json_append(keys, json_string("file_name")); json_append(keys, json_string("title")); json_append(keys, json_string("text"));
    json_set(fa, "keys", keys);
    json_set(fa, "deep", json_array());
    json_t *fn = fa_call(ctx, "_first_text_field", fa);
    if (fa_is_raise(fn)) { free(file_key); return fn; }
    const char *file_name = fa_str(fn) ? fa_str(fn) : "";
    const char *eff = (strcmp(resource_type, "audio") == 0 || strcmp(resource_type, "video") == 0) ? resource_type : "file";
    json_t *r = fa_media_ref(file_key, file_name, eff);
    free(file_key);
    json_free(fn);
    return r;
}
/* ── card / forward text walkers ── */
static bool fa_in_set(const char *s, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++) if (strcmp(s, set[i]) == 0) return true;
    return false;
}
static const char *FA_RICH_BLOCK_TAGS[] = {
    "plain_text", "lark_md", "markdown", "note", "div", "column_set", "column",
    "action", "button", "select_static", "date_picker",
};
static const char *FA_SUPPORTED_CARD_TEXT_KEYS[] = {
    "title", "text", "content", "label", "value", "name", "summary", "subtitle",
    "description", "placeholder", "hint",
};
static const char *FA_SKIP_TEXT_KEYS[] = {
    "tag", "type", "msg_type", "message_type", "chat_id", "open_chat_id", "share_chat_id",
    "file_key", "image_key", "user_id", "open_id", "union_id", "url", "href", "link",
    "token", "template", "locale",
};
/* str(value.get(k1,"") or value.get(k2,"")).strip().lower() */
static char *fa_node_tag2(const json_t *value, const char *k1, const char *k2) {
    const json_t *tn = json_obj_get(value, k1);
    char *raw;
    if (tn && fa_truthy(tn)) raw = fa_py_str_scalar(tn);
    else {
        const json_t *ty = json_obj_get(value, k2);
        raw = ty ? fa_py_str_scalar(ty) : fa_strdup("");
    }
    char *s = fa_strip_free(raw);
    char *l = fa_lower_ascii(s); free(s);
    return l;
}
static char *fa_node_tag(const json_t *value) { return fa_node_tag2(value, "tag", "type"); }
static json_t *fa_collect_text_segments(fa_ctx_t *ctx, const json_t *value, bool in_rich) {
    if (value && value->type == JSON_STRING) {
        json_t *out = json_array();
        if (in_rich) {
            json_t *a = fa_args(); fa_set_s(a, "text", value->str_val); fa_set_null(a, "mentions_map");
            json_t *r = fa_call(ctx, "_normalize_feishu_text", a);
            if (fa_is_raise(r)) { json_free(out); return r; }
            json_append(out, r);
        }
        return out;
    }
    if (value && value->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(value); i++) {
            json_t *sub = fa_collect_text_segments(ctx, json_get(value, i), in_rich);
            if (fa_is_raise(sub)) { json_free(out); return sub; }
            for (size_t j = 0; j < json_len(sub); j++) json_append(out, json_copy(json_get(sub, j)));
            json_free(sub);
        }
        return out;
    }
    if (!value || value->type != JSON_OBJECT) return json_array();
    char *tag = fa_node_tag(value);
    bool next_rich = in_rich || fa_in_set(tag, FA_RICH_BLOCK_TAGS, sizeof(FA_RICH_BLOCK_TAGS)/sizeof(char*));
    free(tag);
    json_t *segments = json_array();
    if (next_rich) {
        for (size_t k = 0; k < sizeof(FA_SUPPORTED_CARD_TEXT_KEYS)/sizeof(char*); k++) {
            const json_t *item = json_obj_get(value, FA_SUPPORTED_CARD_TEXT_KEYS[k]);
            if (!item || item->type != JSON_STRING) continue;
            json_t *a1 = fa_args(); fa_set_s(a1, "text", item->str_val); fa_set_null(a1, "mentions_map");
            json_t *r1 = fa_call(ctx, "_normalize_feishu_text", a1);
            if (fa_is_raise(r1)) { json_free(segments); return r1; }
            const char *nm1 = fa_str(r1);
            if (nm1 && nm1[0]) {
                json_t *a2 = fa_args(); fa_set_s(a2, "text", item->str_val); fa_set_null(a2, "mentions_map");
                json_t *r2 = fa_call(ctx, "_normalize_feishu_text", a2);
                if (fa_is_raise(r2)) { json_free(r1); json_free(segments); return r2; }
                json_append(segments, r2);
            }
            json_free(r1);
        }
    }
    for (size_t i = 0; i < value->c.count; i++) {
        if (fa_in_set(value->c.keys[i], FA_SKIP_TEXT_KEYS, sizeof(FA_SKIP_TEXT_KEYS)/sizeof(char*))) continue;
        json_t *sub = fa_collect_text_segments(ctx, value->c.items[i], next_rich);
        if (fa_is_raise(sub)) { json_free(segments); return sub; }
        for (size_t j = 0; j < json_len(sub); j++) json_append(segments, json_copy(json_get(sub, j)));
        json_free(sub);
    }
    return segments;
}

/* PoP: fa__collect_text_segments @ plugins/platforms/feishu/adapter.py:_collect_text_segments */
static json_t *fa__collect_text_segments(fa_ctx_t *ctx, const json_t *args) {
    bool in_rich = json_get_bool(args, "in_rich_block", false);
    return fa_collect_text_segments(ctx, json_obj_get(args, "value"), in_rich);
}

/* PoP: fa__collect_card_lines @ plugins/platforms/feishu/adapter.py:_collect_card_lines */
static json_t *fa__collect_card_lines(fa_ctx_t *ctx, const json_t *args) {
    json_t *a = fa_args(); fa_set_copy(a, "value", json_obj_get(args, "payload")); fa_set_b(a, "in_rich_block", false);
    json_t *lines = fa_call(ctx, "_collect_text_segments", a);
    if (fa_is_raise(lines)) return lines;
    json_t *filtered = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        json_t *na = fa_args(); fa_set_copy(na, "text", json_get(lines, i)); fa_set_null(na, "mentions_map");
        json_t *r = fa_call(ctx, "_normalize_feishu_text", na);
        if (fa_is_raise(r)) { json_free(lines); json_free(filtered); return r; }
        const char *nm = fa_str(r);
        if (nm && nm[0]) json_append(filtered, json_copy(r));
        json_free(r);
    }
    json_free(lines);
    json_t *ua = fa_args(); json_set(ua, "lines", filtered);
    return fa_call(ctx, "_unique_lines", ua);
}

/* PoP: fa__collect_action_labels @ plugins/platforms/feishu/adapter.py:_collect_action_labels */
static json_t *fa__collect_action_labels(fa_ctx_t *ctx, const json_t *args) {
    json_t *nodes = json_array();
    fa_walk_collect(json_obj_get(args, "payload"), nodes);
    json_t *labels = json_array();
    static const char *action_tags[] = {"button", "select_static", "overflow", "date_picker", "picker"};
    for (size_t i = 0; i < json_len(nodes); i++) {
        const json_t *item = json_get(nodes, i);
        if (!item || item->type != JSON_OBJECT) continue;
        char *tag = fa_node_tag(item);
        bool is_action = fa_in_set(tag, action_tags, sizeof(action_tags)/sizeof(char*));
        free(tag);
        if (!is_action) continue;
        json_t *fa = fa_args(); fa_set_copy(fa, "payload", item);
        json_t *keys = json_array();
        json_append(keys, json_string("text")); json_append(keys, json_string("name")); json_append(keys, json_string("value"));
        json_set(fa, "keys", keys);
        json_t *deep = json_array();
        json_append(deep, json_string("text")); json_append(deep, json_string("content"));
        json_append(deep, json_string("name")); json_append(deep, json_string("value"));
        json_set(fa, "deep", deep);
        json_t *r = fa_call(ctx, "_first_text_field", fa);
        if (fa_is_raise(r)) { json_free(nodes); json_free(labels); return r; }
        const char *lab = fa_str(r);
        if (lab && lab[0]) json_append(labels, json_copy(r));
        json_free(r);
    }
    json_free(nodes);
    json_t *ua = fa_args(); json_set(ua, "lines", labels);
    return fa_call(ctx, "_unique_lines", ua);
}

/* PoP: fa__collect_forward_entries @ plugins/platforms/feishu/adapter.py:_collect_forward_entries */
static json_t *fa__collect_forward_entries(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *candidates = json_array();
    static const char *cand_keys[] = {"messages", "items", "message_list", "records", "content"};
    for (size_t k = 0; k < sizeof(cand_keys)/sizeof(char*); k++) {
        const json_t *v = json_obj_get(payload, cand_keys[k]);
        if (v && v->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(v); i++) json_append(candidates, json_copy(json_get(v, i)));
    }
    json_t *entries = json_array();
    for (size_t i = 0; i < json_len(candidates); i++) {
        const json_t *item = json_get(candidates, i);
        if (!item || item->type != JSON_OBJECT) {
            char *raw = fa_str_or_empty(item);
            json_t *na = fa_args(); fa_set_s(na, "text", raw); fa_set_null(na, "mentions_map"); free(raw);
            json_t *r = fa_call(ctx, "_normalize_feishu_text", na);
            if (fa_is_raise(r)) { json_free(candidates); json_free(entries); return r; }
            const char *text = fa_str(r);
            if (text && text[0]) {
                fa_buf_t b; fa_buf_init(&b); fa_buf_str(&b, "- "); fa_buf_str(&b, text);
                char *s = fa_buf_take(&b); json_append(entries, json_string(s)); free(s);
            }
            json_free(r);
            continue;
        }
        /* sender */
        json_t *sa = fa_args(); fa_set_copy(sa, "payload", item);
        json_t *skeys = json_array();
        const char *sk[] = {"sender_name", "user_name", "sender", "name"};
        for (int j = 0; j < 4; j++) json_append(skeys, json_string(sk[j]));
        json_set(sa, "keys", skeys); json_set(sa, "deep", json_array());
        json_t *sr = fa_call(ctx, "_first_text_field", sa);
        if (fa_is_raise(sr)) { json_free(candidates); json_free(entries); return sr; }
        char *sender = fa_strdup(fa_str(sr) ? fa_str(sr) : "");
        json_free(sr);
        /* nested_type */
        char *nested_type = fa_node_tag2(item, "message_type", "msg_type");
        char *body = NULL;
        json_t *body_holder = NULL;
        if (strcmp(nested_type, "post") == 0) {
            const json_t *content = json_obj_get(item, "content");
            const json_t *parg = (content && fa_truthy(content)) ? content : item;
            body_holder = fa_parse_post_payload(ctx, parg, NULL);
            if (fa_is_raise(body_holder)) { free(sender); free(nested_type); json_free(candidates); json_free(entries); return body_holder; }
            body = fa_strdup(fa_str(json_obj_get(body_holder, "text_content")) ? fa_str(json_obj_get(body_holder, "text_content")) : "");
        } else {
            json_t *ba = fa_args(); fa_set_copy(ba, "payload", item);
            json_t *bkeys = json_array();
            const char *bk[] = {"text", "summary", "preview", "content"};
            for (int j = 0; j < 4; j++) json_append(bkeys, json_string(bk[j]));
            json_set(ba, "keys", bkeys);
            json_t *bdeep = json_array();
            const char *bd[] = {"text", "content", "summary", "preview", "title"};
            for (int j = 0; j < 5; j++) json_append(bdeep, json_string(bd[j]));
            json_set(ba, "deep", bdeep);
            body_holder = fa_call(ctx, "_first_text_field", ba);
            if (fa_is_raise(body_holder)) { free(sender); free(nested_type); json_free(candidates); json_free(entries); return body_holder; }
            body = fa_strdup(fa_str(body_holder) ? fa_str(body_holder) : "");
        }
        free(nested_type);
        json_t *nba = fa_args(); fa_set_s(nba, "text", body); fa_set_null(nba, "mentions_map");
        json_t *nr = fa_call(ctx, "_normalize_feishu_text", nba);
        if (fa_is_raise(nr)) { free(sender); free(body); json_free(body_holder); json_free(candidates); json_free(entries); return nr; }
        const char *nbody = fa_str(nr) ? fa_str(nr) : "";
        fa_buf_t b; fa_buf_init(&b);
        if (sender[0] && nbody[0]) { fa_buf_str(&b, "- "); fa_buf_str(&b, sender); fa_buf_str(&b, ": "); fa_buf_str(&b, nbody); }
        else if (nbody[0]) { fa_buf_str(&b, "- "); fa_buf_str(&b, nbody); }
        char *line = fa_buf_take(&b);
        if (line[0]) json_append(entries, json_string(line));
        free(line); free(sender); free(body);
        json_free(nr); json_free(body_holder);
    }
    json_free(candidates);
    json_t *ua = fa_args(); json_set(ua, "lines", entries);
    return fa_call(ctx, "_unique_lines", ua);
}
/* ── message-type normalizer sub-flows ── */
#define FA_FALLBACK_FORWARD_TEXT "[Merged forward message]"
#define FA_FALLBACK_SHARE_CHAT_TEXT "[Shared chat]"
#define FA_FALLBACK_INTERACTIVE_TEXT "[Interactive message]"
#define FA_FALLBACK_IMAGE_TEXT "[Image]"

/* PoP: fa__normalize_merge_forward_message @ plugins/platforms/feishu/adapter.py:_normalize_merge_forward_message */
static json_t *fa__normalize_merge_forward_message(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *ta = fa_args(); fa_set_copy(ta, "payload", payload);
    json_t *tk = json_array();
    const char *tkeys[] = {"title", "summary", "preview"};
    for (int i = 0; i < 3; i++) json_append(tk, json_string(tkeys[i]));
    json_set(ta, "keys", tk);
    json_t *td = json_array();
    const char *tdeep[] = {"title", "summary", "preview", "description"};
    for (int i = 0; i < 4; i++) json_append(td, json_string(tdeep[i]));
    json_set(ta, "deep", td);
    json_t *tr = fa_call(ctx, "_first_text_field", ta);
    if (fa_is_raise(tr)) return tr;
    char *title = fa_strdup(fa_str(tr) ? fa_str(tr) : "");
    json_free(tr);
    json_t *ea = fa_args(); fa_set_copy(ea, "payload", payload);
    json_t *entries = fa_call(ctx, "_collect_forward_entries", ea);
    if (fa_is_raise(entries)) { free(title); return entries; }
    fa_buf_t b; fa_buf_init(&b);
    bool first = true;
    if (title[0]) { fa_buf_str(&b, title); first = false; }
    size_t n = json_len(entries);
    for (size_t i = 0; i < n && i < 8; i++) {
        if (!first) fa_buf_str(&b, "\n");
        fa_buf_str(&b, fa_arr_str(entries, i, ""));
        first = false;
    }
    char *joined = fa_buf_take(&b);
    char *tc = fa_strip_free(joined);
    json_t *metadata = json_object();
    json_set(metadata, "entry_count", json_number((double)n));
    json_set_hdr(metadata, "title", title);
    json_t *r = fa_normalized("merge_forward", (tc && tc[0]) ? tc : FA_FALLBACK_FORWARD_TEXT,
                              "text", json_array(), json_array(), json_array(), "merge_forward", metadata);
    free(tc); free(title);
    json_free(entries);
    return r;
}

/* PoP: fa__normalize_share_chat_message @ plugins/platforms/feishu/adapter.py:_normalize_share_chat_message */
static json_t *fa__normalize_share_chat_message(fa_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *na = fa_args(); fa_set_copy(na, "payload", payload);
    json_t *nk = json_array();
    const char *nkeys[] = {"chat_name", "name", "title"};
    for (int i = 0; i < 3; i++) json_append(nk, json_string(nkeys[i]));
    json_set(na, "keys", nk);
    json_t *nd = json_array();
    for (int i = 0; i < 3; i++) json_append(nd, json_string(nkeys[i]));
    json_set(na, "deep", nd);
    json_t *nr = fa_call(ctx, "_first_text_field", na);
    if (fa_is_raise(nr)) return nr;
    char *chat_name = fa_strdup(fa_str(nr) ? fa_str(nr) : "");
    json_free(nr);
    json_t *ia = fa_args(); fa_set_copy(ia, "payload", payload);
    json_t *ik = json_array();
    const char *ikeys[] = {"chat_id", "open_chat_id", "share_chat_id"};
    for (int i = 0; i < 3; i++) json_append(ik, json_string(ikeys[i]));
    json_set(ia, "keys", ik); json_set(ia, "deep", json_array());
    json_t *ir = fa_call(ctx, "_first_text_field", ia);
    if (fa_is_raise(ir)) { free(chat_name); return ir; }
    char *share_id = fa_strdup(fa_str(ir) ? fa_str(ir) : "");
    json_free(ir);
    fa_buf_t b; fa_buf_init(&b);
    if (chat_name[0]) { fa_buf_str(&b, "Shared chat: "); fa_buf_str(&b, chat_name); }
    else fa_buf_str(&b, FA_FALLBACK_SHARE_CHAT_TEXT);
    if (share_id[0]) { fa_buf_str(&b, "\nChat ID: "); fa_buf_str(&b, share_id); }
    char *tc = fa_buf_take(&b);
    json_t *metadata = json_object();
    json_set_hdr(metadata, "chat_id", share_id);
    json_set_hdr(metadata, "chat_name", chat_name);
    json_t *r = fa_normalized("share_chat", tc, "text", json_array(), json_array(),
                              json_array(), "share_chat", metadata);
    free(tc); free(chat_name); free(share_id);
    return r;
}

/* PoP: fa__normalize_interactive_message @ plugins/platforms/feishu/adapter.py:_normalize_interactive_message */
static json_t *fa__normalize_interactive_message(fa_ctx_t *ctx, const json_t *args) {
    const char *message_type = json_get_str(args, "message_type", "");
    const json_t *payload = json_obj_get(args, "payload");
    const json_t *card = json_obj_get(payload, "card");
    const json_t *card_payload = (card && card->type == JSON_OBJECT) ? card : payload;
    /* title = _first_non_empty_text(_find_header_title(cp), payload.get("title"),
     *                              _find_first_text(cp, keys=...)) — args L-to-R */
    json_t *ha = fa_args(); fa_set_copy(ha, "payload", card_payload);
    json_t *hr = fa_call(ctx, "_find_header_title", ha);
    if (fa_is_raise(hr)) return hr;
    json_t *fa = fa_args(); fa_set_copy(fa, "payload", card_payload);
    json_t *fk = json_array();
    const char *fkeys[] = {"title", "summary", "subtitle"};
    for (int i = 0; i < 3; i++) json_append(fk, json_string(fkeys[i]));
    json_set(fa, "keys", fk);
    json_t *fr = fa_call(ctx, "_find_first_text", fa);
    if (fa_is_raise(fr)) { json_free(hr); return fr; }
    json_t *values = json_array();
    json_append(values, json_copy(hr));
    const json_t *ptitle = json_obj_get(payload, "title");
    json_append(values, ptitle ? json_copy(ptitle) : json_null());
    json_append(values, json_copy(fr));
    json_free(hr); json_free(fr);
    json_t *va = fa_args(); json_set(va, "values", values);
    json_t *tr = fa_call(ctx, "_first_non_empty_text", va);
    if (fa_is_raise(tr)) return tr;
    char *title = fa_strdup(fa_str(tr) ? fa_str(tr) : "");
    json_free(tr);
    json_t *aa = fa_args(); fa_set_copy(aa, "payload", card_payload);
    json_t *actions = fa_call(ctx, "_collect_action_labels", aa);
    if (fa_is_raise(actions)) { free(title); return actions; }
    json_t *la = fa_args(); fa_set_copy(la, "payload", card_payload);
    json_t *card_lines = fa_call(ctx, "_collect_card_lines", la);
    if (fa_is_raise(card_lines)) { free(title); json_free(actions); return card_lines; }
    json_t *lines = json_array();
    if (title[0]) json_append(lines, json_string(title));
    for (size_t i = 0; i < json_len(card_lines); i++) {
        const char *ln = fa_arr_str(card_lines, i, "");
        if (strcmp(ln, title) != 0) json_append(lines, json_string(ln));
    }
    if (json_len(actions) > 0) {
        fa_buf_t ab; fa_buf_init(&ab);
        fa_buf_str(&ab, "Actions: ");
        for (size_t i = 0; i < json_len(actions); i++) {
            if (i > 0) fa_buf_str(&ab, ", ");
            fa_buf_str(&ab, fa_arr_str(actions, i, ""));
        }
        char *s = fa_buf_take(&ab);
        json_append(lines, json_string(s)); free(s);
    }
    fa_buf_t b; fa_buf_init(&b);
    size_t nl = json_len(lines);
    for (size_t i = 0; i < nl && i < 12; i++) {
        if (i > 0) fa_buf_str(&b, "\n");
        fa_buf_str(&b, fa_arr_str(lines, i, ""));
    }
    char *joined = fa_buf_take(&b);
    char *tc = fa_strip_free(joined);
    json_t *metadata = json_object();
    json_set_hdr(metadata, "title", title);
    json_set(metadata, "actions", json_copy(actions));
    json_t *r = fa_normalized(message_type, (tc && tc[0]) ? tc : FA_FALLBACK_INTERACTIVE_TEXT,
                              "text", json_array(), json_array(), json_array(), "interactive", metadata);
    free(tc); free(title);
    json_free(lines); json_free(actions); json_free(card_lines);
    return r;
}

/* ── mentions map ── */
static bool fa_bot_matches_bool(const json_t *bot, const char *open_id,
                                const char *user_id, const char *name) {
    const char *b_oid = json_get_str(bot, "open_id", "");
    const char *b_uid = json_get_str(bot, "user_id", "");
    const char *b_nm = json_get_str(bot, "name", "");
    if (open_id[0] && b_oid[0]) return strcmp(open_id, b_oid) == 0;
    if (user_id[0] && b_uid[0]) return strcmp(user_id, b_uid) == 0;
    return b_nm[0] && strcmp(name, b_nm) == 0;
}

/* PoP: fa__build_mentions_map @ plugins/platforms/feishu/adapter.py:_build_mentions_map */
static json_t *fa__build_mentions_map(fa_ctx_t *ctx, const json_t *args) {
    const json_t *mentions = json_obj_get(args, "mentions");
    const json_t *bot = json_obj_get(args, "bot");
    json_t *result = json_object();
    size_t n = mentions ? json_len(mentions) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *mention = json_get(mentions, i);
        char *key = fa_str_or_empty_field(mention, "key", "");
        if (!key[0]) { free(key); continue; }
        if (strcmp(key, "@_all") == 0) {
            json_set(result, key, fa_mention_ref("", "", true, false));
            free(key);
            continue;
        }
        json_t *ia = fa_args(); fa_set_copy(ia, "mention", mention);
        json_t *ids = fa_call(ctx, "_extract_mention_ids", ia);
        if (fa_is_raise(ids)) { free(key); json_free(result); return ids; }
        const char *open_id = fa_str(json_get(ids, 0)) ? fa_str(json_get(ids, 0)) : "";
        const char *user_id = fa_str(json_get(ids, 1)) ? fa_str(json_get(ids, 1)) : "";
        char *name = fa_strip_free(fa_str_or_empty_field(mention, "name", ""));
        bool is_self = fa_bot_matches_bool(bot, open_id, user_id, name);
        json_set(result, key, fa_mention_ref(name, open_id, false, is_self));
        free(name); free(key);
        json_free(ids);
    }
    return result;
}

/* PoP: fa_normalize_feishu_message @ plugins/platforms/feishu/adapter.py:normalize_feishu_message */
static json_t *fa_normalize_feishu_message(fa_ctx_t *ctx, const json_t *args) {
    json_t *payload = NULL, *mm = NULL, *mention_refs = NULL, *result = NULL, *rr = NULL;
    char *nt = fa_lower_ascii(fa_strip_free(fa_str_or_empty(json_obj_get(args, "message_type"))));
    json_t *pa = fa_args(); fa_set_copy(pa, "raw_content", json_obj_get(args, "raw_content"));
    payload = fa_call(ctx, "_load_feishu_payload", pa);
    if (fa_is_raise(payload)) { rr = payload; payload = NULL; goto done; }
    json_t *ma = fa_args();
    fa_set_copy(ma, "mentions", json_obj_get(args, "mentions"));
    /* live bot defaults to _FeishuBotIdentity() (empty fields), never None */
    const json_t *bot_node = json_obj_get(args, "bot");
    json_t *bot_proj;
    if (bot_node && bot_node->type == JSON_OBJECT) bot_proj = json_copy(bot_node);
    else {
        bot_proj = json_object();
        json_set_hdr(bot_proj, "open_id", "");
        json_set_hdr(bot_proj, "user_id", "");
        json_set_hdr(bot_proj, "name", "");
    }
    json_set(ma, "bot", bot_proj);
    mm = fa_call(ctx, "_build_mentions_map", ma);
    if (fa_is_raise(mm)) { rr = mm; mm = NULL; goto done; }

    if (strcmp(nt, "text") == 0) {
        char *text = fa_str_or_empty_field(payload, "text", "");
        if (strstr(text, "@_all") && !json_obj_get(mm, "@_all"))
            json_set(mm, "@_all", fa_mention_ref("", "", true, false));
        json_t *ta = fa_args(); fa_set_s(ta, "text", text); fa_set_copy(ta, "mentions_map", mm); free(text);
        json_t *tr = fa_call(ctx, "_normalize_feishu_text", ta);
        if (fa_is_raise(tr)) { rr = tr; goto done; }
        result = fa_normalized(nt, fa_str(tr) ? fa_str(tr) : "", "text",
                               json_array(), json_array(), fa_map_values(mm), "plain", json_object());
        json_free(tr);
        goto done;
    }
    if (strcmp(nt, "post") == 0) {
        json_t *parsed = fa_parse_post_payload(ctx, payload, mm);
        if (fa_is_raise(parsed)) { rr = parsed; goto done; }
        result = fa_normalized(nt, fa_str(json_obj_get(parsed, "text_content")) ? fa_str(json_obj_get(parsed, "text_content")) : "",
                               "text", json_copy(json_obj_get(parsed, "image_keys")),
                               json_copy(json_obj_get(parsed, "media_refs")),
                               fa_map_values(mm), "post", json_object());
        json_free(parsed);
        goto done;
    }
    mention_refs = fa_map_values(mm);
    if (strcmp(nt, "image") == 0) {
        char *image_key = fa_strip_free(fa_str_or_empty_field(payload, "image_key", ""));
        char *alt_a = fa_str_or_empty_field(payload, "text", "");
        char *alt_raw;
        if (alt_a[0]) alt_raw = alt_a;
        else {
            free(alt_a);
            char *alt_b = fa_str_or_empty_field(payload, "alt", "");
            alt_raw = alt_b[0] ? alt_b : fa_strdup(FA_FALLBACK_IMAGE_TEXT);
            if (alt_raw != alt_b) free(alt_b);
        }
        json_t *aa = fa_args(); fa_set_s(aa, "text", alt_raw); fa_set_copy(aa, "mentions_map", mm); free(alt_raw);
        json_t *ar = fa_call(ctx, "_normalize_feishu_text", aa);
        if (fa_is_raise(ar)) { free(image_key); rr = ar; goto done; }
        const char *alt_text = fa_str(ar) ? fa_str(ar) : "";
        const char *tc = (strcmp(alt_text, FA_FALLBACK_IMAGE_TEXT) != 0) ? alt_text : "";
        json_t *ik = json_array();
        if (image_key[0]) json_append(ik, json_string(image_key));
        result = fa_normalized(nt, tc, "photo", ik, json_array(), mention_refs, "image", json_object());
        mention_refs = NULL;
        free(image_key);
        json_free(ar);
        goto done;
    }
    if (strcmp(nt, "file") == 0 || strcmp(nt, "audio") == 0 || strcmp(nt, "media") == 0) {
        json_t *mra = fa_args(); fa_set_copy(mra, "payload", payload); fa_set_s(mra, "resource_type", nt);
        json_t *media_ref = fa_call(ctx, "_build_media_ref_from_payload", mra);
        if (fa_is_raise(media_ref)) { rr = media_ref; goto done; }
        const char *fk = fa_str(json_obj_get(media_ref, "file_key")) ? fa_str(json_obj_get(media_ref, "file_key")) : "";
        json_t *mrs = json_array();
        if (fk[0]) json_append(mrs, json_copy(media_ref));
        json_t *pha = fa_args(); fa_set_copy(pha, "file_name", json_obj_get(media_ref, "file_name"));
        json_t *ph = fa_call(ctx, "_attachment_placeholder", pha);
        if (fa_is_raise(ph)) { json_free(media_ref); json_free(mrs); rr = ph; goto done; }
        json_t *metadata = json_object();
        json_set(metadata, "placeholder_text", json_copy(ph));
        const char *preferred = (strcmp(nt, "audio") == 0) ? "audio" : "document";
        result = fa_normalized(nt, "", preferred, json_array(), mrs, mention_refs, nt, metadata);
        mention_refs = NULL;
        json_free(media_ref); json_free(ph);
        goto done;
    }
    if (strcmp(nt, "merge_forward") == 0) {
        json_t *a = fa_args(); fa_set_copy(a, "payload", payload);
        result = fa_call(ctx, "_normalize_merge_forward_message", a);
        goto done;
    }
    if (strcmp(nt, "share_chat") == 0) {
        json_t *a = fa_args(); fa_set_copy(a, "payload", payload);
        result = fa_call(ctx, "_normalize_share_chat_message", a);
        goto done;
    }
    if (strcmp(nt, "interactive") == 0 || strcmp(nt, "card") == 0) {
        json_t *a = fa_args(); fa_set_s(a, "message_type", nt); fa_set_copy(a, "payload", payload);
        result = fa_call(ctx, "_normalize_interactive_message", a);
        goto done;
    }
    result = fa_normalized(nt, "", "text", json_array(), json_array(), json_array(), "plain", json_object());
done:
    free(nt);
    json_free(payload); json_free(mm); json_free(mention_refs);
    return rr ? rr : result;
}