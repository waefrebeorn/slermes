/*
 * features.c — C port of upstream Hermes pm/features.py.
 *
 * PoP: w5features_write_payload @ pm/features.py:write_features
 * PoP: w5features_read_payload  @ pm/features.py:read_features
 * PoP: w5features_opt_in        @ pm/features.py:opt_in_extras
 * PoP: w5features_path          @ pm/features.py:features_path
 *
 * The write/read/opt-in logic is deterministic; filesystem, subprocess
 * inventory and tomllib parsing are injected by the gate.
 */
#include "noc_w5_features_c0a63b.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmpstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* sorted(set(items)) -> malloc'd array of strdup'd strings; n set. */
static char **sorted_unique(const char **items, size_t n, size_t *outn) {
    char **v = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t k = 0;
    if (!v) { if (outn) *outn = 0; return NULL; }
    for (size_t i = 0; i < n; i++) {
        if (!items[i]) continue;
        int dup = 0;
        for (size_t j = 0; j < k; j++) if (!strcmp(v[j], items[i])) { dup = 1; break; }
        if (!dup) v[k++] = strdup(items[i]);
    }
    if (k > 1) qsort(v, k, sizeof(char *), cmpstr);
    if (outn) *outn = k;
    return v;
}
void w5features_free_list(char **arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) free(arr[i]);
    free(arr);
}

/* Append a JSON string token (with the minimal escaping json.dumps uses). */
static char *append_jstr(char *p, const char *s) {
    *p++ = '"';
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { *p++ = '\\'; *p++ = (char)c; }
        else if (c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (c == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else if (c == '\t') { *p++ = '\\'; *p++ = 't'; }
        else *p++ = (char)c;
    }
    *p++ = '"';
    return p;
}

char *w5features_write_payload(const char **extras, size_t n) {
    size_t un = 0;
    char **v = sorted_unique(extras, n, &un);
    /* Generous upper bound: each extra <= 4 bytes + 2 quotes + 1 comma,
     * plus fixed scaffolding. */
    size_t cap = 256;
    for (size_t i = 0; i < un; i++) cap += strlen(v[i]) * 6 + 32;
    char *buf = (char *)calloc(cap + 1, 1);
    char *p = buf;
    if (!buf) { w5features_free_list(v, un); return NULL; }
    p += sprintf(p, "{\n  \"schema\": 1,\n  \"extras\": [");
    if (un == 0) {
        p += sprintf(p, "]\n}\n");
    } else {
        *p++ = '\n';
        for (size_t i = 0; i < un; i++) {
            p += sprintf(p, "    ");
            p = append_jstr(p, v[i]);
            if (i + 1 < un) *p++ = ',';
            *p++ = '\n';
        }
        p += sprintf(p, "  ]\n}\n");
    }
    w5features_free_list(v, un);
    return buf;
}

int w5features_read_payload(const char *text, char ***out, size_t *outn) {
    if (out) *out = NULL;
    if (outn) *outn = 0;
    if (!text) return 0;
    json_t *doc = json_parse(text, NULL);
    if (!doc || doc->type != JSON_OBJECT) { if (doc) json_free(doc); return 0; }
    json_t *ex = json_obj_get(doc, "extras");
    if (!ex || ex->type != JSON_ARRAY) { json_free(doc); return 0; }
    size_t n = json_len(ex);
    const char **items = (const char **)calloc(n ? n : 1, sizeof(char *));
    char **owned = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(ex, i);
        if (!e) continue;
        char *s = NULL;
        if (e->type == JSON_STRING) s = strdup(e->str_val ? e->str_val : "");
        else if (e->type == JSON_BOOL) s = strdup(e->bool_val ? "True" : "False");
        else if (e->type == JSON_NULL) s = strdup("None");
        else if (e->type == JSON_NUMBER) {
            s = (char *)malloc(64);
            if (s) {
                double d = e->num_val;
                if (d == (double)(long long)d) snprintf(s, 64, "%lld", (long long)d);
                else snprintf(s, 64, "%g", d);
            }
        }
        if (s) { owned[k] = s; items[k] = s; k++; }
    }
    size_t un = 0;
    char **v = sorted_unique(items, k, &un);
    for (size_t i = 0; i < k; i++) free(owned[i]);
    free(owned);
    free(items);
    json_free(doc);
    if (out) *out = v;
    if (outn) *outn = un;
    return 1;
}

int w5features_opt_in(const char **names, size_t nn,
                      const char **declared, size_t nd,
                      char ***out, size_t *outn) {
    if (out) *out = NULL;
    if (outn) *outn = 0;
    for (size_t i = 0; i < nn; i++) {
        int found = 0;
        for (size_t j = 0; j < nd; j++)
            if (declared[j] && !strcmp(names[i], declared[j])) { found = 1; break; }
        if (!found) return 0; /* ValueError on undeclared extras */
    }
    size_t un = 0;
    char **v = sorted_unique(names, nn, &un);
    if (out) *out = v;
    if (outn) *outn = un;
    return 1;
}

int w5features_path(const char *base_dir, char *out, size_t cap) {
    if (!out || !cap) return 0;
    snprintf(out, cap, "%s/%s", base_dir ? base_dir : "", W5FEATURES_FILENAME);
    return 1;
}
