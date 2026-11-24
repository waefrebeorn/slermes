/*
 * plugins_cmd_catalog.c — C home for upstream Hermes hermes_cli/plugins_cmd_catalog.py.
 *
 * PoP: pluginscmd_looks_like_catalog_name @ hermes_cli/plugins_cmd_catalog.py:looks_like_catalog_name
 * PoP: pluginscmd_at_catalog_pin @ hermes_cli/plugins_cmd_catalog.py:at_catalog_pin
 * PoP: pluginscmd_normalized_platforms @ hermes_cli/plugins_cmd_catalog.py:normalized_platforms
 * PoP: pluginscmd_added @ hermes_cli/plugins_cmd_catalog.py:_added
 * PoP: pluginscmd_surface_delta @ hermes_cli/plugins_cmd_catalog.py:surface_delta
 * PoP: pluginscmd_surface_delta_lines @ hermes_cli/plugins_cmd_catalog.py:surface_delta_lines
 * PoP: pluginscmd_pin_label @ hermes_cli/plugins_cmd_catalog.py:pin_label
 */
#include "noc_w18_pluginscmd_9aa05e.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *dupn(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *dup_lower(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { for (size_t i = 0; i < n; ++i) out[i] = (char)tolower((unsigned char)s[i]); out[n] = '\0'; }
    return out;
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static int cmpstr(const void *a, const void *b) {
    const char *x = *(const char * const *)a, *y = *(const char * const *)b;
    return strcmp(x, y);
}

int pluginscmd_looks_like_catalog_name(const char *identifier) {
    if (!identifier || !*identifier) return 0;
    if (strchr(identifier, '/') || strchr(identifier, '\\')) return 0;
    static const char *const SCHEMES[] = {"https://", "http://", "git@", "ssh://", "file://"};
    for (size_t i = 0; i < 5; ++i)
        if (!strncmp(identifier, SCHEMES[i], strlen(SCHEMES[i]))) return 0;
    size_t n = strlen(identifier);
    if (n < 1 || n > 64) return 0;              /* _NAME_RE {1,64} */
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)identifier[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

int pluginscmd_at_catalog_pin(const struct json_t *sidecar, const char *entry_sha) {
    if (!entry_sha || !*entry_sha) return 0;
    char *want = dup_lower(entry_sha);
    json_t *sv = sidecar ? json_obj_get(sidecar, "sha") : NULL;
    json_t *pv = sidecar ? json_obj_get(sidecar, "pin") : NULL;
    char *sl = dup_lower(jstr(sv));
    char *pl = dup_lower(jstr(pv));
    int ok = !strcmp(want, sl) || !strcmp(want, pl);
    free(want); free(sl); free(pl);
    return ok;
}

/* Collect strings from a JSON array into a deduped, sorted C array. */
static char **strset_from(const json_t *arr, size_t *outn) {
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    char **v = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < n; ++i) {
        json_t *it = json_get(arr, i);
        if (it && it->type == JSON_STRING) v[m++] = dupstr(it->str_val ? it->str_val : "");
    }
    qsort(v, m, sizeof(char *), cmpstr);
    size_t w = 0;
    for (size_t i = 0; i < m; ++i) {
        if (w == 0 || strcmp(v[i], v[w - 1])) v[w++] = v[i];
        else free(v[i]);
    }
    *outn = w;
    return v;
}
static int in_strset(char **v, size_t n, const char *s) {
    for (size_t i = 0; i < n; ++i) if (!strcmp(v[i], s)) return 1;
    return 0;
}
static void free_strset(char **v, size_t n) {
    for (size_t i = 0; i < n; ++i) free(v[i]);
    free(v);
}
static json_t *strset_to_json(char **v, size_t n) {
    json_t *out = json_array();
    for (size_t i = 0; i < n; ++i) json_append(out, json_string(v[i]));
    return out;
}

