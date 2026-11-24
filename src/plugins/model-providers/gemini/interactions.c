/*
 * interactions.c - C11 port of upstream plugins/model-providers/gemini/interactions.py
 * (gates w33_interactio_95c25f_p1 / w33_interactio_95c25f_p2).
 *
 * PoP: w33_interactio_95c25f_p1_input_steps @ plugins/model-providers/gemini/interactions.py:build_input_steps
 * PoP: w33_interactio_95c25f_p1_request @ plugins/model-providers/gemini/interactions.py:build_interactions_request
 * PoP: w33_interactio_95c25f_p1_continuation @ plugins/model-providers/gemini/interactions.py:continuation_request
 * PoP: w33_interactio_95c25f_p1_tools @ plugins/model-providers/gemini/interactions.py:_tools
 * PoP: w33_interactio_95c25f_p1_tool_choice @ plugins/model-providers/gemini/interactions.py:_tool_choice
 * PoP: w33_interactio_95c25f_p1_response_format @ plugins/model-providers/gemini/interactions.py:_response_format
 * PoP: w33_interactio_95c25f_p1_image @ plugins/model-providers/gemini/interactions.py:_image_content
 * PoP: w33_interactio_95c25f_p1_content_blocks @ plugins/model-providers/gemini/interactions.py:_content_blocks
 * PoP: w33_interactio_95c25f_p1_function_result @ plugins/model-providers/gemini/interactions.py:_function_result
 * PoP: w33_interactio_95c25f_p1_call_arguments @ plugins/model-providers/gemini/interactions.py:_call_arguments
 * PoP: w33_interactio_95c25f_p1_assistant_steps @ plugins/model-providers/gemini/interactions.py:_assistant_steps
 * PoP: w33_interactio_95c25f_p1_prepare_params @ agent/gemini_schema.py:prepare_gemini_tool_parameters
 * PoP: w33_interactio_95c25f_p1_thinking @ agent/gemini_native_adapter.py:_normalize_thinking_config
 * PoP: w33_interactio_95c25f_p1_max_tokens @ agent/gemini_native_adapter.py:_effective_gemini_max_output_tokens
 * PoP: w33_interactio_95c25f_p2_usage @ plugins/model-providers/gemini/interactions.py:usage_namespace
 * PoP: w33_interactio_95c25f_p2_merge_usage @ plugins/model-providers/gemini/interactions.py:merge_usage
 * PoP: w33_interactio_95c25f_p2_count @ plugins/model-providers/gemini/interactions.py:_count
 * PoP: w33_interactio_95c25f_p2_finish_reason @ plugins/model-providers/gemini/interactions.py:_finish_reason
 * PoP: w33_interactio_95c25f_p2_interaction_error @ plugins/model-providers/gemini/interactions.py:interaction_error
 * PoP: w33_interactio_95c25f_p2_thought_text @ plugins/model-providers/gemini/interactions.py:_thought_text
 * PoP: w33_interactio_95c25f_p2_translate @ plugins/model-providers/gemini/interactions.py:translate_interaction
 * PoP: w33_interactio_95c25f_p2_next_hop @ plugins/model-providers/gemini/interactions.py:StreamState.next_hop
 * PoP: w33_interactio_95c25f_p2_close_dropped_hop @ plugins/model-providers/gemini/interactions.py:StreamState.close_dropped_hop
 * PoP: w33_interactio_95c25f_p2_tool_delta @ plugins/model-providers/gemini/interactions.py:_tool_delta
 * PoP: w33_interactio_95c25f_p2_token @ plugins/model-providers/gemini/interactions.py:_token
 * PoP: w33_interactio_95c25f_p2_on_error @ plugins/model-providers/gemini/interactions.py:_on_error
 * PoP: w33_interactio_95c25f_p2_on_created @ plugins/model-providers/gemini/interactions.py:_on_created
 * PoP: w33_interactio_95c25f_p2_start_thought @ plugins/model-providers/gemini/interactions.py:_start_thought
 * PoP: w33_interactio_95c25f_p2_output_signature @ plugins/model-providers/gemini/interactions.py:_output_signature
 * PoP: w33_interactio_95c25f_p2_start_output @ plugins/model-providers/gemini/interactions.py:_start_output
 * PoP: w33_interactio_95c25f_p2_start_call @ plugins/model-providers/gemini/interactions.py:_start_call
 * PoP: w33_interactio_95c25f_p2_on_step_start @ plugins/model-providers/gemini/interactions.py:_on_step_start
 * PoP: w33_interactio_95c25f_p2_delta_text @ plugins/model-providers/gemini/interactions.py:_delta_text
 * PoP: w33_interactio_95c25f_p2_delta_summary @ plugins/model-providers/gemini/interactions.py:_delta_summary
 * PoP: w33_interactio_95c25f_p2_delta_signature @ plugins/model-providers/gemini/interactions.py:_delta_signature
 * PoP: w33_interactio_95c25f_p2_delta_arguments @ plugins/model-providers/gemini/interactions.py:_delta_arguments
 * PoP: w33_interactio_95c25f_p2_on_step_delta @ plugins/model-providers/gemini/interactions.py:_on_step_delta
 * PoP: w33_interactio_95c25f_p2_on_step_stop @ plugins/model-providers/gemini/interactions.py:_on_step_stop
 * PoP: w33_interactio_95c25f_p2_on_status @ plugins/model-providers/gemini/interactions.py:_on_status
 * PoP: w33_interactio_95c25f_p2_on_completed @ plugins/model-providers/gemini/interactions.py:_on_completed
 * PoP: w33_interactio_95c25f_p2_translate_stream_event @ plugins/model-providers/gemini/interactions.py:translate_stream_event
 * PoP: w33_interactio_95c25f_p2_event_token @ plugins/model-providers/gemini/interactions.py:event_token
 * PoP: w33_interactio_95c25f_p2_finish_ceiling @ plugins/model-providers/gemini/interactions.py:finish_ceiling
 * PoP: w33_interactio_95c25f_p2_iter_events @ plugins/model-providers/gemini/interactions.py:iter_events
 * PoP: w33_interactio_95c25f_p2_stream_chunk @ agent/gemini_native_adapter.py:_make_stream_chunk
 * PoP: w33_interactio_95c25f_p2_tool_call_ns @ agent/gemini_native_adapter.py:_tool_call_ns
 * PoP: w33_interactio_95c25f_p2_extra_from_part @ agent/gemini_native_adapter.py:_tool_call_extra_from_part
 *
 * SEAMS (stay in the caller): the httpx POST, SSE line reading, the Retry-After /
 * API-key guidance of http_error (it needs the response object), uuid minting and
 * the wall clock (both normalized in the differential gates).
 *
 * Everything here is a pure function of the values it is handed.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w33_interactio_95c25f_p1.h"
#include "noc_w33_interactio_95c25f_p2.h"

/* ── generic value helpers ─────────────────────────────────────────────────── */

