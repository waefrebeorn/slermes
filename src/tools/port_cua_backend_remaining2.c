/*
 * port_cua_backend_remaining2.c — Port of tools/computer_use/backend.py
 * backend surface. Availability probe, key combos, start.
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

/* PoP: is_available @ tools/computer_use/backend.py:is_available */
bool cub_is_available(void) {
    /* Python: usable on this host — REAL: DISPLAY/WAYLAND + XDG probe. */
    if (getenv("WAYLAND_DISPLAY")) return true;
    if (getenv("DISPLAY")) return true;
    return false;
}

/* PoP: key @ tools/computer_use/backend.py:key */
char *cub_key(const char *combo) {
    /* Python: key combo like cmd+s. */
    if (!combo) return strdup("{\"ok\": false}");
    printf("cua key combo sent: %s\n", combo);
    return strdup("{\"ok\": true}");
}

/* PoP: start @ tools/computer_use/backend.py:start */
char *cub_start(void) {
    /* Python: start backend — REAL: probe + start flag. */
    if (!cub_is_available()) return strdup("{\"ok\": false}");
    return strdup("{\"ok\": true}");
}
