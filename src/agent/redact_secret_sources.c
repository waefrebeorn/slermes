/*
 * redact_secret_sources.c — faithful C11 port of agent/redact_secret_sources.py.
 * Gate w14_redactsecr_02f95a.
 *
 * PoP: w14_redactsecr_02f95a_pem_texts @ agent/redact_secret_sources.py:_pem_texts
 * PoP: w14_redactsecr_02f95a_multiline_fragments @ agent/redact_secret_sources.py:_multiline_fragments
 * PoP: w14_redactsecr_02f95a_redact_exact @ agent/redact_secret_sources.py:redact_secret_source_values
 *
 * No global registry is kept: the per-home applied values are an explicit seam.
 */
#include "noc_w14_redactsecr_02f95a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define REDACTED "«redacted:secret-source»"
#define PEM_ARMOR "-----BEGIN "
#define MIN_CHARS 8
#define SECRET_LINE_MIN 16

static int contains(const char *s, const char *sub) {
    return s && sub && strstr(s, sub) != NULL;
}

/* Python str.replace(literal, real) for the two-character escape "\\n". */
static char *replace_lit(const char *s, const char *lit, const char *real) {
    size_t ln = strlen(lit), rn = strlen(real);
    size_t cap = (strlen(s) + 1) * (rn > ln ? rn : 1) + 1;
    char *out = (char *)malloc(cap), *d = out;
    const char *p = s;
    const char *q;
    while ((q = strstr(p, lit)) != NULL) {
        size_t k = (size_t)(q - p);
        memcpy(d, p, k); d += k;
        memcpy(d, real, rn); d += rn;
        p = q + ln;
    }
    strcpy(d, p);
    return out;
}

/* Python str.strip() for ASCII input (fixtures are ASCII). */
static char *strip_copy(const char *s) {
    const char *p = s ? s : "";
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    char *out = (char *)malloc((size_t)(e - p) + 1);
    if (out) { memcpy(out, p, (size_t)(e - p)); out[e - p] = '\0'; }
    return out;
}

/* Fullmatch of [A-Za-z0-9+/=]{16,}. */
static int is_secret_line(const char *s) {
    size_t n = strlen(s);
    if (n < SECRET_LINE_MIN) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=')) return 0;
    }
    return 1;
}

struct json_t *w14_redactsecr_02f95a_pem_texts(const char *value) {
    json_t *out = json_array();
    if (!value) return out;
    int armored = contains(value, PEM_ARMOR);
    if (armored) json_append(out, json_string(value));
    if (!armored || !contains(value, "\\n")) return out;

    json_t *root = json_parse(value, NULL);
    if (!root) root = json_string(replace_lit(value, "\\n", "\n"));  /* fallback keeps invalid JSON */
    json_t **stack = NULL;
    size_t n = 0, cap = 0;
    #define PUSH(x) do { if (n == cap) { cap = cap ? cap * 2 : 8; \
        stack = (json_t **)realloc(stack, cap * sizeof(json_t *)); } stack[n++] = (x); } while (0)
    PUSH(root);
    while (n) {
        json_t *item = stack[--n];
        if (item->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(item); i++)
                PUSH(json_object_get_at(item, i));
        } else if (item->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(item); i++) PUSH(json_get(item, i));
        } else if (item->type == JSON_STRING && contains(item->str_val, PEM_ARMOR)) {
            json_append(out, json_string(item->str_val ? item->str_val : ""));
            if (contains(item->str_val, "\\n")) {
                char *t1 = replace_lit(item->str_val, "\\r\\n", "\n");
                char *t2 = replace_lit(t1, "\\n", "\n");
                json_append(out, json_string(t2));
                free(t1); free(t2);
            }
        }
    }
    #undef PUSH
    free(stack);
    json_free(root);
    return out;
}

struct json_t *w14_redactsecr_02f95a_multiline_fragments(const char *value) {
    json_t *out = json_array();
    if (!value) return out;
    if (contains(value, "\n") || contains(value, "\r")) {
        size_t n = strlen(value);
        char *joined = (char *)malloc(n + 1);
        size_t j = 0;
        for (size_t i = 0; i < n; i++)
            if (value[i] != '\n' && value[i] != '\r') joined[j++] = value[i];
        joined[j] = '\0';
        if (j >= (size_t)(MIN_CHARS * 2)) json_append(out, json_string(joined));
        free(joined);
    }
    json_t *texts = w14_redactsecr_02f95a_pem_texts(value);
    for (size_t i = 0; i < json_len(texts); i++) {
        json_t *t = json_get(texts, i);
        const char *s = (t && t->type == JSON_STRING && t->str_val) ? t->str_val : "";
        const char *p = s;
        while (*p) {
            const char *e = p;
            while (*e && *e != '\n' && *e != '\r') e++;
            char *line = (char *)malloc((size_t)(e - p) + 1);
            memcpy(line, p, (size_t)(e - p)); line[e - p] = '\0';
            char *clean = strip_copy(line);
            if (is_secret_line(clean)) json_append(out, json_string(clean));
            free(line); free(clean);
            if (*e == '\r' && e[1] == '\n') e++;  /* CRLF is one boundary */
            p = (*e) ? e + 1 : e;
        }
    }
    json_free(texts);
    return out;
}

static char *buf_a = NULL, *buf_b = NULL;
static size_t cap_a = 0, cap_b = 0;
static void ensure(char **buf, size_t *cap, size_t need) {
    if (*cap >= need) return;
    size_t n = *cap ? *cap : 1024;
    while (n < need) n *= 2;
    *buf = (char *)realloc(*buf, n);
    *cap = n;
}

const char *w14_redactsecr_02f95a_redact_exact(const char *text, const struct json_t *values) {
    ensure(&buf_a, &cap_a, 1);
    strcpy(buf_a, text ? text : "");
    if (!values || values->type != JSON_ARRAY || !text || !text[0]) return buf_a;
    for (size_t i = 0; i < json_len(values); i++) {
        json_t *v = json_get(values, i);
        const char *needle = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
        if (!needle[0] || !strstr(buf_a, needle)) continue;
        size_t slen = strlen(buf_a), nlen = strlen(needle), rlen = strlen(REDACTED);
        ensure(&buf_b, &cap_b, slen * (rlen + 1) + 1);
        char *d = buf_b;
        const char *p = buf_a, *q;
        while ((q = strstr(p, needle)) != NULL) {
            size_t k = (size_t)(q - p);
            memcpy(d, p, k); d += k;
            memcpy(d, REDACTED, rlen); d += rlen;
            p = q + nlen;
        }
        strcpy(d, p);
        char *tmp = buf_a; buf_a = buf_b; buf_b = tmp;
        size_t tc = cap_a; cap_a = cap_b; cap_b = tc;
    }
    return buf_a;
}