/* Python truthiness of a JSON value. */
static int gt_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static char *gt_strdup(const char *s) {
    size_t n;
    char *o;
    if (!s) s = "";
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() over the scalar shapes a wire value can hold. */
static char *gt_pystr(const json_t *v) {
    char buf[64];
    if (!v) return gt_strdup("None");
    switch (v->type) {
    case JSON_NULL:   return gt_strdup("None");
    case JSON_BOOL:   return gt_strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return gt_strdup(buf);
    case JSON_STRING: return gt_strdup(v->str_val ? v->str_val : "");
    default:          return gt_strdup("");
    }
}

/* str(v or "") */
static char *gt_pystr_or_empty(const json_t *v) {
    return gt_truthy(v) ? gt_pystr(v) : gt_strdup("");
}

/* str.strip() */
static char *gt_trim(const char *s) {
    size_t a = 0, b;
    char *o;
    if (!s) return gt_strdup("");
    b = strlen(s);
    while (a < b && strchr(" \t\n\r\v\f", s[a])) a++;
    while (b > a && strchr(" \t\n\r\v\f", s[b - 1])) b--;
    o = (char *)malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

static void gt_lower_in_place(char *s) {
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static void gt_upper_in_place(char *s) {
    if (!s) return;
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

static void gt_replace_str(char **dst, const char *src) {
    if (!dst) return;
    free(*dst);
    *dst = src ? gt_strdup(src) : NULL;
}

/* An int/float/bool as a number; None is none of those. */
static int gt_number_view(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python int(str): whitespace, sign, decimal digits with '_' between digits. */
static int gt_str_to_int(const char *s, long long *out) {
    const char *p = s;
    long long acc = 0;
    int neg = 0, digits = 0, prev_digit = 0;
    if (!s) return 0;
    while (*p && strchr(" \t\n\r\v\f", *p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (acc > 900000000000000000LL) return 0;
            acc = acc * 10 + (*p - '0');
            digits++;
            prev_digit = 1;
            p++;
        } else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
            prev_digit = 0;
            p++;
        } else {
            break;
        }
    }
    while (*p && strchr(" \t\n\r\v\f", *p)) p++;
    if (*p || !digits) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

/* int(v): truncation toward zero, 0 when the conversion fails. */
static double gt_int_view(const json_t *v) {
    double d = 0.0;
    long long i = 0;
    if (!v) return 0.0;
    if (gt_number_view(v, &d)) {
        if (!(d == d)) return 0.0;                    /* NaN */
        if (d > 9.0e18 || d < -9.0e18) return 0.0;    /* inf / overflow */
        return (double)(long long)d;
    }
    if (v->type == JSON_STRING && gt_str_to_int(v->str_val, &i)) return (double)i;
    return 0.0;
}

/* int(event.get("index") or 0) */
static int gt_index_of(const json_t *v) {
    if (!gt_truthy(v)) return 0;
    return (int)gt_int_view(v);
}

/* _count: int(usage.get(key) or 0), 0 on any conversion failure. */
static double gt_count(const json_t *usage, const char *key) {
    const json_t *v;
    if (!usage || usage->type != JSON_OBJECT) return 0.0;
    v = json_obj_get(usage, key);
    if (!gt_truthy(v)) return 0.0;
    return gt_int_view(v);
}

/* sep.join(...) over an array of strings. */
static char *gt_join(const json_t *arr, const char *sep) {
    size_t total = 1, i, pos = 0, seplen = strlen(sep);
    char *out;
    for (i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        total += (v && v->type == JSON_STRING && v->str_val) ? strlen(v->str_val) : 0;
        if (i) total += seplen;
    }
    out = (char *)malloc(total);
    if (!out) return NULL;
    for (i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (i && seplen) { memcpy(out + pos, sep, seplen); pos += seplen; }
        if (v && v->type == JSON_STRING && v->str_val) {
            size_t n = strlen(v->str_val);
            memcpy(out + pos, v->str_val, n);
            pos += n;
        }
    }
    out[pos] = '\0';
    return out;
}

/* ── Python-compatible JSON dump (json.dumps defaults) ─────────────────────── */

/* Python's json.dumps writes ", " between items and ": " after keys; the wire
 * strings this port hands back (tool-call arguments) are compared verbatim. */
typedef struct {
    char *p;
    size_t n, cap;
} gt_buf;

static void gt_buf_put(gt_buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < b->n + n + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void gt_buf_str(gt_buf *b, const char *s) {
    if (!s) s = "";
    gt_buf_put(b, s, strlen(s));
}

static void gt_dump_number(gt_buf *b, double v) {
    char buf[64];
    int prec;
    if (v == (double)(long long)v && v < 9.0e18 && v > -9.0e18) {
        snprintf(buf, sizeof buf, "%.0f", v);
        gt_buf_str(b, buf);
        return;
    }
    for (prec = 17; prec >= 1; prec--) {
        snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (strtod(buf, NULL) == v) break;
    }
    gt_buf_str(b, buf);
}

static void gt_dump(gt_buf *b, const json_t *v) {
    size_t i;
    if (!v) { gt_buf_str(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   gt_buf_str(b, "null"); return;
    case JSON_BOOL:   gt_buf_str(b, v->bool_val ? "true" : "false"); return;
    case JSON_NUMBER: gt_dump_number(b, v->num_val); return;
    case JSON_STRING: {
        char *e = json_escape_string(v->str_val);
        gt_buf_str(b, e ? e : "\"\"");
        free(e);
        return;
    }
    case JSON_ARRAY:
        gt_buf_str(b, "[");
        for (i = 0; i < json_len(v); i++) {
            if (i) gt_buf_str(b, ", ");
            gt_dump(b, json_get(v, i));
        }
        gt_buf_str(b, "]");
        return;
    case JSON_OBJECT:
        gt_buf_str(b, "{");
        for (i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            char *e = json_escape_string(k);
            if (i) gt_buf_str(b, ", ");
            gt_buf_str(b, e ? e : "\"\"");
            free(e);
            gt_buf_str(b, ": ");
            gt_dump(b, json_object_get_at(v, i));
        }
        gt_buf_str(b, "}");
        return;
    }
}

/* json.dumps(v, ensure_ascii=False); caller frees. */
static char *gt_pyjson_dumps(const json_t *v) {
    gt_buf b;
    memset(&b, 0, sizeof b);
    gt_dump(&b, v);
    if (!b.p) return gt_strdup("null");
    return b.p;
}

/* ── OpenAI content -> Interactions blocks ─────────────────────────────────── */

/* _text_of: a plain str, or a {"type": "text", "text": <str>} item. */
static char *gt_text_of(const json_t *item) {
    if (!item) return NULL;
    if (item->type == JSON_STRING) return gt_strdup(item->str_val ? item->str_val : "");
    if (item->type == JSON_OBJECT) {
        const json_t *type = json_obj_get(item, "type");
        const json_t *text = json_obj_get(item, "text");
        if (type && type->type == JSON_STRING && type->str_val && !strcmp(type->str_val, "text") &&
            text && text->type == JSON_STRING)
            return gt_strdup(text->str_val ? text->str_val : "");
    }
    return NULL;
}

/* _coerce_content_to_text */
static char *gt_coerce(const json_t *content) {
    json_t *parts;
    char *out;
    size_t i;
    if (!content || content->type == JSON_NULL) return gt_strdup("");
    if (content->type == JSON_ARRAY) {
        parts = json_array();
        for (i = 0; i < json_len(content); i++) {
            char *t = gt_text_of(json_get(content, i));
            if (t) json_append(parts, json_string(t));
            free(t);
        }
        out = gt_join(parts, "\n");
        json_free(parts);
        return out;
    }
    return gt_pystr(content);
}

static int gt_b64val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64.b64decode(validate=False): stray bytes ignored, padding rules kept. */
static unsigned char *gt_b64decode(const char *s, size_t *out_len) {
    size_t n = 0, pad = 0, i, bytes, oi = 0;
    char *clean;
    unsigned char *out;
    int seen_pad = 0;
    unsigned acc = 0, bits = 0;
    if (!s) return NULL;
    clean = (char *)malloc(strlen(s) + 1);
    if (!clean) return NULL;
    for (i = 0; s[i]; i++) {
        if (s[i] == '=') { seen_pad = 1; pad++; continue; }
        if (gt_b64val((unsigned char)s[i]) < 0) continue;
        if (seen_pad) { free(clean); return NULL; }
        clean[n++] = s[i];
    }
    if (pad > 2 || n % 4 == 1 || (n + pad) % 4 != 0) { free(clean); return NULL; }
    bytes = n * 3 / 4;
    out = (unsigned char *)malloc(bytes ? bytes : 1);
    if (!out) { free(clean); return NULL; }
    for (i = 0; i < n; i++) {
        acc = (acc << 6) | (unsigned)gt_b64val((unsigned char)clean[i]);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (oi < bytes) out[oi++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    free(clean);
    *out_len = oi;
    return out;
}

static char *gt_b64encode(const unsigned char *d, size_t len) {
    static const char *A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t outlen = 4 * ((len + 2) / 3), i, oi = 0;
    char *o = (char *)malloc(outlen + 1);
    if (!o) return NULL;
    for (i = 0; i < len; i += 3) {
        unsigned v = (unsigned)d[i] << 16;
        if (i + 1 < len) v |= (unsigned)d[i + 1] << 8;
        if (i + 2 < len) v |= (unsigned)d[i + 2];
        o[oi++] = A[(v >> 18) & 63];
        o[oi++] = A[(v >> 12) & 63];
        o[oi++] = (i + 1 < len) ? A[(v >> 6) & 63] : '=';
        o[oi++] = (i + 2 < len) ? A[v & 63] : '=';
    }
    o[oi] = '\0';
    return o;
}

static int gt_image_mime_ok(const char *mime) {
    static const char *const OK[] = {
        "image/png", "image/jpeg", "image/webp", "image/heic",
        "image/heif", "image/gif", "image/bmp", "image/tiff",
    };
    size_t i;
    for (i = 0; i < sizeof OK / sizeof OK[0]; i++)
        if (!strcmp(mime, OK[i])) return 1;
    return 0;
}

/* _image_content */
static json_t *gt_image_impl(const json_t *item) {
    const json_t *url_obj, *u;
    const char *url, *comma, *colon, *semi, *mstart;
    size_t mlen;
    char *mime, *tmp, *data;
    unsigned char *raw;
    size_t rawlen = 0;
    json_t *out;

    if (!item || item->type != JSON_OBJECT) return NULL;
    {
        const json_t *type = json_obj_get(item, "type");
        if (!type || type->type != JSON_STRING || !type->str_val || strcmp(type->str_val, "image_url"))
            return NULL;
    }
    url_obj = json_obj_get(item, "image_url");
    u = (url_obj && url_obj->type == JSON_OBJECT) ? json_obj_get(url_obj, "url") : NULL;
    if (!u || u->type != JSON_STRING || !u->str_val) return NULL;
    url = u->str_val;
    if (strncmp(url, "data:", 5) != 0) return NULL;
    comma = strchr(url, ',');
    if (!comma) return NULL;
    colon = (const char *)memchr(url, ':', (size_t)(comma - url));
    if (!colon) return NULL;
    mstart = colon + 1;
    semi = (const char *)memchr(mstart, ';', (size_t)(comma - mstart));
    mlen = semi ? (size_t)(semi - mstart) : (size_t)(comma - mstart);
    tmp = (char *)malloc(mlen + 1);
    if (!tmp) return NULL;
    memcpy(tmp, mstart, mlen);
    tmp[mlen] = '\0';
    mime = gt_trim(tmp);
    free(tmp);
    if (!mime) return NULL;
    gt_lower_in_place(mime);

    raw = gt_b64decode(comma + 1, &rawlen);
    if (!raw) { free(mime); return NULL; }
    data = gt_b64encode(raw, rawlen);
    free(raw);
    if (!gt_image_mime_ok(mime)) { free(mime); free(data); return NULL; }
    out = json_object();
    json_set(out, "type", json_string("image"));
    json_set(out, "mime_type", json_string(mime));
    json_set(out, "data", json_string(data ? data : ""));
    free(mime);
    free(data);
    return out;
}

/* _content_blocks */
static json_t *gt_blocks(const json_t *content) {
    json_t *out = json_array();
    size_t i;
    if (!content || content->type != JSON_ARRAY) {
        char *text = gt_coerce(content);
        if (text && text[0]) {
            json_t *b = json_object();
            json_set(b, "type", json_string("text"));
            json_set(b, "text", json_string(text));
            json_append(out, b);
        }
        free(text);
        return out;
    }
    for (i = 0; i < json_len(content); i++) {
        const json_t *item = json_get(content, i);
        char *text = gt_text_of(item);
        if (text && text[0]) {
            json_t *b = json_object();
            json_set(b, "type", json_string("text"));
            json_set(b, "text", json_string(text));
            json_append(out, b);
        } else if (item && item->type == JSON_OBJECT) {
            json_t *img = gt_image_impl(item);
            if (img) json_append(out, img);
        }
        free(text);
    }
    return out;
}

/* _looks_like_json_schema: any same-document $ref pointer in the value. */
static int gt_looks_schema(const json_t *node) {
    size_t i;
    if (!node) return 0;
    if (node->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(node); i++) {
            const char *k = json_object_get_key_at(node, i);
            const json_t *v = json_object_get_at(node, i);
            if (k && !strcmp(k, "$ref") && v && v->type == JSON_STRING && v->str_val &&
                !strncmp(v->str_val, "#/", 2))
                return 1;
            if (gt_looks_schema(v)) return 1;
        }
        return 0;
    }
    if (node->type == JSON_ARRAY) {
        for (i = 0; i < json_len(node); i++)
            if (gt_looks_schema(json_get(node, i))) return 1;
        return 0;
    }
    return 0;
}

/* _call_arguments */
static json_t *gt_call_arguments(const json_t *tool_call) {
    const json_t *fn = (tool_call && tool_call->type == JSON_OBJECT)
                           ? json_obj_get(tool_call, "function") : NULL;
    const json_t *raw = (fn && fn->type == JSON_OBJECT) ? json_obj_get(fn, "arguments") : NULL;
    json_t *args = NULL;
    if (raw && raw->type == JSON_STRING && raw->str_val && raw->str_val[0]) {
        args = json_parse(raw->str_val, NULL);
        if (!args) {
            json_t *o = json_object();
            json_set(o, "_raw", json_string(raw->str_val));
            return o;
        }
    } else if (raw && raw->type == JSON_OBJECT) {
        args = json_copy(raw);
    } else {
        args = json_object();
    }
    if (args->type == JSON_OBJECT) return args;
    {
        json_t *o = json_object();
        json_set(o, "_value", args);
        return o;
    }
}

/* _tool_call_extra_signature: replayed Gemini thoughtSignature. */
static char *gt_tool_sig(const json_t *tool_call) {
    const json_t *extra, *sig = NULL;
    if (!tool_call || tool_call->type != JSON_OBJECT) return NULL;
    extra = json_obj_get(tool_call, "extra_content");
    if (extra && extra->type == JSON_OBJECT) {
        sig = json_obj_get(extra, "google");
        if (!sig) sig = json_obj_get(extra, "thought_signature");
    }
    if (sig && sig->type == JSON_OBJECT) {
        const json_t *inner = json_obj_get(sig, "thought_signature");
        if (!inner) inner = json_obj_get(sig, "thoughtSignature");
        sig = inner;
    }
    if (sig && sig->type == JSON_STRING && sig->str_val && sig->str_val[0])
        return gt_strdup(sig->str_val);
    return NULL;
}

/* _tool_call_id */
static char *gt_tool_call_id(const json_t *tool_call) {
    const json_t *id;
    if (!tool_call || tool_call->type != JSON_OBJECT) return gt_strdup("");
    id = json_obj_get(tool_call, "id");
    if (!gt_truthy(id)) id = json_obj_get(tool_call, "call_id");
    if (!gt_truthy(id)) return gt_strdup("");
    return gt_pystr(id);
}

/* _thought_text */
static char *gt_thought_text(const json_t *step) {
    json_t *parts = json_array();
    const json_t *summary;
    char *out;
    size_t i;
    if (!step || step->type != JSON_OBJECT) {
        json_free(parts);
        return gt_strdup("");
    }
    summary = json_obj_get(step, "summary");
    if (summary && summary->type == JSON_ARRAY) {
        for (i = 0; i < json_len(summary); i++) {
            const json_t *c = json_get(summary, i);
            const json_t *type, *text;
            if (!c || c->type != JSON_OBJECT) continue;
            type = json_obj_get(c, "type");
            if (type && (!(type->type == JSON_STRING) || !type->str_val || strcmp(type->str_val, "text")))
                continue;
            text = json_obj_get(c, "text");
            if (text && text->type == JSON_STRING)
                json_append(parts, json_string(text->str_val ? text->str_val : ""));
        }
    }
    out = gt_join(parts, "");
    json_free(parts);
    return out;
}

/* ── parameter schema preparation (prepare_gemini_tool_parameters) ─────────── */

static json_t *gt_empty_object_schema(void) {
    json_t *o = json_object();
    json_set(o, "type", json_string("object"));
    json_set(o, "properties", json_object());
    return o;
}

static const json_t *gt_resolve_ref(const json_t *root, const char *ref) {
    const json_t *node = root;
    const char *p;
    if (!ref || ref[0] != '#' || ref[1] != '/') return NULL;
    p = ref + 2;
    for (;;) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        char *part = (char *)malloc(len + 1);
        size_t i, w = 0;
        if (!part) return NULL;
        for (i = 0; i < len; i++) {
            if (p[i] == '~' && i + 1 < len) {
                if (p[i + 1] == '1') { part[w++] = '/'; i++; continue; }
                if (p[i + 1] == '0') { part[w++] = '~'; i++; continue; }
            }
            part[w++] = p[i];
        }
        part[w] = '\0';
        if (!node || node->type != JSON_OBJECT) { free(part); return NULL; }
        node = json_obj_get(node, part);
        free(part);
        if (!node) return NULL;
        if (!slash) break;
        p = slash + 1;
    }
    return node->type == JSON_OBJECT ? node : NULL;
}

static json_t *gt_inline_refs(const json_t *node, const json_t *root, int *budget,
                              const char **stack, int depth, int *failed) {
    size_t i;
    if (*failed || !node) return NULL;
    if (node->type == JSON_ARRAY) {
        json_t *arr = json_array();
        for (i = 0; i < json_len(node); i++) {
            json_t *v = gt_inline_refs(json_get(node, i), root, budget, stack, depth, failed);
            if (*failed) { json_free(arr); return NULL; }
            json_append(arr, v);
        }
        return arr;
    }
    if (node->type != JSON_OBJECT) return json_copy(node);
    {
        const json_t *refn = json_obj_get(node, "$ref");
        if (!refn || refn->type != JSON_STRING) {
            json_t *o = json_object();
            for (i = 0; i < json_object_size(node); i++) {
                const char *k = json_object_get_key_at(node, i);
                json_t *v = gt_inline_refs(json_object_get_at(node, i), root, budget, stack, depth, failed);
                if (*failed) { json_free(o); return NULL; }
                if (k) json_set(o, k, v);
            }
            return o;
        }
        {
            const char *ref = refn->str_val ? refn->str_val : "";
            const json_t *target;
            json_t *inlined;
            int sibling = 0;
            int d;
            for (d = 0; d < depth; d++)
                if (stack[d] && !strcmp(stack[d], ref)) { *failed = 1; return NULL; }
            (*budget)--;
            if (*budget < 0) { *failed = 1; return NULL; }
            target = gt_resolve_ref(root, ref);
            if (!target) { *failed = 1; return NULL; }
            if (depth >= 256) { *failed = 1; return NULL; }
            stack[depth] = ref;
            inlined = gt_inline_refs(target, root, budget, stack, depth + 1, failed);
            if (*failed) return NULL;
            for (i = 0; i < json_object_size(node); i++) {
                const char *k = json_object_get_key_at(node, i);
                if (k && strcmp(k, "$ref")) { sibling = 1; break; }
            }
            if (!sibling) return inlined;
            if (inlined->type != JSON_OBJECT) { json_free(inlined); *failed = 1; return NULL; }
            for (i = 0; i < json_object_size(node); i++) {
                const char *k = json_object_get_key_at(node, i);
                json_t *v;
                if (!k || !strcmp(k, "$ref")) continue;
                v = gt_inline_refs(json_object_get_at(node, i), root, budget, stack, depth, failed);
                if (*failed) { json_free(inlined); return NULL; }
                json_set(inlined, k, v);
            }
            return inlined;
        }
    }
}

static json_t *gt_prepare_params(const json_t *params) {
    json_t *schema, *inlined;
    int budget = 256, failed = 0;
    const char *stack[257];

    if (!params || params->type != JSON_OBJECT || json_object_size(params) == 0)
        return gt_empty_object_schema();
    schema = json_copy(params);
    if (!schema) return gt_empty_object_schema();
    json_obj_del(schema, "$schema");
    memset(stack, 0, sizeof stack);
    inlined = gt_inline_refs(schema, schema, &budget, stack, 0, &failed);
    if (failed || !inlined) {
        if (inlined) json_free(inlined);
    } else {
        json_free(schema);
        schema = inlined;
    }
    json_obj_del(schema, "$defs");
    json_obj_del(schema, "definitions");
    if (json_object_size(schema) == 0) {
        json_free(schema);
        return gt_empty_object_schema();
    }
    {
        const json_t *type = json_obj_get(schema, "type");
        if (type && type->type == JSON_STRING && type->str_val && !strcmp(type->str_val, "object") &&
            !json_has(schema, "properties"))
            json_set(schema, "properties", json_object());
    }
    return schema;
}

/* ── thinking config / output budget ───────────────────────────────────────── */

static const json_t *gt_get2(const json_t *obj, const char *camel, const char *snake) {
    const json_t *v = json_obj_get(obj, camel);
    if (!v) v = json_obj_get(obj, snake);
    return v;
}

/* _normalize_thinking_config */
static json_t *gt_thinking(const json_t *config) {
    json_t *out;
    const json_t *v;
    if (!config || config->type != JSON_OBJECT) return NULL;
    out = json_object();
    v = gt_get2(config, "thinkingBudget", "thinking_budget");
    {
        double d;
        if (gt_number_view(v, &d)) json_set(out, "thinkingBudget", json_number((double)(long long)d));
    }
    v = gt_get2(config, "includeThoughts", "include_thoughts");
    if (v && v->type == JSON_BOOL) json_set(out, "includeThoughts", json_bool(v->bool_val));
    v = gt_get2(config, "thinkingLevel", "thinking_level");
    if (v && v->type == JSON_STRING && v->str_val) {
        char *t = gt_trim(v->str_val);
        if (t && t[0]) {
            gt_lower_in_place(t);
            json_set(out, "thinkingLevel", json_string(t));
        }
        free(t);
    }
    if (json_object_size(out) == 0) { json_free(out); return NULL; }
    return out;
}

/* _thinking_requests_output_headroom */
static int gt_headroom(const json_t *config) {
    json_t *n = gt_thinking(config);
    const json_t *incl, *budget;
    int result;
    if (!n) return 0;
    incl = json_obj_get(n, "includeThoughts");
    if (incl && incl->type == JSON_BOOL && !incl->bool_val) { json_free(n); return 0; }
    budget = json_obj_get(n, "thinkingBudget");
    result = json_object_size(n) > 0;
    if (budget && budget->type == JSON_NUMBER && budget->num_val <= 0.0 &&
        !json_has(n, "thinkingLevel"))
        result = 0;
    json_free(n);
    return result;
}

#define GT_DEFAULT_MAX_OUTPUT_TOKENS 65535.0
#define GT_SINGLE_REQUEST_OUTPUT_CAP 65536.0

/* _effective_gemini_max_output_tokens */
static double gt_effective_max(const json_t *max_tokens, const json_t *thinking_config) {
    double requested = (max_tokens && max_tokens->type != JSON_NULL) ? gt_int_view(max_tokens) : 0.0;
    if (requested <= 0.0 || gt_headroom(thinking_config))
        return requested > GT_DEFAULT_MAX_OUTPUT_TOKENS ? requested : GT_DEFAULT_MAX_OUTPUT_TOKENS;
    return requested;
}

/* ── request side ──────────────────────────────────────────────────────────── */

static const char *gt_step_type(const json_t *step) {
    const json_t *t;
    if (!step || step->type != JSON_OBJECT) return NULL;
    t = json_obj_get(step, "type");
    return (t && t->type == JSON_STRING) ? t->str_val : NULL;
}

static void gt_thought_step(json_t *steps, json_t *replayed, const char *sig) {
    size_t i;
    json_t *step;
    if (!sig || !sig[0]) return;
    for (i = 0; i < json_len(replayed); i++) {
        const json_t *v = json_get(replayed, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, sig)) return;
    }
    json_append(replayed, json_string(sig));
    step = json_object();
    json_set(step, "type", json_string("thought"));
    json_set(step, "signature", json_string(sig));
    json_append(steps, step);
}

/* _assistant_steps: local step list for one assistant message. */
static json_t *gt_assistant_steps(const json_t *message, json_t *names) {
    json_t *steps = json_array();
    json_t *replayed = json_array();
    json_t *tool_calls = json_array();
    json_t *text_blocks = json_array();
    json_t *blocks;
    size_t i;

    blocks = gt_blocks(json_obj_get(message, "content"));
    for (i = 0; i < json_len(blocks); i++) {
        const json_t *b = json_get(blocks, i);
        const json_t *type = b ? json_obj_get(b, "type") : NULL;
        if (type && type->type == JSON_STRING && type->str_val && !strcmp(type->str_val, "text"))
            json_append(text_blocks, json_copy(b));
    }
    json_free(blocks);

    {
        const json_t *tcs = json_obj_get(message, "tool_calls");
        if (tcs && tcs->type == JSON_ARRAY) {
            for (i = 0; i < json_len(tcs); i++) {
                const json_t *tc = json_get(tcs, i);
                if (tc && tc->type == JSON_OBJECT) json_append(tool_calls, json_copy(tc));
            }
        }
    }

    if (json_len(text_blocks) > 0) {
        char *sig = gt_tool_sig(message);
        gt_thought_step(steps, replayed, sig);
        free(sig);
        {
            json_t *mo = json_object();
            json_set(mo, "type", json_string("model_output"));
            json_set(mo, "content", json_copy(text_blocks));
            json_append(steps, mo);
        }
    }
    for (i = 0; i < json_len(tool_calls); i++) {
        const json_t *tc = json_get(tool_calls, i);
        const json_t *fn = json_obj_get(tc, "function");
        const json_t *namev = (fn && fn->type == JSON_OBJECT) ? json_obj_get(fn, "name") : NULL;
        char *name = gt_pystr(namev);
        char *call_id = gt_tool_call_id(tc);
        char *sig = gt_tool_sig(tc);
        json_t *step;
        if (!call_id || !call_id[0]) { free(call_id); call_id = gt_strdup("call_000000000000"); }
        if (name && name[0]) json_set(names, call_id, json_string(name));
        gt_thought_step(steps, replayed, sig);
        free(sig);
        step = json_object();
        json_set(step, "type", json_string("function_call"));
        json_set(step, "id", json_string(call_id));
        json_set(step, "name", json_string(name ? name : ""));
        json_set(step, "arguments", gt_call_arguments(tc));
        json_append(steps, step);
        free(name);
        free(call_id);
    }
    if (json_len(text_blocks) == 0 && json_len(tool_calls) == 0) {
        char *sig = gt_tool_sig(message);
        gt_thought_step(steps, replayed, sig);
        free(sig);
    }
    /* A turn that calls tools must open with a thought step. */
    if (json_len(tool_calls) > 0) {
        const char *first = gt_step_type(json_get(steps, 0));
        if (!first || strcmp(first, "thought")) {
            json_t *skip = json_object();
            json_t *shifted = json_array();
            json_set(skip, "type", json_string("thought"));
            json_set(skip, "signature", json_string("skip_thought_signature_validator"));
            json_append(shifted, skip);
            for (i = 0; i < json_len(steps); i++)
                json_append(shifted, json_get(steps, i) ? json_copy(json_get(steps, i)) : json_null());
            json_free(steps);
            steps = shifted;
        }
    }
    json_free(text_blocks);
    json_free(tool_calls);
    json_free(replayed);
    return steps;
}

/* _function_result */
static json_t *gt_function_result(const json_t *message, const json_t *names) {
    char *call_id, *text;
    const json_t *raw, *namev;
    json_t *parsed = NULL, *images, *blocks, *step, *result;
    json_t *blank = NULL;
    size_t i;

    if (!message || message->type != JSON_OBJECT) {
        blank = json_object();
        message = blank;
    }
    call_id = gt_pystr_or_empty(json_obj_get(message, "tool_call_id"));
    raw = json_obj_get(message, "content");
    text = gt_coerce(raw);
    if (!text) text = gt_strdup("");
    {
        char *t = gt_trim(text);
        if (t && (t[0] == '{' || t[0] == '[')) parsed = json_parse(text, NULL);
        free(t);
    }
    images = json_array();
    if (raw && raw->type == JSON_ARRAY) {
        blocks = gt_blocks(raw);
        for (i = 0; i < json_len(blocks); i++) {
            const json_t *b = json_get(blocks, i);
            const json_t *type = b ? json_obj_get(b, "type") : NULL;
            if (type && type->type == JSON_STRING && type->str_val && !strcmp(type->str_val, "image"))
                json_append(images, json_copy(b));
        }
        json_free(blocks);
    }
    if (json_len(images) > 0) {
        result = json_array();
        if (text[0]) {
            json_t *tb = json_object();
            json_set(tb, "type", json_string("text"));
            json_set(tb, "text", json_string(text));
            json_append(result, tb);
        }
        for (i = 0; i < json_len(images); i++)
            json_append(result, json_copy(json_get(images, i)));
    } else if (parsed && parsed->type == JSON_OBJECT && !gt_looks_schema(parsed)) {
        result = json_copy(parsed);
    } else {
        result = json_string(text);
    }
    step = json_object();
    json_set(step, "type", json_string("function_result"));
    json_set(step, "call_id", json_string(call_id));
    json_set(step, "result", result);
    free(parsed);
    json_free(images);

    namev = (names && names->type == JSON_OBJECT) ? json_obj_get(names, call_id) : NULL;
    if (!gt_truthy(namev)) namev = json_obj_get(message, "name");
    if (gt_truthy(namev)) {
        char *n = gt_pystr(namev);
        json_set(step, "name", json_string(n ? n : ""));
        free(n);
    }
    free(call_id);
    free(text);
    if (blank) json_free(blank);
    return step;
}

/* build_input_steps */
json_t *w33_interactio_95c25f_p1_input_steps(const json_t *messages) {
    json_t *steps = json_array();
    json_t *system = json_array();
    json_t *names = json_object();
    json_t *out;
    char *joined = NULL;
    size_t i;

    if (messages && messages->type == JSON_ARRAY) {
        for (i = 0; i < json_len(messages); i++) {
            const json_t *message = json_get(messages, i);
            const json_t *rolev;
            const char *role;
            if (!message || message->type != JSON_OBJECT) continue;
            rolev = json_obj_get(message, "role");
            role = (rolev && rolev->type == JSON_STRING && rolev->str_val) ? rolev->str_val : "user";
            if (!strcmp(role, "system") || !strcmp(role, "developer")) {
                char *text = gt_coerce(json_obj_get(message, "content"));
                json_append(system, json_string(text ? text : ""));
                free(text);
            } else if (!strcmp(role, "tool") || !strcmp(role, "function")) {
                json_append(steps, gt_function_result(message, names));
            } else if (!strcmp(role, "assistant")) {
                json_t *as = gt_assistant_steps(message, names);
                size_t j;
                for (j = 0; j < json_len(as); j++)
                    json_append(steps, json_get(as, j) ? json_copy(json_get(as, j)) : json_null());
                json_free(as);
            } else {
                json_t *blocks = gt_blocks(json_obj_get(message, "content"));
                if (json_len(blocks) > 0) {
                    json_t *last = json_len(steps) ? json_get(steps, json_len(steps) - 1) : NULL;
                    const char *last_type = gt_step_type(last);
                    if (last && last_type && !strcmp(last_type, "user_input")) {
                        json_t *content = json_obj_get(last, "content");
                        size_t j;
                        for (j = 0; j < json_len(blocks); j++)
                            json_append(content, json_get(blocks, j) ? json_copy(json_get(blocks, j)) : json_null());
                    } else {
                        json_t *ui = json_object();
                        json_set(ui, "type", json_string("user_input"));
                        json_set(ui, "content", json_copy(blocks));
                        json_append(steps, ui);
                    }
                }
                json_free(blocks);
            }
        }
    }
    /* "\n".join(part for part in system if part).strip() */
    {
        json_t *nonempty = json_array();
        for (i = 0; i < json_len(system); i++) {
            const json_t *v = json_get(system, i);
            if (gt_truthy(v)) json_append(nonempty, json_copy(v));
        }
        {
            char *j = gt_join(nonempty, "\n");
            joined = gt_trim(j ? j : "");
            free(j);
        }
        json_free(nonempty);
    }
    out = json_object();
    json_set(out, "steps", steps);
    json_set(out, "system_instruction", (joined && joined[0]) ? json_string(joined) : json_null());
    free(joined);
    json_free(system);
    json_free(names);
    return out;
}

/* _tools */
static json_t *gt_tools(const json_t *tools) {
    json_t *out = json_array();
    size_t i;
    if (!tools || tools->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(tools); i++) {
        const json_t *tool = json_get(tools, i);
        const json_t *fn = (tool && tool->type == JSON_OBJECT) ? json_obj_get(tool, "function") : NULL;
        const json_t *name, *desc, *params;
        json_t *entry;
        if (!fn || fn->type != JSON_OBJECT) continue;
        name = json_obj_get(fn, "name");
        if (!name || name->type != JSON_STRING || !name->str_val || !name->str_val[0]) continue;
        entry = json_object();
        json_set(entry, "type", json_string("function"));
        json_set(entry, "name", json_string(name->str_val));
        desc = json_obj_get(fn, "description");
        if (desc && desc->type == JSON_STRING && desc->str_val && desc->str_val[0])
            json_set(entry, "description", json_string(desc->str_val));
        params = json_obj_get(fn, "parameters");
        if (params && params->type == JSON_OBJECT)
            json_set(entry, "parameters", gt_prepare_params(params));
        json_append(out, entry);
    }
    return out;
}

/* _tool_choice */
static json_t *gt_tool_choice(const json_t *tool_choice) {
    if (tool_choice && tool_choice->type == JSON_STRING && tool_choice->str_val) {
        const char *s = tool_choice->str_val;
        if (!strcmp(s, "auto")) return json_string("auto");
        if (!strcmp(s, "required")) return json_string("any");
        if (!strcmp(s, "none")) return json_string("none");
        return json_null();
    }
    if (tool_choice && tool_choice->type == JSON_OBJECT) {
        const json_t *fn = json_obj_get(tool_choice, "function");
        const json_t *name = (fn && fn->type == JSON_OBJECT) ? json_obj_get(fn, "name") : NULL;
        if (name && name->type == JSON_STRING && name->str_val && name->str_val[0]) {
            json_t *tools = json_array();
            json_t *allowed = json_object();
            json_t *out = json_object();
            json_append(tools, json_string(name->str_val));
            json_set(allowed, "mode", json_string("any"));
            json_set(allowed, "tools", tools);
            json_set(out, "allowed_tools", allowed);
            return out;
        }
    }
    return json_null();
}

/* _response_format */
static json_t *gt_response_format(const json_t *response_format) {
    const json_t *type, *spec, *schema;
    json_t *out;
    if (!response_format || response_format->type != JSON_OBJECT) return json_null();
    type = json_obj_get(response_format, "type");
    if (!type || type->type != JSON_STRING || !type->str_val) return json_null();
    if (strcmp(type->str_val, "json_object") && strcmp(type->str_val, "json_schema")) return json_null();
    spec = strcmp(type->str_val, "json_schema") ? NULL : json_obj_get(response_format, "json_schema");
    schema = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "schema") : NULL;
    out = json_object();
    json_set(out, "type", json_string("text"));
    json_set(out, "mime_type", json_string("application/json"));
    if (schema && schema->type == JSON_OBJECT)
        json_set(out, "schema", gt_prepare_params(schema));
    return out;
}

json_t *w33_interactio_95c25f_p1_image(const json_t *item) {
    json_t *out = gt_image_impl(item);
    return out ? out : json_null();
}

json_t *w33_interactio_95c25f_p1_content_blocks(const json_t *content) {
    return gt_blocks(content);
}

json_t *w33_interactio_95c25f_p1_function_result(const json_t *spec) {
    const json_t *message = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "message") : NULL;
    const json_t *names = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "names") : NULL;
    return gt_function_result(message, names);
}

json_t *w33_interactio_95c25f_p1_call_arguments(const json_t *tool_call) {
    return gt_call_arguments(tool_call);
}

json_t *w33_interactio_95c25f_p1_tools(const json_t *tools) {
    return gt_tools(tools);
}

json_t *w33_interactio_95c25f_p1_tool_choice(const json_t *tool_choice) {
    return gt_tool_choice(tool_choice);
}

json_t *w33_interactio_95c25f_p1_response_format(const json_t *response_format) {
    return gt_response_format(response_format);
}

json_t *w33_interactio_95c25f_p1_continuation(const json_t *spec) {
    json_t *out = json_object();
    const json_t *request, *iid, *tok;
    size_t i;
    if (!spec || spec->type != JSON_OBJECT) return out;
    request = json_obj_get(spec, "request");
    if (request && request->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(request); i++) {
            const char *k = json_object_get_key_at(request, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(request, i)));
        }
    }
    iid = json_obj_get(spec, "interaction_id");
    tok = json_obj_get(spec, "token");
    json_set(out, "previous_interaction_id", iid ? json_copy(iid) : json_null());
    json_set(out, "continuation_token", tok ? json_copy(tok) : json_null());
    return out;
}

/* build_interactions_request */
json_t *w33_interactio_95c25f_p1_request(const json_t *spec) {
    json_t *out, *generation, *steps_obj, *steps, *interaction_tools, *choice = NULL;
    json_t *thinking, *fmt, *system;
    const json_t *messages, *tools, *tool_choice, *thinking_config, *stop, *response_format;
    const json_t *tv, *pv, *mv;
    int suppress = 0;

    if (!spec || spec->type != JSON_OBJECT) spec = json_object();
    messages = json_obj_get(spec, "messages");
    tools = json_obj_get(spec, "tools");
    tool_choice = json_obj_get(spec, "tool_choice");
    thinking_config = json_obj_get(spec, "thinking_config");
    stop = json_obj_get(spec, "stop");
    response_format = json_obj_get(spec, "response_format");

    steps_obj = w33_interactio_95c25f_p1_input_steps(messages);
    steps = json_obj_get(steps_obj, "steps");
    system = json_obj_get(steps_obj, "system_instruction");

    thinking = gt_thinking(thinking_config);
    if (!thinking) thinking = json_object();

    generation = json_object();
    tv = json_obj_get(spec, "temperature");
    if (tv && tv->type != JSON_NULL) json_set(generation, "temperature", json_copy(tv));
    pv = json_obj_get(spec, "top_p");
    if (pv && pv->type != JSON_NULL) json_set(generation, "top_p", json_copy(pv));
    json_set(generation, "max_output_tokens",
             json_number(gt_effective_max(json_obj_get(spec, "max_tokens"), thinking_config)));
    if (gt_truthy(stop)) {
        if (stop->type == JSON_ARRAY) {
            json_set(generation, "stop_sequences", json_copy(stop));
        } else {
            char *s = gt_pystr(stop);
            json_t *seq = json_array();
            json_append(seq, json_string(s ? s : ""));
            json_set(generation, "stop_sequences", seq);
            free(s);
        }
    }
    {
        const json_t *level = json_obj_get(thinking, "thinkingLevel");
        if (level && level->type == JSON_STRING && level->str_val &&
            (!strcmp(level->str_val, "minimal") || !strcmp(level->str_val, "low") ||
             !strcmp(level->str_val, "medium") || !strcmp(level->str_val, "high") ||
             !strcmp(level->str_val, "xhigh")))
            json_set(generation, "thinking_level", json_string(level->str_val));
    }
    {
        const json_t *incl = json_obj_get(thinking, "includeThoughts");
        if (incl && incl->type == JSON_BOOL)
            json_set(generation, "thinking_summaries", json_string(incl->bool_val ? "auto" : "none"));
    }
    interaction_tools = gt_tools(tools);
    if (json_len(interaction_tools) > 0) {
        choice = gt_tool_choice(tool_choice);
        if (choice->type != JSON_NULL) {
            json_set(generation, "tool_choice", choice);
        } else {
            json_free(choice);
            choice = NULL;
        }
    }

    out = json_object();
    mv = json_obj_get(spec, "model");
    json_set(out, "model", mv ? json_copy(mv) : json_null());
    json_set(out, "input", json_copy(steps));
    json_set(out, "store", json_bool(false));
    if (system && system->type == JSON_STRING && system->str_val && system->str_val[0])
        json_set(out, "system_instruction", json_copy(system));
    if (json_len(interaction_tools) > 0) json_set(out, "tools", interaction_tools);
    else json_free(interaction_tools);

    fmt = gt_response_format(response_format);
    if (choice && choice->type == JSON_STRING && choice->str_val && !strcmp(choice->str_val, "any"))
        suppress = 1;
    if (choice && choice->type == JSON_OBJECT) suppress = 1;
    if (fmt && fmt->type != JSON_NULL && !suppress) json_set(out, "response_format", fmt);
    else if (fmt) json_free(fmt);
    json_set(out, "generation_config", generation);
    if (json_get_num(generation, "max_output_tokens", 0.0) > GT_SINGLE_REQUEST_OUTPUT_CAP)
        json_set(out, "store", json_bool(true));
    json_free(steps_obj);
    json_free(thinking);
    return out;
}

/* ── response side: usage / finish / errors ───────────────────────────────── */

/* _finish_reason */
static const char *gt_status_finish(const char *status) {
    if (!status) status = "";
    if (!strcmp(status, "incomplete") || !strcmp(status, "budget_exceeded") ||
        !strcmp(status, "requires_continuation"))
        return "length";
    return "stop"; /* completed, requires_action, cancelled, anything unknown */
}

static const char *gt_finish_reason(const char *status, int has_tools) {
    const char *m = gt_status_finish(status);
    return (has_tools && !strcmp(m, "stop")) ? "tool_calls" : m;
}

json_t *w33_interactio_95c25f_p2_finish_reason(const json_t *spec) {
    const json_t *status = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "status") : NULL;
    const json_t *tools = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "has_tool_calls") : NULL;
    const char *s = (status && status->type == JSON_STRING) ? status->str_val : "";
    return json_string(gt_finish_reason(s, gt_truthy(tools)));
}

