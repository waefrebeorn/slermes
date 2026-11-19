/*
 * openai_routes_flow.c — gateway/platforms/api_server_openai_routes.py
 * OpenAI-compatible route FLOW cores + leaves (LIVE authority:
 * C:/Users/eman5/hermes-agent/gateway/platforms/api_server_openai_routes.py).
 *
 * Ports the DECISION sequences of the /v1/chat/completions + /v1/responses
 * request flows: the result-flag / finish-reason / response-status error
 * mapping chains (_result_flags, _budget_exhausted, _finish_reason,
 * _response_status, _hermes_extras), the stream-transform branch
 * (_post_stream_transform), the request validation chains
 * (_parse_chat_messages, _parse_responses_input, _parse_conversation_history),
 * model-route resolution (_select_request_route), the idempotency store units
 * (_settled, _matching_replay, _store_nonstream_response,
 * _responses_fingerprint_keys), the Responses transcript builder
 * (_build_response_conversation_history) and the payload-capping leaves
 * (_cap_text, _cap_history_tool_outputs, _is_reasoning_input_item).
 *
 * The four route handlers (_handle_chat_completions, _handle_responses,
 * _handle_get_response, _handle_delete_response) are already ported in
 * api_server_adapter_chat.c / api_server_adapter_responses.c — NOT re-ported.
 *
 * SEAM DOCTRINE: every live effect (api_server helper imports, adapter route
 * methods, response-store reads/writes, turn-boundary detection) enters
 * through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` borrowed JSON object
 * (null-valued keys omitted on BOTH sides); result NEW json_t* (NULL == None,
 * never a JSON_NULL node); {"__raise__": cls, "msg": msg} == exception. The
 * harness records {"ev": name, "args": args, "ret": result} per call in order;
 * tests/oracle_openai_routes_flow.py drives LIVE Python with the same scripts
 * and compares ordered trace + return. The cores perform NO I/O.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == Python None; a {"__raise__": cls, "msg": msg} object == a raised
 * exception (propagated or caught exactly as the live def does). Tuples are
 * JSON arrays; dict key order is insertion order (semantic in comparisons).
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   normalize_chat_content      _normalize_chat_content(content)   args{content}
 *   normalize_multimodal_content _normalize_multimodal_content(content) args{content}
 *   multimodal_validation_error _multimodal_validation_error(exc, param=) args{exc,param}
 *   error_response              _error_response(message, status)   args{message,status}
 *   resolve_route               self._resolve_route(model_alias)   args{model_alias}
 *   request_agent_overrides     _request_agent_overrides(body, virtual_model=, allow_bare_model=)
 *                               args{body,virtual_model,allow_bare_model}
 *   request_route_conflict_error self._request_route_conflict_error(session_id=, ...)
 *                               args{session_id,gateway_session_key,requested_model,requested_provider,route}
 *   store.request_key_matches   store.request_key_matches(key, binding) args{key,binding}
 *   store.get                   store.get(key)                     args{key}
 *   store.get_conversation      store.get_conversation(conversation) args{conversation}
 *   store.put                   store.put(key, value)              args{key,value}
 *   store.set_conversation      store.set_conversation(conversation, response_id)
 *                               args{conversation,response_id}
 *   turn_start_index            api_server_turn_boundary.response_turn_start_index(ch, um, result)
 *                               args{conversation_history,user_message,result}
 *
 * Internal (NOT seamed; real on both sides): _is_reasoning_input_item (inside
 * the parse chains), _cap_text (inside the cappers), _cap_history_tool_outputs
 * (inside _build_response_conversation_history), json.loads / json.dumps
 * (Python-default formatting, inside _cap_history_tool_outputs).
 *
 * Self-contained: the oracle harness #includes this .c directly; only libc
 * and libjson are used.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */

static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
/* Python `x is not None` (a present, non-null node). */
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static const char *jstr(const json_t *v) {
    return (is_str(v) && v->str_val) ? v->str_val : NULL;
}

