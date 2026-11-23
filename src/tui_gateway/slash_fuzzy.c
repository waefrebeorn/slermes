/*
 * slash_fuzzy.c — faithful C11 port of tui_gateway/slash_fuzzy.py.
 * Gate w10_slashfuzzy_f16515.
 *
 * PoP: w10_slashfuzzy_f16515_tokenize_search_text @ tui_gateway/slash_fuzzy.py:tokenize_search_text
 * PoP: w10_slashfuzzy_f16515_normalize_slash_search_query @ tui_gateway/slash_fuzzy.py:normalize_slash_search_query
 * PoP: w10_slashfuzzy_f16515_score_slash_completion_item @ tui_gateway/slash_fuzzy.py:score_slash_completion_item
 * PoP: w10_slashfuzzy_f16515_fuzzy_rank_slash_items @ tui_gateway/slash_fuzzy.py:fuzzy_rank_slash_items
 */
#include "hermes_json.h"
#include "noc_w10_slashfuzzy_f16515.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n && s) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int is_alnum_ascii(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

/* str() coercion of a JSON value (subset used by the Python source). */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupn("", 0);
    switch (v->type) {
        case JSON_NULL:   return dupn("None", 4);
        case JSON_STRING: return dupn(v->str_val ? v->str_val : "", v->str_val ? strlen(v->str_val) : 0);
        case JSON_BOOL:   return dupn(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            } else {
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            }
            return dupn(buf, strlen(buf));
        default: return dupn("", 0);
    }
}

/* Python str.strip() (whitespace) then str.lstrip("/") on an owned string. */
static char *strip_lslash(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    while (a < b && s[a] == '/') a++;
    return dupn(s + a, b - a);
}

/* Python str.strip() only. */
static char *py_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return dupn(s + a, b - a);
}

struct json_t *w10_slashfuzzy_f16515_tokenize_search_text(const char *value) {
    if (!value) value = "";
    size_t n = strlen(value);
    char *norm = (char *)malloc(n + 1);
    if (!norm) return NULL;
    for (size_t i = 0; i < n; i++) norm[i] = (char)tolower((unsigned char)value[i]);
    norm[n] = '\0';
    json_t *out = json_array();
    json_append(out, json_string(norm));
    size_t i = 0;
    while (i < n) {
        if (!is_alnum_ascii((unsigned char)norm[i])) { i++; continue; }
        size_t j = i;
        while (j < n && is_alnum_ascii((unsigned char)norm[j])) j++;
        char *tok = dupn(norm + i, j - i);
        json_append(out, json_string(tok));
        free(tok);
        i = j;
    }
    free(norm);
    return out;
}

char *w10_slashfuzzy_f16515_normalize_slash_search_query(const char *query) {
    if (!query) query = "";
    size_t n = strlen(query), a = 0, b = n;
    while (a < b && isspace((unsigned char)query[a])) a++;
    while (b > a && isspace((unsigned char)query[b - 1])) b--;
    while (a < b && query[a] == '/') a++;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    for (size_t i = a; i < b; i++) out[i - a] = (char)tolower((unsigned char)query[i]);
    out[b - a] = '\0';
    return out;
}

static int starts_with(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

/* _score_fields(fields, query, offset): first matching tier wins. */
static double score_fields(const json_t *fields, const char *q, int offset) {
    if (!q) q = "";
    size_t cnt = fields ? json_len(fields) : 0;
    for (int bump = 0; bump < 3; bump++) {
        int any = 0;
        for (size_t i = 0; i < cnt && !any; i++) {
            const char *f = json_string_value(json_get(fields, i));
            if (!f) continue;
            if (bump == 0) {
                if (!strcmp(f, q)) any = 1;
                else { char *sf = (char *)malloc(strlen(f) + 2); sf[0] = '/'; strcpy(sf + 1, f);
                       if (!strcmp(sf, q)) any = 1; free(sf); }
            } else if (bump == 1) {
                if (starts_with(f, q)) any = 1;
                else { char *sf = (char *)malloc(strlen(f) + 2); sf[0] = '/'; strcpy(sf + 1, f);
                       if (starts_with(sf, q)) any = 1; free(sf); }
            } else { /* bump == 2 */
                if (strstr(f, q) != NULL) any = 1;
            }
        }
        if (any) return (double)(offset + bump);
    }
    return INFINITY;
}

double w10_slashfuzzy_f16515_score_slash_completion_item(const json_t *item, const char *query) {
    char *raw = py_str(item ? json_obj_get(item, "text") : NULL);
    char *name = strip_lslash(raw);
    free(raw);
    json_t *f1 = w10_slashfuzzy_f16515_tokenize_search_text(name);
    free(name);
    char *meta = py_str(item ? json_obj_get(item, "meta") : NULL);
    json_t *f2 = w10_slashfuzzy_f16515_tokenize_search_text(meta);
    free(meta);
    double a = score_fields(f1, query, 0);
    double b = score_fields(f2, query, 3);
    json_free(f1);
    json_free(f2);
    return a < b ? a : b;
}

typedef struct { json_t *item; double score; } rank_ent_t;

struct json_t *w10_slashfuzzy_f16515_fuzzy_rank_slash_items(const json_t *items,
        const json_t *catalog, const char *query) {
    size_t ni = items ? json_len(items) : 0;
    size_t nc = catalog ? json_len(catalog) : 0;
    char **seen = (char **)calloc(ni ? ni : 1, sizeof(char *));
    size_t nseen = 0;
    for (size_t i = 0; i < ni; i++) {
        char *raw = py_str(json_obj_get(json_get(items, i), "text"));
        seen[nseen++] = py_strip(raw);
        free(raw);
    }
    json_t **merged = (json_t **)calloc(ni + nc + 1, sizeof(json_t *));
    size_t nm = 0;
    for (size_t i = 0; i < ni; i++) merged[nm++] = json_get(items, i);
    for (size_t j = 0; j < nc; j++) {
        json_t *it = json_get(catalog, j);
        char *raw = py_str(json_obj_get(it, "text"));
        char *s = py_strip(raw);
        free(raw);
        int in_seen = 0;
        for (size_t k = 0; k < nseen; k++) if (!strcmp(seen[k], s)) { in_seen = 1; break; }
        free(s);
        double sc = w10_slashfuzzy_f16515_score_slash_completion_item(it, query);
        if (!in_seen && !isinf(sc)) merged[nm++] = it;
    }
    rank_ent_t *ents = (rank_ent_t *)calloc(nm ? nm : 1, sizeof(rank_ent_t));
    size_t ne = 0;
    for (size_t k = 0; k < nm; k++) {
        double sc = w10_slashfuzzy_f16515_score_slash_completion_item(merged[k], query);
        if (!isinf(sc)) { ents[ne].item = merged[k]; ents[ne].score = sc; ne++; }
    }
    /* stable insertion sort by score ascending */
    for (size_t i = 1; i < ne; i++) {
        rank_ent_t key = ents[i];
        size_t j = i;
        while (j > 0 && ents[j - 1].score > key.score) { ents[j] = ents[j - 1]; j--; }
        ents[j] = key;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < ne; i++) json_append(out, json_copy(ents[i].item));
    for (size_t k = 0; k < nseen; k++) free(seen[k]);
    free(seen); free(merged); free(ents);
    return out;
}