/* usage_namespace */
json_t *w33_interactio_95c25f_p2_usage(const json_t *usage) {
    json_t *o = json_object(), *pd, *cd;
    double thoughts = gt_count(usage, "total_thought_tokens");
    json_set(o, "prompt_tokens", json_number(gt_count(usage, "total_input_tokens")));
    json_set(o, "completion_tokens", json_number(gt_count(usage, "total_output_tokens") + thoughts));
    json_set(o, "total_tokens", json_number(gt_count(usage, "total_tokens")));
    pd = json_object();
    json_set(pd, "cached_tokens", json_number(gt_count(usage, "total_cached_tokens")));
    json_set(o, "prompt_tokens_details", pd);
    cd = json_object();
    json_set(cd, "reasoning_tokens", json_number(thoughts));
    json_set(o, "completion_tokens_details", cd);
    if (usage && usage->type == JSON_OBJECT && json_has(usage, "initial_cached_tokens")) {
        json_set(o, "initial_prompt_tokens", json_number(gt_count(usage, "initial_input_tokens")));
        json_set(o, "initial_cached_tokens", json_number(gt_count(usage, "initial_cached_tokens")));
    }
    return o;
}

/* merge_usage */
json_t *w33_interactio_95c25f_p2_merge_usage(const json_t *usages) {
    static const char *const KEYS[6] = {
        "total_input_tokens", "total_cached_tokens", "total_output_tokens",
        "total_tool_use_tokens", "total_thought_tokens", "total_tokens",
    };
    json_t *o = json_object();
    size_t n = (usages && usages->type == JSON_ARRAY) ? json_len(usages) : 0;
    size_t k, i;
    for (k = 0; k < 6; k++) {
        int present = 0;
        double sum = 0.0;
        for (i = 0; i < n; i++) {
            const json_t *u = json_get(usages, i);
            if (u && u->type == JSON_OBJECT && json_has(u, KEYS[k])) {
                present = 1;
                sum += gt_count(u, KEYS[k]);
            }
        }
        if (present) json_set(o, KEYS[k], json_number(sum));
    }
    if (n > 0) {
        const json_t *first = json_get(usages, 0);
        json_set(o, "initial_input_tokens", json_number(gt_count(first, "total_input_tokens")));
        json_set(o, "initial_cached_tokens", json_number(gt_count(first, "total_cached_tokens")));
    }
    return o;
}

