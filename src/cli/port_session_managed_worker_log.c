/*
 * port_session_managed_worker_log.c — Port of
 * gateway/session_managed_worker_log.py::worker_log_path.
 *
 * Mirrors the oracle's LIVE Python 3.12 pathlib on this win32 box. Given the
 * owning profile's home string, return `home / 'logs' / LOG_NAME` normalized to
 * '/' separators when the home is absolute, else NULL (test doubles /
 * drive-relative / single-root inputs).
 *
 * is_absolute (probed against the live interpreter):
 *   - path starting with '\\' (UNC)            -> absolute      e.g. \\server\share
 *   - drive letter + ':' + root separator      -> absolute      e.g. C:\ or C:/
 *   - drive letter + ':' with NO separator     -> NOT absolute  e.g. "C:"
 *   - single leading '/' or '\' with no drive  -> NOT absolute  e.g. /home, \foo
 * Output: every separator normalized to '/', consecutive seps collapsed, then
 * "/logs/managed-worker.log" appended on a single '/'.
 */

#include "port_session_managed_worker_log.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_absolute(const char *p)
{
    /* UNC: two leading backslashes -> absolute (even bare \\ on this build). */
    if (p[0] == '\\' && p[1] == '\\')
        return 1;
    /* drive-rooted: "X:" followed by '/' or '\' (drive alone "C:" is NOT
     * absolute on the live Python 3.12 pathlib). */
    if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        char c = p[2];
        if (c == '\\' || c == '/')
            return 1;
    }
    return 0;
}

/* PoP: worker_log_path @ gateway/session_managed_worker_log.py:worker_log_path */
char *smwl_worker_log_path(const char *profile)
{
    if (!profile || !*profile || !is_absolute(profile))
        return NULL;

    static const char SUF[] = "/logs/" SMWL_LOG_NAME;
    size_t cap = strlen(profile) + sizeof(SUF) + 1 + 16;
    if (cap > (1u << 20))
        return NULL;
    char *out = malloc(cap);
    if (!out)
        return NULL;

    size_t n = 0;
    for (const char *q = profile; *q; q++) {
        if (*q == '\\' || *q == '/') {
            /* count the run of separators */
            int nseps = 1;
            while (q[nseps] == '\\' || q[nseps] == '/')
                nseps++;
            if (n == 0) {
                /* leading separators: preserve the UNC pair '//...', else one '/'. */
                int emit = nseps >= 2 ? 2 : 1;
                if (emit > 1 && n + 2 < cap) {
                    out[n++] = '/';
                    out[n++] = '/';
                } else if (emit == 1 && n + 1 < cap) {
                    out[n++] = '/';
                }
            } else if (out[n - 1] != '/') {
                out[n++] = '/';
            }
            q += nseps - 1;
        } else {
            if (n + 1 < cap)
                out[n++] = *q;
        }
    }
    /* strip a trailing '/' before appending the suffix */
    if (n > 0 && out[n - 1] == '/')
        n--;
    memcpy(out + n, SUF, sizeof(SUF)); /* includes NUL */
    return out;
}