static char *xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* Python type name of a parsed JSON value (integral doubles read as int). */
static const char *py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* Python == across JSON values (bool is an int subclass; deep containers). */
static bool py_eq(const json_t *a, const json_t *b) {
    bool ap = jpresent(a), bp = jpresent(b);
    if (!ap || !bp) return ap == bp;
    if (is_bool(a) && is_bool(b)) return a->bool_val == b->bool_val;
    if ((is_bool(a) || is_num(a)) && (is_bool(b) || is_num(b))) {
        double av = is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (is_str(a) && is_str(b)) return strcmp(a->str_val, b->str_val) == 0;
    if (is_arr(a) && is_arr(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (is_obj(a) && is_obj(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* Shortest round-trip float repr (Python str(float)); integral -> int form. */
static char *py_float_str(double d) {
    if (d == (double)(long long)d) return xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

/* Python str() of a scalar JSON value (containers: simple repr syntax; the
 * fixture domain keeps str()-coerced containers quote-free). */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s%s", out, i ? ", " : "", item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s]", out); free(out); return next;
        }
        case JSON_OBJECT: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s'%s': %s", out, i ? ", " : "", v->c.keys[i], item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s}", out); free(out); return next;
        }
    }
    return strdup("");
}

/* ── UTF-8 char helpers (Python len/slice count CHARACTERS, not bytes) ── */

static size_t utf8_advance(unsigned char c) {
    return (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
}
static size_t utf8_len(const char *s) {
    size_t n = 0;
    for (size_t i = 0; s[i]; ) { i += utf8_advance((unsigned char)s[i]); n++; }
    return n;
}
/* Byte offset of char index `chars` (clamped to the string end). */
static size_t utf8_off(const char *s, size_t chars) {
    size_t i = 0, n = 0;
    while (s[i] && n < chars) { i += utf8_advance((unsigned char)s[i]); n++; }
    return i;
}
/* Decode one codepoint; advances *i past it. */
static unsigned long utf8_cp(const char *s, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    unsigned long cp;
    size_t adv;
    if (c < 0x80) { cp = c; adv = 1; }
    else if (c < 0xE0) { cp = c & 0x1F; adv = 2; }
    else if (c < 0xF0) { cp = c & 0x0F; adv = 3; }
    else { cp = c & 0x07; adv = 4; }
    for (size_t k = 1; k < adv && s[*i + k]; k++)
        cp = (cp << 6) | ((unsigned char)s[*i + k] & 0x3F);
    *i += adv;
    return cp;
}

/* ASCII lower copy (fixture domain: the "truncat" probe is ASCII). */
static char *ascii_lower_dup(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return out;
}

/* ── trace / seam / raise plumbing ── */

typedef struct orf_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} orf_seams_t;

static json_t *orf_call(const orf_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *orf_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
static bool orf_is_raise(const json_t *r) { return orf_raise_class(r) != NULL; }

static json_t *raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static json_t *raise_type(const char *msg) { return raise_obj("TypeError", msg); }
static json_t *raise_value(const char *msg) { return raise_obj("ValueError", msg); }
static json_t *raise_attr(const json_t *v, const char *attr) {
    char *m = xsf("'%s' object has no attribute '%s'", py_type_name(v), attr);
    json_t *r = raise_obj("AttributeError", m);
    free(m);
    return r;
}
/* Propagate a seam/helper raise result (transfers ownership). */
#define ORF_PROP(r) do { if (orf_is_raise(r)) return (r); } while (0)

/* `except ValueError` also catches its JSONDecodeError subclass. */
static bool is_valueerror_kind(const char *cls) {
    return cls && (!strcmp(cls, "ValueError") || !strcmp(cls, "JSONDecodeError"));
}

/* args builders — null-valued keys are omitted on BOTH sides (trace parity). */
static json_t *args0(void) { return json_object(); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    if (jpresent(v)) json_set(a, k, json_copy(v));
}
static void aset_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── Python iteration / unpack semantics ── */

/* list(v) / for x in v: arrays copy, dicts yield keys, strings yield chars;
 * anything else raises TypeError "'<type>' object is not iterable".
 * Returns a NEW array (caller frees) or a raise object. */
static json_t *py_iter_list(const json_t *v) {
    if (is_arr(v)) return json_copy(v);
    if (is_obj(v)) {
        json_t *out = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(out, json_string(v->c.keys[i]));
        return out;
    }
    if (is_str(v)) {
        json_t *out = json_array();
        const char *s = v->str_val ? v->str_val : "";
        for (size_t i = 0; s[i]; ) {
            size_t start = i;
            utf8_cp(s, &i);
            char *ch = xsf("%.*s", (int)(i - start), s + start);
            json_append(out, json_string(ch));
            free(ch);
        }
        return out;
    }
    char *m = xsf("'%s' object is not iterable", py_type_name(v));
    json_t *r = raise_type(m);
    free(m);
    return r;
}

/* a, b = v — returns NULL on success (a/b NEW copies), else a raise object. */
static json_t *unpack2(const json_t *v, json_t **a, json_t **b) {
    *a = *b = NULL;
    if (!jpresent(v)) {
        char *m = xsf("cannot unpack non-iterable %s object", py_type_name(v));
        json_t *r = raise_type(m);
        free(m);
        return r;
    }
    if (!is_arr(v) && !is_obj(v) && !is_str(v)) {
        char *m = xsf("cannot unpack non-iterable %s object", py_type_name(v));
        json_t *r = raise_type(m);
        free(m);
        return r;
    }
    json_t *it = py_iter_list(v);
    if (orf_is_raise(it)) return it;
    size_t n = it->c.count;
    if (n < 2) {
        char *m = xsf("not enough values to unpack (expected 2, got %zu)", n);
        json_free(it);
        return raise_value(m);
    }
    if (n > 2) {
        char *m = xsf("too many values to unpack (expected 2, got %zu)", n);
        json_free(it);
        return raise_value(m);
    }
    *a = json_copy(it->c.items[0]);
    *b = json_copy(it->c.items[1]);
    json_free(it);
    return NULL;
}

/* ── Python json.loads / json.dumps (default formatting) ── */

/* json.loads(s): NULL on any ValueError (the live capper catches it). The
 * fixture domain stays inside the libjson/Python common grammar (no NaN,
 * leading-zero numbers, raw control chars or invalid escapes). */
static json_t *py_json_loads(const char *s) {
    char *err = NULL;
    json_t *v = json_parse(s, &err);
    free(err);
    return v;
}

/* ensure_ascii string escaping into a growable buffer. */
typedef struct { char *buf; size_t len, cap; } sbuf_t;
static void sb_init(sbuf_t *b) { b->cap = 64; b->len = 0; b->buf = malloc(b->cap); b->buf[0] = '\0'; }
static void sb_putc(sbuf_t *b, char c) {
    if (b->len + 2 > b->cap) { b->cap *= 2; b->buf = realloc(b->buf, b->cap); }
    b->buf[b->len++] = c; b->buf[b->len] = '\0';
}
static void sb_puts(sbuf_t *b, const char *s) { while (*s) sb_putc(b, *s++); }
static void sb_json_string(sbuf_t *b, const char *s) {
    sb_putc(b, '"');
    for (size_t i = 0; s[i]; ) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') { sb_puts(b, "\\\""); i++; continue; }
        if (c == '\\') { sb_puts(b, "\\\\"); i++; continue; }
        if (c < 0x20) {
            switch (c) {
                case '\b': sb_puts(b, "\\b"); break;
                case '\f': sb_puts(b, "\\f"); break;
                case '\n': sb_puts(b, "\\n"); break;
                case '\r': sb_puts(b, "\\r"); break;
                case '\t': sb_puts(b, "\\t"); break;
                default: {
                    char tmp[8];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)c);
                    sb_puts(b, tmp);
                }
            }
            i++;
            continue;
        }
        if (c < 0x7f) { sb_putc(b, (char)c); i++; continue; }
        unsigned long cp = utf8_cp(s, &i);
        char tmp[16];
        if (cp >= 0x10000) {
            unsigned long v = cp - 0x10000;
            snprintf(tmp, sizeof(tmp), "\\u%04x\\u%04x",
                     (unsigned)(0xD800 + (v >> 10)), (unsigned)(0xDC00 + (v & 0x3FF)));
        } else {
            snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)cp);
        }
        sb_puts(b, tmp);
    }
    sb_putc(b, '"');
}
static void sb_json_value(sbuf_t *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: sb_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char *s = py_float_str(v->num_val);
            sb_puts(b, s);
            free(s);
            return;
        }
        case JSON_STRING: sb_json_string(b, v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY: {
            sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                sb_json_value(b, v->c.items[i]);
            }
            sb_putc(b, ']');
            return;
        }
        case JSON_OBJECT: {
            sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                sb_json_string(b, v->c.keys[i]);
                sb_puts(b, ": ");
                sb_json_value(b, v->c.items[i]);
            }
            sb_putc(b, '}');
            return;
        }
    }
}
/* json.dumps(v) with Python defaults: separators (', ', ': '), ensure_ascii.
 * Caller frees. Fixture domain: numbers are integral (parsed JSON loses the
 * int/float identity; non-integral floats would need float.__repr__ parity). */