/* interaction_error -> the GeminiAPIError fields */
static json_t *gt_error_body(const json_t *errors, const char *fallback) {
    const json_t *first = NULL;
    char *rpc_raw, *msg_raw, *rpc, *msg, *up, *digits;
    json_t *out, *details;
    int status = 0, has_status = 0;
    size_t i;

    if (errors && errors->type == JSON_ARRAY) {
        for (i = 0; i < json_len(errors); i++) {
            const json_t *e = json_get(errors, i);
            if (e && e->type == JSON_OBJECT) { first = e; break; }
        }
    } else if (errors && errors->type == JSON_OBJECT) {
        first = errors;
    }
    rpc_raw = gt_pystr_or_empty(first ? json_obj_get(first, "code") : NULL);
    msg_raw = gt_pystr_or_empty(first ? json_obj_get(first, "message") : NULL);
    rpc = gt_trim(rpc_raw);
    msg = gt_trim(msg_raw);
    free(rpc_raw);
    free(msg_raw);
    if (!msg) msg = gt_strdup("");
    if (!msg[0]) { gt_replace_str(&msg, fallback ? fallback : "Gemini interaction failed"); }

    up = gt_strdup(rpc);
    gt_upper_in_place(up);
    {
        static const struct { const char *name; int code; } MAP[] = {
            {"INVALID_ARGUMENT", 400}, {"INVALID_REQUEST", 400}, {"FAILED_PRECONDITION", 400},
            {"OUT_OF_RANGE", 400}, {"UNAUTHENTICATED", 401}, {"PERMISSION_DENIED", 403},
            {"NOT_FOUND", 404}, {"RESOURCE_EXHAUSTED", 429}, {"TOO_MANY_REQUESTS", 429},
            {"CANCELLED", 499}, {"INTERNAL", 500}, {"UNKNOWN", 500}, {"UNAVAILABLE", 503},
            {"SERVICE_UNAVAILABLE", 503}, {"DEADLINE_EXCEEDED", 504},
        };
        size_t m;
        for (m = 0; m < sizeof MAP / sizeof MAP[0]; m++) {
            if (!strcmp(up, MAP[m].name)) { status = MAP[m].code; has_status = 1; break; }
        }
    }
    if (!has_status && rpc[0]) {
        int all_digits = 1;
        for (digits = rpc; *digits; digits++)
            if (!isdigit((unsigned char)*digits)) { all_digits = 0; break; }
        if (all_digits) {
            long long v = 0;
            if (gt_str_to_int(rpc, &v)) { status = (int)v; has_status = 1; }
        }
    }
    free(up);

    out = json_object();
    details = json_object();
    json_set(details, "status", json_string(rpc));
    json_set(details, "message", json_string(msg));
    {
        char text[1024];
        snprintf(text, sizeof text, "Gemini interaction error (%s): %s",
                 rpc[0] ? rpc : "error", msg);
        json_set(out, "message", json_string(text));
    }
    if (has_status && status == 401) json_set(out, "code", json_string("gemini_unauthorized"));
    else if (has_status && status == 429) json_set(out, "code", json_string("gemini_rate_limited"));
    else if (has_status && status == 404) json_set(out, "code", json_string("gemini_model_not_found"));
    else if (has_status && status != 0) {
        char code[64];
        snprintf(code, sizeof code, "gemini_http_%d", status);
        json_set(out, "code", json_string(code));
    } else json_set(out, "code", json_string("gemini_interaction_failed"));
    json_set(out, "status_code", has_status ? json_number(status) : json_null());
    json_set(out, "details", details);
    free(rpc);
    free(msg);
    return out;
}

