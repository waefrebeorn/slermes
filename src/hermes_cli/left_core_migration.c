/*
 * left_core_migration.c — C home for upstream Hermes hermes_cli/left_core_migration.py.
 *
 * PoP: leftcorem_first_cause @ hermes_cli/left_core_migration.py:_first_cause
 * PoP: leftcorem_delegation_backend_routes @ hermes_cli/left_core_migration.py:_delegation_backend_routes
 */
#include "noc_w18_leftcorem_324dab.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s);

/* Python str.strip() over the ASCII whitespace set (the fixtures are UTF-8 but
 * line/cause separators are ASCII).  Returns a fresh NUL-terminated string. */
static char *py_strip(const char *s) {
    static const char *WS = " \t\n\r\v\f";
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && strchr(WS, *a)) a++;
    const char *b = a + strlen(a);
    while (b > a && strchr(WS, b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = '\0'; }
    return out;
}

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* Python str.rstrip(". ") */
static char *py_rstrip_dot_space(char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n && (s[n - 1] == '.' || s[n - 1] == ' ')) { s[--n] = '\0'; }
    return s;
}

char *leftcorem_first_cause(const char *error) {
    if (!error) error = "";
    char **lines = NULL;
    size_t nlines = 0, cap = 0;
    const char *p = error;
    while (*p) {
        const char *start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        size_t len = (size_t)(p - start);
        char *raw = (char *)malloc(len + 1);
        if (raw) {
            if (len) memcpy(raw, start, len);
            raw[len] = '\0';
            char *st = py_strip(raw);
            free(raw);
            if (st && *st) {
                if (nlines == cap) {
                    size_t nc = cap ? cap * 2 : 8;
                    char **np = (char **)realloc(lines, nc * sizeof(*np));
                    if (np) { lines = np; cap = nc; }
                }
                if (nlines < cap) lines[nlines++] = st;
                else free(st);
            } else {
                free(st);
            }
        }
        if (*p == '\r') { p++; if (*p == '\n') p++; }
        else if (*p == '\n') p++;
    }

    char *cause = NULL;
    for (size_t i = 0; i < nlines; ++i) {
        const char *ln = lines[i];
        if (!strncmp(ln, "fatal:", 6) || !strncmp(ln, "error:", 6)) {
            const char *colon = strchr(ln, ':');
            if (colon) { cause = py_strip(colon + 1); }
            break;
        }
    }

    char *base;
    if (cause && *cause) {
        base = cause;
    } else {
        free(cause);
        base = nlines ? dupstr(lines[0]) : dupstr("unknown error");
    }
    /* py_rstrip_dot_space trims in place and returns the SAME allocation;
     * do not free(base) here — caller owns the returned pointer. */
    char *res = py_rstrip_dot_space(base);
    for (size_t i = 0; i < nlines; ++i) free(lines[i]);
    free(lines);
    return res;
}

struct json_t *leftcorem_delegation_backend_routes(const struct json_t *delegation) {
    json_t *out = json_array();
    if (!delegation || delegation->type != JSON_OBJECT) return out;
    json_t *backends = json_obj_get(delegation, "backends");
    if (!backends || backends->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(backends); ++i) {
        json_t *b = json_object_get_at(backends, i);
        if (!b || b->type != JSON_OBJECT) continue;
        json_t *opts = json_obj_get(b, "options");
        if (!opts || opts->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < json_len(opts); ++j) {
            json_t *o = json_get(opts, j);
            if (o && o->type == JSON_OBJECT) json_append(out, json_copy(o));
        }
    }
    return out;
}
