/*
 * ssh_workspace_fs.c — C home for upstream Hermes hermes_cli/ssh_workspace_fs.py.
 *
 * PoP: w15_sshworkspa_85e9cc_error_code @ hermes_cli/ssh_workspace_fs.py:SshWorkspaceFs._error_code
 * PoP: w15_sshworkspa_85e9cc_normalize @ hermes_cli/ssh_workspace_fs.py:SshWorkspaceFs.normalize
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "noc_w15_sshworkspa_85e9cc.h"

#define CAP 8192

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* Python str.strip() for the ASCII whitespace our fixtures use. */
static void strip_range(const char *s, size_t len, char *buf, size_t cap) {
    if (len >= cap) len = cap - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    size_t b = 0, e = len;
    while (b < e && isspace((unsigned char)buf[b])) b++;
    while (e > b && isspace((unsigned char)buf[e - 1])) e--;
    if (b > 0) memmove(buf, buf + b, e - b);
    buf[e - b] = '\0';
}

/* posixpath.normpath, faithful to CPython's Lib/posixpath.py. */
static void posix_normpath(const char *path, char *out, size_t cap) {
    const char *p = path ? path : "";
    int initial = 0;
    if (*p == '/') {
        initial = 1;
        if (p[1] == '/' && p[2] != '/') { initial = 2; p += 2; }
        else { p += 1; }
        while (*p == '/') p++;
    }
    char *comps[512];
    size_t n = 0;
    const char *cur = p;
    while (1) {
        const char *slash = strchr(cur, '/');
        size_t cl = slash ? (size_t)(slash - cur) : strlen(cur);
        char comp[CAP];
        if (cl >= sizeof(comp)) cl = sizeof(comp) - 1;
        memcpy(comp, cur, cl); comp[cl] = '\0';
        if (!(cl == 0 || (cl == 1 && comp[0] == '.'))) {
            if (!(cl == 2 && comp[0] == '.' && comp[1] == '.') ||
                (initial == 0 && n == 0) ||
                (n > 0 && !strcmp(comps[n - 1], ".."))) {
                if (n < 512) comps[n++] = dupstr(comp);
            } else if (n > 0) {
                free(comps[--n]);
            }
        }
        if (!slash) break;
        cur = slash + 1;
    }
    size_t pos = 0;
    for (int i = 0; i < initial; i++) { if (pos + 1 < cap) out[pos++] = '/'; }
    for (size_t i = 0; i < n; i++) {
        if (i > 0) { if (pos + 1 < cap) out[pos++] = '/'; }
        size_t cl = strlen(comps[i]);
        if (pos + cl + 1 > cap) cl = cap - pos - 1;
        memcpy(out + pos, comps[i], cl); pos += cl;
        free(comps[i]);
    }
    out[pos] = '\0';
    if (pos == 0) { out[0] = '.'; out[1] = '\0'; }
}

/* posixpath.join(a, b) for two components. */
static void posix_join(const char *a, const char *b, char *out, size_t cap) {
    if (!a || !a[0]) { snprintf(out, cap, "%s", b ? b : ""); return; }
    if (!b || !b[0]) { snprintf(out, cap, "%s", a); return; }
    if (b[0] == '/') { snprintf(out, cap, "%s", b); return; }
    if (a[strlen(a) - 1] == '/') snprintf(out, cap, "%s%s", a, b);
    else snprintf(out, cap, "%s/%s", a, b);
}

static int starts_with_file_colon(const char *s) {
    char pfx[8];
    size_t n = strlen(s) < 5 ? strlen(s) : 5;
    for (size_t i = 0; i < n; i++) pfx[i] = (char)tolower((unsigned char)s[i]);
    pfx[n] = '\0';
    return strcmp(pfx, "file:") == 0;
}

json_t *w15_sshworkspa_85e9cc_error_code(const char *output, const char *fallback) {
    static const char marker[] = "__HERMES_FS_ERROR__:";
    const char *fb = (fallback && *fallback) ? fallback : "EIO";
    const char *s = output ? output : "";
    while (*s) {
        const char *nl = strchr(s, '\n');
        size_t len = nl ? (size_t)(nl - s) : strlen(s);
        if (len >= sizeof(marker) - 1 && !strncmp(s, marker, sizeof(marker) - 1)) {
            char tail[CAP];
            strip_range(s + sizeof(marker) - 1, len - (sizeof(marker) - 1), tail, sizeof(tail));
            return json_string(tail[0] ? tail : fb);
        }
        if (!nl) break;
        s = nl + 1;
        if (*s == '\r') s++;
    }
    return json_string(fb);
}

json_t *w15_sshworkspa_85e9cc_normalize(const char *path, const char *cwd, const char *remote_home) {
    char raw0[CAP];
    strip_range(path ? path : "", strlen(path ? path : ""), raw0, sizeof(raw0));
    if (!raw0[0]) return json_string("");
    if (starts_with_file_colon(raw0)) return json_string("");

    const char *home = remote_home ? remote_home : "";
    const char *wd = cwd ? cwd : "";
    char raw[CAP];
    if (!strcmp(raw0, "~")) snprintf(raw, sizeof(raw), "%s", home);
    else if (raw0[0] == '~' && raw0[1] == '/') posix_join(home, raw0 + 2, raw, sizeof(raw));
    else if (raw0[0] != '/') posix_join(wd, raw0, raw, sizeof(raw));
    else snprintf(raw, sizeof(raw), "%s", raw0);

    char cur[CAP], next[CAP];
    posix_normpath(raw, cur, sizeof(cur));
    for (;;) {
        strip_range(cur, strlen(cur), next, sizeof(next));
        if (!strcmp(next, cur)) break;
        posix_normpath(next, cur, sizeof(cur));
    }
    return json_string(cur);
}

void hermes_cli_ssh_workspace_fs_port_pending(void) { (void)0; }
