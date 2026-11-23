/*
 * uv_cache_health.c — real C11 port of upstream Hermes pm/uv_cache_health.py.
 *
 * PoP: uvcachehea_normalize @ pm/uv_cache_health.py:normalize
 * PoP: uvcachehea_valid @ pm/uv_cache_health.py:_valid
 * PoP: uvcachehea_failed @ pm/uv_cache_health.py:failed_cached_packages
 * PoP: uvcachehea_dist_name @ pm/uv_cache_health.py:_dist_name
 *
 * Only the pure text-parsing core is ported; the filesystem sweep and lock
 * handling (_stream_damaged/_damaged/corrupt_cache_entries/remove_corrupt_entries)
 * are deliberately out of scope for this differential gate.
 */
#include "noc_w6_uvcachehea_d7f380.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CORRUPT1 "the wheel is invalid"
#define CORRUPT2 "`record` file is invalid"

static char *lower_ascii(const char *s) {
    size_t n = strlen(s ? s : ""), i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static int ci_prefix(const char *s, const char *p) {
    while (*p) {
        if (!s || !*s) return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*p)) return 0;
        s++; p++;
    }
    return 1;
}

static const char *ci_find(const char *from, const char *needle) {
    const char *p = from;
    while (p && *p) {
        if (ci_prefix(p, needle)) return p;
        p++;
    }
    return NULL;
}

static const char *ci_find_range(const char *from, const char *end, const char *needle) {
    const char *p = from;
    size_t n = strlen(needle);
    while (p && p + n <= end) {
        if (ci_prefix(p, needle)) return p;
        p++;
    }
    return NULL;
}

/* re.sub(r"[-_.]+", "-", name).lower() */
static char *normalize_s(const char *name) {
    const char *p = name ? name : "";
    size_t cap = strlen(p) * 2 + 2, n = 0;
    char *out = (char *)malloc(cap);
    int sep = 0;
    if (!out) return NULL;
    for (; *p; p++) {
        if (*p == '-' || *p == '_' || *p == '.') { sep = 1; continue; }
        if (sep) { out[n++] = '-'; sep = 0; }
        out[n++] = (char)tolower((unsigned char)*p);
    }
    if (sep) out[n++] = '-';
    out[n] = '\0';
    return out;
}

/* _NAME = [A-Za-z0-9](?:[A-Za-z0-9._-]*[A-Za-z0-9])?  (fullmatch) */
static int valid_name_s(const char *name) {
    size_t n;
    if (!name || !*name) return 0;
    n = strlen(name);
    if (!isalnum((unsigned char)name[0]) || !isalnum((unsigned char)name[n-1])) return 0;
    for (size_t i = 1; i + 1 < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* archive-v0[\\/][^\s`'"]+.*\(os error [23]\)  — re.IGNORECASE, no DOTALL. */
static int missing_archive(const char *text) {
    const char *line = text ? text : "";
    while (*line) {
        const char *nl = strchr(line, '\n');
        const char *end = nl ? nl : line + strlen(line);
        const char *p = line;
        while (p + 10 <= end) {
            const char *q, *start, *found;
            if (!ci_prefix(p, "archive-v0")) { p++; continue; }
            q = p + 10;
            if (q >= end || (*q != '\\' && *q != '/')) { p++; continue; }
            q++;
            start = q;
            while (q < end && !isspace((unsigned char)*q) && *q != '`' && *q != '\'' && *q != '"') q++;
            if (q == start) { p++; continue; }
            found = ci_find_range(q, end, "(os error 2)");
            if (!found) found = ci_find_range(q, end, "(os error 3)");
            if (found) return 1;
            p++;
        }
        if (!nl) break;
        line = nl + 1;
    }
    return 0;
}

static void append_unique(json_t *arr, const char *s) {
    size_t i;
    for (i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return;
    }
    json_append(arr, json_string(s));
}

static int cmp_str(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

json_t *uvcachehea_normalize(const char *name) {
    char *s = normalize_s(name);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

json_t *uvcachehea_valid(const char *name) {
    char *s;
    json_t *r;
    if (!valid_name_s(name)) return json_null();
    s = normalize_s(name);
    r = json_string(s ? s : "");
    free(s);
    return r;
}

json_t *uvcachehea_failed(const char *output) {
    const char *text = output ? output : "";
    char *lowered;
    json_t *names = json_array();
    const char *p, *needle = "failed to install: ";
    size_t nlen = strlen(needle);

    lowered = lower_ascii(text);
    if (!lowered) return names;
    if (!strstr(lowered, CORRUPT1) && !strstr(lowered, CORRUPT2) && !missing_archive(text)) {
        free(lowered);
        return names;
    }
    free(lowered);

    p = text;
    while ((p = ci_find(p, needle)) != NULL) {
        const char *start = p + nlen;
        const char *q = start, *found = NULL;
        while (*q && !isspace((unsigned char)*q) && *q != '(' && *q != ')') {
            if (q[0] == '.' && tolower((unsigned char)q[1]) == 'w' &&
                tolower((unsigned char)q[2]) == 'h' && tolower((unsigned char)q[3]) == 'l') {
                char next = q[4];
                if (!(isalnum((unsigned char)next) || next == '_')) { found = q + 4; break; }
            }
            q++;
        }
        if (found) {
            size_t glen = (size_t)(found - start);
            if (glen && glen < 1024) {
                char group[1024], proj[1024];
                char *dash;
                memcpy(group, start, glen);
                group[glen] = '\0';
                dash = strchr(group, '-');
                if (dash) {
                    size_t plen = (size_t)(dash - group);
                    memcpy(proj, group, plen);
                    proj[plen] = '\0';
                } else {
                    strcpy(proj, group);
                }
                if (valid_name_s(proj)) {
                    char *nm = normalize_s(proj);
                    append_unique(names, nm ? nm : "");
                    free(nm);
                }
            }
            p = found;
        } else {
            p = start;
        }
    }
    /* Python returns a set; make the order deterministic for the oracle. */
    if (json_len(names) > 1) {
        const char **tmp = (const char **)malloc(json_len(names) * sizeof(char *));
        size_t i;
        if (tmp) {
            for (i = 0; i < json_len(names); i++) tmp[i] = json_get(names, i)->str_val;
            qsort(tmp, json_len(names), sizeof(char *), cmp_str);
            for (i = 0; i < json_len(names); i++) {
                json_t *v = json_get(names, i);
                if (v) { free(v->str_val); v->str_val = strdup(tmp[i]); }
            }
            free(tmp);
        }
    }
    return names;
}

json_t *uvcachehea_dist_name(const char *info) {
    static const char suffix[] = ".dist-info";
    size_t sl = sizeof(suffix) - 1, n, stem, i;
    int found = 0;
    char name[1024];
    char *nm;
    json_t *r;
    if (!info) return json_null();
    n = strlen(info);
    if (n <= sl || strcmp(info + n - sl, suffix)) return json_null();
    stem = n - sl;
    i = stem;
    while (i > 0) { i--; if (info[i] == '-') { found = 1; break; } }
    if (!found || i == 0 || i + 1 >= stem || i >= sizeof(name)) return json_null();
    memcpy(name, info, i);
    name[i] = '\0';
    if (!valid_name_s(name)) return json_null();
    nm = normalize_s(name);
    r = json_string(nm ? nm : "");
    free(nm);
    return r;
}
