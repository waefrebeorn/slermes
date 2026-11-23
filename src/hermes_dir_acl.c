/*
 * hermes_dir_acl.c — real C11 port of the pure leaves of hermes_dir_acl.py.
 *
 * PoP: hermesdira_strip_win32_prefix  @ hermes_dir_acl.py:_strip_win32_prefix
 * PoP: hermesdira_unusable_dir_remedy @ hermes_dir_acl.py:unusable_dir_remedy
 * PoP: hermesdira_constants           @ hermes_dir_acl.py:TOKEN_ELEVATION_TYPE_DEFAULT
 *
 * The ACL-writing code is inherently OS-handle driven (pywin32/icacls, live
 * token probes) and cannot be exercised by a JSON leaf oracle. These three
 * exports are the deterministic string/constant leaves of the module.
 */
#include "noc_w5_hermesdira_b128e3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *hd_dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/*
 * PoP: hermesdira_strip_win32_prefix @ hermes_dir_acl.py:_strip_win32_prefix
 */
char *hermesdira_strip_win32_prefix(const char *path) {
    const char *s = path ? path : "";
    /* "\\\\?\\UNC\\" is 8 bytes; the generic extended prefix is 4 bytes. */
    if (!strncmp(s, "\\\\?\\UNC\\", 8)) {
        size_t rest = strlen(s + 8);
        char *out = (char *)malloc(2 + rest + 1);
        if (!out) return NULL;
        out[0] = '\\';
        out[1] = '\\';
        memcpy(out + 2, s + 8, rest + 1);
        return out;
    }
    if (!strncmp(s, "\\\\?\\", 4)) return hd_dup_n(s + 4, strlen(s + 4));
    return hd_dup_n(s, strlen(s));
}

/*
 * PoP: hermesdira_unusable_dir_remedy @ hermes_dir_acl.py:unusable_dir_remedy
 */
char *hermesdira_unusable_dir_remedy(const char *path, int windows) {
    const char *p = path ? path : "";
    const char *fmt = windows
        ? "from an elevated prompt run: icacls \"%s\" /reset /t"
        : "run: sudo chown -R \"$(id -un)\" \"%s\"";
    int n = snprintf(NULL, 0, fmt, p);
    char *out;
    if (n < 0) return NULL;
    out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    snprintf(out, (size_t)n + 1, fmt, p);
    return out;
}

/*
 * PoP: hermesdira_constants @ hermes_dir_acl.py:TOKEN_ELEVATION_TYPE_DEFAULT
 */
json_t *hermesdira_constants(void) {
    json_t *o = json_object();
    json_set(o, "TOKEN_ELEVATION_TYPE_DEFAULT", json_number(1));
    json_set(o, "FILE_ATTRIBUTE_DIRECTORY", json_number(16));
    json_set(o, "FILE_ATTRIBUTE_REPARSE_POINT", json_number(1024));
    return o;
}
