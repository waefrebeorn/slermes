/*
 * pip_check.c — faithful C11 port of upstream Hermes pm/pip_check.py.
 *
 * PoP: w5_pipcheck_88ae1a_normalize @ pm/pip_check.py:_normalize
 * PoP: w5_pipcheck_88ae1a_override_requirements @ pm/pip_check.py:override_requirements
 * PoP: w5_pipcheck_88ae1a_overridden_versions @ pm/pip_check.py:overridden_versions
 * PoP: w5_pipcheck_88ae1a_unexpected @ pm/pip_check.py:unexpected_incompatibilities
 *
 * File/TOML I/O is exposed as a fixture seam (already-parsed overrides and
 * (name, version) pairs). The line/total/version regexes are ported directly.
 */
#include "noc_w5_pipcheck_88ae1a.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { char **v; size_t n, cap; } w5p_strvec;

static void w5p_push(w5p_strvec *s, char *p) {
    if (s->n >= s->cap) {
        size_t need = s->cap ? s->cap * 2 : 8;
        char **tmp = (char **)realloc(s->v, need * sizeof(char *));
        if (!tmp) { free(p); return; }
        s->v = tmp;
        s->cap = need;
    }
    s->v[s->n++] = p;
}

static void w5p_free(w5p_strvec *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL; s->n = s->cap = 0;
}

static int w5p_has(const w5p_strvec *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) if (!strcmp(s->v[i], x)) return 1;
    return 0;
}

static char *w5p_dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *w5p_strip(const char *s) {
    if (!s) return NULL;
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return w5p_dupn(s + a, b - a);
}

struct json_t *w5_pipcheck_88ae1a_normalize(const char *name) {
    if (!name) return json_string("");
    size_t n = strlen(name);
    char *out = (char *)malloc(n + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (c == '-' || c == '_' || c == '.') {
            if (k == 0 || out[k - 1] != '-') out[k++] = '-';
        } else {
            out[k++] = (char)tolower((unsigned char)c);
        }
    }
    out[k] = '\0';
    struct json_t *r = json_string(out);
    free(out);
    return r;
}

/* str.splitlines() over the line boundaries fixtures exercise (\n, \r\n, \r). */
static w5p_strvec w5p_splitlines(const char *text) {
    w5p_strvec out = {0};
    if (!text) return out;
    size_t start = 0, n = strlen(text);
    for (size_t i = 0; i < n; i++) {
        if (text[i] == '\r' || text[i] == '\n') {
            w5p_push(&out, w5p_dupn(text + start, i - start));
            if (text[i] == '\r' && i + 1 < n && text[i + 1] == '\n') i++;
            start = i + 1;
        }
    }
    if (start < n) w5p_push(&out, w5p_dupn(text + start, n - start));
    return out;
}

/* ^The package `[^`]+` requires  */
static int w5p_report_line(const char *line) {
    const char p[] = "The package `";
    if (strncmp(line, p, sizeof(p) - 1)) return 0;
    const char *g = line + sizeof(p) - 1;
    const char *close = strchr(g, '`');
    if (!close || close == g) return 0;
    return !strncmp(close, "` requires ", 11);
}

