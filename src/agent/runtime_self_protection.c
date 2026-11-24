/*
 * runtime_self_protection.c — real C11 port of selected pure helpers of
 * upstream Hermes agent/runtime_self_protection.py.
 *
 * PoP: w17_runtimesel_1fc3a5_version_spec_matches @ agent/runtime_self_protection.py:_version_spec_matches
 * PoP: w17_runtimesel_1fc3a5_overlaps @ agent/runtime_self_protection.py:_overlaps
 * PoP: w17_runtimesel_1fc3a5_strip_prefixes @ agent/runtime_self_protection.py:_strip_prefixes
 */
#include "noc_w17_runtimesel_1fc3a5.h"
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v' || (c >= 0x1C && c <= 0x1F);
}

static char *strip_lower(const char *s, size_t *out_len) {
    const char *a, *b;
    char *out;
    size_t i, n;
    if (!s) s = "";
    a = s;
    while (*a && ascii_ws((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && ascii_ws((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) { *out_len = 0; return NULL; }
    for (i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)a[i]);
    out[n] = 0;
    *out_len = n;
    return out;
}

int w17_runtimesel_1fc3a5_version_spec_matches(const char *spec, const char *version,
                                               const char *uv_root_name) {
    char *s;
    size_t sl;
    int result = 0;
    if (!version) version = "";
    if (!uv_root_name) uv_root_name = "";
    s = strip_lower(spec, &sl);
    if (!s) return 0;
    if (sl == 0) { free(s); return 0; }
    if (!strcmp(s, "--all") || !strcmp(s, "-a") || !strcmp(s, "all")) {
        result = 1;
    } else if (s[0] == '-') {
        result = 0;
    } else {
        /* uv_root_name.lower().startswith(spec) */
        size_t rl = strlen(uv_root_name), i;
        int root_prefix = 1;
        if (sl > rl) root_prefix = 0;
        else for (i = 0; i < sl; i++) {
            if ((char)tolower((unsigned char)uv_root_name[i]) != s[i]) { root_prefix = 0; break; }
        }
        if (root_prefix) {
            result = 1;
        } else {
            size_t vl = strlen(version);
            if (!strcmp(s, version)) result = 1;
            else if (sl < vl && !strncmp(version, s, sl) && version[sl] == '.') result = 1;
        }
    }
    free(s);
    return result;
}

int w17_runtimesel_1fc3a5_overlaps(const char *a, const char *b) {
    size_t la, lb;
    if (!a || !*a || !b || !*b) return 0;
    if (!strcmp(a, b)) return 1;
    la = strlen(a); lb = strlen(b);
    /* os.sep is '\\' on this (nt) platform. */
    if (la > lb && a[lb] == '\\' && !strncmp(a, b, lb)) return 1;
    if (lb > la && b[la] == '\\' && !strncmp(b, a, la)) return 1;
    return 0;
}

static int command_prefix(const char *w) {
    static const char *const pfx[] = {"sudo", "command", "nohup", "exec",
                                      "env", "nice", "time", NULL};
    int i;
    for (i = 0; pfx[i]; i++) if (!strcmp(w, pfx[i])) return 1;
    return 0;
}

/* re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", first) and not startswith('-','/') */
static int env_assignment(const char *w) {
    const char *p;
    if (!w || !*w || w[0] == '-' || w[0] == '/') return 0;
    if (!(isalpha((unsigned char)w[0]) || w[0] == '_')) return 0;
    p = w + 1;
    while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
    return *p == '=' ? 1 : 0;
}

json_t *w17_runtimesel_1fc3a5_strip_prefixes(json_t *words) {
    json_t *out = json_array();
    size_t i, n = 0, k;
    if (words && words->type == JSON_ARRAY) n = words->c.count;
    i = 0;
    while (i < n) {
        json_t *w = words->c.items[i];
        const char *s = (w && w->type == JSON_STRING) ? w->str_val : NULL;
        if (!s) break;
        if (command_prefix(s)) { i++; continue; }
        if (env_assignment(s)) { i++; continue; }
        break;
    }
    for (k = i; k < n; k++) json_append(out, json_copy(words->c.items[k]));
    return out;
}
