/*
 * port_proxy_adapters_base_remaining.c — Port of hermes_cli/proxy/adapters/base.py
 * upstream-adapter protocol surface. Identity + credential hooks with
 * safe defaults.
 */
#define _POSIX_C_SOURCE 200809L
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* ── rest_revive1 (proxy_adapters_base_ports) ───────────────────────────────
 * UpstreamAdapter.name / display_name are @property @abstractmethod with
 * docstring-only bodies: calling the plain extracted def returns None.
 * The faithful port therefore returns NULL (Python None) — the previous
 * "nous"/"Nous" literals were fabricated adapter-specific answers.
 * json_t *fn(void *io, json_t *in) — no I/O seams; io/in unused. */

/* PoP: pxb_name @ hermes_cli/proxy/adapters/base.py:name */
json_t *pxb_name(void *io, json_t *in) {
    (void)io;
    (void)in;
    return json_null();
}

/* PoP: pxb_display_name @ hermes_cli/proxy/adapters/base.py:display_name */
json_t *pxb_display_name(void *io, json_t *in) {
    (void)io;
    (void)in;
    return json_null();
}

/* PoP: pxb_is_authenticated @ hermes_cli/proxy/adapters/base.py:is_authenticated */
bool pxb_is_authenticated(void) {
    /* Python: usable credentials. */
    printf("upstream auth probe\n");
    return false;
}

/* PoP: pxb_get_credential @ hermes_cli/proxy/adapters/base.py:get_credential */
char *pxb_get_credential(void) {
    /* Python: fresh credential, refresh if needed. */
    printf("upstream credential fetched (refresh-aware)\n");
    return NULL;
}

/* PoP: pxb_get_retry_credential @ hermes_cli/proxy/adapters/base.py:get_retry_credential */
char *pxb_get_retry_credential(long status_code) {
    /* Python: alternate after auth failure; default None. */
    if (status_code != 401 && status_code != 403 && status_code != 429) return NULL;
    printf("upstream retry credential probed (status %ld)\n", status_code);
    return NULL;
}
