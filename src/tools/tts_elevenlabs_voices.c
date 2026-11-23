/*
 * tts_elevenlabs_voices.c - C home for upstream Hermes tools/tts_elevenlabs_voices.py.
 * PoP: w15_ttselevenl_3eca90_clip @ tools/tts_elevenlabs_voices.py:_clip
 * PoP: w15_ttselevenl_3eca90_voice_row @ tools/tts_elevenlabs_voices.py:_voice_row
 * PoP: w15_ttselevenl_3eca90_request_params @ tools/tts_elevenlabs_voices.py:_request_params
 * PoP: w15_ttselevenl_3eca90_next_page @ tools/tts_elevenlabs_voices.py:_next_page
 *
 * Pure shaping helpers for the ElevenLabs voice-search page. HTTP, keys and error
 * envelopes stay in Python; only deterministic text clipping, row projection,
 * request building and cursor arithmetic are mirrored here.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_ttselevenl_3eca90.h"

static const char *const DEFAULT_ELEVENLABS_BASE_URL = "https://api.elevenlabs.io";
static const char *const LIBRARY_SORTS[] = {"usage_character_count_1y", "trending", "cloned_by_count", "created_date"};
static const size_t DESCRIPTION_MAX_CHARS = 240;

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL)   return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}
static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}
typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 1 >= b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; char *np = (char *)realloc(b->p, nc); if (!np) return; b->p = np; b->cap = nc; }
    b->p[b->n++] = c;
}
static void bputs(buf_t *b, const char *s) { for (const char *q = s; *q; q++) bput(b, *q); }
static char *bdone(buf_t *b) { if (!b->p) b->p = (char *)calloc(1, 1); if (b->n) b->p[b->n] = 0; return b->p; }

/* UTF-8 code-point helpers (Python len/slice count characters, not bytes). */
static size_t u8step(const unsigned char *p) {
    unsigned char c = *p;
    if (c >= 0xF0) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}
static size_t u8_count(const char *s) {
    size_t n = 0; const unsigned char *p = (const unsigned char *)s;
    while (*p) { p += u8step(p); n++; }
    return n;
}
static char *u8_head(const char *s, size_t keep) {
    const unsigned char *p = (const unsigned char *)s; size_t i = 0;
    while (*p && i < keep) { p += u8step(p); i++; }
    size_t n = (size_t)(p - (const unsigned char *)s);
    char *out = (char *)malloc(n + 1); memcpy(out, s, n); out[n] = 0; return out;
}

/* Python repr of a str: quote choice + minimal escapes (used for {page!r}). */
static char *py_repr_str(const char *s) {
    int has_s = strchr(s, '\'') != NULL, has_d = strchr(s, '"') != NULL;
    char q = (has_s && !has_d) ? '"' : '\'';
    buf_t b = {0}; bput(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\' || c == (unsigned char)q) { bput(&b, '\\'); bput(&b, (char)c); }
        else if (c == '\n') { bput(&b, '\\'); bput(&b, 'n'); }
        else if (c == '\r') { bput(&b, '\\'); bput(&b, 'r'); }
        else if (c == '\t') { bput(&b, '\\'); bput(&b, 't'); }
        else if (c < 0x20 || c == 0x7f) { static const char hx[] = "0123456789abcdef"; bput(&b, '\\'); bput(&b, 'x'); bput(&b, hx[c >> 4]); bput(&b, hx[c & 15]); }
        else bput(&b, (char)c);
    }
    bput(&b, q); return bdone(&b);
}
static json_t *raise_value_error(const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string("ValueError"));
    json_set(r, "msg", json_string(msg));
    return r;
}

struct json_t *w15_ttselevenl_3eca90_clip(const struct json_t *text) {
    char *raw = py_str(text);
    /* " ".join(str(text or "").split()) */
    buf_t b = {0};
    const char *p = raw; int first = 1;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (!first) bput(&b, ' ');
        for (const char *q = start; q < p; q++) bput(&b, *q);
        first = 0;
    }
    char *collapsed = bdone(&b);
    json_t *out;
    if (u8_count(collapsed) <= DESCRIPTION_MAX_CHARS) {
        out = json_string(collapsed);
    } else {
        char *head = u8_head(collapsed, DESCRIPTION_MAX_CHARS - 1);
        buf_t b2 = {0}; bputs(&b2, head); bputs(&b2, "\xe2\x80\xa6"); /* U+2026 */
        char *clipped = bdone(&b2);
        out = json_string(clipped);
        free(clipped); free(head);
    }
    free(collapsed); free(raw);
    return out;
}

