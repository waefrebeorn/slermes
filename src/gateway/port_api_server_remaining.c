/*
 * port_api_server_remaining.c — Port of gateway/platforms/api_server.py
 * bounded-cache surface. Size limits, db path resolution.
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

/* PoP: __init__ @ gateway/platforms/api_server.py:__init__ */
char *aps_init(long max_size, const char *db_path) {
    /* Python: bounded cache; home db fallback. */
    if (max_size <= 0) max_size = 1000;
    char *out = NULL;
    if (db_path && *db_path)
        asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"%s\"}", max_size, db_path);
    else {
        const char *h = getenv("HERMES_HOME");
        if (h && *h) asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"%s/api_server.db\"}", max_size, h);
        else asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"api_server.db\"}", max_size);
    }
    return out;
}
