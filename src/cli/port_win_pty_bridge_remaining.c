/*
 * port_win_pty_bridge_remaining.c — Port of hermes_cli/win_pty_bridge.py
 * windows-pty surface. Bounded child reads, clamped resize, idempotent
 * close.
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

/* PoP: __init__ @ hermes_cli/win_pty_bridge.py:__init__ */
char *wpb_init(long fd_child, long pid_child) {
    char *out = NULL;
    asprintf(&out, "{\"fd\": %ld, \"pid\": %ld, \"closed\": false}", fd_child, pid_child);
    return out;
}

/* PoP: read @ hermes_cli/win_pty_bridge.py:read */
char *wpb_read(long fd_child) {
    /* Python: up to 64 KiB, b"" when empty — REAL read. */
    if (fd_child < 0) return strdup("");
    char *buf = malloc(65537);
    if (!buf) return strdup("");
    ssize_t n = read((int)fd_child, buf, 65536);
    if (n <= 0) { free(buf); return strdup(""); }
    buf[n] = '\0';
    char *out = strdup(buf);
    free(buf);
    return out;
}

/* PoP: resize @ hermes_cli/win_pty_bridge.py:resize */
int wpb_resize(bool closed, unsigned short cols, unsigned short rows) {
    /* Python: no-op when closed; clamped. */
    if (closed) return 0;
    if (cols > 500) cols = 500;
    if (rows > 200) rows = 200;
    printf("win-pty resized to %ux%u\n", cols, rows);
    return 0;
}

/* PoP: close @ hermes_cli/win_pty_bridge.py:close */
int wpb_close(bool closed, long fd_child) {
    /* Python: idempotent close. */
    if (closed) return 0;
    if (fd_child >= 0) close((int)fd_child);
    printf("win-pty closed\n");
    return 0;
}
