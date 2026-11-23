/*
 * i18n_layers.c — C home for upstream Hermes agent/i18n_layers.py.
 * PoP: w6_i18nlayers_f42323_normalize_language_id @ agent/i18n_layers.py:normalize_language_id
 * PoP: w6_i18nlayers_f42323_is_language_id @ agent/i18n_layers.py:is_language_id
 * PoP: w6_i18nlayers_f42323_flatten @ agent/i18n_layers.py:flatten
 * PoP: w6_i18nlayers_f42323_non_text_leaves @ agent/i18n_layers.py:non_text_leaves
 */
#include "noc_w6_i18nlayers_f42323.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

char *w6_i18nlayers_f42323_normalize_language_id(const struct json_t *value) {
    const char *s;
    size_t n, j = 0;
    char *out;
    if (!value || value->type != JSON_STRING) return strdup("");
    s = value->str_val ? value->str_val : "";
    while (*s && is_space(*s)) s++;
    n = strlen(s);
    while (n && is_space(s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == '_') c = '-';
        out[j++] = c;
    }
    out[j] = '\0';
    return out;
}

/* ^[a-z]{2,3}(-[a-z0-9]{2,8})*$ over the normalized id. */
int w6_i18nlayers_f42323_is_language_id(const struct json_t *value) {
    char *s = w6_i18nlayers_f42323_normalize_language_id(value);
    size_t len = strlen(s), pos, gstart, first = 0;
    int ok = 1;
    while (first < len && s[first] >= 'a' && s[first] <= 'z') first++;
    if (first < 2 || first > 3) ok = 0;
    pos = first;
    while (ok && pos < len) {
        size_t glen;
        if (s[pos] != '-') { ok = 0; break; }
        pos++;
        gstart = pos;
        while (pos < len && ((s[pos] >= 'a' && s[pos] <= 'z') || (s[pos] >= '0' && s[pos] <= '9'))) pos++;
        glen = pos - gstart;
        if (glen < 2 || glen > 8) ok = 0;
    }
    free(s);
    return ok ? 1 : 0;
}

static char *child_prefix(const char *prefix, const char *key) {
    char *out;
    size_t n;
    if (!prefix || !*prefix) return strdup(key ? key : "");
    n = strlen(prefix) + 1 + strlen(key ? key : "") + 1;
    out = (char *)malloc(n);
    snprintf(out, n, "%s.%s", prefix, key ? key : "");
    return out;
}

static void flatten_rec(const struct json_t *node, const char *prefix, json_t *out) {
    if (!node) return;
    if (node->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(node); i++) {
            const char *key = json_object_get_key_at(node, i);
            char *p = child_prefix(prefix, key);
            flatten_rec(json_object_get_at(node, i), p, out);
            free(p);
        }
    } else if (node->type == JSON_STRING) {
        json_set(out, prefix ? prefix : "", json_copy(node));
    }
}

struct json_t *w6_i18nlayers_f42323_flatten(const struct json_t *node) {
    json_t *out = json_object();
    flatten_rec(node, "", out);
    return out;
}

static void ntl_rec(const struct json_t *node, const char *prefix, json_t *out) {
    if (!node) return;
    if (node->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(node); i++) {
            const char *key = json_object_get_key_at(node, i);
            char *p = child_prefix(prefix, key);
            ntl_rec(json_object_get_at(node, i), p, out);
            free(p);
        }
    } else if (node->type != JSON_STRING) {
        json_append(out, json_string(prefix && *prefix ? prefix : "<root>"));
    }
}

struct json_t *w6_i18nlayers_f42323_non_text_leaves(const struct json_t *node) {
    json_t *out = json_array();
    ntl_rec(node, "", out);
    return out;
}