json_t *w33_interactio_95c25f_p2_interaction_error(const json_t *spec) {
    const json_t *errors = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "errors") : NULL;
    const json_t *fb = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "fallback") : NULL;
    char *fallback = gt_pystr(fb);
    json_t *out;
    if (!fb) { free(fallback); fallback = gt_strdup("Gemini interaction failed"); }
    out = gt_error_body(errors, fallback);
    free(fallback);
    return out;
}

/* translate_interaction */
json_t *w33_interactio_95c25f_p2_translate(const json_t *spec) {
    const json_t *interactions = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "interactions") : NULL;
    const json_t *modelv = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "model") : NULL;
    const char *model = (modelv && modelv->type == JSON_STRING && modelv->str_val) ? modelv->str_val : "";
    size_t n = (interactions && interactions->type == JSON_ARRAY) ? json_len(interactions) : 0;
    const json_t *last = n ? json_get(interactions, n - 1) : NULL;
    json_t *text, *reasoning, *tool_calls, *usages, *out, *message, *usage_obj, *envelope;
    json_t *choices, *choice, *extra;
    char *signature = NULL, *text_signature = NULL;
    int any_text = 0;
    size_t i, j;

    if (last && last->type == JSON_OBJECT) {
        const json_t *st = json_obj_get(last, "status");
        const char *status = (st && st->type == JSON_STRING && st->str_val) ? st->str_val : "";
        if (!strcmp(status, "failed")) {
            out = json_object();
            json_set(out, "__raise__", gt_error_body(json_obj_get(last, "errors"), "Gemini interaction failed"));
            return out;
        }
    }

    text = json_array();
    reasoning = json_array();
    tool_calls = json_array();

    for (i = 0; i < n; i++) {
        const json_t *inter = json_get(interactions, i);
        const json_t *steps;
        if (!inter || inter->type != JSON_OBJECT) continue;
        steps = json_obj_get(inter, "steps");
        if (!steps || steps->type != JSON_ARRAY) continue;
        for (j = 0; j < json_len(steps); j++) {
            const json_t *step = json_get(steps, j);
            const char *kind = gt_step_type(step);
            if (!kind) continue;
            if (!strcmp(kind, "thought")) {
                char *t = gt_thought_text(step);
                json_append(reasoning, json_string(t ? t : ""));
                free(t);
                {
                    const json_t *sig = json_obj_get(step, "signature");
                    if (sig && sig->type == JSON_STRING && sig->str_val && sig->str_val[0])
                        gt_replace_str(&signature, sig->str_val);
                }
            } else if (!strcmp(kind, "model_output")) {
                const json_t *content = json_obj_get(step, "content");
                int piece_nonempty = 0;
                if (content && content->type == JSON_ARRAY) {
                    size_t k;
                    for (k = 0; k < json_len(content); k++) {
                        const json_t *c = json_get(content, k);
                        const json_t *ct, *cv;
                        if (!c || c->type != JSON_OBJECT) continue;
                        ct = json_obj_get(c, "type");
                        if (!ct || ct->type != JSON_STRING || !ct->str_val || strcmp(ct->str_val, "text"))
                            continue;
                        cv = json_obj_get(c, "text");
                        if (cv && cv->type == JSON_STRING && cv->str_val) {
                            json_append(text, json_string(cv->str_val));
                            if (cv->str_val[0]) piece_nonempty = 1;
                        } else {
                            json_append(text, json_string(""));
                        }
                    }
                }
                if (piece_nonempty) {
                    any_text = 1;
                    if (signature) gt_replace_str(&text_signature, signature);
                }
            } else if (!strcmp(kind, "function_call")) {
                const json_t *namev = json_obj_get(step, "name");
                char *name, *call_id, *args_json;
                json_t *tc, *fn, *tc_extra;
                if (!gt_truthy(namev)) continue;
                name = gt_pystr(namev);
                {
                    const json_t *idv = json_obj_get(step, "id");
                    call_id = gt_truthy(idv) ? gt_pystr(idv) : gt_strdup("call_000000000000");
                }
                {
                    const json_t *argv = json_obj_get(step, "arguments");
                    if (gt_truthy(argv)) args_json = gt_pyjson_dumps(argv);
                    else { json_t *e = json_object(); args_json = gt_pyjson_dumps(e); json_free(e); }
                }
                tc = json_object();
                json_set(tc, "id", json_string(call_id));
                json_set(tc, "type", json_string("function"));
                json_set(tc, "index", json_number((double)json_len(tool_calls)));
                fn = json_object();
                json_set(fn, "name", json_string(name));
                json_set(fn, "arguments", json_string(args_json ? args_json : "{}"));
                json_set(tc, "function", fn);
                if (signature && signature[0]) {
                    tc_extra = json_object();
                    json_set(tc_extra, "google", json_object());
                    json_set(json_obj_get(tc_extra, "google"), "thought_signature", json_string(signature));
                    json_set(tc, "extra_content", tc_extra);
                }
                json_append(tool_calls, tc);
                free(name);
                free(call_id);
                free(args_json);
            }
        }
    }

    usages = json_array();
    for (i = 0; i < n; i++) {
        const json_t *inter = json_get(interactions, i);
        const json_t *u = (inter && inter->type == JSON_OBJECT) ? json_obj_get(inter, "usage") : NULL;
        if (u && gt_truthy(u)) json_append(usages, json_copy(u));
        else json_append(usages, json_object());
    }
    if (n > 1) {
        json_t *merged = w33_interactio_95c25f_p2_merge_usage(usages);
        usage_obj = w33_interactio_95c25f_p2_usage(merged);
        json_free(merged);
    } else {
        usage_obj = w33_interactio_95c25f_p2_usage(json_len(usages) ? json_get(usages, 0) : NULL);
    }

    {
        const json_t *st = (last && last->type == JSON_OBJECT) ? json_obj_get(last, "status") : NULL;
        char *status = gt_pystr_or_empty(st);
        const char *finish = gt_finish_reason(status[0] ? status : "completed", json_len(tool_calls) > 0);
        char *content = gt_join(text, "");
        char *joined_reasoning = gt_join(reasoning, "");
        const char *msg_sig = any_text ? text_signature
                                       : (json_len(tool_calls) ? NULL : signature);
        message = json_object();
        json_set(message, "role", json_string("assistant"));
        json_set(message, "content", json_string(content ? content : ""));
        json_set(message, "tool_calls", json_len(tool_calls) ? json_copy(tool_calls) : json_null());
        if (joined_reasoning && joined_reasoning[0])
            json_set(message, "reasoning", json_string(joined_reasoning));
        else
            json_set(message, "reasoning", json_null());
        if (joined_reasoning && joined_reasoning[0])
            json_set(message, "reasoning_content", json_string(joined_reasoning));
        else
            json_set(message, "reasoning_content", json_null());
        json_set(message, "reasoning_details", json_null());
        extra = NULL;
        if (msg_sig && msg_sig[0]) {
            json_t *google = json_object();
            json_set(google, "thought_signature", json_string(msg_sig));
            extra = json_object();
            json_set(extra, "google", google);
        }
        json_set(message, "extra_content", extra ? extra : json_null());

        choice = json_object();
        json_set(choice, "index", json_number(0));
        json_set(choice, "message", message);
        json_set(choice, "finish_reason", json_string(finish));
        choices = json_array();
        json_append(choices, choice);
        envelope = json_object();
        json_set(envelope, "id", json_string("chatcmpl-000000000000"));
        json_set(envelope, "object", json_string("chat.completion"));
        json_set(envelope, "created", json_number(0));
        json_set(envelope, "model", json_string(model));
        json_set(envelope, "choices", choices);
        json_set(envelope, "usage", usage_obj);
        free(content);
        free(joined_reasoning);
        free(status);
    }
    json_free(text);
    json_free(reasoning);
    json_free(tool_calls);
    json_free(usages);
    free(signature);
    free(text_signature);
    return envelope;
}

