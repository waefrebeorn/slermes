/*
 * main_web_build.c — C home for upstream Hermes hermes_cli/main_web_build.py.
 *
 * PoP: w12_mainwebbu_a76b78_web_project_root @ hermes_cli/main_web_build.py:_web_project_root
 * PoP: w12_mainwebbu_a76b78_web_dist_dir @ hermes_cli/main_web_build.py:_web_dist_dir
 *
 * Faithful pathlib.Path semantics for the Windows host (WindowsPath: '/' normalized
 * to '\', duplicate separators collapsed, trailing separators stripped, '.' for empty).
 */
#include "noc_w12_mainwebbu_a76b78.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_drive_root(const char *s) {
    return s && strlen(s) == 3 && s[1] == ':' && s[2] == '\\';
}

/* Normalize like pathlib.WindowsPath stringification. Caller frees. */
static char *norm_path(const char *p) {
    if (!p || !*p) return strdup(".");
    size_t n = strlen(p);
    char *tmp = (char *)malloc(n + 2);
    if (!tmp) return strdup(".");
    size_t m = 0;
    int unc = (n >= 2 && p[0] == '/' && p[1] == '/');
    for (size_t i = 0; i < n; ++i) {
        char c = p[i] == '/' ? '\\' : p[i];
        if (c == '\\' && m > 0 && tmp[m - 1] == '\\') {
            if (!(unc && m == 2)) continue; /* collapse repeated separators, keep UNC head */
        }
        tmp[m++] = c;
    }
    tmp[m] = '\0';
    /* strip trailing separators unless the path is a root */
    while (m > 1 && tmp[m - 1] == '\\' && !is_drive_root(tmp)) tmp[--m] = '\0';
    if (m == 0) { free(tmp); return strdup("."); }
    char *out = (char *)malloc(m + 1);
    if (!out) { free(tmp); return strdup("."); }
    memcpy(out, tmp, m); out[m] = '\0';
    free(tmp);
    return out;
}

/* WindowsPath.parent. Caller frees. */
static char *path_parent(const char *raw) {
    char *p = norm_path(raw);
    if (!strcmp(p, ".")) return p;
    size_t n = strlen(p);
    size_t idx = (size_t)-1;
    for (size_t i = 0; i < n; ++i) if (p[i] == '\\') idx = i;
    if (idx == (size_t)-1) { free(p); return strdup("."); }
    if (idx == 0) { free(p); return strdup("\\"); }
    if (idx == 2 && p[1] == ':') {
        char *out = (char *)malloc(4);
        out[0] = p[0]; out[1] = ':'; out[2] = '\\'; out[3] = '\0';
        free(p); return out;
    }
    char *out = (char *)malloc(idx + 1);
    memcpy(out, p, idx); out[idx] = '\0';
    free(p);
    return out;
}

/* WindowsPath.name. Caller frees. */
static char *path_name(const char *raw) {
    char *p = norm_path(raw);
    if (!strcmp(p, ".") || !strcmp(p, "\\") || is_drive_root(p)) return p;
    size_t n = strlen(p);
    size_t idx = (size_t)-1;
    for (size_t i = 0; i < n; ++i) if (p[i] == '\\') idx = i;
    char *out = strdup(idx == (size_t)-1 ? p : p + idx + 1);
    free(p);
    return out;
}

/* Path / component. Caller frees. */
static char *path_join(const char *a, const char *b) {
    if (!strcmp(a, ".")) return strdup(b);
    size_t la = strlen(a), lb = strlen(b);
    char *out = (char *)malloc(la + lb + 2);
    if (!out) return strdup(b);
    memcpy(out, a, la);
    size_t n = la;
    if (la == 0 || a[la - 1] != '\\') out[n++] = '\\';
    memcpy(out + n, b, lb + 1);
    return out;
}

char *w12_mainwebbu_a76b78_web_project_root(const char *web_dir) {
    char *par = path_parent(web_dir);
    char *nm = path_name(par);
    int is_apps = !strcmp(nm, "apps");
    free(nm);
    if (!is_apps) return par;
    char *gp = path_parent(par);
    free(par);
    return gp;
}

char *w12_mainwebbu_a76b78_web_dist_dir(const char *web_dir) {
    char *root = w12_mainwebbu_a76b78_web_project_root(web_dir);
    char *j1 = path_join(root, "hermes_cli");
    char *j2 = path_join(j1, "web_dist");
    free(root);
    free(j1);
    return j2;
}
