/*
 * stream_delivery.c — C port of upstream Hermes agent/stream_delivery.py.
 *
 * PoP: streamdeliv_normalize_interim_visible_text @ agent/stream_delivery.py:StreamDeliveryMixin._normalize_interim_visible_text
 * PoP: streamdeliv_interim_content_was_streamed @ agent/stream_delivery.py:StreamDeliveryMixin._interim_content_was_streamed
 * PoP: streamdeliv_interim_content_fully_streamed @ agent/stream_delivery.py:StreamDeliveryMixin._interim_content_fully_streamed
 * PoP: streamdeliv_codex_commentary_texts @ agent/stream_delivery.py:_codex_commentary_texts
 *
 * Think-strip is an explicit seam (identity in the oracle); the pure visible-text
 * normalization, prefix/exact streamed comparison and Codex commentary extraction
 * are reproduced faithfully here.
 */
#include "noc_w7_streamdeliv_9f8ea8.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* re.sub(r"\s+", " ", text).strip() for a string; "" for anything else. */
static char *normalize(const char *text) {
    size_t cap = text ? strlen(text) * 2 + 2 : 2;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t n = 0;
    const char *p = text ? text : "";
    int in_ws = 0;
    while (*p) {
        if (is_ws((unsigned char)*p)) { in_ws = 1; p++; continue; }
        if (in_ws && n) out[n++] = ' ';
        in_ws = 0;
        out[n++] = *p++;
    }
    out[n] = '\0';
    return out;
}

json_t *streamdeliv_normalize_interim_visible_text(const struct json_t *value) {
    if (!value || value->type != JSON_STRING) return json_string("");
    char *s = normalize(value->str_val ? value->str_val : "");
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

static int cmp_streamed(const char *content, const char *streamed, int exact) {
    char *v = normalize(content);
    char *s = normalize(streamed);
    int ok = 0;
    if (v && s) {
        if (exact) ok = v[0] && !strcmp(v, s);
        else ok = v[0] && s[0] && strncmp(v, s, strlen(s)) == 0;
    }
    free(v); free(s);
    return ok ? 1 : 0;
}

int streamdeliv_interim_content_was_streamed(const char *content, const char *streamed) {
    return cmp_streamed(content, streamed, 0);
}

int streamdeliv_interim_content_fully_streamed(const char *content, const char *streamed) {
    return cmp_streamed(content, streamed, 1);
}

static char *strip_ws(const char *s) {
    size_t a = 0, b = strlen(s);
    while (a < b && is_ws((unsigned char)s[a])) a++;
    while (b > a && is_ws((unsigned char)s[b - 1])) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}

json_t *streamdeliv_codex_commentary_texts(const struct json_t *assistant_msg) {
    json_t *out = json_array();
    json_t *items = json_obj_get(assistant_msg, "codex_message_items");
    if (!json_is_array(items)) return out;
    for (size_t i = 0; i < json_len(items); i++) {
        json_t *item = json_get(items, i);
        if (!json_is_object(item)) continue;
        const char *type = json_get_str(item, "type", "");
        if (strcmp(type, "message")) continue;
        const char *phase = json_get_str(item, "phase", "");
        json_t *parts = json_obj_get(item, "content");
        if (!json_is_array(parts)) continue;
        char *ph = strip_ws(phase);
        int is_commentary = 0;
        if (ph) {
            for (char *q = ph; *q; q++) *q = (char)tolower((unsigned char)*q);
            is_commentary = !strcmp(ph, "commentary");
        }
        free(ph);
        if (!is_commentary) continue;
        char *text = strip_ws("");  /* empty accumulator */
        size_t cap = 0, n = 0;
        free(text); text = NULL;
        for (size_t j = 0; j < json_len(parts); j++) {
            json_t *part = json_get(parts, j);
            if (!json_is_object(part)) continue;
            const char *ptype = json_get_str(part, "type", "");
            if (strcmp(ptype, "output_text")) continue;
            json_t *tv = json_obj_get(part, "text");
            if (!tv || tv->type != JSON_STRING || !tv->str_val) continue;
            size_t l = strlen(tv->str_val);
            if (n + l + 1 > cap) {
                size_t nc = (n + l + 1) * 2 + 16;
                char *tmp = (char *)realloc(text, nc);
                if (!tmp) break;
                text = tmp; cap = nc;
                if (n == 0) text[0] = '\0';
            }
            memcpy(text + n, tv->str_val, l);
            n += l;
            text[n] = '\0';
        }
        if (!text) { text = (char *)malloc(1); if (text) text[0] = '\0'; }
        char *trimmed = strip_ws(text);
        free(text);
        if (trimmed && trimmed[0]) json_append(out, json_string(trimmed));
        free(trimmed);
    }
    return out;
}