/* ── response side: streaming ──────────────────────────────────────────────── */

typedef struct {
    json_t *steps;              /* step index (decimal string) -> entry object */
    int tool_calls;
    char *signature;
    char *output_signature;
    char *interaction_id;
    char *status;
    char *continuation_token;
    json_t *hop_usage;          /* array of usage objects */
    int offset;
    int hop_done;
    int completed_hops;
    json_t *checkpoint_usage;   /* NULL until a checkpoint sets it */
} gt_stream;

typedef struct {
    gt_stream st;
    json_t *raise_body;
} gt_sctx;

static json_t *gt_extra_from_part(const char *sig) {
    json_t *google, *o;
    if (!sig || !sig[0]) return NULL;
    google = json_object();
    json_set(google, "thought_signature", json_string(sig));
    o = json_object();
    json_set(o, "google", google);
    return o;
}

/* _make_stream_chunk (takes ownership of content / tool_calls) */
static json_t *gt_chunk(const char *model, json_t *content, const char *reasoning,
                        json_t *tool_calls, const char *finish, const char *text_signature) {
    json_t *delta = json_object(), *choice = json_object(), *choices = json_array(), *env = json_object();
    json_set(delta, "role", json_string("assistant"));
    json_set(delta, "content", content ? content : json_null());
    json_set(delta, "tool_calls", tool_calls ? tool_calls : json_null());
    json_set(delta, "reasoning", (reasoning && reasoning[0]) ? json_string(reasoning) : json_null());
    json_set(delta, "reasoning_content", (reasoning && reasoning[0]) ? json_string(reasoning) : json_null());
    {
        json_t *extra = gt_extra_from_part(text_signature);
        json_set(delta, "extra_content", extra ? extra : json_null());
    }
    json_set(choice, "index", json_number(0));
    json_set(choice, "delta", delta);
    json_set(choice, "finish_reason", finish ? json_string(finish) : json_null());
    json_append(choices, choice);
    json_set(env, "id", json_string("chatcmpl-000000000000"));
    json_set(env, "object", json_string("chat.completion.chunk"));
    json_set(env, "created", json_number(0));
    json_set(env, "model", json_string(model ? model : ""));
    json_set(env, "choices", choices);
    json_set(env, "usage", json_null());
    return env;
}

