/*
 * plugin_session.c — C home for upstream Hermes hermes_cli/plugin_session.py.
 * PoP: w12_pluginsess_6b33f7_session_surface @ hermes_cli/plugin_session.py:session_surface
 * PoP: w12_pluginsess_6b33f7_context_surface @ hermes_cli/plugin_session.py:context_surface
 * PoP: w12_pluginsess_6b33f7_session_cwd     @ hermes_cli/plugin_session.py:session_cwd
 *
 * The gateway/task-local lookups are injected as already-resolved values by
 * the harness (matching the oracle's monkeypatched seams), so these functions
 * are the real decision logic of the Python originals.
 */
#include "noc_w12_pluginsess_6b33f7.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char *py_strip(char *s)
{
    size_t n;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

void w12_pluginsess_6b33f7_session_surface(bool has_policy, const char *platform,
                          const char *env_platform, char *out, size_t cap)
{
    const char *r = has_policy ? platform : env_platform;
    snprintf(out, cap, "%s", r ? r : "");
}

void w12_pluginsess_6b33f7_context_surface(bool cli_ref, const char *surface, char *out, size_t cap)
{
    snprintf(out, cap, "%s", cli_ref ? "cli" : (surface ? surface : ""));
}

void w12_pluginsess_6b33f7_session_cwd(const char *scoped, bool has_policy, const char *policy_cwd,
                     const char *terminal_cwd, char *out, size_t cap)
{
    char tmp[2048];
    if (scoped && scoped[0]) { snprintf(out, cap, "%s", scoped); return; }
    if (has_policy && policy_cwd && policy_cwd[0]) {
        snprintf(out, cap, "%s", policy_cwd); return;
    }
    snprintf(tmp, sizeof(tmp), "%s", terminal_cwd ? terminal_cwd : "");
    snprintf(out, cap, "%s", py_strip(tmp));
}
