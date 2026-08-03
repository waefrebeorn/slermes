/*
 * port_lsp_init_remaining.c — Port of agent/lsp/__init__.py service surface.
 * Process-wide singleton, shutdown, atexit wrapper.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "lsp_common.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* Process-wide LSP service singleton (Python module-level _service). */
static lsp_service_t *s_lsp_service = NULL;

/* PoP: get_service @ agent/lsp/__init__.py:get_service */
char *lspi_get_service(void) {
    /* Python: process-wide singleton or None. */
    if (s_lsp_service) {
        return strdup("{\"active\": true}");
    }
    /* Lazy-create when LSP is enabled (lsp.enabled config default). */
    extern lsp_service_t *lsp_service_create(bool enabled, lsp_server_desc_t **servers);
    const char *enabled_env = getenv("HERMES_LSP_ENABLED");
    bool enabled = enabled_env ? (strcmp(enabled_env, "1") == 0 ||
                                  strcasecmp(enabled_env, "true") == 0 ||
                                  strcasecmp(enabled_env, "yes") == 0)
                               : false;
    s_lsp_service = lsp_service_create(enabled, NULL);
    if (!s_lsp_service) return NULL;
    return strdup("{\"active\": true}");
}

/* PoP: shutdown_service @ agent/lsp/__init__.py:shutdown_service */
int lspi_shutdown_service(void) {
    /* Python: safe multiple times. */
    extern void lsp_service_destroy(lsp_service_t *svc);
    if (s_lsp_service) {
        lsp_service_destroy(s_lsp_service);
        s_lsp_service = NULL;
    }
    return 0;
}

/* PoP: _atexit_shutdown @ agent/lsp/__init__.py:_atexit_shutdown */
int lspi_atexit_shutdown(void) {
    /* Python: atexit wrapper around shutdown_service. */
    return lspi_shutdown_service();
}