struct json_t *pluginscmd_normalized_platforms(const struct json_t *platforms) {
    size_t n = (platforms && platforms->type == JSON_ARRAY) ? json_len(platforms) : 0;
    char **raw = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < n; ++i) {
        json_t *it = json_get(platforms, i);
        if (!it || it->type != JSON_STRING) continue;
        char *lo = dup_lower(jstr(it));
        if (!strcmp(lo, "windows")) { free(lo); lo = dupstr("win32"); }
        else if (!strcmp(lo, "macos")) { free(lo); lo = dupstr("darwin"); }
        raw[m++] = lo;
    }
    qsort(raw, m, sizeof(char *), cmpstr);
    size_t w = 0;
    for (size_t i = 0; i < m; ++i) {
        if (w == 0 || strcmp(raw[i], raw[w - 1])) raw[w++] = raw[i];
        else free(raw[i]);
    }
    json_t *out = strset_to_json(raw, w);
    free_strset(raw, w);
    return out;
}

struct json_t *pluginscmd_added(const char *surface, const struct json_t *old_set,
                                const struct json_t *new_set) {
    size_t no = 0, nn = 0;
    char **oldv = strset_from(old_set, &no);
    char **newv = strset_from(new_set, &nn);
    char **res = (char **)calloc(nn ? nn : 1, sizeof(char *));
    size_t nr = 0;
    if (surface && !strcmp(surface, "desktop")) {
        if (no == 0) for (size_t i = 0; i < nn; ++i) res[nr++] = dupstr(newv[i]);
    } else {
        for (size_t i = 0; i < nn; ++i)
            if (!in_strset(oldv, no, newv[i])) res[nr++] = dupstr(newv[i]);
    }
    json_t *out = strset_to_json(res, nr);
    free_strset(res, nr);
    free_strset(oldv, no);
    free_strset(newv, nn);
    return out;
}

struct json_t *pluginscmd_surface_delta(const struct json_t *old_map, const struct json_t *new_map) {
    static const char *const KEYS[] = {"capabilities", "tools", "hooks",
                                       "python_dependencies", "desktop", "requires_auth", "oauth"};
    json_t *out = json_object();
    for (size_t i = 0; i < 7; ++i) {
        json_t *o = old_map ? json_obj_get(old_map, KEYS[i]) : NULL;
        json_t *n = new_map ? json_obj_get(new_map, KEYS[i]) : NULL;
        json_t *a = pluginscmd_added(KEYS[i], o, n);
        if (json_len(a) > 0) json_set(out, KEYS[i], a);
        else json_free(a);
    }
    return out;
}

static const char *label_for(const char *k) {
    if (!strcmp(k, "capabilities")) return "host capabilities";
    if (!strcmp(k, "tools")) return "tools";
    if (!strcmp(k, "hooks")) return "hooks";
    if (!strcmp(k, "python_dependencies")) return "Python dependencies";
    if (!strcmp(k, "desktop")) return "Desktop UI half";
    if (!strcmp(k, "requires_auth")) return "your sign-in for";
    if (!strcmp(k, "oauth")) return "your OAuth sign-in for";
    return k;
}

struct json_t *pluginscmd_surface_delta_lines(const struct json_t *delta) {
    json_t *out = json_array();
    if (!delta || delta->type != JSON_OBJECT) return out;
    char line[4096];
    for (size_t i = 0; i < json_object_size(delta); ++i) {
        const char *k = json_object_get_key_at(delta, i);
        json_t *v = json_object_get_at(delta, i);
        if (!k || !v) continue;
        size_t used = snprintf(line, sizeof(line), "%s: ", label_for(k));
        for (size_t j = 0; j < json_len(v); ++j) {
            if (j) { if (used + 2 < sizeof(line)) { line[used++] = ','; line[used++] = ' '; } }
            const char *s = jstr(json_get(v, j));
            size_t sl = strlen(s);
            if (used + sl + 1 < sizeof(line)) { memcpy(line + used, s, sl); used += sl; }
        }
        line[used] = '\0';
        json_append(out, json_string(line));
    }
    return out;
}

char *pluginscmd_pin_label(const char *version, const char *sha) {
    if (!sha) sha = "";
    size_t n = strlen(sha);
    char *short_sha = dupn(sha, n < 8 ? n : 8);
    if (version && *version) {
        size_t vl = strlen(version);
        char *out = (char *)malloc(vl + 3 + (n < 8 ? n : 8) + 1);
        if (out) snprintf(out, vl + 3 + (n < 8 ? n : 8) + 1, "%s @ %s", version, short_sha);
        free(short_sha);
        return out;
    }
    return short_sha;
}
