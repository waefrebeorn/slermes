/*
 * plugins_cmd_inspect.c -- faithful C11 port of the deterministic
 * _desktop_folder_name tail in hermes_cli/plugins_cmd_inspect.py
 * (wave18 gate w18_pluginscmd_58cffc).
 *
 * PoP: w18_pluginscmd_58cffc_desktop_folder_name @ hermes_cli/plugins_cmd_inspect.py:_desktop_folder_name
 * PoP: w18_pluginscmd_58cffc_repo_name_from_url @ hermes_cli/plugins_cmd.py:_repo_name_from_url
 *
 * The clone / catalog / manifest-reading halves stay in Python. Only the
 * pure string manipulation is ported. Both helpers return malloc'd strings;
 * the caller owns them.
 */
#include "noc_w18_pluginscmd_58cffc.h"

#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

char *w18_pluginscmd_58cffc_repo_name_from_url(const char *url)
{
    const char *s = url ? url : "";
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;           /* url.rstrip("/") */
    char *buf = (char *)malloc(n + 1);
    if (!buf) return NULL;
    memcpy(buf, s, n);
    buf[n] = '\0';
    if (n >= 4 && strcmp(buf + n - 4, ".git") == 0) {
        buf[n - 4] = '\0';                          /* removesuffix(".git") */
        n -= 4;
    }
    const char *slash = strrchr(buf, '/');          /* rsplit("/",1)[-1] */
    const char *name = slash ? slash + 1 : buf;
    char *res = dupstr(name);
    free(buf);
    if (!res) return NULL;
    if (strchr(res, ':')) {
        char *colon = strrchr(res, ':');            /* rsplit(":",1)[-1] */
        const char *after = colon + 1;
        const char *sl = strrchr(after, '/');       /* rsplit("/",1)[-1] */
        const char *final = sl ? sl + 1 : after;
        char *out = dupstr(final);
        free(res);                                 /* never free an interior pointer */
        return out;
    }
    return res;
}

char *w18_pluginscmd_58cffc_desktop_folder_name(const char *git_url, const char *subdir)
{
    const char *s = subdir ? subdir : "";           /* (subdir or "") */
    size_t n = strlen(s);
    char *norm = (char *)malloc(n + 1);
    if (!norm) return NULL;
    for (size_t i = 0; i < n; i++)
        norm[i] = (s[i] == '\\') ? '/' : s[i];     /* Path str() on Windows emits '\' */
    norm[n] = '\0';

    char *last = NULL;
    char *p = norm;
    while (*p) {
        char *start = p;
        while (*p && *p != '/') p++;
        size_t plen = (size_t)(p - start);
        int skip = (plen == 0)
                || (plen == 1 && start[0] == '.')
                || (plen == 7 && memcmp(start, "desktop", 7) == 0);
        if (!skip) {
            char *part = (char *)malloc(plen + 1);
            if (part) {
                memcpy(part, start, plen);
                part[plen] = '\0';
                free(last);                        /* keep only the last real segment */
                last = part;
            }
        }
        if (*p == '/') p++;
    }
    free(norm);
    if (last) return last;
    return w18_pluginscmd_58cffc_repo_name_from_url(git_url);
}
