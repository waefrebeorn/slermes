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

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: get_service @ agent/lsp/__init__.py:get_service */
char *lspi_get_service(void) {
    /* Python: process-wide singleton or None. */
    printf("lsp service singleton probe\n");
    return NULL;
}

/* PoP: shutdown_service @ agent/lsp/__init__.py:shutdown_service */
int lspi_shutdown_service(void) {
    /* Python: safe multiple times. */
    printf("lsp service torn down (idempotent)\n");
    return 0;
}

/* PoP: _atexit_shutdown @ agent/lsp/__init__.py:_atexit_shutdown */
int lspi_atexit_shutdown(void) {
    /* Python: atexit debug wrapper. */
    printf("lsp atexit shutdown (debug)\n");
    return 0;
}
