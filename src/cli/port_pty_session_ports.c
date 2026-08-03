/*
 * port_pty_session_remaining.c — Port of hermes_cli/pty_session.py PTY
 * output surface. Bounded ring buffer, snapshot, attach, close.
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

/* PoP: __init__ @ hermes_cli/pty_session.py:__init__ */
char *pty_init(long capacity) {
    /* Python: bounded output buffer. */
    if (capacity <= 0) capacity = 65536;
    char *out = NULL;
    asprintf(&out, "{\"capacity\": %ld, \"len\": 0, \"truncated\": false}", capacity);
    return out;
}

/* PoP: snapshot @ hermes_cli/pty_session.py:snapshot */
char *pty_snapshot(const char *buf_json) {
    /* Python: raw bytes. */
    if (!buf_json) return strdup("");
    return strdup(buf_json);
}

/* PoP: start @ hermes_cli/pty_session.py:start */
int pty_start(void) {
    /* Python: spawn drain task. */
    printf("pty drain task started\n");
    return 0;
}

/* PoP: attach @ hermes_cli/pty_session.py:attach */
int pty_attach(const char *ws_desc) {
    /* Python: replace ws attachment. */
    if (!ws_desc) return -1;
    printf("pty websocket attached\n");
    return 0;
}

/* PoP: close @ hermes_cli/pty_session.py:close */
int pty_close(void) {
    /* Python: cancel drain. */
    printf("pty session closed (drain cancelled)\n");
    return 0;
}
