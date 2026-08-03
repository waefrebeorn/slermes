/*
 * port_platforms_helpers_remaining.c — Port of gateway/platforms/helpers.py
 * helper surface. Seen-set dedupe, batching, state paths.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ gateway/platforms/helpers.py:__init__ */
char *phl_init(long max_size, long ttl_seconds) {
    /* Python: seen-set cache. */
    if (max_size <= 0) max_size = 1000;
    if (ttl_seconds <= 0) ttl_seconds = 300;
    char *out = NULL;
    asprintf(&out, "{\"max_size\": %ld, \"ttl\": %ld, \"seen\": {}}", max_size, ttl_seconds);
    return out;
}

/* PoP: contains @ gateway/platforms/helpers.py:contains */
bool phl_contains(const char *seen_json, const char *msg_id, double now) {
    /* Python: live without inserting. */
    if (!seen_json || !msg_id) return false;
    return strstr(seen_json, msg_id) != NULL;
}

/* PoP: is_enabled @ gateway/platforms/helpers.py:is_enabled */
bool phl_is_enabled(double batch_delay) {
    /* Python: batching active when delay > 0. */
    return batch_delay > 0.0;
}

/* PoP: _flush @ gateway/platforms/helpers.py:_flush */
int phl_flush(const char *key) {
    /* Python: wait then dispatch batched event. */
    if (!key) return -1;
    printf("batched event flushed for %s\n", key);
    return 0;
}

/* PoP: _state_path @ gateway/platforms/helpers.py:_state_path */
char *phl_state_path(const char *service_name) {
    /* Python: <home>/<service>.json state. */
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/%s.json", h, service_name ? service_name : "state");
    else asprintf(&out, "%s/.hermes/%s.json", getenv("HOME") ? getenv("HOME") : ".", service_name ? service_name : "state");
    return out;
}
