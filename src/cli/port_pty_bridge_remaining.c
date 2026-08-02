/*
 * port_pty_bridge_remaining.c — Port of hermes_cli/pty_bridge.py PTY
 * bridge surface. Real PTY reads, TIOCSWINSZ resize, child terminate.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ hermes_cli/pty_bridge.py:__init__ */
char *ptb_init(long fd_master, long child_pid) {
    char *out = NULL;
    asprintf(&out, "{\"fd\": %ld, \"child_pid\": %ld}", fd_master, child_pid);
    return out;
}

/* PoP: read @ hermes_cli/pty_bridge.py:read */
char *ptb_read(long fd_master) {
    /* Python: up to 64 KiB raw — REAL read. */
    if (fd_master < 0) return NULL;
    char *buf = malloc(65536);
    if (!buf) return NULL;
    ssize_t n = read((int)fd_master, buf, 65536);
    if (n <= 0) { free(buf); return NULL; }
    buf[n] = '\0';
    char *out = strdup(buf);
    free(buf);
    return out;
}

/* PoP: resize @ hermes_cli/pty_bridge.py:resize */
int ptb_resize(long fd_master, unsigned short rows, unsigned short cols) {
    /* Python: TIOCSWINSZ — REAL ioctl. */
    if (fd_master < 0) return -1;
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_row = rows;
    ws.ws_col = cols;
    if (ioctl((int)fd_master, TIOCSWINSZ, &ws) != 0) return -1;
    return 0;
}

/* PoP: close @ hermes_cli/pty_bridge.py:close */
int ptb_close(long fd_master, long child_pid) {
    /* Python: SIGTERM → 0.5s → SIGKILL, idempotent. */
    if (child_pid > 0) {
        kill((pid_t)child_pid, SIGTERM);
        usleep(500000);
        kill((pid_t)child_pid, SIGKILL);
    }
    if (fd_master >= 0) close((int)fd_master);
    return 0;
}
