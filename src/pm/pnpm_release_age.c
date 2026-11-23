/*
 * pnpm_release_age.c — faithful C11 port of pm/pnpm_release_age.py
 * (gate w7_pnpmrelease_bf14bc).
 *
 * PoP: w7_pnpmrelease_is_release_age_failure @ pm/pnpm_release_age.py:is_release_age_failure
 * PoP: w7_pnpmrelease_kept_versions @ pm/pnpm_release_age.py:_kept_versions
 * PoP: w7_pnpmrelease_yaml_scalar @ pm/pnpm_release_age.py:_yaml_scalar
 * PoP: w7_pnpmrelease_exclude_hint @ pm/pnpm_release_age.py:exclude_hint
 *
 * The pnpm subprocess/YAML-rewrite functions are intentionally not gated here;
 * only the deterministic helpers are exported.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const W7_REQUIRES_SAVE = "ERR_PNPM_STRICT_MIN_RELEASE_AGE_REQUIRES_SAVE";
static const char *const W7_NO_MATURE = "ERR_PNPM_NO_MATURE_MATCHING_VERSION";

json_t *w7_pnpmrelease_is_release_age_failure(const char *output) {
    if (!output) return json_bool(false);
    return json_bool(strstr(output, W7_REQUIRES_SAVE) != NULL ||
                     strstr(output, W7_NO_MATURE) != NULL);
}

static char *w7_trim(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int w7_locked_has(const json_t *locked, const char *name, const char *version) {
    if (!locked || locked->type != JSON_OBJECT || !name || !version) return 0;
    json_t *versions = json_obj_get(locked, name);
    if (!versions || versions->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < versions->c.count; i++) {
        json_t *v = versions->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, version)) return 1;
    }
    return 0;
}

json_t *w7_pnpmrelease_kept_versions(const char *entry, const json_t *locked) {
    if (!entry) return json_null();
    const char *at = strrchr(entry, '@');
    if (!at || at == entry) return json_null();          /* rpartition('@') -> empty name */
    if (strchr(entry, '*') && strchr(entry, '*') < at) return json_null();  /* '*' in name */
    size_t namelen = (size_t)(at - entry);
    if (!namelen || !at[1]) return json_null();
    char *name = (char *)malloc(namelen + 1);
    if (!name) return json_null();
    memcpy(name, entry, namelen);
    name[namelen] = '\0';
    json_t *out = json_array();
    const char *cur = at + 1;
    for (;;) {
        const char *sep = strstr(cur, "||");
        size_t n = sep ? (size_t)(sep - cur) : strlen(cur);
        char *raw = (char *)malloc(n + 1);
        if (raw) {
            memcpy(raw, cur, n);
            raw[n] = '\0';
            char *v = w7_trim(raw);
            if (v && w7_locked_has(locked, name, v)) json_append(out, json_string(v));
            free(v); free(raw);
        }
        if (!sep) break;
        cur = sep + 2;
    }
    free(name);
    return out;
}

/* A YAML plain scalar round-trips unless its first character is an indicator,
 * it contains ': ' or ' #', or it is implicitly typed by YAML. Mirrors
 * hermes_yaml.safe_load(f"- {value}") == [value]. */
static int w7_yaml_needs_quote(const char *v) {
    if (!v || !*v) return 1;
    unsigned char c0 = (unsigned char)v[0];
    if (strchr("@!&*?%'\",[]{}#>|", (int)c0)) return 1;
    if ((c0 == '-' || c0 == '?' || c0 == ':') && (v[1] == ' ' || v[1] == '\t')) return 1;
    if (strstr(v, ": ") || strstr(v, " #")) return 1;
    if (strchr(v, '\n') || strchr(v, '\r') || strchr(v, '\t')) return 1;
    if (strchr(v, '\'' ) || strchr(v, '"')) return 1;
    static const char *const words[] = {
        "true", "false", "null", "yes", "no", "on", "off", "~",
        ".nan", ".inf", "-.inf", "+.inf", NULL};
    for (int i = 0; words[i]; i++) if (!strcasecmp(v, words[i])) return 1;
    char *end = NULL;
    (void)strtod(v, &end);
    if (end && *end == '\0') return 1;   /* YAML parses as a number */
    return 0;
}

json_t *w7_pnpmrelease_yaml_scalar(const char *value) {
    if (!value) return json_string("''");
    if (!w7_yaml_needs_quote(value)) return json_string(value);
    size_t n = strlen(value);
    char *quoted = (char *)malloc(2 * n + 3);
    if (!quoted) return json_string(value);
    quoted[0] = '\'';
    size_t k = 1;
    for (size_t i = 0; i < n; i++) {
        quoted[k++] = value[i];
        if (value[i] == '\'') quoted[k++] = '\'';
    }
    quoted[k++] = '\'';
    quoted[k] = '\0';
    json_t *out = json_string(quoted);
    free(quoted);
    return out;
}

/* Growable string builder for exclude_hint. */
typedef struct { char *p; size_t n, cap; } w7_buf;
static void w7_buf_add(w7_buf *b, const char *s) {
    size_t m = strlen(s);
    if (b->n + m + 1 > b->cap) {
        size_t nc = (b->n + m + 1) * 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, m);
    b->n += m;
    b->p[b->n] = '\0';
}

json_t *w7_pnpmrelease_exclude_hint(const json_t *picks) {
    w7_buf b = {0};
    b.p = (char *)malloc(1);
    if (!b.p) return json_string("");
    b.p[0] = '\0'; b.cap = 1;
    if (!picks || picks->type != JSON_ARRAY || picks->c.count == 0) {
        w7_buf_add(&b, "  a range's only matches are newer than minimumReleaseAge (pnpm-workspace.yaml): wait for them\n"
                       "  to age, or add each exact name@version to minimumReleaseAgeExclude");
    } else {
        w7_buf_add(&b, "  only versions newer than minimumReleaseAge (pnpm-workspace.yaml) match: wait for them to age,\n"
                       "  or add them to minimumReleaseAgeExclude:\n");
        for (size_t i = 0; i < picks->c.count; i++) {
            if (i) w7_buf_add(&b, "\n");
            w7_buf_add(&b, "    - ");
            json_t *pick = picks->c.items[i];
            const char *s = (pick && pick->type == JSON_STRING && pick->str_val) ? pick->str_val : "";
            json_t *scalar = w7_pnpmrelease_yaml_scalar(s);
            w7_buf_add(&b, scalar->str_val ? scalar->str_val : "");
            json_free(scalar);
            w7_buf_add(&b, "  # <why it cannot wait>");
        }
    }
    json_t *out = json_string(b.p);
    free(b.p);
    return out;
}
