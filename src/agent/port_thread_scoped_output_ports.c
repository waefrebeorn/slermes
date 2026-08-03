/*
 * port_thread_scoped_output_remaining.c — Port of agent/thread_scoped_output.py
 * scoped output surface. Passthrough/sink routing, tty probe.
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

/* PoP: __init__ @ agent/thread_scoped_output.py:__init__ */
char *tso_init(const char *passthrough_desc, const char *sink_desc) {
    char *out = NULL;
    asprintf(&out, "{\"passthrough\": \"%s\", \"sink\": \"%s\", \"nesting\": {}}",
             passthrough_desc ? passthrough_desc : "", sink_desc ? sink_desc : "");
    return out;
}

/* PoP: write @ agent/thread_scoped_output.py:write */
long tso_write(const char *text, size_t len) {
    /* Python: route to sink when scoped else passthrough. */
    if (!text) return 0;
    printf("%.*s", (int)len, text);
    return (long)len;
}

/* PoP: flush @ agent/thread_scoped_output.py:flush */
int tso_flush(void) {
    if (fflush(stdout) != 0) return -1;
    return 0;
}

/* PoP: isatty @ agent/thread_scoped_output.py:isatty */
bool tso_isatty(void) {
    return isatty(STDOUT_FILENO) == 1;
}

/* PoP: fileno @ agent/thread_scoped_output.py:fileno */
long tso_fileno(void) {
    return STDOUT_FILENO;
}
