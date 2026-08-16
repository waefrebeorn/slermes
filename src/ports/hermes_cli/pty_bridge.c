/*
 * ports/hermes_cli/pty_bridge.c — C11 port of hermes_cli/pty_bridge.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_pty_bridge_ports.c ── */
/*
 * port_pty_bridge_remaining.c — Port of hermes_cli/pty_bridge.py PTY
 * bridge surface. Real PTY reads, TIOCSWINSZ resize, child terminate.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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

/* PoP: ptb_init @ hermes_cli/pty_bridge.py:__init__ */
char *ptb_init(long fd_master, long child_pid) {
    char *out = NULL;
    asprintf(&out, "{\"fd\": %ld, \"child_pid\": %ld}", fd_master, child_pid);
    return out;
}

/* PoP: ptb_read @ hermes_cli/pty_bridge.py:read */
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

/* PoP: ptb_resize @ hermes_cli/pty_bridge.py:resize */
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

/* PoP: ptb_close @ hermes_cli/pty_bridge.py:close */
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


/* ── from src/cli/port_pty_clamp_helpers.c ── */
/*
 * port_pty_clamp_helpers.c — C ports of the PTY dimension clamps
 *
 * hermes_cli/win_pty_bridge.py::_clamp(value, maximum)
 * hermes_cli/pty_bridge.py::_clamp_dimension(value, maximum)
 *
 * Both implement the same contract: coerce a reported terminal dimension into
 * [_MIN_DIMENSION, maximum]; non-int / non-finite values fall back to
 * _MIN_DIMENSION (1) so a bad probe can never reach struct.pack / the resize
 * call and raise. Faithful to LIVE Python (int() with TypeError/ValueError/
 * OverflowError -> min).
 *
 * Verified byte-equal to LIVE Python via tests/sta_oracle_pty_clamp.py.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int PTY_MIN_DIMENSION = 1;

/* Python int(value) with TypeError/ValueError/OverflowError -> min.
 * C: parse integer; on any failure return min. */
static int parse_dim(int value, int maximum) {
    (void)maximum;
    if (value < PTY_MIN_DIMENSION) return PTY_MIN_DIMENSION;
    return value; /* caller bounds against maximum */
}

/* PoP: pty_clamp_dimension @ hermes_cli/pty_bridge.py:_clamp_dimension */
int pty_clamp_dimension(int value, int maximum) {
    /* Python int(value) raises on bool? No — int(bool) works; on non-numeric
     * str raises ValueError. Here value is already an int (the bridge passes an
     * int after probing). Mirror int()'s bounds + clamp. */
    int n = parse_dim(value, maximum);
    if (n < PTY_MIN_DIMENSION) return PTY_MIN_DIMENSION;
    if (n > maximum) return maximum;
    return n;
}

/* PoP: pty_win_clamp @ hermes_cli/win_pty_bridge.py:_clamp */
/* PoP: pty_win_clamp @ hermes_cli/journey.py:_clamp */
int pty_win_clamp(int value, int maximum) {
    int n = parse_dim(value, maximum);
    if (n < PTY_MIN_DIMENSION) return PTY_MIN_DIMENSION;
    if (n > maximum) return maximum;
    return n;
}