/* _tool_delta */
static json_t *gt_tool_delta(const char *model, const json_t *entry, const char *arguments,
                             int first, const char *sig) {
    json_t *ns = json_object(), *fn = json_object(), *tcs = json_array();
    const char *id = json_get_str(entry, "id", "");
    const char *name = first ? json_get_str(entry, "name", "") : "";
    const json_t *idxv = json_obj_get(entry, "index");
    json_set(ns, "id", json_string((id && id[0]) ? id : "call_000000000000"));
    json_set(ns, "type", json_string("function"));
    json_set(ns, "index", idxv ? json_copy(idxv) : json_number(0));
    json_set(fn, "name", json_string(name ? name : ""));
    json_set(fn, "arguments", json_string(arguments ? arguments : ""));
    json_set(ns, "function", fn);
    if (first) {
        json_t *e = gt_extra_from_part(sig);
        if (e) json_set(ns, "extra_content", e);
    }
    json_append(tcs, ns);
    return gt_chunk(model, NULL, NULL, tcs, NULL, NULL);
}

static char *gt_step_key(int idx) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d", idx);
    return gt_strdup(buf);
}

static json_t *gt_entry_at(gt_stream *s, int index) {
    char *k = gt_step_key(s->offset + index);
    json_t *e = json_obj_get(s->steps, k);
    free(k);
    return e;
}

static const char *gt_token(const json_t *source) {
    const json_t *tok;
    if (!source || source->type != JSON_OBJECT) return NULL;
    tok = json_obj_get(source, "continuation_token");
    if (tok && tok->type == JSON_STRING && tok->str_val && tok->str_val[0]) return tok->str_val;
    return NULL;
}

/* _output_signature */
static void gt_output_signature(const char *model, gt_sctx *c, json_t *out) {
    if (!c->st.signature || !c->st.signature[0]) return;
    if (c->st.output_signature && !strcmp(c->st.output_signature, c->st.signature)) return;
    gt_replace_str(&c->st.output_signature, c->st.signature);
    json_append(out, gt_chunk(model, NULL, NULL, NULL, NULL, c->st.signature));
}

/* _start_thought */
static void gt_start_thought(const json_t *step, const char *model, gt_sctx *c, json_t *out) {
    const json_t *sig = json_obj_get(step, "signature");
    char *text;
    if (sig && sig->type == JSON_STRING && sig->str_val && sig->str_val[0])
        gt_replace_str(&c->st.signature, sig->str_val);
    text = gt_thought_text(step);
    if (text && text[0]) json_append(out, gt_chunk(model, NULL, text, NULL, NULL, NULL));
    free(text);
}

/* _start_output */
static void gt_start_output(const json_t *step, const char *model, gt_sctx *c, json_t *out) {
    const json_t *content = json_obj_get(step, "content");
    size_t i;
    gt_output_signature(model, c, out);
    if (!content || content->type != JSON_ARRAY) return;
    for (i = 0; i < json_len(content); i++) {
        const json_t *b = json_get(content, i);
        const json_t *type, *text;
        if (!b || b->type != JSON_OBJECT) continue;
        type = json_obj_get(b, "type");
        if (!type || type->type != JSON_STRING || !type->str_val || strcmp(type->str_val, "text")) continue;
        text = json_obj_get(b, "text");
        if (text && text->type == JSON_STRING && text->str_val && text->str_val[0])
            json_append(out, gt_chunk(model, json_string(text->str_val), NULL, NULL, NULL, NULL));
    }
}

/* _start_call */
static void gt_start_call(const json_t *step, json_t *entry, const char *model, gt_sctx *c, json_t *out) {
    const json_t *idv = json_obj_get(step, "id");
    const json_t *namev = json_obj_get(step, "name");
    const json_t *args = json_obj_get(step, "arguments");
    char *id = gt_truthy(idv) ? gt_pystr(idv) : gt_strdup("call_000000000000");
    char *name = gt_pystr_or_empty(namev);
    char *initial;
    if (args && args->type == JSON_OBJECT && json_object_size(args) > 0) {
        initial = gt_pyjson_dumps(args);
    } else {
        initial = gt_strdup("");
    }
    json_set(entry, "index", json_number((double)c->st.tool_calls));
    json_set(entry, "id", json_string(id));
    json_set(entry, "name", json_string(name));
    c->st.tool_calls++;
    json_set(entry, "sent", json_bool(initial && initial[0]));
    json_append(out, gt_tool_delta(model, entry, initial, 1, c->st.signature));
    free(id);
    free(name);
    free(initial);
}

/* _on_step_start */
static void gt_on_step_start(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    const json_t *step = json_obj_get(event, "step");
    const json_t *typev;
    json_t *entry;
    char *key;
    int idx;
    if (!step || step->type != JSON_OBJECT) step = NULL;
    idx = c->st.offset + gt_index_of(json_obj_get(event, "index"));
    key = gt_step_key(idx);
    entry = json_object();
    typev = step ? json_obj_get(step, "type") : NULL;
    json_set(entry, "type", typev ? json_copy(typev) : json_null());
    json_set(c->st.steps, key, entry);
    free(key);
    if (!typev || typev->type != JSON_STRING || !typev->str_val) return;
    if (!strcmp(typev->str_val, "thought")) gt_start_thought(step, model, c, out);
    else if (!strcmp(typev->str_val, "model_output")) gt_start_output(step, model, c, out);
    else if (!strcmp(typev->str_val, "function_call")) gt_start_call(step, entry, model, c, out);
}

static const char *gt_entry_type(const json_t *entry) {
    const json_t *t = entry ? json_obj_get(entry, "type") : NULL;
    return (t && t->type == JSON_STRING) ? t->str_val : NULL;
}

/* _delta_text */
static void gt_delta_text(const json_t *delta, const json_t *entry, const char *model,
                          gt_sctx *c, json_t *out) {
    const json_t *text = json_obj_get(delta, "text");
    const char *etype = gt_entry_type(entry);
    (void)c;
    if (!gt_truthy(text)) return;
    if (etype && !strcmp(etype, "thought"))
        json_append(out, gt_chunk(model, NULL, gt_pystr(text), NULL, NULL, NULL));
    else
        json_append(out, gt_chunk(model, json_copy(text), NULL, NULL, NULL, NULL));
}

/* _delta_summary */
static void gt_delta_summary(const json_t *delta, const json_t *entry, const char *model,
                             gt_sctx *c, json_t *out) {
    const json_t *content = json_obj_get(delta, "content");
    const json_t *type, *text;
    (void)entry;
    (void)c;
    if (!content || content->type != JSON_OBJECT) return;
    type = json_obj_get(content, "type");
    if (type && (!(type->type == JSON_STRING) || !type->str_val || strcmp(type->str_val, "text"))) return;
    text = json_obj_get(content, "text");
    if (!gt_truthy(text) || text->type != JSON_STRING) return;
    json_append(out, gt_chunk(model, NULL, text->str_val, NULL, NULL, NULL));
}

/* _delta_signature */
static void gt_delta_signature(const json_t *delta, const json_t *entry, const char *model,
                               gt_sctx *c, json_t *out) {
    const json_t *sig = json_obj_get(delta, "signature");
    const char *etype = gt_entry_type(entry);
    if (!sig || sig->type != JSON_STRING || !sig->str_val || !sig->str_val[0]) return;
    gt_replace_str(&c->st.signature, sig->str_val);
    if (etype && !strcmp(etype, "model_output")) gt_output_signature(model, c, out);
}

/* _delta_arguments */
static void gt_delta_arguments(const json_t *delta, json_t *entry, const char *model,
                               gt_sctx *c, json_t *out) {
    const json_t *args = json_obj_get(delta, "arguments");
    const char *etype = gt_entry_type(entry);
    char *s;
    (void)c;
    if (!etype || strcmp(etype, "function_call")) return;
    if (!gt_truthy(args)) return;
    if (entry) json_set(entry, "sent", json_bool(true));
    s = gt_pystr(args);
    json_append(out, gt_tool_delta(model, entry, s, 0, NULL));
    free(s);
}

/* _on_step_delta */
static void gt_on_step_delta(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    json_t *entry = gt_entry_at(&c->st, gt_index_of(json_obj_get(event, "index")));
    const json_t *delta = json_obj_get(event, "delta");
    const char *dtype;
    if (!delta || delta->type != JSON_OBJECT) return;
    dtype = json_get_str(delta, "type", "");
    if (!dtype || !dtype[0]) return;
    if (!strcmp(dtype, "text")) gt_delta_text(delta, entry, model, c, out);
    else if (!strcmp(dtype, "thought_summary")) gt_delta_summary(delta, entry, model, c, out);
    else if (!strcmp(dtype, "thought_signature")) gt_delta_signature(delta, entry, model, c, out);
    else if (!strcmp(dtype, "arguments_delta")) gt_delta_arguments(delta, entry, model, c, out);
}

/* _on_step_stop */
static void gt_on_step_stop(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    json_t *entry = gt_entry_at(&c->st, gt_index_of(json_obj_get(event, "index")));
    const char *etype = gt_entry_type(entry);
    const json_t *sent = entry ? json_obj_get(entry, "sent") : NULL;
    if (!etype || strcmp(etype, "function_call") || gt_truthy(sent)) return;
    json_set(entry, "sent", json_bool(true));
    json_append(out, gt_tool_delta(model, entry, "{}", 0, NULL));
}

