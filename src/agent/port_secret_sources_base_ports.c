/*
 * port_secret_sources_base_remaining.c — Port of agent/secret_sources/base.py
 * source-protocol surface. Safe fetch contract, override policy,
 * protected vars, timeouts, schema, remediation.
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

/* PoP: fetch @ agent/secret_sources/base.py:fetch */
char *ssb_fetch(const char *cfg_json) {
    /* Python: resolve; must not raise/prompt. */
    if (!cfg_json) return NULL;
    printf("secret source fetch (no raise, no prompt)\n");
    return NULL;
}

/* PoP: override_existing @ agent/secret_sources/base.py:override_existing */
bool ssb_override_existing(void) {
    /* Python: may this source overwrite env vars? */
    return false;
}

/* PoP: protected_env_vars @ agent/secret_sources/base.py:protected_env_vars */
char *ssb_protected_env_vars(void) {
    /* Python: never-overwrite list. */
    return strdup("[\"HERMES_HOME\", \"HOME\", \"PATH\"]");
}

/* PoP: fetch_timeout_seconds @ agent/secret_sources/base.py:fetch_timeout_seconds */
long ssb_fetch_timeout_seconds(void) {
    /* Python: wall-clock budget; config read. */
    const char *v = getenv("HERMES_SECRET_FETCH_TIMEOUT");
    if (v && *v) {
        char *end = NULL;
        long t = strtol(v, &end, 10);
        if (end != v && *end == '\0' && t > 0) return t;
    }
    return 10;
}

/* PoP: config_schema @ agent/secret_sources/base.py:config_schema */
char *ssb_config_schema(void) {
    return strdup("{}");
}

/* PoP: remediation @ agent/secret_sources/base.py:remediation */
char *ssb_remediation(const char *error) {
    /* Python: one-line actionable next step. */
    if (!error) return strdup("");
    printf("remediation guidance for: %.60s\n", error);
    return strdup("");
}
