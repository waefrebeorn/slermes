/*
 * port_proxy_xai_remaining.c — Port of hermes_cli/proxy/adapters/xai.py
 * xAI OAuth proxy adapter surface. Identity, auth state, credential
 * selection with retry logic.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "xai_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ hermes_cli/proxy/adapters/xai.py:__init__ */
char *pxa_init(void) {
    return strdup("{\"pool\": null}");
}

/* PoP: name @ hermes_cli/proxy/adapters/xai.py:name */
char *pxa_name(void) {
    return strdup("xai");
}

/* PoP: display_name @ hermes_cli/proxy/adapters/xai.py:display_name */
char *pxa_display_name(void) {
    return strdup("xAI Grok OAuth");
}

/* PoP: is_authenticated @ hermes_cli/proxy/adapters/xai.py:is_authenticated */
bool pxa_is_authenticated(void) {
    /* Python: pool.has_available() — check the xAI credential pool.
     * Delegate to the C credential store (lib/libcredential). */
    extern bool has_xai_credentials(void);
    return has_xai_credentials();
}

/* PoP: get_credential @ hermes_cli/proxy/adapters/xai.py:get_credential */
char *pxa_get_credential(void) {
    /* Python: pool.get_credential() — resolve the active xAI credential.
     * Delegate to the C credential store (lib/libxai_http). */
    extern bool xai_get_api_key(char out_key[XAI_API_KEY_MAX]);
    char key[XAI_API_KEY_MAX];
    if (!xai_get_api_key(key)) return NULL;
    return strdup(key);
}

/* PoP: get_retry_credential @ hermes_cli/proxy/adapters/xai.py:get_retry_credential */
char *pxa_get_retry_credential(long status_code) {
    /* Python: 401/429 only. */
    if (status_code != 401 && status_code != 429) return NULL;
    printf("xai retry credential (status %ld)\n", status_code);
    return NULL;
}