/* ^The package `[^`]+` requires `([A-Za-z0-9][A-Za-z0-9._-]*)[^`]*`, but `([^`]+)` is installed$ */
static int w5p_version_mismatch(const char *line, char **g1, char **g2) {
    *g1 = *g2 = NULL;
    const char p[] = "The package `";
    if (strncmp(line, p, sizeof(p) - 1)) return 0;
    const char *cur = line + sizeof(p) - 1;
    const char *close = strchr(cur, '`');
    if (!close || close == cur) return 0;
    cur = close;
    if (strncmp(cur, "` requires `", 12)) return 0;
    cur += 12;
    if (!((*cur >= 'a' && *cur <= 'z') || (*cur >= 'A' && *cur <= 'Z') || (*cur >= '0' && *cur <= '9')))
        return 0;
    const char *a1 = cur++;
    while ((*cur >= 'a' && *cur <= 'z') || (*cur >= 'A' && *cur <= 'Z') ||
           (*cur >= '0' && *cur <= '9') || *cur == '.' || *cur == '_' || *cur == '-') cur++;
    *g1 = w5p_dupn(a1, (size_t)(cur - a1));
    while (*cur && *cur != '`') cur++;
    if (*cur != '`') { free(*g1); *g1 = NULL; return 0; }
    cur++;
    if (strncmp(cur, ", but `", 7)) { free(*g1); *g1 = NULL; return 0; }
    cur += 7;
    const char *a2 = cur;
    while (*cur && *cur != '`') cur++;
    if (*cur != '`' || cur == a2) { free(*g1); *g1 = NULL; return 0; }
    *g2 = w5p_dupn(a2, (size_t)(cur - a2));
    cur++;
    if (strcmp(cur, " is installed")) { free(*g1); free(*g2); *g1 = *g2 = NULL; return 0; }
    return 1;
}

/* Python re.search with MULTILINE over the RAW text: a CR before the LF is
 * part of the line and makes `y$` fail, so split only on LF here. */
static int w5p_found_count(const char *report, long long *count) {
    if (!report) return 0;
    const char *start = report;
    const char *p = report;
    for (;; p++) {
        if (*p != '\n' && *p != '\0') continue;
        size_t len = (size_t)(p - start);
        const char pfx[] = "Found ";
        if (len > sizeof(pfx) - 1 && !strncmp(start, pfx, sizeof(pfx) - 1)) {
            const char *d = start + sizeof(pfx) - 1;
            const char *e = d;
            while (e < p && isdigit((unsigned char)*e)) e++;
            const char *tail;
            if (e > d && (size_t)(p - e) == 16 && !strncmp(e, " incompatibility", 16)) tail = e + 16;
            else if (e > d && (size_t)(p - e) == 18 && !strncmp(e, " incompatibilities", 18)) tail = e + 18;
            else tail = NULL;
            if (tail == p) {
                char *num = w5p_dupn(d, (size_t)(e - d));
                *count = strtoll(num, NULL, 10);
                free(num);
                return 1;
            }
        }
        if (*p == '\0') break;
        start = p + 1;
    }
    return 0;
}

struct json_t *w5_pipcheck_88ae1a_override_requirements(const struct json_t *entries) {
    struct json_t *out = json_array();
    if (!entries || entries->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(entries); i++) {
        const json_t *e = json_get(entries, i);
        if (!e || e->type != JSON_OBJECT) continue;
        for (size_t k = 0; k < json_object_size(e); k++) {
            const char *key = json_object_get_key_at(e, k);
            if (strcmp(key, "name") && strcmp(key, "specifier") && strcmp(key, "marker")) {
                json_free(out);
                struct json_t *err = json_object();
                json_set(err, "__error__", json_string("ValueError"));
                return err;
            }
        }
        const json_t *nv = json_obj_get(e, "name");
        if (!nv || nv->type != JSON_STRING) {
            json_free(out);
            struct json_t *err = json_object();
            json_set(err, "__error__", json_string("KeyError"));
            return err;
        }
        const json_t *sv = json_obj_get(e, "specifier");
        const char *spec = (sv && sv->type == JSON_STRING) ? sv->str_val : "";
        const json_t *mv = json_obj_get(e, "marker");
        int has_marker = (mv && mv->type == JSON_STRING && mv->str_val[0] != '\0');
        size_t buflen = strlen(nv->str_val) + strlen(spec) +
                        (has_marker ? strlen(mv->str_val) + 4 : 0) + 1;
        char *line = (char *)malloc(buflen);
        if (!line) continue;
        strcpy(line, nv->str_val);
        strcat(line, spec);
        if (has_marker) { strcat(line, " ; "); strcat(line, mv->str_val); }
        json_append(out, json_string(line));
        free(line);
    }
    return out;
}

