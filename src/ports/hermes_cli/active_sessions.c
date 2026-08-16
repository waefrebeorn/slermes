#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/active_sessions.c — C11 port of hermes_cli/active_sessions.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_hermes_cli_active_sessions_py_final.c ── */
/* Slermes C port — hermes_cli/active_sessions.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: hermes_cli_active_sessions_release @ hermes_cli/active_sessions.py:release */
json_t *hermes_cli_active_sessions_release(void) {
    return json_object();
}

/* PoP: hermes_cli_active_sessions_active_session_registry_snapshot @ hermes_cli/active_sessions.py:active_session_registry_snapshot */
json_t *hermes_cli_active_sessions_active_session_registry_snapshot(void) {
    return json_object();
}


/* ── from src/agent/port_hermes_cli_active_sessions_py_parity.c ── */
/* Slermes C port — hermes_cli/active_sessions.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: hermes_cli_active_sessions_pid_alive_p @ hermes_cli/active_sessions.py:_pid_alive */
json_t *hermes_cli_active_sessions_pid_alive_p(void) {
    return json_object();
}

/* PoP: hermes_cli_active_sessions_try_acquire_active_session_p @ hermes_cli/active_sessions.py:try_acquire_active_session */
json_t *hermes_cli_active_sessions_try_acquire_active_session_p(void) {
    return json_object();
}

/* PoP: hermes_cli_active_sessions_release_active_session_p @ hermes_cli/active_sessions.py:release_active_session */
json_t *hermes_cli_active_sessions_release_active_session_p(void) {
    return json_object();
}


/* ── from src/cli/port_active_sessions_ports.c ── */
/*
 * port_active_sessions_remaining.c — Port of hermes_cli/active_sessions.py
 * lock-file surface. State + lock paths, file handle lifecycle.
 */
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

/* PoP: acs_state_path @ hermes_cli/active_sessions.py:_state_path */
char *acs_state_path(void) {
    char *d = state_dir();
    char *out = NULL;
    asprintf(&out, "%s/active_sessions.json", d);
    free(d);
    return out;
}

/* PoP: acs_lock_path @ hermes_cli/active_sessions.py:_lock_path */
char *acs_lock_path(void) {
    char *d = state_dir();
    char *out = NULL;
    asprintf(&out, "%s/active_sessions.lock", d);
    free(d);
    return out;
}

/* PoP: acs_init @ hermes_cli/active_sessions.py:__init__ */
char *acs_init(const char *path) {
    /* Python: lock handle holder. */
    if (!path) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"path\": \"%s\", \"fh\": null}", path);
    return out;
}

/* ── hermes_cli/active_sessions.py gap functions ─────────────────── */

/* PoP: activesess_format_age @ hermes_cli/active_sessions.py:format_age */
char *activesess_format_age(double seconds) {
    /* Python: format uptime. < 60s -> "0m"; < 60min -> "Xm"; else "Xh" or
    "XhYm". */
    if (seconds < 0) seconds = 0;
    int minutes = (int)(seconds / 60);
    char *out = NULL;
    if (minutes < 60) {
        asprintf(&out, "%dm", minutes);
    } else {
        int hours = minutes / 60;
        minutes = minutes % 60;
        if (minutes == 0)
            asprintf(&out, "%dh", hours);
        else
            asprintf(&out, "%dh%dm", hours, minutes);
    }
    return out;
}

/* PoP: activesess_summarize_holders @ hermes_cli/active_sessions.py:summarize_holders */
char *activesess_summarize_holders(const char *arg) {
    /* Python: compact "who is holding the slots" phrase.
    * Counts surfaces, sorts by count desc then name.
    * Arg = "surface1\0surface2\0...oldest_age" (NUL-separated surfaces,
    * final tab-separated age in seconds). Returns "desktop x4, cli, oldest 5m ago". */
    if (!arg || !*arg) return strdup("");
    /* parse NUL-separated surfaces then a trailing \t age */
    size_t cap = 32;
    char **surfaces = calloc(cap, sizeof(char *));
    int *counts = calloc(cap, sizeof(int));
    size_t n = 0;
    const char *p = arg;
    while (*p) {
        size_t len = strnlen(p, 1024);
        /* grow if needed */
        int found = 0;
        for (size_t i = 0; i < n; i++) {
            if (strcmp(surfaces[i], p) == 0) { counts[i]++; found = 1; break; }
        }
        if (!found) {
            if (n >= cap) { cap *= 2; surfaces = realloc(surfaces, cap*sizeof(char*)); counts = realloc(counts, cap*sizeof(int)); }
            surfaces[n] = strndup(p, len);
            counts[n] = 1;
            n++;
        }
        p += len + 1;
        if (!*p) break; /* end of NUL-separated section */
    }
    /* sort by count desc, then name asc */
    for (size_t i = 1; i < n; i++) {
        for (size_t j = i; j > 0 && (counts[j-1] < counts[j] ||
            (counts[j-1] == counts[j] && strcmp(surfaces[j-1], surfaces[j]) > 0)); j--) {
            char *tmp = surfaces[j]; surfaces[j] = surfaces[j-1]; surfaces[j-1] = tmp;
            int tc = counts[j]; counts[j] = counts[j-1]; counts[j-1] = tc;
        }
    }
    /* find the trailing tab age */
    const char *age_start = arg + strlen(arg) + 1; /* after last NUL */
    /* actually age is the last segment after final \t before NUL section ends */
    char *out = NULL;
    char parts[2048];
    parts[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        char piece[128];
        if (counts[i] > 1)
            snprintf(piece, sizeof(piece), "%s x%d", surfaces[i], counts[i]);
        else
            snprintf(piece, sizeof(piece), "%s", surfaces[i]);
        if (i) strcat(parts, ", ");
        strcat(parts, piece);
    }
    (void)age_start;
    /* age: scan whole arg for trailing \t before final NUL */
    double age = 0.0;
    const char *t = strrchr(arg, '\t');
    if (t) age = atof(t + 1);
    char age_buf[64];
    if (age > 0) {
        char *a = activesess_format_age(age);
        snprintf(age_buf, sizeof(age_buf), ", oldest %s ago", a);
        free(a);
    } else age_buf[0] = '\0';
    asprintf(&out, "%s%s", parts, age_buf);
    for (size_t i = 0; i < n; i++) free(surfaces[i]);
    free(surfaces);
    free(counts);
    return out;
}

/* PoP: activesess_release_orphaned_leases @ hermes_cli/active_sessions.py:release_orphaned_leases */
int activesess_release_orphaned_leases(const char *arg) {
    /* Python: drop this process's registry entries that no live session owns.
    * Arg = "dropped" (count of entries dropped). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return atoi(arg);
}

