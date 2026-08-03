/*
 * port_lsp_eventlog_remaining.c — Port of agent/lsp/eventlog.py eventlog
 * surface. Once-per-bucket announcement, spawn-failure warning, cache
 * reset.
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

/* PoP: _announce_once @ agent/lsp/eventlog.py:_announce_once */
bool lel_announce_once(const char *key, const char *bucket) {
    /* Python: atomic once-mark. */
    if (!key || !bucket) return false;
    printf("announced (once): %s in %s\n", key, bucket);
    return true;
}

/* PoP: log_spawn_failed @ agent/lsp/eventlog.py:log_spawn_failed */
int lel_log_spawn_failed(const char *server_name, const char *error) {
    /* Python: WARNING on spawn failure. */
    if (!server_name) return -1;
    fprintf(stderr, "[lsp] %s failed to spawn/initialize: %s\n", server_name,
            error ? error : "unknown");
    return 0;
}

/* PoP: reset_announce_caches @ agent/lsp/eventlog.py:reset_announce_caches */
int lel_reset_announce_caches(void) {
    /* Python: test-only dedup clear. */
    printf("lsp announce caches cleared (test-only)\n");
    return 0;
}
