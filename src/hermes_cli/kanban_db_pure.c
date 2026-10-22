#define _POSIX_C_SOURCE 200809L
/* kanban_db_pure.c - Faithful port of the pure, deterministic helpers in
 * hermes_cli/kanban_db.py (the Kanban board/dispatcher DB layer).
 *
 * Ports the side-effect-free surface: reasoning-effort normalization,
 * claim/crash/rate-limit TTL resolution (env override modeled as an
 * explicit argument: NULL uses the built-in default, exactly like Python's
 * os.environ read), relative-age rendering, board-slug validation, and
 * display-name derivation.
 *
 * PoP (Python -> C):
 *   normalize_reasoning_effort        -> kanban_normalize_reasoning_effort
 *   _resolve_claim_ttl_seconds        -> kanban_resolve_claim_ttl_seconds
 *   _resolve_crash_grace_seconds      -> kanban_resolve_crash_grace_seconds
 *   _resolve_rate_limit_cooldown_seconds -> kanban_resolve_rate_limit_cooldown_seconds
 *   _relative_age                     -> kanban_relative_age
 *   _normalize_board_slug             -> kanban_normalize_board_slug
 *   _default_board_display_name       -> kanban_default_board_display_name
 *
 * Functions that Python reads from os.environ take an explicit `env`
 * argument (the env-var value, or NULL for "unset"). Callers receive
 * freshly malloc'd strings (free with free()) or NULL.
 */

#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

#define KANBAN_DEFAULT_CLAIM_TTL_SECONDS    (15 * 60)
#define KANBAN_DEFAULT_CRASH_GRACE_SECONDS  30
#define KANBAN_DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS 300

/* VALID_REASONING_EFFORTS from hermes_constants.py */
static const char *VALID_REASONING_EFFORTS[] = {
    "minimal", "low", "medium", "high", "xhigh", "max", "ultra", NULL
};

/* --- normalize_reasoning_effort -----------------------------------  */
/* Returns malloc'd normalized string, or NULL (None) for empty, or
 * "__ERR__: ..." when invalid. */
char *kanban_normalize_reasoning_effort(const char *effort) {
    char buf[128];
    if (!effort) effort = "";
    size_t i = 0;
    while (effort[i] && isspace((unsigned char)effort[i])) i++;
    size_t st = i;
    size_t en = strlen(effort);
    while (en > st && isspace((unsigned char)effort[en - 1])) en--;
    size_t o = 0;
    for (size_t k = st; k < en; k++) buf[o++] = (char)tolower((unsigned char)effort[k]);
    buf[o] = '\0';
    if (!buf[0]) return NULL; /* None */
    if (strcmp(buf, "none") == 0) return strdup("none");
    for (int v = 0; VALID_REASONING_EFFORTS[v]; v++) {
        if (strcmp(buf, VALID_REASONING_EFFORTS[v]) == 0) return strdup(buf);
    }
    char *err = strdup("ERROR:ValueError");
    return err;
}

/* --- ttl/grace/cooldown resolvers (env modeled as explicit arg) ---  */
/* PoP: kanban_resolve_claim_ttl_seconds @ hermes_cli/kanban_db.py:_resolve_claim_ttl_seconds */

int kanban_resolve_claim_ttl_seconds(int ttl_seconds) {
    if (ttl_seconds > 0) return ttl_seconds;
    return KANBAN_DEFAULT_CLAIM_TTL_SECONDS;
}/* PoP: kanban_resolve_crash_grace_seconds @ hermes_cli/kanban_db.py:_resolve_crash_grace_seconds */

int kanban_resolve_crash_grace_seconds(const char *env) {
    if (env && *env) {
        char *end; long v = strtol(env, &end, 10);
        if (*end == '\0' && v >= 0) return (int)v;
    }
    return KANBAN_DEFAULT_CRASH_GRACE_SECONDS;
}/* PoP: kanban_resolve_rate_limit_cooldown_seconds @ hermes_cli/kanban_db.py:_resolve_rate_limit_cooldown_seconds */

int kanban_resolve_rate_limit_cooldown_seconds(const char *env) {
    if (env && *env) {
        char *end; long v = strtol(env, &end, 10);
        if (*end == '\0' && v >= 0) return (int)v;
    }
    return KANBAN_DEFAULT_RATE_LIMIT_COOLDOWN_SECONDS;
}

/* --- _relative_age ------------------------------------------------ */
char *kanban_relative_age(long ts, long now) {
    if (now <= 0) now = (long)time(NULL);
    long delta = now - ts;
    if (delta < 0) return strdup("just now");
    if (delta < 60) return strdup("just now");
    if (delta < 3600) {
        long m = delta / 60;
        char b[32]; snprintf(b, sizeof(b), "%ldm ago", m); return strdup(b);
    }
    if (delta < 86400) {
        long h = delta / 3600;
        char b[32]; snprintf(b, sizeof(b), "%ldh ago", h); return strdup(b);
    }
    long d = delta / 86400;
    char b[32]; snprintf(b, sizeof(b), "%ldd ago", d); return strdup(b);
}

/* --- _normalize_board_slug ----------------------------------------  */
/* Returns malloc'd slug, or NULL for empty, or "__ERR__: ..." invalid. */
char *kanban_normalize_board_slug(const char *slug) {
    if (!slug) return NULL;
    char *s = strdup(slug);
    /* strip + lowercase */
    size_t l = strlen(s);
    size_t st = 0;
    while (st < l && isspace((unsigned char)s[st])) st++;
    size_t en = l;
    while (en > st && isspace((unsigned char)s[en - 1])) en--;
    size_t o = 0;
    for (size_t k = st; k < en; k++) s[o++] = (char)tolower((unsigned char)s[k]);
    s[o] = '\0';
    if (!s[0]) { free(s); return NULL; }
    /* regex ^[a-z0-9][a-z0-9\-_]{0,63}$ */
    size_t len = strlen(s);
    if (len < 1 || len > 64) { free(s); return strdup("ERROR:ValueError"); }
    char c0 = s[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9'))) {
        free(s); return strdup("ERROR:ValueError");
    }
    for (size_t k = 1; k < len; k++) {
        char ch = s[k];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) {
            free(s); return strdup("ERROR:ValueError");
        }
    }
    return s; /* malloc'd */
}

/* --- _default_board_display_name ---------------------------------- */
char *kanban_default_board_display_name(const char *slug) {
    if (!slug) slug = "";
    /* slug.replace("_","-").split("-") -> capitalize each part -> join " " */
    char *work = strdup(slug);
    size_t L = strlen(work);
    for (size_t k = 0; k < L; k++) if (work[k] == '_') work[k] = '-';
    /* tokenize on '-' */
    char out[512]; out[0] = '\0';
    char *p = work;
    int any = 0;
    while (*p) {
        char *dash = strchr(p, '-');
        char saved = 0;
        if (dash) { saved = *dash; *dash = '\0'; }
        if (*p) {
            char cap[256];
            strncpy(cap, p, sizeof(cap) - 1); cap[sizeof(cap)-1] = '\0';
            /* Python str.capitalize(): first upper, REST LOWER. */
            cap[0] = (char)toupper((unsigned char)cap[0]);
            for (size_t ci = 1; cap[ci]; ci++)
                cap[ci] = (char)tolower((unsigned char)cap[ci]);
            if (any) strcat(out, " ");
            strcat(out, cap);
            any = 1;
        }
        if (dash) { *dash = saved; p = dash + 1; } else break;
    }
    free(work);
    if (!any) return strdup(slug);
    return strdup(out);
}
