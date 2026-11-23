/*
 * install.c — C port of upstream Hermes pm/install.py (gate w5_install_184801).
 *
 * PoP: w5_install_184801_extra_key        @ pm/install.py:_extra_key
 * PoP: w5_install_184801_identity         @ pm/install.py:_identity
 * PoP: w5_install_184801_dependents_first @ pm/install.py:_dependents_first
 *
 * _dependents_first moves each package ahead of the packages it depends on so
 * an extending package wins on PATH. The full install/venv transaction is I/O
 * bound and intentionally not ported.
 */
#include "noc_w5_install_184801.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupn(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static char *dupstr(const char *s) { return dupn(s, s ? strlen(s) : 0); }

void w5_install_184801_free(char *s) { free(s); }
void w5_install_184801_strings_free(char **arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) free(arr[i]);
    free(arr);
}

/* PEP 685 key: re.sub(r"[-_.]+", "-", name).lower() */
char *w5_install_184801_extra_key(const char *name) {
    const char *s = name ? name : "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 2);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; ) {
        if (s[i] == '-' || s[i] == '_' || s[i] == '.') {
            while (i < n && (s[i] == '-' || s[i] == '_' || s[i] == '.')) i++;
            out[o++] = '-';
        } else {
            unsigned char c = (unsigned char)s[i++];
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return out;
}

json_t *w5_install_184801_identity(const char *target,
                                   const char *const *sha256s, size_t n) {
    if (!n) return json_null();
    json_t *hashes = json_array();
    for (size_t i = 0; i < n; i++)
        json_append(hashes, json_string(sha256s[i] ? sha256s[i] : ""));
    json_t *out = json_array();
    json_append(out, json_string(target ? target : ""));
    json_append(out, hashes);
    return out;
}

static int index_of(const char *const *arr, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (arr[i] && !strcmp(arr[i], s)) return (int)i;
    return -1;
}

char **w5_install_184801_dependents_first(const char *const *names, size_t n_names,
                                          const char *const *const *deps,
                                          size_t *out_n) {
    char **order = (char **)calloc(n_names ? n_names : 1, sizeof(char *));
    size_t n = 0;
    if (!order) { if (out_n) *out_n = 0; return NULL; }
    for (size_t i = 0; i < n_names; i++) order[n++] = dupstr(names[i] ? names[i] : "");

    for (size_t i = 0; i < n_names; i++) {
        const char *name = names[i] ? names[i] : "";
        int minpos = -1;
        const char *const *d = deps ? deps[i] : NULL;
        if (d) {
            for (size_t k = 0; d[k]; k++) {
                const char *dep = d[k];
                if (!strcmp(dep, name)) continue;               /* set minus {name} */
                int pos = index_of((const char *const *)order, n, dep);
                if (pos >= 0 && (minpos < 0 || pos < minpos)) minpos = pos;
            }
        }
        int here = index_of((const char *const *)order, n, name);
        if (minpos >= 0 && here >= 0 && minpos < here) {
            char *pick = order[here];
            for (int j = here; j > minpos; j--) order[j] = order[j - 1];
            order[minpos] = pick;
        }
    }
    if (out_n) *out_n = n;
    return order;
}
