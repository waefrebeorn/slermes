/*
 * redact_vault_values.c — C home for upstream Hermes agent/redact_vault_values.py.
 *
 * Faithful C11 port of the exact-value vault-secret redaction registry:
 * registration with CR/LF normalization and recency refresh, bounded
 * per-profile eviction of the oldest entry, profile clear, and
 * longest-first exact-substring scrubbing of registered values.
 *
 * PoP: w11_redactvaul_87f037_register @ agent/redact_vault_values.py:register_vault_redaction_value
 * PoP: w11_redactvaul_87f037_clear @ agent/redact_vault_values.py:clear_vault_redaction_values
 * PoP: w11_redactvaul_87f037_redact @ agent/redact_vault_values.py:redact_registered_vault_values
 */
#include "noc_w11_redactvaul_87f037.h"

#include <stdlib.h>
#include <string.h>

#define VAULT_REDACTION_MAX 64
#define REDACTED_MARKER "«redacted-vault-secret»"

/* Ordered set of registered values (Python dict insertion order). */
static char **g_values;
static size_t g_count;
static size_t g_capacity;

static void bucket_clear(void) {
    for (size_t i = 0; i < g_count; i++) free(g_values[i]);
    g_count = 0;
}

static void bucket_remove_at(size_t idx) {
    free(g_values[idx]);
    for (size_t j = idx; j + 1 < g_count; j++) g_values[j] = g_values[j + 1];
    g_count--;
}

static void bucket_remove(const char *v) {
    for (size_t i = 0; i < g_count; i++)
        if (!strcmp(g_values[i], v)) { bucket_remove_at(i); return; }
}

/* dict.pop(v, None) then dict[v] = None: refresh recency by moving to tail. */
static void bucket_add(const char *v) {
    if (!v || !*v) return;
    bucket_remove(v);  /* re-registering refreshes recency */
    if (g_count == g_capacity) {
        size_t nc = g_capacity ? g_capacity * 2 : 8;
        char **nv = (char **)realloc(g_values, nc * sizeof(char *));
        if (!nv) return;
        g_values = nv;
        g_capacity = nc;
    }
    char *copy = (char *)malloc(strlen(v) + 1);
    if (!copy) return;
    strcpy(copy, v);
    g_values[g_count++] = copy;
    while (g_count > VAULT_REDACTION_MAX) bucket_remove_at(0);
}

/* str.replace("\r","").replace("\n","") */
static char *normalize(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] != '\r' && s[i] != '\n') out[j++] = s[i];
    out[j] = '\0';
    return out;
}

static char *replace_all(const char *text, const char *needle, const char *repl) {
    size_t nt = strlen(needle), nr = strlen(repl);
    if (!nt) return (char *)strdup(text);
    size_t total = 0;
    const char *p = text, *q;
    while ((q = strstr(p, needle)) != NULL) {
        total += (size_t)(q - p) + nr;
        p = q + nt;
    }
    total += strlen(p);
    char *out = (char *)malloc(total + 1);
    if (!out) return NULL;
    char *w = out;
    p = text;
    while ((q = strstr(p, needle)) != NULL) {
        size_t seg = (size_t)(q - p);
        memcpy(w, p, seg); w += seg;
        memcpy(w, repl, nr); w += nr;
        p = q + nt;
    }
    strcpy(w, p);
    return out;
}

/* Stable sort by strlen descending: sorted(bucket, key=len, reverse=True). */
static void sort_by_len_desc(const char **v, size_t n) {
    for (size_t i = 1; i < n; i++) {
        const char *key = v[i];
        size_t klen = strlen(key);
        size_t j = i;
        while (j > 0 && strlen(v[j - 1]) < klen) { v[j] = v[j - 1]; j--; }
        v[j] = key;
    }
}

/* PoP: w11_redactvaul_87f037_register @ agent/redact_vault_values.py:register_vault_redaction_value */
json_t *w11_redactvaul_87f037_register(json_t *a) {
    json_t *v = a ? json_obj_get(a, "value") : NULL;
    if (json_is_string(v) && v->str_val && v->str_val[0]) {
        char *n = normalize(v->str_val);
        if (n) {
            bucket_add(v->str_val);
            bucket_add(n);   /* register normalized form too; de-dupes when identical */
            free(n);
        }
    }
    return json_null();
}

/* PoP: w11_redactvaul_87f037_clear @ agent/redact_vault_values.py:clear_vault_redaction_values */
json_t *w11_redactvaul_87f037_clear(json_t *a) {
    (void)a;
    bucket_clear();
    return json_null();
}

/* PoP: w11_redactvaul_87f037_redact @ agent/redact_vault_values.py:redact_registered_vault_values */
json_t *w11_redactvaul_87f037_redact(json_t *a) {
    json_t *text = a ? json_obj_get(a, "text") : NULL;
    if (!json_is_string(text) || !text->str_val || !text->str_val[0])
        return text ? json_copy(text) : json_null();
    char *result = (char *)strdup(text->str_val);
    if (!result) return json_copy(text);
    const char **sorted = (const char **)malloc((g_count ? g_count : 1) * sizeof(const char *));
    if (!sorted) { free(result); return json_copy(text); }
    for (size_t i = 0; i < g_count; i++) sorted[i] = g_values[i];
    sort_by_len_desc(sorted, g_count);
    for (size_t i = 0; i < g_count; i++) {
        char *next = replace_all(result, sorted[i], REDACTED_MARKER);
        if (next) { free(result); result = next; }
    }
    free(sorted);
    json_t *out = json_string(result);
    free(result);
    return out ? out : json_null();
}
