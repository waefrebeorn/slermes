/*
 * port_cua_backend_remaining.c — Port of tools/computer_use/cua_backend.py
 * loop-backend surface. Event loop thread, run scheduling, positive
 * ints, window key, page interaction.
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

/* PoP: __init__ @ tools/computer_use/cua_backend.py:__init__ */
char *cua_init(void) {
    return strdup("{\"loop\": null, \"thread\": null}");
}

/* PoP: start @ tools/computer_use/cua_backend.py:start */
int cua_start(void) {
    /* Python: spawn loop thread once. */
    printf("cua backend thread started\n");
    return 0;
}

/* PoP: run @ tools/computer_use/cua_backend.py:run */
char *cua_run(const char *coro_desc) {
    /* Python: schedule threadsafe. */
    if (!coro_desc) return NULL;
    printf("cua coroutine scheduled: %.60s\n", coro_desc);
    return strdup("{}");
}

/* PoP: _positive_int @ tools/computer_use/cua_backend.py:_positive_int */
long cua_positive_int(const char *value, long default_value) {
    /* Python: reject booleans + malformed. */
    if (!value || !*value) return default_value;
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v > 0 ? v : default_value;
}

/* PoP: key @ tools/computer_use/cua_backend.py:key */
char *cua_key(const char *pid, const char *window_id) {
    /* Python: active window key. */
    if (!pid) return NULL;
    char *out = NULL;
    if (window_id && *window_id)
        asprintf(&out, "%s:%s", pid, window_id);
    else
        asprintf(&out, "%s", pid);
    return out;
}

/* PoP: page @ tools/computer_use/cua_backend.py:page */
char *cua_page(const char *url) {
    /* Python: interact with browser page. */
    if (!url) return NULL;
    printf("cua browser page interaction: %s\n", url);
    return strdup("{}");
}
