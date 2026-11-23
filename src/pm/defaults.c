/*
 * defaults.c — C port of upstream Hermes pm/defaults.py.
 *
 * PoP: w5defaults_canonical         @ pm/defaults.py:canonical_default_name
 * PoP: w5defaults_declined_from_json @ pm/defaults.py:declined (core logic)
 * PoP: w5defaults_default_packages   @ pm/defaults.py:default_packages
 *
 * RETIRED_DEFAULTS maps the retired "agent-browser" opt-out onto "chromium".
 * Filesystem/registry access is injected by the gate (text and pkg metadata).
 */
#include "noc_w5_defaults_7e72be.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *w5defaults_canonical(const char *name, char *out, size_t cap) {
    const char *src = name ? name : "";
    if (!strcmp(src, "agent-browser")) src = "chromium";
    if (out && cap) snprintf(out, cap, "%s", src);
    return out ? out : src;
}

static int contains(const char **arr, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (arr[i] && !strcmp(arr[i], s)) return 1;
    return 0;
}
static int cmpstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int w5defaults_declined_from_json(const char *text, char ***out, size_t *outn) {
    if (out) *out = NULL;
    if (outn) *outn = 0;
    if (!text) return 0;
    json_t *doc = json_parse(text, NULL);
    if (!doc) return 0;
    json_t *names = NULL;
    if (doc->type == JSON_OBJECT) names = json_obj_get(doc, "declined");
    char **res = NULL;
    size_t n = 0, cap = 0;
    if (names) {
        if (names->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(names); i++) {
                json_t *e = json_get(names, i);
                if (!e || e->type != JSON_STRING) continue; /* isinstance(name, str) */
                char buf[512], can[512];
                snprintf(buf, sizeof(buf), "%s", e->str_val ? e->str_val : "");
                w5defaults_canonical(buf, can, sizeof(can));
                if (contains((const char **)res, n, can)) continue;
                if (n == cap) {
                    cap = cap ? cap * 2 : 8;
                    char **tmp = (char **)realloc(res, cap * sizeof(char *));
                    if (!tmp) break;
                    res = tmp;
                }
                res[n++] = strdup(can);
            }
        } else if (names->type == JSON_STRING) {
            /* Python iterates a bare string character-by-character. */
            const char *s = names->str_val ? names->str_val : "";
            for (const char *p = s; *p; p++) {
                char can[2] = {*p, '\0'};
                if (contains((const char **)res, n, can)) continue;
                if (n == cap) {
                    cap = cap ? cap * 2 : 8;
                    char **tmp = (char **)realloc(res, cap * sizeof(char *));
                    if (!tmp) break;
                    res = tmp;
                }
                res[n++] = strdup(can);
            }
        }
    }
    json_free(doc);
    if (n > 1) qsort(res, n, sizeof(char *), cmpstr);
    if (out) *out = res;
    if (outn) *outn = n;
    return (int)n;
}

int w5defaults_default_packages(const char **names, size_t nnames,
                                const w5defaults_pkg_t *pkgs, size_t npkgs,
                                const char **declined, size_t ndeclined,
                                char ***out, size_t *outn) {
    char **res = (char **)calloc(nnames ? nnames : 1, sizeof(char *));
    size_t k = 0;
    for (size_t i = 0; i < nnames; i++) {
        const char *name = names[i];
        const w5defaults_pkg_t *p = NULL;
        for (size_t j = 0; j < npkgs; j++)
            if (pkgs[j].found && !strcmp(pkgs[j].name, name)) { p = &pkgs[j]; break; }
        if (!p) continue; /* KeyError -> skip */
        if (p->is_default && !p->is_internal && !p->missing &&
            !contains(declined, ndeclined, name))
            res[k++] = strdup(name);
    }
    if (out) *out = res;
    if (outn) *outn = k;
    return (int)k;
}

void w5defaults_free_list(char **arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) free(arr[i]);
    free(arr);
}