struct json_t *w15_ttselevenl_3eca90_voice_row(const struct json_t *voice) {
    int is_obj = voice && voice->type == JSON_OBJECT;
    json_t *r = json_object();
    const json_t *vid = is_obj ? json_obj_get(voice, "voice_id") : NULL;
    const json_t *nm = is_obj ? json_obj_get(voice, "name") : NULL;
    json_set(r, "voice_id", vid ? json_copy(vid) : json_null());
    json_set(r, "name", nm ? json_copy(nm) : json_null());
    const json_t *labels = is_obj ? json_obj_get(voice, "labels") : NULL;
    int has_labels = labels && labels->type == JSON_OBJECT;
    static const char *const KEYS[] = {"gender", "age", "accent", "language", "descriptive", "use_case"};
    for (size_t i = 0; i < 6; i++) {
        const char *k = KEYS[i];
        const json_t *value = is_obj ? json_obj_get(voice, k) : NULL;
        if (!jtruthy(value) && has_labels) {
            value = json_obj_get(labels, k);
            if (!jtruthy(value)) {
                char spaced[64]; size_t n = 0;
                for (const char *q = k; *q && n + 1 < sizeof(spaced); q++) spaced[n++] = (*q == '_') ? ' ' : *q;
                spaced[n] = 0;
                value = json_obj_get(labels, spaced);
            }
        }
        if (jtruthy(value)) json_set(r, k, json_copy(value));
    }
    const json_t *cat = is_obj ? json_obj_get(voice, "category") : NULL;
    if (jtruthy(cat)) json_set(r, "category", json_copy(cat));
    const json_t *desc = is_obj ? json_obj_get(voice, "description") : NULL;
    if (jtruthy(desc)) {
        json_t *clipped = w15_ttselevenl_3eca90_clip(desc);
        json_set(r, "description", clipped);
    }
    const json_t *rate = is_obj ? json_obj_get(voice, "rate") : NULL;
    int excluded = (!rate || rate->type == JSON_NULL
                    || (rate->type == JSON_NUMBER && rate->num_val == 1.0)
                    || (rate->type == JSON_BOOL && rate->bool_val));
    if (!excluded) json_set(r, "credit_rate", json_copy(rate));
    const json_t *pv = is_obj ? json_obj_get(voice, "preview_url") : NULL;
    if (jtruthy(pv)) json_set(r, "preview_url", json_copy(pv));
    return r;
}

static int all_digits(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return 0;
    return 1;
}
static int is_library_sort(const char *s) {
    for (size_t i = 0; i < 4; i++) if (!strcmp(s, LIBRARY_SORTS[i])) return 1;
    return 0;
}

struct json_t *w15_ttselevenl_3eca90_request_params(const char *source, const char *query,
                                                   const struct json_t *filters, const char *use_case,
                                                   const char *sort, const char *page, long long page_size) {
    const char *src = source ? source : "";
    const char *q = query ? query : "";
    const char *uc = use_case ? use_case : "";
    const char *so = sort ? sort : "";
    const char *pg = page ? page : "";
    json_t *params = json_object();
    json_set(params, "page_size", json_number((double)page_size));
    if (filters && filters->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(filters); i++) {
            const char *k = json_object_get_key_at(filters, i);
            if (k) json_set(params, k, json_copy(json_object_get_at(filters, i)));
        }
    }
    if (q[0]) json_set(params, "search", json_string(q));
    json_t *pair = json_array();
    if (!strcmp(src, "account")) {
        json_set(params, "include_total_count", json_string("false"));
        if (pg[0]) json_set(params, "next_page_token", json_string(pg));
        json_append(pair, json_string("/v2/voices"));
        json_append(pair, params);
        return pair;
    }
    if (pg[0] && !all_digits(pg)) {
        char *pr = py_repr_str(pg);
        size_t n = strlen(pr) + 96;
        char *msg = (char *)malloc(n);
        snprintf(msg, n, "Invalid library page %s: pass next_page from the previous result.", pr);
        json_t *e = raise_value_error(msg);
        free(msg); free(pr); json_free(params); json_free(pair);
        return e;
    }
    if (so[0] && !is_library_sort(so)) {
        static const char *const MSG = "find_voices.sort must be one of usage_character_count_1y, trending, cloned_by_count, created_date";
        json_t *e = raise_value_error(MSG);
        json_free(params); json_free(pair);
        return e;
    }
    json_set(params, "sort", json_string(so[0] ? so : LIBRARY_SORTS[0]));
    if (uc[0]) {
        json_t *a = json_array(); json_append(a, json_string(uc));
        json_set(params, "use_cases", a);
    }
    long long pn = 0;
    if (pg[0]) pn = strtoll(pg, NULL, 10);
    json_set(params, "page", json_number((double)pn));
    json_append(pair, json_string("/v1/shared-voices"));
    json_append(pair, params);
    return pair;
}

struct json_t *w15_ttselevenl_3eca90_next_page(const char *source, const struct json_t *body, const char *page) {
    const json_t *hm = (body && body->type == JSON_OBJECT) ? json_obj_get(body, "has_more") : NULL;
    if (!jtruthy(hm)) return json_null();
    if (source && !strcmp(source, "account")) {
        const json_t *tok = json_obj_get(body, "next_page_token");
        if (jtruthy(tok)) return json_copy(tok);
        return json_null();
    }
    long long pn = 0;
    if (page && page[0]) pn = strtoll(page, NULL, 10);
    char b[48]; snprintf(b, sizeof(b), "%lld", pn + 1);
    return json_string(b);
}

