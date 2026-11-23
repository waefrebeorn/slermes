/*
 * session_source.c — C11 port of the source-vocabulary core of upstream
 * Hermes agent/session_source.py.
 *
 * PoP: w7_sessionsour_f0a029_create_surface @ agent/session_source.py:create_surface
 * PoP: w7_sessionsour_f0a029_session_source_for @ agent/session_source.py:session_source_for
 * PoP: w7_sessionsour_f0a029_is_ui_transport @ agent/session_source.py:UI_TRANSPORT_SOURCES
 * PoP: w7_sessionsour_f0a029_is_cli_family @ agent/session_source.py:CLI_FAMILY_SOURCES
 * PoP: w7_sessionsour_f0a029_is_daybreak @ agent/session_source.py:DAYBREAK_SOURCES
 * PoP: w7_sessionsour_f0a029_is_attended @ agent/session_source.py:ATTENDED_SOURCES
 *
 * get_session_env is an env read; the fixture injects its three values through
 * an explicit seam rather than mutating the process environment.
 */
#include "noc_w7_sessionsour_f0a029.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static bool in_set(const char *v, const char *const *set) {
    if (!v) return false;
    for (int i = 0; set[i]; i++) if (!strcmp(v, set[i])) return true;
    return false;
}
static const char *const UI[] = {"tui", "desktop", NULL};
static const char *const CLI_FAMILY[] = {"cli", "oneshot", NULL};
/* DAYBREAK_SOURCES = UI | {"cli"} */
static const char *const DAYBREAK[] = {"tui", "desktop", "cli", NULL};
/* ATTENDED_SOURCES = UI | {"cli","acp"} */
static const char *const ATTENDED[] = {"tui", "desktop", "cli", "acp", NULL};

/* Python str.strip() for the ASCII whitespace used by session labels.
 * Always returns a fresh allocation so the caller can free it uniformly. */
static char *strip(const char *s) {
    if (!s) s = "";
    const char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - start);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, start, n); out[n] = '\0'; }
    return out;
}

const char *w7_sessionsour_f0a029_create_surface(const char *source) {
    return (source && !strcmp(source, "desktop")) ? "gui" : (source ? source : "");
}

char *w7_sessionsour_f0a029_session_source_for(const char *platform, const char *source,
                                               bool single_query, bool explicit) {
    char *trimmed = strip(source);
    if (!trimmed) trimmed = (char *)calloc(1, 1);
    if (single_query && !explicit && in_set(trimmed, UI)) {
        free(trimmed);
        trimmed = (char *)calloc(1, 1);
    }
    const char *plat = (platform && *platform) ? platform : "cli";
    if (single_query && !*trimmed && !strcmp(plat, "cli")) {
        free(trimmed);
        char *out = (char *)malloc(8);
        if (out) strcpy(out, "oneshot"); /* ONESHOT_SOURCE */
        return out;
    }
    if (*trimmed) return trimmed;
    size_t n = strlen(plat);
    char *out = (char *)malloc(n + 1);
    if (out) memcpy(out, plat, n + 1);
    free(trimmed);
    return out;
}

bool w7_sessionsour_f0a029_is_ui_transport(const char *source) { return in_set(source, UI); }
bool w7_sessionsour_f0a029_is_cli_family(const char *source) { return in_set(source, CLI_FAMILY); }
bool w7_sessionsour_f0a029_is_daybreak(const char *source) { return in_set(source, DAYBREAK); }
bool w7_sessionsour_f0a029_is_attended(const char *source) { return in_set(source, ATTENDED); }

