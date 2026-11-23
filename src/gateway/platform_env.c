/*
 * platform_env.c — faithful C11 port of upstream Hermes gateway/platform_env.py.
 *
 * PoP: w7_platformenv_955637_csv_list @ gateway/platform_env.py:csv_list
 * PoP: w7_platformenv_955637_strip_slash @ gateway/platform_env.py:strip_slash
 * PoP: w7_platformenv_955637_strip_lower @ gateway/platform_env.py:strip_lower
 * PoP: w7_platformenv_955637_truthy_token @ gateway/platform_env.py:truthy_token
 * PoP: w7_platformenv_955637_int_or @ gateway/platform_env.py:int_or
 * PoP: w7_platformenv_955637_mention_patterns @ gateway/platform_env.py:mention_patterns
 * PoP: w7_platformenv_955637_env_first @ gateway/platform_env.py:env_first
 */
#include "hermes_json.h"
#include "noc_w7_platformenv_955637.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* str.strip() for ASCII whitespace (the source operates on config/env text). */
static char *pe_strip(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static char *pe_lower_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static int pe_is_truthy_token(const char *v) {
    char *low = pe_lower_dup(v ? v : "");
    if (!low) return 0;
    int yes = !strcmp(low, "true") || !strcmp(low, "1") ||
              !strcmp(low, "yes") || !strcmp(low, "on");
    free(low);
    return yes;
}

struct json_t *w7_platformenv_955637_csv_list(const char *value) {
    struct json_t *arr = json_array();
    const char *p = value ? value : "";
    while (1) {
        const char *comma = strchr(p, ',');
        size_t n = comma ? (size_t)(comma - p) : strlen(p);
        char *seg = (char *)malloc(n + 1);
        if (!seg) { json_free(arr); return arr; }
        memcpy(seg, p, n); seg[n] = '\0';
        char *part = pe_strip(seg);
        free(seg);
        if (part && *part) json_append(arr, json_string(part));
        free(part);
        if (!comma) break;
        p = comma + 1;
    }
    return arr;
}

struct json_t *w7_platformenv_955637_strip_slash(const char *value) {
    const char *s = value ? value : "";
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return json_string("");
    memcpy(out, s, n); out[n] = '\0';
    struct json_t *r = json_string(out);
    free(out);
    return r;
}

struct json_t *w7_platformenv_955637_strip_lower(const char *value) {
    char *s = pe_strip(value);
    char *low = pe_lower_dup(s ? s : "");
    struct json_t *r = json_string(low ? low : "");
    free(low); free(s);
    return r;
}

struct json_t *w7_platformenv_955637_truthy_token(const char *value) {
    return json_bool(pe_is_truthy_token(value) ? true : false);
}

struct json_t *w7_platformenv_955637_int_or(const char *value, const char *default_str) {
    long long def = 0;
    if (default_str && *default_str) {
        errno = 0;
        def = strtoll(default_str, NULL, 10);
        if (errno == ERANGE) def = 0;
    }
    char *stripped = pe_strip(value);
    if (!stripped || !*stripped) { free(stripped); return json_number((double)def); }
    char *end = NULL;
    errno = 0;
    long long parsed = strtoll(stripped, &end, 10);
    if (errno == ERANGE || end == stripped || *end != '\0') {
        free(stripped);
        return json_number((double)def);
    }
    free(stripped);
    return json_number((double)parsed);
}

struct json_t *w7_platformenv_955637_mention_patterns(const char *value) {
    const char *src = value ? value : "";
    char *trimmed = pe_strip(src);
    struct json_t *parsed = json_parse(trimmed ? trimmed : "", NULL);
    free(trimmed);
    if (parsed) return parsed;  /* json.loads succeeded */
    /* csv_list(value.replace("\n", ",")) */
    size_t n = strlen(src);
    char *replaced = (char *)malloc(n + 1);
    if (!replaced) return json_array();
    for (size_t i = 0; i < n; i++) replaced[i] = (src[i] == '\n') ? ',' : src[i];
    replaced[n] = '\0';
    struct json_t *arr = w7_platformenv_955637_csv_list(replaced);
    free(replaced);
    return arr;
}

static const char *pe_env_get(const struct json_t *values, const char *name) {
    if (!values || values->type != JSON_OBJECT || !name) return "";
    struct json_t *v = json_obj_get((struct json_t *)values, name);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

struct json_t *w7_platformenv_955637_env_first(const struct json_t *names,
                                                const struct json_t *values) {
    if (names && names->type == JSON_STRING) {
        return json_string(pe_env_get(values, names->str_val));
    }
    if (names && names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(names); i++) {
            struct json_t *item = json_get((struct json_t *)names, i);
            if (item && item->type == JSON_STRING && item->str_val) {
                const char *v = pe_env_get(values, item->str_val);
                if (v && *v) return json_string(v);
            }
        }
    }
    return json_string("");
}
