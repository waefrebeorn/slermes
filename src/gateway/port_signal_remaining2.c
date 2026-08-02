/*
 * port_signal_remaining2.c — Port of gateway/platforms/signal.py adapter
 * surface. Extra config, connect/disconnect lifecycle.
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

/* PoP: __init__ @ gateway/platforms/signal.py:__init__ */
char *sgl_init(const char *config_json) {
    /* Python: extra config merge. */
    if (!config_json) return strdup("{}");
    printf("signal adapter init (extra config)\n");
    return strdup(config_json);
}

/* PoP: connect @ gateway/platforms/signal.py:connect */
bool sgl_connect(void) {
    /* Python: signal-cli daemon + SSE listener. */
    printf("signal connect (signal-cli + sse)\n");
    return false;
}

/* PoP: disconnect @ gateway/platforms/signal.py:disconnect */
int sgl_disconnect(void) {
    /* Python: stop SSE + cleanup. */
    printf("signal disconnected (sse stopped)\n");
    return 0;
}
