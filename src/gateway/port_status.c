/* port_status.c -- pure helpers ported from gateway/status.py.
 *
 * PoP: st_hermes_home_assignments     @ gateway/status.py:hermes_home_assignments
 * PoP: st_command_line_names_home     @ gateway/status.py:command_line_names_hermes_home
 * PoP: st_profile_name_for_home       @ gateway/status.py:_profile_name_for_home
 *
 * The rest of the module routes through _gateway_command_subcommand, which
 * pulls in command_line_runs_inline_source / inline_bootstrap_argv; that
 * parser is a separate unit.
 */
#include "port_status.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ── hermes_home_assignments ──────────────────────────────────────────────
 * Token-bounded HERMES_HOME=<value> extraction. Values are quote-stripped and
 * have any trailing separator removed. The name must be at the start or after
 * whitespace, so FOO=hermes_home=/x is not an assignment.
 * Writes up to `maxn` values into `out`; returns the count found.
 */
static int
scan_assignment(const char *s, size_t *consumed, char *val, size_t vcap)
{
    const char *p = s;
    if (*p == '"') {
        p++;
        size_t w = 0;
        while (*p && *p != '"') {
            if (w + 1 < vcap)
                val[w++] = *p;
            p++;
        }
        val[w] = '\0';
        if (*p == '"')
            p++;
    } else if (*p == '\'') {
        p++;
        size_t w = 0;
        while (*p && *p != '\'') {
            if (w + 1 < vcap)
                val[w++] = *p;
            p++;
        }
        val[w] = '\0';
        if (*p == '\'')
            p++;
    } else {
        size_t w = 0;
        while (*p && !isspace((unsigned char)*p)) {
            if (w + 1 < vcap)
                val[w++] = *p;
            p++;
        }
        val[w] = '\0';
    }
    /* trailing separator, matching .rstrip("/") */
    size_t n = strlen(val);
    while (n && val[n - 1] == '/')
        val[--n] = '\0';
    *consumed = (size_t)(p - s);
    return 1;
}

int
st_hermes_home_assignments(const char *command, char **out, int maxn)
{
    if (!command)
        return 0;
    static const char needle[] = "hermes_home=";
    int n = 0;
    const char *p = command;
    while ((p = strstr(p, needle)) != NULL) {
        int at_token_start = (p == command) || isspace((unsigned char)p[-1]);
        const char *val = p + sizeof(needle) - 1;
        if (at_token_start && *val && n < maxn) {
            char buf[1024];
            size_t used = 0;
            scan_assignment(val, &used, buf, sizeof(buf));
            out[n] = strdup(buf);
            if (!out[n])
                break;
            n++;
        }
        p = val;
    }
    return n;
}

/* ── command_line_names_hermes_home ───────────────────────────────────────
 * Literal, token-bounded match: recovers an unquoted value containing spaces
 * (HERMES_HOME=C:/Users/John Doe/.hermes) that the token parser would cut.
 */
int
st_command_line_names_home(const char *command_lc, const char *home_lc)
{
    if (!command_lc || !home_lc)
        return 0;

    char *vals[64];
    int n = st_hermes_home_assignments(command_lc, vals, 64);
    for (int i = 0; i < n; i++) {
        int hit = strcmp(vals[i], home_lc) == 0;
        free(vals[i]);
        if (hit)
            return 1;
    }

    /* literal scan: hermes_home=<home>/? followed by whitespace or end */
    size_t hl = strlen(home_lc);
    const char needle[] = "hermes_home=";
    const char *p = command_lc;
    while ((p = strstr(p, needle)) != NULL) {
        int at_token_start = (p == command_lc) || isspace((unsigned char)p[-1]);
        const char *v = p + sizeof(needle) - 1;
        if (at_token_start && strncmp(v, home_lc, hl) == 0) {
            const char *after = v + hl;
            if (*after == '/')
                after++;
            if (*after == '\0' || isspace((unsigned char)*after))
                return 1;
        }
        p = v;
    }
    return 0;
}

/* ── _profile_name_for_home ───────────────────────────────────────────────
 * "<root>/profiles/<name>" -> the name; anything else -> NULL.
 * Caller frees.
 */
char *
st_profile_name_for_home(const char *profile_home)
{
    if (!profile_home || !*profile_home)
        return NULL;
    size_t len = strlen(profile_home);
    while (len > 1 && profile_home[len - 1] == '/')
        len--;

    /* last path segment */
    size_t end = len;
    size_t start = end;
    while (start > 0 && profile_home[start - 1] != '/')
        start--;

    /* its parent must be a directory literally named "profiles" */
    if (start == 0)
        return NULL;
    size_t pend = start - 1;               /* index of the '/' before the name */
    while (pend > 0 && profile_home[pend - 1] == '/')
        pend--;
    size_t pstart = pend;
    while (pstart > 0 && profile_home[pstart - 1] != '/')
        pstart--;
    if (pend - pstart != 8 || strncmp(profile_home + pstart, "profiles", 8) != 0)
        return NULL;

    size_t nlen = end - start;
    char *out = (char *)malloc(nlen + 1);
    if (!out)
        return NULL;
    memcpy(out, profile_home + start, nlen);
    out[nlen] = '\0';
    return out;
}
