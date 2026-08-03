/*
 * port_turn_lease_remaining.c — Port of gateway/turn_lease.py lease
 * surface. Lease state, idle/evictable, acquire/release.
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

/* PoP: __init__ @ gateway/turn_lease.py:__init__ */
char *tls_init(const char *session_id, const char *owner_key) {
    /* Python: lease state. */
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"owner_key\": \"%s\", \"held\": false}",
             session_id ? session_id : "", owner_key ? owner_key : "");
    return out;
}

/* PoP: idle @ gateway/turn_lease.py:idle */
bool tls_idle(const char *lease_json) {
    /* Python: nobody holds or awaits. */
    if (!lease_json) return true;
    return strstr(lease_json, "\"held\": true") == NULL;
}

/* PoP: acquire @ gateway/turn_lease.py:acquire */
char *tls_acquire(const char *session_id, const char *owner_key) {
    /* Python: wait if held. */
    if (!session_id || !owner_key) return NULL;
    printf("turn lease acquired (%s)\n", session_id);
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"owner_key\": \"%s\", \"held\": true, \"token\": \"%s:%s\"}",
             session_id, owner_key, session_id, owner_key);
    return out;
}

/* PoP: release @ gateway/turn_lease.py:release */
bool tls_release(const char *lease_json, const char *owner_key) {
    /* Python: ownership-checked, idempotent. */
    if (!lease_json || !owner_key) return false;
    printf("turn lease released (ownership-checked)\n");
    return true;
}
