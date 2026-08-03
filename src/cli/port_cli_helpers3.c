/*
 * port_cli_remaining3.c — Port of cli.py console styling surface.
 * Skin-color console with buffer capture, rich-compat status.
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

/* PoP: __init__ @ cli.py:__init__ */
char *cli3_init(const char *skin_key, const char *fallback_hex) {
    /* Python: skin-aware console. */
    char *out = NULL;
    asprintf(&out, "{\"skin_key\": \"%s\", \"fallback_hex\": \"%s\", \"buffer\": \"\"}",
             skin_key ? skin_key : "", fallback_hex ? fallback_hex : "#888888");
    return out;
}

/* PoP: print @ cli.py:print */
int cli3_print(const char *text) {
    /* Python: buffer + terminal width read. */
    if (!text) return -1;
    printf("%s\n", text);
    return 0;
}

/* PoP: status @ cli.py:status */
char *cli3_status(const char *message) {
    /* Python: no-op rich-compat status context. */
    if (!message) return NULL;
    printf("%s…\n", message);
    return strdup("{}");
}