static char *py_json_dumps(const json_t *v) {
    sbuf_t b;
    sb_init(&b);
    sb_json_value(&b, v);
    return b.buf;
}

/* ── _cap_text core (char-aware head + cut marker) ── */

/* text[:keep] + "...[" + str(len(text) - keep) + " more chars]".
 * Returns a NEW string node or a raise object. */
static json_t *cap_text_core(const char *text, double keep) {
    if (keep != (double)(long long)keep)
        return raise_type("slice indices must be integers or None or have an __index__ method");
    long long k = (long long)keep;
    long long chars = (long long)utf8_len(text);
    long long n = k < 0 ? (chars + k > 0 ? chars + k : 0) : (k < chars ? k : chars);
    size_t off = utf8_off(text, (size_t)n);
    char *out = xsf("%.*s...[%lld more chars]", (int)off, text, chars - k);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: orf_cap_text @ gateway/platforms/api_server_openai_routes.py:_cap_text */
json_t *orf_cap_text(const json_t *text, double keep) {
    if (is_str(text)) return cap_text_core(text->str_val ? text->str_val : "", keep);
    if (is_arr(text)) {
        /* list[:keep] slices fine, then list + str fails on concatenation. */
        if (keep != (double)(long long)keep)
            return raise_type("slice indices must be integers or None or have an __index__ method");
        return raise_type("can only concatenate list (not \"str\") to list");
    }
    char *m = xsf("'%s' object is not subscriptable", py_type_name(text));
    json_t *r = raise_type(m);
    free(m);
    return r;
}

/* ── _is_reasoning_input_item (pure leaf; real inside the parse chains) ── */

static bool orf_is_reasoning(const json_t *item) {
    if (!is_obj(item)) return false;
    const json_t *t = json_obj_get(item, "type");
    return is_str(t) && strcmp(t->str_val, "reasoning") == 0;
}

/* PoP: orf_is_reasoning_input_item @ gateway/platforms/api_server_openai_routes.py:_is_reasoning_input_item */
json_t *orf_is_reasoning_input_item(const json_t *item) {
    return json_bool(orf_is_reasoning(item));
}

/* ── _result_flags (pure leaf) ── */

/* PoP: orf_result_flags @ gateway/platforms/api_server_openai_routes.py:_result_flags */
json_t *orf_result_flags(const json_t *result) {
    json_t *out = json_array();
    if (!is_obj(result)) {
        json_append(out, json_bool(true));
        json_append(out, json_bool(false));
        json_append(out, json_bool(false));
        json_append(out, json_null());
        return out;
    }
    const json_t *completed = json_obj_get(result, "completed");
    bool comp_default = completed ? jtruth(completed) : true;  /* get("completed", True) */
    bool comp = comp_default && !jtruth(json_obj_get(result, "interrupted"));
    json_append(out, json_bool(comp));
    json_append(out, json_bool(jtruth(json_obj_get(result, "partial"))));
    json_append(out, json_bool(jtruth(json_obj_get(result, "failed"))));
    const json_t *err = json_obj_get(result, "error");
    json_append(out, err ? json_copy(err) : json_null());
    return out;
}

/* ── _budget_exhausted (pure leaf) ── */

/* PoP: orf_budget_exhausted @ gateway/platforms/api_server_openai_routes.py:_budget_exhausted */
json_t *orf_budget_exhausted(const json_t *result) {
    char *reason = strdup("");
    if (is_obj(result)) {
        const json_t *v = json_obj_get(result, "turn_exit_reason");
        if (jtruth(v)) { free(reason); reason = py_str(v); }
    }
    bool r = strncmp(reason, "budget_exhausted", 16) == 0
          || strncmp(reason, "max_iterations_reached(", 23) == 0;
    free(reason);
    return json_bool(r);
}

/* ── _finish_reason (error-mapping decision chain) ── */

/* PoP: orf_finish_reason @ gateway/platforms/api_server_openai_routes.py:_finish_reason */
json_t *orf_finish_reason(const json_t *completed, const json_t *is_partial,
                          const json_t *is_failed, const json_t *err_msg,
                          const json_t *agent_error, bool budget_exhausted) {
    if (jtruth(is_partial) && jtruth(err_msg)) {
        const char *es = jstr(err_msg);
        if (!es) return raise_attr(err_msg, "lower");
        char *low = ascii_lower_dup(es);
        bool trunc = strstr(low, "truncat") != NULL;
        free(low);
        if (trunc) return json_string("length");
    }
    if (jpresent(agent_error) || jtruth(is_failed)
        || (!jtruth(completed) && jtruth(err_msg)))
        return json_string("error");
    if (budget_exhausted) return json_string("length");
    return json_string(jtruth(completed) ? "stop" : "cancelled");
}

/* ── _settled (pure leaf) ── */

/* PoP: orf_settled @ gateway/platforms/api_server_openai_routes.py:_settled */
json_t *orf_settled(const json_t *record) {
    if (!jpresent(record)) return json_bool(false);
    if (!is_obj(record)) return raise_attr(record, "get");
    return json_bool(jpresent(json_obj_get(record, "response")));
}

/* ── _responses_fingerprint_keys (pure leaf + module constant) ── */

static const char *ORF_FINGERPRINT_KEYS[] = {
    "input", "instructions", "previous_response_id", "conversation",
    "conversation_history", "model", "provider", "model_options", "tools",
};

/* PoP: orf_responses_fingerprint_keys @ gateway/platforms/api_server_openai_routes.py:_responses_fingerprint_keys */
json_t *orf_responses_fingerprint_keys(const json_t *body) {
    if (!is_obj(body)) return raise_attr(body, "get");
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(ORF_FINGERPRINT_KEYS) / sizeof(ORF_FINGERPRINT_KEYS[0]); i++)
        json_append(out, json_string(ORF_FINGERPRINT_KEYS[i]));
    if (jpresent(json_obj_get(body, "truncation")))
        json_append(out, json_string("truncation"));
    return out;
}

