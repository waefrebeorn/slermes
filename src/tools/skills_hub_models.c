/*
 * skills_hub_models.c -- faithful C11 port of upstream Hermes
 * tools/skills_hub_models.py (gate w10_skillshub_6c5e0d).
 *
 * PoP: w10_skillshub_6c5e0d_matches_query              @ tools/skills_hub_models.py:_matches_query
 * PoP: w10_skillshub_6c5e0d_dedupe_by_trust            @ tools/skills_hub_models.py:_dedupe_by_trust
 * PoP: w10_skillshub_6c5e0d_normalize_bundle_path      @ tools/skills_hub_models.py:_normalize_bundle_path
 * PoP: w10_skillshub_6c5e0d_validate_skill_name        @ tools/skills_hub_models.py:_validate_skill_name
 * PoP: w10_skillshub_6c5e0d_validate_install_parent_path @ tools/skills_hub_models.py:_validate_install_parent_path
 * PoP: w10_skillshub_6c5e0d_validate_bundle_rel_path   @ tools/skills_hub_models.py:_validate_bundle_rel_path
 * PoP: w10_skillshub_6c5e0d_hermes_tags                @ tools/skills_hub_models.py:_hermes_tags
 * PoP: w10_skillshub_6c5e0d_query_is_concrete          @ tools/skills_hub_models.py:_query_is_concrete
 * PoP: w10_skillshub_6c5e0d_source_url_for_bundle      @ tools/skills_hub_models.py:source_url_for_bundle
 *
 * The dataclasses / HTTP / cache layer is not portable; the nine deterministic
 * helpers above are ported verbatim.
 */
#include "noc_w10_skillshub_6c5e0d.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str() coercion for the JSON scalar types used by these helpers. */
static char *py_str(const json_t *v) {
    if (!v) return (char *)calloc(1, 1);
    char buf[64];
    switch (v->type) {
        case JSON_STRING: {
            const char *s = v->str_val ? v->str_val : "";
            char *o = (char *)malloc(strlen(s) + 1); if (o) strcpy(o, s); return o;
        }
        case JSON_NULL: { char *o = (char *)malloc(5); if (o) strcpy(o, "None"); return o; }
        case JSON_BOOL: { const char *s = v->bool_val ? "True" : "False";
                          char *o = (char *)malloc(strlen(s) + 1); if (o) strcpy(o, s); return o; }
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
            else snprintf(buf, sizeof buf, "%.17g", d);
            char *o = (char *)malloc(strlen(buf) + 1); if (o) strcpy(o, buf); return o;
        }
        default: { char *o = (char *)calloc(1, 1); return o; }
    }
}

static char *ascii_lower(const char *s) {
    if (!s) s = "";
    char *o = (char *)malloc(strlen(s) + 1); if (!o) return NULL;
    size_t i; for (i = 0; s[i]; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[i] = '\0'; return o;
}

static json_t *raise_value_error(void) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string("ValueError"));
    return o;
}

json_t *w10_skillshub_6c5e0d_matches_query(const char *query_lower, const json_t *fields) {
    if (!query_lower) query_lower = "";
    char *parts = (char *)calloc(1, 1);
    size_t cap = 1;
    size_t nf = (fields && fields->type == JSON_ARRAY) ? json_len(fields) : 0;
    for (size_t i = 0; i < nf; i++) {
        json_t *f = json_get(fields, i);
        char *piece;
        if (f && f->type == JSON_ARRAY) {
            size_t m = json_len(f);
            size_t n = 1; for (size_t j = 0; j < m; j++) { char *s = py_str(json_get(f, j)); n += strlen(s) + 1; free(s); }
            piece = (char *)calloc(n + 1, 1);
            for (size_t j = 0; j < m; j++) {
                char *s = py_str(json_get(f, j));
                if (*piece) strcat(piece, " ");
                strcat(piece, s);
                free(s);
            }
        } else {
            piece = py_str(f);
        }
        size_t need = strlen(parts) + strlen(piece) + 2;
        if (need > cap) { cap = need * 2 + 1; char *t = (char *)realloc(parts, cap); if (t) { parts = t; } }
        if (*parts) strcat(parts, " ");
        strcat(parts, piece);
        free(piece);
    }
    char *low = ascii_lower(parts);
    free(parts);
    int found = strstr(low, query_lower) != NULL;
    free(low);
    return json_bool(found);
}

