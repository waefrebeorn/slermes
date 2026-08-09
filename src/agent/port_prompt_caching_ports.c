/*
 * port_prompt_caching_remaining.c — Port of agent/prompt_caching.py cache
 * marker surface. Marker build + application across formats.
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

/* PoP: _build_marker @ agent/prompt_caching.py:_build_marker */
char *pca_build_marker(const char *ttl) {
    /* Python: '5m' or '1h' cache_control. */
    if (!ttl) ttl = "5m";
    char *out = NULL;
    asprintf(&out, "{\"type\": \"ephemeral\", \"ttl\": \"%s\"}", ttl);
    return out;
}

/* PoP: _apply_cache_marker @ agent/prompt_caching.py:_apply_cache_marker */
char *pca_apply_cache_marker(const char *message_json, const char *ttl) {
    /* Python: add cache_control handling format variations. */
    if (!message_json) return NULL;
    char *marker = pca_build_marker(ttl);
    char *out = NULL;
    size_t n = strlen(message_json);
    if (n && message_json[n-1] == '}') {
        asprintf(&out, "%.*s, \"cache_control\": %s}", (int)(n - 1), message_json, marker);
    } else {
        asprintf(&out, "%s, \"cache_control\": %s}", message_json, marker);
    }
    free(marker);
    return out;
}

/* PoP: apply_anthropic_cache_control @ agent/prompt_caching.py:apply_anthropic_cache_control */
/* Real json_t*-based implementation lives in port_prompt_caching_plan.c
 * (pca_apply_anthropic_cache_control).  The previous string-based echo stub
 * here was a BOOTLEG placeholder; removed in favour of the faithful port. */