/* ── _response_status (terminal-status decision chain) ── */

/* PoP: orf_response_status @ gateway/platforms/api_server_openai_routes.py:_response_status */
json_t *orf_response_status(const json_t *result) {
    if (!is_obj(result)) return raise_attr(result, "get");
    if (jtruth(json_obj_get(result, "interrupted"))) return json_string("cancelled");
    if (jtruth(json_obj_get(result, "failed")) || jtruth(json_obj_get(result, "error")))
        return json_string("failed");
    const json_t *completed = json_obj_get(result, "completed");
    /* `is False` — identity, not equality: numeric 0 does NOT match. */
    if (is_bool(completed) && !completed->bool_val) return json_string("incomplete");
    return json_string("completed");
}

/* ── _hermes_extras (pure dict builder + error_code chain) ── */

/* PoP: orf_hermes_extras @ gateway/platforms/api_server_openai_routes.py:_hermes_extras */
json_t *orf_hermes_extras(const json_t *completed, const json_t *is_partial,
                          const json_t *is_failed, const json_t *err_msg,
                          const json_t *finish_reason, bool budget_exhausted) {
    bool length = is_str(finish_reason) && strcmp(finish_reason->str_val, "length") == 0;
    const char *error_code = (budget_exhausted && length) ? "iteration_budget_exhausted"
                           : length ? "output_truncated" : "agent_error";
    json_t *o = json_object();
    json_set(o, "completed", completed ? json_copy(completed) : json_null());
    json_set(o, "partial", is_partial ? json_copy(is_partial) : json_null());
    json_set(o, "failed", is_failed ? json_copy(is_failed) : json_null());
    json_set(o, "error", err_msg ? json_copy(err_msg) : json_null());
    json_set(o, "error_code", json_string(error_code));
    return o;
}