/* _on_status */
static void gt_on_status(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    const json_t *st = json_obj_get(event, "status");
    const json_t *iid = json_obj_get(event, "interaction_id");
    const char *tok;
    (void)model;
    (void)out;
    if (gt_truthy(st)) {
        char *s = gt_pystr(st);
        gt_replace_str(&c->st.status, s);
        free(s);
    }
    if (gt_truthy(iid)) {
        char *s = gt_pystr(iid);
        gt_replace_str(&c->st.interaction_id, s);
        free(s);
    }
    tok = gt_token(event);
    if (tok) {
        const json_t *usage;
        gt_replace_str(&c->st.continuation_token, tok);
        usage = json_obj_get(event, "usage");
        if (c->st.checkpoint_usage) { json_free(c->st.checkpoint_usage); c->st.checkpoint_usage = NULL; }
        c->st.checkpoint_usage = (usage && usage->type == JSON_OBJECT) ? json_copy(usage) : json_object();
    }
}

/* _on_completed */
static void gt_on_completed(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    const json_t *inter = json_obj_get(event, "interaction");
    const json_t *idv, *statusv, *usage;
    const char *tok;
    char *status;
    if (!inter || inter->type != JSON_OBJECT) inter = NULL;
    idv = inter ? json_obj_get(inter, "id") : NULL;
    if (gt_truthy(idv)) {
        char *s = gt_pystr(idv);
        gt_replace_str(&c->st.interaction_id, s);
        free(s);
    }
    statusv = inter ? json_obj_get(inter, "status") : NULL;
    if (gt_truthy(statusv)) status = gt_pystr(statusv);
    else if (c->st.status && c->st.status[0]) status = gt_strdup(c->st.status);
    else status = gt_strdup("completed");
    gt_replace_str(&c->st.status, status);
    free(status);
    tok = gt_token(inter);
    if (tok) gt_replace_str(&c->st.continuation_token, tok);
    if (c->st.status && !strcmp(c->st.status, "failed")) {
        json_t *body = gt_error_body(inter ? json_obj_get(inter, "errors") : NULL,
                                     "Gemini interaction failed");
        json_free(c->raise_body);
        c->raise_body = body;
        return;
    }
    c->st.hop_done = 1;
    c->st.completed_hops++;
    usage = inter ? json_obj_get(inter, "usage") : NULL;
    json_append(c->st.hop_usage, (usage && gt_truthy(usage)) ? json_copy(usage) : json_object());
    if (c->st.status && !strcmp(c->st.status, "requires_continuation")) return;
    {
        json_t *u = json_len(c->st.hop_usage) > 1
                        ? w33_interactio_95c25f_p2_merge_usage(c->st.hop_usage)
                        : json_copy(json_get(c->st.hop_usage, 0));
        json_t *chunk = gt_chunk(model, NULL, NULL, NULL,
                                 gt_finish_reason(c->st.status, c->st.tool_calls > 0), NULL);
        json_set(chunk, "usage", w33_interactio_95c25f_p2_usage(u));
        json_append(out, chunk);
        json_free(u);
    }
}

/* _on_created */
static void gt_on_created(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    const json_t *inter = json_obj_get(event, "interaction");
    const json_t *idv = (inter && inter->type == JSON_OBJECT) ? json_obj_get(inter, "id") : NULL;
    (void)model;
    (void)out;
    if (gt_truthy(idv)) {
        char *s = gt_pystr(idv);
        gt_replace_str(&c->st.interaction_id, s);
        free(s);
    }
}

/* _on_error */
static void gt_on_error(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    (void)model;
    (void)out;
    json_free(c->raise_body);
    c->raise_body = gt_error_body(json_obj_get(event, "error"), "Gemini stream error");
}

/* translate_stream_event */
static void gt_translate_stream_event(const json_t *event, const char *model, gt_sctx *c, json_t *out) {
    const char *etype = (event && event->type == JSON_OBJECT) ? json_get_str(event, "event_type", "") : "";
    if (!etype || !etype[0]) return;
    if (!strcmp(etype, "error")) gt_on_error(event, model, c, out);
    else if (!strcmp(etype, "interaction.created")) gt_on_created(event, model, c, out);
    else if (!strcmp(etype, "step.start")) gt_on_step_start(event, model, c, out);
    else if (!strcmp(etype, "step.delta")) gt_on_step_delta(event, model, c, out);
    else if (!strcmp(etype, "step.stop")) gt_on_step_stop(event, model, c, out);
    else if (!strcmp(etype, "interaction.status_update")) gt_on_status(event, model, c, out);
    else if (!strcmp(etype, "interaction.completed")) gt_on_completed(event, model, c, out);
}

/* StreamState.next_hop */
static void gt_next_hop(gt_stream *s) {
    size_t i;
    int max = -1;
    for (i = 0; i < json_object_size(s->steps); i++) {
        const char *k = json_object_get_key_at(s->steps, i);
        long long v;
        if (k && gt_str_to_int(k, &v)) {
            if ((int)v > max) max = (int)v;
        }
    }
    s->offset = max + 1;
    gt_replace_str(&s->status, NULL);
    gt_replace_str(&s->continuation_token, NULL);
    if (s->checkpoint_usage) { json_free(s->checkpoint_usage); s->checkpoint_usage = NULL; }
    s->hop_done = 0;
}

/* StreamState.close_dropped_hop */
static void gt_close_dropped_hop(gt_stream *s) {
    if (!s->hop_done && s->checkpoint_usage) {
        json_append(s->hop_usage, json_copy(s->checkpoint_usage));
        json_free(s->checkpoint_usage);
        s->checkpoint_usage = NULL;
    }
}

json_t *w33_interactio_95c25f_p2_event_token(const json_t *event) {
    const char *tok = gt_token(event);
    const json_t *inter;
    if (tok) return json_string(tok);
    inter = (event && event->type == JSON_OBJECT) ? json_obj_get(event, "interaction") : NULL;
    tok = gt_token((inter && inter->type == JSON_OBJECT) ? inter : NULL);
    return tok ? json_string(tok) : json_null();
}

json_t *w33_interactio_95c25f_p2_finish_ceiling(const json_t *spec) {
    const json_t *modelv = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "model") : NULL;
    const json_t *hops = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "hop_usage") : NULL;
    const char *model = (modelv && modelv->type == JSON_STRING && modelv->str_val) ? modelv->str_val : "";
    json_t *usages = json_array();
    json_t *merged, *chunk;
    size_t i;
    if (hops && hops->type == JSON_ARRAY) {
        for (i = 0; i < json_len(hops); i++) {
            const json_t *u = json_get(hops, i);
            if (u && u->type == JSON_OBJECT) json_append(usages, json_copy(u));
        }
    }
    merged = w33_interactio_95c25f_p2_merge_usage(usages);
    chunk = gt_chunk(model, NULL, NULL, NULL, "length", NULL);
    json_set(chunk, "usage", w33_interactio_95c25f_p2_usage(merged));
    json_free(merged);
    json_free(usages);
    return chunk;
}

json_t *w33_interactio_95c25f_p2_iter_events(const json_t *lines) {
    json_t *out = json_array();
    size_t i;
    if (!lines || lines->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(lines); i++) {
        const json_t *p = json_get(lines, i);
        if (p && p->type == JSON_OBJECT && gt_truthy(json_obj_get(p, "event_type")))
            json_append(out, json_copy(p));
    }
    return out;
}

/* translate_stream_event driver over a hop script */
json_t *w33_interactio_95c25f_p2_stream(const json_t *spec) {
    gt_sctx c;
    json_t *chunks = json_array(), *out;
    const json_t *modelv, *script;
    const char *model;
    size_t i;

    memset(&c, 0, sizeof c);
    c.st.steps = json_object();
    c.st.hop_usage = json_array();
    modelv = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "model") : NULL;
    model = (modelv && modelv->type == JSON_STRING && modelv->str_val) ? modelv->str_val : "";
    script = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "script") : NULL;

    if (script && script->type == JSON_ARRAY) {
        for (i = 0; i < json_len(script); i++) {
            const json_t *item = json_get(script, i);
            const json_t *opv, *ev;
            if (!item || item->type != JSON_OBJECT) continue;
            opv = json_obj_get(item, "op");
            if (opv && opv->type == JSON_STRING && opv->str_val) {
                if (!strcmp(opv->str_val, "next_hop")) gt_next_hop(&c.st);
                else if (!strcmp(opv->str_val, "close_dropped_hop")) gt_close_dropped_hop(&c.st);
                else if (!strcmp(opv->str_val, "finish_ceiling")) {
                    json_t *u = w33_interactio_95c25f_p2_merge_usage(c.st.hop_usage);
                    json_t *chunk = gt_chunk(model, NULL, NULL, NULL, "length", NULL);
                    json_set(chunk, "usage", w33_interactio_95c25f_p2_usage(u));
                    json_append(chunks, chunk);
                    json_free(u);
                }
            } else {
                ev = json_obj_get(item, "event");
                if (ev && ev->type == JSON_OBJECT) {
                    gt_translate_stream_event(ev, model, &c, chunks);
                } else {
                    json_t *blank = json_object();
                    gt_translate_stream_event(blank, model, &c, chunks);
                    json_free(blank);
                }
            }
            if (c.raise_body) break;
        }
    }

    if (c.raise_body) {
        json_free(chunks);
        out = json_object();
        json_set(out, "__raise__", c.raise_body);
    } else {
        json_t *state = json_object();
        out = json_object();
        json_set(state, "interaction_id", c.st.interaction_id ? json_string(c.st.interaction_id) : json_null());
        json_set(state, "status", c.st.status ? json_string(c.st.status) : json_null());
        json_set(state, "continuation_token",
                 c.st.continuation_token ? json_string(c.st.continuation_token) : json_null());
        json_set(state, "tool_calls", json_number((double)c.st.tool_calls));
        json_set(state, "offset", json_number((double)c.st.offset));
        json_set(state, "hop_done", json_bool(c.st.hop_done != 0));
        json_set(state, "completed_hops", json_number((double)c.st.completed_hops));
        json_set(state, "hop_usage", json_copy(c.st.hop_usage));
        json_set(state, "checkpoint_usage",
                 c.st.checkpoint_usage ? json_copy(c.st.checkpoint_usage) : json_null());
        json_set(out, "chunks", chunks);
        json_set(out, "state", state);
    }
    free(c.st.signature);
    free(c.st.output_signature);
    free(c.st.interaction_id);
    free(c.st.status);
    free(c.st.continuation_token);
    json_free(c.st.steps);
    json_free(c.st.hop_usage);
    if (c.st.checkpoint_usage) json_free(c.st.checkpoint_usage);
    return out;
}