json_t *w10_skillshub_6c5e0d_dedupe_by_trust(const json_t *results) {
    json_t *out = json_array();
    size_t *idx = NULL; int *rank = NULL; char **ids = NULL;
    size_t nkept = 0, cap = 0;
    size_t n = (results && results->type == JSON_ARRAY) ? json_len(results) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_get(results, i);
        if (!r || r->type != JSON_OBJECT) continue;
        const char *ident = json_get_str(r, "identifier", "");
        const char *trust = json_get_str(r, "trust_level", "");
        int tr = !strcmp(trust, "builtin") ? 2 : !strcmp(trust, "trusted") ? 1 : 0;
        size_t found = nkept; int have = 0;
        for (size_t k = 0; k < nkept; k++) if (!strcmp(ids[k], ident)) { found = k; have = 1; break; }
        if (!have) {
            if (nkept == cap) {
                cap = cap ? cap * 2 : 8;
                idx = (size_t *)realloc(idx, cap * sizeof(size_t));
                rank = (int *)realloc(rank, cap * sizeof(int));
                ids = (char **)realloc(ids, cap * sizeof(char *));
            }
            ids[nkept] = (char *)malloc(strlen(ident) + 1); strcpy(ids[nkept], ident);
            idx[nkept] = i; rank[nkept] = tr; nkept++;
        } else if (tr > rank[found]) {
            rank[found] = tr; idx[found] = i;  /* dict assignment keeps original position */
        }
    }
    for (size_t k = 0; k < nkept; k++) {
        json_append(out, json_copy(json_get(results, idx[k])));
        free(ids[k]);
    }
    free(idx); free(rank); free(ids);
    return out;
}

/* _normalize_bundle_path: returns NULL (with *raised=1) on ValueError. */
static char *normalize_path(const char *path_value, int allow_nested, int *raised) {
    *raised = 0;
    if (!path_value) { *raised = 1; return NULL; }
    while (*path_value && isspace((unsigned char)*path_value)) path_value++;
    size_t l = strlen(path_value);
    while (l && isspace((unsigned char)path_value[l - 1])) l--;
    if (l == 0) { *raised = 1; return NULL; }
    char *norm = (char *)malloc(l + 1);
    if (!norm) { *raised = 1; return NULL; }
    for (size_t i = 0; i < l; i++) norm[i] = path_value[i] == '\\' ? '/' : path_value[i];
    norm[l] = '\0';
    if (norm[0] == '/') { free(norm); *raised = 1; return NULL; }
    char **parts = (char **)malloc((l + 1) * sizeof(char *));
    size_t np = 0;
    char *cur = norm;
    for (char *p = norm;; p++) {
        if (*p == '/' || *p == '\0') {
            size_t plen = (size_t)(p - cur);
            if (plen && !(plen == 1 && cur[0] == '.')) {
                parts[np] = (char *)malloc(plen + 1);
                memcpy(parts[np], cur, plen); parts[np][plen] = '\0'; np++;
            }
            if (*p == '\0') break;
            cur = p + 1;
        }
    }
    int bad = np == 0 || (!allow_nested && np != 1);
    for (size_t i = 0; i < np && !bad; i++) {
        if (!strcmp(parts[i], "..")) bad = 1;
        if (strchr(parts[i], ':')) bad = 1;
    }
    if (bad) {
        for (size_t i = 0; i < np; i++) free(parts[i]);
        free(parts); free(norm); *raised = 1; return NULL;
    }
    size_t total = np; for (size_t i = 0; i < np; i++) total += strlen(parts[i]);
    char *out = (char *)malloc(total + 1);
    out[0] = '\0';
    for (size_t i = 0; i < np; i++) {
        if (i) strcat(out, "/");
        strcat(out, parts[i]);
        free(parts[i]);
    }
    free(parts); free(norm);
    return out;
}