/* ── _post_stream_transform (stream rewrite branch) ── */

/* PoP: orf_post_stream_transform @ gateway/platforms/api_server_openai_routes.py:_post_stream_transform */
json_t *orf_post_stream_transform(const json_t *result) {
    json_t *out = json_array();
    if (!is_obj(result) || !jtruth(json_obj_get(result, "response_transformed"))) {
        json_append(out, json_string(""));
        json_append(out, json_bool(false));
        return out;
    }
    const json_t *final_v = json_obj_get(result, "final_response");
    const json_t *orig_v = json_obj_get(result, "pre_transform_response");
    json_t *final = jtruth(final_v) ? json_copy(final_v) : json_string("");
    json_t *original = jtruth(orig_v) ? json_copy(orig_v) : json_string("");
    if (jtruth(original)) {
        const char *fs = jstr(final);
        if (!fs) {
            json_t *r = raise_attr(final, "startswith");
            json_free(final); json_free(original); json_free(out);
            return r;
        }
        const char *os = jstr(original);
        if (!os) {
            char *m = xsf("startswith first arg must be str or a tuple of str, not %s",
                          py_type_name(original));
            json_t *r = raise_type(m);
            free(m);
            json_free(final); json_free(original); json_free(out);
            return r;
        }
        size_t olen = strlen(os);
        if (strncmp(fs, os, olen) == 0) {
            json_append(out, json_string(fs + olen));
            json_append(out, json_bool(true));
            json_free(final); json_free(original);
            return out;
        }
    }
    json_append(out, final);
    json_append(out, json_bool(false));
    json_free(original);
    return out;
}

/* ── _matching_replay (store read unit) ── */

/* PoP: orf_matching_replay @ gateway/platforms/api_server_openai_routes.py:_matching_replay */
json_t *orf_matching_replay(const json_t *key, const json_t *binding, const orf_seams_t *s) {
    json_t *a = args0();
    aset_j(a, "key", key);
    aset_j(a, "binding", binding);
    json_t *matches = orf_call(s, "store.request_key_matches", a);
    json_free(a);
    ORF_PROP(matches);
    json_t *out = json_array();
    if (!jtruth(matches)) {
        json_free(matches);
        json_append(out, json_bool(false));
        json_append(out, json_null());
        return out;
    }
    json_free(matches);
    a = args0();
    aset_j(a, "key", key);
    json_t *rec = orf_call(s, "store.get", a);
    json_free(a);
    if (orf_is_raise(rec)) { json_free(out); return rec; }
    json_append(out, json_bool(true));
    json_append(out, rec ? rec : json_null());
    return out;
}

/* ── _store_nonstream_response (store write unit) ── */

/* PoP: orf_store_nonstream_response @ gateway/platforms/api_server_openai_routes.py:_store_nonstream_response */
json_t *orf_store_nonstream_response(const json_t *snapshot, const json_t *replay_record,
                                     const json_t *conversation, const json_t *terminal_replay,
                                     const orf_seams_t *s) {
    if (jpresent(snapshot)) {
        json_t *rid = NULL, *data = NULL;
        json_t *u = unpack2(snapshot, &rid, &data);
        if (u) return u;
        /* advance = conversation and (not terminal_replay
         *             or store.get_conversation(conversation) in (None, response_id)) */
        bool advance = false;
        if (jtruth(conversation)) {
            if (!jtruth(terminal_replay)) {
                advance = true;
            } else {
                json_t *a = args0();
                aset_j(a, "conversation", conversation);
                json_t *gc = orf_call(s, "store.get_conversation", a);
                json_free(a);
                if (orf_is_raise(gc)) { json_free(rid); json_free(data); return gc; }
                advance = !jpresent(gc) || py_eq(gc, rid);
                json_free(gc);
            }
        }
        json_t *a = args0();
        aset_j(a, "key", rid);
        aset_j(a, "value", data);
        json_t *pr = orf_call(s, "store.put", a);
        json_free(a);
        if (orf_is_raise(pr)) { json_free(rid); json_free(data); return pr; }
        json_free(pr);
        if (advance) {
            a = args0();
            aset_j(a, "conversation", conversation);
            aset_j(a, "response_id", rid);
            json_t *sr = orf_call(s, "store.set_conversation", a);
            json_free(a);
            if (orf_is_raise(sr)) { json_free(rid); json_free(data); return sr; }
            json_free(sr);
        }
        json_free(rid);
        json_free(data);
    }
    if (jpresent(replay_record)) {
        json_t *rk = NULL, *rv = NULL;
        json_t *u = unpack2(replay_record, &rk, &rv);
        if (u) return u;
        json_t *a = args0();
        aset_j(a, "key", rk);
        aset_j(a, "value", rv);
        json_t *pr = orf_call(s, "store.put", a);
        json_free(a);
        json_free(rk);
        json_free(rv);
        if (orf_is_raise(pr)) return pr;
        json_free(pr);
    }
    return NULL;
}

/* ── shared multimodal ValueError translation ── */

/* Normalize seam raised: ValueError-kind -> multimodal_validation_error seam
 * (returns its NEW result, *caught=true); any other raise propagates
 * (*caught=false, result is the raise). A non-raise passes through. */
