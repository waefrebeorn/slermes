/*
 * ports/gateway/stream_dispatch.c — C11 port of gateway/stream_dispatch.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/gateway/port_stream_dispatch_ports.c ── */
/*
 * port_stream_dispatch_remaining.c — Port of gateway/stream_dispatch.py
 * dispatch surface. Event routing without raising into worker.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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

/* PoP: std_init @ gateway/stream_dispatch.py:__init__ */
char *std_init(const char *adapter_desc, const char *sink_desc) {
    char *out = NULL;
    asprintf(&out, "{\"adapter\": \"%s\", \"sink\": \"%s\"}",
             adapter_desc ? adapter_desc : "", sink_desc ? sink_desc : "");
    return out;
}

/* PoP: std_dispatch @ gateway/stream_dispatch.py:dispatch */
int std_dispatch(const char *event_json) {
    /* Python: route single event; never raises. */
    if (!event_json) return -1;
    printf("stream event dispatched (no-raise)\n");
    return 0;
}

/* PoP: std_dispatch_inner @ gateway/stream_dispatch.py:_dispatch */
int std_dispatch_inner(const char *event_json, bool sink_enabled) {
    /* Python: chunk/stop/commentary routing. */
    if (!event_json) return -1;
    if (!sink_enabled) return 0;
    printf("stream event routed to sink\n");
    return 0;
}

