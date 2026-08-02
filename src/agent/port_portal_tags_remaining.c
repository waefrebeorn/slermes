/*
 * port_portal_tags_remaining.c — Port of agent/portal_tags.py tags surface.
 * Version resolution, client tag, canonical product tags.
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

/* PoP: _hermes_version @ agent/portal_tags.py:_hermes_version */
char *ptg_hermes_version(void) {
    /* Python: release version or fallback. */
    const char *v = getenv("HERMES_VERSION");
    if (v && *v) return strdup(v);
    return strdup("0.0.0");
}

/* PoP: hermes_client_tag @ agent/portal_tags.py:hermes_client_tag */
char *ptg_hermes_client_tag(void) {
    /* Python: client=hermes-<version>. */
    char *ver = ptg_hermes_version();
    char *out = NULL;
    asprintf(&out, "client=hermes-%s", ver);
    free(ver);
    return out;
}

/* PoP: nous_portal_tags @ agent/portal_tags.py:nous_portal_tags */
char *ptg_nous_portal_tags(void) {
    /* Python: canonical product tags, fresh list. */
    return strdup("[\"hermes\", \"agent\"]");
}