static json_t *mm_translate(json_t *r, const char *param, const orf_seams_t *s, bool *caught) {
    *caught = false;
    if (!orf_is_raise(r)) return r;
    if (!is_valueerror_kind(orf_raise_class(r))) return r;
    *caught = true;
    const char *msg = json_get_str(r, "msg", "");
    json_t *a = args0();
    aset_s(a, "exc", msg);
    aset_s(a, "param", param);
    json_free(r);
    json_t *err = orf_call(s, "multimodal_validation_error", a);
    json_free(a);
    return err;
}

/* ── _parse_chat_messages (request validation chain) ── */

/* PoP: orf_parse_chat_messages @ gateway/platforms/api_server_openai_routes.py:_parse_chat_messages */
json_t *orf_parse_chat_messages(const json_t *messages, const orf_seams_t *s) {
    json_t *it = py_iter_list(messages);
    ORF_PROP(it);
    json_t *system = NULL;
    json_t *conv = json_array();
    for (size_t idx = 0; idx < it->c.count; idx++) {
        const json_t *msg = it->c.items[idx];
        if (!is_obj(msg)) {
            json_t *r = raise_attr(msg, "get");
            json_free(it); json_free(system); json_free(conv);
            return r;
        }
        const json_t *role = json_obj_get(msg, "role");
        const json_t *raw = json_obj_get(msg, "content");
        const char *rs = jstr(role);
        if (rs && strcmp(rs, "system") == 0) {
            json_t *a = args0();
            if (raw) aset_j(a, "content", raw); else aset_s(a, "content", "");
            json_t *c = orf_call(s, "normalize_chat_content", a);
            json_free(a);
            if (orf_is_raise(c)) { json_free(it); json_free(system); json_free(conv); return c; }
            if (!system) {
                system = c;
            } else {
                /* system_prompt + "\n" + content (left-assoc str concat). */
                const char *ls = jstr(system);
                if (!ls) {
                    char *m = xsf("unsupported operand type(s) for +: '%s' and 'str'",
                                  py_type_name(system));
                    json_t *r = raise_type(m);
                    free(m);
                    json_free(c); json_free(it); json_free(system); json_free(conv);
                    return r;
                }
                const char *cs = jstr(c);
                if (!cs) {
                    char *m = xsf("can only concatenate str (not \"%s\") to str",
                                  py_type_name(c));
                    json_t *r = raise_type(m);
                    free(m);
                    json_free(c); json_free(it); json_free(system); json_free(conv);
                    return r;
                }
                char *joined = xsf("%s\n%s", ls, cs);
                json_free(system);
                json_free(c);
                system = json_string(joined);
                free(joined);
            }
        } else if (rs && (strcmp(rs, "user") == 0 || strcmp(rs, "assistant") == 0)) {
            json_t *a = args0();
            if (raw) aset_j(a, "content", raw); else aset_s(a, "content", "");
            json_t *c = orf_call(s, "normalize_multimodal_content", a);
            json_free(a);
            bool caught = false;
            char *param = xsf("messages[%zu].content", idx);
            c = mm_translate(c, param, s, &caught);
            free(param);
            if (caught) {
                /* return None, None, error_response_or_raise */
                if (orf_is_raise(c)) { json_free(it); json_free(system); json_free(conv); return c; }
                json_t *out = json_array();
                json_append(out, json_null());
                json_append(out, json_null());
                json_append(out, c);
                json_free(it); json_free(system); json_free(conv);
                return out;
            }
            ORF_PROP(c);
            json_t *row = json_object();
            json_set(row, "role", json_string(rs));
            json_set(row, "content", c ? c : json_null());
            json_append(conv, row);
        }
        /* other roles: skipped */
    }
    json_free(it);
    json_t *out = json_array();
    json_append(out, system ? system : json_null());
    json_append(out, conv);
    json_append(out, json_null());
    return out;
}

/* ── _parse_responses_input (request validation chain) ── */

/* PoP: orf_parse_responses_input @ gateway/platforms/api_server_openai_routes.py:_parse_responses_input */
json_t *orf_parse_responses_input(const json_t *raw_input, const orf_seams_t *s) {
    json_t *msgs = json_array();
    if (is_str(raw_input)) {
        json_t *row = json_object();
        json_set(row, "role", json_string("user"));
        json_set(row, "content", json_copy(raw_input));
        json_append(msgs, row);
    } else if (is_arr(raw_input)) {
        for (size_t idx = 0; idx < raw_input->c.count; idx++) {
            const json_t *item = raw_input->c.items[idx];
            if (is_str(item)) {
                json_t *row = json_object();
                json_set(row, "role", json_string("user"));
                json_set(row, "content", json_copy(item));
                json_append(msgs, row);
            } else if (orf_is_reasoning(item)) {
                continue;
            } else if (is_obj(item)) {
                const json_t *raw = json_obj_get(item, "content");
                json_t *a = args0();
                if (raw) aset_j(a, "content", raw); else aset_s(a, "content", "");
                json_t *c = orf_call(s, "normalize_multimodal_content", a);
                json_free(a);
                bool caught = false;
                char *param = xsf("input[%zu].content", idx);
                c = mm_translate(c, param, s, &caught);
                free(param);
                if (caught) {
                    if (orf_is_raise(c)) { json_free(msgs); return c; }
                    json_t *out = json_array();
                    json_append(out, json_null());
                    json_append(out, c);
                    json_free(msgs);
                    return out;
                }
                ORF_PROP(c);
                const json_t *role = json_obj_get(item, "role");
                json_t *row = json_object();
                json_set(row, "role", role ? json_copy(role) : json_string("user"));
                json_set(row, "content", c ? c : json_null());
                json_append(msgs, row);
            }
            /* other item types: silently skipped */
        }
    } else {
        json_free(msgs);
        json_t *a = args0();
        aset_s(a, "message", "'input' must be a string or array");
        aset_n(a, "status", 400);
        json_t *err = orf_call(s, "error_response", a);
        json_free(a);
        ORF_PROP(err);
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, err);
        return out;
    }
    json_t *out = json_array();
    json_append(out, msgs);
    json_append(out, json_null());
    return out;
}

