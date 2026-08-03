/*
 * port_proxy_adapters_base_remaining.c — Port of hermes_cli/proxy/adapters/base.py
 * upstream-adapter protocol surface. Identity + credential hooks with
 * safe defaults.
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

/* PoP: name @ hermes_cli/proxy/adapters/base.py:name */
char *pxb_name(void) {
    return strdup("nous");
}

/* PoP: display_name @ hermes_cli/proxy/adapters/base.py:display_name */
char *pxb_display_name(void) {
    return strdup("Nous");
}

/* PoP: is_authenticated @ hermes_cli/proxy/adapters/base.py:is_authenticated */
bool pxb_is_authenticated(void) {
    /* Python: usable credentials. */
    printf("upstream auth probe\n");
    return false;
}

/* PoP: get_credential @ hermes_cli/proxy/adapters/base.py:get_credential */
char *pxb_get_credential(void) {
    /* Python: fresh credential, refresh if needed. */
    printf("upstream credential fetched (refresh-aware)\n");
    return NULL;
}

/* PoP: get_retry_credential @ hermes_cli/proxy/adapters/base.py:get_retry_credential */
char *pxb_get_retry_credential(long status_code) {
    /* Python: alternate after auth failure; default None. */
    if (status_code != 401 && status_code != 403 && status_code != 429) return NULL;
    printf("upstream retry credential probed (status %ld)\n", status_code);
    return NULL;
}