static int w5p_json_array_has_string(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

static void w5p_sort_json_string_array(json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY || arr->c.count < 2) return;
    for (size_t i = 1; i < arr->c.count; i++) {
        size_t j = i;
        while (j > 0 && strcmp(arr->c.items[j - 1]->str_val, arr->c.items[j]->str_val) > 0) {
            json_t *t = arr->c.items[j - 1];
            arr->c.items[j - 1] = arr->c.items[j];
            arr->c.items[j] = t;
            j--;
        }
    }
}

struct json_t *w5_pipcheck_88ae1a_overridden_versions(const struct json_t *entries,
                                                     const struct json_t *packages) {
    struct json_t *out = json_object();
    w5p_strvec overridden = {0};
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const json_t *e = json_get(entries, i);
            if (!e || e->type != JSON_OBJECT) continue;
            const json_t *nv = json_obj_get(e, "name");
            if (!nv || nv->type != JSON_STRING) continue;
            struct json_t *norm = w5_pipcheck_88ae1a_normalize(nv->str_val);
            const char *s = json_string_value(norm);
            if (s && !w5p_has(&overridden, s)) w5p_push(&overridden, w5p_dupn(s, strlen(s)));
            json_free(norm);
        }
    }
    if (packages && packages->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(packages); i++) {
            const json_t *pair = json_get(packages, i);
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
            const json_t *nv = json_get(pair, 0), *vv = json_get(pair, 1);
            if (!nv || nv->type != JSON_STRING || !vv || vv->type != JSON_STRING) continue;
            struct json_t *norm = w5_pipcheck_88ae1a_normalize(nv->str_val);
            const char *key = json_string_value(norm);
            if (!key || !w5p_has(&overridden, key)) { json_free(norm); continue; }
            json_free(norm);
            json_t *bucket = json_obj_get(out, key);
            if (!bucket) { bucket = json_array(); json_set(out, key, bucket); }
            if (!w5p_json_array_has_string(bucket, vv->str_val))
                json_append(bucket, json_string(vv->str_val));
        }
    }
    w5p_free(&overridden);
    for (size_t i = 0; i < json_object_size(out); i++)
        w5p_sort_json_string_array((json_t *)json_object_get_at(out, i));
    return out;
}

struct json_t *w5_pipcheck_88ae1a_unexpected(const char *report, const struct json_t *lock) {
    struct json_t *out = json_array();
    w5p_strvec lines = {0};
    w5p_strvec raw = w5p_splitlines(report);
    for (size_t i = 0; i < raw.n; i++) {
        char *s = w5p_strip(raw.v[i]);
        if (s && w5p_report_line(s)) w5p_push(&lines, s);
        else free(s);
    }
    w5p_free(&raw);
    long long found = 0;
    int have_found = w5p_found_count(report ? report : "", &found);
    if (lines.n == 0 || !have_found || found != (long long)lines.n) {
        w5p_free(&lines);
        json_free(out);
        return json_null();
    }
    int has_lock = lock && lock->type == JSON_OBJECT;
    struct json_t *expected = NULL;
    if (has_lock) {
        expected = w5_pipcheck_88ae1a_overridden_versions(json_obj_get(lock, "overrides"),
                                                          json_obj_get(lock, "packages"));
    }
    for (size_t i = 0; i < lines.n; i++) {
        int accounted = 0;
        char *g1 = NULL, *g2 = NULL;
        if (w5p_version_mismatch(lines.v[i], &g1, &g2) && expected) {
            struct json_t *norm = w5_pipcheck_88ae1a_normalize(g1);
            const char *key = json_string_value(norm);
            if (key) {
                const json_t *bucket = json_obj_get(expected, key);
                if (w5p_json_array_has_string(bucket, g2)) accounted = 1;
            }
            json_free(norm);
        }
        free(g1); free(g2);
        if (!accounted) json_append(out, json_string(lines.v[i]));
    }
    w5p_free(&lines);
    if (expected) json_free(expected);
    return out;
}