/* ── _parse_conversation_history (request validation chain) ── */

/* PoP: orf_parse_conversation_history @ gateway/platforms/api_server_openai_routes.py:_parse_conversation_history */
json_t *orf_parse_conversation_history(const json_t *raw_history, const orf_seams_t *s) {
    if (!is_arr(raw_history)) {
        json_t *a = args0();
        aset_s(a, "message", "'conversation_history' must be an array of message objects");
        aset_n(a, "status", 400);
        json_t *err = orf_call(s, "error_response", a);
        json_free(a);
        ORF_PROP(err);
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, err);
        return out;
    }
    json_t *hist = json_array();
    for (size_t i = 0; i < raw_history->c.count; i++) {
        const json_t *entry = raw_history->c.items[i];
        if (orf_is_reasoning(entry)) continue;
        if (!is_obj(entry) || !json_has(entry, "role") || !json_has(entry, "content")) {
            json_free(hist);
            char *m = xsf("conversation_history[%zu] must have 'role' and 'content' fields", i);
            json_t *a = args0();
            aset_s(a, "message", m);
            aset_n(a, "status", 400);
            free(m);
            json_t *err = orf_call(s, "error_response", a);
            json_free(a);
            ORF_PROP(err);
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, err);
            return out;
        }
        const json_t *content = json_obj_get(entry, "content");
        json_t *a = args0();
        aset_j(a, "content", content);
        json_t *c = orf_call(s, "normalize_multimodal_content", a);
        json_free(a);
        bool caught = false;
        char *param = xsf("conversation_history[%zu].content", i);
        c = mm_translate(c, param, s, &caught);
        free(param);
        if (caught) {
            if (orf_is_raise(c)) { json_free(hist); return c; }
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, c);
            json_free(hist);
            return out;
        }
        ORF_PROP(c);
        char *role_s = py_str(json_obj_get(entry, "role"));
        json_t *row = json_object();
        json_set(row, "role", json_string(role_s));
        json_set(row, "content", c ? c : json_null());
        free(role_s);
        json_append(hist, row);
    }
    json_t *out = json_array();
    json_append(out, hist);
    json_append(out, json_null());
    return out;
}

/* ── _select_request_route (model resolution flow) ── */

/* PoP: orf_select_request_route @ gateway/platforms/api_server_openai_routes.py:_select_request_route */
json_t *orf_select_request_route(const json_t *body, const json_t *session_id,
                                 const json_t *gateway_session_key, const json_t *model_alias,
                                 const json_t *model_name, bool direct_model_requests,
                                 const orf_seams_t *s) {
    json_t *a = args0();
    aset_j(a, "model_alias", model_alias);
    json_t *route = orf_call(s, "resolve_route", a);
    json_free(a);
    ORF_PROP(route);
    a = args0();
    aset_j(a, "body", body);
    aset_j(a, "virtual_model", model_name);
    aset_b(a, "allow_bare_model", direct_model_requests);
    json_t *overrides = orf_call(s, "request_agent_overrides", a);
    json_free(a);
    if (orf_is_raise(overrides)) { json_free(route); return overrides; }
    if (!is_obj(overrides)) {
        json_t *r = raise_attr(overrides, "get");
        json_free(route); json_free(overrides);
        return r;
    }
    a = args0();
    aset_j(a, "session_id", session_id);
    aset_j(a, "gateway_session_key", gateway_session_key);
    aset_j(a, "requested_model", json_obj_get(overrides, "requested_model"));
    aset_j(a, "requested_provider", json_obj_get(overrides, "requested_provider"));
    aset_j(a, "route", route);
    json_t *err = orf_call(s, "request_route_conflict_error", a);
    json_free(a);
    if (orf_is_raise(err)) { json_free(route); json_free(overrides); return err; }
    json_t *error = NULL;
    if (jtruth(err)) {
        a = args0();
        aset_j(a, "message", err);
        aset_n(a, "status", 400);
        error = orf_call(s, "error_response", a);
        json_free(a);
        if (orf_is_raise(error)) {
            json_free(route); json_free(overrides); json_free(err);
            return error;
        }
    }
    json_free(err);
    json_t *out = json_array();
    json_append(out, route ? route : json_null());
    json_append(out, overrides);
    json_append(out, error ? error : json_null());
    return out;
}

