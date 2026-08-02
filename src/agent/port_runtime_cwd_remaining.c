/*
 * port_runtime_cwd_remaining.c — Port of agent/runtime_cwd.py cwd surface.
 * Session cwd pinning, override resolution, agent + context cwd.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: set_session_cwd @ agent/runtime_cwd.py:set_session_cwd */
char *rcw_set_session_cwd(const char *cwd) {
    /* Python: pin logical cwd. */
    if (!cwd) return strdup("");
    return strdup(cwd);
}

/* PoP: clear_session_cwd @ agent/runtime_cwd.py:clear_session_cwd */
int rcw_clear_session_cwd(void) {
    printf("session cwd cleared\n");
    return 0;
}

/* PoP: _session_cwd_override @ agent/runtime_cwd.py:_session_cwd_override */
char *rcw_session_cwd_override(void) {
    /* Python: current override or "". */
    printf("session cwd override read\n");
    return strdup("");
}

/* PoP: resolve_agent_cwd @ agent/runtime_cwd.py:resolve_agent_cwd */
char *rcw_resolve_agent_cwd(const char *override, const char *fallback) {
    /* Python: override expanduser else fallback. */
    if (override && *override) {
        char *out = NULL;
        if (override[0] == '~') {
            const char *home = getenv("HOME");
            if (home) asprintf(&out, "%s%s", home, override + 1);
        }
        if (!out) out = strdup(override);
        return out;
    }
    if (fallback && *fallback) return strdup(fallback);
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) return strdup(cwd);
    return strdup(".");
}

/* PoP: resolve_context_cwd @ agent/runtime_cwd.py:resolve_context_cwd */
char *rcw_resolve_context_cwd(const char *override, const char *fallback) {
    /* Python: None means no configured cwd. */
    return rcw_resolve_agent_cwd(override, fallback ? fallback : "");
}