static json_t *wrap_norm(const char *value, int allow_nested) {
    int raised = 0;
    char *s = normalize_path(value, allow_nested, &raised);
    if (raised) return raise_value_error();
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

json_t *w10_skillshub_6c5e0d_normalize_bundle_path(const char *path_value, const char *field_name, int allow_nested) {
    (void)field_name;  /* only present in the error message, which is collapsed to the type */
    return wrap_norm(path_value, allow_nested);
}
json_t *w10_skillshub_6c5e0d_validate_skill_name(const char *name) { return wrap_norm(name, 0); }
json_t *w10_skillshub_6c5e0d_validate_install_parent_path(const char *category) { return wrap_norm(category, 1); }
json_t *w10_skillshub_6c5e0d_validate_bundle_rel_path(const char *rel_path) { return wrap_norm(rel_path, 1); }

json_t *w10_skillshub_6c5e0d_hermes_tags(const json_t *fm) {
    json_t *metadata = fm ? json_obj_get(fm, "metadata") : NULL;
    json_t *hermes = (metadata && metadata->type == JSON_OBJECT) ? json_obj_get(metadata, "hermes") : NULL;
    json_t *tags = (hermes && hermes->type == JSON_OBJECT) ? json_obj_get(hermes, "tags") : NULL;
    return tags ? json_copy(tags) : json_array();
}

/* Regex _VALUELESS_QUERY_FLAG_RE: (?:[A-Za-z0-9_~-]|%[0-9A-Fa-f]{2})+ fullmatch */
static int valueless_flag(const char *s) {
    if (!s || !*s) return 0;
    size_t i = 0;
    while (s[i]) {
        char c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '_' || c == '~' || c == '-') { i++; continue; }
        if (c == '%' && i + 2 < strlen(s) && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])) {
            i += 3; continue;
        }
        return 0;
    }
    return 1;
}

json_t *w10_skillshub_6c5e0d_query_is_concrete(const char *query) {
    if (!query) query = "";
    const char *p = query;
    int all = 1;
    for (;;) {
        const char *q = strchr(p, '&');
        size_t n = q ? (size_t)(q - p) : strlen(p);
        char *part = (char *)malloc(n + 1);
        memcpy(part, p, n); part[n] = '\0';
        char *eq = strchr(part, '=');
        int ok = (eq && eq != part) || valueless_flag(part);
        free(part);
        if (!ok) { all = 0; break; }
        if (!q) break;
        p = q + 1;
    }
    return json_bool(all);
}

static int split_repo(const char *ident, char **a, char **b, char **c) {
    *a = *b = *c = NULL;
    if (!ident) return 0;
    const char *s1 = strchr(ident, '/');
    if (!s1) return 0;
    size_t n1 = (size_t)(s1 - ident);
    *a = (char *)malloc(n1 + 1); memcpy(*a, ident, n1); (*a)[n1] = '\0';
    const char *s2 = strchr(s1 + 1, '/');
    if (!s2) { *b = strdup(s1 + 1); return 2; }
    size_t n2 = (size_t)(s2 - (s1 + 1));
    *b = (char *)malloc(n2 + 1); memcpy(*b, s1 + 1, n2); (*b)[n2] = '\0';
    *c = (char *)calloc(1, 1);
    if (*(s2 + 1)) { free(*c); *c = strdup(s2 + 1); }
    return 3;
}

json_t *w10_skillshub_6c5e0d_source_url_for_bundle(const json_t *bundle) {
    json_t *md = bundle ? json_obj_get(bundle, "metadata") : NULL;
    const char *explicit = NULL;
    char *owned = NULL;
    if (md && md->type == JSON_OBJECT) {
        json_t *sv = json_obj_get(md, "source_url");
        if (!sv || sv->type == JSON_NULL) sv = json_obj_get(md, "url");
        if (sv && sv->type != JSON_NULL) { owned = py_str(sv); explicit = owned; }
    }
    if (explicit) { json_t *o = json_string(explicit); free(owned); return o; }
    const char *source = bundle ? json_get_str(bundle, "source", "") : "";
    const char *ident = json_get_str(bundle, "identifier", "");
    if (!strcmp(source, "github")) {
        char *a, *b, *c; int parts = split_repo(ident, &a, &b, &c);
        if (parts >= 2) {
            char *url;
            if (parts == 3 && *c) url = (char *)malloc(strlen(a) + strlen(b) + strlen(c) + 40);
            else url = (char *)malloc(strlen(a) + strlen(b) + 40);
            if (parts == 3 && *c) sprintf(url, "https://github.com/%s/%s/tree/main/%s", a, b, c);
            else sprintf(url, "https://github.com/%s/%s", a, b);
            json_t *o = json_string(url); free(url); free(a); free(b); free(c); return o;
        }
        /* fall through to identifier */
    }
    return json_string(ident);
}