/* ── _cap_history_tool_outputs (payload capper; real json.loads/dumps) ── */

/* PoP: orf_cap_history_tool_outputs @ gateway/platforms/api_server_openai_routes.py:_cap_history_tool_outputs */
json_t *orf_cap_history_tool_outputs(const json_t *history, double max_chars) {
    if (max_chars <= 0) return json_copy(history);
    json_t *it = py_iter_list(history);
    ORF_PROP(it);
    json_t *out = json_array();
    for (size_t mi = 0; mi < it->c.count; mi++) {
        const json_t *msg = it->c.items[mi];
        if (!is_obj(msg)) {
            json_append(out, json_copy(msg));
            continue;
        }
        json_t *m = json_copy(msg);
        const json_t *role = json_obj_get(m, "role");
        const char *rs = jstr(role);
        const json_t *content = json_obj_get(m, "content");
        if (rs && strcmp(rs, "tool") == 0 && is_str(content)
            && (double)utf8_len(content->str_val) > max_chars) {
            json_t *capped = cap_text_core(content->str_val, max_chars);
            if (orf_is_raise(capped)) { json_free(m); json_free(it); json_free(out); return capped; }
            json_set(m, "content", capped);
        }
        const json_t *tool_calls = json_obj_get(m, "tool_calls");
        if (rs && strcmp(rs, "assistant") == 0 && is_arr(tool_calls)) {
            json_t *capped_calls = json_array();
            for (size_t ci = 0; ci < tool_calls->c.count; ci++) {
                const json_t *call = tool_calls->c.items[ci];
                const json_t *fn = is_obj(call) ? json_obj_get(call, "function") : NULL;
                const json_t *raw = is_obj(fn) ? json_obj_get(fn, "arguments") : NULL;
                json_t *c = json_copy(call);
                if (is_str(raw) && (double)utf8_len(raw->str_val) > max_chars) {
                    json_t *args = py_json_loads(raw->str_val);
                    if (is_obj(args)) {
                        for (size_t ki = 0; ki < args->c.count; ki++) {
                            const json_t *v = args->c.items[ki];
                            if (is_str(v) && (double)utf8_len(v->str_val) > max_chars) {
                                json_t *capped = cap_text_core(v->str_val, max_chars);
                                if (orf_is_raise(capped)) {
                                    json_free(args); json_free(c); json_free(capped_calls);
                                    json_free(m); json_free(it); json_free(out);
                                    return capped;
                                }
                                json_set(args, args->c.keys[ki], capped);
                            }
                        }
                        char *dumps = py_json_dumps(args);
                        json_free(args);
                        json_t *newfn = json_copy(fn);
                        json_set(newfn, "arguments", json_string(dumps));
                        free(dumps);
                        json_set(c, "function", newfn);
                    } else {
                        json_free(args);
                    }
                }
                json_append(capped_calls, c);
            }
            json_set(m, "tool_calls", capped_calls);
        }
        json_append(out, m);
    }
    json_free(it);
    return out;
}

/* ── _build_response_conversation_history (transcript build flow) ── */

/* PoP: orf_build_response_conversation_history @ gateway/platforms/api_server_openai_routes.py:_build_response_conversation_history */
json_t *orf_build_response_conversation_history(const json_t *conversation_history,
                                                const json_t *user_message, const json_t *result,
                                                const json_t *final_response,
                                                double tool_output_max_chars, const orf_seams_t *s) {
    json_t *prior = py_iter_list(conversation_history);
    ORF_PROP(prior);
    json_t *current_user = json_object();
    json_set(current_user, "role", json_string("user"));
    json_set(current_user, "content", user_message ? json_copy(user_message) : json_null());
    if (is_obj(result)) {
        const json_t *row = json_obj_get(result, "_turn_user_row");
        if (is_obj(row)) {
            json_free(current_user);
            current_user = json_copy(row);
        }
    }
    const json_t *agent_messages = is_obj(result) ? json_obj_get(result, "messages") : NULL;
    json_t *history = NULL;
    if (is_arr(agent_messages) && agent_messages->c.count > 0) {
        json_t *a = args0();
        aset_j(a, "conversation_history", conversation_history);
        aset_j(a, "user_message", user_message);
        aset_j(a, "result", result);
        json_t *ts = orf_call(s, "turn_start_index", a);
        json_free(a);
        if (orf_is_raise(ts)) { json_free(prior); json_free(current_user); return ts; }
        bool compressed = jtruth(json_obj_get(result, "_compressed"));
        if (jtruth(ts) || compressed) {
            history = json_copy(agent_messages);
            json_free(prior);
        } else {
            history = prior;
            json_append(history, current_user);
            current_user = NULL;
            for (size_t i = 0; i < agent_messages->c.count; i++)
                json_append(history, json_copy(agent_messages->c.items[i]));
        }
        json_free(ts);
    } else {
        history = prior;
        json_append(history, current_user);
        current_user = NULL;
        json_t *assistant = json_object();
        json_set(assistant, "role", json_string("assistant"));
        json_set(assistant, "content", final_response ? json_copy(final_response) : json_null());
        json_append(history, assistant);
    }
    json_free(current_user);
    json_t *ret = orf_cap_history_tool_outputs(history, tool_output_max_chars);
    json_free(history);
    return ret;
}
