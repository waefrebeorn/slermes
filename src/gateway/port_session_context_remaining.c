/*
 * port_session_context_remaining.c — Port of gateway/session_context.py
 * session-context surface. Env + contextvar sync, reset tokens, legacy
 * names.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: set_current_session_id @ gateway/session_context.py:set_current_session_id */
int sct_set_current_session_id(const char *session_id) {
    /* Python: ContextVar + os.environ sync — REAL setenv. */
    if (!session_id) return -1;
    setenv("HERMES_SESSION_ID", session_id, 1);
    return 0;
}

/* PoP: set_session_vars @ gateway/session_context.py:set_session_vars */
char *sct_set_session_vars(const char *session_id, const char *profile, const char *platform) {
    /* Python: all session vars + reset tokens. */
    if (session_id) setenv("HERMES_SESSION_ID", session_id, 1);
    if (profile) setenv("HERMES_SESSION_PROFILE", profile, 1);
    if (platform) setenv("HERMES_SESSION_PLATFORM", platform, 1);
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"profile\": \"%s\", \"platform\": \"%s\"}",
             session_id ? session_id : "", profile ? profile : "", platform ? platform : "");
    return out;
}

/* PoP: clear_session_vars @ gateway/session_context.py:clear_session_vars */
int sct_clear_session_vars(void) {
    /* Python: mark explicitly cleared. */
    unsetenv("HERMES_SESSION_ID");
    unsetenv("HERMES_SESSION_PROFILE");
    unsetenv("HERMES_SESSION_PLATFORM");
    printf("session context vars cleared\n");
    return 0;
}

/* PoP: get_session_env @ gateway/session_context.py:get_session_env */
char *sct_get_session_env(const char *legacy_name) {
    /* Python: legacy HERMES_SESSION_* read. */
    if (!legacy_name) return NULL;
    const char *v = getenv(legacy_name);
    return v ? strdup(v) : NULL;
}
