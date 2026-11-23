/*
 * file_tools_paths.c - C home for upstream Hermes tools/file_tools_paths.py.
 *
 * PoP: w9_filetoolsp_5a4ce8_normalize_posix @ tools/file_tools_paths.py:_normalize_without_host_deref
 * PoP: w9_filetoolsp_5a4ce8_ssh_escapes_home @ tools/file_tools_paths.py:_ssh_path_escapes_home
 * PoP: w9_filetoolsp_5a4ce8_sentinel @ tools/file_tools_paths.py:_TERMINAL_CWD_SENTINELS
 * PoP: w9_filetoolsp_5a4ce8_expand_tilde @ tools/file_tools_paths.py:_expand_tilde
 */
#include "noc_w9_filetoolsp_5a4ce8.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Faithful port of posixpath.normpath (PurePosixPath(str) is the same). */
char *w9_filetoolsp_5a4ce8_normalize_posix(const char *path) {
    if (!path || !*path) return dupstr(".");
    int initial = 0;
    if (path[0] == '/') {
        if (path[1] == '/' && path[2] != '/') initial = 2;
        else initial = 1;
    }
    char *buf = dupstr(path);
    size_t cap = strlen(buf) + 2;
    char **parts = (char **)malloc(cap * sizeof(char *));
    char **out = (char **)malloc(cap * sizeof(char *));
    size_t n = 0, m = 0;
    char *cur = buf;
    while (1) {
        char *slash = strchr(cur, '/');
        if (slash) { *slash = '\0'; parts[n++] = cur; cur = slash + 1; }
        else { parts[n++] = cur; break; }
    }
    for (size_t i = 0; i < n; ++i) {
        const char *cc = parts[i];
        if (!*cc || !strcmp(cc, ".")) continue;
        int append = strcmp(cc, "..") != 0 || (!initial && m == 0) ||
                     (m > 0 && !strcmp(out[m - 1], ".."));
        if (append) out[m++] = (char *)cc;
        else if (m > 0) m--;
    }
    size_t len = (size_t)initial;
    for (size_t i = 0; i < m; ++i) len += strlen(out[i]) + (i ? 1 : 0);
    char *res = (char *)malloc(len + 2);
    size_t j = 0;
    for (int i = 0; i < initial; ++i) res[j++] = '/';
    for (size_t i = 0; i < m; ++i) {
        if (i) res[j++] = '/';
        size_t l = strlen(out[i]);
        memcpy(res + j, out[i], l);
        j += l;
    }
    res[j] = '\0';
    if (j == 0) { res[0] = '.'; res[1] = '\0'; }
    free(parts); free(out); free(buf);
    return res;
}

int w9_filetoolsp_5a4ce8_ssh_escapes_home(const char *resolved) {
    if (!resolved || resolved[0] != '~') return 0;
    const char *slash = strchr(resolved, '/');
    const char *tail = slash ? slash + 1 : "";
    if (!strcmp(tail, "..")) return 1;
    return strncmp(tail, "../", 3) == 0;
}

int w9_filetoolsp_5a4ce8_sentinel(const char *value) {
    if (!value) return 1;
    /* Python (value or "").strip().lower() */
    const unsigned char *a = (const unsigned char *)value;
    while (*a && (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\v' || *a == '\f')) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' ||
                     b[-1] == '\r' || b[-1] == '\v' || b[-1] == '\f')) b--;
    size_t n = (size_t)(b - a);
    char buf[64];
    if (n >= sizeof(buf)) return 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = a[i];
        buf[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    buf[n] = '\0';
    return !*buf || !strcmp(buf, ".") || !strcmp(buf, "./") ||
           !strcmp(buf, "auto") || !strcmp(buf, "cwd");
}

char *w9_filetoolsp_5a4ce8_expand_tilde(const char *path, const char *home) {
    if (!path || !*path || !strchr(path, '~')) return dupstr(path ? path : "");
    if (home && *home && (!strcmp(path, "~") || (path[0] == '~' && path[1] == '/'))) {
        if (!strcmp(path, "~")) return dupstr(home);
        const char *rest = path + 2;
        size_t n = strlen(home) + 1 + strlen(rest) + 1;
        char *res = (char *)malloc(n);
        if (res) snprintf(res, n, "%s/%s", home, rest);
        return res;
    }
    /* os.path.expanduser identity for the injected (no-home) fixtures. */
    return dupstr(path);
}
