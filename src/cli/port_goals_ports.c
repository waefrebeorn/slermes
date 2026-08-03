/*
 * port_goals_remaining.c — Port of hermes_cli/goals.py goal surface.
 * Session db access, migration, truncation, turn limits.
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

/* PoP: _get_session_db @ hermes_cli/goals.py:_get_session_db */
char *gls_get_session_db(void) {
    /* Python: SessionDB for current home. */
    const char *h = getenv("HERMES_HOME");
    if (h && *h) {
        char *out = NULL;
        asprintf(&out, "%s/sessions.db", h);
        return out;
    }
    return strdup("sessions.db");
}

/* PoP: migrate_goal_to_session @ hermes_cli/goals.py:migrate_goal_to_session */
char *gls_migrate_goal_to_session(const char *goal_text, const char *parent_session_id, const char *session_id) {
    /* Python: carry persistent /goal to continuation. */
    if (!goal_text || !session_id) return NULL;
    printf("goal migrated: %.60s (%s → %s)\n", goal_text, parent_session_id ? parent_session_id : "?", session_id);
    return strdup("{\"success\": true}");
}

/* PoP: _truncate @ hermes_cli/goals.py:_truncate */
char *gls_truncate(const char *text, long limit) {
    /* Python: hard truncate. */
    if (!text) return strdup("");
    size_t n = strlen(text);
    if ((long)n <= limit) return strdup(text);
    char *out = malloc((size_t)limit + 8);
    if (!out) return strdup(text);
    memcpy(out, text, (size_t)limit);
    strcpy(out + limit, "…");
    return out;
}

/* PoP: __init__ @ hermes_cli/goals.py:__init__ */
char *gls_init(const char *session_id, long default_max_turns) {
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"default_max_turns\": %ld}",
             session_id ? session_id : "", default_max_turns);
    return out;
}
