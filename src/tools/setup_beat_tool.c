/*
 * setup_beat_tool.c — C home for upstream Hermes tools/setup_beat_tool.py.
 *
 * PoP: w8_setupbeatt_cca545_suggest_from @ tools/setup_beat_tool.py:_suggest_from
 * PoP: w8_setupbeatt_cca545_unlisted_suggestions @ tools/setup_beat_tool.py:_unlisted_suggestions
 */
#include "noc_w8_setupbeatt_cca545.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *strip_ws(const char *s) {
    if (!s) return NULL;
    const unsigned char *a = (const unsigned char *)s;
    while (*a && is_ws(*a)) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && is_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = '\0'; }
    return out;
}

static int cmp_strptr(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x ? x : "", y ? y : "");
}

/* Python sorted(slugs) then ", ".join(...), as the card suffix. */
char *w8_setupbeatt_cca545_suggest_from(const struct json_t *slugs) {
    if (!slugs || slugs->type != JSON_ARRAY || !json_len(slugs)) return (char *)strdup("");
    size_t n = json_len(slugs);
    const char **items = (const char **)calloc(n, sizeof(const char *));
    if (!items) return (char *)strdup("");
    size_t k = 0;
    for (size_t i = 0; i < n; ++i) {
        struct json_t *v = json_get(slugs, i);
        if (v && v->type == JSON_STRING) items[k++] = v->str_val;
    }
    qsort(items, k, sizeof(const char *), cmp_strptr);
    size_t cap = 128;
    for (size_t i = 0; i < k; ++i) cap += strlen(items[i]) + 2;
    char *out = (char *)malloc(cap);
    if (out) {
        size_t at = (size_t)sprintf(out, " Suggest only connectors from this list, by these slugs: ");
        for (size_t i = 0; i < k; ++i) {
            if (i) at += (size_t)sprintf(out + at, ", ");
            at += (size_t)sprintf(out + at, "%s", items[i]);
        }
        sprintf(out + at, ".");
    }
    free(items);
    return out;
}

/* str(slug) for the JSON scalar kinds used here (string/number/bool/null). */
static char *py_str(const struct json_t *v) {
    if (!v) return (char *)strdup("None");
    switch (v->type) {
        case JSON_STRING: return (char *)strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return (char *)strdup(v->bool_val ? "True" : "False");
        case JSON_NULL: return (char *)strdup("None");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                sprintf(buf, "%lld", (long long)v->num_val);
            else
                sprintf(buf, "%g", v->num_val);
            return (char *)strdup(buf);
        }
        default: return (char *)strdup("");
    }
}

static int in_slugs(const struct json_t *slugs, const char *name) {
    if (!slugs || slugs->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(slugs); ++i) {
        struct json_t *v = json_get(slugs, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

char *w8_setupbeatt_cca545_unlisted_suggestions(const struct json_t *args,
        const struct json_t *slugs) {
    if (!args || args->type != JSON_OBJECT) return NULL;
    struct json_t *suggest = json_obj_get(args, "suggest");
    int is_arr = suggest && suggest->type == JSON_ARRAY && json_len(suggest);
    int is_str = suggest && suggest->type == JSON_STRING && suggest->str_val && *suggest->str_val;
    if (!is_arr && !is_str) return NULL;   /* empty/falsy suggest -> () -> None */
    size_t count = is_str ? strlen(suggest->str_val) : json_len(suggest);
    const char **wanted = (const char **)calloc(count ? count : 1, sizeof(const char *));
    size_t nw = 0;
    if (wanted) {
        for (size_t i = 0; i < count; ++i) {
            char *raw;
            if (is_str) {
                raw = (char *)malloc(2);
                if (raw) { raw[0] = suggest->str_val[i]; raw[1] = '\0'; }
            } else {
                raw = py_str(json_get(suggest, i));
            }
            char *t = strip_ws(raw ? raw : "");
            if (t && *t) {
                char *lower = (char *)malloc(strlen(t) + 1);
                if (lower) {
                    size_t j;
                    for (j = 0; t[j]; ++j) lower[j] = (char)tolower((unsigned char)t[j]);
                    lower[j] = '\0';
                    wanted[nw++] = lower;
                }
            }
            free(t); free(raw);
        }
    }
    size_t nunlisted = 0;
    for (size_t i = 0; i < nw; ++i)
        if (!in_slugs(slugs, wanted[i])) nunlisted++;
    if (!nunlisted || !slugs || slugs->type != JSON_ARRAY) {
        for (size_t i = 0; i < nw; ++i) free((void *)wanted[i]);
        free(wanted);
        return NULL;
    }
    char *suffix = w8_setupbeatt_cca545_suggest_from(slugs);
    size_t cap = 512;
    for (size_t i = 0; i < nw; ++i) cap += strlen(wanted[i]) + 4;
    cap += suffix ? strlen(suffix) : 0;
    char *joined = (char *)malloc(cap);
    if (!joined) { free(suffix); for (size_t i = 0; i < nw; ++i) free((void *)wanted[i]); free(wanted); return NULL; }
    size_t at = 0;
    for (size_t i = 0; i < nw; ++i) {
        if (!in_slugs(slugs, wanted[i])) {
            if (at) at += (size_t)sprintf(joined + at, ", ");
            at += (size_t)sprintf(joined + at, "%s", wanted[i]);
        }
    }
    char *out = (char *)malloc(cap + 256);
    if (out) {
        sprintf(out,
            "{\"error\": \"not connectors here\", \"reminder\": "
            "\"No card shown: %s cannot be connected here. Do not mention them as connections."
            "%s Then call setup_beat(beat=\\\"connectors\\\", suggest=[...]) again, "
            "or without suggest when none fits.\"}",
            joined, suffix ? suffix : "");
    }
    free(joined); free(suffix);
    for (size_t i = 0; i < nw; ++i) free((void *)wanted[i]);
    free(wanted);
    return out;
}
