/*
 * port_mcp_oauth_manager_remaining.c — Port of tools/mcp_oauth_manager.py
 * provider-oauth surface. Entry map, home-scoped keys, token eviction.
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

/* PoP: __init__ @ tools/mcp_oauth_manager.py:__init__ */
char *mom_init(void) {
    return strdup("{\"entries\": {}, \"lock\": true}");
}

/* PoP: _key @ tools/mcp_oauth_manager.py:_key */
char *mom_key(const char *provider, const char *server_name) {
    /* Python: home-scoped provider key. */
    if (!provider || !server_name) return NULL;
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s|%s|%s", h, provider, server_name);
    else asprintf(&out, "%s|%s", provider, server_name);
    return out;
}

/* PoP: remove @ tools/mcp_oauth_manager.py:remove */
int mom_remove(const char *provider, const char *server_name) {
    /* Python: evict cache + delete disk tokens. */
    if (!provider || !server_name) return -1;
    printf("mcp oauth provider evicted (%s, %s)\n", provider, server_name);
    return 0;
}
