/*
 * port_active_sessions_remaining.c — Port of hermes_cli/active_sessions.py
 * lock-file surface. State + lock paths, file handle lifecycle.
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

static char *state_dir(void) {
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/state", h);
    else asprintf(&out, "%s/.hermes/state", getenv("HOME") ? getenv("HOME") : ".");
    return out;
}

/* PoP: _state_path @ hermes_cli/active_sessions.py:_state_path */
char *acs_state_path(void) {
    char *d = state_dir();
    char *out = NULL;
    asprintf(&out, "%s/active_sessions.json", d);
    free(d);
    return out;
}

/* PoP: _lock_path @ hermes_cli/active_sessions.py:_lock_path */
char *acs_lock_path(void) {
    char *d = state_dir();
    char *out = NULL;
    asprintf(&out, "%s/active_sessions.lock", d);
    free(d);
    return out;
}

/* PoP: __init__ @ hermes_cli/active_sessions.py:__init__ */
char *acs_init(const char *path) {
    /* Python: lock handle holder. */
    if (!path) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"path\": \"%s\", \"fh\": null}", path);
    return out;
}
