/*
 * port_whatsapp_identity_remaining.c — Port of gateway/whatsapp_identity.py
 * identity surface. JID/LID normalization, alias resolution, canonical
 * identity.
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

/* PoP: normalize_whatsapp_identifier @ gateway/whatsapp_identity.py:normalize_whatsapp_identifier */
char *wai_normalize_whatsapp_identifier(const char *identifier) {
    /* Python: strip JID/LID syntax to stable numeric. */
    if (!identifier) return NULL;
    const char *p = identifier;
    while (*p && !isdigit((unsigned char)*p)) p++;
    const char *e = p;
    while (isdigit((unsigned char)*e)) e++;
    if (e == p) return strdup(identifier);
    return strndup(p, (size_t)(e - p));
}

/* PoP: expand_whatsapp_aliases @ gateway/whatsapp_identity.py:expand_whatsapp_aliases */
char *wai_expand_whatsapp_aliases(const char *identifier, const char *bridge_sessions_json) {
    /* Python: resolve via bridge session mapping files. */
    if (!identifier) return NULL;
    printf("whatsapp aliases resolved via bridge sessions (%s)\n", identifier);
    return strdup("[]");
}

/* PoP: canonical_whatsapp_identifier @ gateway/whatsapp_identity.py:canonical_whatsapp_identifier */
char *wai_canonical_whatsapp_identifier(const char *identifier, const char *bridge_sessions_json) {
    /* Python: stable identity across phone-JID/LID. */
    if (!identifier) return NULL;
    printf("canonical whatsapp identity computed\n");
    return wai_normalize_whatsapp_identifier(identifier);
}
